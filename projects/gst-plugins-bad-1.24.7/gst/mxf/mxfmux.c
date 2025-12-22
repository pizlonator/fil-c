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

/**
 * SECTION:element-mxfmux
 * @title: mxfmux
 *
 * mxfmux muxes different streams into an MXF file.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v filesrc location=/path/to/audio ! decodebin ! queue ! mxfmux name=m ! filesink location=file.mxf   filesrc location=/path/to/video ! decodebin ! queue ! m.
 * ]| This pipeline muxes an audio and video file into a single MXF file.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <string.h>

#include "gstmxfelements.h"
#include "mxfmux.h"

#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

GST_DEBUG_CATEGORY_STATIC (mxfmux_debug);
#define GST_CAT_DEFAULT mxfmux_debug

#define GST_TYPE_MXF_MUX_PAD            (gst_mxf_mux_pad_get_type())
#define GST_MXF_MUX_PAD(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MXF_MUX_PAD, GstMXFMuxPad))
#define GST_MXF_MUX_PAD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MXF_MUX_PAD, GstMXFMuxPadClass))
#define GST_MXF_MUX_PAD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_MXF_MUX_PAD, GstMXFMuxPadClass))
#define GST_IS_MXF_MUX_PAD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MXF_MUX_PAD))
#define GST_IS_MXF_MUX_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MXF_MUX_PAD))

typedef struct
{
  GstAggregatorPad parent;

  guint64 pos;
  GstClockTime last_timestamp;

  MXFMetadataFileDescriptor *descriptor;

  GstAdapter *adapter;
  gboolean have_complete_edit_unit;

  gpointer mapping_data;
  const MXFEssenceElementWriter *writer;
  MXFEssenceElementWriteFunc write_func;

  MXFMetadataSourcePackage *source_package;
  MXFMetadataTimelineTrack *source_track;
} GstMXFMuxPad;

typedef struct
{
  GstAggregatorPadClass parent_class;
} GstMXFMuxPadClass;

GType gst_mxf_mux_pad_get_type (void);

G_DEFINE_TYPE (GstMXFMuxPad, gst_mxf_mux_pad, GST_TYPE_AGGREGATOR_PAD);

static void
gst_mxf_mux_pad_finalize (GObject * object)
{
  GstMXFMuxPad *pad = GST_MXF_MUX_PAD (object);

  g_object_unref (pad->adapter);
  g_free (pad->mapping_data);

  G_OBJECT_CLASS (gst_mxf_mux_pad_parent_class)->finalize (object);
}

static void
gst_mxf_mux_pad_class_init (GstMXFMuxPadClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  object_class->finalize = gst_mxf_mux_pad_finalize;
}

static void
gst_mxf_mux_pad_init (GstMXFMuxPad * pad)
{
  pad->adapter = gst_adapter_new ();
}

static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/mxf")
    );

enum
{
  PROP_0
};

#define gst_mxf_mux_parent_class parent_class
G_DEFINE_TYPE (GstMXFMux, gst_mxf_mux, GST_TYPE_AGGREGATOR);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (mxfmux, "mxfmux", GST_RANK_PRIMARY,
    GST_TYPE_MXF_MUX, mxf_element_init (plugin));

static void gst_mxf_mux_finalize (GObject * object);

static GstFlowReturn gst_mxf_mux_aggregate (GstAggregator * aggregator,
    gboolean timeout);
static gboolean gst_mxf_mux_stop (GstAggregator * aggregator);

static gboolean gst_mxf_mux_src_event (GstAggregator * aggregator,
    GstEvent * event);
static gboolean gst_mxf_mux_sink_event (GstAggregator * aggregator,
    GstAggregatorPad * aggpad, GstEvent * event);
static GstAggregatorPad *gst_mxf_mux_create_new_pad (GstAggregator * aggregator,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);

static void gst_mxf_mux_reset (GstMXFMux * mux);

static GstFlowReturn
gst_mxf_mux_push (GstMXFMux * mux, GstBuffer * buf)
{
  guint size = gst_buffer_get_size (buf);
  GstFlowReturn ret;

  ret = gst_aggregator_finish_buffer (GST_AGGREGATOR (mux), buf);
  mux->offset += size;

  return ret;
}

static void
gst_mxf_mux_class_init (GstMXFMuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstAggregatorClass *gstaggregator_class;
  const GstPadTemplate **p;

  GST_DEBUG_CATEGORY_INIT (mxfmux_debug, "mxfmux", 0, "MXF muxer");

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstaggregator_class = (GstAggregatorClass *) klass;

  gobject_class->finalize = gst_mxf_mux_finalize;

  gstaggregator_class->create_new_pad =
      GST_DEBUG_FUNCPTR (gst_mxf_mux_create_new_pad);
  gstaggregator_class->src_event = GST_DEBUG_FUNCPTR (gst_mxf_mux_src_event);
  gstaggregator_class->sink_event = GST_DEBUG_FUNCPTR (gst_mxf_mux_sink_event);
  gstaggregator_class->stop = GST_DEBUG_FUNCPTR (gst_mxf_mux_stop);
  gstaggregator_class->aggregate = GST_DEBUG_FUNCPTR (gst_mxf_mux_aggregate);
  gstaggregator_class->negotiate = NULL;

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &src_templ, GST_TYPE_MXF_MUX_PAD);

  p = mxf_essence_element_writer_get_pad_templates ();
  while (p && *p) {
    gst_element_class_add_pad_template (gstelement_class,
        (GstPadTemplate *) gst_object_ref (GST_OBJECT (*p)));
    p++;
  }

  gst_element_class_set_static_metadata (gstelement_class, "MXF muxer",
      "Codec/Muxer",
      "Muxes video/audio streams into a MXF stream",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  gst_type_mark_as_plugin_api (GST_TYPE_MXF_MUX_PAD, 0);
}

static void
gst_mxf_mux_init (GstMXFMux * mux)
{
  mux->index_table = g_array_new (FALSE, FALSE, sizeof (MXFIndexTableSegment));
  gst_mxf_mux_reset (mux);
}

static void
gst_mxf_mux_finalize (GObject * object)
{
  GstMXFMux *mux = GST_MXF_MUX (object);

  gst_mxf_mux_reset (mux);

  if (mux->metadata) {
    g_hash_table_destroy (mux->metadata);
    mux->metadata = NULL;
    g_list_free (mux->metadata_list);
    mux->metadata_list = NULL;
  }

  if (mux->index_table) {
    gsize n;
    for (n = 0; n < mux->index_table->len; ++n)
      g_free (g_array_index (mux->index_table, MXFIndexTableSegment,
              n).index_entries);
    g_array_free (mux->index_table, TRUE);
    mux->index_table = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_mxf_mux_reset (GstMXFMux * mux)
{
  GList *l;
  gsize n;

  GST_OBJECT_LOCK (mux);
  for (l = GST_ELEMENT_CAST (mux)->sinkpads; l; l = l->next) {
    GstMXFMuxPad *pad = l->data;

    gst_adapter_clear (pad->adapter);
    g_free (pad->mapping_data);
    pad->mapping_data = NULL;

    pad->pos = 0;
    pad->last_timestamp = 0;
    pad->descriptor = NULL;
    pad->have_complete_edit_unit = FALSE;

    pad->source_package = NULL;
    pad->source_track = NULL;
  }
  GST_OBJECT_UNLOCK (mux);

  mux->state = GST_MXF_MUX_STATE_HEADER;

  if (mux->metadata) {
    g_hash_table_destroy (mux->metadata);
    mux->preface = NULL;
    g_list_free (mux->metadata_list);
    mux->metadata_list = NULL;
  }
  mux->metadata = mxf_metadata_hash_table_new ();

  mxf_partition_pack_reset (&mux->partition);
  mxf_primer_pack_reset (&mux->primer);
  memset (&mux->min_edit_rate, 0, sizeof (MXFFraction));
  mux->last_gc_timestamp = 0;
  mux->last_gc_position = 0;
  mux->offset = 0;

  if (mux->index_table)
    for (n = 0; n < mux->index_table->len; ++n)
      g_free (g_array_index (mux->index_table, MXFIndexTableSegment,
              n).index_entries);
  g_array_set_size (mux->index_table, 0);
  mux->current_index_pos = 0;
  mux->last_keyframe_pos = 0;
}

static gboolean
gst_mxf_mux_src_event (GstAggregator * aggregator, GstEvent * event)
{
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      /* disable seeking for now */
      gst_event_unref (event);
      return FALSE;
    default:
      return GST_AGGREGATOR_CLASS (parent_class)->src_event (aggregator, event);
      break;
  }

  g_assert_not_reached ();
}

