/*
 * Copyright © 2010 - 2015 Intel Corporation
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
 */

#ifndef ELK_COMPILER_H
#define ELK_COMPILER_H

#include <stdio.h>
#include "c11/threads.h"
#include "dev/intel_device_info.h"
#include "isl/isl.h"
#include "util/macros.h"
#include "util/mesa-sha1.h"
#include "util/enum_operators.h"
#include "util/ralloc.h"
#include "util/u_math.h"
#include "elk_isa_info.h"
#include "../intel_shader_enums.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ra_regs;
struct nir_shader;
struct shader_info;

struct nir_shader_compiler_options;
typedef struct nir_shader nir_shader;

#define REG_CLASS_COUNT 20

struct elk_compiler {
   const struct intel_device_info *devinfo;

   /* This lock must be taken if the compiler is to be modified in any way,
    * including adding something to the ralloc child list.
    */
   mtx_t mutex;

   struct elk_isa_info isa;

   struct {
      struct ra_regs *regs;

      /**
       * Array of the ra classes for the unaligned contiguous register
       * block sizes used.
       */
      struct ra_class **classes;
   } vec4_reg_set;

   struct {
      struct ra_regs *regs;

      /**
       * Array of the ra classes for the unaligned contiguous register
       * block sizes used, indexed by register size.
       */
      struct ra_class *classes[REG_CLASS_COUNT];

      /**
       * ra class for the aligned barycentrics we use for PLN, which doesn't
       * appear in *classes.
       */
      struct ra_class *aligned_bary_class;
   } fs_reg_sets[3];

   void (*shader_debug_log)(void *, unsigned *id, const char *str, ...) PRINTFLIKE(3, 4);
   void (*shader_perf_log)(void *, unsigned *id, const char *str, ...) PRINTFLIKE(3, 4);

   bool scalar_stage[MESA_ALL_SHADER_STAGES];
   struct nir_shader_compiler_options *nir_options[MESA_ALL_SHADER_STAGES];

   /**
    * Apply workarounds for SIN and COS output range problems.
    * This can negatively impact performance.
    */
   bool precise_trig;

   /**
    * Is 3DSTATE_CONSTANT_*'s Constant Buffer 0 relative to Dynamic State
    * Base Address?  (If not, it's a normal GPU address.)
    */
   bool constant_buffer_0_is_relative;

   /**
    * Whether or not the driver supports NIR shader constants.  This controls
    * whether nir_opt_large_constants will be run.
    */
   bool supports_shader_constants;

   /**
    * Whether indirect UBO loads should use the sampler or go through the
    * data/constant cache.  For the sampler, UBO surface states have to be set
    * up with VK_FORMAT_R32G32B32A32_FLOAT whereas if it's going through the
    * constant or data cache, UBOs must use VK_FORMAT_RAW.
    */
   bool indirect_ubos_use_sampler;

   /**
    * Calling the ra_allocate function after each register spill can take
    * several minutes. This option speeds up shader compilation by spilling
    * more registers after the ra_allocate failure. Required for
    * Cyberpunk 2077, which uses a watchdog thread to terminate the process
    * in case the render thread hasn't responded within 2 minutes.
    */
   int spilling_rate;
};

#define elk_shader_debug_log(compiler, data, fmt, ... ) do {    \
   static unsigned id = 0;                                      \
   compiler->shader_debug_log(data, &id, fmt, ##__VA_ARGS__);   \
} while (0)

#define elk_shader_perf_log(compiler, data, fmt, ... ) do {     \
   static unsigned id = 0;                                      \
   compiler->shader_perf_log(data, &id, fmt, ##__VA_ARGS__);    \
} while (0)

/**
 * We use a constant subgroup size of 32.  It really only needs to be a
 * maximum and, since we do SIMD32 for compute shaders in some cases, it
 * needs to be at least 32.  SIMD8 and SIMD16 shaders will still claim a
 * subgroup size of 32 but will act as if 16 or 24 of those channels are
 * disabled.
 */
#define ELK_SUBGROUP_SIZE 32

/**
 * Program key structures.
 *
 * When drawing, we look for the currently bound shaders in the program
 * cache.  This is essentially a hash table lookup, and these are the keys.
 *
 * Sometimes OpenGL features specified as state need to be simulated via
 * shader code, due to a mismatch between the API and the hardware.  This
 * is often referred to as "non-orthagonal state" or "NOS".  We store NOS
 * in the program key so it's considered when searching for a program.  If
 * we haven't seen a particular combination before, we have to recompile a
 * new specialized version.
 *
 * Shader compilation should not look up state in gl_context directly, but
 * instead use the copy in the program key.  This guarantees recompiles will
 * happen correctly.
 *
 *  @{
 */

enum PACKED elk_gfx6_gather_sampler_wa {
   ELK_WA_SIGN = 1,      /* whether we need to sign extend */
   ELK_WA_8BIT = 2,      /* if we have an 8bit format needing wa */
   ELK_WA_16BIT = 4,     /* if we have a 16bit format needing wa */
};

#define ELK_MAX_SAMPLERS 32

/* Provide explicit padding for each member, to ensure that the compiler
 * initializes every bit in the shader cache keys.  The keys will be compared
 * with memcmp.
 */
PRAGMA_DIAGNOSTIC_PUSH
PRAGMA_DIAGNOSTIC_ERROR(-Wpadded)

/**
 * Sampler information needed by VS, WM, and GS program cache keys.
 */
struct elk_sampler_prog_key_data {
   /**
    * EXT_texture_swizzle and DEPTH_TEXTURE_MODE swizzles.
    *
    * This field is not consumed by the back-end compiler and is only relevant
    * for the crocus OpenGL driver for Broadwell and earlier hardware.
    */
   uint16_t swizzles[ELK_MAX_SAMPLERS];

   uint32_t gl_clamp_mask[3];

   /**
    * For RG32F, gather4's channel select is broken.
    */
   uint32_t gather_channel_quirk_mask;

   /**
    * For Sandybridge, which shader w/a we need for gather quirks.
    */
   enum elk_gfx6_gather_sampler_wa gfx6_gather_wa[ELK_MAX_SAMPLERS];
};

enum elk_robustness_flags {
   ELK_ROBUSTNESS_UBO  = BITFIELD_BIT(0),
   ELK_ROBUSTNESS_SSBO = BITFIELD_BIT(1),
};

struct elk_base_prog_key {
   unsigned program_string_id;

   enum elk_robustness_flags robust_flags:2;

   unsigned padding:22;

   /**
    * Apply workarounds for SIN and COS input range problems.
    * This limits input range for SIN and COS to [-2p : 2p] to
    * avoid precision issues.
    */
   bool limit_trig_input_range;

   struct elk_sampler_prog_key_data tex;
};

/**
 * The VF can't natively handle certain types of attributes, such as GL_FIXED
 * or most 10_10_10_2 types.  These flags enable various VS workarounds to
 * "fix" attributes at the beginning of shaders.
 */
#define ELK_ATTRIB_WA_COMPONENT_MASK    7  /* mask for GL_FIXED scale channel count */
#define ELK_ATTRIB_WA_NORMALIZE     8   /* normalize in shader */
#define ELK_ATTRIB_WA_BGRA          16  /* swap r/b channels in shader */
#define ELK_ATTRIB_WA_SIGN          32  /* interpret as signed in shader */
#define ELK_ATTRIB_WA_SCALE         64  /* interpret as scaled in shader */

/**
 * OpenGL attribute slots fall in [0, VERT_ATTRIB_MAX - 1] with the range
 * [VERT_ATTRIB_GENERIC0, VERT_ATTRIB_MAX - 1] reserved for up to 16 user
 * input vertex attributes. In Vulkan, we expose up to 28 user vertex input
 * attributes that are mapped to slots also starting at VERT_ATTRIB_GENERIC0.
 */
#define MAX_GL_VERT_ATTRIB     VERT_ATTRIB_MAX
#define MAX_VK_VERT_ATTRIB     (VERT_ATTRIB_GENERIC0 + 28)

