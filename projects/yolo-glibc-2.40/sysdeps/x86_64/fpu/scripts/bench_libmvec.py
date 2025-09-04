#!/usr/bin/python3
# Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

"""Benchmark program generator script

This script takes a function name as input and generates a program using
an libmvec input file located in the sysdeps/x86_64/fpu directory.  The
name of the input file should be of the form libmvec-foo-inputs where
'foo' is the name of the function.
"""

from __future__ import print_function
import sys
import os
import itertools
import re

# Macro definitions for functions that take no arguments.  For functions
# that take arguments, the STRUCT_TEMPLATE, ARGS_TEMPLATE and
# VARIANTS_TEMPLATE are used instead.
DEFINES_TEMPLATE = '''
#define CALL_BENCH_FUNC(v, i) %(func)s ()
#define NUM_VARIANTS (1)
#define NUM_SAMPLES(v) (1)
#define VARIANT(v) FUNCNAME "()"
'''

# Structures to store arguments for the function call.  A function may
# have its inputs partitioned to represent distinct performance
# characteristics or distinct flavors of the function.  Each such
# variant is represented by the _VARIANT structure.  The ARGS structure
# represents a single set of arguments.
BENCH_VEC_TEMPLATE = '''
#define CALL_BENCH_FUNC(v, i) (__extension__ ({ \\
  %(defs)s mx0 = %(func)s (%(func_args)s); \\
  mx0; }))
'''

BENCH_SCALAR_TEMPLATE = '''
#define CALL_BENCH_FUNC(v, i) %(func)s (%(func_args)s)
'''

STRUCT_TEMPLATE = '''struct args
{
%(args)s
  double timing;
};

struct _variants
{
  const char *name;
  int count;
  struct args *in;
};
'''

# The actual input arguments.
ARGS_TEMPLATE = '''struct args in%(argnum)d[%(num_args)d] = {
%(args)s
};
'''

# The actual variants, along with macros defined to access the variants.
VARIANTS_TEMPLATE = '''struct _variants variants[%(num_variants)d] = {
%(variants)s
};

#define NUM_VARIANTS %(num_variants)d
#define NUM_SAMPLES(i) (variants[i].count)
#define VARIANT(i) (variants[i].name)
'''

# Epilogue for the generated source file.
EPILOGUE = '''
#define BENCH_FUNC(i, j) ({%(getret)s CALL_BENCH_FUNC (i, j);})
#define FUNCNAME "%(func)s"
#include <bench-libmvec-skeleton.c>'''


