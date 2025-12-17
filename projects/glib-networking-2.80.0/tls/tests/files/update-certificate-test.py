#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

import sys
import cryptography.x509

try:
    in_path = sys.argv[1]
    out_path = sys.argv[2]
except IndexError:
    sys.exit('USAGE: update-test-database.py server.pem output_header.h')

with open(in_path, 'rb') as in_file:
    cert_data = in_file.read()

cert = cryptography.x509.load_pem_x509_certificate(cert_data)

header = '''/* This file is generated from update-certificate-test.py */

#define EXPECTED_NOT_VALID_BEFORE "{}Z"
#define EXPECTED_NOT_VALID_AFTER "{}Z"
'''.format(cert.not_valid_before.isoformat(), cert.not_valid_after.isoformat())

with open(out_path, 'w') as out_file:
    out_file.write(header)
