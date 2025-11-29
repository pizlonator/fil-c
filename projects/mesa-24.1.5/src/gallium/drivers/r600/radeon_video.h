/*
 * Copyright 2013 Advanced Micro Devices, Inc.
 * Authors:
 *      Christian König <christian.koenig@amd.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef RADEON_VIDEO_H
#define RADEON_VIDEO_H

#include "winsys/radeon_winsys.h"
#include "vl/vl_video_buffer.h"

#define RVID_ERR(fmt, args...) \
	fprintf(stderr, "EE %s:%d %s UVD - "fmt, __FILE__, __LINE__, __func__, ##args)

/* video buffer representation */
struct rvid_buffer
{
	unsigned		usage;
	struct r600_resource	*res;
};

/* generate an stream handle */
unsigned rvid_alloc_stream_handle(void);

/* create a buffer in the winsys */
bool rvid_create_buffer(struct pipe_screen *screen, struct rvid_buffer *buffer,
			unsigned size, unsigned usage);

/* destroy a buffer */
void rvid_destroy_buffer(struct rvid_buffer *buffer);

/* reallocate a buffer, preserving its content */
bool rvid_resize_buffer(struct pipe_screen *screen, struct radeon_cmdbuf *cs,
			struct rvid_buffer *new_buf, unsigned new_size);

/* clear the buffer with zeros */
void rvid_clear_buffer(struct pipe_context *context, struct rvid_buffer* buffer);

/* join surfaces into the same buffer with identical tiling params
   sum up their sizes and replace the backend buffers with a single bo */
void rvid_join_surfaces(struct r600_common_context *rctx,
                        struct pb_buffer_lean** buffers[VL_NUM_COMPONENTS],
                        struct radeon_surf *surfaces[VL_NUM_COMPONENTS]);

/* returns supported codecs and other parameters */
int rvid_get_video_param(struct pipe_screen *screen,
                         enum pipe_video_profile profile,
                         enum pipe_video_entrypoint entrypoint,
                         enum pipe_video_cap param);

/* the hardware only supports NV12 */
bool rvid_is_format_supported(struct pipe_screen *screen,
			      enum pipe_format format,
			      enum pipe_video_profile profile,
			      enum pipe_video_entrypoint entrypoint);

#endif // RADEON_VIDEO_H
