/*
 Copyright (C) Intel Corp.  2006.  All Rights Reserved.
 Intel funded Tungsten Graphics to
 develop this 3D driver.

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:

 The above copyright notice and this permission notice (including the
 next paragraph) shall be included in all copies or substantial
 portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 **********************************************************************/
 /*
  * Authors:
  *   Keith Whitwell <keithw@vmware.com>
  */

#include "elk_clip.h"


struct elk_reg get_tmp( struct elk_clip_compile *c )
{
   struct elk_reg tmp = elk_vec4_grf(c->last_tmp, 0);

   if (++c->last_tmp > c->prog_data.total_grf)
      c->prog_data.total_grf = c->last_tmp;

   return tmp;
}

static void release_tmp( struct elk_clip_compile *c, struct elk_reg tmp )
{
   if (tmp.nr == c->last_tmp-1)
      c->last_tmp--;
}


static struct elk_reg make_plane_ud(GLuint x, GLuint y, GLuint z, GLuint w)
{
   return elk_imm_ud((w<<24) | (z<<16) | (y<<8) | x);
}


void elk_clip_init_planes( struct elk_clip_compile *c )
{
   struct elk_codegen *p = &c->func;

   if (!c->key.nr_userclip) {
      elk_MOV(p, get_element_ud(c->reg.fixed_planes, 0), make_plane_ud( 0,    0, 0xff, 1));
      elk_MOV(p, get_element_ud(c->reg.fixed_planes, 1), make_plane_ud( 0,    0,    1, 1));
      elk_MOV(p, get_element_ud(c->reg.fixed_planes, 2), make_plane_ud( 0, 0xff,    0, 1));
      elk_MOV(p, get_element_ud(c->reg.fixed_planes, 3), make_plane_ud( 0,    1,    0, 1));
      elk_MOV(p, get_element_ud(c->reg.fixed_planes, 4), make_plane_ud(0xff,  0,    0, 1));
      elk_MOV(p, get_element_ud(c->reg.fixed_planes, 5), make_plane_ud( 1,    0,    0, 1));
   }
}



#define W 3

/* Project 'pos' to screen space (or back again), overwrite with results:
 */
void elk_clip_project_position(struct elk_clip_compile *c, struct elk_reg pos )
{
   struct elk_codegen *p = &c->func;

   /* calc rhw
    */
   elk_math_invert(p, get_element(pos, W), get_element(pos, W));

   /* value.xyz *= value.rhw
    */
   elk_set_default_access_mode(p, ELK_ALIGN_16);
   elk_MUL(p, elk_writemask(pos, WRITEMASK_XYZ), pos,
           elk_swizzle(pos, ELK_SWIZZLE_WWWW));
   elk_set_default_access_mode(p, ELK_ALIGN_1);
}


static void elk_clip_project_vertex( struct elk_clip_compile *c,
				     struct elk_indirect vert_addr )
{
   struct elk_codegen *p = &c->func;
   struct elk_reg tmp = get_tmp(c);
   GLuint hpos_offset = elk_varying_to_offset(&c->vue_map, VARYING_SLOT_POS);
   GLuint ndc_offset = elk_varying_to_offset(&c->vue_map,
                                             ELK_VARYING_SLOT_NDC);

   /* Fixup position.  Extract from the original vertex and re-project
    * to screen space:
    */
   elk_MOV(p, tmp, deref_4f(vert_addr, hpos_offset));
   elk_clip_project_position(c, tmp);
   elk_MOV(p, deref_4f(vert_addr, ndc_offset), tmp);

   release_tmp(c, tmp);
}




/* Interpolate between two vertices and put the result into a0.0.
 * Increment a0.0 accordingly.
 *
 * Beware that dest_ptr can be equal to v0_ptr!
 */
