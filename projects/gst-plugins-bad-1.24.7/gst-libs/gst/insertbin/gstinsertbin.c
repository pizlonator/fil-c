/*
 * GStreamer
 *
 *  Copyright 2013 Collabora Ltd
 *   @author: Olivier Crete <olivier.crete@collabora.com>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

/**
 * SECTION:gstinsertbin
 * @short_description: A #GstBin to insertally link filter-like elements.
 *
 * This element is a #GstBin that has a single source and sink pad. It allows
 * the user (the application) to easily add and remove filter-like element
 * (that has a single source and sink pad), to the pipeline while it is running.
 * It features a fully asynchronous API inspired by GLib's GAsyncResult based
 * APIs.
 *
 * Each operation (addition or removal) can take a callback, this callback
 * is guaranteed to be called. Unlike GIO, there is no guarantee about where
 * this callback will be called from, it could be called before the action
 * returns or it could be called later from another thread. The signature of
 * this callback GstInsertBinCallback().
 *
 * Apart from the library API, since 1.24 insertbin can also be found in the
 * registry:
 *
 * ``` C
 *   GstElement *pipeline, *insertbin, *videoflip;
 *
 *   gst_init (NULL, NULL);
 *   pipeline =
 *       gst_parse_launch ("videotestsrc ! insertbin name=i ! autovideosink",
 *       NULL);
 *
 *   ...
 *
 *   insertbin = gst_bin_get_by_name (GST_BIN (pipeline), "i");
 *   videoflip = gst_element_factory_make ("videoflip", NULL);
 *
 *   ...
 *
 *   g_object_set (videoflip, "method", 1, NULL);
 *   g_signal_emit_by_name (insertbin, "append", videoflip, NULL, NULL);
 *
 *   ...
 * ```
 *
 * Since: 1.2
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstinsertbin.h"


GST_DEBUG_CATEGORY_STATIC (insert_bin_debug);
#define GST_CAT_DEFAULT (insert_bin_debug)


static GstStaticPadTemplate gst_insert_bin_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate gst_insert_bin_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

enum
{
  SIG_PREPEND,
  SIG_APPEND,
  SIG_INSERT_BEFORE,
  SIG_INSERT_AFTER,
  SIG_REMOVE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

enum
{
  PROP_0,
};

struct _GstInsertBinPrivate
{
  GstPad *srcpad;
  GstPad *sinkpad;

  GQueue change_queue;
};

typedef enum
{
  GST_INSERT_BIN_ACTION_ADD,
  GST_INSERT_BIN_ACTION_REMOVE
} GstInsertBinAction;


typedef enum
{
  DIRECTION_NONE,
  DIRECTION_AFTER,
  DIRECTION_BEFORE
} GstInsertBinDirection;

struct ChangeData
{
  GstElement *element;
  GstInsertBinAction action;
  GstElement *sibling;
  GstInsertBinDirection direction;

  GstInsertBinCallback callback;
  gpointer user_data;
};

static void gst_insert_bin_dispose (GObject * object);


static void gst_insert_bin_do_change (GstInsertBin * self, GstPad * pad);
static GstPadProbeReturn pad_blocked_cb (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data);

G_DEFINE_TYPE_WITH_PRIVATE (GstInsertBin, gst_insert_bin, GST_TYPE_BIN);

static void
gst_insert_bin_class_init (GstInsertBinClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (insert_bin_debug, "insertbin", 0, "Insert Bin");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_insert_bin_src_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_insert_bin_sink_template);
  gst_element_class_set_static_metadata (gstelement_class, "Insert Bin",
      "Generic/Bin/Filter", "Auto-links filter style elements insertally",
      "Olivier Crete <olivier.crete@collabora.com>");

  gobject_class->dispose = gst_insert_bin_dispose;

  /**
   * GstInsertBin::prepend:
   * @element: the #GstElement to add
   * @callback: the callback to call when the element has been added or not, or
   *  %NULL
   * @user_data: The data to pass to the callback
   * @user_data2: The user data of the signal (ignored)
   *
   * This action signal adds the filter like element before any other element
   * in the bin.
   *
   * Same as gst_insert_bin_prepend()
   *
   * Since: 1.2
   */
  signals[SIG_PREPEND] = g_signal_new_class_handler ("prepend",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_insert_bin_prepend),
      NULL, NULL, NULL,
      G_TYPE_NONE, 3, GST_TYPE_ELEMENT, G_TYPE_POINTER, G_TYPE_POINTER);

  /**
   * GstInsertBin::append:
   * @element: the #GstElement to add
   * @callback: the callback to call when the element has been added or not, or
   *  %NULL
   * @user_data: The data to pass to the callback
   * @user_data2: The user data of the signal (ignored)
   *
   * This action signal adds the filter like element after any other element
   * in the bin.
   *
   * Same as gst_insert_bin_append()
   *
   * Since: 1.2
   */
  signals[SIG_APPEND] = g_signal_new_class_handler ("append",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_insert_bin_append),
      NULL, NULL, NULL,
      G_TYPE_NONE, 3, GST_TYPE_ELEMENT, G_TYPE_POINTER, G_TYPE_POINTER);

  /**
   * GstInsertBin::insert-before:
   * @element: the #GstElement to add
   * @sibling: the #GstElement to add @element before
   * @callback: the callback to call when the element has been added or not, or
   *  %NULL
   * @user_data: The data to pass to the callback
   * @user_data2: The user data of the signal (ignored)
   *
   * This action signal adds the filter like element before the @sibling
   * element in the bin.
   *
   * Same as gst_insert_bin_insert_before()
   *
   * Since: 1.2
   */
  signals[SIG_INSERT_BEFORE] = g_signal_new_class_handler ("insert-before",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_insert_bin_insert_before),
      NULL, NULL, NULL,
      G_TYPE_NONE, 4, GST_TYPE_ELEMENT, GST_TYPE_ELEMENT,
      G_TYPE_POINTER, G_TYPE_POINTER);

  /**
   * GstInsertBin::insert-after:
   * @element: the #GstElement to add
   * @sibling: the #GstElement to add @element after
   * @callback: the callback to call when the element has been added or not, or
   *  %NULL
   * @user_data: The data to pass to the callback
   * @user_data2: The user data of the signal (ignored)
   *
   * This action signal adds the filter like element after the @sibling
   * element in the bin.
   * element in the bin.
   *
   * Same as gst_insert_bin_insert_after()
   *
   * Since: 1.2
   */
  signals[SIG_INSERT_AFTER] = g_signal_new_class_handler ("insert-after",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_insert_bin_insert_after),
      NULL, NULL, NULL,
      G_TYPE_NONE, 4, GST_TYPE_ELEMENT, GST_TYPE_ELEMENT,
      G_TYPE_POINTER, G_TYPE_POINTER);

  /**
   * GstInsertBin::remove:
   * @element: the #GstElement to remove
   * @callback: the callback to call when the element has been removed or not,
   * or %NULL
   * @user_data: The data to pass to the callback
   * @user_data2: The user data of the signal (ignored)
   *
   * This action signal removed the filter like element from the bin.
   *
   * Same as gst_insert_bin_remove()
   *
   * Since: 1.2
   */
  signals[SIG_REMOVE] = g_signal_new_class_handler ("remove",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_CALLBACK (gst_insert_bin_remove),
      NULL, NULL, NULL,
      G_TYPE_NONE, 3, GST_TYPE_ELEMENT, G_TYPE_POINTER, G_TYPE_POINTER);
}

