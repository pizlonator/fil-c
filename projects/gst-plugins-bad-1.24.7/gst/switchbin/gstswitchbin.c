/* switchbin
 * Copyright (C) 2016  Carlos Rafael Giani
 *
 * gstswitchbin.c: Element for switching between paths based on input caps
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
 * SECTION:element-switchbin
 * @title: switchbin
 *
 * switchbin is a helper element that chooses between a set of processing
 * chains (called "paths") based on incoming caps, the caps of the paths,
 * and the result of caps queries issued to the elements within the paths.
 * It switches between these paths based on thes caps. Paths are child objects,
 * which are accessed by the #GstChildProxy interface.
 *
 * The intent is to allow for easy construction of dynamic pipelines that
 * automatically switches between paths based on the caps, which is useful
 * for cases when certain elements are only to be used for certain types
 * of dataflow. One common example is a switchbin that inserts postprocessing
 * elements only if the incoming caps are of a type that allows for such
 * postprocessing, like when a video dataflow could be raw frames in some
 * cases and encoded MPEG video in others - postprocessing plugins for
 * color space conversion, scaling and such then should only be inserted
 * if the data consists of raw frames, while encoded video is passed
 * through unchanged.
 *
 * Each path has an "element" property. If a #GstElement is passed to this,
 * switchbin takes ownership over that element. (Any previously set element
 * is removed and unref'd before the new one is set.) The element property
 * can also be NULL for a special passthrough mode (see below). In addition,
 * each path has a "caps" property, which is used for finding matching
 * paths. These caps are referred to as the "path caps".
 *
 * NOTE: Currently, switchbin has a limitation that path elements must
 * have exactly one "sink" and one "src" pad, both of which need to be
 * always available, so no request and no sometimes pads.
 *
 * Whenever new input caps are encountered at the switchbin's sinkpad,
 * the first path with matching caps is picked. A "match" means that the
 * result of gst_caps_can_intersect() is TRUE. The paths are looked at
 * in order: path \#0's caps are looked at first, checked against the new
 * input caps with gst_caps_can_intersect(), and if the return value
 * is TRUE, path \#0 is picked. Otherwise, path \#1's caps are looked at etc.
 * If no path matches, a GST_STREAM_ERROR_WRONG_TYPE error is reported.
 *
 * For queries, the concept of "allowed caps" is important. These are the
 * caps that are possible to use with this switchbin. They are computed
 * differently for sink- and for srcpads.
 *
 * Allowed sinkpad caps are computed by visiting each path, issuing an
 * internal caps query to the path element's sink pad, intersecting the
 * result from that query with the path caps, and appending that intersection
 * to the overall allowed sinkpad caps. Allowed srcpad caps are similar,
 * except that the result of the internal query is directly attached to the
 * overall allowed srcpad caps (no intersection with path caps takes place):
 * The intuition behind this is that in sinkpad direction, only caps that
 * are compatible with both the path caps and whatever the internal element
 * can handle are really usable - other caps will be rejected. In srcpad
 * direction, path caps do not exert an influence.
 *
 * The switchbin responds to caps and accept-caps queries in a particular
 * way. They involve the aforementioned "allowed caps".
 *
 * Caps queries are responded to by first checking if there are any paths.
 * If num-paths is 0, the query always fails. If there is no current path
 * selected, or if the path has no element, the allowed sink/srcpad caps
 * (depending on whether the query comes from the sink- or srcpad direction)
 * is directly used as the response. If a current path is selected, and it
 * has an element, the query is forwarded to that element instead.
 *
 * Accept-caps queries are handled by checking if the switchbin can find
 * a path whose caps match the caps from that query. If there is one, the
 * response to that query is TRUE, otherwise FALSE.
 *
 * As mentioned before, path caps can in theory be any kind of caps. However,
 * they always only affect the input side (= the sink pad side of the switchbin).
 * Path elements can produce output of any type, so their srcpad caps can be
 * anything, even caps that are entirely different. For example, it is perfectly
 * valid if the path caps are "video/x-raw", the path element sink pad template
 * caps also are "video/x-raw", and the src pad caps of the elements are
 * "application/x-rtp".
 *
 * Path elements can be set to NULL. Such paths perform dataflow passthrough.
 * The path then just forwards data. This includes caps and accept-caps queries.
 * Since there is no element, the internal caps queries go to the switchbin
 * peers instead (to the upstream peer when the query is at the switchbin's
 * srcpad, and to the downstream peer if the query is at the sinkpad).
 *
 * <refsect2>
 * <title>Example launch line</title>
 *
 * In this example, if the data is raw PCM audio with 44.1 kHz, a volume
 * element is used for reducing the audio volume to 10%. Otherwise, it is
 * just passed through. So, 44.1 kHz PCM audio will sound quiet, while
 * 48 kHz PCM and any non-PCM data will be passed through unmodified.
 *
 * |[
 *   gst-launch-1.0 uridecodebin uri=<URI> ! switchbin num-paths=2 \
 *     path0::element="audioconvert ! volume volume=0.1" path0::caps="audio/x-raw, rate=44100" \
 *     path1::caps="ANY" ! \
 *     autoaudiosink
 * ]|
 *
 * This example's path \#1 is a passthrough path. Its caps are "ANY" caps,
 * and its element is NULL (the default value). Dataflow is passed through,
 * and caps and accept-caps queries are forwarded to the switchbin peers.
 *
 * NOTE: Setting the caps to NULL instead of ANY would have accomplíshed
 * the same in this example, since NULL path caps are internally
 * interpreted as ANY caps.
 *
 * </refsect2>
 *
 */

#include <string.h>
#include <stdio.h>
#include "gstswitchbin.h"


