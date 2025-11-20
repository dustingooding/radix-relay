//! Key rotation functionality for Signal Protocol keys
//!
//! Implements periodic rotation of signed pre-keys and Kyber pre-keys,
//! plus consumption-based pre-key management following Signal Protocol security model.

use crate::keys::{generate_pre_keys, generate_signed_pre_key};
use crate::storage_trait::{
    ExtendedKyberPreKeyStore, ExtendedPreKeyStore, ExtendedSignedPreKeyStore,
    SignalStorageContainer,
};
use libsignal_protocol::*;

pub const MIN_PRE_KEY_COUNT: usize = 50;
pub const REPLENISH_COUNT: u32 = 100;
pub const ROTATION_INTERVAL_SECS: u64 = 7 * 24 * 60 * 60;
pub const GRACE_PERIOD_SECS: u64 = 7 * 24 * 60 * 60;

pub async fn rotate_signed_pre_key<S: SignalStorageContainer>(
    storage: &mut S,
    identity_key_pair: &IdentityKeyPair,
) -> Result<(), Box<dyn std::error::Error>> {
    let current_max_id = storage
        .signed_pre_key_store()
        .get_max_signed_pre_key_id()
        .await?
        .unwrap_or(0);

    let new_signed_pre_key = generate_signed_pre_key(identity_key_pair, current_max_id + 1).await?;

    storage
        .signed_pre_key_store()
        .save_signed_pre_key(new_signed_pre_key.id()?, &new_signed_pre_key)
        .await?;

    Ok(())
}

pub async fn signed_pre_key_needs_rotation<S: SignalStorageContainer>(
    storage: &mut S,
) -> Result<bool, Box<dyn std::error::Error>> {
    let max_id = storage
        .signed_pre_key_store()
        .get_max_signed_pre_key_id()
        .await?;

    let Some(id) = max_id else {
        return Ok(true);
    };

    let current_key = storage
        .signed_pre_key_store()
        .get_signed_pre_key(SignedPreKeyId::from(id))
        .await?;

    let key_timestamp = current_key.timestamp()?.epoch_millis();
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)?
        .as_millis() as u64;

    let key_age_secs = (now - key_timestamp) / 1000;
    Ok(key_age_secs > ROTATION_INTERVAL_SECS)
}

pub async fn cleanup_expired_signed_pre_keys<S: SignalStorageContainer>(
    storage: &mut S,
) -> Result<(), Box<dyn std::error::Error>> {
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)?
        .as_millis() as u64;

    let cutoff = now - (GRACE_PERIOD_SECS * 1000);

    let expired_ids = storage
        .signed_pre_key_store()
        .get_signed_pre_keys_older_than(cutoff)
        .await?;

    let current_count = storage.signed_pre_key_store().signed_pre_key_count().await;
    if current_count <= 1 {
        return Ok(());
    }

    for id in expired_ids {
        if storage.signed_pre_key_store().signed_pre_key_count().await > 1 {
            storage
                .signed_pre_key_store()
                .delete_signed_pre_key(id)
                .await?;
        }
    }

    Ok(())
}

pub async fn consume_pre_key<S: SignalStorageContainer>(
    storage: &mut S,
    pre_key_id: PreKeyId,
) -> Result<(), Box<dyn std::error::Error>> {
    storage.pre_key_store().delete_pre_key(pre_key_id).await?;

    let current_count = storage.pre_key_store().pre_key_count().await;
    if current_count < MIN_PRE_KEY_COUNT {
        replenish_pre_keys(storage).await?;
    }

    Ok(())
}

pub async fn replenish_pre_keys<S: SignalStorageContainer>(
    storage: &mut S,
) -> Result<(), Box<dyn std::error::Error>> {
    let next_id = storage
        .pre_key_store()
        .get_max_pre_key_id()
        .await?
        .map(|id| id + 1)
        .unwrap_or(1);

    let new_pre_keys = generate_pre_keys(next_id, REPLENISH_COUNT).await?;

    for (key_id, key_pair) in &new_pre_keys {
        let record = PreKeyRecord::new((*key_id).into(), key_pair);
        storage
            .pre_key_store()
            .save_pre_key((*key_id).into(), &record)
            .await?;
    }

    Ok(())
}