void elk_clip_interp_vertex( struct elk_clip_compile *c,
			     struct elk_indirect dest_ptr,
			     struct elk_indirect v0_ptr, /* from */
			     struct elk_indirect v1_ptr, /* to */
			     struct elk_reg t0,
			     bool force_edgeflag)
{
   struct elk_codegen *p = &c->func;
   struct elk_reg t_nopersp, v0_ndc_copy;
   GLuint slot;

   /* Just copy the vertex header:
    */
   /*
    * After CLIP stage, only first 256 bits of the VUE are read
    * back on Ironlake, so needn't change it
    */
   elk_copy_indirect_to_indirect(p, dest_ptr, v0_ptr, 1);


   /* First handle the 3D and NDC interpolation, in case we
    * need noperspective interpolation. Doing it early has no
    * performance impact in any case.
    */

   /* Take a copy of the v0 NDC coordinates, in case dest == v0. */
   if (c->key.contains_noperspective_varying) {
      GLuint offset = elk_varying_to_offset(&c->vue_map,
                                                 ELK_VARYING_SLOT_NDC);
      v0_ndc_copy = get_tmp(c);
      elk_MOV(p, v0_ndc_copy, deref_4f(v0_ptr, offset));
   }

   /* Compute the new 3D position
    *
    * dest_hpos = v0_hpos * (1 - t0) + v1_hpos * t0
    */
   {
      GLuint delta = elk_varying_to_offset(&c->vue_map, VARYING_SLOT_POS);
      struct elk_reg tmp = get_tmp(c);
      elk_MUL(p, vec4(elk_null_reg()), deref_4f(v1_ptr, delta), t0);
      elk_MAC(p, tmp, negate(deref_4f(v0_ptr, delta)), t0);
      elk_ADD(p, deref_4f(dest_ptr, delta), deref_4f(v0_ptr, delta), tmp);
      release_tmp(c, tmp);
   }

   /* Recreate the projected (NDC) coordinate in the new vertex header */
   elk_clip_project_vertex(c, dest_ptr);

   /* If we have noperspective attributes,
    * we need to compute the screen-space t
    */
   if (c->key.contains_noperspective_varying) {
      GLuint delta = elk_varying_to_offset(&c->vue_map,
                                                ELK_VARYING_SLOT_NDC);
      struct elk_reg tmp = get_tmp(c);
      t_nopersp = get_tmp(c);

      /* t_nopersp = vec4(v1.xy, dest.xy) */
      elk_MOV(p, t_nopersp, deref_4f(v1_ptr, delta));
      elk_MOV(p, tmp, deref_4f(dest_ptr, delta));
      elk_set_default_access_mode(p, ELK_ALIGN_16);
      elk_MOV(p,
              elk_writemask(t_nopersp, WRITEMASK_ZW),
              elk_swizzle(tmp, ELK_SWIZZLE_XYXY));

      /* t_nopersp = vec4(v1.xy, dest.xy) - v0.xyxy */
      elk_ADD(p, t_nopersp, t_nopersp,
              negate(elk_swizzle(v0_ndc_copy, ELK_SWIZZLE_XYXY)));

      /* Add the absolute values of the X and Y deltas so that if
       * the points aren't in the same place on the screen we get
       * nonzero values to divide.
       *
       * After that, we have vert1 - vert0 in t_nopersp.x and
       * vertnew - vert0 in t_nopersp.y
       *
       * t_nopersp = vec2(|v1.x  -v0.x| + |v1.y  -v0.y|,
       *                  |dest.x-v0.x| + |dest.y-v0.y|)
       */
      elk_ADD(p,
              elk_writemask(t_nopersp, WRITEMASK_XY),
              elk_abs(elk_swizzle(t_nopersp, ELK_SWIZZLE_XZXZ)),
              elk_abs(elk_swizzle(t_nopersp, ELK_SWIZZLE_YWYW)));
      elk_set_default_access_mode(p, ELK_ALIGN_1);

      /* If the points are in the same place, just substitute a
       * value to avoid divide-by-zero
       */
      elk_CMP(p, vec1(elk_null_reg()), ELK_CONDITIONAL_EQ,
              vec1(t_nopersp),
              elk_imm_f(0));
      elk_IF(p, ELK_EXECUTE_1);
      elk_MOV(p, t_nopersp, elk_imm_vf4(elk_float_to_vf(1.0),
                                        elk_float_to_vf(0.0),
                                        elk_float_to_vf(0.0),
                                        elk_float_to_vf(0.0)));
      elk_ENDIF(p);

      /* Now compute t_nopersp = t_nopersp.y/t_nopersp.x and broadcast it. */
      elk_math_invert(p, get_element(t_nopersp, 0), get_element(t_nopersp, 0));
      elk_MUL(p, vec1(t_nopersp), vec1(t_nopersp),
            vec1(suboffset(t_nopersp, 1)));
      elk_set_default_access_mode(p, ELK_ALIGN_16);
      elk_MOV(p, t_nopersp, elk_swizzle(t_nopersp, ELK_SWIZZLE_XXXX));
      elk_set_default_access_mode(p, ELK_ALIGN_1);

      release_tmp(c, tmp);
      release_tmp(c, v0_ndc_copy);
   }

   /* Now we can iterate over each attribute
    * (could be done in pairs?)
    */
   for (slot = 0; slot < c->vue_map.num_slots; slot++) {
      int varying = c->vue_map.slot_to_varying[slot];
      GLuint delta = elk_vue_slot_to_offset(slot);

      /* HPOS, NDC already handled above */
      if (varying == VARYING_SLOT_POS || varying == ELK_VARYING_SLOT_NDC)
         continue;


      if (varying == VARYING_SLOT_EDGE) {
	 if (force_edgeflag)
	    elk_MOV(p, deref_4f(dest_ptr, delta), elk_imm_f(1));
	 else
	    elk_MOV(p, deref_4f(dest_ptr, delta), deref_4f(v0_ptr, delta));
      } else if (varying == VARYING_SLOT_PSIZ) {
         /* PSIZ doesn't need interpolation because it isn't used by the
          * fragment shader.
          */
      } else if (varying < VARYING_SLOT_MAX) {
	 /* This is a true vertex result (and not a special value for the VUE
	  * header), so interpolate:
	  *
	  *        New = attr0 + t*attr1 - t*attr0
          *
          * Unless the attribute is flat shaded -- in which case just copy
          * from one of the sources (doesn't matter which; already copied from pv)
	  */
         GLuint interp = c->key.interp_mode[slot];

         if (interp != INTERP_MODE_FLAT) {
            struct elk_reg tmp = get_tmp(c);
            struct elk_reg t =
               interp == INTERP_MODE_NOPERSPECTIVE ? t_nopersp : t0;

            elk_MUL(p,
                  vec4(elk_null_reg()),
                  deref_4f(v1_ptr, delta),
                  t);

            elk_MAC(p,
                  tmp,
                  negate(deref_4f(v0_ptr, delta)),
                  t);

            elk_ADD(p,
                  deref_4f(dest_ptr, delta),
                  deref_4f(v0_ptr, delta),
                  tmp);

            release_tmp(c, tmp);
         }
         else {
            elk_MOV(p,
                  deref_4f(dest_ptr, delta),
                  deref_4f(v0_ptr, delta));
         }
      }
   }

   if (c->vue_map.num_slots % 2) {
      GLuint delta = elk_vue_slot_to_offset(c->vue_map.num_slots);

      elk_MOV(p, deref_4f(dest_ptr, delta), elk_imm_f(0));
   }

   if (c->key.contains_noperspective_varying)
      release_tmp(c, t_nopersp);
}

