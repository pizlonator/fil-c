/*
 * Copyright © 2016 Intel Corporation
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

#ifndef BLORP_GENX_EXEC_ELK_H
#define BLORP_GENX_EXEC_ELK_H

#if GFX_VER > 8
#error "ELK doesn't support Gfx > 8."
#endif

#include "blorp_priv.h"
#include "dev/intel_device_info.h"
#include "common/intel_sample_positions.h"
#include "common/intel_l3_config.h"
#include "genxml/gen_macros.h"
#include "intel/compiler/elk/elk_compiler.h"

/**
 * This file provides the blorp pipeline setup and execution functionality.
 * It defines the following function:
 *
 * static void
 * blorp_exec(struct blorp_context *blorp, void *batch_data,
 *            const struct blorp_params *params);
 *
 * It is the job of whoever includes this header to wrap this in something
 * to get an externally visible symbol.
 *
 * In order for the blorp_exec function to work, the driver must provide
 * implementations of the following static helper functions.
 */

static void *
blorp_emit_dwords(struct blorp_batch *batch, unsigned n);

static uint64_t
blorp_emit_reloc(struct blorp_batch *batch,
                 void *location, struct blorp_address address, uint32_t delta);

static void
blorp_measure_start(struct blorp_batch *batch,
                    const struct blorp_params *params);

static void
blorp_measure_end(struct blorp_batch *batch,
                  const struct blorp_params *params);

static void *
blorp_alloc_dynamic_state(struct blorp_batch *batch,
                          uint32_t size,
                          uint32_t alignment,
                          uint32_t *offset);

UNUSED static void *
blorp_alloc_general_state(struct blorp_batch *batch,
                          uint32_t size,
                          uint32_t alignment,
                          uint32_t *offset);

static void *
blorp_alloc_vertex_buffer(struct blorp_batch *batch, uint32_t size,
                          struct blorp_address *addr);
static void
blorp_vf_invalidate_for_vb_48b_transitions(struct blorp_batch *batch,
                                           const struct blorp_address *addrs,
                                           uint32_t *sizes,
                                           unsigned num_vbs);

UNUSED static struct blorp_address
blorp_get_workaround_address(struct blorp_batch *batch);

static bool
blorp_alloc_binding_table(struct blorp_batch *batch, unsigned num_entries,
                          unsigned state_size, unsigned state_alignment,
                          uint32_t *bt_offset, uint32_t *surface_offsets,
                          void **surface_maps);

static uint32_t
blorp_binding_table_offset_to_pointer(struct blorp_batch *batch,
                                      uint32_t offset);

static void
blorp_flush_range(struct blorp_batch *batch, void *start, size_t size);

static void
blorp_surface_reloc(struct blorp_batch *batch, uint32_t ss_offset,
                    struct blorp_address address, uint32_t delta);

static uint64_t
blorp_get_surface_address(struct blorp_batch *batch,
                          struct blorp_address address);

#if GFX_VER >= 7
static struct blorp_address
blorp_get_surface_base_address(struct blorp_batch *batch);
#endif

#if GFX_VER >= 7
static const struct intel_l3_config *
blorp_get_l3_config(struct blorp_batch *batch);
#endif

static void
blorp_pre_emit_urb_config(struct blorp_batch *batch,
                          struct intel_urb_config *urb_config);

static void
blorp_emit_urb_config(struct blorp_batch *batch,
                      struct intel_urb_config *urb_config);

static void
blorp_emit_pipeline(struct blorp_batch *batch,
                    const struct blorp_params *params);

static void
blorp_emit_pre_draw(struct blorp_batch *batch,
                    const struct blorp_params *params);
static void
blorp_emit_post_draw(struct blorp_batch *batch,
                     const struct blorp_params *params);

static inline unsigned
elk_blorp_get_urb_length(const struct elk_wm_prog_data *prog_data)
{
   if (prog_data == NULL)
      return 1;

   /* From the BSpec: 3D Pipeline - Strips and Fans - 3DSTATE_SBE
    *
    * read_length = ceiling((max_source_attr+1)/2)
    */
   return MAX2((prog_data->num_varying_inputs + 1) / 2, 1);
}

/***** BEGIN blorp_exec implementation ******/

static uint64_t
_blorp_combine_address(struct blorp_batch *batch, void *location,
                       struct blorp_address address, uint32_t delta)
{
   if (address.buffer == NULL) {
      return address.offset + delta;
   } else {
      return blorp_emit_reloc(batch, location, address, delta);
   }
}

#define __gen_address_type struct blorp_address
#define __gen_user_data struct blorp_batch
#define __gen_combine_address _blorp_combine_address

#include "genxml/genX_pack.h"
#include "common/intel_genX_state_elk.h"

#define _blorp_cmd_length(cmd) cmd ## _length
#define _blorp_cmd_length_bias(cmd) cmd ## _length_bias
#define _blorp_cmd_header(cmd) cmd ## _header
#define _blorp_cmd_pack(cmd) cmd ## _pack

#define blorp_emit(batch, cmd, name)                              \
   for (struct cmd name = { _blorp_cmd_header(cmd) },             \
        *_dst = blorp_emit_dwords(batch, _blorp_cmd_length(cmd)); \
        __builtin_expect(_dst != NULL, 1);                        \
        _blorp_cmd_pack(cmd)(batch, (void *)_dst, &name),         \
        _dst = NULL)

#define blorp_emitn(batch, cmd, n, ...) ({                  \
      uint32_t *_dw = blorp_emit_dwords(batch, n);          \
      if (_dw) {                                            \
         struct cmd template = {                            \
            _blorp_cmd_header(cmd),                         \
            .DWordLength = n - _blorp_cmd_length_bias(cmd), \
            __VA_ARGS__                                     \
         };                                                 \
         _blorp_cmd_pack(cmd)(batch, _dw, &template);       \
      }                                                     \
      _dw ? _dw + 1 : NULL; /* Array starts at dw[1] */     \
   })

#define STRUCT_ZERO(S) ({ struct S t; memset(&t, 0, sizeof(t)); t; })

#define blorp_emit_dynamic(batch, state, name, align, offset)      \
   for (struct state name = STRUCT_ZERO(state),                         \
        *_dst = blorp_alloc_dynamic_state(batch,                   \
                                          _blorp_cmd_length(state) * 4, \
                                          align, offset);               \
        __builtin_expect(_dst != NULL, 1);                              \
        _blorp_cmd_pack(state)(batch, (void *)_dst, &name),             \
        blorp_flush_range(batch, _dst, _blorp_cmd_length(state) * 4),   \
        _dst = NULL)

/* 3DSTATE_URB
 * 3DSTATE_URB_VS
 * 3DSTATE_URB_HS
 * 3DSTATE_URB_DS
 * 3DSTATE_URB_GS
 *
 * Assign the entire URB to the VS. Even though the VS disabled, URB space
 * is still needed because the clipper loads the VUE's from the URB. From
 * the Sandybridge PRM, Volume 2, Part 1, Section 3DSTATE,
 * Dword 1.15:0 "VS Number of URB Entries":
 *     This field is always used (even if VS Function Enable is DISABLED).
 *
 * The warning below appears in the PRM (Section 3DSTATE_URB), but we can
 * safely ignore it because this batch contains only one draw call.
 *     Because of URB corruption caused by allocating a previous GS unit
 *     URB entry to the VS unit, software is required to send a “GS NULL
 *     Fence” (Send URB fence with VS URB size == 1 and GS URB size == 0)
 *     plus a dummy DRAW call before any case where VS will be taking over
 *     GS URB space.
 *
 * If the 3DSTATE_URB_VS is emitted, than the others must be also.
 * From the Ivybridge PRM, Volume 2 Part 1, section 1.7.1 3DSTATE_URB_VS:
 *
 *     3DSTATE_URB_HS, 3DSTATE_URB_DS, and 3DSTATE_URB_GS must also be
 *     programmed in order for the programming of this state to be
 *     valid.
 */
static void
emit_urb_config(struct blorp_batch *batch,
                const struct blorp_params *params,
                UNUSED enum intel_urb_deref_block_size *deref_block_size)
{
   /* Once vertex fetcher has written full VUE entries with complete
    * header the space requirement is as follows per vertex (in bytes):
    *
    *     Header    Position    Program constants
    *   +--------+------------+-------------------+
    *   |   16   |     16     |      n x 16       |
    *   +--------+------------+-------------------+
    *
    * where 'n' stands for number of varying inputs expressed as vec4s.
    */
   struct elk_wm_prog_data *wm_prog_data = params->wm_prog_data;
   const unsigned num_varyings =
      wm_prog_data ? wm_prog_data->num_varying_inputs : 0;
   const unsigned total_needed = 16 + 16 + num_varyings * 16;

   /* The URB size is expressed in units of 64 bytes (512 bits) */
   const unsigned vs_entry_size = DIV_ROUND_UP(total_needed, 64);

   ASSERTED struct elk_sf_prog_data *sf_prog_data = params->sf_prog_data;
   ASSERTED const unsigned sf_entry_size =
      sf_prog_data ? sf_prog_data->urb_entry_size : 0;

#if GFX_VER >= 7
   assert(sf_entry_size == 0);

   struct intel_urb_config urb_cfg = {
      .size = { vs_entry_size, 1, 1, 1 },
   };

   bool constrained;
   intel_get_urb_config(batch->blorp->compiler->elk->devinfo,
                        blorp_get_l3_config(batch),
                        false, false, &urb_cfg,
                        deref_block_size, &constrained);

   /* Tell drivers about the config. */
   blorp_pre_emit_urb_config(batch, &urb_cfg);

#if GFX_VERx10 == 70
   /* From the IVB PRM Vol. 2, Part 1, Section 3.2.1:
    *
    *    "A PIPE_CONTROL with Post-Sync Operation set to 1h and a depth stall
    *    needs to be sent just prior to any 3DSTATE_VS, 3DSTATE_URB_VS,
    *    3DSTATE_CONSTANT_VS, 3DSTATE_BINDING_TABLE_POINTER_VS,
    *    3DSTATE_SAMPLER_STATE_POINTER_VS command.  Only one PIPE_CONTROL
    *    needs to be sent before any combination of VS associated 3DSTATE."
    */
   blorp_emit(batch, GENX(PIPE_CONTROL), pc) {
      pc.DepthStallEnable  = true;
      pc.PostSyncOperation = WriteImmediateData;
      pc.Address           = blorp_get_workaround_address(batch);
   }
#endif

   for (int i = 0; i <= MESA_SHADER_GEOMETRY; i++) {
      blorp_emit(batch, GENX(3DSTATE_URB_VS), urb) {
         urb._3DCommandSubOpcode      += i;
         urb.VSURBStartingAddress      = urb_cfg.start[i];
         urb.VSURBEntryAllocationSize  = urb_cfg.size[i] - 1;
         urb.VSNumberofURBEntries      = urb_cfg.entries[i];
      }
   }

#else /* GFX_VER < 7 */
   struct intel_urb_config urb_cfg = {
      .size = { vs_entry_size, 0, 0, 0, sf_entry_size, },
   };
   blorp_emit_urb_config(batch, &urb_cfg);
#endif
}