static gboolean
gst_mxf_mux_set_caps (GstMXFMux * mux, GstMXFMuxPad * pad, GstCaps * caps)
{
  gboolean ret = TRUE;
  MXFUUID d_instance_uid = { {0,} };
  MXFMetadataFileDescriptor *old_descriptor = pad->descriptor;
  GList *l;

  GST_DEBUG_OBJECT (pad, "Setting caps %" GST_PTR_FORMAT, caps);

  if (old_descriptor) {
    memcpy (&d_instance_uid, &MXF_METADATA_BASE (old_descriptor)->instance_uid,
        16);
    pad->descriptor = NULL;
    g_free (pad->mapping_data);
    pad->mapping_data = NULL;
  }

  pad->descriptor =
      pad->writer->get_descriptor (GST_PAD_PAD_TEMPLATE (pad), caps,
      &pad->write_func, &pad->mapping_data);

  if (!pad->descriptor) {
    GST_ERROR_OBJECT (mux,
        "Couldn't get descriptor for pad '%s' with caps %" GST_PTR_FORMAT,
        GST_PAD_NAME (pad), caps);
    return FALSE;
  }

  if (mxf_uuid_is_zero (&d_instance_uid))
    mxf_uuid_init (&d_instance_uid, mux->metadata);

  memcpy (&MXF_METADATA_BASE (pad->descriptor)->instance_uid, &d_instance_uid,
      16);

  if (old_descriptor) {
    for (l = mux->metadata_list; l; l = l->next) {
      MXFMetadataBase *tmp = l->data;

      if (mxf_uuid_is_equal (&d_instance_uid, &tmp->instance_uid)) {
        l->data = pad->descriptor;
        break;
      }
    }
  } else {
    mux->metadata_list = g_list_prepend (mux->metadata_list, pad->descriptor);
  }

  g_hash_table_replace (mux->metadata,
      &MXF_METADATA_BASE (pad->descriptor)->instance_uid, pad->descriptor);

  if (old_descriptor) {
    if (mux->preface && mux->preface->content_storage &&
        mux->preface->content_storage->packages) {
      guint i, j;

      for (i = 0; i < mux->preface->content_storage->n_packages; i++) {
        MXFMetadataSourcePackage *package;

        if (!MXF_IS_METADATA_SOURCE_PACKAGE (mux->preface->
                content_storage->packages[i]))
          continue;

        package =
            MXF_METADATA_SOURCE_PACKAGE (mux->preface->
            content_storage->packages[i]);

        if (!package->descriptor)
          continue;

        if (MXF_IS_METADATA_MULTIPLE_DESCRIPTOR (package->descriptor)) {
          MXFMetadataMultipleDescriptor *tmp =
              MXF_METADATA_MULTIPLE_DESCRIPTOR (package->descriptor);

          for (j = 0; j < tmp->n_sub_descriptors; j++) {
            if (tmp->sub_descriptors[j] ==
                MXF_METADATA_GENERIC_DESCRIPTOR (old_descriptor)) {
              tmp->sub_descriptors[j] =
                  MXF_METADATA_GENERIC_DESCRIPTOR (pad->descriptor);
              memcpy (&tmp->sub_descriptors_uids[j], &d_instance_uid, 16);
            }
          }
        } else if (package->descriptor ==
            MXF_METADATA_GENERIC_DESCRIPTOR (old_descriptor)) {
          package->descriptor =
              MXF_METADATA_GENERIC_DESCRIPTOR (pad->descriptor);
          memcpy (&package->descriptor_uid, &d_instance_uid, 16);
        }
      }
    }
  }

  return ret;
}

static gboolean
gst_mxf_mux_sink_event (GstAggregator * aggregator,
    GstAggregatorPad * aggpad, GstEvent * event)
{
  GstMXFMux *mux = GST_MXF_MUX (aggregator);
  gboolean ret = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TAG:
      /* TODO: do something with the tags */
      break;
    case GST_EVENT_CAPS:{
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);

      ret = gst_mxf_mux_set_caps (mux, GST_MXF_MUX_PAD (aggpad), caps);

      break;
    }
    default:
      break;
  }

  /* now GstAggregator can take care of the rest, e.g. EOS */
  if (ret)
    ret =
        GST_AGGREGATOR_CLASS (parent_class)->sink_event (aggregator, aggpad,
        event);

  return ret;
}

static char *
gst_mxf_mux_create_pad_name (GstPadTemplate * templ, guint id)
{
  GString *string;

  string = g_string_new (GST_PAD_TEMPLATE_NAME_TEMPLATE (templ));
  g_string_truncate (string, string->len - 2);
  g_string_append_printf (string, "%u", id);

  return g_string_free (string, FALSE);
}

static GstAggregatorPad *
gst_mxf_mux_create_new_pad (GstAggregator * aggregator,
    GstPadTemplate * templ, const gchar * pad_name, const GstCaps * caps)
{
  GstMXFMux *mux = GST_MXF_MUX (aggregator);
  GstMXFMuxPad *pad;
  guint pad_number;
  gchar *name = NULL;
  const MXFEssenceElementWriter *writer;

  if (mux->state != GST_MXF_MUX_STATE_HEADER) {
    GST_WARNING_OBJECT (mux, "Can't request pads after writing header");
    return NULL;
  }

  writer = mxf_essence_element_writer_find (templ);
  if (!writer) {
    GST_ERROR_OBJECT (mux, "Not our template");
    return NULL;
  }
  pad_number = g_atomic_int_add ((gint *) & mux->n_pads, 1);
  name = gst_mxf_mux_create_pad_name (templ, pad_number);

  GST_DEBUG_OBJECT (mux, "Creating pad '%s'", name);
  pad =
      g_object_new (GST_TYPE_MXF_MUX_PAD, "name", name, "direction",
      GST_PAD_SINK, "template", templ, NULL);
  g_free (name);
  pad->last_timestamp = 0;
  pad->writer = writer;

  gst_pad_use_fixed_caps (GST_PAD_CAST (pad));

  return GST_AGGREGATOR_PAD (pad);
}

