//! SQLite storage implementation for libsignal
//!
//! This module provides a SQLite-backed storage implementation that persists
//! data across application restarts, unlike the in-memory storage.

use crate::storage_trait::*;
use async_trait::async_trait;
use libsignal_protocol::{
    message_decrypt, message_encrypt, process_prekey_bundle, CiphertextMessage, Direction,
    GenericSignedPreKey, IdentityChange, IdentityKey, IdentityKeyPair, IdentityKeyStore,
    KyberPreKeyId, KyberPreKeyRecord, KyberPreKeyStore, PreKeyBundle, PreKeyId, PreKeyRecord,
    PreKeyStore, ProtocolAddress, SessionRecord, SessionStore, SignalProtocolError, SignedPreKeyId,
    SignedPreKeyRecord, SignedPreKeyStore, UsePQRatchet,
};
use rusqlite::Connection;
use std::sync::{Arc, Mutex};

/// Bundle metadata tuple: (pre_key_id, signed_pre_key_id, kyber_pre_key_id)
type BundleMetadata = (u32, u32, u32);

/// SQLite-backed Signal Protocol storage with data persistence
pub struct SqliteStorage {
    connection: Arc<Mutex<Connection>>,
    session_store: Option<SqliteSessionStore>,
    identity_store: Option<SqliteIdentityStore>,
    pre_key_store: Option<SqlitePreKeyStore>,
    signed_pre_key_store: Option<SqliteSignedPreKeyStore>,
    kyber_pre_key_store: Option<SqliteKyberPreKeyStore>,
    is_closed: bool,
}

impl SqliteStorage {
    pub async fn new(db_path: &str) -> Result<Self, Box<dyn std::error::Error>> {
        let connection = Arc::new(Mutex::new(Connection::open(db_path)?));

        Ok(Self {
            connection,
            session_store: None,
            identity_store: None,
            pre_key_store: None,
            signed_pre_key_store: None,
            kyber_pre_key_store: None,
            is_closed: false,
        })
    }

    pub fn is_closed(&self) -> bool {
        self.is_closed
    }

    pub fn initialize_schema(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        {
            let conn = self.connection.lock().unwrap();

            conn.execute(
                "CREATE TABLE IF NOT EXISTS schema_info (
                    version INTEGER NOT NULL DEFAULT 1,
                    updated_at INTEGER DEFAULT (strftime('%s', 'now'))
                )",
                [],
            )?;

            conn.execute("INSERT OR IGNORE INTO schema_info (version) VALUES (1)", [])?;

            SqliteIdentityStore::create_tables(&conn)?;
            SqliteSessionStore::create_tables(&conn)?;
            SqlitePreKeyStore::create_tables(&conn)?;
            SqliteSignedPreKeyStore::create_tables(&conn)?;
            SqliteKyberPreKeyStore::create_tables(&conn)?;

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
            )?;

            conn.execute(
                "CREATE INDEX IF NOT EXISTS idx_contacts_alias
                 ON contacts(user_alias) WHERE user_alias IS NOT NULL",
                [],
            )?;

            conn.execute(
                "CREATE INDEX IF NOT EXISTS idx_contacts_nostr_pubkey
                 ON contacts(nostr_pubkey)",
                [],
            )?;

            conn.execute(
                "CREATE TABLE IF NOT EXISTS settings (
                    key TEXT PRIMARY KEY,
                    value TEXT NOT NULL,
                    updated_at INTEGER DEFAULT (strftime('%s', 'now'))
                )",
                [],
            )?;

            conn.execute(
                "CREATE TABLE IF NOT EXISTS bundle_metadata (
                    id INTEGER PRIMARY KEY CHECK (id = 1),
                    pre_key_id INTEGER NOT NULL,
                    signed_pre_key_id INTEGER NOT NULL,
                    kyber_pre_key_id INTEGER NOT NULL,
                    published_at INTEGER NOT NULL
                )",
                [],
            )?;
        }

        self.session_store = Some(SqliteSessionStore::new(self.connection.clone()));
        self.identity_store = Some(SqliteIdentityStore::new(self.connection.clone()));
        self.pre_key_store = Some(SqlitePreKeyStore::new(self.connection.clone()));
        self.signed_pre_key_store = Some(SqliteSignedPreKeyStore::new(self.connection.clone()));
        self.kyber_pre_key_store = Some(SqliteKyberPreKeyStore::new(self.connection.clone()));

        Ok(())
    }

    pub fn get_schema_version(&self) -> Result<i32, Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn.prepare("SELECT version FROM schema_info")?;
        let version: i32 = stmt.query_row([], |row| row.get(0))?;
        Ok(version)
    }

    pub fn connection(&self) -> Arc<Mutex<Connection>> {
        self.connection.clone()
    }

    pub fn get_last_message_timestamp(&self) -> Result<u64, Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        let mut stmt =
            conn.prepare("SELECT value FROM settings WHERE key = 'last_message_timestamp'")?;

        match stmt.query_row([], |row| row.get::<_, String>(0)) {
            Ok(value_str) => Ok(value_str.parse::<u64>()?),
            Err(rusqlite::Error::QueryReturnedNoRows) => Ok(0),
            Err(e) => Err(Box::new(e)),
        }
    }

    pub fn set_last_message_timestamp(
        &mut self,
        timestamp: u64,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        conn.execute(
            "INSERT OR REPLACE INTO settings (key, value, updated_at) VALUES ('last_message_timestamp', ?1, strftime('%s', 'now'))",
            [timestamp.to_string()],
        )?;
        Ok(())
    }

    pub fn record_published_bundle(
        &mut self,
        pre_key_id: u32,
        signed_pre_key_id: u32,
        kyber_pre_key_id: u32,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        let now = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)?
            .as_secs();

        conn.execute(
            "INSERT OR REPLACE INTO bundle_metadata (id, pre_key_id, signed_pre_key_id, kyber_pre_key_id, published_at)
             VALUES (1, ?1, ?2, ?3, ?4)",
            rusqlite::params![pre_key_id, signed_pre_key_id, kyber_pre_key_id, now],
        )?;
        Ok(())
    }

    pub fn get_last_published_bundle_metadata(
        &self,
    ) -> Result<Option<BundleMetadata>, Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn.prepare(
            "SELECT pre_key_id, signed_pre_key_id, kyber_pre_key_id FROM bundle_metadata WHERE id = 1",
        )?;

        match stmt.query_row([], |row| Ok((row.get(0)?, row.get(1)?, row.get(2)?))) {
            Ok(result) => Ok(Some(result)),
            Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
            Err(e) => Err(Box::new(e)),
        }
    }
}

impl SignalStorageContainer for SqliteStorage {
    type SessionStore = SqliteSessionStore;
    type IdentityStore = SqliteIdentityStore;
    type PreKeyStore = SqlitePreKeyStore;
    type SignedPreKeyStore = SqliteSignedPreKeyStore;
    type KyberPreKeyStore = SqliteKyberPreKeyStore;

    fn session_store(&mut self) -> &mut Self::SessionStore {
        if self.is_closed {
            panic!("Storage has been closed");
        }
        self.session_store
            .as_mut()
            .expect("Storage not initialized")
    }

    fn identity_store(&mut self) -> &mut Self::IdentityStore {
        if self.is_closed {
            panic!("Storage has been closed");
        }
        self.identity_store
            .as_mut()
            .expect("Storage not initialized")
    }

    fn pre_key_store(&mut self) -> &mut Self::PreKeyStore {
        if self.is_closed {
            panic!("Storage has been closed");
        }
        self.pre_key_store
            .as_mut()
            .expect("Storage not initialized")
    }

    fn signed_pre_key_store(&mut self) -> &mut Self::SignedPreKeyStore {
        if self.is_closed {
            panic!("Storage has been closed");
        }
        self.signed_pre_key_store
            .as_mut()
            .expect("Storage not initialized")
    }

    fn kyber_pre_key_store(&mut self) -> &mut Self::KyberPreKeyStore {
        if self.is_closed {
            panic!("Storage has been closed");
        }
        self.kyber_pre_key_store
            .as_mut()
            .expect("Storage not initialized")
    }

    fn initialize(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        self.initialize_schema()
    }

    fn close(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        if self.is_closed {
            return Ok(());
        }

        {
            let conn = self.connection.lock().unwrap();
            conn.execute("PRAGMA optimize", [])?;
            conn.execute("PRAGMA wal_checkpoint(FULL)", []).ok(); // WAL checkpoint if using WAL mode
        }

        self.is_closed = true;

        self.session_store = None;
        self.identity_store = None;
        self.pre_key_store = None;
        self.signed_pre_key_store = None;
        self.kyber_pre_key_store = None;

        Ok(())
    }

    fn storage_type(&self) -> &'static str {
        "sqlite"
    }
}

/// SQLite-backed session storage with data persistence
pub struct SqliteSessionStore {
    connection: Arc<Mutex<Connection>>,
}

impl SqliteSessionStore {
    pub fn new(connection: Arc<Mutex<Connection>>) -> Self {
        Self { connection }
    }

    pub fn create_tables(connection: &Connection) -> Result<(), Box<dyn std::error::Error>> {
        connection.execute(
            "CREATE TABLE IF NOT EXISTS sessions (
                address TEXT NOT NULL,
                device_id INTEGER NOT NULL DEFAULT 1,
                session_data BLOB NOT NULL,
                created_at INTEGER DEFAULT (strftime('%s', 'now')),
                updated_at INTEGER DEFAULT (strftime('%s', 'now')),
                PRIMARY KEY (address, device_id)
            )",
            [],
        )?;

        Ok(())
    }
}

#[async_trait(?Send)]
impl SessionStore for SqliteSessionStore {
    async fn load_session(
        &self,
        address: &ProtocolAddress,
    ) -> Result<Option<SessionRecord>, SignalProtocolError> {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn
            .prepare("SELECT session_data FROM sessions WHERE address = ? AND device_id = ?")
            .map_err(|e| {
                SignalProtocolError::InvalidState(
                    "storage",
                    format!("Failed to prepare statement: {}", e),
                )
            })?;

        let result = stmt.query_row(
            [address.name(), &u32::from(address.device_id()).to_string()],
            |row| {
                let data: Vec<u8> = row.get(0)?;
                Ok(data)
            },
        );

        match result {
            Ok(data) => {
                let session = SessionRecord::deserialize(&data).map_err(|e| {
                    SignalProtocolError::InvalidState(
                        "storage",
                        format!("Failed to deserialize session: {}", e),
                    )
                })?;
                Ok(Some(session))
            }
            Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
            Err(e) => Err(SignalProtocolError::InvalidState(
                "storage",
                format!("Database error: {}", e),
            )),
        }
    }

