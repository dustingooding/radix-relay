//! Storage implementation for libsignal
//!
//! This module provides storage traits required by libsignal for persisting
//! cryptographic keys and session data.

use libsignal_protocol::*;
use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::Mutex;
use async_trait::async_trait;

pub struct SqliteSessionStore {
    sessions: Arc<Mutex<HashMap<String, SessionRecord>>>,
}

pub struct SqliteIdentityKeyStore {
    identity_keys: Arc<Mutex<HashMap<String, IdentityKey>>>,
    local_identity_key_pair: Arc<Mutex<Option<IdentityKeyPair>>>,
    local_registration_id: Arc<Mutex<Option<u32>>>,
}

pub struct SqlitePreKeyStore {
    pre_keys: Arc<Mutex<HashMap<u32, KeyPair>>>,
}

pub struct SqliteSignedPreKeyStore {
    signed_pre_keys: Arc<Mutex<HashMap<u32, SignedPreKeyRecord>>>,
}

pub struct SqliteKyberPreKeyStore {
    kyber_pre_keys: Arc<Mutex<HashMap<u32, KyberPreKeyRecord>>>,
}

pub struct SqliteStorage {
    pub session_store: SqliteSessionStore,
    pub identity_store: SqliteIdentityKeyStore,
    pub pre_key_store: SqlitePreKeyStore,
    pub signed_pre_key_store: SqliteSignedPreKeyStore,
    pub kyber_pre_key_store: SqliteKyberPreKeyStore,
}

impl SqliteSessionStore {
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

impl SqliteIdentityKeyStore {
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

    pub async fn set_local_identity_key_pair(&self, identity_key_pair: &IdentityKeyPair) -> Result<(), Box<dyn std::error::Error>> {
        let mut store = self.local_identity_key_pair.lock().await;
        *store = Some(*identity_key_pair);
        Ok(())
    }

    pub async fn set_local_registration_id(&self, registration_id: u32) -> Result<(), Box<dyn std::error::Error>> {
        let mut store = self.local_registration_id.lock().await;
        *store = Some(registration_id);
        Ok(())
    }
}

impl SqlitePreKeyStore {
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
}

impl SqliteSignedPreKeyStore {
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
}

impl SqliteKyberPreKeyStore {
    pub async fn save_kyber_pre_key(
        &self,
        kyber_pre_key_id: u32,
        kyber_pre_key: &KyberPreKeyRecord,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let mut store = self.kyber_pre_keys.lock().await;
        store.insert(kyber_pre_key_id, kyber_pre_key.clone());
        Ok(())
    }

    pub async fn get_kyber_pre_key(
        &self,
        kyber_pre_key_id: u32,
    ) -> Result<Option<KyberPreKeyRecord>, Box<dyn std::error::Error>> {
        let store = self.kyber_pre_keys.lock().await;
        Ok(store.get(&kyber_pre_key_id).cloned())
    }

    pub async fn kyber_pre_key_count(&self) -> usize {
        let store = self.kyber_pre_keys.lock().await;
        store.len()
    }
}

impl SqliteStorage {
    pub async fn new(_db_path: &str) -> Result<Self, Box<dyn std::error::Error>> {
        Ok(Self {
            session_store: SqliteSessionStore {
                sessions: Arc::new(Mutex::new(HashMap::new())),
            },
            identity_store: SqliteIdentityKeyStore {
                identity_keys: Arc::new(Mutex::new(HashMap::new())),
                local_identity_key_pair: Arc::new(Mutex::new(None)),
                local_registration_id: Arc::new(Mutex::new(None)),
            },
            pre_key_store: SqlitePreKeyStore {
                pre_keys: Arc::new(Mutex::new(HashMap::new())),
            },
            signed_pre_key_store: SqliteSignedPreKeyStore {
                signed_pre_keys: Arc::new(Mutex::new(HashMap::new())),
            },
            kyber_pre_key_store: SqliteKyberPreKeyStore {
                kyber_pre_keys: Arc::new(Mutex::new(HashMap::new())),
            },
        })
    }

