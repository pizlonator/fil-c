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
#include "elk_prim.h"


/* This is performed against the original triangles, so no indirection
 * required:
BZZZT!
 */
static void compute_tri_direction( struct elk_clip_compile *c )
{
   struct elk_codegen *p = &c->func;
   struct elk_reg e = c->reg.tmp0;
   struct elk_reg f = c->reg.tmp1;
   GLuint hpos_offset = elk_varying_to_offset(&c->vue_map, VARYING_SLOT_POS);
   struct elk_reg v0 = byte_offset(c->reg.vertex[0], hpos_offset);
   struct elk_reg v1 = byte_offset(c->reg.vertex[1], hpos_offset);
   struct elk_reg v2 = byte_offset(c->reg.vertex[2], hpos_offset);


   struct elk_reg v0n = get_tmp(c);
   struct elk_reg v1n = get_tmp(c);
   struct elk_reg v2n = get_tmp(c);

   /* Convert to NDC.
    * NOTE: We can't modify the original vertex coordinates,
    * as it may impact further operations.
    * So, we have to keep normalized coordinates in temp registers.
    *
    * TBD-KC
    * Try to optimize unnecessary MOV's.
    */
   elk_MOV(p, v0n, v0);
   elk_MOV(p, v1n, v1);
   elk_MOV(p, v2n, v2);

   elk_clip_project_position(c, v0n);
   elk_clip_project_position(c, v1n);
   elk_clip_project_position(c, v2n);

   /* Calculate the vectors of two edges of the triangle:
    */
   elk_ADD(p, e, v0n, negate(v2n));
   elk_ADD(p, f, v1n, negate(v2n));

   /* Take their crossproduct:
    */
   elk_set_default_access_mode(p, ELK_ALIGN_16);
   elk_MUL(p, vec4(elk_null_reg()), elk_swizzle(e, ELK_SWIZZLE_YZXW),
           elk_swizzle(f, ELK_SWIZZLE_ZXYW));
   elk_MAC(p, vec4(e),  negate(elk_swizzle(e, ELK_SWIZZLE_ZXYW)),
           elk_swizzle(f, ELK_SWIZZLE_YZXW));
   elk_set_default_access_mode(p, ELK_ALIGN_1);

   elk_MUL(p, c->reg.dir, c->reg.dir, vec4(e));
}


static void cull_direction( struct elk_clip_compile *c )
{
   struct elk_codegen *p = &c->func;
   GLuint conditional;

   assert (!(c->key.fill_ccw == ELK_CLIP_FILL_MODE_CULL &&
	     c->key.fill_cw == ELK_CLIP_FILL_MODE_CULL));

   if (c->key.fill_ccw == ELK_CLIP_FILL_MODE_CULL)
      conditional = ELK_CONDITIONAL_GE;
   else
      conditional = ELK_CONDITIONAL_L;

   elk_CMP(p,
	   vec1(elk_null_reg()),
	   conditional,
	   get_element(c->reg.dir, 2),
	   elk_imm_f(0));

   elk_IF(p, ELK_EXECUTE_1);
   {
      elk_clip_kill_thread(c);
   }
   elk_ENDIF(p);
}



