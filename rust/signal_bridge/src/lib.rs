//! Signal Protocol integration for Radix Relay
//!
//! This crate provides a bridge between Radix Relay's C++ transport layer
//! and the official Signal Protocol Rust implementation for end-to-end encryption.

mod encryption_trait;
mod keys;
mod memory_storage;
mod session_trait;
mod sqlite_storage;
mod storage_trait;

#[cfg(test)]
mod session;

#[cfg(test)]
mod encryption;

use crate::sqlite_storage::SqliteStorage;
use crate::storage_trait::{
    ExtendedIdentityStore, ExtendedKyberPreKeyStore, ExtendedPreKeyStore, ExtendedSessionStore,
    ExtendedSignedPreKeyStore, ExtendedStorageOps, SignalStorageContainer,
};
use ::hkdf::Hkdf;
use libsignal_protocol::{
    CiphertextMessage, DeviceId, IdentityKeyPair, IdentityKeyStore, ProtocolAddress, SessionStore,
};
use nostr::{Keys, PublicKey, SecretKey};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct NodeIdentity {
    pub hostname: String,
    pub username: String,
    pub platform: String,
    pub mac_address: String,
    pub install_id: String,
}

pub struct NostrIdentity;

impl NostrIdentity {
    pub fn derive_from_signal_identity(
        identity_key_pair: &IdentityKeyPair,
    ) -> Result<Keys, SignalBridgeError> {
        let identity_public_key = identity_key_pair.identity_key();

        let hk = Hkdf::<sha2::Sha256>::new(None, &identity_public_key.serialize());
        let mut derived_key = [0u8; 32];
        hk.expand(b"radix_relay_nostr_derivation", &mut derived_key)
            .map_err(|_| SignalBridgeError::KeyDerivation("HKDF expansion failed".to_string()))?;

        let secret_key = SecretKey::from_slice(&derived_key)
            .map_err(|e| SignalBridgeError::KeyDerivation(format!("Invalid secret key: {}", e)))?;

        Ok(Keys::new(secret_key))
    }