static void
change_data_free (struct ChangeData *data)
{
  gst_object_unref (data->element);
  if (data->sibling)
    gst_object_unref (data->sibling);
  g_free (data);
}


static void
gst_insert_bin_change_data_complete (GstInsertBin * self,
    struct ChangeData *data, gboolean success)
{
  if (data->callback)
    data->callback (self, data->element, success, data->user_data);

  change_data_free (data);
}

static void
gst_insert_bin_init (GstInsertBin * self)
{
  GstProxyPad *internal;

  self->priv = gst_insert_bin_get_instance_private (self);

  g_queue_init (&self->priv->change_queue);

  self->priv->sinkpad = gst_ghost_pad_new_no_target_from_template ("sink",
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (self),
          "sink"));
  gst_element_add_pad (GST_ELEMENT (self), self->priv->sinkpad);

  internal = gst_proxy_pad_get_internal (GST_PROXY_PAD (self->priv->sinkpad));
  self->priv->srcpad = gst_ghost_pad_new_from_template ("src",
      GST_PAD (internal),
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (self), "src"));
  gst_object_unref (internal);
  gst_element_add_pad (GST_ELEMENT (self), self->priv->srcpad);
}

static void
gst_insert_bin_dispose (GObject * object)
{
  GstInsertBin *self = GST_INSERT_BIN (object);
  struct ChangeData *data;

  while ((data = g_queue_pop_head (&self->priv->change_queue)))
    gst_insert_bin_change_data_complete (self, data, FALSE);

  gst_ghost_pad_set_target (GST_GHOST_PAD (self->priv->srcpad), NULL);
  gst_ghost_pad_set_target (GST_GHOST_PAD (self->priv->sinkpad), NULL);

  G_OBJECT_CLASS (gst_insert_bin_parent_class)->dispose (object);
}

