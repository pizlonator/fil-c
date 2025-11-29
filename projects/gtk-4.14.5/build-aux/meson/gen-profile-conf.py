#!/usr/bin/env python3

import os
import subprocess
import sys

repodir = sys.argv[1]
profile = sys.argv[2]

sys.stdout.write("/* This file is auto-generated. Do not edit. */\n")
sys.stdout.write("#pragma once\n")
sys.stdout.write("\n")
sys.stdout.write(f"#define PROFILE \"{profile}\"\n")

short_sha = os.environ.get('CI_COMMIT_SHORT_SHA')
if short_sha is None:
    cmd = ["git", "-C", repodir, "rev-parse", "--short", "HEAD"]
    try:
        with subprocess.Popen(cmd, stdout=subprocess.PIPE) as p:
            short_sha = p.stdout.read().decode('utf-8').rstrip("\n")
    except FileNotFoundError:
        short_sha = ''
        if profile != 'default':
            short_sha = 'devel'

sys.stdout.write(f"#define VCS_TAG \"{short_sha}\"\n")
