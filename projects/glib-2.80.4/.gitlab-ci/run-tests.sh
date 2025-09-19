#!/bin/bash

set -ex

meson test -v \
        -C _build \
        --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
        "$@"

# Run only the flaky tests, so we can log the failures but without hard failing
meson test -v \
        -C _build \
        --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
        "$@" --setup=unstable_tests --suite=failing --suite=flaky || true