static gboolean
validate_element (GstInsertBin * self, GstElement * element)
{
  gboolean valid = TRUE;

  GST_OBJECT_LOCK (element);
  if (element->numsrcpads != 1 || element->numsinkpads != 1) {
    GST_WARNING_OBJECT (self,
        "Element does not have a single src and sink pad");
    valid = FALSE;
  }

  if (GST_OBJECT_PARENT (element) != NULL) {
    GST_WARNING_OBJECT (self, "Element already has a parent");
    valid = FALSE;
  }
  GST_OBJECT_UNLOCK (element);

  return valid;
}

static GstPad *
get_single_pad (GstElement * element, GstPadDirection direction)
{
  GstPad *pad = NULL;

  GST_OBJECT_LOCK (element);
  if (direction == GST_PAD_SRC) {
    if (element->numsrcpads == 1)
      pad = element->srcpads->data;
  } else {
    if (element->numsinkpads == 1)
      pad = element->sinkpads->data;
  }

  if (pad)
    gst_object_ref (pad);
  GST_OBJECT_UNLOCK (element);

  return pad;
}

static gboolean
is_right_direction_for_block (GstPad * pad)
{
  gboolean right_direction;

  GST_OBJECT_LOCK (pad);
  switch (pad->mode) {
    case GST_PAD_MODE_NONE:
      right_direction = TRUE;
      break;
    case GST_PAD_MODE_PUSH:
      right_direction = (pad->direction == GST_PAD_SRC);
      break;
    case GST_PAD_MODE_PULL:
      right_direction = (pad->direction == GST_PAD_SINK);
      break;
    default:
      right_direction = FALSE;
      g_assert_not_reached ();
  }
  GST_OBJECT_UNLOCK (pad);

  return right_direction;
}