static GstFlowReturn
gst_mxf_mux_create_metadata (GstMXFMux * mux)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GList *l;
  GArray *tmp;

  GST_DEBUG_OBJECT (mux, "Creating MXF metadata");

  GST_OBJECT_LOCK (mux);

  for (l = GST_ELEMENT_CAST (mux)->sinkpads; l; l = l->next) {
    GstMXFMuxPad *pad = l->data;
    GstCaps *caps;
    GstBuffer *buffer;

    if (!pad || !pad->descriptor) {
      GST_OBJECT_UNLOCK (mux);
      return GST_FLOW_ERROR;
    }

    caps = gst_pad_get_current_caps (GST_PAD_CAST (pad));
    if (!caps) {
      GST_OBJECT_UNLOCK (mux);
      return GST_FLOW_ERROR;
    }

    buffer = gst_aggregator_pad_peek_buffer (GST_AGGREGATOR_PAD (pad));
    if (pad->writer->update_descriptor)
      pad->writer->update_descriptor (pad->descriptor,
          caps, pad->mapping_data, buffer);
    if (buffer)
      gst_buffer_unref (buffer);
    gst_caps_unref (caps);
  }

  /* Preface */
  mux->preface =
      (MXFMetadataPreface *) g_object_new (MXF_TYPE_METADATA_PREFACE, NULL);
  mxf_uuid_init (&MXF_METADATA_BASE (mux->preface)->instance_uid,
      mux->metadata);
  g_hash_table_insert (mux->metadata,
      &MXF_METADATA_BASE (mux->preface)->instance_uid, mux->preface);
  mux->metadata_list = g_list_prepend (mux->metadata_list, mux->preface);

  mxf_timestamp_set_now (&mux->preface->last_modified_date);
  mux->preface->version = 258;
  mux->preface->object_model_version = 1;

  mxf_op_set_generalized (&mux->preface->operational_pattern, MXF_OP_1a, TRUE,
      TRUE, FALSE);

  tmp = g_array_new (FALSE, FALSE, sizeof (MXFUL));
  for (l = GST_ELEMENT_CAST (mux)->sinkpads; l; l = l->next) {
    GstMXFMuxPad *pad = l->data;
    guint i;
    gboolean found = FALSE;

    if (!pad || !pad->descriptor ||
        mxf_ul_is_zero (&pad->descriptor->essence_container)) {
      GST_OBJECT_UNLOCK (mux);
      return GST_FLOW_ERROR;
    }

    for (i = 0; i < tmp->len; i++) {
      if (mxf_ul_is_equal (&pad->descriptor->essence_container,
              &g_array_index (tmp, MXFUL, i))) {
        found = TRUE;
        break;
      }
    }

    if (found)
      continue;

    g_array_append_val (tmp, pad->descriptor->essence_container);
  }
  mux->preface->n_essence_containers = tmp->len;
  mux->preface->essence_containers = (MXFUL *) g_array_free (tmp, FALSE);

  /* This will later be used as UID for the material package */
  mxf_uuid_init (&mux->preface->primary_package_uid, mux->metadata);

  /* Identifications */
  {
    MXFMetadataIdentification *identification;
    static const guint8 gst_uid[] = {
      0xe5, 0xde, 0xcd, 0x04, 0x24, 0x90, 0x69, 0x18,
      0x8a, 0xc9, 0xb5, 0xd7, 0x02, 0x58, 0x46, 0x78
    };
    guint major, minor, micro, nano;

    mux->preface->n_identifications = 1;
    mux->preface->identifications = g_new0 (MXFMetadataIdentification *, 1);
    identification = mux->preface->identifications[0] =
        (MXFMetadataIdentification *)
        g_object_new (MXF_TYPE_METADATA_IDENTIFICATION, NULL);

    mxf_uuid_init (&MXF_METADATA_BASE (identification)->instance_uid,
        mux->metadata);
    g_hash_table_insert (mux->metadata,
        &MXF_METADATA_BASE (identification)->instance_uid, identification);
    mux->metadata_list = g_list_prepend (mux->metadata_list, identification);

    mxf_uuid_init (&identification->this_generation_uid, NULL);

    identification->company_name = g_strdup ("GStreamer");
    identification->product_name = g_strdup ("GStreamer Multimedia Framework");

    gst_version (&major, &minor, &micro, &nano);
    identification->product_version.major = major;
    identification->product_version.minor = minor;
    identification->product_version.patch = micro;
    identification->product_version.build = nano;
    identification->product_version.release =
        (nano == 0) ? 1 : (nano == 1) ? 2 : 4;

    identification->version_string =
        g_strdup_printf ("%u.%u.%u.%u", major, minor, micro, nano);
    memcpy (&identification->product_uid, &gst_uid, 16);

    memcpy (&identification->modification_date,
        &mux->preface->last_modified_date, sizeof (MXFTimestamp));
    memcpy (&identification->toolkit_version, &identification->product_version,
        sizeof (MXFProductVersion));

#ifdef HAVE_SYS_UTSNAME_H
    {
      struct utsname sys_details;

      if (uname (&sys_details) == 0) {
        identification->platform = g_strdup_printf ("%s %s %s",
            sys_details.sysname, sys_details.release, sys_details.machine);
      }
    }
#endif

#if defined(G_OS_WIN32)
    if (identification->platform == NULL)
      identification->platform = g_strdup ("Microsoft Windows");
#elif defined(G_OS_BEOS)
    if (identification->platform == NULL)
      identification->platform = g_strdup ("BEOS");
#elif defined(G_OS_UNIX)
    if (identification->platform == NULL)
      identification->platform = g_strdup ("Unix");
#endif
  }

  /* Content storage */
  {
    MXFMetadataContentStorage *cstorage;
    guint i;

    cstorage = mux->preface->content_storage = (MXFMetadataContentStorage *)
        g_object_new (MXF_TYPE_METADATA_CONTENT_STORAGE, NULL);
    mxf_uuid_init (&MXF_METADATA_BASE (cstorage)->instance_uid, mux->metadata);
    g_hash_table_insert (mux->metadata,
        &MXF_METADATA_BASE (cstorage)->instance_uid, cstorage);
    mux->metadata_list = g_list_prepend (mux->metadata_list, cstorage);

    cstorage->n_packages = 2;
    cstorage->packages = g_new0 (MXFMetadataGenericPackage *, 2);

    /* Source package */
    {
      MXFMetadataSourcePackage *p;

      cstorage->packages[1] = (MXFMetadataGenericPackage *)
          g_object_new (MXF_TYPE_METADATA_SOURCE_PACKAGE, NULL);
      mxf_uuid_init (&MXF_METADATA_BASE (cstorage->packages[1])->instance_uid,
          mux->metadata);
      g_hash_table_insert (mux->metadata,
          &MXF_METADATA_BASE (cstorage->packages[1])->instance_uid,
          cstorage->packages[1]);
      mux->metadata_list =
          g_list_prepend (mux->metadata_list, cstorage->packages[1]);
      p = (MXFMetadataSourcePackage *) cstorage->packages[1];

      mxf_umid_init (&p->parent.package_uid);
      p->parent.name = g_strdup ("Source package");
      memcpy (&p->parent.package_creation_date,
          &mux->preface->last_modified_date, sizeof (MXFTimestamp));
      memcpy (&p->parent.package_modified_date,
          &mux->preface->last_modified_date, sizeof (MXFTimestamp));

      p->parent.n_tracks = GST_ELEMENT_CAST (mux)->numsinkpads + 1;
      p->parent.tracks = g_new0 (MXFMetadataTrack *, p->parent.n_tracks);

      if (p->parent.n_tracks > 2) {
        MXFMetadataMultipleDescriptor *d;

        p->descriptor = (MXFMetadataGenericDescriptor *)
            g_object_new (MXF_TYPE_METADATA_MULTIPLE_DESCRIPTOR, NULL);
        d = (MXFMetadataMultipleDescriptor *) p->descriptor;
        d->n_sub_descriptors = p->parent.n_tracks - 1;
        d->sub_descriptors =
            g_new0 (MXFMetadataGenericDescriptor *, p->parent.n_tracks - 1);

        mxf_uuid_init (&MXF_METADATA_BASE (d)->instance_uid, mux->metadata);
        g_hash_table_insert (mux->metadata,
            &MXF_METADATA_BASE (d)->instance_uid, d);
        mux->metadata_list = g_list_prepend (mux->metadata_list, d);
      }

      /* Tracks */
      {
        guint n;

        n = 1;

        /* Essence tracks */
        for (l = GST_ELEMENT_CAST (mux)->sinkpads; l; l = l->next) {
          GstMXFMuxPad *pad = l->data;
          MXFMetadataTimelineTrack *track;
          MXFMetadataSequence *sequence;
          MXFMetadataSourceClip *clip;
          GstCaps *caps;
          GstBuffer *buffer;

          p->parent.tracks[n] = (MXFMetadataTrack *)
              g_object_new (MXF_TYPE_METADATA_TIMELINE_TRACK, NULL);
          track = (MXFMetadataTimelineTrack *) p->parent.tracks[n];
          mxf_uuid_init (&MXF_METADATA_BASE (track)->instance_uid,
              mux->metadata);
          g_hash_table_insert (mux->metadata,
              &MXF_METADATA_BASE (track)->instance_uid, track);
          mux->metadata_list = g_list_prepend (mux->metadata_list, track);

          caps = gst_pad_get_current_caps (GST_PAD_CAST (pad));
          buffer = gst_aggregator_pad_peek_buffer (GST_AGGREGATOR_PAD (pad));
          track->parent.track_id = n + 1;
          track->parent.track_number =
              pad->writer->get_track_number_template (pad->descriptor,
              caps, pad->mapping_data);

          /* FIXME: All tracks in a source package must have the same edit
           * rate! This means that if we have different edit rates, we need to
           * make them different source packages and essence containers with
           * a different BodySID */
          pad->writer->get_edit_rate (pad->descriptor,
              caps, pad->mapping_data, buffer, p, track, &track->edit_rate);
          if (buffer)
            gst_buffer_unref (buffer);
          gst_caps_unref (caps);

          sequence = track->parent.sequence = (MXFMetadataSequence *)
              g_object_new (MXF_TYPE_METADATA_SEQUENCE, NULL);
          mxf_uuid_init (&MXF_METADATA_BASE (sequence)->instance_uid,
              mux->metadata);
          g_hash_table_insert (mux->metadata,
              &MXF_METADATA_BASE (sequence)->instance_uid, sequence);
          mux->metadata_list = g_list_prepend (mux->metadata_list, sequence);

          memcpy (&sequence->data_definition, &pad->writer->data_definition,
              16);

          sequence->n_structural_components = 1;
          sequence->structural_components =
              g_new0 (MXFMetadataStructuralComponent *, 1);

          clip = (MXFMetadataSourceClip *)
              g_object_new (MXF_TYPE_METADATA_SOURCE_CLIP, NULL);
          sequence->structural_components[0] =
              (MXFMetadataStructuralComponent *) clip;
          mxf_uuid_init (&MXF_METADATA_BASE (clip)->instance_uid,
              mux->metadata);
          g_hash_table_insert (mux->metadata,
              &MXF_METADATA_BASE (clip)->instance_uid, clip);
          mux->metadata_list = g_list_prepend (mux->metadata_list, clip);

          memcpy (&clip->parent.data_definition, &sequence->data_definition,
              16);
          clip->start_position = 0;

          pad->source_package = p;
          pad->source_track = track;
          pad->descriptor->linked_track_id = n + 1;
          if (p->parent.n_tracks == 2) {
            p->descriptor = (MXFMetadataGenericDescriptor *) pad->descriptor;
          } else {
            MXF_METADATA_MULTIPLE_DESCRIPTOR (p->
                descriptor)->sub_descriptors[n - 1] =
                (MXFMetadataGenericDescriptor *) pad->descriptor;
          }

          n++;
        }
      }
    }

    /* Material package */
    {
      MXFMetadataMaterialPackage *p;
      MXFFraction min_edit_rate = { 0, 0 };
      gdouble min_edit_rate_d = G_MAXDOUBLE;

      cstorage->packages[0] = (MXFMetadataGenericPackage *)
          g_object_new (MXF_TYPE_METADATA_MATERIAL_PACKAGE, NULL);
      memcpy (&MXF_METADATA_BASE (cstorage->packages[0])->instance_uid,
          &mux->preface->primary_package_uid, 16);
      g_hash_table_insert (mux->metadata,
          &MXF_METADATA_BASE (cstorage->packages[0])->instance_uid,
          cstorage->packages[0]);
      mux->metadata_list =
          g_list_prepend (mux->metadata_list, cstorage->packages[0]);
      p = (MXFMetadataMaterialPackage *) cstorage->packages[0];

      mxf_umid_init (&p->package_uid);
      p->name = g_strdup ("Material package");
      memcpy (&p->package_creation_date, &mux->preface->last_modified_date,
          sizeof (MXFTimestamp));
      memcpy (&p->package_modified_date, &mux->preface->last_modified_date,
          sizeof (MXFTimestamp));

      p->n_tracks = GST_ELEMENT_CAST (mux)->numsinkpads + 1;
      p->tracks = g_new0 (MXFMetadataTrack *, p->n_tracks);

      /* Tracks */
      {
        guint n;

        n = 1;
        /* Essence tracks */
        for (l = GST_ELEMENT_CAST (mux)->sinkpads; l; l = l->next) {
          GstMXFMuxPad *pad = l->data;
          GstCaps *caps;
          GstBuffer *buffer;
          MXFMetadataSourcePackage *source_package;
          MXFMetadataTimelineTrack *track, *source_track;
          MXFMetadataSequence *sequence;
          MXFMetadataSourceClip *clip;

          source_package = MXF_METADATA_SOURCE_PACKAGE (cstorage->packages[1]);
          source_track =
              MXF_METADATA_TIMELINE_TRACK (source_package->parent.tracks[n]);

          p->tracks[n] = (MXFMetadataTrack *)
              g_object_new (MXF_TYPE_METADATA_TIMELINE_TRACK, NULL);
          track = (MXFMetadataTimelineTrack *) p->tracks[n];
          mxf_uuid_init (&MXF_METADATA_BASE (track)->instance_uid,
              mux->metadata);
          g_hash_table_insert (mux->metadata,
              &MXF_METADATA_BASE (track)->instance_uid, track);
          mux->metadata_list = g_list_prepend (mux->metadata_list, track);

          track->parent.track_id = n + 1;
          track->parent.track_number = 0;

          caps = gst_pad_get_current_caps (GST_PAD_CAST (pad));
          buffer = gst_aggregator_pad_peek_buffer (GST_AGGREGATOR_PAD (pad));
          pad->writer->get_edit_rate (pad->descriptor,
              caps, pad->mapping_data,
              buffer, source_package, source_track, &track->edit_rate);
          if (buffer)
            gst_buffer_unref (buffer);
          gst_caps_unref (caps);

          if (track->edit_rate.n != source_track->edit_rate.n ||
              track->edit_rate.d != source_track->edit_rate.d) {
            memcpy (&source_track->edit_rate, &track->edit_rate,
                sizeof (MXFFraction));
          }

          if (track->edit_rate.d <= 0 || track->edit_rate.n <= 0) {
            GST_ERROR_OBJECT (mux, "Invalid edit rate");
            GST_OBJECT_UNLOCK (mux);
            return GST_FLOW_ERROR;
          }

          if (min_edit_rate_d >
              ((gdouble) track->edit_rate.n) / ((gdouble) track->edit_rate.d)) {
            min_edit_rate_d =
                ((gdouble) track->edit_rate.n) / ((gdouble) track->edit_rate.d);
            memcpy (&min_edit_rate, &track->edit_rate, sizeof (MXFFraction));
          }

          sequence = track->parent.sequence = (MXFMetadataSequence *)
              g_object_new (MXF_TYPE_METADATA_SEQUENCE, NULL);
          mxf_uuid_init (&MXF_METADATA_BASE (sequence)->instance_uid,
              mux->metadata);
          g_hash_table_insert (mux->metadata,
              &MXF_METADATA_BASE (sequence)->instance_uid, sequence);
          mux->metadata_list = g_list_prepend (mux->metadata_list, sequence);

          memcpy (&sequence->data_definition, &pad->writer->data_definition,
              16);
          sequence->n_structural_components = 1;
          sequence->structural_components =
              g_new0 (MXFMetadataStructuralComponent *, 1);

          clip = (MXFMetadataSourceClip *)
              g_object_new (MXF_TYPE_METADATA_SOURCE_CLIP, NULL);
          sequence->structural_components[0] =
              (MXFMetadataStructuralComponent *) clip;
          mxf_uuid_init (&MXF_METADATA_BASE (clip)->instance_uid,
              mux->metadata);
          g_hash_table_insert (mux->metadata,
              &MXF_METADATA_BASE (clip)->instance_uid, clip);
          mux->metadata_list = g_list_prepend (mux->metadata_list, clip);

          memcpy (&clip->parent.data_definition, &sequence->data_definition,
              16);
          clip->start_position = 0;

          memcpy (&clip->source_package_id, &cstorage->packages[1]->package_uid,
              32);
          clip->source_track_id = n + 1;

          n++;
        }

        n = 0;
        /* Timecode track */
        {
          MXFMetadataTimelineTrack *track;
          MXFMetadataSequence *sequence;
          MXFMetadataTimecodeComponent *component;

          p->tracks[n] = (MXFMetadataTrack *)
              g_object_new (MXF_TYPE_METADATA_TIMELINE_TRACK, NULL);
          track = (MXFMetadataTimelineTrack *) p->tracks[n];
          mxf_uuid_init (&MXF_METADATA_BASE (track)->instance_uid,
              mux->metadata);
          g_hash_table_insert (mux->metadata,
              &MXF_METADATA_BASE (track)->instance_uid, track);
          mux->metadata_list = g_list_prepend (mux->metadata_list, track);

          track->parent.track_id = n + 1;
          track->parent.track_number = 0;
          track->parent.track_name = g_strdup ("Timecode track");
          /* FIXME: Is this correct? */
          memcpy (&track->edit_rate, &min_edit_rate, sizeof (MXFFraction));

          sequence = track->parent.sequence = (MXFMetadataSequence *)
              g_object_new (MXF_TYPE_METADATA_SEQUENCE, NULL);
          mxf_uuid_init (&MXF_METADATA_BASE (sequence)->instance_uid,
              mux->metadata);
          g_hash_table_insert (mux->metadata,
              &MXF_METADATA_BASE (sequence)->instance_uid, sequence);
          mux->metadata_list = g_list_prepend (mux->metadata_list, sequence);

          memcpy (&sequence->data_definition,
              mxf_metadata_track_identifier_get
              (MXF_METADATA_TRACK_TIMECODE_12M_INACTIVE), 16);

          sequence->n_structural_components = 1;
          sequence->structural_components =
              g_new0 (MXFMetadataStructuralComponent *, 1);

          component = (MXFMetadataTimecodeComponent *)
              g_object_new (MXF_TYPE_METADATA_TIMECODE_COMPONENT, NULL);
          sequence->structural_components[0] =
              (MXFMetadataStructuralComponent *) component;
          mxf_uuid_init (&MXF_METADATA_BASE (component)->instance_uid,
              mux->metadata);
          g_hash_table_insert (mux->metadata,
              &MXF_METADATA_BASE (component)->instance_uid, component);
          mux->metadata_list = g_list_prepend (mux->metadata_list, component);

          memcpy (&component->parent.data_definition,
              &sequence->data_definition, 16);

          component->start_timecode = 0;
          if (track->edit_rate.d == 0)
            component->rounded_timecode_base = 1;
          else
            component->rounded_timecode_base =
                (((gdouble) track->edit_rate.n) /
                ((gdouble) track->edit_rate.d) + 0.5);
          /* TODO: drop frame */
        }

        memcpy (&mux->min_edit_rate, &min_edit_rate, sizeof (MXFFraction));
      }
    }

    /* Timecode track */
    {
      MXFMetadataSourcePackage *p;
      MXFMetadataTimelineTrack *track;
      MXFMetadataSequence *sequence;
      MXFMetadataTimecodeComponent *component;
      guint n = 0;

      p = (MXFMetadataSourcePackage *) cstorage->packages[1];

      p->parent.tracks[n] = (MXFMetadataTrack *)
          g_object_new (MXF_TYPE_METADATA_TIMELINE_TRACK, NULL);
      track = (MXFMetadataTimelineTrack *) p->parent.tracks[n];
      mxf_uuid_init (&MXF_METADATA_BASE (track)->instance_uid, mux->metadata);
      g_hash_table_insert (mux->metadata,
          &MXF_METADATA_BASE (track)->instance_uid, track);
      mux->metadata_list = g_list_prepend (mux->metadata_list, track);

      track->parent.track_id = n + 1;
      track->parent.track_number = 0;
      track->parent.track_name = g_strdup ("Timecode track");
      /* FIXME: Is this correct? */
      memcpy (&track->edit_rate, &mux->min_edit_rate, sizeof (MXFFraction));

      sequence = track->parent.sequence = (MXFMetadataSequence *)
          g_object_new (MXF_TYPE_METADATA_SEQUENCE, NULL);
      mxf_uuid_init (&MXF_METADATA_BASE (sequence)->instance_uid,
          mux->metadata);
      g_hash_table_insert (mux->metadata,
          &MXF_METADATA_BASE (sequence)->instance_uid, sequence);
      mux->metadata_list = g_list_prepend (mux->metadata_list, sequence);

      memcpy (&sequence->data_definition,
          mxf_metadata_track_identifier_get
          (MXF_METADATA_TRACK_TIMECODE_12M_INACTIVE), 16);

      sequence->n_structural_components = 1;
      sequence->structural_components =
          g_new0 (MXFMetadataStructuralComponent *, 1);

      component = (MXFMetadataTimecodeComponent *)
          g_object_new (MXF_TYPE_METADATA_TIMECODE_COMPONENT, NULL);
      sequence->structural_components[0] =
          (MXFMetadataStructuralComponent *) component;
      mxf_uuid_init (&MXF_METADATA_BASE (component)->instance_uid,
          mux->metadata);
      g_hash_table_insert (mux->metadata,
          &MXF_METADATA_BASE (component)->instance_uid, component);
      mux->metadata_list = g_list_prepend (mux->metadata_list, component);

      memcpy (&component->parent.data_definition,
          &sequence->data_definition, 16);

      component->start_timecode = 0;
      if (track->edit_rate.d == 0)
        component->rounded_timecode_base = 1;
      else
        component->rounded_timecode_base =
            (((gdouble) track->edit_rate.n) /
            ((gdouble) track->edit_rate.d) + 0.5);
      /* TODO: drop frame */
    }


    for (i = 1; i < cstorage->packages[1]->n_tracks; i++) {
      MXFMetadataTrack *track = cstorage->packages[1]->tracks[i];
      guint j;
      guint32 templ;
      guint8 n_type, n;

      if ((track->track_number & 0x00ff00ff) != 0)
        continue;

      templ = track->track_number;
      n_type = 0;

      for (j = 1; j < cstorage->packages[1]->n_tracks; j++) {
        MXFMetadataTrack *tmp = cstorage->packages[1]->tracks[j];

        if (tmp->track_number == templ) {
          n_type++;
        }
      }

      n = 0;
      for (j = 1; j < cstorage->packages[1]->n_tracks; j++) {
        MXFMetadataTrack *tmp = cstorage->packages[1]->tracks[j];

        if (tmp->track_number == templ) {
          n++;
          tmp->track_number |= (n_type << 16) | (n);
        }
      }
    }

    cstorage->n_essence_container_data = 1;
    cstorage->essence_container_data =
        g_new0 (MXFMetadataEssenceContainerData *, 1);
    cstorage->essence_container_data[0] = (MXFMetadataEssenceContainerData *)
        g_object_new (MXF_TYPE_METADATA_ESSENCE_CONTAINER_DATA, NULL);
    mxf_uuid_init (&MXF_METADATA_BASE (cstorage->essence_container_data[0])->
        instance_uid, mux->metadata);
    g_hash_table_insert (mux->metadata,
        &MXF_METADATA_BASE (cstorage->essence_container_data[0])->instance_uid,
        cstorage->essence_container_data[0]);
    mux->metadata_list =
        g_list_prepend (mux->metadata_list,
        cstorage->essence_container_data[0]);

    cstorage->essence_container_data[0]->linked_package =
        MXF_METADATA_SOURCE_PACKAGE (cstorage->packages[1]);
    cstorage->essence_container_data[0]->index_sid = 2;
    cstorage->essence_container_data[0]->body_sid = 1;
  }

  /* Sort descriptors at the correct places */
  {
    GList *l;
    GList *descriptors = NULL;

    for (l = mux->metadata_list; l; l = l->next) {
      MXFMetadataBase *m = l->data;

      if (MXF_IS_METADATA_GENERIC_DESCRIPTOR (m)
          && !MXF_IS_METADATA_MULTIPLE_DESCRIPTOR (m)) {
        descriptors = l;
        l->prev->next = NULL;
        l->prev = NULL;
        break;
      }
    }

    g_assert (descriptors != NULL);

    for (l = mux->metadata_list; l; l = l->next) {
      MXFMetadataBase *m = l->data;
      GList *s;

      if (MXF_IS_METADATA_MULTIPLE_DESCRIPTOR (m) ||
          MXF_IS_METADATA_SOURCE_PACKAGE (m)) {
        s = l->prev;
        l->prev = g_list_last (descriptors);
        s->next = descriptors;
        descriptors->prev = s;
        l->prev->next = l;
        break;
      }
    }
  }

  GST_OBJECT_UNLOCK (mux);

  mux->metadata_list = g_list_reverse (mux->metadata_list);

  return ret;
}

