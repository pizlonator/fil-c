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

#include "gstmsdkdecproputil.h"

void
gst_msdkdec_prop_install_output_oder_property (GObjectClass * gobject_class)
{
  g_object_class_install_property (gobject_class, GST_MSDKDEC_PROP_OUTPUT_ORDER,
      g_param_spec_enum ("output-order", "DecodedFramesOutputOrder",
          "Decoded frames output order",
          gst_msdkdec_output_order_get_type (),
          PROP_OUTPUT_ORDER_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

void
gst_msdkdec_prop_install_error_report_property (GObjectClass * gobject_class)
{
  g_object_class_install_property (gobject_class, GST_MSDKDEC_PROP_ERROR_REPORT,
      g_param_spec_boolean ("report-error", "report-error",
          "Report bitstream error information",
          PROP_ERROR_REPORT_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

gboolean
gst_msdkdec_prop_check_state (GstState state, GParamSpec * pspec)
{
  if ((state != GST_STATE_READY && state != GST_STATE_NULL) &&
      !(pspec->flags & GST_PARAM_MUTABLE_PLAYING)) {
    return FALSE;
  }
  return TRUE;
}
