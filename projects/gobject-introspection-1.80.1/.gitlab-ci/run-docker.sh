#!/bin/bash

if [ ! -x "$(command -v docker)" ] || [ docker --help |& grep -q podman ]; then
        # Docker is actually implemented by podman, and its OCI output
        # is incompatible with some of the dockerd instances on GitLab
        # CI runners.
        echo "Using: Podman"
        format="--format docker"
        CMD="podman"
else
        echo "Using: Docker"
        format=""
        CMD="sudo docker"
fi

set -e

REGISTRY="registry.gitlab.gnome.org/gnome/gobject-introspection"
TAG="${REGISTRY}:v13"

${CMD} build \
        ${format} \
        --build-arg HOST_USER_ID="$UID" \
        --tag "${TAG}" \
        --file "Dockerfile" .
${CMD} run \
        --rm \
        --volume "$(pwd)/..:/home/user/app" --workdir "/home/user/app" \
        --tty --interactive \
        "${TAG}" \
        bash