#if GFX_VER >= 7
static void
blorp_emit_memcpy(struct blorp_batch *batch,
                  struct blorp_address dst,
                  struct blorp_address src,
                  uint32_t size);
#endif

static void
blorp_emit_vertex_data(struct blorp_batch *batch,
                       const struct blorp_params *params,
                       struct blorp_address *addr,
                       uint32_t *size)
{
   const float vertices[] = {
      /* v0 */ (float)params->x1, (float)params->y1, params->z,
      /* v1 */ (float)params->x0, (float)params->y1, params->z,
      /* v2 */ (float)params->x0, (float)params->y0, params->z,
   };

   void *data = blorp_alloc_vertex_buffer(batch, sizeof(vertices), addr);
   memcpy(data, vertices, sizeof(vertices));
   *size = sizeof(vertices);
   blorp_flush_range(batch, data, *size);
}

static void
blorp_emit_input_varying_data(struct blorp_batch *batch,
                              const struct blorp_params *params,
                              struct blorp_address *addr,
                              uint32_t *size)
{
   const unsigned vec4_size_in_bytes = 4 * sizeof(float);
   const unsigned max_num_varyings =
      DIV_ROUND_UP(sizeof(params->wm_inputs), vec4_size_in_bytes);
   struct elk_wm_prog_data *wm_prog_data = params->wm_prog_data;
   const unsigned num_varyings =
      wm_prog_data ? wm_prog_data->num_varying_inputs : 0;

   *size = 16 + num_varyings * vec4_size_in_bytes;

   const uint32_t *const inputs_src = (const uint32_t *)&params->wm_inputs;
   void *data = blorp_alloc_vertex_buffer(batch, *size, addr);
   if (data == NULL)
      return;
   uint32_t *inputs = data;

   /* Copy in the VS inputs */
   assert(sizeof(params->vs_inputs) == 16);
   memcpy(inputs, &params->vs_inputs, sizeof(params->vs_inputs));
   inputs += 4;

   if (params->wm_prog_data) {
      /* Walk over the attribute slots, determine if the attribute is used by
       * the program and when necessary copy the values from the input storage
       * to the vertex data buffer.
       */
      for (unsigned i = 0; i < max_num_varyings; i++) {
         const gl_varying_slot attr = VARYING_SLOT_VAR0 + i;

         const int input_index = wm_prog_data->urb_setup[attr];
         if (input_index < 0)
            continue;

         memcpy(inputs, inputs_src + i * 4, vec4_size_in_bytes);

         inputs += 4;
      }
   }

   blorp_flush_range(batch, data, *size);

   if (params->dst_clear_color_as_input) {
#if GFX_VER >= 7
      /* In this case, the clear color isn't known statically and instead
       * comes in through an indirect which we have to copy into the vertex
       * buffer before we execute the 3DPRIMITIVE.  We already copied the
       * value of params->wm_inputs.clear_color into the vertex buffer in the
       * loop above.  Now we emit code to stomp it from the GPU with the
       * actual clear color value.
       */
      assert(num_varyings == 1);

      /* The clear color is the first thing after the header */
      struct blorp_address clear_color_input_addr = *addr;
      clear_color_input_addr.offset += 16;

      const unsigned clear_color_size = batch->blorp->isl_dev->ss.clear_value_size;
      blorp_emit_memcpy(batch, clear_color_input_addr,
                        params->dst.clear_color_addr,
                        clear_color_size);
#else
      unreachable("MCS partial resolve is not a thing on SNB and earlier");
#endif
   }
}

static void
blorp_fill_vertex_buffer_state(struct GENX(VERTEX_BUFFER_STATE) *vb,
                               unsigned idx,
                               struct blorp_address addr, uint32_t size,
                               uint32_t stride)
{
   vb[idx].VertexBufferIndex = idx;
   vb[idx].BufferStartingAddress = addr;
   vb[idx].BufferPitch = stride;

#if GFX_VER >= 6
   vb[idx].MOCS = addr.mocs;
#endif

#if GFX_VER >= 7
   vb[idx].AddressModifyEnable = true;
#endif

#if GFX_VER >= 8
   vb[idx].BufferSize = size;
#elif GFX_VER >= 5
   vb[idx].BufferAccessType = stride > 0 ? VERTEXDATA : INSTANCEDATA;
   vb[idx].EndAddress = vb[idx].BufferStartingAddress;
   vb[idx].EndAddress.offset += size - 1;
#elif GFX_VER == 4
   vb[idx].BufferAccessType = stride > 0 ? VERTEXDATA : INSTANCEDATA;
   vb[idx].MaxIndex = stride > 0 ? size / stride : 0;
#endif
}

static void
blorp_emit_vertex_buffers(struct blorp_batch *batch,
                          const struct blorp_params *params)
{
   struct GENX(VERTEX_BUFFER_STATE) vb[2] = {};
   const uint32_t num_vbs = ARRAY_SIZE(vb);

   struct blorp_address addrs[2] = {};
   uint32_t sizes[2] = {};
   blorp_emit_vertex_data(batch, params, &addrs[0], &sizes[0]);
   if (sizes[0] == 0)
      return;
   blorp_fill_vertex_buffer_state(vb, 0, addrs[0], sizes[0],
                                  3 * sizeof(float));

   blorp_emit_input_varying_data(batch, params, &addrs[1], &sizes[1]);
   blorp_fill_vertex_buffer_state(vb, 1, addrs[1], sizes[1], 0);

   blorp_vf_invalidate_for_vb_48b_transitions(batch, addrs, sizes, num_vbs);

   const unsigned num_dwords = 1 + num_vbs * GENX(VERTEX_BUFFER_STATE_length);
   uint32_t *dw = blorp_emitn(batch, GENX(3DSTATE_VERTEX_BUFFERS), num_dwords);
   if (!dw)
      return;

   for (unsigned i = 0; i < num_vbs; i++) {
      GENX(VERTEX_BUFFER_STATE_pack)(batch, dw, &vb[i]);
      dw += GENX(VERTEX_BUFFER_STATE_length);
   }
}

static void
blorp_emit_vertex_elements(struct blorp_batch *batch,
                           const struct blorp_params *params)
{
   struct elk_wm_prog_data *wm_prog_data = params->wm_prog_data;
   const unsigned num_varyings =
      wm_prog_data ? wm_prog_data->num_varying_inputs : 0;
   bool need_ndc = batch->blorp->compiler->elk->devinfo->ver <= 5;
   const unsigned num_elements = 2 + need_ndc + num_varyings;

   struct GENX(VERTEX_ELEMENT_STATE) ve[num_elements];
   memset(ve, 0, num_elements * sizeof(*ve));

   /* Setup VBO for the rectangle primitive..
    *
    * A rectangle primitive (3DPRIM_RECTLIST) consists of only three
    * vertices. The vertices reside in screen space with DirectX
    * coordinates (that is, (0, 0) is the upper left corner).
    *
    *   v2 ------ implied
    *    |        |
    *    |        |
    *   v1 ----- v0
    *
    * Since the VS is disabled, the clipper loads each VUE directly from
    * the URB. This is controlled by the 3DSTATE_VERTEX_BUFFERS and
    * 3DSTATE_VERTEX_ELEMENTS packets below. The VUE contents are as follows:
    *   dw0: Reserved, MBZ.
    *   dw1: Render Target Array Index. Below vertex fetcher gets programmed
    *        to assign this with primitive instance identifier which will be
    *        used for layered clears. All other renders have only one instance
    *        and therefore the value will be effectively zero.
    *   dw2: Viewport Index. The HiZ op disables viewport mapping and
    *        scissoring, so set the dword to 0.
    *   dw3: Point Width: The HiZ op does not emit the POINTLIST primitive,
    *        so set the dword to 0.
    *   dw4: Vertex Position X.
    *   dw5: Vertex Position Y.
    *   dw6: Vertex Position Z.
    *   dw7: Vertex Position W.
    *
    *   dw8: Flat vertex input 0
    *   dw9: Flat vertex input 1
    *   ...
    *   dwn: Flat vertex input n - 8
    *
    * For details, see the Sandybridge PRM, Volume 2, Part 1, Section 1.5.1
    * "Vertex URB Entry (VUE) Formats".
    *
    * Only vertex position X and Y are going to be variable, Z is fixed to
    * zero and W to one. Header words dw0,2,3 are zero. There is no need to
    * include the fixed values in the vertex buffer. Vertex fetcher can be
    * instructed to fill vertex elements with constant values of one and zero
    * instead of reading them from the buffer.
    * Flat inputs are program constants that are not interpolated. Moreover
    * their values will be the same between vertices.
    *
    * See the vertex element setup below.
    */
   unsigned slot = 0;

   ve[slot] = (struct GENX(VERTEX_ELEMENT_STATE)) {
      .VertexBufferIndex = 1,
      .Valid = true,
      .SourceElementFormat = ISL_FORMAT_R32G32B32A32_FLOAT,
      .SourceElementOffset = 0,
      .Component0Control = VFCOMP_STORE_SRC,

      /* From Gfx8 onwards hardware is no more instructed to overwrite
       * components using an element specifier. Instead one has separate
       * 3DSTATE_VF_SGVS (System Generated Value Setup) state packet for it.
       */
#if GFX_VER >= 8
      .Component1Control = VFCOMP_STORE_0,
#elif GFX_VER >= 5
      .Component1Control = VFCOMP_STORE_IID,
#else
      .Component1Control = VFCOMP_STORE_0,
#endif
      .Component2Control = VFCOMP_STORE_0,
      .Component3Control = VFCOMP_STORE_0,
#if GFX_VER <= 5
      .DestinationElementOffset = slot * 4,
#endif
   };
   slot++;

#if GFX_VER <= 5
   /* On Iron Lake and earlier, a native device coordinates version of the
    * position goes right after the normal VUE header and before position.
    * Since w == 1 for all of our coordinates, this is just a copy of the
    * position.
    */
   ve[slot] = (struct GENX(VERTEX_ELEMENT_STATE)) {
      .VertexBufferIndex = 0,
      .Valid = true,
      .SourceElementFormat = ISL_FORMAT_R32G32B32_FLOAT,
      .SourceElementOffset = 0,
      .Component0Control = VFCOMP_STORE_SRC,
      .Component1Control = VFCOMP_STORE_SRC,
      .Component2Control = VFCOMP_STORE_SRC,
      .Component3Control = VFCOMP_STORE_1_FP,
      .DestinationElementOffset = slot * 4,
   };
   slot++;
#endif

   ve[slot] = (struct GENX(VERTEX_ELEMENT_STATE)) {
      .VertexBufferIndex = 0,
      .Valid = true,
      .SourceElementFormat = ISL_FORMAT_R32G32B32_FLOAT,
      .SourceElementOffset = 0,
      .Component0Control = VFCOMP_STORE_SRC,
      .Component1Control = VFCOMP_STORE_SRC,
      .Component2Control = VFCOMP_STORE_SRC,
      .Component3Control = VFCOMP_STORE_1_FP,
#if GFX_VER <= 5
      .DestinationElementOffset = slot * 4,
#endif
   };
   slot++;

   for (unsigned i = 0; i < num_varyings; ++i) {
      ve[slot] = (struct GENX(VERTEX_ELEMENT_STATE)) {
         .VertexBufferIndex = 1,
         .Valid = true,
         .SourceElementFormat = ISL_FORMAT_R32G32B32A32_FLOAT,
         .SourceElementOffset = 16 + i * 4 * sizeof(float),
         .Component0Control = VFCOMP_STORE_SRC,
         .Component1Control = VFCOMP_STORE_SRC,
         .Component2Control = VFCOMP_STORE_SRC,
         .Component3Control = VFCOMP_STORE_SRC,
#if GFX_VER <= 5
         .DestinationElementOffset = slot * 4,
#endif
      };
      slot++;
   }

   const unsigned num_dwords =
      1 + GENX(VERTEX_ELEMENT_STATE_length) * num_elements;
   uint32_t *dw = blorp_emitn(batch, GENX(3DSTATE_VERTEX_ELEMENTS), num_dwords);
   if (!dw)
      return;

   for (unsigned i = 0; i < num_elements; i++) {
      GENX(VERTEX_ELEMENT_STATE_pack)(batch, dw, &ve[i]);
      dw += GENX(VERTEX_ELEMENT_STATE_length);
   }

   blorp_emit(batch, GENX(3DSTATE_VF_STATISTICS), vf) {
      vf.StatisticsEnable = false;
   }

#if GFX_VER >= 8
   /* Overwrite Render Target Array Index (2nd dword) in the VUE header with
    * primitive instance identifier. This is used for layered clears.
    */
   blorp_emit(batch, GENX(3DSTATE_VF_SGVS), sgvs) {
      sgvs.InstanceIDEnable = true;
      sgvs.InstanceIDComponentNumber = COMP_1;
      sgvs.InstanceIDElementOffset = 0;
   }

   for (unsigned i = 0; i < num_elements; i++) {
      blorp_emit(batch, GENX(3DSTATE_VF_INSTANCING), vf) {
         vf.VertexElementIndex = i;
         vf.InstancingEnable = false;
      }
   }

   blorp_emit(batch, GENX(3DSTATE_VF_TOPOLOGY), topo) {
      topo.PrimitiveTopologyType = _3DPRIM_RECTLIST;
   }
#endif
}

