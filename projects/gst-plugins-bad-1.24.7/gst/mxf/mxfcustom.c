/* GStreamer
 * Copyright (C) 2022 Edward Hervey <edward@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/* Custom mappings
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/video/video.h>
#include <string.h>

#include "mxfcustom.h"
#include "mxfessence.h"

GST_DEBUG_CATEGORY_EXTERN (mxf_debug);
#define GST_CAT_DEFAULT mxf_debug

/* Custom Canon XF-HEVC essence */
static const MXFUL mxf_canon_xf_hevc = { {0x06, 0x0E, 0x2B, 0x34,
        0x04, 0x01, 0x01, 0x0c,
        0x0e, 0x15, 0x00, 0x04,
    0x02, 0x10, 0x00, 0x01}
};

static gboolean
mxf_is_canon_xfhevc_essence_track (const MXFMetadataFileDescriptor * d)
{
  return mxf_ul_is_equal (&d->essence_container, &mxf_canon_xf_hevc);
}

static GstFlowReturn
mxf_canon_xfhevc_handle_essence_element (const MXFUL * key, GstBuffer * buffer,
    GstCaps * caps,
    MXFMetadataTimelineTrack * track,
    gpointer mapping_data, GstBuffer ** outbuf)
{
  *outbuf = buffer;
  /* Blindly accept it */
  return GST_FLOW_OK;
}

static MXFEssenceWrapping
mxf_canon_xfhevc_get_track_wrapping (const MXFMetadataTimelineTrack * track)
{
  /* Assume it's always frame wrapping */
  return MXF_ESSENCE_WRAPPING_FRAME_WRAPPING;
}

static GstCaps *
mxf_canon_xfhevc_create_caps (MXFMetadataTimelineTrack * track,
    GstTagList ** tags, gboolean * intra_only,
    MXFEssenceElementHandleFunc * handler, gpointer * mapping_data)
{
  GstCaps *caps = NULL;

  g_return_val_if_fail (track != NULL, NULL);

  *handler = mxf_canon_xfhevc_handle_essence_element;
  *intra_only = TRUE;
  caps = gst_caps_from_string ("video/x-h265");

  return caps;
}

static const MXFEssenceElementHandler mxf_canon_xfhevc_essence_element_handler = {
  mxf_is_canon_xfhevc_essence_track,
  mxf_canon_xfhevc_get_track_wrapping,
  mxf_canon_xfhevc_create_caps
};

void
mxf_custom_init (void)
{
  mxf_essence_element_handler_register
      (&mxf_canon_xfhevc_essence_element_handler);
}
