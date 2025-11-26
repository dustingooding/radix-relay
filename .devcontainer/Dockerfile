# Use Arch Linux as base
FROM docker.io/archlinux:latest

# Update system and install base development tools
RUN pacman -Syu --noconfirm && \
    pacman -S --noconfirm \
        base-devel \
        git \
        cmake \
        ninja \
        ccache \
        wget \
        curl \
        unzip \
        zip \
        openssh \
        rsync \
        sudo

# Install C++ compilers and tools
ARG GCC_VER="14"
ARG LLVM_VER="19"

RUN pacman -S --noconfirm \
        gcc \
        clang \
        lldb \
        lld \
        clang-tools-extra \
        gdb \
        cppcheck \
        doxygen \
        graphviz

# Install required system dependencies
# Note: Boost is the only C++ library we install from pacman because:
# 1. It's large and time-consuming to build
# 2. Arch has exactly the version we need (1.89.0)
# 3. CMake can find it reliably
# Other C++ libraries (fmt, spdlog, catch2, etc.) will be downloaded by CPM as needed
RUN pacman -S --noconfirm \
        boost=1.89.0-3 \
        boost-libs=1.89.0-3 \
        protobuf=33.0-2 \
        openssl

# Install Rust toolchain
ENV RUSTUP_HOME=/usr/local/rustup \
    CARGO_HOME=/usr/local/cargo \
    PATH=/usr/local/cargo/bin:$PATH

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --default-toolchain stable && \
    chmod -R a+rw ${RUSTUP_HOME} ${CARGO_HOME} && \
    rustup --version && \
    cargo --version && \
    rustc --version

# Install editors
RUN pacman -S --noconfirm \
        neovim \
        nano

# Install Python and development tools
RUN pacman -S --noconfirm \
        python \
        python-pip \
        gcovr

# Set default compiler environment variables
ARG USE_CLANG
ENV CC=${USE_CLANG:+"clang"}
ENV CXX=${USE_CLANG:+"clang++"}
ENV CC=${CC:-"gcc"}
ENV CXX=${CXX:-"g++"}

# Create a non-root user for development
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME || true \
    && useradd --uid $USER_UID --gid $USER_GID -m -s /bin/bash $USERNAME \
    && echo "$USERNAME ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# Clean package cache to reduce image size
RUN pacman -Scc --noconfirm

CMD ["/bin/bash"]
