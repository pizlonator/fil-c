#!/usr/bin/python
# Copyright (C) 2021-2022 Daiki Ueno
# SPDX-License-Identifier: LGPL-2.1-or-later

import argparse
import cligen.package
import cligen.types
import json
import sys

parser = argparse.ArgumentParser(description='generate documentation')
parser.add_argument('json', type=argparse.FileType('r'))
parser.add_argument('outfile', type=argparse.FileType('w'))
parser.add_argument('--format', choices=['man', 'texi'])
parser.add_argument('--level', type=int, default=0)
parser.add_argument('--section-node', action='store_true')
parser.add_argument('--include', action='append')
parser.add_argument('--package', help='package', required=True)
parser.add_argument('--version', help='version')
parser.add_argument('--license', help='license')
parser.add_argument('--authors', help='authors')
parser.add_argument('--copyright-year', help='copyright year')
parser.add_argument('--copyright-holder', help='copyright holder')
parser.add_argument('--bug-email', help='bug report email address')

args = parser.parse_args()
kwargs = {
    'name': args.package,
    'version': args.version
}
if args.license:
    kwargs['license'] = args.license
if args.copyright_year:
    kwargs['copyright_year'] = args.copyright_year
if args.copyright_holder:
    kwargs['copyright_holder'] = args.copyright_holder
if args.bug_email:
    kwargs['bug_email'] = args.bug_email
info = cligen.package.Info(**kwargs)
desc = cligen.types.Desc.from_json(json.load(args.json))

includes = dict()
if args.include:
    for i in args.include:
        (section, infile) = i.split('=')
        includes[section] = open(infile, 'r')

if args.format == 'man':
    import cligen.doc.man
    cligen.doc.man.generate(desc, info, includes, args.outfile)
elif args.format == 'texi':
    import cligen.doc.texi
    cligen.doc.texi.generate(desc, info, includes, args.outfile,
                             level=args.level, section_node=args.section_node)
else:
    sys.stderr.write(f'Unknown format {args.format}\n')
    sys.exit(1)
