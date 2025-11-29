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

intel_nir_files = files(
  'intel_nir.h',
  'intel_nir.c',

  'intel_nir_blockify_uniform_loads.c',
  'intel_nir_clamp_image_1d_2d_array_sizes.c',
  'intel_nir_clamp_per_vertex_loads.c',
  'intel_nir_lower_conversions.c',
  'intel_nir_lower_non_uniform_barycentric_at_sample.c',
  'intel_nir_lower_non_uniform_resource_intel.c',
  'intel_nir_lower_shading_rate_output.c',
  'intel_nir_lower_sparse.c',
  'intel_nir_lower_texture.c',
  'intel_nir_opt_peephole_ffma.c',
  'intel_nir_opt_peephole_imul32x16.c',
  'intel_nir_tcs_workarounds.c',
)

libintel_compiler_brw_files = files(
  'brw_cfg.cpp',
  'brw_cfg.h',
  'brw_compile_gs.cpp',
  'brw_compile_tcs.cpp',
  'brw_compile_vs.cpp',
  'brw_compiler.c',
  'brw_compiler.h',
  'brw_dead_control_flow.cpp',
  'brw_debug_recompile.c',
  'brw_disasm.c',
  'brw_disasm_info.cpp',
  'brw_disasm_info.h',
  'brw_eu.c',
  'brw_eu_compact.c',
  'brw_eu_defines.h',
  'brw_eu_emit.c',
  'brw_eu.h',
  'brw_eu_validate.c',
  'brw_fs_bank_conflicts.cpp',
  'brw_fs_builder.h',
  'brw_fs_cmod_propagation.cpp',
  'brw_fs_combine_constants.cpp',
  'brw_fs_copy_propagation.cpp',
  'brw_fs.cpp',
  'brw_fs_cse.cpp',
  'brw_fs_dead_code_eliminate.cpp',
  'brw_fs_generator.cpp',
  'brw_fs.h',
  'brw_fs_live_variables.cpp',
  'brw_fs_live_variables.h',
  'brw_fs_lower.cpp',
  'brw_fs_lower_dpas.cpp',
  'brw_fs_lower_integer_multiplication.cpp',
  'brw_fs_lower_pack.cpp',
  'brw_fs_lower_regioning.cpp',
  'brw_fs_lower_simd_width.cpp',
  'brw_fs_nir.cpp',
  'brw_fs_opt.cpp',
  'brw_fs_opt_algebraic.cpp',
  'brw_fs_opt_virtual_grfs.cpp',
  'brw_fs_reg_allocate.cpp',
  'brw_fs_register_coalesce.cpp',
  'brw_fs_saturate_propagation.cpp',
  'brw_fs_scoreboard.cpp',
  'brw_fs_sel_peephole.cpp',
  'brw_fs_thread_payload.cpp',
  'brw_fs_validate.cpp',
  'brw_fs_visitor.cpp',
  'brw_fs_workaround.cpp',
  'brw_inst.h',
  'brw_ir.h',
  'brw_ir_allocator.h',
  'brw_ir_analysis.h',
  'brw_ir_fs.h',
  'brw_ir_performance.h',
  'brw_ir_performance.cpp',
  'brw_isa_info.h',
  'brw_lower_logical_sends.cpp',
  'brw_mesh.cpp',
  'brw_nir.h',
  'brw_nir.c',
  'brw_nir_analyze_ubo_ranges.c',
  'brw_nir_lower_cooperative_matrix.c',
  'brw_nir_lower_cs_intrinsics.c',
  'brw_nir_lower_alpha_to_coverage.c',
  'brw_nir_lower_intersection_shader.c',
  'brw_nir_lower_ray_queries.c',
  'brw_nir_lower_rt_intrinsics.c',
  'brw_nir_lower_shader_calls.c',
  'brw_nir_lower_storage_image.c',
  'brw_nir_rt.h',
  'brw_nir_rt.c',
  'brw_nir_rt_builder.h',
  'brw_packed_float.c',
  'brw_predicated_break.cpp',
  'brw_prim.h',
  'brw_private.h',
  'brw_reg.h',
  'brw_reg_type.c',
  'brw_reg_type.h',
  'brw_rt.h',
  'brw_schedule_instructions.cpp',
  'brw_shader.cpp',
  'brw_simd_selection.cpp',
  'brw_vue_map.c',
)

brw_device_sha1_gen_src = custom_target('brw_device_sha1_gen.c',
                                        input : ['brw_device_sha1_gen_c.py', '../dev/intel_device_info.py'],
                                        output : ['brw_device_sha1_gen.c'],
                                        command : [prog_python, '@INPUT0@', '--outdir', meson.current_build_dir()])


brw_nir_trig = custom_target(
  'brw_nir_trig_workarounds.c',
  input : 'brw_nir_trig_workarounds.py',
  output : 'brw_nir_trig_workarounds.c',
  command : [
    prog_python, '@INPUT@', '-p', dir_compiler_nir,
  ],
  depend_files : nir_algebraic_depends,
  capture : true,
)

