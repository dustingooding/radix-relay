//! In-memory storage implementation for libsignal
//!
//! This module provides an in-memory storage implementation that implements
//! the storage traits. Data is lost when the process terminates.

use crate::storage_trait::*;
use libsignal_protocol::*;
use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::Mutex;
use async_trait::async_trait;

fn address_key(address: &ProtocolAddress) -> String {
    format!("{}:{}", address.name(), u32::from(address.device_id()))
}

pub struct MemorySessionStore {
    sessions: Arc<Mutex<HashMap<String, SessionRecord>>>,
}

impl MemorySessionStore {
    pub fn new() -> Self {
        Self {
            sessions: Arc::new(Mutex::new(HashMap::new())),
        }
    }
}

#[async_trait(?Send)]
impl SessionStore for MemorySessionStore {
    async fn load_session(&self, address: &ProtocolAddress) -> Result<Option<SessionRecord>, SignalProtocolError> {
        let key = address_key(address);
        let store = self.sessions.lock().await;
        Ok(store.get(&key).cloned())
    }

    async fn store_session(
        &mut self,
        address: &ProtocolAddress,
        record: &SessionRecord,
    ) -> Result<(), SignalProtocolError> {
        let key = address_key(address);
        let mut store = self.sessions.lock().await;
        store.insert(key, record.clone());
        Ok(())
    }
}

#[async_trait(?Send)]
impl ExtendedSessionStore for MemorySessionStore {
    async fn session_count(&self) -> usize {
        let store = self.sessions.lock().await;
        store.len()
    }

    async fn clear_all_sessions(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let mut store = self.sessions.lock().await;
        store.clear();
        Ok(())
    }
}

pub struct MemoryIdentityStore {
    identity_keys: Arc<Mutex<HashMap<String, IdentityKey>>>,
    local_identity_key_pair: Arc<Mutex<Option<IdentityKeyPair>>>,
    local_registration_id: Arc<Mutex<Option<u32>>>,
}

impl MemoryIdentityStore {
    pub fn new() -> Self {
        Self {
            identity_keys: Arc::new(Mutex::new(HashMap::new())),
            local_identity_key_pair: Arc::new(Mutex::new(None)),
            local_registration_id: Arc::new(Mutex::new(None)),
        }
    }
}

#[async_trait(?Send)]
impl IdentityKeyStore for MemoryIdentityStore {
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

        let key = address_key(address);
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
        let key = address_key(address);
        let store = self.identity_keys.lock().await;
        match store.get(&key) {
            Some(stored_key) => Ok(*stored_key == *identity_key),
            None => Ok(true), // Trust on first use
        }
    }

    async fn get_identity(&self, address: &ProtocolAddress) -> Result<Option<IdentityKey>, SignalProtocolError> {
        let key = address_key(address);
        let store = self.identity_keys.lock().await;
        Ok(store.get(&key).copied())
    }
}

#[async_trait(?Send)]
impl ExtendedIdentityStore for MemoryIdentityStore {
    async fn identity_count(&self) -> usize {
        let store = self.identity_keys.lock().await;
        store.len()
    }

    async fn set_local_identity_key_pair(&self, identity_key_pair: &IdentityKeyPair) -> Result<(), Box<dyn std::error::Error>> {
        let mut store = self.local_identity_key_pair.lock().await;
        *store = Some(*identity_key_pair);
        Ok(())
    }

    async fn set_local_registration_id(&self, registration_id: u32) -> Result<(), Box<dyn std::error::Error>> {
        let mut store = self.local_registration_id.lock().await;
        *store = Some(registration_id);
        Ok(())
    }
}

pub struct MemoryPreKeyStore {
    pre_keys: Arc<Mutex<HashMap<u32, KeyPair>>>,
}

impl MemoryPreKeyStore {
    pub fn new() -> Self {
        Self {
            pre_keys: Arc::new(Mutex::new(HashMap::new())),
        }
    }
}

#[async_trait(?Send)]
impl PreKeyStore for MemoryPreKeyStore {
    async fn get_pre_key(&self, prekey_id: PreKeyId) -> Result<PreKeyRecord, SignalProtocolError> {
        let store = self.pre_keys.lock().await;
        match store.get(&u32::from(prekey_id)) {
            Some(key_pair) => Ok(PreKeyRecord::new(prekey_id, key_pair)),
            None => Err(SignalProtocolError::InvalidPreKeyId),
        }
    }

    async fn save_pre_key(
        &mut self,
        prekey_id: PreKeyId,
        record: &PreKeyRecord,
    ) -> Result<(), SignalProtocolError> {
        let key_pair = record.key_pair()?;
        let mut store = self.pre_keys.lock().await;
        store.insert(u32::from(prekey_id), key_pair);
        Ok(())
    }

    async fn remove_pre_key(&mut self, prekey_id: PreKeyId) -> Result<(), SignalProtocolError> {
        let mut store = self.pre_keys.lock().await;
        store.remove(&u32::from(prekey_id));
        Ok(())
    }
}

#[async_trait(?Send)]
impl ExtendedPreKeyStore for MemoryPreKeyStore {
    async fn pre_key_count(&self) -> usize {
        let store = self.pre_keys.lock().await;
        store.len()
    }

