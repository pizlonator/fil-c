# Copyright (C) 2024 Igalia S.L.
# SPDX-License-Identifier: MIT


import argparse
import sys


# Inverse DeMorgan's laws to facilitate folding iand/ior into braa/brao. Only
# apply if the inot is only used by branch conditions. Otherwise, it would just
# end-up generating more instructions.
cond_lowering = [
    (('inot(is_only_used_by_if)', ('iand', 'a', 'b')),
     ('ior', ('inot', 'a'), ('inot', 'b'))),
    (('inot(is_only_used_by_if)', ('ior', 'a', 'b')),
     ('iand', ('inot', 'a'), ('inot', 'b'))),
]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--import-path', required=True)
    args = parser.parse_args()
    sys.path.insert(0, args.import_path)
    run()


def run():
    import nir_algebraic  # pylint: disable=import-error

    print('#include "ir3_nir.h"')
    print(nir_algebraic.AlgebraicPass("ir3_nir_opt_branch_and_or_not",
                                      cond_lowering).render())


if __name__ == '__main__':
    main()
