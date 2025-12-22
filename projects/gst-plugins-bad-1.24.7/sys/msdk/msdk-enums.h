/* GStreamer Intel MSDK plugin
 * Copyright (c) 2018, Intel Corporation
 *
 * Author: Sreerenj Balachandran <sreerenj.balachandran@intel.com>
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

#ifndef __MSDKENUMS_H__
#define __MSDKENUMS_H__

#include "msdk.h"

G_BEGIN_DECLS

#define _MFX_TRELLIS_NONE    0

/*========= MSDK Decoder Enums =========================*/
typedef enum
{
  GST_MSDKDEC_OUTPUT_ORDER_DISPLAY = 0,
  GST_MSDKDEC_OUTPUT_ORDER_DECODE,
} GstMskdDecOutputOrder;

GType
gst_msdkdec_output_order_get_type (void);

/*========= MSDK Encoder Enums =========================*/
GType
gst_msdkenc_rate_control_get_type (void);

GType
gst_msdkenc_trellis_quantization_get_type (void);

GType
gst_msdkenc_rc_lookahead_ds_get_type (void);

GType
gst_msdkenc_mbbrc_get_type (void);

GType
gst_msdkenc_lowdelay_brc_get_type (void);

GType
gst_msdkenc_adaptive_i_get_type (void);

GType
gst_msdkenc_adaptive_b_get_type (void);

GType
gst_msdkenc_tune_mode_get_type (void);

/*========= MSDK VPP Enums =========================*/

GType
gst_msdkvpp_rotation_get_type (void);

typedef enum
{
  GST_MSDKVPP_DEINTERLACE_MODE_AUTO = 0,
  GST_MSDKVPP_DEINTERLACE_MODE_INTERLACED,
  GST_MSDKVPP_DEINTERLACE_MODE_DISABLED,
} GstMskdVPPDeinterlaceMode;

GType
gst_msdkvpp_deinterlace_mode_get_type (void);

#define _MFX_DEINTERLACE_METHOD_NONE 0
GType
gst_msdkvpp_deinterlace_method_get_type (void);

GType
gst_msdkvpp_mirroring_get_type (void);

GType
gst_msdkvpp_scaling_mode_get_type (void);

#define _MFX_FRC_ALGORITHM_NONE 0
GType
gst_msdkvpp_frc_algorithm_get_type (void);

GType
gst_msdkenc_transform_skip_get_type (void);

GType
gst_msdkenc_intra_refresh_type_get_type (void);

G_END_DECLS
#endif