/* 3DSTATE_VIEWPORT_STATE_POINTERS */
static uint32_t
blorp_emit_cc_viewport(struct blorp_batch *batch)
{
   uint32_t cc_vp_offset;
   blorp_emit_dynamic(batch, GENX(CC_VIEWPORT), vp, 32, &cc_vp_offset) {
      vp.MinimumDepth = batch->blorp->config.use_unrestricted_depth_range ?
                           -FLT_MAX : 0.0;
      vp.MaximumDepth = batch->blorp->config.use_unrestricted_depth_range ?
                           FLT_MAX : 1.0;
   }

#if GFX_VER >= 7
   blorp_emit(batch, GENX(3DSTATE_VIEWPORT_STATE_POINTERS_CC), vsp) {
      vsp.CCViewportPointer = cc_vp_offset;
   }
#elif GFX_VER == 6
   blorp_emit(batch, GENX(3DSTATE_VIEWPORT_STATE_POINTERS), vsp) {
      vsp.CCViewportStateChange = true;
      vsp.PointertoCC_VIEWPORT = cc_vp_offset;
   }
#endif

   return cc_vp_offset;
}

static uint32_t
blorp_emit_sampler_state(struct blorp_batch *batch)
{
   uint32_t offset;
   blorp_emit_dynamic(batch, GENX(SAMPLER_STATE), sampler, 32, &offset) {
      sampler.MipModeFilter = MIPFILTER_NONE;
      sampler.MagModeFilter = MAPFILTER_LINEAR;
      sampler.MinModeFilter = MAPFILTER_LINEAR;
      sampler.MinLOD = 0;
      sampler.MaxLOD = 0;
      sampler.TCXAddressControlMode = TCM_CLAMP;
      sampler.TCYAddressControlMode = TCM_CLAMP;
      sampler.TCZAddressControlMode = TCM_CLAMP;
      sampler.MaximumAnisotropy = RATIO21;
      sampler.RAddressMinFilterRoundingEnable = true;
      sampler.RAddressMagFilterRoundingEnable = true;
      sampler.VAddressMinFilterRoundingEnable = true;
      sampler.VAddressMagFilterRoundingEnable = true;
      sampler.UAddressMinFilterRoundingEnable = true;
      sampler.UAddressMagFilterRoundingEnable = true;
#if GFX_VER > 6
      sampler.NonnormalizedCoordinateEnable = true;
#endif
   }

   return offset;
}

UNUSED static uint32_t
blorp_emit_sampler_state_ps(struct blorp_batch *batch)
{
   uint32_t offset = blorp_emit_sampler_state(batch);

#if GFX_VER >= 7
   blorp_emit(batch, GENX(3DSTATE_SAMPLER_STATE_POINTERS_PS), ssp) {
      ssp.PointertoPSSamplerState = offset;
   }
#elif GFX_VER == 6
   blorp_emit(batch, GENX(3DSTATE_SAMPLER_STATE_POINTERS), ssp) {
      ssp.VSSamplerStateChange = true;
      ssp.GSSamplerStateChange = true;
      ssp.PSSamplerStateChange = true;
      ssp.PointertoPSSamplerState = offset;
   }
#endif

   return offset;
}

/* What follows is the code for setting up a "pipeline" on Sandy Bridge and
 * later hardware.  This file will be included by i965 for gfx4-5 as well, so
 * this code is guarded by GFX_VER >= 6.
 */
#if GFX_VER >= 6

static void
blorp_emit_vs_config(struct blorp_batch *batch,
                     const struct blorp_params *params)
{
   struct elk_vs_prog_data *vs_prog_data = params->vs_prog_data;

   blorp_emit(batch, GENX(3DSTATE_VS), vs) {
      if (vs_prog_data) {
         vs.Enable = true;

         vs.KernelStartPointer = params->vs_prog_kernel;

         vs.DispatchGRFStartRegisterForURBData =
            vs_prog_data->base.base.dispatch_grf_start_reg;
         vs.VertexURBEntryReadLength =
            vs_prog_data->base.urb_read_length;
         vs.VertexURBEntryReadOffset = 0;

         vs.MaximumNumberofThreads =
            batch->blorp->isl_dev->info->max_vs_threads - 1;

         assert(GFX_VER < 8 ||
                vs_prog_data->base.dispatch_mode == INTEL_DISPATCH_MODE_SIMD8);
#if GFX_VER >= 8 && GFX_VER < 20
         vs.SIMD8DispatchEnable = true;
#endif
      }
   }
}

static void
blorp_emit_sf_config(struct blorp_batch *batch,
                     const struct blorp_params *params,
                     UNUSED enum intel_urb_deref_block_size urb_deref_block_size)
{
   const struct elk_wm_prog_data *prog_data = params->wm_prog_data;

   /* 3DSTATE_SF
    *
    * Disable ViewportTransformEnable (dw2.1)
    *
    * From the SandyBridge PRM, Volume 2, Part 1, Section 1.3, "3D
    * Primitives Overview":
    *     RECTLIST: Viewport Mapping must be DISABLED (as is typical with the
    *     use of screen- space coordinates).
    *
    * A solid rectangle must be rendered, so set FrontFaceFillMode (dw2.4:3)
    * and BackFaceFillMode (dw2.5:6) to SOLID(0).
    *
    * From the Sandy Bridge PRM, Volume 2, Part 1, Section
    * 6.4.1.1 3DSTATE_SF, Field FrontFaceFillMode:
    *     SOLID: Any triangle or rectangle object found to be front-facing
    *     is rendered as a solid object. This setting is required when
    *     (rendering rectangle (RECTLIST) objects.
    */

#if GFX_VER >= 8

   blorp_emit(batch, GENX(3DSTATE_SF), sf) {
   }

   blorp_emit(batch, GENX(3DSTATE_RASTER), raster) {
      raster.CullMode = CULLMODE_NONE;
   }

   blorp_emit(batch, GENX(3DSTATE_SBE), sbe) {
      sbe.VertexURBEntryReadOffset = 1;
      if (prog_data) {
         sbe.NumberofSFOutputAttributes = prog_data->num_varying_inputs;
         sbe.VertexURBEntryReadLength = elk_blorp_get_urb_length(prog_data);
         sbe.ConstantInterpolationEnable = prog_data->flat_inputs;
      } else {
         sbe.NumberofSFOutputAttributes = 0;
         sbe.VertexURBEntryReadLength = 1;
      }
      sbe.ForceVertexURBEntryReadLength = true;
      sbe.ForceVertexURBEntryReadOffset = true;
   }

#elif GFX_VER >= 7

   blorp_emit(batch, GENX(3DSTATE_SF), sf) {
      sf.FrontFaceFillMode = FILL_MODE_SOLID;
      sf.BackFaceFillMode = FILL_MODE_SOLID;

      sf.MultisampleRasterizationMode = params->num_samples > 1 ?
         MSRASTMODE_ON_PATTERN : MSRASTMODE_OFF_PIXEL;

#if GFX_VER == 7
      sf.DepthBufferSurfaceFormat = params->depth_format;
#endif
   }

   blorp_emit(batch, GENX(3DSTATE_SBE), sbe) {
      sbe.VertexURBEntryReadOffset = 1;
      if (prog_data) {
         sbe.NumberofSFOutputAttributes = prog_data->num_varying_inputs;
         sbe.VertexURBEntryReadLength = elk_blorp_get_urb_length(prog_data);
         sbe.ConstantInterpolationEnable = prog_data->flat_inputs;
      } else {
         sbe.NumberofSFOutputAttributes = 0;
         sbe.VertexURBEntryReadLength = 1;
      }
   }

#else /* GFX_VER <= 6 */

   blorp_emit(batch, GENX(3DSTATE_SF), sf) {
      sf.FrontFaceFillMode = FILL_MODE_SOLID;
      sf.BackFaceFillMode = FILL_MODE_SOLID;

      sf.MultisampleRasterizationMode = params->num_samples > 1 ?
         MSRASTMODE_ON_PATTERN : MSRASTMODE_OFF_PIXEL;

      sf.VertexURBEntryReadOffset = 1;
      if (prog_data) {
         sf.NumberofSFOutputAttributes = prog_data->num_varying_inputs;
         sf.VertexURBEntryReadLength = elk_blorp_get_urb_length(prog_data);
         sf.ConstantInterpolationEnable = prog_data->flat_inputs;
      } else {
         sf.NumberofSFOutputAttributes = 0;
         sf.VertexURBEntryReadLength = 1;
      }
   }

#endif /* GFX_VER */
}

