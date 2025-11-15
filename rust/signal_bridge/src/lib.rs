//! Signal Protocol integration for Radix Relay
//!
//! This crate provides a bridge between Radix Relay's C++ transport layer
//! and the official Signal Protocol Rust implementation for end-to-end encryption.

mod contact_manager;
mod encryption_trait;
mod keys;
mod memory_storage;
mod nostr_identity;
mod session_trait;
mod sqlite_storage;
mod storage_trait;

#[cfg(test)]
mod session;

#[cfg(test)]
mod encryption;

pub use contact_manager::{ContactInfo, ContactManager};
pub use nostr_identity::NostrIdentity;

use crate::sqlite_storage::SqliteStorage;
use crate::storage_trait::{
    ExtendedIdentityStore, ExtendedKyberPreKeyStore, ExtendedPreKeyStore, ExtendedSessionStore,
    ExtendedSignedPreKeyStore, ExtendedStorageOps, SignalStorageContainer,
};
use base64::Engine;
use libsignal_protocol::{
    CiphertextMessage, DeviceId, IdentityKeyPair, IdentityKeyStore, ProtocolAddress, SessionStore,
};
use nostr::{EventBuilder, Keys, Kind, Tag};
use serde::{Deserialize, Serialize};
use std::time::{SystemTime, UNIX_EPOCH};

#[derive(Serialize, Deserialize, Clone)]
struct SerializablePreKeyBundle {
    pub registration_id: u32,
    pub device_id: u32,
    pub pre_key_id: Option<u32>,
    pub pre_key_public: Option<Vec<u8>>,
    pub signed_pre_key_id: u32,
    pub signed_pre_key_public: Vec<u8>,
    pub signed_pre_key_signature: Vec<u8>,
    pub identity_key: Vec<u8>,
    pub kyber_pre_key_id: u32,
    pub kyber_pre_key_public: Vec<u8>,
    pub kyber_pre_key_signature: Vec<u8>,
}

pub struct SignalBridge {
    storage: SqliteStorage,
    contact_manager: ContactManager,
}

impl SignalBridge {
    async fn ensure_keys_exist(
        storage: &mut SqliteStorage,
    ) -> Result<IdentityKeyPair, SignalBridgeError> {
        use crate::keys::{generate_identity_key_pair, generate_pre_keys, generate_signed_pre_key};
        use crate::storage_trait::{
            ExtendedIdentityStore, ExtendedKyberPreKeyStore, ExtendedPreKeyStore,
            ExtendedSignedPreKeyStore,
        };
        use libsignal_protocol::*;

        let identity_key_pair = match storage.identity_store().get_identity_key_pair().await {
            Ok(existing_identity) => existing_identity,
            Err(_) => {
                let new_identity = generate_identity_key_pair().await?;
                let new_registration_id = rand::random::<u32>();

                storage
                    .identity_store()
                    .set_local_identity_key_pair(&new_identity)
                    .await?;
                storage
                    .identity_store()
                    .set_local_registration_id(new_registration_id)
                    .await?;

                new_identity
            }
        };

        let current_pre_key_count = storage.pre_key_store().pre_key_count().await;
        if current_pre_key_count < 5 {
            let pre_keys = generate_pre_keys(1, 10).await?;
            for (key_id, key_pair) in &pre_keys {
                let record = PreKeyRecord::new((*key_id).into(), key_pair);
                storage
                    .pre_key_store()
                    .save_pre_key((*key_id).into(), &record)
                    .await?;
            }
        }

        let current_signed_pre_key_count =
            storage.signed_pre_key_store().signed_pre_key_count().await;
        if current_signed_pre_key_count == 0 {
            let signed_pre_key = generate_signed_pre_key(&identity_key_pair, 1).await?;
            storage
                .signed_pre_key_store()
                .save_signed_pre_key(signed_pre_key.id()?, &signed_pre_key)
                .await?;
        }

        let current_kyber_pre_key_count = storage.kyber_pre_key_store().kyber_pre_key_count().await;
        if current_kyber_pre_key_count == 0 {
            let mut rng = rand::rng();
            let kyber_keypair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
            let kyber_signature = identity_key_pair
                .private_key()
                .calculate_signature(&kyber_keypair.public_key.serialize(), &mut rng)
                .map_err(|e| SignalBridgeError::Protocol(e.to_string()))?;

            let now = std::time::SystemTime::now();
            let kyber_record = KyberPreKeyRecord::new(
                KyberPreKeyId::from(1u32),
                Timestamp::from_epoch_millis(
                    now.duration_since(std::time::UNIX_EPOCH)?.as_millis() as u64,
                ),
                &kyber_keypair,
                &kyber_signature,
            );
            storage
                .kyber_pre_key_store()
                .save_kyber_pre_key(KyberPreKeyId::from(1u32), &kyber_record)
                .await?;
        }

        Ok(identity_key_pair)
    }

    pub async fn new(db_path: &str) -> Result<Self, SignalBridgeError> {
        if let Some(parent) = std::path::Path::new(db_path).parent() {
            std::fs::create_dir_all(parent)?;
        }

        let mut storage = SqliteStorage::new(db_path).await?;
        storage.initialize()?;

        let schema_version = storage.get_schema_version()?;
        if schema_version < 1 {
            return Err(SignalBridgeError::SchemaVersionTooOld);
        }

        Self::ensure_keys_exist(&mut storage).await?;

        println!("SignalBridge initialized with {} storage, {} existing sessions, {} peer identities, {} pre-keys, {} signed pre-keys, {} kyber pre-keys",
                 storage.storage_type(),
                 storage.session_store().session_count().await,
                 storage.identity_store().identity_count().await,
                 storage.pre_key_store().pre_key_count().await,
                 storage.signed_pre_key_store().signed_pre_key_count().await,
                 storage.kyber_pre_key_store().kyber_pre_key_count().await);

        let contact_manager = ContactManager::new(storage.connection());

        Ok(Self {
            storage,
            contact_manager,
        })
    }

    pub async fn encrypt_message(
        &mut self,
        peer: &str,
        plaintext: &[u8],
    ) -> Result<Vec<u8>, SignalBridgeError> {
        if peer.is_empty() {
            return Err(SignalBridgeError::InvalidInput(
                "Specify a peer name".to_string(),
            ));
        }

        let session_address = match self
            .contact_manager
            .lookup_contact(peer, self.storage.session_store())
            .await
        {
            Ok(contact) => contact.rdx_fingerprint,
            Err(_) => peer.to_string(),
        };

        let address = ProtocolAddress::new(
            session_address.clone(),
            DeviceId::new(1).map_err(|e| SignalBridgeError::Protocol(e.to_string()))?,
        );

        let session = self.storage.session_store().load_session(&address).await?;
        if session.is_none() {
            return Err(SignalBridgeError::SessionNotFound(format!(
                "Establish a session with {} before sending messages",
                peer
            )));
        }

        let ciphertext = self.storage.encrypt_message(&address, plaintext).await?;
        Ok(ciphertext.serialize().to_vec())
    }

    pub async fn decrypt_message(
        &mut self,
        peer: &str,
        ciphertext_bytes: &[u8],
    ) -> Result<Vec<u8>, SignalBridgeError> {
        use libsignal_protocol::{PreKeySignalMessage, SignalMessage};

        if peer.is_empty() {
            return Err(SignalBridgeError::InvalidInput(
                "Specify a peer name".to_string(),
            ));
        }

        if ciphertext_bytes.is_empty() {
            return Err(SignalBridgeError::InvalidInput(
                "Provide a message to decrypt".to_string(),
            ));
        }

        let session_address = match self
            .contact_manager
            .lookup_contact(peer, self.storage.session_store())
            .await
        {
            Ok(contact) => contact.rdx_fingerprint,
            Err(_) => peer.to_string(),
        };

        let address = ProtocolAddress::new(
            session_address,
            DeviceId::new(1).map_err(|e| SignalBridgeError::Protocol(e.to_string()))?,
        );

        let ciphertext = if let Ok(prekey_msg) = PreKeySignalMessage::try_from(ciphertext_bytes) {
            CiphertextMessage::PreKeySignalMessage(prekey_msg)
        } else if let Ok(signal_msg) = SignalMessage::try_from(ciphertext_bytes) {
            CiphertextMessage::SignalMessage(signal_msg)
        } else {
            return Err(SignalBridgeError::Serialization(
                "Provide a valid Signal Protocol message".to_string(),
            ));
        };

        let plaintext = self.storage.decrypt_message(&address, &ciphertext).await?;
        Ok(plaintext)
    }