static void copy_bfc( struct elk_clip_compile *c )
{
   struct elk_codegen *p = &c->func;
   GLuint conditional;

   /* Do we have any colors to copy?
    */
   if (!(elk_clip_have_varying(c, VARYING_SLOT_COL0) &&
         elk_clip_have_varying(c, VARYING_SLOT_BFC0)) &&
       !(elk_clip_have_varying(c, VARYING_SLOT_COL1) &&
         elk_clip_have_varying(c, VARYING_SLOT_BFC1)))
      return;

   /* In some weird degenerate cases we can end up testing the
    * direction twice, once for culling and once for bfc copying.  Oh
    * well, that's what you get for setting weird GL state.
    */
   if (c->key.copy_bfc_ccw)
      conditional = ELK_CONDITIONAL_GE;
   else
      conditional = ELK_CONDITIONAL_L;

   elk_CMP(p,
	   vec1(elk_null_reg()),
	   conditional,
	   get_element(c->reg.dir, 2),
	   elk_imm_f(0));

   elk_IF(p, ELK_EXECUTE_1);
   {
      GLuint i;

      for (i = 0; i < 3; i++) {
	 if (elk_clip_have_varying(c, VARYING_SLOT_COL0) &&
             elk_clip_have_varying(c, VARYING_SLOT_BFC0))
	    elk_MOV(p,
		    byte_offset(c->reg.vertex[i],
                                elk_varying_to_offset(&c->vue_map,
                                                      VARYING_SLOT_COL0)),
		    byte_offset(c->reg.vertex[i],
                                elk_varying_to_offset(&c->vue_map,
                                                      VARYING_SLOT_BFC0)));

	 if (elk_clip_have_varying(c, VARYING_SLOT_COL1) &&
             elk_clip_have_varying(c, VARYING_SLOT_BFC1))
	    elk_MOV(p,
		    byte_offset(c->reg.vertex[i],
                                elk_varying_to_offset(&c->vue_map,
                                                      VARYING_SLOT_COL1)),
		    byte_offset(c->reg.vertex[i],
                                elk_varying_to_offset(&c->vue_map,
                                                      VARYING_SLOT_BFC1)));
      }
   }
   elk_ENDIF(p);
}




/*
  GLfloat iz	= 1.0 / dir.z;
  GLfloat ac	= dir.x * iz;
  GLfloat bc	= dir.y * iz;
  offset = ctx->Polygon.OffsetUnits * DEPTH_SCALE;
  offset += MAX2( abs(ac), abs(bc) ) * ctx->Polygon.OffsetFactor;
  if (ctx->Polygon.OffsetClamp && isfinite(ctx->Polygon.OffsetClamp)) {
    if (ctx->Polygon.OffsetClamp < 0)
      offset = MAX2( offset, ctx->Polygon.OffsetClamp );
    else
      offset = MIN2( offset, ctx->Polygon.OffsetClamp );
  }
  offset *= MRD;
*/
static void compute_offset( struct elk_clip_compile *c )
{
   struct elk_codegen *p = &c->func;
   struct elk_reg off = c->reg.offset;
   struct elk_reg dir = c->reg.dir;

   elk_math_invert(p, get_element(off, 2), get_element(dir, 2));
   elk_MUL(p, vec2(off), vec2(dir), get_element(off, 2));

   elk_CMP(p,
	   vec1(elk_null_reg()),
	   ELK_CONDITIONAL_GE,
	   elk_abs(get_element(off, 0)),
	   elk_abs(get_element(off, 1)));

   elk_SEL(p, vec1(off),
           elk_abs(get_element(off, 0)), elk_abs(get_element(off, 1)));
   elk_inst_set_pred_control(p->devinfo, elk_last_inst, ELK_PREDICATE_NORMAL);

   elk_MUL(p, vec1(off), vec1(off), elk_imm_f(c->key.offset_factor));
   elk_ADD(p, vec1(off), vec1(off), elk_imm_f(c->key.offset_units));
   if (c->key.offset_clamp && isfinite(c->key.offset_clamp)) {
      elk_CMP(p,
              vec1(elk_null_reg()),
              c->key.offset_clamp < 0 ? ELK_CONDITIONAL_GE : ELK_CONDITIONAL_L,
              vec1(off),
              elk_imm_f(c->key.offset_clamp));
      elk_SEL(p, vec1(off), vec1(off), elk_imm_f(c->key.offset_clamp));
   }
}