def gen_source(func_types, directives, all_vals):
  """Generate source for the function

  Generate the C source for the function from the values and
  directives.

  Args:
    func: The function name
    directives: A dictionary of directives applicable to this function
    all_vals: A dictionary input values
  """
  # The includes go in first.
  for header in directives['includes']:
    print('#include <%s>' % header)

  for header in directives['include-sources']:
    print('#include "%s"' % header)

  argtype_vtable = {
    2: '128',
    4: '256',
    8: '512'
  }
  prefix_vtable = {
    2: 'b',
    4: 'c',
    8: 'e'
  }

  # Get all the function properties
  funcname_argtype = ''
  float_flag = False
  if func_types[1] == 'float':
    float_flag = True
  avx_flag = False
  if func_types[3] == 'avx2':
    avx_flag = True
  funcname_stride = int(func_types[2][4:])
  funcname_origin = func_types[-1]
  if float_flag:
    funcname_origin = funcname_origin[:-1]

  if funcname_stride == 1:
    # Prepare for scalar functions file generation
    funcname_prefix = ''
    funcname_prefix_1 = ''
    funcname_argtype = 'double'
    if float_flag:
      funcname_argtype = 'float'
  else:
    # Prepare for libmvec functions file generation
    funcname_prefix_1 = len(directives['args']) * 'v' + '_'
    aligned_stride = funcname_stride
    if float_flag:
      aligned_stride /= 2
    funcname_prefix = '_ZGV'
    if (avx_flag and (aligned_stride == 4)):
      funcname_prefix += 'd'
    else:
      funcname_prefix += prefix_vtable[aligned_stride]
    funcname_prefix = funcname_prefix + 'N' + func_types[2][4:]
    funcname_argtype = '__m' + argtype_vtable[aligned_stride]
    if not float_flag:
      funcname_argtype += 'd'

  # Include x86intrin.h for vector functions
  if not funcname_stride == 1:
    print('#include <x86intrin.h>')
    if (avx_flag and (aligned_stride == 4)):
      # For bench-float-vlen8-avx2* and bench-double-vlen4-avx2*
      print('#define REQUIRE_AVX2')
    elif aligned_stride == 8:
      # For bench-float-vlen16* and bench-double-vlen8*
      print('#define REQUIRE_AVX512F')
    elif aligned_stride == 4:
      # For bench-float-vlen8* and bench-double-vlen4* without avx2
      print('#define REQUIRE_AVX')
  else:
    print('#define FUNCTYPE %s' % funcname_argtype)

  print('#define STRIDE %d ' % funcname_stride)

  funcname = funcname_prefix + funcname_prefix_1 + funcname_origin
  if float_flag:
    funcname += 'f'

  funcname_rettype = funcname_argtype
  if directives['ret'] == '':
    funcname_rettype = 'void'

  funcname_inputtype = []
  for arg, i in zip(directives['args'], itertools.count()):
    funcname_inputtype.append(funcname_argtype)
    if arg[0] == '<' and arg[-1] == '>':
      pos = arg.rfind('*')
      if pos == -1:
        die('Output argument must be a pointer type')
      funcname_inputtype[i] += ' *'

  if not funcname_stride == 1:
    if len(directives['args']) == 2:
      print('extern %s %s (%s, %s);' % (funcname_rettype, funcname, funcname_inputtype[0], funcname_inputtype[1]))
    elif len(directives['args']) == 3:
      print('extern %s %s (%s, %s, %s);' % (funcname_rettype, funcname, funcname_inputtype[0], funcname_inputtype[1], funcname_inputtype[2]))
    else:
      print('extern %s %s (%s);' % (funcname_rettype, funcname, funcname_inputtype[0]))

  # Print macros.  This branches out to a separate routine if
  # the function takes arguments.
  if not directives['args']:
    print(DEFINES_TEMPLATE % {'funcname': funcname})
    outargs = []
  else:
    outargs = _print_arg_data(funcname, float_flag, funcname_argtype, funcname_stride, directives, all_vals)

  # Print the output variable definitions if necessary.
  for out in outargs:
    print(out)

  # If we have a return value from the function, make sure it is
  # assigned to prevent the compiler from optimizing out the
  # call.
  getret = ''

  if directives['ret']:
    if funcname_argtype != '':
      print('static %s volatile ret;' % funcname_argtype)
      getret = 'ret ='
    else:
      print('static %s volatile ret;' % directives['ret'])
      getret = 'ret ='

  # Test initialization.
  if directives['init']:
    print('#define BENCH_INIT %s' % directives['init'])

  print(EPILOGUE % {'getret': getret, 'func': funcname})


def _print_arg_data(func, float_flag, funcname_argtype, funcname_stride, directives, all_vals):
  """Print argument data

  This is a helper function for gen_source that prints structure and
  values for arguments and their variants and returns output arguments
  if any are found.

  Args:
    func: Function name
    float_flag: True if function is float type
    funcname_argtype: Type for vector variants
    funcname_stride: Vector Length
    directives: A dictionary of directives applicable to this function
    all_vals: A dictionary input values

  Returns:
    Returns a list of definitions for function arguments that act as
    output parameters.
  """
  # First, all of the definitions.  We process writing of
  # CALL_BENCH_FUNC, struct args and also the output arguments
  # together in a single traversal of the arguments list.
  func_args = []
  _func_args = []
  arg_struct = []
  outargs = []
  # Conversion function for each type
  vtable = {
    '__m128d': '_mm_loadu_pd',
    '__m256d': '_mm256_loadu_pd',
    '__m512d': '_mm512_loadu_pd',
    '__m128': '_mm_loadu_ps',
    '__m256': '_mm256_loadu_ps',
    '__m512': '_mm512_loadu_ps',
    'double': '',
    'float': ''
  }

  # For double max_vlen=8, for float max_vlen=16.
  if float_flag == True:
    max_vlen = 16
  else:
    max_vlen = 8

  for arg, i in zip(directives['args'], itertools.count()):
    if arg[0] == '<' and arg[-1] == '>':
      outargs.append('static %s out%d __attribute__((used));' % (funcname_argtype, i))
      func_args.append('&out%d' % i)
      _func_args.append('&out%d' % i)
    else:
      arg_struct.append('  %s arg%d[STRIDE];' % (arg, i))
      func_args.append('%s (variants[v].in[i].arg%d)' %
                       (vtable[funcname_argtype], i))
      _func_args.append('variants[v].in[i].arg%d[0]' % i)

  if funcname_stride == 1:
    print(BENCH_SCALAR_TEMPLATE % {'func': func,
                                   'func_args': ', '.join(_func_args)})
  elif directives['ret'] == '':
    print(BENCH_SCALAR_TEMPLATE % {'func': func,
                                   'func_args': ', '.join(func_args)})
  else:
    print(BENCH_VEC_TEMPLATE % {'func': func, 'func_args': ', '.join(func_args),
                                'defs': funcname_argtype})
  print(STRUCT_TEMPLATE % {'args': '\n'.join(arg_struct)})

  # Now print the values.
  variants = []
  for (k, _vals), i in zip(all_vals.items(), itertools.count()):
    vals = []
    temp_vals = []
    j = 0
    temp_j = 0
    result_v = ['', '', '']
    for _v in _vals:
      nums = _v.split(',')
      for l in range(0, len(nums)):
        result_v[l] = result_v[l] + nums[l].strip() + ','
      j += 1
      temp_j += 1

      if temp_j == funcname_stride:
        final_result = ''
        for l in range(0, len(nums)):
          final_result = final_result + '{' + result_v[l][:-1] + '},'
        temp_vals.append(final_result[:-1])
        temp_j = 0
        result_v = ['', '', '']

      # Make sure amount of test data is multiple of max_vlen
      # to keep data size same for all vector length.
      if j == max_vlen:
        vals.extend(temp_vals)
        temp_vals = []
        j = 0

    out = ['  {%s, 0},' % v for v in vals]

    # Members for the variants structure list that we will
    # print later.
    variants.append('  {"%s", %d, in%d},' % (k, len(vals), i))
    print(ARGS_TEMPLATE % {'argnum': i, 'num_args': len(vals),
                           'args': '\n'.join(out)})

  # Print the variants and the last set of macros.
  print(VARIANTS_TEMPLATE % {'num_variants': len(all_vals),
                             'variants': '\n'.join(variants)})
  return outargs


