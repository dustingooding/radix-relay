# Hybrid Routing

Intelligent routing between internet and mesh networks.

!!! info "Implementation Status"
    **Nostr transport is functional**. Hybrid routing will be implemented after BLE transport is complete.

## Strategy

1. **Internet-Preferred** - Use Nostr when available
2. **Mesh Fallback** - Switch to BLE when internet unavailable
3. **Bridge Nodes** - Relay between networks
4. **Automatic Failover** - Seamless transition

## Use Cases

### Urban Environment

- Primary: Nostr (fast, long-range)
- Fallback: BLE (infrastructure failure)

### Remote/Disaster Areas

- Primary: BLE mesh (no infrastructure)
- Bridge: Nodes with internet relay to Nostr

### Activist Operations

- Flexible: Switch based on threat model
- Mesh-only: Maximum privacy mode
