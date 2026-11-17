#!/usr/bin/env python3
# Copyright © 2019 Christian Persch
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

prefix = os.environ['MESON_INSTALL_PREFIX']
icondir = os.path.join(prefix, sys.argv[1])

rv = subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])
sys.exit(0)
