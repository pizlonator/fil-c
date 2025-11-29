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

#ifndef BRW_FS_H
#define BRW_FS_H

#include "brw_cfg.h"
#include "brw_compiler.h"
#include "brw_ir_allocator.h"
#include "brw_ir_fs.h"
#include "brw_fs_live_variables.h"
#include "brw_ir_performance.h"
#include "compiler/nir/nir.h"

struct bblock_t;
namespace {
   struct acp_entry;
}

struct fs_visitor;

namespace brw {
   /**
    * Register pressure analysis of a shader.  Estimates how many registers
    * are live at any point of the program in GRF units.
    */
   struct register_pressure {
      register_pressure(const fs_visitor *v);
      ~register_pressure();

      analysis_dependency_class
      dependency_class() const
      {
         return (DEPENDENCY_INSTRUCTION_IDENTITY |
                 DEPENDENCY_INSTRUCTION_DATA_FLOW |
                 DEPENDENCY_VARIABLES);
      }

      bool
      validate(const fs_visitor *) const
      {
         /* FINISHME */
         return true;
      }

      unsigned *regs_live_at_ip;
   };
}

#define UBO_START ((1 << 16) - 4)

/**
 * Scratch data used when compiling a GLSL geometry shader.
 */
struct brw_gs_compile
{
   struct brw_gs_prog_key key;
   struct intel_vue_map input_vue_map;

   unsigned control_data_bits_per_vertex;
   unsigned control_data_header_size_bits;
};

namespace brw {
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
struct thread_payload {
   /** The number of thread payload registers the hardware will supply. */
   uint8_t num_regs;

   virtual ~thread_payload() = default;

protected:
   thread_payload() : num_regs() {}
};

struct vs_thread_payload : public thread_payload {
   vs_thread_payload(const fs_visitor &v);

   fs_reg urb_handles;
};

struct tcs_thread_payload : public thread_payload {
   tcs_thread_payload(const fs_visitor &v);

   fs_reg patch_urb_output;
   fs_reg primitive_id;
   fs_reg icp_handle_start;
};

struct tes_thread_payload : public thread_payload {
   tes_thread_payload(const fs_visitor &v);

   fs_reg patch_urb_input;
   fs_reg primitive_id;
   fs_reg coords[3];
   fs_reg urb_output;
};

struct gs_thread_payload : public thread_payload {
   gs_thread_payload(fs_visitor &v);

   fs_reg urb_handles;
   fs_reg primitive_id;
   fs_reg instance_id;
   fs_reg icp_handle_start;
};

struct fs_thread_payload : public thread_payload {
   fs_thread_payload(const fs_visitor &v,
                     bool &source_depth_to_render_target);

   uint8_t subspan_coord_reg[2];
   uint8_t source_depth_reg[2];
   uint8_t source_w_reg[2];
   uint8_t aa_dest_stencil_reg[2];
   uint8_t dest_depth_reg[2];
   uint8_t sample_pos_reg[2];
   uint8_t sample_mask_in_reg[2];
   uint8_t barycentric_coord_reg[BRW_BARYCENTRIC_MODE_COUNT][2];

   uint8_t depth_w_coef_reg;
   uint8_t pc_bary_coef_reg;
   uint8_t npc_bary_coef_reg;
   uint8_t sample_offsets_reg;
};

struct cs_thread_payload : public thread_payload {
   cs_thread_payload(const fs_visitor &v);

   void load_subgroup_id(const brw::fs_builder &bld, fs_reg &dest) const;

   fs_reg local_invocation_id[3];

protected:
   fs_reg subgroup_id_;
};

struct task_mesh_thread_payload : public cs_thread_payload {
   task_mesh_thread_payload(fs_visitor &v);

   fs_reg extended_parameter_0;
   fs_reg local_index;
   fs_reg inline_parameter;

   fs_reg urb_output;

   /* URB to read Task memory inputs. Only valid for MESH stage. */
   fs_reg task_urb_input;
};

struct bs_thread_payload : public thread_payload {
   bs_thread_payload(const fs_visitor &v);

   fs_reg global_arg_ptr;
   fs_reg local_arg_ptr;

