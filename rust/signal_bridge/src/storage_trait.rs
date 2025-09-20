//! Storage trait definitions for libsignal
//!
//! This module provides trait definitions for storage backends, allowing
//! dependency injection of different storage implementations (memory, database, etc.).

use libsignal_protocol::*;
use async_trait::async_trait;

pub trait SignalStorageContainer {
    type SessionStore: SessionStore + ExtendedSessionStore;
    type IdentityStore: IdentityKeyStore + ExtendedIdentityStore;
    type PreKeyStore: PreKeyStore + ExtendedPreKeyStore;
    type SignedPreKeyStore: SignedPreKeyStore + ExtendedSignedPreKeyStore;
    type KyberPreKeyStore: KyberPreKeyStore + ExtendedKyberPreKeyStore;

    fn session_store(&mut self) -> &mut Self::SessionStore;

    fn identity_store(&mut self) -> &mut Self::IdentityStore;

    fn pre_key_store(&mut self) -> &mut Self::PreKeyStore;

    fn signed_pre_key_store(&mut self) -> &mut Self::SignedPreKeyStore;

    fn kyber_pre_key_store(&mut self) -> &mut Self::KyberPreKeyStore;

    fn initialize(&mut self) -> Result<(), Box<dyn std::error::Error>>;

    fn close(&mut self) -> Result<(), Box<dyn std::error::Error>>;

    fn storage_type(&self) -> &'static str;
}

#[async_trait(?Send)]
pub trait ExtendedStorageOps {
    async fn establish_session_from_bundle(
        &mut self,
        address: &ProtocolAddress,
        bundle: &PreKeyBundle,
    ) -> Result<(), Box<dyn std::error::Error>>;

    async fn encrypt_message(
        &mut self,
        remote_address: &ProtocolAddress,
        plaintext: &[u8],
    ) -> Result<CiphertextMessage, SignalProtocolError>;

    async fn decrypt_message(
        &mut self,
        remote_address: &ProtocolAddress,
        ciphertext: &CiphertextMessage,
    ) -> Result<Vec<u8>, SignalProtocolError>;
}

#[async_trait(?Send)]
pub trait ExtendedSessionStore {
    async fn session_count(&self) -> usize;
    async fn clear_all_sessions(&mut self) -> Result<(), Box<dyn std::error::Error>>;
    async fn delete_session(&mut self, address: &ProtocolAddress) -> Result<(), Box<dyn std::error::Error>>;
}

#[async_trait(?Send)]
pub trait ExtendedIdentityStore {
    async fn identity_count(&self) -> usize;
    async fn set_local_identity_key_pair(&self, identity_key_pair: &IdentityKeyPair) -> Result<(), Box<dyn std::error::Error>>;
    async fn set_local_registration_id(&self, registration_id: u32) -> Result<(), Box<dyn std::error::Error>>;
    async fn get_peer_identity(&self, address: &ProtocolAddress) -> Result<Option<IdentityKey>, Box<dyn std::error::Error>>;
    async fn delete_identity(&mut self, address: &ProtocolAddress) -> Result<(), Box<dyn std::error::Error>>;
    async fn clear_all_identities(&mut self) -> Result<(), Box<dyn std::error::Error>>;
    async fn clear_local_identity(&mut self) -> Result<(), Box<dyn std::error::Error>>;
}

#[async_trait(?Send)]
pub trait ExtendedPreKeyStore {
    async fn pre_key_count(&self) -> usize;
    async fn clear_all_pre_keys(&mut self) -> Result<(), Box<dyn std::error::Error>>;
}

#[async_trait(?Send)]
pub trait ExtendedSignedPreKeyStore {
    async fn signed_pre_key_count(&self) -> usize;
    async fn clear_all_signed_pre_keys(&mut self) -> Result<(), Box<dyn std::error::Error>>;
}

#[async_trait(?Send)]
pub trait ExtendedKyberPreKeyStore {
    async fn kyber_pre_key_count(&self) -> usize;
    async fn clear_all_kyber_pre_keys(&mut self) -> Result<(), Box<dyn std::error::Error>>;
}