static void
blorp_emit_ps_config(struct blorp_batch *batch,
                     const struct blorp_params *params)
{
   const struct elk_wm_prog_data *prog_data = params->wm_prog_data;

   /* Even when thread dispatch is disabled, max threads (dw5.25:31) must be
    * nonzero to prevent the GPU from hanging.  While the documentation doesn't
    * mention this explicitly, it notes that the valid range for the field is
    * [1,39] = [2,40] threads, which excludes zero.
    *
    * To be safe (and to minimize extraneous code) we go ahead and fully
    * configure the WM state whether or not there is a WM program.
    */

#if GFX_VER >= 8
   const struct intel_device_info *devinfo = batch->blorp->compiler->elk->devinfo;

   blorp_emit(batch, GENX(3DSTATE_WM), wm);

   blorp_emit(batch, GENX(3DSTATE_PS), ps) {
      if (params->src.enabled) {
         ps.SamplerCount = 1; /* Up to 4 samplers */
         ps.BindingTableEntryCount = 2;
      } else {
         ps.BindingTableEntryCount = 1;
      }

      /* SAMPLER_STATE prefetching is broken on Gfx11 - Wa_1606682166 */
      if (GFX_VER == 11)
         ps.SamplerCount = 0;

      /* 3DSTATE_PS expects the number of threads per PSD, which is always 64
       * for pre Gfx11 and 128 for gfx11+; On gfx11+ If a programmed value is
       * k, it implies 2(k+1) threads. It implicitly scales for different GT
       * levels (which have some # of PSDs).
       *
       * In Gfx8 the format is U8-2 whereas in Gfx9+ it is U9-1.
       */
      ps.MaximumNumberofThreadsPerPSD =
         devinfo->max_threads_per_psd - (GFX_VER == 8 ? 2 : 1);

      switch (params->fast_clear_op) {
      case ISL_AUX_OP_NONE:
         break;
      case ISL_AUX_OP_FULL_RESOLVE:
         ps.RenderTargetResolveEnable = true;
         break;
      case ISL_AUX_OP_FAST_CLEAR:
         ps.RenderTargetFastClearEnable = true;
         break;
      default:
         unreachable("Invalid fast clear op");
      }

      /* The RENDER_SURFACE_STATE page for TGL says:
       *
       *   For an 8 bpp surface with NUM_MULTISAMPLES = 1, Surface Width not
       *   multiple of 64 pixels and more than 1 mip level in the view, Fast
       *   Clear is not supported when AUX_CCS_E is set in this field.
       *
       * The granularity of a fast-clear or ambiguate operation is likely one
       * CCS element. For an 8 bpp primary surface, this maps to 32px x 4rows.
       * Due to the surface layout parameters, if LOD0's width isn't a
       * multiple of 64px, LOD1 and LOD2+ will share CCS elements. Assert that
       * these operations aren't occurring on these LODs.
       *
       * We don't explicitly check for TGL+ because the restriction is
       * technically applicable to all hardware. Platforms prior to TGL don't
       * support CCS on 8 bpp surfaces. So, these unaligned fast clear
       * operations shouldn't be occurring prior to TGL as well.
       */
      if (isl_format_get_layout(params->dst.surf.format)->bpb == 8 &&
          params->dst.surf.logical_level0_px.width % 64 != 0 &&
          params->dst.surf.levels >= 3 &&
          params->dst.view.base_level >= 1) {
         assert(params->num_samples == 1);
         assert(!ps.RenderTargetFastClearEnable);
      }

      if (prog_data) {
         intel_set_ps_dispatch_state(&ps, devinfo, prog_data,
                                     params->num_samples,
                                     0 /* msaa_flags */);

         ps.DispatchGRFStartRegisterForConstantSetupData0 =
            elk_wm_prog_data_dispatch_grf_start_reg(prog_data, ps, 0);
         ps.DispatchGRFStartRegisterForConstantSetupData1 =
            elk_wm_prog_data_dispatch_grf_start_reg(prog_data, ps, 1);
         ps.DispatchGRFStartRegisterForConstantSetupData2 =
            elk_wm_prog_data_dispatch_grf_start_reg(prog_data, ps, 2);

         ps.KernelStartPointer0 = params->wm_prog_kernel +
                                  elk_wm_prog_data_prog_offset(prog_data, ps, 0);
         ps.KernelStartPointer1 = params->wm_prog_kernel +
                                  elk_wm_prog_data_prog_offset(prog_data, ps, 1);
         ps.KernelStartPointer2 = params->wm_prog_kernel +
                                  elk_wm_prog_data_prog_offset(prog_data, ps, 2);
      }
   }

   blorp_emit(batch, GENX(3DSTATE_PS_EXTRA), psx) {
      if (prog_data) {
         psx.PixelShaderValid = true;
         psx.AttributeEnable = prog_data->num_varying_inputs > 0;
         psx.PixelShaderIsPerSample = prog_data->persample_dispatch;
         psx.PixelShaderComputedDepthMode = prog_data->computed_depth_mode;
      }

      if (params->src.enabled)
         psx.PixelShaderKillsPixel = true;
   }

#elif GFX_VER >= 7
   const struct intel_device_info *devinfo = batch->blorp->compiler->elk->devinfo;

   blorp_emit(batch, GENX(3DSTATE_WM), wm) {
      switch (params->hiz_op) {
      case ISL_AUX_OP_FAST_CLEAR:
         wm.DepthBufferClear = true;
         break;
      case ISL_AUX_OP_FULL_RESOLVE:
         wm.DepthBufferResolveEnable = true;
         break;
      case ISL_AUX_OP_AMBIGUATE:
         wm.HierarchicalDepthBufferResolveEnable = true;
         break;
      case ISL_AUX_OP_NONE:
         break;
      default:
         unreachable("not reached");
      }

      if (prog_data) {
         wm.ThreadDispatchEnable = true;
         wm.PixelShaderComputedDepthMode = prog_data->computed_depth_mode;
      }

      if (params->src.enabled)
         wm.PixelShaderKillsPixel = true;

      if (params->num_samples > 1) {
         wm.MultisampleRasterizationMode = MSRASTMODE_ON_PATTERN;
         wm.MultisampleDispatchMode =
            (prog_data && prog_data->persample_dispatch) ?
            MSDISPMODE_PERSAMPLE : MSDISPMODE_PERPIXEL;
      } else {
         wm.MultisampleRasterizationMode = MSRASTMODE_OFF_PIXEL;
         wm.MultisampleDispatchMode = MSDISPMODE_PERSAMPLE;
      }
   }

   blorp_emit(batch, GENX(3DSTATE_PS), ps) {
      ps.MaximumNumberofThreads =
         batch->blorp->isl_dev->info->max_wm_threads - 1;

#if GFX_VERx10 == 75
      ps.SampleMask = 1;
#endif

      if (prog_data) {
         intel_set_ps_dispatch_state(&ps, devinfo, prog_data,
                                     params->num_samples,
                                     0 /* msaa_flags */);

         ps.DispatchGRFStartRegisterForConstantSetupData0 =
            elk_wm_prog_data_dispatch_grf_start_reg(prog_data, ps, 0);
         ps.DispatchGRFStartRegisterForConstantSetupData1 =
            elk_wm_prog_data_dispatch_grf_start_reg(prog_data, ps, 1);
         ps.DispatchGRFStartRegisterForConstantSetupData2 =
            elk_wm_prog_data_dispatch_grf_start_reg(prog_data, ps, 2);

         ps.KernelStartPointer0 = params->wm_prog_kernel +
                                  elk_wm_prog_data_prog_offset(prog_data, ps, 0);
         ps.KernelStartPointer1 = params->wm_prog_kernel +
                                  elk_wm_prog_data_prog_offset(prog_data, ps, 1);
         ps.KernelStartPointer2 = params->wm_prog_kernel +
                                  elk_wm_prog_data_prog_offset(prog_data, ps, 2);

         ps.AttributeEnable = prog_data->num_varying_inputs > 0;
      } else {
         /* Gfx7 hardware gets angry if we don't enable at least one dispatch
          * mode, so just enable 16-pixel dispatch if we don't have a program.
          */
         ps._16PixelDispatchEnable = true;
      }

      if (params->src.enabled)
         ps.SamplerCount = 1; /* Up to 4 samplers */

      switch (params->fast_clear_op) {
      case ISL_AUX_OP_NONE:
         break;
      case ISL_AUX_OP_FULL_RESOLVE:
         ps.RenderTargetResolveEnable = true;
         break;
      case ISL_AUX_OP_FAST_CLEAR:
         ps.RenderTargetFastClearEnable = true;
         break;
      default:
         unreachable("Invalid fast clear op");
      }
   }

#else /* GFX_VER <= 6 */

   blorp_emit(batch, GENX(3DSTATE_WM), wm) {
      wm.MaximumNumberofThreads =
         batch->blorp->isl_dev->info->max_wm_threads - 1;

      switch (params->hiz_op) {
      case ISL_AUX_OP_FAST_CLEAR:
         wm.DepthBufferClear = true;
         break;
      case ISL_AUX_OP_FULL_RESOLVE:
         wm.DepthBufferResolveEnable = true;
         break;
      case ISL_AUX_OP_AMBIGUATE:
         wm.HierarchicalDepthBufferResolveEnable = true;
         break;
      case ISL_AUX_OP_NONE:
         break;
      default:
         unreachable("not reached");
      }

      if (prog_data) {
         wm.ThreadDispatchEnable = true;

         wm._8PixelDispatchEnable = prog_data->dispatch_8;
         wm._16PixelDispatchEnable = prog_data->dispatch_16;
         wm._32PixelDispatchEnable = prog_data->dispatch_32;

         wm.DispatchGRFStartRegisterForConstantSetupData0 =
            elk_wm_prog_data_dispatch_grf_start_reg(prog_data, wm, 0);
         wm.DispatchGRFStartRegisterForConstantSetupData1 =
            elk_wm_prog_data_dispatch_grf_start_reg(prog_data, wm, 1);
         wm.DispatchGRFStartRegisterForConstantSetupData2 =
            elk_wm_prog_data_dispatch_grf_start_reg(prog_data, wm, 2);

         wm.KernelStartPointer0 = params->wm_prog_kernel +
                                  elk_wm_prog_data_prog_offset(prog_data, wm, 0);
         wm.KernelStartPointer1 = params->wm_prog_kernel +
                                  elk_wm_prog_data_prog_offset(prog_data, wm, 1);
         wm.KernelStartPointer2 = params->wm_prog_kernel +
                                  elk_wm_prog_data_prog_offset(prog_data, wm, 2);

         wm.NumberofSFOutputAttributes = prog_data->num_varying_inputs;
      }

      if (params->src.enabled) {
         wm.SamplerCount = 1; /* Up to 4 samplers */
         wm.PixelShaderKillsPixel = true; /* TODO: temporarily smash on */
      }

      if (params->num_samples > 1) {
         wm.MultisampleRasterizationMode = MSRASTMODE_ON_PATTERN;
         wm.MultisampleDispatchMode =
            (prog_data && prog_data->persample_dispatch) ?
            MSDISPMODE_PERSAMPLE : MSDISPMODE_PERPIXEL;
      } else {
         wm.MultisampleRasterizationMode = MSRASTMODE_OFF_PIXEL;
         wm.MultisampleDispatchMode = MSDISPMODE_PERSAMPLE;
      }
   }

#endif /* GFX_VER */
}