   void load_shader_type(const brw::fs_builder &bld, fs_reg &dest) const;
};

enum instruction_scheduler_mode {
   SCHEDULE_PRE,
   SCHEDULE_PRE_NON_LIFO,
   SCHEDULE_PRE_LIFO,
   SCHEDULE_POST,
   SCHEDULE_NONE,
};

class instruction_scheduler;

/**
 * The fragment shader front-end.
 *
 * Translates either GLSL IR or Mesa IR (for ARB_fragment_program) into FS IR.
 */
struct fs_visitor
{
public:
   fs_visitor(const struct brw_compiler *compiler,
              const struct brw_compile_params *params,
              const brw_base_prog_key *key,
              struct brw_stage_prog_data *prog_data,
              const nir_shader *shader,
              unsigned dispatch_width,
              bool needs_register_pressure,
              bool debug_enabled);
   fs_visitor(const struct brw_compiler *compiler,
              const struct brw_compile_params *params,
              const brw_wm_prog_key *key,
              struct brw_wm_prog_data *prog_data,
              const nir_shader *shader,
              unsigned dispatch_width,
              unsigned num_polygons,
              bool needs_register_pressure,
              bool debug_enabled);
   fs_visitor(const struct brw_compiler *compiler,
              const struct brw_compile_params *params,
              struct brw_gs_compile *gs_compile,
              struct brw_gs_prog_data *prog_data,
              const nir_shader *shader,
              bool needs_register_pressure,
              bool debug_enabled);
   void init();
   ~fs_visitor();

   void import_uniforms(fs_visitor *v);

   void VARYING_PULL_CONSTANT_LOAD(const brw::fs_builder &bld,
                                   const fs_reg &dst,
                                   const fs_reg &surface,
                                   const fs_reg &surface_handle,
                                   const fs_reg &varying_offset,
                                   uint32_t const_offset,
                                   uint8_t alignment,
                                   unsigned components);

   bool run_fs(bool allow_spilling, bool do_rep_send);
   bool run_vs();
   bool run_tcs();
   bool run_tes();
   bool run_gs();
   bool run_cs(bool allow_spilling);
   bool run_bs(bool allow_spilling);
   bool run_task(bool allow_spilling);
   bool run_mesh(bool allow_spilling);
   void allocate_registers(bool allow_spilling);
   uint32_t compute_max_register_pressure();
   void assign_curb_setup();
   void assign_urb_setup();
   void convert_attr_sources_to_hw_regs(fs_inst *inst);
   void assign_vs_urb_setup();
   void assign_tcs_urb_setup();
   void assign_tes_urb_setup();
   void assign_gs_urb_setup();
   bool assign_regs(bool allow_spilling, bool spill_all);
   void assign_regs_trivial();
   void calculate_payload_ranges(unsigned payload_node_count,
                                 int *payload_last_use_ip) const;
   void assign_constant_locations();
   bool get_pull_locs(const fs_reg &src, unsigned *out_surf_index,
                      unsigned *out_pull_index);
   void invalidate_analysis(brw::analysis_dependency_class c);

   instruction_scheduler *prepare_scheduler(void *mem_ctx);
   void schedule_instructions_pre_ra(instruction_scheduler *sched,
                                     instruction_scheduler_mode mode);
   void schedule_instructions_post_ra();

   void vfail(const char *msg, va_list args);
   void fail(const char *msg, ...);
   void limit_dispatch_width(unsigned n, const char *msg);

   void emit_repclear_shader();
   void emit_interpolation_setup();

   void set_tcs_invocation_id();

   fs_inst *emit_single_fb_write(const brw::fs_builder &bld,
                                 fs_reg color1, fs_reg color2,
                                 fs_reg src0_alpha, unsigned components);
   void do_emit_fb_writes(int nr_color_regions, bool replicate_alpha);
   void emit_fb_writes();
   void emit_urb_writes(const fs_reg &gs_vertex_count = fs_reg());
   void emit_gs_control_data_bits(const fs_reg &vertex_count);
   fs_reg gs_urb_channel_mask(const fs_reg &dword_index);
   fs_reg gs_urb_per_slot_dword_index(const fs_reg &vertex_count);
   void emit_gs_thread_end();
   bool mark_last_urb_write_with_eot();
   void emit_tcs_thread_end();
   void emit_urb_fence();
   void emit_cs_terminate();

