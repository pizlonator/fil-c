/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#ifndef ELK_FS_H
#define ELK_FS_H

#include "elk_shader.h"
#include "elk_ir_fs.h"
#include "elk_fs_live_variables.h"
#include "elk_ir_performance.h"
#include "compiler/nir/nir.h"

struct elk_bblock_t;
namespace {
   struct acp_entry;
}

class elk_fs_visitor;

namespace elk {
   /**
    * Register pressure analysis of a shader.  Estimates how many registers
    * are live at any point of the program in GRF units.
    */
   struct register_pressure {
      register_pressure(const elk_fs_visitor *v);
      ~register_pressure();

      analysis_dependency_class
      dependency_class() const
      {
         return (DEPENDENCY_INSTRUCTION_IDENTITY |
                 DEPENDENCY_INSTRUCTION_DATA_FLOW |
                 DEPENDENCY_VARIABLES);
      }

      bool
      validate(const elk_fs_visitor *) const
      {
         /* FINISHME */
         return true;
      }

      unsigned *regs_live_at_ip;
   };
}

struct elk_gs_compile;

namespace elk {
class fs_builder;
}

struct shader_stats {
   const char *scheduler_mode;
   unsigned promoted_constants;
   unsigned spill_count;
   unsigned fill_count;
   unsigned max_register_pressure;
};

/** Register numbers for thread payload fields. */
struct elk_elk_thread_payload {
   /** The number of thread payload registers the hardware will supply. */
   uint8_t num_regs;

   virtual ~elk_elk_thread_payload() = default;

protected:
   elk_elk_thread_payload() : num_regs() {}
};

struct elk_vs_thread_payload : public elk_elk_thread_payload {
   elk_vs_thread_payload(const elk_fs_visitor &v);

   elk_fs_reg urb_handles;
};

struct elk_tcs_thread_payload : public elk_elk_thread_payload {
   elk_tcs_thread_payload(const elk_fs_visitor &v);

   elk_fs_reg patch_urb_output;
   elk_fs_reg primitive_id;
   elk_fs_reg icp_handle_start;
};

struct elk_tes_thread_payload : public elk_elk_thread_payload {
   elk_tes_thread_payload(const elk_fs_visitor &v);

   elk_fs_reg patch_urb_input;
   elk_fs_reg primitive_id;
   elk_fs_reg coords[3];
   elk_fs_reg urb_output;
};

struct elk_gs_thread_payload : public elk_elk_thread_payload {
   elk_gs_thread_payload(elk_fs_visitor &v);

   elk_fs_reg urb_handles;
   elk_fs_reg primitive_id;
   elk_fs_reg instance_id;
   elk_fs_reg icp_handle_start;
};

struct elk_fs_thread_payload : public elk_elk_thread_payload {
   elk_fs_thread_payload(const elk_fs_visitor &v,
                     bool &source_depth_to_render_target,
                     bool &runtime_check_aads_emit);

   uint8_t subspan_coord_reg[2];
   uint8_t source_depth_reg[2];
   uint8_t source_w_reg[2];
   uint8_t aa_dest_stencil_reg[2];
   uint8_t dest_depth_reg[2];
   uint8_t sample_pos_reg[2];
   uint8_t sample_mask_in_reg[2];
   uint8_t depth_w_coef_reg;
   uint8_t barycentric_coord_reg[ELK_BARYCENTRIC_MODE_COUNT][2];
};

struct elk_cs_thread_payload : public elk_elk_thread_payload {
   elk_cs_thread_payload(const elk_fs_visitor &v);

   void load_subgroup_id(const elk::fs_builder &bld, elk_fs_reg &dest) const;

   elk_fs_reg local_invocation_id[3];

protected:
   elk_fs_reg subgroup_id_;
};

class elk_fs_instruction_scheduler;

/**
 * The fragment shader front-end.
 *
 * Translates either GLSL IR or Mesa IR (for ARB_fragment_program) into FS IR.
 */
