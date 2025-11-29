/*
 * Copyright 2024 Alyssa Rosenzweig
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "agx_bo.h"
#include "agx_compile.h"
#include "agx_nir_lower_vbo.h"
#include "agx_pack.h"
#include "nir_lower_blend.h"

struct agx_linked_shader {
   /* Mapped executable memory */
   struct agx_bo *bo;

   /* Set if the linked SW vertex shader reads base vertex/instance. The VS
    * prolog can read base instance even when the API VS does not, which is why
    * this needs to be aggregated in the linker.
    */
   bool uses_base_param;

   /* Coefficient register bindings */
   struct agx_varyings_fs cf;

   /* Data structures packed for the linked program */
   struct agx_usc_shader_packed shader;
   struct agx_usc_registers_packed regs;
   struct agx_usc_fragment_properties_packed fragment_props;
   struct agx_output_select_packed osel;
   struct agx_fragment_control_packed fragment_control;
};

struct agx_linked_shader *
agx_fast_link(void *memctx, struct agx_device *dev, bool fragment,
              struct agx_shader_part *main, struct agx_shader_part *prolog,
              struct agx_shader_part *epilog, unsigned nr_samples_shaded);

/* These parts of the vertex element affect the generated code */
struct agx_velem_key {
   uint32_t divisor;
   uint16_t stride;
   uint8_t format;
   uint8_t pad;
};

struct agx_vs_prolog_key {
   struct agx_velem_key attribs[AGX_MAX_VBUFS];

   /* Bit mask of attribute components to load */
   BITSET_DECLARE(component_mask, VERT_ATTRIB_MAX * 4);

   /* Whether running as a hardware vertex shader (versus compute) */
   bool hw;

   /* If !hw and the draw call is indexed, the index size */
   uint8_t sw_index_size_B;
};

struct agx_fs_prolog_key {
   /* glSampleMask() mask */
   uint8_t api_sample_mask;

   /* Number of cull planes requiring lowering */
   uint8_t cull_distance_size;

   /* Need to count FRAGMENT_SHADER_INVOCATIONS */
   bool statistics;

   /* Need to lower desktop OpenGL polygon stipple */
   bool polygon_stipple;

   /* If we discard, whether we need to run Z/S tests */
   bool run_zs_tests;

   /* If we emulate cull distance, the base offset for our allocated coefficient
    * registers so we don't interfere with the main shader.
    */
   unsigned cf_base;
};

struct agx_blend_key {
   nir_lower_blend_rt rt[8];
   unsigned logicop_func;
   bool alpha_to_coverage, alpha_to_one;
   bool padding[2];
};
static_assert(sizeof(struct agx_blend_key) == 232, "packed");

struct agx_fs_epilog_link_info {
   /* Base index of spilled render targets in the binding table */
   uint8_t rt_spill_base;

   /* Bit mask of the bit size written to each render target. Bit i set if RT i
    * uses 32-bit registers, else 16-bit registers.
    */
   uint8_t size_32;

   /* If set, the API fragment shader uses sample shading. This means the epilog
    * will be invoked per-sample as well.
    */
   bool sample_shading;

   /* If set, broadcast the render target #0 value to all render targets. This
    * implements gl_FragColor semantics.
    */
   bool broadcast_rt0;

   /* If set, force render target 0's W channel to 1.0. This optimizes blending
    * calculations in some applications.
    */
   bool rt0_w_1;

   /* If set, the API fragment shader wants to write depth/stencil respectively.
    * This happens in the epilog for correctness when the epilog discards.
    */
   bool write_z, write_s;
};

struct agx_fs_epilog_key {
   /* Mask of render targets written by the main shader */
   uint8_t rt_written;

   struct agx_fs_epilog_link_info link;

   /* Blend state. Blending happens in the epilog. */
   struct agx_blend_key blend;

   /* Tilebuffer configuration */
   enum pipe_format rt_formats[8];
   uint8_t nr_samples;
   bool force_small_tile;
};

void agx_nir_vs_prolog(struct nir_builder *b, const void *key_);
void agx_nir_fs_epilog(struct nir_builder *b, const void *key_);
void agx_nir_fs_prolog(struct nir_builder *b, const void *key_);

bool agx_nir_lower_vs_input_to_prolog(nir_shader *s,
                                      BITSET_WORD *attrib_components_read);

bool agx_nir_lower_fs_output_to_epilog(nir_shader *s,
                                       struct agx_fs_epilog_link_info *out);

bool agx_nir_lower_fs_active_samples_to_register(nir_shader *s);
