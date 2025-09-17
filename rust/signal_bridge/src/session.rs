//! Session management for libsignal
//!
//! This module provides session establishment and management functions
//! for the Signal Protocol's Double Ratchet algorithm.

use libsignal_protocol::*;
use crate::storage::SqliteStorage;

pub async fn establish_session_from_bundle(
    address: &ProtocolAddress,
    bundle: &PreKeyBundle,
    session_store: &mut dyn SessionStore,
    identity_store: &mut dyn IdentityKeyStore,
) -> Result<(), Box<dyn std::error::Error>> {
    let mut rng = rand::rng();
    let timestamp = std::time::SystemTime::now();

    process_prekey_bundle(
        address,
        session_store,
        identity_store,
        bundle,
        timestamp,
        &mut rng,
        UsePQRatchet::Yes,
    ).await?;

    Ok(())
}

pub async fn establish_session_from_bundle_with_storage(
    address: &ProtocolAddress,
    bundle: &PreKeyBundle,
    storage: &mut SqliteStorage,
) -> Result<(), Box<dyn std::error::Error>> {
    establish_session_from_bundle(
        address,
        bundle,
        &mut storage.session_store,
        &mut storage.identity_store,
    ).await
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::keys::{generate_identity_key_pair, generate_pre_keys, generate_signed_pre_key};

    #[tokio::test]
    async fn test_establish_session_from_bundle() -> Result<(), Box<dyn std::error::Error>> {
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

        let storage = SqliteStorage::new(":memory:").await?;
        assert_eq!(storage.session_count().await, 0);

        let mut main_storage = SqliteStorage::new(":memory:").await?;

        let alice_identity = generate_identity_key_pair().await?;
        main_storage.set_local_identity_key_pair(&alice_identity).await?;
        main_storage.set_local_registration_id(12346).await?;

        establish_session_from_bundle(
            &bob_address,
            &bundle,
            &mut main_storage.session_store,
            &mut main_storage.identity_store
        ).await?;

        assert_eq!(main_storage.session_count().await, 1);
        let session = main_storage.load_session(&bob_address).await?;
        assert!(session.is_some());

        Ok(())
    }

    #[tokio::test]
    async fn test_establish_session_from_bundle_with_storage() -> Result<(), Box<dyn std::error::Error>> {
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

        let mut storage = SqliteStorage::new(":memory:").await?;
        let alice_identity = generate_identity_key_pair().await?;
        storage.set_local_identity_key_pair(&alice_identity).await?;
        storage.set_local_registration_id(12346).await?;

        assert_eq!(storage.session_count().await, 0);

        establish_session_from_bundle_with_storage(&bob_address, &bundle, &mut storage).await?;

        assert_eq!(storage.session_count().await, 1);
        let session = storage.load_session(&bob_address).await?;
        assert!(session.is_some());

        Ok(())
    }
}
