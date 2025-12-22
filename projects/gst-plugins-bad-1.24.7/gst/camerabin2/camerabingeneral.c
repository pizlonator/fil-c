/*
 * GStreamer
 * Copyright (C) 2008 Nokia Corporation <multimedia@maemo.org>
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

/**
 * SECTION:camerabingeneral
 * @title: GstCameraBin2
 * @short_description: helper functions for #GstCameraBin2 and it's modules
 *
 * Common helper functions for #GstCameraBin2, #GstCameraBin2Image and
 * #GstCameraBin2Video.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <glib.h>
#include <gst/basecamerabinsrc/gstbasecamerasrc.h>
#include <glib/gi18n-lib.h>

#include "camerabingeneral.h"

/**
 * gst_camerabin_add_element:
 * @bin: add an element to this bin
 * @new_elem: new element to be added
 *
 * Adds given element to given @bin. Looks for an unconnected src pad
 * from the @bin and links the element to it.  Raises an error if adding
 * or linking failed. Unrefs the element in the case of an error.
 *
 * Returns: %TRUE if adding and linking succeeded, %FALSE otherwise.
 */
gboolean
gst_camerabin_add_element (GstBin * bin, GstElement * new_elem)
{
  return gst_camerabin_add_element_full (bin, NULL, new_elem, NULL);
}

/**
 * gst_camerabin_add_element_full:
 * @bin: add an element to this bin
 * @srcpad:  src pad name, or NULL for any
 * @new_elem: new element to be added
 * @dstpad:  dst pad name, or NULL for any
 *
 * Adds given element to given @bin. Looks for an unconnected src pad
 * (with name @srcpad, if specified) from the @bin and links the element
 * to it.  Raises an error if adding or linking failed. Unrefs the element
 * in the case of an error.
 *
 * Returns: %TRUE if adding and linking succeeded, %FALSE otherwise.
 */
gboolean
gst_camerabin_add_element_full (GstBin * bin, const gchar * srcpad,
    GstElement * new_elem, const gchar * dstpad)
{
  gboolean ret;

  g_return_val_if_fail (bin, FALSE);
  g_return_val_if_fail (new_elem, FALSE);

  ret = gst_camerabin_try_add_element (bin, srcpad, new_elem, dstpad);

  if (!ret) {
    gchar *elem_name = gst_element_get_name (new_elem);
    GST_ELEMENT_ERROR (bin, CORE, NEGOTIATION, (NULL),
        ("linking %s failed", elem_name));
    g_free (elem_name);
    gst_object_unref (new_elem);
  }

  return ret;
}

/**
 * gst_camerabin_try_add_element:
 * @bin: tries adding an element to this bin
 * @srcpad:  src pad name, or NULL for any
 * @new_elem: new element to be added
 * @dstpad:  dst pad name, or NULL for any
 *
 * Adds given element to given @bin. Looks for an unconnected src pad
 * (with name @srcpad, if specified) from the @bin and links the element to
 * it.
 *
 * Returns: %TRUE if adding and linking succeeded, %FALSE otherwise.
 */
gboolean
gst_camerabin_try_add_element (GstBin * bin, const gchar * srcpad,
    GstElement * new_elem, const gchar * dstpad)
{
  GstPad *bin_pad;
  GstElement *bin_elem;
  gboolean ret = TRUE;

  g_return_val_if_fail (bin, FALSE);
  g_return_val_if_fail (new_elem, FALSE);

  /* Get pads for linking */
  bin_pad = gst_bin_find_unlinked_pad (bin, GST_PAD_SRC);
  /* Add to bin */
  gst_bin_add (GST_BIN (bin), new_elem);
  /* Link, if unconnected pad was found, otherwise just add it to bin */
  if (bin_pad) {
    GST_DEBUG_OBJECT (bin, "linking %s to %s:%s", GST_OBJECT_NAME (new_elem),
        GST_DEBUG_PAD_NAME (bin_pad));
    bin_elem = gst_pad_get_parent_element (bin_pad);
    gst_object_unref (bin_pad);
    if (!gst_element_link_pads_full (bin_elem, srcpad, new_elem, dstpad,
            GST_PAD_LINK_CHECK_CAPS)) {
      gst_object_ref (new_elem);
      gst_bin_remove (bin, new_elem);
      ret = FALSE;
    }
    gst_object_unref (bin_elem);
  } else {
    GST_INFO_OBJECT (bin, "no unlinked source pad in bin");
  }

  return ret;
}

