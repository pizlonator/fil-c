#!/usr/bin/env python
# Copyright © 2020 Christian Persch
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import array, fcntl, sys, termios

buffer = array.array('H', [0, 0, 0, 0])
ret = fcntl.ioctl(sys.stdin.fileno(), termios.TIOCGWINSZ, buffer)
if ret != 0:
    print(f'ioctl(TIOCGWINSZ) failed: {ret}')
    sys.exit(1)

print(f'{buffer[0]} rows ({buffer[3]} px), {buffer[1]} columns ({buffer[2]} px)')
sys.exit(0)
