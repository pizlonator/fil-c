/**************************************************************************
 *
 * Copyright 2007 VMware, Inc.
 * Copyright 2012 Marek Olšák <maraeo@gmail.com>
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

/*
 * This converts the VBO's vertex attribute/array information into
 * Gallium vertex state and binds it.
 *
 * Authors:
 *   Keith Whitwell <keithw@vmware.com>
 *   Marek Olšák <maraeo@gmail.com>
 */

#include "st_context.h"
#include "st_atom.h"
#include "st_draw.h"
#include "st_program.h"

#include "cso_cache/cso_context.h"
#include "util/u_cpu_detect.h"
#include "util/u_math.h"
#include "util/u_upload_mgr.h"
#include "util/u_threaded_context.h"
#include "main/bufferobj.h"
#include "main/glformats.h"
#include "main/varray.h"
#include "main/arrayobj.h"

enum st_fill_tc_set_vb {
   FILL_TC_SET_VB_OFF,        /* always works */
   FILL_TC_SET_VB_ON,         /* specialized version (faster) */
};

enum st_use_vao_fast_path {
   VAO_FAST_PATH_OFF,         /* more complicated version (slower) */
   VAO_FAST_PATH_ON,          /* always works (faster) */
};

enum st_allow_zero_stride_attribs {
   ZERO_STRIDE_ATTRIBS_OFF,   /* specialized version (faster) */
   ZERO_STRIDE_ATTRIBS_ON,    /* always works */
};

/* Whether vertex attrib indices are equal to their vertex buffer indices. */
enum st_identity_attrib_mapping {
   IDENTITY_ATTRIB_MAPPING_OFF,  /* always works */
   IDENTITY_ATTRIB_MAPPING_ON,   /* specialized version (faster) */
};

enum st_allow_user_buffers {
   USER_BUFFERS_OFF,          /* specialized version (faster) */
   USER_BUFFERS_ON,           /* always works */
};

enum st_update_velems {
   UPDATE_VELEMS_OFF,         /* specialized version (faster) */
   UPDATE_VELEMS_ON,          /* always works */
};

/* Always inline the non-64bit element code, so that the compiler can see
 * that velements is on the stack.
 */
static void ALWAYS_INLINE
init_velement(struct pipe_vertex_element *velements,
              const struct gl_vertex_format *vformat,
              int src_offset, unsigned src_stride,
              unsigned instance_divisor,
              int vbo_index, bool dual_slot, int idx)
{
   velements[idx].src_offset = src_offset;
   velements[idx].src_stride = src_stride;
   velements[idx].src_format = vformat->_PipeFormat;
   velements[idx].instance_divisor = instance_divisor;
   velements[idx].vertex_buffer_index = vbo_index;
   velements[idx].dual_slot = dual_slot;
   assert(velements[idx].src_format);
}

/* ALWAYS_INLINE helps the compiler realize that most of the parameters are
 * on the stack.
 */
template<util_popcnt POPCNT,
         st_fill_tc_set_vb FILL_TC_SET_VB,
         st_use_vao_fast_path USE_VAO_FAST_PATH,
         st_allow_zero_stride_attribs ALLOW_ZERO_STRIDE_ATTRIBS,
         st_identity_attrib_mapping HAS_IDENTITY_ATTRIB_MAPPING,
         st_allow_user_buffers ALLOW_USER_BUFFERS,
         st_update_velems UPDATE_VELEMS> void ALWAYS_INLINE