void elk_clip_emit_vue(struct elk_clip_compile *c,
		       struct elk_indirect vert,
                       enum elk_urb_write_flags flags,
		       GLuint header)
{
   struct elk_codegen *p = &c->func;
   bool allocate = flags & ELK_URB_WRITE_ALLOCATE;

   elk_clip_ff_sync(c);

   /* Any URB entry that is allocated must subsequently be used or discarded,
    * so it doesn't make sense to mark EOT and ALLOCATE at the same time.
    */
   assert(!(allocate && (flags & ELK_URB_WRITE_EOT)));

   /* Copy the vertex from vertn into m1..mN+1:
    */
   elk_copy_from_indirect(p, elk_message_reg(1), vert, c->nr_regs);

   /* Overwrite PrimType and PrimStart in the message header, for
    * each vertex in turn:
    */
   elk_MOV(p, get_element_ud(c->reg.R0, 2), elk_imm_ud(header));


   /* Send each vertex as a separate write to the urb.  This
    * is different to the concept in elk_sf_emit.c, where
    * subsequent writes are used to build up a single urb
    * entry.  Each of these writes instantiates a separate
    * urb entry - (I think... what about 'allocate'?)
    */
   elk_urb_WRITE(p,
		 allocate ? c->reg.R0 : retype(elk_null_reg(), ELK_REGISTER_TYPE_UD),
		 0,
		 c->reg.R0,
                 flags,
		 c->nr_regs + 1, /* msg length */
		 allocate ? 1 : 0, /* response_length */
		 0,		/* urb offset */
		 ELK_URB_SWIZZLE_NONE);
}



