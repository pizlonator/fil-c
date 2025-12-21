/* GStreamer encoding profiles library
 * Copyright (C) 2009-2010 Edward Hervey <edward.hervey@collabora.co.uk>
 *           (C) 2009-2010 Nokia Corporation
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
 * SECTION:encoding-profile
 * @title: GstEncodingProfile
 * @short_description: Encoding profile library
 * @symbols:
 * - GstEncodingProfile
 * - GstEncodingContainerProfile
 * - GstEncodingVideoProfile
 * - GstEncodingAudioProfile
 *
 * Functions to create and handle encoding profiles.
 *
 * Encoding profiles describe the media types and settings one wishes to use for
 * an encoding process. The top-level profiles are commonly
 * #GstEncodingContainerProfile(s) (which contains a user-readable name and
 * description along with which container format to use). These, in turn,
 * reference one or more #GstEncodingProfile(s) which indicate which encoding
 * format should be used on each individual streams.
 *
 * #GstEncodingProfile(s) can be provided to the 'encodebin' element, which will
 * take care of selecting and setting up the required elements to produce an
 * output stream conforming to the specifications of the profile.
 *
 * The encoding profiles do not necessarily specify which #GstElement to use for
 * the various encoding and muxing steps, as they allow to specifying the format
 * one wishes to use.
 *
 * Encoding profiles can be created at runtime by the application or loaded from
 * (and saved to) file using the #GstEncodingTarget API.
 *
 * ## The encoding profile serialization format
 *
 * Encoding profiles can be serialized to be used in the command line tools or
 * to set it on other other #GObject-s using #gst_util_set_object_arg for
 * example.
 *
 * The serialization format aims at being simple to understand although flexible
 * enough to describe any possible encoding profile. There are several ways to
 * describe the profile depending on the context but the general idea is that it
 * is a colon separated list of EncodingProfiles descriptions, the first one
 * needs to describe a #GstEncodingContainerProfile and the following ones
 * describe elementary streams.
 *
 * ### Using encoders and muxer element factory name
 *
 * ```
 *   muxer_factory_name:video_encoder_factory_name:audio_encoder_factory_name
 * ```
 *
 * For example to encode a stream into a WebM container, with an OGG audio
 * stream and a VP8 video stream, the serialized #GstEncodingProfile looks like:
 *
 * ```
 *   webmmux:vp8enc:vorbisenc
 * ```
 *
 * ### Define the encoding profile in a generic way using caps:
 *
 * ```
 *   muxer_source_caps:video_encoder_source_caps:audio_encoder_source_caps
 * ```
 *
 * For example to encode a stream into a WebM container, with an OGG audio
 * stream and a VP8 video stream, the serialized #GstEncodingProfile looks like:
 *
 * ```
 *   video/webm:video/x-vp8:audio/x-vorbis
 * ```
 *
 * It is possible to mix caps and element type names so you can specify a
 * specific video encoder while using caps for other encoders/muxer.
 *
 * ### Using preset
 *
 * You can also set the preset name of the encoding profile using the
 * caps+preset_name syntax as in:
 *
 * ```
 *   video/webm:video/x-vp8+youtube-preset:audio/x-vorbis
 * ```
 *
 * ### Setting properties on muxers or on the encoding profile itself
 *
 * Moreover, you can set the extra properties:
 *
 *  * `|element-properties,property1=true` (See
 *    #gst_encoding_profile_set_element_properties)
 *  * `|presence=true` (See See #gst_encoding_profile_get_presence)
 *  * `|single-segment=true` (See #gst_encoding_profile_set_single_segment)
 *  * `|single-segment=true` (See
 *    #gst_encoding_video_profile_set_variableframerate)
 *
 * for example:
 *
 * ```
 *   video/webm:video/x-vp8|presence=1|element-properties,target-bitrate=500000:audio/x-vorbis
 * ```
 *
 * ### Enforcing properties to the stream itself (video size, number of audio channels, etc..)
 *
 * You can also use the `restriction_caps->encoded_format_caps` syntax to
 * specify the restriction caps to be set on a #GstEncodingProfile
 *
 * It corresponds to the restriction #GstCaps to apply before the encoder that
 * will be used in the profile (See #gst_encoding_profile_get_restriction). The
 * fields present in restriction caps are properties of the raw stream (that is,
 * before encoding), such as height and width for video and depth and sampling
 * rate for audio. This property does not make sense for muxers. See
 * #gst_encoding_profile_get_restriction for more details.
 *
 * To force a video stream to be encoded with a Full HD resolution (using WebM
 * as the container format, VP8 as the video codec and Vorbis as the audio
 * codec), you should use:
 *
 * ```
 *   "video/webm:video/x-raw,width=1920,height=1080->video/x-vp8:audio/x-vorbis"
 * ```
 *
 * > NOTE: Make sure to enclose into quotes to avoid '>' to be reinterpreted by
 * > the shell.
 *
 * In the case you are specifying encoders directly, the following is also
 * possible:
 *
 * ```
 *   matroskamux:x264enc,width=1920,height=1080:audio/x-vorbis
 * ```
 *
 * ## Some serialized encoding formats examples
 *
 * ### MP3 audio and H264 in MP4**
 *
 * ```
 *   video/quicktime,variant=iso:video/x-h264:audio/mpeg,mpegversion=1,layer=3
 * ```
 *
 * ### Vorbis and theora in OGG
 *
 * ```
 *   application/ogg:video/x-theora:audio/x-vorbis
 * ```
 *
 * ### AC3 and H264 in MPEG-TS
 *
 * ```
 *   video/mpegts:video/x-h264:audio/x-ac3
 * ```
 *
 * ## Loading a profile from encoding targets
 *
 * Anywhere you have to use a string to define a #GstEncodingProfile, you
 * can use load it from a #GstEncodingTarget using the following syntaxes:
 *
 * ```
 *   target_name[/profilename/category]
 * ```
 *
 * or
 *
 * ```
 *   /path/to/target.gep:profilename
 * ```
 *
 * ## Examples
 *
 * ### Creating a profile
 *
 * ``` c
 * #include <gst/pbutils/encoding-profile.h>
 * ...
 * GstEncodingProfile *
 * create_ogg_theora_profile(void)
 *{
 *  GstEncodingContainerProfile *prof;
 *  GstCaps *caps;
 *
 *  caps = gst_caps_from_string("application/ogg");
 *  prof = gst_encoding_container_profile_new("Ogg audio/video",
 *     "Standard OGG/THEORA/VORBIS",
 *     caps, NULL);
 *  gst_caps_unref (caps);
 *
 *  caps = gst_caps_from_string("video/x-theora");
 *  gst_encoding_container_profile_add_profile(prof,
 *       (GstEncodingProfile*) gst_encoding_video_profile_new(caps, NULL, NULL, 0));
 *  gst_caps_unref (caps);
 *
 *  caps = gst_caps_from_string("audio/x-vorbis");
 *  gst_encoding_container_profile_add_profile(prof,
 *       (GstEncodingProfile*) gst_encoding_audio_profile_new(caps, NULL, NULL, 0));
 *  gst_caps_unref (caps);
 *
 *  return (GstEncodingProfile*) prof;
 *}
 *
 * ```
 *
 * ### Example: Using an encoder preset with a profile
 *
 * ``` c
 * #include <gst/pbutils/encoding-profile.h>
 * ...
 * GstEncodingProfile *
 * create_ogg_theora_profile(void)
 *{
 *  GstEncodingVideoProfile *v;
 *  GstEncodingAudioProfile *a;
 *  GstEncodingContainerProfile *prof;
 *  GstCaps *caps;
 *  GstPreset *preset;
 *
 *  caps = gst_caps_from_string ("application/ogg");
 *  prof = gst_encoding_container_profile_new ("Ogg audio/video",
 *     "Standard OGG/THEORA/VORBIS",
 *     caps, NULL);
 *  gst_caps_unref (caps);
 *
 *  preset = GST_PRESET (gst_element_factory_make ("theoraenc", "theorapreset"));
 *  g_object_set (preset, "bitrate", 1000, NULL);
 *  // The preset will be saved on the filesystem,
 *  // so try to use a descriptive name
 *  gst_preset_save_preset (preset, "theora_bitrate_preset");
 *  gst_object_unref (preset);
 *
 *  caps = gst_caps_from_string ("video/x-theora");
 *  v = gst_encoding_video_profile_new (caps, "theora_bitrate_preset", NULL, 0);
 *  gst_encoding_container_profile_add_profile (prof, (GstEncodingProfile*) v);
 *  gst_caps_unref (caps);
 *
 *  caps = gst_caps_from_string ("audio/x-vorbis");
 *  a = gst_encoding_audio_profile_new (caps, NULL, NULL, 0);
 *  gst_encoding_container_profile_add_profile (prof, (GstEncodingProfile*) a);
 *  gst_caps_unref (caps);
 *
 *  return (GstEncodingProfile*) prof;
 *}
 *
 * ```
 *
 * ### Listing categories, targets and profiles
 *
 * ``` c
 * #include <gst/pbutils/encoding-profile.h>
 * ...
 * GstEncodingProfile *prof;
 * GList *categories, *tmpc;
 * GList *targets, *tmpt;
 * ...
 * categories = gst_encoding_list_available_categories ();
 *
 * ... Show available categories to user ...
 *
 * for (tmpc = categories; tmpc; tmpc = tmpc->next) {
 *   gchar *category = (gchar *) tmpc->data;
 *
 *   ... and we can list all targets within that category ...
 *
 *   targets = gst_encoding_list_all_targets (category);
 *
 *   ... and show a list to our users ...
 *
 *   g_list_foreach (targets, (GFunc) gst_encoding_target_unref, NULL);
 *   g_list_free (targets);
 * }
 *
 * g_list_foreach (categories, (GFunc) g_free, NULL);
 * g_list_free (categories);
 *
 * ...
 * ```
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "encoding-profile.h"
#include "encoding-target.h"

#include <string.h>

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT gst_pb_utils_encoding_profile_ensure_debug_category()

static GstDebugCategory *
gst_pb_utils_encoding_profile_ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    GstDebugCategory *cat = NULL;

    GST_DEBUG_CATEGORY_INIT (cat, "encoding-profile", 0,
        "GstPbUtils encoding profile");

    g_once_init_leave (&cat_gonce, (gsize) cat);
  }

  return (GstDebugCategory *) cat_gonce;
}
#endif /* GST_DISABLE_GST_DEBUG */