setup_arrays(struct gl_context *ctx,
             const struct gl_vertex_array_object *vao,
             const GLbitfield dual_slot_inputs,
             const GLbitfield inputs_read,
             GLbitfield mask,
             struct cso_velems_state *velements,
             struct pipe_vertex_buffer *vbuffer, unsigned *num_vbuffers)
{
   /* Set up enabled vertex arrays. */
   if (USE_VAO_FAST_PATH) {
      const GLubyte *attribute_map =
         !HAS_IDENTITY_ATTRIB_MAPPING ?
               _mesa_vao_attribute_map[vao->_AttributeMapMode] : NULL;
      struct pipe_context *pipe = ctx->pipe;
      struct tc_buffer_list *next_buffer_list = NULL;

      if (FILL_TC_SET_VB)
         next_buffer_list = tc_get_next_buffer_list(pipe);

      /* Note: I did try to unroll this loop by passing the number of
       * iterations as a template parameter, but it resulted in more overhead.
       */
      while (mask) {
         const gl_vert_attrib attr = (gl_vert_attrib)u_bit_scan(&mask);
         const struct gl_array_attributes *attrib;
         const struct gl_vertex_buffer_binding *binding;

         if (HAS_IDENTITY_ATTRIB_MAPPING) {
            attrib = &vao->VertexAttrib[attr];
            binding = &vao->BufferBinding[attr];
         } else {
            attrib = &vao->VertexAttrib[attribute_map[attr]];
            binding = &vao->BufferBinding[attrib->BufferBindingIndex];
         }
         const unsigned bufidx = (*num_vbuffers)++;

         /* Set the vertex buffer. */
         if (!ALLOW_USER_BUFFERS || binding->BufferObj) {
            assert(binding->BufferObj);
            struct pipe_resource *buf =
               _mesa_get_bufferobj_reference(ctx, binding->BufferObj);
            vbuffer[bufidx].buffer.resource = buf;
            vbuffer[bufidx].is_user_buffer = false;
            vbuffer[bufidx].buffer_offset = binding->Offset +
                                            attrib->RelativeOffset;
            if (FILL_TC_SET_VB)
               tc_track_vertex_buffer(pipe, bufidx, buf, next_buffer_list);
         } else {
            vbuffer[bufidx].buffer.user = attrib->Ptr;
            vbuffer[bufidx].is_user_buffer = true;
            vbuffer[bufidx].buffer_offset = 0;
            assert(!FILL_TC_SET_VB);
         }

         if (!UPDATE_VELEMS)
            continue;

         /* Determine the vertex element index without popcnt
          * if !ALLOW_ZERO_STRIDE_ATTRIBS, which means that we don't need
          * to leave any holes for zero-stride attribs, thus the mapping from
          * vertex elements to vertex buffers is identity.
          */
         unsigned index;

         if (ALLOW_ZERO_STRIDE_ATTRIBS) {
            assert(POPCNT != POPCNT_INVALID);
            index = util_bitcount_fast<POPCNT>(inputs_read &
                                               BITFIELD_MASK(attr));
         } else {
            index = bufidx;
            assert(index == util_bitcount(inputs_read &
                                          BITFIELD_MASK(attr)));
         }

         /* Set the vertex element. */
         init_velement(velements->velems, &attrib->Format, 0, binding->Stride,
                       binding->InstanceDivisor, bufidx,
                       dual_slot_inputs & BITFIELD_BIT(attr), index);
      }
      return;
   }

   /* The slow path needs more fields initialized, which is not done if it's
    * disabled.
    */
   assert(!ctx->Const.UseVAOFastPath || vao->SharedAndImmutable);

   /* Require these because we don't use them here and we don't want to
    * generate identical template variants.
    */
   assert(!FILL_TC_SET_VB);
   assert(ALLOW_ZERO_STRIDE_ATTRIBS);
   assert(!HAS_IDENTITY_ATTRIB_MAPPING);
   assert(ALLOW_USER_BUFFERS);
   assert(UPDATE_VELEMS);