pub async fn rotate_kyber_pre_key<S: SignalStorageContainer>(
    storage: &mut S,
    identity_key_pair: &IdentityKeyPair,
) -> Result<(), Box<dyn std::error::Error>> {
    let current_max_id = storage
        .kyber_pre_key_store()
        .get_max_kyber_pre_key_id()
        .await?
        .unwrap_or(0);

    let mut rng = rand::rng();
    let kyber_keypair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
    let kyber_signature = identity_key_pair
        .private_key()
        .calculate_signature(&kyber_keypair.public_key.serialize(), &mut rng)?;

    let now = std::time::SystemTime::now();
    let kyber_record = KyberPreKeyRecord::new(
        KyberPreKeyId::from(current_max_id + 1),
        Timestamp::from_epoch_millis(now.duration_since(std::time::UNIX_EPOCH)?.as_millis() as u64),
        &kyber_keypair,
        &kyber_signature,
    );

    storage
        .kyber_pre_key_store()
        .save_kyber_pre_key(KyberPreKeyId::from(current_max_id + 1), &kyber_record)
        .await?;

    Ok(())
}

pub async fn kyber_pre_key_needs_rotation<S: SignalStorageContainer>(
    storage: &mut S,
) -> Result<bool, Box<dyn std::error::Error>> {
    let max_id = storage
        .kyber_pre_key_store()
        .get_max_kyber_pre_key_id()
        .await?;

    let Some(id) = max_id else {
        return Ok(true);
    };

    let current_key = storage
        .kyber_pre_key_store()
        .get_kyber_pre_key(KyberPreKeyId::from(id))
        .await?;

    let key_timestamp = current_key.timestamp()?.epoch_millis();
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)?
        .as_millis() as u64;

    let key_age_secs = (now - key_timestamp) / 1000;
    Ok(key_age_secs > ROTATION_INTERVAL_SECS)
}