/* GstEncodingProfile API */
#define PROFILE_LOCK(profile) (g_mutex_lock(&((GstEncodingProfile*)profile)->lock))
#define PROFILE_UNLOCK(profile) (g_mutex_unlock(&((GstEncodingProfile*)profile)->lock))

struct _GstEncodingProfile
{
  GObject parent;

  /*< public > */
  gchar *name;
  gchar *description;
  GstCaps *format;
  gchar *preset;
  gchar *preset_name;
  guint presence;
  gboolean allow_dynamic_output;
  gboolean enabled;
  gboolean single_segment;

  GMutex lock;                  // {
  GstCaps *restriction;
  GstStructure *element_properties;
  // }
};

struct _GstEncodingProfileClass
{
  GObjectClass parent_class;

  void (*copy) (GstEncodingProfile * self, GstEncodingProfile * copy);
};

enum
{
  FIRST_PROPERTY,
  PROP_RESTRICTION_CAPS,
  PROP_ELEMENT_PROPERTIES,
  LAST_PROPERTY
};

static GParamSpec *_properties[LAST_PROPERTY];

static void string_to_profile_transform (const GValue * src_value,
    GValue * dest_value);
static gboolean gst_encoding_profile_deserialize_valfunc (GValue * value,
    const gchar * s);
static gchar *gst_encoding_profile_serialize_valfunc (GValue * value);

static void gst_encoding_profile_class_init (GstEncodingProfileClass * klass);
static gpointer gst_encoding_profile_parent_class = NULL;

static void
gst_encoding_profile_class_intern_init (gpointer klass)
{
  gst_encoding_profile_parent_class = g_type_class_peek_parent (klass);
  gst_encoding_profile_class_init ((GstEncodingProfileClass *) klass);
}

GType
gst_encoding_profile_get_type (void)
{
  static gsize g_define_type_id_init = 0;

  if (g_once_init_enter (&g_define_type_id_init)) {
    GType g_define_type_id = g_type_register_static_simple (G_TYPE_OBJECT,
        g_intern_static_string ("GstEncodingProfile"),
        sizeof (GstEncodingProfileClass),
        (GClassInitFunc) gst_encoding_profile_class_intern_init,
        sizeof (GstEncodingProfile),
        NULL,
        (GTypeFlags) 0);
    static GstValueTable gstvtable = {
      G_TYPE_NONE,
      (GstValueCompareFunc) NULL,
      (GstValueSerializeFunc) gst_encoding_profile_serialize_valfunc,
      (GstValueDeserializeFunc) gst_encoding_profile_deserialize_valfunc
    };

    gstvtable.type = g_define_type_id;

    /* Register a STRING=>PROFILE GValueTransformFunc */
    g_value_register_transform_func (G_TYPE_STRING, g_define_type_id,
        string_to_profile_transform);
    /* Register gst-specific GValue functions */
    gst_value_register (&gstvtable);

    g_once_init_leave (&g_define_type_id_init, g_define_type_id);
  }
  return g_define_type_id_init;
}


static void
_encoding_profile_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstEncodingProfile *prof = (GstEncodingProfile *) object;

  switch (prop_id) {
    case PROP_RESTRICTION_CAPS:
      gst_value_set_caps (value, prof->restriction);
      break;
    case PROP_ELEMENT_PROPERTIES:
      PROFILE_LOCK (prof);
      gst_value_set_structure (value, prof->element_properties);
      PROFILE_UNLOCK (prof);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
_encoding_profile_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstEncodingProfile *prof = (GstEncodingProfile *) object;

  switch (prop_id) {
    case PROP_RESTRICTION_CAPS:
      gst_encoding_profile_set_restriction (prof, gst_caps_copy
          (gst_value_get_caps (value)));
      break;
    case PROP_ELEMENT_PROPERTIES:
    {
      const GstStructure *structure = gst_value_get_structure (value);

      gst_encoding_profile_set_element_properties (prof,
          structure ? gst_structure_copy (structure) : NULL);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_encoding_profile_finalize (GObject * object)
{
  GstEncodingProfile *prof = (GstEncodingProfile *) object;
  g_free (prof->name);
  if (prof->format)
    gst_caps_unref (prof->format);
  g_free (prof->preset);
  g_free (prof->description);
  if (prof->restriction)
    gst_caps_unref (prof->restriction);
  g_free (prof->preset_name);

  G_OBJECT_CLASS (gst_encoding_profile_parent_class)->finalize (object);
}

static void
gst_encoding_profile_class_init (GstEncodingProfileClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_encoding_profile_finalize;

  gobject_class->set_property = _encoding_profile_set_property;
  gobject_class->get_property = _encoding_profile_get_property;

  _properties[PROP_RESTRICTION_CAPS] =
      g_param_spec_boxed ("restriction-caps", "Restriction caps",
      "The restriction caps to use", GST_TYPE_CAPS,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GstEncodingProfile:element-properties:
   *
   * A #GstStructure defining the properties to be set to the element
   * the profile represents.
   *
   * For example for `av1enc`:
   *
   * ```
   * element-properties,row-mt=true, end-usage=vbr
   * ```
   *
   * Since: 1.20
   */
  _properties[PROP_ELEMENT_PROPERTIES] =
      g_param_spec_boxed ("element-properties", "Element properties",
      "The element properties to use. "
      "Example: {properties,boolean-prop=true,string-prop=\"hi\"}.",
      GST_TYPE_STRUCTURE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, LAST_PROPERTY, _properties);
}

/**
 * gst_encoding_profile_get_name:
 * @profile: a #GstEncodingProfile
 *
 * Returns: (nullable): the name of the profile, can be %NULL.
 */
const gchar *
gst_encoding_profile_get_name (GstEncodingProfile * profile)
{
  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), NULL);

  return profile->name;
}

/**
 * gst_encoding_profile_get_description:
 * @profile: a #GstEncodingProfile
 *
 * Returns: (nullable): the description of the profile, can be %NULL.
 */
const gchar *
gst_encoding_profile_get_description (GstEncodingProfile * profile)
{
  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), NULL);

  return profile->description;
}

/**
 * gst_encoding_profile_get_format:
 * @profile: a #GstEncodingProfile
 *
 * Returns: (transfer full): (nullable): the #GstCaps corresponding to the media format used
 * in the profile. Unref after usage.
 */
GstCaps *
gst_encoding_profile_get_format (GstEncodingProfile * profile)
{
  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), NULL);

  return (profile->format ? gst_caps_ref (profile->format) : NULL);
}

/**
 * gst_encoding_profile_get_preset:
 * @profile: a #GstEncodingProfile
 *
 * Returns: (nullable): the name of the #GstPreset to be used in the profile.
 * This is the name that has been set when saving the preset.
 */
const gchar *
gst_encoding_profile_get_preset (GstEncodingProfile * profile)
{
  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), NULL);

  return profile->preset;
}

/**
 * gst_encoding_profile_get_preset_name:
 * @profile: a #GstEncodingProfile
 *
 * Returns: (nullable): the name of the #GstPreset factory to be used in the profile.
 */
const gchar *
gst_encoding_profile_get_preset_name (GstEncodingProfile * profile)
{
  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), NULL);

  return profile->preset_name;
}

/**
 * gst_encoding_profile_get_presence:
 * @profile: a #GstEncodingProfile
 *
 * Returns: The number of times the profile is used in its parent
 * container profile. If 0, it is not a mandatory stream.
 */
guint
gst_encoding_profile_get_presence (GstEncodingProfile * profile)
{
  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), 0);

  return profile->presence;
}

/**
 * gst_encoding_profile_get_enabled:
 * @profile: a #GstEncodingProfile
 *
 * Returns: Whether @profile is enabled or not
 *
 * Since: 1.6
 */