static void
gst_insert_bin_block_pad_unlock (GstInsertBin * self)
{
  struct ChangeData *data;
  GstPad *pad;
  GstPadProbeType probetype;

again:

  data = g_queue_peek_head (&self->priv->change_queue);
  if (!data) {
    GST_OBJECT_UNLOCK (self);
    return;
  }

  if (data->action == GST_INSERT_BIN_ACTION_ADD &&
      !validate_element (self, data->element))
    goto error;

  if (data->action == GST_INSERT_BIN_ACTION_ADD) {
    if (data->sibling) {
      if (data->direction == DIRECTION_BEFORE)
        pad = get_single_pad (data->sibling, GST_PAD_SINK);
      else
        pad = get_single_pad (data->sibling, GST_PAD_SRC);
    } else {
      if (data->direction == DIRECTION_BEFORE)
        pad = (GstPad *)
            gst_proxy_pad_get_internal (GST_PROXY_PAD (self->priv->srcpad));
      else
        pad = (GstPad *)
            gst_proxy_pad_get_internal (GST_PROXY_PAD (self->priv->sinkpad));
    }

    if (!pad) {
      GST_WARNING_OBJECT (self, "Can not obtain a sibling pad to block"
          " before adding");
      goto error;
    }

    if (!is_right_direction_for_block (pad)) {
      GstPad *peer = gst_pad_get_peer (pad);

      if (peer) {
        gst_object_unref (pad);
        pad = peer;
      }
    }
  } else {
    GstPad *element_pad;

    element_pad = get_single_pad (data->element, GST_PAD_SINK);
    if (!element_pad) {
      GST_WARNING_OBJECT (self, "Can not obtain the element's sink pad");
      goto error;
    }

    if (!is_right_direction_for_block (element_pad)) {
      pad = gst_pad_get_peer (element_pad);
    } else {
      gst_object_unref (element_pad);

      element_pad = get_single_pad (data->element, GST_PAD_SRC);
      if (!element_pad) {
        GST_WARNING_OBJECT (self, "Can not obtain the element's src pad");
        goto error;
      }

      pad = gst_pad_get_peer (element_pad);
    }
    gst_object_unref (element_pad);

    if (!pad) {
      GST_WARNING_OBJECT (self, "Can not obtain a sibling pad for removing");
      goto error;
    }
  }

  if (GST_PAD_IS_SRC (pad))
    probetype = GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM;
  else
    probetype = GST_PAD_PROBE_TYPE_BLOCK_UPSTREAM;

  GST_OBJECT_UNLOCK (self);
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_IDLE | probetype, pad_blocked_cb,
      self, NULL);
  gst_object_unref (pad);
  return;

error:
  g_queue_pop_head (&self->priv->change_queue);
  gst_insert_bin_change_data_complete (self, data, FALSE);
  goto again;
}

static GstPadProbeReturn
wait_and_drop_eos_cb (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstPad *peer;

  if (GST_EVENT_TYPE (GST_PAD_PROBE_INFO_EVENT (info)) != GST_EVENT_EOS)
    return GST_PAD_PROBE_PASS;

  peer = gst_pad_get_peer (pad);
  if (peer) {
    gst_pad_unlink (pad, peer);
    gst_object_unref (peer);
  }

  return GST_PAD_PROBE_DROP;
}

