# Copyright © 2017 Dylan Baker

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

files_libnouveauwinsys = files(
  'nouveau.h',
  'nouveau.c',
  'nouveau_drm_public.h',
  'nouveau_drm_winsys.c',
)

libnouveauwinsys = static_library(
  'nouveauwinsys',
  files_libnouveauwinsys,
  include_directories : [
    inc_src,
    inc_include,
    inc_nouveau_drm,
    inc_gallium,
    inc_gallium_aux,
    inc_gallium_drivers,
  ],
  c_args: [
    cc.get_supported_arguments('-Wno-gnu-variable-sized-type-not-at-end')
  ],
  gnu_symbol_visibility : 'hidden',
  dependencies : [dep_libdrm, idep_mesautil],
)

idep_libnouveauwinsys = declare_dependency(
  include_directories : [
    include_directories('.'),
    inc_nouveau_drm,
  ],
  link_with : [libnouveauwinsys],
)
