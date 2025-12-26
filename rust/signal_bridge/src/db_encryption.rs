//! Database encryption key management
//!
//! This module handles generation, storage, and retrieval of SQLCipher
//! database encryption keys using platform-specific secure storage.

use thiserror::Error;

#[derive(Error, Debug)]
pub enum DbEncryptionError {
    #[error("Key storage error: {0}")]
    KeyStorage(String),
}

/// Database encryption key (32 bytes for AES-256)
pub type DbKey = [u8; 32];

/// Generates a new random database encryption key
pub fn generate_db_key() -> Result<DbKey, DbEncryptionError> {
    use rand::RngCore;

    let mut key = [0u8; 32];
    let mut rng = rand::rng();
    rng.fill_bytes(&mut key);
    Ok(key)
}

/// Derives key file path from database path
fn get_key_file_path(db_path: &str) -> String {
    format!("{}.key", db_path)
}

/// Stores database encryption key in platform-specific secure storage
pub fn store_db_key(db_path: &str, key: &DbKey) -> Result<(), DbEncryptionError> {
    use std::fs;
    use std::io::Write;

    if db_path == ":memory:" {
        return Ok(());
    }

    let key_path = get_key_file_path(db_path);

    if let Some(parent) = std::path::Path::new(&key_path).parent() {
        fs::create_dir_all(parent).map_err(|e| {
            DbEncryptionError::KeyStorage(format!("Failed to create key directory: {}", e))
        })?;
    }

    let mut file = fs::File::create(&key_path)
        .map_err(|e| DbEncryptionError::KeyStorage(format!("Failed to create key file: {}", e)))?;

    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        let permissions = fs::Permissions::from_mode(0o600);
        fs::set_permissions(&key_path, permissions).map_err(|e| {
            DbEncryptionError::KeyStorage(format!("Failed to set key file permissions: {}", e))
        })?;
    }

    file.write_all(key)
        .map_err(|e| DbEncryptionError::KeyStorage(format!("Failed to write key: {}", e)))?;

    Ok(())
}

/// Retrieves database encryption key from platform-specific secure storage
pub fn retrieve_db_key(db_path: &str) -> Result<DbKey, DbEncryptionError> {
    use std::fs;
    use std::io::Read;

    if db_path == ":memory:" {
        return Err(DbEncryptionError::KeyStorage(
            "In-memory databases do not have persistent keys".to_string(),
        ));
    }

    let key_path = get_key_file_path(db_path);

    let mut file = fs::File::open(&key_path)
        .map_err(|e| DbEncryptionError::KeyStorage(format!("Failed to open key file: {}", e)))?;

    let mut key = [0u8; 32];
    file.read_exact(&mut key)
        .map_err(|e| DbEncryptionError::KeyStorage(format!("Failed to read key: {}", e)))?;

    Ok(key)
}

/// Gets or creates a database encryption key for the given database path
pub fn get_or_create_db_key(db_path: &str) -> Result<DbKey, DbEncryptionError> {
    match retrieve_db_key(db_path) {
        Ok(key) => Ok(key),
        Err(_) => {
            let key = generate_db_key()?;
            store_db_key(db_path, &key)?;
            Ok(key)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    fn cleanup_test_key(db_path: &str) {
        let key_path = get_key_file_path(db_path);
        let _ = fs::remove_file(key_path);
    }

    #[test]
    fn test_generate_db_key_produces_32_bytes() {
        let key = generate_db_key().expect("Failed to generate key");
        assert_eq!(key.len(), 32, "Key should be 32 bytes for AES-256");
    }

    #[test]
    fn test_generate_db_key_produces_random_keys() {
        let key1 = generate_db_key().expect("Failed to generate key1");
        let key2 = generate_db_key().expect("Failed to generate key2");
        assert_ne!(key1, key2, "Two generated keys should be different");
    }

    #[test]
    fn test_store_and_retrieve_db_key() {
        let test_db_path = "test_db_for_key_storage.db";
        let original_key = generate_db_key().expect("Failed to generate key");

        store_db_key(test_db_path, &original_key).expect("Failed to store key");
        let retrieved_key = retrieve_db_key(test_db_path).expect("Failed to retrieve key");

        assert_eq!(
            original_key, retrieved_key,
            "Retrieved key should match stored key"
        );

        cleanup_test_key(test_db_path);
    }

    #[test]
    fn test_get_or_create_creates_new_key() {
        let test_db_path = "test_db_for_get_or_create.db";

        cleanup_test_key(test_db_path);

        let key1 = get_or_create_db_key(test_db_path).expect("Failed to get or create key");
        assert_eq!(key1.len(), 32, "Key should be 32 bytes");

        let key2 = get_or_create_db_key(test_db_path).expect("Failed to get or create key again");
        assert_eq!(
            key1, key2,
            "get_or_create should return same key on subsequent calls"
        );

        cleanup_test_key(test_db_path);
    }

    #[test]
    fn test_retrieve_nonexistent_key_returns_error() {
        let test_db_path = "nonexistent_db_path_12345.db";
        let result = retrieve_db_key(test_db_path);
        assert!(
            result.is_err(),
            "Retrieving non-existent key should return error"
        );
    }

    #[test]
    fn test_key_file_permissions_on_unix() {
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;

            let test_db_path = "test_db_for_permissions.db";
            let key = generate_db_key().expect("Failed to generate key");

            store_db_key(test_db_path, &key).expect("Failed to store key");

            let key_path = get_key_file_path(test_db_path);
            let metadata = fs::metadata(&key_path).expect("Failed to get key file metadata");
            let permissions = metadata.permissions();

            assert_eq!(
                permissions.mode() & 0o777,
                0o600,
                "Key file should have 0600 permissions"
            );

            cleanup_test_key(test_db_path);
        }
    }
}
