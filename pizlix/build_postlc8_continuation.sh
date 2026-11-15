#!/bin/bash

set -e
set -x

./build_postlc8.sh

./build_postlc9_continuation.sh