static void merge_edgeflags( struct elk_clip_compile *c )
{
   struct elk_codegen *p = &c->func;
   struct elk_reg tmp0 = get_element_ud(c->reg.tmp0, 0);

   elk_AND(p, tmp0, get_element_ud(c->reg.R0, 2), elk_imm_ud(PRIM_MASK));
   elk_CMP(p,
	   vec1(elk_null_reg()),
	   ELK_CONDITIONAL_EQ,
	   tmp0,
	   elk_imm_ud(_3DPRIM_POLYGON));

   /* Get away with using reg.vertex because we know that this is not
    * a _3DPRIM_TRISTRIP_REVERSE:
    */
   elk_IF(p, ELK_EXECUTE_1);
   {
      elk_AND(p, vec1(elk_null_reg()), get_element_ud(c->reg.R0, 2), elk_imm_ud(1<<8));
      elk_inst_set_cond_modifier(p->devinfo, elk_last_inst, ELK_CONDITIONAL_EQ);
      elk_MOV(p, byte_offset(c->reg.vertex[0],
                             elk_varying_to_offset(&c->vue_map,
                                                   VARYING_SLOT_EDGE)),
              elk_imm_f(0));
      elk_inst_set_pred_control(p->devinfo, elk_last_inst, ELK_PREDICATE_NORMAL);

      elk_AND(p, vec1(elk_null_reg()), get_element_ud(c->reg.R0, 2), elk_imm_ud(1<<9));
      elk_inst_set_cond_modifier(p->devinfo, elk_last_inst, ELK_CONDITIONAL_EQ);
      elk_MOV(p, byte_offset(c->reg.vertex[2],
                             elk_varying_to_offset(&c->vue_map,
                                                   VARYING_SLOT_EDGE)),
              elk_imm_f(0));
      elk_inst_set_pred_control(p->devinfo, elk_last_inst, ELK_PREDICATE_NORMAL);
   }
   elk_ENDIF(p);
}



static void apply_one_offset( struct elk_clip_compile *c,
			  struct elk_indirect vert )
{
   struct elk_codegen *p = &c->func;
   GLuint ndc_offset = elk_varying_to_offset(&c->vue_map,
                                             ELK_VARYING_SLOT_NDC);
   struct elk_reg z = deref_1f(vert, ndc_offset +
			       2 * type_sz(ELK_REGISTER_TYPE_F));

   elk_ADD(p, z, z, vec1(c->reg.offset));
}



/***********************************************************************
 * Output clipped polygon as an unfilled primitive:
 */
static void emit_lines(struct elk_clip_compile *c,
		       bool do_offset)
{
   struct elk_codegen *p = &c->func;
   struct elk_indirect v0 = elk_indirect(0, 0);
   struct elk_indirect v1 = elk_indirect(1, 0);
   struct elk_indirect v0ptr = elk_indirect(2, 0);
   struct elk_indirect v1ptr = elk_indirect(3, 0);

   /* Need a separate loop for offset:
    */
   if (do_offset) {
      elk_MOV(p, c->reg.loopcount, c->reg.nr_verts);
      elk_MOV(p, get_addr_reg(v0ptr), elk_address(c->reg.inlist));

      elk_DO(p, ELK_EXECUTE_1);
      {
	 elk_MOV(p, get_addr_reg(v0), deref_1uw(v0ptr, 0));
	 elk_ADD(p, get_addr_reg(v0ptr), get_addr_reg(v0ptr), elk_imm_uw(2));

	 apply_one_offset(c, v0);

	 elk_ADD(p, c->reg.loopcount, c->reg.loopcount, elk_imm_d(-1));
         elk_inst_set_cond_modifier(p->devinfo, elk_last_inst, ELK_CONDITIONAL_G);
      }
      elk_WHILE(p);
      elk_inst_set_pred_control(p->devinfo, elk_last_inst, ELK_PREDICATE_NORMAL);
   }

   /* v1ptr = &inlist[nr_verts]
    * *v1ptr = v0
    */
   elk_MOV(p, c->reg.loopcount, c->reg.nr_verts);
   elk_MOV(p, get_addr_reg(v0ptr), elk_address(c->reg.inlist));
   elk_ADD(p, get_addr_reg(v1ptr), get_addr_reg(v0ptr), retype(c->reg.nr_verts, ELK_REGISTER_TYPE_UW));
   elk_ADD(p, get_addr_reg(v1ptr), get_addr_reg(v1ptr), retype(c->reg.nr_verts, ELK_REGISTER_TYPE_UW));
   elk_MOV(p, deref_1uw(v1ptr, 0), deref_1uw(v0ptr, 0));

   elk_DO(p, ELK_EXECUTE_1);
   {
      elk_MOV(p, get_addr_reg(v0), deref_1uw(v0ptr, 0));
      elk_MOV(p, get_addr_reg(v1), deref_1uw(v0ptr, 2));
      elk_ADD(p, get_addr_reg(v0ptr), get_addr_reg(v0ptr), elk_imm_uw(2));

      /* draw edge if edgeflag != 0 */
      elk_CMP(p,
	      vec1(elk_null_reg()), ELK_CONDITIONAL_NZ,
	      deref_1f(v0, elk_varying_to_offset(&c->vue_map,
                                                 VARYING_SLOT_EDGE)),
	      elk_imm_f(0));
      elk_IF(p, ELK_EXECUTE_1);
      {
	 elk_clip_emit_vue(c, v0, ELK_URB_WRITE_ALLOCATE_COMPLETE,
                           (_3DPRIM_LINESTRIP << URB_WRITE_PRIM_TYPE_SHIFT)
                           | URB_WRITE_PRIM_START);
	 elk_clip_emit_vue(c, v1, ELK_URB_WRITE_ALLOCATE_COMPLETE,
                           (_3DPRIM_LINESTRIP << URB_WRITE_PRIM_TYPE_SHIFT)
                           | URB_WRITE_PRIM_END);
      }
      elk_ENDIF(p);

      elk_ADD(p, c->reg.loopcount, c->reg.loopcount, elk_imm_d(-1));
      elk_inst_set_cond_modifier(p->devinfo, elk_last_inst, ELK_CONDITIONAL_NZ);
   }
   elk_WHILE(p);
   elk_inst_set_pred_control(p->devinfo, elk_last_inst, ELK_PREDICATE_NORMAL);
}



