# Security

Security is fundamental to Radix Relay's design.

## Security Model

### Threat Model

Radix Relay is designed to protect against:

- **Passive Surveillance**: All messages encrypted end-to-end
- **Active Attacks**: Authenticated encryption prevents tampering
- **Metadata Analysis**: Nostr provides some metadata protection
- **Key Compromise**: Forward and future secrecy limits damage

### Out of Scope

- **Traffic Analysis**: Network-level observation can reveal communication patterns
- **Endpoint Security**: Device compromise can expose messages
- **Timing Attacks**: Message timing may leak information

## Encryption

### Signal Protocol

Radix Relay uses the Signal Protocol for end-to-end encryption:

- **X3DH** for key agreement
- **Double Ratchet** for message encryption
- **Kyber** for post-quantum security

See [Signal Protocol Documentation](../architecture/signal-protocol.md) for details.

## Security Practices

### Reporting Vulnerabilities

**Do not** report security vulnerabilities publicly.

Instead:
1. Email [radix-relay@proton.me](mailto:radix-relay@proton.me)
2. Or create a private security advisory on GitHub
3. Include steps to reproduce
4. Allow time for a fix before public disclosure

### Responsible Disclosure

We follow a 90-day disclosure timeline:
1. Report received and acknowledged
2. Fix developed and tested
3. Security advisory published
4. Details disclosed after fix is deployed

## Pre-Release Warning

!!! danger "Do Not Use for Sensitive Communications"
    Radix Relay is pre-release software. It has not undergone a security audit. Do not use it for communications that require confidentiality until Phase 1 audit is complete.

## Audit Status

- **Phase 1 Audit**: Planned
- **Continuous Review**: In progress
- **Bug Bounty**: Not yet available

## Security Updates

Security updates will be announced via:
- GitHub Security Advisories
- Project README
- Email to known users (when applicable)
