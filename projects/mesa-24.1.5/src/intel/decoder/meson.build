# Copyright © 2017 Intel Corporation
# SPDX-License-Identifier: MIT

libintel_decoder_files = files(
  'intel_decoder.h',
  )

libintel_decoder_deps = [
  idep_intel_dev
]

if not dep_expat.found()
  libintel_decoder_files += files(
    'intel_batch_decoder_stub.c',
  )
else
  libintel_decoder_deps += dep_expat
  libintel_decoder_files += files(
    'intel_decoder.c',
    'intel_batch_decoder.c',
  )
endif

libintel_decoder_brw = static_library(
  'intel_decoder_brw',
  [libintel_decoder_files, 'intel_batch_decoder_brw.c', genX_xml_h, sha1_h],
  include_directories : [inc_include, inc_src, inc_intel],
  c_args : [no_override_init_args, sse2_args],
  gnu_symbol_visibility : 'hidden',
  dependencies : libintel_decoder_deps,
  build_by_default : false,
)

idep_intel_decoder_brw = declare_dependency(
  link_with : [libintel_decoder_brw],
  dependencies : libintel_decoder_deps,
)

libintel_decoder_elk = static_library(
  'intel_decoder_elk',
  [libintel_decoder_files, 'intel_batch_decoder_elk.c', genX_xml_h, sha1_h],
  include_directories : [inc_include, inc_src, inc_intel],
  c_args : [no_override_init_args, sse2_args],
  gnu_symbol_visibility : 'hidden',
  dependencies : libintel_decoder_deps,
  build_by_default : false,
)

idep_intel_decoder_elk = declare_dependency(
  link_with : [libintel_decoder_elk],
  dependencies : libintel_decoder_deps,
)

if with_tests and not with_platform_android
  gentest_xml = 'tests/gentest.xml'
  _name = 'gentest_pack.h'
  gentest_pack = custom_target(
    _name,
    input : [gen_pack_header_py, gentest_xml],
    output : _name,
    command : [prog_python, '@INPUT@'],
    capture : true,
    depend_files: gen_pack_header_deps
  )

  genxml_path = join_paths(meson.current_source_dir(),
                           '@0@'.format(gentest_xml))

  test(
    'genxml_test',
    executable(
      'genxml_test',
      ['tests/genxml_test.c', gentest_pack],
      include_directories : [
        inc_include,
        inc_src,
        inc_intel
      ],
      dependencies : [
        idep_libintel_common,
        idep_intel_decoder_brw,
        idep_intel_decoder_elk,
        idep_mesautil,
        idep_intel_dev,
        idep_genxml,
      ],
      c_args : [
        '-DGENXML_DIR="@0@"'.format(fs.parent(genxml_path)),
        '-DGENXML_FILE="@0@"'.format(fs.name(genxml_path)),
      ],
    ),
    args : ['-quiet'],
    suite : ['intel'],
  )
endif
