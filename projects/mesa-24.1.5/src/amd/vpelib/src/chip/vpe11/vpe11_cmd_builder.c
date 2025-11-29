/* Copyright 2023 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: AMD
 *
 */
#include "vpe_assert.h"
#include "common.h"
#include "vpe_priv.h"
#include "vpe_command.h"
#include "vpe10_cmd_builder.h"
#include "vpe11_cmd_builder.h"
#include "plane_desc_writer.h"
#include "reg_helper.h"

void vpe11_construct_cmd_builder(struct vpe_priv *vpe_priv, struct cmd_builder *builder)
{
    builder->build_noops            = vpe10_build_noops;
    builder->build_vpe_cmd          = vpe10_build_vpe_cmd;
    builder->build_plane_descriptor = vpe10_build_plane_descriptor;
    // build collaborate sync cmd
    builder->build_collaborate_sync_cmd = vpe11_build_collaborate_sync_cmd;
}

enum vpe_status vpe11_build_collaborate_sync_cmd(
    struct vpe_priv *vpe_priv, struct vpe_build_bufs *cur_bufs, bool is_end)
{
    struct vpe_buf *buf = &cur_bufs->cmd_buf;
    uint32_t       *cmd_space;
    uint64_t        size                   = 2 * sizeof(uint32_t);
    uint32_t        collarborate_sync_data = vpe_priv->collaborate_sync_index;
    enum vpe_status status                 = VPE_STATUS_OK;

    if (buf->size < size)
        status = VPE_STATUS_BUFFER_OVERFLOW;

    if (status == VPE_STATUS_OK) {
        cmd_space = (uint32_t*)(uintptr_t)buf->cpu_va;
        *cmd_space++ = VPE_COLLABORATE_SYNC_CMD_HEADER;
        *cmd_space++ = VPE_COLLABORATE_SYNC_DATA_MASK(collarborate_sync_data);

        if (is_end == true) {
            vpe_priv->collaborate_sync_index++;
        }
        buf->cpu_va += size;
        buf->gpu_va += size;
        buf->size -= size;
    }

    return status;
}