GST_DEBUG_CATEGORY (switch_bin_debug);
#define GST_CAT_DEFAULT switch_bin_debug

enum
{
  PROP_0,
  PROP_NUM_PATHS,
  PROP_CURRENT_PATH,
  PROP_LAST
};

#define DEFAULT_NUM_PATHS 0
GParamSpec *switchbin_props[PROP_LAST];

#define PATH_LOCK(obj) g_mutex_lock(&(GST_SWITCH_BIN_CAST (obj)->path_mutex))
#define PATH_UNLOCK(obj) g_mutex_unlock(&(GST_SWITCH_BIN_CAST (obj)->path_mutex))
#define PATH_UNLOCK_AND_CHECK(obj) gst_switch_bin_unlock_paths_and_notify(GST_SWITCH_BIN_CAST (obj));


static GstStaticPadTemplate static_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate static_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);




static void gst_switch_bin_child_proxy_iface_init (gpointer iface,
    gpointer iface_data);
static GObject *gst_switch_bin_child_proxy_get_child_by_index (GstChildProxy *
    child_proxy, guint index);
static guint gst_switch_bin_child_proxy_get_children_count (GstChildProxy *
    child_proxy);


G_DEFINE_TYPE_WITH_CODE (GstSwitchBin,
    gst_switch_bin,
    GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,
        gst_switch_bin_child_proxy_iface_init)
    );
GST_ELEMENT_REGISTER_DEFINE (switchbin, "switchbin", GST_RANK_NONE,
    gst_switch_bin_get_type ());

static void gst_switch_bin_unlock_paths_and_notify (GstSwitchBin * switchbin);

static void gst_switch_bin_dispose (GObject * object);
static void gst_switch_bin_finalize (GObject * object);
static void gst_switch_bin_set_property (GObject * object, guint prop_id,
    GValue const *value, GParamSpec * pspec);
static void gst_switch_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_switch_bin_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_switch_bin_sink_query (GstPad * pad,
    GstObject * parent, GstQuery * query);
static gboolean gst_switch_bin_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

static gboolean gst_switch_bin_set_num_paths (GstSwitchBin * switch_bin,
    guint new_num_paths);
static gboolean gst_switch_bin_select_path_for_caps (GstSwitchBin *
    switch_bin, GstCaps * caps);
static gboolean gst_switch_bin_switch_to_path (GstSwitchBin * switch_bin,
    GstSwitchBinPath * switch_bin_path);
static GstSwitchBinPath *gst_switch_bin_find_matching_path (GstSwitchBin *
    switch_bin, GstCaps const *caps);

static void gst_switch_bin_set_sinkpad_block (GstSwitchBin * switch_bin,
    gboolean do_block);
static GstPadProbeReturn gst_switch_bin_blocking_pad_probe (GstPad * pad,
    GstPadProbeInfo * info, gpointer user_data);

static GstCaps *gst_switch_bin_get_allowed_caps (GstSwitchBin * switch_bin,
    GstPad * switch_bin_pad, gchar const *pad_name, GstCaps * filter);
static gboolean gst_switch_bin_are_caps_acceptable (GstSwitchBin *
    switch_bin, GstCaps const *caps);

static void
gst_switch_bin_unlock_paths_and_notify (GstSwitchBin * switchbin)
{
  gboolean do_notify = switchbin->path_changed;
  switchbin->path_changed = FALSE;
  PATH_UNLOCK (switchbin);

  if (do_notify)
    g_object_notify_by_pspec (G_OBJECT (switchbin),
        switchbin_props[PROP_CURRENT_PATH]);
}

static void
gst_switch_bin_child_proxy_iface_init (gpointer iface,
    G_GNUC_UNUSED gpointer iface_data)
{
  GstChildProxyInterface *child_proxy_iface = iface;

  child_proxy_iface->get_child_by_index =
      GST_DEBUG_FUNCPTR (gst_switch_bin_child_proxy_get_child_by_index);
  child_proxy_iface->get_children_count =
      GST_DEBUG_FUNCPTR (gst_switch_bin_child_proxy_get_children_count);
}


static GObject *
gst_switch_bin_child_proxy_get_child_by_index (GstChildProxy * child_proxy,
    guint index)
{
  GObject *result;
  GstSwitchBin *switch_bin = GST_SWITCH_BIN (child_proxy);

  PATH_LOCK (switch_bin);

  if (G_UNLIKELY (index >= switch_bin->num_paths))
    result = NULL;
  else
    result = g_object_ref (G_OBJECT (switch_bin->paths[index]));

  PATH_UNLOCK (switch_bin);

  return result;
}


static guint
gst_switch_bin_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  guint result;
  GstSwitchBin *switch_bin = GST_SWITCH_BIN (child_proxy);

  PATH_LOCK (switch_bin);
  result = switch_bin->num_paths;
  PATH_UNLOCK (switch_bin);

  return result;
}



