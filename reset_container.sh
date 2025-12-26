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
# THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS'' AND ANY
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
ROOTFUL=false
PIZLIX=false

print_help() {
    cat <<EOF
Usage: $0 [-r] [-p' [-h]

Delete the container image for this checkout, forcing a rebuild on next run.

Options:
  -r    Reset rootful mode image (requires sudo)
  -p    Reset pizlix image
  -h    Show this help message

EOF
    exit 0
}

while getopts "rph" opt; do
    case $opt in
        r)
            ROOTFUL=true
            ;;
        p)
            PIZLIX=true
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
    if [ "$PIZLIX" = true ]; then
        echo "Error: Pizlix mode (-p) requires running with sudo"
        echo "Run: sudo $0 -p"
    else
        echo "Error: Rootful mode (-r) requires running with sudo"
        echo "Run: sudo $0 -r"
    fi
    exit 1
fi

# Get the directory where this script lives
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Create the same image tag that enter_container.sh uses
CHECKOUT_HASH=$(echo -n "${SCRIPT_DIR}" | sha256sum | cut -c1-8)
IMAGE_NAME="fil-c-dev"

# Compute image tag based on mode (same logic as enter_container.sh)
if [ "$ROOTFUL" = true ]; then
    FILE_OWNER_UID=$(stat -c %u "${SCRIPT_DIR}")
    if [ "$PIZLIX" = true ]; then
        IMAGE_TAG="${CHECKOUT_HASH}-pizlix-uid${FILE_OWNER_UID}"
    else
        IMAGE_TAG="${CHECKOUT_HASH}-rootful-uid${FILE_OWNER_UID}"
    fi
else
    IMAGE_TAG="${CHECKOUT_HASH}"
fi

DOCKERFILE_PATH="${SCRIPT_DIR}/.dockerfile-${IMAGE_TAG}"

# Check if the image exists
if podman image exists "${IMAGE_NAME}:${IMAGE_TAG}"; then
    echo "Removing ${IMAGE_NAME}:${IMAGE_TAG} container image..."
    podman rmi "${IMAGE_NAME}:${IMAGE_TAG}"
    echo "Image removed successfully!"

    # Clean up generated Dockerfile if it exists
    if [ -f "${DOCKERFILE_PATH}" ]; then
        echo "Removing generated Dockerfile ${DOCKERFILE_PATH}..."
        rm -f "${DOCKERFILE_PATH}"
    fi
else
    echo "Image ${IMAGE_NAME}:${IMAGE_TAG} does not exist. Nothing to do."
fi