   while (mask) {
      /* The attribute index to start pulling a binding */
      const gl_vert_attrib i = (gl_vert_attrib)(ffs(mask) - 1);
      const struct gl_vertex_buffer_binding *const binding
         = _mesa_draw_buffer_binding(vao, i);
      const unsigned bufidx = (*num_vbuffers)++;

      if (binding->BufferObj) {
         /* Set the binding */
         vbuffer[bufidx].buffer.resource =
            _mesa_get_bufferobj_reference(ctx, binding->BufferObj);
         vbuffer[bufidx].is_user_buffer = false;
         vbuffer[bufidx].buffer_offset = _mesa_draw_binding_offset(binding);
      } else {
         /* Set the binding */
         const void *ptr = (const void *)_mesa_draw_binding_offset(binding);
         vbuffer[bufidx].buffer.user = ptr;
         vbuffer[bufidx].is_user_buffer = true;
         vbuffer[bufidx].buffer_offset = 0;
      }

      const GLbitfield boundmask = _mesa_draw_bound_attrib_bits(binding);
      GLbitfield attrmask = mask & boundmask;
      /* Mark the those attributes as processed */
      mask &= ~boundmask;
      /* We can assume that we have array for the binding */
      assert(attrmask);


      /* Walk attributes belonging to the binding */
      do {
         const gl_vert_attrib attr = (gl_vert_attrib)u_bit_scan(&attrmask);
         const struct gl_array_attributes *const attrib
            = _mesa_draw_array_attrib(vao, attr);
         const GLuint off = _mesa_draw_attributes_relative_offset(attrib);
         assert(POPCNT != POPCNT_INVALID);

         init_velement(velements->velems, &attrib->Format, off,
                       binding->Stride, binding->InstanceDivisor, bufidx,
                       dual_slot_inputs & BITFIELD_BIT(attr),
                       util_bitcount_fast<POPCNT>(inputs_read &
                                                  BITFIELD_MASK(attr)));
      } while (attrmask);
   }
}

/* Only used by the select/feedback mode. */
void
st_setup_arrays(struct st_context *st,
                const struct gl_vertex_program *vp,
                const struct st_common_variant *vp_variant,
                struct cso_velems_state *velements,
                struct pipe_vertex_buffer *vbuffer, unsigned *num_vbuffers)
{
   struct gl_context *ctx = st->ctx;
   GLbitfield enabled_arrays = _mesa_get_enabled_vertex_arrays(ctx);

   setup_arrays<POPCNT_NO, FILL_TC_SET_VB_OFF, VAO_FAST_PATH_ON,
                ZERO_STRIDE_ATTRIBS_ON, IDENTITY_ATTRIB_MAPPING_OFF,
                USER_BUFFERS_ON, UPDATE_VELEMS_ON>
      (ctx, ctx->Array._DrawVAO, vp->Base.DualSlotInputs,
       vp_variant->vert_attrib_mask,
       vp_variant->vert_attrib_mask & enabled_arrays,
       velements, vbuffer, num_vbuffers);
}

/* ALWAYS_INLINE helps the compiler realize that most of the parameters are
 * on the stack.
 *
 * Return the index of the vertex buffer where current attribs have been
 * uploaded.
 */
template<util_popcnt POPCNT,
         st_fill_tc_set_vb FILL_TC_SET_VB,
         st_update_velems UPDATE_VELEMS> void ALWAYS_INLINE
