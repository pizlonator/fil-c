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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <string.h>

#include "mxfessence.h"

GST_DEBUG_CATEGORY_EXTERN (mxf_debug);
#define GST_CAT_DEFAULT mxf_debug

static GSList *_mxf_essence_element_handler_registry = NULL;

void
mxf_essence_element_handler_register (const MXFEssenceElementHandler * handler)
{
  _mxf_essence_element_handler_registry =
      g_slist_prepend (_mxf_essence_element_handler_registry,
      (gpointer) handler);
}

const MXFEssenceElementHandler *
mxf_essence_element_handler_find (const MXFMetadataTimelineTrack * track)
{
  GSList *l;
  guint i;
  MXFMetadataTrack *parent = (MXFMetadataTrack *) track;

  g_return_val_if_fail (track != NULL, NULL);

  if (parent->descriptor == NULL)
    return NULL;

  for (i = 0; i < parent->n_descriptor; i++) {
    MXFMetadataFileDescriptor *d = parent->descriptor[i];

    if (d) {
      for (l = _mxf_essence_element_handler_registry; l; l = l->next) {
        MXFEssenceElementHandler *current = l->data;

        if (current->handles_descriptor (d))
          return current;
      }
    }
  }

  return NULL;
}

static GList *_essence_element_writer_registry = NULL;
static GPtrArray *_essence_element_writer_pad_templates = NULL;

void
mxf_essence_element_writer_register (const MXFEssenceElementWriter * writer)
{
  _essence_element_writer_registry =
      g_list_prepend (_essence_element_writer_registry, (gpointer) writer);

  if (!_essence_element_writer_pad_templates)
    _essence_element_writer_pad_templates = g_ptr_array_new ();

  if (_essence_element_writer_pad_templates->len > 0 &&
      g_ptr_array_index (_essence_element_writer_pad_templates,
          _essence_element_writer_pad_templates->len - 1) == NULL)
    g_ptr_array_remove_index (_essence_element_writer_pad_templates,
        _essence_element_writer_pad_templates->len - 1);

  g_ptr_array_add (_essence_element_writer_pad_templates,
      (gpointer) writer->pad_template);
}

const GstPadTemplate **
mxf_essence_element_writer_get_pad_templates (void)
{
  if (!_essence_element_writer_pad_templates
      || _essence_element_writer_pad_templates->len == 0)
    return NULL;

  if (g_ptr_array_index (_essence_element_writer_pad_templates,
          _essence_element_writer_pad_templates->len - 1))
    g_ptr_array_add (_essence_element_writer_pad_templates, NULL);

  return (const GstPadTemplate **) _essence_element_writer_pad_templates->pdata;
}

const MXFEssenceElementWriter *
mxf_essence_element_writer_find (const GstPadTemplate * templ)
{
  GList *l = _essence_element_writer_registry;

  for (; l; l = l->next) {
    MXFEssenceElementWriter *writer = l->data;

    if (writer->pad_template == templ)
      return writer;
  }

  return NULL;
}
