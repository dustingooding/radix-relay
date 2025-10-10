//! Nostr identity derivation from Signal Protocol keys
//!
//! This module handles deriving Nostr keypairs from Signal Protocol identity keys
//! using HKDF, ensuring deterministic key generation.

use crate::SignalBridgeError;
use hkdf::Hkdf;
use libsignal_protocol::{IdentityKey, IdentityKeyPair};
use nostr::{Keys, PublicKey, SecretKey};
use sha2::Sha256;

pub struct NostrIdentity;

impl NostrIdentity {
    pub fn derive_from_signal_identity(
        identity_key_pair: &IdentityKeyPair,
    ) -> Result<Keys, SignalBridgeError> {
        let identity_public_key = identity_key_pair.identity_key();

        let hk = Hkdf::<Sha256>::new(None, &identity_public_key.serialize());
        let mut derived_key = [0u8; 32];
        hk.expand(b"radix_relay_nostr_derivation", &mut derived_key)
            .map_err(|_| SignalBridgeError::KeyDerivation("HKDF expansion failed".to_string()))?;

        let secret_key = SecretKey::from_slice(&derived_key)
            .map_err(|e| SignalBridgeError::KeyDerivation(format!("Invalid secret key: {}", e)))?;

        Ok(Keys::new(secret_key))
    }

    pub fn derive_public_key_from_peer_identity(
        peer_identity: &IdentityKey,
    ) -> Result<PublicKey, SignalBridgeError> {
        let hk = Hkdf::<Sha256>::new(None, &peer_identity.serialize());
        let mut derived_key = [0u8; 32];
        hk.expand(b"radix_relay_nostr_derivation", &mut derived_key)
            .map_err(|_| SignalBridgeError::KeyDerivation("HKDF expansion failed".to_string()))?;

        let secret_key = SecretKey::from_slice(&derived_key)
            .map_err(|e| SignalBridgeError::KeyDerivation(format!("Invalid secret key: {}", e)))?;

        let full_keys = Keys::new(secret_key);
        Ok(full_keys.public_key())
    }
}