class elk_fs_visitor : public elk_backend_shader
{
public:
   elk_fs_visitor(const struct elk_compiler *compiler,
              const struct elk_compile_params *params,
              const elk_base_prog_key *key,
              struct elk_stage_prog_data *prog_data,
              const nir_shader *shader,
              unsigned dispatch_width,
              bool needs_register_pressure,
              bool debug_enabled);
   elk_fs_visitor(const struct elk_compiler *compiler,
              const struct elk_compile_params *params,
              const elk_wm_prog_key *key,
              struct elk_wm_prog_data *prog_data,
              const nir_shader *shader,
              unsigned dispatch_width,
              bool needs_register_pressure,
              bool debug_enabled);
   elk_fs_visitor(const struct elk_compiler *compiler,
              const struct elk_compile_params *params,
              struct elk_gs_compile *gs_compile,
              struct elk_gs_prog_data *prog_data,
              const nir_shader *shader,
              bool needs_register_pressure,
              bool debug_enabled);
   void init();
   ~elk_fs_visitor();

   elk_fs_reg vgrf(const glsl_type *const type);
   void import_uniforms(elk_fs_visitor *v);

   void VARYING_PULL_CONSTANT_LOAD(const elk::fs_builder &bld,
                                   const elk_fs_reg &dst,
                                   const elk_fs_reg &surface,
                                   const elk_fs_reg &surface_handle,
                                   const elk_fs_reg &varying_offset,
                                   uint32_t const_offset,
                                   uint8_t alignment,
                                   unsigned components);
   void DEP_RESOLVE_MOV(const elk::fs_builder &bld, int grf);

   bool run_fs(bool allow_spilling, bool do_rep_send);
   bool run_vs();
   bool run_tcs();
   bool run_tes();
   bool run_gs();
   bool run_cs(bool allow_spilling);
   void optimize();
   void allocate_registers(bool allow_spilling);
   uint32_t compute_max_register_pressure();
   void fixup_3src_null_dest();
   void assign_curb_setup();
   void assign_urb_setup();
   void convert_attr_sources_to_hw_regs(elk_fs_inst *inst);
   void assign_vs_urb_setup();
   void assign_tcs_urb_setup();
   void assign_tes_urb_setup();
   void assign_gs_urb_setup();
   bool assign_regs(bool allow_spilling, bool spill_all);
   void assign_regs_trivial();
   void calculate_payload_ranges(unsigned payload_node_count,
                                 int *payload_last_use_ip) const;
   bool split_virtual_grfs();
   bool compact_virtual_grfs();
   void assign_constant_locations();
   bool get_pull_locs(const elk_fs_reg &src, unsigned *out_surf_index,
                      unsigned *out_pull_index);
   bool lower_constant_loads();
   virtual void invalidate_analysis(elk::analysis_dependency_class c);

#ifndef NDEBUG
   void validate();
#else
   void validate() {}
#endif

   bool opt_algebraic();
   bool opt_redundant_halt();
   bool opt_cse();
   bool opt_cse_local(const elk::fs_live_variables &live, elk_bblock_t *block, int &ip);

   bool opt_copy_propagation();
   bool opt_bank_conflicts();
   bool register_coalesce();
   bool compute_to_mrf();
   bool eliminate_find_live_channel();
   bool dead_code_eliminate();
   bool remove_duplicate_mrf_writes();
   bool remove_extra_rounding_modes();

   elk_fs_instruction_scheduler *prepare_scheduler(void *mem_ctx);
   void schedule_instructions_pre_ra(elk_fs_instruction_scheduler *sched,
                                     instruction_scheduler_mode mode);
   void schedule_instructions_post_ra();

   void insert_gfx4_send_dependency_workarounds();
   void insert_gfx4_pre_send_dependency_workarounds(elk_bblock_t *block,
                                                    elk_fs_inst *inst);
   void insert_gfx4_post_send_dependency_workarounds(elk_bblock_t *block,
                                                     elk_fs_inst *inst);
   void vfail(const char *msg, va_list args);
   void fail(const char *msg, ...);
   void limit_dispatch_width(unsigned n, const char *msg);
   bool lower_uniform_pull_constant_loads();
   bool lower_load_payload();
   bool lower_pack();
   bool lower_regioning();
   bool lower_logical_sends();
   bool lower_integer_multiplication();
   bool lower_minmax();
   bool lower_simd_width();
   bool lower_barycentrics();
   bool lower_find_live_channel();
   bool lower_scoreboard();
   bool lower_sub_sat();
   bool opt_combine_constants();

