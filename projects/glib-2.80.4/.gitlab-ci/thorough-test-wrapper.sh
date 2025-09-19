#!/bin/bash
#
# Copyright 2024 GNOME Foundation, Inc.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Original author: Philip Withnall

set -e

# If the test is run under Python (e.g. the first argument to this script is
# /usr/bin/python3) or if it’s the special xmllint test in GLib, then don’t
# pass the GTest `-m thorough` argument to it.
if [[ "$1" == *"python"* ||
      "$1" == *"xmllint" ]]; then
  args=()
else
  # See the documentation for g_test_init()
  args=("-m" "thorough")
fi

exec "$@" "${args[@]}"