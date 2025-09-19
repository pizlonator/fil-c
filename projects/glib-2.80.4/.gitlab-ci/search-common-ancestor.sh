#!/bin/bash

set -e

ancestor_horizon=28  # days (4 weeks)

# Recently, git is picky about directory ownership. Tell it not to worry.
git config --global --add safe.directory "$PWD"

# We need to add a new remote for the upstream target branch, since this script
# could be running in a personal fork of the repository which has out of date
# branches.
#
# Limit the fetch to a certain date horizon to limit the amount of data we get.
# If the branch was forked from origin/main before this horizon, it should
# probably be rebased.
if ! git ls-remote --exit-code upstream >/dev/null 2>&1 ; then
    git remote add upstream https://gitlab.gnome.org/GNOME/glib.git
fi

# Work out the newest common ancestor between the detached HEAD that this CI job
# has checked out, and the upstream target branch (which will typically be
# `upstream/main` or `upstream/glib-2-62`).
# `${CI_MERGE_REQUEST_TARGET_BRANCH_NAME}` or `${CI_MERGE_REQUEST_SOURCE_BRANCH_NAME}`
# are only defined if we’re running in a merge request pipeline,
# fall back to `${CI_DEFAULT_BRANCH}` or `${CI_COMMIT_BRANCH}` respectively
# otherwise.

source_branch="${CI_MERGE_REQUEST_SOURCE_BRANCH_NAME:-${CI_COMMIT_BRANCH}}"
target_branch="${CI_MERGE_REQUEST_TARGET_BRANCH_NAME:-${CI_DEFAULT_BRANCH}}"

git fetch --shallow-since="$(date --date="${ancestor_horizon} days ago" +%Y-%m-%d)" origin "${source_branch}"
git fetch --shallow-since="$(date --date="${ancestor_horizon} days ago" +%Y-%m-%d)" upstream "${target_branch}"

newest_common_ancestor_sha=$(git merge-base "upstream/${target_branch}" "origin/${source_branch}")

if [ -z "${newest_common_ancestor_sha}" ]; then
    echo "Couldn’t find common ancestor with upstream main branch. This typically"
    echo "happens if you branched from main a long time ago. Please update"
    echo "your clone, rebase, and re-push your branch."
    exit 1
fi