   void emit_repclear_shader();
   void emit_interpolation_setup_gfx4();
   void emit_interpolation_setup_gfx6();
   bool opt_peephole_sel();
   bool opt_saturate_propagation();
   bool opt_cmod_propagation();
   bool opt_zero_samples();

   void set_tcs_invocation_id();

   void emit_alpha_test();
   elk_fs_inst *emit_single_fb_write(const elk::fs_builder &bld,
                                 elk_fs_reg color1, elk_fs_reg color2,
                                 elk_fs_reg src0_alpha, unsigned components);
   void do_emit_fb_writes(int nr_color_regions, bool replicate_alpha);
   void emit_fb_writes();
   void emit_urb_writes(const elk_fs_reg &gs_vertex_count = elk_fs_reg());
   void emit_gs_control_data_bits(const elk_fs_reg &vertex_count);
   void emit_gs_thread_end();
   bool mark_last_urb_write_with_eot();
   void emit_tcs_thread_end();
   void emit_urb_fence();
   void emit_cs_terminate();

   elk_fs_reg interp_reg(const elk::fs_builder &bld, unsigned location,
                     unsigned channel, unsigned comp);
   elk_fs_reg per_primitive_reg(const elk::fs_builder &bld,
                            int location, unsigned comp);

   virtual void dump_instruction_to_file(const elk_backend_instruction *inst, FILE *file) const;
   virtual void dump_instructions_to_file(FILE *file) const;

   const elk_base_prog_key *const key;
   const struct elk_sampler_prog_key_data *key_tex;

   struct elk_gs_compile *gs_compile;

   struct elk_stage_prog_data *prog_data;

   elk_analysis<elk::fs_live_variables, elk_backend_shader> live_analysis;
   elk_analysis<elk::register_pressure, elk_fs_visitor> regpressure_analysis;
   elk_analysis<elk::performance, elk_fs_visitor> performance_analysis;

   /** Number of uniform variable components visited. */
   unsigned uniforms;

   /** Byte-offset for the next available spot in the scratch space buffer. */
   unsigned last_scratch;

   /**
    * Array mapping UNIFORM register numbers to the push parameter index,
    * or -1 if this uniform register isn't being uploaded as a push constant.
    */
   int *push_constant_loc;

   elk_fs_reg frag_depth;
   elk_fs_reg frag_stencil;
   elk_fs_reg sample_mask;
   elk_fs_reg outputs[VARYING_SLOT_MAX];
   elk_fs_reg dual_src_output;
   int first_non_payload_grf;
   /** Either ELK_MAX_GRF or GFX7_MRF_HACK_START */
   unsigned max_grf;

   bool failed;
   char *fail_msg;

   elk_elk_thread_payload *payload_;

   elk_elk_thread_payload &payload() {
      return *this->payload_;
   }

   elk_vs_thread_payload &vs_payload() {
      assert(stage == MESA_SHADER_VERTEX);
      return *static_cast<elk_vs_thread_payload *>(this->payload_);
   }

   elk_tcs_thread_payload &tcs_payload() {
      assert(stage == MESA_SHADER_TESS_CTRL);
      return *static_cast<elk_tcs_thread_payload *>(this->payload_);
   }

   elk_tes_thread_payload &tes_payload() {
      assert(stage == MESA_SHADER_TESS_EVAL);
      return *static_cast<elk_tes_thread_payload *>(this->payload_);
   }

   elk_gs_thread_payload &gs_payload() {
      assert(stage == MESA_SHADER_GEOMETRY);
      return *static_cast<elk_gs_thread_payload *>(this->payload_);
   }

   elk_fs_thread_payload &fs_payload() {
      assert(stage == MESA_SHADER_FRAGMENT);
      return *static_cast<elk_fs_thread_payload *>(this->payload_);
   };

