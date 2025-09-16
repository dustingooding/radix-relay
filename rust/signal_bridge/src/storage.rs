//! Storage implementation for libsignal
//!
//! This module provides storage traits required by libsignal for persisting
//! cryptographic keys and session data.

use libsignal_protocol::*;
use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::Mutex;

pub struct SqliteStorage {
    identity_keys: Arc<Mutex<HashMap<String, IdentityKey>>>,
    pre_keys: Arc<Mutex<HashMap<u32, KeyPair>>>,
    signed_pre_keys: Arc<Mutex<HashMap<u32, SignedPreKeyRecord>>>,
    sessions: Arc<Mutex<HashMap<String, SessionRecord>>>,
}

impl SqliteStorage {
    pub async fn new(_db_path: &str) -> Result<Self, Box<dyn std::error::Error>> {
        Ok(Self {
            identity_keys: Arc::new(Mutex::new(HashMap::new())),
            pre_keys: Arc::new(Mutex::new(HashMap::new())),
            signed_pre_keys: Arc::new(Mutex::new(HashMap::new())),
            sessions: Arc::new(Mutex::new(HashMap::new())),
        })
    }

    pub async fn save_identity(
        &self,
        address: &ProtocolAddress,
        identity_key: &IdentityKey,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let key = format!("{}:{}", address.name(), u32::from(address.device_id()));
        let mut store = self.identity_keys.lock().await;
        store.insert(key, *identity_key);
        Ok(())
    }

    pub async fn get_identity(
        &self,
        address: &ProtocolAddress,
    ) -> Result<Option<IdentityKey>, Box<dyn std::error::Error>> {
        let key = format!("{}:{}", address.name(), u32::from(address.device_id()));
        let store = self.identity_keys.lock().await;
        Ok(store.get(&key).copied())
    }

    pub async fn identity_count(&self) -> usize {
        let store = self.identity_keys.lock().await;
        store.len()
    }

    pub async fn save_pre_key(
        &self,
        pre_key_id: u32,
        key_pair: &KeyPair,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let mut store = self.pre_keys.lock().await;
        store.insert(pre_key_id, *key_pair);
        Ok(())
    }

    pub async fn get_pre_key(
        &self,
        pre_key_id: u32,
    ) -> Result<Option<KeyPair>, Box<dyn std::error::Error>> {
        let store = self.pre_keys.lock().await;
        Ok(store.get(&pre_key_id).copied())
    }

    pub async fn pre_key_count(&self) -> usize {
        let store = self.pre_keys.lock().await;
        store.len()
    }

    pub async fn save_signed_pre_key(
        &self,
        signed_pre_key_id: u32,
        signed_pre_key: &SignedPreKeyRecord,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let mut store = self.signed_pre_keys.lock().await;
        store.insert(signed_pre_key_id, signed_pre_key.clone());
        Ok(())
    }

    pub async fn get_signed_pre_key(
        &self,
        signed_pre_key_id: u32,
    ) -> Result<Option<SignedPreKeyRecord>, Box<dyn std::error::Error>> {
        let store = self.signed_pre_keys.lock().await;
        Ok(store.get(&signed_pre_key_id).cloned())
    }

    pub async fn signed_pre_key_count(&self) -> usize {
        let store = self.signed_pre_keys.lock().await;
        store.len()
    }

    pub async fn store_session(
        &self,
        address: &ProtocolAddress,
        session_record: &SessionRecord,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let key = format!("{}:{}", address.name(), u32::from(address.device_id()));
        let mut store = self.sessions.lock().await;
        store.insert(key, session_record.clone());
        Ok(())
    }

    pub async fn load_session(
        &self,
        address: &ProtocolAddress,
    ) -> Result<Option<SessionRecord>, Box<dyn std::error::Error>> {
        let key = format!("{}:{}", address.name(), u32::from(address.device_id()));
        let store = self.sessions.lock().await;
        Ok(store.get(&key).cloned())
    }

    pub async fn session_count(&self) -> usize {
        let store = self.sessions.lock().await;
        store.len()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_identity_key_store_save_identity() -> Result<(), Box<dyn std::error::Error>> {
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
        let storage = SqliteStorage::new(":memory:").await?;
        let address = ProtocolAddress::new("nonexistent_user".to_string(), DeviceId::new(1)?);

        let retrieved = storage.get_identity(&address).await?;
        assert!(retrieved.is_none());

        Ok(())
    }

    #[tokio::test]
    async fn test_identity_key_store_save_and_get() -> Result<(), Box<dyn std::error::Error>> {
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
        let storage = SqliteStorage::new(":memory:").await?;
        let pre_key_id = 999;

        let retrieved = storage.get_pre_key(pre_key_id).await?;
        assert!(retrieved.is_none());

        Ok(())
    }

    #[tokio::test]
    async fn test_pre_key_store_save_and_get() -> Result<(), Box<dyn std::error::Error>> {
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
        let storage = SqliteStorage::new(":memory:").await?;
        let signed_pre_key_id = 999;

        let retrieved = storage.get_signed_pre_key(signed_pre_key_id).await?;
        assert!(retrieved.is_none());

        Ok(())
    }

    #[tokio::test]
    async fn test_signed_pre_key_store_save_and_get() -> Result<(), Box<dyn std::error::Error>> {
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

    #[tokio::test]
    async fn test_session_store_save_session() -> Result<(), Box<dyn std::error::Error>> {
        let storage = SqliteStorage::new(":memory:").await?;
        let address = ProtocolAddress::new("test_user".to_string(), DeviceId::new(1)?);
        let session_record = SessionRecord::new_fresh();

        assert_eq!(storage.session_count().await, 0);

        let result = storage.store_session(&address, &session_record).await;
        assert!(result.is_ok());

        assert_eq!(storage.session_count().await, 1);

        Ok(())
    }

    #[tokio::test]
    async fn test_session_store_get_nonexistent() -> Result<(), Box<dyn std::error::Error>> {
        let storage = SqliteStorage::new(":memory:").await?;
        let address = ProtocolAddress::new("nonexistent_user".to_string(), DeviceId::new(1)?);

        let retrieved = storage.load_session(&address).await?;
        assert!(retrieved.is_none());

        Ok(())
    }

    #[tokio::test]
    async fn test_session_store_save_and_get() -> Result<(), Box<dyn std::error::Error>> {
        let storage = SqliteStorage::new(":memory:").await?;
        let address = ProtocolAddress::new("test_user".to_string(), DeviceId::new(1)?);
        let session_record = SessionRecord::new_fresh();

        storage.store_session(&address, &session_record).await?;
        let retrieved = storage.load_session(&address).await?;

        assert!(retrieved.is_some());

        Ok(())
    }
}
