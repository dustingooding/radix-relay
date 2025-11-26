# Transport Layer

Unified transport abstraction for Radix Relay.

## Overview

The transport layer provides a unified interface for multiple communication mechanisms:

- **Nostr** - ✅ **Fully Implemented** - Internet-based relay network
- **BLE** - 📋 **Planned** - Local mesh networking
- **Future protocols** - Extensible design

## Nostr Transport (Implemented)

### Components

**WebSocket Transport** ([lib/transport/](https://github.com/dustingooding/radix-relay/tree/main/lib/transport))

- Boost.Beast-based async WebSocket implementation
- TLS/SSL support (wss:// required for security)
- Connection lifecycle management
- Read/write operations with coroutines

**Nostr Protocol** ([lib/nostr/](https://github.com/dustingooding/radix-relay/tree/main/lib/nostr))

- Event types: OK, EOSE, EVENT, REQ, CLOSE
- Message kinds: encrypted_message (40001), bundle_announcement (30078)
- JSON serialization/deserialization
- Tag parsing and validation

**Session Orchestrator**

- Coordinates transport, Signal bridge, and presentation layers
- Handles bundle discovery and session establishment
- Message routing and decryption
- Automatic key maintenance and bundle republishing

### Features

✅ **Connection Management**

- Async connect to Nostr relays
- TLS/SSL encryption (wss://)
- Connection state tracking
- Error handling and recovery

✅ **Message Handling**

- Event-based architecture
- Subscription management
- Request tracking with timeouts
- Duplicate message detection

✅ **Integration**

- Works with Signal Protocol for E2E encryption
- Contact management and aliases
- Bundle announcement publishing
- Session persistence

### Implementation

The Nostr transport implementation includes:

- Connecting to real Nostr relays (e.g., relay.damus.io)
- Publishing pre-key bundles
- Discovering peer bundles
- Establishing encrypted sessions
- Sending/receiving encrypted messages

## BLE Transport (Planned)

### Planned Components

- BLE device discovery
- GATT server/client implementation
- Mesh networking protocol
- Platform-specific APIs (BlueZ, CoreBluetooth, etc.)

## Design Goals

1. **Abstraction** - Uniform interface regardless of transport
2. **Flexibility** - Easy to add new transports
3. **Efficiency** - Minimal overhead
4. **Reliability** - Handle connection failures gracefully