static void
gst_switch_bin_class_init (GstSwitchBinClass * klass)
{
  GObjectClass *object_class;
  GstElementClass *element_class;

  GST_DEBUG_CATEGORY_INIT (switch_bin_debug, "switchbin", 0, "switch bin");

  object_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&static_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&static_src_template));

  object_class->dispose = GST_DEBUG_FUNCPTR (gst_switch_bin_dispose);
  object_class->finalize = GST_DEBUG_FUNCPTR (gst_switch_bin_finalize);
  object_class->set_property = GST_DEBUG_FUNCPTR (gst_switch_bin_set_property);
  object_class->get_property = GST_DEBUG_FUNCPTR (gst_switch_bin_get_property);

  /**
   * GstSwitchBin:num-paths
   *
   * Configure how many paths the switchbin will be choosing between. Attempting
   * to configure a path outside the range 0..(n-1) will fail. Reducing the
   * number of paths will release any paths outside the new range, which might
   * trigger activation of a new path by re-assessing the current caps.
   */
  switchbin_props[PROP_NUM_PATHS] = g_param_spec_uint ("num-paths",
      "Number of paths", "Number of paths", 0, G_MAXUINT - 1,
      DEFAULT_NUM_PATHS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
      PROP_NUM_PATHS, switchbin_props[PROP_NUM_PATHS]);

  /**
   * GstSwitchBin:current-path
   *
   * Returns the currently selected path number. If there is no current
   * path (due to no caps, or unsupported caps), the value is #G_MAXUINT. Read-only.
   */
  switchbin_props[PROP_CURRENT_PATH] =
      g_param_spec_uint ("current-path", "Current Path",
      "Currently selected path", 0, G_MAXUINT,
      0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class,
      PROP_CURRENT_PATH, switchbin_props[PROP_CURRENT_PATH]);

  gst_element_class_set_static_metadata (element_class,
      "switchbin",
      "Generic/Bin",
      "Switch between sub-pipelines (paths) based on input caps",
      "Carlos Rafael Giani <dv@pseudoterminal.org>");
}


static void
gst_switch_bin_init (GstSwitchBin * switch_bin)
{
  GstPad *pad;

  switch_bin->num_paths = DEFAULT_NUM_PATHS;
  switch_bin->paths = NULL;
  switch_bin->current_path = NULL;
  switch_bin->blocking_probe_id = 0;
  switch_bin->last_caps = NULL;

  switch_bin->sinkpad = gst_ghost_pad_new_no_target_from_template ("sink",
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (switch_bin),
          "sink")
      );
  gst_element_add_pad (GST_ELEMENT (switch_bin), switch_bin->sinkpad);

  switch_bin->srcpad = gst_ghost_pad_new_no_target_from_template ("src",
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (switch_bin),
          "src")
      );
  gst_element_add_pad (GST_ELEMENT (switch_bin), switch_bin->srcpad);

  gst_pad_set_event_function (switch_bin->sinkpad, gst_switch_bin_sink_event);
  gst_pad_set_query_function (switch_bin->sinkpad, gst_switch_bin_sink_query);
  gst_pad_set_query_function (switch_bin->srcpad, gst_switch_bin_src_query);

  switch_bin->input_identity =
      gst_element_factory_make ("identity", "input-identity");

  gst_bin_add (GST_BIN (switch_bin), switch_bin->input_identity);
  pad = gst_element_get_static_pad (switch_bin->input_identity, "sink");
  gst_ghost_pad_set_target (GST_GHOST_PAD (switch_bin->sinkpad), pad);
  gst_object_unref (GST_OBJECT (pad));
}

static void
gst_switch_bin_dispose (GObject * object)
{
  GstSwitchBin *switch_bin = GST_SWITCH_BIN (object);
  guint i;

  /* Chaining up will release all children of the bin,
   * invalidating any pointer to elements in the paths, so make sure
   * and clear those first */
  PATH_LOCK (switch_bin);
  for (i = 0; i < switch_bin->num_paths; ++i) {
    if (switch_bin->paths[i])
      switch_bin->paths[i]->element = NULL;
  }
  PATH_UNLOCK (switch_bin);
  G_OBJECT_CLASS (gst_switch_bin_parent_class)->dispose (object);
}

static void
gst_switch_bin_finalize (GObject * object)
{
  GstSwitchBin *switch_bin = GST_SWITCH_BIN (object);
  guint i;

  if (switch_bin->last_caps != NULL)
    gst_caps_unref (switch_bin->last_caps);

  for (i = 0; i < switch_bin->num_paths; ++i) {
    gst_object_unparent (GST_OBJECT (switch_bin->paths[i]));
  }
  g_free (switch_bin->paths);

  G_OBJECT_CLASS (gst_switch_bin_parent_class)->finalize (object);
}