   elk_cs_thread_payload &cs_payload() {
      assert(gl_shader_stage_uses_workgroup(stage));
      return *static_cast<elk_cs_thread_payload *>(this->payload_);
   }

   bool source_depth_to_render_target;
   bool runtime_check_aads_emit;

   elk_fs_reg pixel_x;
   elk_fs_reg pixel_y;
   elk_fs_reg pixel_z;
   elk_fs_reg wpos_w;
   elk_fs_reg pixel_w;
   elk_fs_reg delta_xy[ELK_BARYCENTRIC_MODE_COUNT];
   elk_fs_reg final_gs_vertex_count;
   elk_fs_reg control_data_bits;
   elk_fs_reg invocation_id;

   unsigned grf_used;
   bool spilled_any_registers;
   bool needs_register_pressure;

   const unsigned dispatch_width; /**< 8, 16 or 32 */
   unsigned max_dispatch_width;

   /* The API selected subgroup size */
   unsigned api_subgroup_size; /**< 0, 8, 16, 32 */

   struct shader_stats shader_stats;

   void lower_mul_dword_inst(elk_fs_inst *inst, elk_bblock_t *block);
   void lower_mul_qword_inst(elk_fs_inst *inst, elk_bblock_t *block);
   void lower_mulh_inst(elk_fs_inst *inst, elk_bblock_t *block);

   unsigned workgroup_size() const;

   void debug_optimizer(const nir_shader *nir,
                        const char *pass_name,
                        int iteration, int pass_num) const;
};

/**
 * Return the flag register used in fragment shaders to keep track of live
 * samples.  On Gfx7+ we use f1.0-f1.1 to allow discard jumps in SIMD32
 * dispatch mode, while earlier generations are constrained to f0.1, which
 * limits the dispatch width to SIMD16 for fragment shaders that use discard.
 */
static inline unsigned
sample_mask_flag_subreg(const elk_fs_visitor &s)
{
   assert(s.stage == MESA_SHADER_FRAGMENT);
   return s.devinfo->ver >= 7 ? 2 : 1;
}

/**
 * The fragment shader code generator.
 *
 * Translates FS IR to actual i965 assembly code.
 */
class elk_fs_generator
{
public:
   elk_fs_generator(const struct elk_compiler *compiler,
                const struct elk_compile_params *params,
                struct elk_stage_prog_data *prog_data,
                bool runtime_check_aads_emit,
                gl_shader_stage stage);
   ~elk_fs_generator();

   void enable_debug(const char *shader_name);
   int generate_code(const elk_cfg_t *cfg, int dispatch_width,
                     struct shader_stats shader_stats,
                     const elk::performance &perf,
                     struct elk_compile_stats *stats);
   void add_const_data(void *data, unsigned size);
   const unsigned *get_assembly();

private:
   void fire_fb_write(elk_fs_inst *inst,
                      struct elk_reg payload,
                      struct elk_reg implied_header,
                      GLuint nr);
   void generate_send(elk_fs_inst *inst,
                      struct elk_reg dst,
                      struct elk_reg desc,
                      struct elk_reg payload);
   void generate_fb_write(elk_fs_inst *inst, struct elk_reg payload);
   void generate_cs_terminate(elk_fs_inst *inst, struct elk_reg payload);
   void generate_barrier(elk_fs_inst *inst, struct elk_reg src);
   bool generate_linterp(elk_fs_inst *inst, struct elk_reg dst,
			 struct elk_reg *src);
   void generate_tex(elk_fs_inst *inst, struct elk_reg dst,
                     struct elk_reg surface_index,
                     struct elk_reg sampler_index);
   void generate_ddx(const elk_fs_inst *inst,
                     struct elk_reg dst, struct elk_reg src);
   void generate_ddy(const elk_fs_inst *inst,
                     struct elk_reg dst, struct elk_reg src);
   void generate_scratch_write(elk_fs_inst *inst, struct elk_reg src);
   void generate_scratch_read(elk_fs_inst *inst, struct elk_reg dst);
   void generate_scratch_read_gfx7(elk_fs_inst *inst, struct elk_reg dst);
   void generate_scratch_header(elk_fs_inst *inst, struct elk_reg dst);
   void generate_uniform_pull_constant_load(elk_fs_inst *inst, struct elk_reg dst,
                                            struct elk_reg index,
                                            struct elk_reg offset);
   void generate_varying_pull_constant_load_gfx4(elk_fs_inst *inst,
                                                 struct elk_reg dst,
                                                 struct elk_reg index);

