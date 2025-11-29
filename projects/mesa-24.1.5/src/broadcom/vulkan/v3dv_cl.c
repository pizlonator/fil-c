/*
 * Copyright © 2019 Raspberry Pi Ltd
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

#include "v3dv_private.h"

/* We don't expect that the packets we use in this file change across hw
 * versions, so we just explicitly set the V3D_VERSION and include v3dx_pack
 * here
 */
#define V3D_VERSION 42
#include "broadcom/common/v3d_macros.h"
#include "broadcom/cle/v3dx_pack.h"

/* The Control List Executor (CLE) pre-fetches V3D_CLE_READAHEAD bytes from
 * the Control List buffer. The usage of these last bytes should be avoided or
 * the CLE would pre-fetch the data after the end of the CL buffer, reporting
 * the kernel "MMU error from client CLE".
 */
#define V3D42_CLE_READAHEAD 256u
#define V3D42_CLE_BUFFER_MIN_SIZE 4096u
#define V3D71_CLE_READAHEAD 1024u
#define V3D71_CLE_BUFFER_MIN_SIZE 16384u

void
v3dv_cl_init(struct v3dv_job *job, struct v3dv_cl *cl)
{
   cl->base = NULL;
   cl->next = cl->base;
   cl->bo = NULL;
   cl->size = 0;
   cl->job = job;
   list_inithead(&cl->bo_list);
}

void
v3dv_cl_destroy(struct v3dv_cl *cl)
{
   list_for_each_entry_safe(struct v3dv_bo, bo, &cl->bo_list, list_link) {
      assert(cl->job);
      list_del(&bo->list_link);
      v3dv_bo_free(cl->job->device, bo);
   }

   /* Leave the CL in a reset state to catch use after destroy instances */
   v3dv_cl_init(NULL, cl);
}

enum v3dv_cl_chain_type {
   V3D_CL_BO_CHAIN_NONE = 0,
   V3D_CL_BO_CHAIN_WITH_BRANCH,
   V3D_CL_BO_CHAIN_WITH_RETURN_FROM_SUB_LIST,
};

static bool
cl_alloc_bo(struct v3dv_cl *cl, uint32_t space, enum
            v3dv_cl_chain_type chain_type)
{
   /* The last bytes of a CLE buffer are unusable because of readahead
    * prefetch, so we need to take it into account when allocating a new BO
    * for the CL. We also reserve space for the BRANCH/RETURN_FROM_SUB_LIST
    * packet so we can always emit these last packets to the BO when
    * needed. We will need to increase cl->size by the packet length before
    * calling cl_submit to use this reserved space.
    */
   uint32_t unusable_space = 0;
   uint32_t cle_readahead = V3DV_X(cl->job->device, CLE_READAHEAD);
   uint32_t cle_buffer_min_size = V3DV_X(cl->job->device, CLE_BUFFER_MIN_SIZE);
   switch (chain_type) {
   case V3D_CL_BO_CHAIN_WITH_BRANCH:
      unusable_space = cle_readahead + cl_packet_length(BRANCH);
      break;
   case V3D_CL_BO_CHAIN_WITH_RETURN_FROM_SUB_LIST:
      unusable_space = cle_readahead + cl_packet_length(RETURN_FROM_SUB_LIST);
      break;
   case V3D_CL_BO_CHAIN_NONE:
      break;
   }

   /* If we are growing, double the BO allocation size to reduce the number
    * of allocations with large command buffers. This has a very significant
    * impact on the number of draw calls per second reported by vkoverhead.
    */
   space = align(space + unusable_space, cle_buffer_min_size);
   if (cl->bo)
      space = MAX2(cl->bo->size * 2, space);

   struct v3dv_bo *bo = v3dv_bo_alloc(cl->job->device, space, "CL", true);
   if (!bo) {
      fprintf(stderr, "failed to allocate memory for command list\n");
      v3dv_flag_oom(NULL, cl->job);
      return false;
   }

   list_addtail(&bo->list_link, &cl->bo_list);

   bool ok = v3dv_bo_map(cl->job->device, bo, bo->size);
   if (!ok) {
      fprintf(stderr, "failed to map command list buffer\n");
      v3dv_flag_oom(NULL, cl->job);
      return false;
   }

   /* Chain to the new BO from the old one if requested */
   if (cl->bo) {
      switch (chain_type) {
      case V3D_CL_BO_CHAIN_WITH_BRANCH:
         cl->bo->cl_branch_offset = v3dv_cl_offset(cl);
         cl->size += cl_packet_length(BRANCH);
         assert(cl->size + cle_readahead <= cl->bo->size);
         cl_emit(cl, BRANCH, branch) {
            branch.address = v3dv_cl_address(bo, 0);
         }
         break;
      case V3D_CL_BO_CHAIN_WITH_RETURN_FROM_SUB_LIST:
         /* We do not want to emit branches from secondary command lists, instead,
          * we will branch to them when we execute them in a primary using
          * 'branch to sub list' commands, expecting each linked secondary to
          * end with a 'return from sub list' command.
          */
         cl->size += cl_packet_length(RETURN_FROM_SUB_LIST);
         assert(cl->size + cle_readahead <= cl->bo->size);
         cl_emit(cl, RETURN_FROM_SUB_LIST, ret);
         FALLTHROUGH;
      case V3D_CL_BO_CHAIN_NONE:
         v3dv_job_add_bo_unchecked(cl->job, bo);
         break;
      }
   } else {
      v3dv_job_add_bo_unchecked(cl->job, bo);
   }

   cl->bo = bo;
   cl->base = cl->bo->map;
   /* Take only into account the usable size of the BO to guarantee that
    * we never write in the last bytes of the CL buffer because of the
    * readahead of the CLE
    */
   cl->size = cl->bo->size - unusable_space;
   cl->next = cl->base;

   return true;
}

uint32_t
v3dv_cl_ensure_space(struct v3dv_cl *cl, uint32_t space, uint32_t alignment)
{
   uint32_t offset = align(v3dv_cl_offset(cl), alignment);

   if (offset + space <= cl->size) {
      cl->next = cl->base + offset;
      return offset;
   }

   cl_alloc_bo(cl, space, V3D_CL_BO_CHAIN_NONE);

   return 0;
}

void
v3dv_cl_ensure_space_with_branch(struct v3dv_cl *cl, uint32_t space)
{
   if (v3dv_cl_offset(cl) + space <= cl->size)
      return;

   enum v3dv_cl_chain_type  chain_type = V3D_CL_BO_CHAIN_WITH_BRANCH;
   if (cl->job->type == V3DV_JOB_TYPE_GPU_CL_INCOMPLETE)
      chain_type = V3D_CL_BO_CHAIN_WITH_RETURN_FROM_SUB_LIST;

   cl_alloc_bo(cl, space, chain_type);
}