   fs_reg interp_reg(const brw::fs_builder &bld, unsigned location,
                     unsigned channel, unsigned comp);
   fs_reg per_primitive_reg(const brw::fs_builder &bld,
                            int location, unsigned comp);

   void dump_instruction_to_file(const fs_inst *inst, FILE *file) const;
   void dump_instructions_to_file(FILE *file) const;

   /* Convenience functions based on the above. */
   void dump_instruction(const fs_inst *inst, FILE *file = stderr) const {
      dump_instruction_to_file(inst, file);
   }
   void dump_instructions(const char *name = nullptr) const;

   void calculate_cfg();

   const struct brw_compiler *compiler;
   void *log_data; /* Passed to compiler->*_log functions */

   const struct intel_device_info * const devinfo;
   const nir_shader *nir;

   /** ralloc context for temporary data used during compile */
   void *mem_ctx;

   /** List of fs_inst. */
   exec_list instructions;

   cfg_t *cfg;

   gl_shader_stage stage;
   bool debug_enabled;

   brw::simple_allocator alloc;

   const brw_base_prog_key *const key;

   struct brw_gs_compile *gs_compile;

   struct brw_stage_prog_data *prog_data;

   brw_analysis<brw::fs_live_variables, fs_visitor> live_analysis;
   brw_analysis<brw::register_pressure, fs_visitor> regpressure_analysis;
   brw_analysis<brw::performance, fs_visitor> performance_analysis;
   brw_analysis<brw::idom_tree, fs_visitor> idom_analysis;

   /** Number of uniform variable components visited. */
   unsigned uniforms;

   /** Byte-offset for the next available spot in the scratch space buffer. */
   unsigned last_scratch;

   /**
    * Array mapping UNIFORM register numbers to the push parameter index,
    * or -1 if this uniform register isn't being uploaded as a push constant.
    */
   int *push_constant_loc;

   fs_reg frag_depth;
   fs_reg frag_stencil;
   fs_reg sample_mask;
   fs_reg outputs[VARYING_SLOT_MAX];
   fs_reg dual_src_output;
   int first_non_payload_grf;

   bool failed;
   char *fail_msg;

   thread_payload *payload_;

   thread_payload &payload() {
      return *this->payload_;
   }

   vs_thread_payload &vs_payload() {
      assert(stage == MESA_SHADER_VERTEX);
      return *static_cast<vs_thread_payload *>(this->payload_);
   }

   tcs_thread_payload &tcs_payload() {
      assert(stage == MESA_SHADER_TESS_CTRL);
      return *static_cast<tcs_thread_payload *>(this->payload_);
   }

   tes_thread_payload &tes_payload() {
      assert(stage == MESA_SHADER_TESS_EVAL);
      return *static_cast<tes_thread_payload *>(this->payload_);
   }

   gs_thread_payload &gs_payload() {
      assert(stage == MESA_SHADER_GEOMETRY);
      return *static_cast<gs_thread_payload *>(this->payload_);
   }

   fs_thread_payload &fs_payload() {
      assert(stage == MESA_SHADER_FRAGMENT);
      return *static_cast<fs_thread_payload *>(this->payload_);
   };

   cs_thread_payload &cs_payload() {
      assert(gl_shader_stage_uses_workgroup(stage));
      return *static_cast<cs_thread_payload *>(this->payload_);
   }

   task_mesh_thread_payload &task_mesh_payload() {
      assert(stage == MESA_SHADER_TASK || stage == MESA_SHADER_MESH);
      return *static_cast<task_mesh_thread_payload *>(this->payload_);
   }

   bs_thread_payload &bs_payload() {
      assert(stage >= MESA_SHADER_RAYGEN && stage <= MESA_SHADER_CALLABLE);
      return *static_cast<bs_thread_payload *>(this->payload_);
   }

   bool source_depth_to_render_target;

   fs_reg pixel_x;
   fs_reg pixel_y;
   fs_reg pixel_z;
   fs_reg wpos_w;
   fs_reg pixel_w;
   fs_reg delta_xy[BRW_BARYCENTRIC_MODE_COUNT];
   fs_reg final_gs_vertex_count;
   fs_reg control_data_bits;
   fs_reg invocation_id;