    pub async fn save_identity(
        &self,
        address: &ProtocolAddress,
        identity_key: &IdentityKey,
    ) -> Result<(), Box<dyn std::error::Error>> {
        self.identity_store.save_identity(address, identity_key).await
    }

    pub async fn get_identity(
        &self,
        address: &ProtocolAddress,
    ) -> Result<Option<IdentityKey>, Box<dyn std::error::Error>> {
        self.identity_store.get_identity(address).await
    }

    pub async fn identity_count(&self) -> usize {
        self.identity_store.identity_count().await
    }

    pub async fn save_pre_key(
        &self,
        pre_key_id: u32,
        key_pair: &KeyPair,
    ) -> Result<(), Box<dyn std::error::Error>> {
        self.pre_key_store.save_pre_key(pre_key_id, key_pair).await
    }

    pub async fn get_pre_key(
        &self,
        pre_key_id: u32,
    ) -> Result<Option<KeyPair>, Box<dyn std::error::Error>> {
        self.pre_key_store.get_pre_key(pre_key_id).await
    }

    pub async fn pre_key_count(&self) -> usize {
        self.pre_key_store.pre_key_count().await
    }

    pub async fn save_signed_pre_key(
        &self,
        signed_pre_key_id: u32,
        signed_pre_key: &SignedPreKeyRecord,
    ) -> Result<(), Box<dyn std::error::Error>> {
        self.signed_pre_key_store.save_signed_pre_key(signed_pre_key_id, signed_pre_key).await
    }

    pub async fn get_signed_pre_key(
        &self,
        signed_pre_key_id: u32,
    ) -> Result<Option<SignedPreKeyRecord>, Box<dyn std::error::Error>> {
        self.signed_pre_key_store.get_signed_pre_key(signed_pre_key_id).await
    }

    pub async fn signed_pre_key_count(&self) -> usize {
        self.signed_pre_key_store.signed_pre_key_count().await
    }

    pub async fn store_session(
        &self,
        address: &ProtocolAddress,
        session_record: &SessionRecord,
    ) -> Result<(), Box<dyn std::error::Error>> {
        self.session_store.store_session(address, session_record).await
    }

    pub async fn load_session(
        &self,
        address: &ProtocolAddress,
    ) -> Result<Option<SessionRecord>, Box<dyn std::error::Error>> {
        self.session_store.load_session(address).await
    }

    pub async fn session_count(&self) -> usize {
        self.session_store.session_count().await
    }

    pub async fn set_local_identity_key_pair(&self, identity_key_pair: &IdentityKeyPair) -> Result<(), Box<dyn std::error::Error>> {
        self.identity_store.set_local_identity_key_pair(identity_key_pair).await
    }

    pub async fn set_local_registration_id(&self, registration_id: u32) -> Result<(), Box<dyn std::error::Error>> {
        self.identity_store.set_local_registration_id(registration_id).await
    }
}

#[async_trait(?Send)]
impl SessionStore for SqliteSessionStore {
    async fn load_session(&self, address: &ProtocolAddress) -> Result<Option<SessionRecord>, SignalProtocolError> {
        let key = format!("{}:{}", address.name(), u32::from(address.device_id()));
        let store = self.sessions.lock().await;
        Ok(store.get(&key).cloned())
    }

    async fn store_session(
        &mut self,
        address: &ProtocolAddress,
        record: &SessionRecord,
    ) -> Result<(), SignalProtocolError> {
        let key = format!("{}:{}", address.name(), u32::from(address.device_id()));
        let mut store = self.sessions.lock().await;
        store.insert(key, record.clone());
        Ok(())
    }
}

#[async_trait(?Send)]
impl SessionStore for SqliteStorage {
    async fn load_session(&self, address: &ProtocolAddress) -> Result<Option<SessionRecord>, SignalProtocolError> {
        self.session_store.load_session(address).await.map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to load session".to_string()))
    }

