//! Signal Protocol integration for Radix Relay
//!
//! This crate provides a bridge between Radix Relay's C++ transport layer
//! and the official Signal Protocol Rust implementation for end-to-end encryption.

mod storage_trait;
mod memory_storage;
mod sqlite_storage;
mod session_trait;
mod encryption_trait;
mod keys;

#[cfg(test)]
mod session;

#[cfg(test)]
mod encryption;


use crate::sqlite_storage::SqliteStorage;
use crate::storage_trait::{SignalStorageContainer, ExtendedStorageOps, ExtendedSessionStore, ExtendedIdentityStore, ExtendedPreKeyStore, ExtendedSignedPreKeyStore, ExtendedKyberPreKeyStore};
use libsignal_protocol::{CiphertextMessage, ProtocolAddress, DeviceId, SessionStore, IdentityKeyPair};
use serde::{Serialize, Deserialize};

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
    async fn ensure_keys_exist(storage: &mut SqliteStorage) -> Result<IdentityKeyPair, Box<dyn std::error::Error>> {
        use crate::keys::{generate_identity_key_pair, generate_pre_keys, generate_signed_pre_key};
        use crate::storage_trait::{ExtendedIdentityStore, ExtendedPreKeyStore, ExtendedSignedPreKeyStore, ExtendedKyberPreKeyStore};
        use libsignal_protocol::*;

        let identity_key_pair = match storage.identity_store().get_identity_key_pair().await {
            Ok(existing_identity) => {
                existing_identity
            }
            Err(_) => {
                let new_identity = generate_identity_key_pair().await?;
                let new_registration_id = rand::random::<u32>();

                storage.identity_store().set_local_identity_key_pair(&new_identity).await?;
                storage.identity_store().set_local_registration_id(new_registration_id).await?;

                new_identity
            }
        };

        let current_pre_key_count = storage.pre_key_store().pre_key_count().await;
        if current_pre_key_count < 5 {
            let pre_keys = generate_pre_keys(1, 10).await?;
            for (key_id, key_pair) in &pre_keys {
                let record = PreKeyRecord::new((*key_id).into(), key_pair);
                storage.pre_key_store().save_pre_key((*key_id).into(), &record).await?;
            }
        }

        let current_signed_pre_key_count = storage.signed_pre_key_store().signed_pre_key_count().await;
        if current_signed_pre_key_count == 0 {
            let signed_pre_key = generate_signed_pre_key(&identity_key_pair, 1).await?;
            storage.signed_pre_key_store().save_signed_pre_key(signed_pre_key.id()?, &signed_pre_key).await?;
        }

        let current_kyber_pre_key_count = storage.kyber_pre_key_store().kyber_pre_key_count().await;
        if current_kyber_pre_key_count == 0 {
            let mut rng = rand::rng();
            let kyber_keypair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
            let kyber_signature = identity_key_pair.private_key()
                .calculate_signature(&kyber_keypair.public_key.serialize(), &mut rng)?;

            let now = std::time::SystemTime::now();
            let kyber_record = KyberPreKeyRecord::new(
                KyberPreKeyId::from(1u32),
                Timestamp::from_epoch_millis(now.duration_since(std::time::UNIX_EPOCH)?.as_millis() as u64),
                &kyber_keypair,
                &kyber_signature,
            );
            storage.kyber_pre_key_store().save_kyber_pre_key(KyberPreKeyId::from(1u32), &kyber_record).await?;
        }

        Ok(identity_key_pair)
    }

    pub async fn new(db_path: &str) -> Result<Self, Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(db_path).await?;
        storage.initialize()?;

        let schema_version = storage.get_schema_version()?;
        if schema_version < 1 {
            return Err("Database schema version is too old".into());
        }

        Self::ensure_keys_exist(&mut storage).await?;

        println!("SignalBridge initialized with {} storage, {} existing sessions, {} identities, {} pre-keys, {} signed pre-keys, {} kyber pre-keys",
                 storage.storage_type(),
                 storage.session_store().session_count().await,
                 storage.identity_store().identity_count().await,
                 storage.pre_key_store().pre_key_count().await,
                 storage.signed_pre_key_store().signed_pre_key_count().await,
                 storage.kyber_pre_key_store().kyber_pre_key_count().await);

        Ok(Self { storage })
    }

    pub async fn encrypt_message(&mut self, peer: &str, plaintext: &[u8]) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
        let address = ProtocolAddress::new(peer.to_string(), DeviceId::new(1)?);

        let session = self.storage.session_store().load_session(&address).await?;
        if session.is_none() {
            return Err(format!("No session established with peer: {}", peer).into());
        }

        let ciphertext = self.storage.encrypt_message(&address, plaintext).await?;
        Ok(ciphertext.serialize().to_vec())
    }

    pub async fn decrypt_message(&mut self, peer: &str, ciphertext_bytes: &[u8]) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
        use libsignal_protocol::{PreKeySignalMessage, SignalMessage};

        let address = ProtocolAddress::new(peer.to_string(), DeviceId::new(1)?);

        let ciphertext = if let Ok(prekey_msg) = PreKeySignalMessage::try_from(ciphertext_bytes) {
            CiphertextMessage::PreKeySignalMessage(prekey_msg)
        } else if let Ok(signal_msg) = SignalMessage::try_from(ciphertext_bytes) {
            CiphertextMessage::SignalMessage(signal_msg)
        } else {
            return Err("Unable to deserialize ciphertext message".into());
        };

        let plaintext = self.storage.decrypt_message(&address, &ciphertext).await?;
        Ok(plaintext)
    }


    pub async fn establish_session(&mut self, peer: &str, pre_key_bundle_bytes: &[u8]) -> Result<(), Box<dyn std::error::Error>> {
        use libsignal_protocol::*;
        use crate::storage_trait::ExtendedStorageOps;

        let serializable: SerializablePreKeyBundle = bincode::deserialize(pre_key_bundle_bytes)?;

        let bundle = PreKeyBundle::new(
            serializable.registration_id,
            DeviceId::new(serializable.device_id.try_into()?)?,
            serializable.pre_key_id.map(|id| {
                let public_key = PublicKey::deserialize(&serializable.pre_key_public.as_ref().unwrap())?;
                Ok::<_, Box<dyn std::error::Error>>((PreKeyId::from(id), public_key))
            }).transpose()?,
            SignedPreKeyId::from(serializable.signed_pre_key_id),
            PublicKey::deserialize(&serializable.signed_pre_key_public)?,
            serializable.signed_pre_key_signature,
            KyberPreKeyId::from(serializable.kyber_pre_key_id),
            kem::PublicKey::deserialize(&serializable.kyber_pre_key_public)?,
            serializable.kyber_pre_key_signature,
            IdentityKey::decode(&serializable.identity_key)?,
        )?;

        let address = ProtocolAddress::new(peer.to_string(), DeviceId::new(1)?);

        self.storage.establish_session_from_bundle(&address, &bundle).await?;

        Ok(())
    }

    pub async fn generate_pre_key_bundle(&mut self) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
        use libsignal_protocol::*;

        let identity_key = *self.storage.identity_store().get_identity_key_pair().await?.identity_key();
        let registration_id = self.storage.identity_store().get_local_registration_id().await?;

        let pre_key_record = self.storage.pre_key_store().get_pre_key(PreKeyId::from(1u32)).await?;
        let signed_pre_key_record = self.storage.signed_pre_key_store().get_signed_pre_key(SignedPreKeyId::from(1u32)).await?;
        let kyber_pre_key_record = self.storage.kyber_pre_key_store().get_kyber_pre_key(KyberPreKeyId::from(1u32)).await?;

        let bundle = PreKeyBundle::new(
            registration_id,
            DeviceId::new(1)?,
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

    pub async fn cleanup_all_sessions(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let session_count = self.storage.session_store().session_count().await;
        if session_count > 0 {
            println!("Cleaning up {} sessions", session_count);
            self.storage.session_store().clear_all_sessions().await?;
        }
        Ok(())
    }

    pub async fn clear_peer_session(&mut self, peer: &str) -> Result<(), Box<dyn std::error::Error>> {
        let address = ProtocolAddress::new(peer.to_string(), DeviceId::new(1)?);

        self.storage.session_store().delete_session(&address).await?;

        if let Ok(_) = self.storage.identity_store().get_peer_identity(&address).await {
            self.storage.identity_store().delete_identity(&address).await?;
        }

        println!("Cleared session and identity for peer: {}", peer);
        Ok(())
    }

    pub async fn clear_all_sessions(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let session_count = self.storage.session_store().session_count().await;
        let identity_count = self.storage.identity_store().identity_count().await;

        if session_count > 0 || identity_count > 0 {
            self.storage.session_store().clear_all_sessions().await?;
            self.storage.identity_store().clear_all_identities().await?;

            println!("Cleared all {} sessions and {} peer identities", session_count, identity_count);
        }
        Ok(())
    }

    pub async fn reset_identity(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        use crate::storage_trait::{ExtendedIdentityStore, ExtendedPreKeyStore, ExtendedSignedPreKeyStore, ExtendedKyberPreKeyStore};

        println!("WARNING: Resetting identity - all existing sessions will be invalidated");

        self.clear_all_sessions().await?;

        self.storage.pre_key_store().clear_all_pre_keys().await?;
        self.storage.signed_pre_key_store().clear_all_signed_pre_keys().await?;
        self.storage.kyber_pre_key_store().clear_all_kyber_pre_keys().await?;
        self.storage.identity_store().clear_local_identity().await?;

        Self::ensure_keys_exist(&mut self.storage).await?;

        println!("Identity reset complete - new identity generated with fresh keys");
        Ok(())
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

#[cfg(test)]
mod tests {
    use libsignal_protocol::*;

    #[test]
    fn test_libsignal_basic_types() {
        let device_id = DeviceId::new(1).expect("Valid device ID");
        let protocol_address = ProtocolAddress::new("test_device".to_string(), device_id);
        assert_eq!(protocol_address.name(), "test_device");
        assert_eq!(protocol_address.device_id(), device_id);
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
    async fn test_signalbridge_persistence_across_instantiations() -> Result<(), Box<dyn std::error::Error>> {
        use crate::SignalBridge;
        use std::env;

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let alice_db_path = temp_dir.join(format!("test_persistence_alice_{}_{}.db", process_id, timestamp));
        let alice_db_str = alice_db_path.to_str().unwrap();
        let bob_db_path = temp_dir.join(format!("test_persistence_bob_{}_{}.db", process_id, timestamp));
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
            let second_ciphertext = alice_reopened.encrypt_message("bob", second_message).await?;

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

        let alice_db_path = temp_dir.join(format!("test_clear_peer_alice_{}_{}.db", process_id, timestamp));
        let alice_db_str = alice_db_path.to_str().unwrap();
        let bob_db_path = temp_dir.join(format!("test_clear_peer_bob_{}_{}.db", process_id, timestamp));
        let bob_db_str = bob_db_path.to_str().unwrap();

        let mut alice = SignalBridge::new(alice_db_str).await?;
        let mut bob = SignalBridge::new(bob_db_str).await?;

        let bob_bundle = bob.generate_pre_key_bundle().await?;
        alice.establish_session("bob", &bob_bundle).await?;

        let message = b"Test message";
        let _ciphertext = alice.encrypt_message("bob", message).await?;

        alice.clear_peer_session("bob").await?;

        let result = alice.encrypt_message("bob", message).await;
        assert!(result.is_err(), "Encryption should fail after clearing peer session");

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

        let alice_db_path = temp_dir.join(format!("test_clear_all_alice_{}_{}.db", process_id, timestamp));
        let alice_db_str = alice_db_path.to_str().unwrap();
        let bob_db_path = temp_dir.join(format!("test_clear_all_bob_{}_{}.db", process_id, timestamp));
        let bob_db_str = bob_db_path.to_str().unwrap();
        let charlie_db_path = temp_dir.join(format!("test_clear_all_charlie_{}_{}.db", process_id, timestamp));
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
        assert!(result1.is_err(), "Encryption to Bob should fail after clearing all sessions");
        assert!(result2.is_err(), "Encryption to Charlie should fail after clearing all sessions");

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

        let alice_db_path = temp_dir.join(format!("test_reset_identity_alice_{}_{}.db", process_id, timestamp));
        let alice_db_str = alice_db_path.to_str().unwrap();
        let bob_db_path = temp_dir.join(format!("test_reset_identity_bob_{}_{}.db", process_id, timestamp));
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
            assert_ne!(original_alice_bundle, new_alice_bundle, "Alice's identity should be different after reset");

            let message = b"Test message";
            let result = alice.encrypt_message("bob", message).await;
            assert!(result.is_err(), "Encryption should fail after identity reset");
        }

        Ok(())
    }

}
