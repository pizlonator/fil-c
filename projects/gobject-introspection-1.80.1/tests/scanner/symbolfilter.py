# -*- Mode: Python; py-indent-offset: 4 -*-
# vim: tabstop=4 shiftwidth=4 expandtab
#
# Copyright (C) 2015 Garrett Regier <garrett.regier@riftio.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
# USA

"""
Script which reads symbols in the form of "FooBar" from stdin and
translates them to snake case like "foo_bar".
"""

import sys


def ensure_snake_case(text):
    text = ''.join(x if x.islower() else '_' + x.lower() for x in text)

    # Remove the extra '_' for the starting uppercase letter
    return text[1:]


if __name__ == '__main__':
    text = ensure_snake_case(sys.stdin.read())
    sys.stdout.write(text)