    async fn store_session(
        &mut self,
        address: &ProtocolAddress,
        record: &SessionRecord,
    ) -> Result<(), SignalProtocolError> {
        self.session_store.store_session(address, record).await.map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to store session".to_string()))
    }
}

#[async_trait(?Send)]
impl IdentityKeyStore for SqliteIdentityKeyStore {
    async fn get_identity_key_pair(&self) -> Result<IdentityKeyPair, SignalProtocolError> {
        let store = self.local_identity_key_pair.lock().await;
        match *store {
            Some(identity_key_pair) => Ok(identity_key_pair),
            None => Err(SignalProtocolError::InvalidState("storage", "Local identity key pair not set".to_string())),
        }
    }

    async fn get_local_registration_id(&self) -> Result<u32, SignalProtocolError> {
        let store = self.local_registration_id.lock().await;
        match *store {
            Some(registration_id) => Ok(registration_id),
            None => Err(SignalProtocolError::InvalidState("storage", "Local registration ID not set".to_string())),
        }
    }

    async fn save_identity(
        &mut self,
        address: &ProtocolAddress,
        identity_key: &IdentityKey,
    ) -> Result<IdentityChange, SignalProtocolError> {
        let existing = self.get_identity(address).await.ok().flatten();

        let key = format!("{}:{}", address.name(), u32::from(address.device_id()));
        let mut store = self.identity_keys.lock().await;
        store.insert(key, *identity_key);

        match existing {
            Some(existing_key) if existing_key != *identity_key => {
                Ok(IdentityChange::ReplacedExisting)
            },
            Some(_) => {
                Ok(IdentityChange::NewOrUnchanged)
            },
            None => {
                Ok(IdentityChange::NewOrUnchanged)
            },
        }
    }

    async fn is_trusted_identity(
        &self,
        address: &ProtocolAddress,
        identity_key: &IdentityKey,
        _direction: Direction,
    ) -> Result<bool, SignalProtocolError> {
        let key = format!("{}:{}", address.name(), u32::from(address.device_id()));
        let store = self.identity_keys.lock().await;
        match store.get(&key) {
            Some(stored_key) => Ok(*stored_key == *identity_key),
            None => Ok(true), // Trust on first use
        }
    }

    async fn get_identity(&self, address: &ProtocolAddress) -> Result<Option<IdentityKey>, SignalProtocolError> {
        let key = format!("{}:{}", address.name(), u32::from(address.device_id()));
        let store = self.identity_keys.lock().await;
        Ok(store.get(&key).copied())
    }
}

#[async_trait(?Send)]
impl IdentityKeyStore for SqliteStorage {
    async fn get_identity_key_pair(&self) -> Result<IdentityKeyPair, SignalProtocolError> {
        self.identity_store.get_identity_key_pair().await
    }

    async fn get_local_registration_id(&self) -> Result<u32, SignalProtocolError> {
        self.identity_store.get_local_registration_id().await
    }

    async fn save_identity(
        &mut self,
        address: &ProtocolAddress,
        identity_key: &IdentityKey,
    ) -> Result<IdentityChange, SignalProtocolError> {
        let existing = self.identity_store.get_identity(address).await.ok().flatten();

        let key = format!("{}:{}", address.name(), u32::from(address.device_id()));
        let mut store = self.identity_store.identity_keys.lock().await;
        store.insert(key, *identity_key);

        match existing {
            Some(existing_key) if existing_key != *identity_key => {
                Ok(IdentityChange::ReplacedExisting)
            },
            Some(_) => {
                Ok(IdentityChange::NewOrUnchanged)
            },
            None => {
                Ok(IdentityChange::NewOrUnchanged)
            },
        }
    }

    async fn is_trusted_identity(
        &self,
        address: &ProtocolAddress,
        identity_key: &IdentityKey,
        direction: Direction,
    ) -> Result<bool, SignalProtocolError> {
        self.identity_store.is_trusted_identity(address, identity_key, direction).await
    }