/**
 * Max number of binding table entries used for stream output.
 *
 * From the OpenGL 3.0 spec, table 6.44 (Transform Feedback State), the
 * minimum value of MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS is 64.
 *
 * On Gfx6, the size of transform feedback data is limited not by the number
 * of components but by the number of binding table entries we set aside.  We
 * use one binding table entry for a float, one entry for a vector, and one
 * entry per matrix column.  Since the only way we can communicate our
 * transform feedback capabilities to the client is via
 * MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS, we need to plan for the
 * worst case, in which all the varyings are floats, so we use up one binding
 * table entry per component.  Therefore we need to set aside at least 64
 * binding table entries for use by transform feedback.
 *
 * Note: since we don't currently pack varyings, it is currently impossible
 * for the client to actually use up all of these binding table entries--if
 * all of their varyings were floats, they would run out of varying slots and
 * fail to link.  But that's a bug, so it seems prudent to go ahead and
 * allocate the number of binding table entries we will need once the bug is
 * fixed.
 */
#define ELK_MAX_SOL_BINDINGS 64

/** The program key for Vertex Shaders. */
struct elk_vs_prog_key {
   struct elk_base_prog_key base;

   /**
    * Per-attribute workaround flags
    *
    * For each attribute, a combination of ELK_ATTRIB_WA_*.
    *
    * For OpenGL, where we expose a maximum of 16 user input attributes
    * we only need up to VERT_ATTRIB_MAX slots, however, in Vulkan
    * slots preceding VERT_ATTRIB_GENERIC0 are unused and we can
    * expose up to 28 user input vertex attributes that are mapped to slots
    * starting at VERT_ATTRIB_GENERIC0, so this array needs to be large
    * enough to hold this many slots.
    */
   uint8_t gl_attrib_wa_flags[MAX2(MAX_GL_VERT_ATTRIB, MAX_VK_VERT_ATTRIB)];

   /**
    * For pre-Gfx6 hardware, a bitfield indicating which texture coordinates
    * are going to be replaced with point coordinates (as a consequence of a
    * call to glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE)).  Because
    * our SF thread requires exact matching between VS outputs and FS inputs,
    * these texture coordinates will need to be unconditionally included in
    * the VUE, even if they aren't written by the vertex shader.
    */
   uint8_t point_coord_replace;
   unsigned clamp_pointsize:1;

   bool copy_edgeflag:1;

   bool clamp_vertex_color:1;

   /**
    * How many user clipping planes are being uploaded to the vertex shader as
    * push constants.
    *
    * These are used for lowering legacy gl_ClipVertex/gl_Position clipping to
    * clip distances.
    */
   unsigned nr_userclip_plane_consts:4;

   uint32_t padding: 25;
};

/** The program key for Tessellation Control Shaders. */
struct elk_tcs_prog_key
{
   struct elk_base_prog_key base;

   /** A bitfield of per-vertex outputs written. */
   uint64_t outputs_written;

   enum tess_primitive_mode _tes_primitive_mode;

   /** Number of input vertices, 0 means dynamic */
   unsigned input_vertices;

   /** A bitfield of per-patch outputs written. */
   uint32_t patch_outputs_written;

   bool quads_workaround;
   uint32_t padding:24;
};

#define ELK_MAX_TCS_INPUT_VERTICES (32)

static inline uint32_t
elk_tcs_prog_key_input_vertices(const struct elk_tcs_prog_key *key)
{
   return key->input_vertices != 0 ?
          key->input_vertices : ELK_MAX_TCS_INPUT_VERTICES;
}

/** The program key for Tessellation Evaluation Shaders. */
struct elk_tes_prog_key
{
   struct elk_base_prog_key base;

   /** A bitfield of per-vertex inputs read. */
   uint64_t inputs_read;

   /** A bitfield of per-patch inputs read. */
   uint32_t patch_inputs_read;

   /**
    * How many user clipping planes are being uploaded to the tessellation
    * evaluation shader as push constants.
    *
    * These are used for lowering legacy gl_ClipVertex/gl_Position clipping to
    * clip distances.
    */
   unsigned nr_userclip_plane_consts:4;
   unsigned clamp_pointsize:1;
   uint32_t padding:27;
};

/** The program key for Geometry Shaders. */
struct elk_gs_prog_key
{
   struct elk_base_prog_key base;

   /**
    * How many user clipping planes are being uploaded to the geometry shader
    * as push constants.
    *
    * These are used for lowering legacy gl_ClipVertex/gl_Position clipping to
    * clip distances.
    */
   unsigned nr_userclip_plane_consts:4;
   unsigned clamp_pointsize:1;
   unsigned padding:27;
};

enum elk_sf_primitive {
   ELK_SF_PRIM_POINTS = 0,
   ELK_SF_PRIM_LINES = 1,
   ELK_SF_PRIM_TRIANGLES = 2,
   ELK_SF_PRIM_UNFILLED_TRIS = 3,
};

struct elk_sf_prog_key {
   uint64_t attrs;
   bool contains_flat_varying;
   unsigned char interp_mode[65]; /* ELK_VARYING_SLOT_COUNT */
   uint8_t point_sprite_coord_replace;
   enum elk_sf_primitive primitive:2;
   bool do_twoside_color:1;
   bool frontface_ccw:1;
   bool do_point_sprite:1;
   bool do_point_coord:1;
   bool sprite_origin_lower_left:1;
   bool userclip_active:1;
   unsigned padding: 32;
};

enum elk_clip_mode {
   ELK_CLIP_MODE_NORMAL             = 0,
   ELK_CLIP_MODE_CLIP_ALL           = 1,
   ELK_CLIP_MODE_CLIP_NON_REJECTED  = 2,
   ELK_CLIP_MODE_REJECT_ALL         = 3,
   ELK_CLIP_MODE_ACCEPT_ALL         = 4,
   ELK_CLIP_MODE_KERNEL_CLIP        = 5,
};

enum elk_clip_fill_mode {
   ELK_CLIP_FILL_MODE_LINE = 0,
   ELK_CLIP_FILL_MODE_POINT = 1,
   ELK_CLIP_FILL_MODE_FILL = 2,
   ELK_CLIP_FILL_MODE_CULL = 3,
};

/* Note that if unfilled primitives are being emitted, we have to fix
 * up polygon offset and flatshading at this point:
 */
struct elk_clip_prog_key {
   uint64_t attrs;
   float offset_factor;
   float offset_units;
   float offset_clamp;
   bool contains_flat_varying;
   bool contains_noperspective_varying;
   unsigned char interp_mode[65]; /* ELK_VARYING_SLOT_COUNT */
   unsigned primitive:4;
   unsigned nr_userclip:4;
   bool pv_first:1;
   bool do_unfilled:1;
   enum elk_clip_fill_mode fill_cw:2;  /* includes cull information */
   enum elk_clip_fill_mode fill_ccw:2; /* includes cull information */
   bool offset_cw:1;
   bool offset_ccw:1;
   bool copy_bfc_cw:1;
   bool copy_bfc_ccw:1;
   enum elk_clip_mode clip_mode:3;
   uint64_t padding:51;
};

/* A big lookup table is used to figure out which and how many
 * additional regs will inserted before the main payload in the WM
 * program execution.  These mainly relate to depth and stencil
 * processing and the early-depth-test optimization.
 */
enum elk_wm_iz_bits {
   ELK_WM_IZ_PS_KILL_ALPHATEST_BIT     = 0x1,
   ELK_WM_IZ_PS_COMPUTES_DEPTH_BIT     = 0x2,
   ELK_WM_IZ_DEPTH_WRITE_ENABLE_BIT    = 0x4,
   ELK_WM_IZ_DEPTH_TEST_ENABLE_BIT     = 0x8,
   ELK_WM_IZ_STENCIL_WRITE_ENABLE_BIT  = 0x10,
   ELK_WM_IZ_STENCIL_TEST_ENABLE_BIT   = 0x20,
   ELK_WM_IZ_BIT_MAX                   = 0x40
};

enum elk_sometimes {
   ELK_NEVER = 0,
   ELK_SOMETIMES,
   ELK_ALWAYS
};

