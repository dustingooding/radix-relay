# Architecture

Radix Relay implements a layered architecture for hybrid mesh communications.

## System Overview

```
┌─────────────────────────────────────────────┐
│              CLI Interface                  │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│       Signal Protocol Encryption            │
│  (X3DH + Double Ratchet + Kyber)            │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│          Hybrid Routing Layer               │
│   (Internet-preferred, mesh fallback)       │
└─────────────────┬───────────────────────────┘
                  │
        ┌─────────┴─────────┐
        │                   │
┌───────▼──────┐     ┌──────▼────────┐
│    Nostr     │     │      BLE      │
│  Transport   │     │   Transport   │
└──────────────┘     └───────────────┘
```

## Key Components

### [Signal Protocol](signal-protocol.md)

End-to-end encryption with forward and future secrecy using X3DH key agreement and Double Ratchet algorithm.

### [Transport Layer](transport-layer.md)

Unified interface for multiple transport mechanisms (Nostr, BLE, and future protocols).

### [Hybrid Routing](hybrid-routing.md)

Intelligent routing that prefers internet connectivity but seamlessly falls back to mesh networking.

## Design Principles

1. **Security First** - All messages encrypted end-to-end
2. **Resilient** - Works with partial infrastructure
3. **Modular** - Clean separation between layers
4. **Extensible** - Easy to add new transports