static void
gst_insert_bin_do_change (GstInsertBin * self, GstPad * pad)
{
  struct ChangeData *data;

  GST_OBJECT_LOCK (self);

  if (!is_right_direction_for_block (pad)) {
    GST_WARNING_OBJECT (self, "Block pad does not have the expected direction");
    goto next;
  }

  while ((data = g_queue_pop_head (&self->priv->change_queue)) != NULL) {
    GstPad *peer = NULL;
    GstPad *other_peer = NULL;

    GST_OBJECT_UNLOCK (self);


    if (data->action == GST_INSERT_BIN_ACTION_ADD &&
        !validate_element (self, data->element))
      goto error;

    peer = gst_pad_get_peer (pad);

    if (peer == NULL) {
      GST_WARNING_OBJECT (self, "Blocked pad has no peer");
      goto error;
    }

    if (data->action == GST_INSERT_BIN_ACTION_ADD) {
      GstPad *srcpad = NULL, *sinkpad = NULL;
      GstPad *peersrcpad, *peersinkpad;

      /* First let's make sure we have the right pad */
      if (data->sibling) {
        GstElement *parent = NULL;
        GstPad *siblingpad;

        if ((gst_pad_get_direction (pad) == GST_PAD_SRC &&
                data->direction == DIRECTION_BEFORE) ||
            (gst_pad_get_direction (pad) == GST_PAD_SINK &&
                data->direction == DIRECTION_AFTER))
          siblingpad = peer;
        else
          siblingpad = pad;

        parent = gst_pad_get_parent_element (siblingpad);
        if (parent != NULL)
          gst_object_unref (parent);

        if (parent != data->sibling)
          goto retry;
      } else {
        GstObject *parent;
        GstPad *ghost;
        GstPad *proxypad;

        if (data->direction == DIRECTION_BEFORE) {
          ghost = self->priv->srcpad;
          if (gst_pad_get_direction (pad) == GST_PAD_SINK)
            proxypad = pad;
          else
            proxypad = peer;
        } else {
          ghost = self->priv->sinkpad;
          if (gst_pad_get_direction (pad) == GST_PAD_SINK)
            proxypad = peer;
          else
            proxypad = pad;
        }

        if (!GST_IS_PROXY_PAD (proxypad))
          goto retry;
        parent = gst_pad_get_parent (proxypad);
        if (!parent)
          goto retry;
        gst_object_unref (parent);
        if (GST_PAD_CAST (parent) != ghost)
          goto retry;
      }

      if (gst_pad_get_direction (pad) == GST_PAD_SRC) {
        peersrcpad = pad;
        peersinkpad = peer;
      } else {
        peersrcpad = peer;
        peersinkpad = pad;
      }

      if (GST_IS_PROXY_PAD (peersrcpad)) {
        GstObject *parent = gst_pad_get_parent (peersrcpad);

        if (GST_PAD_CAST (parent) == self->priv->sinkpad)
          peersrcpad = NULL;

        if (parent)
          gst_object_unref (parent);
      }

      if (GST_IS_PROXY_PAD (peersinkpad)) {
        GstObject *parent = gst_pad_get_parent (peersinkpad);

        if (GST_PAD_CAST (parent) == self->priv->srcpad)
          peersinkpad = NULL;

        if (parent)
          gst_object_unref (parent);
      }

      if (peersinkpad && peersrcpad) {
        gst_pad_unlink (peersrcpad, peersinkpad);
      } else {
        if (!peersinkpad)
          gst_ghost_pad_set_target (GST_GHOST_PAD (self->priv->srcpad), NULL);
        if (!peersrcpad)
          gst_ghost_pad_set_target (GST_GHOST_PAD (self->priv->sinkpad), NULL);
      }

      srcpad = get_single_pad (data->element, GST_PAD_SRC);
      sinkpad = get_single_pad (data->element, GST_PAD_SINK);

      if (srcpad == NULL || sinkpad == NULL) {
        GST_WARNING_OBJECT (self, "Can not get element src or sink pad");
        goto error;
      }

      if (!gst_bin_add (GST_BIN (self), data->element)) {
        GST_WARNING_OBJECT (self, "Can not add element to bin");
        goto error;
      }

      if (peersrcpad) {
        if (GST_PAD_LINK_FAILED (gst_pad_link (peersrcpad, sinkpad))) {
          GST_WARNING_OBJECT (self, "Can not link sibling's %s:%s pad"
              " to element's %s:%s pad", GST_DEBUG_PAD_NAME (peersrcpad),
              GST_DEBUG_PAD_NAME (sinkpad));
          goto error;
        }
      } else {
        if (!gst_ghost_pad_set_target (GST_GHOST_PAD (self->priv->sinkpad),
                sinkpad)) {
          GST_WARNING_OBJECT (self, "Can not set %s:%s as target for %s:%s",
              GST_DEBUG_PAD_NAME (sinkpad),
              GST_DEBUG_PAD_NAME (self->priv->sinkpad));
          goto error;
        }
      }

      if (peersinkpad) {
        if (GST_PAD_LINK_FAILED (gst_pad_link (srcpad, peersinkpad))) {
          GST_WARNING_OBJECT (self, "Can not link element's %s:%s pad"
              " to sibling's %s:%s pad", GST_DEBUG_PAD_NAME (srcpad),
              GST_DEBUG_PAD_NAME (peersinkpad));
          goto error;
        }
      } else {
        if (!gst_ghost_pad_set_target (GST_GHOST_PAD (self->priv->srcpad),
                srcpad)) {
          GST_WARNING_OBJECT (self, "Can not set %s:%s as target for %s:%s",
              GST_DEBUG_PAD_NAME (srcpad),
              GST_DEBUG_PAD_NAME (self->priv->srcpad));
          goto error;
        }
      }

      gst_object_unref (srcpad);
      gst_object_unref (sinkpad);

      if (!gst_element_sync_state_with_parent (data->element)) {
        GST_WARNING_OBJECT (self, "Can not sync element's state with parent");
        goto error;
      }
    } else {
      GstElement *parent = NULL;
      GstPad *other_pad;
      GstCaps *caps = NULL, *peercaps = NULL;
      gboolean can_intersect;
      gboolean success;

      parent = gst_pad_get_parent_element (peer);
      if (parent != NULL)
        gst_object_unref (parent);

      if (parent != data->element)
        goto retry;

      if (gst_pad_get_direction (peer) == GST_PAD_SRC)
        other_pad = get_single_pad (data->element, GST_PAD_SINK);
      else
        other_pad = get_single_pad (data->element, GST_PAD_SRC);

      if (!other_pad) {
        GST_WARNING_OBJECT (self, "Can not get element's other pad");
        goto error;
      }

      other_peer = gst_pad_get_peer (other_pad);
      gst_object_unref (other_pad);

      if (!other_peer) {
        GST_WARNING_OBJECT (self, "Can not get element's other peer");
        goto error;
      }

      /* Get the negotiated caps for the source pad peer,
       * because renegotiation while the pipeline is playing doesn't work
       * that fast.
       */
      if (gst_pad_get_direction (pad) == GST_PAD_SRC)
        caps = gst_pad_get_current_caps (pad);
      else
        peercaps = gst_pad_get_current_caps (other_peer);
      if (!caps)
        caps = gst_pad_query_caps (pad, NULL);
      if (!peercaps)
        peercaps = gst_pad_query_caps (other_peer, NULL);
      can_intersect = gst_caps_can_intersect (caps, peercaps);
      gst_caps_unref (caps);
      gst_caps_unref (peercaps);

      if (!can_intersect) {
        GST_WARNING_OBJECT (self, "Pads are incompatible without the element");
        goto error;
      }

      if (gst_pad_get_direction (other_peer) == GST_PAD_SRC &&
          gst_pad_is_active (other_peer)) {
        gulong probe_id;

        probe_id = gst_pad_add_probe (other_peer,
            GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
            wait_and_drop_eos_cb, NULL, NULL);
        gst_pad_send_event (peer, gst_event_new_eos ());
        gst_pad_remove_probe (other_peer, probe_id);
      }

      gst_element_set_locked_state (data->element, TRUE);
      gst_element_set_state (data->element, GST_STATE_NULL);
      if (!gst_bin_remove (GST_BIN (self), data->element)) {
        GST_WARNING_OBJECT (self, "Element removal rejected");
        goto error;
      }
      gst_element_set_locked_state (data->element, FALSE);

      if (gst_pad_get_direction (pad) == GST_PAD_SRC)
        success = GST_PAD_LINK_SUCCESSFUL (gst_pad_link_full (pad, other_peer,
                GST_PAD_LINK_CHECK_HIERARCHY |
                GST_PAD_LINK_CHECK_TEMPLATE_CAPS));
      else
        success = GST_PAD_LINK_SUCCESSFUL (gst_pad_link_full (other_peer, pad,
                GST_PAD_LINK_CHECK_HIERARCHY |
                GST_PAD_LINK_CHECK_TEMPLATE_CAPS));
      gst_object_unref (other_peer);
      other_peer = NULL;

      if (!success) {
        GST_ERROR_OBJECT (self, "Could not re-link after the element's"
            " removal");
        goto error;
      }
    }


    gst_insert_bin_change_data_complete (self, data, TRUE);
    gst_object_unref (peer);

    GST_OBJECT_LOCK (self);
    continue;
  done:
    if (other_peer != NULL)
      gst_object_unref (other_peer);

    if (peer != NULL)
      gst_object_unref (peer);
    break;
  retry:
    GST_OBJECT_LOCK (self);
    g_queue_push_head (&self->priv->change_queue, data);
    goto done;
  error:
    /* Handle error */
    gst_insert_bin_change_data_complete (self, data, FALSE);
    GST_OBJECT_LOCK (self);
    goto done;
  }

next:
  gst_insert_bin_block_pad_unlock (self);
}



