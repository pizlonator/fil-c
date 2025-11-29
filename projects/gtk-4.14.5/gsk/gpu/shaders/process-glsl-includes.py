#!/usr/bin/env python3

import sys
import re
import os

loaded_files = []

def load (path):
    if (path in loaded_files):
        return

    loaded_files.append (path)

    with open(path) as f:
        lines = f.readlines()
        for line in lines:
            match = re.search (r"^#include \"(.*)\"$", line)
            if (match):
                load (os.path.join (os.path.dirname(path), match.group(1)))
            else:
                print (line, end="")

load (sys.argv[1])