static inline enum elk_sometimes
elk_sometimes_invert(enum elk_sometimes x)
{
   return (enum elk_sometimes)((int)ELK_ALWAYS - (int)x);
}

/** The program key for Fragment/Pixel Shaders. */
struct elk_wm_prog_key {
   struct elk_base_prog_key base;

   uint64_t input_slots_valid;
   float alpha_test_ref;
   uint8_t color_outputs_valid;

   /* Some collection of ELK_WM_IZ_* */
   uint8_t iz_lookup;
   bool stats_wm:1;
   bool flat_shade:1;
   unsigned nr_color_regions:5;
   bool emit_alpha_test:1;
   enum compare_func alpha_test_func:3; /* < For Gfx4/5 MRT alpha test */
   bool alpha_test_replicate_alpha:1;
   enum elk_sometimes alpha_to_coverage:2;
   bool clamp_fragment_color:1;

   bool force_dual_color_blend:1;

   /** Whether or inputs are interpolated at sample rate by default
    *
    * This corresponds to the sample shading API bit in Vulkan or OpenGL which
    * controls how inputs with no interpolation qualifier are interpolated.
    * This is distinct from the way that using gl_SampleID or similar requires
    * us to run per-sample.  Even when running per-sample due to gl_SampleID,
    * we may still interpolate unqualified inputs at the pixel center.
    */
   enum elk_sometimes persample_interp:2;

   /* Whether or not we are running on a multisampled framebuffer */
   enum elk_sometimes multisample_fbo:2;

   enum elk_sometimes line_aa:2;

   bool coherent_fb_fetch:1;
   bool ignore_sample_mask_out:1;

   uint64_t padding:56;
};

struct elk_cs_prog_key {
   struct elk_base_prog_key base;
};

struct elk_ff_gs_prog_key {
   uint64_t attrs;

   /**
    * Map from the index of a transform feedback binding table entry to the
    * gl_varying_slot that should be streamed out through that binding table
    * entry.
    */
   unsigned char transform_feedback_bindings[ELK_MAX_SOL_BINDINGS];

   /**
    * Map from the index of a transform feedback binding table entry to the
    * swizzles that should be used when streaming out data through that
    * binding table entry.
    */
   unsigned char transform_feedback_swizzles[ELK_MAX_SOL_BINDINGS];

   /**
    * Hardware primitive type being drawn, e.g. _3DPRIM_TRILIST.
    */
   unsigned primitive:8;

   unsigned pv_first:1;
   unsigned need_gs_prog:1;

   /**
    * Number of varyings that are output to transform feedback.
    */
   unsigned num_transform_feedback_bindings:7; /* 0-ELK_MAX_SOL_BINDINGS */
   uint64_t padding:47;
};

/* elk_any_prog_key is any of the keys that map to an API stage */
union elk_any_prog_key {
   struct elk_base_prog_key base;
   struct elk_vs_prog_key vs;
   struct elk_tcs_prog_key tcs;
   struct elk_tes_prog_key tes;
   struct elk_gs_prog_key gs;
   struct elk_wm_prog_key wm;
   struct elk_cs_prog_key cs;
};

PRAGMA_DIAGNOSTIC_POP

/** Max number of render targets in a shader */
#define ELK_MAX_DRAW_BUFFERS 8

/**
 * Binding table index for the first gfx6 SOL binding.
 */
#define ELK_GFX6_SOL_BINDING_START 0

struct elk_ubo_range
{
   uint16_t block;

   /* In units of 32-byte registers */
   uint8_t start;
   uint8_t length;
};

/* We reserve the first 2^16 values for builtins */
#define ELK_PARAM_IS_BUILTIN(param) (((param) & 0xffff0000) == 0)

enum elk_param_builtin {
   ELK_PARAM_BUILTIN_ZERO,

   ELK_PARAM_BUILTIN_CLIP_PLANE_0_X,
   ELK_PARAM_BUILTIN_CLIP_PLANE_0_Y,
   ELK_PARAM_BUILTIN_CLIP_PLANE_0_Z,
   ELK_PARAM_BUILTIN_CLIP_PLANE_0_W,
   ELK_PARAM_BUILTIN_CLIP_PLANE_1_X,
   ELK_PARAM_BUILTIN_CLIP_PLANE_1_Y,
   ELK_PARAM_BUILTIN_CLIP_PLANE_1_Z,
   ELK_PARAM_BUILTIN_CLIP_PLANE_1_W,
   ELK_PARAM_BUILTIN_CLIP_PLANE_2_X,
   ELK_PARAM_BUILTIN_CLIP_PLANE_2_Y,
   ELK_PARAM_BUILTIN_CLIP_PLANE_2_Z,
   ELK_PARAM_BUILTIN_CLIP_PLANE_2_W,
   ELK_PARAM_BUILTIN_CLIP_PLANE_3_X,
   ELK_PARAM_BUILTIN_CLIP_PLANE_3_Y,
   ELK_PARAM_BUILTIN_CLIP_PLANE_3_Z,
   ELK_PARAM_BUILTIN_CLIP_PLANE_3_W,
   ELK_PARAM_BUILTIN_CLIP_PLANE_4_X,
   ELK_PARAM_BUILTIN_CLIP_PLANE_4_Y,
   ELK_PARAM_BUILTIN_CLIP_PLANE_4_Z,
   ELK_PARAM_BUILTIN_CLIP_PLANE_4_W,
   ELK_PARAM_BUILTIN_CLIP_PLANE_5_X,
   ELK_PARAM_BUILTIN_CLIP_PLANE_5_Y,
   ELK_PARAM_BUILTIN_CLIP_PLANE_5_Z,
   ELK_PARAM_BUILTIN_CLIP_PLANE_5_W,
   ELK_PARAM_BUILTIN_CLIP_PLANE_6_X,
   ELK_PARAM_BUILTIN_CLIP_PLANE_6_Y,
   ELK_PARAM_BUILTIN_CLIP_PLANE_6_Z,
   ELK_PARAM_BUILTIN_CLIP_PLANE_6_W,
   ELK_PARAM_BUILTIN_CLIP_PLANE_7_X,
   ELK_PARAM_BUILTIN_CLIP_PLANE_7_Y,
   ELK_PARAM_BUILTIN_CLIP_PLANE_7_Z,
   ELK_PARAM_BUILTIN_CLIP_PLANE_7_W,

   ELK_PARAM_BUILTIN_TESS_LEVEL_OUTER_X,
   ELK_PARAM_BUILTIN_TESS_LEVEL_OUTER_Y,
   ELK_PARAM_BUILTIN_TESS_LEVEL_OUTER_Z,
   ELK_PARAM_BUILTIN_TESS_LEVEL_OUTER_W,
   ELK_PARAM_BUILTIN_TESS_LEVEL_INNER_X,
   ELK_PARAM_BUILTIN_TESS_LEVEL_INNER_Y,

   ELK_PARAM_BUILTIN_PATCH_VERTICES_IN,

   ELK_PARAM_BUILTIN_BASE_WORK_GROUP_ID_X,
   ELK_PARAM_BUILTIN_BASE_WORK_GROUP_ID_Y,
   ELK_PARAM_BUILTIN_BASE_WORK_GROUP_ID_Z,
   ELK_PARAM_BUILTIN_SUBGROUP_ID,
   ELK_PARAM_BUILTIN_WORK_GROUP_SIZE_X,
   ELK_PARAM_BUILTIN_WORK_GROUP_SIZE_Y,
   ELK_PARAM_BUILTIN_WORK_GROUP_SIZE_Z,
   ELK_PARAM_BUILTIN_WORK_DIM,
};

#define ELK_PARAM_BUILTIN_CLIP_PLANE(idx, comp) \
   (ELK_PARAM_BUILTIN_CLIP_PLANE_0_X + ((idx) << 2) + (comp))

#define ELK_PARAM_BUILTIN_IS_CLIP_PLANE(param)  \
   ((param) >= ELK_PARAM_BUILTIN_CLIP_PLANE_0_X && \
    (param) <= ELK_PARAM_BUILTIN_CLIP_PLANE_7_W)