st_setup_current(struct st_context *st,
                 const GLbitfield dual_slot_inputs,
                 const GLbitfield inputs_read,
                 GLbitfield curmask,
                 struct cso_velems_state *velements,
                 struct pipe_vertex_buffer *vbuffer, unsigned *num_vbuffers)
{
   /* Process values that should have better been uniforms in the application */
   if (curmask) {
      struct gl_context *ctx = st->ctx;
      assert(POPCNT != POPCNT_INVALID);
      unsigned num_attribs = util_bitcount_fast<POPCNT>(curmask);
      unsigned num_dual_attribs = util_bitcount_fast<POPCNT>(curmask &
                                                             dual_slot_inputs);
      /* num_attribs includes num_dual_attribs, so adding num_dual_attribs
       * doubles the size of those attribs.
       */
      unsigned max_size = (num_attribs + num_dual_attribs) * 16;

      const unsigned bufidx = (*num_vbuffers)++;
      vbuffer[bufidx].is_user_buffer = false;
      vbuffer[bufidx].buffer.resource = NULL;
      /* vbuffer[bufidx].buffer_offset is set below */

      /* Use const_uploader for zero-stride vertex attributes, because
       * it may use a better memory placement than stream_uploader.
       * The reason is that zero-stride attributes can be fetched many
       * times (thousands of times), so a better placement is going to
       * perform better.
       */
      struct u_upload_mgr *uploader = st->can_bind_const_buffer_as_vertex ?
                                      st->pipe->const_uploader :
                                      st->pipe->stream_uploader;
      uint8_t *ptr = NULL;

      u_upload_alloc(uploader, 0, max_size, 16,
                     &vbuffer[bufidx].buffer_offset,
                     &vbuffer[bufidx].buffer.resource, (void**)&ptr);
      uint8_t *cursor = ptr;

      if (FILL_TC_SET_VB) {
         struct pipe_context *pipe = ctx->pipe;
         tc_track_vertex_buffer(pipe, bufidx, vbuffer[bufidx].buffer.resource,
                                tc_get_next_buffer_list(pipe));
      }

      do {
         const gl_vert_attrib attr = (gl_vert_attrib)u_bit_scan(&curmask);
         const struct gl_array_attributes *const attrib
            = _mesa_draw_current_attrib(ctx, attr);
         const unsigned size = attrib->Format._ElementSize;

         /* When the current attribs are set (e.g. via glColor3ub or
          * glVertexAttrib2s), they are always converted to float32 or int32
          * or dual slots being 2x int32, so they are always dword-aligned.
          * glBegin/End behaves in the same way. It's really an internal Mesa
          * inefficiency that is convenient here, which is why this assertion
          * is always true.
          */
         assert(size % 4 == 0); /* assume a hw-friendly alignment */
         memcpy(cursor, attrib->Ptr, size);

         if (UPDATE_VELEMS) {
            init_velement(velements->velems, &attrib->Format, cursor - ptr,
                          0, 0, bufidx, dual_slot_inputs & BITFIELD_BIT(attr),
                          util_bitcount_fast<POPCNT>(inputs_read &
                                                     BITFIELD_MASK(attr)));
         }

         cursor += size;
      } while (curmask);

      /* Always unmap. The uploader might use explicit flushes. */
      u_upload_unmap(uploader);
   }
}

/* Only used by the select/feedback mode. */
void
st_setup_current_user(struct st_context *st,
                      const struct gl_vertex_program *vp,
                      const struct st_common_variant *vp_variant,
                      struct cso_velems_state *velements,
                      struct pipe_vertex_buffer *vbuffer, unsigned *num_vbuffers)
{
   struct gl_context *ctx = st->ctx;
   const GLbitfield enabled_arrays = _mesa_get_enabled_vertex_arrays(ctx);
   const GLbitfield inputs_read = vp_variant->vert_attrib_mask;
   const GLbitfield dual_slot_inputs = vp->Base.DualSlotInputs;

   /* Process values that should have better been uniforms in the application */
   GLbitfield curmask = inputs_read & ~enabled_arrays;
   /* For each attribute, make an own user buffer binding. */
   while (curmask) {
      const gl_vert_attrib attr = (gl_vert_attrib)u_bit_scan(&curmask);
      const struct gl_array_attributes *const attrib
         = _mesa_draw_current_attrib(ctx, attr);
      const unsigned bufidx = (*num_vbuffers)++;

      init_velement(velements->velems, &attrib->Format, 0, 0, 0,
                    bufidx, dual_slot_inputs & BITFIELD_BIT(attr),
                    util_bitcount(inputs_read & BITFIELD_MASK(attr)));

      vbuffer[bufidx].is_user_buffer = true;
      vbuffer[bufidx].buffer.user = attrib->Ptr;
      vbuffer[bufidx].buffer_offset = 0;
   }
}

template<util_popcnt POPCNT,
         st_fill_tc_set_vb FILL_TC_SET_VB,
         st_use_vao_fast_path USE_VAO_FAST_PATH,
         st_allow_zero_stride_attribs ALLOW_ZERO_STRIDE_ATTRIBS,
         st_identity_attrib_mapping HAS_IDENTITY_ATTRIB_MAPPING,
         st_allow_user_buffers ALLOW_USER_BUFFERS,
         st_update_velems UPDATE_VELEMS> void ALWAYS_INLINE
