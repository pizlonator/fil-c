#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import re
import os
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

debug = os.getenv('GTK_GENTYPEFUNCS_DEBUG') is not None

out_file = sys.argv[1]
in_files = sys.argv[2:]

funcs = []


if debug: print ('Output file: ', out_file)

if debug: print (len(in_files), 'input files')

def open_file(filename, mode):
    if sys.version_info[0] < 3:
        return open(filename, mode=mode)
    else:
        return open(filename, mode=mode, encoding='utf-8')

for filename in in_files:
  if debug: print ('Input file: ', filename)
  with open_file(filename, "r") as f:
    for line in f:
      line = line.rstrip('\n').rstrip('\r')
      # print line
      match = re.search(r'\bg[dst]k_[a-zA-Z0-9_]*_get_type\b', line)
      if match:
        func = match.group(0)
        if not func in funcs:
          funcs.append(func)
          if debug: print ('Found ', func)

file_output = ['G_GNUC_BEGIN_IGNORE_DEPRECATIONS']

funcs = sorted(funcs)

for f in funcs:
  if f.startswith('gdk_x11'):
    file_output += ['#ifdef GDK_WINDOWING_X11']
    file_output += ['*tp++ = {0}();'.format(f)]
    file_output += ['#endif']
  elif f.startswith('gdk_broadway') or f.startswith('gsk_broadway'):
    file_output += ['#ifdef GDK_WINDOWING_BROADWAY']
    file_output += ['*tp++ = {0}();'.format(f)]
    file_output += ['#endif']
  elif f.startswith('gdk_wayland'):
    file_output += ['#ifdef GDK_WINDOWING_WAYLAND']
    file_output += ['*tp++ = {0}();'.format(f)]
    file_output += ['#endif']
  elif f.startswith('gdk_win32'):
    file_output += ['#ifdef GDK_WINDOWING_WIN32']
    file_output += ['*tp++ = {0}();'.format(f)]
    file_output += ['#endif']
  elif f.startswith('gdk_macos'):
    file_output += ['#ifdef GDK_WINDOWING_MACOS']
    file_output += ['*tp++ = {0}();'.format(f)]
    file_output += ['#endif']
  elif f.startswith('gsk_vulkan'):
    file_output += ['#ifdef GDK_RENDERING_VULKAN']
    file_output += ['*tp++ = {0}();'.format(f)]
    file_output += ['#endif']
  elif f.startswith('gsk_ngl'):
    file_output += ['']
  else:
    file_output += ['*tp++ = {0}();'.format(f)]

file_output += ['G_GNUC_END_IGNORE_DEPRECATIONS']

if debug: print (len(funcs), 'functions')

tmp_file = out_file + '~'
with open(tmp_file, 'w') as f:
    f.write('\n'.join(file_output))
replace_if_changed(tmp_file, out_file)