#define ELK_PARAM_BUILTIN_CLIP_PLANE_IDX(param) \
   (((param) - ELK_PARAM_BUILTIN_CLIP_PLANE_0_X) >> 2)

#define ELK_PARAM_BUILTIN_CLIP_PLANE_COMP(param) \
   (((param) - ELK_PARAM_BUILTIN_CLIP_PLANE_0_X) & 0x3)

enum elk_shader_reloc_id {
   ELK_SHADER_RELOC_CONST_DATA_ADDR_LOW,
   ELK_SHADER_RELOC_CONST_DATA_ADDR_HIGH,
   ELK_SHADER_RELOC_SHADER_START_OFFSET,
   ELK_SHADER_RELOC_DESCRIPTORS_ADDR_HIGH,
};

enum elk_shader_reloc_type {
   /** An arbitrary 32-bit value */
   ELK_SHADER_RELOC_TYPE_U32,
   /** A MOV instruction with an immediate source */
   ELK_SHADER_RELOC_TYPE_MOV_IMM,
};

/** Represents a code relocation
 *
 * Relocatable constants are immediates in the code which we want to be able
 * to replace post-compile with the actual value.
 */
struct elk_shader_reloc {
   /** The 32-bit ID of the relocatable constant */
   uint32_t id;

   /** Type of this relocation */
   enum elk_shader_reloc_type type;

   /** The offset in the shader to the relocated value
    *
    * For MOV_IMM relocs, this is an offset to the MOV instruction.  This
    * allows us to do some sanity checking while we update the value.
    */
   uint32_t offset;

   /** Value to be added to the relocated value before it is written */
   uint32_t delta;
};

/** A value to write to a relocation */
struct elk_shader_reloc_value {
   /** The 32-bit ID of the relocatable constant */
   uint32_t id;

   /** The value with which to replace the relocated immediate */
   uint32_t value;
};

struct elk_stage_prog_data {
   struct elk_ubo_range ubo_ranges[4];

   unsigned nr_params;       /**< number of float params/constants */

   gl_shader_stage stage;

   /* zero_push_reg is a bitfield which indicates what push registers (if any)
    * should be zeroed by SW at the start of the shader.  The corresponding
    * push_reg_mask_param specifies the param index (in 32-bit units) where
    * the actual runtime 64-bit mask will be pushed.  The shader will zero
    * push reg i if
    *
    *    reg_used & zero_push_reg & ~*push_reg_mask_param & (1ull << i)
    *
    * If this field is set, elk_compiler::compact_params must be false.
    */
   uint64_t zero_push_reg;
   unsigned push_reg_mask_param;

   unsigned curb_read_length;
   unsigned total_scratch;
   unsigned total_shared;

   unsigned program_size;

   unsigned const_data_size;
   unsigned const_data_offset;

   unsigned num_relocs;
   const struct elk_shader_reloc *relocs;

   /** Does this program pull from any UBO or other constant buffers? */
   bool has_ubo_pull;

   /**
    * Register where the thread expects to find input data from the URB
    * (typically uniforms, followed by vertex or fragment attributes).
    */
   unsigned dispatch_grf_start_reg;

   bool use_alt_mode; /**< Use ALT floating point mode?  Otherwise, IEEE. */

   /* 32-bit identifiers for all push/pull parameters.  These can be anything
    * the driver wishes them to be; the core of the back-end compiler simply
    * re-arranges them.  The one restriction is that the bottom 2^16 values
    * are reserved for builtins defined in the elk_param_builtin enum defined
    * above.
    */
   uint32_t *param;

   /* Whether shader uses atomic operations. */
   bool uses_atomic_load_store;
};

static inline uint32_t *
elk_stage_prog_data_add_params(struct elk_stage_prog_data *prog_data,
                               unsigned nr_new_params)
{
   unsigned old_nr_params = prog_data->nr_params;
   prog_data->nr_params += nr_new_params;
   prog_data->param = reralloc(ralloc_parent(prog_data->param),
                               prog_data->param, uint32_t,
                               prog_data->nr_params);
   return prog_data->param + old_nr_params;
}

enum elk_barycentric_mode {
   ELK_BARYCENTRIC_PERSPECTIVE_PIXEL       = 0,
   ELK_BARYCENTRIC_PERSPECTIVE_CENTROID    = 1,
   ELK_BARYCENTRIC_PERSPECTIVE_SAMPLE      = 2,
   ELK_BARYCENTRIC_NONPERSPECTIVE_PIXEL    = 3,
   ELK_BARYCENTRIC_NONPERSPECTIVE_CENTROID = 4,
   ELK_BARYCENTRIC_NONPERSPECTIVE_SAMPLE   = 5,
   ELK_BARYCENTRIC_MODE_COUNT              = 6
};
#define ELK_BARYCENTRIC_PERSPECTIVE_BITS \
   ((1 << ELK_BARYCENTRIC_PERSPECTIVE_PIXEL) | \
    (1 << ELK_BARYCENTRIC_PERSPECTIVE_CENTROID) | \
    (1 << ELK_BARYCENTRIC_PERSPECTIVE_SAMPLE))
#define ELK_BARYCENTRIC_NONPERSPECTIVE_BITS \
   ((1 << ELK_BARYCENTRIC_NONPERSPECTIVE_PIXEL) | \
    (1 << ELK_BARYCENTRIC_NONPERSPECTIVE_CENTROID) | \
    (1 << ELK_BARYCENTRIC_NONPERSPECTIVE_SAMPLE))

enum elk_pixel_shader_computed_depth_mode {
   ELK_PSCDEPTH_OFF   = 0, /* PS does not compute depth */
   ELK_PSCDEPTH_ON    = 1, /* PS computes depth; no guarantee about value */
   ELK_PSCDEPTH_ON_GE = 2, /* PS guarantees output depth >= source depth */
   ELK_PSCDEPTH_ON_LE = 3, /* PS guarantees output depth <= source depth */
};

/* Data about a particular attempt to compile a program.  Note that
 * there can be many of these, each in a different GL state
 * corresponding to a different elk_wm_prog_key struct, with different
 * compiled programs.
 */
struct elk_wm_prog_data {
   struct elk_stage_prog_data base;

   unsigned num_per_primitive_inputs;
   unsigned num_varying_inputs;

   uint8_t reg_blocks_8;
   uint8_t reg_blocks_16;
   uint8_t reg_blocks_32;

   uint8_t dispatch_grf_start_reg_16;
   uint8_t dispatch_grf_start_reg_32;
   uint32_t prog_offset_16;
   uint32_t prog_offset_32;

   struct {
      /** @{
       * surface indices the WM-specific surfaces
       */
      uint32_t render_target_read_start;
      /** @} */
   } binding_table;

   uint8_t color_outputs_written;
   uint8_t computed_depth_mode;

   bool computed_stencil;
   bool early_fragment_tests;
   bool post_depth_coverage;
   bool inner_coverage;
   bool dispatch_8;
   bool dispatch_16;
   bool dispatch_32;
   bool dual_src_blend;
   bool uses_pos_offset;
   bool uses_omask;
   bool uses_kill;
   bool uses_src_depth;
   bool uses_src_w;
   bool uses_sample_mask;
   bool uses_vmask;
   bool has_side_effects;
   bool pulls_bary;

   bool contains_flat_varying;
   bool contains_noperspective_varying;

   /** True if the shader wants sample shading
    *
    * This corresponds to whether or not a gl_SampleId, gl_SamplePosition, or
    * a sample-qualified input are used in the shader.  It is independent of
    * GL_MIN_SAMPLE_SHADING_VALUE in GL or minSampleShading in Vulkan.
    */
   bool sample_shading;

   /** Should this shader be dispatched per-sample */
   enum elk_sometimes persample_dispatch;

   /**
    * Shader writes the SampleMask and this is AND-ed with the API's
    * SampleMask to generate a new coverage mask.
    */
   enum elk_sometimes alpha_to_coverage;

   unsigned msaa_flags_param;

   /**
    * Mask of which interpolation modes are required by the fragment shader.
    * Those interpolations are delivered as part of the thread payload. Used
    * in hardware setup on gfx6+.
    */
   uint32_t barycentric_interp_modes;