st_update_array_templ(struct st_context *st,
                      const GLbitfield enabled_arrays,
                      const GLbitfield enabled_user_arrays,
                      const GLbitfield nonzero_divisor_arrays)
{
   struct gl_context *ctx = st->ctx;

   /* vertex program validation must be done before this */
   /* _NEW_PROGRAM, ST_NEW_VS_STATE */
   const struct gl_vertex_program *vp =
      (struct gl_vertex_program *)ctx->VertexProgram._Current;
   const struct st_common_variant *vp_variant = st->vp_variant;
   const GLbitfield inputs_read = vp_variant->vert_attrib_mask;
   const GLbitfield dual_slot_inputs = vp->Base.DualSlotInputs;
   const GLbitfield userbuf_arrays =
      ALLOW_USER_BUFFERS ? inputs_read & enabled_user_arrays : 0;
   bool uses_user_vertex_buffers = userbuf_arrays != 0;

   st->draw_needs_minmax_index =
      (userbuf_arrays & ~nonzero_divisor_arrays) != 0;

   struct pipe_vertex_buffer vbuffer_local[PIPE_MAX_ATTRIBS];
   struct pipe_vertex_buffer *vbuffer;
   unsigned num_vbuffers = 0, num_vbuffers_tc;
   struct cso_velems_state velements;

   if (FILL_TC_SET_VB) {
      assert(!uses_user_vertex_buffers);
      assert(POPCNT != POPCNT_INVALID);
      num_vbuffers_tc = util_bitcount_fast<POPCNT>(inputs_read &
                                                   enabled_arrays);

      /* Add up to 1 vertex buffer for zero-stride vertex attribs. */
      num_vbuffers_tc += ALLOW_ZERO_STRIDE_ATTRIBS &&
                         inputs_read & ~enabled_arrays;
      vbuffer = tc_add_set_vertex_buffers_call(st->pipe, num_vbuffers_tc);
   } else {
      vbuffer = vbuffer_local;
   }

   /* ST_NEW_VERTEX_ARRAYS */
   /* Setup arrays */
   setup_arrays<POPCNT, FILL_TC_SET_VB, USE_VAO_FAST_PATH,
                ALLOW_ZERO_STRIDE_ATTRIBS, HAS_IDENTITY_ATTRIB_MAPPING,
                ALLOW_USER_BUFFERS, UPDATE_VELEMS>
      (ctx, ctx->Array._DrawVAO, dual_slot_inputs, inputs_read,
       inputs_read & enabled_arrays, &velements, vbuffer, &num_vbuffers);

   /* _NEW_CURRENT_ATTRIB */
   /* Setup zero-stride attribs. */
   if (ALLOW_ZERO_STRIDE_ATTRIBS) {
      st_setup_current<POPCNT, FILL_TC_SET_VB, UPDATE_VELEMS>
         (st, dual_slot_inputs, inputs_read, inputs_read & ~enabled_arrays,
          &velements, vbuffer, &num_vbuffers);
   } else {
      assert(!(inputs_read & ~enabled_arrays));
   }

   if (FILL_TC_SET_VB)
         assert(num_vbuffers == num_vbuffers_tc);

   if (UPDATE_VELEMS) {
      struct cso_context *cso = st->cso_context;
      velements.count = vp->num_inputs + vp_variant->key.passthrough_edgeflags;

      /* Set vertex buffers and elements. */
      if (FILL_TC_SET_VB) {
         cso_set_vertex_elements(cso, &velements);
      } else {
         cso_set_vertex_buffers_and_elements(cso, &velements, num_vbuffers,
                                             uses_user_vertex_buffers, vbuffer);
      }
      /* The driver should clear this after it has processed the update. */
      ctx->Array.NewVertexElements = false;
      st->uses_user_vertex_buffers = uses_user_vertex_buffers;
   } else {
      /* Only vertex buffers. */
      if (!FILL_TC_SET_VB)
         cso_set_vertex_buffers(st->cso_context, num_vbuffers, true, vbuffer);

      /* This can change only when we update vertex elements. */
      assert(st->uses_user_vertex_buffers == uses_user_vertex_buffers);
   }
}

