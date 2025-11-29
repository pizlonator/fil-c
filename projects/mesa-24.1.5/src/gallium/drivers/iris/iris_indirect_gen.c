/* Copyright © 2023 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <errno.h>

#ifdef HAVE_VALGRIND
#include <valgrind.h>
#include <memcheck.h>
#define VG(x) x
#else
#define VG(x)
#endif

#include "pipe/p_defines.h"
#include "pipe/p_state.h"
#include "pipe/p_context.h"
#include "pipe/p_screen.h"
#include "util/u_upload_mgr.h"
#include "compiler/nir/nir_builder.h"
#include "compiler/nir/nir_serialize.h"
#include "intel/common/intel_aux_map.h"
#include "intel/common/intel_l3_config.h"
#include "intel/common/intel_sample_positions.h"
#include "intel/ds/intel_tracepoints.h"
#include "iris_batch.h"
#include "iris_context.h"
#include "iris_defines.h"
#include "iris_pipe.h"
#include "iris_resource.h"
#include "iris_utrace.h"

#include "iris_genx_macros.h"

#if GFX_VER >= 9
#include "intel/compiler/brw_compiler.h"
#include "intel/common/intel_genX_state_brw.h"
#else
#include "intel/compiler/elk/elk_compiler.h"
#include "intel/common/intel_genX_state_elk.h"
#endif

#include "libintel_shaders.h"

#if GFX_VERx10 == 80
# include "intel_gfx8_shaders_code.h"
#elif GFX_VERx10 == 90
# include "intel_gfx9_shaders_code.h"
#elif GFX_VERx10 == 110
# include "intel_gfx11_shaders_code.h"
#elif GFX_VERx10 == 120
# include "intel_gfx12_shaders_code.h"
#elif GFX_VERx10 == 125
# include "intel_gfx125_shaders_code.h"
#elif GFX_VERx10 == 200
# include "intel_gfx20_shaders_code.h"
#else
# error "Unsupported generation"
#endif

#define load_param(b, bit_size, struct_name, field_name)          \
   nir_load_uniform(b, 1, bit_size, nir_imm_int(b, 0),            \
                    .base = offsetof(struct_name, field_name),   \
                    .range = bit_size / 8)

static nir_def *
load_fragment_index(nir_builder *b)
{
   nir_def *pos_in = nir_f2i32(b, nir_trim_vector(b, nir_load_frag_coord(b), 2));
   return nir_iadd(b,
                   nir_imul_imm(b, nir_channel(b, pos_in, 1), 8192),
                   nir_channel(b, pos_in, 0));
}

static nir_shader *
load_shader_lib(struct iris_screen *screen, void *mem_ctx)
{
   const nir_shader_compiler_options *nir_options =
#if GFX_VER >= 9
      screen->brw->nir_options[MESA_SHADER_KERNEL];
#else
      screen->elk->nir_options[MESA_SHADER_KERNEL];
#endif

   struct blob_reader blob;
   blob_reader_init(&blob, (void *)genX(intel_shaders_nir),
                    sizeof(genX(intel_shaders_nir)));
   return nir_deserialize(mem_ctx, nir_options, &blob);
}

static unsigned
iris_call_generation_shader(struct iris_screen *screen, nir_builder *b)
{
   genX(libiris_write_draw)(
      b,
      load_param(b, 64, struct iris_gen_indirect_params, generated_cmds_addr),
      load_param(b, 64, struct iris_gen_indirect_params, indirect_data_addr),
      load_param(b, 64, struct iris_gen_indirect_params, draw_id_addr),
      load_param(b, 32, struct iris_gen_indirect_params, indirect_data_stride),
      load_param(b, 64, struct iris_gen_indirect_params, draw_count_addr),
      load_param(b, 32, struct iris_gen_indirect_params, draw_base),
      load_param(b, 32, struct iris_gen_indirect_params, max_draw_count),
      load_param(b, 32, struct iris_gen_indirect_params, flags),
      load_param(b, 32, struct iris_gen_indirect_params, ring_count),
      load_param(b, 64, struct iris_gen_indirect_params, gen_addr),
      load_param(b, 64, struct iris_gen_indirect_params, end_addr),
      load_fragment_index(b));
   return sizeof(struct iris_gen_indirect_params);
}

void
genX(init_screen_gen_state)(struct iris_screen *screen)
{
   screen->vtbl.load_shader_lib = load_shader_lib;
   screen->vtbl.call_generation_shader = iris_call_generation_shader;
}

/**
 * Stream out temporary/short-lived state.
 *
 * This allocates space, pins the BO, and includes the BO address in the
 * returned offset (which works because all state lives in 32-bit memory
 * zones).
 */