    async fn store_session(
        &mut self,
        address: &ProtocolAddress,
        record: &SessionRecord,
    ) -> Result<(), SignalProtocolError> {
        let conn = self.connection.lock().unwrap();
        let serialized = record.serialize().map_err(|e| {
            SignalProtocolError::InvalidState(
                "storage",
                format!("Failed to serialize session: {}", e),
            )
        })?;

        conn.execute(
            "INSERT OR REPLACE INTO sessions (address, device_id, session_data, updated_at)
             VALUES (?, ?, ?, strftime('%s', 'now'))",
            rusqlite::params![address.name(), u32::from(address.device_id()), &serialized],
        )
        .map_err(|e| {
            SignalProtocolError::InvalidState("storage", format!("Failed to store session: {}", e))
        })?;

        Ok(())
    }
}

#[async_trait(?Send)]
impl ExtendedSessionStore for SqliteSessionStore {
    async fn session_count(&self) -> usize {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn.prepare("SELECT COUNT(*) FROM sessions").unwrap();
        stmt.query_row([], |row| {
            let count: i64 = row.get(0)?;
            Ok(count as usize)
        })
        .unwrap_or(0)
    }

    async fn clear_all_sessions(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        conn.execute("DELETE FROM sessions", [])?;
        Ok(())
    }
    async fn delete_session(
        &mut self,
        address: &ProtocolAddress,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        conn.execute(
            "DELETE FROM sessions WHERE address = ?1 AND device_id = ?2",
            [address.name(), &u32::from(address.device_id()).to_string()],
        )?;
        Ok(())
    }
}

/// SQLite-backed identity key storage with data persistence
pub struct SqliteIdentityStore {
    connection: Arc<Mutex<Connection>>,
}

impl SqliteIdentityStore {
    pub fn new(connection: Arc<Mutex<Connection>>) -> Self {
        Self { connection }
    }

    pub fn create_tables(connection: &Connection) -> Result<(), Box<dyn std::error::Error>> {
        connection.execute(
            "CREATE TABLE IF NOT EXISTS local_identity (
                id INTEGER PRIMARY KEY CHECK (id = 1),
                registration_id INTEGER NOT NULL,
                private_key BLOB NOT NULL,
                public_key BLOB NOT NULL,
                created_at INTEGER DEFAULT (strftime('%s', 'now')),
                updated_at INTEGER DEFAULT (strftime('%s', 'now'))
            )",
            [],
        )?;

        connection.execute(
            "CREATE TABLE IF NOT EXISTS identity_keys (
                address TEXT NOT NULL,
                device_id INTEGER NOT NULL DEFAULT 1,
                public_key BLOB NOT NULL,
                trust_level INTEGER DEFAULT 0,
                first_seen INTEGER DEFAULT (strftime('%s', 'now')),
                last_seen INTEGER DEFAULT (strftime('%s', 'now')),
                verified_at INTEGER NULL,
                PRIMARY KEY (address, device_id)
            )",
            [],
        )?;

        Ok(())
    }
}

#[async_trait(?Send)]
impl IdentityKeyStore for SqliteIdentityStore {
    async fn get_identity_key_pair(&self) -> Result<IdentityKeyPair, SignalProtocolError> {
        let conn = self.connection.lock().unwrap();

        let mut stmt = conn
            .prepare("SELECT private_key FROM local_identity WHERE id = 1")
            .map_err(|e| {
                SignalProtocolError::InvalidState(
                    "storage",
                    format!("Failed to prepare statement: {}", e),
                )
            })?;

        let result = stmt.query_row([], |row| {
            let serialized_keypair: Vec<u8> = row.get(0)?;
            Ok(serialized_keypair)
        });

        match result {
            Ok(serialized_keypair) => {
                let identity_key_pair = IdentityKeyPair::try_from(&serialized_keypair[..])
                    .map_err(|e| {
                        SignalProtocolError::InvalidState(
                            "storage",
                            format!("Failed to deserialize identity key pair: {}", e),
                        )
                    })?;
                Ok(identity_key_pair)
            }
            Err(rusqlite::Error::QueryReturnedNoRows) => Err(SignalProtocolError::InvalidState(
                "storage",
                "Local identity key pair not set".to_string(),
            )),
            Err(e) => Err(SignalProtocolError::InvalidState(
                "storage",
                format!("Database error: {}", e),
            )),
        }
    }

    async fn get_local_registration_id(&self) -> Result<u32, SignalProtocolError> {
        let conn = self.connection.lock().unwrap();

        let mut stmt = conn
            .prepare("SELECT registration_id FROM local_identity WHERE id = 1")
            .map_err(|e| {
                SignalProtocolError::InvalidState(
                    "storage",
                    format!("Failed to prepare statement: {}", e),
                )
            })?;

        let result = stmt.query_row([], |row| {
            let registration_id: u32 = row.get(0)?;
            Ok(registration_id)
        });

        match result {
            Ok(registration_id) => Ok(registration_id),
            Err(rusqlite::Error::QueryReturnedNoRows) => Err(SignalProtocolError::InvalidState(
                "storage",
                "Local registration ID not set".to_string(),
            )),
            Err(e) => Err(SignalProtocolError::InvalidState(
                "storage",
                format!("Database error: {}", e),
            )),
        }
    }

    async fn save_identity(
        &mut self,
        address: &ProtocolAddress,
        identity_key: &IdentityKey,
    ) -> Result<IdentityChange, SignalProtocolError> {
        let conn = self.connection.lock().unwrap();
        let serialized_key = identity_key.serialize();

        let mut stmt = conn
            .prepare("SELECT public_key FROM identity_keys WHERE address = ? AND device_id = ?")
            .map_err(|e| {
                SignalProtocolError::InvalidState(
                    "storage",
                    format!("Failed to prepare statement: {}", e),
                )
            })?;

        let existing_key = stmt.query_row(
            [address.name(), &u32::from(address.device_id()).to_string()],
            |row| {
                let key_data: Vec<u8> = row.get(0)?;
                Ok(key_data)
            },
        );

        let change_type = match existing_key {
            Ok(existing_data) => {
                if existing_data == serialized_key.as_ref() {
                    IdentityChange::NewOrUnchanged
                } else {
                    IdentityChange::ReplacedExisting
                }
            }
            Err(rusqlite::Error::QueryReturnedNoRows) => IdentityChange::NewOrUnchanged,
            Err(e) => {
                return Err(SignalProtocolError::InvalidState(
                    "storage",
                    format!("Database error: {}", e),
                ))
            }
        };

        conn.execute(
            "INSERT OR REPLACE INTO identity_keys (address, device_id, public_key, last_seen)
             VALUES (?, ?, ?, strftime('%s', 'now'))",
            rusqlite::params![
                address.name(),
                u32::from(address.device_id()),
                &serialized_key.as_ref()
            ],
        )
        .map_err(|e| {
            SignalProtocolError::InvalidState("storage", format!("Failed to store identity: {}", e))
        })?;

        Ok(change_type)
    }

    async fn is_trusted_identity(
        &self,
        address: &ProtocolAddress,
        identity_key: &IdentityKey,
        _direction: Direction,
    ) -> Result<bool, SignalProtocolError> {
        let stored_identity = self.get_identity(address).await?;

        match stored_identity {
            None => Ok(true), // First time seeing this identity, trust it (TOFU - Trust On First Use)
            Some(stored_key) => Ok(stored_key == *identity_key), // Trust if it matches what we have stored
        }
    }

    async fn get_identity(
        &self,
        address: &ProtocolAddress,
    ) -> Result<Option<IdentityKey>, SignalProtocolError> {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn
            .prepare("SELECT public_key FROM identity_keys WHERE address = ? AND device_id = ?")
            .map_err(|e| {
                SignalProtocolError::InvalidState(
                    "storage",
                    format!("Failed to prepare statement: {}", e),
                )
            })?;

        let result = stmt.query_row(
            [address.name(), &u32::from(address.device_id()).to_string()],
            |row| {
                let key_data: Vec<u8> = row.get(0)?;
                Ok(key_data)
            },
        );

        match result {
            Ok(key_data) => {
                let identity_key = IdentityKey::try_from(&key_data[..]).map_err(|e| {
                    SignalProtocolError::InvalidState(
                        "storage",
                        format!("Failed to deserialize identity key: {}", e),
                    )
                })?;
                Ok(Some(identity_key))
            }
            Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
            Err(e) => Err(SignalProtocolError::InvalidState(
                "storage",
                format!("Database error: {}", e),
            )),
        }
    }
}

#[async_trait(?Send)]
impl ExtendedIdentityStore for SqliteIdentityStore {
    async fn identity_count(&self) -> usize {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn.prepare("SELECT COUNT(*) FROM identity_keys").unwrap();
        stmt.query_row([], |row| {
            let count: i64 = row.get(0)?;
            Ok(count as usize)
        })
        .unwrap_or(0)
    }

    async fn set_local_identity_key_pair(
        &self,
        identity_key_pair: &IdentityKeyPair,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();

        let serialized_keypair = identity_key_pair.serialize();
        let public_key_bytes = identity_key_pair.identity_key().serialize();

        conn.execute(
            "INSERT OR REPLACE INTO local_identity (id, private_key, public_key, registration_id, updated_at)
             VALUES (1, ?, ?, COALESCE((SELECT registration_id FROM local_identity WHERE id = 1), 0), strftime('%s', 'now'))",
            rusqlite::params![&serialized_keypair.as_ref(), &public_key_bytes.as_ref()],
        )?;

        Ok(())
    }