   /**
    * Whether nonperspective interpolation modes are used by the
    * barycentric_interp_modes or fragment shader through interpolator messages.
    */
   bool uses_nonperspective_interp_modes;

   /**
    * Mask of which FS inputs are marked flat by the shader source.  This is
    * needed for setting up 3DSTATE_SF/SBE.
    */
   uint32_t flat_inputs;

   /**
    * The FS inputs
    */
   uint64_t inputs;

   /* Mapping of VUE slots to interpolation modes.
    * Used by the Gfx4-5 clip/sf/wm stages.
    */
   unsigned char interp_mode[65]; /* ELK_VARYING_SLOT_COUNT */

   /**
    * Map from gl_varying_slot to the position within the FS setup data
    * payload where the varying's attribute vertex deltas should be delivered.
    * For varying slots that are not used by the FS, the value is -1.
    */
   int urb_setup[VARYING_SLOT_MAX];
   int urb_setup_channel[VARYING_SLOT_MAX];

   /**
    * Cache structure into the urb_setup array above that contains the
    * attribute numbers of active varyings out of urb_setup.
    * The actual count is stored in urb_setup_attribs_count.
    */
   uint8_t urb_setup_attribs[VARYING_SLOT_MAX];
   uint8_t urb_setup_attribs_count;
};

#ifdef GFX_VERx10

/** Returns the SIMD width corresponding to a given KSP index
 *
 * The "Variable Pixel Dispatch" table in the PRM (which can be found, for
 * example in Vol. 7 of the SKL PRM) has a mapping from dispatch widths to
 * kernel start pointer (KSP) indices that is based on what dispatch widths
 * are enabled.  This function provides, effectively, the reverse mapping.
 *
 * If the given KSP is valid with respect to the SIMD8/16/32 enables, a SIMD
 * width of 8, 16, or 32 is returned.  If the KSP is invalid, 0 is returned.
 */
static inline unsigned
elk_fs_simd_width_for_ksp(unsigned ksp_idx, bool simd8_enabled,
                          bool simd16_enabled, bool simd32_enabled)
{
   /* This function strictly ignores contiguous dispatch */
   switch (ksp_idx) {
   case 0:
      return simd8_enabled ? 8 :
             (simd16_enabled && !simd32_enabled) ? 16 :
             (simd32_enabled && !simd16_enabled) ? 32 : 0;
   case 1:
      return (simd32_enabled && (simd16_enabled || simd8_enabled)) ? 32 : 0;
   case 2:
      return (simd16_enabled && (simd32_enabled || simd8_enabled)) ? 16 : 0;
   default:
      unreachable("Invalid KSP index");
   }
}

#define elk_wm_state_simd_width_for_ksp(wm_state, ksp_idx)              \
   elk_fs_simd_width_for_ksp((ksp_idx), (wm_state)._8PixelDispatchEnable, \
                             (wm_state)._16PixelDispatchEnable, \
                             (wm_state)._32PixelDispatchEnable)

#endif

#define elk_wm_state_has_ksp(wm_state, ksp_idx) \
   (elk_wm_state_simd_width_for_ksp((wm_state), (ksp_idx)) != 0)

static inline uint32_t
_elk_wm_prog_data_prog_offset(const struct elk_wm_prog_data *prog_data,
                              unsigned simd_width)
{
   switch (simd_width) {
   case 8: return 0;
   case 16: return prog_data->prog_offset_16;
   case 32: return prog_data->prog_offset_32;
   default: return 0;
   }
}

#define elk_wm_prog_data_prog_offset(prog_data, wm_state, ksp_idx) \
   _elk_wm_prog_data_prog_offset(prog_data, \
      elk_wm_state_simd_width_for_ksp(wm_state, ksp_idx))

static inline uint8_t
_elk_wm_prog_data_dispatch_grf_start_reg(const struct elk_wm_prog_data *prog_data,
                                         unsigned simd_width)
{
   switch (simd_width) {
   case 8: return prog_data->base.dispatch_grf_start_reg;
   case 16: return prog_data->dispatch_grf_start_reg_16;
   case 32: return prog_data->dispatch_grf_start_reg_32;
   default: return 0;
   }
}

#define elk_wm_prog_data_dispatch_grf_start_reg(prog_data, wm_state, ksp_idx) \
   _elk_wm_prog_data_dispatch_grf_start_reg(prog_data, \
      elk_wm_state_simd_width_for_ksp(wm_state, ksp_idx))

static inline uint8_t
_elk_wm_prog_data_reg_blocks(const struct elk_wm_prog_data *prog_data,
                             unsigned simd_width)
{
   switch (simd_width) {
   case 8: return prog_data->reg_blocks_8;
   case 16: return prog_data->reg_blocks_16;
   case 32: return prog_data->reg_blocks_32;
   default: return 0;
   }
}

#define elk_wm_prog_data_reg_blocks(prog_data, wm_state, ksp_idx) \
   _elk_wm_prog_data_reg_blocks(prog_data, \
      elk_wm_state_simd_width_for_ksp(wm_state, ksp_idx))

static inline bool
elk_wm_prog_data_is_persample(const struct elk_wm_prog_data *prog_data,
                              enum intel_msaa_flags pushed_msaa_flags)
{
   if (pushed_msaa_flags & INTEL_MSAA_FLAG_ENABLE_DYNAMIC) {
      if (!(pushed_msaa_flags & INTEL_MSAA_FLAG_MULTISAMPLE_FBO))
         return false;

      if (prog_data->sample_shading)
         assert(pushed_msaa_flags & INTEL_MSAA_FLAG_PERSAMPLE_DISPATCH);

      if (pushed_msaa_flags & INTEL_MSAA_FLAG_PERSAMPLE_DISPATCH)
         assert(prog_data->persample_dispatch != ELK_NEVER);
      else
         assert(prog_data->persample_dispatch != ELK_ALWAYS);

      return (pushed_msaa_flags & INTEL_MSAA_FLAG_PERSAMPLE_DISPATCH) != 0;
   }

   assert(prog_data->persample_dispatch == ELK_ALWAYS ||
          prog_data->persample_dispatch == ELK_NEVER);

   return prog_data->persample_dispatch;
}

static inline uint32_t
elk_wm_prog_data_barycentric_modes(const struct elk_wm_prog_data *prog_data,
                               enum intel_msaa_flags pushed_msaa_flags)
{
   uint32_t modes = prog_data->barycentric_interp_modes;

   /* In the non dynamic case, we can just return the computed modes from
    * compilation time.
    */
   if (!(pushed_msaa_flags & INTEL_MSAA_FLAG_ENABLE_DYNAMIC))
      return modes;

