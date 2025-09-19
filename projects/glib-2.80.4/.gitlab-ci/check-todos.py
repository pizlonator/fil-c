#!/usr/bin/env python3
#
# Copyright © 2019 Endless Mobile, Inc.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Original author: Philip Withnall

"""
Checks that a merge request doesn’t add any instances of the string ‘todo’
(in uppercase), or similar keywords. It may remove instances of that keyword,
or move them around, according to the logic of `git log -S`.
"""

import argparse
import re
import subprocess
import sys


# We have to specify these keywords obscurely to avoid the script matching
# itself. The keyword ‘fixme’ (in upper case) is explicitly allowed because
# that’s conventionally used as a way of marking a workaround which needs to
# be merged for now, but is to be grepped for and reverted or reworked later.
BANNED_KEYWORDS = ["TO" + "DO", "X" + "XX", "W" + "IP"]


def main():
    parser = argparse.ArgumentParser(
        description="Check a range of commits to ensure they don’t contain "
        "banned keywords."
    )
    parser.add_argument("commits", help="SHA to diff from, or range of commits to diff")
    args = parser.parse_args()

    banned_words_seen = set()
    seen_in_log = False
    seen_in_diff = False

    # Check the log messages for banned words.
    log_process = subprocess.run(
        ["git", "log", "--no-color", args.commits + "..HEAD"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding="utf-8",
        check=True,
    )
    log_lines = log_process.stdout.strip().split("\n")

    for line in log_lines:
        for keyword in BANNED_KEYWORDS:
            if re.search(r"(^|\W+){}(\W+|$)".format(keyword), line):
                banned_words_seen.add(keyword)
                seen_in_log = True

    # Check the diff for banned words.
    diff_process = subprocess.run(
        ["git", "diff", "-U0", "--no-color", args.commits],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        encoding="utf-8",
        check=True,
    )
    diff_lines = diff_process.stdout.strip().split("\n")

    for line in diff_lines:
        if not line.startswith("+ "):
            continue

        for keyword in BANNED_KEYWORDS:
            if re.search(r"(^|\W+){}(\W+|$)".format(keyword), line):
                banned_words_seen.add(keyword)
                seen_in_diff = True

    if banned_words_seen:
        if seen_in_log and seen_in_diff:
            where = "commit message and diff"
        elif seen_in_log:
            where = "commit message"
        elif seen_in_diff:
            where = "commit diff"

        print(
            "Saw banned keywords in a {}: {}. "
            "This indicates the branch is a work in progress and should not "
            "be merged in its current "
            "form.".format(where, ", ".join(banned_words_seen))
        )
        sys.exit(1)


if __name__ == "__main__":
    main()