    async fn set_local_registration_id(
        &self,
        registration_id: u32,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();

        conn.execute(
            "INSERT OR REPLACE INTO local_identity (id, registration_id, private_key, public_key, updated_at)
             VALUES (1, ?, COALESCE((SELECT private_key FROM local_identity WHERE id = 1), X''), COALESCE((SELECT public_key FROM local_identity WHERE id = 1), X''), strftime('%s', 'now'))",
            rusqlite::params![registration_id],
        )?;

        Ok(())
    }
    async fn get_peer_identity(
        &self,
        address: &ProtocolAddress,
    ) -> Result<Option<IdentityKey>, Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn
            .prepare("SELECT public_key FROM identity_keys WHERE address = ? AND device_id = ?")?;
        match stmt.query_row(
            [address.name(), &u32::from(address.device_id()).to_string()],
            |row| {
                let public_key_bytes: Vec<u8> = row.get(0)?;
                Ok(public_key_bytes)
            },
        ) {
            Ok(key_bytes) => Ok(Some(IdentityKey::decode(&key_bytes)?)),
            Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
            Err(e) => Err(e.into()),
        }
    }
    async fn delete_identity(
        &mut self,
        address: &ProtocolAddress,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        conn.execute(
            "DELETE FROM identity_keys WHERE address = ? AND device_id = ?",
            [address.name(), &u32::from(address.device_id()).to_string()],
        )?;
        Ok(())
    }
    async fn clear_all_identities(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        conn.execute("DELETE FROM identity_keys", [])?;
        Ok(())
    }
    async fn clear_local_identity(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        conn.execute("DELETE FROM local_identity", [])?;
        Ok(())
    }
}

/// SQLite-backed pre-key storage with data persistence
pub struct SqlitePreKeyStore {
    connection: Arc<Mutex<Connection>>,
}

impl SqlitePreKeyStore {
    pub fn new(connection: Arc<Mutex<Connection>>) -> Self {
        Self { connection }
    }

    pub fn create_tables(conn: &Connection) -> Result<(), Box<dyn std::error::Error>> {
        conn.execute(
            "CREATE TABLE IF NOT EXISTS pre_keys (
                id INTEGER PRIMARY KEY,
                key_data BLOB NOT NULL,
                created_at INTEGER DEFAULT (strftime('%s', 'now')),
                used_at INTEGER NULL
            )",
            [],
        )?;
        Ok(())
    }
}

#[async_trait(?Send)]
impl PreKeyStore for SqlitePreKeyStore {
    async fn get_pre_key(&self, prekey_id: PreKeyId) -> Result<PreKeyRecord, SignalProtocolError> {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn
            .prepare("SELECT key_data FROM pre_keys WHERE id = ?")
            .map_err(|e| {
                SignalProtocolError::InvalidState(
                    "storage",
                    format!("Failed to prepare statement: {}", e),
                )
            })?;

        let result = stmt.query_row([u32::from(prekey_id)], |row| {
            let key_data: Vec<u8> = row.get(0)?;
            Ok(key_data)
        });

        match result {
            Ok(key_data) => {
                let prekey_record = PreKeyRecord::deserialize(&key_data).map_err(|e| {
                    SignalProtocolError::InvalidState(
                        "storage",
                        format!("Failed to deserialize pre key: {}", e),
                    )
                })?;
                Ok(prekey_record)
            }
            Err(rusqlite::Error::QueryReturnedNoRows) => Err(SignalProtocolError::InvalidPreKeyId),
            Err(e) => Err(SignalProtocolError::InvalidState(
                "storage",
                format!("Database error: {}", e),
            )),
        }
    }

    async fn save_pre_key(
        &mut self,
        prekey_id: PreKeyId,
        record: &PreKeyRecord,
    ) -> Result<(), SignalProtocolError> {
        let conn = self.connection.lock().unwrap();
        let serialized = record.serialize().map_err(|e| {
            SignalProtocolError::InvalidState(
                "storage",
                format!("Failed to serialize pre key: {}", e),
            )
        })?;

        conn.execute(
            "INSERT OR REPLACE INTO pre_keys (id, key_data, created_at, used_at)
             VALUES (?, ?, strftime('%s', 'now'), NULL)",
            rusqlite::params![u32::from(prekey_id), &serialized],
        )
        .map_err(|e| {
            SignalProtocolError::InvalidState("storage", format!("Failed to store pre key: {}", e))
        })?;

        Ok(())
    }

    async fn remove_pre_key(&mut self, prekey_id: PreKeyId) -> Result<(), SignalProtocolError> {
        let conn = self.connection.lock().unwrap();
        conn.execute(
            "DELETE FROM pre_keys WHERE id = ?",
            rusqlite::params![u32::from(prekey_id)],
        )
        .map_err(|e| {
            SignalProtocolError::InvalidState("storage", format!("Failed to remove pre key: {}", e))
        })?;
        Ok(())
    }
}

#[async_trait(?Send)]
impl ExtendedPreKeyStore for SqlitePreKeyStore {
    async fn pre_key_count(&self) -> usize {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn.prepare("SELECT COUNT(*) FROM pre_keys").unwrap();
        stmt.query_row([], |row| {
            let count: i64 = row.get(0)?;
            Ok(count as usize)
        })
        .unwrap_or(0)
    }

    async fn clear_all_pre_keys(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        conn.execute("DELETE FROM pre_keys", [])?;
        Ok(())
    }

    async fn get_max_pre_key_id(&self) -> Result<Option<u32>, Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn.prepare("SELECT MAX(id) FROM pre_keys")?;
        let max_id: Option<u32> = stmt.query_row([], |row| row.get(0)).ok();
        Ok(max_id)
    }

    async fn delete_pre_key(&mut self, id: PreKeyId) -> Result<(), Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        conn.execute("DELETE FROM pre_keys WHERE id = ?", [u32::from(id)])?;
        Ok(())
    }
}

/// SQLite-backed signed pre-key storage with data persistence
pub struct SqliteSignedPreKeyStore {
    connection: Arc<Mutex<Connection>>,
}

impl SqliteSignedPreKeyStore {
    pub fn new(connection: Arc<Mutex<Connection>>) -> Self {
        Self { connection }
    }

    pub fn create_tables(conn: &Connection) -> Result<(), Box<dyn std::error::Error>> {
        conn.execute(
            "CREATE TABLE IF NOT EXISTS signed_pre_keys (
                id INTEGER PRIMARY KEY,
                key_data BLOB NOT NULL,
                signature BLOB NOT NULL,
                created_at INTEGER DEFAULT (strftime('%s', 'now')),
                expires_at INTEGER NOT NULL,
                is_current BOOLEAN DEFAULT FALSE
            )",
            [],
        )?;
        Ok(())
    }
}

#[async_trait(?Send)]
impl SignedPreKeyStore for SqliteSignedPreKeyStore {
    async fn get_signed_pre_key(
        &self,
        signed_prekey_id: SignedPreKeyId,
    ) -> Result<SignedPreKeyRecord, SignalProtocolError> {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn
            .prepare("SELECT key_data FROM signed_pre_keys WHERE id = ?")
            .map_err(|e| {
                SignalProtocolError::InvalidState(
                    "storage",
                    format!("Failed to prepare statement: {}", e),
                )
            })?;

        let result = stmt.query_row([u32::from(signed_prekey_id)], |row| {
            let key_data: Vec<u8> = row.get(0)?;
            Ok(key_data)
        });

        match result {
            Ok(key_data) => {
                let signed_prekey_record =
                    SignedPreKeyRecord::deserialize(&key_data).map_err(|e| {
                        SignalProtocolError::InvalidState(
                            "storage",
                            format!("Failed to deserialize signed pre key: {}", e),
                        )
                    })?;
                Ok(signed_prekey_record)
            }
            Err(rusqlite::Error::QueryReturnedNoRows) => {
                Err(SignalProtocolError::InvalidSignedPreKeyId)
            }
            Err(e) => Err(SignalProtocolError::InvalidState(
                "storage",
                format!("Database error: {}", e),
            )),
        }
    }

    async fn save_signed_pre_key(
        &mut self,
        signed_prekey_id: SignedPreKeyId,
        record: &SignedPreKeyRecord,
    ) -> Result<(), SignalProtocolError> {
        let conn = self.connection.lock().unwrap();
        let serialized = record.serialize().map_err(|e| {
            SignalProtocolError::InvalidState(
                "storage",
                format!("Failed to serialize signed pre key: {}", e),
            )
        })?;

        let record_timestamp_secs = record
            .timestamp()
            .map_err(|e| {
                SignalProtocolError::InvalidState(
                    "storage",
                    format!("Failed to get record timestamp: {}", e),
                )
            })?
            .epoch_millis()
            / 1000;

        // Set expiration to 30 days from record creation (Signal Protocol recommendation)
        let expires_at = record_timestamp_secs + (30 * 24 * 60 * 60); // 30 days in seconds

        conn.execute(
            "INSERT OR REPLACE INTO signed_pre_keys (id, key_data, signature, created_at, expires_at, is_current)
             VALUES (?, ?, ?, ?, ?, FALSE)",
            rusqlite::params![u32::from(signed_prekey_id), &serialized, &record.signature().map_err(|e| SignalProtocolError::InvalidState("storage", format!("Failed to get signature: {}", e)))?, record_timestamp_secs as i64, expires_at],
        ).map_err(|e| SignalProtocolError::InvalidState("storage", format!("Failed to store signed pre key: {}", e)))?;

        Ok(())
    }
}

#[async_trait(?Send)]
impl ExtendedSignedPreKeyStore for SqliteSignedPreKeyStore {
    async fn signed_pre_key_count(&self) -> usize {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn
            .prepare("SELECT COUNT(*) FROM signed_pre_keys")
            .unwrap();
        stmt.query_row([], |row| {
            let count: i64 = row.get(0)?;
            Ok(count as usize)
        })
        .unwrap_or(0)
    }

    async fn clear_all_signed_pre_keys(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        conn.execute("DELETE FROM signed_pre_keys", [])?;
        Ok(())
    }

    async fn get_max_signed_pre_key_id(&self) -> Result<Option<u32>, Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn.prepare("SELECT MAX(id) FROM signed_pre_keys")?;
        let max_id: Option<u32> = stmt.query_row([], |row| row.get(0)).ok();
        Ok(max_id)
    }

    async fn delete_signed_pre_key(
        &mut self,
        id: SignedPreKeyId,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        conn.execute("DELETE FROM signed_pre_keys WHERE id = ?", [u32::from(id)])?;
        Ok(())
    }

    async fn get_signed_pre_keys_older_than(
        &self,
        timestamp_millis: u64,
    ) -> Result<Vec<SignedPreKeyId>, Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        let timestamp_secs = (timestamp_millis / 1000) as i64;
        let mut stmt = conn.prepare("SELECT id FROM signed_pre_keys WHERE created_at < ?")?;
        let ids = stmt
            .query_map([timestamp_secs], |row| {
                let id: u32 = row.get(0)?;
                Ok(SignedPreKeyId::from(id))
            })?
            .collect::<Result<Vec<_>, _>>()?;
        Ok(ids)
    }
}

