#!/bin/bash

set -e

# Wrap everything in a subshell so we can propagate the exit status.
exit_status=0
(

source .gitlab-ci/search-common-ancestor.sh

git diff -U0 --no-color "${newest_common_ancestor_sha}" | .gitlab-ci/clang-format-diff.py -binary "clang-format-14" -p1

) || exit_status=$?

# The style check is not infallible. The clang-format configuration cannot
# perfectly describe GLib’s coding style: in particular, it cannot align
# function arguments. The documented coding style for GLib takes priority over
# clang-format suggestions. Hopefully we can eventually improve clang-format to
# be configurable enough for our coding style. That’s why this CI check is OK
# to fail: the idea is that people can look through the output and ignore it if
# it’s wrong. (That situation can also happen if someone touches pre-existing
# badly formatted code and it doesn’t make sense to tidy up the wider coding
# style with the changes they’re making.)
echo ""
echo "Note that clang-format output is advisory and cannot always match the GLib coding style, documented at"
echo "   https://gitlab.gnome.org/GNOME/gtk/blob/HEAD/docs/CODING-STYLE.md"
echo "Warnings from this tool can be ignored in favour of the documented coding style,"
echo "or in favour of matching the style of existing surrounding code."

exit ${exit_status}