static void
gst_switch_bin_set_property (GObject * object, guint prop_id,
    GValue const *value, GParamSpec * pspec)
{
  GstSwitchBin *switch_bin = GST_SWITCH_BIN (object);
  switch (prop_id) {
    case PROP_NUM_PATHS:
      PATH_LOCK (switch_bin);
      gst_switch_bin_set_num_paths (switch_bin, g_value_get_uint (value));
      PATH_UNLOCK_AND_CHECK (switch_bin);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_switch_bin_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstSwitchBin *switch_bin = GST_SWITCH_BIN (object);
  switch (prop_id) {
    case PROP_NUM_PATHS:
      PATH_LOCK (switch_bin);
      g_value_set_uint (value, switch_bin->num_paths);
      PATH_UNLOCK_AND_CHECK (switch_bin);
      break;
    case PROP_CURRENT_PATH:
      PATH_LOCK (switch_bin);
      if (switch_bin->current_path) {
        guint i;
        for (i = 0; i < switch_bin->num_paths; ++i) {
          if (switch_bin->paths[i] == switch_bin->current_path) {
            g_value_set_uint (value, i);
            break;
          }
        }
      } else {
        /* No valid path, return MAXUINT */
        g_value_set_uint (value, G_MAXUINT);
      }
      PATH_UNLOCK (switch_bin);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static gboolean
gst_switch_bin_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstSwitchBin *switch_bin = GST_SWITCH_BIN (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      /* Intercept the caps event to switch to an appropriate path, then
       * resume default caps event processing */

      GstCaps *caps;
      gboolean ret;

      gst_event_parse_caps (event, &caps);
      GST_DEBUG_OBJECT (switch_bin,
          "sink pad got caps event with caps %" GST_PTR_FORMAT
          " ; looking for matching path", (gpointer) caps);

      PATH_LOCK (switch_bin);
      ret = gst_switch_bin_select_path_for_caps (switch_bin, caps);
      PATH_UNLOCK_AND_CHECK (switch_bin);

      if (!ret) {
        gst_event_unref (event);
        return FALSE;
      } else
        return gst_pad_event_default (pad, parent, event);
    }

    default:
      GST_DEBUG_OBJECT (switch_bin, "sink event: %s",
          gst_event_type_get_name (GST_EVENT_TYPE (event)));
      return gst_pad_event_default (pad, parent, event);
  }
}


static gboolean
gst_switch_bin_handle_query (GstPad * pad, GstObject * parent, GstQuery * query,
    char const *pad_name)
{
  GstSwitchBin *switch_bin = GST_SWITCH_BIN (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);

      GST_DEBUG_OBJECT (switch_bin, "new caps query; filter: %" GST_PTR_FORMAT,
          filter);

      caps =
          gst_switch_bin_get_allowed_caps (switch_bin, pad, pad_name, filter);

      if (caps != NULL) {
        GST_DEBUG_OBJECT (switch_bin, "%s caps query:  caps: %" GST_PTR_FORMAT,
            pad_name, (gpointer) caps);
        gst_query_set_caps_result (query, caps);
        gst_caps_unref (caps);
        return TRUE;
      } else
        return FALSE;
    }

    case GST_QUERY_ACCEPT_CAPS:
    {
      GstCaps *caps;
      gboolean acceptable;

      gst_query_parse_accept_caps (query, &caps);
      PATH_LOCK (switch_bin);
      acceptable = gst_switch_bin_are_caps_acceptable (switch_bin, caps);
      PATH_UNLOCK (switch_bin);
      GST_DEBUG_OBJECT (switch_bin,
          "%s accept_caps query:  acceptable: %d  caps: %" GST_PTR_FORMAT,
          pad_name, (gint) acceptable, (gpointer) caps);

      gst_query_set_accept_caps_result (query, acceptable);
      return TRUE;
    }

    default:
      return gst_pad_query_default (pad, parent, query);
  }
}


static gboolean
gst_switch_bin_sink_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  return gst_switch_bin_handle_query (pad, parent, query, "sink");
}


static gboolean
gst_switch_bin_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  return gst_switch_bin_handle_query (pad, parent, query, "src");
}


static gboolean
gst_switch_bin_set_num_paths (GstSwitchBin * switch_bin, guint new_num_paths)
{
  guint i;
  gboolean cur_path_removed = FALSE;

  /* must be called with path lock held */

  if (switch_bin->num_paths == new_num_paths) {
    GST_DEBUG_OBJECT (switch_bin,
        "no change in number of paths - ignoring call");
    return TRUE;
  } else if (switch_bin->num_paths < new_num_paths) {
    /* New number of paths is larger -> N new paths need to be created & added,
     * where N = new_num_paths - switch_bin->num_paths. */

    GST_DEBUG_OBJECT (switch_bin, "adding %u new paths",
        new_num_paths - switch_bin->num_paths);

    switch_bin->paths =
        g_realloc (switch_bin->paths, sizeof (GstObject *) * new_num_paths);

    for (i = switch_bin->num_paths; i < new_num_paths; ++i) {
      GstSwitchBinPath *path;
      gchar *path_name;

      /* default names would be something like "switchbinpath0" */
      path_name = g_strdup_printf ("path%u", i);
      switch_bin->paths[i] = path =
          g_object_new (gst_switch_bin_path_get_type (), "name", path_name,
          NULL);
      path->bin = switch_bin;

      gst_object_set_parent (GST_OBJECT (path), GST_OBJECT (switch_bin));
      gst_child_proxy_child_added (GST_CHILD_PROXY (switch_bin),
          G_OBJECT (path), path_name);

      GST_DEBUG_OBJECT (switch_bin, "added path #%u \"%s\" (%p)", i, path_name,
          (gpointer) path);
      g_free (path_name);
    }
  } else {
    /* New number of paths is smaller -> the last N paths need to be removed,
     * where N = switch_bin->num_paths - new_num_paths. If one of the paths
     * that are being removed is the current path, then a new current path
     * is selected. */
    gchar *path_name;

    GST_DEBUG_OBJECT (switch_bin, "removing the last %u paths",
        switch_bin->num_paths - new_num_paths);

    for (i = new_num_paths; i < switch_bin->num_paths; ++i) {
      GstSwitchBinPath *path = switch_bin->paths[i];
      path_name = g_strdup (GST_OBJECT_NAME (path));

      if (path == switch_bin->current_path) {
        cur_path_removed = TRUE;
        gst_switch_bin_switch_to_path (switch_bin, NULL);

        GST_DEBUG_OBJECT (switch_bin,
            "path #%u \"%s\" (%p) is the current path - selecting a new current path will be necessary",
            i, path_name, (gpointer) (switch_bin->paths[i]));
      }

      gst_child_proxy_child_removed (GST_CHILD_PROXY (switch_bin),
          G_OBJECT (path), path_name);
      gst_object_unparent (GST_OBJECT (switch_bin->paths[i]));

      GST_DEBUG_OBJECT (switch_bin, "removed path #%u \"%s\" (%p)", i,
          path_name, (gpointer) (switch_bin->paths[i]));
      g_free (path_name);
    }

    switch_bin->paths =
        g_realloc (switch_bin->paths, sizeof (GstObject *) * new_num_paths);
  }

  switch_bin->num_paths = new_num_paths;

  if (new_num_paths > 0) {
    if (cur_path_removed) {
      /* Select a new current path if the previous one was removed above */

      if (switch_bin->last_caps != NULL) {
        GST_DEBUG_OBJECT (switch_bin,
            "current path was removed earlier - need to select a new one based on the last caps %"
            GST_PTR_FORMAT, (gpointer) (switch_bin->last_caps));
        return gst_switch_bin_select_path_for_caps (switch_bin,
            switch_bin->last_caps);
      } else {
        /* This should not happen. Every time a current path is selected, the
         * caps that were used for the selection are copied as the last_caps.
         * So, if a current path exists, but last_caps is NULL, it indicates
         * a bug. For example, if the current path was selected without calling
         * gst_switch_bin_select_path_for_caps(). */
        g_assert_not_reached ();
        return FALSE;           /* shuts up compiler warning */
      }
    } else
      return TRUE;
  } else
    return gst_switch_bin_switch_to_path (switch_bin, NULL);
}


