
#[cxx::bridge]
mod ffi {
    extern "Rust" {
        fn hello_world() -> String;
        fn add_numbers(a: i32, b: i32) -> i32;
    }
}

pub fn hello_world() -> String {
    "Hello World from Rust via CXX bridge!".to_string()
}

pub fn add_numbers(a: i32, b: i32) -> i32 {
    a + b
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
}