gboolean
gst_encoding_profile_is_enabled (GstEncodingProfile * profile)
{
  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), FALSE);

  return profile->enabled;
}

/**
 * gst_encoding_profile_get_restriction:
 * @profile: a #GstEncodingProfile
 *
 * Returns: (transfer full) (nullable): The restriction #GstCaps to apply before the encoder
 * that will be used in the profile. The fields present in restriction caps are
 * properties of the raw stream (that is before encoding), such as height and
 * width for video and depth and sampling rate for audio. Does not apply to
 * #GstEncodingContainerProfile (since there is no corresponding raw stream).
 * Can be %NULL. Unref after usage.
 */
GstCaps *
gst_encoding_profile_get_restriction (GstEncodingProfile * profile)
{
  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), NULL);


  return (profile->restriction ? gst_caps_ref (profile->restriction) : NULL);
}

/**
 * gst_encoding_profile_set_name:
 * @profile: a #GstEncodingProfile
 * @name: (nullable): the name to set on the profile
 *
 * Set @name as the given name for the @profile. A copy of @name will be made
 * internally.
 */
void
gst_encoding_profile_set_name (GstEncodingProfile * profile, const gchar * name)
{
  g_return_if_fail (GST_IS_ENCODING_PROFILE (profile));

  g_free (profile->name);
  profile->name = g_strdup (name);
}

/**
 * gst_encoding_profile_set_description:
 * @profile: a #GstEncodingProfile
 * @description: (nullable): the description to set on the profile
 *
 * Set @description as the given description for the @profile. A copy of
 * @description will be made internally.
 */
void
gst_encoding_profile_set_description (GstEncodingProfile * profile,
    const gchar * description)
{
  g_return_if_fail (GST_IS_ENCODING_PROFILE (profile));

  g_free (profile->description);
  profile->description = g_strdup (description);
}

/**
 * gst_encoding_profile_set_format:
 * @profile: a #GstEncodingProfile
 * @format: (transfer none): the media format to use in the profile.
 *
 * Sets the media format used in the profile.
 */
void
gst_encoding_profile_set_format (GstEncodingProfile * profile, GstCaps * format)
{
  g_return_if_fail (GST_IS_ENCODING_PROFILE (profile));

  if (profile->format)
    gst_caps_unref (profile->format);
  profile->format = gst_caps_ref (format);
}

/**
 * gst_encoding_profile_get_allow_dynamic_output:
 * @profile: a #GstEncodingProfile
 *
 * Get whether the format that has been negotiated in at some point can be renegotiated
 * later during the encoding.
 */
gboolean
gst_encoding_profile_get_allow_dynamic_output (GstEncodingProfile * profile)
{
  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), FALSE);

  return profile->allow_dynamic_output;
}

/**
 * gst_encoding_profile_set_allow_dynamic_output:
 * @profile: a #GstEncodingProfile
 * @allow_dynamic_output: Whether the format that has been negotiated first can be renegotiated
 * during the encoding
 *
 * Sets whether the format that has been negotiated in at some point can be renegotiated
 * later during the encoding.
 */
void
gst_encoding_profile_set_allow_dynamic_output (GstEncodingProfile * profile,
    gboolean allow_dynamic_output)
{
  g_return_if_fail (GST_IS_ENCODING_PROFILE (profile));

  profile->allow_dynamic_output = allow_dynamic_output;
}

/**
 * gst_encoding_profile_get_single_segment:
 * @profile: a #GstEncodingProfile
 *
 * Returns: #TRUE if the stream represented by @profile should use a single
 * segment before the encoder, #FALSE otherwise. This means that buffers will be retimestamped
 * and segments will be eat so as to appear as one segment.
 *
 * Since: 1.18
 */
gboolean
gst_encoding_profile_get_single_segment (GstEncodingProfile * profile)
{
  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), FALSE);

  return profile->single_segment;
}

/**
 * gst_encoding_profile_set_single_segment:
 * @profile: a #GstEncodingProfile
 * @single_segment: #TRUE if the stream represented by @profile should use a
 * single segment before the encoder, #FALSE otherwise.
 *
 * If using a single segment, buffers will be retimestamped and segments will be
 * eat so as to appear as one segment.
 *
 * > *NOTE*: Single segment is not property supported when using
 * > #encodebin:avoid-reencoding
 *
 * Since: 1.18
 */
void
gst_encoding_profile_set_single_segment (GstEncodingProfile * profile,
    gboolean single_segment)
{
  g_return_if_fail (GST_IS_ENCODING_PROFILE (profile));

  profile->single_segment = single_segment;
}

/**
 * gst_encoding_profile_set_preset:
 * @profile: a #GstEncodingProfile
 * @preset: (nullable): the element preset to use
 *
 * Sets the name of the #GstElement that implements the #GstPreset interface
 * to use for the profile.
 * This is the name that has been set when saving the preset.
 */
void
gst_encoding_profile_set_preset (GstEncodingProfile * profile,
    const gchar * preset)
{
  g_return_if_fail (GST_IS_ENCODING_PROFILE (profile));

  g_free (profile->preset);
  profile->preset = g_strdup (preset);
}

/**
 * gst_encoding_profile_set_preset_name:
 * @profile: a #GstEncodingProfile
 * @preset_name: (nullable): The name of the preset to use in this @profile.
 *
 * Sets the name of the #GstPreset's factory to be used in the profile.
 */
void
gst_encoding_profile_set_preset_name (GstEncodingProfile * profile,
    const gchar * preset_name)
{
  g_return_if_fail (GST_IS_ENCODING_PROFILE (profile));

  g_free (profile->preset_name);
  profile->preset_name = g_strdup (preset_name);
}

/**
 * gst_encoding_profile_set_presence:
 * @profile: a #GstEncodingProfile
 * @presence: the number of time the profile can be used
 *
 * Set the number of time the profile is used in its parent
 * container profile. If 0, it is not a mandatory stream
 */
void
gst_encoding_profile_set_presence (GstEncodingProfile * profile, guint presence)
{
  g_return_if_fail (GST_IS_ENCODING_PROFILE (profile));

  profile->presence = presence;
}

/**
 * gst_encoding_profile_set_enabled:
 * @profile: a #GstEncodingProfile
 * @enabled: %FALSE to disable @profile, %TRUE to enable it
 *
 * Set whether the profile should be used or not.
 *
 * Since: 1.6
 */
void
gst_encoding_profile_set_enabled (GstEncodingProfile * profile,
    gboolean enabled)
{
  g_return_if_fail (GST_IS_ENCODING_PROFILE (profile));

  profile->enabled = enabled;
}

/**
 * gst_encoding_profile_set_restriction:
 * @profile: a #GstEncodingProfile
 * @restriction: (nullable) (transfer full): the restriction to apply
 *
 * Set the restriction #GstCaps to apply before the encoder
 * that will be used in the profile. See gst_encoding_profile_get_restriction()
 * for more about restrictions. Does not apply to #GstEncodingContainerProfile.
 */
void
gst_encoding_profile_set_restriction (GstEncodingProfile * profile,
    GstCaps * restriction)
{
  g_return_if_fail (restriction == NULL || GST_IS_CAPS (restriction));
  g_return_if_fail (GST_IS_ENCODING_PROFILE (profile));

  if (profile->restriction)
    gst_caps_unref (profile->restriction);
  profile->restriction = restriction;

  g_object_notify_by_pspec (G_OBJECT (profile),
      _properties[PROP_RESTRICTION_CAPS]);
}

/**
 * gst_encoding_profile_set_element_properties:
 * @self: a #GstEncodingProfile
 * @element_properties: (transfer full): A #GstStructure defining the properties
 * to be set to the element the profile represents.
 *
 * This allows setting the muxing/encoding element properties.
 *
 * **Set properties generically**
 *
 * ``` properties
 *  [element-properties, boolean-prop=true, string-prop="hi"]
 * ```
 *
 * **Mapping properties with well known element factories**
 *
 * ``` properties
 * element-properties-map, map = {
 *      [openh264enc, gop-size=32, ],
 *      [x264enc, key-int-max=32, tune=zerolatency],
 *  }
 * ```
 *
 * Since: 1.20
 */
void
gst_encoding_profile_set_element_properties (GstEncodingProfile * self,
    GstStructure * element_properties)
{
  g_return_if_fail (GST_IS_ENCODING_PROFILE (self));
  g_return_if_fail (!element_properties
      || GST_IS_STRUCTURE (element_properties));

#ifndef G_DISABLE_CHECKS
  if (element_properties &&
      (gst_structure_has_name (element_properties, "element-properties-map")
          || gst_structure_has_name (element_properties, "properties-map")
          || gst_structure_has_name (element_properties, "map")))
    g_return_if_fail (gst_structure_has_field_typed (element_properties, "map",
            GST_TYPE_LIST));
#endif

  PROFILE_LOCK (self);
  if (self->element_properties)
    gst_structure_free (self->element_properties);
  if (element_properties)
    self->element_properties = element_properties;
  else
    self->element_properties = NULL;
  PROFILE_UNLOCK (self);

  g_object_notify_by_pspec (G_OBJECT (self),
      _properties[PROP_ELEMENT_PROPERTIES]);
}