static gboolean
gst_switch_bin_select_path_for_caps (GstSwitchBin * switch_bin, GstCaps * caps)
{
  /* must be called with path lock held */

  gboolean ret;
  GstSwitchBinPath *path;

  path = gst_switch_bin_find_matching_path (switch_bin, caps);
  if (path == NULL) {
    /* No matching path found, the caps are incompatible. Report this and exit. */

    GST_ELEMENT_ERROR (switch_bin, STREAM, WRONG_TYPE,
        ("could not find compatible path"), ("sink caps: %" GST_PTR_FORMAT,
            (gpointer) caps));
    ret = FALSE;
  } else {
    /* Matching path found. Try to switch to it. */

    GST_DEBUG_OBJECT (switch_bin, "found matching path \"%s\" (%p) - switching",
        GST_OBJECT_NAME (path), (gpointer) path);
    ret = gst_switch_bin_switch_to_path (switch_bin, path);
  }

  if (ret && (caps != switch_bin->last_caps))
    gst_caps_replace (&(switch_bin->last_caps), caps);

  return ret;
}


static gboolean
gst_switch_bin_switch_to_path (GstSwitchBin * switch_bin,
    GstSwitchBinPath * switch_bin_path)
{
  /* must be called with path lock held */

  gboolean ret = TRUE;

  if (switch_bin_path != NULL)
    GST_DEBUG_OBJECT (switch_bin, "switching to path \"%s\" (%p)",
        GST_OBJECT_NAME (switch_bin_path), (gpointer) switch_bin_path);
  else
    GST_DEBUG_OBJECT (switch_bin,
        "switching to NULL path (= disabling current path)");

  /* No current path set and no path is to be set -> nothing to do */
  if ((switch_bin_path == NULL) && (switch_bin->current_path == NULL))
    return TRUE;

  /* If this path is already the current one, do nothing */
  if (switch_bin->current_path == switch_bin_path)
    return TRUE;

  /* Block incoming data to be able to safely switch */
  gst_switch_bin_set_sinkpad_block (switch_bin, TRUE);

  /* Unlink the current path's element (if there is a current path) */
  if (switch_bin->current_path != NULL) {
    GstSwitchBinPath *cur_path = switch_bin->current_path;

    if (cur_path->element != NULL) {
      gst_element_set_state (cur_path->element, GST_STATE_NULL);
      gst_element_unlink (switch_bin->input_identity, cur_path->element);
    }

    gst_ghost_pad_set_target (GST_GHOST_PAD (switch_bin->srcpad), NULL);

    switch_bin->current_path = NULL;
    switch_bin->path_changed = TRUE;
  }

  /* Link the new path's element (if a new path is specified) */
  if (switch_bin_path != NULL) {
    if (switch_bin_path->element != NULL) {
      GstPad *pad;

      /* There is a path element. Link it into the pipeline. Data passes through
       * it now, since its associated path just became the current one. */

      /* TODO: currently, only elements with one "src" "sink" always-pad are supported;
       * add support for request and sometimes pads */

      pad = gst_element_get_static_pad (switch_bin_path->element, "src");
      if (pad == NULL) {
        GST_ERROR_OBJECT (switch_bin,
            "path element has no static srcpad - cannot link");
        ret = FALSE;
        goto finish;
      }

      if (!gst_ghost_pad_set_target (GST_GHOST_PAD (switch_bin->srcpad), pad)) {
        GST_ERROR_OBJECT (switch_bin,
            "could not set the path element's srcpad as the ghost srcpad's target");
        ret = FALSE;
      }

      gst_object_unref (GST_OBJECT (pad));

      if (!ret)
        goto finish;

      if (!gst_element_link (switch_bin->input_identity,
              switch_bin_path->element)) {
        GST_ERROR_OBJECT (switch_bin,
            "linking the path element's sinkpad failed ; check if the path element's sink caps and the upstream elements connected to the switchbin's sinkpad match");
        ret = FALSE;
        goto finish;
      }

      /* Unlock the element's state in case it was locked earlier
       * so its state can be synced to the switchbin's */
      gst_element_set_locked_state (switch_bin_path->element, FALSE);
      if (!gst_element_sync_state_with_parent (switch_bin_path->element)) {
        GST_ERROR_OBJECT (switch_bin,
            "could not sync the path element's state with that of the switchbin");
        ret = FALSE;
        goto finish;
      }
    } else {
      GstPad *srcpad;

      /* There is no path element. Just forward data. */
      GST_DEBUG_OBJECT (switch_bin, "path has no element ; will forward data");

      srcpad = gst_element_get_static_pad (switch_bin->input_identity, "src");

      g_assert (srcpad != NULL);

      if (!gst_ghost_pad_set_target (GST_GHOST_PAD (switch_bin->srcpad),
              srcpad)) {
        GST_ERROR_OBJECT (switch_bin,
            "could not set the path element's srcpad as the ghost srcpad's target");
        ret = FALSE;
      }

      gst_object_unref (GST_OBJECT (srcpad));
    }
  }

  switch_bin->current_path = switch_bin_path;
  switch_bin->path_changed = TRUE;

  /* If there is a new path to use, unblock the input */
  if (switch_bin_path != NULL)
    gst_switch_bin_set_sinkpad_block (switch_bin, FALSE);

finish:
  return ret;
}


