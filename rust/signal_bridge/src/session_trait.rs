//! Session management using trait-based storage
//!
//! This module provides session establishment and management functions
//! for the Signal Protocol's Double Ratchet algorithm with dependency injection.

#[cfg(test)]
mod tests {
    use crate::keys::{generate_identity_key_pair, generate_pre_keys, generate_signed_pre_key};
    use crate::memory_storage::MemoryStorage;
    use crate::storage_trait::{ExtendedIdentityStore, ExtendedSessionStore};
    use libsignal_protocol::*;

    #[tokio::test]
    async fn test_establish_session_from_bundle_with_memory_storage(
    ) -> Result<(), Box<dyn std::error::Error>> {
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

        let mut storage = MemoryStorage::new();
        let alice_identity = generate_identity_key_pair().await?;
        storage
            .identity_store
            .set_local_identity_key_pair(&alice_identity)
            .await?;
        storage
            .identity_store
            .set_local_registration_id(12346)
            .await?;

        assert_eq!(storage.session_store.session_count().await, 0);

        storage
            .establish_session_from_bundle(&bob_address, &bundle)
            .await?;

        assert_eq!(storage.session_store.session_count().await, 1);
        let session = storage.session_store.load_session(&bob_address).await?;
        assert!(session.is_some());

        Ok(())
    }

    #[tokio::test]
    async fn test_session_resilience_with_memory_storage() -> Result<(), Box<dyn std::error::Error>>
    {
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

        let mut storage = MemoryStorage::new();
        let alice_identity = generate_identity_key_pair().await?;
        storage
            .identity_store
            .set_local_identity_key_pair(&alice_identity)
            .await?;
        storage
            .identity_store
            .set_local_registration_id(12346)
            .await?;

        storage
            .establish_session_from_bundle(&bob_address, &bundle)
            .await?;
        assert_eq!(storage.session_store.session_count().await, 1);

        let mut new_storage = MemoryStorage::new();
        new_storage
            .identity_store
            .set_local_identity_key_pair(&alice_identity)
            .await?;
        new_storage
            .identity_store
            .set_local_registration_id(12346)
            .await?;

        assert_eq!(new_storage.session_store.session_count().await, 0);

        new_storage
            .establish_session_from_bundle(&bob_address, &bundle)
            .await?;
        assert_eq!(new_storage.session_store.session_count().await, 1);

        Ok(())
    }
}
