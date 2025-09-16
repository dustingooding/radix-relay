//! Signal Protocol integration for Radix Relay
//!
//! This crate provides a bridge between Radix Relay's C++ transport layer
//! and the official Signal Protocol Rust implementation for end-to-end encryption.

mod storage;

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

    #[test]
    fn test_identity_key_generation() -> Result<(), SignalProtocolError> {
        let mut rng = rand::rng();
        let identity_key_pair = IdentityKeyPair::generate(&mut rng);

        let identity_key = identity_key_pair.identity_key();
        let public_key_bytes = identity_key.public_key().serialize();
        assert_eq!(public_key_bytes.len(), 33);

        let private_key_bytes = identity_key_pair.private_key().serialize();
        assert_eq!(private_key_bytes.len(), 32);

        assert!(!public_key_bytes.iter().all(|&x| x == 0));
        assert!(!private_key_bytes.iter().all(|&x| x == 0));

        Ok(())
    }

    #[tokio::test]
    async fn test_identity_key_store_save_identity() -> Result<(), Box<dyn std::error::Error>> {
        use crate::storage::SqliteStorage;

        let storage = SqliteStorage::new(":memory:").await?;
        let mut rng = rand::rng();
        let identity_key_pair = IdentityKeyPair::generate(&mut rng);
        let address = ProtocolAddress::new("test_user".to_string(), DeviceId::new(1)?);

        assert_eq!(storage.identity_count().await, 0);

        let result = storage.save_identity(&address, identity_key_pair.identity_key()).await;
        assert!(result.is_ok());

        assert_eq!(storage.identity_count().await, 1);

        Ok(())
    }

    #[tokio::test]
    async fn test_identity_key_store_get_identity_nonexistent() -> Result<(), Box<dyn std::error::Error>> {
        use crate::storage::SqliteStorage;

        let storage = SqliteStorage::new(":memory:").await?;
        let address = ProtocolAddress::new("nonexistent_user".to_string(), DeviceId::new(1)?);

        let retrieved = storage.get_identity(&address).await?;
        assert!(retrieved.is_none());

        Ok(())
    }

    #[tokio::test]
    async fn test_identity_key_store_save_and_get() -> Result<(), Box<dyn std::error::Error>> {
        use crate::storage::SqliteStorage;

        let storage = SqliteStorage::new(":memory:").await?;
        let mut rng = rand::rng();
        let identity_key_pair = IdentityKeyPair::generate(&mut rng);
        let address = ProtocolAddress::new("test_user".to_string(), DeviceId::new(1)?);

        storage.save_identity(&address, identity_key_pair.identity_key()).await?;
        let retrieved = storage.get_identity(&address).await?;

        assert!(retrieved.is_some());
        assert_eq!(retrieved.unwrap().serialize(), identity_key_pair.identity_key().serialize());

        Ok(())
    }

    #[tokio::test]
    async fn test_pre_key_store_save_pre_key() -> Result<(), Box<dyn std::error::Error>> {
        use crate::storage::SqliteStorage;

        let storage = SqliteStorage::new(":memory:").await?;
        let mut rng = rand::rng();
        let key_pair = KeyPair::generate(&mut rng);
        let pre_key_id = 42;

        assert_eq!(storage.pre_key_count().await, 0);

        let result = storage.save_pre_key(pre_key_id, &key_pair).await;
        assert!(result.is_ok());

        assert_eq!(storage.pre_key_count().await, 1);

        Ok(())
    }

    #[tokio::test]
    async fn test_pre_key_store_get_nonexistent() -> Result<(), Box<dyn std::error::Error>> {
        use crate::storage::SqliteStorage;

        let storage = SqliteStorage::new(":memory:").await?;
        let pre_key_id = 999;

        let retrieved = storage.get_pre_key(pre_key_id).await?;
        assert!(retrieved.is_none());

        Ok(())
    }

    #[tokio::test]
    async fn test_pre_key_store_save_and_get() -> Result<(), Box<dyn std::error::Error>> {
        use crate::storage::SqliteStorage;

        let storage = SqliteStorage::new(":memory:").await?;
        let mut rng = rand::rng();
        let key_pair = KeyPair::generate(&mut rng);
        let pre_key_id = 42;

        storage.save_pre_key(pre_key_id, &key_pair).await?;
        let retrieved = storage.get_pre_key(pre_key_id).await?;

        assert!(retrieved.is_some());
        assert_eq!(retrieved.unwrap().public_key.serialize(), key_pair.public_key.serialize());

        Ok(())
    }

    #[tokio::test]
    async fn test_signed_pre_key_store_save_signed_pre_key() -> Result<(), Box<dyn std::error::Error>> {
        use crate::storage::SqliteStorage;

        let storage = SqliteStorage::new(":memory:").await?;
        let mut rng = rand::rng();
        let identity_key_pair = IdentityKeyPair::generate(&mut rng);
        let signed_pre_key_id = 1;
        let timestamp = Timestamp::from_epoch_millis(12345);
        let key_pair = KeyPair::generate(&mut rng);
        let signature = identity_key_pair.private_key().calculate_signature(&key_pair.public_key.serialize(), &mut rng)?;
        let signed_pre_key = SignedPreKeyRecord::new(signed_pre_key_id.into(), timestamp, &key_pair, &signature);

        assert_eq!(storage.signed_pre_key_count().await, 0);

        let result = storage.save_signed_pre_key(signed_pre_key_id, &signed_pre_key).await;
        assert!(result.is_ok());

        assert_eq!(storage.signed_pre_key_count().await, 1);

        Ok(())
    }

    #[tokio::test]
    async fn test_signed_pre_key_store_get_nonexistent() -> Result<(), Box<dyn std::error::Error>> {
        use crate::storage::SqliteStorage;

        let storage = SqliteStorage::new(":memory:").await?;
        let signed_pre_key_id = 999;

        let retrieved = storage.get_signed_pre_key(signed_pre_key_id).await?;
        assert!(retrieved.is_none());

        Ok(())
    }

    #[tokio::test]
    async fn test_signed_pre_key_store_save_and_get() -> Result<(), Box<dyn std::error::Error>> {
        use crate::storage::SqliteStorage;

        let storage = SqliteStorage::new(":memory:").await?;
        let mut rng = rand::rng();
        let identity_key_pair = IdentityKeyPair::generate(&mut rng);
        let signed_pre_key_id = 1;
        let timestamp = Timestamp::from_epoch_millis(12345);
        let key_pair = KeyPair::generate(&mut rng);
        let signature = identity_key_pair.private_key().calculate_signature(&key_pair.public_key.serialize(), &mut rng)?;
        let signed_pre_key = SignedPreKeyRecord::new(signed_pre_key_id.into(), timestamp, &key_pair, &signature);

        storage.save_signed_pre_key(signed_pre_key_id, &signed_pre_key).await?;
        let retrieved = storage.get_signed_pre_key(signed_pre_key_id).await?;

        assert!(retrieved.is_some());
        assert_eq!(retrieved.unwrap().id()?, signed_pre_key_id.into());

        Ok(())
    }
}
