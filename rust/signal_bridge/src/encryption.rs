//! Encryption and decryption tests
//!
//! This module contains tests for message encryption and decryption functionality

#[cfg(test)]
mod tests {
    use crate::keys::{generate_identity_key_pair, generate_pre_keys, generate_signed_pre_key};
    use crate::memory_storage::MemoryStorage;
    use crate::storage_trait::ExtendedIdentityStore;
    use libsignal_protocol::*;

    #[tokio::test]
    async fn test_encrypt_message_basic() -> Result<(), Box<dyn std::error::Error>> {
        let bob_identity = generate_identity_key_pair().await?;
        let bob_pre_keys = generate_pre_keys(1, 1).await?;
        let bob_signed_pre_key = generate_signed_pre_key(&bob_identity, 1).await?;
        let bob_address = ProtocolAddress::new("bob".to_string(), DeviceId::new(1)?);

        let mut rng = rand::rng();
        let kyber_keypair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
        let kyber_signature = bob_identity
            .private_key()
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
        alice_storage
            .identity_store
            .set_local_identity_key_pair(&alice_identity)
            .await?;
        alice_storage
            .identity_store
            .set_local_registration_id(12346)
            .await?;

        alice_storage
            .establish_session_from_bundle(&bob_address, &bundle)
            .await?;

        let plaintext = b"Hello, Bob!";
        let ciphertext = alice_storage
            .encrypt_message(&bob_address, plaintext)
            .await?;

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
        let kyber_signature = bob_identity
            .private_key()
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
        alice_storage
            .identity_store
            .set_local_identity_key_pair(&alice_identity)
            .await?;
        alice_storage
            .identity_store
            .set_local_registration_id(12346)
            .await?;

        alice_storage
            .establish_session_from_bundle(&bob_address, &bundle)
            .await?;

        let mut bob_storage = MemoryStorage::new();
        bob_storage
            .identity_store
            .set_local_identity_key_pair(&bob_identity)
            .await?;
        bob_storage
            .identity_store
            .set_local_registration_id(12345)
            .await?;

        for (key_id, key_pair) in &bob_pre_keys {
            let record = PreKeyRecord::new((*key_id).into(), key_pair);
            bob_storage
                .pre_key_store
                .save_pre_key((*key_id).into(), &record)
                .await?;
        }
        bob_storage
            .signed_pre_key_store
            .save_signed_pre_key(bob_signed_pre_key.id()?, &bob_signed_pre_key)
            .await?;

        let kyber_pre_key_record = KyberPreKeyRecord::new(
            KyberPreKeyId::from(1u32),
            Timestamp::from_epoch_millis(12345),
            &kyber_keypair_for_storage,
            &kyber_signature,
        );
        bob_storage
            .kyber_pre_key_store
            .save_kyber_pre_key(KyberPreKeyId::from(1u32), &kyber_pre_key_record)
            .await?;

        let plaintext = b"Hello, Bob! This is a secret message.";
        let ciphertext = alice_storage
            .encrypt_message(&bob_address, plaintext)
            .await?;

        let alice_address = ProtocolAddress::new("alice".to_string(), DeviceId::new(1)?);
        let decrypted = bob_storage
            .decrypt_message(&alice_address, &ciphertext)
            .await?;

        assert_eq!(decrypted, plaintext);

        Ok(())
    }

