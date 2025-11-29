/*
 * Copyright 2024 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef INTEL_SHADER_ENUMS_H
#define INTEL_SHADER_ENUMS_H

#include <stdint.h>

#include "compiler/shader_enums.h"
#include "util/enum_operators.h"

#ifdef __cplusplus
extern "C" {
#endif

enum intel_msaa_flags {
   /** Must be set whenever any dynamic MSAA is used
    *
    * This flag mostly exists to let us assert that the driver understands
    * dynamic MSAA so we don't run into trouble with drivers that don't.
    */
   INTEL_MSAA_FLAG_ENABLE_DYNAMIC = (1 << 0),

   /** True if the framebuffer is multisampled */
   INTEL_MSAA_FLAG_MULTISAMPLE_FBO = (1 << 1),

   /** True if this shader has been dispatched per-sample */
   INTEL_MSAA_FLAG_PERSAMPLE_DISPATCH = (1 << 2),

   /** True if inputs should be interpolated per-sample by default */
   INTEL_MSAA_FLAG_PERSAMPLE_INTERP = (1 << 3),

   /** True if this shader has been dispatched with alpha-to-coverage */
   INTEL_MSAA_FLAG_ALPHA_TO_COVERAGE = (1 << 4),

   /** True if this shader has been dispatched coarse
    *
    * This is intentionally chose to be bit 15 to correspond to the coarse bit
    * in the pixel interpolator messages.
    */
   INTEL_MSAA_FLAG_COARSE_PI_MSG = (1 << 15),

   /** True if this shader has been dispatched coarse
    *
    * This is intentionally chose to be bit 18 to correspond to the coarse bit
    * in the render target messages.
    */
   INTEL_MSAA_FLAG_COARSE_RT_WRITES = (1 << 18),
};
MESA_DEFINE_CPP_ENUM_BITFIELD_OPERATORS(intel_msaa_flags)

/**
 * @defgroup Tessellator parameter enumerations.
 *
 * These correspond to the hardware values in 3DSTATE_TE, and are provided
 * as part of the tessellation evaluation shader.
 *
 * @{
 */
enum intel_tess_partitioning {
   INTEL_TESS_PARTITIONING_INTEGER         = 0,
   INTEL_TESS_PARTITIONING_ODD_FRACTIONAL  = 1,
   INTEL_TESS_PARTITIONING_EVEN_FRACTIONAL = 2,
};

enum intel_tess_output_topology {
   INTEL_TESS_OUTPUT_TOPOLOGY_POINT   = 0,
   INTEL_TESS_OUTPUT_TOPOLOGY_LINE    = 1,
   INTEL_TESS_OUTPUT_TOPOLOGY_TRI_CW  = 2,
   INTEL_TESS_OUTPUT_TOPOLOGY_TRI_CCW = 3,
};

enum intel_tess_domain {
   INTEL_TESS_DOMAIN_QUAD    = 0,
   INTEL_TESS_DOMAIN_TRI     = 1,
   INTEL_TESS_DOMAIN_ISOLINE = 2,
};
/** @} */

enum intel_shader_dispatch_mode {
   INTEL_DISPATCH_MODE_4X1_SINGLE = 0,
   INTEL_DISPATCH_MODE_4X2_DUAL_INSTANCE = 1,
   INTEL_DISPATCH_MODE_4X2_DUAL_OBJECT = 2,
   INTEL_DISPATCH_MODE_SIMD8 = 3,

   INTEL_DISPATCH_MODE_TCS_SINGLE_PATCH = 0,
   INTEL_DISPATCH_MODE_TCS_MULTI_PATCH = 2,
};

/**
 * Data structure recording the relationship between the gl_varying_slot enum
 * and "slots" within the vertex URB entry (VUE).  A "slot" is defined as a
 * single octaword within the VUE (128 bits).
 *
 * Note that each BRW register contains 256 bits (2 octawords), so when
 * accessing the VUE in URB_NOSWIZZLE mode, each register corresponds to two
 * consecutive VUE slots.  When accessing the VUE in URB_INTERLEAVED mode (as
 * in a vertex shader), each register corresponds to a single VUE slot, since
 * it contains data for two separate vertices.
 */
struct intel_vue_map {
   /**
    * Bitfield representing all varying slots that are (a) stored in this VUE
    * map, and (b) actually written by the shader.  Does not include any of
    * the additional varying slots defined in brw_varying_slot.
    */
   uint64_t slots_valid;

   /**
    * Is this VUE map for a separate shader pipeline?
    *
    * Separable programs (GL_ARB_separate_shader_objects) can be mixed and matched
    * without the linker having a chance to dead code eliminate unused varyings.
    *
    * This means that we have to use a fixed slot layout, based on the output's
    * location field, rather than assigning slots in a compact contiguous block.
    */
   bool separate;

   /**
    * Map from gl_varying_slot value to VUE slot.  For gl_varying_slots that are
    * not stored in a slot (because they are not written, or because
    * additional processing is applied before storing them in the VUE), the
    * value is -1.
    */
   signed char varying_to_slot[VARYING_SLOT_TESS_MAX];

   /**
    * Map from VUE slot to gl_varying_slot value.  For slots that do not
    * directly correspond to a gl_varying_slot, the value comes from
    * brw_varying_slot.
    *
    * For slots that are not in use, the value is BRW_VARYING_SLOT_PAD.
    */
   signed char slot_to_varying[VARYING_SLOT_TESS_MAX];

   /**
    * Total number of VUE slots in use
    */
   int num_slots;

   /**
    * Number of position VUE slots.  If num_pos_slots > 1, primitive
    * replication is being used.
    */
   int num_pos_slots;

   /**
    * Number of per-patch VUE slots. Only valid for tessellation control
    * shader outputs and tessellation evaluation shader inputs.
    */
   int num_per_patch_slots;

   /**
    * Number of per-vertex VUE slots. Only valid for tessellation control
    * shader outputs and tessellation evaluation shader inputs.
    */
   int num_per_vertex_slots;
};

struct intel_cs_dispatch_info {
   uint32_t group_size;
   uint32_t simd_size;
   uint32_t threads;

   /* RightExecutionMask field used in GPGPU_WALKER. */
   uint32_t right_mask;
};

enum PACKED intel_compute_walk_order {
   INTEL_WALK_ORDER_XYZ = 0,
   INTEL_WALK_ORDER_XZY = 1,
   INTEL_WALK_ORDER_YXZ = 2,
   INTEL_WALK_ORDER_YZX = 3,
   INTEL_WALK_ORDER_ZXY = 4,
   INTEL_WALK_ORDER_ZYX = 5,
};

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* INTEL_SHADER_ENUMS_H */