static GstFlowReturn
gst_mxf_mux_init_partition_pack (GstMXFMux * mux)
{
  GList *l;
  guint i = 0;

  mxf_partition_pack_reset (&mux->partition);
  mux->partition.type = MXF_PARTITION_PACK_HEADER;
  mux->partition.closed = mux->partition.complete = FALSE;
  mux->partition.major_version = 0x0001;
  mux->partition.minor_version = 0x0002;
  mux->partition.kag_size = 1;
  mux->partition.this_partition = 0;
  mux->partition.prev_partition = 0;
  mux->partition.footer_partition = 0;
  mux->partition.header_byte_count = 0;
  mux->partition.index_byte_count = 0;
  mux->partition.index_sid = 0;
  mux->partition.body_offset = 0;
  mux->partition.body_sid = 0;

  memcpy (&mux->partition.operational_pattern,
      &mux->preface->operational_pattern, 16);

  GST_OBJECT_LOCK (mux);
  mux->partition.n_essence_containers = GST_ELEMENT_CAST (mux)->numsinkpads;
  mux->partition.essence_containers =
      g_new0 (MXFUL, mux->partition.n_essence_containers);

  for (l = GST_ELEMENT_CAST (mux)->sinkpads; l; l = l->next) {
    GstMXFMuxPad *pad = l->data;
    guint j;
    gboolean found = FALSE;

    for (j = 0; j <= i; j++) {
      if (mxf_ul_is_equal (&pad->descriptor->essence_container,
              &mux->partition.essence_containers[j])) {
        found = TRUE;
        break;
      }
    }

    if (found)
      continue;

    memcpy (&mux->partition.essence_containers[i],
        &pad->descriptor->essence_container, 16);
    i++;
  }
  mux->partition.n_essence_containers = i;
  GST_OBJECT_UNLOCK (mux);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mxf_mux_write_header_metadata (GstMXFMux * mux)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buf;
  GList *buffers = NULL;
  GList *l;
  MXFMetadataBase *m;
  guint64 header_byte_count = 0;

  for (l = mux->metadata_list; l; l = l->next) {
    m = l->data;
    buf = mxf_metadata_base_to_buffer (m, &mux->primer);
    header_byte_count += gst_buffer_get_size (buf);
    buffers = g_list_prepend (buffers, buf);
  }

  buffers = g_list_reverse (buffers);
  buf = mxf_primer_pack_to_buffer (&mux->primer);
  header_byte_count += gst_buffer_get_size (buf);
  buffers = g_list_prepend (buffers, buf);

  mux->partition.header_byte_count = header_byte_count;
  buf = mxf_partition_pack_to_buffer (&mux->partition);
  if ((ret = gst_mxf_mux_push (mux, buf)) != GST_FLOW_OK) {
    GST_ERROR_OBJECT (mux, "Failed pushing partition: %s",
        gst_flow_get_name (ret));
    g_list_foreach (buffers, (GFunc) gst_mini_object_unref, NULL);
    g_list_free (buffers);
    return ret;
  }

  for (l = buffers; l; l = l->next) {
    buf = l->data;
    l->data = NULL;
    if ((ret = gst_mxf_mux_push (mux, buf)) != GST_FLOW_OK) {
      GST_ERROR_OBJECT (mux, "Failed pushing buffer: %s",
          gst_flow_get_name (ret));
      g_list_foreach (l, (GFunc) gst_mini_object_unref, NULL);
      g_list_free (buffers);
      return ret;
    }
  }

  g_list_free (buffers);

  return ret;
}