static void *
upload_state(struct iris_batch *batch,
             struct u_upload_mgr *uploader,
             struct iris_state_ref *ref,
             unsigned size,
             unsigned alignment)
{
   void *p = NULL;
   u_upload_alloc(uploader, 0, size, alignment, &ref->offset, &ref->res, &p);
   iris_use_pinned_bo(batch, iris_resource_bo(ref->res), false, IRIS_DOMAIN_NONE);
   return p;
}

static uint32_t *
stream_state(struct iris_batch *batch,
             struct u_upload_mgr *uploader,
             struct pipe_resource **out_res,
             unsigned size,
             unsigned alignment,
             uint32_t *out_offset)
{
   void *ptr = NULL;

   u_upload_alloc(uploader, 0, size, alignment, out_offset, out_res, &ptr);

   struct iris_bo *bo = iris_resource_bo(*out_res);
   iris_use_pinned_bo(batch, bo, false, IRIS_DOMAIN_NONE);

   iris_record_state_size(batch->state_sizes,
                          bo->address + *out_offset, size);

   *out_offset += iris_bo_offset_from_base_address(bo);

   return ptr;
}

static void
emit_indirect_generate_draw(struct iris_batch *batch,
                            struct iris_address params_addr,
                            unsigned params_size,
                            unsigned ring_count)
{
   struct iris_screen *screen = batch->screen;
   struct iris_context *ice = batch->ice;
   struct isl_device *isl_dev = &screen->isl_dev;
   const struct intel_device_info *devinfo = screen->devinfo;

   /* State emission */
   uint32_t ves_dws[1 + 2 * GENX(VERTEX_ELEMENT_STATE_length)];
   iris_pack_command(GENX(3DSTATE_VERTEX_ELEMENTS), ves_dws, ve) {
      ve.DWordLength = 1 + GENX(VERTEX_ELEMENT_STATE_length) * 2 -
                           GENX(3DSTATE_VERTEX_ELEMENTS_length_bias);
   }
   iris_pack_state(GENX(VERTEX_ELEMENT_STATE), &ves_dws[1], ve) {
      ve.VertexBufferIndex = 1;
      ve.Valid = true;
      ve.SourceElementFormat = ISL_FORMAT_R32G32B32A32_FLOAT;
      ve.SourceElementOffset = 0;
      ve.Component0Control = VFCOMP_STORE_SRC;
      ve.Component1Control = VFCOMP_STORE_0;
      ve.Component2Control = VFCOMP_STORE_0;
      ve.Component3Control = VFCOMP_STORE_0;
   }
   iris_pack_state(GENX(VERTEX_ELEMENT_STATE), &ves_dws[3], ve) {
      ve.VertexBufferIndex   = 0;
      ve.Valid               = true;
      ve.SourceElementFormat = ISL_FORMAT_R32G32B32_FLOAT;
      ve.SourceElementOffset = 0;
      ve.Component0Control   = VFCOMP_STORE_SRC;
      ve.Component1Control   = VFCOMP_STORE_SRC;
      ve.Component2Control   = VFCOMP_STORE_SRC;
      ve.Component3Control   = VFCOMP_STORE_1_FP;
   }

   iris_batch_emit(batch, ves_dws, sizeof(ves_dws));

   iris_emit_cmd(batch, GENX(3DSTATE_VF_STATISTICS), vf);
   iris_emit_cmd(batch, GENX(3DSTATE_VF_SGVS), sgvs) {
      sgvs.InstanceIDEnable = true;
      sgvs.InstanceIDComponentNumber = COMP_1;
      sgvs.InstanceIDElementOffset = 0;
   }
#if GFX_VER >= 11
   iris_emit_cmd(batch, GENX(3DSTATE_VF_SGVS_2), sgvs);
#endif
   iris_emit_cmd(batch, GENX(3DSTATE_VF_INSTANCING), vfi) {
      vfi.InstancingEnable   = false;
      vfi.VertexElementIndex = 0;
   }
   iris_emit_cmd(batch, GENX(3DSTATE_VF_INSTANCING), vfi) {
      vfi.InstancingEnable   = false;
      vfi.VertexElementIndex = 1;
   }

   iris_emit_cmd(batch, GENX(3DSTATE_VF_TOPOLOGY), topo) {
      topo.PrimitiveTopologyType = _3DPRIM_RECTLIST;
   }

   ice->shaders.urb.cfg.size[MESA_SHADER_VERTEX] = 1;
   ice->shaders.urb.cfg.size[MESA_SHADER_TESS_CTRL] = 1;
   ice->shaders.urb.cfg.size[MESA_SHADER_TESS_EVAL] = 1;
   ice->shaders.urb.cfg.size[MESA_SHADER_GEOMETRY] = 1;
   genX(emit_urb_config)(batch,
                         false /* has_tess_eval */,
                         false /* has_geometry */);

   iris_emit_cmd(batch, GENX(3DSTATE_PS_BLEND), ps_blend) {
      ps_blend.HasWriteableRT = true;
   }

   iris_emit_cmd(batch, GENX(3DSTATE_WM_DEPTH_STENCIL), wm);

#if GFX_VER >= 12
   iris_emit_cmd(batch, GENX(3DSTATE_DEPTH_BOUNDS), db) {
      db.DepthBoundsTestEnable = false;
      db.DepthBoundsTestMinValue = 0.0;
      db.DepthBoundsTestMaxValue = 1.0;
   }
#endif

   iris_emit_cmd(batch, GENX(3DSTATE_MULTISAMPLE), ms);
   iris_emit_cmd(batch, GENX(3DSTATE_SAMPLE_MASK), sm) {
      sm.SampleMask = 0x1;
   }

   iris_emit_cmd(batch, GENX(3DSTATE_VS), vs);
   iris_emit_cmd(batch, GENX(3DSTATE_HS), hs);
   iris_emit_cmd(batch, GENX(3DSTATE_TE), te);
   iris_emit_cmd(batch, GENX(3DSTATE_DS), DS);

   iris_emit_cmd(batch, GENX(3DSTATE_STREAMOUT), so);

   iris_emit_cmd(batch, GENX(3DSTATE_GS), gs);

   iris_emit_cmd(batch, GENX(3DSTATE_CLIP), clip) {
      clip.PerspectiveDivideDisable = true;
   }

   iris_emit_cmd(batch, GENX(3DSTATE_SF), sf) {
#if GFX_VER >= 12
      sf.DerefBlockSize = ice->state.urb_deref_block_size;
#endif
   }

   iris_emit_cmd(batch, GENX(3DSTATE_RASTER), raster) {
      raster.CullMode = CULLMODE_NONE;
   }

   const struct iris_compiled_shader *shader = ice->draw.generation.shader;
   const struct iris_fs_data *fs_data = iris_fs_data_const(shader);

   iris_emit_cmd(batch, GENX(3DSTATE_SBE), sbe) {
      sbe.VertexURBEntryReadOffset = 1;
      sbe.NumberofSFOutputAttributes = fs_data->num_varying_inputs;
      sbe.VertexURBEntryReadLength = MAX2((fs_data->num_varying_inputs + 1) / 2, 1);
      sbe.ConstantInterpolationEnable = fs_data->flat_inputs;
      sbe.ForceVertexURBEntryReadLength = true;
      sbe.ForceVertexURBEntryReadOffset = true;
#if GFX_VER >= 9
      for (unsigned i = 0; i < 32; i++)
         sbe.AttributeActiveComponentFormat[i] = ACF_XYZW;
#endif
   }

   iris_emit_cmd(batch, GENX(3DSTATE_WM), wm) {
      if (fs_data->has_side_effects || fs_data->uses_kill)
         wm.ForceThreadDispatchEnable = ForceON;
   }

   iris_emit_cmd(batch, GENX(3DSTATE_PS), ps) {
#if GFX_VER >= 9
      struct brw_wm_prog_data *wm_prog_data = brw_wm_prog_data(shader->brw_prog_data);
#else
      struct elk_wm_prog_data *wm_prog_data = elk_wm_prog_data(shader->elk_prog_data);
#endif
      intel_set_ps_dispatch_state(&ps, devinfo, wm_prog_data,
                                  1 /* rasterization_samples */,
                                  0 /* msaa_flags */);

      ps.VectorMaskEnable       = fs_data->uses_vmask;

      ps.BindingTableEntryCount = GFX_VER == 9 ? 1 : 0;
#if GFX_VER < 20
      ps.PushConstantEnable     = shader->nr_params > 0 ||
                                  shader->ubo_ranges[0].length;
#endif

#if GFX_VER >= 9
      ps.DispatchGRFStartRegisterForConstantSetupData0 =
         brw_wm_prog_data_dispatch_grf_start_reg(wm_prog_data, ps, 0);
      ps.DispatchGRFStartRegisterForConstantSetupData1 =
         brw_wm_prog_data_dispatch_grf_start_reg(wm_prog_data, ps, 1);
#if GFX_VER < 20
      ps.DispatchGRFStartRegisterForConstantSetupData2 =
         brw_wm_prog_data_dispatch_grf_start_reg(wm_prog_data, ps, 2);
#endif

      ps.KernelStartPointer0 = KSP(ice->draw.generation.shader) +
         brw_wm_prog_data_prog_offset(wm_prog_data, ps, 0);
      ps.KernelStartPointer1 = KSP(ice->draw.generation.shader) +
         brw_wm_prog_data_prog_offset(wm_prog_data, ps, 1);
#if GFX_VER < 20
      ps.KernelStartPointer2 = KSP(ice->draw.generation.shader) +
         brw_wm_prog_data_prog_offset(wm_prog_data, ps, 2);
#endif
#else
      ps.DispatchGRFStartRegisterForConstantSetupData0 =
         elk_wm_prog_data_dispatch_grf_start_reg(wm_prog_data, ps, 0);
      ps.DispatchGRFStartRegisterForConstantSetupData1 =
         elk_wm_prog_data_dispatch_grf_start_reg(wm_prog_data, ps, 1);
      ps.DispatchGRFStartRegisterForConstantSetupData2 =
         elk_wm_prog_data_dispatch_grf_start_reg(wm_prog_data, ps, 2);

      ps.KernelStartPointer0 = KSP(ice->draw.generation.shader) +
         elk_wm_prog_data_prog_offset(wm_prog_data, ps, 0);
      ps.KernelStartPointer1 = KSP(ice->draw.generation.shader) +
         elk_wm_prog_data_prog_offset(wm_prog_data, ps, 1);
      ps.KernelStartPointer2 = KSP(ice->draw.generation.shader) +
         elk_wm_prog_data_prog_offset(wm_prog_data, ps, 2);
#endif

      ps.MaximumNumberofThreadsPerPSD = devinfo->max_threads_per_psd - 1;
   }

   iris_emit_cmd(batch, GENX(3DSTATE_PS_EXTRA), psx) {
      psx.PixelShaderValid = true;
#if GFX_VER < 20
      psx.AttributeEnable = fs_data->num_varying_inputs > 0;
#endif
      psx.PixelShaderIsPerSample = fs_data->is_per_sample;
      psx.PixelShaderComputedDepthMode = fs_data->computed_depth_mode;
#if GFX_VER >= 9
#if GFX_VER >= 20
      assert(!fs_data->pulls_bary);
#else
      psx.PixelShaderPullsBary = fs_data->pulls_bary;
#endif
      psx.PixelShaderComputesStencil = fs_data->computed_stencil;
#endif
      psx.PixelShaderHasUAV = GFX_VER == 8;
   }

   iris_emit_cmd(batch, GENX(3DSTATE_VIEWPORT_STATE_POINTERS_CC), cc) {
      uint32_t cc_vp_address;
      uint32_t *cc_vp_map =
         stream_state(batch, ice->state.dynamic_uploader,
                      &ice->state.last_res.cc_vp,
                      4 * GENX(CC_VIEWPORT_length), 32, &cc_vp_address);

      iris_pack_state(GENX(CC_VIEWPORT), cc_vp_map, ccv) {
         ccv.MinimumDepth = 0.0f;
         ccv.MaximumDepth = 1.0f;
      }
      cc.CCViewportPointer = cc_vp_address;
   }

#if GFX_VER >= 12
   /* Disable Primitive Replication. */
   iris_emit_cmd(batch, GENX(3DSTATE_PRIMITIVE_REPLICATION), pr);
#endif

#if GFX_VERx10 == 125
   /* DG2: Wa_22011440098
    * MTL: Wa_18022330953
    *
    * In 3D mode, after programming push constant alloc command immediately
    * program push constant command(ZERO length) without any commit between
    * them.
    *
    * Note that Wa_16011448509 isn't needed here as all address bits are zero.
    */
   iris_emit_cmd(batch, GENX(3DSTATE_CONSTANT_ALL), c) {
      /* Update empty push constants for all stages (bitmask = 11111b) */
      c.ShaderUpdateEnable = 0x1f;
      c.MOCS = iris_mocs(NULL, isl_dev, 0);
   }
#endif

   float x0 = 0.0f, x1 = MIN2(ring_count, 8192);
   float y0 = 0.0f, y1 = DIV_ROUND_UP(ring_count, 8192);
   float z = 0.0f;

   float *vertices =
      upload_state(batch, ice->state.dynamic_uploader,
                   &ice->draw.generation.vertices,
                   ALIGN(9 * sizeof(float), 8), 8);

   vertices[0] = x1; vertices[1] = y1; vertices[2] = z; /* v0 */
   vertices[3] = x0; vertices[4] = y1; vertices[5] = z; /* v1 */
   vertices[6] = x0; vertices[7] = y0; vertices[8] = z; /* v2 */


   uint32_t vbs_dws[1 + GENX(VERTEX_BUFFER_STATE_length)];
   iris_pack_command(GENX(3DSTATE_VERTEX_BUFFERS), vbs_dws, vbs) {
      vbs.DWordLength = ARRAY_SIZE(vbs_dws) -
                        GENX(3DSTATE_VERTEX_BUFFERS_length_bias);
   }
   _iris_pack_state(batch, GENX(VERTEX_BUFFER_STATE), &vbs_dws[1], vb) {
      vb.VertexBufferIndex     = 0;
      vb.AddressModifyEnable   = true;
      vb.BufferStartingAddress = ro_bo(iris_resource_bo(ice->draw.generation.vertices.res),
                                       ice->draw.generation.vertices.offset);
      vb.BufferPitch           = 3 * sizeof(float);
      vb.BufferSize            = 9 * sizeof(float);
      vb.MOCS                  = iris_mocs(NULL, isl_dev, ISL_SURF_USAGE_VERTEX_BUFFER_BIT);
#if GFX_VER >= 12
      vb.L3BypassDisable       = true;
#endif
   }
   iris_batch_emit(batch, vbs_dws, sizeof(vbs_dws));

#if GFX_VERx10 > 120
   uint32_t const_dws[GENX(3DSTATE_CONSTANT_ALL_length) +
                      GENX(3DSTATE_CONSTANT_ALL_DATA_length)];

   iris_pack_command(GENX(3DSTATE_CONSTANT_ALL), const_dws, all) {
      all.DWordLength = ARRAY_SIZE(const_dws) -
         GENX(3DSTATE_CONSTANT_ALL_length_bias);
      all.ShaderUpdateEnable = 1 << MESA_SHADER_FRAGMENT;
      all.MOCS = isl_mocs(isl_dev, 0, false);
      all.PointerBufferMask = 0x1;
   }
   _iris_pack_state(batch, GENX(3DSTATE_CONSTANT_ALL_DATA),
                    &const_dws[GENX(3DSTATE_CONSTANT_ALL_length)], data) {
      data.PointerToConstantBuffer = params_addr;
      data.ConstantBufferReadLength = DIV_ROUND_UP(params_size, 32);
   }
   iris_batch_emit(batch, const_dws, sizeof(const_dws));
#else
   /* The Skylake PRM contains the following restriction:
    *
    *    "The driver must ensure The following case does not occur without a
    *     flush to the 3D engine: 3DSTATE_CONSTANT_* with buffer 3 read length
    *     equal to zero committed followed by a 3DSTATE_CONSTANT_* with buffer
    *     0 read length not equal to zero committed."
    *
    * To avoid this, we program the highest slot.
    */
   iris_emit_cmd(batch, GENX(3DSTATE_CONSTANT_PS), c) {
#if GFX_VER > 8
      c.MOCS = iris_mocs(NULL, isl_dev, ISL_SURF_USAGE_CONSTANT_BUFFER_BIT);
#endif
      c.ConstantBody.ReadLength[3] = DIV_ROUND_UP(params_size, 32);
      c.ConstantBody.Buffer[3] = params_addr;
   }
#endif

#if GFX_VER <= 9
   /* Gfx9 requires 3DSTATE_BINDING_TABLE_POINTERS_XS to be re-emitted in
    * order to commit constants. TODO: Investigate "Disable Gather at Set
    * Shader" to go back to legacy mode...
    *
    * The null writes of the generation shader also appear to disturb the next
    * RT writes, so we choose to reemit the binding table to a null RT on Gfx8
    * too.
    */
   struct iris_binder *binder = &ice->state.binder;
   iris_emit_cmd(batch, GENX(3DSTATE_BINDING_TABLE_POINTERS_PS), ptr) {
      ptr.PointertoPSBindingTable =
         binder->bt_offset[MESA_SHADER_FRAGMENT] >> IRIS_BT_OFFSET_SHIFT;
   }
   uint32_t *bt_map = binder->map + binder->bt_offset[MESA_SHADER_FRAGMENT];
   uint32_t surf_base_offset = binder->bo->address;
   bt_map[0] = ice->state.null_fb.offset - surf_base_offset;
#endif

   genX(maybe_emit_breakpoint)(batch, true);

   iris_emit_cmd(batch, GENX(3DPRIMITIVE), prim) {
      prim.VertexAccessType         = SEQUENTIAL;
      prim.PrimitiveTopologyType    = _3DPRIM_RECTLIST;
      prim.VertexCountPerInstance   = 3;
      prim.InstanceCount            = 1;
   }


   /* We've smashed all state compared to what the normal 3D pipeline
    * rendering tracks for GL.
    */

   uint64_t skip_bits = (IRIS_DIRTY_POLYGON_STIPPLE |
                         IRIS_DIRTY_SO_BUFFERS |
                         IRIS_DIRTY_SO_DECL_LIST |
                         IRIS_DIRTY_LINE_STIPPLE |
                         IRIS_ALL_DIRTY_FOR_COMPUTE |
                         IRIS_DIRTY_SCISSOR_RECT |
                         IRIS_DIRTY_VF);
   /* Wa_14016820455
    * On Gfx 12.5 platforms, the SF_CL_VIEWPORT pointer can be invalidated
    * likely by a read cache invalidation when clipping is disabled, so we
    * don't skip its dirty bit here, in order to reprogram it.
    */
   if (GFX_VERx10 != 125)
      skip_bits |= IRIS_DIRTY_SF_CL_VIEWPORT;

   uint64_t skip_stage_bits = (IRIS_ALL_STAGE_DIRTY_FOR_COMPUTE |
                               IRIS_STAGE_DIRTY_UNCOMPILED_VS |
                               IRIS_STAGE_DIRTY_UNCOMPILED_TCS |
                               IRIS_STAGE_DIRTY_UNCOMPILED_TES |
                               IRIS_STAGE_DIRTY_UNCOMPILED_GS |
                               IRIS_STAGE_DIRTY_UNCOMPILED_FS |
                               IRIS_STAGE_DIRTY_SAMPLER_STATES_VS |
                               IRIS_STAGE_DIRTY_SAMPLER_STATES_TCS |
                               IRIS_STAGE_DIRTY_SAMPLER_STATES_TES |
                               IRIS_STAGE_DIRTY_SAMPLER_STATES_GS);

   if (!ice->shaders.prog[MESA_SHADER_TESS_EVAL]) {
      /* Generation disabled tessellation, but it was already off anyway */
      skip_stage_bits |= IRIS_STAGE_DIRTY_TCS |
                         IRIS_STAGE_DIRTY_TES |
                         IRIS_STAGE_DIRTY_CONSTANTS_TCS |
                         IRIS_STAGE_DIRTY_CONSTANTS_TES |
                         IRIS_STAGE_DIRTY_BINDINGS_TCS |
                         IRIS_STAGE_DIRTY_BINDINGS_TES;
   }

   if (!ice->shaders.prog[MESA_SHADER_GEOMETRY]) {
      /* Generation disabled geometry shaders, but it was already off
       * anyway
       */
      skip_stage_bits |= IRIS_STAGE_DIRTY_GS |
                         IRIS_STAGE_DIRTY_CONSTANTS_GS |
                         IRIS_STAGE_DIRTY_BINDINGS_GS;
   }

   ice->state.dirty |= ~skip_bits;
   ice->state.stage_dirty |= ~skip_stage_bits;

   for (int i = 0; i < ARRAY_SIZE(ice->shaders.urb.cfg.size); i++)
      ice->shaders.urb.cfg.size[i] = 0;

#if GFX_VER <= 9
   /* Now reupdate the binding tables with the new offsets for the actual
    * application shaders.
    */
   iris_binder_reserve_3d(ice);
   screen->vtbl.update_binder_address(batch, binder);
#endif
}