   void generate_set_sample_id(elk_fs_inst *inst,
                               struct elk_reg dst,
                               struct elk_reg src0,
                               struct elk_reg src1);

   void generate_halt(elk_fs_inst *inst);

   void generate_mov_indirect(elk_fs_inst *inst,
                              struct elk_reg dst,
                              struct elk_reg reg,
                              struct elk_reg indirect_byte_offset);

   void generate_shuffle(elk_fs_inst *inst,
                         struct elk_reg dst,
                         struct elk_reg src,
                         struct elk_reg idx);

   void generate_quad_swizzle(const elk_fs_inst *inst,
                              struct elk_reg dst, struct elk_reg src,
                              unsigned swiz);

   bool patch_halt_jumps();

   const struct elk_compiler *compiler;
   const struct elk_compile_params *params;

   const struct intel_device_info *devinfo;

   struct elk_codegen *p;
   struct elk_stage_prog_data * const prog_data;

   unsigned dispatch_width; /**< 8, 16 or 32 */

   exec_list discard_halt_patches;
   bool runtime_check_aads_emit;
   bool debug_flag;
   const char *shader_name;
   gl_shader_stage stage;
   void *mem_ctx;
};

namespace elk {
   elk_fs_reg
   fetch_payload_reg(const elk::fs_builder &bld, uint8_t regs[2],
                     elk_reg_type type = ELK_REGISTER_TYPE_F,
                     unsigned n = 1);

   elk_fs_reg
   fetch_barycentric_reg(const elk::fs_builder &bld, uint8_t regs[2]);

   inline elk_fs_reg
   dynamic_msaa_flags(const struct elk_wm_prog_data *wm_prog_data)
   {
      return elk_fs_reg(UNIFORM, wm_prog_data->msaa_flags_param,
                    ELK_REGISTER_TYPE_UD);
   }

   void
   check_dynamic_msaa_flag(const fs_builder &bld,
                           const struct elk_wm_prog_data *wm_prog_data,
                           enum intel_msaa_flags flag);

   bool
   lower_src_modifiers(elk_fs_visitor *v, elk_bblock_t *block, elk_fs_inst *inst, unsigned i);
}

void elk_shuffle_from_32bit_read(const elk::fs_builder &bld,
                             const elk_fs_reg &dst,
                             const elk_fs_reg &src,
                             uint32_t first_component,
                             uint32_t components);

elk_fs_reg elk_setup_imm_df(const elk::fs_builder &bld,
                    double v);

elk_fs_reg elk_setup_imm_b(const elk::fs_builder &bld,
                   int8_t v);

elk_fs_reg elk_setup_imm_ub(const elk::fs_builder &bld,
                   uint8_t v);

enum elk_barycentric_mode elk_barycentric_mode(nir_intrinsic_instr *intr);

uint32_t elk_fb_write_msg_control(const elk_fs_inst *inst,
                                  const struct elk_wm_prog_data *prog_data);

void elk_compute_urb_setup_index(struct elk_wm_prog_data *wm_prog_data);

bool elk_nir_lower_simd(nir_shader *nir, unsigned dispatch_width);

elk_fs_reg elk_sample_mask_reg(const elk::fs_builder &bld);
void elk_emit_predicate_on_sample_mask(const elk::fs_builder &bld, elk_fs_inst *inst);

int elk_get_subgroup_id_param_index(const intel_device_info *devinfo,
                                    const elk_stage_prog_data *prog_data);

void nir_to_elk(elk_fs_visitor *s);

#endif /* ELK_FS_H */