   if (pushed_msaa_flags & INTEL_MSAA_FLAG_PERSAMPLE_INTERP) {
      assert(prog_data->persample_dispatch == ELK_ALWAYS ||
             (pushed_msaa_flags & INTEL_MSAA_FLAG_PERSAMPLE_DISPATCH));

      /* Making dynamic per-sample interpolation work is a bit tricky.  The
       * hardware will hang if SAMPLE is requested but per-sample dispatch is
       * not enabled.  This means we can't preemptively add SAMPLE to the
       * barycentrics bitfield.  Instead, we have to add it late and only
       * on-demand.  Annoyingly, changing the number of barycentrics requested
       * changes the whole PS shader payload so we very much don't want to do
       * that.  Instead, if the dynamic per-sample interpolation flag is set,
       * we check to see if SAMPLE was requested and, if not, replace the
       * highest barycentric bit in the [non]perspective grouping (CENTROID,
       * if it exists, else PIXEL) with SAMPLE.  The shader will stomp all the
       * barycentrics in the shader with SAMPLE so it really doesn't matter
       * which one we replace.  The important thing is that we keep the number
       * of barycentrics in each [non]perspective grouping the same.
       */
      if ((modes & ELK_BARYCENTRIC_PERSPECTIVE_BITS) &&
          !(modes & BITFIELD_BIT(ELK_BARYCENTRIC_PERSPECTIVE_SAMPLE))) {
         int sample_mode =
            util_last_bit(modes & ELK_BARYCENTRIC_PERSPECTIVE_BITS) - 1;
         assert(modes & BITFIELD_BIT(sample_mode));

         modes &= ~BITFIELD_BIT(sample_mode);
         modes |= BITFIELD_BIT(ELK_BARYCENTRIC_PERSPECTIVE_SAMPLE);
      }

      if ((modes & ELK_BARYCENTRIC_NONPERSPECTIVE_BITS) &&
          !(modes & BITFIELD_BIT(ELK_BARYCENTRIC_NONPERSPECTIVE_SAMPLE))) {
         int sample_mode =
            util_last_bit(modes & ELK_BARYCENTRIC_NONPERSPECTIVE_BITS) - 1;
         assert(modes & BITFIELD_BIT(sample_mode));

         modes &= ~BITFIELD_BIT(sample_mode);
         modes |= BITFIELD_BIT(ELK_BARYCENTRIC_NONPERSPECTIVE_SAMPLE);
      }
   } else {
      /* If we're not using per-sample interpolation, we need to disable the
       * per-sample bits.
       *
       * SKL PRMs, Volume 2a: Command Reference: Instructions,
       * 3DSTATE_WM:Barycentric Interpolation Mode:

       *    "MSDISPMODE_PERSAMPLE is required in order to select Perspective
       *     Sample or Non-perspective Sample barycentric coordinates."
       */
      modes &= ~(BITFIELD_BIT(ELK_BARYCENTRIC_PERSPECTIVE_SAMPLE) |
                 BITFIELD_BIT(ELK_BARYCENTRIC_NONPERSPECTIVE_SAMPLE));
   }

   return modes;
}

struct elk_push_const_block {
   unsigned dwords;     /* Dword count, not reg aligned */
   unsigned regs;
   unsigned size;       /* Bytes, register aligned */
};

struct elk_cs_prog_data {
   struct elk_stage_prog_data base;

   unsigned local_size[3];

   /* Program offsets for the 8/16/32 SIMD variants.  Multiple variants are
    * kept when using variable group size, and the right one can only be
    * decided at dispatch time.
    */
   unsigned prog_offset[3];

   /* Bitmask indicating which program offsets are valid. */
   unsigned prog_mask;

   /* Bitmask indicating which programs have spilled. */
   unsigned prog_spilled;

   bool uses_barrier;
   bool uses_num_work_groups;

   struct {
      struct elk_push_const_block cross_thread;
      struct elk_push_const_block per_thread;
   } push;

   struct {
      /** @{
       * surface indices the CS-specific surfaces
       */
      uint32_t work_groups_start;
      /** @} */
   } binding_table;
};

static inline uint32_t
elk_cs_prog_data_prog_offset(const struct elk_cs_prog_data *prog_data,
                             unsigned dispatch_width)
{
   assert(dispatch_width == 8 ||
          dispatch_width == 16 ||
          dispatch_width == 32);
   const unsigned index = dispatch_width / 16;
   assert(prog_data->prog_mask & (1 << index));
   return prog_data->prog_offset[index];
}

struct elk_ff_gs_prog_data {
   unsigned urb_read_length;
   unsigned total_grf;

   /**
    * Gfx6 transform feedback: Amount by which the streaming vertex buffer
    * indices should be incremented each time the GS is invoked.
    */
   unsigned svbi_postincrement_value;
};

/**
 * Enum representing the i965-specific vertex results that don't correspond
 * exactly to any element of gl_varying_slot.  The values of this enum are
 * assigned such that they don't conflict with gl_varying_slot.
 */
typedef enum
{
   ELK_VARYING_SLOT_NDC = VARYING_SLOT_MAX,
   ELK_VARYING_SLOT_PAD,
   /**
    * Technically this is not a varying but just a placeholder that
    * compile_sf_prog() inserts into its VUE map to cause the gl_PointCoord
    * builtin variable to be compiled correctly. see compile_sf_prog() for
    * more info.
    */
   ELK_VARYING_SLOT_PNTC,
   ELK_VARYING_SLOT_COUNT
} elk_varying_slot;

/**
 * We always program SF to start reading at an offset of 1 (2 varying slots)
 * from the start of the vertex URB entry.  This causes it to skip:
 * - VARYING_SLOT_PSIZ and ELK_VARYING_SLOT_NDC on gfx4-5
 * - VARYING_SLOT_PSIZ and VARYING_SLOT_POS on gfx6+
 */
#define ELK_SF_URB_ENTRY_READ_OFFSET 1

/**
 * Bitmask indicating which fragment shader inputs represent varyings (and
 * hence have to be delivered to the fragment shader by the SF/SBE stage).
 */
#define ELK_FS_VARYING_INPUT_MASK \
   (BITFIELD64_RANGE(0, VARYING_SLOT_MAX) & \
    ~VARYING_BIT_POS & ~VARYING_BIT_FACE)

void elk_print_vue_map(FILE *fp, const struct intel_vue_map *vue_map,
                       gl_shader_stage stage);

/**
 * Convert a VUE slot number into a byte offset within the VUE.
 */
static inline unsigned elk_vue_slot_to_offset(unsigned slot)
{
   return 16*slot;
}

/**
 * Convert a vertex output (elk_varying_slot) into a byte offset within the
 * VUE.
 */
static inline unsigned
elk_varying_to_offset(const struct intel_vue_map *vue_map, unsigned varying)
{
   return elk_vue_slot_to_offset(vue_map->varying_to_slot[varying]);
}

void elk_compute_vue_map(const struct intel_device_info *devinfo,
                         struct intel_vue_map *vue_map,
                         uint64_t slots_valid,
                         bool separate_shader,
                         uint32_t pos_slots);

void elk_compute_tess_vue_map(struct intel_vue_map *const vue_map,
                              uint64_t slots_valid,
                              uint32_t is_patch);

/* elk_interpolation_map.c */
void elk_setup_vue_interpolation(const struct intel_vue_map *vue_map,
                                 struct nir_shader *nir,
                                 struct elk_wm_prog_data *prog_data);

struct elk_vue_prog_data {
   struct elk_stage_prog_data base;
   struct intel_vue_map vue_map;

   /** Should the hardware deliver input VUE handles for URB pull loads? */
   bool include_vue_handles;

   unsigned urb_read_length;
   unsigned total_grf;

   uint32_t clip_distance_mask;
   uint32_t cull_distance_mask;

   /* Used for calculating urb partitions.  In the VS, this is the size of the
    * URB entry used for both input and output to the thread.  In the GS, this
    * is the size of the URB entry used for output.
    */
   unsigned urb_entry_size;

   enum intel_shader_dispatch_mode dispatch_mode;
};

struct elk_vs_prog_data {
   struct elk_vue_prog_data base;

   uint64_t inputs_read;
   uint64_t double_inputs_read;

   unsigned nr_attribute_slots;

   bool uses_vertexid;
   bool uses_instanceid;
   bool uses_is_indexed_draw;
   bool uses_firstvertex;
   bool uses_baseinstance;
   bool uses_drawid;
};

struct elk_tcs_prog_data
{
   struct elk_vue_prog_data base;

   /** Should the non-SINGLE_PATCH payload provide primitive ID? */
   bool include_primitive_id;

   /** Number vertices in output patch */
   int instances;

   /** Track patch count threshold */
   int patch_count_threshold;
};


struct elk_tes_prog_data
{
   struct elk_vue_prog_data base;

   enum intel_tess_partitioning partitioning;
   enum intel_tess_output_topology output_topology;
   enum intel_tess_domain domain;
   bool include_primitive_id;
};

struct elk_gs_prog_data
{
   struct elk_vue_prog_data base;

   unsigned vertices_in;

   /**
    * Size of an output vertex, measured in HWORDS (32 bytes).
    */
   unsigned output_vertex_size_hwords;

   unsigned output_topology;

   /**
    * Size of the control data (cut bits or StreamID bits), in hwords (32
    * bytes).  0 if there is no control data.
    */
   unsigned control_data_header_size_hwords;