/**
 * gst_encoding_profile_get_element_properties:
 * @self: a #GstEncodingProfile
 *
 * Returns: (transfer full) (nullable): The properties that are going to be set on the underlying element
 *
 * Since: 1.20
 */
GstStructure *
gst_encoding_profile_get_element_properties (GstEncodingProfile * self)
{
  GstStructure *res = NULL;

  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (self), NULL);

  PROFILE_LOCK (self);
  if (self->element_properties)
    res = gst_structure_copy (self->element_properties);
  PROFILE_UNLOCK (self);

  return res;
}

/* Container profiles */

struct _GstEncodingContainerProfile
{
  GstEncodingProfile parent;

  GList *encodingprofiles;
};

struct _GstEncodingContainerProfileClass
{
  GstEncodingProfileClass parent;
};

G_DEFINE_TYPE (GstEncodingContainerProfile, gst_encoding_container_profile,
    GST_TYPE_ENCODING_PROFILE);

static void
gst_encoding_container_profile_init (GstEncodingContainerProfile * prof)
{
  /* Nothing to initialize */
}

static void
gst_encoding_container_profile_finalize (GObject * object)
{
  GstEncodingContainerProfile *prof = (GstEncodingContainerProfile *) object;

  g_list_foreach (prof->encodingprofiles, (GFunc) g_object_unref, NULL);
  g_list_free (prof->encodingprofiles);

  G_OBJECT_CLASS (gst_encoding_container_profile_parent_class)->finalize
      ((GObject *) prof);
}

static void
gst_encoding_container_profile_copy (GstEncodingProfile * profile,
    GstEncodingProfile * copy_profile)
{
  GstEncodingContainerProfile *self = GST_ENCODING_CONTAINER_PROFILE (profile),
      *copy = GST_ENCODING_CONTAINER_PROFILE (copy_profile);
  GList *tmp;

  for (tmp = self->encodingprofiles; tmp; tmp = tmp->next) {
    gst_encoding_container_profile_add_profile (copy,
        gst_encoding_profile_copy (tmp->data));
  }
}

static void
gst_encoding_container_profile_class_init (GstEncodingContainerProfileClass * k)
{
  GObjectClass *gobject_class = (GObjectClass *) k;

  gobject_class->finalize = gst_encoding_container_profile_finalize;

  ((GstEncodingProfileClass *) k)->copy = gst_encoding_container_profile_copy;
}

/**
 * gst_encoding_container_profile_get_profiles:
 * @profile: a #GstEncodingContainerProfile
 *
 * Returns: (element-type GstPbutils.EncodingProfile) (transfer none):
 * the list of contained #GstEncodingProfile.
 */
const GList *
gst_encoding_container_profile_get_profiles (GstEncodingContainerProfile *
    profile)
{
  g_return_val_if_fail (GST_IS_ENCODING_CONTAINER_PROFILE (profile), NULL);

  return profile->encodingprofiles;
}

/* Video profiles */

struct _GstEncodingVideoProfile
{
  GstEncodingProfile parent;

  guint pass;
  gboolean variableframerate;
};

struct _GstEncodingVideoProfileClass
{
  GstEncodingProfileClass parent;
};

G_DEFINE_TYPE (GstEncodingVideoProfile, gst_encoding_video_profile,
    GST_TYPE_ENCODING_PROFILE);

static void
gst_encoding_video_profile_copy (GstEncodingProfile * profile,
    GstEncodingProfile * copy_profile)
{
  GstEncodingVideoProfile *self = GST_ENCODING_VIDEO_PROFILE (profile),
      *copy = GST_ENCODING_VIDEO_PROFILE (copy_profile);

  copy->pass = self->pass;
  copy->variableframerate = self->variableframerate;
}

static void
gst_encoding_video_profile_init (GstEncodingVideoProfile * prof)
{
  /* Nothing to initialize */
}

static void
gst_encoding_video_profile_class_init (GstEncodingVideoProfileClass * klass)
{
  ((GstEncodingProfileClass *) klass)->copy = gst_encoding_video_profile_copy;
}

/**
 * gst_encoding_video_profile_get_pass:
 * @prof: a #GstEncodingVideoProfile
 *
 * Get the pass number if this is part of a multi-pass profile.
 *
 * Returns: The pass number. Starts at 1 for multi-pass. 0 if this is
 * not a multi-pass profile
 */
guint
gst_encoding_video_profile_get_pass (GstEncodingVideoProfile * prof)
{
  g_return_val_if_fail (GST_IS_ENCODING_VIDEO_PROFILE (prof), 0);

  return prof->pass;
}

/**
 * gst_encoding_video_profile_get_variableframerate:
 * @prof: a #GstEncodingVideoProfile
 *
 * > *NOTE*: Fixed framerate won't be enforced when #encodebin:avoid-reencoding
 * > is set.
 *
 * Returns: Whether non-constant video framerate is allowed for encoding.
 */
gboolean
gst_encoding_video_profile_get_variableframerate (GstEncodingVideoProfile *
    prof)
{
  g_return_val_if_fail (GST_IS_ENCODING_VIDEO_PROFILE (prof), FALSE);

  return prof->variableframerate;
}

/**
 * gst_encoding_video_profile_set_pass:
 * @prof: a #GstEncodingVideoProfile
 * @pass: the pass number for this profile
 *
 * Sets the pass number of this video profile. The first pass profile should have
 * this value set to 1. If this video profile isn't part of a multi-pass profile,
 * you may set it to 0 (the default value).
 */
void
gst_encoding_video_profile_set_pass (GstEncodingVideoProfile * prof, guint pass)
{
  g_return_if_fail (GST_IS_ENCODING_VIDEO_PROFILE (prof));

  prof->pass = pass;
}

/**
 * gst_encoding_video_profile_set_variableframerate:
 * @prof: a #GstEncodingVideoProfile
 * @variableframerate: a boolean
 *
 * If set to %TRUE, then the incoming stream will be allowed to have non-constant
 * framerate. If set to %FALSE (default value), then the incoming stream will
 * be normalized by dropping/duplicating frames in order to produce a
 * constance framerate.
 */
void
gst_encoding_video_profile_set_variableframerate (GstEncodingVideoProfile *
    prof, gboolean variableframerate)
{
  g_return_if_fail (GST_IS_ENCODING_VIDEO_PROFILE (prof));

  prof->variableframerate = variableframerate;
}

/* Audio profiles */

struct _GstEncodingAudioProfile
{
  GstEncodingProfile parent;
};

struct _GstEncodingAudioProfileClass
{
  GstEncodingProfileClass parent;
};

G_DEFINE_TYPE (GstEncodingAudioProfile, gst_encoding_audio_profile,
    GST_TYPE_ENCODING_PROFILE);

static void
gst_encoding_audio_profile_init (GstEncodingAudioProfile * prof)
{
  /* Nothing to initialize */
}

static void
gst_encoding_audio_profile_class_init (GstEncodingAudioProfileClass * klass)
{
}

static inline gboolean
_gst_caps_is_equal_safe (GstCaps * a, GstCaps * b)
{
  if (a == b)
    return TRUE;
  if ((a == NULL) || (b == NULL))
    return FALSE;
  return gst_caps_is_equal (a, b);
}

static gint
_compare_container_encoding_profiles (GstEncodingContainerProfile * ca,
    GstEncodingContainerProfile * cb)
{
  GList *tmp;

  if (g_list_length (ca->encodingprofiles) !=
      g_list_length (cb->encodingprofiles))
    return -1;

  for (tmp = ca->encodingprofiles; tmp; tmp = tmp->next) {
    GstEncodingProfile *prof = (GstEncodingProfile *) tmp->data;
    if (!gst_encoding_container_profile_contains_profile (ca, prof))
      return -1;
  }

  return 0;
}

static gint
_compare_encoding_profiles (const GstEncodingProfile * a,
    const GstEncodingProfile * b)
{
  if ((G_TYPE_FROM_INSTANCE (a) != G_TYPE_FROM_INSTANCE (b)) ||
      !_gst_caps_is_equal_safe (a->format, b->format) ||
      (g_strcmp0 (a->preset, b->preset) != 0) ||
      (g_strcmp0 (a->preset_name, b->preset_name) != 0) ||
      (g_strcmp0 (a->name, b->name) != 0) ||
      (g_strcmp0 (a->description, b->description) != 0))
    return -1;

  if (GST_IS_ENCODING_CONTAINER_PROFILE (a))
    return
        _compare_container_encoding_profiles (GST_ENCODING_CONTAINER_PROFILE
        (a), GST_ENCODING_CONTAINER_PROFILE (b));

  if (GST_IS_ENCODING_VIDEO_PROFILE (a)) {
    GstEncodingVideoProfile *va = (GstEncodingVideoProfile *) a;
    GstEncodingVideoProfile *vb = (GstEncodingVideoProfile *) b;

    if ((va->pass != vb->pass)
        || (va->variableframerate != vb->variableframerate))
      return -1;
  }

  return 0;
}

