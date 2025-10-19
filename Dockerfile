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

# A Docker image for Fil-C development and compilation.
# Using Ubuntu 22.04 LTS for maximum binary compatibility across Linux distributions.
FROM ubuntu:22.04

# Set non-interactive mode to avoid tzdata and other prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Update system package lists and upgrade installed packages
RUN apt-get update && apt-get upgrade -y

# Install essential build tools (compiler, linker, etc.)
RUN apt-get install -y build-essential

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

# Install development dependencies required for building Fil-C and related projects
RUN apt-get install -y \
    pkg-config \
    clang cmake ninja-build ruby \
    patchelf bison flex texinfo gettext autopoint

# Install basic utilities for development and version control
RUN apt-get install -y curl vim git

# Additional build dependencies for complete Fil-C development
RUN apt-get install -y \
    gcc g++ make gawk perl \
    python3 python3-pip python3-setuptools \
    wget rsync file less sudo \
    libncurses-dev libssl-dev zlib1g-dev \
    xz-utils bzip2 gzip

# Clean up apt cache to reduce image size
RUN rm -rf /var/lib/apt/lists/*

# Set up reasonable shell environment
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
ENV SHELL=/bin/bash

# Create the project source directory inside the container
RUN mkdir -p /fil-c

# OpenSSH server install will want this.
RUN mkdir -p /var/empty

# Make sure that we have a place to put coredumps and Fil-C panics
RUN mkdir -p /var/coredumps /var/filc/panics
RUN chmod 1777 /var/coredumps /var/filc/panics

# Set the working directory to the project source directory
WORKDIR /fil-c

# Start an interactive Bash shell by default when the container runs
CMD ["/bin/bash"]
