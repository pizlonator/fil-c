export XDG_RUNTIME_DIR="/run/user/$(id -u)"
mkdir -m 700 "$XDG_RUNTIME_DIR" 2>/dev/null || true