/**
 * gst_encoding_container_profile_contains_profile:
 * @container: a #GstEncodingContainerProfile
 * @profile: a #GstEncodingProfile
 *
 * Checks if @container contains a #GstEncodingProfile identical to
 * @profile.
 *
 * Returns: %TRUE if @container contains a #GstEncodingProfile identical
 * to @profile, else %FALSE.
 */
gboolean
gst_encoding_container_profile_contains_profile (GstEncodingContainerProfile *
    container, GstEncodingProfile * profile)
{
  g_return_val_if_fail (GST_IS_ENCODING_CONTAINER_PROFILE (container), FALSE);
  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), FALSE);

  return (g_list_find_custom (container->encodingprofiles, profile,
          (GCompareFunc) _compare_encoding_profiles) != NULL);
}

/**
 * gst_encoding_container_profile_add_profile:
 * @container: the #GstEncodingContainerProfile to use
 * @profile: (transfer full): the #GstEncodingProfile to add.
 *
 * Add a #GstEncodingProfile to the list of profiles handled by @container.
 *
 * No copy of @profile will be made, if you wish to use it elsewhere after this
 * method you should increment its reference count.
 *
 * Returns: %TRUE if the @stream was properly added, else %FALSE.
 */
gboolean
gst_encoding_container_profile_add_profile (GstEncodingContainerProfile *
    container, GstEncodingProfile * profile)
{
  g_return_val_if_fail (GST_IS_ENCODING_CONTAINER_PROFILE (container), FALSE);
  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), FALSE);

  container->encodingprofiles =
      g_list_append (container->encodingprofiles, profile);

  return TRUE;
}

static GstEncodingProfile *
common_creation (GType objtype, GstCaps * format, const gchar * preset,
    const gchar * name, const gchar * description, GstCaps * restriction,
    guint presence)
{
  GstEncodingProfile *prof;

  prof = (GstEncodingProfile *) g_object_new (objtype, NULL);

  if (name)
    prof->name = g_strdup (name);
  if (description)
    prof->description = g_strdup (description);
  if (preset)
    prof->preset = g_strdup (preset);
  if (format)
    prof->format = gst_caps_ref (format);
  if (restriction)
    prof->restriction = gst_caps_ref (restriction);
  prof->presence = presence;
  prof->preset_name = NULL;
  prof->allow_dynamic_output = TRUE;
  prof->enabled = TRUE;

  return prof;
}

/**
 * gst_encoding_container_profile_new:
 * @name: (nullable): The name of the container profile, can be %NULL
 * @description: (nullable): The description of the container profile,
 *     can be %NULL
 * @format: (transfer none): The format to use for this profile
 * @preset: (nullable): The preset to use for this profile.
 *
 * Creates a new #GstEncodingContainerProfile.
 *
 * Returns: The newly created #GstEncodingContainerProfile.
 */
GstEncodingContainerProfile *
gst_encoding_container_profile_new (const gchar * name,
    const gchar * description, GstCaps * format, const gchar * preset)
{
  g_return_val_if_fail (GST_IS_CAPS (format), NULL);

  return (GstEncodingContainerProfile *)
      common_creation (GST_TYPE_ENCODING_CONTAINER_PROFILE, format, preset,
      name, description, NULL, 0);
}

/**
 * gst_encoding_video_profile_new:
 * @format: (transfer none): the #GstCaps
 * @preset: (nullable): the preset(s) to use on the encoder, can be %NULL
 * @restriction: (nullable): the #GstCaps used to restrict the input to the encoder, can be
 * NULL. See gst_encoding_profile_get_restriction() for more details.
 * @presence: the number of time this stream must be used. 0 means any number of
 *  times (including never)
 *
 * Creates a new #GstEncodingVideoProfile
 *
 * All provided allocatable arguments will be internally copied, so can be
 * safely freed/unreferenced after calling this method.
 *
 * If you wish to control the pass number (in case of multi-pass scenarios),
 * please refer to the gst_encoding_video_profile_set_pass() documentation.
 *
 * If you wish to use/force a constant framerate please refer to the
 * gst_encoding_video_profile_set_variableframerate() documentation.
 *
 * Returns: the newly created #GstEncodingVideoProfile.
 */
GstEncodingVideoProfile *
gst_encoding_video_profile_new (GstCaps * format, const gchar * preset,
    GstCaps * restriction, guint presence)
{
  return (GstEncodingVideoProfile *)
      common_creation (GST_TYPE_ENCODING_VIDEO_PROFILE, format, preset, NULL,
      NULL, restriction, presence);
}

/**
 * gst_encoding_audio_profile_new:
 * @format: (transfer none): the #GstCaps
 * @preset: (nullable): the preset(s) to use on the encoder, can be %NULL
 * @restriction: (nullable): the #GstCaps used to restrict the input to the encoder, can be
 * NULL. See gst_encoding_profile_get_restriction() for more details.
 * @presence: the number of time this stream must be used. 0 means any number of
 *  times (including never)
 *
 * Creates a new #GstEncodingAudioProfile
 *
 * All provided allocatable arguments will be internally copied, so can be
 * safely freed/unreferenced after calling this method.
 *
 * Returns: the newly created #GstEncodingAudioProfile.
 */
GstEncodingAudioProfile *
gst_encoding_audio_profile_new (GstCaps * format, const gchar * preset,
    GstCaps * restriction, guint presence)
{
  return (GstEncodingAudioProfile *)
      common_creation (GST_TYPE_ENCODING_AUDIO_PROFILE, format, preset, NULL,
      NULL, restriction, presence);
}


/**
 * gst_encoding_profile_is_equal:
 * @a: a #GstEncodingProfile
 * @b: a #GstEncodingProfile
 *
 * Checks whether the two #GstEncodingProfile are equal
 *
 * Returns: %TRUE if @a and @b are equal, else %FALSE.
 */
gboolean
gst_encoding_profile_is_equal (GstEncodingProfile * a, GstEncodingProfile * b)
{
  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (a), FALSE);
  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (b), FALSE);

  return (_compare_encoding_profiles (a, b) == 0);
}


/**
 * gst_encoding_profile_get_input_caps:
 * @profile: a #GstEncodingProfile
 *
 * Computes the full output caps that this @profile will be able to consume.
 *
 * Returns: (transfer full): The full caps the given @profile can consume. Call
 * gst_caps_unref() when you are done with the caps.
 */
GstCaps *
gst_encoding_profile_get_input_caps (GstEncodingProfile * profile)
{
  GstCaps *out, *tmp;
  GList *ltmp;
  GstStructure *st, *outst;
  GQuark out_name;
  guint i, len;
  GstCaps *fcaps;

  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), NULL);

  if (GST_IS_ENCODING_CONTAINER_PROFILE (profile)) {
    GstCaps *res = gst_caps_new_empty ();

    for (ltmp = GST_ENCODING_CONTAINER_PROFILE (profile)->encodingprofiles;
        ltmp; ltmp = ltmp->next) {
      GstEncodingProfile *sprof = (GstEncodingProfile *) ltmp->data;
      res = gst_caps_merge (res, gst_encoding_profile_get_input_caps (sprof));
    }
    return res;
  }

  fcaps = profile->format;

  /* fast-path */
  if ((profile->restriction == NULL) || gst_caps_is_any (profile->restriction))
    return gst_caps_ref (fcaps);

  /* Combine the format with the restriction caps */
  outst = gst_caps_get_structure (fcaps, 0);
  out_name = gst_structure_get_name_id (outst);
  tmp = gst_caps_new_empty ();
  len = gst_caps_get_size (profile->restriction);

  for (i = 0; i < len; i++) {
    st = gst_structure_copy (gst_caps_get_structure (profile->restriction, i));
    st->name = out_name;
    gst_caps_append_structure (tmp, st);
  }

  out = gst_caps_intersect (tmp, fcaps);
  gst_caps_unref (tmp);

  return out;
}

/**
 * gst_encoding_profile_get_type_nick:
 * @profile: a #GstEncodingProfile
 *
 * Returns: the human-readable name of the type of @profile.
 */
const gchar *
gst_encoding_profile_get_type_nick (GstEncodingProfile * profile)
{
  if (GST_IS_ENCODING_CONTAINER_PROFILE (profile))
    return "container";
  if (GST_IS_ENCODING_VIDEO_PROFILE (profile))
    return "video";
  if (GST_IS_ENCODING_AUDIO_PROFILE (profile))
    return "audio";

  g_assert_not_reached ();
  return NULL;
}

extern const gchar *pb_utils_get_file_extension_from_caps (const GstCaps *
    caps);
gboolean pb_utils_is_tag (const GstCaps * caps);

static gboolean
gst_encoding_profile_has_format (GstEncodingProfile * profile,
    const gchar * media_type)
{
  GstCaps *caps;
  gboolean ret;

  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), FALSE);

  caps = gst_encoding_profile_get_format (profile);
  ret = gst_structure_has_name (gst_caps_get_structure (caps, 0), media_type);
  gst_caps_unref (caps);

  return ret;
}

