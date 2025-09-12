
use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};

#[cxx::bridge(namespace = "radix_relay")]
mod ffi {
    extern "Rust" {
        fn hello_world() -> String;
        fn add_numbers(a: i32, b: i32) -> i32;
        fn get_node_identity_fingerprint() -> String;
    }
}

pub fn hello_world() -> String {
    "Hello World from Rust via CXX bridge!".to_string()
}

pub fn add_numbers(a: i32, b: i32) -> i32 {
    a + b
}

pub fn get_node_identity_fingerprint() -> String {
    // Generate a deterministic node identity fingerprint
    // In a real implementation, this would use actual identity key material
    let mut hasher = DefaultHasher::new();

    // Use some system-specific information for fingerprint generation
    let hostname = std::env::var("HOSTNAME").unwrap_or_else(|_| "unknown".to_string());
    let user = std::env::var("USER").unwrap_or_else(|_| "unknown".to_string());

    hostname.hash(&mut hasher);
    user.hash(&mut hasher);
    "radix-node".hash(&mut hasher);

    let hash = hasher.finish();

    // Format as a readable fingerprint (similar to SSH key fingerprints)
    format!("RDX:{:016x}", hash)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello_world() {
        let result = hello_world();
        assert_eq!(result, "Hello World from Rust via CXX bridge!");
    }

    #[test]
    fn test_add_numbers() {
        assert_eq!(add_numbers(2, 3), 5);
        assert_eq!(add_numbers(-1, 1), 0);
    }

    #[test]
    fn test_get_node_identity_fingerprint() {
        let fingerprint = get_node_identity_fingerprint();
        assert!(fingerprint.starts_with("RDX:"));
        assert_eq!(fingerprint.len(), 20); // "RDX:" + 16 hex chars

        // Should be deterministic for same environment
        let fingerprint2 = get_node_identity_fingerprint();
        assert_eq!(fingerprint, fingerprint2);
    }
}