#define RING_SIZE (128 * 1024)

static void
ensure_ring_bo(struct iris_context *ice, struct iris_screen *screen)
{
   struct iris_bufmgr *bufmgr = screen->bufmgr;

   if (ice->draw.generation.ring_bo != NULL)
      return;

   ice->draw.generation.ring_bo =
      iris_bo_alloc(bufmgr, "gen ring",
                    RING_SIZE, 8, IRIS_MEMZONE_OTHER,
                    BO_ALLOC_NO_SUBALLOC);
   iris_get_backing_bo(ice->draw.generation.ring_bo)->real.capture = true;
}

struct iris_gen_indirect_params *
genX(emit_indirect_generate)(struct iris_batch *batch,
                             const struct pipe_draw_info *draw,
                             const struct pipe_draw_indirect_info *indirect,
                             const struct pipe_draw_start_count_bias *sc,
                             struct iris_address *out_params_addr)
{
   struct iris_screen *screen = batch->screen;
   struct iris_context *ice = batch->ice;

   iris_ensure_indirect_generation_shader(batch);
   ensure_ring_bo(ice, screen);

   const size_t struct_stride = draw->index_size > 0 ?
      sizeof(uint32_t) * 5 :
      sizeof(uint32_t) * 4;
   unsigned cmd_stride = 0;
   if (ice->state.vs_uses_draw_params ||
       ice->state.vs_uses_derived_draw_params) {
      cmd_stride += 4; /* 3DSTATE_VERTEX_BUFFERS */

      if (ice->state.vs_uses_draw_params)
         cmd_stride += 4 * GENX(VERTEX_BUFFER_STATE_length);

      if (ice->state.vs_uses_derived_draw_params)
         cmd_stride += 4 * GENX(VERTEX_BUFFER_STATE_length);
   }
   cmd_stride += 4 * GENX(3DPRIMITIVE_length);

