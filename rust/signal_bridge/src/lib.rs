//! Signal Protocol integration for Radix Relay
//!
//! This crate provides a bridge between Radix Relay's C++ transport layer
//! and the official Signal Protocol Rust implementation for end-to-end encryption.

pub mod storage_trait;
pub mod memory_storage;
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
    async fn test_session_establishment_integration() -> Result<(), Box<dyn std::error::Error>> {
        use crate::keys::{generate_identity_key_pair, generate_pre_keys, generate_signed_pre_key};
        use crate::memory_storage::MemoryStorage;
        use crate::storage_trait::{ExtendedIdentityStore, ExtendedSessionStore};

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

        let mut storage = MemoryStorage::new();
        let alice_identity = generate_identity_key_pair().await?;
        storage.identity_store.set_local_identity_key_pair(&alice_identity).await?;
        storage.identity_store.set_local_registration_id(12346).await?;

        assert_eq!(storage.session_store.session_count().await, 0);

        storage.establish_session_from_bundle(&bob_address, &bundle).await?;

        assert_eq!(storage.session_store.session_count().await, 1);
        let session = storage.session_store.load_session(&bob_address).await?;
        assert!(session.is_some());

        Ok(())
    }

}
