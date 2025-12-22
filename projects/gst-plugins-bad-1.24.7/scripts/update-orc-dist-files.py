#!/usr/bin/env python3
#
# update-orc-dist-files.py ORC-FILE GENERATED-HEADER GENERATED-SOURCE
#
# Copies generated orc .c and .h files into source dir as -dist.[ch] backups,
# based on location of passed .orc file.
#
# Copyright (C) 2020 Tim-Philipp Müller <tim centricular com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
# Boston, MA 02110-1301, USA.

import shutil
import subprocess
import sys

assert(len(sys.argv) == 4)

orc_file = sys.argv[1]
gen_header = sys.argv[2]
gen_source = sys.argv[3]

# split off .orc suffix
assert(orc_file.endswith('.orc'))
orc_src_base = sys.argv[1][:-4]

# figure out names of disted backup files
dist_h = orc_src_base + "-dist.h"
dist_c = orc_src_base + "-dist.c"

# copy generated files from build dir into source dir
shutil.copyfile(gen_header, dist_h)
shutil.copyfile(gen_source, dist_c)

# run gst-indent on the .c files (twice, because gnu indent)
subprocess.run(['gst-indent', dist_c])
subprocess.run(['gst-indent', dist_c])
