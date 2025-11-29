nvk_classes = [
  'cl902d',
  'cl9039',
  'cl906f',
  'cl9097',
  'cl90b5',
  'cl90c0',
  'cla0b5',
  'cla097',
  'cla297',
  'clb097',
  'clb0c0',
  'clb197',
  'clb1c0',
  'clc097',
  'clc1b5',
  'cla0c0',
  'clc0c0',
  'clc397',
  'clc3c0',
  'clc597',
  'clc5c0',
  'clc697',
  'clc6c0',
  'clc7c0',
]

hwref_gens = [
  'maxwell/gm107',
  'pascal/gp100',
  'hopper/gh100',
  'turing/tu102',
]

nvk_cl_header_depend_files = [
  files('class_parser.py')
]

cl_generated = []
foreach cl : nvk_classes
  cl_generated += custom_target(
    cl + '.h',
    input : ['class_parser.py', 'nvidia/classes/'+cl+'.h'],
    output : ['nvk_'+cl+'.h', 'nvk_'+cl+'.c'],
    command : [prog_python, '@INPUT0@', '--in-h', '@INPUT1@',
               '--out-h', '@OUTPUT0@', '--out-c', '@OUTPUT1@'],
    depend_files: nvk_cl_header_depend_files,
  )
endforeach

_libnvidia_headers = static_library(
  'nvidia_headers_c',
  ['nv_push.c', cl_generated],
  include_directories : ['.', 'nvidia/classes', inc_include, inc_src],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
)

idep_nvidia_headers = declare_dependency(
  include_directories : include_directories('.', 'nvidia/classes'),
  sources : cl_generated,
  link_with : _libnvidia_headers,
)

executable(
  'nv_push_dump',
  'nv_push_dump.c',
  dependencies : [ idep_mesautil, idep_nvidia_headers ],
  build_by_default : with_tools.contains('nouveau'),
  install : with_tools.contains('nouveau'),
)

# Only generate Rust bindings for NVK
if with_nouveau_vk
  add_languages('rust', required: true)
  rust = import('rust')

  cl_rs_generated = []
  foreach cl : nvk_classes
    cl_rs_generated += custom_target(
      'nvh_classes_' + cl + '.rs',
      input : ['class_parser.py', 'nvidia/classes/'+cl+'.h'],
      output : ['nvh_classes_'+cl+'.rs'],
      command : [prog_python, '@INPUT0@', '--in-h', '@INPUT1@',
                 '--out-rs', '@OUTPUT0@'],
      depend_files: nvk_cl_header_depend_files,
    )

    fs = import('fs')
    if cl.endswith('c0') and fs.is_file('nvidia/classes/'+cl+'qmd.h')
      cl_rs_generated += custom_target(
        'nvh_classes_' + cl + '_qmd.rs',
        input : ['struct_parser.py', 'nvidia/classes/'+cl+'qmd.h'],
        output : ['nvh_classes_'+cl+'_qmd.rs'],
        command : [prog_python, '@INPUT0@', '--in-h', '@INPUT1@',
                   '--out-rs', '@OUTPUT0@']
      )
    endif

    if cl.endswith('97')
      cl_rs_generated += custom_target(
        'nvh_classes_' + cl + '_tex.rs',
        input : ['struct_parser.py', 'nvidia/classes/'+cl+'tex.h'],
        output : ['nvh_classes_'+cl+'_tex.rs'],
        command : [prog_python, '@INPUT0@', '--in-h', '@INPUT1@',
                   '--out-rs', '@OUTPUT0@']
      )
    endif
  endforeach

  fs = import('fs')
  foreach family_gen : hwref_gens
    family = fs.parent(family_gen)
    gen = fs.name(family_gen)
    cl_rs_generated += rust.bindgen(
      input : ['nvidia/hwref/'+family+'/'+gen+'/dev_mmu.h'],
      output : 'nvh_hwref_'+gen+'_mmu.rs',
      args : ['--allowlist-var', 'NV_MMU_.*'],
    )
  endforeach

  _nvidia_headers_lib_rs = custom_target(
    'lib.rs',
    input : ['lib_rs_gen.py'],
    output : ['lib.rs'],
    command : [prog_python, '@INPUT0@', '--out-rs', '@OUTPUT0@',
               cl_rs_generated]
  )

  _nvidia_headers_rs = static_library(
    'nvidia_headers',
    _nvidia_headers_lib_rs,
    gnu_symbol_visibility : 'hidden',
    rust_abi : 'rust',
  )

  idep_nvidia_headers_rs = declare_dependency(
    link_with : _nvidia_headers_rs,
  )
endif
