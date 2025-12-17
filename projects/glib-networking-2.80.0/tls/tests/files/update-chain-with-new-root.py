#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

import sys

try:
    chain_path = sys.argv[1]
    new_root_path = sys.argv[2]
except IndexError:
    sys.exit('USAGE: update-chain-with-new-root.py ca-file.pem new-ca.pem')

with open(new_root_path, 'rb') as new_file:
    new_cert_lines = new_file.readlines()

with open(chain_path, 'rb+') as chain_file:
    chain_file_lines = chain_file.readlines()
    new_chain_file_lines = []

    # Replace the lines of the old root with the new cert
    for i, line in enumerate(chain_file_lines):
        if b'BEGIN CERTIFICATE' in line:
            new_chain_file_lines += chain_file_lines[:i]
            new_chain_file_lines += new_cert_lines
            continue
        if b'END CERTIFICATE' in line:
            new_chain_file_lines += chain_file_lines[i + 1:]
            break

    # Write over old file
    chain_file.seek(0)
    chain_file.writelines(new_chain_file_lines)
