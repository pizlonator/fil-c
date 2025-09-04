#!/usr/bin/python3
# Copyright (C) 2023-2024 Free Software Foundation, Inc.
# This file is part of the GNU C Library.
#
# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

import sys

TEMPLATE = """
#include <math.h>
#include <arm_neon.h>

#define STRIDE {rowlen}

#define CALL_BENCH_FUNC_1(v, i) (__extension__ ({{                                 \\
   {rtype} mx0 = {fname}(vld1q_f{prec_short} (&variants[v].in->arg0[i * STRIDE])); \\
   mx0; }}))

#define CALL_BENCH_FUNC_2(v, i) (__extension__ ({{                                 \\
   {rtype} mx0 = {fname}(vld1q_f{prec_short} (&variants[v].in->arg0[i * STRIDE]),  \\
                         vld1q_f{prec_short} (&variants[v].in->arg1[i * STRIDE])); \\
   mx0; }}))

struct args_1
{{
  {stype} arg0[{nelems}];
}};

struct args_2
{{
  {stype} arg0[{nelems}];
  {stype} arg1[{nelems}];
}};

struct _variants
{{
  const char *name;
  const struct args_{arity} *in;
}};

static const struct args_{arity} in0 = {{
{in_data}
}};

static const struct _variants variants[1] = {{
  {{"", &in0}},
}};

#define NUM_VARIANTS 1
#define NUM_SAMPLES(i) ({nelems} / STRIDE)
#define VARIANT(i) (variants[i].name)

static {rtype} volatile ret;

#define BENCH_FUNC(i, j) ({{ ret = CALL_BENCH_FUNC_{arity}(i, j); }})
#define FUNCNAME "{fname}"
#include <bench-libmvec-skeleton.c>
"""

def main(name):
    _, prec, _, func = name.split("-")
    scalar_to_advsimd_type = {"double": "float64x2_t", "float": "float32x4_t"}
    rowlen = {"double": 2, "float": 4}[prec]
    rtype = scalar_to_advsimd_type[prec]
    atype = scalar_to_advsimd_type[prec]
    prec_short = {"double": 64, "float": 32}[prec]
    input_filename = {"double": f"{func}-inputs", "float": f"{func}f-inputs"}[prec]

    with open(f"../benchtests/libmvec/{input_filename}") as f:
        input_file = f.readlines()
    in_vals = (l.strip() for l in input_file if l and not l.startswith("#"))
    # Split in case of multivariate signature
    in_vals = (l.split(", ") for l in in_vals)
    # Transpose
    in_vals = list(zip(*in_vals))
    in_data = ",\n".join("{" + (", ".join(val for val in col) + "}")
                         for col in in_vals)

    arity = [l for l in input_file if l.startswith("## args: ")][0].count(prec)
    fname = f"_ZGVnN{rowlen}{'v' * arity}_{func}{'f' if prec == 'float' else ''}"

    print(TEMPLATE.format(rowlen=rowlen,
                          rtype=rtype,
                          atype=atype,
                          fname=fname,
                          prec_short=prec_short,
                          in_data=in_data,
                          stype=prec,
                          arity=arity,
                          nelems=len(in_vals[0])))


if __name__ == "__main__":
    main(sys.argv[1])
