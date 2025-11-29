libnouveau_ws = static_library(
  'nouveau_ws',
  [
    'nouveau_bo.h',
    'nouveau_bo.c',
    'nouveau_context.h',
    'nouveau_context.c',
    'nouveau_device.h',
    'nouveau_device.c',
    'nouveau_private.h',
    cl_generated,
  ],
  include_directories : [
    inc_include,
    inc_src,
    inc_nouveau_drm,
  ],
  c_args: [ cc.get_supported_arguments('-Wno-gnu-variable-sized-type-not-at-end') ],
  dependencies : [
    dep_libdrm,
    dep_valgrind,
    idep_nvidia_headers,
  ],
  gnu_symbol_visibility : 'hidden',
)

idep_nouveau_ws = declare_dependency(
  link_with: libnouveau_ws,
  include_directories : include_directories('.')
)