static gboolean
gst_encoding_container_profile_has_video (GstEncodingContainerProfile * profile)
{
  const GList *l;

  g_return_val_if_fail (GST_IS_ENCODING_CONTAINER_PROFILE (profile), FALSE);

  for (l = profile->encodingprofiles; l != NULL; l = l->next) {
    if (GST_IS_ENCODING_VIDEO_PROFILE (l->data))
      return TRUE;
    if (GST_IS_ENCODING_CONTAINER_PROFILE (l->data) &&
        gst_encoding_container_profile_has_video (l->data))
      return TRUE;
  }

  return FALSE;
}

/**
 * gst_encoding_profile_get_file_extension:
 * @profile: a #GstEncodingProfile
 *
 * Returns: (nullable): a suitable file extension for @profile, or NULL.
 */
const gchar *
gst_encoding_profile_get_file_extension (GstEncodingProfile * profile)
{
  GstEncodingContainerProfile *cprofile;
  const gchar *ext = NULL;
  gboolean has_video;
  GstCaps *caps;
  guint num_children;

  g_return_val_if_fail (GST_IS_ENCODING_PROFILE (profile), NULL);

  caps = gst_encoding_profile_get_format (profile);
  ext = pb_utils_get_file_extension_from_caps (caps);

  if (!GST_IS_ENCODING_CONTAINER_PROFILE (profile))
    goto done;

  cprofile = GST_ENCODING_CONTAINER_PROFILE (profile);

  num_children = g_list_length (cprofile->encodingprofiles);

  /* if it's a tag container profile (e.g. id3mux/apemux), we need
   * to look at what's inside it */
  if (pb_utils_is_tag (caps)) {
    GST_DEBUG ("tag container profile");
    if (num_children == 1) {
      GstEncodingProfile *child_profile = cprofile->encodingprofiles->data;

      ext = gst_encoding_profile_get_file_extension (child_profile);
    } else {
      GST_WARNING ("expected exactly one child profile with tag profile");
    }
    goto done;
  }

  if (num_children == 0)
    goto done;

  /* special cases */
  has_video = gst_encoding_container_profile_has_video (cprofile);

  /* Ogg */
  if (g_strcmp0 (ext, "ogg") == 0) {
    /* ogg with video => .ogv */
    if (has_video) {
      ext = "ogv";
      goto done;
    }
    /* ogg with just speex audio => .spx */
    if (num_children == 1) {
      GstEncodingProfile *child_profile = cprofile->encodingprofiles->data;

      if (GST_IS_ENCODING_AUDIO_PROFILE (child_profile) &&
          gst_encoding_profile_has_format (child_profile, "audio/x-speex")) {
        ext = "spx";
        goto done;
      }
    }
    /* does anyone actually use .oga for ogg audio files? */
    goto done;
  }

  /* Matroska */
  if (has_video && g_strcmp0 (ext, "mka") == 0) {
    ext = "mkv";
    goto done;
  }

  /* Windows Media / ASF */
  if (gst_encoding_profile_has_format (profile, "video/x-ms-asf")) {
    const GList *l;
    guint num_wmv = 0, num_wma = 0, num_other = 0;

    for (l = cprofile->encodingprofiles; l != NULL; l = l->next) {
      if (gst_encoding_profile_has_format (l->data, "video/x-wmv"))
        ++num_wmv;
      else if (gst_encoding_profile_has_format (l->data, "audio/x-wma"))
        ++num_wma;
      else
        ++num_other;
    }

    if (num_other > 0)
      ext = "asf";
    else if (num_wmv > 0)
      ext = "wmv";
    else if (num_wma > 0)
      ext = "wma";

    goto done;
  }

done:

  GST_INFO ("caps %" GST_PTR_FORMAT ", ext: %s", caps, GST_STR_NULL (ext));
  gst_caps_unref (caps);
  return ext;
}

/**
 * gst_encoding_profile_find:
 * @targetname: (transfer none): The name of the target
 * @profilename: (transfer none) (nullable): The name of the profile, if %NULL
 * provided, it will default to the encoding profile called `default`.
 * @category: (transfer none) (nullable): The target category. Can be %NULL
 *
 * Find the #GstEncodingProfile with the specified name and category.
 *
 * Returns: (transfer full) (nullable): The matching #GstEncodingProfile or %NULL.
 */
GstEncodingProfile *
gst_encoding_profile_find (const gchar * targetname, const gchar * profilename,
    const gchar * category)
{
  GstEncodingProfile *res = NULL;
  GstEncodingTarget *target;

  g_return_val_if_fail (targetname != NULL, NULL);

  target = gst_encoding_target_load (targetname, category, NULL);
  if (target) {
    res =
        gst_encoding_target_get_profile (target,
        profilename ? profilename : "default");
    gst_encoding_target_unref (target);
  }

  return res;
}

static GstEncodingProfile *
combo_search (const gchar * pname)
{
  GstEncodingProfile *res = NULL;
  gchar **split;
  gint split_length;

  /* Splitup */
  split = g_strsplit (pname, "/", 3);
  split_length = g_strv_length (split);
  if (split_length > 3)
    goto done;

  res = gst_encoding_profile_find (split[0],
      split_length == 2 ? split[1] : NULL, split_length == 3 ? split[2] : NULL);


done:
  g_strfreev (split);

  return res;
}

static GstCaps *
get_profile_format_from_possible_factory_name (const gchar * factory_desc,
    gchar ** new_factory_name, GstCaps ** restrictions,
    gboolean * is_rendering_muxer)
{
  GList *tmp;
  GstCaps *caps = NULL, *tmpcaps = gst_caps_from_string (factory_desc);
  GstStructure *tmpstruct;
  GstElementFactory *fact = NULL;

  if (is_rendering_muxer)
    *is_rendering_muxer = FALSE;
  *new_factory_name = NULL;
  if (gst_caps_get_size (tmpcaps) != 1)
    goto done;

  tmpstruct = gst_caps_get_structure (tmpcaps, 0);
  fact = gst_element_factory_find (gst_structure_get_name (tmpstruct));
  if (!fact)
    goto done;

  if (!gst_element_factory_list_is_type (fact,
          GST_ELEMENT_FACTORY_TYPE_ENCODER | GST_ELEMENT_FACTORY_TYPE_MUXER)) {
    GST_ERROR_OBJECT (fact,
        "is not an encoder or muxer, it can't be"
        " used in an encoding profile.");
    goto done;
  }

  for (tmp = (GList *) gst_element_factory_get_static_pad_templates (fact);
      tmp; tmp = tmp->next) {
    GstStaticPadTemplate *templ = ((GstStaticPadTemplate *) tmp->data);

    if (templ->direction == GST_PAD_SRC) {
      GstCaps *tmpcaps = gst_static_caps_get (&templ->static_caps);

      if (gst_caps_get_size (tmpcaps) > 0)
        caps =
            gst_caps_new_empty_simple (gst_structure_get_name
            (gst_caps_get_structure (tmpcaps, 0)));

      gst_caps_unref (tmpcaps);
      if (caps)
        break;
    }
  }

  if (caps) {
    *new_factory_name = g_strdup (gst_structure_get_name (tmpstruct));

    if (gst_structure_n_fields (tmpstruct) && restrictions) {
      const gchar *sname =
          gst_structure_get_name (gst_caps_get_structure (caps, 0));

      if (g_str_has_prefix (sname, "audio/"))
        gst_structure_set_name (tmpstruct, "audio/x-raw");
      else if (g_str_has_prefix (sname, "video/") ||
          g_str_has_prefix (sname, "image/"))
        gst_structure_set_name (tmpstruct, "video/x-raw");

      *restrictions = tmpcaps;
      tmpcaps = NULL;
    }
  } else if (gst_element_factory_list_is_type (fact,
          GST_ELEMENT_FACTORY_TYPE_MUXER)) {
    *new_factory_name = g_strdup (gst_structure_get_name (tmpstruct));

    caps = gst_caps_ref (gst_caps_new_empty ());
    if (is_rendering_muxer)
      *is_rendering_muxer = TRUE;
  }


done:
  if (fact)
    gst_object_unref (fact);

  if (tmpcaps)
    gst_caps_unref (tmpcaps);

  return caps;
}

