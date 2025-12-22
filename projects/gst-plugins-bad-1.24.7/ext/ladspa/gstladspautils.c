/* GStreamer LADSPA utils
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
 *               2001 Steve Baker <stevebaker_org@yahoo.co.uk>
 *               2003 Andy Wingo <wingo at pobox.com>
 *               2013 Juan Manuel Borges Caño <juanmabcmail@gmail.com>
 *               2013 Stefan Sauer <ensonic@users.sf.net>
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

/*
 * This module is smartly shared between the source, transform and
 * sink elements. Handling any specific LADSPA <-> gstreamer interaction.
 *
 * FIXME:
 * Assigning channel orders could be tricky since LADSPA seems to not
 * specify order of channels in a really nice computer parseable way,
 * stereo is probably wrong, more than stereo is crazy. LADSPA has
 * no channel order. All that could be done is to parse the port names
 * for "(Left)/(Right)", "-L/-R" or ":l/:r" - these are the 3 patterns
 * seen most of the time.  By now, it just let's them pass in / pass out.
 * Some nice effort might be done to set channel-masks and/or channel
 * positions correctly, if this is needed and expected, users will tell.
 *
 * This affects mainly interleaving, right now, it just interleaves all
 * input and output ports. This is the right thing in 90% of the cases,
 * but will e.g. create a 4 channel out for a plugin that has 2 stereo
 * 'pairs'.
 *
 * Also, gstreamer supports not-interleaved audio, where you just memcpy
 * each channel after each other: c1...c1c2....c2 and so on. This is not
 * taken into account, but could be added to the _transform and caps easily
 * if users demands it.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstladspa.h"
#include "gstladspautils.h"
#include "gstladspafilter.h"
#include "gstladspasource.h"
#include "gstladspasink.h"

#include <math.h>

#ifdef HAVE_LRDF
#include <lrdf.h>
#endif

GST_DEBUG_CATEGORY_EXTERN (ladspa_debug);
#define GST_CAT_DEFAULT ladspa_debug

/*
 * Interleaved buffer: (c1c2c1c2...)
 * De-interleaved buffer: (c1c1...c2c2...)
 */
static inline void
gst_ladspa_ladspa_deinterleave_data (GstLADSPA * ladspa, LADSPA_Data * outdata,
    guint samples, guint8 * indata)
{
  guint i, j;
  const guint audio_in = ladspa->klass->count.audio.in;

  for (i = 0; i < audio_in; i++)
    for (j = 0; j < samples; j++)
      outdata[i * samples + j] = ((LADSPA_Data *) indata)[j * audio_in + i];
}

/*
 * Interleaved buffer: (c1c2c1c2...)
 * De-interleaved buffer: (c1c1...c2c2...)
 */
static inline void
gst_ladspa_interleave_ladspa_data (GstLADSPA * ladspa, guint8 * outdata,
    guint samples, LADSPA_Data * indata)
{
  guint i, j;
  const guint audio_out = ladspa->klass->count.audio.out;

  for (i = 0; i < audio_out; i++)
    for (j = 0; j < samples; j++)
      ((LADSPA_Data *) outdata)[j * audio_out + i] = indata[i * samples + j];
}

/*
 * Connect the audio in ports.
 */
static inline void
gst_ladspa_connect_audio_in (GstLADSPA * ladspa, guint samples,
    LADSPA_Data * data)
{
  guint i;

  for (i = 0; i < ladspa->klass->count.audio.in; i++) {
    ladspa->ports.audio.in[i] = data + (i * samples);
    ladspa->klass->descriptor->connect_port (ladspa->handle,
        ladspa->klass->map.audio.in[i], ladspa->ports.audio.in[i]);
  }
}

/*
 * Connect the audio out ports.
 */
static inline void
gst_ladspa_connect_audio_out (GstLADSPA * ladspa, guint samples,
    LADSPA_Data * data)
{
  guint i;

  for (i = 0; i < ladspa->klass->count.audio.out; i++) {
    ladspa->ports.audio.out[i] = data + (i * samples);
    ladspa->klass->descriptor->connect_port (ladspa->handle,
        ladspa->klass->map.audio.out[i], ladspa->ports.audio.out[i]);
  }
}