static const guint8 _gc_essence_element_ul[] = {
  0x06, 0x0e, 0x2b, 0x34, 0x01, 0x02, 0x01, 0x01,
  0x0d, 0x01, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00
};

static GstFlowReturn
gst_mxf_mux_handle_buffer (GstMXFMux * mux, GstMXFMuxPad * pad)
{
  GstBuffer *buf = gst_aggregator_pad_peek_buffer (GST_AGGREGATOR_PAD (pad));
  GstBuffer *outbuf = NULL;
  GstMapInfo map;
  gsize buf_size;
  GstFlowReturn ret = GST_FLOW_OK;
  guint8 slen, ber[9];
  gboolean flush = gst_aggregator_pad_is_eos (GST_AGGREGATOR_PAD (pad))
      && !pad->have_complete_edit_unit && buf == NULL;
  gboolean is_keyframe = buf ?
      !GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT) : TRUE;
  GstClockTime pts = buf ? GST_BUFFER_PTS (buf) : GST_CLOCK_TIME_NONE;
  GstClockTime dts = buf ? GST_BUFFER_DTS (buf) : GST_CLOCK_TIME_NONE;

  if (pad->have_complete_edit_unit) {
    GST_DEBUG_OBJECT (pad,
        "Handling remaining buffer for track %u at position %" G_GINT64_FORMAT,
        pad->source_track->parent.track_id, pad->pos);
    if (buf)
      gst_buffer_unref (buf);
    buf = NULL;
  } else if (!flush) {
    if (buf)
      gst_buffer_unref (buf);
    buf = gst_aggregator_pad_pop_buffer (GST_AGGREGATOR_PAD (pad));
  }

  if (buf) {
    GST_DEBUG_OBJECT (pad,
        "Handling buffer of size %" G_GSIZE_FORMAT " for track %u at position %"
        G_GINT64_FORMAT, gst_buffer_get_size (buf),
        pad->source_track->parent.track_id, pad->pos);
  } else {
    flush = TRUE;
    GST_DEBUG_OBJECT (pad,
        "Flushing for track %u at position %" G_GINT64_FORMAT,
        pad->source_track->parent.track_id, pad->pos);
  }

  ret = pad->write_func (buf, pad->mapping_data, pad->adapter, &outbuf, flush);
  if (ret != GST_FLOW_OK && ret != GST_FLOW_CUSTOM_SUCCESS) {
    GST_ERROR_OBJECT (pad,
        "Failed handling buffer for track %u, reason %s",
        pad->source_track->parent.track_id, gst_flow_get_name (ret));
    return ret;
  }

  if (ret == GST_FLOW_CUSTOM_SUCCESS) {
    pad->have_complete_edit_unit = TRUE;
    ret = GST_FLOW_OK;
  } else {
    pad->have_complete_edit_unit = FALSE;
  }

  buf = outbuf;
  if (buf == NULL)
    return ret;

  /* We currently only index the first essence stream */
  if (pad == (GstMXFMuxPad *) GST_ELEMENT_CAST (mux)->sinkpads->data) {
    MXFIndexTableSegment *segment;
    const gint max_segment_size = G_MAXUINT16 / 11;

    if (mux->index_table->len == 0 ||
        g_array_index (mux->index_table, MXFIndexTableSegment,
            mux->current_index_pos).index_duration >= max_segment_size) {

      if (mux->index_table->len > 0)
        mux->current_index_pos++;

      if (mux->index_table->len <= mux->current_index_pos) {
        MXFIndexTableSegment s;

        memset (&segment, 0, sizeof (segment));

        mxf_uuid_init (&s.instance_id, mux->metadata);
        memcpy (&s.index_edit_rate, &pad->source_track->edit_rate,
            sizeof (s.index_edit_rate));
        if (mux->index_table->len > 0)
          s.index_start_position =
              g_array_index (mux->index_table, MXFIndexTableSegment,
              mux->index_table->len - 1).index_start_position;
        else
          s.index_start_position = 0;
        s.index_duration = 0;
        s.edit_unit_byte_count = 0;
        s.index_sid =
            mux->preface->content_storage->essence_container_data[0]->index_sid;
        s.body_sid =
            mux->preface->content_storage->essence_container_data[0]->body_sid;
        s.slice_count = 0;
        s.pos_table_count = 0;
        s.n_delta_entries = 0;
        s.delta_entries = NULL;
        s.n_index_entries = 0;
        s.index_entries = g_new0 (MXFIndexEntry, max_segment_size);
        g_array_append_val (mux->index_table, s);
      }
    }
    segment =
        &g_array_index (mux->index_table, MXFIndexTableSegment,
        mux->current_index_pos);

    if (dts != GST_CLOCK_TIME_NONE && pts != GST_CLOCK_TIME_NONE) {
      guint64 pts_pos;
      guint64 pts_index_pos, pts_segment_pos;
      gint64 index_pos_diff;
      MXFIndexTableSegment *pts_segment;

      pts =
          gst_segment_to_running_time (&pad->parent.segment, GST_FORMAT_TIME,
          pts);
      pts_pos =
          gst_util_uint64_scale_round (pts, pad->source_track->edit_rate.n,
          pad->source_track->edit_rate.d * GST_SECOND);

      index_pos_diff = pts_pos - pad->pos;
      pts_index_pos = mux->current_index_pos;
      pts_segment_pos = segment->n_index_entries;
      if (index_pos_diff >= 0) {
        while (pts_segment_pos + index_pos_diff >= max_segment_size) {
          index_pos_diff -= max_segment_size - pts_segment_pos;
          pts_segment_pos = 0;
          pts_index_pos++;

          if (pts_index_pos >= mux->index_table->len) {
            MXFIndexTableSegment s;

            memset (&segment, 0, sizeof (segment));

            mxf_uuid_init (&s.instance_id, mux->metadata);
            memcpy (&s.index_edit_rate, &pad->source_track->edit_rate,
                sizeof (s.index_edit_rate));
            if (mux->index_table->len > 0)
              s.index_start_position =
                  g_array_index (mux->index_table, MXFIndexTableSegment,
                  mux->index_table->len - 1).index_start_position;
            else
              s.index_start_position = 0;
            s.index_duration = 0;
            s.edit_unit_byte_count = 0;
            s.index_sid =
                mux->preface->content_storage->
                essence_container_data[0]->index_sid;
            s.body_sid =
                mux->preface->content_storage->
                essence_container_data[0]->body_sid;
            s.slice_count = 0;
            s.pos_table_count = 0;
            s.n_delta_entries = 0;
            s.delta_entries = NULL;
            s.n_index_entries = 0;
            s.index_entries = g_new0 (MXFIndexEntry, max_segment_size);
            g_array_append_val (mux->index_table, s);
          }
        }
      } else {
        while (pts_segment_pos + index_pos_diff <= 0) {
          if (pts_index_pos == 0) {
            pts_index_pos = G_MAXUINT64;
            break;
          }
          index_pos_diff += pts_segment_pos;
          pts_segment_pos = max_segment_size;
          pts_index_pos--;
        }
      }
      if (pts_index_pos != G_MAXUINT64) {
        g_assert (index_pos_diff < 127 && index_pos_diff >= -127);
        pts_segment =
            &g_array_index (mux->index_table, MXFIndexTableSegment,
            pts_index_pos);
        pts_segment->index_entries[pts_segment_pos +
            index_pos_diff].temporal_offset = -index_pos_diff;
      }
    }

    /* Leave temporal offset initialized at 0, above code will set it as necessary */
    ;
    if (is_keyframe)
      mux->last_keyframe_pos = pad->pos;
    segment->index_entries[segment->n_index_entries].key_frame_offset =
        MIN (pad->pos - mux->last_keyframe_pos, 127);
    segment->index_entries[segment->n_index_entries].flags = is_keyframe ? 0x80 : 0x20; /* FIXME: Need to distinguish all the cases */
    segment->index_entries[segment->n_index_entries].stream_offset =
        mux->partition.body_offset;

    segment->n_index_entries++;
    segment->index_duration++;
  }

  buf_size = gst_buffer_get_size (buf);
  slen = mxf_ber_encode_size (buf_size, ber);
  outbuf = gst_buffer_new_and_alloc (16 + slen);
  gst_buffer_map (outbuf, &map, GST_MAP_WRITE);
  memcpy (map.data, _gc_essence_element_ul, 16);
  GST_WRITE_UINT32_BE (map.data + 12, pad->source_track->parent.track_number);
  memcpy (map.data + 16, ber, slen);
  gst_buffer_unmap (outbuf, &map);
  outbuf = gst_buffer_append (outbuf, buf);

  GST_DEBUG_OBJECT (pad,
      "Pushing buffer of size %" G_GSIZE_FORMAT " for track %u",
      gst_buffer_get_size (outbuf), pad->source_track->parent.track_id);

  mux->partition.body_offset += gst_buffer_get_size (outbuf);
  if ((ret = gst_mxf_mux_push (mux, outbuf)) != GST_FLOW_OK) {
    GST_ERROR_OBJECT (pad,
        "Failed pushing buffer for track %u, reason %s",
        pad->source_track->parent.track_id, gst_flow_get_name (ret));
    return ret;
  }

  pad->pos++;
  pad->last_timestamp =
      gst_util_uint64_scale (GST_SECOND * pad->pos,
      pad->source_track->edit_rate.d, pad->source_track->edit_rate.n);

  return ret;
}

