#!/usr/bin/env python3
# Copyright © 2019, 2022 Christian Persch
#
# This programme is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# This programme is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this programme.  If not, see <https://www.gnu.org/licenses/>.

import os
import subprocess
import sys

if os.environ.get('DESTDIR'):
    sys.exit(0)

argc = len(sys.argv)
if argc < 3:
    sys.exit(1)

prefix = os.environ['MESON_INSTALL_PREFIX']
desktopdatadir = sys.argv[1]

exit_code = 0

for i in range(2, argc):
    try:
        desktopfile = os.path.join(prefix, desktopdatadir, sys.argv[i])
        result = subprocess.run(['desktop-file-validate',
                                 desktopfile])
        if result.returncode != 0:
            exit_code = 1

    except FileNotFoundError:
        # desktop-file-validate not installed
        pass

sys.exit(exit_code)
