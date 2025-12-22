/* GStreamer
 * Copyright (C) 2011 Andoni Morales Alastruey <ylatuya@gmail.com>
 *
 * gstfragment.c:
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

#include <glib.h>
#include <gst/base/gsttypefindhelper.h>
#include <gst/base/gstadapter.h>
#include "gstfragment.h"
#include "gsturidownloader_debug.h"

#define GST_CAT_DEFAULT uridownloader_debug

enum
{
  PROP_0,
  PROP_INDEX,
  PROP_NAME,
  PROP_DURATION,
  PROP_DISCONTINOUS,
  PROP_BUFFER,
  PROP_CAPS,
  PROP_LAST
};

struct _GstFragmentPrivate
{
  GstBuffer *buffer;
  GstCaps *caps;
  GMutex lock;
};

G_DEFINE_TYPE_WITH_PRIVATE (GstFragment, gst_fragment, G_TYPE_OBJECT);

static void gst_fragment_dispose (GObject * object);
static void gst_fragment_finalize (GObject * object);

static void
gst_fragment_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec)
{
  GstFragment *fragment = GST_FRAGMENT (object);

  switch (property_id) {
    case PROP_CAPS:
      gst_fragment_set_caps (fragment, g_value_get_boxed (value));
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_fragment_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec)
{
  GstFragment *fragment = GST_FRAGMENT (object);

  switch (property_id) {
    case PROP_INDEX:
      g_value_set_uint (value, fragment->index);
      break;

    case PROP_NAME:
      g_value_set_string (value, fragment->name);
      break;

    case PROP_DURATION:
      g_value_set_uint64 (value, fragment->stop_time - fragment->start_time);
      break;

    case PROP_DISCONTINOUS:
      g_value_set_boolean (value, fragment->discontinuous);
      break;

    case PROP_BUFFER:
      g_value_take_boxed (value, gst_fragment_get_buffer (fragment));
      break;

    case PROP_CAPS:
      g_value_take_boxed (value, gst_fragment_get_caps (fragment));
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}



static void
gst_fragment_class_init (GstFragmentClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_fragment_set_property;
  gobject_class->get_property = gst_fragment_get_property;
  gobject_class->dispose = gst_fragment_dispose;
  gobject_class->finalize = gst_fragment_finalize;

  g_object_class_install_property (gobject_class, PROP_INDEX,
      g_param_spec_uint ("index", "Index", "Index of the fragment", 0,
          G_MAXUINT, 0, G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, PROP_NAME,
      g_param_spec_string ("name", "Name",
          "Name of the fragment (eg:fragment-12.ts)", NULL, G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, PROP_DISCONTINOUS,
      g_param_spec_boolean ("discontinuous", "Discontinuous",
          "Whether this fragment has a discontinuity or not",
          FALSE, G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, PROP_DURATION,
      g_param_spec_uint64 ("duration", "Fragment duration",
          "Duration of the fragment", 0, G_MAXUINT64, 0, G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, PROP_BUFFER,
      g_param_spec_boxed ("buffer", "Buffer",
          "The fragment's buffer", GST_TYPE_BUFFER,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CAPS,
      g_param_spec_boxed ("caps", "Fragment caps",
          "The caps of the fragment's buffer. (NULL = detect)", GST_TYPE_CAPS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_fragment_init (GstFragment * fragment)
{
  GstFragmentPrivate *priv;

  fragment->priv = priv = gst_fragment_get_instance_private (fragment);

  g_mutex_init (&fragment->priv->lock);
  priv->buffer = NULL;
  fragment->download_start_time = gst_util_get_timestamp ();
  fragment->start_time = 0;
  fragment->stop_time = 0;
  fragment->index = 0;
  fragment->name = g_strdup ("");
  fragment->completed = FALSE;
  fragment->discontinuous = FALSE;
  fragment->headers = NULL;
}

GstFragment *
gst_fragment_new (void)
{
  return GST_FRAGMENT (g_object_new (GST_TYPE_FRAGMENT, NULL));
}

static void
gst_fragment_finalize (GObject * gobject)
{
  GstFragment *fragment = GST_FRAGMENT (gobject);

  g_free (fragment->uri);
  g_free (fragment->redirect_uri);
  g_free (fragment->name);
  if (fragment->headers)
    gst_structure_free (fragment->headers);
  g_mutex_clear (&fragment->priv->lock);

  G_OBJECT_CLASS (gst_fragment_parent_class)->finalize (gobject);
}

void
gst_fragment_dispose (GObject * object)
{
  GstFragmentPrivate *priv = GST_FRAGMENT (object)->priv;

  if (priv->buffer != NULL) {
    gst_buffer_unref (priv->buffer);
    priv->buffer = NULL;
  }

  if (priv->caps != NULL) {
    gst_caps_unref (priv->caps);
    priv->caps = NULL;
  }

  G_OBJECT_CLASS (gst_fragment_parent_class)->dispose (object);
}

GstBuffer *
gst_fragment_get_buffer (GstFragment * fragment)
{
  g_return_val_if_fail (fragment != NULL, NULL);

  if (!fragment->completed)
    return NULL;
  if (!fragment->priv->buffer)
    return NULL;

  gst_buffer_ref (fragment->priv->buffer);
  return fragment->priv->buffer;
}

void
gst_fragment_set_caps (GstFragment * fragment, GstCaps * caps)
{
  g_return_if_fail (fragment != NULL);

  g_mutex_lock (&fragment->priv->lock);
  gst_caps_replace (&fragment->priv->caps, caps);
  g_mutex_unlock (&fragment->priv->lock);
}

GstCaps *
gst_fragment_get_caps (GstFragment * fragment)
{
  g_return_val_if_fail (fragment != NULL, NULL);

  if (!fragment->completed)
    return NULL;

  g_mutex_lock (&fragment->priv->lock);
  if (fragment->priv->caps == NULL) {
    guint64 offset, offset_end;

    /* FIXME: This is currently necessary as typefinding only
     * works with 0 offsets... need to find a better way to
     * do that */
    offset = GST_BUFFER_OFFSET (fragment->priv->buffer);
    offset_end = GST_BUFFER_OFFSET_END (fragment->priv->buffer);
    GST_BUFFER_OFFSET (fragment->priv->buffer) = GST_BUFFER_OFFSET_NONE;
    GST_BUFFER_OFFSET_END (fragment->priv->buffer) = GST_BUFFER_OFFSET_NONE;
    fragment->priv->caps =
        gst_type_find_helper_for_buffer (NULL, fragment->priv->buffer, NULL);
    GST_BUFFER_OFFSET (fragment->priv->buffer) = offset;
    GST_BUFFER_OFFSET_END (fragment->priv->buffer) = offset_end;
  }
  gst_caps_ref (fragment->priv->caps);
  g_mutex_unlock (&fragment->priv->lock);

  return fragment->priv->caps;
}

gboolean
gst_fragment_add_buffer (GstFragment * fragment, GstBuffer * buffer)
{
  g_return_val_if_fail (fragment != NULL, FALSE);
  g_return_val_if_fail (buffer != NULL, FALSE);

  if (fragment->completed) {
    GST_WARNING ("Fragment is completed, could not add more buffers");
    return FALSE;
  }

  GST_DEBUG ("Adding new buffer to the fragment");
  /* We steal the buffers you pass in */
  if (fragment->priv->buffer == NULL)
    fragment->priv->buffer = buffer;
  else
    fragment->priv->buffer = gst_buffer_append (fragment->priv->buffer, buffer);
  return TRUE;
}