static uint32_t
blorp_emit_blend_state(struct blorp_batch *batch,
                       const struct blorp_params *params)
{
   struct GENX(BLEND_STATE) blend = { };

   uint32_t offset;
   int size = GENX(BLEND_STATE_length) * 4;
   size += GENX(BLEND_STATE_ENTRY_length) * 4 * params->num_draw_buffers;
   uint32_t *state = blorp_alloc_dynamic_state(batch, size, 64, &offset);
   if (state == NULL)
      return 0;
   uint32_t *pos = state;

   GENX(BLEND_STATE_pack)(NULL, pos, &blend);
   pos += GENX(BLEND_STATE_length);

   for (unsigned i = 0; i < params->num_draw_buffers; ++i) {
      struct GENX(BLEND_STATE_ENTRY) entry = {
         .PreBlendColorClampEnable = true,
         .PostBlendColorClampEnable = true,
         .ColorClampRange = COLORCLAMP_RTFORMAT,

         .WriteDisableRed = params->color_write_disable & 1,
         .WriteDisableGreen = params->color_write_disable & 2,
         .WriteDisableBlue = params->color_write_disable & 4,
         .WriteDisableAlpha = params->color_write_disable & 8,
      };
      GENX(BLEND_STATE_ENTRY_pack)(NULL, pos, &entry);
      pos += GENX(BLEND_STATE_ENTRY_length);
   }

   blorp_flush_range(batch, state, size);

#if GFX_VER >= 7
   blorp_emit(batch, GENX(3DSTATE_BLEND_STATE_POINTERS), sp) {
      sp.BlendStatePointer = offset;
#if GFX_VER >= 8
      sp.BlendStatePointerValid = true;
#endif
   }
#endif

#if GFX_VER >= 8
   blorp_emit(batch, GENX(3DSTATE_PS_BLEND), ps_blend) {
      ps_blend.HasWriteableRT = true;
   }
#endif

   return offset;
}

static uint32_t
blorp_emit_color_calc_state(struct blorp_batch *batch,
                            UNUSED const struct blorp_params *params)
{
   uint32_t offset;
   blorp_emit_dynamic(batch, GENX(COLOR_CALC_STATE), cc, 64, &offset) {
#if GFX_VER <= 8
      cc.StencilReferenceValue = params->stencil_ref;
#endif
   }

#if GFX_VER >= 7
   blorp_emit(batch, GENX(3DSTATE_CC_STATE_POINTERS), sp) {
      sp.ColorCalcStatePointer = offset;
#if GFX_VER >= 8
      sp.ColorCalcStatePointerValid = true;
#endif
   }
#endif

   return offset;
}

static uint32_t
blorp_emit_depth_stencil_state(struct blorp_batch *batch,
                               const struct blorp_params *params)
{
#if GFX_VER >= 8
   struct GENX(3DSTATE_WM_DEPTH_STENCIL) ds = {
      GENX(3DSTATE_WM_DEPTH_STENCIL_header),
   };
#else
   struct GENX(DEPTH_STENCIL_STATE) ds = { 0 };
#endif

   if (params->depth.enabled) {
      ds.DepthBufferWriteEnable = true;

      switch (params->hiz_op) {
      /* See the following sections of the Sandy Bridge PRM, Volume 2, Part1:
       *   - 7.5.3.1 Depth Buffer Clear
       *   - 7.5.3.2 Depth Buffer Resolve
       *   - 7.5.3.3 Hierarchical Depth Buffer Resolve
       */
      case ISL_AUX_OP_FULL_RESOLVE:
         ds.DepthTestEnable = true;
         ds.DepthTestFunction = COMPAREFUNCTION_NEVER;
         break;

      case ISL_AUX_OP_NONE:
      case ISL_AUX_OP_FAST_CLEAR:
      case ISL_AUX_OP_AMBIGUATE:
         ds.DepthTestEnable = false;
         break;
      case ISL_AUX_OP_PARTIAL_RESOLVE:
         unreachable("Invalid HIZ op");
      }
   }

   if (params->stencil.enabled) {
      ds.StencilBufferWriteEnable = true;
      ds.StencilTestEnable = true;
      ds.DoubleSidedStencilEnable = false;

      ds.StencilTestFunction = COMPAREFUNCTION_ALWAYS;
      ds.StencilPassDepthPassOp = STENCILOP_REPLACE;

      ds.StencilWriteMask = params->stencil_mask;
   }

#if GFX_VER >= 8
   uint32_t offset = 0;
   uint32_t *dw = blorp_emit_dwords(batch,
                                    GENX(3DSTATE_WM_DEPTH_STENCIL_length));
   if (!dw)
      return 0;

   GENX(3DSTATE_WM_DEPTH_STENCIL_pack)(NULL, dw, &ds);
#else
   uint32_t offset;
   void *state = blorp_alloc_dynamic_state(batch,
                                           GENX(DEPTH_STENCIL_STATE_length) * 4,
                                           64, &offset);
   GENX(DEPTH_STENCIL_STATE_pack)(NULL, state, &ds);
   blorp_flush_range(batch, state, GENX(DEPTH_STENCIL_STATE_length) * 4);
#endif

#if GFX_VER == 7
   blorp_emit(batch, GENX(3DSTATE_DEPTH_STENCIL_STATE_POINTERS), sp) {
      sp.PointertoDEPTH_STENCIL_STATE = offset;
   }
#endif

   return offset;
}

static void
blorp_emit_3dstate_multisample(struct blorp_batch *batch,
                               const struct blorp_params *params)
{
   blorp_emit(batch, GENX(3DSTATE_MULTISAMPLE), ms) {
      ms.NumberofMultisamples       = __builtin_ffs(params->num_samples) - 1;
      ms.PixelLocation              = CENTER;
#if GFX_VER >= 7 && GFX_VER < 8
      switch (params->num_samples) {
      case 1:
         INTEL_SAMPLE_POS_1X(ms.Sample);
         break;
      case 2:
         INTEL_SAMPLE_POS_2X(ms.Sample);
         break;
      case 4:
         INTEL_SAMPLE_POS_4X(ms.Sample);
         break;
      case 8:
         INTEL_SAMPLE_POS_8X(ms.Sample);
         break;
      default:
         break;
      }
#elif GFX_VER < 7
      INTEL_SAMPLE_POS_4X(ms.Sample);
#endif
   }
}

static void
blorp_emit_pipeline(struct blorp_batch *batch,
                    const struct blorp_params *params)
{
   uint32_t blend_state_offset = 0;
   uint32_t color_calc_state_offset;
   uint32_t depth_stencil_state_offset;

   enum intel_urb_deref_block_size urb_deref_block_size;
   emit_urb_config(batch, params, &urb_deref_block_size);

   if (params->wm_prog_data) {
      blend_state_offset = blorp_emit_blend_state(batch, params);
   }
   color_calc_state_offset = blorp_emit_color_calc_state(batch, params);
   depth_stencil_state_offset = blorp_emit_depth_stencil_state(batch, params);

#if GFX_VER == 6
   /* 3DSTATE_CC_STATE_POINTERS
    *
    * The pointer offsets are relative to
    * CMD_STATE_BASE_ADDRESS.DynamicStateBaseAddress.
    *
    * The HiZ op doesn't use BLEND_STATE or COLOR_CALC_STATE.
    *
    * The dynamic state emit helpers emit their own STATE_POINTERS packets on
    * gfx7+.  However, on gfx6 and earlier, they're all lumpped together in
    * one CC_STATE_POINTERS packet so we have to emit that here.
    */
   blorp_emit(batch, GENX(3DSTATE_CC_STATE_POINTERS), cc) {
      cc.BLEND_STATEChange = params->wm_prog_data ? true : false;
      cc.ColorCalcStatePointerValid = true;
      cc.DEPTH_STENCIL_STATEChange = true;
      cc.PointertoBLEND_STATE = blend_state_offset;
      cc.ColorCalcStatePointer = color_calc_state_offset;
      cc.PointertoDEPTH_STENCIL_STATE = depth_stencil_state_offset;
   }
#else
   (void)blend_state_offset;
   (void)color_calc_state_offset;
   (void)depth_stencil_state_offset;
#endif

   UNUSED uint32_t mocs = isl_mocs(batch->blorp->isl_dev, 0, false);

#if GFX_VER == 7
#define CONSTANT_MOCS xs.ConstantBody.MOCS = mocs
#else
#define CONSTANT_MOCS
#endif
   blorp_emit(batch, GENX(3DSTATE_CONSTANT_VS), xs) { CONSTANT_MOCS; }
#if GFX_VER >= 7
   blorp_emit(batch, GENX(3DSTATE_CONSTANT_HS), xs) { CONSTANT_MOCS; }
   blorp_emit(batch, GENX(3DSTATE_CONSTANT_DS), xs) { CONSTANT_MOCS; }
#endif
   blorp_emit(batch, GENX(3DSTATE_CONSTANT_GS), xs) { CONSTANT_MOCS; }
   blorp_emit(batch, GENX(3DSTATE_CONSTANT_PS), xs) { CONSTANT_MOCS; }
#undef CONSTANT_MOCS

   if (params->src.enabled)
      blorp_emit_sampler_state_ps(batch);

   blorp_emit_3dstate_multisample(batch, params);

   blorp_emit(batch, GENX(3DSTATE_SAMPLE_MASK), mask) {
      mask.SampleMask = (1 << params->num_samples) - 1;
   }

   /* From the BSpec, 3D Pipeline > Geometry > Vertex Shader > State,
    * 3DSTATE_VS, Dword 5.0 "VS Function Enable":
    *
    *   [DevSNB] A pipeline flush must be programmed prior to a
    *   3DSTATE_VS command that causes the VS Function Enable to
    *   toggle. Pipeline flush can be executed by sending a PIPE_CONTROL
    *   command with CS stall bit set and a post sync operation.
    *
    * We've already done one at the start of the BLORP operation.
    */
   blorp_emit_vs_config(batch, params);
#if GFX_VER >= 7
   blorp_emit(batch, GENX(3DSTATE_HS), hs);
   blorp_emit(batch, GENX(3DSTATE_TE), te);
   blorp_emit(batch, GENX(3DSTATE_DS), DS);
   blorp_emit(batch, GENX(3DSTATE_STREAMOUT), so);
#endif
   blorp_emit(batch, GENX(3DSTATE_GS), gs);

   blorp_emit(batch, GENX(3DSTATE_CLIP), clip) {
      clip.PerspectiveDivideDisable = true;
   }

   blorp_emit_sf_config(batch, params, urb_deref_block_size);
   blorp_emit_ps_config(batch, params);

   blorp_emit_cc_viewport(batch);
}

