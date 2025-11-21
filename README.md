# [⇌] Radix Relay

[![ci](https://github.com/dustingooding/radix-relay/actions/workflows/ci.yml/badge.svg)](https://github.com/dustingooding/radix-relay/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/dustingooding/radix-relay/graph/badge.svg?token=D48EPLUL27)](https://codecov.io/gh/dustingooding/radix-relay)
[![CodeQL](https://github.com/dustingooding/radix-relay/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/dustingooding/radix-relay/actions/workflows/codeql-analysis.yml)

Open-source hybrid mesh communications system combining internet (Nostr) and local mesh (BLE) transports with Signal Protocol encryption.

**Status**: Phase 1 Development (Nostr + BLE + Signal Protocol)

## Security Notice

This is pre-release software. Do not use for sensitive communications until Phase 1 security audit is complete.

## Features

- **Hybrid Transport**: Automatic failover between internet (Nostr) and local mesh (BLE)
- **Signal Protocol**: End-to-end encryption with perfect forward secrecy
- **Cross-Platform**: Linux, Windows, macOS support
- **Emergency Focus**: Designed for activists, disaster response, and infrastructure-independent communications

## License

This project is licensed under the GNU Affero General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

## Contributing

This project is in active Phase 1 development. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Security

For security issues, please email <radix-relay@proton.me> or create a private security advisory on GitHub.

## Documentation

### Getting Started

- [Dependency Setup](README_dependencies.md)
- [Building Details](README_building.md)
- [Troubleshooting](README_troubleshooting.md)
- [Docker](README_docker.md)

### Architecture & Security

- [Signal Protocol Integration](docs/SIGNAL_PROTOCOL.md) - End-to-end encryption, key management, and bundle republishing

## Acknowledgments

This project is built using the excellent [cmake_template](https://github.com/cpp-best-practices/cmake_template) by [Jason Turner](https://github.com/lefticus) (@lefticus), which provides a robust foundation with modern CMake practices, comprehensive CI/CD, static analysis, and cross-platform support.