static GstSwitchBinPath *
gst_switch_bin_find_matching_path (GstSwitchBin * switch_bin,
    GstCaps const *caps)
{
  /* must be called with path lock held */

  guint i;

  for (i = 0; i < switch_bin->num_paths; ++i) {
    GstSwitchBinPath *path = switch_bin->paths[i];

    /* Path caps are never supposed to be NULL. Even if the user
     * specifies NULL as caps in the path properties, the code in
     * gst_switch_bin_path_set_property () turns them into ANY caps. */
    g_assert (path->caps != NULL);

    if (gst_caps_can_intersect (caps, path->caps))
      return path;
  }

  return NULL;
}


static GstCaps *
gst_switch_bin_get_allowed_caps (GstSwitchBin * switch_bin,
    GstPad * switch_bin_pad, gchar const *pad_name, GstCaps * filter)
{
  guint i;
  guint num_paths;
  GstSwitchBinPath **paths;
  GstCaps *total_path_caps;
  gboolean peer_caps_queried = FALSE;
  gboolean peer_caps_query_successful;
  GstCaps *peer_caps = NULL;
  gboolean is_sink_pad =
      (gst_pad_get_direction (switch_bin_pad) == GST_PAD_SINK);

  /* Acquire references to the paths, path elements, and path caps, then
   * operate on those references instead of on the actual paths. That way,
   * we do not need to keep the path lock acquired for the entirety of the
   * function, which is important, since we need to issue caps queries to
   * other elements here. Doing that while the path lock is acquired can
   * cause deadlocks. And since we operate on references here, concurrent
   * changes to the paths won't cause race conditions. */

  PATH_LOCK (switch_bin);

  if (switch_bin->num_paths == 0) {
    PATH_UNLOCK (switch_bin);

    /* No paths exist, so nothing can be returned. This is not
     * necessarily an error - it can happen that caps queries take
     * place before the caller had a chance to set up paths for example. */
    GST_DEBUG_OBJECT (switch_bin, "no paths exist; "
        "cannot return any allowed caps");
    return NULL;
  }

  num_paths = switch_bin->num_paths;
  paths = g_malloc0_n (num_paths, sizeof (GstSwitchBinPath *));
  for (i = 0; i < num_paths; ++i)
    paths[i] = gst_object_ref (switch_bin->paths[i]);

  PATH_UNLOCK (switch_bin);

  /* From this moment on, the original paths are no longer accessed,
   * so we can release the path lock. */

  /* The allowed caps are a combination of the caps of all paths, the filter
   * caps, and the result of issuing caps queries to the path elements
   * (or to the switchbin sink/srcpads when paths have no elements). */

  total_path_caps = gst_caps_new_empty ();

  for (i = 0; i < num_paths; ++i) {
    GstSwitchBinPath *path = paths[i];
    GstCaps *queried_caps = NULL;
    GstCaps *intersected_caps;
    GstCaps *path_caps;
    GstElement *path_element;
    gboolean query_successful;
    GstPad *pad;
    GstQuery *caps_query;

    GST_OBJECT_LOCK (path);

    /* Path caps are never supposed to be NULL. Even if the user
     * specifies NULL as caps in the path properties, the code in
     * gst_switch_bin_path_set_property () turns them into ANY caps. */
    g_assert (path->caps != NULL);
    path_caps = gst_caps_ref (path->caps);

    path_element =
        (path->element != NULL) ? gst_object_ref (path->element) : NULL;

    GST_OBJECT_UNLOCK (path);

    /* We need to check what caps are handled by up/downstream, relative
     * to the switchbin src/sinkcaps. If there is an element, issue a
     * caps query to it to get that information. If there is no element,
     * then this path is a passthrough path, so the logical step is to
     * query peers instead. */

    if (path_element != NULL) {
      pad = gst_element_get_static_pad (path_element, pad_name);
      caps_query = gst_query_new_caps (filter);

      query_successful = gst_pad_query (pad, caps_query);
      if (query_successful) {
        gst_query_parse_caps_result (caps_query, &queried_caps);
        /* Ref the caps, otherwise they will be gone when the query is unref'd. */
        gst_caps_ref (queried_caps);
        GST_DEBUG_OBJECT (switch_bin, "queried element of path #%u "
            "(with filter applied if one is present), and query succeeded; "
            "result: %" GST_PTR_FORMAT, i, (gpointer) queried_caps);
      } else {
        GST_DEBUG_OBJECT (switch_bin, "queried element of path #%u "
            "(with filter applied if one is present), but query failed", i);
      }

      gst_query_unref (caps_query);
      gst_object_unref (GST_OBJECT (pad));

      gst_object_unref (GST_OBJECT (path_element));
    } else {
      /* Unlike in the non-NULL element case above, we issue a query
       * only once. We need to query the peer, and that peer does not
       * differ between paths, so querying more than once is redundant. */
      if (!peer_caps_queried) {
        pad = is_sink_pad ? switch_bin->srcpad : switch_bin->sinkpad;
        caps_query = gst_query_new_caps (filter);

        peer_caps_query_successful = gst_pad_peer_query (pad, caps_query);
        if (peer_caps_query_successful) {
          gst_query_parse_caps_result (caps_query, &peer_caps);
          /* Ref the caps, otherwise they will be gone when the query is unref'd. */
          gst_caps_ref (peer_caps);
          GST_DEBUG_OBJECT (switch_bin, "queried peer of %s pad (with filter "
              "applied if one is present), and query succeeded; result: %"
              GST_PTR_FORMAT, is_sink_pad ? "sink" : "src", (gpointer)
              peer_caps);
        } else {
          GST_DEBUG_OBJECT (switch_bin, "queried peer of %s pad "
              "(with filter applied if one is present), but query failed",
              is_sink_pad ? "sink" : "src");
        }

        gst_query_unref (caps_query);

        peer_caps_queried = TRUE;
      }

      /* Ref the caps here again because they are unref'd further below and
       * we want to keep the peer_caps around until all paths are handled. */
      if (peer_caps != NULL)
        queried_caps = gst_caps_ref (peer_caps);
      query_successful = peer_caps_query_successful;
    }

    if (query_successful) {
      /* If the caps query above succeeded, we know what up/downstream can
       * handle. In the sinkpad direction, the path caps further restrict
       * what caps can be used in this path, so intersect them with the
       * queried caps. In the srcpad direction, no such restriction exists. */

      if (is_sink_pad)
        intersected_caps = gst_caps_intersect (queried_caps, path_caps);
      else
        intersected_caps = gst_caps_copy (queried_caps);

      gst_caps_append (total_path_caps, intersected_caps);

      gst_caps_unref (path_caps);
    } else {
      /* If the query failed (for example, because the pad is not yet linked),
       * we have to make assumptions. In the sinkpad direction, the safest
       * bet is to use the path caps, since no matter what, only caps that
       * are a match with them can pass through this path. In the srcpad
       * direction, there are no restriction, so use ANY caps. */

      if (is_sink_pad)
        gst_caps_append (total_path_caps, path_caps);
      else
        gst_caps_append (total_path_caps, gst_caps_new_any ());
    }

    gst_caps_replace (&queried_caps, NULL);
  }

  /* Apply filter caps if present */
  if (filter != NULL) {
    GstCaps *tmp_caps = total_path_caps;
    /* Use filter caps as first caps in intersection along with the
     * GST_CAPS_INTERSECT_FIRST mode. This makes it possible to
     * define the order of the resulting caps by making it follow
     * the order of the filter caps. */
    total_path_caps = gst_caps_intersect_full (filter, tmp_caps,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp_caps);
  }

  gst_caps_replace (&peer_caps, NULL);

  /* Clear up the path references we acquired while holding
   * the path lock earlier. */
  for (i = 0; i < num_paths; ++i)
    gst_object_unref (paths[i]);
  g_free (paths);

  return total_path_caps;
}