typedef void (*update_array_func)(struct st_context *st,
                                  const GLbitfield enabled_arrays,
                                  const GLbitfield enabled_user_attribs,
                                  const GLbitfield nonzero_divisor_attribs);

/* This just initializes the table of all st_update_array variants. */
struct st_update_array_table {
   update_array_func funcs[2][2][2][2][2][2];

   template<util_popcnt POPCNT,
            st_fill_tc_set_vb FILL_TC_SET_VB,
            st_allow_zero_stride_attribs ALLOW_ZERO_STRIDE_ATTRIBS,
            st_identity_attrib_mapping HAS_IDENTITY_ATTRIB_MAPPING,
            st_allow_user_buffers ALLOW_USER_BUFFERS,
            st_update_velems UPDATE_VELEMS>
   void init_one()
   {
      /* These conditions reduce the number of compiled variants. */
      /* The TC path is only valid without user buffers.
       */
      constexpr st_fill_tc_set_vb fill_tc_set_vb =
         !ALLOW_USER_BUFFERS ? FILL_TC_SET_VB : FILL_TC_SET_VB_OFF;

      /* POPCNT is unused without zero-stride attribs and without TC. */
      constexpr util_popcnt popcnt =
         !ALLOW_ZERO_STRIDE_ATTRIBS && !fill_tc_set_vb ?
            POPCNT_INVALID : POPCNT;

      funcs[POPCNT][FILL_TC_SET_VB][ALLOW_ZERO_STRIDE_ATTRIBS]
           [HAS_IDENTITY_ATTRIB_MAPPING][ALLOW_USER_BUFFERS][UPDATE_VELEMS] =
         st_update_array_templ<
            popcnt,
            fill_tc_set_vb,
            VAO_FAST_PATH_ON,
            ALLOW_ZERO_STRIDE_ATTRIBS,
            HAS_IDENTITY_ATTRIB_MAPPING,
            ALLOW_USER_BUFFERS,
            UPDATE_VELEMS>;
   }

   /* We have to do this in stages because of the combinatorial explosion of
    * variants.
    */
   template<util_popcnt POPCNT,
            st_fill_tc_set_vb FILL_TC_SET_VB,
            st_allow_zero_stride_attribs ALLOW_ZERO_STRIDE_ATTRIBS>
   void init_last_3_args()
   {
      init_one<POPCNT, FILL_TC_SET_VB, ALLOW_ZERO_STRIDE_ATTRIBS,
               IDENTITY_ATTRIB_MAPPING_OFF, USER_BUFFERS_OFF,
               UPDATE_VELEMS_OFF>();
      init_one<POPCNT, FILL_TC_SET_VB, ALLOW_ZERO_STRIDE_ATTRIBS,
               IDENTITY_ATTRIB_MAPPING_OFF,
               USER_BUFFERS_OFF, UPDATE_VELEMS_ON>();
      init_one<POPCNT, FILL_TC_SET_VB, ALLOW_ZERO_STRIDE_ATTRIBS,
               IDENTITY_ATTRIB_MAPPING_OFF,
               USER_BUFFERS_ON,  UPDATE_VELEMS_OFF>();
      init_one<POPCNT, FILL_TC_SET_VB, ALLOW_ZERO_STRIDE_ATTRIBS,
               IDENTITY_ATTRIB_MAPPING_OFF,
               USER_BUFFERS_ON,  UPDATE_VELEMS_ON>();
      init_one<POPCNT, FILL_TC_SET_VB, ALLOW_ZERO_STRIDE_ATTRIBS,
               IDENTITY_ATTRIB_MAPPING_ON,
               USER_BUFFERS_OFF, UPDATE_VELEMS_OFF>();
      init_one<POPCNT, FILL_TC_SET_VB, ALLOW_ZERO_STRIDE_ATTRIBS,
               IDENTITY_ATTRIB_MAPPING_ON,
               USER_BUFFERS_OFF, UPDATE_VELEMS_ON>();
      init_one<POPCNT, FILL_TC_SET_VB, ALLOW_ZERO_STRIDE_ATTRIBS,
               IDENTITY_ATTRIB_MAPPING_ON,
               USER_BUFFERS_ON,  UPDATE_VELEMS_OFF>();
      init_one<POPCNT, FILL_TC_SET_VB, ALLOW_ZERO_STRIDE_ATTRIBS,
               IDENTITY_ATTRIB_MAPPING_ON,
               USER_BUFFERS_ON,  UPDATE_VELEMS_ON>();
   }

