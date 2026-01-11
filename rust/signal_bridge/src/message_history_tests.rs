//! Tests for message history storage and retrieval

#[cfg(test)]
mod tests {
    use crate::db_encryption::generate_db_key;
    use rusqlite::Connection;
    use std::fs;
    use std::sync::{Arc, Mutex};

    fn cleanup_test_db(db_path: &str) {
        let _ = fs::remove_file(db_path);
        let key_path = format!("{}.key", db_path);
        let _ = fs::remove_file(key_path);
    }

    fn create_test_storage(db_path: &str) -> Arc<Mutex<Connection>> {
        let key = generate_db_key().expect("Failed to generate key");
        let conn = Connection::open(db_path).expect("Failed to open connection");
        conn.pragma_update(None, "key", hex::encode(key))
            .expect("Failed to set encryption key");

        conn.execute(
            "CREATE TABLE IF NOT EXISTS schema_info (
                version INTEGER NOT NULL DEFAULT 1,
                updated_at INTEGER DEFAULT (strftime('%s', 'now'))
            )",
            [],
        )
        .expect("Failed to create schema_info");

        conn.execute("INSERT OR IGNORE INTO schema_info (version) VALUES (2)", [])
            .expect("Failed to insert schema version");

        conn.execute(
            "CREATE TABLE IF NOT EXISTS contacts (
                rdx_fingerprint TEXT PRIMARY KEY,
                nostr_pubkey TEXT UNIQUE NOT NULL,
                user_alias TEXT,
                signal_identity_key BLOB NOT NULL,
                first_seen INTEGER NOT NULL,
                last_updated INTEGER NOT NULL
            )",
            [],
        )
        .expect("Failed to create contacts table");

        conn.execute(
            "CREATE TABLE IF NOT EXISTS conversations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                rdx_fingerprint TEXT NOT NULL UNIQUE,
                nostr_pubkey TEXT,
                last_message_timestamp INTEGER NOT NULL,
                unread_count INTEGER DEFAULT 0,
                archived BOOLEAN DEFAULT 0,
                FOREIGN KEY (rdx_fingerprint) REFERENCES contacts(rdx_fingerprint) ON DELETE CASCADE
            )",
            [],
        )
        .expect("Failed to create conversations table");

        conn.execute(
            "CREATE INDEX IF NOT EXISTS idx_conversations_timestamp
             ON conversations(last_message_timestamp DESC)",
            [],
        )
        .expect("Failed to create timestamp index");

        conn.execute(
            "CREATE INDEX IF NOT EXISTS idx_conversations_unread
             ON conversations(unread_count) WHERE unread_count > 0",
            [],
        )
        .expect("Failed to create unread index");

        conn.execute(
            "CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                conversation_id INTEGER NOT NULL,
                direction INTEGER NOT NULL,
                timestamp INTEGER NOT NULL,
                message_type INTEGER NOT NULL,
                content BLOB NOT NULL,
                delivery_status INTEGER DEFAULT 0,
                was_prekey_message BOOLEAN DEFAULT 0,
                session_established BOOLEAN DEFAULT 0,
                FOREIGN KEY (conversation_id) REFERENCES conversations(id) ON DELETE CASCADE
            )",
            [],
        )
        .expect("Failed to create messages table");

        conn.execute(
            "CREATE INDEX IF NOT EXISTS idx_messages_conversation
             ON messages(conversation_id, timestamp DESC)",
            [],
        )
        .expect("Failed to create conversation index");

        conn.execute(
            "CREATE INDEX IF NOT EXISTS idx_messages_timestamp
             ON messages(timestamp DESC)",
            [],
        )
        .expect("Failed to create timestamp index");

        conn.execute(
            "CREATE INDEX IF NOT EXISTS idx_messages_undelivered
             ON messages(delivery_status) WHERE delivery_status IN (0, 3)",
            [],
        )
        .expect("Failed to create delivery status index");

        Arc::new(Mutex::new(conn))
    }

    fn insert_test_contact(conn: &Connection, rdx_fingerprint: &str, nostr_pubkey: &str) {
        let now = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_secs() as i64;

        conn.execute(
            "INSERT OR IGNORE INTO contacts
             (rdx_fingerprint, nostr_pubkey, signal_identity_key, first_seen, last_updated)
             VALUES (?1, ?2, ?3, ?4, ?5)",
            rusqlite::params![rdx_fingerprint, nostr_pubkey, vec![0u8; 32], now, now],
        )
        .expect("Failed to insert test contact");
    }

    #[test]
    fn test_store_incoming_message() {
        use crate::message_history::{MessageDirection, MessageHistory};

        let test_db_path = "test_store_incoming_message.db";
        cleanup_test_db(test_db_path);

        let conn = create_test_storage(test_db_path);
        insert_test_contact(&conn.lock().unwrap(), "rdx:test123", "nostr:abc");

        let history = MessageHistory::new(conn.clone());
        let timestamp = 1234567890u64;
        let plaintext = b"Test message content";

        let message_id = history
            .store_incoming_message("rdx:test123", timestamp, plaintext, false, true)
            .expect("Failed to store incoming message");

        assert!(message_id > 0, "Message ID should be positive");

        let stored = history
            .get_message(message_id)
            .expect("Failed to retrieve message");

        assert_eq!(stored.direction, MessageDirection::Incoming);
        assert_eq!(stored.timestamp, timestamp);
        assert_eq!(
            stored.content,
            String::from_utf8(plaintext.to_vec()).unwrap()
        );
        assert!(!stored.was_prekey_message);
        assert!(stored.session_established);

        cleanup_test_db(test_db_path);
    }

    #[test]
    fn test_store_outgoing_message() {
        use crate::message_history::{DeliveryStatus, MessageDirection, MessageHistory};

        let test_db_path = "test_store_outgoing_message.db";
        cleanup_test_db(test_db_path);

        let conn = create_test_storage(test_db_path);
        insert_test_contact(&conn.lock().unwrap(), "rdx:test456", "nostr:def");

        let history = MessageHistory::new(conn.clone());
        let timestamp = 1234567890u64;
        let plaintext = b"Outgoing test message";

        let message_id = history
            .store_outgoing_message("rdx:test456", timestamp, plaintext)
            .expect("Failed to store outgoing message");

        assert!(message_id > 0, "Message ID should be positive");

        let stored = history
            .get_message(message_id)
            .expect("Failed to retrieve message");

        assert_eq!(stored.direction, MessageDirection::Outgoing);
        assert_eq!(stored.timestamp, timestamp);
        assert_eq!(
            stored.content,
            String::from_utf8(plaintext.to_vec()).unwrap()
        );
        assert_eq!(stored.delivery_status, DeliveryStatus::Pending);

        cleanup_test_db(test_db_path);
    }

    #[test]
    fn test_update_delivery_status() {
        use crate::message_history::{DeliveryStatus, MessageHistory};

        let test_db_path = "test_update_delivery_status.db";
        cleanup_test_db(test_db_path);

        let conn = create_test_storage(test_db_path);
        insert_test_contact(&conn.lock().unwrap(), "rdx:test789", "nostr:ghi");

        let history = MessageHistory::new(conn.clone());
        let message_id = history
            .store_outgoing_message("rdx:test789", 1234567890, b"Test message")
            .expect("Failed to store message");

        history
            .update_delivery_status(message_id, DeliveryStatus::Sent)
            .expect("Failed to update delivery status");

        let stored = history
            .get_message(message_id)
            .expect("Failed to retrieve message");

        assert_eq!(stored.delivery_status, DeliveryStatus::Sent);

        cleanup_test_db(test_db_path);
    }

    #[test]
    fn test_get_conversation_messages() {
        use crate::message_history::MessageHistory;

        let test_db_path = "test_get_conversation_messages.db";
        cleanup_test_db(test_db_path);

        let conn = create_test_storage(test_db_path);
        insert_test_contact(&conn.lock().unwrap(), "rdx:conv123", "nostr:jkl");

        let history = MessageHistory::new(conn.clone());

        history
            .store_incoming_message("rdx:conv123", 1000, b"Message 1", false, true)
            .expect("Failed to store message 1");

        history
            .store_outgoing_message("rdx:conv123", 2000, b"Message 2")
            .expect("Failed to store message 2");

        history
            .store_incoming_message("rdx:conv123", 3000, b"Message 3", false, true)
            .expect("Failed to store message 3");

        let messages = history
            .get_conversation_messages("rdx:conv123", 10, 0)
            .expect("Failed to get conversation messages");

        assert_eq!(messages.len(), 3, "Should retrieve 3 messages");
        assert_eq!(
            messages[0].timestamp, 3000,
            "Messages should be ordered newest first"
        );
        assert_eq!(messages[1].timestamp, 2000);
        assert_eq!(messages[2].timestamp, 1000);

        cleanup_test_db(test_db_path);
    }

    #[test]
    fn test_get_conversation_messages_pagination() {
        use crate::message_history::MessageHistory;

        let test_db_path = "test_pagination.db";
        cleanup_test_db(test_db_path);

        let conn = create_test_storage(test_db_path);
        insert_test_contact(&conn.lock().unwrap(), "rdx:page123", "nostr:mno");

        let history = MessageHistory::new(conn.clone());

        for i in 0..10 {
            history
                .store_incoming_message("rdx:page123", 1000 + i * 100, b"Test", false, true)
                .expect("Failed to store message");
        }

        let page1 = history
            .get_conversation_messages("rdx:page123", 5, 0)
            .expect("Failed to get page 1");

        let page2 = history
            .get_conversation_messages("rdx:page123", 5, 5)
            .expect("Failed to get page 2");

        assert_eq!(page1.len(), 5, "First page should have 5 messages");
        assert_eq!(page2.len(), 5, "Second page should have 5 messages");
        assert!(
            page1[0].timestamp > page2[0].timestamp,
            "Pages should not overlap"
        );

        cleanup_test_db(test_db_path);
    }

    #[test]
    fn test_get_conversations() {
        use crate::message_history::MessageHistory;

        let test_db_path = "test_get_conversations.db";
        cleanup_test_db(test_db_path);

        let conn = create_test_storage(test_db_path);
        insert_test_contact(&conn.lock().unwrap(), "rdx:alice", "nostr:alice");
        insert_test_contact(&conn.lock().unwrap(), "rdx:bob", "nostr:bob");

        let history = MessageHistory::new(conn.clone());

        history
            .store_incoming_message("rdx:alice", 1000, b"From Alice", false, true)
            .expect("Failed to store Alice message");

        history
            .store_incoming_message("rdx:bob", 2000, b"From Bob", false, true)
            .expect("Failed to store Bob message");

        let conversations = history
            .get_conversations(false)
            .expect("Failed to get conversations");

        assert_eq!(conversations.len(), 2, "Should have 2 conversations");
        assert_eq!(
            conversations[0].rdx_fingerprint, "rdx:bob",
            "Most recent conversation should be first"
        );
        assert_eq!(conversations[0].last_message_timestamp, 2000);
        assert_eq!(conversations[1].rdx_fingerprint, "rdx:alice");

        cleanup_test_db(test_db_path);
    }

    #[test]
    fn test_unread_count() {
        use crate::message_history::MessageHistory;

        let test_db_path = "test_unread_count.db";
        cleanup_test_db(test_db_path);

        let conn = create_test_storage(test_db_path);
        insert_test_contact(&conn.lock().unwrap(), "rdx:unread", "nostr:unread");

        let history = MessageHistory::new(conn.clone());

        history
            .store_incoming_message("rdx:unread", 1000, b"Unread 1", false, true)
            .expect("Failed to store message 1");

        history
            .store_incoming_message("rdx:unread", 2000, b"Unread 2", false, true)
            .expect("Failed to store message 2");

        let count = history
            .get_unread_count("rdx:unread")
            .expect("Failed to get unread count");

        assert_eq!(count, 2, "Should have 2 unread messages");

        history
            .mark_conversation_read("rdx:unread")
            .expect("Failed to mark as read");

        let count_after = history
            .get_unread_count("rdx:unread")
            .expect("Failed to get unread count after marking read");

        assert_eq!(
            count_after, 0,
            "Should have 0 unread messages after marking read"
        );

        cleanup_test_db(test_db_path);
    }

    #[test]
    fn test_delete_message() {
        use crate::message_history::MessageHistory;

        let test_db_path = "test_delete_message.db";
        cleanup_test_db(test_db_path);

        let conn = create_test_storage(test_db_path);
        insert_test_contact(&conn.lock().unwrap(), "rdx:delete", "nostr:delete");

        let history = MessageHistory::new(conn.clone());

        let message_id = history
            .store_incoming_message("rdx:delete", 1000, b"To be deleted", false, true)
            .expect("Failed to store message");

        history
            .delete_message(message_id)
            .expect("Failed to delete message");

        let result = history.get_message(message_id);
        assert!(result.is_err(), "Deleted message should not be retrievable");

        cleanup_test_db(test_db_path);
    }

    #[test]
    fn test_delete_conversation() {
        use crate::message_history::MessageHistory;

        let test_db_path = "test_delete_conversation.db";
        cleanup_test_db(test_db_path);

        let conn = create_test_storage(test_db_path);
        insert_test_contact(&conn.lock().unwrap(), "rdx:delconv", "nostr:delconv");

        let history = MessageHistory::new(conn.clone());

        history
            .store_incoming_message("rdx:delconv", 1000, b"Message 1", false, true)
            .expect("Failed to store message 1");

        history
            .store_incoming_message("rdx:delconv", 2000, b"Message 2", false, true)
            .expect("Failed to store message 2");

        history
            .delete_conversation("rdx:delconv")
            .expect("Failed to delete conversation");

        let messages = history
            .get_conversation_messages("rdx:delconv", 10, 0)
            .expect("Failed to get messages");

        assert_eq!(messages.len(), 0, "All messages should be deleted");

        cleanup_test_db(test_db_path);
    }

    #[test]
    fn test_conversation_cascade_delete() {
        use crate::message_history::MessageHistory;

        let test_db_path = "test_cascade_delete.db";
        cleanup_test_db(test_db_path);

        let conn = create_test_storage(test_db_path);
        insert_test_contact(&conn.lock().unwrap(), "rdx:cascade", "nostr:cascade");

        let history = MessageHistory::new(conn.clone());

        history
            .store_incoming_message("rdx:cascade", 1000, b"Test message", false, true)
            .expect("Failed to store message");

        conn.lock()
            .unwrap()
            .execute(
                "DELETE FROM contacts WHERE rdx_fingerprint = ?1",
                ["rdx:cascade"],
            )
            .expect("Failed to delete contact");

        let messages = history
            .get_conversation_messages("rdx:cascade", 10, 0)
            .expect("Failed to get messages");

        assert_eq!(
            messages.len(),
            0,
            "Messages should cascade delete with conversation"
        );

        cleanup_test_db(test_db_path);
    }

    #[test]
    fn test_encrypted_message_content_stored_as_plaintext() {
        use crate::message_history::MessageHistory;

        let test_db_path = "test_plaintext_storage.db";
        cleanup_test_db(test_db_path);

        let conn = create_test_storage(test_db_path);
        insert_test_contact(&conn.lock().unwrap(), "rdx:plain", "nostr:plain");

        let history = MessageHistory::new(conn.clone());
        let sensitive_content = b"This is sensitive plaintext";

        let message_id = history
            .store_incoming_message("rdx:plain", 1000, sensitive_content, false, true)
            .expect("Failed to store message");

        let raw_content: Vec<u8> = conn
            .lock()
            .unwrap()
            .query_row(
                "SELECT content FROM messages WHERE id = ?1",
                [message_id],
                |row| row.get(0),
            )
            .expect("Failed to query raw content");

        assert_eq!(
            raw_content,
            sensitive_content.to_vec(),
            "Content should be stored as plaintext (SQLCipher encrypts entire DB)"
        );

        cleanup_test_db(test_db_path);
    }

    #[test]
    fn test_mark_conversation_read_with_timestamp() {
        use crate::message_history::MessageHistory;

        let test_db_path = "test_mark_read_timestamp.db";
        cleanup_test_db(test_db_path);

        let conn = create_test_storage(test_db_path);
        insert_test_contact(
            &conn.lock().unwrap(),
            "rdx:timestamp_test",
            "nostr:timestamp_test",
        );

        let history = MessageHistory::new(conn.clone());

        history
            .store_incoming_message("rdx:timestamp_test", 1000, b"Message 1", false, true)
            .expect("Failed to store message 1");

        history
            .store_incoming_message("rdx:timestamp_test", 2000, b"Message 2", false, true)
            .expect("Failed to store message 2");

        history
            .store_incoming_message("rdx:timestamp_test", 3000, b"Message 3", false, true)
            .expect("Failed to store message 3");

        let initial_count = history
            .get_unread_count("rdx:timestamp_test")
            .expect("Failed to get initial unread count");
        assert_eq!(initial_count, 3, "Should have 3 unread messages initially");

        history
            .mark_conversation_read_up_to("rdx:timestamp_test", 2000)
            .expect("Failed to mark as read up to timestamp 2000");

        let count_after = history
            .get_unread_count("rdx:timestamp_test")
            .expect("Failed to get unread count after marking");

        assert_eq!(
            count_after, 1,
            "Should have 1 unread message remaining (message at timestamp 3000)"
        );

        let messages = history
            .get_conversation_messages("rdx:timestamp_test", 10, 0)
            .expect("Failed to get messages");

        assert_eq!(messages.len(), 3, "All messages should still exist");

        cleanup_test_db(test_db_path);
    }

    #[test]
    fn test_mark_conversation_read_with_timestamp_race_condition() {
        use crate::message_history::MessageHistory;

        let test_db_path = "test_mark_read_race.db";
        cleanup_test_db(test_db_path);

        let conn = create_test_storage(test_db_path);
        insert_test_contact(&conn.lock().unwrap(), "rdx:race_test", "nostr:race_test");

        let history = MessageHistory::new(conn.clone());

        history
            .store_incoming_message("rdx:race_test", 1000, b"Old message 1", false, true)
            .expect("Failed to store old message 1");

        history
            .store_incoming_message("rdx:race_test", 2000, b"Old message 2", false, true)
            .expect("Failed to store old message 2");

        let messages = history
            .get_conversation_messages("rdx:race_test", 10, 0)
            .expect("Failed to get messages");

        let newest_timestamp = messages.first().map(|m| m.timestamp).unwrap_or(0);

        history
            .store_incoming_message(
                "rdx:race_test",
                3000,
                b"New message during race",
                false,
                true,
            )
            .expect("Failed to store new message");

        history
            .mark_conversation_read_up_to("rdx:race_test", newest_timestamp)
            .expect("Failed to mark as read");

        let count_after = history
            .get_unread_count("rdx:race_test")
            .expect("Failed to get unread count");

        assert_eq!(
            count_after, 1,
            "New message that arrived during race should remain unread"
        );

        cleanup_test_db(test_db_path);
    }
}