static void emit_points(struct elk_clip_compile *c,
			bool do_offset )
{
   struct elk_codegen *p = &c->func;

   struct elk_indirect v0 = elk_indirect(0, 0);
   struct elk_indirect v0ptr = elk_indirect(2, 0);

   elk_MOV(p, c->reg.loopcount, c->reg.nr_verts);
   elk_MOV(p, get_addr_reg(v0ptr), elk_address(c->reg.inlist));

   elk_DO(p, ELK_EXECUTE_1);
   {
      elk_MOV(p, get_addr_reg(v0), deref_1uw(v0ptr, 0));
      elk_ADD(p, get_addr_reg(v0ptr), get_addr_reg(v0ptr), elk_imm_uw(2));

      /* draw if edgeflag != 0
       */
      elk_CMP(p,
	      vec1(elk_null_reg()), ELK_CONDITIONAL_NZ,
	      deref_1f(v0, elk_varying_to_offset(&c->vue_map,
                                                 VARYING_SLOT_EDGE)),
	      elk_imm_f(0));
      elk_IF(p, ELK_EXECUTE_1);
      {
	 if (do_offset)
	    apply_one_offset(c, v0);

	 elk_clip_emit_vue(c, v0, ELK_URB_WRITE_ALLOCATE_COMPLETE,
                           (_3DPRIM_POINTLIST << URB_WRITE_PRIM_TYPE_SHIFT)
                           | URB_WRITE_PRIM_START | URB_WRITE_PRIM_END);
      }
      elk_ENDIF(p);

      elk_ADD(p, c->reg.loopcount, c->reg.loopcount, elk_imm_d(-1));
      elk_inst_set_cond_modifier(p->devinfo, elk_last_inst, ELK_CONDITIONAL_NZ);
   }
   elk_WHILE(p);
   elk_inst_set_pred_control(p->devinfo, elk_last_inst, ELK_PREDICATE_NORMAL);
}







static void emit_primitives( struct elk_clip_compile *c,
			     GLuint mode,
			     bool do_offset )
{
   switch (mode) {
   case ELK_CLIP_FILL_MODE_FILL:
      elk_clip_tri_emit_polygon(c);
      break;

   case ELK_CLIP_FILL_MODE_LINE:
      emit_lines(c, do_offset);
      break;

   case ELK_CLIP_FILL_MODE_POINT:
      emit_points(c, do_offset);
      break;

   case ELK_CLIP_FILL_MODE_CULL:
      unreachable("not reached");
   }
}



