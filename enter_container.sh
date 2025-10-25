#!/bin/bash
#
# Copyright (c) 2025 Epic Games, Inc. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL EPIC GAMES, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

set -e

# Parse command-line options
FORCE_NEW=false
ROOTFUL=false

print_help() {
    cat <<EOF
Usage: $0 [-f] [-r] [-h]

Enter the Fil-C development container for this checkout.

By default, if a container is already running for this checkout, this script
will attach to it. Otherwise, it will create a new container instance.

Options:
  -f    Force creation of a new container instance (don't attach to existing)
  -r    Use rootful mode (requires sudo, for /opt/fil development)
  -h    Show this help message

Rootful mode (-r):
  - Runs container as real root using sudo podman
  - Mounts /opt/fil (if it exists) shared with host
  - Creates user matching file owner for su commands
  - Use for /opt/fil development and testing

EOF
    exit 0
}

while getopts "frh" opt; do
    case $opt in
        f)
            FORCE_NEW=true
            ;;
        r)
            ROOTFUL=true
            ;;
        h)
            print_help
            ;;
        \?)
            echo "Invalid option: -$OPTARG" >&2
            echo "Use -h for help" >&2
            exit 1
            ;;
    esac
done

# Check if rootful mode requires sudo
if [ "$ROOTFUL" = true ] && [ $EUID -ne 0 ]; then
    echo "Error: Rootful mode (-r) requires running with sudo"
    echo "Run: sudo $0 -r"
    exit 1
fi

# Get the directory where this script lives
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Detect file owner UID/GID
FILE_OWNER_UID=$(stat -c %u "${SCRIPT_DIR}")
FILE_OWNER_GID=$(stat -c %g "${SCRIPT_DIR}")

# Check that rootless mode is run as the file owner
if [ "$ROOTFUL" = false ]; then
    CURRENT_UID=$(id -u)
    if [ "$CURRENT_UID" -ne "$FILE_OWNER_UID" ]; then
        FILE_OWNER_NAME=$(stat -c %U "${SCRIPT_DIR}")
        echo "Error: Rootless mode must be run as the checkout owner"
        echo "  Checkout owner: ${FILE_OWNER_NAME} (UID ${FILE_OWNER_UID})"
        echo "  Current user: $(whoami) (UID ${CURRENT_UID})"
        echo ""
        echo "Either:"
        echo "  1. Run as ${FILE_OWNER_NAME}: sudo -u ${FILE_OWNER_NAME} $0"
        echo "  2. Use rootful mode: sudo $0 -r"
        exit 1
    fi
fi

# Create a unique image tag based on the checkout path
# This allows multiple Fil-C checkouts to have separate images
CHECKOUT_HASH=$(echo -n "${SCRIPT_DIR}" | sha256sum | cut -c1-8)
IMAGE_NAME="fil-c-dev"

# Set image tag and container label based on mode
if [ "$ROOTFUL" = true ]; then
    IMAGE_TAG="${CHECKOUT_HASH}-rootful-uid${FILE_OWNER_UID}"
    CONTAINER_LABEL="fil-c-checkout-rootful=${CHECKOUT_HASH}"
    PODMAN_CMD="podman"  # Already running as root via sudo
else
    IMAGE_TAG="${CHECKOUT_HASH}"
    CONTAINER_LABEL="fil-c-checkout=${CHECKOUT_HASH}"
    PODMAN_CMD="podman"  # Rootless
    # For display purposes only
    HOST_UID=$(id -u)
fi

# If not forcing a new container, check if one is already running
if [ "$FORCE_NEW" = false ]; then
    CONTAINERS=$($PODMAN_CMD ps --filter "label=${CONTAINER_LABEL}" --format "{{.ID}}")

    if [ -n "$CONTAINERS" ]; then
        # Count how many containers we found
        CONTAINER_COUNT=$(echo "$CONTAINERS" | wc -l)

        if [ "$CONTAINER_COUNT" -eq 1 ]; then
            CONTAINER_ID="$CONTAINERS"
            echo "Attaching to existing Fil-C container ${CONTAINER_ID}..."
        else
            echo "Found ${CONTAINER_COUNT} running Fil-C containers for this checkout:"
            echo "$CONTAINERS"
            echo "Attaching to the first one..."
            CONTAINER_ID=$(echo "$CONTAINERS" | head -n 1)
        fi

        exec $PODMAN_CMD exec -it "$CONTAINER_ID" /bin/bash
    fi
