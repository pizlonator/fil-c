#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

import sys
import cryptography.x509

try:
    in_path = sys.argv[1]
    out_path = sys.argv[2]
except IndexError:
    sys.exit('USAGE: update-test-database.py ca.pem output_header.h')

with open(in_path, 'rb') as in_file:
    cert_data = in_file.read()

cert = cryptography.x509.load_pem_x509_certificate(cert_data)
subject_data = cert.subject.public_bytes()
hex_subject = ''.join('\\x%02X' % b for b in subject_data)

header = '''/* This file is generated from update-test-database.py */

#define ISSUER_DATA "{}"
'''.format(hex_subject)

with open(out_path, 'w') as out_file:
    out_file.write(header)
