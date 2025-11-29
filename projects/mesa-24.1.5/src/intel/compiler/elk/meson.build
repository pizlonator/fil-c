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

libintel_compiler_elk_files = files(
  'elk_cfg.cpp',
  'elk_cfg.h',
  'elk_clip.h',
  'elk_clip_line.c',
  'elk_clip_point.c',
  'elk_clip_tri.c',
  'elk_clip_unfilled.c',
  'elk_clip_util.c',
  'elk_compile_clip.c',
  'elk_compile_ff_gs.c',
  'elk_compile_sf.c',
  'elk_compiler.c',
  'elk_compiler.h',
  'elk_dead_control_flow.cpp',
  'elk_dead_control_flow.h',
  'elk_debug_recompile.c',
  'elk_disasm.c',
  'elk_disasm.h',
  'elk_disasm_info.c',
  'elk_disasm_info.h',
  'elk_eu.c',
  'elk_eu_compact.c',
  'elk_eu_defines.h',
  'elk_eu_emit.c',
  'elk_eu_opcodes.h',
  'elk_eu.h',
  'elk_eu_util.c',
  'elk_eu_validate.c',
  'elk_fs_bank_conflicts.cpp',
  'elk_fs_builder.h',
  'elk_fs_cmod_propagation.cpp',
  'elk_fs_combine_constants.cpp',
  'elk_fs_copy_propagation.cpp',
  'elk_fs.cpp',
  'elk_fs_cse.cpp',
  'elk_fs_dead_code_eliminate.cpp',
  'elk_fs_generator.cpp',
  'elk_fs.h',
  'elk_fs_live_variables.cpp',
  'elk_fs_live_variables.h',
  'elk_fs_lower_pack.cpp',
  'elk_fs_lower_regioning.cpp',
  'elk_fs_nir.cpp',
  'elk_fs_reg_allocate.cpp',
  'elk_fs_register_coalesce.cpp',
  'elk_fs_saturate_propagation.cpp',
  'elk_fs_sel_peephole.cpp',
  'elk_fs_thread_payload.cpp',
  'elk_fs_validate.cpp',
  'elk_fs_visitor.cpp',
  'elk_inst.h',
  'elk_interpolation_map.c',
  'elk_ir.h',
  'elk_ir_allocator.h',
  'elk_ir_analysis.h',
  'elk_ir_fs.h',
  'elk_ir_performance.h',
  'elk_ir_performance.cpp',
  'elk_ir_vec4.h',
  'elk_isa_info.h',
  'elk_lower_logical_sends.cpp',
  'elk_nir.h',
  'elk_nir.c',
  'elk_nir_analyze_boolean_resolves.c',
  'elk_nir_analyze_ubo_ranges.c',
  'elk_nir_attribute_workarounds.c',
  'elk_nir_lower_alpha_to_coverage.c',
  'elk_nir_lower_cs_intrinsics.c',
  'elk_nir_lower_storage_image.c',
  'elk_nir_options.h',
  'elk_nir_options.c',
  'elk_nir_private.h',
  'elk_packed_float.c',
  'elk_predicated_break.cpp',
  'elk_prim.h',
  'elk_private.h',
  'elk_reg.h',
  'elk_reg_type.c',
  'elk_reg_type.h',
  'elk_schedule_instructions.cpp',
  'elk_shader.cpp',
  'elk_shader.h',
  'elk_simd_selection.cpp',
  'elk_vec4_builder.h',
  'elk_vec4_cmod_propagation.cpp',
  'elk_vec4_copy_propagation.cpp',
  'elk_vec4.cpp',
  'elk_vec4_cse.cpp',
  'elk_vec4_dead_code_eliminate.cpp',
  'elk_vec4_generator.cpp',
  'elk_vec4_gs_visitor.cpp',
  'elk_vec4_gs_visitor.h',
  'elk_vec4.h',
  'elk_vec4_live_variables.cpp',
  'elk_vec4_live_variables.h',
  'elk_vec4_nir.cpp',
  'elk_vec4_gs_nir.cpp',
  'elk_vec4_reg_allocate.cpp',
  'elk_vec4_surface_builder.cpp',
  'elk_vec4_surface_builder.h',
  'elk_vec4_tcs.cpp',
  'elk_vec4_tcs.h',
  'elk_vec4_tes.cpp',
  'elk_vec4_tes.h',
  'elk_vec4_visitor.cpp',
  'elk_vec4_vs_visitor.cpp',
  'elk_vec4_vs.h',
  'elk_vue_map.c',
  'elk_gfx6_gs_visitor.cpp',
  'elk_gfx6_gs_visitor.h',
)