fi

# Generate Dockerfile based on mode
DOCKERFILE_PATH="${SCRIPT_DIR}/.dockerfile-${IMAGE_TAG}"

echo "Generating Dockerfile at ${DOCKERFILE_PATH}..."

# Write the common Dockerfile header
# NOTE: We're using the single-quote here document, so that things like $(nproc) are literal.
cat > "${DOCKERFILE_PATH}" <<'DOCKERFILE_END'
# A Docker image for Fil-C development and compilation.
# Using Ubuntu 22.04 LTS for maximum binary compatibility across Linux distributions.
FROM ubuntu:22.04

# Set non-interactive mode to avoid tzdata and other prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Update system package lists and upgrade installed packages
RUN apt-get update && apt-get upgrade -y

# Install essential build tools (compiler, linker, etc.)
RUN apt-get install -y build-essential perl libxml-parser-perl

# Build specific versions of autotools from source (required for Fil-C)
# These must be built in order: m4, autoconf, automake, libtool
COPY pizlix/m4-1.4.19.tar.xz /usr/local/src/
RUN cd /usr/local/src && \
    tar -xf m4-1.4.19.tar.xz && \
    cd m4-1.4.19 && \
    ./configure --prefix=/usr/local && \
    make -j $(nproc) && \
    make -j $(nproc) install && \
    cd /usr/local/src && \
    rm -rf m4-1.4.19 m4-1.4.19.tar.xz

COPY pizlix/autoconf-2.72.tar /usr/local/src/
RUN cd /usr/local/src && \
    tar -xf autoconf-2.72.tar && \
    cd autoconf-2.72 && \
    ./configure --prefix=/usr/local && \
    make -j $(nproc) && \
    make -j $(nproc) install && \
    cd /usr/local/src && \
    rm -rf autoconf-2.72 autoconf-2.72.tar

COPY pizlix/automake-1.17.tar.xz /usr/local/src/
RUN cd /usr/local/src && \
    tar -xf automake-1.17.tar.xz && \
    cd automake-1.17 && \
    ./configure --prefix=/usr/local && \
    make -j $(nproc) && \
    make -j $(nproc) install && \
    cd /usr/local/src && \
    rm -rf automake-1.17 automake-1.17.tar.xz

COPY pizlix/libtool-2.4.7.tar.xz /usr/local/src/
RUN cd /usr/local/src && \
    tar -xf libtool-2.4.7.tar.xz && \
    cd libtool-2.4.7 && \
    ./configure --prefix=/usr/local && \
    make -j $(nproc) && \
    make -j $(nproc) install && \
    cd /usr/local/src && \
    rm -rf libtool-2.4.7 libtool-2.4.7.tar.xz

COPY pizlix/intltool-0.51.0.tar.gz /usr/local/src/
RUN cd /usr/local/src && \
    tar -xf intltool-0.51.0.tar.gz && \
    cd intltool-0.51.0 && \
    ./configure --prefix=/usr/local && \
    make -j $(nproc) && \
    make -j $(nproc) install && \
    cd /usr/local/src && \
    rm -rf intltool-0.51.0 intltool-0.51.0.tar.gz

# Install development dependencies required for building Fil-C and related projects
RUN apt-get install -y \
    pkg-config \
    clang cmake ninja-build ruby \
    patchelf bison flex texinfo gettext autopoint

# Install basic utilities for development and version control
RUN apt-get install -y curl vim git

# Additional build dependencies for complete Fil-C development
RUN apt-get install -y \
    gcc g++ make gawk \
    python3 python3-pip python3-setuptools \
    wget rsync file less sudo \
    libncurses-dev libssl-dev zlib1g-dev \
    xz-utils bzip2 gzip gdb lldb mg

RUN pip install meson

DOCKERFILE_END

# Add user creation for rootful mode
if [ "$ROOTFUL" = true ]; then
    cat >> "${DOCKERFILE_PATH}" <<DOCKERFILE_END