   unsigned grf_used;
   bool spilled_any_registers;
   bool needs_register_pressure;

   const unsigned dispatch_width; /**< 8, 16 or 32 */
   const unsigned max_polygons;
   unsigned max_dispatch_width;

   /* The API selected subgroup size */
   unsigned api_subgroup_size; /**< 0, 8, 16, 32 */

   struct shader_stats shader_stats;

   unsigned workgroup_size() const;

   void debug_optimizer(const nir_shader *nir,
                        const char *pass_name,
                        int iteration, int pass_num) const;
};

/**
 * Return the flag register used in fragment shaders to keep track of live
 * samples.  On Gfx7+ we use f1.0-f1.1 to allow discard jumps in SIMD32
 * dispatch mode.
 */
static inline unsigned
sample_mask_flag_subreg(const fs_visitor &s)
{
   assert(s.stage == MESA_SHADER_FRAGMENT);
   return 2;
}

/**
 * The fragment shader code generator.
 *
 * Translates FS IR to actual i965 assembly code.
 */
class fs_generator
{
public:
   fs_generator(const struct brw_compiler *compiler,
                const struct brw_compile_params *params,
                struct brw_stage_prog_data *prog_data,
                gl_shader_stage stage);
   ~fs_generator();

   void enable_debug(const char *shader_name);
   int generate_code(const cfg_t *cfg, int dispatch_width,
                     struct shader_stats shader_stats,
                     const brw::performance &perf,
                     struct brw_compile_stats *stats,
                     unsigned max_polygons = 0);
   void add_const_data(void *data, unsigned size);
   void add_resume_sbt(unsigned num_resume_shaders, uint64_t *sbt);
   const unsigned *get_assembly();

private:
   void generate_send(fs_inst *inst,
                      struct brw_reg dst,
                      struct brw_reg desc,
                      struct brw_reg ex_desc,
                      struct brw_reg payload,
                      struct brw_reg payload2);
   void generate_barrier(fs_inst *inst, struct brw_reg src);
   void generate_ddx(const fs_inst *inst,
                     struct brw_reg dst, struct brw_reg src);
   void generate_ddy(const fs_inst *inst,
                     struct brw_reg dst, struct brw_reg src);
   void generate_scratch_header(fs_inst *inst, struct brw_reg dst);

   void generate_halt(fs_inst *inst);

   void generate_mov_indirect(fs_inst *inst,
                              struct brw_reg dst,
                              struct brw_reg reg,
                              struct brw_reg indirect_byte_offset);

   void generate_shuffle(fs_inst *inst,
                         struct brw_reg dst,
                         struct brw_reg src,
                         struct brw_reg idx);

   void generate_quad_swizzle(const fs_inst *inst,
                              struct brw_reg dst, struct brw_reg src,
                              unsigned swiz);

   bool patch_halt_jumps();

   const struct brw_compiler *compiler;
   const struct brw_compile_params *params;

   const struct intel_device_info *devinfo;

   struct brw_codegen *p;
   struct brw_stage_prog_data * const prog_data;

   unsigned dispatch_width; /**< 8, 16 or 32 */

   exec_list discard_halt_patches;
   bool debug_flag;
   const char *shader_name;
   gl_shader_stage stage;
   void *mem_ctx;
};

namespace brw {
   fs_reg
   fetch_payload_reg(const brw::fs_builder &bld, uint8_t regs[2],
                     brw_reg_type type = BRW_REGISTER_TYPE_F,
                     unsigned n = 1);

   fs_reg
   fetch_barycentric_reg(const brw::fs_builder &bld, uint8_t regs[2]);

   inline fs_reg
   dynamic_msaa_flags(const struct brw_wm_prog_data *wm_prog_data)
   {
      return fs_reg(UNIFORM, wm_prog_data->msaa_flags_param,
                    BRW_REGISTER_TYPE_UD);
   }

   void
   check_dynamic_msaa_flag(const fs_builder &bld,
                           const struct brw_wm_prog_data *wm_prog_data,
                           enum intel_msaa_flags flag);