    pub fn derive_public_key_from_peer_identity(
        peer_identity: &libsignal_protocol::IdentityKey,
    ) -> Result<PublicKey, SignalBridgeError> {
        let hk = Hkdf::<sha2::Sha256>::new(None, &peer_identity.serialize());
        let mut derived_key = [0u8; 32];
        hk.expand(b"radix_relay_nostr_derivation", &mut derived_key)
            .map_err(|_| SignalBridgeError::KeyDerivation("HKDF expansion failed".to_string()))?;

        let secret_key = SecretKey::from_slice(&derived_key)
            .map_err(|e| SignalBridgeError::KeyDerivation(format!("Invalid secret key: {}", e)))?;

        let full_keys = Keys::new(secret_key);
        Ok(full_keys.public_key())
    }
}

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

        Ok(Self { storage })
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

        let address = ProtocolAddress::new(
            peer.to_string(),
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

        let address = ProtocolAddress::new(
            peer.to_string(),
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
        let peer_identity = self
            .storage
            .identity_store()
            .get_identity(&ProtocolAddress::new(
                peer.to_string(),
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

    pub async fn generate_node_fingerprint(
        &mut self,
        node_identity: &NodeIdentity,
    ) -> Result<String, SignalBridgeError> {
        let identity_key_pair = self
            .storage
            .identity_store()
            .get_identity_key_pair()
            .await?;
        let registration_id = self
            .storage
            .identity_store()
            .get_local_registration_id()
            .await?;

        let mut hasher = Sha256::new();

        hasher.update(identity_key_pair.identity_key().serialize());
        hasher.update(registration_id.to_be_bytes());

        hasher.update(node_identity.hostname.as_bytes());
        hasher.update(node_identity.username.as_bytes());
        hasher.update(node_identity.platform.as_bytes());
        hasher.update(node_identity.mac_address.as_bytes());
        hasher.update(node_identity.install_id.as_bytes());

        hasher.update(b"radix-node-fingerprint");

        let result = hasher.finalize();
        Ok(format!("RDX:{:x}", result))
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

// CXX Bridge for C++ interop
// Note: CXX doesn't support async functions, so we use sync wrappers with internal Tokio runtime
#[cxx::bridge(namespace = "radix_relay")]
mod ffi {
    #[derive(Debug)]
    pub struct NodeIdentity {
        pub hostname: String,
        pub username: String,
        pub platform: String,
        pub mac_address: String,
        pub install_id: String,
    }

    extern "Rust" {
        type SignalBridge;

        // RAII: Single constructor handles all complexity
        fn new_signal_bridge(db_path: &str) -> Box<SignalBridge>;

        // Core crypto operations - sync wrappers for async functions
        fn encrypt_message(bridge: &mut SignalBridge, peer: &str, plaintext: &[u8]) -> Vec<u8>;

        fn decrypt_message(bridge: &mut SignalBridge, peer: &str, ciphertext: &[u8]) -> Vec<u8>;

        fn establish_session(bridge: &mut SignalBridge, peer: &str, bundle: &[u8]);

        fn generate_pre_key_bundle(bridge: &mut SignalBridge) -> Vec<u8>;

        // Session management
        fn clear_peer_session(bridge: &mut SignalBridge, peer: &str);

        fn clear_all_sessions(bridge: &mut SignalBridge);

        fn reset_identity(bridge: &mut SignalBridge);

        // Node fingerprint generation
        fn generate_node_fingerprint(bridge: &mut SignalBridge, identity: NodeIdentity) -> String;
        // Nostr key derivation
        fn derive_my_nostr_keypair(bridge: &mut SignalBridge) -> Vec<u8>;
        fn derive_peer_nostr_pubkey(bridge: &mut SignalBridge, peer: &str) -> Vec<u8>;
    }
}

// CXX Bridge implementation functions - sync wrappers for async methods
// Note: Panics on error for simplicity. C++ can catch exceptions.
pub fn new_signal_bridge(db_path: &str) -> Box<SignalBridge> {
    let rt = tokio::runtime::Runtime::new().expect("Failed to create tokio runtime");
    let bridge = rt
        .block_on(SignalBridge::new(db_path))
        .expect("Failed to create SignalBridge");
    Box::new(bridge)
}

pub fn encrypt_message(bridge: &mut SignalBridge, peer: &str, plaintext: &[u8]) -> Vec<u8> {
    let rt = tokio::runtime::Runtime::new().expect("Failed to create tokio runtime");
    rt.block_on(bridge.encrypt_message(peer, plaintext))
        .expect("Failed to encrypt message")
}

pub fn decrypt_message(bridge: &mut SignalBridge, peer: &str, ciphertext: &[u8]) -> Vec<u8> {
    let rt = tokio::runtime::Runtime::new().expect("Failed to create tokio runtime");
    rt.block_on(bridge.decrypt_message(peer, ciphertext))
        .expect("Failed to decrypt message")
}

pub fn establish_session(bridge: &mut SignalBridge, peer: &str, bundle: &[u8]) {
    let rt = tokio::runtime::Runtime::new().expect("Failed to create tokio runtime");
    rt.block_on(bridge.establish_session(peer, bundle))
        .expect("Failed to establish session");
}

pub fn generate_pre_key_bundle(bridge: &mut SignalBridge) -> Vec<u8> {
    let rt = tokio::runtime::Runtime::new().expect("Failed to create tokio runtime");
    rt.block_on(bridge.generate_pre_key_bundle())
        .expect("Failed to generate pre-key bundle")
}

pub fn clear_peer_session(bridge: &mut SignalBridge, peer: &str) {
    let rt = tokio::runtime::Runtime::new().expect("Failed to create tokio runtime");
    rt.block_on(bridge.clear_peer_session(peer))
        .expect("Failed to clear peer session");
}

pub fn clear_all_sessions(bridge: &mut SignalBridge) {
    let rt = tokio::runtime::Runtime::new().expect("Failed to create tokio runtime");
    rt.block_on(bridge.clear_all_sessions())
        .expect("Failed to clear all sessions");
}

pub fn reset_identity(bridge: &mut SignalBridge) {
    let rt = tokio::runtime::Runtime::new().expect("Failed to create tokio runtime");
    rt.block_on(bridge.reset_identity())
        .expect("Failed to reset identity");
}

pub fn generate_node_fingerprint(bridge: &mut SignalBridge, identity: ffi::NodeIdentity) -> String {
    let node_identity = NodeIdentity {
        hostname: identity.hostname,
        username: identity.username,
        platform: identity.platform,
        mac_address: identity.mac_address,
        install_id: identity.install_id,
    };

    let rt = tokio::runtime::Runtime::new().expect("Failed to create tokio runtime");
    rt.block_on(bridge.generate_node_fingerprint(&node_identity))
        .expect("Failed to generate node fingerprint")
}

pub fn derive_my_nostr_keypair(bridge: &mut SignalBridge) -> Vec<u8> {
    let rt = tokio::runtime::Runtime::new().expect("Failed to create tokio runtime");
    let keys = rt
        .block_on(bridge.derive_nostr_keypair())
        .expect("Failed to derive nostr keypair");
    keys.public_key().to_bytes().to_vec()
}

pub fn derive_peer_nostr_pubkey(bridge: &mut SignalBridge, peer: &str) -> Vec<u8> {
    let rt = tokio::runtime::Runtime::new().expect("Failed to create tokio runtime");
    rt.block_on(bridge.derive_peer_nostr_key(peer))
        .expect("Failed to derive peer nostr key")
}

#[cfg(test)]
mod tests {
    use super::*;

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
        alice.establish_session("bob", &bob_bundle).await.unwrap();

        let bob_nostr_key = alice.derive_peer_nostr_key("bob").await.unwrap();

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
        alice.establish_session("bob", &bob_bundle).await?;

        let plaintext = b"Hello Bob! This is Alice using SignalBridge.";
        let ciphertext = alice.encrypt_message("bob", plaintext).await?;
        let decrypted = bob.decrypt_message("alice", &ciphertext).await?;

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
        let ciphertext = {
            let mut alice = SignalBridge::new(alice_db_str).await?;
            let mut bob = SignalBridge::new(bob_db_str).await?;

            let bob_bundle = bob.generate_pre_key_bundle().await?;
            alice.establish_session("bob", &bob_bundle).await?;

            alice.encrypt_message("bob", original_plaintext).await?
        };

        {
            let mut alice_reopened = SignalBridge::new(alice_db_str).await?;

            let second_message = b"Second message after restart";
            let second_ciphertext = alice_reopened
                .encrypt_message("bob", second_message)
                .await?;

            assert!(!second_ciphertext.is_empty());
            assert_ne!(ciphertext, second_ciphertext);
        }

        {
            let mut bob_reopened = SignalBridge::new(bob_db_str).await?;

            let decrypted1 = bob_reopened.decrypt_message("alice", &ciphertext).await?;
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
        alice.establish_session("bob", &bob_bundle).await?;

        let message = b"Test message";
        let _ciphertext = alice.encrypt_message("bob", message).await?;

        alice.clear_peer_session("bob").await?;

        let result = alice.encrypt_message("bob", message).await;
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

        alice.establish_session("bob", &bob_bundle).await?;
        alice.establish_session("charlie", &charlie_bundle).await?;

        let message = b"Test message";
        let _ciphertext1 = alice.encrypt_message("bob", message).await?;
        let _ciphertext2 = alice.encrypt_message("charlie", message).await?;

        alice.clear_all_sessions().await?;

        let result1 = alice.encrypt_message("bob", message).await;
        let result2 = alice.encrypt_message("charlie", message).await;
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

        let original_alice_bundle = {
            let mut alice = SignalBridge::new(alice_db_str).await?;
            let mut bob = SignalBridge::new(bob_db_str).await?;

            let bob_bundle = bob.generate_pre_key_bundle().await?;
            alice.establish_session("bob", &bob_bundle).await?;

            alice.generate_pre_key_bundle().await?
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
            let result = alice.encrypt_message("bob", message).await;
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
}
