#!/bin/bash
#
# Copyright © 2025 Christian Persch
#
# This library is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library.  If not, see <https://www.gnu.org/licenses/>.

# Usage: make-release.sh [token-file] [ref] [version]
# e.g.:  make-release.sh token master 1.99.90 # release from master
# or  :  make-release.sh token stable-1-0 1.0.17 # release from stable-1-0 branch

# The project ID is displayed on the project's page in gitlab, or use
# the API to get it (replacing ${NAMESPACE} and ${PROJECT} accordingly):
# curl --silent -H "Content-Type: application/json" "https://gitlab.gnome.org/api/v4/projects/${NAMESPACE}%2F${PROJECT}" | jq -r .id

CI_PROJECT_ID=1893

if [ $BASH_ARGC -ne 3 ]; then
    echo "Usage: $0 TOKEN-FILE COMMIT-REF VERSION"
    exit 1
fi

TOKEN_FILE="$1"
COMMIT_REF="$2"
PROJECT_VERSION="$3"

CI_API_V4_URL="https://gitlab.gnome.org/api/v4"

request=$(curl --silent \
     --request POST \
     --form-string "token=$(cat $TOKEN_FILE)" \
     --form-string "ref=${COMMIT_REF}" \
     --form-string "variables[TRIGGER_ACTION]=release" \
     "${CI_API_V4_URL}/projects/${CI_PROJECT_ID}/trigger/pipeline")

pipeline_url=$(echo "$request" | jq .web_url)

# The release pipeline creates the tag. This used to trigger the
# pipeline for the tag, which causes the tarball to be installed
# using the gnome release service (if configured). However, this
# https://gitlab.com/gitlab-org/gitlab/-/issues/569187 broke this
# deliberately, and unless and until there exist a way to do this
# again (see https://gitlab.com/gitlab-org/gitlab/-/issues/569974)
# the tag's pipeline needs to be triggered manually.
#
# As an added complication, while the release pipeline knows the
# version (by tracing meson), here we don't know it. The user must
# input it manually.

echo "Monitor ${pipeline_url} for completion."
echo "When completed, press ENTER; on failure, press Ctrl-C to abort."

read

request=$(curl --silent \
     --request POST \
     --form-string "token=$(cat $TOKEN_FILE)" \
     --form-string "ref=${PROJECT_VERSION}" \
     "${CI_API_V4_URL}/projects/${CI_PROJECT_ID}/trigger/pipeline")

tag_pipeline_url=$(echo "$request" | jq .web_url)

if [ "$tag_pipeline_url" == "null" ]; then
  echo "$request" | jq .
else
  echo "Tag pipeline ${tag_pipeline_url}"
fi