    pub async fn establish_session(
        &mut self,
        peer: &str,
        pre_key_bundle_bytes: &[u8],
    ) -> Result<(), SignalBridgeError> {
        if peer.is_empty() {
            return Err(SignalBridgeError::InvalidInput(
                "Specify a peer name".to_string(),
            ));
        }

        if pre_key_bundle_bytes.is_empty() {
            return Err(SignalBridgeError::InvalidInput(
                "Provide a pre-key bundle from the peer".to_string(),
            ));
        }
        use crate::storage_trait::ExtendedStorageOps;
        use libsignal_protocol::*;

        let serializable: SerializablePreKeyBundle = bincode::deserialize(pre_key_bundle_bytes)?;

        let bundle = PreKeyBundle::new(
            serializable.registration_id,
            DeviceId::new(serializable.device_id.try_into().map_err(|e| {
                SignalBridgeError::InvalidInput(format!("Invalid device ID: {}", e))
            })?)
            .map_err(|e| SignalBridgeError::Protocol(e.to_string()))?,
            serializable
                .pre_key_id
                .map(|id| {
                    let public_key =
                        PublicKey::deserialize(serializable.pre_key_public.as_ref().unwrap())
                            .map_err(|e| SignalBridgeError::Serialization(e.to_string()))?;
                    Ok::<_, SignalBridgeError>((PreKeyId::from(id), public_key))
                })
                .transpose()?,
            SignedPreKeyId::from(serializable.signed_pre_key_id),
            PublicKey::deserialize(&serializable.signed_pre_key_public)
                .map_err(|e| SignalBridgeError::Serialization(e.to_string()))?,
            serializable.signed_pre_key_signature,
            KyberPreKeyId::from(serializable.kyber_pre_key_id),
            kem::PublicKey::deserialize(&serializable.kyber_pre_key_public)
                .map_err(|e| SignalBridgeError::Serialization(e.to_string()))?,
            serializable.kyber_pre_key_signature,
            IdentityKey::decode(&serializable.identity_key)
                .map_err(|e| SignalBridgeError::Serialization(e.to_string()))?,
        )?;

        let address = ProtocolAddress::new(
            peer.to_string(),
            DeviceId::new(1).map_err(|e| SignalBridgeError::Protocol(e.to_string()))?,
        );

        self.storage
            .establish_session_from_bundle(&address, &bundle)
            .await?;

        Ok(())
    }

    pub async fn generate_pre_key_bundle(&mut self) -> Result<Vec<u8>, SignalBridgeError> {
        use libsignal_protocol::*;

        // TODO: PreKey Management & Rotation (Phase 7)
        // Current implementation always uses PreKey #1, which is NOT production-ready:
        // 1. One-time pre-keys should be consumed after use (delete after session establishment)
        // 2. Should track available pre-keys and replenish when count < threshold (e.g., 10)
        // 3. Should generate batches of ~100 pre-keys when replenishing
        // 4. Signed pre-keys should rotate periodically (e.g., weekly) for forward secrecy
        // 5. Kyber pre-keys should also rotate with signed pre-keys
        // 6. Need to track which pre-keys are "in bundles" (pending use) vs truly available
        // For now this works for demo/testing where each identity establishes limited sessions.

        let identity_key = *self
            .storage
            .identity_store()
            .get_identity_key_pair()
            .await?
            .identity_key();
        let registration_id = self
            .storage
            .identity_store()
            .get_local_registration_id()
            .await?;

        let pre_key_record = self
            .storage
            .pre_key_store()
            .get_pre_key(PreKeyId::from(1u32))
            .await?;
        let signed_pre_key_record = self
            .storage
            .signed_pre_key_store()
            .get_signed_pre_key(SignedPreKeyId::from(1u32))
            .await?;
        let kyber_pre_key_record = self
            .storage
            .kyber_pre_key_store()
            .get_kyber_pre_key(KyberPreKeyId::from(1u32))
            .await?;

        let bundle = PreKeyBundle::new(
            registration_id,
            DeviceId::new(1).map_err(|e| SignalBridgeError::Protocol(e.to_string()))?,
            Some((PreKeyId::from(1u32), pre_key_record.key_pair()?.public_key)),
            signed_pre_key_record.id()?,
            signed_pre_key_record.public_key()?,
            signed_pre_key_record.signature()?.to_vec(),
            KyberPreKeyId::from(1u32),
            kyber_pre_key_record.key_pair()?.public_key,
            kyber_pre_key_record.signature()?.to_vec(),
            identity_key,
        )?;

        let serializable = SerializablePreKeyBundle {
            registration_id: bundle.registration_id()?,
            device_id: bundle.device_id()?.into(),
            pre_key_id: bundle.pre_key_id()?.map(|id| id.into()),
            pre_key_public: bundle.pre_key_public()?.map(|key| key.serialize().to_vec()),
            signed_pre_key_id: bundle.signed_pre_key_id()?.into(),
            signed_pre_key_public: bundle.signed_pre_key_public()?.serialize().to_vec(),
            signed_pre_key_signature: bundle.signed_pre_key_signature()?.to_vec(),
            identity_key: bundle.identity_key()?.serialize().to_vec(),
            kyber_pre_key_id: bundle.kyber_pre_key_id()?.into(),
            kyber_pre_key_public: bundle.kyber_pre_key_public()?.serialize().to_vec(),
            kyber_pre_key_signature: bundle.kyber_pre_key_signature()?.to_vec(),
        };

        Ok(bincode::serialize(&serializable)?)
    }

    pub async fn cleanup_all_sessions(&mut self) -> Result<(), SignalBridgeError> {
        let session_count = self.storage.session_store().session_count().await;
        if session_count > 0 {
            println!("Cleaning up {} sessions", session_count);
            self.storage.session_store().clear_all_sessions().await?;
        }
        Ok(())
    }

    pub async fn clear_peer_session(&mut self, peer: &str) -> Result<(), SignalBridgeError> {
        if peer.is_empty() {
            return Err(SignalBridgeError::InvalidInput(
                "Specify a peer name".to_string(),
            ));
        }
        let address = ProtocolAddress::new(
            peer.to_string(),
            DeviceId::new(1).map_err(|e| SignalBridgeError::Protocol(e.to_string()))?,
        );

        self.storage
            .session_store()
            .delete_session(&address)
            .await?;

        if (self
            .storage
            .identity_store()
            .get_peer_identity(&address)
            .await)
            .is_ok()
        {
            self.storage
                .identity_store()
                .delete_identity(&address)
                .await?;
        }

        println!("Cleared session and identity for peer: {}", peer);
        Ok(())
    }

    pub async fn clear_all_sessions(&mut self) -> Result<(), SignalBridgeError> {
        let session_count = self.storage.session_store().session_count().await;
        let identity_count = self.storage.identity_store().identity_count().await;

        if session_count > 0 || identity_count > 0 {
            self.storage.session_store().clear_all_sessions().await?;
            self.storage.identity_store().clear_all_identities().await?;

            println!(
                "Cleared all {} sessions and {} peer identities",
                session_count, identity_count
            );
        }
        Ok(())
    }

    pub async fn reset_identity(&mut self) -> Result<(), SignalBridgeError> {
        use crate::storage_trait::{
            ExtendedIdentityStore, ExtendedKyberPreKeyStore, ExtendedPreKeyStore,
            ExtendedSignedPreKeyStore,
        };

        println!("WARNING: Resetting identity - all existing sessions will be invalidated");

        self.clear_all_sessions().await?;

        self.storage.pre_key_store().clear_all_pre_keys().await?;
        self.storage
            .signed_pre_key_store()
            .clear_all_signed_pre_keys()
            .await?;
        self.storage
            .kyber_pre_key_store()
            .clear_all_kyber_pre_keys()
            .await?;
        self.storage.identity_store().clear_local_identity().await?;

        Self::ensure_keys_exist(&mut self.storage).await?;

        println!("Identity reset complete - new identity generated with fresh keys");
        Ok(())
    }

    pub async fn derive_nostr_keypair(&mut self) -> Result<Keys, SignalBridgeError> {
        let identity_key_pair = self
            .storage
            .identity_store()
            .get_identity_key_pair()
            .await?;
        NostrIdentity::derive_from_signal_identity(&identity_key_pair)
    }

    pub async fn derive_peer_nostr_key(
        &mut self,
        peer: &str,
    ) -> Result<Vec<u8>, SignalBridgeError> {
        let session_address = match self
            .contact_manager
            .lookup_contact(peer, self.storage.session_store())
            .await
        {
            Ok(contact) => contact.rdx_fingerprint,
            Err(_) => peer.to_string(),
        };

        let peer_identity = self
            .storage
            .identity_store()
            .get_identity(&ProtocolAddress::new(
                session_address,
                DeviceId::new(1).unwrap(),
            ))
            .await?
            .ok_or_else(|| {
                SignalBridgeError::SessionNotFound(format!("No identity found for peer: {}", peer))
            })?;

        let peer_nostr_public_key =
            NostrIdentity::derive_public_key_from_peer_identity(&peer_identity)?;
        Ok(peer_nostr_public_key.to_bytes().to_vec())
    }

    pub async fn create_subscription_for_self(
        &mut self,
        subscription_id: &str,
    ) -> Result<String, SignalBridgeError> {
        let keys = self.derive_nostr_keypair().await?;
        let our_pubkey = keys.public_key().to_hex();

        let subscription = serde_json::json!([
            "REQ",
            subscription_id,
            {
                "kinds": [40001],
                "#p": [our_pubkey]
            }
        ]);

        Ok(subscription.to_string())
    }

