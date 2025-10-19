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

print_help() {
    cat <<EOF
Usage: $0 [-f] [-h]

Enter the Fil-C development container for this checkout.

By default, if a container is already running for this checkout, this script
will attach to it. Otherwise, it will create a new container instance.

Options:
  -f    Force creation of a new container instance (don't attach to existing)
  -h    Show this help message

EOF
    exit 0
}

while getopts "fh" opt; do
    case $opt in
        f)
            FORCE_NEW=true
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

# Get the directory where this script lives
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Create a unique image tag based on the checkout path
# This allows multiple Fil-C checkouts to have separate images
CHECKOUT_HASH=$(echo -n "${SCRIPT_DIR}" | sha256sum | cut -c1-8)
IMAGE_NAME="fil-c-dev"
IMAGE_TAG="${CHECKOUT_HASH}"

# Get current user's UID
# (This part is kinda dumb, only needed for what we echo below.)
HOST_UID=$(id -u)

# If not forcing a new container, check if one is already running
if [ "$FORCE_NEW" = false ]; then
    CONTAINERS=$(podman ps --filter "label=fil-c-checkout=${CHECKOUT_HASH}" --format "{{.ID}}")

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

        exec podman exec -it "$CONTAINER_ID" /bin/bash
    fi
fi

# Build the image if it doesn't exist
if ! podman image exists "${IMAGE_NAME}:${IMAGE_TAG}"; then
    echo "Building ${IMAGE_NAME}:${IMAGE_TAG} container image..."
    podman build -f "${SCRIPT_DIR}/Dockerfile" -t "${IMAGE_NAME}:${IMAGE_TAG}" "${SCRIPT_DIR}"
    echo "Image built successfully!"
fi

# Run a new container with:
# - Rootless podman's default user namespace (container root = host user)
# - Files created by root in container will be owned by host user
# - Privileged mode for build operations
# - Read-write mount of the entire repo to /fil-c
echo "Entering Fil-C development container..."
echo "  - Running as root in container (maps to UID ${HOST_UID} on host)"
echo "  - Files created will be owned by you automatically"
echo "  - Working directory: /fil-c (maps to ${SCRIPT_DIR})"
echo ""

exec podman run --rm -it \
    --hostname "fil-c-${CHECKOUT_HASH}" \
    --label "fil-c-checkout=${CHECKOUT_HASH}" \
    --privileged \
    --ulimit core=-1 \
    --volume "${SCRIPT_DIR}:/fil-c:rw" \
    --workdir /fil-c \
    "${IMAGE_NAME}:${IMAGE_TAG}" \
    /bin/bash
