//! Storage trait definitions for libsignal
//!
//! This module provides trait definitions for storage backends, allowing
//! dependency injection of different storage implementations (memory, database, etc.).

use libsignal_protocol::*;
use async_trait::async_trait;

/// Storage container that holds separate store instances for libsignal
pub trait SignalStorageContainer {
    type SessionStore: SessionStore + ExtendedSessionStore + Send + Sync;
    type IdentityStore: IdentityKeyStore + ExtendedIdentityStore + Send + Sync;
    type PreKeyStore: PreKeyStore + ExtendedPreKeyStore + Send + Sync;
    type SignedPreKeyStore: SignedPreKeyStore + ExtendedSignedPreKeyStore + Send + Sync;
    type KyberPreKeyStore: KyberPreKeyStore + ExtendedKyberPreKeyStore + Send + Sync;

    /// Get mutable reference to session store
    fn session_store(&mut self) -> &mut Self::SessionStore;

    /// Get mutable reference to identity store
    fn identity_store(&mut self) -> &mut Self::IdentityStore;

    /// Get mutable reference to pre key store
    fn pre_key_store(&mut self) -> &mut Self::PreKeyStore;

    /// Get mutable reference to signed pre key store
    fn signed_pre_key_store(&mut self) -> &mut Self::SignedPreKeyStore;

    /// Get mutable reference to kyber pre key store
    fn kyber_pre_key_store(&mut self) -> &mut Self::KyberPreKeyStore;

    /// Initialize the storage backend
    fn initialize(&mut self) -> Result<(), Box<dyn std::error::Error>>;

    /// Close/cleanup the storage backend
    fn close(&mut self) -> Result<(), Box<dyn std::error::Error>>;

    /// Get storage implementation name for debugging
    fn storage_type(&self) -> &'static str;
}

/// Extended storage operations that handle libsignal calls
#[async_trait(?Send)]
pub trait ExtendedStorageOps {
    /// Establish session from bundle - handles libsignal multiple borrows internally
    async fn establish_session_from_bundle(
        &mut self,
        address: &ProtocolAddress,
        bundle: &PreKeyBundle,
    ) -> Result<(), Box<dyn std::error::Error>>;

    /// Encrypt message - handles libsignal multiple borrows internally
    async fn encrypt_message(
        &mut self,
        remote_address: &ProtocolAddress,
        plaintext: &[u8],
    ) -> Result<CiphertextMessage, SignalProtocolError>;

    /// Decrypt message - handles libsignal multiple borrows internally
    async fn decrypt_message(
        &mut self,
        remote_address: &ProtocolAddress,
        ciphertext: &CiphertextMessage,
    ) -> Result<Vec<u8>, SignalProtocolError>;
}

/// Extended session storage operations
#[async_trait(?Send)]
pub trait ExtendedSessionStore {
    async fn session_count(&self) -> usize;
    async fn clear_all_sessions(&mut self) -> Result<(), Box<dyn std::error::Error>>;
}

/// Extended identity key storage operations
#[async_trait(?Send)]
pub trait ExtendedIdentityStore {
    async fn identity_count(&self) -> usize;
    async fn set_local_identity_key_pair(&self, identity_key_pair: &IdentityKeyPair) -> Result<(), Box<dyn std::error::Error>>;
    async fn set_local_registration_id(&self, registration_id: u32) -> Result<(), Box<dyn std::error::Error>>;
}

/// Extended pre key storage operations
#[async_trait(?Send)]
pub trait ExtendedPreKeyStore {
    async fn pre_key_count(&self) -> usize;
    async fn clear_all_pre_keys(&mut self) -> Result<(), Box<dyn std::error::Error>>;
}

/// Extended signed pre key storage operations
#[async_trait(?Send)]
pub trait ExtendedSignedPreKeyStore {
    async fn signed_pre_key_count(&self) -> usize;
    async fn clear_all_signed_pre_keys(&mut self) -> Result<(), Box<dyn std::error::Error>>;
}

/// Extended kyber pre key storage operations
#[async_trait(?Send)]
pub trait ExtendedKyberPreKeyStore {
    async fn kyber_pre_key_count(&self) -> usize;
    async fn clear_all_kyber_pre_keys(&mut self) -> Result<(), Box<dyn std::error::Error>>;
}
