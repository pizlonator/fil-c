/*
 * Copyright 2018-2021 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "compiler/nir/nir.h"
#include "util/u_dynarray.h"
#include "shader_enums.h"

struct agx_cf_binding {
   /* Base coefficient register */
   unsigned cf_base;

   /* Slot being bound */
   gl_varying_slot slot;

   /* First component bound.
    *
    * Must be 2 (Z) or 3 (W) if slot == VARYING_SLOT_POS.
    */
   unsigned offset : 2;

   /* Number of components bound */
   unsigned count : 3;

   /* Is smooth shading enabled? If false, flat shading is used */
   bool smooth : 1;

   /* Perspective correct interpolation */
   bool perspective : 1;
};

/* Conservative bound, * 4 due to offsets (TODO: maybe worth eliminating
 * coefficient register aliasing?)
 */
#define AGX_MAX_CF_BINDINGS (VARYING_SLOT_MAX * 4)

struct agx_varyings_fs {
   /* Number of coefficient registers used */
   unsigned nr_cf;

   /* Number of coefficient register bindings */
   unsigned nr_bindings;

   /* Whether gl_FragCoord.z is read */
   bool reads_z;

   /* Coefficient register bindings */
   struct agx_cf_binding bindings[AGX_MAX_CF_BINDINGS];
};

union agx_varyings {
   struct agx_varyings_fs fs;
};

struct agx_interp_info {
   /* Bit masks indexed by I/O location of flat and linear varyings */
   uint64_t flat;
   uint64_t linear;
};

struct agx_shader_info {
   union agx_varyings varyings;

   /* Number of uniforms */
   unsigned push_count;

   /* Local memory allocation in bytes */
   unsigned local_size;

   /* Scratch memory allocation in bytes for main/preamble respectively */
   unsigned scratch_size, preamble_scratch_size;

   /* Size in bytes of the main sahder */
   unsigned main_size;

   /* Does the shader have a preamble? If so, it is at offset preamble_offset.
    * The main shader is at offset main_offset. The preamble is executed first.
    */
   bool has_preamble;
   unsigned preamble_offset, main_offset;

   /* Does the shader read the tilebuffer? */
   bool reads_tib;

   /* Does the shader potentially draw to a nonzero viewport? */
   bool nonzero_viewport;

   /* Does the shader write layer and/or viewport index? Written together */
   bool writes_layer_viewport;

   /* Does the shader control the sample mask? */
   bool writes_sample_mask;

   /* Depth layout, never equal to NONE */
   enum gl_frag_depth_layout depth_layout;

   /* Based only the compiled shader, should tag writes be disabled? This is set
    * based on what is outputted. Note if rasterizer discard is used, that needs
    * to disable tag writes regardless of this flag.
    */
   bool tag_write_disable;

   /* Shader is incompatible with triangle merging */
   bool disable_tri_merging;

   /* Reads draw ID system value */
   bool uses_draw_id;

   /* Reads base vertex/instance */
   bool uses_base_param;

   /* Number of 16-bit registers used by the main shader and preamble
    * respectively.
    */
   unsigned nr_gprs, nr_preamble_gprs;

   /* Output mask set during driver lowering */
   uint64_t outputs;

   /* Immediate data that must be uploaded and mapped as uniform registers */
   unsigned immediate_base_uniform;
   unsigned immediate_size_16;
   uint16_t immediates[512];
};

struct agx_shader_part {
   struct agx_shader_info info;
   void *binary;
   size_t binary_size;
};

#define AGX_MAX_RTS (8)

enum agx_format {
   AGX_FORMAT_I8 = 0,
   AGX_FORMAT_I16 = 1,
   AGX_FORMAT_I32 = 2,
   AGX_FORMAT_F16 = 3,
   AGX_FORMAT_U8NORM = 4,
   AGX_FORMAT_S8NORM = 5,
   AGX_FORMAT_U16NORM = 6,
   AGX_FORMAT_S16NORM = 7,
   AGX_FORMAT_RGB10A2 = 8,
   AGX_FORMAT_SRGBA8 = 10,
   AGX_FORMAT_RG11B10F = 12,
   AGX_FORMAT_RGB9E5 = 13,

   /* Keep last */
   AGX_NUM_FORMATS,
};