/*
 * Process a block of audio with the ladspa plugin.
 */
static inline void
gst_ladspa_run (GstLADSPA * ladspa, guint nframes)
{
  ladspa->klass->descriptor->run (ladspa->handle, nframes);
}

/*
 * The data entry/exit point.
 */
gboolean
gst_ladspa_transform (GstLADSPA * ladspa, guint8 * outdata, guint samples,
    guint8 * indata)
{
  LADSPA_Data *in, *out;

  in = g_new0 (LADSPA_Data, samples * ladspa->klass->count.audio.in);
  out = g_new0 (LADSPA_Data, samples * ladspa->klass->count.audio.out);

  gst_ladspa_ladspa_deinterleave_data (ladspa, in, samples, indata);

  gst_ladspa_connect_audio_in (ladspa, samples, in);
  gst_ladspa_connect_audio_out (ladspa, samples, out);

  gst_ladspa_run (ladspa, samples);

  gst_ladspa_interleave_ladspa_data (ladspa, outdata, samples, out);

  g_free (out);
  g_free (in);

  return TRUE;
}

static gboolean
gst_ladspa_activate (GstLADSPA * ladspa)
{
  g_return_val_if_fail (ladspa->handle != NULL, FALSE);
  g_return_val_if_fail (ladspa->activated == FALSE, FALSE);

  GST_DEBUG ("activating LADSPA plugin");

  if (ladspa->klass->descriptor->activate)
    ladspa->klass->descriptor->activate (ladspa->handle);

  ladspa->activated = TRUE;

  return TRUE;
}

static gboolean
gst_ladspa_deactivate (GstLADSPA * ladspa)
{
  g_return_val_if_fail (ladspa->handle != NULL, FALSE);
  g_return_val_if_fail (ladspa->activated == TRUE, FALSE);

  GST_DEBUG ("LADSPA deactivating plugin");

  if (ladspa->klass->descriptor->deactivate)
    ladspa->klass->descriptor->deactivate (ladspa->handle);

  ladspa->activated = FALSE;

  return TRUE;
}

static gboolean
gst_ladspa_open (GstLADSPA * ladspa, unsigned long rate)
{
  guint i;

  GST_DEBUG ("LADSPA instantiating plugin at %lu Hz", rate);

  if (!(ladspa->handle =
          ladspa->klass->descriptor->instantiate (ladspa->klass->descriptor,
              rate))) {
    GST_WARNING ("could not instantiate LADSPA plugin");
    return FALSE;
  }

  ladspa->rate = rate;

  /* connect the control ports */
  for (i = 0; i < ladspa->klass->count.control.in; i++)
    ladspa->klass->descriptor->connect_port (ladspa->handle,
        ladspa->klass->map.control.in[i], &(ladspa->ports.control.in[i]));
  for (i = 0; i < ladspa->klass->count.control.out; i++)
    ladspa->klass->descriptor->connect_port (ladspa->handle,
        ladspa->klass->map.control.out[i], &(ladspa->ports.control.out[i]));

  return TRUE;
}

static void
gst_ladspa_close (GstLADSPA * ladspa)
{
  g_return_if_fail (ladspa->handle != NULL);
  g_return_if_fail (ladspa->activated == FALSE);

  GST_DEBUG ("LADSPA deinstantiating plugin");

  if (ladspa->klass->descriptor->cleanup)
    ladspa->klass->descriptor->cleanup (ladspa->handle);

  ladspa->rate = 0;
  ladspa->handle = NULL;
}

/*
 * Safe open.
 */
gboolean
gst_ladspa_setup (GstLADSPA * ladspa, unsigned long rate)
{
  gboolean ret = TRUE;

  GST_DEBUG ("LADSPA setting up plugin");

  if (ladspa->handle && ladspa->rate != rate) {
    if (ladspa->activated)
      gst_ladspa_deactivate (ladspa);

    gst_ladspa_close (ladspa);
  }

  if (!ladspa->handle) {
    gst_ladspa_open (ladspa, rate);
    if (!(ret = gst_ladspa_activate (ladspa)))
      gst_ladspa_close (ladspa);
  }

  return ret;
}

/*
 * Safe close.
 */
