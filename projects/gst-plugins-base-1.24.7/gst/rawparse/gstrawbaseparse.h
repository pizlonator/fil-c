/* GStreamer
 * Copyright (C) <2016> Carlos Rafael Giani <dv at pseudoterminal dot org>
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

#ifndef __GST_RAW_BASE_PARSE_H__
#define __GST_RAW_BASE_PARSE_H__

#include <gst/gst.h>
#include <gst/base/base.h>

G_BEGIN_DECLS

#define GST_TYPE_RAW_BASE_PARSE \
  (gst_raw_base_parse_get_type())
#define GST_RAW_BASE_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_RAW_BASE_PARSE, GstRawBaseParse))
#define GST_RAW_BASE_PARSE_CAST(obj) \
  ((GstRawBaseParse *)(obj))
#define GST_RAW_BASE_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_RAW_BASE_PARSE, GstRawBaseParseClass))
#define GST_RAW_BASE_PARSE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_RAW_BASE_PARSE, GstRawBaseParseClass))
#define GST_IS_RAW_BASE_PARSE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_RAW_BASE_PARSE))
#define GST_IS_RAW_BASE_PARSE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_RAW_BASE_PARSE))

#define GST_RAW_BASE_PARSE_CONFIG_MUTEX_LOCK(obj)   g_mutex_lock(&(((GstRawBaseParse *)(obj))->config_mutex))
#define GST_RAW_BASE_PARSE_CONFIG_MUTEX_UNLOCK(obj) g_mutex_unlock(&(((GstRawBaseParse *)(obj))->config_mutex))

typedef enum _GstRawBaseParseConfig GstRawBaseParseConfig;
typedef struct _GstRawBaseParse GstRawBaseParse;
typedef struct _GstRawBaseParseClass GstRawBaseParseClass;

/**
 * GstRawBaseParseConfig:
 * @GST_RAW_BASE_PARSE_CONFIG_CURRENT: configuration that is currently active
 * @GST_RAW_BASE_PARSE_CONFIG_SINKCAPS: configuration that is defined by the input sink caps
 * @GST_RAW_BASE_PARSE_CONFIG_PROPERTIES: configuration that is defined by class properties
 *
 * Identifier for the type of parser configuration.
 */
enum _GstRawBaseParseConfig
{
  GST_RAW_BASE_PARSE_CONFIG_CURRENT = 1,
  GST_RAW_BASE_PARSE_CONFIG_SINKCAPS,
  GST_RAW_BASE_PARSE_CONFIG_PROPERTIES
};

/**
 * GstRawBaseParse:
 *
 * The opaque #GstRawBaseParse data structure.
 */
struct _GstRawBaseParse
{
  GstBaseParse parent;

  /*< private > */

  /* TRUE if the source pad caps have been set already. This is used
   * for checking if the source pad caps have to be set. */
  gboolean src_caps_set;

  /* Mutex which protects access to and modifications on the configs. */
  GMutex config_mutex;
};