    pub async fn generate_prekey_bundle_announcement(
        &mut self,
        project_version: &str,
    ) -> Result<String, SignalBridgeError> {
        let bundle_bytes = self.generate_pre_key_bundle().await?;

        let rdx_fingerprint = self
            .add_contact_from_bundle(&bundle_bytes, Some("self"))
            .await?;

        let bundle_base64 = base64::engine::general_purpose::STANDARD.encode(&bundle_bytes);
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map_err(|e| SignalBridgeError::Protocol(e.to_string()))?
            .as_secs();

        let tags = vec![
            vec!["d".to_string(), "radix_prekey_bundle_v1".to_string()],
            vec!["rdx".to_string(), rdx_fingerprint.clone()],
            vec!["radix_version".to_string(), project_version.to_string()],
            vec!["bundle_timestamp".to_string(), timestamp.to_string()],
        ];

        let (pubkey, event_id, signature) = self
            .sign_nostr_event(timestamp, 30078, tags.clone(), &bundle_base64)
            .await?;

        let event = serde_json::json!({
            "id": event_id,
            "pubkey": pubkey,
            "created_at": timestamp,
            "kind": 30078,
            "tags": [
                ["d", "radix_prekey_bundle_v1"],
                ["rdx", rdx_fingerprint],
                ["radix_version", project_version],
                ["bundle_timestamp", timestamp.to_string()],
            ],
            "content": bundle_base64,
            "sig": signature,
        });

        serde_json::to_string(&event).map_err(|e| SignalBridgeError::Serialization(e.to_string()))
    }

    pub async fn add_contact_and_establish_session(
        &mut self,
        bundle_bytes: &[u8],
        user_alias: Option<&str>,
    ) -> Result<String, SignalBridgeError> {
        use crate::SerializablePreKeyBundle;
        use libsignal_protocol::IdentityKey;

        let bundle: SerializablePreKeyBundle = bincode::deserialize(bundle_bytes).map_err(|e| {
            SignalBridgeError::InvalidInput(format!("Failed to deserialize bundle: {}", e))
        })?;

        let bundle_identity_key = IdentityKey::decode(&bundle.identity_key)
            .map_err(|e| SignalBridgeError::Protocol(e.to_string()))?;

        let our_identity_pair = self
            .storage
            .identity_store()
            .get_identity_key_pair()
            .await
            .map_err(|e| SignalBridgeError::Storage(e.to_string()))?;
        let our_identity_key = our_identity_pair.identity_key();

        if bundle_identity_key.public_key() == our_identity_key.public_key() {
            return Err(SignalBridgeError::InvalidInput(
                "Ignoring bundle from self".to_string(),
            ));
        }

        let rdx = self
            .add_contact_from_bundle(bundle_bytes, user_alias)
            .await?;

        self.establish_session(&rdx, bundle_bytes).await?;

        Ok(rdx)
    }

    pub async fn sign_nostr_event(
        &mut self,
        created_at: u64,
        kind: u32,
        tags: Vec<Vec<String>>,
        content: &str,
    ) -> Result<(String, String, String), SignalBridgeError> {
        let keys = self.derive_nostr_keypair().await?;

        let nostr_tags: Vec<Tag> = tags
            .into_iter()
            .map(|tag_vec| {
                if tag_vec.is_empty() {
                    Tag::parse(&[""]).unwrap_or_else(|_| Tag::parse(&["unknown"]).unwrap())
                } else {
                    Tag::parse(&tag_vec).unwrap_or_else(|_| Tag::parse(&["unknown"]).unwrap())
                }
            })
            .collect();

        let event = EventBuilder::new(Kind::Custom(kind as u16), content, nostr_tags)
            .custom_created_at(nostr::Timestamp::from(created_at))
            .to_event(&keys)
            .map_err(|e| SignalBridgeError::Protocol(format!("Failed to create event: {}", e)))?;

        Ok((
            keys.public_key().to_hex(),
            event.id.to_hex(),
            event.sig.to_string(),
        ))
    }

    pub async fn generate_node_fingerprint(&mut self) -> Result<String, SignalBridgeError> {
        let identity_key_pair = self
            .storage
            .identity_store()
            .get_identity_key_pair()
            .await?;

        Ok(ContactManager::generate_identity_fingerprint_from_key(
            identity_key_pair.identity_key(),
        ))
    }

    pub async fn add_contact_from_bundle(
        &mut self,
        bundle_bytes: &[u8],
        user_alias: Option<&str>,
    ) -> Result<String, SignalBridgeError> {
        self.contact_manager
            .add_contact_from_bundle(bundle_bytes, user_alias, self.storage.session_store())
            .await
    }

    pub async fn lookup_contact(
        &mut self,
        identifier: &str,
    ) -> Result<ContactInfo, SignalBridgeError> {
        self.contact_manager
            .lookup_contact(identifier, self.storage.session_store())
            .await
    }

    pub async fn assign_contact_alias(
        &mut self,
        identifier: &str,
        new_alias: &str,
    ) -> Result<(), SignalBridgeError> {
        self.contact_manager
            .assign_contact_alias(identifier, new_alias, self.storage.session_store())
            .await
    }

    pub async fn list_contacts(&mut self) -> Result<Vec<ContactInfo>, SignalBridgeError> {
        self.contact_manager
            .list_contacts(self.storage.session_store())
            .await
    }
}

impl Drop for SignalBridge {
    fn drop(&mut self) {
        if !self.storage.is_closed() {
            if let Err(e) = self.storage.close() {
                eprintln!("Warning: Failed to close storage properly: {}", e);
            }
        }
    }
}

#[derive(Debug, thiserror::Error)]
pub enum SignalBridgeError {
    #[error("Storage error: {0}")]
    Storage(String),
    #[error("Signal Protocol error: {0}")]
    Protocol(String),
    #[error("Serialization error: {0}")]
    Serialization(String),
    #[error("Invalid input: {0}")]
    InvalidInput(String),
    #[error("{0}")]
    SessionNotFound(String),
    #[error("Key derivation error: {0}")]
    KeyDerivation(String),
    #[error("Update your database to a newer schema version")]
    SchemaVersionTooOld,
}

impl From<libsignal_protocol::SignalProtocolError> for SignalBridgeError {
    fn from(err: libsignal_protocol::SignalProtocolError) -> Self {
        SignalBridgeError::Protocol(err.to_string())
    }
}

impl From<bincode::Error> for SignalBridgeError {
    fn from(err: bincode::Error) -> Self {
        SignalBridgeError::Serialization(err.to_string())
    }
}

impl From<std::time::SystemTimeError> for SignalBridgeError {
    fn from(err: std::time::SystemTimeError) -> Self {
        SignalBridgeError::Protocol(err.to_string())
    }
}

impl From<rusqlite::Error> for SignalBridgeError {
    fn from(err: rusqlite::Error) -> Self {
        SignalBridgeError::Storage(err.to_string())
    }
}

impl From<Box<dyn std::error::Error>> for SignalBridgeError {
    fn from(err: Box<dyn std::error::Error>) -> Self {
        SignalBridgeError::Storage(err.to_string())
    }
}

impl From<std::num::TryFromIntError> for SignalBridgeError {
    fn from(err: std::num::TryFromIntError) -> Self {
        SignalBridgeError::InvalidInput(format!("Integer conversion error: {}", err))
    }
}

impl From<std::io::Error> for SignalBridgeError {
    fn from(err: std::io::Error) -> Self {
        SignalBridgeError::Storage(format!("Filesystem error: {}", err))
    }
}

#[cxx::bridge(namespace = "radix_relay")]
mod ffi {
    #[derive(Clone, Debug)]
    pub struct ContactInfo {
        pub rdx_fingerprint: String,
        pub nostr_pubkey: String,
        pub user_alias: String,
        pub has_active_session: bool,
    }

    extern "Rust" {
        type SignalBridge;

        fn new_signal_bridge(db_path: &str) -> Result<Box<SignalBridge>>;

        fn encrypt_message(
            bridge: &mut SignalBridge,
            peer: &str,
            plaintext: &[u8],
        ) -> Result<Vec<u8>>;

        fn decrypt_message(
            bridge: &mut SignalBridge,
            peer: &str,
            ciphertext: &[u8],
        ) -> Result<Vec<u8>>;

        fn establish_session(bridge: &mut SignalBridge, peer: &str, bundle: &[u8]) -> Result<()>;

        fn generate_pre_key_bundle(bridge: &mut SignalBridge) -> Result<Vec<u8>>;

        fn clear_peer_session(bridge: &mut SignalBridge, peer: &str) -> Result<()>;

        fn clear_all_sessions(bridge: &mut SignalBridge) -> Result<()>;

        fn reset_identity(bridge: &mut SignalBridge) -> Result<()>;

        fn generate_node_fingerprint(bridge: &mut SignalBridge) -> Result<String>;

        fn sign_nostr_event(bridge: &mut SignalBridge, event_json: &str) -> Result<String>;

        fn create_and_sign_encrypted_message(
            bridge: &mut SignalBridge,
            session_id: &str,
            encrypted_content: &str,
            timestamp: u64,
            project_version: &str,
        ) -> Result<String>;

        fn create_subscription_for_self(
            bridge: &mut SignalBridge,
            subscription_id: &str,
        ) -> Result<String>;

        fn lookup_contact(bridge: &mut SignalBridge, identifier: &str) -> Result<ContactInfo>;

        fn assign_contact_alias(
            bridge: &mut SignalBridge,
            identifier: &str,
            new_alias: &str,
        ) -> Result<()>;

        fn list_contacts(bridge: &mut SignalBridge) -> Result<Vec<ContactInfo>>;

        fn generate_prekey_bundle_announcement(
            bridge: &mut SignalBridge,
            project_version: &str,
        ) -> Result<String>;

        fn add_contact_and_establish_session(
            bridge: &mut SignalBridge,
            bundle_bytes: &[u8],
            user_alias: &str,
        ) -> Result<String>;

        fn add_contact_and_establish_session_from_base64(
            bridge: &mut SignalBridge,
            bundle_base64: &str,
            user_alias: &str,
        ) -> Result<String>;

        fn extract_rdx_from_bundle(
            bridge: &mut SignalBridge,
            bundle_bytes: &[u8],
        ) -> Result<String>;

        fn extract_rdx_from_bundle_base64(
            bridge: &mut SignalBridge,
            bundle_base64: &str,
        ) -> Result<String>;
    }
}

