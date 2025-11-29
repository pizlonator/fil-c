/*
 * Copyright 2010 Marek Olšák <maraeo@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "r300_context.h"
#include "util/u_index_modify.h"
#include "util/u_upload_mgr.h"


void r300_translate_index_buffer(struct r300_context *r300,
                                 const struct pipe_draw_info *info,
                                 struct pipe_resource **out_buffer,
                                 unsigned *index_size, unsigned index_offset,
                                 unsigned *start, unsigned count,
                                 const uint8_t **export_ptr)
{
    unsigned out_offset;
    void **ptr = (void **)export_ptr;

    switch (*index_size) {
    case 1:
        *out_buffer = NULL;
        u_upload_alloc(r300->uploader, 0, count * 2, 4,
                       &out_offset, out_buffer, ptr);

        util_shorten_ubyte_elts_to_userptr(
                &r300->context, info, PIPE_MAP_UNSYNCHRONIZED, index_offset,
                *start, count, *ptr);

        *index_size = 2;
        *start = out_offset / 2;
        break;

    case 2:
        if (index_offset) {
            *out_buffer = NULL;
            u_upload_alloc(r300->uploader, 0, count * 2, 4,
                           &out_offset, out_buffer, ptr);

            util_rebuild_ushort_elts_to_userptr(&r300->context, info,
                                                PIPE_MAP_UNSYNCHRONIZED,
                                                index_offset, *start,
                                                count, *ptr);

            *start = out_offset / 2;
        }
        break;

    case 4:
        if (index_offset) {
            *out_buffer = NULL;
            u_upload_alloc(r300->uploader, 0, count * 4, 4,
                           &out_offset, out_buffer, ptr);

            util_rebuild_uint_elts_to_userptr(&r300->context, info,
                                              PIPE_MAP_UNSYNCHRONIZED,
                                              index_offset, *start,
                                              count, *ptr);

            *start = out_offset / 4;
        }
        break;
    }
}