gboolean
gst_ladspa_cleanup (GstLADSPA * ladspa)
{
  gboolean ret = TRUE;

  GST_DEBUG ("LADSPA cleaning up plugin");

  if (ladspa->handle) {
    if (ladspa->activated)
      ret = gst_ladspa_deactivate (ladspa);
    gst_ladspa_close (ladspa);
  }

  return ret;
}

static gchar *
gst_ladspa_object_class_get_param_name (GstLADSPAClass * ladspa_class,
    GObjectClass * object_class, unsigned long portnum)
{
  const LADSPA_Descriptor *desc = ladspa_class->descriptor;
  gchar *name, **namev, **v, *tmp;
  guint i;

  /* beauty in the mess */
  name = g_strdup ("");
  namev = g_strsplit_set (desc->PortNames[portnum], "[]()", 0);
  for (i = 0, v = namev; *v; i++, v++) {
    if (!(i % 2)) {
      tmp = name;
      name = g_strconcat (name, *v, NULL);
      g_free (tmp);
    }
  }
  g_strfreev (namev);
  g_strstrip (name);
  tmp = name;
  name = g_ascii_strdown (name, -1);
  g_free (tmp);

  /* this is the same thing that param_spec_* will do */
  g_strcanon (name, G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "-", '-');

  /* satisfy glib2 (argname[0] must be [A-Za-z]) */
  if (!((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A'
              && name[0] <= 'Z'))) {
    tmp = name;
    name = g_strconcat ("param-", name, NULL);
    g_free (tmp);
  }

  /* check for duplicate property names */
  if (g_object_class_find_property (G_OBJECT_CLASS (object_class), name)) {
    gint n = 1;
    gchar *nprop = g_strdup_printf ("%s-%d", name, n++);

    while (g_object_class_find_property (G_OBJECT_CLASS (object_class), nprop)) {
      g_free (nprop);
      nprop = g_strdup_printf ("%s-%d", name, n++);
    }
    g_free (name);
    name = nprop;
  }

  GST_DEBUG ("LADSPA built property name '%s' from port name '%s'", name,
      desc->PortNames[portnum]);

  return name;
}

