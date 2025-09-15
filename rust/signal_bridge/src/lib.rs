//! Signal Protocol integration for Radix Relay
//!
//! This crate provides a bridge between Radix Relay's C++ transport layer
//! and the official Signal Protocol Rust implementation for end-to-end encryption.

#[cfg(test)]
mod tests {
    use libsignal_protocol::*;

    #[test]
    fn test_libsignal_basic_types() {
        let device_id = DeviceId::new(1).expect("Valid device ID");
        let protocol_address = ProtocolAddress::new("test_device".to_string(), device_id);
        assert_eq!(protocol_address.name(), "test_device");
        assert_eq!(protocol_address.device_id(), device_id);
    }
}
