#!/bin/sh

# Enter the Fil-C development container.
#
# This script runs a Docker container from the "fil-c" image,
# mounting your local project directory ($HOME/projects/fil-c)
# to /opt/fil-c inside the container for live editing and compilation.
#
# Usage: ./enter_container.sh

docker run \
  -it \
  --rm \
  --mount type=bind,source="${HOME}/projects/fil-c",target="/opt/fil-c" \
  fil-c