/**
 * GstRawBaseParseClass:
 * @parent_class:              The parent class structure
 * @set_current_config:        Sets the new current configuration. Subclasses must internally
 *                             switch to this new configuration. Return FALSE if this failed,
 *                             TRUE otherwise.
 * @get_current_config:        Gets the current configuration. All return values except
 *                             except GST_RAW_BASE_PARSE_CONFIG_CURRENT are valid.
 * @set_config_from_caps:      Parses the caps and copies its information to the configuration.
 *                             Returns FALSE if this failed, TRUE otherwise. Specified caps
 *                             are not unref'd.
 * @get_caps_from_config:      Creates a new caps structure out of the information from the
 *                             specified configuration. Ownership over the returned caps are
 *                             transferred to the caller. If something fails during the caps
 *                             creation, the vfunc must make sure to destroy any partially
 *                             created caps; the *caps value is always set to NULL in case of
 *                             failure. Returns FALSE in case of failure,
 *                             TRUE in case of success.
 * @get_config_frame_size:     Gets the size of one frame, in bytes, from the specified
 *                             configuration. This must be the size of the complete frame,
 *                             including any overhead (metadata, headers, padding bytes etc.).
 * @get_max_frames_per_buffer: Optional.
 *                             Returns up to how many complete frames one output buffer may
 *                             contain. The value must be nonzero. This is useful for example
 *                             with video parsers which need to ensure that one output buffer
 *                             contains only one video frame, even if the input buffer contains
 *                             several complete frames. If this vfunc is not set, then there
 *                             is no maximum number of frames per buffer - the parser reads
 *                             as many complete frames as possible from the input buffer.
 * @is_config_ready:           Returns TRUE if the specified configuration is ready, FALSE
 *                             otherwise.
 * @process:                   Optional.
 *                             This is useful to do any last minute processing before the
 *                             data is pushed downstream. One example is channel reordering
 *                             in audio parsers.
 *                             in_data is the complete input buffer, total_num_in_bytes is
 *                             the total amount of bytes this input buffer contains (including
 *                             excess bytes that form an incomplete rame). num_valid_in_bytes
 *                             is the subset of these bytes that are to be pushed downstream.
 *                             If for example the frame size is 4, and total_num_in_bytes is
 *                             411, then num_valid_in_bytes will be 408, since the last 3
 *                             bytes form an incomplete frame.
 *                             The value of num_valid_in_bytes excludes the overhead bytes
 *                             indicated by @get_overhead_size.
 *                             If the subclass creates a new buffer here, *processed_data
 *                             must be set to the new buffer's pointer. If the subclass does
 *                             not create any new buffer, and just expects the first
 *                             num_valid_in_bytes of the input buffer to be pushed downstream,
 *                             then *processed_data must be set to NULL.
 *                             If this vfunc is not set, then the parser behaves as if this
 *                             vfunc set *processed_data data to NULL.
 * @is_unit_format_supported:  Returns TRUE if the given format is supported by the
 *                             @get_units_per_second function, FALSE otherwise.
 * @get_units_per_second:      Returns how many units per second exist for a given format.
 *                             For example, with an audio parser and format DEFAULT, the units
 *                             per second are typically the number of samples per second
 *                             (= the sample rate). For video parsers, this would be the frame
 *                             rate. If BYTES or TIME are used as format, then the result must
 *                             not include any extra overhead (metadata, headers, padding etc.)
 * @get_overhead_size:         Optional.
 *                             Returns the number of bytes that make up the portion of a frame
 *                             that isn't payload. Examples are padding bytes, headers, and
 *                             other kinds of metadata. If this vfunc isn't defined, then an
 *                             overhead size of 0 bytes is assumed.
 *
 * Subclasses are required to override all vfuncs except for @process, which is optional.
 * The raw base parser lock is held during all vfunc calls.
 */
struct _GstRawBaseParseClass
{
  GstBaseParseClass parent_class;

  gboolean              (*set_current_config)        (GstRawBaseParse *raw_base_parse,
                                                      GstRawBaseParseConfig config);
  GstRawBaseParseConfig (*get_current_config)        (GstRawBaseParse *raw_base_parse);

  gboolean              (*set_config_from_caps)      (GstRawBaseParse * raw_base_parse,
                                                      GstRawBaseParseConfig config,
                                                      GstCaps * caps);
  gboolean              (*get_caps_from_config)      (GstRawBaseParse * raw_base_parse,
                                                      GstRawBaseParseConfig config,
                                                      GstCaps ** caps);

  gsize                 (*get_config_frame_size)     (GstRawBaseParse * raw_base_parse,
                                                      GstRawBaseParseConfig config);
  guint                 (*get_max_frames_per_buffer) (GstRawBaseParse * raw_base_parse,
                                                      GstRawBaseParseConfig config);

  gboolean              (*is_config_ready)           (GstRawBaseParse * raw_base_parse,
                                                      GstRawBaseParseConfig config);

  gboolean              (*process)                   (GstRawBaseParse * raw_base_parse,
                                                      GstRawBaseParseConfig config,
                                                      GstBuffer * in_data,
                                                      gsize total_num_in_bytes,
                                                      gsize num_valid_in_bytes,
                                                      GstBuffer ** processed_data);

  gboolean              (*is_unit_format_supported)  (GstRawBaseParse * raw_base_parse,
                                                      GstFormat format);
  void                  (*get_units_per_second)      (GstRawBaseParse * raw_base_parse,
                                                      GstFormat format,
                                                      GstRawBaseParseConfig config,
                                                      gsize * units_per_sec_n,
                                                      gsize * units_per_sec_d);

  gint                  (*get_overhead_size)         (GstRawBaseParse * raw_base_parse,
                                                      GstRawBaseParseConfig config);

  gint                  (*get_alignment)             (GstRawBaseParse * raw_base_parse,
                                                      GstRawBaseParseConfig config);
};

void gst_raw_base_parse_invalidate_src_caps (GstRawBaseParse * raw_base_parse);

GType gst_raw_base_parse_get_type (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstRawBaseParse, gst_object_unref)

G_END_DECLS

#endif