static GstPadProbeReturn
pad_blocked_cb (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstInsertBin *self = GST_INSERT_BIN (user_data);

  g_assert (GST_PAD_PROBE_INFO_TYPE (info) &
      (GST_PAD_PROBE_TYPE_BLOCK | GST_PAD_PROBE_TYPE_IDLE));

  gst_insert_bin_do_change (self, pad);

  return GST_PAD_PROBE_REMOVE;
}

static void
gst_insert_bin_add_operation (GstInsertBin * self,
    GstElement * element, GstInsertBinAction action, GstElement * sibling,
    GstInsertBinDirection direction, GstInsertBinCallback callback,
    gpointer user_data)
{
  struct ChangeData *data = g_new (struct ChangeData, 1);
  gboolean block_pad;

  data->element = element;
  data->action = action;
  data->sibling = sibling;
  if (data->sibling)
    gst_object_ref (data->sibling);
  data->direction = direction;
  data->callback = callback;
  data->user_data = user_data;

  GST_OBJECT_LOCK (self);
  block_pad = g_queue_is_empty (&self->priv->change_queue);
  g_queue_push_tail (&self->priv->change_queue, data);

  if (block_pad)
    gst_insert_bin_block_pad_unlock (self);
  else
    GST_OBJECT_UNLOCK (self);
}

