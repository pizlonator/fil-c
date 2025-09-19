#!/bin/bash

set -e

source .gitlab-ci/search-common-ancestor.sh

./.gitlab-ci/check-todos.py "${newest_common_ancestor_sha}"