    async fn clear_all_pre_keys(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let mut store = self.pre_keys.lock().await;
        store.clear();
        Ok(())
    }
}

pub struct MemorySignedPreKeyStore {
    signed_pre_keys: Arc<Mutex<HashMap<u32, SignedPreKeyRecord>>>,
}

impl MemorySignedPreKeyStore {
    pub fn new() -> Self {
        Self {
            signed_pre_keys: Arc::new(Mutex::new(HashMap::new())),
        }
    }
}

#[async_trait(?Send)]
impl SignedPreKeyStore for MemorySignedPreKeyStore {
    async fn get_signed_pre_key(&self, signed_prekey_id: SignedPreKeyId) -> Result<SignedPreKeyRecord, SignalProtocolError> {
        let store = self.signed_pre_keys.lock().await;
        match store.get(&u32::from(signed_prekey_id)) {
            Some(record) => Ok(record.clone()),
            None => Err(SignalProtocolError::InvalidSignedPreKeyId),
        }
    }

    async fn save_signed_pre_key(
        &mut self,
        signed_prekey_id: SignedPreKeyId,
        record: &SignedPreKeyRecord,
    ) -> Result<(), SignalProtocolError> {
        let mut store = self.signed_pre_keys.lock().await;
        store.insert(u32::from(signed_prekey_id), record.clone());
        Ok(())
    }
}

#[async_trait(?Send)]
impl ExtendedSignedPreKeyStore for MemorySignedPreKeyStore {
    async fn signed_pre_key_count(&self) -> usize {
        let store = self.signed_pre_keys.lock().await;
        store.len()
    }

    async fn clear_all_signed_pre_keys(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let mut store = self.signed_pre_keys.lock().await;
        store.clear();
        Ok(())
    }
}

pub struct MemoryKyberPreKeyStore {
    kyber_pre_keys: Arc<Mutex<HashMap<u32, KyberPreKeyRecord>>>,
}

impl MemoryKyberPreKeyStore {
    pub fn new() -> Self {
        Self {
            kyber_pre_keys: Arc::new(Mutex::new(HashMap::new())),
        }
    }
}

#[async_trait(?Send)]
impl KyberPreKeyStore for MemoryKyberPreKeyStore {
    async fn get_kyber_pre_key(&self, kyber_prekey_id: KyberPreKeyId) -> Result<KyberPreKeyRecord, SignalProtocolError> {
        let store = self.kyber_pre_keys.lock().await;
        match store.get(&u32::from(kyber_prekey_id)) {
            Some(record) => Ok(record.clone()),
            None => Err(SignalProtocolError::InvalidKyberPreKeyId),
        }
    }

    async fn save_kyber_pre_key(
        &mut self,
        kyber_prekey_id: KyberPreKeyId,
        record: &KyberPreKeyRecord,
    ) -> Result<(), SignalProtocolError> {
        let mut store = self.kyber_pre_keys.lock().await;
        store.insert(u32::from(kyber_prekey_id), record.clone());
        Ok(())
    }

    async fn mark_kyber_pre_key_used(&mut self, _kyber_prekey_id: KyberPreKeyId) -> Result<(), SignalProtocolError> {
        // For memory storage, we don't need to track usage
        Ok(())
    }
}

#[async_trait(?Send)]
impl ExtendedKyberPreKeyStore for MemoryKyberPreKeyStore {
    async fn kyber_pre_key_count(&self) -> usize {
        let store = self.kyber_pre_keys.lock().await;
        store.len()
    }

    async fn clear_all_kyber_pre_keys(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let mut store = self.kyber_pre_keys.lock().await;
        store.clear();
        Ok(())
    }
}

pub struct MemoryStorage {
    pub session_store: MemorySessionStore,
    pub identity_store: MemoryIdentityStore,
    pub pre_key_store: MemoryPreKeyStore,
    pub signed_pre_key_store: MemorySignedPreKeyStore,
    pub kyber_pre_key_store: MemoryKyberPreKeyStore,
}

impl MemoryStorage {
    pub fn new() -> Self {
        Self {
            session_store: MemorySessionStore::new(),
            identity_store: MemoryIdentityStore::new(),
            pre_key_store: MemoryPreKeyStore::new(),
            signed_pre_key_store: MemorySignedPreKeyStore::new(),
            kyber_pre_key_store: MemoryKyberPreKeyStore::new(),
        }
    }
}

impl SignalStorageContainer for MemoryStorage {
    type SessionStore = MemorySessionStore;
    type IdentityStore = MemoryIdentityStore;
    type PreKeyStore = MemoryPreKeyStore;
    type SignedPreKeyStore = MemorySignedPreKeyStore;
    type KyberPreKeyStore = MemoryKyberPreKeyStore;

    fn session_store(&mut self) -> &mut Self::SessionStore {
        &mut self.session_store
    }

    fn identity_store(&mut self) -> &mut Self::IdentityStore {
        &mut self.identity_store
    }

    fn pre_key_store(&mut self) -> &mut Self::PreKeyStore {
        &mut self.pre_key_store
    }

