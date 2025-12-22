/* GStreamer
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifndef __MXF_ESSENCE_H__
#define __MXF_ESSENCE_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include "mxftypes.h"
#include "mxfmetadata.h"

typedef enum {
  MXF_ESSENCE_WRAPPING_FRAME_WRAPPING,
  MXF_ESSENCE_WRAPPING_CLIP_WRAPPING,
  MXF_ESSENCE_WRAPPING_CUSTOM_WRAPPING,
  MXF_ESSENCE_WRAPPING_UNKNOWN_WRAPPING
} MXFEssenceWrapping;

typedef GstFlowReturn (*MXFEssenceElementHandleFunc) (const MXFUL *key, GstBuffer *buffer, GstCaps *caps, MXFMetadataTimelineTrack *track, gpointer mapping_data, GstBuffer **outbuf);

typedef struct {
  gboolean (*handles_descriptor) (const MXFMetadataFileDescriptor *d);
  MXFEssenceWrapping (*get_track_wrapping) (const MXFMetadataTimelineTrack *track);
  GstCaps * (*create_caps) (MXFMetadataTimelineTrack *track, GstTagList **tags, gboolean * intra_only, MXFEssenceElementHandleFunc *handler, gpointer *mapping_data);
} MXFEssenceElementHandler;

typedef GstFlowReturn (*MXFEssenceElementWriteFunc) (GstBuffer *buffer, gpointer mapping_data, GstAdapter *adapter, GstBuffer **outbuf, gboolean flush);

typedef struct {
   MXFMetadataFileDescriptor * (*get_descriptor) (GstPadTemplate *tmpl, GstCaps *caps, MXFEssenceElementWriteFunc *handler, gpointer *mapping_data);
   void (*update_descriptor) (MXFMetadataFileDescriptor *d, GstCaps *caps, gpointer mapping_data, GstBuffer *buf);
   void (*get_edit_rate) (MXFMetadataFileDescriptor *a, GstCaps *caps, gpointer mapping_data, GstBuffer *buf, MXFMetadataSourcePackage *package, MXFMetadataTimelineTrack *track, MXFFraction *edit_rate);
   guint32 (*get_track_number_template) (MXFMetadataFileDescriptor *a, GstCaps *caps, gpointer mapping_data);
   const GstPadTemplate *pad_template;
   MXFUL data_definition;
} MXFEssenceElementWriter;

void mxf_essence_element_handler_register (const MXFEssenceElementHandler *handler);
const MXFEssenceElementHandler * mxf_essence_element_handler_find (const MXFMetadataTimelineTrack *track);

void mxf_essence_element_writer_register (const MXFEssenceElementWriter *writer);
const GstPadTemplate ** mxf_essence_element_writer_get_pad_templates (void);
const MXFEssenceElementWriter *mxf_essence_element_writer_find (const GstPadTemplate *templ);

#endif /* __MXF_ESSENCE_H__ */
