#!/usr/bin/env python3
#
# Generate gdk.gresources.xml
#
# Usage: gen-gdk-gresources-xml SRCDIR_GDK [OUTPUT-FILE]

import os, sys
import filecmp

def replace_if_changed(new, old):
  '''
  Compare contents and only replace if changed to avoid triggering a rebuild.
  '''
  try:
    changed = not filecmp.cmp(new, old, shallow=False)
  except FileNotFoundError:
    changed = True
  if changed:
    os.replace(new, old)
  else:
    os.remove(new)

srcdir = sys.argv[1]

xml = '''<?xml version='1.0' encoding='UTF-8'?>
<gresources>
  <gresource prefix='/org/gtk/libgdk'>
    <file alias='cursor/default'>default_cursor.png</file>
'''

def get_files(subdir,extension):
  return sorted(filter(lambda x: x.endswith((extension)), os.listdir(os.path.join(srcdir,subdir))))

xml += '''
  </gresource>
</gresources>'''

if len(sys.argv) > 2:
  outfile = sys.argv[2]
  tmpfile = outfile + '~'
  with open(tmpfile, 'w') as f:
    f.write(xml)
  replace_if_changed(tmpfile, outfile)
else:
  print(xml)
