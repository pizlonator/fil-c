#!/usr/bin/env python3

import os
import sys

def usage():
    print('Usage:')
    print('find_xdg_file.py FILENAME')
    print('')
    print('Looks for FILENAME in the XDG data directories and returns if path if found')

if len(sys.argv) != 2:
    usage()
    sys.exit(1)

filename = sys.argv[1]

data_home = os.getenv('XDG_DATA_HOME')
if not data_home or data_home == '':
    data_home = os.path.join(os.path.expanduser("~"), "local", "share")

data_dirs_str = os.getenv('XDG_DATA_DIRS')
if not data_dirs_str or data_dirs_str == '':
    data_dirs_str = '/usr/local/share/:/usr/share/'

dirs = []
dirs += [ data_home ]
for _dir in data_dirs_str.split(':'):
    dirs += [ _dir ]

for _dir in dirs:
    full_path = os.path.join(_dir, filename)
    if os.path.exists(full_path):
        print(full_path)
        sys.exit(0)

print(f"'{filename}' not found in XDG data directories")
sys.exit(1)
