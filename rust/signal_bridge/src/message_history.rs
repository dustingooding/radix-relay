//! Message history storage and retrieval
//!
//! This module provides persistent storage for message history using SQLCipher-encrypted
//! SQLite database. Messages are stored as plaintext (already decrypted by Signal Protocol)
//! and protected by full database encryption.

use rusqlite::Connection;
use std::sync::{Arc, Mutex};
use thiserror::Error;

#[derive(Error, Debug)]
pub enum MessageHistoryError {
    #[error("Database error: {0}")]
    Database(#[from] rusqlite::Error),

    #[error("Conversation not found: {0}")]
    ConversationNotFound(String),

    #[error("Message not found: {0}")]
    MessageNotFound(i64),

    #[error("UTF-8 conversion error: {0}")]
    Utf8Error(#[from] std::string::FromUtf8Error),
}

/// Message direction
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum MessageDirection {
    Incoming = 0,
    Outgoing = 1,
}

impl From<i64> for MessageDirection {
    fn from(value: i64) -> Self {
        match value {
            0 => MessageDirection::Incoming,
            1 => MessageDirection::Outgoing,
            _ => MessageDirection::Incoming,
        }
    }
}

/// Message type
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum MessageType {
    Text = 0,
    BundleAnnouncement = 1,
    System = 2,
}

impl From<i64> for MessageType {
    fn from(value: i64) -> Self {
        match value {
            0 => MessageType::Text,
            1 => MessageType::BundleAnnouncement,
            2 => MessageType::System,
            _ => MessageType::Text,
        }
    }
}

/// Delivery status for outgoing messages
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum DeliveryStatus {
    Pending = 0,
    Sent = 1,
    Delivered = 2,
    Failed = 3,
}

impl From<i64> for DeliveryStatus {
    fn from(value: i64) -> Self {
        match value {
            0 => DeliveryStatus::Pending,
            1 => DeliveryStatus::Sent,
            2 => DeliveryStatus::Delivered,
            3 => DeliveryStatus::Failed,
            _ => DeliveryStatus::Pending,
        }
    }
}

/// Stored message record
#[derive(Debug, Clone)]
pub struct StoredMessage {
    pub id: i64,
    pub conversation_id: i64,
    pub direction: MessageDirection,
    pub timestamp: u64,
    pub message_type: MessageType,
    pub content: String,
    pub delivery_status: DeliveryStatus,
    pub was_prekey_message: bool,
    pub session_established: bool,
}

/// Conversation/thread record
#[derive(Debug, Clone)]
pub struct Conversation {
    pub id: i64,
    pub rdx_fingerprint: String,
    pub last_message_timestamp: u64,
    pub unread_count: u32,
    pub archived: bool,
}

/// Message history storage and retrieval
pub struct MessageHistory {
    connection: Arc<Mutex<Connection>>,
}

impl MessageHistory {
    pub fn new(connection: Arc<Mutex<Connection>>) -> Self {
        Self { connection }
    }

    /// Get or create a conversation for a contact
    fn get_or_create_conversation(
        &self,
        rdx_fingerprint: &str,
    ) -> Result<i64, MessageHistoryError> {
        let conn = self.connection.lock().unwrap();

        let result: Result<i64, rusqlite::Error> = conn.query_row(
            "SELECT id FROM conversations WHERE rdx_fingerprint = ?1",
            [rdx_fingerprint],
            |row| row.get(0),
        );

        match result {
            Ok(id) => Ok(id),
            Err(rusqlite::Error::QueryReturnedNoRows) => {
                let nostr_pubkey: Option<String> = conn
                    .query_row(
                        "SELECT nostr_pubkey FROM contacts WHERE rdx_fingerprint = ?1",
                        [rdx_fingerprint],
                        |row| row.get(0),
                    )
                    .ok();

                conn.execute(
                    "INSERT INTO conversations (rdx_fingerprint, nostr_pubkey, last_message_timestamp)
                     VALUES (?1, ?2, 0)",
                    rusqlite::params![rdx_fingerprint, nostr_pubkey],
                )?;

                Ok(conn.last_insert_rowid())
            }
            Err(e) => Err(e.into()),
        }
    }

    /// Update conversation timestamp and unread count
    fn update_conversation(
        &self,
        conversation_id: i64,
        timestamp: u64,
        increment_unread: bool,
    ) -> Result<(), MessageHistoryError> {
        let conn = self.connection.lock().unwrap();

        if increment_unread {
            conn.execute(
                "UPDATE conversations
                 SET last_message_timestamp = ?1, unread_count = unread_count + 1
                 WHERE id = ?2",
                rusqlite::params![timestamp as i64, conversation_id],
            )?;
        } else {
            conn.execute(
                "UPDATE conversations
                 SET last_message_timestamp = ?1
                 WHERE id = ?2",
                rusqlite::params![timestamp as i64, conversation_id],
            )?;
        }

        Ok(())
    }

    /// Store an incoming message
    pub fn store_incoming_message(
        &self,
        rdx_fingerprint: &str,
        timestamp: u64,
        plaintext: &[u8],
        was_prekey_message: bool,
        session_established: bool,
    ) -> Result<i64, MessageHistoryError> {
        let conversation_id = self.get_or_create_conversation(rdx_fingerprint)?;

        let conn = self.connection.lock().unwrap();
        conn.execute(
            "INSERT INTO messages
             (conversation_id, direction, timestamp, message_type, content,
              was_prekey_message, session_established)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)",
            rusqlite::params![
                conversation_id,
                MessageDirection::Incoming as i64,
                timestamp as i64,
                MessageType::Text as i64,
                plaintext,
                was_prekey_message,
                session_established,
            ],
        )?;

        let message_id = conn.last_insert_rowid();
        drop(conn);

        self.update_conversation(conversation_id, timestamp, true)?;

        Ok(message_id)
    }

    /// Store an outgoing message
    pub fn store_outgoing_message(
        &self,
        rdx_fingerprint: &str,
        timestamp: u64,
        plaintext: &[u8],
    ) -> Result<i64, MessageHistoryError> {
        let conversation_id = self.get_or_create_conversation(rdx_fingerprint)?;

        let conn = self.connection.lock().unwrap();
        conn.execute(
            "INSERT INTO messages
             (conversation_id, direction, timestamp, message_type, content, delivery_status)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
            rusqlite::params![
                conversation_id,
                MessageDirection::Outgoing as i64,
                timestamp as i64,
                MessageType::Text as i64,
                plaintext,
                DeliveryStatus::Pending as i64,
            ],
        )?;

        let message_id = conn.last_insert_rowid();
        drop(conn);

        self.update_conversation(conversation_id, timestamp, false)?;

        Ok(message_id)
    }

    /// Update message delivery status
    pub fn update_delivery_status(
        &self,
        message_id: i64,
        status: DeliveryStatus,
    ) -> Result<(), MessageHistoryError> {
        let conn = self.connection.lock().unwrap();
        conn.execute(
            "UPDATE messages SET delivery_status = ?1 WHERE id = ?2",
            rusqlite::params![status as i64, message_id],
        )?;
        Ok(())
    }

    /// Retrieve message by ID
    pub fn get_message(&self, message_id: i64) -> Result<StoredMessage, MessageHistoryError> {
        let conn = self.connection.lock().unwrap();
        let result = conn.query_row(
            "SELECT id, conversation_id, direction, timestamp, message_type, content,
                    delivery_status, was_prekey_message, session_established
             FROM messages WHERE id = ?1",
            [message_id],
            |row| {
                let content_bytes: Vec<u8> = row.get(5)?;
                Ok(StoredMessage {
                    id: row.get(0)?,
                    conversation_id: row.get(1)?,
                    direction: row.get::<_, i64>(2)?.into(),
                    timestamp: row.get::<_, i64>(3)? as u64,
                    message_type: row.get::<_, i64>(4)?.into(),
                    content: String::from_utf8(content_bytes).unwrap_or_default(),
                    delivery_status: row.get::<_, i64>(6)?.into(),
                    was_prekey_message: row.get(7)?,
                    session_established: row.get(8)?,
                })
            },
        );

        match result {
            Ok(msg) => Ok(msg),
            Err(rusqlite::Error::QueryReturnedNoRows) => {
                Err(MessageHistoryError::MessageNotFound(message_id))
            }
            Err(e) => Err(e.into()),
        }
    }

    /// Get messages for a conversation (paginated, newest first)
    pub fn get_conversation_messages(
        &self,
        rdx_fingerprint: &str,
        limit: u32,
        offset: u32,
    ) -> Result<Vec<StoredMessage>, MessageHistoryError> {
        let conn = self.connection.lock().unwrap();

        let conversation_id: Result<i64, rusqlite::Error> = conn.query_row(
            "SELECT id FROM conversations WHERE rdx_fingerprint = ?1",
            [rdx_fingerprint],
            |row| row.get(0),
        );

        let conversation_id = match conversation_id {
            Ok(id) => id,
            Err(rusqlite::Error::QueryReturnedNoRows) => return Ok(Vec::new()),
            Err(e) => return Err(e.into()),
        };

        let mut stmt = conn.prepare(
            "SELECT id, conversation_id, direction, timestamp, message_type, content,
                    delivery_status, was_prekey_message, session_established
             FROM messages
             WHERE conversation_id = ?1
             ORDER BY timestamp DESC
             LIMIT ?2 OFFSET ?3",
        )?;

        let messages = stmt
            .query_map(rusqlite::params![conversation_id, limit, offset], |row| {
                let content_bytes: Vec<u8> = row.get(5)?;
                Ok(StoredMessage {
                    id: row.get(0)?,
                    conversation_id: row.get(1)?,
                    direction: row.get::<_, i64>(2)?.into(),
                    timestamp: row.get::<_, i64>(3)? as u64,
                    message_type: row.get::<_, i64>(4)?.into(),
                    content: String::from_utf8(content_bytes).unwrap_or_default(),
                    delivery_status: row.get::<_, i64>(6)?.into(),
                    was_prekey_message: row.get(7)?,
                    session_established: row.get(8)?,
                })
            })?
            .collect::<Result<Vec<_>, _>>()?;

        Ok(messages)
    }

    /// Get all conversations ordered by recent activity
    pub fn get_conversations(
        &self,
        include_archived: bool,
    ) -> Result<Vec<Conversation>, MessageHistoryError> {
        let conn = self.connection.lock().unwrap();

        let query = if include_archived {
            "SELECT id, rdx_fingerprint, last_message_timestamp, unread_count, archived
             FROM conversations
             ORDER BY last_message_timestamp DESC"
        } else {
            "SELECT id, rdx_fingerprint, last_message_timestamp, unread_count, archived
             FROM conversations
             WHERE archived = 0
             ORDER BY last_message_timestamp DESC"
        };

        let mut stmt = conn.prepare(query)?;

        let conversations = stmt
            .query_map([], |row| {
                Ok(Conversation {
                    id: row.get(0)?,
                    rdx_fingerprint: row.get(1)?,
                    last_message_timestamp: row.get::<_, i64>(2)? as u64,
                    unread_count: row.get::<_, i64>(3)? as u32,
                    archived: row.get(4)?,
                })
            })?
            .collect::<Result<Vec<_>, _>>()?;

        Ok(conversations)
    }

    /// Get unread message count for a conversation
    pub fn get_unread_count(&self, rdx_fingerprint: &str) -> Result<u32, MessageHistoryError> {
        let conn = self.connection.lock().unwrap();

        let result: Result<i64, rusqlite::Error> = conn.query_row(
            "SELECT unread_count FROM conversations WHERE rdx_fingerprint = ?1",
            [rdx_fingerprint],
            |row| row.get(0),
        );

        match result {
            Ok(count) => Ok(count as u32),
            Err(rusqlite::Error::QueryReturnedNoRows) => Ok(0),
            Err(e) => Err(e.into()),
        }
    }

    /// Mark conversation as read
    pub fn mark_conversation_read(&self, rdx_fingerprint: &str) -> Result<(), MessageHistoryError> {
        let conn = self.connection.lock().unwrap();
        conn.execute(
            "UPDATE conversations SET unread_count = 0 WHERE rdx_fingerprint = ?1",
            [rdx_fingerprint],
        )?;
        Ok(())
    }

    /// Delete message
    pub fn delete_message(&self, message_id: i64) -> Result<(), MessageHistoryError> {
        let conn = self.connection.lock().unwrap();
        conn.execute("DELETE FROM messages WHERE id = ?1", [message_id])?;
        Ok(())
    }

    /// Delete entire conversation
    pub fn delete_conversation(&self, rdx_fingerprint: &str) -> Result<(), MessageHistoryError> {
        let conn = self.connection.lock().unwrap();

        let conversation_id: Result<i64, rusqlite::Error> = conn.query_row(
            "SELECT id FROM conversations WHERE rdx_fingerprint = ?1",
            [rdx_fingerprint],
            |row| row.get(0),
        );

        if let Ok(id) = conversation_id {
            conn.execute("DELETE FROM messages WHERE conversation_id = ?1", [id])?;
            conn.execute("DELETE FROM conversations WHERE id = ?1", [id])?;
        }

        Ok(())
    }
}
