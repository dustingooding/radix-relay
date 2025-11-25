#!/usr/bin/env bash
# Build all documentation: Doxygen (C++), rustdoc (Rust), and MkDocs (narrative)

set -euo pipefail

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Building Radix Relay documentation...${NC}"

# Build C++ API docs with Doxygen
echo -e "${GREEN}[1/3] Building C++ API documentation (Doxygen)...${NC}"
if command -v doxygen &> /dev/null; then
    doxygen Doxyfile
    # Copy Doxygen output to MkDocs site directory
    mkdir -p out/site/api/cpp
    cp -r out/doxygen/html/* out/site/api/cpp/
else
    echo "Warning: doxygen not found, skipping C++ API docs"
fi

# Build Rust API docs with rustdoc
echo -e "${GREEN}[2/3] Building Rust API documentation (rustdoc)...${NC}"
if command -v cargo &> /dev/null; then
    # Build docs for the signal_bridge crate
    cargo doc --manifest-path rust/Cargo.toml --no-deps --all-features
    # Copy rustdoc output to MkDocs site directory
    mkdir -p out/site/api/rust
    cp -r rust/target/doc/* out/site/api/rust/
else
    echo "Warning: cargo not found, skipping Rust API docs"
fi

# Build narrative documentation with MkDocs
echo -e "${GREEN}[3/3] Building narrative documentation (MkDocs)...${NC}"
mkdocs build

echo -e "${BLUE}Documentation build complete!${NC}"
echo -e "View locally: ${GREEN}out/site/index.html${NC}"
