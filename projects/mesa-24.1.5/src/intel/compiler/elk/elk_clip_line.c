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

static void elk_clip_line_alloc_regs( struct elk_clip_compile *c )
{
   const struct intel_device_info *devinfo = c->func.devinfo;
   GLuint i = 0,j;

   /* Register usage is static, precompute here:
    */
   c->reg.R0 = retype(elk_vec8_grf(i, 0), ELK_REGISTER_TYPE_UD); i++;

   if (c->key.nr_userclip) {
      c->reg.fixed_planes = elk_vec4_grf(i, 0);
      i += (6 + c->key.nr_userclip + 1) / 2;

      c->prog_data.curb_read_length = (6 + c->key.nr_userclip + 1) / 2;
   }
   else
      c->prog_data.curb_read_length = 0;


   /* Payload vertices plus space for more generated vertices:
    */
   for (j = 0; j < 4; j++) {
      c->reg.vertex[j] = elk_vec4_grf(i, 0);
      i += c->nr_regs;
   }

   c->reg.t           = elk_vec1_grf(i, 0);
   c->reg.t0          = elk_vec1_grf(i, 1);
   c->reg.t1          = elk_vec1_grf(i, 2);
   c->reg.planemask   = retype(elk_vec1_grf(i, 3), ELK_REGISTER_TYPE_UD);
   c->reg.plane_equation = elk_vec4_grf(i, 4);
   i++;

   c->reg.dp0         = elk_vec1_grf(i, 0); /* fixme - dp4 will clobber r.1,2,3 */
   c->reg.dp1         = elk_vec1_grf(i, 4);
   i++;

   if (!c->key.nr_userclip) {
      c->reg.fixed_planes = elk_vec8_grf(i, 0);
      i++;
   }

   c->reg.vertex_src_mask = retype(elk_vec1_grf(i, 0), ELK_REGISTER_TYPE_UD);
   c->reg.clipdistance_offset = retype(elk_vec1_grf(i, 1), ELK_REGISTER_TYPE_W);
   i++;

   if (devinfo->ver == 5) {
      c->reg.ff_sync = retype(elk_vec1_grf(i, 0), ELK_REGISTER_TYPE_UD);
      i++;
   }

   c->first_tmp = i;
   c->last_tmp = i;

   c->prog_data.urb_read_length = c->nr_regs; /* ? */
   c->prog_data.total_grf = i;
}


/* Line clipping, more or less following the following algorithm:
 *
 *  for (p=0;p<MAX_PLANES;p++) {
 *     if (clipmask & (1 << p)) {
 *        GLfloat dp0 = DOTPROD( vtx0, plane[p] );
 *        GLfloat dp1 = DOTPROD( vtx1, plane[p] );
 *
 *        if (dp1 < 0.0f) {
 *           GLfloat t = dp1 / (dp1 - dp0);
 *           if (t > t1) t1 = t;
 *        } else {
 *           GLfloat t = dp0 / (dp0 - dp1);
 *           if (t > t0) t0 = t;
 *        }
 *
 *        if (t0 + t1 >= 1.0)
 *           return;
 *     }
 *  }
 *
 *  interp( ctx, newvtx0, vtx0, vtx1, t0 );
 *  interp( ctx, newvtx1, vtx1, vtx0, t1 );
 *
 */
