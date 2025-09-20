//! Encryption and decryption tests
//!
//! This module contains tests for message encryption and decryption functionality

#[cfg(test)]
mod tests {
    use libsignal_protocol::*;
    use crate::memory_storage::MemoryStorage;
    use crate::storage_trait::ExtendedIdentityStore;
    use crate::keys::{generate_identity_key_pair, generate_pre_keys, generate_signed_pre_key};

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

        let mut alice_storage = MemoryStorage::new();
        let alice_identity = generate_identity_key_pair().await?;
        alice_storage.identity_store.set_local_identity_key_pair(&alice_identity).await?;
        alice_storage.identity_store.set_local_registration_id(12346).await?;

        alice_storage.establish_session_from_bundle(&bob_address, &bundle).await?;

        let plaintext = b"Hello, Bob!";
        let ciphertext = alice_storage.encrypt_message(&bob_address, plaintext).await?;

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

        let mut alice_storage = MemoryStorage::new();
        let alice_identity = generate_identity_key_pair().await?;
        alice_storage.identity_store.set_local_identity_key_pair(&alice_identity).await?;
        alice_storage.identity_store.set_local_registration_id(12346).await?;

        alice_storage.establish_session_from_bundle(&bob_address, &bundle).await?;

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
        let ciphertext = alice_storage.encrypt_message(&bob_address, plaintext).await?;

        let alice_address = ProtocolAddress::new("alice".to_string(), DeviceId::new(1)?);
        let decrypted = bob_storage.decrypt_message(&alice_address, &ciphertext).await?;

        assert_eq!(decrypted, plaintext);

        Ok(())
    }

    #[tokio::test]
    async fn test_encrypt_without_session_fails() -> Result<(), Box<dyn std::error::Error>> {
        let mut alice_storage = MemoryStorage::new();
        let alice_identity = generate_identity_key_pair().await?;
        alice_storage.identity_store.set_local_identity_key_pair(&alice_identity).await?;
        alice_storage.identity_store.set_local_registration_id(12346).await?;

        let bob_address = ProtocolAddress::new("bob".to_string(), DeviceId::new(1)?);
        let plaintext = b"Hello, Bob!";

        let result = alice_storage.encrypt_message(&bob_address, plaintext).await;
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

        let mut alice_storage = MemoryStorage::new();
        let alice_identity = generate_identity_key_pair().await?;
        alice_storage.identity_store.set_local_identity_key_pair(&alice_identity).await?;
        alice_storage.identity_store.set_local_registration_id(12346).await?;

        alice_storage.establish_session_from_bundle(&bob_address, &bundle).await?;

        let messages = vec![
            b"First message".as_slice(),
            b"Second message with more content".as_slice(),
            b"Third message!".as_slice(),
        ];

        let mut ciphertexts = Vec::new();
        for message in &messages {
            let ciphertext = alice_storage.encrypt_message(&bob_address, message).await?;
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
