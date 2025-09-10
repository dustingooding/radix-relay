/// A simple test function to verify Rust compilation works
pub fn hello_radix_relay() -> String {
    "Hello from Radix Relay Rust workspace!".to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello_function() {
        let result = hello_radix_relay();
        assert_eq!(result, "Hello from Radix Relay Rust workspace!");
    }
}