/// SQLite-backed Kyber post-quantum pre-key storage with data persistence
pub struct SqliteKyberPreKeyStore {
    connection: Arc<Mutex<Connection>>,
}

impl SqliteKyberPreKeyStore {
    pub fn new(connection: Arc<Mutex<Connection>>) -> Self {
        Self { connection }
    }

    pub fn create_tables(conn: &Connection) -> Result<(), Box<dyn std::error::Error>> {
        conn.execute(
            "CREATE TABLE IF NOT EXISTS kyber_pre_keys (
                id INTEGER PRIMARY KEY,
                key_data BLOB NOT NULL,
                signature BLOB NOT NULL,
                created_at INTEGER DEFAULT (strftime('%s', 'now')),
                expires_at INTEGER NOT NULL,
                is_current BOOLEAN DEFAULT FALSE
            )",
            [],
        )?;
        Ok(())
    }
}

#[async_trait(?Send)]
impl KyberPreKeyStore for SqliteKyberPreKeyStore {
    async fn get_kyber_pre_key(
        &self,
        kyber_prekey_id: KyberPreKeyId,
    ) -> Result<KyberPreKeyRecord, SignalProtocolError> {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn
            .prepare("SELECT key_data FROM kyber_pre_keys WHERE id = ?")
            .map_err(|e| {
                SignalProtocolError::InvalidState(
                    "storage",
                    format!("Failed to prepare statement: {}", e),
                )
            })?;

        let result = stmt.query_row([u32::from(kyber_prekey_id)], |row| {
            let key_data: Vec<u8> = row.get(0)?;
            Ok(key_data)
        });

        match result {
            Ok(key_data) => {
                let kyber_prekey_record =
                    KyberPreKeyRecord::deserialize(&key_data).map_err(|e| {
                        SignalProtocolError::InvalidState(
                            "storage",
                            format!("Failed to deserialize kyber pre key: {}", e),
                        )
                    })?;
                Ok(kyber_prekey_record)
            }
            Err(rusqlite::Error::QueryReturnedNoRows) => {
                Err(SignalProtocolError::InvalidKyberPreKeyId)
            }
            Err(e) => Err(SignalProtocolError::InvalidState(
                "storage",
                format!("Database error: {}", e),
            )),
        }
    }

    async fn save_kyber_pre_key(
        &mut self,
        kyber_prekey_id: KyberPreKeyId,
        record: &KyberPreKeyRecord,
    ) -> Result<(), SignalProtocolError> {
        let conn = self.connection.lock().unwrap();
        let serialized = record.serialize().map_err(|e| {
            SignalProtocolError::InvalidState(
                "storage",
                format!("Failed to serialize kyber pre key: {}", e),
            )
        })?;

        // Use the record's timestamp for created_at
        let record_timestamp_secs = record
            .timestamp()
            .map_err(|e| {
                SignalProtocolError::InvalidState(
                    "storage",
                    format!("Failed to get timestamp: {}", e),
                )
            })?
            .epoch_millis()
            / 1000;

        // Set expiration to 30 days from record creation (Signal Protocol recommendation)
        let expires_at = record_timestamp_secs + (30 * 24 * 60 * 60); // 30 days in seconds

        conn.execute(
            "INSERT OR REPLACE INTO kyber_pre_keys (id, key_data, signature, created_at, expires_at, is_current)
             VALUES (?, ?, ?, ?, ?, FALSE)",
            rusqlite::params![u32::from(kyber_prekey_id), &serialized, &record.signature().map_err(|e| SignalProtocolError::InvalidState("storage", format!("Failed to get signature: {}", e)))?, record_timestamp_secs as i64, expires_at],
        ).map_err(|e| SignalProtocolError::InvalidState("storage", format!("Failed to store kyber pre key: {}", e)))?;

        Ok(())
    }

    async fn mark_kyber_pre_key_used(
        &mut self,
        kyber_prekey_id: KyberPreKeyId,
    ) -> Result<(), SignalProtocolError> {
        let conn = self.connection.lock().unwrap();
        conn.execute(
            "UPDATE kyber_pre_keys SET is_current = FALSE WHERE id = ?",
            rusqlite::params![u32::from(kyber_prekey_id)],
        )
        .map_err(|e| {
            SignalProtocolError::InvalidState(
                "storage",
                format!("Failed to mark kyber pre key as used: {}", e),
            )
        })?;
        Ok(())
    }
}

#[async_trait(?Send)]
impl ExtendedKyberPreKeyStore for SqliteKyberPreKeyStore {
    async fn kyber_pre_key_count(&self) -> usize {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn.prepare("SELECT COUNT(*) FROM kyber_pre_keys").unwrap();
        stmt.query_row([], |row| {
            let count: i64 = row.get(0)?;
            Ok(count as usize)
        })
        .unwrap_or(0)
    }

    async fn clear_all_kyber_pre_keys(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        conn.execute("DELETE FROM kyber_pre_keys", [])?;
        Ok(())
    }

    async fn get_max_kyber_pre_key_id(&self) -> Result<Option<u32>, Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        let mut stmt = conn.prepare("SELECT MAX(id) FROM kyber_pre_keys")?;
        let max_id: Option<u32> = stmt.query_row([], |row| row.get(0)).ok();
        Ok(max_id)
    }

    async fn delete_kyber_pre_key(
        &mut self,
        id: KyberPreKeyId,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        conn.execute("DELETE FROM kyber_pre_keys WHERE id = ?", [u32::from(id)])?;
        Ok(())
    }

    async fn get_kyber_pre_keys_older_than(
        &self,
        timestamp_millis: u64,
    ) -> Result<Vec<KyberPreKeyId>, Box<dyn std::error::Error>> {
        let conn = self.connection.lock().unwrap();
        let timestamp_secs = (timestamp_millis / 1000) as i64;
        let mut stmt = conn.prepare("SELECT id FROM kyber_pre_keys WHERE created_at < ?")?;
        let ids = stmt
            .query_map([timestamp_secs], |row| {
                let id: u32 = row.get(0)?;
                Ok(KyberPreKeyId::from(id))
            })?
            .collect::<Result<Vec<_>, _>>()?;
        Ok(ids)
    }
}

#[async_trait(?Send)]
impl ExtendedStorageOps for SqliteStorage {
    async fn establish_session_from_bundle(
        &mut self,
        address: &ProtocolAddress,
        bundle: &PreKeyBundle,
    ) -> Result<(), Box<dyn std::error::Error>> {
        let mut rng = rand::rng();
        let timestamp = std::time::SystemTime::now();

        process_prekey_bundle(
            address,
            self.session_store
                .as_mut()
                .expect("Storage not initialized"),
            self.identity_store
                .as_mut()
                .expect("Storage not initialized"),
            bundle,
            timestamp,
            &mut rng,
            UsePQRatchet::Yes,
        )
        .await?;

        Ok(())
    }

    async fn encrypt_message(
        &mut self,
        remote_address: &ProtocolAddress,
        plaintext: &[u8],
    ) -> Result<CiphertextMessage, SignalProtocolError> {
        let mut rng = rand::rng();
        let now = std::time::SystemTime::now();

        message_encrypt(
            plaintext,
            remote_address,
            self.session_store
                .as_mut()
                .expect("Storage not initialized"),
            self.identity_store
                .as_mut()
                .expect("Storage not initialized"),
            now,
            &mut rng,
        )
        .await
    }

    async fn decrypt_message(
        &mut self,
        remote_address: &ProtocolAddress,
        ciphertext: &CiphertextMessage,
    ) -> Result<Vec<u8>, SignalProtocolError> {
        let mut rng = rand::rng();

        message_decrypt(
            ciphertext,
            remote_address,
            self.session_store
                .as_mut()
                .expect("Storage not initialized"),
            self.identity_store
                .as_mut()
                .expect("Storage not initialized"),
            self.pre_key_store
                .as_mut()
                .expect("Storage not initialized"),
            self.signed_pre_key_store
                .as_mut()
                .expect("Storage not initialized"),
            self.kyber_pre_key_store
                .as_mut()
                .expect("Storage not initialized"),
            &mut rng,
            UsePQRatchet::Yes,
        )
        .await
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use libsignal_protocol::{kem, process_prekey_bundle, DeviceId, PreKeyBundle, Timestamp};

    #[tokio::test]
    async fn test_sqlite_storage_creation() -> Result<(), Box<dyn std::error::Error>> {
        let storage = SqliteStorage::new(":memory:").await?;
        assert_eq!(storage.storage_type(), "sqlite");
        Ok(())
    }

    #[tokio::test]
    async fn test_sqlite_storage_schema_initialization() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize_schema()?;

        let version = storage.get_schema_version()?;
        assert_eq!(version, 1);

        Ok(())
    }

    #[tokio::test]
    async fn test_sqlite_session_store_count_empty() -> Result<(), Box<dyn std::error::Error>> {
        let connection = Arc::new(Mutex::new(Connection::open(":memory:")?));
        SqliteSessionStore::create_tables(&connection.lock().unwrap())?;
        let session_store = SqliteSessionStore::new(connection);

        assert_eq!(session_store.session_count().await, 0);

        Ok(())
    }

    #[tokio::test]
    async fn test_sqlite_identity_store_count_empty() -> Result<(), Box<dyn std::error::Error>> {
        let connection = Arc::new(Mutex::new(Connection::open(":memory:")?));
        SqliteIdentityStore::create_tables(&connection.lock().unwrap())?;
        let identity_store = SqliteIdentityStore::new(connection);

        assert_eq!(identity_store.identity_count().await, 0);

        Ok(())
    }

    #[tokio::test]
    async fn test_sqlite_pre_key_store_count_empty() -> Result<(), Box<dyn std::error::Error>> {
        let connection = Arc::new(Mutex::new(Connection::open(":memory:")?));
        SqlitePreKeyStore::create_tables(&connection.lock().unwrap())?;
        let pre_key_store = SqlitePreKeyStore::new(connection);

        assert_eq!(pre_key_store.pre_key_count().await, 0);

        Ok(())
    }