static GstEncodingProfile *
create_encoding_profile_from_caps (GstCaps * caps, gchar * preset_name,
    GstCaps * restrictioncaps, gint presence, gboolean single_segment,
    gchar * factory_name, GList * muxers_and_encoders, GstCaps * raw_audio_caps,
    GstCaps * raw_video_caps, gboolean is_rendering_muxer)
{
  GstEncodingProfile *profile = NULL;
  GList *factories = NULL;
  gboolean is_raw_audio = FALSE, is_raw_video = FALSE;

  if (is_rendering_muxer) {
    profile =
        GST_ENCODING_PROFILE (gst_encoding_container_profile_new
        ("User profile", "User profile", caps, NULL));
    goto done;
  }

  if (gst_caps_can_intersect (raw_audio_caps, caps)) {
    is_raw_audio = TRUE;
  } else if (gst_caps_can_intersect (raw_video_caps, caps)) {
    is_raw_video = TRUE;
  } else {
    factories = gst_element_factory_list_filter (muxers_and_encoders, caps,
        GST_PAD_SRC, FALSE);

    if (!factories) {
      GST_INFO ("Could not find factory for %" GST_PTR_FORMAT, caps);
      return NULL;
    }
  }

  if (is_raw_audio || (factories
          && gst_element_factory_list_is_type (factories->data,
              GST_ELEMENT_FACTORY_TYPE_AUDIO_ENCODER)))
    profile =
        GST_ENCODING_PROFILE (gst_encoding_audio_profile_new (caps, preset_name,
            restrictioncaps, presence));
  else if (is_raw_video || (factories
          && gst_element_factory_list_is_type (factories->data,
              GST_ELEMENT_FACTORY_TYPE_VIDEO_ENCODER)))
    profile =
        GST_ENCODING_PROFILE (gst_encoding_video_profile_new (caps, preset_name,
            restrictioncaps, presence));
  else if (gst_element_factory_list_is_type (factories->data,
          GST_ELEMENT_FACTORY_TYPE_MUXER))
    profile =
        GST_ENCODING_PROFILE (gst_encoding_container_profile_new
        ("User profile", "User profile", caps, NULL));

  if (factories)
    gst_plugin_feature_list_free (factories);

done:
  if (factory_name && profile)
    gst_encoding_profile_set_preset_name (profile, factory_name);
  gst_encoding_profile_set_single_segment (profile, single_segment);

  g_free (factory_name);

  return profile;
}

static gboolean
gst_structure_validate_name (const gchar * name)
{
  const gchar *s;

  g_return_val_if_fail (name != NULL, FALSE);

  if (G_UNLIKELY (!g_ascii_isalpha (*name)))
    return FALSE;

  /* FIXME: test name string more */
  s = &name[1];
  while (*s && (g_ascii_isalnum (*s) || strchr ("/-_.:+", *s) != NULL))
    s++;

  if (*s == ',')
    return TRUE;

  if (G_UNLIKELY (*s != '\0'))
    return FALSE;

  return TRUE;
}

static GstEncodingProfile *
create_encoding_stream_profile (gchar * serialized_profile,
    GList * muxers_and_encoders, GstCaps * raw_audio_caps,
    GstCaps * raw_video_caps)
{
  GstCaps *caps;
  guint presence = 0;
  gboolean single_segment = FALSE;
  gchar *strcaps = NULL, *strpresence, **strprops_v =
      NULL, **restriction_format, **preset_v = NULL, *preset_name =
      NULL, *factory_name = NULL, *variable_framerate = NULL;
  GstStructure *element_properties = NULL;
  GstCaps *restrictioncaps = NULL;
  GstEncodingProfile *profile = NULL;

  restriction_format = g_strsplit (serialized_profile, "->", 0);
  if (restriction_format[1]) {
    restrictioncaps = gst_caps_from_string (restriction_format[0]);
    strcaps = g_strdup (restriction_format[1]);
  } else {
    restrictioncaps = NULL;
    strcaps = g_strdup (restriction_format[0]);
  }
  g_strfreev (restriction_format);

  preset_v = g_strsplit (strcaps, "+", 0);
  if (preset_v[1]) {
    strpresence = preset_v[1];
    g_free (strcaps);
    strcaps = g_strdup (preset_v[0]);
  } else {
    strpresence = preset_v[0];
  }

  strprops_v = g_strsplit (strpresence, "|", 0);
  if (strprops_v[1]) {          /* We have a properties */
    gchar *endptr;
    guint propi;

    if (preset_v[1]) {          /* We have preset and properties */
      preset_name = g_strdup (strprops_v[0]);
    } else {                    /* We have a properties but no preset */
      g_free (strcaps);
      strcaps = g_strdup (strprops_v[0]);
    }

    for (propi = 1; strprops_v[propi]; propi++) {
      gchar **propv;
      gchar *presence_str = NULL;
      gchar *prop = strprops_v[propi];
      GstStructure *tmpstruct = NULL;

      if (gst_structure_validate_name (prop))
        tmpstruct = gst_structure_new_from_string (prop);
      if (tmpstruct) {
        if (element_properties)
          gst_structure_free (element_properties);

        element_properties = tmpstruct;

        continue;
      }

      propv = g_strsplit (prop, "=", -1);
      if (propv[1] && propv[2]) {
        g_warning ("Wrong format for property: %s, only 1 `=` is expected",
            prop);
        g_strfreev (propv);
        goto cleanup;
      }

      if (!propv[1]) {
        presence_str = propv[0];
      } else if (!g_strcmp0 (propv[0], "presence")) {
        presence_str = propv[1];
      } else if (!g_strcmp0 (propv[0], "variable-framerate")) {
        variable_framerate = g_strdup (propv[1]);
      } else if (!g_strcmp0 (propv[0], "single-segment")) {
        GValue v = G_VALUE_INIT;

        g_value_init (&v, G_TYPE_BOOLEAN);
        if (!gst_value_deserialize (&v, propv[1])) {
          g_warning ("Invalid value for property 'single-segment': %s",
              propv[1]);
          g_strfreev (propv);
          goto cleanup;
        }

        single_segment = g_value_get_boolean (&v);
        g_value_reset (&v);
      } else {
        g_warning ("Unsupported property: %s", propv[0]);
        g_strfreev (propv);
        goto cleanup;
      }

      if (presence_str) {
        presence = g_ascii_strtoll (presence_str, &endptr, 10);

        if (endptr == strprops_v[1]) {
          g_warning ("Wrong presence %s", presence_str);
          g_strfreev (propv);
          goto cleanup;
        }
      }
      g_strfreev (propv);
    }
  } else {                      /* We have no presence */
    if (preset_v[1]) {          /* Not presence but preset */
      preset_name = g_strdup (preset_v[1]);
      g_free (strcaps);
      strcaps = g_strdup (preset_v[0]);
    }                           /* Else we have no presence nor preset */
  }

  GST_DEBUG ("Creating preset with restrictions: %" GST_PTR_FORMAT
      ", caps: %s, preset %s, presence %d", restrictioncaps, strcaps,
      preset_name ? preset_name : "none", presence);

  caps = gst_caps_from_string (strcaps);
  if (caps) {
    profile = create_encoding_profile_from_caps (caps, preset_name,
        restrictioncaps, presence, single_segment, NULL, muxers_and_encoders,
        raw_audio_caps, raw_video_caps, FALSE);
    gst_caps_unref (caps);
  }

  if (!profile) {
    gboolean is_rendering_muxer;

    caps = get_profile_format_from_possible_factory_name (strcaps,
        &factory_name, restrictioncaps ? NULL : &restrictioncaps,
        &is_rendering_muxer);
    if (caps) {
      profile = create_encoding_profile_from_caps (caps, preset_name,
          restrictioncaps, presence, single_segment, factory_name,
          muxers_and_encoders, raw_audio_caps, raw_video_caps,
          is_rendering_muxer);
      gst_caps_unref (caps);
    }
  }

  if (variable_framerate) {
    if (GST_IS_ENCODING_VIDEO_PROFILE (profile)) {
      GValue v = {
        0,
      };
      g_value_init (&v, G_TYPE_BOOLEAN);
      if (gst_value_deserialize (&v, variable_framerate)) {
        gst_encoding_video_profile_set_variableframerate
            (GST_ENCODING_VIDEO_PROFILE (profile), g_value_get_boolean (&v));
      } else {
        GST_WARNING ("Invalid value for variable_framerate: %s",
            variable_framerate);

      }
      g_value_reset (&v);
    } else {
      GST_WARNING
          ("Variable framerate specified on a non video encoding profile");
    }
  }

  if (profile == NULL) {
    GST_ERROR ("No way to create a profile for description: %s",
        serialized_profile);
  } else if (element_properties) {
    gst_encoding_profile_set_element_properties (profile,
        g_steal_pointer (&element_properties));
  }

cleanup:
  g_free (strcaps);
  g_free (variable_framerate);
  g_strfreev (strprops_v);
  g_strfreev (preset_v);
  g_free (preset_name);
  if (element_properties)
    gst_structure_free (element_properties);
  if (restrictioncaps)
    gst_caps_unref (restrictioncaps);

  return profile;
}

static GstEncodingProfile *
parse_encoding_profile (const gchar * value)
{
  GstEncodingProfile *res = NULL;
  gchar *caps_str = NULL;
  gchar **strcaps_v =
      g_regex_split_simple ("(?<!\\\\)(?:\\\\\\\\)*:", value, 0, 0);
  guint i;
  GList *muxers_and_encoders =
      gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_ENCODER |
      GST_ELEMENT_FACTORY_TYPE_MUXER,
      GST_RANK_MARGINAL);
  GstCaps *raw_video_caps = gst_caps_new_empty_simple ("video/x-raw");
  GstCaps *raw_audio_caps = gst_caps_new_empty_simple ("audio/x-raw");

  /* The regex returns NULL if no ":" found, handle that case. */
  if (strcaps_v == NULL)
    strcaps_v = g_strsplit (value, ":", 0);

  for (i = 0; strcaps_v[i] && *strcaps_v[i]; i++) {
    GstEncodingProfile *profile;
    caps_str = g_strcompress (strcaps_v[i]);
    profile =
        create_encoding_stream_profile (caps_str, muxers_and_encoders,
        raw_audio_caps, raw_video_caps);

    if (!profile) {
      GST_ERROR ("Could not create profile for caps: %s", caps_str);
      goto error;
    }

    if (res) {
      if (!GST_IS_ENCODING_CONTAINER_PROFILE (res)) {
        GST_ERROR ("The first described encoding profile was not a container"
            " but you are trying to add more profiles to it. This is not possible");
        goto error;
      }

      if (!gst_encoding_container_profile_add_profile
          (GST_ENCODING_CONTAINER_PROFILE (res), profile)) {
        GST_ERROR ("Can not add profile for caps: %s", caps_str);
        goto error;
      }
    } else {
      res = profile;
    }

    g_clear_pointer (&caps_str, g_free);
  }

