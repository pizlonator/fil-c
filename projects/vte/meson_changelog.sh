#!/usr/bin/env bash
# Copyright © 2021 Christian Persch
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

set -e

top_srcdir="$MESON_SOURCE_ROOT"
top_builddir="MESON_BUILD_ROOT"
top_distdir="$MESON_DIST_ROOT"

if ! test -e "${top_srcdir}"/.git; then
    echo "Must be run from gnome-terminal git checkout"
    exit 1
fi

if ! test -e "${top_distdir}"; then
    echo "Must be run from 'meson dist'"
    exit 1
fi

GIT_DIR="${top_srcdir}"/.git git log --stat > "${top_distdir}"/ChangeLog