pub async fn cleanup_expired_kyber_pre_keys<S: SignalStorageContainer>(
    storage: &mut S,
) -> Result<(), Box<dyn std::error::Error>> {
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)?
        .as_millis() as u64;

    let cutoff = now - (GRACE_PERIOD_SECS * 1000);

    let expired_ids = storage
        .kyber_pre_key_store()
        .get_kyber_pre_keys_older_than(cutoff)
        .await?;

    let current_count = storage.kyber_pre_key_store().kyber_pre_key_count().await;
    if current_count <= 1 {
        return Ok(());
    }

    for id in expired_ids {
        if storage.kyber_pre_key_store().kyber_pre_key_count().await > 1 {
            storage
                .kyber_pre_key_store()
                .delete_kyber_pre_key(id)
                .await?;
        }
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::keys::{generate_identity_key_pair, generate_signed_pre_key};
    use crate::sqlite_storage::SqliteStorage;
    use crate::storage_trait::{
        ExtendedIdentityStore, ExtendedKyberPreKeyStore, ExtendedPreKeyStore,
        ExtendedSignedPreKeyStore, SignalStorageContainer,
    };

    #[tokio::test]
    async fn test_rotate_signed_pre_key_generates_new_key() -> Result<(), Box<dyn std::error::Error>>
    {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        let identity_key_pair = generate_identity_key_pair().await?;
        storage
            .identity_store()
            .set_local_identity_key_pair(&identity_key_pair)
            .await?;

        let initial_key = generate_signed_pre_key(&identity_key_pair, 1).await?;
        storage
            .signed_pre_key_store()
            .save_signed_pre_key(initial_key.id()?, &initial_key)
            .await?;

        assert_eq!(
            storage.signed_pre_key_store().signed_pre_key_count().await,
            1
        );

        rotate_signed_pre_key(&mut storage, &identity_key_pair).await?;

        assert_eq!(
            storage.signed_pre_key_store().signed_pre_key_count().await,
            2
        );

        let max_id = storage
            .signed_pre_key_store()
            .get_max_signed_pre_key_id()
            .await?
            .expect("Should have max ID");
        assert_eq!(max_id, 2);

        Ok(())
    }

    #[tokio::test]
    async fn test_signed_pre_key_needs_rotation_when_old() -> Result<(), Box<dyn std::error::Error>>
    {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        let identity_key_pair = generate_identity_key_pair().await?;
        storage
            .identity_store()
            .set_local_identity_key_pair(&identity_key_pair)
            .await?;

        let old_timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)?
            .as_secs()
            - (8 * 24 * 60 * 60);

        let mut rng = rand::rng();
        let key_pair = KeyPair::generate(&mut rng);
        let signature = identity_key_pair
            .private_key()
            .calculate_signature(&key_pair.public_key.serialize(), &mut rng)?;
        let old_key = SignedPreKeyRecord::new(
            1.into(),
            Timestamp::from_epoch_millis(old_timestamp * 1000),
            &key_pair,
            &signature,
        );
        storage
            .signed_pre_key_store()
            .save_signed_pre_key(old_key.id()?, &old_key)
            .await?;

        let needs_rotation = signed_pre_key_needs_rotation(&mut storage).await?;
        assert!(
            needs_rotation,
            "Key older than rotation interval should need rotation"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_signed_pre_key_does_not_need_rotation_when_fresh(
    ) -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        let identity_key_pair = generate_identity_key_pair().await?;
        storage
            .identity_store()
            .set_local_identity_key_pair(&identity_key_pair)
            .await?;

        let fresh_key = generate_signed_pre_key(&identity_key_pair, 1).await?;
        storage
            .signed_pre_key_store()
            .save_signed_pre_key(fresh_key.id()?, &fresh_key)
            .await?;

        let needs_rotation = signed_pre_key_needs_rotation(&mut storage).await?;
        assert!(!needs_rotation, "Fresh key should not need rotation");

        Ok(())
    }

    #[tokio::test]
    async fn test_cleanup_expired_signed_pre_keys() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        let identity_key_pair = generate_identity_key_pair().await?;
        storage
            .identity_store()
            .set_local_identity_key_pair(&identity_key_pair)
            .await?;

        let old_timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)?
            .as_secs()
            - (14 * 24 * 60 * 60);

        let mut rng = rand::rng();
        let key_pair = KeyPair::generate(&mut rng);
        let signature = identity_key_pair
            .private_key()
            .calculate_signature(&key_pair.public_key.serialize(), &mut rng)?;
        let expired_key = SignedPreKeyRecord::new(
            1.into(),
            Timestamp::from_epoch_millis(old_timestamp * 1000),
            &key_pair,
            &signature,
        );
        storage
            .signed_pre_key_store()
            .save_signed_pre_key(expired_key.id()?, &expired_key)
            .await?;

        let fresh_key = generate_signed_pre_key(&identity_key_pair, 2).await?;
        storage
            .signed_pre_key_store()
            .save_signed_pre_key(fresh_key.id()?, &fresh_key)
            .await?;

        assert_eq!(
            storage.signed_pre_key_store().signed_pre_key_count().await,
            2
        );

        cleanup_expired_signed_pre_keys(&mut storage).await?;

        assert_eq!(
            storage.signed_pre_key_store().signed_pre_key_count().await,
            1
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_consume_pre_key_removes_key() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        let mut rng = rand::rng();
        for i in 1..=51 {
            let key_pair = KeyPair::generate(&mut rng);
            let record = PreKeyRecord::new(i.into(), &key_pair);
            storage
                .pre_key_store()
                .save_pre_key(i.into(), &record)
                .await?;
        }

        assert_eq!(storage.pre_key_store().pre_key_count().await, 51);

        consume_pre_key(&mut storage, PreKeyId::from(1)).await?;

        assert_eq!(storage.pre_key_store().pre_key_count().await, 50);

        Ok(())
    }

    #[tokio::test]
    async fn test_replenish_pre_keys_when_low() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        assert_eq!(storage.pre_key_store().pre_key_count().await, 0);

        replenish_pre_keys(&mut storage).await?;

        assert_eq!(
            storage.pre_key_store().pre_key_count().await,
            REPLENISH_COUNT as usize
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_consume_pre_key_triggers_replenishment() -> Result<(), Box<dyn std::error::Error>>
    {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        let mut rng = rand::rng();
        for i in 1..=49 {
            let key_pair = KeyPair::generate(&mut rng);
            let record = PreKeyRecord::new(i.into(), &key_pair);
            storage
                .pre_key_store()
                .save_pre_key(i.into(), &record)
                .await?;
        }

        assert_eq!(storage.pre_key_store().pre_key_count().await, 49);

        consume_pre_key(&mut storage, PreKeyId::from(1)).await?;

        assert!(storage.pre_key_store().pre_key_count().await >= MIN_PRE_KEY_COUNT);

        Ok(())
    }

    #[tokio::test]
    async fn test_rotate_kyber_pre_key_generates_new_key() -> Result<(), Box<dyn std::error::Error>>
    {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        let identity_key_pair = generate_identity_key_pair().await?;
        storage
            .identity_store()
            .set_local_identity_key_pair(&identity_key_pair)
            .await?;

        let mut rng = rand::rng();
        let kyber_keypair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
        let kyber_signature = identity_key_pair
            .private_key()
            .calculate_signature(&kyber_keypair.public_key.serialize(), &mut rng)?;
        let now = std::time::SystemTime::now();
        let initial_key = KyberPreKeyRecord::new(
            KyberPreKeyId::from(1u32),
            Timestamp::from_epoch_millis(
                now.duration_since(std::time::UNIX_EPOCH)?.as_millis() as u64
            ),
            &kyber_keypair,
            &kyber_signature,
        );
        storage
            .kyber_pre_key_store()
            .save_kyber_pre_key(KyberPreKeyId::from(1u32), &initial_key)
            .await?;

        assert_eq!(storage.kyber_pre_key_store().kyber_pre_key_count().await, 1);

        rotate_kyber_pre_key(&mut storage, &identity_key_pair).await?;

        assert_eq!(storage.kyber_pre_key_store().kyber_pre_key_count().await, 2);

        let max_id = storage
            .kyber_pre_key_store()
            .get_max_kyber_pre_key_id()
            .await?
            .expect("Should have max ID");
        assert_eq!(max_id, 2);

        Ok(())
    }

    #[tokio::test]
    async fn test_kyber_pre_key_needs_rotation_when_old() -> Result<(), Box<dyn std::error::Error>>
    {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        let identity_key_pair = generate_identity_key_pair().await?;
        storage
            .identity_store()
            .set_local_identity_key_pair(&identity_key_pair)
            .await?;

        // Create a Kyber key with an old timestamp (8 days ago)
        let old_timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)?
            .as_secs()
            - (8 * 24 * 60 * 60);

        let mut rng = rand::rng();
        let kyber_keypair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
        let kyber_signature = identity_key_pair
            .private_key()
            .calculate_signature(&kyber_keypair.public_key.serialize(), &mut rng)?;
        let old_key = KyberPreKeyRecord::new(
            KyberPreKeyId::from(1u32),
            Timestamp::from_epoch_millis(old_timestamp * 1000),
            &kyber_keypair,
            &kyber_signature,
        );
        storage
            .kyber_pre_key_store()
            .save_kyber_pre_key(KyberPreKeyId::from(1u32), &old_key)
            .await?;

        let needs_rotation = kyber_pre_key_needs_rotation(&mut storage).await?;
        assert!(
            needs_rotation,
            "Kyber key older than rotation interval should need rotation"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_kyber_pre_key_does_not_need_rotation_when_fresh(
    ) -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        let identity_key_pair = generate_identity_key_pair().await?;
        storage
            .identity_store()
            .set_local_identity_key_pair(&identity_key_pair)
            .await?;

        // Create a fresh Kyber key
        let mut rng = rand::rng();
        let kyber_keypair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
        let kyber_signature = identity_key_pair
            .private_key()
            .calculate_signature(&kyber_keypair.public_key.serialize(), &mut rng)?;
        let now = std::time::SystemTime::now();
        let fresh_key = KyberPreKeyRecord::new(
            KyberPreKeyId::from(1u32),
            Timestamp::from_epoch_millis(
                now.duration_since(std::time::UNIX_EPOCH)?.as_millis() as u64
            ),
            &kyber_keypair,
            &kyber_signature,
        );
        storage
            .kyber_pre_key_store()
            .save_kyber_pre_key(KyberPreKeyId::from(1u32), &fresh_key)
            .await?;

        let needs_rotation = kyber_pre_key_needs_rotation(&mut storage).await?;
        assert!(!needs_rotation, "Fresh Kyber key should not need rotation");

        Ok(())
    }

    #[tokio::test]
    async fn test_cleanup_expired_kyber_pre_keys() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        let identity_key_pair = generate_identity_key_pair().await?;
        storage
            .identity_store()
            .set_local_identity_key_pair(&identity_key_pair)
            .await?;

        // Create an expired Kyber key (14 days old, past grace period)
        let old_timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)?
            .as_secs()
            - (14 * 24 * 60 * 60);

        let mut rng = rand::rng();
        let kyber_keypair1 = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
        let kyber_signature1 = identity_key_pair
            .private_key()
            .calculate_signature(&kyber_keypair1.public_key.serialize(), &mut rng)?;
        let expired_key = KyberPreKeyRecord::new(
            KyberPreKeyId::from(1u32),
            Timestamp::from_epoch_millis(old_timestamp * 1000),
            &kyber_keypair1,
            &kyber_signature1,
        );
        storage
            .kyber_pre_key_store()
            .save_kyber_pre_key(KyberPreKeyId::from(1u32), &expired_key)
            .await?;

        // Create a fresh Kyber key
        let kyber_keypair2 = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
        let kyber_signature2 = identity_key_pair
            .private_key()
            .calculate_signature(&kyber_keypair2.public_key.serialize(), &mut rng)?;
        let now = std::time::SystemTime::now();
        let fresh_key = KyberPreKeyRecord::new(
            KyberPreKeyId::from(2u32),
            Timestamp::from_epoch_millis(
                now.duration_since(std::time::UNIX_EPOCH)?.as_millis() as u64
            ),
            &kyber_keypair2,
            &kyber_signature2,
        );
        storage
            .kyber_pre_key_store()
            .save_kyber_pre_key(KyberPreKeyId::from(2u32), &fresh_key)
            .await?;

        assert_eq!(storage.kyber_pre_key_store().kyber_pre_key_count().await, 2);

        cleanup_expired_kyber_pre_keys(&mut storage).await?;

        assert_eq!(storage.kyber_pre_key_store().kyber_pre_key_count().await, 1);

        Ok(())
    }
}