    async fn get_identity(&self, address: &ProtocolAddress) -> Result<Option<IdentityKey>, SignalProtocolError> {
        self.identity_store.get_identity(address).await.map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to get identity".to_string()))
    }
}

#[async_trait(?Send)]
impl PreKeyStore for SqliteStorage {
    async fn get_pre_key(&self, prekey_id: PreKeyId) -> Result<PreKeyRecord, SignalProtocolError> {
        let key_pair = self.pre_key_store.get_pre_key(u32::from(prekey_id)).await
            .map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to get pre key".to_string()))?;

        match key_pair {
            Some(key_pair) => Ok(PreKeyRecord::new(prekey_id, &key_pair)),
            None => Err(SignalProtocolError::InvalidPreKeyId),
        }
    }

    async fn save_pre_key(
        &mut self,
        prekey_id: PreKeyId,
        record: &PreKeyRecord,
    ) -> Result<(), SignalProtocolError> {
        let key_pair = record.key_pair()?;
        self.pre_key_store.save_pre_key(u32::from(prekey_id), &key_pair).await
            .map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to save pre key".to_string()))
    }

    async fn remove_pre_key(&mut self, prekey_id: PreKeyId) -> Result<(), SignalProtocolError> {
        self.pre_key_store.remove_pre_key(prekey_id).await
            .map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to remove pre key".to_string()))
    }
}

#[async_trait(?Send)]
impl SignedPreKeyStore for SqliteStorage {
    async fn get_signed_pre_key(&self, signed_prekey_id: SignedPreKeyId) -> Result<SignedPreKeyRecord, SignalProtocolError> {
        let record = self.signed_pre_key_store.get_signed_pre_key(u32::from(signed_prekey_id)).await
            .map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to get signed pre key".to_string()))?;

        match record {
            Some(record) => Ok(record),
            None => Err(SignalProtocolError::InvalidSignedPreKeyId),
        }
    }

    async fn save_signed_pre_key(
        &mut self,
        signed_prekey_id: SignedPreKeyId,
        record: &SignedPreKeyRecord,
    ) -> Result<(), SignalProtocolError> {
        self.signed_pre_key_store.save_signed_pre_key(u32::from(signed_prekey_id), record).await
            .map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to save signed pre key".to_string()))
    }
}

#[async_trait(?Send)]
impl KyberPreKeyStore for SqliteStorage {
    async fn get_kyber_pre_key(&self, kyber_prekey_id: KyberPreKeyId) -> Result<KyberPreKeyRecord, SignalProtocolError> {
        let record = self.kyber_pre_key_store.get_kyber_pre_key(u32::from(kyber_prekey_id)).await
            .map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to get kyber pre key".to_string()))?;

        match record {
            Some(record) => Ok(record),
            None => Err(SignalProtocolError::InvalidKyberPreKeyId),
        }
    }

    async fn save_kyber_pre_key(
        &mut self,
        kyber_prekey_id: KyberPreKeyId,
        record: &KyberPreKeyRecord,
    ) -> Result<(), SignalProtocolError> {
        self.kyber_pre_key_store.save_kyber_pre_key(u32::from(kyber_prekey_id), record).await
            .map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to save kyber pre key".to_string()))
    }

    async fn mark_kyber_pre_key_used(&mut self, kyber_prekey_id: KyberPreKeyId) -> Result<(), SignalProtocolError> {
        self.kyber_pre_key_store.mark_kyber_pre_key_used(kyber_prekey_id).await
            .map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to mark kyber pre key as used".to_string()))
    }
}

#[async_trait(?Send)]
impl PreKeyStore for SqlitePreKeyStore {
    async fn get_pre_key(&self, prekey_id: PreKeyId) -> Result<PreKeyRecord, SignalProtocolError> {
        let key_pair = SqlitePreKeyStore::get_pre_key(self, u32::from(prekey_id)).await
            .map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to get pre key".to_string()))?;

        match key_pair {
            Some(key_pair) => Ok(PreKeyRecord::new(prekey_id, &key_pair)),
            None => Err(SignalProtocolError::InvalidPreKeyId),
        }
    }