# Configure passwordless sudo for all users (dev container only)
# Preserve core dump limit instead of resetting to 0
RUN echo "Defaults rlimit_core=default" >> /etc/sudoers.d/dev-container && \
    echo "ALL ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers.d/dev-container && \
    chmod 0440 /etc/sudoers.d/dev-container

# Allow core dumps when using sudo (disabled by default for security)
RUN echo "Set disable_coredump false" >> /etc/sudo.conf

# Create user matching host file owner (for su commands in build scripts)
RUN groupadd -g ${FILE_OWNER_GID} builder 2>/dev/null || true && \
    useradd -u ${FILE_OWNER_UID} -g ${FILE_OWNER_GID} -m -s /bin/bash builder

DOCKERFILE_END
fi

# Write the common Dockerfile footer
cat >> "${DOCKERFILE_PATH}" <<'DOCKERFILE_END'

# Set up reasonable shell environment
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV SHELL=/bin/bash

# Create the project source directory inside the container
RUN mkdir -p /fil-c

# OpenSSH server install will want this.
RUN mkdir -p /var/empty

# The optfil build expects /opt/fil to exist already.
RUN mkdir -p /opt/fil

# Make sure that we have a place to put coredumps and Fil-C panics
RUN mkdir -p /var/coredumps /var/filc/panics
RUN chmod 1777 /var/coredumps /var/filc/panics

# Set the working directory to the project source directory
WORKDIR /fil-c

# Start an interactive Bash shell by default when the container runs
CMD ["/bin/bash"]
DOCKERFILE_END

# Build the image if it doesn't exist
if ! $PODMAN_CMD image exists "${IMAGE_NAME}:${IMAGE_TAG}"; then
    echo "Building ${IMAGE_NAME}:${IMAGE_TAG} container image..."
    $PODMAN_CMD build -f "${DOCKERFILE_PATH}" -t "${IMAGE_NAME}:${IMAGE_TAG}" "${SCRIPT_DIR}"
    echo "Image built successfully!"
fi

# Prepare volume mounts and run command based on mode
if [ "$ROOTFUL" = true ]; then
    # Rootful mode: mount checkout and /opt/fil if it exists
    VOLUME_ARGS="--volume ${SCRIPT_DIR}:/fil-c:rw"
    if [ -d /opt/fil ]; then
        VOLUME_ARGS="${VOLUME_ARGS} --volume /opt/fil:/opt/fil:rw"
        echo "Entering Fil-C development container (rootful mode)..."
        echo "  - Running as real root"
        echo "  - /opt/fil mounted and shared with host"
        echo "  - Files created will be owned by root"
        echo "  - Working directory: /fil-c (maps to ${SCRIPT_DIR})"
    else
        echo "Entering Fil-C development container (rootful mode)..."
        echo "  - Running as real root"
        echo "  - /opt/fil not found on host (not mounted)"
        echo "  - Working directory: /fil-c (maps to ${SCRIPT_DIR})"
    fi
    echo ""

    exec $PODMAN_CMD run --rm -it \
        --hostname "fil-c-${CHECKOUT_HASH}" \
        --label "${CONTAINER_LABEL}" \
        --ulimit core=-1 \
        ${VOLUME_ARGS} \
        --workdir /fil-c \
        "${IMAGE_NAME}:${IMAGE_TAG}" \
        /bin/bash
else
    # Rootless mode: standard setup
    echo "Entering Fil-C development container (rootless mode)..."
    echo "  - Running as root in container (maps to UID ${HOST_UID} on host)"
    echo "  - Files created will be owned by you automatically"
    echo "  - Working directory: /fil-c (maps to ${SCRIPT_DIR})"
    echo ""

    exec $PODMAN_CMD run --rm -it \
        --hostname "fil-c-${CHECKOUT_HASH}" \
        --label "${CONTAINER_LABEL}" \
        --privileged \
        --ulimit core=-1 \
        --volume "${SCRIPT_DIR}:/fil-c:rw" \
        --workdir /fil-c \
        "${IMAGE_NAME}:${IMAGE_TAG}" \
        /bin/bash
fi