/******** This is the end of the pipeline setup code ********/

#endif /* GFX_VER >= 6 */

#if GFX_VER >= 7
static void
blorp_emit_memcpy(struct blorp_batch *batch,
                  struct blorp_address dst,
                  struct blorp_address src,
                  uint32_t size)
{
   assert(size % 4 == 0);

   for (unsigned dw = 0; dw < size; dw += 4) {
#if GFX_VER >= 8
      blorp_emit(batch, GENX(MI_COPY_MEM_MEM), cp) {
         cp.DestinationMemoryAddress = dst;
         cp.SourceMemoryAddress = src;
      }
#else
      /* IVB does not have a general purpose register for command streamer
       * commands. Therefore, we use an alternate temporary register.
       */
#define BLORP_TEMP_REG 0x2440 /* GFX7_3DPRIM_BASE_VERTEX */
      blorp_emit(batch, GENX(MI_LOAD_REGISTER_MEM), load) {
         load.RegisterAddress = BLORP_TEMP_REG;
         load.MemoryAddress = src;
      }
      blorp_emit(batch, GENX(MI_STORE_REGISTER_MEM), store) {
         store.RegisterAddress = BLORP_TEMP_REG;
         store.MemoryAddress = dst;
      }
#undef BLORP_TEMP_REG
#endif
      dst.offset += 4;
      src.offset += 4;
   }
}
#endif

static void
blorp_emit_surface_state(struct blorp_batch *batch,
                         const struct blorp_surface_info *surface,
                         UNUSED enum isl_aux_op aux_op,
                         void *state, uint32_t state_offset,
                         uint8_t color_write_disable,
                         bool is_render_target)
{
   const struct isl_device *isl_dev = batch->blorp->isl_dev;
   struct isl_surf surf = surface->surf;

   if (surf.dim == ISL_SURF_DIM_1D &&
       surf.dim_layout == ISL_DIM_LAYOUT_GFX4_2D) {
      assert(surf.logical_level0_px.height == 1);
      surf.dim = ISL_SURF_DIM_2D;
   }

   if (isl_aux_usage_has_hiz(surface->aux_usage)) {
      /* BLORP doesn't render with depth so we can't use HiZ */
      assert(!is_render_target);
      /* We can't reinterpret HiZ */
      assert(surface->surf.format == surface->view.format);
   }

   enum isl_aux_usage aux_usage = surface->aux_usage;

   /* On gfx12, implicit CCS has no aux buffer */
   bool use_aux_address = (aux_usage != ISL_AUX_USAGE_NONE) &&
                          (surface->aux_addr.buffer != NULL);

   isl_channel_mask_t write_disable_mask = 0;
   if (is_render_target && GFX_VER <= 5) {
      if (color_write_disable & BITFIELD_BIT(0))
         write_disable_mask |= ISL_CHANNEL_RED_BIT;
      if (color_write_disable & BITFIELD_BIT(1))
         write_disable_mask |= ISL_CHANNEL_GREEN_BIT;
      if (color_write_disable & BITFIELD_BIT(2))
         write_disable_mask |= ISL_CHANNEL_BLUE_BIT;
      if (color_write_disable & BITFIELD_BIT(3))
         write_disable_mask |= ISL_CHANNEL_ALPHA_BIT;
   }

   isl_surf_fill_state(batch->blorp->isl_dev, state,
                       .surf = &surf, .view = &surface->view,
                       .aux_surf = &surface->aux_surf, .aux_usage = aux_usage,
                       .address =
                          blorp_get_surface_address(batch, surface->addr),
                       .aux_address = !use_aux_address ? 0 :
                          blorp_get_surface_address(batch, surface->aux_addr),
                       .mocs = surface->addr.mocs,
                       .clear_color = surface->clear_color,
                       .write_disables = write_disable_mask);

   blorp_surface_reloc(batch, state_offset + isl_dev->ss.addr_offset,
                       surface->addr, 0);

   if (use_aux_address) {
      /* On gfx7 and prior, the bottom 12 bits of the MCS base address are
       * used to store other information.  This should be ok, however, because
       * surface buffer addresses are always 4K page alinged.
       */
      assert((surface->aux_addr.offset & 0xfff) == 0);
      uint32_t *aux_addr = state + isl_dev->ss.aux_addr_offset;
      blorp_surface_reloc(batch, state_offset + isl_dev->ss.aux_addr_offset,
                          surface->aux_addr, *aux_addr);
   }

   if (aux_usage != ISL_AUX_USAGE_NONE && surface->clear_color_addr.buffer) {
#if GFX_VER >= 7
      /* Fast clears just whack the AUX surface and don't actually use the
       * clear color for anything.  We can avoid the MI memcpy on that case.
       */
      if (aux_op != ISL_AUX_OP_FAST_CLEAR) {
         struct blorp_address dst_addr = blorp_get_surface_base_address(batch);
         dst_addr.offset += state_offset + isl_dev->ss.clear_value_offset;
         blorp_emit_memcpy(batch, dst_addr, surface->clear_color_addr,
                           isl_dev->ss.clear_value_size);
      }
#else
      unreachable("Fast clears are only supported on gfx7+");
#endif
   }

   blorp_flush_range(batch, state, GENX(RENDER_SURFACE_STATE_length) * 4);
}

static void
blorp_emit_null_surface_state(struct blorp_batch *batch,
                              const struct blorp_surface_info *surface,
                              uint32_t *state)
{
   struct GENX(RENDER_SURFACE_STATE) ss = {
      .SurfaceType = SURFTYPE_NULL,
      .SurfaceFormat = ISL_FORMAT_R8G8B8A8_UNORM,
      .Width = surface->surf.logical_level0_px.width - 1,
      .Height = surface->surf.logical_level0_px.height - 1,
      .MIPCountLOD = surface->view.base_level,
      .MinimumArrayElement = surface->view.base_array_layer,
      .Depth = surface->view.array_len - 1,
      .RenderTargetViewExtent = surface->view.array_len - 1,
#if GFX_VER >= 6
      .NumberofMultisamples = ffs(surface->surf.samples) - 1,
      .MOCS = isl_mocs(batch->blorp->isl_dev, 0, false),
#endif

#if GFX_VER >= 7
      .SurfaceArray = surface->surf.dim != ISL_SURF_DIM_3D,
#endif

#if GFX_VER >= 8
      .TileMode = YMAJOR,
#else
      .TiledSurface = true,
#endif
   };

   GENX(RENDER_SURFACE_STATE_pack)(NULL, state, &ss);

   blorp_flush_range(batch, state, GENX(RENDER_SURFACE_STATE_length) * 4);
}

static uint32_t
blorp_setup_binding_table(struct blorp_batch *batch,
                           const struct blorp_params *params)
{
   const struct isl_device *isl_dev = batch->blorp->isl_dev;
   uint32_t surface_offsets[2], bind_offset = 0;
   void *surface_maps[2];

   if (params->use_pre_baked_binding_table) {
      bind_offset = params->pre_baked_binding_table_offset;
   } else {
      unsigned num_surfaces = 1 + params->src.enabled;
      if (!blorp_alloc_binding_table(batch, num_surfaces,
                                     isl_dev->ss.size, isl_dev->ss.align,
                                     &bind_offset, surface_offsets, surface_maps))
         return 0;

      if (params->dst.enabled) {
         blorp_emit_surface_state(batch, &params->dst,
                                  params->fast_clear_op,
                                  surface_maps[BLORP_RENDERBUFFER_BT_INDEX],
                                  surface_offsets[BLORP_RENDERBUFFER_BT_INDEX],
                                  params->color_write_disable, true);
      } else {
         assert(params->depth.enabled || params->stencil.enabled);
         const struct blorp_surface_info *surface =
            params->depth.enabled ? &params->depth : &params->stencil;
         blorp_emit_null_surface_state(batch, surface,
                                       surface_maps[BLORP_RENDERBUFFER_BT_INDEX]);
      }

      if (params->src.enabled) {
         blorp_emit_surface_state(batch, &params->src,
                                  params->fast_clear_op,
                                  surface_maps[BLORP_TEXTURE_BT_INDEX],
                                  surface_offsets[BLORP_TEXTURE_BT_INDEX],
                                  0, false);
      }
   }

   return bind_offset;
}

static void
blorp_emit_btp(struct blorp_batch *batch, uint32_t bind_offset)
{
#if GFX_VER >= 7
   blorp_emit(batch, GENX(3DSTATE_BINDING_TABLE_POINTERS_VS), bt);
   blorp_emit(batch, GENX(3DSTATE_BINDING_TABLE_POINTERS_HS), bt);
   blorp_emit(batch, GENX(3DSTATE_BINDING_TABLE_POINTERS_DS), bt);
   blorp_emit(batch, GENX(3DSTATE_BINDING_TABLE_POINTERS_GS), bt);

   blorp_emit(batch, GENX(3DSTATE_BINDING_TABLE_POINTERS_PS), bt) {
      bt.PointertoPSBindingTable =
         blorp_binding_table_offset_to_pointer(batch, bind_offset);
   }
#elif GFX_VER >= 6
   blorp_emit(batch, GENX(3DSTATE_BINDING_TABLE_POINTERS), bt) {
      bt.PSBindingTableChange = true;
      bt.PointertoPSBindingTable =
         blorp_binding_table_offset_to_pointer(batch, bind_offset);
   }
#else
   blorp_emit(batch, GENX(3DSTATE_BINDING_TABLE_POINTERS), bt) {
      bt.PointertoPSBindingTable =
         blorp_binding_table_offset_to_pointer(batch, bind_offset);
   }
#endif
}

