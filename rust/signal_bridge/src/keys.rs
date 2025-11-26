//! Key generation infrastructure for libsignal
//!
//! This module provides key generation functions for identity keys,
//! pre-keys, and signed pre-keys required by the Signal Protocol.

use libsignal_protocol::*;

/// Generates a new Signal Protocol identity key pair
///
/// # Returns
/// Identity key pair containing public and private Curve25519 keys
pub async fn generate_identity_key_pair() -> Result<IdentityKeyPair, Box<dyn std::error::Error>> {
    let mut rng = rand::rng();
    let identity_key_pair = IdentityKeyPair::generate(&mut rng);
    Ok(identity_key_pair)
}

/// Generates a batch of one-time prekeys
///
/// # Arguments
/// * `start_id` - Starting ID for the prekey sequence
/// * `count` - Number of prekeys to generate
///
/// # Returns
/// Vector of (key_id, KeyPair) tuples
pub async fn generate_pre_keys(
    start_id: u32,
    count: u32,
) -> Result<Vec<(u32, KeyPair)>, Box<dyn std::error::Error>> {
    let mut rng = rand::rng();
    let mut pre_keys = Vec::new();

    for i in 0..count {
        let key_id = start_id + i;
        let key_pair = KeyPair::generate(&mut rng);
        pre_keys.push((key_id, key_pair));
    }

    Ok(pre_keys)
}

