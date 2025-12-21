#!/usr/bin/env python3
#
# meson-pkg-config-file-fixup.py PC_FILE VAR1,VAR2,VAR3
#
# Fix up escaping of custom variables in meson-generated .pc file
#
# Copyright (C) 2021 Tim-Philipp Müller <tim centricular com>
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

import os
import sys

if len(sys.argv) < 3:
  sys.exit('Usage: {} PC_FILE_BASE_NAME VAR1 [VAR2 [VAR3 ..]]'.format(sys.argv[0]))

pc_name = sys.argv[1]
pc_vars = sys.argv[2:]

build_root = os.environ['MESON_BUILD_ROOT']

# Poking into the private dir is not entirely kosher of course..
pc_files = [
  os.path.join(build_root, 'meson-private', pc_name + '.pc'),
  os.path.join(build_root, 'meson-uninstalled', pc_name + '-uninstalled.pc')
]

for pc_file in pc_files:
  out_lines = ''

  with open(pc_file, 'r') as f:
    for line in f:
      r = line.strip().split('=', 1)
      if len(r) == 2 and r[0] in pc_vars:
        out_lines += '{}={}\n'.format(r[0], r[1].replace('\\ ', ' '))
      else:
        out_lines += line

  with open(pc_file, 'w') as f_new:
      f_new.write(out_lines)

sys.exit(0)