static void
blorp_emit_depth_stencil_config(struct blorp_batch *batch,
                                const struct blorp_params *params)
{
   const struct isl_device *isl_dev = batch->blorp->isl_dev;

   uint32_t *dw = blorp_emit_dwords(batch, isl_dev->ds.size / 4);
   if (dw == NULL)
      return;

   struct isl_depth_stencil_hiz_emit_info info = { };

   if (params->depth.enabled) {
      info.view = &params->depth.view;
      info.mocs = params->depth.addr.mocs;
   } else if (params->stencil.enabled) {
      info.view = &params->stencil.view;
      info.mocs = params->stencil.addr.mocs;
   } else {
      info.mocs = isl_mocs(isl_dev, 0, false);
   }

   if (params->depth.enabled) {
      info.depth_surf = &params->depth.surf;

      info.depth_address =
         blorp_emit_reloc(batch, dw + isl_dev->ds.depth_offset / 4,
                          params->depth.addr, 0);

      info.hiz_usage = params->depth.aux_usage;
      if (isl_aux_usage_has_hiz(info.hiz_usage)) {
         info.hiz_surf = &params->depth.aux_surf;

         struct blorp_address hiz_address = params->depth.aux_addr;
#if GFX_VER == 6
         /* Sandy bridge hardware does not technically support mipmapped HiZ.
          * However, we have a special layout that allows us to make it work
          * anyway by manually offsetting to the specified miplevel.
          */
         assert(info.hiz_surf->dim_layout == ISL_DIM_LAYOUT_GFX6_STENCIL_HIZ);
         uint64_t offset_B;
         isl_surf_get_image_offset_B_tile_sa(info.hiz_surf,
                                             info.view->base_level, 0, 0,
                                             &offset_B, NULL, NULL);
         hiz_address.offset += offset_B;
#endif

         info.hiz_address =
            blorp_emit_reloc(batch, dw + isl_dev->ds.hiz_offset / 4,
                             hiz_address, 0);

         info.depth_clear_value = params->depth.clear_color.f32[0];
      }
   }

   if (params->stencil.enabled) {
      info.stencil_surf = &params->stencil.surf;

      info.stencil_aux_usage = params->stencil.aux_usage;
      struct blorp_address stencil_address = params->stencil.addr;
#if GFX_VER == 6
      /* Sandy bridge hardware does not technically support mipmapped stencil.
       * However, we have a special layout that allows us to make it work
       * anyway by manually offsetting to the specified miplevel.
       */
      assert(info.stencil_surf->dim_layout == ISL_DIM_LAYOUT_GFX6_STENCIL_HIZ);
      uint64_t offset_B;
      isl_surf_get_image_offset_B_tile_sa(info.stencil_surf,
                                          info.view->base_level, 0, 0,
                                          &offset_B, NULL, NULL);
      stencil_address.offset += offset_B;
#endif

      info.stencil_address =
         blorp_emit_reloc(batch, dw + isl_dev->ds.stencil_offset / 4,
                          stencil_address, 0);
   }

   isl_emit_depth_stencil_hiz_s(isl_dev, dw, &info);
}

#if GFX_VER >= 8
/* Emits the Optimized HiZ sequence specified in the BDW+ PRMs. The
 * depth/stencil buffer extents are ignored to handle APIs which perform
 * clearing operations without such information.
 * */
static void
blorp_emit_gfx8_hiz_op(struct blorp_batch *batch,
                       const struct blorp_params *params)
{
   /* We should be performing an operation on a depth or stencil buffer.
    */
   assert(params->depth.enabled || params->stencil.enabled);

   blorp_measure_start(batch, params);

   /* The stencil buffer should only be enabled if a fast clear operation is
    * requested.
    */
   if (params->stencil.enabled)
      assert(params->hiz_op == ISL_AUX_OP_FAST_CLEAR);

   /* From the BDW PRM Volume 2, 3DSTATE_WM_HZ_OP:
    *
    * 3DSTATE_MULTISAMPLE packet must be used prior to this packet to change
    * the Number of Multisamples. This packet must not be used to change
    * Number of Multisamples in a rendering sequence.
    *
    * Since HIZ may be the first thing in a batch buffer, play safe and always
    * emit 3DSTATE_MULTISAMPLE.
    */
   blorp_emit_3dstate_multisample(batch, params);

   /* From the BDW PRM Volume 7, Depth Buffer Clear:
    *
    *    The clear value must be between the min and max depth values
    *    (inclusive) defined in the CC_VIEWPORT. If the depth buffer format is
    *    D32_FLOAT, then +/-DENORM values are also allowed.
    *
    * Set the bounds to match our hardware limits, [0.0, 1.0].
    */
   if (params->depth.enabled && params->hiz_op == ISL_AUX_OP_FAST_CLEAR) {
      assert(params->depth.clear_color.f32[0] >= 0.0f);
      assert(params->depth.clear_color.f32[0] <= 1.0f);
      blorp_emit_cc_viewport(batch);
   }

   /* According to the SKL PRM formula for WM_INT::ThreadDispatchEnable, the
    * 3DSTATE_WM::ForceThreadDispatchEnable field can force WM thread dispatch
    * even when WM_HZ_OP is active.  However, WM thread dispatch is normally
    * disabled for HiZ ops and it appears that force-enabling it can lead to
    * GPU hangs on at least Skylake.  Since we don't know the current state of
    * the 3DSTATE_WM packet, just emit a dummy one prior to 3DSTATE_WM_HZ_OP.
    */
   blorp_emit(batch, GENX(3DSTATE_WM), wm);

   /* If we can't alter the depth stencil config and multiple layers are
    * involved, the HiZ op will fail. This is because the op requires that a
    * new config is emitted for each additional layer.
    */
   if (batch->flags & BLORP_BATCH_NO_EMIT_DEPTH_STENCIL) {
      assert(params->num_layers <= 1);
   } else {
      blorp_emit_depth_stencil_config(batch, params);
   }

   /* TODO - If we ever start using 3DSTATE_WM_HZ_OP::StencilBufferResolveEnable
    * we need to implement required steps, flushes documented in Wa_1605967699.
    */
   blorp_emit(batch, GENX(3DSTATE_WM_HZ_OP), hzp) {
      switch (params->hiz_op) {
      case ISL_AUX_OP_FAST_CLEAR:
         hzp.StencilBufferClearEnable = params->stencil.enabled;
         hzp.DepthBufferClearEnable = params->depth.enabled;
         hzp.StencilClearValue = params->stencil_ref;
         hzp.FullSurfaceDepthandStencilClear = params->full_surface_hiz_op;
         break;
      case ISL_AUX_OP_FULL_RESOLVE:
         assert(params->full_surface_hiz_op);
         hzp.DepthBufferResolveEnable = true;
         break;
      case ISL_AUX_OP_AMBIGUATE:
         assert(params->full_surface_hiz_op);
         hzp.HierarchicalDepthBufferResolveEnable = true;
         break;
      case ISL_AUX_OP_PARTIAL_RESOLVE:
      case ISL_AUX_OP_NONE:
         unreachable("Invalid HIZ op");
      }

      hzp.NumberofMultisamples = ffs(params->num_samples) - 1;
      hzp.SampleMask = 0xFFFF;

      /* Due to a hardware issue, this bit MBZ */
      assert(hzp.ScissorRectangleEnable == false);

      /* Contrary to the HW docs both fields are inclusive */
      hzp.ClearRectangleXMin = params->x0;
      hzp.ClearRectangleYMin = params->y0;

      /* Contrary to the HW docs both fields are exclusive */
      hzp.ClearRectangleXMax = params->x1;
      hzp.ClearRectangleYMax = params->y1;
   }

   /* PIPE_CONTROL w/ all bits clear except for “Post-Sync Operation” must set
    * to “Write Immediate Data” enabled.
    */
   blorp_emit(batch, GENX(PIPE_CONTROL), pc) {
      pc.PostSyncOperation = WriteImmediateData;
      pc.Address = blorp_get_workaround_address(batch);
   }

   blorp_emit(batch, GENX(3DSTATE_WM_HZ_OP), hzp);

   blorp_measure_end(batch, params);
}
#endif

static void
blorp_update_clear_color(UNUSED struct blorp_batch *batch,
                         const struct blorp_surface_info *info)
{
   assert(info->clear_color_addr.buffer != NULL);
#if GFX_VER >= 7
   blorp_emit(batch, GENX(MI_STORE_DATA_IMM), sdi) {
      sdi.Address = info->clear_color_addr;
      sdi.ImmediateData = ISL_CHANNEL_SELECT_RED   << 25 |
                          ISL_CHANNEL_SELECT_GREEN << 22 |
                          ISL_CHANNEL_SELECT_BLUE  << 19 |
                          ISL_CHANNEL_SELECT_ALPHA << 16;
      if (isl_format_has_int_channel(info->view.format)) {
         for (unsigned i = 0; i < 4; i++) {
            assert(info->clear_color.u32[i] == 0 ||
                   info->clear_color.u32[i] == 1);
         }
         sdi.ImmediateData |= (info->clear_color.u32[0] != 0) << 31;
         sdi.ImmediateData |= (info->clear_color.u32[1] != 0) << 30;
         sdi.ImmediateData |= (info->clear_color.u32[2] != 0) << 29;
         sdi.ImmediateData |= (info->clear_color.u32[3] != 0) << 28;
      } else {
         for (unsigned i = 0; i < 4; i++) {
            assert(info->clear_color.f32[i] == 0.0f ||
                   info->clear_color.f32[i] == 1.0f);
         }
         sdi.ImmediateData |= (info->clear_color.f32[0] != 0.0f) << 31;
         sdi.ImmediateData |= (info->clear_color.f32[1] != 0.0f) << 30;
         sdi.ImmediateData |= (info->clear_color.f32[2] != 0.0f) << 29;
         sdi.ImmediateData |= (info->clear_color.f32[3] != 0.0f) << 28;
      }
   }
#endif
}

static bool
blorp_uses_bti_rt_writes(const struct blorp_batch *batch, const struct blorp_params *params)
{
   if (batch->flags & (BLORP_BATCH_USE_BLITTER | BLORP_BATCH_USE_COMPUTE))
      return false;

   /* HIZ clears use WM_HZ ops rather than a clear shader using RT writes. */
   return params->hiz_op == ISL_AUX_OP_NONE;
}