   /**
    * Format of the control data (either GFX7_GS_CONTROL_DATA_FORMAT_GSCTL_SID
    * if the control data is StreamID bits, or
    * GFX7_GS_CONTROL_DATA_FORMAT_GSCTL_CUT if the control data is cut bits).
    * Ignored if control_data_header_size is 0.
    */
   unsigned control_data_format;

   bool include_primitive_id;

   /**
    * The number of vertices emitted, if constant - otherwise -1.
    */
   int static_vertex_count;

   int invocations;

   /**
    * Gfx6: Provoking vertex convention for odd-numbered triangles
    * in tristrips.
    */
   unsigned pv_first:1;

   /**
    * Gfx6: Number of varyings that are output to transform feedback.
    */
   unsigned num_transform_feedback_bindings:7; /* 0-ELK_MAX_SOL_BINDINGS */

   /**
    * Gfx6: Map from the index of a transform feedback binding table entry to the
    * gl_varying_slot that should be streamed out through that binding table
    * entry.
    */
   unsigned char transform_feedback_bindings[64 /* ELK_MAX_SOL_BINDINGS */];

   /**
    * Gfx6: Map from the index of a transform feedback binding table entry to the
    * swizzles that should be used when streaming out data through that
    * binding table entry.
    */
   unsigned char transform_feedback_swizzles[64 /* ELK_MAX_SOL_BINDINGS */];
};

struct elk_sf_prog_data {
   uint32_t urb_read_length;
   uint32_t total_grf;

   /* Each vertex may have up to 12 attributes, 4 components each,
    * except WPOS which requires only 2.  (11*4 + 2) == 44 ==> 11
    * rows.
    *
    * Actually we use 4 for each, so call it 12 rows.
    */
   unsigned urb_entry_size;
};

struct elk_clip_prog_data {
   uint32_t curb_read_length;	/* user planes? */
   uint32_t clip_mode;
   uint32_t urb_read_length;
   uint32_t total_grf;
};

/* elk_any_prog_data is prog_data for any stage that maps to an API stage */
union elk_any_prog_data {
   struct elk_stage_prog_data base;
   struct elk_vue_prog_data vue;
   struct elk_vs_prog_data vs;
   struct elk_tcs_prog_data tcs;
   struct elk_tes_prog_data tes;
   struct elk_gs_prog_data gs;
   struct elk_wm_prog_data wm;
   struct elk_cs_prog_data cs;
};