static GParamSpec *
gst_ladspa_object_class_get_param_spec (GstLADSPAClass * ladspa_class,
    GObjectClass * object_class, unsigned long portnum)
{
  const LADSPA_Descriptor *desc = ladspa_class->descriptor;
  GParamSpec *ret;
  gchar *name;
  gint hintdesc, perms;
  gdouble lower, upper, def;

  name =
      gst_ladspa_object_class_get_param_name (ladspa_class, object_class,
      portnum);
  perms = G_PARAM_READABLE;
  if (LADSPA_IS_PORT_INPUT (desc->PortDescriptors[portnum]))
    perms |= G_PARAM_WRITABLE | G_PARAM_CONSTRUCT;
  if (LADSPA_IS_PORT_CONTROL (desc->PortDescriptors[portnum]))
    perms |= GST_PARAM_CONTROLLABLE;

  /* short name for hint descriptor */
  hintdesc = desc->PortRangeHints[portnum].HintDescriptor;

  if (LADSPA_IS_HINT_TOGGLED (hintdesc)) {
    ret =
        g_param_spec_boolean (name, name, desc->PortNames[portnum], FALSE,
        perms);
    g_free (name);
    return ret;
  }

  if (LADSPA_IS_HINT_BOUNDED_BELOW (hintdesc))
    lower = desc->PortRangeHints[portnum].LowerBound;
  else
    lower = -G_MAXFLOAT;

  if (LADSPA_IS_HINT_BOUNDED_ABOVE (hintdesc))
    upper = desc->PortRangeHints[portnum].UpperBound;
  else
    upper = G_MAXFLOAT;

  if (LADSPA_IS_HINT_SAMPLE_RATE (hintdesc)) {
    /* FIXME:! (*= ladspa->rate?, *= GST_AUDIO_DEF_RATE?) */
    if (LADSPA_IS_HINT_BOUNDED_BELOW (hintdesc))
      lower *= 44100;
    if (LADSPA_IS_HINT_BOUNDED_ABOVE (hintdesc))
      upper *= 44100;
  }

  if (LADSPA_IS_HINT_INTEGER (hintdesc)) {
    lower = CLAMP (lower, G_MININT, G_MAXINT);
    upper = CLAMP (upper, G_MININT, G_MAXINT);
  }

  /* default to lower bound */
  def = lower;

#ifdef LADSPA_IS_HINT_HAS_DEFAULT
  if (LADSPA_IS_HINT_HAS_DEFAULT (hintdesc)) {
    if (LADSPA_IS_HINT_DEFAULT_0 (hintdesc))
      def = 0.0;
    else if (LADSPA_IS_HINT_DEFAULT_1 (hintdesc))
      def = 1.0;
    else if (LADSPA_IS_HINT_DEFAULT_100 (hintdesc))
      def = 100.0;
    else if (LADSPA_IS_HINT_DEFAULT_440 (hintdesc))
      def = 440.0;
    if (LADSPA_IS_HINT_DEFAULT_MINIMUM (hintdesc))
      def = lower;
    else if (LADSPA_IS_HINT_DEFAULT_MAXIMUM (hintdesc))
      def = upper;
    else if (LADSPA_IS_HINT_LOGARITHMIC (hintdesc)) {
      if (LADSPA_IS_HINT_DEFAULT_LOW (hintdesc))
        def = exp (0.75 * log (lower) + 0.25 * log (upper));
      else if (LADSPA_IS_HINT_DEFAULT_MIDDLE (hintdesc))
        def = exp (0.5 * log (lower) + 0.5 * log (upper));
      else if (LADSPA_IS_HINT_DEFAULT_HIGH (hintdesc))
        def = exp (0.25 * log (lower) + 0.75 * log (upper));
    } else {
      if (LADSPA_IS_HINT_DEFAULT_LOW (hintdesc))
        def = 0.75 * lower + 0.25 * upper;
      else if (LADSPA_IS_HINT_DEFAULT_MIDDLE (hintdesc))
        def = 0.5 * lower + 0.5 * upper;
      else if (LADSPA_IS_HINT_DEFAULT_HIGH (hintdesc))
        def = 0.25 * lower + 0.75 * upper;
    }
  }
#endif /* LADSPA_IS_HINT_HAS_DEFAULT */

  if (lower > upper) {
    gfloat tmp;

    /* silently swap */
    tmp = lower;
    lower = upper;
    upper = tmp;
  }

  def = CLAMP (def, lower, upper);

  if (LADSPA_IS_HINT_INTEGER (hintdesc)) {
    ret =
        g_param_spec_int (name, name, desc->PortNames[portnum], lower, upper,
        def, perms);
  } else {
    ret =
        g_param_spec_float (name, name, desc->PortNames[portnum], lower, upper,
        def, perms);
  }

  g_free (name);

  return ret;
}

