
use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};

#[cxx::bridge(namespace = "radix_relay")]
mod ffi {
    extern "Rust" {
        fn generate_node_fingerprint(hostname: &str, user: &str) -> String;
        fn get_node_identity_fingerprint() -> String;
    }
}

pub fn generate_node_fingerprint(hostname: &str, user: &str) -> String {
    let mut hasher = DefaultHasher::new();

    hostname.hash(&mut hasher);
    user.hash(&mut hasher);
    "radix-node".hash(&mut hasher);

    let hash = hasher.finish();

    format!("RDX:{:016x}", hash)
}

pub fn get_node_identity_fingerprint() -> String {
    let hostname = std::env::var("HOSTNAME").unwrap_or_else(|_| "unknown".to_string());
    let user = std::env::var("USER").unwrap_or_else(|_| "unknown".to_string());
    generate_node_fingerprint(&hostname, &user)
}

#[cfg(test)]
mod tests {
    use super::*;


    #[test]
    fn test_get_node_identity_fingerprint() {
        let fingerprint = get_node_identity_fingerprint();
        assert!(fingerprint.starts_with("RDX:"));
        assert_eq!(fingerprint.len(), 20); // "RDX:" + 16 hex chars

        // Should be deterministic for same environment
        let fingerprint2 = get_node_identity_fingerprint();
        assert_eq!(fingerprint, fingerprint2);
    }

    #[test]
    fn test_fingerprint_format() {
        let fingerprint = get_node_identity_fingerprint();

        // Check format: RDX: followed by exactly 16 hex characters
        assert!(fingerprint.starts_with("RDX:"));
        let hex_part = &fingerprint[4..];
        assert_eq!(hex_part.len(), 16);
        assert!(hex_part.chars().all(|c| c.is_ascii_hexdigit()));
    }

    #[test]
    fn test_fingerprint_with_missing_env_vars() {
        // Test the fallback behavior when env vars are missing
        // This tests the unwrap_or_else paths
        let original_hostname = std::env::var("HOSTNAME").ok();
        let original_user = std::env::var("USER").ok();

        // Temporarily remove env vars to test fallbacks
        std::env::remove_var("HOSTNAME");
        std::env::remove_var("USER");

        let fingerprint = get_node_identity_fingerprint();
        assert!(fingerprint.starts_with("RDX:"));
        assert_eq!(fingerprint.len(), 20);

        // Restore original env vars
        if let Some(hostname) = original_hostname {
            std::env::set_var("HOSTNAME", hostname);
        }
        if let Some(user) = original_user {
            std::env::set_var("USER", user);
        }
    }

    #[test]
    fn test_fingerprint_changes_with_environment() {
        // Save original values
        let original_hostname = std::env::var("HOSTNAME").ok();
        let original_user = std::env::var("USER").ok();

        // Test with different environment values
        std::env::set_var("HOSTNAME", "test-host-1");
        std::env::set_var("USER", "test-user-1");
        let fingerprint1 = get_node_identity_fingerprint();

        std::env::set_var("HOSTNAME", "test-host-2");
        std::env::set_var("USER", "test-user-2");
        let fingerprint2 = get_node_identity_fingerprint();

        // Different environments should produce different fingerprints
        assert_ne!(fingerprint1, fingerprint2);

        // Both should still be valid format
        assert!(fingerprint1.starts_with("RDX:"));
        assert!(fingerprint2.starts_with("RDX:"));

        // Restore original values
        if let Some(hostname) = original_hostname {
            std::env::set_var("HOSTNAME", hostname);
        } else {
            std::env::remove_var("HOSTNAME");
        }
        if let Some(user) = original_user {
            std::env::set_var("USER", user);
        } else {
            std::env::remove_var("USER");
        }
    }

    #[test]
    fn test_generate_node_fingerprint_pure() {
        let fingerprint1 = generate_node_fingerprint("host1", "user1");
        let fingerprint2 = generate_node_fingerprint("host2", "user2");
        let fingerprint3 = generate_node_fingerprint("host1", "user1");

        assert!(fingerprint1.starts_with("RDX:"));
        assert!(fingerprint2.starts_with("RDX:"));
        assert_eq!(fingerprint1.len(), 20);
        assert_eq!(fingerprint2.len(), 20);

        assert_ne!(fingerprint1, fingerprint2);
        assert_eq!(fingerprint1, fingerprint3);
    }

    #[test]
    fn test_generate_node_fingerprint_edge_cases() {
        let empty_fingerprint = generate_node_fingerprint("", "");
        let special_chars_fingerprint = generate_node_fingerprint("host-with.dots", "user@domain");
        let unicode_fingerprint = generate_node_fingerprint("🏠", "👤");

        assert!(empty_fingerprint.starts_with("RDX:"));
        assert!(special_chars_fingerprint.starts_with("RDX:"));
        assert!(unicode_fingerprint.starts_with("RDX:"));

        assert_ne!(empty_fingerprint, special_chars_fingerprint);
        assert_ne!(special_chars_fingerprint, unicode_fingerprint);
    }
}
