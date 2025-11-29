#!/bin/bash

set -e

srcdir=$(pwd)

mkdir -p _ccache
export CCACHE_BASEDIR="$(pwd)"
export CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"

ccache --zero-stats
ccache --show-stats
export CCACHE_DISABLE=true
meson \
        -Dx11-backend=true \
        -Dwayland-backend=true \
        -Dbroadway-backend=true \
        -Dvulkan=enabled \
        -Dprofiler=true \
        --werror \
        ${EXTRA_MESON_FLAGS:-} \
        _build $srcdir
unset CCACHE_DISABLE

cd _build

ninja
ccache --show-stats

set +e

xvfb-run -a -s "-screen 0 1024x768x24" \
    meson test \
        --timeout-multiplier 2 \
        --print-errorlogs \
        --suite=gtk \
        --no-suite=gtk:a11y

# Save the exit code
exit_code=$?

# We always want to run the report generators
$srcdir/.gitlab-ci/meson-junit-report.py \
        --project-name=gtk \
        --job-id="${CI_JOB_NAME}" \
        --output=report.xml \
        meson-logs/testlog.json

$srcdir/.gitlab-ci/meson-html-report.py \
        --project-name=GTK \
        --job-id="${CI_JOB_NAME}" \
        --reftest-output-dir="testsuite/reftests/output" \
        --output=report.html \
        meson-logs/testlog.json

exit $exit_code