struct agx_fs_shader_key {
   /* Normally, access to the tilebuffer must be guarded by appropriate fencing
    * instructions to ensure correct results in the presence of out-of-order
    * hardware optimizations. However, specially dispatched clear shaders are
    * not subject to these conditions and can omit the wait instructions.
    *
    * Must (only) be set for special clear shaders.
    *
    * Must not be used with sample mask writes (including discards) or
    * tilebuffer loads (including blending).
    */
   bool ignore_tib_dependencies;

   /* When dynamic sample shading is used, the fragment shader is wrapped in a
    * loop external to the API shader. This bit indicates that we are compiling
    * inside the sample loop, meaning the execution nesting counter is already
    * zero and must be preserved.
    */
   bool inside_sample_loop;

   /* Base coefficient register. 0 for API shaders but nonzero for FS prolog */
   uint8_t cf_base;
};

struct agx_shader_key {
   /* Number of reserved preamble slots at the start */
   unsigned reserved_preamble;

   /* Does the target GPU need explicit cluster coherency for atomics?
    * Only used on G13X.
    */
   bool needs_g13x_coherency;

   /* Library routines to link against */
   const nir_shader *libagx;

   /* Whether scratch memory is available in the given shader stage */
   bool has_scratch;

   /* Whether we're compiling the helper program used for scratch allocation.
    * This has special register allocation requirements.
    */
   bool is_helper;

   /* Whether the driver supports uploading constants for this shader. If
    * false, constants will not be promoted to uniforms.
    */
   bool promote_constants;

   /* Set if this is a non-monolithic shader that must be linked with additional
    * shader parts before the program can be used. This suppresses omission of
    * `stop` instructions, which the linker must insert instead.
    */
   bool no_stop;

   /* Set if this is a secondary shader part (prolog or epilog). This prevents
    * the compiler from allocating uniform registers. For example, this turns
    * off preambles.
    */
   bool secondary;

   union {
      struct agx_fs_shader_key fs;
   };
};

struct agx_interp_info agx_gather_interp_info(nir_shader *nir);
uint64_t agx_gather_texcoords(nir_shader *nir);

void agx_preprocess_nir(nir_shader *nir, const nir_shader *libagx);
bool agx_nir_lower_discard_zs_emit(nir_shader *s);
bool agx_nir_lower_sample_mask(nir_shader *s);

bool agx_nir_lower_cull_distance_fs(struct nir_shader *s,
                                    unsigned nr_distances);

void agx_compile_shader_nir(nir_shader *nir, struct agx_shader_key *key,
                            struct util_debug_callback *debug,
                            struct agx_shader_part *out);

struct agx_occupancy {
   unsigned max_registers;
   unsigned max_threads;
};

struct agx_occupancy agx_occupancy_for_register_count(unsigned halfregs);
unsigned agx_max_registers_for_occupancy(unsigned occupancy);

static const nir_shader_compiler_options agx_nir_options = {
   .lower_fdiv = true,
   .fuse_ffma16 = true,
   .fuse_ffma32 = true,
   .lower_flrp16 = true,
   .lower_flrp32 = true,
   .lower_fpow = true,
   .lower_fmod = true,
   .lower_bitfield_insert = true,
   .lower_ifind_msb = true,
   .lower_find_lsb = true,
   .lower_uadd_carry = true,
   .lower_usub_borrow = true,
   .lower_fisnormal = true,
   .lower_scmp = true,
   .lower_isign = true,
   .lower_fsign = true,
   .lower_iabs = true,
   .lower_fdph = true,
   .lower_ffract = true,
   .lower_ldexp = true,
   .lower_pack_half_2x16 = true,
   .lower_pack_64_2x32 = true,
   .lower_unpack_half_2x16 = true,
   .lower_extract_byte = true,
   .lower_insert_byte = true,
   .lower_insert_word = true,
   .has_cs_global_id = true,
   .lower_hadd = true,
   .vectorize_io = true,
   .use_interpolated_input_intrinsics = true,
   .has_isub = true,
   .support_16bit_alu = true,
   .max_unroll_iterations = 32,
   .lower_uniforms_to_ubo = true,
   .lower_int64_options =
      (nir_lower_int64_options) ~(nir_lower_iadd64 | nir_lower_imul_2x32_64),
   .lower_doubles_options = (nir_lower_doubles_options)(~0),
   .lower_fquantize2f16 = true,
   .compact_arrays = true,
};