    #[tokio::test]
    async fn test_sqlite_signed_pre_key_store_count_empty() -> Result<(), Box<dyn std::error::Error>>
    {
        let connection = Arc::new(Mutex::new(Connection::open(":memory:")?));
        SqliteSignedPreKeyStore::create_tables(&connection.lock().unwrap())?;
        let signed_pre_key_store = SqliteSignedPreKeyStore::new(connection);
        assert_eq!(signed_pre_key_store.signed_pre_key_count().await, 0);

        Ok(())
    }

    #[tokio::test]
    async fn test_sqlite_kyber_pre_key_store_count_empty() -> Result<(), Box<dyn std::error::Error>>
    {
        let connection = Arc::new(Mutex::new(Connection::open(":memory:")?));
        SqliteKyberPreKeyStore::create_tables(&connection.lock().unwrap())?;
        let kyber_pre_key_store = SqliteKyberPreKeyStore::new(connection);
        assert_eq!(kyber_pre_key_store.kyber_pre_key_count().await, 0);

        Ok(())
    }

    #[tokio::test]
    async fn test_sqlite_storage_container() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        assert_eq!(storage.storage_type(), "sqlite");
        assert_eq!(storage.session_store().session_count().await, 0);
        assert_eq!(storage.identity_store().identity_count().await, 0);