done:
  g_free (caps_str);
  g_strfreev (strcaps_v);
  gst_caps_unref (raw_audio_caps);
  gst_caps_unref (raw_video_caps);
  gst_plugin_feature_list_free (muxers_and_encoders);

  return res;

error:
  g_clear_object (&res);

  goto done;
}

static GstEncodingProfile *
profile_from_string (const gchar * string)
{
  GstEncodingProfile *profile;
  gchar *filename_end;

  profile = combo_search (string);

  if (profile)
    return profile;

  filename_end = g_strrstr (string, ".gep");
  if (filename_end) {
    GstEncodingTarget *target;
    gchar *profilename = NULL, *filename;

    if (filename_end[4] == ':')
      profilename = g_strdup (&filename_end[5]);

    if (filename_end[4] == '\0' || profilename) {
      filename = g_strndup (string, filename_end - string + strlen (".gep"));

      target = gst_encoding_target_load_from_file (filename, NULL);
      if (target) {
        profile = gst_encoding_target_get_profile (target,
            profilename ? profilename : "default");
        gst_encoding_target_unref (target);
      }

      g_free (profilename);
      g_free (filename);
    }
  }

  if (!profile)
    profile = parse_encoding_profile (string);

  return profile;
}

/* GValue transform function */
static void
string_to_profile_transform (const GValue * src_value, GValue * dest_value)
{
  const gchar *profilename;
  GstEncodingProfile *profile;

  profilename = g_value_get_string (src_value);

  profile = profile_from_string (profilename);

  if (profile)
    g_value_take_object (dest_value, (GObject *) profile);
}

static void
serialize_profile (GString * res, GstEncodingProfile * profile)
{
  gchar *tmp;

  if (res->len)
    g_string_append_c (res, ':');

  if (profile->restriction) {
    tmp = gst_caps_to_string (profile->restriction);
    g_string_append_printf (res, "%s->", tmp);
    g_free (tmp);
  }

  tmp = gst_caps_to_string (profile->format);
  g_string_append (res, tmp);

  if (profile->presence)
    g_string_append_printf (res, "|presence=%d", profile->presence);

  if (profile->single_segment)
    g_string_append_printf (res, "%ssingle-segment=true",
        profile->presence ? "" : "|");

  if (GST_IS_ENCODING_CONTAINER_PROFILE (profile)) {
    GList *tmp;

    for (tmp = GST_ENCODING_CONTAINER_PROFILE (profile)->encodingprofiles; tmp;
        tmp = tmp->next)
      serialize_profile (res, tmp->data);
  }
}

static gchar *
gst_encoding_profile_serialize_valfunc (GValue * value)
{
  GString *res = g_string_new (NULL);
  GstEncodingProfile *profile = g_value_get_object (value);

  serialize_profile (res, profile);

  return g_string_free (res, FALSE);
}

static gboolean
gst_encoding_profile_deserialize_valfunc (GValue * value, const gchar * s)
{
  GstEncodingProfile *profile;

  profile = profile_from_string (s);

  if (profile) {
    g_value_take_object (value, (GObject *) profile);
    return TRUE;
  }

  return FALSE;
}

static GstEncodingProfile *
create_stream_profile_recurse (GstEncodingProfile * toplevel,
    GstDiscovererStreamInfo * sinfo)
{
  GstEncodingProfile *profile = NULL;
  GstStructure *s;
  GstCaps *caps;

  caps = gst_discoverer_stream_info_get_caps (sinfo);

  /* Should unify this with copy_and_clean_caps() */
  caps = gst_caps_make_writable (caps);
  s = gst_caps_get_structure (caps, 0);

  gst_structure_remove_fields (s, "codec_data", "streamheader", "parsed",
      "colorimetry", "framed", "stream-format", "alignment", "tier", "level",
      "profile", "chroma-format", "bit-depth-luma", "bit-depth-chroma", NULL);

  GST_LOG ("Stream: %" GST_PTR_FORMAT, caps);
  if (GST_IS_DISCOVERER_AUDIO_INFO (sinfo)) {
    profile =
        (GstEncodingProfile *) gst_encoding_audio_profile_new (caps, NULL,
        NULL, 0);
  } else if (GST_IS_DISCOVERER_VIDEO_INFO (sinfo)) {
    profile =
        (GstEncodingProfile *) gst_encoding_video_profile_new (caps, NULL,
        NULL, 0);
  } else if (GST_IS_DISCOVERER_CONTAINER_INFO (sinfo)) {
    GList *streams, *stream;

    streams =
        gst_discoverer_container_info_get_streams (GST_DISCOVERER_CONTAINER_INFO
        (sinfo));

    if (!toplevel || !GST_IS_ENCODING_CONTAINER_PROFILE (toplevel)) {
      GstEncodingProfile *prev_toplevel = toplevel;

      toplevel = (GstEncodingProfile *)
          gst_encoding_container_profile_new ("auto-generated",
          "Automatically generated from GstDiscovererInfo", caps, NULL);
      if (prev_toplevel)
        gst_encoding_container_profile_add_profile
            (GST_ENCODING_CONTAINER_PROFILE (toplevel), prev_toplevel);
    }

    for (stream = streams; stream; stream = stream->next)
      create_stream_profile_recurse (toplevel,
          (GstDiscovererStreamInfo *) stream->data);
    gst_discoverer_stream_info_list_free (streams);
  } else {
    GST_FIXME ("Ignoring stream of type '%s'",
        g_type_name (G_OBJECT_TYPE (sinfo)));
    /* subtitles or other ? ignore for now */
  }
  gst_caps_unref (caps);

  if (profile) {
    const gchar *stream_id = gst_discoverer_stream_info_get_stream_id (sinfo);

    if (stream_id) {
      const gchar *subid = strchr (stream_id, '/');

      gst_encoding_profile_set_name (profile, subid ? subid : stream_id);
    }

    if (GST_IS_ENCODING_CONTAINER_PROFILE (toplevel))
      gst_encoding_container_profile_add_profile ((GstEncodingContainerProfile
              *)
          toplevel, profile);
  }

  if (!toplevel && profile)
    toplevel = profile;

  sinfo = gst_discoverer_stream_info_get_next (sinfo);
  if (sinfo)
    return create_stream_profile_recurse (toplevel, sinfo);

  return toplevel;
}

/**
 * gst_encoding_profile_from_discoverer:
 * @info: (transfer none): The #GstDiscovererInfo to read from
 *
 * Creates a #GstEncodingProfile matching the formats from the given
 * #GstDiscovererInfo. Streams other than audio or video (eg,
 * subtitles), are currently ignored.
 *
 * Returns: (transfer full) (nullable): The new #GstEncodingProfile or %NULL.
 */
GstEncodingProfile *
gst_encoding_profile_from_discoverer (GstDiscovererInfo * info)
{
  GstEncodingProfile *profile;
  GstDiscovererStreamInfo *sinfo;

  if (!info || gst_discoverer_info_get_result (info) != GST_DISCOVERER_OK)
    return NULL;

  sinfo = gst_discoverer_info_get_stream_info (info);
  if (!sinfo)
    return NULL;

  profile = create_stream_profile_recurse (NULL, sinfo);
  if (GST_IS_ENCODING_CONTAINER_PROFILE (profile)) {
    if (!gst_encoding_container_profile_get_profiles
        (GST_ENCODING_CONTAINER_PROFILE (profile))) {
      GST_ERROR ("Failed to add any streams");
      g_object_unref (profile);
      return NULL;
    }
  }

  return (GstEncodingProfile *) profile;
}

/**
 * gst_encoding_profile_copy:
 * @self: The #GstEncodingProfile to copy
 *
 * Makes a deep copy of @self
 *
 * Returns: (transfer full): The copy of @self
 *
 * Since: 1.12
 */
GstEncodingProfile *
gst_encoding_profile_copy (GstEncodingProfile * self)
{
  GstEncodingProfileClass *klass =
      (GstEncodingProfileClass *) G_OBJECT_GET_CLASS (self);
  GstEncodingProfile *copy =
      common_creation (G_OBJECT_TYPE (self), self->format, self->preset,
      self->name, self->description, self->restriction, self->presence);

  copy->enabled = self->enabled;
  copy->allow_dynamic_output = self->allow_dynamic_output;
  gst_encoding_profile_set_preset_name (copy, self->preset_name);
  gst_encoding_profile_set_description (copy, self->description);

  if (klass->copy)
    klass->copy (self, copy);

  return copy;
}
