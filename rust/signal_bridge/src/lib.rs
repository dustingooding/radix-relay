//! Signal Protocol integration for Radix Relay
//!
//! This crate provides a bridge between Radix Relay's C++ transport layer
//! and the official Signal Protocol Rust implementation for end-to-end encryption.

pub mod storage_trait;
pub mod memory_storage;
pub mod sqlite_storage;
pub mod session_trait;
pub mod encryption_trait;


#[cfg(test)]
mod keys;

#[cfg(test)]
mod session;

#[cfg(test)]
mod encryption;

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
    async fn test_key_generation_storage_integration() -> Result<(), Box<dyn std::error::Error>> {
        use crate::keys::{generate_identity_key_pair, generate_pre_keys, generate_signed_pre_key};
        use crate::memory_storage::MemoryStorage;
        use crate::storage_trait::{ExtendedIdentityStore, ExtendedPreKeyStore, ExtendedSignedPreKeyStore};

        let identity_key_pair = generate_identity_key_pair().await?;
        let pre_keys = generate_pre_keys(1, 5).await?;
        let signed_pre_key = generate_signed_pre_key(&identity_key_pair, 1).await?;

        let mut storage = MemoryStorage::new();

        let address = ProtocolAddress::new("test_user".to_string(), DeviceId::new(1)?);
        storage.identity_store.save_identity(&address, identity_key_pair.identity_key()).await?;
        let retrieved_identity = storage.identity_store.get_identity(&address).await?;
        assert!(retrieved_identity.is_some());
        assert_eq!(
            retrieved_identity.unwrap().serialize(),
            identity_key_pair.identity_key().serialize()
        );

        for (key_id, key_pair) in &pre_keys {
            let record = PreKeyRecord::new((*key_id).into(), key_pair);
            storage.pre_key_store.save_pre_key((*key_id).into(), &record).await?;
            let retrieved_record = storage.pre_key_store.get_pre_key((*key_id).into()).await?;
            assert_eq!(
                retrieved_record.key_pair()?.public_key.serialize(),
                key_pair.public_key.serialize()
            );
        }

        storage.signed_pre_key_store.save_signed_pre_key(signed_pre_key.id()?, &signed_pre_key).await?;
        let retrieved_signed_key = storage.signed_pre_key_store.get_signed_pre_key(signed_pre_key.id()?).await?;
        assert_eq!(
            retrieved_signed_key.id()?,
            signed_pre_key.id()?
        );

        assert_eq!(storage.identity_store.identity_count().await, 1);
        assert_eq!(storage.pre_key_store.pre_key_count().await, 5);
        assert_eq!(storage.signed_pre_key_store.signed_pre_key_count().await, 1);

        Ok(())
    }

    #[tokio::test]
    async fn test_sqlite_extended_storage_ops_integration() -> Result<(), Box<dyn std::error::Error>> {
        use crate::keys::{generate_identity_key_pair, generate_pre_keys, generate_signed_pre_key};
        use crate::sqlite_storage::SqliteStorage;
        use crate::storage_trait::{ExtendedStorageOps, SignalStorageContainer, ExtendedIdentityStore, ExtendedSessionStore};
        use std::fs;

        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let temp_dir = std::env::temp_dir();
        let db_path = temp_dir.join(format!("test_sqlite_alice_{}_{}.db", process_id, timestamp));
        let db_path_str = db_path.to_str().unwrap();

        if db_path.exists() {
            let _ = fs::remove_file(&db_path);
        }

        let mut alice_storage = SqliteStorage::new(db_path_str).await?;
        alice_storage.initialize()?;

        let db_path2 = temp_dir.join(format!("test_sqlite_bob_{}_{}.db", process_id, timestamp));
        let db_path2_str = db_path2.to_str().unwrap();

        if db_path2.exists() {
            let _ = fs::remove_file(&db_path2);
        }

        let mut bob_storage = SqliteStorage::new(db_path2_str).await?;
        bob_storage.initialize()?;

        let mut rng = rand::rng();
        let alice_identity = generate_identity_key_pair().await?;
        let alice_registration_id = 11111u32;
        alice_storage.identity_store().set_local_identity_key_pair(&alice_identity).await?;
        alice_storage.identity_store().set_local_registration_id(alice_registration_id).await?;

        let bob_identity = generate_identity_key_pair().await?;
        let bob_registration_id = 22222u32;
        bob_storage.identity_store().set_local_identity_key_pair(&bob_identity).await?;
        bob_storage.identity_store().set_local_registration_id(bob_registration_id).await?;

        let alice_address = ProtocolAddress::new("alice".to_string(), DeviceId::new(1)?);
        let bob_address = ProtocolAddress::new("bob".to_string(), DeviceId::new(1)?);

        let bob_pre_keys = generate_pre_keys(1, 1).await?;
        let bob_signed_pre_key = generate_signed_pre_key(&bob_identity, 1).await?;
        bob_storage.pre_key_store().save_pre_key(PreKeyId::from(bob_pre_keys[0].0), &PreKeyRecord::new(PreKeyId::from(bob_pre_keys[0].0), &bob_pre_keys[0].1)).await?;
        bob_storage.signed_pre_key_store().save_signed_pre_key(SignedPreKeyId::from(1u32), &bob_signed_pre_key).await?;

        let kyber_keypair = libsignal_protocol::kem::KeyPair::generate(libsignal_protocol::kem::KeyType::Kyber1024, &mut rng);
        let kyber_signature = bob_identity.private_key()
            .calculate_signature(&kyber_keypair.public_key.serialize(), &mut rng)?;

        let now = std::time::SystemTime::now();
        let kyber_record = KyberPreKeyRecord::new(
            KyberPreKeyId::from(1u32),
            Timestamp::from_epoch_millis(now.duration_since(std::time::UNIX_EPOCH)?.as_millis() as u64),
            &kyber_keypair,
            &kyber_signature,
        );
        bob_storage.kyber_pre_key_store().save_kyber_pre_key(KyberPreKeyId::from(1u32), &kyber_record).await?;

        let bob_bundle = PreKeyBundle::new(
            bob_registration_id,
            DeviceId::new(1)?,
            Some((PreKeyId::from(bob_pre_keys[0].0), bob_pre_keys[0].1.public_key)),
            SignedPreKeyId::from(1u32),
            bob_signed_pre_key.public_key()?,
            bob_signed_pre_key.signature()?.to_vec(),
            KyberPreKeyId::from(1u32),
            kyber_keypair.public_key,
            kyber_signature.to_vec(),
            *bob_identity.identity_key(),
        )?;

        alice_storage.establish_session_from_bundle(&bob_address, &bob_bundle).await?;

        assert_eq!(alice_storage.session_store().session_count().await, 1, "Alice should have established session with Bob");

        let plaintext = b"Hello Bob! This is an integration test with real SQLite files.";
        let ciphertext = alice_storage.encrypt_message(&bob_address, plaintext).await?;

        assert!(matches!(ciphertext.message_type(), CiphertextMessageType::PreKey),
               "First message should be a PreKey message");

        bob_storage.identity_store().save_identity(&alice_address, alice_identity.identity_key()).await?;
        let decrypted = bob_storage.decrypt_message(&alice_address, &ciphertext).await?;

        assert_eq!(decrypted, plaintext, "Decrypted message should match original");

        let response_plaintext = b"Hello Alice! Integration test successful with persistent SQLite storage!";
        let response_ciphertext = bob_storage.encrypt_message(&alice_address, response_plaintext).await?;

        assert!(matches!(response_ciphertext.message_type(), CiphertextMessageType::Whisper),
               "Response message should be a Whisper message");

        let response_decrypted = alice_storage.decrypt_message(&bob_address, &response_ciphertext).await?;
        assert_eq!(response_decrypted, response_plaintext, "Response should decrypt correctly");

        alice_storage.close()?;
        bob_storage.close()?;

        assert!(db_path.exists(), "SQLite database file should exist after test");
        assert!(db_path2.exists(), "Bob's SQLite database file should exist after test");

        drop(alice_storage);
        drop(bob_storage);

        #[cfg(windows)]
        std::thread::sleep(std::time::Duration::from_millis(100));

        let _ = fs::remove_file(&db_path);
        let _ = fs::remove_file(&db_path2);

        Ok(())
    }

}
