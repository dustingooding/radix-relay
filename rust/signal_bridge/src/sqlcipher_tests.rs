//! Tests for SQLCipher encrypted database operations

#[cfg(test)]
mod tests {
    use crate::db_encryption::{generate_db_key, DbKey};
    use rusqlite::Connection;
    use std::fs;

    fn cleanup_test_db(db_path: &str) {
        let _ = fs::remove_file(db_path);
    }

    fn open_encrypted_connection(
        db_path: &str,
        key: &DbKey,
    ) -> Result<Connection, rusqlite::Error> {
        let conn = Connection::open(db_path)?;
        conn.pragma_update(None, "key", hex::encode(key))?;
        Ok(conn)
    }

    #[test]
    fn test_create_encrypted_database() {
        let test_db_path = "test_encrypted_db.db";
        cleanup_test_db(test_db_path);

        let key = generate_db_key().expect("Failed to generate key");
        let conn = open_encrypted_connection(test_db_path, &key)
            .expect("Failed to open encrypted connection");

        conn.execute(
            "CREATE TABLE test_table (id INTEGER PRIMARY KEY, value TEXT)",
            [],
        )
        .expect("Failed to create table");

        cleanup_test_db(test_db_path);
    }

    #[test]
    fn test_encrypted_database_stores_and_retrieves_data() {
        let test_db_path = "test_encrypted_db_data.db";
        cleanup_test_db(test_db_path);

        let key = generate_db_key().expect("Failed to generate key");

        {
            let conn = open_encrypted_connection(test_db_path, &key)
                .expect("Failed to open encrypted connection");

            conn.execute(
                "CREATE TABLE test_table (id INTEGER PRIMARY KEY, value TEXT)",
                [],
            )
            .expect("Failed to create table");

            conn.execute("INSERT INTO test_table (value) VALUES (?1)", ["test_value"])
                .expect("Failed to insert data");
        }

        {
            let conn = open_encrypted_connection(test_db_path, &key)
                .expect("Failed to reopen encrypted connection");

            let value: String = conn
                .query_row("SELECT value FROM test_table WHERE id = 1", [], |row| {
                    row.get(0)
                })
                .expect("Failed to retrieve data");

            assert_eq!(
                value, "test_value",
                "Retrieved value should match inserted value"
            );
        }

        cleanup_test_db(test_db_path);
    }

    #[test]
    fn test_wrong_key_cannot_open_encrypted_database() {
        let test_db_path = "test_encrypted_db_wrong_key.db";
        cleanup_test_db(test_db_path);

        let key1 = generate_db_key().expect("Failed to generate key1");
        let key2 = generate_db_key().expect("Failed to generate key2");

        {
            let conn = open_encrypted_connection(test_db_path, &key1)
                .expect("Failed to open encrypted connection");

            conn.execute(
                "CREATE TABLE test_table (id INTEGER PRIMARY KEY, value TEXT)",
                [],
            )
            .expect("Failed to create table");
        }

        {
            let result = open_encrypted_connection(test_db_path, &key2);
            assert!(result.is_ok(), "Connection should open");

            if let Ok(conn) = result {
                let query_result = conn.query_row("SELECT * FROM test_table", [], |_| Ok(()));
                assert!(
                    query_result.is_err(),
                    "Query should fail with wrong encryption key"
                );
            }
        }

        cleanup_test_db(test_db_path);
    }

    #[test]
    fn test_unencrypted_connection_cannot_read_encrypted_database() {
        let test_db_path = "test_encrypted_db_unencrypted_read.db";
        cleanup_test_db(test_db_path);

        let key = generate_db_key().expect("Failed to generate key");

        {
            let conn = open_encrypted_connection(test_db_path, &key)
                .expect("Failed to open encrypted connection");

            conn.execute(
                "CREATE TABLE test_table (id INTEGER PRIMARY KEY, value TEXT)",
                [],
            )
            .expect("Failed to create table");
        }

        {
            let result = Connection::open(test_db_path);
            assert!(result.is_ok(), "Unencrypted connection should open file");

            if let Ok(conn) = result {
                let query_result = conn.query_row("SELECT * FROM test_table", [], |_| Ok(()));
                assert!(
                    query_result.is_err(),
                    "Unencrypted connection should not be able to read encrypted database"
                );
            }
        }

        cleanup_test_db(test_db_path);
    }
}