static void emit_unfilled_primitives( struct elk_clip_compile *c )
{
   struct elk_codegen *p = &c->func;

   /* Direction culling has already been done.
    */
   if (c->key.fill_ccw != c->key.fill_cw &&
       c->key.fill_ccw != ELK_CLIP_FILL_MODE_CULL &&
       c->key.fill_cw != ELK_CLIP_FILL_MODE_CULL)
   {
      elk_CMP(p,
	      vec1(elk_null_reg()),
	      ELK_CONDITIONAL_GE,
	      get_element(c->reg.dir, 2),
	      elk_imm_f(0));

      elk_IF(p, ELK_EXECUTE_1);
      {
	 emit_primitives(c, c->key.fill_ccw, c->key.offset_ccw);
      }
      elk_ELSE(p);
      {
	 emit_primitives(c, c->key.fill_cw, c->key.offset_cw);
      }
      elk_ENDIF(p);
   }
   else if (c->key.fill_cw != ELK_CLIP_FILL_MODE_CULL) {
      emit_primitives(c, c->key.fill_cw, c->key.offset_cw);
   }
   else if (c->key.fill_ccw != ELK_CLIP_FILL_MODE_CULL) {
      emit_primitives(c, c->key.fill_ccw, c->key.offset_ccw);
   }
}




static void check_nr_verts( struct elk_clip_compile *c )
{
   struct elk_codegen *p = &c->func;

   elk_CMP(p, vec1(elk_null_reg()), ELK_CONDITIONAL_L, c->reg.nr_verts, elk_imm_d(3));
   elk_IF(p, ELK_EXECUTE_1);
   {
      elk_clip_kill_thread(c);
   }
   elk_ENDIF(p);
}


void elk_emit_unfilled_clip( struct elk_clip_compile *c )
{
   struct elk_codegen *p = &c->func;

   c->need_direction = ((c->key.offset_ccw || c->key.offset_cw) ||
			(c->key.fill_ccw != c->key.fill_cw) ||
			c->key.fill_ccw == ELK_CLIP_FILL_MODE_CULL ||
			c->key.fill_cw == ELK_CLIP_FILL_MODE_CULL ||
			c->key.copy_bfc_cw ||
			c->key.copy_bfc_ccw);

   elk_clip_tri_alloc_regs(c, 3 + c->key.nr_userclip + 6);
   elk_clip_tri_init_vertices(c);
   elk_clip_init_ff_sync(c);

   assert(elk_clip_have_varying(c, VARYING_SLOT_EDGE));

   if (c->key.fill_ccw == ELK_CLIP_FILL_MODE_CULL &&
       c->key.fill_cw == ELK_CLIP_FILL_MODE_CULL) {
      elk_clip_kill_thread(c);
      return;
   }

   merge_edgeflags(c);

   /* Need to use the inlist indirection here:
    */
   if (c->need_direction)
      compute_tri_direction(c);

   if (c->key.fill_ccw == ELK_CLIP_FILL_MODE_CULL ||
       c->key.fill_cw == ELK_CLIP_FILL_MODE_CULL)
      cull_direction(c);

   if (c->key.offset_ccw ||
       c->key.offset_cw)
      compute_offset(c);

   if (c->key.copy_bfc_ccw ||
       c->key.copy_bfc_cw)
      copy_bfc(c);

   /* Need to do this whether we clip or not:
    */
   if (c->key.contains_flat_varying)
      elk_clip_tri_flat_shade(c);

   elk_clip_init_clipmask(c);
   elk_CMP(p, vec1(elk_null_reg()), ELK_CONDITIONAL_NZ, c->reg.planemask, elk_imm_ud(0));
   elk_IF(p, ELK_EXECUTE_1);
   {
      elk_clip_init_planes(c);
      elk_clip_tri(c);
      check_nr_verts(c);
   }
   elk_ENDIF(p);

   emit_unfilled_primitives(c);
   elk_clip_kill_thread(c);
}