    fn signed_pre_key_store(&mut self) -> &mut Self::SignedPreKeyStore {
        &mut self.signed_pre_key_store
    }

    fn kyber_pre_key_store(&mut self) -> &mut Self::KyberPreKeyStore {
        &mut self.kyber_pre_key_store
    }

    fn initialize(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        Ok(())
    }

    fn close(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        Ok(())
    }

    fn storage_type(&self) -> &'static str {
        "memory"
    }
}

#[async_trait(?Send)]
impl ExtendedStorageOps for MemoryStorage {
    async fn establish_session_from_bundle(
        &mut self,
        address: &ProtocolAddress,
        bundle: &PreKeyBundle,
    ) -> Result<(), Box<dyn std::error::Error>> {
        MemoryStorage::establish_session_from_bundle(self, address, bundle).await
    }

    async fn encrypt_message(
        &mut self,
        remote_address: &ProtocolAddress,
        plaintext: &[u8],
    ) -> Result<CiphertextMessage, SignalProtocolError> {
        MemoryStorage::encrypt_message(self, remote_address, plaintext).await
    }

    async fn decrypt_message(
        &mut self,
        remote_address: &ProtocolAddress,
        ciphertext: &CiphertextMessage,
    ) -> Result<Vec<u8>, SignalProtocolError> {
        MemoryStorage::decrypt_message(self, remote_address, ciphertext).await
    }
}

impl MemoryStorage {
    /// Establish session from bundle - internal method that handles borrowing properly
    pub async fn establish_session_from_bundle(
        &mut self,
        address: &ProtocolAddress,
        bundle: &PreKeyBundle,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let mut rng = rand::rng();
        let timestamp = std::time::SystemTime::now();

        process_prekey_bundle(
            address,
            &mut self.session_store,
            &mut self.identity_store,
            bundle,
            timestamp,
            &mut rng,
            UsePQRatchet::Yes,
        ).await?;

        Ok(())
    }

    /// Encrypt message - internal method that handles borrowing properly
    pub async fn encrypt_message(
        &mut self,
        remote_address: &ProtocolAddress,
        plaintext: &[u8],
    ) -> Result<CiphertextMessage, SignalProtocolError> {
        let mut rng = rand::rng();
        let now = std::time::SystemTime::now();
        message_encrypt(plaintext, remote_address, &mut self.session_store, &mut self.identity_store, now, &mut rng).await
    }

    /// Decrypt message - internal method that handles borrowing properly
    pub async fn decrypt_message(
        &mut self,
        remote_address: &ProtocolAddress,
        ciphertext: &CiphertextMessage,
    ) -> Result<Vec<u8>, SignalProtocolError> {
        let mut rng = rand::rng();

        message_decrypt(
            ciphertext,
            remote_address,
            &mut self.session_store,
            &mut self.identity_store,
            &mut self.pre_key_store,
            &mut self.signed_pre_key_store,
            &mut self.kyber_pre_key_store,
            &mut rng,
            UsePQRatchet::Yes,
        ).await
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_memory_storage_creation() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = MemoryStorage::new();
        storage.initialize()?;
        assert_eq!(storage.storage_type(), "memory");
        Ok(())
    }

    #[tokio::test]
    async fn test_session_storage() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = MemoryStorage::new();
        let address = ProtocolAddress::new("test_user".to_string(), DeviceId::new(1)?);
        let session_record = SessionRecord::new_fresh();

        assert_eq!(storage.session_store.session_count().await, 0);

        storage.session_store.store_session(&address, &session_record).await?;
        assert_eq!(storage.session_store.session_count().await, 1);

        let loaded = storage.session_store.load_session(&address).await?;
        assert!(loaded.is_some());

        storage.session_store.clear_all_sessions().await?;
        assert_eq!(storage.session_store.session_count().await, 0);

        Ok(())
    }

    #[tokio::test]
    async fn test_identity_storage() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = MemoryStorage::new();
        let mut rng = rand::rng();
        let identity_key_pair = IdentityKeyPair::generate(&mut rng);
        let address = ProtocolAddress::new("test_user".to_string(), DeviceId::new(1)?);

        // Test local identity storage
        storage.identity_store.set_local_identity_key_pair(&identity_key_pair).await?;
        storage.identity_store.set_local_registration_id(12345).await?;

        let retrieved_identity = storage.identity_store.get_identity_key_pair().await?;
        assert_eq!(retrieved_identity.identity_key().serialize(), identity_key_pair.identity_key().serialize());

        let retrieved_registration = storage.identity_store.get_local_registration_id().await?;
        assert_eq!(retrieved_registration, 12345);

        // Test remote identity storage
        assert_eq!(storage.identity_store.identity_count().await, 0);

        storage.identity_store.save_identity(&address, identity_key_pair.identity_key()).await?;
        assert_eq!(storage.identity_store.identity_count().await, 1);

        let retrieved = storage.identity_store.get_identity(&address).await?;
        assert!(retrieved.is_some());
        assert_eq!(retrieved.unwrap().serialize(), identity_key_pair.identity_key().serialize());

        Ok(())
    }
}
