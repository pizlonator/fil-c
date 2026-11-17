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

# Usage: make-release.sh [token-file] [ref]
# e.g.:  make-release.sh token master     # release from master
# or  :  make-release.sh token stable-1-0 # release from stable-1-0 branch

# The project ID is displayed on the project's page in gitlab, or use
# the API to get it (replacing ${NAMESPACE} and ${PROJECT} accordingly):
# curl --silent -H "Content-Type: application/json" "https://gitlab.gnome.org/api/v4/projects/${NAMESPACE}%2F${PROJECT}" | jq -r .id

PROJECT_ID=1892

if [ $BASH_ARGC -ne 2 ]; then
    echo Usage: $0 TOKEN-FILE COMMIT-REF
    exit 1
fi

TOKEN_FILE="$1"
COMMIT_REF="$2"

curl --silent \
     --request POST \
     --form-string "token=$(cat $TOKEN_FILE)" \
     --form-string "ref=${COMMIT_REF}" \
     --form-string "variables[TRIGGER_ACTION]=release" \
     "https://gitlab.gnome.org/api/v4/projects/${PROJECT_ID}/trigger/pipeline" | jq .
