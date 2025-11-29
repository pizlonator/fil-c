# Copyright (C) 2023 Red Hat, Inc.
# Copyright (C) 2021 Collabora, Ltd.
# Copyright (C) 2016 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

import argparse
import sys

a = 'a'
b = 'b'
c = 'c'

# common conditions to improve readability
volta = 'nak->sm >= 70 && nak->sm < 75'

algebraic_lowering = [
    # Volta doesn't have `IMNMX`
    (('imin', 'a', 'b'), ('bcsel', ('ilt', a, b), a, b), volta),
    (('imax', 'a', 'b'), ('bcsel', ('ilt', a, b), b, a), volta),
    (('umin', 'a', 'b'), ('bcsel', ('ult', a, b), a, b), volta),
    (('umax', 'a', 'b'), ('bcsel', ('ult', a, b), b, a), volta),
]

late_optimizations = [
    (('iadd@32', ('imul(nak_is_only_used_by_iadd)', a, b), c),
     ('imad', a, b, c), 'nak->sm >= 70'),
]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', required=True, help='Output file.')
    parser.add_argument('-p', '--import-path', required=True)
    args = parser.parse_args()
    sys.path.insert(0, args.import_path)

    import nir_algebraic  # pylint: disable=import-error

    try:
        with open(args.out, 'w', encoding='utf-8') as f:
            f.write('#include "nak_private.h"')
            f.write(nir_algebraic.AlgebraicPass(
                "nak_nir_lower_algebraic_late",
                algebraic_lowering + late_optimizations,
                [
                    ("const struct nak_compiler *", "nak"),
                ]).render())
    except Exception:
        sys.exit(1)

if __name__ == '__main__':
    main()