        Ok(())
    }

    #[tokio::test]
    async fn test_identity_key_store_trait() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize_schema()?;

        let mut rng = rand::rng();
        let identity_key_pair = IdentityKeyPair::generate(&mut rng);

        storage
            .identity_store()
            .set_local_identity_key_pair(&identity_key_pair)
            .await?;
        storage
            .identity_store()
            .set_local_registration_id(12345)
            .await?;

        let retrieved_registration = storage.identity_store().get_local_registration_id().await?;
        assert_eq!(retrieved_registration, 12345);

        let retrieved_identity = storage.identity_store().get_identity_key_pair().await?;
        assert_eq!(
            retrieved_identity.identity_key().serialize(),
            identity_key_pair.identity_key().serialize()
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_pre_key_store_trait() -> Result<(), Box<dyn std::error::Error>> {
        let connection = Arc::new(Mutex::new(Connection::open(":memory:")?));
        SqlitePreKeyStore::create_tables(&connection.lock().unwrap())?;
        let mut pre_key_store = SqlitePreKeyStore::new(connection);

        let mut rng = rand::rng();
        let key_pair = libsignal_protocol::KeyPair::generate(&mut rng);
        let prekey_id = PreKeyId::from(42u32);
        let prekey_record = PreKeyRecord::new(prekey_id, &key_pair);

        pre_key_store
            .save_pre_key(prekey_id, &prekey_record)
            .await?;
        let retrieved_prekey = pre_key_store.get_pre_key(prekey_id).await?;

        assert_eq!(retrieved_prekey.id()?, prekey_id);
        assert_eq!(
            retrieved_prekey.public_key()?.serialize(),
            prekey_record.public_key()?.serialize()
        );

        let non_existent_id = PreKeyId::from(999u32);
        let result = pre_key_store.get_pre_key(non_existent_id).await;
        assert!(result.is_err());

        Ok(())
    }

    #[tokio::test]
    async fn test_signed_pre_key_store_trait() -> Result<(), Box<dyn std::error::Error>> {
        let connection = Arc::new(Mutex::new(Connection::open(":memory:")?));
        SqliteSignedPreKeyStore::create_tables(&connection.lock().unwrap())?;
        let mut signed_pre_key_store = SqliteSignedPreKeyStore::new(connection);

        let mut rng = rand::rng();
        let identity_key_pair = IdentityKeyPair::generate(&mut rng);
        let key_pair = libsignal_protocol::KeyPair::generate(&mut rng);
        let signed_prekey_id = SignedPreKeyId::from(42u32);
        let timestamp = Timestamp::from_epoch_millis(
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)?
                .as_millis() as u64,
        );
        let signature = identity_key_pair
            .private_key()
            .calculate_signature(&key_pair.public_key.serialize(), &mut rng)?;
        let signed_prekey_record =
            SignedPreKeyRecord::new(signed_prekey_id, timestamp, &key_pair, &signature);

        signed_pre_key_store
            .save_signed_pre_key(signed_prekey_id, &signed_prekey_record)
            .await?;
        let retrieved_prekey = signed_pre_key_store
            .get_signed_pre_key(signed_prekey_id)
            .await?;

        assert_eq!(retrieved_prekey.id()?, signed_prekey_id);
        assert_eq!(
            retrieved_prekey.public_key()?.serialize(),
            signed_prekey_record.public_key()?.serialize()
        );

        let non_existent_id = SignedPreKeyId::from(999u32);
        let result = signed_pre_key_store
            .get_signed_pre_key(non_existent_id)
            .await;
        assert!(result.is_err());

        Ok(())
    }

    #[tokio::test]
    async fn test_identity_store_remote_identities() -> Result<(), Box<dyn std::error::Error>> {
        let connection = Arc::new(Mutex::new(Connection::open(":memory:")?));
        SqliteIdentityStore::create_tables(&connection.lock().unwrap())?;
        let mut identity_store = SqliteIdentityStore::new(connection);

        let mut rng = rand::rng();
        let alice_address =
            ProtocolAddress::new("alice@example.com".to_string(), DeviceId::new(1)?);
        let bob_address = ProtocolAddress::new("bob@example.com".to_string(), DeviceId::new(1)?);
        let alice_identity =
            IdentityKey::new(libsignal_protocol::KeyPair::generate(&mut rng).public_key);

        let result = identity_store
            .save_identity(&alice_address, &alice_identity)
            .await?;
        assert_eq!(result, IdentityChange::NewOrUnchanged);

        let retrieved_identity = identity_store.get_identity(&alice_address).await?;
        assert_eq!(retrieved_identity, Some(alice_identity));

        assert!(
            identity_store
                .is_trusted_identity(&alice_address, &alice_identity, Direction::Receiving)
                .await?
        );
        assert!(
            identity_store
                .is_trusted_identity(&alice_address, &alice_identity, Direction::Sending)
                .await?
        );

        let alice_new_identity =
            IdentityKey::new(libsignal_protocol::KeyPair::generate(&mut rng).public_key);
        let result = identity_store
            .save_identity(&alice_address, &alice_new_identity)
            .await?;
        assert_eq!(result, IdentityChange::ReplacedExisting);

        let unknown_identity = identity_store.get_identity(&bob_address).await?;
        assert_eq!(unknown_identity, None);

        Ok(())
    }

    #[tokio::test]
    async fn test_sqlite_storage_close() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize_schema()?;

        let mut rng = rand::rng();
        let identity_key_pair = IdentityKeyPair::generate(&mut rng);
        storage
            .identity_store()
            .set_local_identity_key_pair(&identity_key_pair)
            .await?;

        storage.close()?;

        storage.close()?; // Should not error on second close

        assert!(storage.is_closed(), "Storage should be marked as closed");

        Ok(())
    }

    #[tokio::test]
    async fn test_kyber_pre_key_store_trait() -> Result<(), Box<dyn std::error::Error>> {
        let connection = Arc::new(Mutex::new(Connection::open(":memory:")?));
        SqliteKyberPreKeyStore::create_tables(&connection.lock().unwrap())?;
        let mut kyber_pre_key_store = SqliteKyberPreKeyStore::new(connection);

        let mut rng = rand::rng();
        let key_pair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
        let kyber_prekey_id = KyberPreKeyId::from(42u32);
        let timestamp = Timestamp::from_epoch_millis(
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)?
                .as_millis() as u64,
        );
        let signature = b"test_signature";
        let kyber_prekey_record =
            KyberPreKeyRecord::new(kyber_prekey_id, timestamp, &key_pair, signature);

        kyber_pre_key_store
            .save_kyber_pre_key(kyber_prekey_id, &kyber_prekey_record)
            .await?;
        let retrieved_prekey = kyber_pre_key_store
            .get_kyber_pre_key(kyber_prekey_id)
            .await?;

        assert_eq!(retrieved_prekey.id()?, kyber_prekey_id);
        assert_eq!(
            retrieved_prekey.public_key()?.serialize(),
            kyber_prekey_record.public_key()?.serialize()
        );

        kyber_pre_key_store
            .mark_kyber_pre_key_used(kyber_prekey_id)
            .await?;

        let non_existent_id = KyberPreKeyId::from(999u32);
        let result = kyber_pre_key_store.get_kyber_pre_key(non_existent_id).await;
        assert!(result.is_err());

        Ok(())
    }

    struct MockIdentityStore {
        identity_key_pair: IdentityKeyPair,
        registration_id: u32,
    }

    impl MockIdentityStore {
        fn new(identity_key_pair: IdentityKeyPair) -> Self {
            Self {
                identity_key_pair,
                registration_id: 12345,
            }
        }
    }

    #[async_trait(?Send)]
    impl IdentityKeyStore for MockIdentityStore {
        async fn get_identity_key_pair(&self) -> Result<IdentityKeyPair, SignalProtocolError> {
            Ok(self.identity_key_pair)
        }

        async fn get_local_registration_id(&self) -> Result<u32, SignalProtocolError> {
            Ok(self.registration_id)
        }

        async fn save_identity(
            &mut self,
            _address: &ProtocolAddress,
            _identity: &IdentityKey,
        ) -> Result<IdentityChange, SignalProtocolError> {
            Ok(IdentityChange::NewOrUnchanged)
        }

        async fn is_trusted_identity(
            &self,
            _address: &ProtocolAddress,
            _identity: &IdentityKey,
            _direction: Direction,
        ) -> Result<bool, SignalProtocolError> {
            Ok(true)
        }

        async fn get_identity(
            &self,
            _address: &ProtocolAddress,
        ) -> Result<Option<IdentityKey>, SignalProtocolError> {
            Ok(None)
        }
    }

    #[tokio::test]
    async fn test_session_store_persistence() -> Result<(), Box<dyn std::error::Error>> {
        let connection = Arc::new(Mutex::new(Connection::open(":memory:")?));
        SqliteSessionStore::create_tables(&connection.lock().unwrap())?;
        let mut session_store = SqliteSessionStore::new(connection);

        let address = ProtocolAddress::new("test_user".to_string(), DeviceId::new(1)?);

        let initial_result = session_store.load_session(&address).await?;
        assert!(
            initial_result.is_none(),
            "No session should exist initially"
        );

        // NOTE: Using process_prekey_bundle to create SessionRecord for testing since SessionRecord constructors aren't public
        // This is the intended way to create sessions in libsignal and tests the actual production code path
        let mut rng = rand::rng();
        let alice_identity = IdentityKeyPair::generate(&mut rng);
        let bob_identity = IdentityKeyPair::generate(&mut rng);

        let bob_pre_keys = crate::keys::generate_pre_keys(1, 1).await?;
        let bob_signed_pre_key = crate::keys::generate_signed_pre_key(&bob_identity, 1).await?;
        let kyber_keypair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
        let kyber_signature = bob_identity
            .private_key()
            .calculate_signature(&kyber_keypair.public_key.serialize(), &mut rng)?;

        let bundle = PreKeyBundle::new(
            12345,
            address.device_id(),
            Some((bob_pre_keys[0].0.into(), bob_pre_keys[0].1.public_key)),
            bob_signed_pre_key.id()?,
            bob_signed_pre_key.public_key()?,
            bob_signed_pre_key.signature()?.to_vec(),
            KyberPreKeyId::from(1u32),
            kyber_keypair.public_key,
            kyber_signature.to_vec(),
            *bob_identity.identity_key(),
        )?;

        use libsignal_protocol::UsePQRatchet;
        use std::time::SystemTime;

        process_prekey_bundle(
            &address,
            &mut session_store,
            &mut MockIdentityStore::new(alice_identity),
            &bundle,
            SystemTime::now(),
            &mut rng,
            UsePQRatchet::Yes,
        )
        .await?;

        let loaded_session = session_store.load_session(&address).await?;
        assert!(
            loaded_session.is_some(),
            "Session should exist after process_prekey_bundle"
        );

        let loaded_session = loaded_session.unwrap();
        let _serialized = loaded_session.serialize()?;

        Ok(())
    }

    #[tokio::test]
    async fn test_pre_key_store_removal() -> Result<(), Box<dyn std::error::Error>> {
        let connection = Arc::new(Mutex::new(Connection::open(":memory:")?));
        SqlitePreKeyStore::create_tables(&connection.lock().unwrap())?;
        let mut pre_key_store = SqlitePreKeyStore::new(connection);

        let mut rng = rand::rng();
        let key_pair = libsignal_protocol::KeyPair::generate(&mut rng);
        let prekey_id = PreKeyId::from(42u32);
        let prekey_record = PreKeyRecord::new(prekey_id, &key_pair);

        pre_key_store
            .save_pre_key(prekey_id, &prekey_record)
            .await?;

        let retrieved_prekey = pre_key_store.get_pre_key(prekey_id).await?;
        assert_eq!(retrieved_prekey.id()?, prekey_id);

        pre_key_store.remove_pre_key(prekey_id).await?;

        let result = pre_key_store.get_pre_key(prekey_id).await;
        assert!(
            result.is_err(),
            "PreKey should be removed and no longer retrievable"
        );

        pre_key_store.remove_pre_key(prekey_id).await?;

        Ok(())
    }

    #[tokio::test]
    async fn test_error_handling_nonexistent_keys() -> Result<(), Box<dyn std::error::Error>> {
        let connection = Arc::new(Mutex::new(Connection::open(":memory:")?));
        SqlitePreKeyStore::create_tables(&connection.lock().unwrap())?;
        SqliteSignedPreKeyStore::create_tables(&connection.lock().unwrap())?;
        SqliteKyberPreKeyStore::create_tables(&connection.lock().unwrap())?;
        SqliteSessionStore::create_tables(&connection.lock().unwrap())?;
        SqliteIdentityStore::create_tables(&connection.lock().unwrap())?;

        let pre_key_store = SqlitePreKeyStore::new(connection.clone());
        let signed_pre_key_store = SqliteSignedPreKeyStore::new(connection.clone());
        let kyber_pre_key_store = SqliteKyberPreKeyStore::new(connection.clone());
        let session_store = SqliteSessionStore::new(connection.clone());
        let identity_store = SqliteIdentityStore::new(connection);

        let result = pre_key_store.get_pre_key(PreKeyId::from(999u32)).await;
        assert!(result.is_err(), "Getting nonexistent PreKey should fail");

        let result = signed_pre_key_store
            .get_signed_pre_key(SignedPreKeyId::from(999u32))
            .await;
        assert!(
            result.is_err(),
            "Getting nonexistent SignedPreKey should fail"
        );

        let result = kyber_pre_key_store
            .get_kyber_pre_key(KyberPreKeyId::from(999u32))
            .await;
        assert!(
            result.is_err(),
            "Getting nonexistent KyberPreKey should fail"
        );

        let address = ProtocolAddress::new("nonexistent".to_string(), DeviceId::new(1)?);
        let result = session_store.load_session(&address).await?;
        assert!(
            result.is_none(),
            "Loading nonexistent session should return None"
        );

        let result = identity_store.get_identity(&address).await?;
        assert!(
            result.is_none(),
            "Getting nonexistent remote identity should return None"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_error_handling_database_constraints() -> Result<(), Box<dyn std::error::Error>> {
        let connection = Arc::new(Mutex::new(Connection::open(":memory:")?));
        SqlitePreKeyStore::create_tables(&connection.lock().unwrap())?;
        let mut pre_key_store = SqlitePreKeyStore::new(connection);

        let mut rng = rand::rng();
        let key_pair1 = libsignal_protocol::KeyPair::generate(&mut rng);
        let key_pair2 = libsignal_protocol::KeyPair::generate(&mut rng);
        let prekey_id = PreKeyId::from(42u32);
        let prekey_record1 = PreKeyRecord::new(prekey_id, &key_pair1);
        let prekey_record2 = PreKeyRecord::new(prekey_id, &key_pair2);

        pre_key_store
            .save_pre_key(prekey_id, &prekey_record1)
            .await?;

        pre_key_store
            .save_pre_key(prekey_id, &prekey_record2)
            .await?;

        let retrieved = pre_key_store.get_pre_key(prekey_id).await?;
        assert_eq!(
            retrieved.public_key()?.serialize(),
            prekey_record2.public_key()?.serialize()
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_identity_change_detection() -> Result<(), Box<dyn std::error::Error>> {
        let connection = Arc::new(Mutex::new(Connection::open(":memory:")?));
        SqliteIdentityStore::create_tables(&connection.lock().unwrap())?;
        let mut identity_store = SqliteIdentityStore::new(connection);

        let mut rng = rand::rng();
        let identity1 = IdentityKeyPair::generate(&mut rng);
        let identity2 = IdentityKeyPair::generate(&mut rng);
        let address = ProtocolAddress::new("test_user".to_string(), DeviceId::new(1)?);

        let result = identity_store
            .save_identity(&address, identity1.identity_key())
            .await?;
        assert_eq!(
            result,
            IdentityChange::NewOrUnchanged,
            "First save should be NewOrUnchanged"
        );

        let result = identity_store
            .save_identity(&address, identity1.identity_key())
            .await?;
        assert_eq!(
            result,
            IdentityChange::NewOrUnchanged,
            "Same identity should be NewOrUnchanged"
        );

        let result = identity_store
            .save_identity(&address, identity2.identity_key())
            .await?;
        assert_eq!(
            result,
            IdentityChange::ReplacedExisting,
            "Different identity should be ReplacedExisting"
        );

        let retrieved = identity_store.get_identity(&address).await?;
        assert!(retrieved.is_some(), "Identity should be retrievable");
        assert_eq!(
            retrieved.unwrap().serialize(),
            identity2.identity_key().serialize(),
            "New identity should be stored"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_extended_storage_ops_session_establishment(
    ) -> Result<(), Box<dyn std::error::Error>> {
        use crate::keys::{generate_identity_key_pair, generate_pre_keys, generate_signed_pre_key};
        use libsignal_protocol::*;

        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        let mut rng = rand::rng();
        let identity_key_pair = generate_identity_key_pair().await?;
        let registration_id = 12345u32;

        storage
            .identity_store
            .as_mut()
            .unwrap()
            .set_local_identity_key_pair(&identity_key_pair)
            .await?;
        storage
            .identity_store
            .as_mut()
            .unwrap()
            .set_local_registration_id(registration_id)
            .await?;

        let bob_address = ProtocolAddress::new("bob".to_string(), DeviceId::new(1)?);

        let bob_identity = generate_identity_key_pair().await?;
        let bob_pre_keys = generate_pre_keys(1, 1).await?;
        let bob_signed_pre_key = generate_signed_pre_key(&bob_identity, 1).await?;

        let kyber_keypair = libsignal_protocol::kem::KeyPair::generate(
            libsignal_protocol::kem::KeyType::Kyber1024,
            &mut rng,
        );
        let kyber_signature = bob_identity
            .private_key()
            .calculate_signature(&kyber_keypair.public_key.serialize(), &mut rng)?;

        let bob_bundle = PreKeyBundle::new(
            registration_id,
            DeviceId::new(1)?,
            Some((
                PreKeyId::from(bob_pre_keys[0].0),
                bob_pre_keys[0].1.public_key,
            )),
            SignedPreKeyId::from(1u32),
            bob_signed_pre_key.public_key()?,
            bob_signed_pre_key.signature()?.to_vec(),
            KyberPreKeyId::from(1u32),
            kyber_keypair.public_key,
            kyber_signature.to_vec(),
            *bob_identity.identity_key(),
        )?;

        let session_count_before = storage.session_store().session_count().await;
        assert_eq!(session_count_before, 0, "Should start with no sessions");

        storage
            .establish_session_from_bundle(&bob_address, &bob_bundle)
            .await?;

        let session_count_after = storage.session_store().session_count().await;
        assert_eq!(
            session_count_after, 1,
            "Should have one session after establishment"
        );

        let has_session = storage
            .session_store()
            .load_session(&bob_address)
            .await?
            .is_some();
        assert!(has_session, "Session should exist for Bob");

        Ok(())
    }

    #[tokio::test]
    async fn test_extended_storage_ops_encrypt_decrypt_message(
    ) -> Result<(), Box<dyn std::error::Error>> {
        use crate::keys::{generate_identity_key_pair, generate_pre_keys, generate_signed_pre_key};
        use libsignal_protocol::*;

        let mut alice_storage = SqliteStorage::new(":memory:").await?;
        alice_storage.initialize()?;
        let mut bob_storage = SqliteStorage::new(":memory:").await?;
        bob_storage.initialize()?;

        let mut rng = rand::rng();
        let alice_identity = generate_identity_key_pair().await?;
        let alice_registration_id = 11111u32;
        alice_storage
            .identity_store
            .as_mut()
            .unwrap()
            .set_local_identity_key_pair(&alice_identity)
            .await?;
        alice_storage
            .identity_store
            .as_mut()
            .unwrap()
            .set_local_registration_id(alice_registration_id)
            .await?;

        let bob_identity = generate_identity_key_pair().await?;
        let bob_registration_id = 22222u32;
        bob_storage
            .identity_store
            .as_mut()
            .unwrap()
            .set_local_identity_key_pair(&bob_identity)
            .await?;
        bob_storage
            .identity_store
            .as_mut()
            .unwrap()
            .set_local_registration_id(bob_registration_id)
            .await?;

        let alice_address = ProtocolAddress::new("alice".to_string(), DeviceId::new(1)?);
        let bob_address = ProtocolAddress::new("bob".to_string(), DeviceId::new(1)?);

        let bob_pre_keys = generate_pre_keys(1, 1).await?;
        let bob_signed_pre_key = generate_signed_pre_key(&bob_identity, 1).await?;
        bob_storage
            .pre_key_store
            .as_mut()
            .unwrap()
            .save_pre_key(
                PreKeyId::from(bob_pre_keys[0].0),
                &PreKeyRecord::new(PreKeyId::from(bob_pre_keys[0].0), &bob_pre_keys[0].1),
            )
            .await?;
        bob_storage
            .signed_pre_key_store
            .as_mut()
            .unwrap()
            .save_signed_pre_key(SignedPreKeyId::from(1u32), &bob_signed_pre_key)
            .await?;

        let kyber_keypair = libsignal_protocol::kem::KeyPair::generate(
            libsignal_protocol::kem::KeyType::Kyber1024,
            &mut rng,
        );
        let kyber_signature = bob_identity
            .private_key()
            .calculate_signature(&kyber_keypair.public_key.serialize(), &mut rng)?;

        let now = std::time::SystemTime::now();
        let kyber_record = KyberPreKeyRecord::new(
            KyberPreKeyId::from(1u32),
            Timestamp::from_epoch_millis(
                now.duration_since(std::time::UNIX_EPOCH)?.as_millis() as u64
            ),
            &kyber_keypair,
            &kyber_signature,
        );
        bob_storage
            .kyber_pre_key_store
            .as_mut()
            .unwrap()
            .save_kyber_pre_key(KyberPreKeyId::from(1u32), &kyber_record)
            .await?;

        let bob_bundle = PreKeyBundle::new(
            bob_registration_id,
            DeviceId::new(1)?,
            Some((
                PreKeyId::from(bob_pre_keys[0].0),
                bob_pre_keys[0].1.public_key,
            )),
            SignedPreKeyId::from(1u32),
            bob_signed_pre_key.public_key()?,
            bob_signed_pre_key.signature()?.to_vec(),
            KyberPreKeyId::from(1u32),
            kyber_keypair.public_key,
            kyber_signature.to_vec(),
            *bob_identity.identity_key(),
        )?;

        alice_storage
            .establish_session_from_bundle(&bob_address, &bob_bundle)
            .await?;

        let plaintext = b"Hello, Bob! This is a test message.";
        let ciphertext = alice_storage
            .encrypt_message(&bob_address, plaintext)
            .await?;

        assert!(
            matches!(ciphertext.message_type(), CiphertextMessageType::PreKey),
            "First message should be a PreKey message"
        );

        bob_storage
            .identity_store
            .as_mut()
            .unwrap()
            .save_identity(&alice_address, alice_identity.identity_key())
            .await?;
        let decrypted = bob_storage
            .decrypt_message(&alice_address, &ciphertext)
            .await?;

        assert_eq!(
            decrypted, plaintext,
            "Decrypted message should match original"
        );

        let response_plaintext = b"Hello, Alice! Got your message.";
        let response_ciphertext = bob_storage
            .encrypt_message(&alice_address, response_plaintext)
            .await?;

        assert!(
            matches!(
                response_ciphertext.message_type(),
                CiphertextMessageType::Whisper
            ),
            "Response message should be a Whisper message"
        );

        let response_decrypted = alice_storage
            .decrypt_message(&bob_address, &response_ciphertext)
            .await?;
        assert_eq!(
            response_decrypted, response_plaintext,
            "Response should decrypt correctly"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_extended_storage_ops_error_handling() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        let invalid_address = ProtocolAddress::new("nonexistent".to_string(), DeviceId::new(1)?);

        let plaintext = b"test message";
        let encrypt_result = storage.encrypt_message(&invalid_address, plaintext).await;
        assert!(
            encrypt_result.is_err(),
            "Should fail to encrypt without session"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_sqlite_extended_storage_ops_integration() -> Result<(), Box<dyn std::error::Error>>
    {
        use libsignal_protocol::*;

        use crate::keys::{generate_identity_key_pair, generate_pre_keys, generate_signed_pre_key};
        use crate::sqlite_storage::SqliteStorage;
        use crate::storage_trait::{
            ExtendedIdentityStore, ExtendedSessionStore, ExtendedStorageOps, SignalStorageContainer,
        };
        use std::fs;

        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let temp_dir = std::env::temp_dir();
        let db_path = temp_dir.join(format!("test_sqlite_alice_{}_{}.db", process_id, timestamp));
        let db_path_str = db_path.to_str().unwrap();

        if db_path.exists() {
            let _ = fs::remove_file(&db_path);
        }

        let mut alice_storage = SqliteStorage::new(db_path_str).await?;
        alice_storage.initialize()?;

        let db_path2 = temp_dir.join(format!("test_sqlite_bob_{}_{}.db", process_id, timestamp));
        let db_path2_str = db_path2.to_str().unwrap();

        if db_path2.exists() {
            let _ = fs::remove_file(&db_path2);
        }

        let mut bob_storage = SqliteStorage::new(db_path2_str).await?;
        bob_storage.initialize()?;

        let mut rng = rand::rng();
        let alice_identity = generate_identity_key_pair().await?;
        let alice_registration_id = 11111u32;
        alice_storage
            .identity_store()
            .set_local_identity_key_pair(&alice_identity)
            .await?;
        alice_storage
            .identity_store()
            .set_local_registration_id(alice_registration_id)
            .await?;

        let bob_identity = generate_identity_key_pair().await?;
        let bob_registration_id = 22222u32;
        bob_storage
            .identity_store()
            .set_local_identity_key_pair(&bob_identity)
            .await?;
        bob_storage
            .identity_store()
            .set_local_registration_id(bob_registration_id)
            .await?;

        let alice_address = ProtocolAddress::new("alice".to_string(), DeviceId::new(1)?);
        let bob_address = ProtocolAddress::new("bob".to_string(), DeviceId::new(1)?);

        let bob_pre_keys = generate_pre_keys(1, 1).await?;
        let bob_signed_pre_key = generate_signed_pre_key(&bob_identity, 1).await?;
        bob_storage
            .pre_key_store()
            .save_pre_key(
                PreKeyId::from(bob_pre_keys[0].0),
                &PreKeyRecord::new(PreKeyId::from(bob_pre_keys[0].0), &bob_pre_keys[0].1),
            )
            .await?;
        bob_storage
            .signed_pre_key_store()
            .save_signed_pre_key(SignedPreKeyId::from(1u32), &bob_signed_pre_key)
            .await?;

        let kyber_keypair = libsignal_protocol::kem::KeyPair::generate(
            libsignal_protocol::kem::KeyType::Kyber1024,
            &mut rng,
        );
        let kyber_signature = bob_identity
            .private_key()
            .calculate_signature(&kyber_keypair.public_key.serialize(), &mut rng)?;

        let now = std::time::SystemTime::now();
        let kyber_record = KyberPreKeyRecord::new(
            KyberPreKeyId::from(1u32),
            Timestamp::from_epoch_millis(
                now.duration_since(std::time::UNIX_EPOCH)?.as_millis() as u64
            ),
            &kyber_keypair,
            &kyber_signature,
        );
        bob_storage
            .kyber_pre_key_store()
            .save_kyber_pre_key(KyberPreKeyId::from(1u32), &kyber_record)
            .await?;

        let bob_bundle = PreKeyBundle::new(
            bob_registration_id,
            DeviceId::new(1)?,
            Some((
                PreKeyId::from(bob_pre_keys[0].0),
                bob_pre_keys[0].1.public_key,
            )),
            SignedPreKeyId::from(1u32),
            bob_signed_pre_key.public_key()?,
            bob_signed_pre_key.signature()?.to_vec(),
            KyberPreKeyId::from(1u32),
            kyber_keypair.public_key,
            kyber_signature.to_vec(),
            *bob_identity.identity_key(),
        )?;

        alice_storage
            .establish_session_from_bundle(&bob_address, &bob_bundle)
            .await?;

        assert_eq!(
            alice_storage.session_store().session_count().await,
            1,
            "Alice should have established session with Bob"
        );

        let plaintext = b"Hello Bob! This is an integration test with real SQLite files.";
        let ciphertext = alice_storage
            .encrypt_message(&bob_address, plaintext)
            .await?;

        assert!(
            matches!(ciphertext.message_type(), CiphertextMessageType::PreKey),
            "First message should be a PreKey message"
        );

        bob_storage
            .identity_store()
            .save_identity(&alice_address, alice_identity.identity_key())
            .await?;
        let decrypted = bob_storage
            .decrypt_message(&alice_address, &ciphertext)
            .await?;

        assert_eq!(
            decrypted, plaintext,
            "Decrypted message should match original"
        );

        let response_plaintext =
            b"Hello Alice! Integration test successful with persistent SQLite storage!";
        let response_ciphertext = bob_storage
            .encrypt_message(&alice_address, response_plaintext)
            .await?;

        assert!(
            matches!(
                response_ciphertext.message_type(),
                CiphertextMessageType::Whisper
            ),
            "Response message should be a Whisper message"
        );

        let response_decrypted = alice_storage
            .decrypt_message(&bob_address, &response_ciphertext)
            .await?;
        assert_eq!(
            response_decrypted, response_plaintext,
            "Response should decrypt correctly"
        );

        alice_storage.close()?;
        bob_storage.close()?;

        assert!(
            db_path.exists(),
            "SQLite database file should exist after test"
        );
        assert!(
            db_path2.exists(),
            "Bob's SQLite database file should exist after test"
        );

        drop(alice_storage);
        drop(bob_storage);

        #[cfg(windows)]
        std::thread::sleep(std::time::Duration::from_millis(100));

        let _ = fs::remove_file(&db_path);
        let _ = fs::remove_file(&db_path2);

        Ok(())
    }

    #[tokio::test]
    async fn test_sqlite_storage_preserves_identity_across_instantiations(
    ) -> Result<(), Box<dyn std::error::Error>> {
        use crate::keys::generate_identity_key_pair;
        use std::{env, fs};

        let temp_dir = env::temp_dir();
        let process_id = std::process::id();
        let timestamp = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_millis();

        let db_path = temp_dir.join(format!(
            "test_identity_persistence_{}_{}.db",
            process_id, timestamp
        ));
        let db_path_str = db_path.to_str().unwrap();

        let (original_identity, original_registration_id) = {
            let mut storage = SqliteStorage::new(db_path_str).await?;
            storage.initialize()?;

            let identity = generate_identity_key_pair().await?;
            let registration_id = 12345u32;

            storage
                .identity_store()
                .set_local_identity_key_pair(&identity)
                .await?;
            storage
                .identity_store()
                .set_local_registration_id(registration_id)
                .await?;

            (identity, registration_id)
        }; // Storage goes out of scope here, simulating program restart

        {
            let mut storage_reopened = SqliteStorage::new(db_path_str).await?;
            storage_reopened.initialize()?;

            let retrieved_identity = storage_reopened
                .identity_store()
                .get_identity_key_pair()
                .await?;
            let retrieved_registration_id = storage_reopened
                .identity_store()
                .get_local_registration_id()
                .await?;

            assert_eq!(
                retrieved_identity.identity_key().serialize(),
                original_identity.identity_key().serialize(),
                "Identity key should be preserved across storage instantiations"
            );
            assert_eq!(
                retrieved_registration_id, original_registration_id,
                "Registration ID should be preserved across storage instantiations"
            );
        }

        let _ = fs::remove_file(&db_path);

        Ok(())
    }

    #[tokio::test]
    async fn test_session_delete_operations() -> Result<(), Box<dyn std::error::Error>> {
        use crate::keys::{generate_identity_key_pair, generate_pre_keys, generate_signed_pre_key};
        use libsignal_protocol::*;

        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        let identity = generate_identity_key_pair().await?;
        let registration_id = 12345u32;
        storage
            .identity_store()
            .set_local_identity_key_pair(&identity)
            .await?;
        storage
            .identity_store()
            .set_local_registration_id(registration_id)
            .await?;

        let bob_address = ProtocolAddress::new("bob".to_string(), DeviceId::new(1)?);
        let charlie_address = ProtocolAddress::new("charlie".to_string(), DeviceId::new(1)?);

        let bob_identity = generate_identity_key_pair().await?;
        let bob_pre_keys = generate_pre_keys(1, 1).await?;
        let bob_signed_pre_key = generate_signed_pre_key(&bob_identity, 1).await?;

        let mut rng = rand::rng();
        let kyber_keypair = kem::KeyPair::generate(kem::KeyType::Kyber1024, &mut rng);
        let kyber_signature = bob_identity
            .private_key()
            .calculate_signature(&kyber_keypair.public_key.serialize(), &mut rng)?;

        let bob_bundle = PreKeyBundle::new(
            registration_id,
            DeviceId::new(1)?,
            Some((
                PreKeyId::from(bob_pre_keys[0].0),
                bob_pre_keys[0].1.public_key,
            )),
            SignedPreKeyId::from(1u32),
            bob_signed_pre_key.public_key()?,
            bob_signed_pre_key.signature()?.to_vec(),
            KyberPreKeyId::from(1u32),
            kyber_keypair.public_key,
            kyber_signature.to_vec(),
            *bob_identity.identity_key(),
        )?;

        storage
            .establish_session_from_bundle(&bob_address, &bob_bundle)
            .await?;
        storage
            .establish_session_from_bundle(&charlie_address, &bob_bundle)
            .await?;

        assert_eq!(
            storage.session_store().session_count().await,
            2,
            "Should have 2 sessions"
        );

        storage.session_store().delete_session(&bob_address).await?;
        assert_eq!(
            storage.session_store().session_count().await,
            1,
            "Should have 1 session after deleting Bob's"
        );

        let bob_session = storage.session_store().load_session(&bob_address).await?;
        assert!(bob_session.is_none(), "Bob's session should be deleted");

        let charlie_session = storage
            .session_store()
            .load_session(&charlie_address)
            .await?;
        assert!(
            charlie_session.is_some(),
            "Charlie's session should still exist"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_identity_management_operations() -> Result<(), Box<dyn std::error::Error>> {
        use crate::keys::generate_identity_key_pair;
        use libsignal_protocol::*;

        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        let local_identity = generate_identity_key_pair().await?;
        storage
            .identity_store()
            .set_local_identity_key_pair(&local_identity)
            .await?;

        let bob_address = ProtocolAddress::new("bob".to_string(), DeviceId::new(1)?);
        let charlie_address = ProtocolAddress::new("charlie".to_string(), DeviceId::new(1)?);

        let bob_identity = generate_identity_key_pair().await?;
        let charlie_identity = generate_identity_key_pair().await?;

        let result = storage
            .identity_store()
            .get_peer_identity(&bob_address)
            .await?;
        assert!(
            result.is_none(),
            "Should return None for non-existent peer identity"
        );

        storage
            .identity_store()
            .save_identity(&bob_address, bob_identity.identity_key())
            .await?;
        storage
            .identity_store()
            .save_identity(&charlie_address, charlie_identity.identity_key())
            .await?;

        assert_eq!(
            storage.identity_store().identity_count().await,
            2,
            "Should have 2 peer identities"
        );

        let retrieved_bob = storage
            .identity_store()
            .get_peer_identity(&bob_address)
            .await?;
        assert!(retrieved_bob.is_some(), "Should retrieve Bob's identity");
        assert_eq!(
            retrieved_bob.unwrap(),
            *bob_identity.identity_key(),
            "Retrieved identity should match stored"
        );

        let retrieved_charlie = storage
            .identity_store()
            .get_peer_identity(&charlie_address)
            .await?;
        assert!(
            retrieved_charlie.is_some(),
            "Should retrieve Charlie's identity"
        );
        assert_eq!(
            retrieved_charlie.unwrap(),
            *charlie_identity.identity_key(),
            "Retrieved identity should match stored"
        );

        storage
            .identity_store()
            .delete_identity(&bob_address)
            .await?;
        assert_eq!(
            storage.identity_store().identity_count().await,
            1,
            "Should have 1 identity after deleting Bob's"
        );

        let deleted_bob = storage
            .identity_store()
            .get_peer_identity(&bob_address)
            .await?;
        assert!(deleted_bob.is_none(), "Bob's identity should be deleted");

        let still_charlie = storage
            .identity_store()
            .get_peer_identity(&charlie_address)
            .await?;
        assert!(
            still_charlie.is_some(),
            "Charlie's identity should still exist"
        );

        storage.identity_store().clear_all_identities().await?;
        assert_eq!(
            storage.identity_store().identity_count().await,
            0,
            "Should have 0 identities after clearing all"
        );

        let cleared_charlie = storage
            .identity_store()
            .get_peer_identity(&charlie_address)
            .await?;
        assert!(
            cleared_charlie.is_none(),
            "Charlie's identity should be cleared"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_clear_local_identity() -> Result<(), Box<dyn std::error::Error>> {
        use crate::keys::generate_identity_key_pair;
        use libsignal_protocol::*;

        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize()?;

        let identity = generate_identity_key_pair().await?;
        let registration_id = 12345u32;
        storage
            .identity_store()
            .set_local_identity_key_pair(&identity)
            .await?;
        storage
            .identity_store()
            .set_local_registration_id(registration_id)
            .await?;

        let retrieved_identity = storage.identity_store().get_identity_key_pair().await?;
        assert_eq!(
            retrieved_identity.identity_key().serialize(),
            identity.identity_key().serialize()
        );

        let retrieved_registration = storage.identity_store().get_local_registration_id().await?;
        assert_eq!(retrieved_registration, registration_id);

        storage.identity_store().clear_local_identity().await?;

        let result = storage.identity_store().get_identity_key_pair().await;
        assert!(
            result.is_err(),
            "Should return error when local identity is cleared"
        );

        let result = storage.identity_store().get_local_registration_id().await;
        assert!(
            result.is_err(),
            "Should return error when local registration ID is cleared"
        );

        Ok(())
    }

    #[tokio::test]
    async fn test_bundle_metadata_record_and_retrieve() -> Result<(), Box<dyn std::error::Error>> {
        let mut storage = SqliteStorage::new(":memory:").await?;
        storage.initialize_schema()?;

        // Initially, no bundle metadata should exist
        let initial_metadata = storage.get_last_published_bundle_metadata()?;
        assert_eq!(initial_metadata, None, "Should have no metadata initially");

        // Record a bundle
        storage.record_published_bundle(100, 2, 2)?;

        // Retrieve and verify
        let metadata = storage.get_last_published_bundle_metadata()?;
        assert_eq!(
            metadata,
            Some((100, 2, 2)),
            "Should retrieve the recorded bundle metadata"
        );

        // Update with new bundle (simulating rotation)
        storage.record_published_bundle(99, 3, 3)?;

        // Verify the metadata was updated (not added as new row)
        let updated_metadata = storage.get_last_published_bundle_metadata()?;
        assert_eq!(
            updated_metadata,
            Some((99, 3, 3)),
            "Should retrieve the updated bundle metadata"
        );

        Ok(())
    }
}