void elk_clip_kill_thread(struct elk_clip_compile *c)
{
   struct elk_codegen *p = &c->func;

   elk_clip_ff_sync(c);
   /* Send an empty message to kill the thread and release any
    * allocated urb entry:
    */
   elk_urb_WRITE(p,
		 retype(elk_null_reg(), ELK_REGISTER_TYPE_UD),
		 0,
		 c->reg.R0,
                 ELK_URB_WRITE_UNUSED | ELK_URB_WRITE_EOT_COMPLETE,
		 1, 		/* msg len */
		 0, 		/* response len */
		 0,
		 ELK_URB_SWIZZLE_NONE);
}




struct elk_reg elk_clip_plane0_address( struct elk_clip_compile *c )
{
   return elk_address(c->reg.fixed_planes);
}


struct elk_reg elk_clip_plane_stride( struct elk_clip_compile *c )
{
   if (c->key.nr_userclip) {
      return elk_imm_uw(16);
   }
   else {
      return elk_imm_uw(4);
   }
}


/* Distribute flatshaded attributes from provoking vertex prior to
 * clipping.
 */
void elk_clip_copy_flatshaded_attributes( struct elk_clip_compile *c,
			   GLuint to, GLuint from )
{
   struct elk_codegen *p = &c->func;

   for (int i = 0; i < c->vue_map.num_slots; i++) {
      if (c->key.interp_mode[i] == INTERP_MODE_FLAT) {
         elk_MOV(p,
                 byte_offset(c->reg.vertex[to], elk_vue_slot_to_offset(i)),
                 byte_offset(c->reg.vertex[from], elk_vue_slot_to_offset(i)));
      }
   }
}



void elk_clip_init_clipmask( struct elk_clip_compile *c )
{
   struct elk_codegen *p = &c->func;
   struct elk_reg incoming = get_element_ud(c->reg.R0, 2);

   /* Shift so that lowest outcode bit is rightmost:
    */
   elk_SHR(p, c->reg.planemask, incoming, elk_imm_ud(26));

   if (c->key.nr_userclip) {
      struct elk_reg tmp = retype(vec1(get_tmp(c)), ELK_REGISTER_TYPE_UD);

      /* Rearrange userclip outcodes so that they come directly after
       * the fixed plane bits.
       */
      if (p->devinfo->ver == 5 || p->devinfo->verx10 == 45)
         elk_AND(p, tmp, incoming, elk_imm_ud(0xff<<14));
      else
         elk_AND(p, tmp, incoming, elk_imm_ud(0x3f<<14));

      elk_SHR(p, tmp, tmp, elk_imm_ud(8));
      elk_OR(p, c->reg.planemask, c->reg.planemask, tmp);

      release_tmp(c, tmp);
   }
}

void elk_clip_ff_sync(struct elk_clip_compile *c)
{
    struct elk_codegen *p = &c->func;

    if (p->devinfo->ver == 5) {
        elk_AND(p, elk_null_reg(), c->reg.ff_sync, elk_imm_ud(0x1));
        elk_inst_set_cond_modifier(p->devinfo, elk_last_inst, ELK_CONDITIONAL_Z);
        elk_IF(p, ELK_EXECUTE_1);
        {
            elk_OR(p, c->reg.ff_sync, c->reg.ff_sync, elk_imm_ud(0x1));
            elk_ff_sync(p,
			c->reg.R0,
			0,
			c->reg.R0,
			1, /* allocate */
			1, /* response length */
			0 /* eot */);
        }
        elk_ENDIF(p);
        elk_set_default_predicate_control(p, ELK_PREDICATE_NONE);
    }
}

void elk_clip_init_ff_sync(struct elk_clip_compile *c)
{
    struct elk_codegen *p = &c->func;

    if (p->devinfo->ver == 5) {
        elk_MOV(p, c->reg.ff_sync, elk_imm_ud(0));
    }
}
