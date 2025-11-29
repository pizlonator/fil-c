/*
 * Copyright © 2022 Collabora, Ltd.
 * SPDX-License-Identifier: MIT
 */

#ifndef NAK_H
#define NAK_H

#include "compiler/shader_enums.h"
#include "nir.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct nak_compiler;
struct nir_shader_compiler_options;
struct nv_device_info;

struct nak_compiler *nak_compiler_create(const struct nv_device_info *dev);
void nak_compiler_destroy(struct nak_compiler *nak);

uint64_t nak_debug_flags(const struct nak_compiler *nak);

const struct nir_shader_compiler_options *
nak_nir_options(const struct nak_compiler *nak);

void nak_optimize_nir(nir_shader *nir, const struct nak_compiler *nak);
void nak_preprocess_nir(nir_shader *nir, const struct nak_compiler *nak);

PRAGMA_DIAGNOSTIC_PUSH
PRAGMA_DIAGNOSTIC_ERROR(-Wpadded)
struct nak_fs_key {
   bool zs_self_dep;

   /** True if sample shading is forced on via an API knob such as
    * VkPipelineMultisampleStateCreateInfo::minSampleShading
    */
   bool force_sample_shading;

   uint8_t _pad;

   /**
    * The constant buffer index and offset at which the sample locations table lives.
    * Each sample location is two 4-bit unorm values packed into an 8-bit value
    * with the bottom 4 bits for x and the top 4 bits for y.
   */
   uint8_t sample_locations_cb;
   uint32_t sample_locations_offset;
};
PRAGMA_DIAGNOSTIC_POP
static_assert(sizeof(struct nak_fs_key) == 8, "This struct has no holes");


void nak_postprocess_nir(nir_shader *nir, const struct nak_compiler *nak,
                         nir_variable_mode robust2_modes,
                         const struct nak_fs_key *fs_key);

enum ENUM_PACKED nak_ts_domain {
   NAK_TS_DOMAIN_ISOLINE = 0,
   NAK_TS_DOMAIN_TRIANGLE = 1,
   NAK_TS_DOMAIN_QUAD = 2,
};

enum ENUM_PACKED nak_ts_spacing {
   NAK_TS_SPACING_INTEGER = 0,
   NAK_TS_SPACING_FRACT_ODD = 1,
   NAK_TS_SPACING_FRACT_EVEN = 2,
};

enum ENUM_PACKED nak_ts_prims {
   NAK_TS_PRIMS_POINTS = 0,
   NAK_TS_PRIMS_LINES = 1,
   NAK_TS_PRIMS_TRIANGLES_CW = 2,
   NAK_TS_PRIMS_TRIANGLES_CCW = 3,
};

struct nak_xfb_info {
   uint32_t stride[4];
   uint8_t stream[4];
   uint8_t attr_count[4];
   uint8_t attr_index[4][128];
};

/* This struct MUST have explicit padding fields to ensure that all padding is
 * zeroed and the zeros get properly copied, even across API boundaries.  This
 * is ensured in two ways:
 *
 *  - Bindgen is invoked with --explicit-padding and if a __bindgen_paddingN
 *    member ever crops up, that tells us we need to add an explicit member
 *    here.
 *
 *  - There is a set of const asserts in nak/api.rs which ensure that all of
 *    the union fields are equal to NAK_SHADER_INFO_STAGE_UNION_SIZE.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wpadded"
struct nak_shader_info {
   gl_shader_stage stage;

   /** Shader model */
   uint8_t sm;

   /** Number of GPRs used */
   uint8_t num_gprs;

   /** Number of barriers used */
   uint8_t num_barriers;

   uint8_t _pad0;

   /** Size of shader local (scratch) memory */
   uint32_t slm_size;

   union {
      struct {
         /* Local workgroup size */
         uint16_t local_size[3];

         /* Shared memory size */
         uint16_t smem_size;

         uint8_t _pad[4];
      } cs;

      struct {
         bool writes_depth;
         bool reads_sample_mask;
         bool post_depth_coverage;
         bool uses_sample_shading;
         bool early_fragment_tests;

         uint8_t _pad[7];
      } fs;

      struct {
         enum nak_ts_domain domain;
         enum nak_ts_spacing spacing;
         enum nak_ts_prims prims;

         uint8_t _pad[9];
      } ts;

      /* Used to initialize the union for other stages */
      uint8_t _pad[12];
   };

   struct {
      bool writes_layer;
      bool writes_point_size;
      uint8_t clip_enable;
      uint8_t cull_enable;

      struct nak_xfb_info xfb;
   } vtg;

   /** Shader header for 3D stages */
   uint32_t hdr[32];
};
#pragma GCC diagnostic pop

struct nak_shader_bin {
   struct nak_shader_info info;

   uint32_t code_size;
   const void *code;

   const char *asm_str;
};

void nak_shader_bin_destroy(struct nak_shader_bin *bin);

struct nak_shader_bin *
nak_compile_shader(nir_shader *nir, bool dump_asm,
                   const struct nak_compiler *nak,
                   nir_variable_mode robust2_modes,
                   const struct nak_fs_key *fs_key);

struct nak_qmd_cbuf {
   uint32_t index;
   uint32_t size;
   uint64_t addr;
};

struct nak_qmd_info {
   uint64_t addr;

   uint16_t smem_size;
   uint16_t smem_max;

   uint32_t global_size[3];

   uint32_t num_cbufs;
   struct nak_qmd_cbuf cbufs[8];
};

void nak_fill_qmd(const struct nv_device_info *dev,
                  const struct nak_shader_info *info,
                  const struct nak_qmd_info *qmd_info,
                  void *qmd_out, size_t qmd_size);

uint32_t nak_qmd_dispatch_size_offset(const struct nv_device_info *dev);

#ifdef __cplusplus
}
#endif

#endif /* NAK_H */
