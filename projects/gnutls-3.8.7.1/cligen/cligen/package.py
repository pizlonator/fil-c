# Copyright (C) 2021-2022 Daiki Ueno
# SPDX-License-Identifier: LGPL-2.1-or-later

from typing import NamedTuple, Optional, Sequence
from cligen.types import Desc
import datetime
import io
import os
import textwrap


try:
    import pwd

    def get_default_copyright_holder():
        return pwd.getpwuid(os.getuid()).pw_gecos
except ImportError:
    def get_default_copyright_holder():
        return 'COPYRIGHT HOLDER'


class Info(NamedTuple):
    name: str
    version: str
    license: str = 'gpl3+'
    copyright_year: str = str(datetime.date.today().year)
    copyright_holder: str = get_default_copyright_holder()
    bug_email: Optional[str] = None
    authors: Sequence[str] = list()


BRIEF_LICENSES = {
    'gpl3+': textwrap.dedent('''\
        This program is released under the terms of
        the GNU General Public License, version 3 or later
    ''')
}
SHORT_LICENSES = {
    'gpl3+': textwrap.dedent('''\
        This is free software. It is licensed for use, modification and
        redistribution under the terms of the GNU General Public License,
        version 3 or later <http://gnu.org/licenses/gpl.html>
    ''')
}
FULL_LICENSES = {
    'gpl3+': textwrap.dedent('''\
        This is free software. It is licensed for use, modification and
        redistribution under the terms of the GNU General Public License,
        version 3 or later <http://gnu.org/licenses/gpl.html>

        @PACKAGE_NAME@ is free software: you can redistribute it and/or
        modify it under the terms of the GNU General Public License
        as published by the Free Software Foundation,
        either version 3 of the License, or (at your option) any later version.

        @PACKAGE_NAME@ is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty
        of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
        See the GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program.  If not, see <http://www.gnu.org/licenses/>.
    ''')
}


def license(desc: Desc, info: Info, what='short') -> str:
    if what == 'brief':
        license_text = BRIEF_LICENSES[info.license]
    elif what == 'short':
        license_text = SHORT_LICENSES[info.license]
    elif what == 'full':
        license_text = FULL_LICENSES[info.license]
    return license_text.replace('@PACKAGE_NAME@', info.name)


def version(desc: Desc, info: Info, what='c') -> str:
    out = io.StringIO()

    if what == 'v':
        out.write(f'{desc.tool.name} {info.version}')
    elif what == 'c':
        out.write(textwrap.dedent(f'''\
            {desc.tool.name} {info.version}
            Copyright (C) {info.copyright_year} {info.copyright_holder}
        '''))
        out.write(license(desc, info, 'short'))
        if info.bug_email:
            out.write(textwrap.dedent(f'''\

                Please send bug reports to:  <{info.bug_email}>\
            '''))
    elif what == 'n':
        out.write(textwrap.dedent(f'''\
            {desc.tool.name} {info.version}
            Copyright (C) {info.copyright_year} {info.copyright_holder}
        '''))
        out.write(license(desc, info, 'full'))
        if info.bug_email:
            out.write(textwrap.dedent(f'''\

                Please send bug reports to:  <{info.bug_email}>\
            '''))
    return out.getvalue()
