#!/bin/bash

set -eux -o pipefail

xcodebuild -version || :

if [ -z "$SDKROOT" ]; then
  xcodebuild -showsdks || :
else
  echo "SDKROOT = $SDKROOT"
fi

system_profiler SPSoftwareDataType || :
