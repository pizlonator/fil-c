/*
 * Copyright 2008 Corbin Simpson <MostAwesomeDude@gmail.com>
 * Copyright 2010 Marek Olšák <maraeo@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "draw/draw_context.h"
#include "draw/draw_private.h"

#include "util/u_upload_mgr.h"

#include "util/os_time.h"

#include "r300_context.h"
#include "r300_cs.h"
#include "r300_emit.h"


static void r300_flush_and_cleanup(struct r300_context *r300, unsigned flags,
                                   struct pipe_fence_handle **fence)
{
    struct r300_atom *atom;

    r300_emit_hyperz_end(r300);
    r300_emit_query_end(r300);
    if (r300->screen->caps.is_r500)
        r500_emit_index_bias(r300, 0);

    /* The DDX doesn't set these regs. */
    {
        CS_LOCALS(r300);
        OUT_CS_REG_SEQ(R300_GB_MSPOS0, 2);
        OUT_CS(0x66666666);
        OUT_CS(0x6666666);
    }

    r300->flush_counter++;
    r300->rws->cs_flush(&r300->cs, flags, fence);
    r300->dirty_hw = 0;

    /* New kitchen sink, baby. */
    foreach_atom(r300, atom) {
        if (atom->state || atom->allow_null_state) {
            r300_mark_atom_dirty(r300, atom);
        }
    }
    r300->vertex_arrays_dirty = true;

    /* Unmark HWTCL state for SWTCL. */
    if (!r300->screen->caps.has_tcl) {
        r300->vs_state.dirty = false;
        r300->vs_constants.dirty = false;
        r300->clip_state.dirty = false;
    }
}

void r300_flush(struct pipe_context *pipe,
                unsigned flags,
                struct pipe_fence_handle **fence)
{
    struct r300_context *r300 = r300_context(pipe);

    if (r300->dirty_hw) {
        r300_flush_and_cleanup(r300, flags, fence);
    } else {
        if (fence) {
            /* We have to create a fence object, but the command stream is empty
             * and we cannot emit an empty CS. Let's write to some reg. */
            CS_LOCALS(r300);
            OUT_CS_REG(RB3D_COLOR_CHANNEL_MASK, 0);
            r300->rws->cs_flush(&r300->cs, flags, fence);
        } else {
            /* Even if hw is not dirty, we should at least reset the CS in case
             * the space checking failed for the first draw operation. */
            r300->rws->cs_flush(&r300->cs, flags, NULL);
        }
    }

    /* Update Hyper-Z status. */
    if (r300->hyperz_enabled) {
        /* If there was a Z clear, keep Hyper-Z access. */
        if (r300->num_z_clears) {
            r300->hyperz_time_of_last_flush = os_time_get();
            r300->num_z_clears = 0;
        } else if (r300->hyperz_time_of_last_flush - os_time_get() > 2000000) {
            /* If there hasn't been a Z clear for 2 seconds, revoke Hyper-Z access. */
            r300->hiz_in_use = false;

            /* Decompress the Z buffer. */
            if (r300->zmask_in_use) {
                if (r300->locked_zbuffer) {
                    r300_decompress_zmask_locked(r300);
                } else {
                    r300_decompress_zmask(r300);
                }

                if (fence && *fence)
                    r300->rws->fence_reference(r300->rws, fence, NULL);
                r300_flush_and_cleanup(r300, flags, fence);
            }

            /* Revoke Hyper-Z access, so that some other process can take it. */
            r300->rws->cs_request_feature(&r300->cs, RADEON_FID_R300_HYPERZ_ACCESS,
                                          false);
            r300->hyperz_enabled = false;
        }
    }
}

static void r300_flush_wrapped(struct pipe_context *pipe,
                               struct pipe_fence_handle **fence,
                               unsigned flags)
{
    if (flags & PIPE_FLUSH_HINT_FINISH)
        flags &= ~PIPE_FLUSH_ASYNC;

    r300_flush(pipe, flags, fence);
}

void r300_init_flush_functions(struct r300_context* r300)
{
    r300->context.flush = r300_flush_wrapped;
}
