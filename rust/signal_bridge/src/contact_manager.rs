//! Contact management for Radix Relay
//!
//! This module provides contact database operations separate from Signal Protocol
//! encryption concerns. Contacts are identified by RDX fingerprints with optional
//! user-assigned aliases.

use crate::nostr_identity::NostrIdentity;
use crate::SignalBridgeError;
use libsignal_protocol::{DeviceId, IdentityKey, ProtocolAddress, SessionStore};
use rusqlite::OptionalExtension;
use sha2::{Digest, Sha256};
use std::sync::{Arc, Mutex};
use std::time::{SystemTime, UNIX_EPOCH};

#[derive(Clone, Debug)]
pub struct ContactInfo {
    pub rdx_fingerprint: String,
    pub nostr_pubkey: String,
    pub user_alias: Option<String>,
    pub has_active_session: bool,
}

pub struct ContactManager {
    storage: Arc<Mutex<rusqlite::Connection>>,
}

impl ContactManager {
    pub fn new(storage_connection: Arc<Mutex<rusqlite::Connection>>) -> Self {
        Self {
            storage: storage_connection,
        }
    }

    pub async fn add_contact_from_bundle(
        &mut self,
        bundle_bytes: &[u8],
        user_alias: Option<&str>,
        _session_store: &mut impl SessionStore,
    ) -> Result<String, SignalBridgeError> {
        use crate::SerializablePreKeyBundle;

        let bundle: SerializablePreKeyBundle = bincode::deserialize(bundle_bytes).map_err(|e| {
            SignalBridgeError::InvalidInput(format!("Failed to deserialize bundle: {}", e))
        })?;

        let identity_key = IdentityKey::decode(&bundle.identity_key)
            .map_err(|e| SignalBridgeError::Protocol(e.to_string()))?;

        let nostr_pubkey = NostrIdentity::derive_public_key_from_peer_identity(&identity_key)?;

        let rdx = Self::generate_identity_fingerprint_from_key(&identity_key);

        let now = SystemTime::now().duration_since(UNIX_EPOCH)?.as_secs();
        let conn_lock = self.storage.lock().unwrap();

        conn_lock
            .execute(
                "INSERT OR REPLACE INTO contacts
                 (rdx_fingerprint, nostr_pubkey, user_alias, signal_identity_key, first_seen, last_updated)
                 VALUES (?1, ?2, ?3, ?4,
                         COALESCE((SELECT first_seen FROM contacts WHERE rdx_fingerprint = ?1), ?5),
                         ?5)",
                rusqlite::params![rdx, nostr_pubkey.to_hex(), user_alias, bundle.identity_key, now],
            )
            .map_err(|e| SignalBridgeError::Storage(e.to_string()))?;

        Ok(rdx)
    }

    pub async fn lookup_contact(
        &mut self,
        identifier: &str,
        session_store: &mut impl SessionStore,
    ) -> Result<ContactInfo, SignalBridgeError> {
        let contact = {
            let conn_lock = self.storage.lock().unwrap();

            let mut stmt = conn_lock
                .prepare(
                    "SELECT rdx_fingerprint, nostr_pubkey, user_alias
                     FROM contacts
                     WHERE rdx_fingerprint = ?1 OR user_alias = ?1 OR nostr_pubkey = ?1",
                )
                .map_err(|e| SignalBridgeError::Storage(e.to_string()))?;

            stmt.query_row(rusqlite::params![identifier], |row| {
                Ok(ContactInfo {
                    rdx_fingerprint: row.get(0)?,
                    nostr_pubkey: row.get(1)?,
                    user_alias: row.get(2)?,
                    has_active_session: false,
                })
            })
            .map_err(|e| SignalBridgeError::Storage(format!("Contact not found: {}", e)))?
        };

        let address =
            ProtocolAddress::new(contact.rdx_fingerprint.clone(), DeviceId::new(1).unwrap());
        let has_session = session_store.load_session(&address).await?.is_some();

        Ok(ContactInfo {
            has_active_session: has_session,
            ..contact
        })
    }

    pub async fn assign_contact_alias(
        &mut self,
        identifier: &str,
        new_alias: &str,
        session_store: &mut impl SessionStore,
    ) -> Result<(), SignalBridgeError> {
        let contact = self.lookup_contact(identifier, session_store).await?;

        let now = SystemTime::now().duration_since(UNIX_EPOCH)?.as_secs();
        let conn_lock = self.storage.lock().unwrap();

        // Check if this alias is already assigned to a different contact
        let existing: Option<String> = conn_lock
            .query_row(
                "SELECT rdx_fingerprint FROM contacts WHERE user_alias = ?1",
                rusqlite::params![new_alias],
                |row| row.get(0),
            )
            .optional()
            .map_err(|e| SignalBridgeError::Storage(e.to_string()))?;

        if let Some(existing_rdx) = existing {
            if existing_rdx != contact.rdx_fingerprint {
                return Err(SignalBridgeError::InvalidInput(format!(
                    "Alias '{}' is already assigned to contact {}. Remove it first before reassigning.",
                    new_alias, existing_rdx
                )));
            }
            // If the alias is already assigned to this same contact, that's fine - just update the timestamp
        }

        // Assign the alias to the target contact
        let updated = conn_lock
            .execute(
                "UPDATE contacts SET user_alias = ?1, last_updated = ?2
                 WHERE rdx_fingerprint = ?3",
                rusqlite::params![new_alias, now, contact.rdx_fingerprint],
            )
            .map_err(|e| SignalBridgeError::Storage(e.to_string()))?;

        if updated == 0 {
            return Err(SignalBridgeError::InvalidInput(format!(
                "Contact not found: {}",
                identifier
            )));
        }

        Ok(())
    }

    pub async fn list_contacts(
        &mut self,
        session_store: &mut impl SessionStore,
    ) -> Result<Vec<ContactInfo>, SignalBridgeError> {
        let contacts = {
            let conn_lock = self.storage.lock().unwrap();

            let mut stmt = conn_lock
                .prepare(
                    "SELECT rdx_fingerprint, nostr_pubkey, user_alias FROM contacts
                     ORDER BY last_updated DESC",
                )
                .map_err(|e| SignalBridgeError::Storage(e.to_string()))?;

            let contacts_iter = stmt
                .query_map([], |row| {
                    Ok(ContactInfo {
                        rdx_fingerprint: row.get(0)?,
                        nostr_pubkey: row.get(1)?,
                        user_alias: row.get(2)?,
                        has_active_session: false,
                    })
                })
                .map_err(|e| SignalBridgeError::Storage(e.to_string()))?;

            contacts_iter
                .filter_map(Result::ok)
                .collect::<Vec<ContactInfo>>()
        };

        let mut result = Vec::new();
        for mut contact in contacts {
            let address =
                ProtocolAddress::new(contact.rdx_fingerprint.clone(), DeviceId::new(1).unwrap());
            contact.has_active_session = session_store.load_session(&address).await?.is_some();
            result.push(contact);
        }

        Ok(result)
    }

    pub fn generate_identity_fingerprint_from_key(identity_key: &IdentityKey) -> String {
        let mut hasher = Sha256::new();
        hasher.update(identity_key.serialize());
        hasher.update(b"radix-identity-fingerprint");
        let result = hasher.finalize();
        format!("RDX:{:x}", result)
    }
}
