/* GStreamer Intel MSDK plugin
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __GST_MSDKDEC_H__
#define __GST_MSDKDEC_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include "msdk.h"
#include "gstmsdkcontext.h"
#include "msdk-enums.h"
#include "gstmsdkdecproputil.h"
#include "gstmsdkcaps.h"

G_BEGIN_DECLS

#define GST_TYPE_MSDKDEC \
  (gst_msdkdec_get_type())
#define GST_MSDKDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MSDKDEC,GstMsdkDec))
#define GST_MSDKDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MSDKDEC,GstMsdkDecClass))
#define GST_MSDKDEC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_MSDKDEC,GstMsdkDecClass))
#define GST_IS_MSDKDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MSDKDEC))
#define GST_IS_MSDKDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MSDKDEC))

#define MAX_BS_EXTRA_PARAMS             8
#define MAX_VIDEO_EXTRA_PARAMS          8

typedef struct _GstMsdkDec GstMsdkDec;
typedef struct _GstMsdkDecClass GstMsdkDecClass;
typedef struct _MsdkDecTask MsdkDecTask;
typedef struct _MsdkDecCData MsdkDecCData;

struct _GstMsdkDec
{
  GstVideoDecoder element;

  /* input/output description */
  GstVideoCodecState *input_state;
  GstVideoCodecState *output_state;
  /* aligned msdk pool info */
  GstBufferPool *pool;
  GstBufferPool *alloc_pool;
  GstBufferPool *other_pool;
  /* downstream pool info based on allocation query */
  GstVideoInfo non_msdk_pool_info;
  mfxFrameAllocResponse alloc_resp;
  gboolean use_dmabuf;
  gboolean do_copy;
  gboolean initialized;
  gboolean sfc;
  gboolean ds_has_known_allocator;
  guint64 modifier;

  /* for packetization */
  GstAdapter *adapter;
  /* cap negotiation needed, allocation may or may not be required*/
  gboolean do_renego;
  /* re-allocation is mandatory if enabled */
  gboolean do_realloc;
  /* force reset on resolution change */
  gboolean force_reset_on_res_change;
  /* minimum number of buffers to be allocated, this should
   * include downstream requirement, msdk suggestion and extra
   * surface allocation for smooth display in render pipeline */
  guint min_prealloc_buffers;

  /* MFX context */
  GstMsdkContext *context;
  GstMsdkContext *old_context;
  mfxVideoParam param;
  GArray *tasks;
  guint next_task;

  GList *locked_msdk_surfaces;

  /* element properties */
  gboolean hardware;
  gboolean report_error;
  guint async_depth;

  mfxExtBuffer *bs_extra_params[MAX_BS_EXTRA_PARAMS];
  guint num_bs_extra_params;

  mfxExtBuffer *video_extra_params[MAX_VIDEO_EXTRA_PARAMS];
  guint num_video_extra_params;

#if (MFX_VERSION >= 1025)
  mfxExtDecodeErrorReport error_report;
#endif
};

struct _GstMsdkDecClass
{
  GstVideoDecoderClass parent_class;

  gboolean (*configure) (GstMsdkDec * decoder);

  /* adjust mfx parameters per codec after decode header */
  gboolean (*post_configure) (GstMsdkDec * decoder);

  /* reset mfx parameters per codec */
  gboolean (*preinit_decoder) (GstMsdkDec * decoder);
  /* adjust mfx parameters per codec */
  gboolean (*postinit_decoder) (GstMsdkDec * decoder);
};

struct _MsdkDecCData
{
  GstCaps *sink_caps;
  GstCaps *src_caps;
};

GType gst_msdkdec_get_type (void);

void
gst_msdkdec_add_bs_extra_param (GstMsdkDec * thiz, mfxExtBuffer * param);

void
gst_msdkdec_add_video_extra_param (GstMsdkDec * thiz, mfxExtBuffer * param);

G_END_DECLS

#endif /* __GST_MSDKDEC_H__ */