static gboolean
gst_switch_bin_are_caps_acceptable (GstSwitchBin * switch_bin,
    GstCaps const *caps)
{
  /* must be called with path lock held */

  return (gst_switch_bin_find_matching_path (switch_bin, caps) != NULL);
}


static void
gst_switch_bin_set_sinkpad_block (GstSwitchBin * switch_bin, gboolean do_block)
{
  GstPad *pad;

  if ((do_block && (switch_bin->blocking_probe_id != 0)) || (!do_block
          && (switch_bin->blocking_probe_id == 0)))
    return;

  pad = gst_element_get_static_pad (switch_bin->input_identity, "sink");

  if (do_block) {
    switch_bin->blocking_probe_id =
        gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
        gst_switch_bin_blocking_pad_probe, NULL, NULL);
  } else {
    gst_pad_remove_probe (pad, switch_bin->blocking_probe_id);
    switch_bin->blocking_probe_id = 0;
  }

  GST_DEBUG_OBJECT (switch_bin, "sinkpad block enabled: %d", do_block);

  gst_object_unref (GST_OBJECT (pad));
}


static GstPadProbeReturn
gst_switch_bin_blocking_pad_probe (G_GNUC_UNUSED GstPad * pad,
    GstPadProbeInfo * info, G_GNUC_UNUSED gpointer user_data)
{
  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);
    switch (GST_EVENT_TYPE (event)) {
      case GST_EVENT_CAPS:
      case GST_EVENT_STREAM_START:
        return GST_PAD_PROBE_PASS;
      default:
        break;
    }
  }

  return GST_PAD_PROBE_OK;
}

/************ GstSwitchBinPath ************/


typedef struct _GstSwitchBinPathClass GstSwitchBinPathClass;


#define GST_TYPE_SWITCH_BIN_PATH             (gst_switch_bin_path_get_type())
#define GST_SWITCH_BIN_PATH(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_SWITCH_BIN_PATH, GstSwitchBinPath))
#define GST_SWITCH_BIN_PATH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_SWITCH_BIN_PATH, GstSwitchBinPathClass))
#define GST_SWITCH_BIN_PATH_CAST(obj)        ((GstSwitchBinPath *)(obj))
#define GST_IS_SWITCH_BIN_PATH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_SWITCH_BIN_PATH))
#define GST_IS_SWITCH_BIN_PATH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_SWITCH_BIN_PATH))


struct _GstSwitchBinPathClass
{
  GstObjectClass parent_class;
};


enum
{
  PROP_PATH_0,
  PROP_ELEMENT,
  PROP_PATH_CAPS
};


G_DEFINE_TYPE (GstSwitchBinPath, gst_switch_bin_path, GST_TYPE_OBJECT);


static void gst_switch_bin_path_dispose (GObject * object);
static void gst_switch_bin_path_set_property (GObject * object,
    guint prop_id, GValue const *value, GParamSpec * pspec);
static void gst_switch_bin_path_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_switch_bin_path_use_new_element (GstSwitchBinPath *
    switch_bin_path, GstElement * new_element);



