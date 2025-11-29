#!/usr/bin/env python3

import os
import argparse
import sys
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

parser = argparse.ArgumentParser()
parser.add_argument('--array-name', help='The name of the array variable')
parser.add_argument('--output', metavar='STRING', help='Output filename',
                    default=None)
parser.add_argument('input', metavar='FILE', help='The input file',
                    type=argparse.FileType('r'))

args = parser.parse_args()

if args.output is None:
    output = sys.stdout
else:
    output = args.output + '~'

with open(output, 'w') as f:
    f.write('static const char {}[] = {{\n'.format(args.array_name))
    for line in args.input:
        for ch in line:
            f.write('  0x{:02x},\n'.format(ord(ch)))
    f.write('};')

if args.output is not None:
    replace_if_changed(output, args.output)