static GstFlowReturn
gst_mxf_mux_write_body_partition (GstMXFMux * mux)
{
  GstBuffer *buf;

  mux->partition.type = MXF_PARTITION_PACK_BODY;
  mux->partition.closed = TRUE;
  mux->partition.complete = TRUE;
  mux->partition.this_partition = mux->offset;
  mux->partition.prev_partition = 0;
  mux->partition.footer_partition = 0;
  mux->partition.header_byte_count = 0;
  mux->partition.index_byte_count = 0;
  mux->partition.index_sid = 0;
  mux->partition.body_offset = 0;
  mux->partition.body_sid =
      mux->preface->content_storage->essence_container_data[0]->body_sid;

  buf = mxf_partition_pack_to_buffer (&mux->partition);
  return gst_mxf_mux_push (mux, buf);
}

static GstFlowReturn
gst_mxf_mux_handle_eos (GstMXFMux * mux)
{
  GList *l;
  gboolean have_data = FALSE;
  GstBuffer *packet;

  do {
    GstMXFMuxPad *best = NULL;

    have_data = FALSE;

    GST_OBJECT_LOCK (mux);
    for (l = GST_ELEMENT_CAST (mux)->sinkpads; l; l = l->next) {
      GstMXFMuxPad *pad = l->data;
      GstBuffer *buffer =
          gst_aggregator_pad_peek_buffer (GST_AGGREGATOR_PAD (pad));

      GstClockTime next_gc_timestamp =
          gst_util_uint64_scale ((mux->last_gc_position + 1) * GST_SECOND,
          mux->min_edit_rate.d, mux->min_edit_rate.n);

      if (pad->have_complete_edit_unit ||
          gst_adapter_available (pad->adapter) > 0 || buffer) {
        have_data = TRUE;
        if (pad->last_timestamp < next_gc_timestamp) {
          best = gst_object_ref (pad);
          if (buffer)
            gst_buffer_unref (buffer);
          break;
        }
      }
      if (buffer)
        gst_buffer_unref (buffer);

      if (have_data && !l->next) {
        mux->last_gc_position++;
        mux->last_gc_timestamp = next_gc_timestamp;
        break;
      }
    }
    GST_OBJECT_UNLOCK (mux);

    if (best) {
      gst_mxf_mux_handle_buffer (mux, best);
      gst_object_unref (best);
      have_data = TRUE;
    }
  } while (have_data);

  mux->last_gc_position++;
  mux->last_gc_timestamp =
      gst_util_uint64_scale (mux->last_gc_position * GST_SECOND,
      mux->min_edit_rate.d, mux->min_edit_rate.n);

  /* Update essence track durations */
  GST_OBJECT_LOCK (mux);
  for (l = GST_ELEMENT_CAST (mux)->sinkpads; l; l = l->next) {
    GstMXFMuxPad *pad = l->data;
    guint i;

    /* Update durations */
    pad->source_track->parent.sequence->duration = pad->pos;
    MXF_METADATA_SOURCE_CLIP (pad->source_track->parent.
        sequence->structural_components[0])->parent.duration = pad->pos;
    for (i = 0; i < mux->preface->content_storage->packages[0]->n_tracks; i++) {
      MXFMetadataTimelineTrack *track;

      if (!MXF_IS_METADATA_TIMELINE_TRACK (mux->preface->
              content_storage->packages[0]->tracks[i])
          || !MXF_IS_METADATA_SOURCE_CLIP (mux->preface->
              content_storage->packages[0]->tracks[i]->sequence->
              structural_components[0]))
        continue;

      track =
          MXF_METADATA_TIMELINE_TRACK (mux->preface->
          content_storage->packages[0]->tracks[i]);
      if (MXF_METADATA_SOURCE_CLIP (track->parent.
              sequence->structural_components[0])->source_track_id ==
          pad->source_track->parent.track_id) {
        track->parent.sequence->structural_components[0]->duration = pad->pos;
        track->parent.sequence->duration = pad->pos;
      }
    }
  }
  GST_OBJECT_UNLOCK (mux);

  /* Update timecode track duration */
  {
    MXFMetadataTimelineTrack *track =
        MXF_METADATA_TIMELINE_TRACK (mux->preface->
        content_storage->packages[0]->tracks[0]);
    MXFMetadataSequence *sequence = track->parent.sequence;
    MXFMetadataTimecodeComponent *component =
        MXF_METADATA_TIMECODE_COMPONENT (sequence->structural_components[0]);

    sequence->duration = mux->last_gc_position;
    component->parent.duration = mux->last_gc_position;
  }

  {
    MXFMetadataTimelineTrack *track =
        MXF_METADATA_TIMELINE_TRACK (mux->preface->
        content_storage->packages[1]->tracks[0]);
    MXFMetadataSequence *sequence = track->parent.sequence;
    MXFMetadataTimecodeComponent *component =
        MXF_METADATA_TIMECODE_COMPONENT (sequence->structural_components[0]);

    sequence->duration = mux->last_gc_position;
    component->parent.duration = mux->last_gc_position;
  }

  {
    guint64 body_partition = mux->partition.this_partition;
    guint32 body_sid = mux->partition.body_sid;
    guint64 footer_partition = mux->offset;
    GArray *rip;
    GstFlowReturn ret;
    GstSegment segment;
    MXFRandomIndexPackEntry entry;
    GList *index_entries = NULL, *l;
    guint index_byte_count = 0;
    guint i;
    GstBuffer *buf;

    for (i = 0; i < mux->index_table->len; i++) {
      MXFIndexTableSegment *segment =
          &g_array_index (mux->index_table, MXFIndexTableSegment, i);
      GstBuffer *segment_buffer = mxf_index_table_segment_to_buffer (segment);

      index_byte_count += gst_buffer_get_size (segment_buffer);
      index_entries = g_list_prepend (index_entries, segment_buffer);
    }

    mux->partition.type = MXF_PARTITION_PACK_FOOTER;
    mux->partition.closed = TRUE;
    mux->partition.complete = TRUE;
    mux->partition.this_partition = mux->offset;
    mux->partition.prev_partition = body_partition;
    mux->partition.footer_partition = mux->offset;
    mux->partition.header_byte_count = 0;
    mux->partition.index_byte_count = index_byte_count;
    mux->partition.index_sid =
        mux->preface->content_storage->essence_container_data[0]->index_sid;
    mux->partition.body_offset = 0;
    mux->partition.body_sid = 0;

    gst_mxf_mux_write_header_metadata (mux);

    index_entries = g_list_reverse (index_entries);
    for (l = index_entries; l; l = l->next) {
      if ((ret = gst_mxf_mux_push (mux, l->data)) != GST_FLOW_OK) {
        GST_ERROR_OBJECT (mux, "Failed pushing index table segment");
      }
    }
    g_list_free (index_entries);

    rip = g_array_sized_new (FALSE, FALSE, sizeof (MXFRandomIndexPackEntry), 3);
    entry.offset = 0;
    entry.body_sid = 0;
    g_array_append_val (rip, entry);
    entry.offset = body_partition;
    entry.body_sid = body_sid;
    g_array_append_val (rip, entry);
    entry.offset = footer_partition;
    entry.body_sid = 0;
    g_array_append_val (rip, entry);

    packet = mxf_random_index_pack_to_buffer (rip);
    if ((ret = gst_mxf_mux_push (mux, packet)) != GST_FLOW_OK) {
      GST_ERROR_OBJECT (mux, "Failed pushing random index pack");
    }
    g_array_free (rip, TRUE);

    /* Rewrite header partition with updated values */
    gst_segment_init (&segment, GST_FORMAT_BYTES);
    if (gst_pad_push_event (GST_AGGREGATOR_SRC_PAD (mux),
            gst_event_new_segment (&segment))) {
      mux->offset = 0;
      mux->partition.type = MXF_PARTITION_PACK_HEADER;
      mux->partition.closed = TRUE;
      mux->partition.complete = TRUE;
      mux->partition.this_partition = 0;
      mux->partition.prev_partition = 0;
      mux->partition.footer_partition = footer_partition;
      mux->partition.header_byte_count = 0;
      mux->partition.index_byte_count = 0;
      mux->partition.index_sid = 0;
      mux->partition.body_offset = 0;
      mux->partition.body_sid = 0;

      ret = gst_mxf_mux_write_header_metadata (mux);
      if (ret != GST_FLOW_OK) {
        GST_ERROR_OBJECT (mux, "Rewriting header partition failed");
        return ret;
      }

      g_assert (mux->offset == body_partition);

      mux->partition.type = MXF_PARTITION_PACK_BODY;
      mux->partition.closed = TRUE;
      mux->partition.complete = TRUE;
      mux->partition.this_partition = mux->offset;
      mux->partition.prev_partition = 0;
      mux->partition.footer_partition = footer_partition;
      mux->partition.header_byte_count = 0;
      mux->partition.index_byte_count = 0;
      mux->partition.index_sid = 0;
      mux->partition.body_offset = 0;
      mux->partition.body_sid =
          mux->preface->content_storage->essence_container_data[0]->body_sid;

      buf = mxf_partition_pack_to_buffer (&mux->partition);
      ret = gst_mxf_mux_push (mux, buf);
      if (ret != GST_FLOW_OK) {
        GST_ERROR_OBJECT (mux, "Rewriting body partition failed");
        return ret;
      }
    } else {
      GST_WARNING_OBJECT (mux, "Can't rewrite header partition");
    }
  }

  return GST_FLOW_OK;
}