   bool
   lower_src_modifiers(fs_visitor *v, bblock_t *block, fs_inst *inst, unsigned i);
}

void shuffle_from_32bit_read(const brw::fs_builder &bld,
                             const fs_reg &dst,
                             const fs_reg &src,
                             uint32_t first_component,
                             uint32_t components);

enum brw_barycentric_mode brw_barycentric_mode(const struct brw_wm_prog_key *key,
                                               nir_intrinsic_instr *intr);

uint32_t brw_fb_write_msg_control(const fs_inst *inst,
                                  const struct brw_wm_prog_data *prog_data);

void brw_compute_urb_setup_index(struct brw_wm_prog_data *wm_prog_data);

bool brw_nir_lower_simd(nir_shader *nir, unsigned dispatch_width);

fs_reg brw_sample_mask_reg(const brw::fs_builder &bld);
void brw_emit_predicate_on_sample_mask(const brw::fs_builder &bld, fs_inst *inst);

int brw_get_subgroup_id_param_index(const intel_device_info *devinfo,
                                    const brw_stage_prog_data *prog_data);

void nir_to_brw(fs_visitor *s);

#ifndef NDEBUG
void brw_fs_validate(const fs_visitor &s);
#else
static inline void brw_fs_validate(const fs_visitor &s) {}
#endif

void brw_fs_optimize(fs_visitor &s);

bool brw_fs_lower_3src_null_dest(fs_visitor &s);
bool brw_fs_lower_alu_restrictions(fs_visitor &s);
bool brw_fs_lower_barycentrics(fs_visitor &s);
bool brw_fs_lower_constant_loads(fs_visitor &s);
bool brw_fs_lower_derivatives(fs_visitor &s);
bool brw_fs_lower_dpas(fs_visitor &s);
bool brw_fs_lower_find_live_channel(fs_visitor &s);
bool brw_fs_lower_integer_multiplication(fs_visitor &s);
bool brw_fs_lower_logical_sends(fs_visitor &s);
bool brw_fs_lower_pack(fs_visitor &s);
bool brw_fs_lower_load_payload(fs_visitor &s);
bool brw_fs_lower_regioning(fs_visitor &s);
bool brw_fs_lower_scoreboard(fs_visitor &s);
bool brw_fs_lower_sends_overlapping_payload(fs_visitor &s);
bool brw_fs_lower_simd_width(fs_visitor &s);
bool brw_fs_lower_sub_sat(fs_visitor &s);
bool brw_fs_lower_uniform_pull_constant_loads(fs_visitor &s);
void brw_fs_lower_vgrfs_to_fixed_grfs(fs_visitor &s);

bool brw_fs_opt_algebraic(fs_visitor &s);
bool brw_fs_opt_bank_conflicts(fs_visitor &s);
bool brw_fs_opt_cmod_propagation(fs_visitor &s);
bool brw_fs_opt_combine_constants(fs_visitor &s);
bool brw_fs_opt_compact_virtual_grfs(fs_visitor &s);
bool brw_fs_opt_copy_propagation(fs_visitor &s);
bool brw_fs_opt_cse(fs_visitor &s);
bool brw_fs_opt_dead_code_eliminate(fs_visitor &s);
bool brw_fs_opt_dead_control_flow_eliminate(fs_visitor &s);
bool brw_fs_opt_eliminate_find_live_channel(fs_visitor &s);
bool brw_fs_opt_peephole_sel(fs_visitor &s);
bool brw_fs_opt_predicated_break(fs_visitor &s);
bool brw_fs_opt_register_coalesce(fs_visitor &s);
bool brw_fs_opt_remove_extra_rounding_modes(fs_visitor &s);
bool brw_fs_opt_remove_redundant_halts(fs_visitor &s);
bool brw_fs_opt_saturate_propagation(fs_visitor &s);
bool brw_fs_opt_split_sends(fs_visitor &s);
bool brw_fs_opt_split_virtual_grfs(fs_visitor &s);
bool brw_fs_opt_zero_samples(fs_visitor &s);

bool brw_fs_workaround_emit_dummy_mov_instruction(fs_visitor &s);
bool brw_fs_workaround_memory_fence_before_eot(fs_visitor &s);
bool brw_fs_workaround_nomask_control_flow(fs_visitor &s);

/* Helpers. */
unsigned brw_fs_get_lowered_simd_width(const fs_visitor *shader,
                                       const fs_inst *inst);

#endif /* BRW_FS_H */
