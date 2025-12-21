#!/usr/bin/env python3
#
# extract-release-date-from-doap-file.py VERSION DOAP-FILE
#
# Extract release date for the given release version from a DOAP file
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

import sys
import xml.etree.ElementTree as ET

if len(sys.argv) != 3:
  sys.exit('Usage: {} VERSION DOAP-FILE'.format(sys.argv[0]))

release_version = sys.argv[1]
doap_fn = sys.argv[2]

tree = ET.parse(doap_fn)
root = tree.getroot()

namespaces = {'doap': 'http://usefulinc.com/ns/doap#'}

for v in root.findall('doap:release/doap:Version', namespaces=namespaces):
  if v.findtext('doap:revision', namespaces=namespaces) == release_version:
    release_date = v.findtext('doap:created', namespaces=namespaces)
    if release_date:
      print(release_date)
      sys.exit(0)

sys.exit('Could not find a release with version {} in {}'.format(release_version, doap_fn))