/**
 * gst_camerabin_create_and_add_element:
 * @bin: tries adding an element to this bin
 * @elem_name: name of the element to be created
 * @instance_name: name of the instance of the element to be created
 *
 * Creates an element according to given name and
 * adds it to given @bin. Looks for an unconnected src pad
 * from the @bin and links the element to it.
 *
 * Returns: pointer to the new element if successful, NULL otherwise.
 */
GstElement *
gst_camerabin_create_and_add_element (GstBin * bin, const gchar * elem_name,
    const gchar * instance_name)
{
  GstElement *new_elem;

  g_return_val_if_fail (bin, FALSE);
  g_return_val_if_fail (elem_name, FALSE);

  new_elem = gst_element_factory_make (elem_name, instance_name);
  if (!new_elem) {
    GST_ELEMENT_ERROR (bin, CORE, MISSING_PLUGIN,
        (_("Missing element '%s' - check your GStreamer installation."),
            elem_name), (NULL));
  } else if (!gst_camerabin_add_element (bin, new_elem)) {
    new_elem = NULL;
  }

  return new_elem;
}

/* try to change the state of an element. This function returns the element
 * when the state change could be performed. When this function returns NULL
 * an error occurred and the element is unreffed. */
static GstElement *
try_element (GstElement * bin, GstElement * element)
{
  GstStateChangeReturn ret;

  if (element) {
    ret = gst_element_set_state (element, GST_STATE_READY);
    if (ret == GST_STATE_CHANGE_FAILURE) {
      GST_DEBUG_OBJECT (bin, "failed state change..");
      gst_element_set_state (element, GST_STATE_NULL);
      gst_object_unref (element);
      element = NULL;
    }
  }
  return element;
}

GstElement *
gst_camerabin_setup_default_element (GstBin * bin, GstElement * user_elem,
    const gchar * auto_elem_name, const gchar * default_elem_name,
    const gchar * instance_name)
{
  GstElement *elem;

  if (user_elem) {
    GST_DEBUG_OBJECT (bin, "trying configured element");
    elem = try_element (GST_ELEMENT_CAST (bin), gst_object_ref (user_elem));
  } else {
    /* only try fallback if no specific sink was chosen */
    GST_DEBUG_OBJECT (bin, "trying %s", auto_elem_name);
    elem = gst_element_factory_make (auto_elem_name, instance_name);
    elem = try_element (GST_ELEMENT_CAST (bin), elem);
    if (elem == NULL) {
      /* if default sink from config.h is different then try it too */
      if (strcmp (default_elem_name, auto_elem_name)) {
        GST_DEBUG_OBJECT (bin, "trying %s", default_elem_name);
        elem = gst_element_factory_make (default_elem_name, instance_name);
        elem = try_element (GST_ELEMENT_CAST (bin), elem);
      }
    }
  }
  return elem;
}

/**
 * gst_camerabin_remove_elements_from_bin:
 * @bin: removes all elements from this bin
 *
 * Removes all elements from this @bin.
 */
void
gst_camerabin_remove_elements_from_bin (GstBin * bin)
{
  GstIterator *iter = NULL;
  GValue value = { 0 };
  GstElement *elem = NULL;
  gboolean done = FALSE;

  iter = gst_bin_iterate_elements (bin);
  while (!done) {
    switch (gst_iterator_next (iter, &value)) {
      case GST_ITERATOR_OK:
        elem = (GstElement *) g_value_get_object (&value);
        gst_bin_remove (bin, elem);
        gst_element_set_state (GST_ELEMENT (elem), GST_STATE_NULL);
        /* Iterator increased the element refcount, so unref */
        g_value_unset (&value);
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (iter);
        break;
      case GST_ITERATOR_ERROR:
        GST_WARNING_OBJECT (bin, "error in iterating elements");
        done = TRUE;
        break;
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
    }
  }
  gst_iterator_free (iter);
}
