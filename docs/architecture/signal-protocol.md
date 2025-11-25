# Signal Protocol Integration

## Overview

Radix Relay uses Signal Protocol's X3DH (Extended Triple Diffie-Hellman) key agreement and Double Ratchet algorithm to provide end-to-end encrypted messaging with forward secrecy and future secrecy.

## Key Management

### Key Types

The system manages three types of cryptographic keys:

1. **One-Time Pre-Keys**: Single-use keys consumed when a peer initiates a new session
2. **Signed Pre-Key**: Medium-term key rotated periodically for forward secrecy
3. **Kyber Pre-Key**: Post-quantum key providing quantum resistance, rotated periodically

### Pre-Key Bundles

A pre-key bundle is a package of public keys that allows peers to establish encrypted sessions without real-time interaction. Each bundle contains:

- Identity key (permanent)
- Signed pre-key (rotated ~7 days)
- Kyber pre-key (rotated ~7 days)
- One or more one-time pre-keys (consumed per new session)

Bundles are published to Nostr relays as kind 30078 events with the tag `d:radix_prekey_bundle_v1`.

## Bundle Republishing

### Automatic Republishing Triggers

The system automatically publishes updated pre-key bundles in two scenarios:

#### Connection-Time Republishing

When your node connects to a relay:

1. The system performs key maintenance
2. Checks if signed pre-key or Kyber pre-key have exceeded their rotation period
3. Replenishes one-time pre-keys if the pool is running low
4. Publishes a new bundle if any keys were rotated

**What this means**: Your node will publish an updated bundle approximately every 7 days when it connects to the network, ensuring your peers always have fresh keys for establishing new sessions.

#### Message-Time Republishing

When your node receives a message that consumed a one-time pre-key:

1. The message is successfully decrypted
2. The system detects that a one-time pre-key was consumed (indicates a new session was established)
3. A new bundle is immediately published with a refreshed pre-key pool

**What this means**: Every time a new peer establishes a session with your node, a fresh bundle is published to ensure the next peer also has keys available. This prevents pre-key pool exhaustion under high traffic.

### Bundle Update Frequency

Under normal operation:

- **Rotation-based updates**: ~1 every 7 days (when signed/Kyber keys rotate)
- **Consumption-based updates**: 1 per new peer connection
- **Total expected frequency**: Depends on your network activity

High-traffic nodes with many new peer connections may publish bundles more frequently. This is expected behavior and ensures key availability.

### Bundle Selection

When publishing a bundle, the system always uses the **latest available keys**:

- Most recently generated signed pre-key
- Most recently generated Kyber pre-key
- Fresh one-time pre-keys from the current pool

This ensures peers always receive the newest keys, even if older keys haven't been fully consumed yet.

## Key Rotation and Forward Secrecy

### Why Keys Rotate

**Forward Secrecy**: If a long-term key is compromised, past messages remain secure because they were encrypted with ephemeral keys that have been deleted.

**Future Secrecy**: The Double Ratchet algorithm ensures that if a session key is compromised, future messages become secure again after the next DH ratchet step.

### Rotation Periods

- **Signed Pre-Key**: Rotates every ~7 days during key maintenance
- **Kyber Pre-Key**: Rotates every ~7 days during key maintenance
- **One-Time Pre-Keys**: Consumed on use, replenished when pool drops below threshold

These periods balance security (fresh keys) with network overhead (bundle publications).

## Pre-Key Pool Management

### How the Pool Works

Your node maintains a pool of one-time pre-keys (target size: 100 keys). When peers establish new sessions with you, they consume one pre-key from the pool.

The system automatically replenishes the pool during key maintenance when it drops below a threshold (50 keys), generating new keys to restore it to the target size.

### Normal Operation

Under normal operation, the pre-key pool:

- Maintains sufficient keys for new peer connections
- Automatically replenishes without operator intervention
- Publishes updated bundles when keys are consumed

No manual intervention is required for pre-key pool management.

## Security Properties

### X3DH Key Agreement

Provides:
- **Forward secrecy**: Compromise of long-term keys doesn't compromise past sessions
- **Cryptographic deniability**: Participants can deny message authorship (within the encrypted channel)
- **Asynchronous**: Peers can establish sessions without both being online simultaneously

### Double Ratchet

Provides:
- **Per-message forward secrecy**: Each message uses a unique key
- **Future secrecy**: Self-healing after key compromise
- **Out-of-order message handling**: Messages can be received in any order

### Post-Quantum Security

Kyber pre-keys provide quantum resistance through a post-quantum key encapsulation mechanism (KEM), protecting against future quantum computer attacks.

## Bundle Metadata Tracking

The system tracks which keys have been published in bundles to enable intelligent republishing decisions:

- Records pre-key IDs when bundles are successfully published
- Uses this information to determine when fresh keys are available
- Ensures bundles always contain the latest key material

This metadata enables the system to avoid republishing identical bundles while ensuring peers always have access to the newest keys.

## Integration with Nostr

### Bundle Publication

Pre-key bundles are published as Nostr kind 30078 events (Parameterized Replaceable Events) with:
- Tag `d:radix_prekey_bundle_v1` for identification
- Tag `v:<version>` for version tracking
- Base64-encoded bundle in the event content

The "parameterized replaceable" property ensures each node has exactly one current bundle on the relay.

### Encrypted Messages

Encrypted messages are published as Nostr kind 40001 events with:
- Signal Protocol encrypted payload
- Recipient public key in event tags
- Sender's identity signature

### Discovery

Nodes discover peer bundles by subscribing to kind 30078 events with tag `d:radix_prekey_bundle_v1`. When a new bundle is received, it's stored for future session establishment.

## Expected Behavior

### On Connection

When your node connects to a relay:

1. Performs key maintenance (checks rotation periods, replenishes pool)
2. Publishes bundle if keys rotated (approximately every 7 days)
3. Subscribes to peer identity announcements (kind 30078)
4. Subscribes to incoming encrypted messages (kind 40001)

### On Message Reception

When your node receives an encrypted message:

1. Decrypts the message using the Double Ratchet
2. Detects if a one-time pre-key was consumed (new session)
3. If consumed, immediately publishes a new bundle
4. Delivers decrypted message to the user

### On Session Establishment

When your node establishes a session with a peer:

1. Retrieves the peer's most recent bundle from discovered bundles
2. Performs X3DH key agreement using the bundle
3. Initializes a Double Ratchet session
4. Can now send and receive encrypted messages with that peer

## Performance

Bundle republishing is designed to be lightweight:

- Key generation completes in milliseconds
- Bundle serialization is fast (<5ms)
- Network overhead is minimal (~2KB per bundle)

The system is optimized for:
- Low CPU usage during normal operation
- Minimal memory footprint
- Efficient database operations
- Negligible impact on message latency
