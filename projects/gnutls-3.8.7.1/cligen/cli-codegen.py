#!/usr/bin/python
# Copyright (C) 2021-2022 Daiki Ueno
# SPDX-License-Identifier: LGPL-2.1-or-later

import argparse
import cligen.package
import cligen.types
import cligen.code
import json


parser = argparse.ArgumentParser(description='generate option parsing code')
parser.add_argument('json', type=argparse.FileType('r'))
parser.add_argument('c', type=argparse.FileType('w'))
parser.add_argument('h', type=argparse.FileType('w'))
parser.add_argument('--package', help='package', required=True)
parser.add_argument('--version', help='version', required=True)
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
cligen.code.generate_source(desc, info, args.c)
cligen.code.generate_header(desc, info, args.h)