pub fn new_signal_bridge(db_path: &str) -> Result<Box<SignalBridge>, Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;
    let bridge = rt
        .block_on(SignalBridge::new(db_path))
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;
    Ok(Box::new(bridge))
}

pub fn encrypt_message(
    bridge: &mut SignalBridge,
    peer: &str,
    plaintext: &[u8],
) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;
    rt.block_on(bridge.encrypt_message(peer, plaintext))
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })
}

pub fn decrypt_message(
    bridge: &mut SignalBridge,
    peer: &str,
    ciphertext: &[u8],
) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;
    rt.block_on(bridge.decrypt_message(peer, ciphertext))
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })
}

pub fn establish_session(
    bridge: &mut SignalBridge,
    peer: &str,
    bundle: &[u8],
) -> Result<(), Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;
    rt.block_on(bridge.establish_session(peer, bundle))
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })
}

pub fn generate_pre_key_bundle(
    bridge: &mut SignalBridge,
) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;
    rt.block_on(bridge.generate_pre_key_bundle())
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })
}

pub fn clear_peer_session(
    bridge: &mut SignalBridge,
    peer: &str,
) -> Result<(), Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;
    rt.block_on(bridge.clear_peer_session(peer))
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })
}

pub fn clear_all_sessions(bridge: &mut SignalBridge) -> Result<(), Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;
    rt.block_on(bridge.clear_all_sessions())
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })
}

pub fn reset_identity(bridge: &mut SignalBridge) -> Result<(), Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;
    rt.block_on(bridge.reset_identity())
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })
}

pub fn generate_node_fingerprint(
    bridge: &mut SignalBridge,
) -> Result<String, Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;
    rt.block_on(bridge.generate_node_fingerprint())
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })
}

pub fn sign_nostr_event(
    bridge: &mut SignalBridge,
    event_json: &str,
) -> Result<String, Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;

    let mut event: serde_json::Value = serde_json::from_str(event_json)
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;

    let created_at = event["created_at"].as_u64().ok_or("Missing created_at")?;
    let kind = event["kind"].as_u64().ok_or("Missing kind")? as u32;
    let content = event["content"].as_str().ok_or("Missing content")?;

    let tags_array = event["tags"].as_array().ok_or("Missing tags")?;
    let tags: Vec<Vec<String>> = tags_array
        .iter()
        .map(|tag| {
            tag.as_array()
                .unwrap_or(&vec![])
                .iter()
                .map(|v| v.as_str().unwrap_or("").to_string())
                .collect()
        })
        .collect();

    let (pubkey, event_id, signature) = rt
        .block_on(bridge.sign_nostr_event(created_at, kind, tags, content))
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;

    event["pubkey"] = serde_json::Value::String(pubkey);
    event["id"] = serde_json::Value::String(event_id);
    event["sig"] = serde_json::Value::String(signature);

    serde_json::to_string(&event).map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })
}

pub fn create_and_sign_encrypted_message(
    bridge: &mut SignalBridge,
    session_id: &str,
    encrypted_content: &str,
    timestamp: u64,
    project_version: &str,
) -> Result<String, Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;

    // Derive recipient's Nostr pubkey from session
    let recipient_pubkey_bytes = rt
        .block_on(bridge.derive_peer_nostr_key(session_id))
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;
    let recipient_pubkey = hex::encode(&recipient_pubkey_bytes);

    // Create the Nostr event JSON
    let event_json = serde_json::json!({
        "id": "",
        "pubkey": "",
        "created_at": timestamp,
        "kind": 40001,
        "tags": [
            ["p", recipient_pubkey],
            ["radix_peer", session_id],
            ["radix_version", project_version]
        ],
        "content": encrypted_content,
        "sig": ""
    });

    // Sign the event
    let event_json_str = serde_json::to_string(&event_json)
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;

    // Use the existing signing function
    sign_nostr_event(bridge, &event_json_str)
}

pub fn create_subscription_for_self(
    bridge: &mut SignalBridge,
    subscription_id: &str,
) -> Result<String, Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })?;
    rt.block_on(bridge.create_subscription_for_self(subscription_id))
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })
}

pub fn lookup_contact(
    bridge: &mut SignalBridge,
    identifier: &str,
) -> Result<ffi::ContactInfo, Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()?;
    let contact = rt.block_on(bridge.lookup_contact(identifier))?;
    Ok(ffi::ContactInfo {
        rdx_fingerprint: contact.rdx_fingerprint,
        nostr_pubkey: contact.nostr_pubkey,
        user_alias: contact.user_alias.unwrap_or_default(),
        has_active_session: contact.has_active_session,
    })
}

pub fn assign_contact_alias(
    bridge: &mut SignalBridge,
    identifier: &str,
    new_alias: &str,
) -> Result<(), Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()?;
    rt.block_on(bridge.assign_contact_alias(identifier, new_alias))
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })
}

pub fn list_contacts(
    bridge: &mut SignalBridge,
) -> Result<Vec<ffi::ContactInfo>, Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()?;
    let contacts = rt.block_on(bridge.list_contacts())?;
    Ok(contacts
        .into_iter()
        .map(|c| ffi::ContactInfo {
            rdx_fingerprint: c.rdx_fingerprint,
            nostr_pubkey: c.nostr_pubkey,
            user_alias: c.user_alias.unwrap_or_default(),
            has_active_session: c.has_active_session,
        })
        .collect())
}

pub fn generate_prekey_bundle_announcement(
    bridge: &mut SignalBridge,
    project_version: &str,
) -> Result<String, Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()?;
    rt.block_on(bridge.generate_prekey_bundle_announcement(project_version))
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })
}

pub fn add_contact_and_establish_session(
    bridge: &mut SignalBridge,
    bundle_bytes: &[u8],
    user_alias: &str,
) -> Result<String, Box<dyn std::error::Error>> {
    let rt = tokio::runtime::Runtime::new()?;
    let alias = if user_alias.is_empty() {
        None
    } else {
        Some(user_alias)
    };
    rt.block_on(bridge.add_contact_and_establish_session(bundle_bytes, alias))
        .map_err(|e| -> Box<dyn std::error::Error> { Box::new(e) })
}

pub fn add_contact_and_establish_session_from_base64(
    bridge: &mut SignalBridge,
    bundle_base64: &str,
    user_alias: &str,
) -> Result<String, Box<dyn std::error::Error>> {
    use base64::Engine;
    let bundle_bytes = base64::engine::general_purpose::STANDARD
        .decode(bundle_base64)
        .map_err(|e| -> Box<dyn std::error::Error> {
            Box::new(SignalBridgeError::Protocol(format!(
                "Base64 decode error: {}",
                e
            )))
        })?;

    add_contact_and_establish_session(bridge, &bundle_bytes, user_alias)
}

pub fn extract_rdx_from_bundle(
    _bridge: &mut SignalBridge,
    bundle_bytes: &[u8],
) -> Result<String, Box<dyn std::error::Error>> {
    use libsignal_protocol::IdentityKey;

    let bundle: SerializablePreKeyBundle =
        bincode::deserialize(bundle_bytes).map_err(|e| -> Box<dyn std::error::Error> {
            Box::new(SignalBridgeError::InvalidInput(format!(
                "Failed to deserialize bundle: {}",
                e
            )))
        })?;

    let identity_key =
        IdentityKey::decode(&bundle.identity_key).map_err(|e| -> Box<dyn std::error::Error> {
            Box::new(SignalBridgeError::Protocol(e.to_string()))
        })?;

    Ok(ContactManager::generate_identity_fingerprint_from_key(
        &identity_key,
    ))
}