#define DEFINE_PROG_DATA_DOWNCAST(STAGE, CHECK)                            \
static inline struct elk_##STAGE##_prog_data *                             \
elk_##STAGE##_prog_data(struct elk_stage_prog_data *prog_data)             \
{                                                                          \
   if (prog_data)                                                          \
      assert(CHECK);                                                       \
   return (struct elk_##STAGE##_prog_data *) prog_data;                    \
}                                                                          \
static inline const struct elk_##STAGE##_prog_data *                       \
elk_##STAGE##_prog_data_const(const struct elk_stage_prog_data *prog_data) \
{                                                                          \
   if (prog_data)                                                          \
      assert(CHECK);                                                       \
   return (const struct elk_##STAGE##_prog_data *) prog_data;              \
}

DEFINE_PROG_DATA_DOWNCAST(vs,  prog_data->stage == MESA_SHADER_VERTEX)
DEFINE_PROG_DATA_DOWNCAST(tcs, prog_data->stage == MESA_SHADER_TESS_CTRL)
DEFINE_PROG_DATA_DOWNCAST(tes, prog_data->stage == MESA_SHADER_TESS_EVAL)
DEFINE_PROG_DATA_DOWNCAST(gs,  prog_data->stage == MESA_SHADER_GEOMETRY)
DEFINE_PROG_DATA_DOWNCAST(wm,  prog_data->stage == MESA_SHADER_FRAGMENT)
DEFINE_PROG_DATA_DOWNCAST(cs,  gl_shader_stage_uses_workgroup(prog_data->stage))

DEFINE_PROG_DATA_DOWNCAST(vue, prog_data->stage == MESA_SHADER_VERTEX ||
                               prog_data->stage == MESA_SHADER_TESS_CTRL ||
                               prog_data->stage == MESA_SHADER_TESS_EVAL ||
                               prog_data->stage == MESA_SHADER_GEOMETRY)

/* These are not really elk_stage_prog_data. */
DEFINE_PROG_DATA_DOWNCAST(ff_gs, true)
DEFINE_PROG_DATA_DOWNCAST(clip,  true)
DEFINE_PROG_DATA_DOWNCAST(sf,    true)
#undef DEFINE_PROG_DATA_DOWNCAST

struct elk_compile_stats {
   uint32_t dispatch_width; /**< 0 for vec4 */
   uint32_t max_polygons;
   uint32_t max_dispatch_width;
   uint32_t instructions;
   uint32_t sends;
   uint32_t loops;
   uint32_t cycles;
   uint32_t spills;
   uint32_t fills;
   uint32_t max_live_registers;
};

/** @} */

struct elk_compiler *
elk_compiler_create(void *mem_ctx, const struct intel_device_info *devinfo);

/**
 * Returns a compiler configuration for use with disk shader cache
 *
 * This value only needs to change for settings that can cause different
 * program generation between two runs on the same hardware.
 *
 * For example, it doesn't need to be different for gen 8 and gen 9 hardware,
 * but it does need to be different if INTEL_DEBUG=nocompact is or isn't used.
 */
uint64_t
elk_get_compiler_config_value(const struct elk_compiler *compiler);

unsigned
elk_prog_data_size(gl_shader_stage stage);

unsigned
elk_prog_key_size(gl_shader_stage stage);

struct elk_compile_params {
   void *mem_ctx;

   nir_shader *nir;

   struct elk_compile_stats *stats;

   void *log_data;

   char *error_str;

   uint64_t debug_flag;

   uint32_t source_hash;
};

/**
 * Parameters for compiling a vertex shader.
 *
 * Some of these will be modified during the shader compilation.
 */
struct elk_compile_vs_params {
   struct elk_compile_params base;

   const struct elk_vs_prog_key *key;
   struct elk_vs_prog_data *prog_data;

   bool edgeflag_is_last; /* true for gallium */
};

/**
 * Compile a vertex shader.
 *
 * Returns the final assembly and updates the parameters structure.
 */
const unsigned *
elk_compile_vs(const struct elk_compiler *compiler,
               struct elk_compile_vs_params *params);

/**
 * Parameters for compiling a tessellation control shader.
 *
 * Some of these will be modified during the shader compilation.
 */
struct elk_compile_tcs_params {
   struct elk_compile_params base;

   const struct elk_tcs_prog_key *key;
   struct elk_tcs_prog_data *prog_data;
};

/**
 * Compile a tessellation control shader.
 *
 * Returns the final assembly and updates the parameters structure.
 */
const unsigned *
elk_compile_tcs(const struct elk_compiler *compiler,
                struct elk_compile_tcs_params *params);

/**
 * Parameters for compiling a tessellation evaluation shader.
 *
 * Some of these will be modified during the shader compilation.
 */
struct elk_compile_tes_params {
   struct elk_compile_params base;

   const struct elk_tes_prog_key *key;
   struct elk_tes_prog_data *prog_data;
   const struct intel_vue_map *input_vue_map;
};

/**
 * Compile a tessellation evaluation shader.
 *
 * Returns the final assembly and updates the parameters structure.
 */
const unsigned *
elk_compile_tes(const struct elk_compiler *compiler,
                struct elk_compile_tes_params *params);

/**
 * Parameters for compiling a geometry shader.
 *
 * Some of these will be modified during the shader compilation.
 */
struct elk_compile_gs_params {
   struct elk_compile_params base;

   const struct elk_gs_prog_key *key;
   struct elk_gs_prog_data *prog_data;
};

/**
 * Compile a geometry shader.
 *
 * Returns the final assembly and updates the parameters structure.
 */
const unsigned *
elk_compile_gs(const struct elk_compiler *compiler,
               struct elk_compile_gs_params *params);

/**
 * Compile a strips and fans shader.
 *
 * This is a fixed-function shader determined entirely by the shader key and
 * a VUE map.
 *
 * Returns the final assembly and the program's size.
 */
const unsigned *
elk_compile_sf(const struct elk_compiler *compiler,
               void *mem_ctx,
               const struct elk_sf_prog_key *key,
               struct elk_sf_prog_data *prog_data,
               struct intel_vue_map *vue_map,
               unsigned *final_assembly_size);

/**
 * Compile a clipper shader.
 *
 * This is a fixed-function shader determined entirely by the shader key and
 * a VUE map.
 *
 * Returns the final assembly and the program's size.
 */
const unsigned *
elk_compile_clip(const struct elk_compiler *compiler,
                 void *mem_ctx,
                 const struct elk_clip_prog_key *key,
                 struct elk_clip_prog_data *prog_data,
                 struct intel_vue_map *vue_map,
                 unsigned *final_assembly_size);

/**
 * Parameters for compiling a fragment shader.
 *
 * Some of these will be modified during the shader compilation.
 */
struct elk_compile_fs_params {
   struct elk_compile_params base;

   const struct elk_wm_prog_key *key;
   struct elk_wm_prog_data *prog_data;

   const struct intel_vue_map *vue_map;
   const struct elk_mue_map *mue_map;

   bool allow_spilling;
   bool use_rep_send;
   uint8_t max_polygons;
};

/**
 * Compile a fragment shader.
 *
 * Returns the final assembly and updates the parameters structure.
 */
const unsigned *
elk_compile_fs(const struct elk_compiler *compiler,
               struct elk_compile_fs_params *params);

/**
 * Parameters for compiling a compute shader.
 *
 * Some of these will be modified during the shader compilation.
 */
struct elk_compile_cs_params {
   struct elk_compile_params base;

   const struct elk_cs_prog_key *key;
   struct elk_cs_prog_data *prog_data;
};

/**
 * Compile a compute shader.
 *
 * Returns the final assembly and updates the parameters structure.
 */
const unsigned *
elk_compile_cs(const struct elk_compiler *compiler,
               struct elk_compile_cs_params *params);

/**
 * Compile a fixed function geometry shader.
 *
 * Returns the final assembly and the program's size.
 */
const unsigned *
elk_compile_ff_gs_prog(struct elk_compiler *compiler,
		       void *mem_ctx,
		       const struct elk_ff_gs_prog_key *key,
		       struct elk_ff_gs_prog_data *prog_data,
		       struct intel_vue_map *vue_map,
		       unsigned *final_assembly_size);

void elk_debug_key_recompile(const struct elk_compiler *c, void *log,
                             gl_shader_stage stage,
                             const struct elk_base_prog_key *old_key,
                             const struct elk_base_prog_key *key);

/* Shared Local Memory Size is specified as powers of two,
 * and also have a Gen-dependent minimum value if not zero.
 */
static inline uint32_t
elk_calculate_slm_size(unsigned gen, uint32_t bytes)
{
   assert(bytes <= 64 * 1024);
   if (bytes > 0)
      return MAX2(util_next_power_of_two(bytes), gen >= 9 ? 1024 : 4096);
   else
      return 0;
}

static inline uint32_t
elk_encode_slm_size(unsigned gen, uint32_t bytes)
{
   uint32_t slm_size = 0;

   /* Shared Local Memory is specified as powers of two, and encoded in
    * INTERFACE_DESCRIPTOR_DATA with the following representations:
    *
    * Size   | 0 kB | 1 kB | 2 kB | 4 kB | 8 kB | 16 kB | 32 kB | 64 kB |
    * -------------------------------------------------------------------
    * Gfx7-8 |    0 | none | none |    1 |    2 |     4 |     8 |    16 |
    * -------------------------------------------------------------------
    * Gfx9+  |    0 |    1 |    2 |    3 |    4 |     5 |     6 |     7 |
    */

   if (bytes > 0) {
      slm_size = elk_calculate_slm_size(gen, bytes);
      assert(util_is_power_of_two_nonzero(slm_size));

      assert(slm_size >= 4096);
      /* Convert to the pre-Gfx9 representation. */
      slm_size = slm_size / 4096;
   }

   return slm_size;
}

unsigned
elk_cs_push_const_total_size(const struct elk_cs_prog_data *cs_prog_data,
                             unsigned threads);

void
elk_write_shader_relocs(const struct elk_isa_info *isa,
                        void *program,
                        const struct elk_stage_prog_data *prog_data,
                        struct elk_shader_reloc_value *values,
                        unsigned num_values);

/**
 * Get the dispatch information for a shader to be used with GPGPU_WALKER and
 * similar instructions.
 *
 * If override_local_size is not NULL, it must to point to a 3-element that
 * will override the value from prog_data->local_size.  This is used by
 * ARB_compute_variable_group_size, where the size is set only at dispatch
 * time (so prog_data is outdated).
 */
struct intel_cs_dispatch_info
elk_cs_get_dispatch_info(const struct intel_device_info *devinfo,
                         const struct elk_cs_prog_data *prog_data,
                         const unsigned *override_local_size);

/**
 * Return true if the given shader stage is dispatched contiguously by the
 * relevant fixed function starting from channel 0 of the SIMD thread, which
 * implies that the dispatch mask of a thread can be assumed to have the form
 * '2^n - 1' for some n.
 */
static inline bool
elk_stage_has_packed_dispatch(ASSERTED const struct intel_device_info *devinfo,
                              gl_shader_stage stage,
                              const struct elk_stage_prog_data *prog_data)
{
   /* The code below makes assumptions about the hardware's thread dispatch
    * behavior that could be proven wrong in future generations -- Make sure
    * to do a full test run with elk_fs_test_dispatch_packing() hooked up to
    * the NIR front-end before changing this assertion.
    */
   assert(devinfo->ver <= 8);

   switch (stage) {
   case MESA_SHADER_FRAGMENT: {
      /* The PSD discards subspans coming in with no lit samples, which in the
       * per-pixel shading case implies that each subspan will either be fully
       * lit (due to the VMask being used to allow derivative computations),
       * or not dispatched at all.  In per-sample dispatch mode individual
       * samples from the same subspan have a fixed relative location within
       * the SIMD thread, so dispatch of unlit samples cannot be avoided in
       * general and we should return false.
       */
      const struct elk_wm_prog_data *wm_prog_data =
         (const struct elk_wm_prog_data *)prog_data;
      return !wm_prog_data->persample_dispatch &&
             wm_prog_data->uses_vmask;
   }
   case MESA_SHADER_COMPUTE:
      /* Compute shaders will be spawned with either a fully enabled dispatch
       * mask or with whatever bottom/right execution mask was given to the
       * GPGPU walker command to be used along the workgroup edges -- In both
       * cases the dispatch mask is required to be tightly packed for our
       * invocation index calculations to work.
       */
      return true;
   default:
      /* Most remaining fixed functions are limited to use a packed dispatch
       * mask due to the hardware representation of the dispatch mask as a
       * single counter representing the number of enabled channels.
       */
      return true;
   }
}

/**
 * Computes the first varying slot in the URB produced by the previous stage
 * that is used in the next stage. We do this by testing the varying slots in
 * the previous stage's vue map against the inputs read in the next stage.
 *
 * Note that:
 *
 * - Each URB offset contains two varying slots and we can only skip a
 *   full offset if both slots are unused, so the value we return here is always
 *   rounded down to the closest multiple of two.
 *
 * - gl_Layer and gl_ViewportIndex don't have their own varying slots, they are
 *   part of the vue header, so if these are read we can't skip anything.
 */
static inline int
elk_compute_first_urb_slot_required(uint64_t inputs_read,
                                    const struct intel_vue_map *prev_stage_vue_map)
{
   if ((inputs_read & (VARYING_BIT_LAYER | VARYING_BIT_VIEWPORT | VARYING_BIT_PRIMITIVE_SHADING_RATE)) == 0) {
      for (int i = 0; i < prev_stage_vue_map->num_slots; i++) {
         int varying = prev_stage_vue_map->slot_to_varying[i];
         if (varying > 0 && (inputs_read & BITFIELD64_BIT(varying)) != 0)
            return ROUND_DOWN_TO(i, 2);
      }
   }

   return 0;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ELK_COMPILER_H */
