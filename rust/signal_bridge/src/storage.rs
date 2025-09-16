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
}

impl SqliteStorage {
    pub async fn new(_db_path: &str) -> Result<Self, Box<dyn std::error::Error>> {
        Ok(Self {
            identity_keys: Arc::new(Mutex::new(HashMap::new())),
            pre_keys: Arc::new(Mutex::new(HashMap::new())),
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
}
