#!/usr/bin/env sh
set -e

incomplete_source () {
    printf '%s\n' \
        "$1. Please either:" \
        "* $2," \
        "* or use the source packages instead of a repo archive" \
        "* or use a full Git clone." >&2
    exit 1
}

# This enables easy building from Github's snapshot archives
if [ ! -e ".git" ]; then
    if [ ! -f ".tarball-version" ]; then
    incomplete_source "No version information found" \
        "identify the correct version with \`echo \$version > .tarball-version\`"
    fi
else
    # Just a head start to save a ./configure cycle
    ./build-aux/git-version-gen .tarball-version > .version
fi

autoreconf --install