static gint
_sort_mux_pads (gconstpointer a, gconstpointer b)
{
  const GstMXFMuxPad *pa = a, *pb = b;
  MXFMetadataTrackType ta =
      mxf_metadata_track_identifier_parse (&pa->writer->data_definition);
  MXFMetadataTrackType tb =
      mxf_metadata_track_identifier_parse (&pb->writer->data_definition);

  if (ta != tb)
    return ta - tb;

  return pa->source_track->parent.track_number -
      pa->source_track->parent.track_number;
}


static gboolean
gst_mxf_mux_stop (GstAggregator * aggregator)
{
  GstMXFMux *mux = GST_MXF_MUX (aggregator);

  gst_mxf_mux_reset (mux);

  return TRUE;
}

static GstFlowReturn
gst_mxf_mux_aggregate (GstAggregator * aggregator, gboolean timeout)
{
  GstMXFMux *mux = GST_MXF_MUX (aggregator);
  GstMXFMuxPad *best = NULL;
  GstFlowReturn ret;
  GList *l;
  gboolean eos = TRUE;

  if (timeout) {
    GST_ELEMENT_ERROR (mux, STREAM, MUX, (NULL),
        ("Live mixing and got a timeout. This is not supported yet"));
    ret = GST_FLOW_ERROR;
    goto error;
  }

  if (mux->state == GST_MXF_MUX_STATE_ERROR) {
    GST_ERROR_OBJECT (mux, "Had an error before -- returning");
    return GST_FLOW_ERROR;
  } else if (mux->state == GST_MXF_MUX_STATE_EOS) {
    GST_WARNING_OBJECT (mux, "EOS");
    return GST_FLOW_EOS;
  }

  if (mux->state == GST_MXF_MUX_STATE_HEADER) {
    GstCaps *caps;

    if (GST_ELEMENT_CAST (mux)->sinkpads == NULL) {
      GST_ELEMENT_ERROR (mux, STREAM, MUX, (NULL),
          ("No input streams configured"));
      ret = GST_FLOW_ERROR;
      goto error;
    }

    caps = gst_caps_new_empty_simple ("application/mxf");
    gst_aggregator_set_src_caps (GST_AGGREGATOR (mux), caps);
    gst_caps_unref (caps);

    if ((ret = gst_mxf_mux_create_metadata (mux)) != GST_FLOW_OK)
      goto error;

    if ((ret = gst_mxf_mux_init_partition_pack (mux)) != GST_FLOW_OK)
      goto error;

    if ((ret = gst_mxf_mux_write_header_metadata (mux)) != GST_FLOW_OK)
      goto error;

    /* Sort pads, we will always write in that order */
    GST_OBJECT_LOCK (mux);
    GST_ELEMENT_CAST (mux)->sinkpads =
        g_list_sort (GST_ELEMENT_CAST (mux)->sinkpads, _sort_mux_pads);
    GST_OBJECT_UNLOCK (mux);

    /* Write body partition */
    ret = gst_mxf_mux_write_body_partition (mux);
    if (ret != GST_FLOW_OK)
      goto error;
    mux->state = GST_MXF_MUX_STATE_DATA;
  }

  g_return_val_if_fail (g_hash_table_size (mux->metadata) > 0, GST_FLOW_ERROR);

  do {
    GST_OBJECT_LOCK (mux);
    for (l = GST_ELEMENT_CAST (mux)->sinkpads; l; l = l->next) {
      gboolean pad_eos;
      GstMXFMuxPad *pad = l->data;
      GstBuffer *buffer;
      GstClockTime next_gc_timestamp =
          gst_util_uint64_scale ((mux->last_gc_position + 1) * GST_SECOND,
          mux->min_edit_rate.d, mux->min_edit_rate.n);

      pad_eos = gst_aggregator_pad_is_eos (GST_AGGREGATOR_PAD (pad));
      if (!pad_eos)
        eos = FALSE;

      buffer = gst_aggregator_pad_peek_buffer (GST_AGGREGATOR_PAD (pad));

      if ((!pad_eos || pad->have_complete_edit_unit ||
              gst_adapter_available (pad->adapter) > 0 || buffer)
          && pad->last_timestamp < next_gc_timestamp) {
        if (buffer)
          gst_buffer_unref (buffer);
        best = gst_object_ref (pad);
        break;
      } else if (!eos && !l->next) {
        mux->last_gc_position++;
        mux->last_gc_timestamp = next_gc_timestamp;
        eos = FALSE;
        if (buffer)
          gst_buffer_unref (buffer);
        best = NULL;
        break;
      }
      if (buffer)
        gst_buffer_unref (buffer);
    }
    GST_OBJECT_UNLOCK (mux);
  } while (!eos && best == NULL);

  if (!eos && best) {
    ret = gst_mxf_mux_handle_buffer (mux, best);
    gst_object_unref (best);
    if (ret != GST_FLOW_OK)
      goto error;
  } else if (eos) {
    GST_DEBUG_OBJECT (mux, "Handling EOS");

    if (best)
      gst_object_unref (best);

    gst_mxf_mux_handle_eos (mux);
    mux->state = GST_MXF_MUX_STATE_EOS;
    return GST_FLOW_EOS;
  } else {
    g_assert_not_reached ();
  }

  return GST_FLOW_OK;

error:
  {
    mux->state = GST_MXF_MUX_STATE_ERROR;
    return ret;
  }
}