static void
blorp_exec_3d(struct blorp_batch *batch, const struct blorp_params *params)
{
   if (!(batch->flags & BLORP_BATCH_NO_UPDATE_CLEAR_COLOR)) {
      if (params->fast_clear_op == ISL_AUX_OP_FAST_CLEAR &&
          params->dst.clear_color_addr.buffer != NULL) {
         blorp_update_clear_color(batch, &params->dst);
      }

      if (params->hiz_op == ISL_AUX_OP_FAST_CLEAR &&
          params->depth.clear_color_addr.buffer != NULL) {
         blorp_update_clear_color(batch, &params->depth);
      }
   }

#if GFX_VER >= 8
   if (params->hiz_op != ISL_AUX_OP_NONE) {
      blorp_emit_gfx8_hiz_op(batch, params);
      return;
   }
#endif

   blorp_emit_vertex_buffers(batch, params);
   blorp_emit_vertex_elements(batch, params);

   blorp_emit_pipeline(batch, params);

   blorp_emit_btp(batch, blorp_setup_binding_table(batch, params));

   if (!(batch->flags & BLORP_BATCH_NO_EMIT_DEPTH_STENCIL))
      blorp_emit_depth_stencil_config(batch, params);

   const UNUSED bool use_tbimr = false;
   blorp_emit_pre_draw(batch, params);
   blorp_emit(batch, GENX(3DPRIMITIVE), prim) {
      prim.VertexAccessType = SEQUENTIAL;
      prim.PrimitiveTopologyType = _3DPRIM_RECTLIST;
#if GFX_VER >= 7
      prim.PredicateEnable = batch->flags & BLORP_BATCH_PREDICATE_ENABLE;
#endif
      prim.VertexCountPerInstance = 3;
      prim.InstanceCount = params->num_layers;
   }
   blorp_emit_post_draw(batch, params);
}

#if GFX_VER >= 7

static void
blorp_get_compute_push_const(struct blorp_batch *batch,
                             const struct blorp_params *params,
                             uint32_t threads,
                             uint32_t *state_offset,
                             unsigned *state_size)
{
   const struct elk_cs_prog_data *cs_prog_data = params->cs_prog_data;
   const unsigned push_const_size =
      ALIGN(elk_cs_push_const_total_size(cs_prog_data, threads), 64);
   assert(cs_prog_data->push.cross_thread.size +
          cs_prog_data->push.per_thread.size == sizeof(params->wm_inputs));

   if (push_const_size == 0) {
      *state_offset = 0;
      *state_size = 0;
      return;
   }

   uint32_t push_const_offset;
   uint32_t *push_const =
      blorp_alloc_dynamic_state(batch, push_const_size, 64,
                                &push_const_offset);
   memset(push_const, 0x0, push_const_size);

   void *dst = push_const;
   const void *src = (char *)&params->wm_inputs;

   if (cs_prog_data->push.cross_thread.size > 0) {
      memcpy(dst, src, cs_prog_data->push.cross_thread.size);
      dst += cs_prog_data->push.cross_thread.size;
      src += cs_prog_data->push.cross_thread.size;
   }

   if (cs_prog_data->push.per_thread.size > 0) {
      for (unsigned t = 0; t < threads; t++) {
         memcpy(dst, src, (cs_prog_data->push.per_thread.dwords - 1) * 4);

         uint32_t *subgroup_id = dst + cs_prog_data->push.per_thread.size - 4;
         *subgroup_id = t;

         dst += cs_prog_data->push.per_thread.size;
      }
   }

   *state_offset = push_const_offset;
   *state_size = push_const_size;
}

#endif /* GFX_VER >= 7 */

static void
blorp_exec_compute(struct blorp_batch *batch, const struct blorp_params *params)
{
   assert(!(batch->flags & BLORP_BATCH_NO_UPDATE_CLEAR_COLOR));
   assert(!(batch->flags & BLORP_BATCH_PREDICATE_ENABLE));
   assert(params->hiz_op == ISL_AUX_OP_NONE);

   blorp_measure_start(batch, params);

#if GFX_VER >= 7
   const struct intel_device_info *devinfo = batch->blorp->compiler->elk->devinfo;
   const struct elk_cs_prog_data *cs_prog_data = params->cs_prog_data;
   const struct elk_stage_prog_data *prog_data = &cs_prog_data->base;
   const struct intel_cs_dispatch_info dispatch =
      elk_cs_get_dispatch_info(devinfo, cs_prog_data, NULL);

   uint32_t group_x0 = params->x0 / cs_prog_data->local_size[0];
   uint32_t group_y0 = params->y0 / cs_prog_data->local_size[1];
   uint32_t group_z0 = params->dst.z_offset;
   uint32_t group_x1 = DIV_ROUND_UP(params->x1, cs_prog_data->local_size[0]);
   uint32_t group_y1 = DIV_ROUND_UP(params->y1, cs_prog_data->local_size[1]);
   assert(params->num_layers >= 1);
   uint32_t group_z1 = params->dst.z_offset + params->num_layers;
   assert(cs_prog_data->local_size[2] == 1);

#endif /* GFX_VER >= 7 */

#if GFX_VER >= 7

   /* The MEDIA_VFE_STATE documentation for Gfx8+ says:
    *
    * "A stalling PIPE_CONTROL is required before MEDIA_VFE_STATE unless
    *  the only bits that are changed are scoreboard related: Scoreboard
    *  Enable, Scoreboard Type, Scoreboard Mask, Scoreboard * Delta. For
    *  these scoreboard related states, a MEDIA_STATE_FLUSH is sufficient."
    *
    * Earlier generations say "MI_FLUSH" instead of "stalling PIPE_CONTROL",
    * but MI_FLUSH isn't really a thing, so we assume they meant PIPE_CONTROL.
    */
   blorp_emit(batch, GENX(PIPE_CONTROL), pc) {
      pc.CommandStreamerStallEnable = true;
      pc.StallAtPixelScoreboard = true;
   }

   blorp_emit(batch, GENX(MEDIA_VFE_STATE), vfe) {
      assert(prog_data->total_scratch == 0);
      vfe.MaximumNumberofThreads =
         devinfo->max_cs_threads * devinfo->subslice_total - 1;
      vfe.NumberofURBEntries = GFX_VER >= 8 ? 2 : 0;
      vfe.ResetGatewayTimer =
         Resettingrelativetimerandlatchingtheglobaltimestamp;
      vfe.BypassGatewayControl = BypassingOpenGatewayCloseGatewayprotocol;
#if GFX_VER == 7
      vfe.GPGPUMode = true;
#endif
      vfe.URBEntryAllocationSize = GFX_VER >= 8 ? 2 : 0;

      const uint32_t vfe_curbe_allocation =
         ALIGN(cs_prog_data->push.per_thread.regs * dispatch.threads +
               cs_prog_data->push.cross_thread.regs, 2);
      vfe.CURBEAllocationSize = vfe_curbe_allocation;
   }

   uint32_t push_const_offset;
   unsigned push_const_size;
   blorp_get_compute_push_const(batch, params, dispatch.threads,
                                &push_const_offset, &push_const_size);

   blorp_emit(batch, GENX(MEDIA_CURBE_LOAD), curbe) {
      curbe.CURBETotalDataLength = push_const_size;
      curbe.CURBEDataStartAddress = push_const_offset;
   }

   uint32_t surfaces_offset = blorp_setup_binding_table(batch, params);

   uint32_t samplers_offset =
      params->src.enabled ? blorp_emit_sampler_state(batch) : 0;

   struct GENX(INTERFACE_DESCRIPTOR_DATA) idd = {
      .KernelStartPointer = params->cs_prog_kernel,
      .SamplerStatePointer = samplers_offset,
      .SamplerCount = params->src.enabled ? 1 : 0,
      .BindingTableEntryCount = params->src.enabled ? 2 : 1,
      .BindingTablePointer = surfaces_offset,
      .ConstantURBEntryReadLength = cs_prog_data->push.per_thread.regs,
      .NumberofThreadsinGPGPUThreadGroup = dispatch.threads,
      .SharedLocalMemorySize = elk_encode_slm_size(GFX_VER,
                                                   prog_data->total_shared),
      .BarrierEnable = cs_prog_data->uses_barrier,
#if GFX_VER >= 8 || GFX_VERx10 == 75
      .CrossThreadConstantDataReadLength =
         cs_prog_data->push.cross_thread.regs,
#endif
   };

   uint32_t idd_offset;
   uint32_t size = GENX(INTERFACE_DESCRIPTOR_DATA_length) * sizeof(uint32_t);
   void *state = blorp_alloc_dynamic_state(batch, size, 64, &idd_offset);
   GENX(INTERFACE_DESCRIPTOR_DATA_pack)(NULL, state, &idd);

   blorp_emit(batch, GENX(MEDIA_INTERFACE_DESCRIPTOR_LOAD), mid) {
      mid.InterfaceDescriptorTotalLength        = size;
      mid.InterfaceDescriptorDataStartAddress   = idd_offset;
   }

   blorp_emit(batch, GENX(GPGPU_WALKER), ggw) {
      ggw.SIMDSize                     = dispatch.simd_size / 16;
      ggw.ThreadDepthCounterMaximum    = 0;
      ggw.ThreadHeightCounterMaximum   = 0;
      ggw.ThreadWidthCounterMaximum    = dispatch.threads - 1;
      ggw.ThreadGroupIDStartingX       = group_x0;
      ggw.ThreadGroupIDStartingY       = group_y0;
#if GFX_VER >= 8
      ggw.ThreadGroupIDStartingResumeZ = group_z0;
#else
      ggw.ThreadGroupIDStartingZ       = group_z0;
#endif
      ggw.ThreadGroupIDXDimension      = group_x1;
      ggw.ThreadGroupIDYDimension      = group_y1;
      ggw.ThreadGroupIDZDimension      = group_z1;
      ggw.RightExecutionMask           = dispatch.right_mask;
      ggw.BottomExecutionMask          = 0xffffffff;
   }

#else /* GFX_VER >= 7 */

   unreachable("Compute blorp is not supported on SNB and earlier");

#endif /* GFX_VER >= 7 */

   blorp_measure_end(batch, params);
}

/**
 * \brief Execute a blit or render pass operation.
 *
 * To execute the operation, this function manually constructs and emits a
 * batch to draw a rectangle primitive. The batchbuffer is flushed before
 * constructing and after emitting the batch.
 *
 * This function alters no GL state.
 */
static void
blorp_exec(struct blorp_batch *batch, const struct blorp_params *params)
{
   /* Not supported in Gfx versions that use Elk. */
   assert((batch->flags & BLORP_BATCH_USE_BLITTER) == 0);

   if (batch->flags & BLORP_BATCH_USE_COMPUTE) {
      blorp_exec_compute(batch, params);
   } else {
      blorp_exec_3d(batch, params);
   }
}

#endif /* BLORP_GENX_EXEC_ELK_H */
