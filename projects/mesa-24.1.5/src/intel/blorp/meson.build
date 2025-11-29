# Copyright © 2017 Intel Corporation

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

files_libblorp = files(
  'blorp.c',
  'blorp.h',
  'blorp_blit.c',
  'blorp_clear.c',
  'blorp_nir_builder.h',
  'blorp_priv.h',
)

files_blorp_brw = files(
  'blorp_genX_exec_brw.h',
  'blorp_brw.c',
)

files_blorp_elk = files(
  'blorp_genX_exec_elk.h',
  'blorp_elk.c',
)

libblorp = static_library(
  'blorp',
  [files_libblorp, files_blorp_brw],
  include_directories : [inc_include, inc_src, inc_intel],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  dependencies : [idep_nir_headers, idep_genxml, idep_mesautil, idep_intel_dev],
  build_by_default: false,
)

idep_intel_blorp = declare_dependency(
  link_with: libblorp,
)

libblorp_elk = static_library(
  'blorp_elk',
  [files_libblorp, files_blorp_elk],
  include_directories : [inc_include, inc_src, inc_intel],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  dependencies : [idep_nir_headers, idep_genxml, idep_mesautil, idep_intel_dev],
  build_by_default: true, # FIXME XXX
)

idep_intel_blorp_elk = declare_dependency(
  link_with: libblorp_elk,
)

