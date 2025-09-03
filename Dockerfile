# A Docker image for Fil-C development and compilation.
#
# Build the image with:
#   docker build -t fil-c .
#
# Run a container with:
#   docker run -it --rm \
#     --mount type=bind,source="$HOME/projects/fil-c",target="/opt/fil-c" \
#     fil-c
#
# This mounts your local Fil-C source directory to /opt/fil-c in the container.
# You can edit code on your host and compile/run tests inside the container.

FROM ubuntu:25.04

# Prevent tzdata interactive prompts during install
ENV DEBIAN_FRONTEND=noninteractive

# Update system packages
RUN apt update && apt upgrade -y

# Install essential build tools
RUN apt install -y build-essential

# Install required development dependencies
RUN apt install -y \
    pkg-config autotools-dev automake autoconf libtool \
    clang cmake ninja-build ruby patchelf

# Install basic utilities
RUN apt install -y vim git

# Create project source directory
RUN mkdir -p /opt/fil-c

# Set working directory
WORKDIR /opt/fil-c

# Start an interactive shell by default
CMD /usr/bin/bash
