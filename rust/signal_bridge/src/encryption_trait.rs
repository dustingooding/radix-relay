//! Encryption and decryption functions using trait-based storage
//!
//! This module provides message encryption and decryption functions
//! using the Signal Protocol's Double Ratchet algorithm with dependency injection.

use libsignal_protocol::*;
use crate::storage_trait::ExtendedStorageOps;

pub async fn encrypt_message<S: ExtendedStorageOps>(
    storage: &mut S,
    remote_address: &ProtocolAddress,
    plaintext: &[u8],
) -> Result<CiphertextMessage, SignalProtocolError> {
    storage.encrypt_message(remote_address, plaintext).await
}

pub async fn decrypt_message<S: ExtendedStorageOps>(
    storage: &mut S,
    remote_address: &ProtocolAddress,
    ciphertext: &CiphertextMessage,
) -> Result<Vec<u8>, SignalProtocolError> {
    storage.decrypt_message(remote_address, ciphertext).await
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::memory_storage::MemoryStorage;
    use crate::storage_trait::{ExtendedIdentityStore, ExtendedSessionStore};
    use crate::keys::{generate_identity_key_pair, generate_pre_keys, generate_signed_pre_key};
    use crate::session_trait::establish_session_from_bundle;

    #[tokio::test]
    async fn test_encrypt_message_basic_with_memory_storage() -> Result<(), Box<dyn std::error::Error>> {
        let bob_identity = generate_identity_key_pair().await?;
        let bob_pre_keys = generate_pre_keys(1, 1).await?;
        let bob_signed_pre_key = generate_signed_pre_key(&bob_identity, 1).await?;
        let bob_address = ProtocolAddress::new("bob".to_string(), DeviceId::new(1)?);

        let mut rng = rand::rng();
        let kyber_keypair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
        let kyber_signature = bob_identity.private_key()
            .calculate_signature(&kyber_keypair.public_key.serialize(), &mut rng)?;

        let bundle = PreKeyBundle::new(
            12345,
            bob_address.device_id(),
            Some((bob_pre_keys[0].0.into(), bob_pre_keys[0].1.public_key)),
            bob_signed_pre_key.id()?,
            bob_signed_pre_key.public_key()?,
            bob_signed_pre_key.signature()?.to_vec(),
            KyberPreKeyId::from(1u32),
            kyber_keypair.public_key,
            kyber_signature.to_vec(),
            *bob_identity.identity_key(),
        )?;

        let mut alice_storage = MemoryStorage::new();
        let alice_identity = generate_identity_key_pair().await?;
        alice_storage.identity_store.set_local_identity_key_pair(&alice_identity).await?;
        alice_storage.identity_store.set_local_registration_id(12346).await?;

        establish_session_from_bundle(&bob_address, &bundle, &mut alice_storage).await?;

        let plaintext = b"Hello, Bob!";
        let ciphertext = encrypt_message(&mut alice_storage, &bob_address, plaintext).await?;

        assert!(!ciphertext.serialize().is_empty());
        assert_ne!(ciphertext.serialize(), plaintext);

        Ok(())
    }

    #[tokio::test]
    async fn test_encrypt_decrypt_roundtrip_with_memory_storage() -> Result<(), Box<dyn std::error::Error>> {
        let bob_identity = generate_identity_key_pair().await?;
        let bob_pre_keys = generate_pre_keys(1, 1).await?;
        let bob_signed_pre_key = generate_signed_pre_key(&bob_identity, 1).await?;
        let bob_address = ProtocolAddress::new("bob".to_string(), DeviceId::new(1)?);

        let mut rng = rand::rng();
        let kyber_keypair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
        let kyber_signature = bob_identity.private_key()
            .calculate_signature(&kyber_keypair.public_key.serialize(), &mut rng)?;

        let kyber_keypair_for_storage = kyber_keypair.clone();

        let bundle = PreKeyBundle::new(
            12345,
            bob_address.device_id(),
            Some((bob_pre_keys[0].0.into(), bob_pre_keys[0].1.public_key)),
            bob_signed_pre_key.id()?,
            bob_signed_pre_key.public_key()?,
            bob_signed_pre_key.signature()?.to_vec(),
            KyberPreKeyId::from(1u32),
            kyber_keypair.public_key,
            kyber_signature.to_vec(),
            *bob_identity.identity_key(),
        )?;

        let mut alice_storage = MemoryStorage::new();
        let alice_identity = generate_identity_key_pair().await?;
        alice_storage.identity_store.set_local_identity_key_pair(&alice_identity).await?;
        alice_storage.identity_store.set_local_registration_id(12346).await?;

        establish_session_from_bundle(&bob_address, &bundle, &mut alice_storage).await?;

        let mut bob_storage = MemoryStorage::new();
        bob_storage.identity_store.set_local_identity_key_pair(&bob_identity).await?;
        bob_storage.identity_store.set_local_registration_id(12345).await?;

        for (key_id, key_pair) in &bob_pre_keys {
            let record = PreKeyRecord::new((*key_id).into(), key_pair);
            bob_storage.pre_key_store.save_pre_key((*key_id).into(), &record).await?;
        }
        bob_storage.signed_pre_key_store.save_signed_pre_key(bob_signed_pre_key.id()?, &bob_signed_pre_key).await?;

        let kyber_pre_key_record = KyberPreKeyRecord::new(
            KyberPreKeyId::from(1u32),
            Timestamp::from_epoch_millis(12345),
            &kyber_keypair_for_storage,
            &kyber_signature,
        );
        bob_storage.kyber_pre_key_store.save_kyber_pre_key(KyberPreKeyId::from(1u32), &kyber_pre_key_record).await?;

        let plaintext = b"Hello, Bob! This is a secret message.";
        let ciphertext = encrypt_message(&mut alice_storage, &bob_address, plaintext).await?;

        let alice_address = ProtocolAddress::new("alice".to_string(), DeviceId::new(1)?);
        let decrypted = decrypt_message(&mut bob_storage, &alice_address, &ciphertext).await?;

        assert_eq!(decrypted, plaintext);

        Ok(())
    }

    #[tokio::test]
    async fn test_encrypt_without_session_fails_with_memory_storage() -> Result<(), Box<dyn std::error::Error>> {
        let mut alice_storage = MemoryStorage::new();
        let alice_identity = generate_identity_key_pair().await?;
        alice_storage.identity_store.set_local_identity_key_pair(&alice_identity).await?;
        alice_storage.identity_store.set_local_registration_id(12346).await?;

        let bob_address = ProtocolAddress::new("bob".to_string(), DeviceId::new(1)?);
        let plaintext = b"Hello, Bob!";

        let result = encrypt_message(&mut alice_storage, &bob_address, plaintext).await;
        assert!(result.is_err());

        Ok(())
    }

    #[tokio::test]
    async fn test_storage_crash_simulation() -> Result<(), Box<dyn std::error::Error>> {
        let bob_identity = generate_identity_key_pair().await?;
        let bob_pre_keys = generate_pre_keys(1, 1).await?;
        let bob_signed_pre_key = generate_signed_pre_key(&bob_identity, 1).await?;
        let bob_address = ProtocolAddress::new("bob".to_string(), DeviceId::new(1)?);

        let mut rng = rand::rng();
        let kyber_keypair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
        let kyber_signature = bob_identity.private_key()
            .calculate_signature(&kyber_keypair.public_key.serialize(), &mut rng)?;

        let bundle = PreKeyBundle::new(
            12345,
            bob_address.device_id(),
            Some((bob_pre_keys[0].0.into(), bob_pre_keys[0].1.public_key)),
            bob_signed_pre_key.id()?,
            bob_signed_pre_key.public_key()?,
            bob_signed_pre_key.signature()?.to_vec(),
            KyberPreKeyId::from(1u32),
            kyber_keypair.public_key,
            kyber_signature.to_vec(),
            *bob_identity.identity_key(),
        )?;

        let mut alice_storage = MemoryStorage::new();
        let alice_identity = generate_identity_key_pair().await?;
        alice_storage.identity_store.set_local_identity_key_pair(&alice_identity).await?;
        alice_storage.identity_store.set_local_registration_id(12346).await?;

        establish_session_from_bundle(&bob_address, &bundle, &mut alice_storage).await?;
        assert_eq!(alice_storage.session_store.session_count().await, 1);

        let plaintext = b"Hello before crash!";
        let ciphertext = encrypt_message(&mut alice_storage, &bob_address, plaintext).await?;
        assert!(!ciphertext.serialize().is_empty());

        let mut alice_storage_after_crash = MemoryStorage::new();
        alice_storage_after_crash.identity_store.set_local_identity_key_pair(&alice_identity).await?;
        alice_storage_after_crash.identity_store.set_local_registration_id(12346).await?;

        assert_eq!(alice_storage_after_crash.session_store.session_count().await, 0);

        let plaintext_after_crash = b"Hello after crash!";
        let result = encrypt_message(&mut alice_storage_after_crash, &bob_address, plaintext_after_crash).await;
        assert!(result.is_err());

        Ok(())
    }
}
