#!/bin/bash

set -e
set -x

mkdir build
cd build
cmake .. -DPORT=JSCOnly -DCMAKE_BUILD_TYPE=RelWithDebInfo -G Ninja -DENABLE_JIT=OFF -DENABLE_C_LOOP=ON -DENABLE_SAMPLING_PROFILER=OFF -DENABLE_WEBASSEMBLY=OFF -DUSE_SYSTEM_MALLOC=ON

set +x

echo "SUCCESS"
echo "Now do (cd build && ninja) to build."