   st_update_array_table()
   {
      init_last_3_args<POPCNT_NO,  FILL_TC_SET_VB_OFF,
                       ZERO_STRIDE_ATTRIBS_OFF>();
      init_last_3_args<POPCNT_NO,  FILL_TC_SET_VB_OFF,
                       ZERO_STRIDE_ATTRIBS_ON>();
      init_last_3_args<POPCNT_NO,  FILL_TC_SET_VB_ON,
                       ZERO_STRIDE_ATTRIBS_OFF>();
      init_last_3_args<POPCNT_NO,  FILL_TC_SET_VB_ON,
                       ZERO_STRIDE_ATTRIBS_ON>();
      init_last_3_args<POPCNT_YES, FILL_TC_SET_VB_OFF,
                       ZERO_STRIDE_ATTRIBS_OFF>();
      init_last_3_args<POPCNT_YES, FILL_TC_SET_VB_OFF,
                       ZERO_STRIDE_ATTRIBS_ON>();
      init_last_3_args<POPCNT_YES, FILL_TC_SET_VB_ON,
                       ZERO_STRIDE_ATTRIBS_OFF>();
      init_last_3_args<POPCNT_YES, FILL_TC_SET_VB_ON,
                       ZERO_STRIDE_ATTRIBS_ON>();
   }
};

static st_update_array_table update_array_table;

template<util_popcnt POPCNT,
         st_use_vao_fast_path USE_VAO_FAST_PATH> void ALWAYS_INLINE
st_update_array_impl(struct st_context *st)
{
   struct gl_context *ctx = st->ctx;
   struct gl_vertex_array_object *vao = ctx->Array._DrawVAO;
   const GLbitfield enabled_arrays = _mesa_get_enabled_vertex_arrays(ctx);
   GLbitfield enabled_user_arrays;
   GLbitfield nonzero_divisor_arrays;

   assert(vao->_EnabledWithMapMode ==
          _mesa_vao_enable_to_vp_inputs(vao->_AttributeMapMode, vao->Enabled));

   if (!USE_VAO_FAST_PATH && !vao->SharedAndImmutable)
      _mesa_update_vao_derived_arrays(ctx, vao, false);

   _mesa_get_derived_vao_masks(ctx, enabled_arrays, &enabled_user_arrays,
                               &nonzero_divisor_arrays);

   /* Execute the slow path without using multiple C++ template variants. */
   if (!USE_VAO_FAST_PATH) {
      st_update_array_templ<POPCNT, FILL_TC_SET_VB_OFF, VAO_FAST_PATH_OFF,
                            ZERO_STRIDE_ATTRIBS_ON, IDENTITY_ATTRIB_MAPPING_OFF,
                            USER_BUFFERS_ON, UPDATE_VELEMS_ON>
         (st, enabled_arrays, enabled_user_arrays, nonzero_divisor_arrays);
      return;
   }

   /* The fast path that selects from multiple C++ template variants. */
   const GLbitfield inputs_read = st->vp_variant->vert_attrib_mask;
   const GLbitfield enabled_arrays_read = inputs_read & enabled_arrays;

   /* Check cso_context whether it goes directly to TC. */
   bool fill_tc_set_vbs = st->cso_context->draw_vbo == tc_draw_vbo;
   bool has_zero_stride_attribs = inputs_read & ~enabled_arrays;
   uint32_t non_identity_attrib_mapping =
      vao->_AttributeMapMode == ATTRIBUTE_MAP_MODE_IDENTITY ? 0 :
      vao->_AttributeMapMode == ATTRIBUTE_MAP_MODE_POSITION ? VERT_BIT_GENERIC0
                                                            : VERT_BIT_POS;
   bool has_identity_mapping = !(enabled_arrays_read &
                                 (vao->NonIdentityBufferAttribMapping |
                                  non_identity_attrib_mapping));
   /* has_user_buffers is always false with glthread. */
   bool has_user_buffers = inputs_read & enabled_user_arrays;
   /* Changing from user to non-user buffers and vice versa can switch between
    * cso and u_vbuf, which means that we need to update vertex elements even
    * when they have not changed.
    */
   bool update_velems = ctx->Array.NewVertexElements ||
                        st->uses_user_vertex_buffers != has_user_buffers;

   update_array_table.funcs[POPCNT][fill_tc_set_vbs][has_zero_stride_attribs]
                           [has_identity_mapping][has_user_buffers]
                           [update_velems]
      (st, enabled_arrays, enabled_user_arrays, nonzero_divisor_arrays);
}