pub fn extract_rdx_from_bundle_base64(
    bridge: &mut SignalBridge,
    bundle_base64: &str,
) -> Result<String, Box<dyn std::error::Error>> {
    use base64::Engine;
    let bundle_bytes = base64::engine::general_purpose::STANDARD
        .decode(bundle_base64)
        .map_err(|e| -> Box<dyn std::error::Error> {
            Box::new(SignalBridgeError::Protocol(format!(
                "Base64 decode error: {}",
                e
            )))
        })?;

    extract_rdx_from_bundle(bridge, &bundle_bytes)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{SystemTime, UNIX_EPOCH};

    #[test]
    fn test_libsignal_basic_types() {
        let device_id = DeviceId::new(1).expect("Valid device ID");
        let protocol_address = ProtocolAddress::new("test_device".to_string(), device_id);
        assert_eq!(protocol_address.name(), "test_device");
        assert_eq!(protocol_address.device_id(), device_id);
    }

    #[tokio::test]
    async fn test_nostr_key_derivation_deterministic() {
        let temp_dir = std::env::temp_dir();
        let db_path = temp_dir.join("test_nostr_derivation.db");
        let mut bridge = SignalBridge::new(db_path.to_str().unwrap()).await.unwrap();

        let nostr_keypair1 = bridge.derive_nostr_keypair().await.unwrap();
        let nostr_keypair2 = bridge.derive_nostr_keypair().await.unwrap();

        assert_eq!(nostr_keypair1.secret_key(), nostr_keypair2.secret_key());
        assert_eq!(nostr_keypair1.public_key(), nostr_keypair2.public_key());

        let _ = std::fs::remove_file(&db_path);
    }

    #[tokio::test]
    async fn test_peer_nostr_key_derivation() {
        let temp_dir = std::env::temp_dir();
        let alice_db = temp_dir.join("test_alice_nostr.db");
        let bob_db = temp_dir.join("test_bob_nostr.db");

        let mut alice = SignalBridge::new(alice_db.to_str().unwrap()).await.unwrap();
        let mut bob = SignalBridge::new(bob_db.to_str().unwrap()).await.unwrap();

        let bob_bundle = bob.generate_pre_key_bundle().await.unwrap();
        let bob_rdx = alice
            .add_contact_and_establish_session(&bob_bundle, Some("bob"))
            .await
            .unwrap();

        let bob_nostr_key = alice.derive_peer_nostr_key(&bob_rdx).await.unwrap();

        let bob_actual_nostr = bob.derive_nostr_keypair().await.unwrap();
        assert_eq!(
            bob_nostr_key,
            bob_actual_nostr.public_key().to_bytes().to_vec()
        );

        let _ = std::fs::remove_file(&alice_db);
        let _ = std::fs::remove_file(&bob_db);
    }

    #[test]
    fn test_error_types_creation() {
        let storage_error = SignalBridgeError::Storage("Database connection failed".to_string());
        assert_eq!(
            storage_error.to_string(),
            "Storage error: Database connection failed"
        );

        let protocol_error = SignalBridgeError::Protocol("Invalid device ID".to_string());
        assert_eq!(
            protocol_error.to_string(),
            "Signal Protocol error: Invalid device ID"
        );

        let serialization_error =
            SignalBridgeError::Serialization("Invalid data format".to_string());
        assert_eq!(
            serialization_error.to_string(),
            "Serialization error: Invalid data format"
        );

        let invalid_input_error = SignalBridgeError::InvalidInput("Empty peer name".to_string());
        assert_eq!(
            invalid_input_error.to_string(),
            "Invalid input: Empty peer name"
        );

        let session_not_found_error = SignalBridgeError::SessionNotFound(
            "Establish a session with alice before sending messages".to_string(),
        );
        assert_eq!(
            session_not_found_error.to_string(),
            "Establish a session with alice before sending messages"
        );

        let schema_error = SignalBridgeError::SchemaVersionTooOld;
        assert_eq!(
            schema_error.to_string(),
            "Update your database to a newer schema version"
        );
    }

    #[test]
    fn test_error_from_conversions() {
        let device_result = DeviceId::new(0);
        if let Err(protocol_err) = device_result {
            let bridge_error = SignalBridgeError::Protocol(protocol_err.to_string());
            assert!(matches!(bridge_error, SignalBridgeError::Protocol(_)));
        }

        let invalid_data = vec![0xFF, 0xFF, 0xFF];
        let bincode_result: Result<String, bincode::Error> = bincode::deserialize(&invalid_data);
        if let Err(bincode_err) = bincode_result {
            let bridge_error: SignalBridgeError = bincode_err.into();
            assert!(matches!(bridge_error, SignalBridgeError::Serialization(_)));
        }

        use std::time::{Duration, SystemTime};
        let bad_time = SystemTime::UNIX_EPOCH - Duration::from_secs(1);
        let time_result = bad_time.duration_since(SystemTime::UNIX_EPOCH);
        if let Err(time_err) = time_result {
            let bridge_error: SignalBridgeError = time_err.into();
            assert!(matches!(bridge_error, SignalBridgeError::Protocol(_)));
        }
    }

    #[tokio::test]
    async fn test_crypto_handler_exists() -> Result<(), Box<dyn std::error::Error>> {
        use crate::SignalBridge;
        use std::env;

        let temp_dir = env::temp_dir();
        let db_path = temp_dir.join("test_crypto_handler.db");
        let db_path_str = db_path.to_str().unwrap();

        let _handler = SignalBridge::new(db_path_str).await?;
        Ok(())
    }

    #[tokio::test]
    async fn test_crypto_handler_alice_bob_integration() -> Result<(), Box<dyn std::error::Error>> {
        use crate::SignalBridge;
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let alice_db_path = temp_dir.join(format!("test_alice_{}_{}.db", process_id, timestamp));
        let alice_db_str = alice_db_path.to_str().unwrap();
        let mut alice = SignalBridge::new(alice_db_str).await?;

        let bob_db_path = temp_dir.join(format!("test_bob_{}_{}.db", process_id, timestamp));
        let bob_db_str = bob_db_path.to_str().unwrap();
        let mut bob = SignalBridge::new(bob_db_str).await?;

        let bob_bundle = bob.generate_pre_key_bundle().await?;
        let alice_bundle = alice.generate_pre_key_bundle().await?;

        let bob_rdx = alice
            .add_contact_and_establish_session(&bob_bundle, Some("bob"))
            .await?;
        let alice_rdx = bob
            .add_contact_and_establish_session(&alice_bundle, Some("alice"))
            .await?;

        let plaintext = b"Hello Bob! This is Alice using SignalBridge.";
        let ciphertext = alice.encrypt_message(&bob_rdx, plaintext).await?;
        let decrypted = bob.decrypt_message(&alice_rdx, &ciphertext).await?;

        assert_eq!(plaintext.as_slice(), decrypted.as_slice());

        Ok(())
    }

    #[tokio::test]
    async fn test_signalbridge_persistence_across_instantiations(
    ) -> Result<(), Box<dyn std::error::Error>> {
        use crate::SignalBridge;
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let alice_db_path = temp_dir.join(format!(
            "test_persistence_alice_{}_{}.db",
            process_id, timestamp
        ));
        let alice_db_str = alice_db_path.to_str().unwrap();
        let bob_db_path = temp_dir.join(format!(
            "test_persistence_bob_{}_{}.db",
            process_id, timestamp
        ));
        let bob_db_str = bob_db_path.to_str().unwrap();

        let original_plaintext = b"Hello Bob! This is Alice testing persistence.";
        let (ciphertext, bob_rdx, alice_rdx) = {
            let mut alice = SignalBridge::new(alice_db_str).await?;
            let mut bob = SignalBridge::new(bob_db_str).await?;

            let bob_bundle = bob.generate_pre_key_bundle().await?;
            let alice_bundle = alice.generate_pre_key_bundle().await?;

            let bob_rdx = alice
                .add_contact_and_establish_session(&bob_bundle, Some("bob"))
                .await?;
            let alice_rdx = bob
                .add_contact_and_establish_session(&alice_bundle, Some("alice"))
                .await?;

            let ciphertext = alice.encrypt_message(&bob_rdx, original_plaintext).await?;
            (ciphertext, bob_rdx, alice_rdx)
        };

        {
            let mut alice_reopened = SignalBridge::new(alice_db_str).await?;

            let second_message = b"Second message after restart";
            let second_ciphertext = alice_reopened
                .encrypt_message(&bob_rdx, second_message)
                .await?;

            assert!(!second_ciphertext.is_empty());
            assert_ne!(ciphertext, second_ciphertext);
        }

        {
            let mut bob_reopened = SignalBridge::new(bob_db_str).await?;

            let decrypted1 = bob_reopened
                .decrypt_message(&alice_rdx, &ciphertext)
                .await?;
            assert_eq!(decrypted1, original_plaintext);
        }

        Ok(())
    }

    #[tokio::test]
    async fn test_clear_peer_session() -> Result<(), Box<dyn std::error::Error>> {
        use crate::SignalBridge;
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let alice_db_path = temp_dir.join(format!(
            "test_clear_peer_alice_{}_{}.db",
            process_id, timestamp
        ));
        let alice_db_str = alice_db_path.to_str().unwrap();
        let bob_db_path = temp_dir.join(format!(
            "test_clear_peer_bob_{}_{}.db",
            process_id, timestamp
        ));
        let bob_db_str = bob_db_path.to_str().unwrap();

        let mut alice = SignalBridge::new(alice_db_str).await?;
        let mut bob = SignalBridge::new(bob_db_str).await?;

        let bob_bundle = bob.generate_pre_key_bundle().await?;
        let bob_rdx = alice
            .add_contact_and_establish_session(&bob_bundle, Some("bob"))
            .await?;

        let message = b"Test message";
        let _ciphertext = alice.encrypt_message(&bob_rdx, message).await?;

        alice.clear_peer_session(&bob_rdx).await?;

        let result = alice.encrypt_message(&bob_rdx, message).await;
        assert!(
            result.is_err(),
            "Encryption should fail after clearing peer session"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_clear_all_sessions() -> Result<(), Box<dyn std::error::Error>> {
        use crate::SignalBridge;
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let alice_db_path = temp_dir.join(format!(
            "test_clear_all_alice_{}_{}.db",
            process_id, timestamp
        ));
        let alice_db_str = alice_db_path.to_str().unwrap();
        let bob_db_path = temp_dir.join(format!(
            "test_clear_all_bob_{}_{}.db",
            process_id, timestamp
        ));
        let bob_db_str = bob_db_path.to_str().unwrap();
        let charlie_db_path = temp_dir.join(format!(
            "test_clear_all_charlie_{}_{}.db",
            process_id, timestamp
        ));
        let charlie_db_str = charlie_db_path.to_str().unwrap();

        let mut alice = SignalBridge::new(alice_db_str).await?;
        let mut bob = SignalBridge::new(bob_db_str).await?;
        let mut charlie = SignalBridge::new(charlie_db_str).await?;

        let bob_bundle = bob.generate_pre_key_bundle().await?;
        let charlie_bundle = charlie.generate_pre_key_bundle().await?;

        let bob_rdx = alice
            .add_contact_and_establish_session(&bob_bundle, Some("bob"))
            .await?;
        let charlie_rdx = alice
            .add_contact_and_establish_session(&charlie_bundle, Some("charlie"))
            .await?;

        let message = b"Test message";
        let _ciphertext1 = alice.encrypt_message(&bob_rdx, message).await?;
        let _ciphertext2 = alice.encrypt_message(&charlie_rdx, message).await?;

        alice.clear_all_sessions().await?;

        let result1 = alice.encrypt_message(&bob_rdx, message).await;
        let result2 = alice.encrypt_message(&charlie_rdx, message).await;
        assert!(
            result1.is_err(),
            "Encryption to Bob should fail after clearing all sessions"
        );
        assert!(
            result2.is_err(),
            "Encryption to Charlie should fail after clearing all sessions"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_reset_identity() -> Result<(), Box<dyn std::error::Error>> {
        use crate::SignalBridge;
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let alice_db_path = temp_dir.join(format!(
            "test_reset_identity_alice_{}_{}.db",
            process_id, timestamp
        ));
        let alice_db_str = alice_db_path.to_str().unwrap();
        let bob_db_path = temp_dir.join(format!(
            "test_reset_identity_bob_{}_{}.db",
            process_id, timestamp
        ));
        let bob_db_str = bob_db_path.to_str().unwrap();

        let (original_alice_bundle, bob_rdx) = {
            let mut alice = SignalBridge::new(alice_db_str).await?;
            let mut bob = SignalBridge::new(bob_db_str).await?;

            let bob_bundle = bob.generate_pre_key_bundle().await?;
            let bob_rdx = alice
                .add_contact_and_establish_session(&bob_bundle, Some("bob"))
                .await?;

            let alice_bundle = alice.generate_pre_key_bundle().await?;
            (alice_bundle, bob_rdx)
        };

        {
            let mut alice = SignalBridge::new(alice_db_str).await?;
            alice.reset_identity().await?;

            let new_alice_bundle = alice.generate_pre_key_bundle().await?;
            assert_ne!(
                original_alice_bundle, new_alice_bundle,
                "Alice's identity should be different after reset"
            );

            let message = b"Test message";
            let result = alice.encrypt_message(&bob_rdx, message).await;
            assert!(
                result.is_err(),
                "Encryption should fail after identity reset"
            );
        }

        Ok(())
    }

    #[tokio::test]
    async fn test_encrypt_message_empty_peer_name() {
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let db_path = temp_dir.join(format!("test_empty_peer_{}_{}.db", process_id, timestamp));
        let db_path_str = db_path.to_str().unwrap();
        let mut bridge = SignalBridge::new(db_path_str)
            .await
            .expect("Failed to create bridge");

        let result = bridge.encrypt_message("", b"test message").await;
        assert!(result.is_err());
        if let Err(SignalBridgeError::InvalidInput(msg)) = result {
            assert_eq!(msg, "Specify a peer name");
        } else {
            panic!("Expected InvalidInput error for empty peer name");
        }
    }

    #[tokio::test]
    async fn test_decrypt_message_empty_peer_name() {
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let db_path = temp_dir.join(format!(
            "test_empty_peer_decrypt_{}_{}.db",
            process_id, timestamp
        ));
        let db_path_str = db_path.to_str().unwrap();
        let mut bridge = SignalBridge::new(db_path_str)
            .await
            .expect("Failed to create bridge");

        let result = bridge.decrypt_message("", b"fake_ciphertext").await;
        assert!(result.is_err());
        if let Err(SignalBridgeError::InvalidInput(msg)) = result {
            assert_eq!(msg, "Specify a peer name");
        } else {
            panic!("Expected InvalidInput error for empty peer name");
        }
    }

    #[tokio::test]
    async fn test_decrypt_message_empty_ciphertext() {
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let db_path = temp_dir.join(format!(
            "test_empty_ciphertext_{}_{}.db",
            process_id, timestamp
        ));
        let db_path_str = db_path.to_str().unwrap();
        let mut bridge = SignalBridge::new(db_path_str)
            .await
            .expect("Failed to create bridge");

        let result = bridge.decrypt_message("alice", &[]).await;
        assert!(result.is_err());
        if let Err(SignalBridgeError::InvalidInput(msg)) = result {
            assert_eq!(msg, "Provide a message to decrypt");
        } else {
            panic!("Expected InvalidInput error for empty ciphertext");
        }
    }

    #[tokio::test]
    async fn test_establish_session_empty_peer_name() {
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let db_path = temp_dir.join(format!(
            "test_empty_peer_session_{}_{}.db",
            process_id, timestamp
        ));
        let db_path_str = db_path.to_str().unwrap();
        let mut bridge = SignalBridge::new(db_path_str)
            .await
            .expect("Failed to create bridge");

        let result = bridge.establish_session("", b"fake_bundle").await;
        assert!(result.is_err());
        if let Err(SignalBridgeError::InvalidInput(msg)) = result {
            assert_eq!(msg, "Specify a peer name");
        } else {
            panic!("Expected InvalidInput error for empty peer name");
        }
    }

    #[tokio::test]
    async fn test_establish_session_empty_bundle() {
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let db_path = temp_dir.join(format!("test_empty_bundle_{}_{}.db", process_id, timestamp));
        let db_path_str = db_path.to_str().unwrap();
        let mut bridge = SignalBridge::new(db_path_str)
            .await
            .expect("Failed to create bridge");

        let result = bridge.establish_session("alice", &[]).await;
        assert!(result.is_err());
        if let Err(SignalBridgeError::InvalidInput(msg)) = result {
            assert_eq!(msg, "Provide a pre-key bundle from the peer");
        } else {
            panic!("Expected InvalidInput error for empty pre-key bundle");
        }
    }

    #[tokio::test]
    async fn test_clear_peer_session_empty_peer_name() {
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let db_path = temp_dir.join(format!(
            "test_clear_empty_peer_{}_{}.db",
            process_id, timestamp
        ));
        let db_path_str = db_path.to_str().unwrap();
        let mut bridge = SignalBridge::new(db_path_str)
            .await
            .expect("Failed to create bridge");

        let result = bridge.clear_peer_session("").await;
        assert!(result.is_err());
        if let Err(SignalBridgeError::InvalidInput(msg)) = result {
            assert_eq!(msg, "Specify a peer name");
        } else {
            panic!("Expected InvalidInput error for empty peer name");
        }
    }

    #[tokio::test]
    async fn test_encrypt_message_session_not_found() {
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let db_path = temp_dir.join(format!(
            "test_session_not_found_{}_{}.db",
            process_id, timestamp
        ));
        let db_path_str = db_path.to_str().unwrap();
        let mut bridge = SignalBridge::new(db_path_str)
            .await
            .expect("Failed to create bridge");

        let result = bridge
            .encrypt_message("unknown_peer", b"test message")
            .await;
        assert!(result.is_err());
        if let Err(SignalBridgeError::SessionNotFound(msg)) = result {
            assert_eq!(
                msg,
                "Establish a session with unknown_peer before sending messages"
            );
        } else {
            panic!("Expected SessionNotFound error for unknown peer");
        }
    }

    #[tokio::test]
    async fn test_decrypt_message_malformed_ciphertext() {
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let db_path = temp_dir.join(format!(
            "test_malformed_ciphertext_{}_{}.db",
            process_id, timestamp
        ));
        let db_path_str = db_path.to_str().unwrap();
        let mut bridge = SignalBridge::new(db_path_str)
            .await
            .expect("Failed to create bridge");

        let malformed_data = vec![0x01, 0x02, 0x03, 0x04];
        let result = bridge.decrypt_message("alice", &malformed_data).await;
        assert!(result.is_err());
        if let Err(SignalBridgeError::Serialization(msg)) = result {
            assert_eq!(msg, "Provide a valid Signal Protocol message");
        } else {
            panic!("Expected Serialization error for malformed ciphertext");
        }
    }

    #[tokio::test]
    async fn test_establish_session_malformed_bundle() {
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let db_path = temp_dir.join(format!(
            "test_malformed_bundle_{}_{}.db",
            process_id, timestamp
        ));
        let db_path_str = db_path.to_str().unwrap();
        let mut bridge = SignalBridge::new(db_path_str)
            .await
            .expect("Failed to create bridge");

        let malformed_bundle = vec![0xFF, 0xFE, 0xFD, 0xFC];
        let result = bridge.establish_session("alice", &malformed_bundle).await;
        assert!(result.is_err());
        if let Err(SignalBridgeError::Serialization(msg)) = result {
            assert_eq!(msg, "io error: unexpected end of file");
        } else {
            panic!("Expected Serialization error for malformed bundle");
        }
    }

    #[tokio::test]
    async fn test_sign_nostr_event() -> Result<(), Box<dyn std::error::Error>> {
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let db_path = temp_dir.join(format!("test_sign_nostr_{}_{}.db", process_id, timestamp));
        let db_path_str = db_path.to_str().unwrap();
        let mut bridge = SignalBridge::new(db_path_str).await?;

        let _event_json = r#"{
            "id": "",
            "pubkey": "",
            "created_at": 1234567890,
            "kind": 40001,
            "tags": [
                ["p", "recipient_pubkey"],
                ["radix_peer", "session_id"]
            ],
            "content": "encrypted_content_here",
            "sig": ""
        }"#;

        let result = bridge
            .sign_nostr_event(
                1234567890,
                40001,
                vec![
                    vec!["p".to_string(), "recipient_pubkey".to_string()],
                    vec!["radix_peer".to_string(), "session_id".to_string()],
                ],
                "encrypted_content_here",
            )
            .await;
        assert!(result.is_ok());

        let (pubkey, event_id, signature) = result.unwrap();
        assert!(!pubkey.is_empty());
        assert!(!event_id.is_empty());
        assert!(!signature.is_empty());
        assert_eq!(event_id.len(), 64); // SHA256 hex should be 64 chars
        assert_eq!(pubkey.len(), 64); // Nostr pubkey hex should be 64 chars

        let keys = bridge.derive_nostr_keypair().await?;
        assert_eq!(pubkey, keys.public_key().to_hex());

        Ok(())
    }

    #[tokio::test]
    async fn test_error_message_formatting() {
        let storage_error = SignalBridgeError::Storage("Database locked".to_string());
        assert_eq!(storage_error.to_string(), "Storage error: Database locked");

        let protocol_error = SignalBridgeError::Protocol("Invalid signature".to_string());
        assert_eq!(
            protocol_error.to_string(),
            "Signal Protocol error: Invalid signature"
        );

        let serialization_error = SignalBridgeError::Serialization("Invalid format".to_string());
        assert_eq!(
            serialization_error.to_string(),
            "Serialization error: Invalid format"
        );

        let invalid_input_error = SignalBridgeError::InvalidInput("Name too long".to_string());
        assert_eq!(
            invalid_input_error.to_string(),
            "Invalid input: Name too long"
        );

        let session_not_found_error = SignalBridgeError::SessionNotFound(
            "Establish a session with bob before sending messages".to_string(),
        );
        assert_eq!(
            session_not_found_error.to_string(),
            "Establish a session with bob before sending messages"
        );

        let schema_error = SignalBridgeError::SchemaVersionTooOld;
        assert_eq!(
            schema_error.to_string(),
            "Update your database to a newer schema version"
        );
    }

    #[tokio::test]
    async fn test_add_contact_and_establish_session_returns_rdx() {
        let temp_dir = std::env::temp_dir();
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let alice_db = temp_dir.join(format!("test_contact_alice_{}.db", timestamp));
        let bob_db = temp_dir.join(format!("test_contact_bob_{}.db", timestamp));

        let mut alice = SignalBridge::new(alice_db.to_str().unwrap()).await.unwrap();
        let mut bob = SignalBridge::new(bob_db.to_str().unwrap()).await.unwrap();

        let bob_bundle = bob.generate_pre_key_bundle().await.unwrap();

        let bob_rdx = alice
            .add_contact_and_establish_session(&bob_bundle, None)
            .await
            .unwrap();
        assert!(bob_rdx.starts_with("RDX:"));

        let _ = std::fs::remove_file(&alice_db);
        let _ = std::fs::remove_file(&bob_db);
    }

    #[tokio::test]
    async fn test_lookup_contact_by_rdx() {
        let temp_dir = std::env::temp_dir();
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let alice_db = temp_dir.join(format!("test_lookup_alice_{}.db", timestamp));
        let bob_db = temp_dir.join(format!("test_lookup_bob_{}.db", timestamp));

        let mut alice = SignalBridge::new(alice_db.to_str().unwrap()).await.unwrap();
        let mut bob = SignalBridge::new(bob_db.to_str().unwrap()).await.unwrap();

        let bob_bundle = bob.generate_pre_key_bundle().await.unwrap();
        let bob_rdx = alice
            .add_contact_and_establish_session(&bob_bundle, None)
            .await
            .unwrap();

        let contact = alice.lookup_contact(&bob_rdx).await.unwrap();
        assert_eq!(contact.rdx_fingerprint, bob_rdx);
        assert!(contact.user_alias.is_none());
        assert!(contact.has_active_session);

        let _ = std::fs::remove_file(&alice_db);
        let _ = std::fs::remove_file(&bob_db);
    }

    #[tokio::test]
    async fn test_assign_and_lookup_contact_by_alias() {
        let temp_dir = std::env::temp_dir();
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let alice_db = temp_dir.join(format!("test_alias_alice_{}.db", timestamp));
        let bob_db = temp_dir.join(format!("test_alias_bob_{}.db", timestamp));

        let mut alice = SignalBridge::new(alice_db.to_str().unwrap()).await.unwrap();
        let mut bob = SignalBridge::new(bob_db.to_str().unwrap()).await.unwrap();

        let bob_bundle = bob.generate_pre_key_bundle().await.unwrap();
        let bob_rdx = alice
            .add_contact_and_establish_session(&bob_bundle, None)
            .await
            .unwrap();

        let contact_before = alice.lookup_contact(&bob_rdx).await.unwrap();
        assert!(contact_before.user_alias.is_none());

        alice.assign_contact_alias(&bob_rdx, "bob").await.unwrap();

        let contact_by_alias = alice.lookup_contact("bob").await.unwrap();
        assert_eq!(contact_by_alias.rdx_fingerprint, bob_rdx);
        assert_eq!(contact_by_alias.user_alias, Some("bob".to_string()));

        let _ = std::fs::remove_file(&alice_db);
        let _ = std::fs::remove_file(&bob_db);
    }

    #[tokio::test]
    async fn test_list_contacts() {
        let temp_dir = std::env::temp_dir();
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let alice_db = temp_dir.join(format!("test_list_alice_{}.db", timestamp));
        let bob_db = temp_dir.join(format!("test_list_bob_{}.db", timestamp));
        let charlie_db = temp_dir.join(format!("test_list_charlie_{}.db", timestamp));

        let mut alice = SignalBridge::new(alice_db.to_str().unwrap()).await.unwrap();
        let mut bob = SignalBridge::new(bob_db.to_str().unwrap()).await.unwrap();
        let mut charlie = SignalBridge::new(charlie_db.to_str().unwrap())
            .await
            .unwrap();

        let bob_bundle = bob.generate_pre_key_bundle().await.unwrap();
        let charlie_bundle = charlie.generate_pre_key_bundle().await.unwrap();

        let bob_rdx = alice
            .add_contact_and_establish_session(&bob_bundle, Some("bob"))
            .await
            .unwrap();
        let charlie_rdx = alice
            .add_contact_and_establish_session(&charlie_bundle, None)
            .await
            .unwrap();

        let contacts = alice.list_contacts().await.unwrap();
        assert_eq!(contacts.len(), 2);

        let bob_contact = contacts
            .iter()
            .find(|c| c.rdx_fingerprint == bob_rdx)
            .unwrap();
        assert_eq!(bob_contact.user_alias, Some("bob".to_string()));

        let charlie_contact = contacts
            .iter()
            .find(|c| c.rdx_fingerprint == charlie_rdx)
            .unwrap();
        assert!(charlie_contact.user_alias.is_none());

        let _ = std::fs::remove_file(&alice_db);
        let _ = std::fs::remove_file(&bob_db);
        let _ = std::fs::remove_file(&charlie_db);
    }

    #[tokio::test]
    async fn test_alias_with_multiple_sessions() {
        let temp_dir = std::env::temp_dir();
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let alice_db = temp_dir.join(format!("test_multi_alias_alice_{}.db", timestamp));
        let bob1_db = temp_dir.join(format!("test_multi_alias_bob1_{}.db", timestamp));
        let bob2_db = temp_dir.join(format!("test_multi_alias_bob2_{}.db", timestamp));
        let bob3_db = temp_dir.join(format!("test_multi_alias_bob3_{}.db", timestamp));

        let mut alice = SignalBridge::new(alice_db.to_str().unwrap()).await.unwrap();
        let mut bob1 = SignalBridge::new(bob1_db.to_str().unwrap()).await.unwrap();
        let mut bob2 = SignalBridge::new(bob2_db.to_str().unwrap()).await.unwrap();
        let mut bob3 = SignalBridge::new(bob3_db.to_str().unwrap()).await.unwrap();

        let bob1_bundle = bob1.generate_pre_key_bundle().await.unwrap();
        let bob2_bundle = bob2.generate_pre_key_bundle().await.unwrap();
        let bob3_bundle = bob3.generate_pre_key_bundle().await.unwrap();

        let bob1_rdx = alice
            .add_contact_and_establish_session(&bob1_bundle, None)
            .await
            .unwrap();
        let bob2_rdx = alice
            .add_contact_and_establish_session(&bob2_bundle, None)
            .await
            .unwrap();
        let bob3_rdx = alice
            .add_contact_and_establish_session(&bob3_bundle, None)
            .await
            .unwrap();

        assert_ne!(bob1_rdx, bob2_rdx);
        assert_ne!(bob2_rdx, bob3_rdx);
        assert_ne!(bob1_rdx, bob3_rdx);

        alice.assign_contact_alias(&bob3_rdx, "bob").await.unwrap();

        let contact_by_alias = alice.lookup_contact("bob").await.unwrap();
        assert_eq!(contact_by_alias.rdx_fingerprint, bob3_rdx);
        assert_eq!(contact_by_alias.user_alias, Some("bob".to_string()));

        let contact1 = alice.lookup_contact(&bob1_rdx).await.unwrap();
        assert_eq!(contact1.rdx_fingerprint, bob1_rdx);
        assert!(contact1.user_alias.is_none());

        let contact2 = alice.lookup_contact(&bob2_rdx).await.unwrap();
        assert_eq!(contact2.rdx_fingerprint, bob2_rdx);
        assert!(contact2.user_alias.is_none());

        let _ = std::fs::remove_file(&alice_db);
        let _ = std::fs::remove_file(&bob1_db);
        let _ = std::fs::remove_file(&bob2_db);
        let _ = std::fs::remove_file(&bob3_db);
    }

    #[tokio::test]
    async fn test_reassigning_same_alias_to_different_contact() {
        let temp_dir = std::env::temp_dir();
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let alice_db = temp_dir.join(format!("test_reassign_alias_alice_{}.db", timestamp));
        let bob1_db = temp_dir.join(format!("test_reassign_alias_bob1_{}.db", timestamp));
        let bob2_db = temp_dir.join(format!("test_reassign_alias_bob2_{}.db", timestamp));

        let mut alice = SignalBridge::new(alice_db.to_str().unwrap()).await.unwrap();
        let mut bob1 = SignalBridge::new(bob1_db.to_str().unwrap()).await.unwrap();
        let mut bob2 = SignalBridge::new(bob2_db.to_str().unwrap()).await.unwrap();

        let bob1_bundle = bob1.generate_pre_key_bundle().await.unwrap();
        let bob2_bundle = bob2.generate_pre_key_bundle().await.unwrap();

        let bob1_rdx = alice
            .add_contact_and_establish_session(&bob1_bundle, None)
            .await
            .unwrap();
        let bob2_rdx = alice
            .add_contact_and_establish_session(&bob2_bundle, None)
            .await
            .unwrap();

        alice.assign_contact_alias(&bob1_rdx, "bob").await.unwrap();

        let contact = alice.lookup_contact("bob").await.unwrap();
        assert_eq!(contact.rdx_fingerprint, bob1_rdx);

        let result = alice.assign_contact_alias(&bob2_rdx, "bob").await;
        assert!(result.is_err(), "Should fail when alias is already taken");
        assert!(
            result.unwrap_err().to_string().contains("already assigned"),
            "Error message should indicate alias is already assigned"
        );

        let contact_still = alice.lookup_contact("bob").await.unwrap();
        assert_eq!(
            contact_still.rdx_fingerprint, bob1_rdx,
            "Alias 'bob' should still point to bob1"
        );

        let _ = std::fs::remove_file(&alice_db);
        let _ = std::fs::remove_file(&bob1_db);
        let _ = std::fs::remove_file(&bob2_db);
    }

    #[tokio::test]
    async fn test_generate_bundle_announcement_includes_rdx_tag() {
        let temp_dir = std::env::temp_dir();
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis();
        let db_path = temp_dir.join(format!("test_generate_announcement_{}.db", timestamp));

        let mut alice = SignalBridge::new(db_path.to_str().unwrap()).await.unwrap();

        let event_json = alice
            .generate_prekey_bundle_announcement("1.0.0-test")
            .await
            .unwrap();
        let event: serde_json::Value = serde_json::from_str(&event_json).unwrap();

        assert_eq!(event["kind"], 30078);
        assert!(event["content"].is_string());

        let tags = event["tags"].as_array().unwrap();
        let rdx_tag = tags
            .iter()
            .find(|t| t[0] == "rdx")
            .expect("RDX tag should be present");
        let rdx_value = rdx_tag[1].as_str().unwrap();
        assert!(rdx_value.starts_with("RDX:"));

        let version_tag = tags
            .iter()
            .find(|t| t[0] == "radix_version")
            .expect("Version tag should be present");
        assert_eq!(version_tag[1], "1.0.0-test");

        let _ = std::fs::remove_file(&db_path);
    }

    #[tokio::test]
    async fn test_add_contact_and_establish_session() {
        let temp_dir = std::env::temp_dir();
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis();
        let alice_db = temp_dir.join(format!("test_contact_session_alice_{}.db", timestamp));
        let bob_db = temp_dir.join(format!("test_contact_session_bob_{}.db", timestamp));

        let mut alice = SignalBridge::new(alice_db.to_str().unwrap()).await.unwrap();
        let mut bob = SignalBridge::new(bob_db.to_str().unwrap()).await.unwrap();

        let bob_bundle = bob.generate_pre_key_bundle().await.unwrap();

        let bob_rdx = alice
            .add_contact_and_establish_session(&bob_bundle, Some("bob"))
            .await
            .unwrap();

        assert!(bob_rdx.starts_with("RDX:"));

        let contact = alice.lookup_contact(&bob_rdx).await.unwrap();
        assert_eq!(contact.rdx_fingerprint, bob_rdx);
        assert_eq!(contact.user_alias, Some("bob".to_string()));
        assert!(contact.has_active_session);

        let plaintext = b"Hello Bob!";
        let ciphertext = alice.encrypt_message(&bob_rdx, plaintext).await.unwrap();
        assert!(!ciphertext.is_empty());

        let _ = std::fs::remove_file(&alice_db);
        let _ = std::fs::remove_file(&bob_db);
    }

    #[tokio::test]
    async fn test_extract_rdx_from_bundle_without_establishing_session() {
        let temp_dir = std::env::temp_dir();
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let alice_db = temp_dir.join(format!("test_extract_rdx_alice_{}.db", timestamp));
        let bob_db = temp_dir.join(format!("test_extract_rdx_bob_{}.db", timestamp));

        let mut alice = SignalBridge::new(alice_db.to_str().unwrap()).await.unwrap();
        let mut bob = SignalBridge::new(bob_db.to_str().unwrap()).await.unwrap();

        let bob_bundle = bob.generate_pre_key_bundle().await.unwrap();

        let extracted_rdx = extract_rdx_from_bundle(&mut alice, &bob_bundle).unwrap();

        assert!(extracted_rdx.starts_with("RDX:"));

        let result = alice.encrypt_message(&extracted_rdx, b"test").await;
        assert!(
            result.is_err(),
            "Should not be able to encrypt without establishing session"
        );

        let bob_rdx = alice
            .add_contact_and_establish_session(&bob_bundle, Some("bob"))
            .await
            .unwrap();

        assert_eq!(
            extracted_rdx, bob_rdx,
            "RDX extracted before session should match RDX after session establishment"
        );

        let _ = std::fs::remove_file(&alice_db);
        let _ = std::fs::remove_file(&bob_db);
    }

    #[tokio::test]
    async fn test_extract_rdx_from_bundle_base64() {
        let temp_dir = std::env::temp_dir();
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let alice_db = temp_dir.join(format!("test_extract_rdx_base64_alice_{}.db", timestamp));
        let bob_db = temp_dir.join(format!("test_extract_rdx_base64_bob_{}.db", timestamp));

        let mut alice = SignalBridge::new(alice_db.to_str().unwrap()).await.unwrap();
        let mut bob = SignalBridge::new(bob_db.to_str().unwrap()).await.unwrap();

        let bob_bundle = bob.generate_pre_key_bundle().await.unwrap();
        let bob_bundle_base64 = base64::engine::general_purpose::STANDARD.encode(&bob_bundle);

        let extracted_rdx = extract_rdx_from_bundle_base64(&mut alice, &bob_bundle_base64).unwrap();

        assert!(extracted_rdx.starts_with("RDX:"));

        let bob_rdx = alice
            .add_contact_and_establish_session(&bob_bundle, Some("bob"))
            .await
            .unwrap();

        assert_eq!(
            extracted_rdx, bob_rdx,
            "RDX extracted from base64 should match RDX after session establishment"
        );

        let _ = std::fs::remove_file(&alice_db);
        let _ = std::fs::remove_file(&bob_db);
    }
}
