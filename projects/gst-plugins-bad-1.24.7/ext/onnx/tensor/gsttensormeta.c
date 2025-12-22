/*
 * GStreamer gstreamer-tensormeta
 * Copyright (C) 2023 Collabora Ltd
 *
 * gsttensormeta.c
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

#include "gsttensormeta.h"

static gboolean
gst_tensor_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstTensorMeta *tmeta = (GstTensorMeta *) meta;

  tmeta->num_tensors = 0;
  tmeta->tensor = NULL;

  return TRUE;
}

static void
gst_tensor_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstTensorMeta *tmeta = (GstTensorMeta *) meta;

  for (int i = 0; i < tmeta->num_tensors; i++) {
    g_free (tmeta->tensor[i].dims);
    gst_buffer_unref (tmeta->tensor[i].data);
  }
  g_free (tmeta->tensor);
}

GType
gst_tensor_meta_api_get_type (void)
{
  static GType type = 0;
  static const gchar *tags[] = { NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstTensorMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}


const GstMetaInfo *
gst_tensor_meta_get_info (void)
{
  static const GstMetaInfo *tmeta_info = NULL;

  if (g_once_init_enter (&tmeta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (gst_tensor_meta_api_get_type (),
        "GstTensorMeta",
        sizeof (GstTensorMeta),
        gst_tensor_meta_init,
        gst_tensor_meta_free,
        NULL);                  /* tensor_meta_transform not implemented */
    g_once_init_leave (&tmeta_info, meta);
  }
  return tmeta_info;
}

gint
gst_tensor_meta_get_index_from_id (GstTensorMeta * meta, GQuark id)
{
  for (int i = 0; i < meta->num_tensors; ++i) {
    if ((meta->tensor + i)->id == id)
      return i;
  }

  return GST_TENSOR_MISSING_ID;
}