def _process_directive(d_name, d_val, func_args):
  """Process a directive.

  Evaluate the directive name and value passed and return the
  processed value. This is a helper function for parse_file.

  Args:
    d_name: Name of the directive
    d_val: The string value to process

  Returns:
    The processed value, which may be the string as it is or an object
    that describes the directive.
  """
  # Process the directive values if necessary.  name and ret don't
  # need any processing.
  if d_name.startswith('include'):
    d_val = d_val.split(',')
  elif d_name == 'args':
    d_val = d_val.split(':')
    # Check if args type match
    if not d_val[0] == func_args:
      die("Args mismatch, should be %s, but get %s" % (d_val[0], func_args))

  # Return the values.
  return d_val


def parse_file(func_types):
  """Parse an input file

  Given a function name, open and parse an input file for the function
  and get the necessary parameters for the generated code and the list
  of inputs.

  Args:
    func: The function name

  Returns:
    A tuple of two elements, one a dictionary of directives and the
    other a dictionary of all input values.
  """
  all_vals = {}
  # Valid directives.
  directives = {
    'name': '',
    'args': [],
    'includes': [],
    'include-sources': [],
    'ret': '',
    'init': ''
  }

  func = func_types[-1]
  try:
    with open('../benchtests/libmvec/%s-inputs' % func) as f:
      for line in f:
        # Look for directives and parse it if found.
        if line.startswith('##'):
          try:
            d_name, d_val = line[2:].split(':', 1)
            d_name = d_name.strip()
            d_val = d_val.strip()
            directives[d_name] = _process_directive(d_name, d_val, func_types[1])
          except (IndexError, KeyError):
            die('Invalid directive: %s' % line[2:])

        # Skip blank lines and comments.
        line = line.split('#', 1)[0].rstrip()
        if not line:
          continue

        # Otherwise, we're an input.  Add to the appropriate
        # input set.
        cur_name = directives['name']
        all_vals.setdefault(cur_name, [])
        all_vals[cur_name].append(line)
  except IOError as ex:
    die("Failed to open input file (%s): %s" % (ex.filename, ex.strerror))

  return directives, all_vals


def die(msg):
  """Exit with an error

  Prints an error message to the standard error stream and exits with
  a non-zero status.

  Args:
    msg: The error message to print to standard error
  """
  print('%s\n' % msg, file=sys.stderr)
  sys.exit(os.EX_DATAERR)


def main(args):
  """Main function

  Use the first command line argument as function name and parse its
  input file to generate C source that calls the function repeatedly
  for the input.

  Args:
    args: The command line arguments with the program name dropped

  Returns:
    os.EX_USAGE on error and os.EX_OK on success.
  """
  if len(args) != 1:
    print('Usage: %s <function>' % sys.argv[0])
    return os.EX_USAGE

  func_types = args[0].split('-')
  directives, all_vals = parse_file(func_types)
  gen_source(func_types, directives, all_vals)
  return os.EX_OK


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