libintel_compiler_brw = static_library(
  'intel_compiler',
  [libintel_compiler_brw_files, intel_nir_files, brw_nir_trig, ir_expression_operation_h, [brw_device_sha1_gen_src]],
  include_directories : [inc_include, inc_src, inc_intel],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  dependencies : [idep_nir_headers, idep_mesautil, idep_intel_dev],
  build_by_default : false,
)

idep_intel_compiler_brw = declare_dependency(
  link_with : [libintel_compiler_brw],
  dependencies : [
    idep_nir,
  ],
)

# For now this tool is only going to be used by Anv
if get_option('intel-clc') == 'system'
  prog_intel_clc = find_program('intel_clc', native : true)
  dep_prog_intel_clc = []
elif with_intel_clc
  prog_intel_clc = executable(
    'intel_clc',
    [
      'intel_clc.c',
      'brw_kernel.c',

      # Use just the nir_options part of ELK instead of fully linking.
      'elk/elk_nir_options.h',
      'elk/elk_nir_options.c',
    ],
    link_with : [
      libintel_common,libisl,
    ],
    include_directories : [inc_include, inc_src, inc_intel],
    c_args : [pre_args, no_override_init_args],
    link_args : [ld_args_build_id],
    dependencies : [idep_nir, idep_vtn, idep_mesaclc, idep_mesautil, idep_intel_dev,
                    idep_intel_compiler_brw],
    # If we can run host binaries directly, just build intel_clc for the host.
    # Most commonly this happens when doing a cross compile from an x86_64 build
    # machine to an x86 host
    native : not meson.can_run_host_binaries(),
    install : get_option('install-intel-clc'),
  )
  dep_prog_intel_clc = [prog_intel_clc]
endif

if with_tests
  test(
    'intel_compiler_brw_tests',
    executable(
      'intel_compiler_brw_tests',
      files(
        'test_predicated_break.cpp',
        'test_eu_compact.cpp',
        'test_eu_validate.cpp',
        'test_fs_cmod_propagation.cpp',
        'test_fs_combine_constants.cpp',
        'test_fs_copy_propagation.cpp',
        'test_fs_saturate_propagation.cpp',
        'test_fs_scoreboard.cpp',
        'test_simd_selection.cpp',
        'test_vf_float_conversions.cpp',
      ),
      ir_expression_operation_h,
      include_directories : [inc_include, inc_src, inc_intel],
      link_with : [
        libintel_common, libisl
      ],
      dependencies : [idep_gtest, idep_nir, idep_mesautil, idep_intel_dev,
                      idep_intel_compiler_brw],
    ),
    suite : ['intel'],
    protocol : 'gtest',
  )
endif

if with_intel_tools

bison_command = []
if yacc_is_bison
  bison_command = [
    prog_bison, '@INPUT@', '--defines=@OUTPUT1@',
    '--output=@OUTPUT0@'
  ]
else
  bison_command = [
    prog_bison, '-H', '@OUTPUT1@',
    '-o', '@OUTPUT0@', '@INPUT@'
  ]
endif

brw_gram_tab = custom_target(
  'brw_gram.tab.[ch]',
  input : 'brw_gram.y',
  output : ['brw_gram.tab.c', 'brw_gram.tab.h'],
  command : bison_command
)

brw_lex_yy_c = custom_target(
  'brw_lex.yy.c',
  input : 'brw_lex.l',
  output : 'brw_lex.yy.c',
  command : [prog_flex, '-o', '@OUTPUT@', '@INPUT@']
)

brw_asm_tool = executable(
  'brw_asm',
  ['brw_asm_tool.c', brw_gram_tab[0], brw_gram_tab[1], brw_lex_yy_c],
  dependencies : [idep_mesautil, dep_thread, idep_intel_dev,
                  idep_intel_compiler_brw],
  include_directories : [inc_include, inc_src, inc_intel],
  link_with : [libintel_common],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  install : true
)

asm_testcases = [
  ['skl', 'gfx9'],
  ['icl', 'gfx11'],
  ['tgl', 'gfx12'],
  ['dg2', 'gfx12.5'],
]

test_runner = find_program('tests/run-test.py')
foreach testcase : asm_testcases
  _gen_name = testcase[0]
  _gen_num = testcase[1]
  _gen_folder = join_paths(meson.current_source_dir(), 'tests',
                           _gen_num.replace('gfx', 'gen'))
  test(
    'brw_asm_' + _gen_num, test_runner,
    args : [
      '--brw_asm', brw_asm_tool,
      '--gen_name', _gen_name,
      '--gen_folder', _gen_folder,
    ],
    suite : 'intel',
  )
endforeach

brw_disasm_tool = executable(
  'brw_disasm',
  files('brw_disasm_tool.c'),
  dependencies : [idep_mesautil, dep_thread, idep_intel_dev,
                  idep_intel_compiler_brw],
  include_directories : [inc_include, inc_src, inc_intel],
  link_with : [libintel_common],
  c_args : [no_override_init_args],
  gnu_symbol_visibility : 'hidden',
  install : true
)

endif

subdir('elk')