static void
gst_switch_bin_path_class_init (GstSwitchBinPathClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = GST_DEBUG_FUNCPTR (gst_switch_bin_path_dispose);
  object_class->set_property =
      GST_DEBUG_FUNCPTR (gst_switch_bin_path_set_property);
  object_class->get_property =
      GST_DEBUG_FUNCPTR (gst_switch_bin_path_get_property);

  g_object_class_install_property (object_class,
      PROP_ELEMENT,
      g_param_spec_object ("element",
          "Element",
          "The path's element (if set to NULL, this path passes through dataflow)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_PATH_CAPS,
      g_param_spec_boxed ("caps",
          "Caps",
          "Caps which, if they are a subset of the input caps, select this path as the active one",
          GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
}


static void
gst_switch_bin_path_init (GstSwitchBinPath * switch_bin_path)
{
  switch_bin_path->caps = gst_caps_new_any ();
  switch_bin_path->element = NULL;
  switch_bin_path->bin = NULL;
}


static void
gst_switch_bin_path_dispose (GObject * object)
{
  GstSwitchBinPath *switch_bin_path = GST_SWITCH_BIN_PATH (object);

  if (switch_bin_path->caps != NULL) {
    gst_caps_unref (switch_bin_path->caps);
    switch_bin_path->caps = NULL;
  }

  if (switch_bin_path->element != NULL) {
    gst_switch_bin_path_use_new_element (switch_bin_path, NULL);
  }

  /* element is managed by the bin itself */

  G_OBJECT_CLASS (gst_switch_bin_path_parent_class)->dispose (object);
}


static void
gst_switch_bin_path_set_property (GObject * object, guint prop_id,
    GValue const *value, GParamSpec * pspec)
{
  GstSwitchBinPath *switch_bin_path = GST_SWITCH_BIN_PATH (object);
  switch (prop_id) {
    case PROP_ELEMENT:
    {
      /* Get the object without modifying the refcount */
      GstElement *new_element = GST_ELEMENT (g_value_get_object (value));

      GST_OBJECT_LOCK (switch_bin_path);
      PATH_LOCK (switch_bin_path->bin);
      gst_switch_bin_path_use_new_element (switch_bin_path, new_element);
      PATH_UNLOCK_AND_CHECK (switch_bin_path->bin);
      GST_OBJECT_UNLOCK (switch_bin_path);

      break;
    }

    case PROP_PATH_CAPS:
    {
      GstCaps *old_caps;
      GstCaps const *new_caps = gst_value_get_caps (value);

      GST_OBJECT_LOCK (switch_bin_path);
      old_caps = switch_bin_path->caps;
      if (new_caps == NULL) {
        /* NULL caps are interpreted as ANY */
        switch_bin_path->caps = gst_caps_new_any ();
      } else
        switch_bin_path->caps = gst_caps_copy (new_caps);
      GST_OBJECT_UNLOCK (switch_bin_path);

      if (old_caps != NULL)
        gst_caps_unref (old_caps);

      /* the new caps do not get applied right away
       * they only start to be used with the next stream */

      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_switch_bin_path_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSwitchBinPath *switch_bin_path = GST_SWITCH_BIN_PATH (object);
  switch (prop_id) {
    case PROP_ELEMENT:
      /* If a path element exists, increase its refcount first. This is
       * necessary because the code that called g_object_get() to fetch this
       * element will also unref it when it is finished with it. */
      if (switch_bin_path->element != NULL)
        gst_object_ref (GST_OBJECT (switch_bin_path->element));

      /* Use g_value_take_object() instead of g_value_set_object() as the latter
       * increases the element's refcount for the duration of the GValue's lifetime */
      g_value_take_object (value, switch_bin_path->element);

      break;

    case PROP_PATH_CAPS:
      GST_OBJECT_LOCK (switch_bin_path);
      gst_value_set_caps (value, switch_bin_path->caps);
      GST_OBJECT_UNLOCK (switch_bin_path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static gboolean
gst_switch_bin_path_use_new_element (GstSwitchBinPath * switch_bin_path,
    GstElement * new_element)
{
  /* Must be called with lock */

  GstSwitchBinPath *current_path = switch_bin_path->bin->current_path;
  gboolean is_current_path = (current_path == switch_bin_path);

  /* Before switching the element, make sure it is not linked,
   * which is the case if this is the current path. */
  if (is_current_path)
    gst_switch_bin_switch_to_path (switch_bin_path->bin, NULL);

  /* Remove any present path element prior to using the new one */
  if (switch_bin_path->element != NULL) {
    gst_element_set_state (switch_bin_path->element, GST_STATE_NULL);
    /* gst_bin_remove automatically unrefs the path element */
    gst_bin_remove (GST_BIN (switch_bin_path->bin), switch_bin_path->element);
    switch_bin_path->element = NULL;
  }

  /* If new_element is non-NULL, use it as the path's new element.
   * If it is NULL, store that NULL pointer. Setting the path element
   * to NULL is useful if the caller wants to manually remove the
   * element from the path. (Setting it to NULL unparents & unrefs
   * the path element.) It is also useful if the caller just wants
   * to forward data unaltered in that path (switchbin's input_identity
   * element will then have its srcpad be directly exposed as a ghost
   * pad on the bin). */
  if (new_element != NULL) {
    gst_bin_add (GST_BIN (switch_bin_path->bin), new_element);
    switch_bin_path->element = new_element;

    /* Lock the element's state in case. This prevents freezes, which can happen
     * when an element from a not-current path tries to follow a state change,
     * but is unable to do so as long as it isn't linked. By locking the state,
     * it won't follow state changes, so the freeze does not happen. */
    gst_element_set_locked_state (new_element, TRUE);
  }

  /* We are done. Switch back to the path if it is the current one,
   * since we switched away from it earlier. */
  if (is_current_path)
    return gst_switch_bin_switch_to_path (switch_bin_path->bin, current_path);
  else
    return TRUE;
}