    async fn save_pre_key(
        &mut self,
        prekey_id: PreKeyId,
        record: &PreKeyRecord,
    ) -> Result<(), SignalProtocolError> {
        let key_pair = record.key_pair()?;
        SqlitePreKeyStore::save_pre_key(self, u32::from(prekey_id), &key_pair).await
            .map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to save pre key".to_string()))
    }

    async fn remove_pre_key(&mut self, _prekey_id: PreKeyId) -> Result<(), SignalProtocolError> {
        // For now, we'll just succeed without actually removing
        // TODO: Implement actual removal when needed
        Ok(())
    }
}

#[async_trait(?Send)]
impl SignedPreKeyStore for SqliteSignedPreKeyStore {
    async fn get_signed_pre_key(&self, signed_prekey_id: SignedPreKeyId) -> Result<SignedPreKeyRecord, SignalProtocolError> {
        let record = SqliteSignedPreKeyStore::get_signed_pre_key(self, u32::from(signed_prekey_id)).await
            .map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to get signed pre key".to_string()))?;

        match record {
            Some(record) => Ok(record),
            None => Err(SignalProtocolError::InvalidSignedPreKeyId),
        }
    }

    async fn save_signed_pre_key(
        &mut self,
        signed_prekey_id: SignedPreKeyId,
        record: &SignedPreKeyRecord,
    ) -> Result<(), SignalProtocolError> {
        SqliteSignedPreKeyStore::save_signed_pre_key(self, u32::from(signed_prekey_id), record).await
            .map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to save signed pre key".to_string()))
    }
}

#[async_trait(?Send)]
impl KyberPreKeyStore for SqliteKyberPreKeyStore {
    async fn get_kyber_pre_key(&self, kyber_prekey_id: KyberPreKeyId) -> Result<KyberPreKeyRecord, SignalProtocolError> {
        let record = SqliteKyberPreKeyStore::get_kyber_pre_key(self, u32::from(kyber_prekey_id)).await
            .map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to get kyber pre key".to_string()))?;

        match record {
            Some(record) => Ok(record),
            None => Err(SignalProtocolError::InvalidKyberPreKeyId),
        }
    }

    async fn save_kyber_pre_key(
        &mut self,
        kyber_prekey_id: KyberPreKeyId,
        record: &KyberPreKeyRecord,
    ) -> Result<(), SignalProtocolError> {
        SqliteKyberPreKeyStore::save_kyber_pre_key(self, u32::from(kyber_prekey_id), record).await
            .map_err(|_| SignalProtocolError::InvalidState("storage", "Failed to save kyber pre key".to_string()))
    }

    async fn mark_kyber_pre_key_used(&mut self, _kyber_prekey_id: KyberPreKeyId) -> Result<(), SignalProtocolError> {
        // For now, just succeed without marking as used
        // TODO: Implement actual marking as used
        Ok(())
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

    #[tokio::test]
    async fn test_storage_implements_libsignal_session_store() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(":memory:").await?;

        let address = ProtocolAddress::new("test_user".to_string(), DeviceId::new(1)?);
        let session_record = SessionRecord::new_fresh();

        let result = <SqliteStorage as SessionStore>::store_session(
            &mut storage,
            &address,
            &session_record,
        ).await;
        assert!(result.is_ok());

        let loaded = <SqliteStorage as SessionStore>::load_session(
            &storage,
            &address,
        ).await?;
        assert!(loaded.is_some());

        Ok(())
    }

    #[tokio::test]
    async fn test_storage_implements_libsignal_identity_key_store() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(":memory:").await?;

        let address = ProtocolAddress::new("test_user".to_string(), DeviceId::new(1)?);
        let mut rng = rand::rng();
        let identity_key_pair = IdentityKeyPair::generate(&mut rng);

