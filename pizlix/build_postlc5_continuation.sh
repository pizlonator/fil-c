#!/bin/bash

set -e
set -x

# The last postlc stage: GDM (the login manager) is the final payload, so
# the continuation chain ends here.
./build_postlc5.sh