static void clip_and_emit_line( struct elk_clip_compile *c )
{
   struct elk_codegen *p = &c->func;
   struct elk_indirect vtx0     = elk_indirect(0, 0);
   struct elk_indirect vtx1      = elk_indirect(1, 0);
   struct elk_indirect newvtx0   = elk_indirect(2, 0);
   struct elk_indirect newvtx1   = elk_indirect(3, 0);
   struct elk_indirect plane_ptr = elk_indirect(4, 0);
   struct elk_reg v1_null_ud = retype(vec1(elk_null_reg()), ELK_REGISTER_TYPE_UD);
   GLuint hpos_offset = elk_varying_to_offset(&c->vue_map, VARYING_SLOT_POS);
   GLint clipdist0_offset = c->key.nr_userclip
      ? elk_varying_to_offset(&c->vue_map, VARYING_SLOT_CLIP_DIST0)
      : 0;

   elk_MOV(p, get_addr_reg(vtx0),      elk_address(c->reg.vertex[0]));
   elk_MOV(p, get_addr_reg(vtx1),      elk_address(c->reg.vertex[1]));
   elk_MOV(p, get_addr_reg(newvtx0),   elk_address(c->reg.vertex[2]));
   elk_MOV(p, get_addr_reg(newvtx1),   elk_address(c->reg.vertex[3]));
   elk_MOV(p, get_addr_reg(plane_ptr), elk_clip_plane0_address(c));

   /* Note: init t0, t1 together:
    */
   elk_MOV(p, vec2(c->reg.t0), elk_imm_f(0));

   elk_clip_init_planes(c);
   elk_clip_init_clipmask(c);

   /* -ve rhw workaround */
   if (p->devinfo->has_negative_rhw_bug) {
      elk_AND(p, elk_null_reg(), get_element_ud(c->reg.R0, 2),
              elk_imm_ud(1<<20));
      elk_inst_set_cond_modifier(p->devinfo, elk_last_inst, ELK_CONDITIONAL_NZ);
      elk_OR(p, c->reg.planemask, c->reg.planemask, elk_imm_ud(0x3f));
      elk_inst_set_pred_control(p->devinfo, elk_last_inst, ELK_PREDICATE_NORMAL);
   }

   /* Set the initial vertex source mask: The first 6 planes are the bounds
    * of the view volume; the next 8 planes are the user clipping planes.
    */
   elk_MOV(p, c->reg.vertex_src_mask, elk_imm_ud(0x3fc0));

   /* Set the initial clipdistance offset to be 6 floats before gl_ClipDistance[0].
    * We'll increment 6 times before we start hitting actual user clipping. */
   elk_MOV(p, c->reg.clipdistance_offset, elk_imm_d(clipdist0_offset - 6*sizeof(float)));

   elk_DO(p, ELK_EXECUTE_1);
   {
      /* if (planemask & 1)
       */
      elk_AND(p, v1_null_ud, c->reg.planemask, elk_imm_ud(1));
      elk_inst_set_cond_modifier(p->devinfo, elk_last_inst, ELK_CONDITIONAL_NZ);

      elk_IF(p, ELK_EXECUTE_1);
      {
         elk_AND(p, v1_null_ud, c->reg.vertex_src_mask, elk_imm_ud(1));
         elk_inst_set_cond_modifier(p->devinfo, elk_last_inst, ELK_CONDITIONAL_NZ);
         elk_IF(p, ELK_EXECUTE_1);
         {
            /* user clip distance: just fetch the correct float from each vertex */
            struct elk_indirect temp_ptr = elk_indirect(7, 0);
            elk_ADD(p, get_addr_reg(temp_ptr), get_addr_reg(vtx0), c->reg.clipdistance_offset);
            elk_MOV(p, c->reg.dp0, deref_1f(temp_ptr, 0));
            elk_ADD(p, get_addr_reg(temp_ptr), get_addr_reg(vtx1), c->reg.clipdistance_offset);
            elk_MOV(p, c->reg.dp1, deref_1f(temp_ptr, 0));
         }
         elk_ELSE(p);
         {
            /* fixed plane: fetch the hpos, dp4 against the plane. */
            if (c->key.nr_userclip)
               elk_MOV(p, c->reg.plane_equation, deref_4f(plane_ptr, 0));
            else
               elk_MOV(p, c->reg.plane_equation, deref_4b(plane_ptr, 0));

            elk_DP4(p, vec4(c->reg.dp0), deref_4f(vtx0, hpos_offset), c->reg.plane_equation);
            elk_DP4(p, vec4(c->reg.dp1), deref_4f(vtx1, hpos_offset), c->reg.plane_equation);
         }
         elk_ENDIF(p);

         elk_CMP(p, elk_null_reg(), ELK_CONDITIONAL_L, vec1(c->reg.dp1), elk_imm_f(0.0f));

         elk_IF(p, ELK_EXECUTE_1);
         {
             /*
              * Both can be negative on GM965/G965 due to RHW workaround
              * if so, this object should be rejected.
              */
             if (p->devinfo->has_negative_rhw_bug) {
                 elk_CMP(p, vec1(elk_null_reg()), ELK_CONDITIONAL_LE, c->reg.dp0, elk_imm_f(0.0));
                 elk_IF(p, ELK_EXECUTE_1);
                 {
                     elk_clip_kill_thread(c);
                 }
                 elk_ENDIF(p);
             }

             elk_ADD(p, c->reg.t, c->reg.dp1, negate(c->reg.dp0));
             elk_math_invert(p, c->reg.t, c->reg.t);
             elk_MUL(p, c->reg.t, c->reg.t, c->reg.dp1);

             elk_CMP(p, vec1(elk_null_reg()), ELK_CONDITIONAL_G, c->reg.t, c->reg.t1 );
             elk_MOV(p, c->reg.t1, c->reg.t);
             elk_inst_set_pred_control(p->devinfo, elk_last_inst,
                                       ELK_PREDICATE_NORMAL);
	 }
	 elk_ELSE(p);
	 {
             /* Coming back in.  We know that both cannot be negative
              * because the line would have been culled in that case.
              */

             /* If both are positive, do nothing */
             /* Only on GM965/G965 */
             if (p->devinfo->has_negative_rhw_bug) {
                 elk_CMP(p, vec1(elk_null_reg()), ELK_CONDITIONAL_L, c->reg.dp0, elk_imm_f(0.0));
                 elk_IF(p, ELK_EXECUTE_1);
             }

             {
                 elk_ADD(p, c->reg.t, c->reg.dp0, negate(c->reg.dp1));
                 elk_math_invert(p, c->reg.t, c->reg.t);
                 elk_MUL(p, c->reg.t, c->reg.t, c->reg.dp0);

                 elk_CMP(p, vec1(elk_null_reg()), ELK_CONDITIONAL_G, c->reg.t, c->reg.t0 );
                 elk_MOV(p, c->reg.t0, c->reg.t);
                 elk_inst_set_pred_control(p->devinfo, elk_last_inst,
                                           ELK_PREDICATE_NORMAL);
             }

             if (p->devinfo->has_negative_rhw_bug) {
                 elk_ENDIF(p);
             }
         }
	 elk_ENDIF(p);
      }
      elk_ENDIF(p);

      /* plane_ptr++;
       */
      elk_ADD(p, get_addr_reg(plane_ptr), get_addr_reg(plane_ptr), elk_clip_plane_stride(c));

      /* while (planemask>>=1) != 0
       */
      elk_SHR(p, c->reg.planemask, c->reg.planemask, elk_imm_ud(1));
      elk_inst_set_cond_modifier(p->devinfo, elk_last_inst, ELK_CONDITIONAL_NZ);
      elk_SHR(p, c->reg.vertex_src_mask, c->reg.vertex_src_mask, elk_imm_ud(1));
      elk_inst_set_pred_control(p->devinfo, elk_last_inst, ELK_PREDICATE_NORMAL);
      elk_ADD(p, c->reg.clipdistance_offset, c->reg.clipdistance_offset, elk_imm_w(sizeof(float)));
      elk_inst_set_pred_control(p->devinfo, elk_last_inst, ELK_PREDICATE_NORMAL);
   }
   elk_WHILE(p);
   elk_inst_set_pred_control(p->devinfo, elk_last_inst, ELK_PREDICATE_NORMAL);

   elk_ADD(p, c->reg.t, c->reg.t0, c->reg.t1);
   elk_CMP(p, vec1(elk_null_reg()), ELK_CONDITIONAL_L, c->reg.t, elk_imm_f(1.0));
   elk_IF(p, ELK_EXECUTE_1);
   {
      elk_clip_interp_vertex(c, newvtx0, vtx0, vtx1, c->reg.t0, false);
      elk_clip_interp_vertex(c, newvtx1, vtx1, vtx0, c->reg.t1, false);

      elk_clip_emit_vue(c, newvtx0, ELK_URB_WRITE_ALLOCATE_COMPLETE,
                        (_3DPRIM_LINESTRIP << URB_WRITE_PRIM_TYPE_SHIFT)
                        | URB_WRITE_PRIM_START);
      elk_clip_emit_vue(c, newvtx1, ELK_URB_WRITE_EOT_COMPLETE,
                        (_3DPRIM_LINESTRIP << URB_WRITE_PRIM_TYPE_SHIFT)
                        | URB_WRITE_PRIM_END);
   }
   elk_ENDIF(p);
   elk_clip_kill_thread(c);
}



void elk_emit_line_clip( struct elk_clip_compile *c )
{
   elk_clip_line_alloc_regs(c);
   elk_clip_init_ff_sync(c);

   if (c->key.contains_flat_varying) {
      if (c->key.pv_first)
         elk_clip_copy_flatshaded_attributes(c, 1, 0);
      else
         elk_clip_copy_flatshaded_attributes(c, 0, 1);
   }

   clip_and_emit_line(c);
}
