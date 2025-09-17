//! Encryption and decryption functions for libsignal
//!
//! This module provides message encryption and decryption functions
//! using the Signal Protocol's Double Ratchet algorithm.

use libsignal_protocol::*;
use crate::storage::SqliteStorage;

pub async fn encrypt_message_with_storage(
    storage: &mut SqliteStorage,
    remote_address: &ProtocolAddress,
    plaintext: &[u8],
) -> Result<CiphertextMessage, SignalProtocolError> {
    let mut rng = rand::rng();
    let now = std::time::SystemTime::now();
    message_encrypt(plaintext, remote_address, &mut storage.session_store, &mut storage.identity_store, now, &mut rng).await
}

pub async fn decrypt_message_with_storage(
    storage: &mut SqliteStorage,
    remote_address: &ProtocolAddress,
    ciphertext: &CiphertextMessage,
) -> Result<Vec<u8>, SignalProtocolError> {
    let mut rng = rand::rng();

    message_decrypt(
        ciphertext,
        remote_address,
        &mut storage.session_store,
        &mut storage.identity_store,
        &mut storage.pre_key_store,
        &storage.signed_pre_key_store,
        &mut storage.kyber_pre_key_store,
        &mut rng,
        UsePQRatchet::Yes,
    ).await
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::keys::{generate_identity_key_pair, generate_pre_keys, generate_signed_pre_key};
    use crate::session::establish_session_from_bundle_with_storage;

    #[tokio::test]
    async fn test_encrypt_message_basic() -> Result<(), Box<dyn std::error::Error>> {
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

        let mut alice_storage = SqliteStorage::new(":memory:").await?;
        let alice_identity = generate_identity_key_pair().await?;
        alice_storage.set_local_identity_key_pair(&alice_identity).await?;
        alice_storage.set_local_registration_id(12346).await?;

        establish_session_from_bundle_with_storage(&bob_address, &bundle, &mut alice_storage).await?;

        let plaintext = b"Hello, Bob!";
        let ciphertext = encrypt_message_with_storage(&mut alice_storage, &bob_address, plaintext).await?;

        assert!(!ciphertext.serialize().is_empty());
        assert_ne!(ciphertext.serialize(), plaintext);

        Ok(())
    }

    #[tokio::test]
    async fn test_encrypt_decrypt_roundtrip() -> Result<(), Box<dyn std::error::Error>> {
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

        let mut alice_storage = SqliteStorage::new(":memory:").await?;
        let alice_identity = generate_identity_key_pair().await?;
        alice_storage.set_local_identity_key_pair(&alice_identity).await?;
        alice_storage.set_local_registration_id(12346).await?;

        establish_session_from_bundle_with_storage(&bob_address, &bundle, &mut alice_storage).await?;

        let mut bob_storage = SqliteStorage::new(":memory:").await?;
        bob_storage.set_local_identity_key_pair(&bob_identity).await?;
        bob_storage.set_local_registration_id(12345).await?;

        for (key_id, key_pair) in &bob_pre_keys {
            bob_storage.save_pre_key(*key_id, key_pair).await?;
        }
        bob_storage.save_signed_pre_key(1, &bob_signed_pre_key).await?;

        let kyber_pre_key_record = KyberPreKeyRecord::new(
            KyberPreKeyId::from(1u32),
            Timestamp::from_epoch_millis(12345),
            &kyber_keypair_for_storage,
            &kyber_signature,
        );
        bob_storage.kyber_pre_key_store.save_kyber_pre_key(1, &kyber_pre_key_record).await?;

        let plaintext = b"Hello, Bob! This is a secret message.";
        let ciphertext = encrypt_message_with_storage(&mut alice_storage, &bob_address, plaintext).await?;

        let alice_address = ProtocolAddress::new("alice".to_string(), DeviceId::new(1)?);
        let decrypted = decrypt_message_with_storage(&mut bob_storage, &alice_address, &ciphertext).await?;

        assert_eq!(decrypted, plaintext);

        Ok(())
    }

    #[tokio::test]
    async fn test_encrypt_without_session_fails() -> Result<(), Box<dyn std::error::Error>> {
        let mut alice_storage = SqliteStorage::new(":memory:").await?;
        let alice_identity = generate_identity_key_pair().await?;
        alice_storage.set_local_identity_key_pair(&alice_identity).await?;
        alice_storage.set_local_registration_id(12346).await?;

        let bob_address = ProtocolAddress::new("bob".to_string(), DeviceId::new(1)?);
        let plaintext = b"Hello, Bob!";

        let result = encrypt_message_with_storage(&mut alice_storage, &bob_address, plaintext).await;
        assert!(result.is_err());

        Ok(())
    }

    #[tokio::test]
    async fn test_multiple_messages_encryption() -> Result<(), Box<dyn std::error::Error>> {
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

        let mut alice_storage = SqliteStorage::new(":memory:").await?;
        let alice_identity = generate_identity_key_pair().await?;
        alice_storage.set_local_identity_key_pair(&alice_identity).await?;
        alice_storage.set_local_registration_id(12346).await?;

        establish_session_from_bundle_with_storage(&bob_address, &bundle, &mut alice_storage).await?;

        let messages = vec![
            b"First message".as_slice(),
            b"Second message with more content".as_slice(),
            b"Third message!".as_slice(),
        ];

        let mut ciphertexts = Vec::new();
        for message in &messages {
            let ciphertext = encrypt_message_with_storage(&mut alice_storage, &bob_address, message).await?;
            ciphertexts.push(ciphertext);
        }

        for i in 0..ciphertexts.len() {
            for j in (i + 1)..ciphertexts.len() {
                assert_ne!(ciphertexts[i].serialize(), ciphertexts[j].serialize());
            }
        }

        Ok(())
    }
}