elk_nir_trig = custom_target(
  'elk_nir_trig_workarounds.c',
  input : 'elk_nir_trig_workarounds.py',
  output : 'elk_nir_trig_workarounds.c',
  command : [
    prog_python, '@INPUT@', '-p', dir_compiler_nir,
  ],
  depend_files : nir_algebraic_depends,
  capture : true,
)

libintel_compiler_elk = static_library(
  'intel_compiler_elk',
  [libintel_compiler_elk_files, intel_nir_files, elk_nir_trig, ir_expression_operation_h],
  include_directories : [inc_include, inc_src, inc_intel],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  dependencies : [idep_nir_headers, idep_mesautil, idep_intel_dev],
  build_by_default : false,
)

idep_intel_compiler_elk = declare_dependency(
  link_with : [libintel_compiler_elk],
  dependencies : [
    idep_nir,
  ],
)

if with_tests
  test(
    'intel_compiler_elk_tests',
    executable(
      'intel_compiler_elk_tests',
      files(
        'elk_test_predicated_break.cpp',
        'elk_test_eu_compact.cpp',
        'elk_test_eu_validate.cpp',
        'elk_test_fs_cmod_propagation.cpp',
        'elk_test_fs_combine_constants.cpp',
        'elk_test_fs_copy_propagation.cpp',
        'elk_test_fs_saturate_propagation.cpp',
        'elk_test_simd_selection.cpp',
        'elk_test_vec4_cmod_propagation.cpp',
        'elk_test_vec4_copy_propagation.cpp',
        'elk_test_vec4_dead_code_eliminate.cpp',
        'elk_test_vec4_register_coalesce.cpp',
        'elk_test_vf_float_conversions.cpp',
      ),
      ir_expression_operation_h,
      include_directories : [inc_include, inc_src, inc_intel],
      link_with : [
        libintel_common, libisl
      ],
      dependencies : [idep_gtest, idep_nir, idep_mesautil, idep_intel_dev,
                      idep_intel_compiler_elk],
    ),
    suite : ['intel'],
    protocol : 'gtest',
  )
endif

if with_intel_tools

elk_gram_tab = custom_target(
  'elk_gram.tab.[ch]',
  input : 'elk_gram.y',
  output : ['elk_gram.tab.c', 'elk_gram.tab.h'],
  command : bison_command
)

elk_lex_yy_c = custom_target(
  'elk_lex.yy.c',
  input : 'elk_lex.l',
  output : 'elk_lex.yy.c',
  command : [prog_flex, '-o', '@OUTPUT@', '@INPUT@']
)

elk_asm_tool = executable(
  'elk_asm',
  ['elk_asm_tool.c', elk_gram_tab[0], elk_gram_tab[1], elk_lex_yy_c],
  dependencies : [idep_mesautil, dep_thread, idep_intel_dev, idep_intel_compiler_elk],
  include_directories : [inc_include, inc_src, inc_intel],
  link_with : [libintel_common],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  install : true
)

asm_testcases = [
  ['brw', 'gfx4'],
  ['g4x', 'gfx4.5'],
  ['ilk', 'gfx5'],
  ['snb', 'gfx6'],
  ['ivb', 'gfx7'],
  ['hsw', 'gfx7.5'],
  ['bdw', 'gfx8'],
]

test_runner = find_program('tests/run-test.py')
foreach testcase : asm_testcases
  _gen_name = testcase[0]
  _gen_num = testcase[1]
  _gen_folder = join_paths(meson.current_source_dir(), 'tests',
                           _gen_num.replace('gfx', 'gen'))
  test(
    'elk_asm_' + _gen_num, test_runner,
    args : [
      '--elk_asm', elk_asm_tool,
      '--gen_name', _gen_name,
      '--gen_folder', _gen_folder,
    ],
    suite : 'intel',
  )
endforeach

elk_disasm_tool = executable(
  'elk_disasm',
  files('elk_disasm_tool.c'),
  dependencies : [idep_mesautil, dep_thread, idep_intel_dev, idep_intel_compiler_elk],
  include_directories : [inc_include, inc_src, inc_intel],
  link_with : [libintel_common],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  install : true
)

endif