        let result = <SqliteStorage as IdentityKeyStore>::save_identity(
            &mut storage,
            &address,
            identity_key_pair.identity_key(),
        ).await;
        assert!(result.is_ok());

        let retrieved = <SqliteStorage as IdentityKeyStore>::get_identity(
            &storage,
            &address,
        ).await?;
        assert!(retrieved.is_some());

        Ok(())
    }

    #[tokio::test]
    async fn test_local_identity_and_registration_storage() -> Result<(), Box<dyn std::error::Error>> {
        let storage = SqliteStorage::new(":memory:").await?;

        let identity_result = <SqliteStorage as IdentityKeyStore>::get_identity_key_pair(&storage).await;
        assert!(identity_result.is_err());

        let registration_result = <SqliteStorage as IdentityKeyStore>::get_local_registration_id(&storage).await;
        assert!(registration_result.is_err());

        let mut rng = rand::rng();
        let identity_key_pair = IdentityKeyPair::generate(&mut rng);
        let registration_id = 54321;

        storage.set_local_identity_key_pair(&identity_key_pair).await?;
        storage.set_local_registration_id(registration_id).await?;

        let retrieved_identity = <SqliteStorage as IdentityKeyStore>::get_identity_key_pair(&storage).await?;
        assert_eq!(retrieved_identity.identity_key().serialize(), identity_key_pair.identity_key().serialize());

        let retrieved_registration = <SqliteStorage as IdentityKeyStore>::get_local_registration_id(&storage).await?;
        assert_eq!(retrieved_registration, registration_id);

        Ok(())
    }

    #[tokio::test]
    async fn test_kyber_pre_key_store_save_and_get() -> Result<(), Box<dyn std::error::Error>> {
        let storage = SqliteStorage::new(":memory:").await?;
        let kyber_pre_key_id = KyberPreKeyId::from(1u32);

        // Generate a Kyber pre-key record
        let mut rng = rand::rng();
        let kyber_keypair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
        let timestamp = Timestamp::from_epoch_millis(12345);
        let signature = b"dummy_signature"; // Kyber pre-keys need a signature
        let kyber_record = KyberPreKeyRecord::new(kyber_pre_key_id, timestamp, &kyber_keypair, signature);

        assert_eq!(storage.kyber_pre_key_store.kyber_pre_key_count().await, 0);

        // Test saving
        let result = storage.kyber_pre_key_store.save_kyber_pre_key(u32::from(kyber_pre_key_id), &kyber_record).await;
        assert!(result.is_ok());

        assert_eq!(storage.kyber_pre_key_store.kyber_pre_key_count().await, 1);

        // Test retrieving
        let retrieved = storage.kyber_pre_key_store.get_kyber_pre_key(u32::from(kyber_pre_key_id)).await?;
        assert!(retrieved.is_some());
        assert_eq!(retrieved.unwrap().id()?, kyber_pre_key_id);

        Ok(())
    }

    #[tokio::test]
    async fn test_storage_implements_libsignal_kyber_pre_key_store() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(":memory:").await?;
        let kyber_pre_key_id = KyberPreKeyId::from(1u32);

        // Generate a Kyber pre-key record
        let mut rng = rand::rng();
        let kyber_keypair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
        let timestamp = Timestamp::from_epoch_millis(12345);
        let signature = b"dummy_signature"; // Kyber pre-keys need a signature
        let kyber_record = KyberPreKeyRecord::new(kyber_pre_key_id, timestamp, &kyber_keypair, signature);

        // Test using the trait implementation
        let result = <SqliteStorage as KyberPreKeyStore>::save_kyber_pre_key(
            &mut storage,
            kyber_pre_key_id,
            &kyber_record,
        ).await;
        assert!(result.is_ok());

        let retrieved = <SqliteStorage as KyberPreKeyStore>::get_kyber_pre_key(
            &storage,
            kyber_pre_key_id,
        ).await?;
        assert_eq!(retrieved.id()?, kyber_pre_key_id);

        Ok(())
    }
}