   const unsigned setup_dws =
#if GFX_VER >= 12
      GENX(MI_ARB_CHECK_length) +
#endif
      GENX(MI_BATCH_BUFFER_START_length);
   const unsigned ring_count =
      (RING_SIZE - 4 * setup_dws) /
      (cmd_stride + 4 * 2 /* draw_id, is_indexed_draw */);

   uint32_t params_size = align(sizeof(struct iris_gen_indirect_params), 32);
   struct iris_gen_indirect_params *params =
      upload_state(batch, ice->ctx.const_uploader,
                   &ice->draw.generation.params,
                   params_size, 64);
   *out_params_addr =
      ro_bo(iris_resource_bo(ice->draw.generation.params.res),
            ice->draw.generation.params.offset);

   iris_use_pinned_bo(batch,
                      iris_resource_bo(indirect->buffer),
                      false, IRIS_DOMAIN_NONE);
   if (indirect->indirect_draw_count) {
      iris_use_pinned_bo(batch,
                         iris_resource_bo(indirect->indirect_draw_count),
                         false, IRIS_DOMAIN_NONE);
   }
   iris_use_pinned_bo(batch, ice->draw.generation.ring_bo,
                      false, IRIS_DOMAIN_NONE);

   *params = (struct iris_gen_indirect_params) {
      .generated_cmds_addr  = ice->draw.generation.ring_bo->address,
      .ring_count           = ring_count,
      .draw_id_addr         = ice->draw.generation.ring_bo->address +
                              ring_count * cmd_stride +
                              4 * GENX(MI_BATCH_BUFFER_START_length),
      .draw_count_addr      = indirect->indirect_draw_count ?
                              (iris_resource_bo(indirect->indirect_draw_count)->address +
                               indirect->indirect_draw_count_offset) : 0,
      .indirect_data_addr   = iris_resource_bo(indirect->buffer)->address +
                              indirect->offset,
      .indirect_data_stride = indirect->stride == 0 ?
                              struct_stride : indirect->stride,
      .max_draw_count       = indirect->draw_count,
      .flags                = (draw->index_size > 0 ? ANV_GENERATED_FLAG_INDEXED : 0) |
                              (ice->state.predicate == IRIS_PREDICATE_STATE_USE_BIT ?
                               ANV_GENERATED_FLAG_PREDICATED : 0) |
                              (ice->state.vs_uses_draw_params ?
                               ANV_GENERATED_FLAG_BASE : 0) |
                              (ice->state.vs_uses_derived_draw_params ?
                               ANV_GENERATED_FLAG_DRAWID : 0) |
                              (iris_mocs(NULL, &screen->isl_dev,
                                         ISL_SURF_USAGE_VERTEX_BUFFER_BIT) << 8) |
                              ((cmd_stride / 4) << 16) |
                              util_bitcount64(ice->state.bound_vertex_buffers) << 24,
   };

   genX(maybe_emit_breakpoint)(batch, true);

   emit_indirect_generate_draw(batch, *out_params_addr, params_size,
                               MIN2(ring_count, indirect->draw_count));

   genX(emit_3dprimitive_was)(batch, indirect, ice->state.prim_mode, sc->count);
   genX(maybe_emit_breakpoint)(batch, false);


   return params;
}