static void
gst_insert_bin_add (GstInsertBin * self, GstElement * element,
    GstElement * sibling, GstInsertBinDirection direction,
    GstInsertBinCallback callback, gpointer user_data)
{
  gst_object_ref_sink (element);

  if (!validate_element (self, element))
    goto reject;

  if (sibling) {
    gboolean is_parent;

    GST_OBJECT_LOCK (sibling);
    is_parent = (GST_OBJECT_PARENT (sibling) == GST_OBJECT_CAST (self));
    GST_OBJECT_UNLOCK (sibling);

    if (!is_parent)
      goto reject;
  }


  gst_insert_bin_add_operation (self, element, GST_INSERT_BIN_ACTION_ADD,
      sibling, direction, callback, user_data);
  return;

reject:
  if (callback)
    callback (self, element, FALSE, user_data);
  gst_object_unref (element);
}


/**
 * gst_insert_bin_prepend:
 * @element: the #GstElement to add
 * @callback: (scope async): the callback to call when the element has been
 *  added or not, or %NULL
 * @user_data: The data to pass to the callback
 *
 * This action signal adds the filter like element before any other element
 * in the bin.
 *
 * Same as the #GstInsertBin::prepend signal.
 *
 * Since: 1.2
 */

void
gst_insert_bin_prepend (GstInsertBin * self, GstElement * element,
    GstInsertBinCallback callback, gpointer user_data)
{
  g_return_if_fail (GST_IS_ELEMENT (element));

  gst_insert_bin_add (self, element, NULL, DIRECTION_AFTER,
      callback, user_data);
}

/**
 * gst_insert_bin_append:
 * @element: the #GstElement to add
 * @callback: (scope async): the callback to call when the element has been
 *  added or not, or %NULL
 * @user_data: The data to pass to the callback
 *
 * This action signal adds the filter like element after any other element
 * in the bin.
 *
 * Same as the #GstInsertBin::append signal.
 *
 * Since: 1.2
 */

void
gst_insert_bin_append (GstInsertBin * self, GstElement * element,
    GstInsertBinCallback callback, gpointer user_data)
{
  g_return_if_fail (GST_IS_ELEMENT (element));

  gst_insert_bin_add (self, element, NULL, DIRECTION_BEFORE,
      callback, user_data);
}

