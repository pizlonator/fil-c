# Fil-C Container Development Environment

This document describes the Podman-based container setup for Fil-C development.

## Overview

The container system provides a reproducible build environment for Fil-C using Ubuntu 22.04 LTS with specific versions of build tools. Key features:

- **Rootless Podman** - No root privileges required on the host
- **Per-checkout isolation** - Each checkout can have its own container image
- **Correct file ownership** - Files created in the container are owned by you on the host
- **Passwordless sudo** - Build scripts requiring root work seamlessly

## Scripts

### `./enter_container.sh [-f] [-h]`

The main entry point for the container environment.

**Default behavior:**
- If a container is already running for this checkout, attaches to it
- Otherwise, creates a new container instance

**Options:**
- `-f` - Force creation of a new container (don't attach to existing)
- `-h` - Show help message

**Examples:**
```bash
./enter_container.sh        # Smart attach/create
./enter_container.sh -f     # Always create new instance
```

### `./reset_container.sh`

Deletes the container image for the current checkout, forcing a rebuild on next `enter_container.sh`.

**Use when:**
- You modified the Dockerfile
- You modified the container scripts
- You want to start fresh

```bash
./reset_container.sh
./enter_container.sh  # Will rebuild the image
```

## How It Works

### Per-Checkout Image Isolation

Each checkout gets its own container image based on a hash of the checkout path:

```bash
CHECKOUT_HASH=$(echo -n "${SCRIPT_DIR}" | sha256sum | cut -c1-8)
IMAGE_NAME="fil-c-dev"
IMAGE_TAG="${CHECKOUT_HASH}"
```

This means:
- Different checkouts can experiment with different Dockerfiles independently
- Modifying the Dockerfile in one checkout doesn't affect others
- Images are ~1.5GB each, but Podman shares common layers

### User Namespace Mapping

The container uses **rootless Podman's default user namespace** (NOT `--userns=keep-id`):

- You appear as **root inside the container** (UID 0)
- Container root (UID 0) **maps to your host UID** on the filesystem
- Files created by root in the container are **owned by you on the host**
- No ownership ping-ponging when working in both host and container shells

**Why not `--userns=keep-id`?**
- With `keep-id`, you're your regular user in the container
- Running `sudo` in the container creates files owned by UID 100000 on the host
- This breaks the "no ownership issues" requirement

### Privileged Mode

The container runs with `--privileged` to fix permission issues (e.g., symlink operations that fail in restricted containers).

**Is this safe?**
- Yes, for local development
- The user namespace still isolates you from the real host root
- You can only modify files your regular user could modify
- The container cannot actually damage the host system

### Container Labeling

Running containers are labeled with `fil-c-checkout=${CHECKOUT_HASH}` so `enter_container.sh` can find and attach to them.

## Dockerfile Customizations

### Ubuntu 22.04 LTS

Chosen for:
- cmake 3.22+ (required for LLVM)
- Good binary compatibility
- Long-term support

### Specific Autotools Versions

Fil-C requires specific versions of autotools, built from source in the Dockerfile:

- **m4 1.4.19** - Built first (dependency of autoconf)
- **autoconf 2.72** - Built second (dependency of automake)
- **automake 1.17** - Built third
- **libtool 2.4.7** - Built last

These are installed to `/usr/local` from tarballs in `pizlix/`, taking precedence over system versions.

### Passwordless Sudo

The Dockerfile adds:
```dockerfile
RUN echo "ALL ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers.d/dev-container
```

This allows build scripts like `optfil/build.sh` that require `sudo` to work seamlessly.

## Workflow Examples

### Basic Development Session

```bash
# Terminal 1: Enter container
./enter_container.sh
# You're now root in the container at /fil-c
cd optfil
sudo ./build.sh

# Terminal 2: Attach to same container
./enter_container.sh
# You're in the same container instance
```

### Experimenting with Dockerfile

```bash
# Edit Dockerfile
vim Dockerfile

# Rebuild the image
./reset_container.sh
./enter_container.sh

# Test your changes...
```

### Multiple Checkouts

```bash
# Checkout A
cd /path/to/checkout-a
./enter_container.sh        # Uses image fil-c-dev:hashA

# Checkout B
cd /path/to/checkout-b
./enter_container.sh        # Uses image fil-c-dev:hashB

# Independent container environments!
```

## Container Management

### Detaching and Reattaching

If you want to leave a container running but return to your host shell:

**Detach:** Press **Ctrl-P Ctrl-Q** (hold Ctrl, press P, then press Q)
- Leaves container running
- Returns you to host shell
- Like screen/tmux detach

**Reattach:**
```bash
podman ps  # Find your container ID
podman attach <container-id>
```

Or just use `./enter_container.sh` which will exec into the running container.

**Note:** If you accidentally press Ctrl-C in a running build, it may detach you (not kill the container). Use `podman attach` to reconnect.

## Troubleshooting

### PTY/Terminal Wedging (CRITICAL ISSUE)

**Symptom:** Terminal stops updating, appears frozen, but container is still running.

**Root Cause:** Parallel builds (`make -j32`) where all processes simultaneously output warnings create a perfect storm:
1. **High data rate** - Many warnings from compilation
2. **32 concurrent writes** - All processes writing to PTY simultaneously
3. **ANSI escape sequences** - Compiler color/formatting codes interleave mid-sequence
4. Result: PTY layer gets overwhelmed and wedges

**Example triggers:**
- Building bash with K&R function signature warnings (32 processes × warnings)
- Building glibc with macro redefinition warnings (32 processes × warnings)
- Any parallel build with verbose compiler output

**Solutions:**

**Option 1: Suppress warnings** (recommended for known-good packages)
```bash
# In optfil/build.sh, add -w to compiler flags
CC="/opt/fil/bin/filcc -w" CXX="/opt/fil/bin/fil++ -w" ./configure ...
```

**Option 2: Suppress specific warnings**
```bash
# For Fil-C's macro redefinition warnings in glibc
CC="/opt/fil/bin/filcc -Wno-macro-redefined" ./configure ...
```

**Option 3: Disable color diagnostics** (not tested but might help)
```bash
CC="clang -fno-color-diagnostics" ./configure ...
```

**Recovery if wedged:**
- Press **Ctrl-C** to detach (container keeps running)
- Use `podman attach <container-id>` or `./enter_container.sh` to reconnect
- Or press **Ctrl-Q** if you accidentally pressed Ctrl-S (XOFF flow control)

**Note:** This appears to be a Podman PTY handling bug that doesn't gracefully handle massive concurrent writes with ANSI codes. The issue may be fixed in newer Podman versions (tested on 3.4.4 from 2022).

### Files owned by UID 100000

This means you're using `--userns=keep-id` and ran `sudo` in the container. The current setup doesn't use `--userns=keep-id`, so this shouldn't happen.

### "Operation not permitted" errors

If you see permission errors that don't happen on the host, the container might need `--privileged`. The current setup already includes this.

### Image won't rebuild

After modifying the Dockerfile:
```bash
./reset_container.sh        # Delete old image
./enter_container.sh        # Build new image
```

### Multiple containers running

```bash
# See all running containers for this checkout
podman ps --filter "label=fil-c-checkout=$(echo -n "$PWD" | sha256sum | cut -c1-8)"

# Enter a specific one
podman exec -it <container-id> /bin/bash

# Or force a new one
./enter_container.sh -f
```

### Cleaning up old images

```bash
# List all images
podman images

# Remove specific image
podman rmi fil-c-dev:hashXXXXXXXX

# Prune unused images
podman image prune
```

## Technical Details

### Container Run Command

The full `podman run` command used by `enter_container.sh`:

```bash
podman run --rm -it \
    --hostname "fil-c-${CHECKOUT_HASH}" \
    --label "fil-c-checkout=${CHECKOUT_HASH}" \
    --privileged \
    --ulimit core=-1 \
    --volume "${SCRIPT_DIR}:/fil-c:rw" \
    --workdir /fil-c \
    "${IMAGE_NAME}:${IMAGE_TAG}" \
    /bin/bash
```

Breakdown:
- `--rm` - Auto-remove container when it exits
- `-it` - Interactive with TTY
- `--hostname` - Consistent hostname per checkout
- `--label` - Allows finding running containers
- `--privileged` - Full capabilities within user namespace
- `--ulimit core=-1` - Unlimited core dumps (equivalent to `ulimit -c unlimited`)
- `--volume` - Mount checkout as /fil-c
- `--workdir` - Start in /fil-c

### Why No `--name`?

We use labels instead of container names because:
- Multiple instances can run simultaneously (with `-f`)
- Names must be unique (would conflict)
- Labels allow finding all instances for a checkout

## Future: `/opt/fil` Development with Rootful Podman

The current rootless container setup is great for general Fil-C development (`build_all.sh`, running tests, etc.) but has limitations for `/opt/fil` distribution development:

**Challenge:** `/opt/fil` should be:
- Owned by **root** (it's a system installation)
- Shared between **host and container** (for incremental development)
- Testable with system services (systemd, PAM, sshd integration)

**Current rootless setup issues:**
- Container root → host user UID (files not owned by real root)
- Can't properly test system integration
- Not the "real" deployment scenario

**Proposed solution:** Rootful Podman for `/opt/fil` development

### Rootful Container Approach (Not Yet Implemented)

**Concept:**
```bash
sudo ./enter_container.sh -r  # New -r (rootful) mode
```

**Key differences from current setup:**
- Run with **rootful Podman** (not rootless)
- Container runs as **real root**
- Mount `/opt/fil` from host into container
- Files created by root in container → owned by root on host
- Both host and container share same `/opt/fil`

**Workflow:**
1. Build `/opt/fil` in container (as root)
2. Exit container
3. Test `/opt/fil` on host with real systemd/PAM/sshd
4. Iterate: make changes in container, test on host

**Implementation ideas (discussed but not built):**

**Option A: Dockerfile template approach**
- Generate Dockerfile dynamically with user's UID
- `enter_container.sh` detects file owner UID
- Substitutes UID into Dockerfile template
- Builds image with matching user for `su` to work
- Image tag includes UID: `fil-c-dev:${CHECKOUT_HASH}-uid1000-rootful`

**Option B: Runtime user creation**
- Rootful container detects mounted file ownership
- Creates user with matching UID at startup
- No template needed, but more runtime complexity

**Why not implement yet:**
- Current rootless setup works for most development
- VM is available for `/opt/fil` + system integration testing
- Can implement when need becomes urgent

**Tradeoffs: Rootful Container vs VM**

**Rootful Container (if implemented):**
- ✓ Lighter than VM
- ✓ Share `/opt/fil` cleanly
- ✓ Fast iteration
- ✗ Still can't test real systemd/PAM integration (no init system)
- ✗ More complex setup

**VM:**
- ✓ Real systemd, PAM, full system
- ✓ Matches actual deployment
- ✓ Can test everything
- ✗ Heavier (separate desktop, more resources)
- ✗ Slower to set up

**Current recommendation:** Use VM for `/opt/fil` development and system integration testing. Implement rootful container approach only if VM workflow becomes too cumbersome.
