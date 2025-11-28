#!/bin/bash

set -e

TAG="ebassi/mutestci:latest"

sudo docker build --build-arg HOST_USER_ID="$UID" --tag ${TAG} \
        --file "Dockerfile" .
sudo docker run --rm --security-opt label=disable \
        --volume "$(pwd)/..:/home/user/app" --workdir "/home/user/app" \
        --tty --interactive "${TAG}" bash