    #[tokio::test]
    async fn test_encrypt_without_session_fails() -> Result<(), Box<dyn std::error::Error>> {
        let mut alice_storage = MemoryStorage::new();
        let alice_identity = generate_identity_key_pair().await?;
        alice_storage
            .identity_store
            .set_local_identity_key_pair(&alice_identity)
            .await?;
        alice_storage
            .identity_store
            .set_local_registration_id(12346)
            .await?;

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
        let kyber_signature = bob_identity
            .private_key()
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
        alice_storage
            .identity_store
            .set_local_identity_key_pair(&alice_identity)
            .await?;
        alice_storage
            .identity_store
            .set_local_registration_id(12346)
            .await?;

        alice_storage
            .establish_session_from_bundle(&bob_address, &bundle)
            .await?;

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

    #[tokio::test]
    async fn test_prekey_consumed_on_first_message() -> Result<(), Box<dyn std::error::Error>> {
        use crate::storage_trait::ExtendedPreKeyStore;

        let bob_identity = generate_identity_key_pair().await?;
        let bob_pre_keys = generate_pre_keys(1, 1).await?;
        let bob_signed_pre_key = generate_signed_pre_key(&bob_identity, 1).await?;
        let bob_address = ProtocolAddress::new("bob".to_string(), DeviceId::new(1)?);

        let mut rng = rand::rng();
        let kyber_keypair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
        let kyber_signature = bob_identity
            .private_key()
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
        alice_storage
            .identity_store
            .set_local_identity_key_pair(&alice_identity)
            .await?;
        alice_storage
            .identity_store
            .set_local_registration_id(12346)
            .await?;

        alice_storage
            .establish_session_from_bundle(&bob_address, &bundle)
            .await?;

        let mut bob_storage = MemoryStorage::new();
        bob_storage
            .identity_store
            .set_local_identity_key_pair(&bob_identity)
            .await?;
        bob_storage
            .identity_store
            .set_local_registration_id(12345)
            .await?;

        for (key_id, key_pair) in &bob_pre_keys {
            let record = PreKeyRecord::new((*key_id).into(), key_pair);
            bob_storage
                .pre_key_store
                .save_pre_key((*key_id).into(), &record)
                .await?;
        }
        bob_storage
            .signed_pre_key_store
            .save_signed_pre_key(bob_signed_pre_key.id()?, &bob_signed_pre_key)
            .await?;

        let kyber_pre_key_record = KyberPreKeyRecord::new(
            KyberPreKeyId::from(1u32),
            Timestamp::from_epoch_millis(12345),
            &kyber_keypair_for_storage,
            &kyber_signature,
        );
        bob_storage
            .kyber_pre_key_store
            .save_kyber_pre_key(KyberPreKeyId::from(1u32), &kyber_pre_key_record)
            .await?;

        assert_eq!(bob_storage.pre_key_store.pre_key_count().await, 1);

        let plaintext = b"Hello, Bob! This is the first message.";
        let ciphertext = alice_storage
            .encrypt_message(&bob_address, plaintext)
            .await?;

        let alice_address = ProtocolAddress::new("alice".to_string(), DeviceId::new(1)?);
        let decrypted = bob_storage
            .decrypt_message(&alice_address, &ciphertext)
            .await?;

        assert_eq!(decrypted, plaintext);
        assert_eq!(
            bob_storage.pre_key_store.pre_key_count().await,
            0,
            "Pre-key should be consumed by libsignal's message_decrypt"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_signalbridge_decrypt_returns_prekey_consumption_signal(
    ) -> Result<(), Box<dyn std::error::Error>> {
        use crate::SignalBridge;

        let mut bob_bridge = SignalBridge::new(":memory:").await?;
        let mut alice_bridge = SignalBridge::new(":memory:").await?;

        let (bob_bundle_bytes, _, _, _) = bob_bridge.generate_pre_key_bundle().await?;
        let bob_rdx = alice_bridge
            .add_contact_from_bundle(&bob_bundle_bytes, Some("bob"))
            .await?;

        let (alice_bundle_bytes, _, _, _) = alice_bridge.generate_pre_key_bundle().await?;
        let alice_rdx = bob_bridge
            .add_contact_from_bundle(&alice_bundle_bytes, Some("alice"))
            .await?;

        bob_bridge
            .establish_session(&alice_rdx, &alice_bundle_bytes)
            .await?;
        alice_bridge
            .establish_session(&bob_rdx, &bob_bundle_bytes)
            .await?;

        let plaintext = b"Hello Bob!";
        let ciphertext_bytes = alice_bridge.encrypt_message(&bob_rdx, plaintext).await?;

        let result = bob_bridge
            .decrypt_message_with_metadata(&alice_rdx, &ciphertext_bytes)
            .await?;

        assert_eq!(result.plaintext, plaintext);
        assert!(
            result.should_republish_bundle,
            "First message should signal that bundle should be republished"
        );

        Ok(())
    }
}