/**
 * gst_insert_bin_insert_before:
 * @element: the #GstElement to add
 * @sibling: the #GstElement to add @element before
 * @callback: (scope async): the callback to call when the element has been
 *  added or not, or %NULL
 * @user_data: The data to pass to the callback
 *
 * This action signal adds the filter like element before the @sibling
 * element in the bin.
 *
 * Same as the #GstInsertBin::insert-before signal.
 *
 * Since: 1.2
 */
void
gst_insert_bin_insert_before (GstInsertBin * self, GstElement * element,
    GstElement * sibling, GstInsertBinCallback callback, gpointer user_data)
{
  g_return_if_fail (GST_IS_ELEMENT (element));
  g_return_if_fail (GST_IS_ELEMENT (sibling));

  gst_insert_bin_add (self, element, sibling, DIRECTION_BEFORE,
      callback, user_data);
}

/**
 * gst_insert_bin_insert_after:
 * @element: the #GstElement to add
 * @sibling: the #GstElement to add @element after
 * @callback: (scope async): the callback to call when the element has been
 *  added or not, or %NULL
 * @user_data: The data to pass to the callback
 *
 * This action signal adds the filter like element after the @sibling
 * element in the bin.
 *
 * Same as the #GstInsertBin::insert-after signal.
 *
 * Since: 1.2
 */
void
gst_insert_bin_insert_after (GstInsertBin * self, GstElement * element,
    GstElement * sibling, GstInsertBinCallback callback, gpointer user_data)
{
  g_return_if_fail (GST_IS_ELEMENT (element));
  g_return_if_fail (GST_IS_ELEMENT (sibling));

  gst_insert_bin_add (self, element, sibling, DIRECTION_AFTER,
      callback, user_data);
}

/**
 * gst_insert_bin_remove:
 * @element: the #GstElement to remove
 * @callback: (scope async): the callback to call when the element has been
 *  removed or not, or %NULL
 * @user_data: The data to pass to the callback
 *
 * This action signal removed the filter like element from the bin.
 *
 * Same as the #GstInsertBin::remove signal.
 *
 * Since: 1.2
 */

void
gst_insert_bin_remove (GstInsertBin * self, GstElement * element,
    GstInsertBinCallback callback, gpointer user_data)
{
  GstObject *parent;

  g_return_if_fail (GST_IS_ELEMENT (element));

  parent = gst_element_get_parent (element);

  if (parent) {
    gboolean is_parent;

    is_parent = (parent == GST_OBJECT_CAST (self));
    gst_object_unref (parent);

    if (!is_parent) {
      if (callback)
        callback (self, element, FALSE, user_data);
      return;
    }
  } else {
    GList *item;
    struct ChangeData *data = NULL;

    GST_OBJECT_LOCK (self);
    for (item = self->priv->change_queue.head; item; item = item->next) {
      data = item->data;

      if (data->element == element) {
        if (data->action == GST_INSERT_BIN_ACTION_ADD)
          g_queue_delete_link (&self->priv->change_queue, item);
        break;
      }
      data = NULL;
    }
    GST_OBJECT_UNLOCK (self);

    if (data) {
      gst_object_ref (element);
      gst_insert_bin_change_data_complete (self, data, TRUE);
      if (callback)
        callback (self, element, TRUE, user_data);
      gst_object_unref (element);
    } else {
      if (callback)
        callback (self, element, FALSE, user_data);
    }
    return;
  }

  gst_object_ref (element);

  gst_insert_bin_add_operation (self, element, GST_INSERT_BIN_ACTION_REMOVE,
      NULL, FALSE, callback, user_data);
}

/**
 * gst_insert_bin_new:
 * @name: (allow-none): The name of the new #GstInsertBin element (or %NULL)
 *
 * Creates a new #GstInsertBin
 *
 * Returns: The new #GstInsertBin
 *
 * Since: 1.2
 */

GstElement *
gst_insert_bin_new (const gchar * name)
{
  if (name)
    return g_object_new (GST_TYPE_INSERT_BIN, "name", name, NULL);
  else
    return g_object_new (GST_TYPE_INSERT_BIN, NULL);
}
