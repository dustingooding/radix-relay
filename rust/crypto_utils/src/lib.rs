use crate::ffi::NodeIdentity;
use sha2::{Digest, Sha256};

#[cxx::bridge(namespace = "radix_relay")]
mod ffi {
    struct NodeIdentity {
        hostname: String,
        username: String,
        platform: String,
        machine_id: String,
        mac_address: String,
        install_id: String,
    }

    extern "Rust" {
        fn generate_node_fingerprint(identity: &NodeIdentity) -> String;
    }
}

pub fn generate_node_fingerprint(identity: &NodeIdentity) -> String {
    let mut hasher = Sha256::new();

    hasher.update(identity.hostname.as_bytes());
    hasher.update(identity.username.as_bytes());
    hasher.update(identity.platform.as_bytes());
    hasher.update(identity.machine_id.as_bytes());
    hasher.update(identity.mac_address.as_bytes());
    hasher.update(identity.install_id.as_bytes());
    hasher.update(b"radix-node");

    let result = hasher.finalize();

    format!("RDX:{:x}", result)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_generate_node_fingerprint_known_input() {
        let identity = NodeIdentity {
            hostname: "test-host".to_string(),
            username: "test-user".to_string(),
            platform: "linux".to_string(),
            machine_id: "machine123".to_string(),
            mac_address: "00:11:22:33:44:55".to_string(),
            install_id: "install456".to_string(),
        };

        let fingerprint = generate_node_fingerprint(&identity);
        assert_eq!(
            fingerprint,
            "RDX:dae7dd5b261f004b5a5f08f9af5c468b5ef6d18a7ef9d066f5489341c4932348"
        );
    }

    #[test]
    fn test_generate_node_fingerprint_empty_fields() {
        let identity = NodeIdentity {
            hostname: "".to_string(),
            username: "".to_string(),
            platform: "".to_string(),
            machine_id: "".to_string(),
            mac_address: "".to_string(),
            install_id: "".to_string(),
        };

        let fingerprint = generate_node_fingerprint(&identity);
        assert_eq!(
            fingerprint,
            "RDX:175568d645658bd89cd35d8f9857624b36b27bcb41163539ebe46ec49601217d"
        );
    }
}