void
gst_ladspa_object_set_property (GstLADSPA * ladspa, GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  /* remember, properties have an offset */
  prop_id -= ladspa->klass->properties;

  /* only input ports */
  g_return_if_fail (prop_id < ladspa->klass->count.control.in);

  /* now see what type it is */
  switch (pspec->value_type) {
    case G_TYPE_BOOLEAN:
      ladspa->ports.control.in[prop_id] =
          g_value_get_boolean (value) ? 1.f : 0.f;
      break;
    case G_TYPE_INT:
      ladspa->ports.control.in[prop_id] = g_value_get_int (value);
      break;
    case G_TYPE_FLOAT:
      ladspa->ports.control.in[prop_id] = g_value_get_float (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

void
gst_ladspa_object_get_property (GstLADSPA * ladspa, GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  LADSPA_Data *controls;

  /* remember, properties have an offset */
  prop_id -= ladspa->klass->properties;

  if (prop_id < ladspa->klass->count.control.in) {
    controls = ladspa->ports.control.in;
  } else if (prop_id <
      ladspa->klass->count.control.in + ladspa->klass->count.control.out) {
    controls = ladspa->ports.control.out;
    prop_id -= ladspa->klass->count.control.in;
  } else {
    g_return_if_reached ();
  }

  /* now see what type it is */
  switch (pspec->value_type) {
    case G_TYPE_BOOLEAN:
      g_value_set_boolean (value, controls[prop_id] > 0.5);
      break;
    case G_TYPE_INT:{
      gint64 ival = CLAMP ((gint64) controls[prop_id], G_MININT, G_MAXINT);
      g_value_set_int (value, ival);
      break;
    }
    case G_TYPE_FLOAT:
      g_value_set_float (value, controls[prop_id]);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

void
gst_ladspa_object_class_install_properties (GstLADSPAClass * ladspa_class,
    GObjectClass * object_class, guint offset)
{
  GParamSpec *p;
  gint i;

  ladspa_class->properties = offset;

  /* register properties */
  for (i = 0; i < ladspa_class->count.control.in; i++, offset++) {
    p = gst_ladspa_object_class_get_param_spec (ladspa_class, object_class,
        ladspa_class->map.control.in[i]);
    g_object_class_install_property (object_class, offset, p);
  }
  for (i = 0; i < ladspa_class->count.control.out; i++, offset++) {
    p = gst_ladspa_object_class_get_param_spec (ladspa_class, object_class,
        ladspa_class->map.control.out[i]);
    g_object_class_install_property (object_class, offset, p);
  }
}

void
gst_ladspa_element_class_set_metadata (GstLADSPAClass * ladspa_class,
    GstElementClass * elem_class, const gchar * ladspa_class_tags)
{
  const LADSPA_Descriptor *desc = ladspa_class->descriptor;
  gchar *longname, *author, *extra_ladspa_class_tags = NULL, *tmp;
#ifdef HAVE_LRDF
  gchar *uri;
#endif

  longname = g_locale_to_utf8 (desc->Name, -1, NULL, NULL, NULL);
  if (!longname)
    longname = g_strdup ("no LADSPA description available");

  /* FIXME: no plugin author field different from element author field */
  tmp = g_locale_to_utf8 (desc->Maker, -1, NULL, NULL, NULL);
  if (!tmp)
    tmp = g_strdup ("no LADSPA author available");
  author =
      g_strjoin (", ", tmp,
      "Juan Manuel Borges Caño <juanmabcmail@gmail.com>",
      "Andy Wingo <wingo at pobox.com>",
      "Steve Baker <stevebaker_org@yahoo.co.uk>",
      "Erik Walthinsen <omega@cse.ogi.edu>",
      "Stefan Sauer <ensonic@users.sf.net>",
      "Wim Taymans <wim@fluendo.com>", NULL);
  g_free (tmp);

#ifdef HAVE_LRDF
  /* libldrf support, we want to get extra klass information here */
  uri = g_strdup_printf (LADSPA_BASE "%ld", desc->UniqueID);
  if (uri) {
    lrdf_statement query = { 0, };
    lrdf_uris *uris;
    gchar *str, *base_type = NULL;

    GST_DEBUG ("LADSPA uri (id=%lu) : %s", desc->UniqueID, uri);

    /* we can take this directly from 'desc', keep this example for future
       attributes.

       if ((str = lrdf_get_setting_metadata (uri, "title"))) {
       GST_DEBUG ("LADSPA title : %s", str);
       }
       if ((str = lrdf_get_setting_metadata (uri, "creator"))) {
       GST_DEBUG ("LADSPA creator : %s", str);
       }
     */

    /* get the rdf:type for this plugin */
    query.subject = uri;
    query.predicate = (char *) RDF_BASE "type";
    query.object = (char *) "?";
    query.next = NULL;
    uris = lrdf_match_multi (&query);
    if (uris) {
      if (uris->count == 1) {
        base_type = g_strdup (uris->items[0]);
        GST_DEBUG ("LADSPA base_type :  %s", base_type);
      }
      lrdf_free_uris (uris);
    }

    /* query taxonomy */
    if (base_type) {
      uris = lrdf_get_all_superclasses (base_type);
      if (uris) {
        guint32 j;

        for (j = 0; j < uris->count; j++) {
          if ((str = lrdf_get_label (uris->items[j]))) {
            GST_DEBUG ("LADSPA parent_type_label : %s", str);
            if (extra_ladspa_class_tags) {
              gchar *old_tags = extra_ladspa_class_tags;
              extra_ladspa_class_tags =
                  g_strconcat (extra_ladspa_class_tags, "/", str, NULL);
              g_free (old_tags);
            } else {
              extra_ladspa_class_tags = g_strconcat ("/", str, NULL);
            }
          }
        }
        lrdf_free_uris (uris);
      }
      g_free (base_type);
    }

    /* we can use this for the presets

       uris = lrdf_get_setting_uris (desc->UniqueID);
       if (uris) {
       guint32 j;

       for (j = 0; j < uris->count; j++) {
       GST_INFO ("setting_uri : %s", uris->items[j]);
       if ((str = lrdf_get_label (uris->items[j]))) {
       GST_INFO ("setting_label : %s", str);
       }
       }
       lrdf_free_uris (uris);
       }

     */
  }
  g_free (uri);

  if (extra_ladspa_class_tags) {
    char *s = g_strconcat (ladspa_class_tags, extra_ladspa_class_tags, NULL);
    g_free (extra_ladspa_class_tags);
    extra_ladspa_class_tags = s;
  }
#endif

  GST_INFO ("tags : %s", ladspa_class_tags);
  gst_element_class_set_metadata (elem_class, longname,
      extra_ladspa_class_tags ? extra_ladspa_class_tags : ladspa_class_tags,
      longname, author);

  g_free (extra_ladspa_class_tags);
  g_free (author);
  g_free (longname);
}

void
gst_ladspa_filter_type_class_add_pad_templates (GstLADSPAClass *
    ladspa_class, GstAudioFilterClass * audio_class)
{
  GstCaps *srccaps, *sinkcaps;

  srccaps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, GST_AUDIO_NE (F32),
      "channels", G_TYPE_INT, ladspa_class->count.audio.out,
      "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "layout", G_TYPE_STRING, "interleaved", NULL);

  sinkcaps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, GST_AUDIO_NE (F32),
      "channels", G_TYPE_INT, ladspa_class->count.audio.in,
      "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "layout", G_TYPE_STRING, "interleaved", NULL);

  gst_my_audio_filter_class_add_pad_templates (audio_class, srccaps, sinkcaps);

  gst_caps_unref (sinkcaps);
  gst_caps_unref (srccaps);
}

void
gst_ladspa_source_type_class_add_pad_template (GstLADSPAClass *
    ladspa_class, GstBaseSrcClass * base_class)
{
  GstCaps *srccaps;

  srccaps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, GST_AUDIO_NE (F32),
      "channels", G_TYPE_INT, ladspa_class->count.audio.out,
      "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "layout", G_TYPE_STRING, "interleaved", NULL);

  gst_my_base_source_class_add_pad_template (base_class, srccaps);

  gst_caps_unref (srccaps);
}

void
gst_ladspa_sink_type_class_add_pad_template (GstLADSPAClass * ladspa_class,
    GstBaseSinkClass * base_class)
{
  GstCaps *sinkcaps;

  sinkcaps = gst_caps_new_simple ("audio/x-raw",
      "format", G_TYPE_STRING, GST_AUDIO_NE (F32),
      "channels", G_TYPE_INT, ladspa_class->count.audio.in,
      "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "layout", G_TYPE_STRING, "interleaved", NULL);

  gst_my_base_sink_class_add_pad_template (base_class, sinkcaps);

  gst_caps_unref (sinkcaps);
}

void
gst_ladspa_init (GstLADSPA * ladspa, GstLADSPAClass * ladspa_class)
{
  GST_DEBUG ("LADSPA initializing component");

  ladspa->klass = ladspa_class;

  ladspa->handle = NULL;
  ladspa->activated = FALSE;
  ladspa->rate = 0;

  ladspa->ports.audio.in = g_new0 (LADSPA_Data *, ladspa_class->count.audio.in);
  ladspa->ports.audio.out =
      g_new0 (LADSPA_Data *, ladspa_class->count.audio.out);

  ladspa->ports.control.in =
      g_new0 (LADSPA_Data, ladspa_class->count.control.in);
  ladspa->ports.control.out =
      g_new0 (LADSPA_Data, ladspa_class->count.control.out);
}

void
gst_ladspa_finalize (GstLADSPA * ladspa)
{
  GST_DEBUG ("LADSPA finalizing component");

  g_free (ladspa->ports.control.out);
  ladspa->ports.control.out = NULL;
  g_free (ladspa->ports.control.in);
  ladspa->ports.control.in = NULL;

  g_free (ladspa->ports.audio.out);
  ladspa->ports.audio.out = NULL;
  g_free (ladspa->ports.audio.in);
  ladspa->ports.audio.in = NULL;
}

void
gst_ladspa_class_init (GstLADSPAClass * ladspa_class, GType type)
{
  guint mapper, ix;
  guint audio_in = 0, audio_out = 0, control_in = 0, control_out = 0;
  const GValue *value =
      gst_structure_get_value (ladspa_meta_all, g_type_name (type));
  GstStructure *ladspa_meta = g_value_get_boxed (value);
  const gchar *file_name;
  LADSPA_Descriptor_Function descriptor_function;

  GST_DEBUG ("LADSPA initializing class");

  file_name = gst_structure_get_string (ladspa_meta, "plugin-filename");
  ladspa_class->plugin =
      g_module_open (file_name, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
  g_module_symbol (ladspa_class->plugin, "ladspa_descriptor",
      (gpointer *) & descriptor_function);
  gst_structure_get_uint (ladspa_meta, "element-ix", &ix);

  ladspa_class->descriptor = descriptor_function (ix);
  gst_structure_get_uint (ladspa_meta, "audio-in",
      &ladspa_class->count.audio.in);
  gst_structure_get_uint (ladspa_meta, "audio-out",
      &ladspa_class->count.audio.out);
  gst_structure_get_uint (ladspa_meta, "control-in",
      &ladspa_class->count.control.in);
  gst_structure_get_uint (ladspa_meta, "control-out",
      &ladspa_class->count.control.out);
  ladspa_class->properties = 1;

  ladspa_class->map.audio.in =
      g_new0 (unsigned long, ladspa_class->count.audio.in);
  ladspa_class->map.audio.out =
      g_new0 (unsigned long, ladspa_class->count.audio.out);

  ladspa_class->map.control.in =
      g_new0 (unsigned long, ladspa_class->count.control.in);
  ladspa_class->map.control.out =
      g_new0 (unsigned long, ladspa_class->count.control.out);

  for (mapper = 0; mapper < ladspa_class->descriptor->PortCount; mapper++) {
    LADSPA_PortDescriptor p = ladspa_class->descriptor->PortDescriptors[mapper];

    if (LADSPA_IS_PORT_AUDIO (p)) {
      if (LADSPA_IS_PORT_INPUT (p))
        ladspa_class->map.audio.in[audio_in++] = mapper;
      else
        ladspa_class->map.audio.out[audio_out++] = mapper;
    } else if (LADSPA_IS_PORT_CONTROL (p)) {
      if (LADSPA_IS_PORT_INPUT (p))
        ladspa_class->map.control.in[control_in++] = mapper;
      else
        ladspa_class->map.control.out[control_out++] = mapper;
    }
  }

  g_assert (control_out == ladspa_class->count.control.out);
  g_assert (control_in == ladspa_class->count.control.in);

  g_assert (audio_out == ladspa_class->count.audio.out);
  g_assert (audio_in == ladspa_class->count.audio.in);
}

void
gst_ladspa_class_finalize (GstLADSPAClass * ladspa_class)
{
  GST_DEBUG ("LADSPA finalizing class");

  g_free (ladspa_class->map.control.out);
  ladspa_class->map.control.out = NULL;
  g_free (ladspa_class->map.control.in);
  ladspa_class->map.control.in = NULL;

  g_free (ladspa_class->map.audio.out);
  ladspa_class->map.audio.out = NULL;
  g_free (ladspa_class->map.audio.in);
  ladspa_class->map.audio.in = NULL;

  g_module_close (ladspa_class->plugin);
  ladspa_class->plugin = NULL;
}

/*
 * Create the type & register the element.
 */
void
ladspa_register_element (GstPlugin * plugin, GType parent_type,
    const GTypeInfo * info, GstStructure * ladspa_meta)
{
  const gchar *type_name =
      gst_structure_get_string (ladspa_meta, "element-type-name");

  gst_element_register (plugin, type_name, GST_RANK_NONE,
      g_type_register_static (parent_type, type_name, info, 0));
}