/* The default callback that must be present before st_init_update_array
 * selects the driver-dependent variant.
 */
void
st_update_array(struct st_context *st)
{
   unreachable("st_init_update_array not called");
}

void
st_init_update_array(struct st_context *st)
{
   st_update_func_t *func = &st->update_functions[ST_NEW_VERTEX_ARRAYS_INDEX];

   if (util_get_cpu_caps()->has_popcnt) {
      if (st->ctx->Const.UseVAOFastPath)
         *func = st_update_array_impl<POPCNT_YES, VAO_FAST_PATH_ON>;
      else
         *func = st_update_array_impl<POPCNT_YES, VAO_FAST_PATH_OFF>;
   } else {
      if (st->ctx->Const.UseVAOFastPath)
         *func = st_update_array_impl<POPCNT_NO, VAO_FAST_PATH_ON>;
      else
         *func = st_update_array_impl<POPCNT_NO, VAO_FAST_PATH_OFF>;
   }
}

struct pipe_vertex_state *
st_create_gallium_vertex_state(struct gl_context *ctx,
                               const struct gl_vertex_array_object *vao,
                               struct gl_buffer_object *indexbuf,
                               uint32_t enabled_arrays)
{
   struct st_context *st = st_context(ctx);
   const GLbitfield inputs_read = enabled_arrays;
   const GLbitfield dual_slot_inputs = 0; /* always zero */
   struct pipe_vertex_buffer vbuffer[PIPE_MAX_ATTRIBS];
   unsigned num_vbuffers = 0;
   struct cso_velems_state velements;

   /* This should use the slow path because there is only 1 interleaved
    * vertex buffers.
    */
   setup_arrays<POPCNT_NO, FILL_TC_SET_VB_OFF, VAO_FAST_PATH_OFF,
                ZERO_STRIDE_ATTRIBS_ON, IDENTITY_ATTRIB_MAPPING_OFF,
                USER_BUFFERS_ON, UPDATE_VELEMS_ON>
      (ctx, vao, dual_slot_inputs, inputs_read, inputs_read, &velements,
       vbuffer, &num_vbuffers);

   if (num_vbuffers != 1) {
      assert(!"this should never happen with display lists");
      return NULL;
   }

   velements.count = util_bitcount(inputs_read);

   struct pipe_screen *screen = st->screen;
   struct pipe_vertex_state *state =
      screen->create_vertex_state(screen, &vbuffer[0], velements.velems,
                                  velements.count,
                                  indexbuf ?
                                  indexbuf->buffer : NULL,
                                  enabled_arrays);

   for (unsigned i = 0; i < num_vbuffers; i++)
      pipe_vertex_buffer_unreference(&vbuffer[i]);
   return state;
}