/// Generates a signed prekey with timestamp and signature
///
/// # Arguments
/// * `identity_key_pair` - Identity key pair to sign the prekey
/// * `signed_pre_key_id` - ID for this signed prekey
///
/// # Returns
/// Signed prekey record with signature and timestamp
pub async fn generate_signed_pre_key(
    identity_key_pair: &IdentityKeyPair,
    signed_pre_key_id: u32,
) -> Result<SignedPreKeyRecord, Box<dyn std::error::Error>> {
    let mut rng = rand::rng();
    let timestamp = Timestamp::from_epoch_millis(
        std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)?
            .as_millis() as u64,
    );
    let key_pair = KeyPair::generate(&mut rng);
    let signature = identity_key_pair
        .private_key()
        .calculate_signature(&key_pair.public_key.serialize(), &mut rng)?;

    let signed_pre_key =
        SignedPreKeyRecord::new(signed_pre_key_id.into(), timestamp, &key_pair, &signature);

    Ok(signed_pre_key)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_generate_identity_key_pair() -> Result<(), Box<dyn std::error::Error>> {
        let identity_key_pair = generate_identity_key_pair().await?;

        let public_key_bytes = identity_key_pair.identity_key().public_key().serialize();
        let private_key_bytes = identity_key_pair.private_key().serialize();

        assert_eq!(public_key_bytes.len(), 33); // 33 bytes for Curve25519 public key
        assert_eq!(private_key_bytes.len(), 32); // 32 bytes for Curve25519 private key

        assert!(!public_key_bytes.iter().all(|&x| x == 0));
        assert!(!private_key_bytes.iter().all(|&x| x == 0));

        Ok(())
    }

    #[tokio::test]
    async fn test_generate_pre_keys() -> Result<(), Box<dyn std::error::Error>> {
        let start_id = 100;
        let count = 5;
        let pre_keys = generate_pre_keys(start_id, count).await?;

        assert_eq!(pre_keys.len(), count as usize);

        for (i, (key_id, key_pair)) in pre_keys.iter().enumerate() {
            assert_eq!(*key_id, start_id + i as u32);

            let public_key_bytes = key_pair.public_key.serialize();
            let private_key_bytes = key_pair.private_key.serialize();

            assert_eq!(public_key_bytes.len(), 33);
            assert_eq!(private_key_bytes.len(), 32);
            assert!(!public_key_bytes.iter().all(|&x| x == 0));
            assert!(!private_key_bytes.iter().all(|&x| x == 0));
        }

        Ok(())
    }

    #[tokio::test]
    async fn test_generate_signed_pre_key() -> Result<(), Box<dyn std::error::Error>> {
        let identity_key_pair = generate_identity_key_pair().await?;
        let signed_pre_key_id = 42;

        let signed_pre_key = generate_signed_pre_key(&identity_key_pair, signed_pre_key_id).await?;

        assert_eq!(signed_pre_key.id()?, signed_pre_key_id.into());

        let public_key_bytes = signed_pre_key.public_key()?.serialize();
        let private_key_bytes = signed_pre_key.private_key()?.serialize();

        assert_eq!(public_key_bytes.len(), 33);
        assert_eq!(private_key_bytes.len(), 32);
        assert!(!public_key_bytes.iter().all(|&x| x == 0));
        assert!(!private_key_bytes.iter().all(|&x| x == 0));

        let signature = signed_pre_key.signature()?;
        assert!(!signature.is_empty());
        assert!(!signature.iter().all(|&x| x == 0));

        Ok(())
    }

    #[tokio::test]
    async fn test_signed_pre_key_signature_verification() -> Result<(), Box<dyn std::error::Error>>
    {
        let identity_key_pair = generate_identity_key_pair().await?;
        let signed_pre_key_id = 123;

        let signed_pre_key = generate_signed_pre_key(&identity_key_pair, signed_pre_key_id).await?;

        let public_key_bytes = signed_pre_key.public_key()?.serialize();
        let signature = signed_pre_key.signature()?;
        let identity_public_key = identity_key_pair.identity_key().public_key();

        let is_valid = identity_public_key.verify_signature(&public_key_bytes, &signature);
        assert!(
            is_valid,
            "Signature should be valid when verified with the correct identity key"
        );

        let different_identity = generate_identity_key_pair().await?;
        let different_public_key = different_identity.identity_key().public_key();
        let is_invalid = different_public_key.verify_signature(&public_key_bytes, &signature);
        assert!(
            !is_invalid,
            "Signature should be invalid when verified with a different identity key"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_key_uniqueness() -> Result<(), Box<dyn std::error::Error>> {
        let identity1 = generate_identity_key_pair().await?;
        let identity2 = generate_identity_key_pair().await?;

        let identity1_private = identity1.private_key().serialize();
        let identity2_private = identity2.private_key().serialize();
        let identity1_public = identity1.identity_key().public_key().serialize();
        let identity2_public = identity2.identity_key().public_key().serialize();

        assert_ne!(
            identity1_private, identity2_private,
            "Identity private keys should be unique"
        );
        assert_ne!(
            identity1_public, identity2_public,
            "Identity public keys should be unique"
        );

        let pre_keys1 = generate_pre_keys(1, 10).await?;
        let pre_keys2 = generate_pre_keys(11, 10).await?;

        for i in 0..pre_keys1.len() {
            for j in (i + 1)..pre_keys1.len() {
                let key1_private = pre_keys1[i].1.private_key.serialize();
                let key1_public = pre_keys1[i].1.public_key.serialize();
                let key2_private = pre_keys1[j].1.private_key.serialize();
                let key2_public = pre_keys1[j].1.public_key.serialize();

                assert_ne!(
                    key1_private, key2_private,
                    "Pre-key private keys should be unique within batch"
                );
                assert_ne!(
                    key1_public, key2_public,
                    "Pre-key public keys should be unique within batch"
                );
            }
        }

        for (_, key1) in &pre_keys1 {
            for (_, key2) in &pre_keys2 {
                let key1_private = key1.private_key.serialize();
                let key1_public = key1.public_key.serialize();
                let key2_private = key2.private_key.serialize();
                let key2_public = key2.public_key.serialize();

                assert_ne!(
                    key1_private, key2_private,
                    "Pre-key private keys should be unique across batches"
                );
                assert_ne!(
                    key1_public, key2_public,
                    "Pre-key public keys should be unique across batches"
                );
            }
        }

        let signed_key1 = generate_signed_pre_key(&identity1, 1).await?;
        let signed_key2 = generate_signed_pre_key(&identity1, 2).await?;

        let signed1_private = signed_key1.private_key()?.serialize();
        let signed1_public = signed_key1.public_key()?.serialize();
        let signed2_private = signed_key2.private_key()?.serialize();
        let signed2_public = signed_key2.public_key()?.serialize();

        assert_ne!(
            signed1_private, signed2_private,
            "Signed pre-key private keys should be unique"
        );
        assert_ne!(
            signed1_public, signed2_public,
            "Signed pre-key public keys should be unique"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_signature_tampering_detection() -> Result<(), Box<dyn std::error::Error>> {
        let identity_key_pair = generate_identity_key_pair().await?;
        let signed_pre_key = generate_signed_pre_key(&identity_key_pair, 42).await?;

        let public_key_bytes = signed_pre_key.public_key()?.serialize();
        let original_signature = signed_pre_key.signature()?;
        let identity_public_key = identity_key_pair.identity_key().public_key();

        assert!(identity_public_key.verify_signature(&public_key_bytes, &original_signature));

        let mut tampered_signature = original_signature.clone();
        tampered_signature[0] = tampered_signature[0].wrapping_add(1); // Flip a bit
        assert!(
            !identity_public_key.verify_signature(&public_key_bytes, &tampered_signature),
            "Tampered signature should fail verification"
        );

        let mut tampered_data = public_key_bytes.clone();
        tampered_data[0] = tampered_data[0].wrapping_add(1); // Flip a bit
        assert!(
            !identity_public_key.verify_signature(&tampered_data, &original_signature),
            "Signature should fail verification when data is tampered"
        );

        let different_data = vec![0x42; 33];
        assert!(
            !identity_public_key.verify_signature(&different_data, &original_signature),
            "Signature should not verify against completely different data"
        );

        let empty_signature = vec![];
        assert!(
            !identity_public_key.verify_signature(&public_key_bytes, &empty_signature),
            "Empty signature should fail verification"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_signed_pre_key_timestamp_sanity() -> Result<(), Box<dyn std::error::Error>> {
        let identity_key_pair = generate_identity_key_pair().await?;

        let before_generation = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)?
            .as_millis() as u64;

        let signed_pre_key = generate_signed_pre_key(&identity_key_pair, 1).await?;

        let after_generation = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)?
            .as_millis() as u64;

        let timestamp_millis = signed_pre_key.timestamp()?.epoch_millis();

        assert!(
            timestamp_millis >= before_generation,
            "Timestamp should not be before generation started"
        );
        assert!(
            timestamp_millis <= after_generation,
            "Timestamp should not be after generation completed"
        );

        let one_hour_ago = before_generation - (60 * 60 * 1000);
        assert!(
            timestamp_millis > one_hour_ago,
            "Timestamp should not be more than 1 hour in the past"
        );

        let one_minute_future = after_generation + (60 * 1000);
        assert!(
            timestamp_millis < one_minute_future,
            "Timestamp should not be more than 1 minute in the future"
        );

        let signed_key1 = generate_signed_pre_key(&identity_key_pair, 2).await?;
        std::thread::sleep(std::time::Duration::from_millis(1));
        let signed_key2 = generate_signed_pre_key(&identity_key_pair, 3).await?;

        let timestamp1 = signed_key1.timestamp()?.epoch_millis();
        let timestamp2 = signed_key2.timestamp()?.epoch_millis();

        assert!(
            timestamp2 >= timestamp1,
            "Later generated key should have timestamp >= earlier key"
        );

        Ok(())
    }
}
