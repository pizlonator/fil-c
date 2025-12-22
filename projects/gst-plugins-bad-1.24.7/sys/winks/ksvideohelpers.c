/*
 * Copyright (C) 2007 Haakon Sporsheim <hakon.sporsheim@tandberg.com>
 *               2008 Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>
 *               2009 Knut Inge Hvidsten <knut.inge.hvidsten@tandberg.com>
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

#include "ksvideohelpers.h"

#include <math.h>
#include <uuids.h>
#include "kshelpers.h"

GST_DEBUG_CATEGORY_EXTERN (gst_ks_debug);
#define GST_CAT_DEFAULT gst_ks_debug

static const GUID MEDIASUBTYPE_FOURCC =
    { 0x0 /* FourCC here */ , 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00,
    0x38, 0x9B, 0x71}
};

typedef struct _KsVideoDeviceEntry KsVideoDeviceEntry;

struct _KsVideoDeviceEntry
{
  KsDeviceEntry *device;
  gint priority;
};

static void
ks_video_device_entry_decide_priority (KsVideoDeviceEntry * videodevice)
{
  HANDLE filter_handle;

  videodevice->priority = 0;

  filter_handle = CreateFile (videodevice->device->path,
      GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
  if (ks_is_valid_handle (filter_handle)) {
    GUID *propsets = NULL;
    gulong propsets_len;

    if (ks_object_get_supported_property_sets (filter_handle, &propsets,
            &propsets_len)) {
      gulong i;

      for (i = 0; i < propsets_len; i++) {
        if (memcmp (&propsets[i], &PROPSETID_VIDCAP_CAMERACONTROL,
                sizeof (GUID)) == 0) {
          videodevice->priority++;
          break;
        }
      }

      g_free (propsets);
    }
  }

  CloseHandle (filter_handle);
}

static gint
ks_video_device_entry_compare (gconstpointer a, gconstpointer b)
{
  const KsVideoDeviceEntry *videodevice_a = a;
  const KsVideoDeviceEntry *videodevice_b = b;

  if (videodevice_a->priority > videodevice_b->priority)
    return -1;
  else if (videodevice_a->priority == videodevice_b->priority)
    return 0;
  else
    return 1;
}

GList *
ks_video_device_list_sort_cameras_first (GList * devices)
{
  GList *videodevices = NULL, *walk;
  guint i;

  for (walk = devices; walk != NULL; walk = walk->next) {
    KsDeviceEntry *device = walk->data;
    KsVideoDeviceEntry *videodevice;

    videodevice = g_new (KsVideoDeviceEntry, 1);
    videodevice->device = device;
    ks_video_device_entry_decide_priority (videodevice);

    videodevices = g_list_append (videodevices, videodevice);
  }

  videodevices = g_list_sort (videodevices, ks_video_device_entry_compare);

  g_list_free (devices);
  devices = NULL;

  for (walk = videodevices, i = 0; walk != NULL; walk = walk->next, i++) {
    KsVideoDeviceEntry *videodevice = walk->data;

    videodevice->device->index = i;
    devices = g_list_append (devices, videodevice->device);

    g_free (videodevice);
  }

  g_list_free (videodevices);

  return devices;
}

static GstStructure *
ks_video_format_to_structure (GUID subtype_guid, GUID format_guid,
    gboolean * p_is_rgb)
{
  GstStructure *structure = NULL;
  const gchar *media_type = NULL, *format = NULL;
  /* RGB formats can be bottom-up (upside down) DIB */
  gboolean is_rgb = FALSE;

  if (IsEqualGUID (&subtype_guid, &MEDIASUBTYPE_MJPG) || IsEqualGUID (&subtype_guid, &MEDIASUBTYPE_TVMJ) ||     /* FIXME: NOT tested */
      IsEqualGUID (&subtype_guid, &MEDIASUBTYPE_WAKE) ||        /* FIXME: NOT tested */
      IsEqualGUID (&subtype_guid, &MEDIASUBTYPE_CFCC) ||        /* FIXME: NOT tested */
      IsEqualGUID (&subtype_guid, &MEDIASUBTYPE_IJPG)) {        /* FIXME: NOT tested */
    media_type = "image/jpeg";
  } else if (IsEqualGUID (&subtype_guid, &MEDIASUBTYPE_RGB555)) {
    media_type = "video/x-raw";
    format = "RGB15";
    is_rgb = TRUE;
  } else if (IsEqualGUID (&subtype_guid, &MEDIASUBTYPE_RGB565)) {
    media_type = "video/x-raw";
    format = "RGB16";
    is_rgb = TRUE;
  } else if (IsEqualGUID (&subtype_guid, &MEDIASUBTYPE_RGB24)) {
    media_type = "video/x-raw";
    format = "BGR";
    is_rgb = TRUE;
  } else if (IsEqualGUID (&subtype_guid, &MEDIASUBTYPE_RGB32)) {
    media_type = "video/x-raw";
    format = "BGRx";
    is_rgb = TRUE;
  } else if (IsEqualGUID (&subtype_guid, &MEDIASUBTYPE_ARGB32)) {
    media_type = "video/x-raw";
    format = "BGRA";
    is_rgb = TRUE;
  } else if (IsEqualGUID (&subtype_guid, &MEDIASUBTYPE_ARGB1555)) {
    GST_WARNING ("Unsupported video format ARGB15555");
  } else if (IsEqualGUID (&subtype_guid, &MEDIASUBTYPE_ARGB4444)) {
    GST_WARNING ("Unsupported video format ARGB4444");
  } else if (memcmp (&subtype_guid.Data2, &MEDIASUBTYPE_FOURCC.Data2,
          sizeof (subtype_guid) - sizeof (subtype_guid.Data1)) == 0) {
    guint32 fourcc = subtype_guid.Data1;
    gchar *format =
        g_strdup_printf ("%" GST_FOURCC_FORMAT, GST_FOURCC_ARGS (fourcc));
    if (GST_STR_FOURCC (format) == GST_MAKE_FOURCC ('Y', '1', '6', ' ')) {
      structure = gst_structure_new ("video/x-raw", "format",
          G_TYPE_STRING, "GRAY16_LE", NULL);
    } else {
      structure = gst_structure_new ("video/x-raw", "format",
          G_TYPE_STRING, format, NULL);
    }
    g_free (format);
  } else if (IsEqualGUID (&subtype_guid, &MEDIASUBTYPE_dvsd)) {
    if (IsEqualGUID (&format_guid, &FORMAT_DvInfo)) {
      structure = gst_structure_new ("video/x-dv",
          "systemstream", G_TYPE_BOOLEAN, TRUE, NULL);
    } else if (IsEqualGUID (&format_guid, &FORMAT_VideoInfo)) {
      structure = gst_structure_new ("video/x-dv",
          "systemstream", G_TYPE_BOOLEAN, FALSE,
          "format", G_TYPE_STRING, "dvsd", NULL);
    }
  }

  if (media_type) {
    structure = gst_structure_new_empty (media_type);
    if (format) {
      gst_structure_set (structure, "format", G_TYPE_STRING, format, NULL);
    }
    if (p_is_rgb) {
      *p_is_rgb = is_rgb;
    }
  }

  if (!structure) {
    GST_DEBUG ("Unknown DirectShow Video GUID "
        "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        (guint) subtype_guid.Data1, subtype_guid.Data2, subtype_guid.Data3,
        subtype_guid.Data4[0], subtype_guid.Data4[1], subtype_guid.Data4[2],
        subtype_guid.Data4[3], subtype_guid.Data4[4], subtype_guid.Data4[5],
        subtype_guid.Data4[6], subtype_guid.Data4[7]);
  }

  return structure;
}

static void
guess_aspect (gint width, gint height, gint * par_width, gint * par_height)
{
  /*
   * As we don't have access to the actual pixel aspect, we will try to do a
   * best-effort guess. The guess is based on most sensors being either 4/3
   * or 16/9, and most pixel aspects being close to 1/1.
   */
  if ((width == 768) && (height == 448)) {      /* special case for w448p */
    *par_width = 28;
    *par_height = 27;
  } else {
    if (((float) width / (float) height) < 1.2778) {
      *par_width = 12;
      *par_height = 11;
    } else {
      *par_width = 1;
      *par_height = 1;
    }
  }
}

/* NOTE: would probably be better to use a continued fractions approach here */
static void
compress_fraction (gint64 in_num, gint64 in_den, gint64 * out_num,
    gint64 * out_den)
{
  gdouble on, od, orig;
  guint denominators[] = { 1, 2, 3, 5, 7 }, i;
  const gdouble max_loss = 0.1;

  on = in_num;
  od = in_den;
  orig = on / od;

  for (i = 0; i < G_N_ELEMENTS (denominators); i++) {
    gint64 cur_n, cur_d;
    gdouble cur, loss;

    cur_n = floor ((on / (od / (gdouble) denominators[i])) + 0.5);
    cur_d = denominators[i];
    cur = (gdouble) cur_n / (gdouble) cur_d;
    loss = fabs (cur - orig);

    if (loss <= max_loss) {
      *out_num = cur_n;
      *out_den = cur_d;

      return;
    }
  }

  *out_num = in_num;
  *out_den = in_den;
}

static gboolean
ks_video_append_video_stream_cfg_fields (GstStructure * structure,
    const KS_VIDEO_STREAM_CONFIG_CAPS * vscc)
{
  GValue val = { 0, };
  gint64 min_n, min_d;
  gint64 max_n, max_d;

  g_return_val_if_fail (structure, FALSE);
  g_return_val_if_fail (vscc, FALSE);

  /* width */
  if (vscc->MinOutputSize.cx == vscc->MaxOutputSize.cx) {
    gst_structure_set (structure,
        "width", G_TYPE_INT, vscc->MaxOutputSize.cx, NULL);
  } else {
    gst_structure_set (structure,
        "width", GST_TYPE_INT_RANGE,
        vscc->MinOutputSize.cx, vscc->MaxOutputSize.cx, NULL);
  }

  /* height */
  if (vscc->MinOutputSize.cy == vscc->MaxOutputSize.cy) {
    gst_structure_set (structure,
        "height", G_TYPE_INT, vscc->MaxOutputSize.cy, NULL);
  } else {
    gst_structure_set (structure,
        "height", GST_TYPE_INT_RANGE,
        vscc->MinOutputSize.cy, vscc->MaxOutputSize.cy, NULL);
  }

  /* framerate */
  compress_fraction (NANOSECONDS, vscc->MinFrameInterval, &min_n, &min_d);
  compress_fraction (NANOSECONDS, vscc->MaxFrameInterval, &max_n, &max_d);

  if (min_n == max_n && min_d == max_d) {
    g_value_init (&val, GST_TYPE_FRACTION);
    gst_value_set_fraction (&val, max_n, max_d);
  } else {
    g_value_init (&val, GST_TYPE_FRACTION_RANGE);
    gst_value_set_fraction_range_full (&val, max_n, max_d, min_n, min_d);
  }

  gst_structure_set_value (structure, "framerate", &val);
  g_value_unset (&val);

  {
    gint par_width, par_height;

    guess_aspect (vscc->MaxOutputSize.cx, vscc->MaxOutputSize.cy,
        &par_width, &par_height);

    gst_structure_set (structure,
        "pixel-aspect-ratio", GST_TYPE_FRACTION, par_width, par_height, NULL);
  }

  return TRUE;
}

KsVideoMediaType *
ks_video_media_type_dup (KsVideoMediaType * media_type)
{
  KsVideoMediaType *result = g_new (KsVideoMediaType, 1);

  memcpy (result, media_type, sizeof (KsVideoMediaType));

  result->range = g_malloc (media_type->range->FormatSize);
  memcpy ((gpointer) result->range, media_type->range,
      media_type->range->FormatSize);

  result->format = g_malloc (media_type->format_size);
  memcpy (result->format, media_type->format, media_type->format_size);

  result->translated_caps = gst_caps_ref (media_type->translated_caps);

  return result;
}

void
ks_video_media_type_free (KsVideoMediaType * media_type)
{
  if (media_type == NULL)
    return;

  g_free ((gpointer) media_type->range);

  g_free (media_type->format);

  if (media_type->translated_caps != NULL)
    gst_caps_unref (media_type->translated_caps);

  g_free (media_type);
}

static GList *
ks_video_media_type_list_remove_duplicates (GList * media_types)
{
  GList *master, *duplicates;

  do {
    GList *entry;

    master = duplicates = NULL;

    /* Find the first set of duplicates and their master */
    for (entry = media_types; entry != NULL && duplicates == NULL;
        entry = entry->next) {
      KsVideoMediaType *mt = entry->data;
      GList *other_entry;

      for (other_entry = media_types; other_entry != NULL;
          other_entry = other_entry->next) {
        KsVideoMediaType *other_mt = other_entry->data;

        if (other_mt == mt)
          continue;

        if (gst_caps_is_equal (mt->translated_caps, other_mt->translated_caps))
          duplicates = g_list_prepend (duplicates, other_mt);
      }

      if (duplicates != NULL)
        master = entry;
    }

    if (duplicates != NULL) {
      KsVideoMediaType *selected_mt = master->data;

      /*
       * Pick a FORMAT_VideoInfo2 if present, if not we just stay with the
       * first entry
       */
      for (entry = duplicates; entry != NULL; entry = entry->next) {
        KsVideoMediaType *mt = entry->data;

        if (IsEqualGUID (&mt->range->Specifier, &FORMAT_VideoInfo2)) {
          ks_video_media_type_free (selected_mt);
          selected_mt = mt;
        } else {
          ks_video_media_type_free (mt);
        }

        /* Remove the dupe from the main list */
        media_types = g_list_remove (media_types, mt);
      }

      /* Update master node with the selected MediaType */
      master->data = selected_mt;

      g_list_free (duplicates);
    }
  }
  while (master != NULL);

  return media_types;
}

GList *
ks_video_probe_filter_for_caps (HANDLE filter_handle)
{
  GList *ret = NULL;
  gulong pin_count;
  guint pin_id;

  if (!ks_filter_get_pin_property (filter_handle, 0, KSPROPSETID_Pin,
          KSPROPERTY_PIN_CTYPES, &pin_count, sizeof (pin_count), NULL))
    goto beach;

  GST_DEBUG ("pin_count = %lu", pin_count);

  for (pin_id = 0; pin_id < pin_count; pin_id++) {
    KSPIN_COMMUNICATION pin_comm;
    KSPIN_DATAFLOW pin_flow;
    GUID pin_cat;

    if (!ks_filter_get_pin_property (filter_handle, pin_id, KSPROPSETID_Pin,
            KSPROPERTY_PIN_COMMUNICATION, &pin_comm, sizeof (pin_comm), NULL))
      continue;

    if (!ks_filter_get_pin_property (filter_handle, pin_id, KSPROPSETID_Pin,
            KSPROPERTY_PIN_DATAFLOW, &pin_flow, sizeof (pin_flow), NULL))
      continue;

    if (!ks_filter_get_pin_property (filter_handle, pin_id, KSPROPSETID_Pin,
            KSPROPERTY_PIN_CATEGORY, &pin_cat, sizeof (pin_cat), NULL))
      continue;

    GST_DEBUG ("pin[%u]: pin_comm=%d, pin_flow=%d", pin_id, pin_comm, pin_flow);

    if (pin_flow == KSPIN_DATAFLOW_OUT &&
        memcmp (&pin_cat, &PINNAME_CAPTURE, sizeof (GUID)) == 0) {
      KSMULTIPLE_ITEM *items;

      if (ks_filter_get_pin_property_multi (filter_handle, pin_id,
              KSPROPSETID_Pin, KSPROPERTY_PIN_DATARANGES, &items, NULL)) {
        KSDATARANGE *range = (KSDATARANGE *) (items + 1);
        guint i;

        for (i = 0; i < items->Count; i++) {
          if (IsEqualGUID (&range->MajorFormat, &MEDIATYPE_Video)
              || IsEqualGUID (&range->MajorFormat, &MEDIATYPE_Interleaved)) {
            KsVideoMediaType *entry;
            gpointer src_vscc = NULL, src_format = NULL;
            GstStructure *media_structure;

            entry = g_new0 (KsVideoMediaType, 1);
            entry->pin_id = pin_id;

            entry->range = g_malloc (range->FormatSize);
            memcpy ((gpointer) entry->range, range, range->FormatSize);

            if (IsEqualGUID (&range->Specifier, &FORMAT_VideoInfo)) {
              KS_DATARANGE_VIDEO *vr = (KS_DATARANGE_VIDEO *) entry->range;

              src_vscc = &vr->ConfigCaps;
              src_format = &vr->VideoInfoHeader;

              entry->format_size = sizeof (vr->VideoInfoHeader);
              entry->sample_size = vr->VideoInfoHeader.bmiHeader.biSizeImage;
            } else if (IsEqualGUID (&range->Specifier, &FORMAT_VideoInfo2)) {
              KS_DATARANGE_VIDEO2 *vr = (KS_DATARANGE_VIDEO2 *) entry->range;

              src_vscc = &vr->ConfigCaps;
              src_format = &vr->VideoInfoHeader;

              entry->format_size = sizeof (vr->VideoInfoHeader);
              entry->sample_size = vr->VideoInfoHeader.bmiHeader.biSizeImage;
            } else if (IsEqualGUID (&range->Specifier, &FORMAT_MPEGVideo)) {
              /* Untested and probably wrong... */
              KS_DATARANGE_MPEG1_VIDEO *vr =
                  (KS_DATARANGE_MPEG1_VIDEO *) entry->range;

              src_vscc = &vr->ConfigCaps;
              src_format = &vr->VideoInfoHeader;

              entry->format_size = sizeof (vr->VideoInfoHeader);
              entry->sample_size =
                  vr->VideoInfoHeader.hdr.bmiHeader.biSizeImage;
            } else if (IsEqualGUID (&range->Specifier, &FORMAT_MPEG2Video)) {
              KS_DATARANGE_MPEG2_VIDEO *vr =
                  (KS_DATARANGE_MPEG2_VIDEO *) entry->range;
              src_vscc = &vr->ConfigCaps;
              src_format = &vr->VideoInfoHeader;

              entry->format_size = sizeof (vr->VideoInfoHeader);
              entry->sample_size =
                  vr->VideoInfoHeader.hdr.bmiHeader.biSizeImage;
            } else if (IsEqualGUID (&range->Specifier, &FORMAT_DvInfo)) {
              KS_DATARANGE_DVVIDEO *vr = (KS_DATARANGE_DVVIDEO *) entry->range;

              src_vscc = NULL;
              src_format = &vr->DVVideoInfo;

              entry->format_size = sizeof (vr->DVVideoInfo);
              entry->sample_size = vr->DataRange.SampleSize;
            } else {
              gchar *guid_str;

              guid_str = ks_guid_to_string (&range->Specifier);
              GST_DEBUG ("pin[%u]: ignoring unknown specifier GUID %s",
                  pin_id, guid_str);
              g_free (guid_str);

              ks_video_media_type_free (entry);
              entry = NULL;
            }

            /* XXX: We've been doing g_assert (entry->sample_size != 0)
             * since more than 10 years ago, but it's quite questionable.
             * According to MS documentation,
             * SampleSize field in KSDATARANGE struct is ignored,
             * and biSizeImage in KS_BITMAPINFOHEADER is required only if
             * compression is used. (driver maybe set zero for raw image).
             * But since we've expected the value shouldn't be zero,
             * let's ignore a media type with entry->sample_size == 0 for now.
             * Actually people should not use this plugin in newly written code
             * anymore */
            if (entry && entry->sample_size == 0)
              g_clear_pointer (&entry, ks_video_media_type_free);

            if (entry != NULL) {
              if (src_vscc != NULL) {
                memcpy ((gpointer) & entry->vscc, src_vscc,
                    sizeof (entry->vscc));
              }

              entry->format = g_malloc (entry->format_size);
              memcpy (entry->format, src_format, entry->format_size);

              media_structure =
                  ks_video_format_to_structure (range->SubFormat,
                  range->Specifier, &entry->is_rgb);

              if (media_structure == NULL) {
                g_warning ("ks_video_format_to_structure returned NULL");
                ks_video_media_type_free (entry);
                entry = NULL;
              } else if (src_vscc == NULL) {
                entry->translated_caps = gst_caps_new_empty ();
                gst_caps_append_structure (entry->translated_caps,
                    media_structure);
              } else if (ks_video_append_video_stream_cfg_fields
                  (media_structure, &entry->vscc)) {
                entry->translated_caps = gst_caps_new_empty ();
                gst_caps_append_structure (entry->translated_caps,
                    media_structure);
              } else {
                gst_structure_free (media_structure);
                ks_video_media_type_free (entry);
                entry = NULL;
              }

              if (entry != NULL)
                ret = g_list_prepend (ret, entry);
            }
          }

          /* REVISIT: Each KSDATARANGE should start on a 64-bit boundary */
          range = (KSDATARANGE *) (((guchar *) range) + range->FormatSize);
        }

        g_free (items);
      }
    }
  }

  if (ret != NULL) {
    ret = g_list_reverse (ret);
    ret = ks_video_media_type_list_remove_duplicates (ret);
  }

beach:
  return ret;
}

KSPIN_CONNECT *
ks_video_create_pin_conn_from_media_type (KsVideoMediaType * media_type)
{
  KSPIN_CONNECT *conn = NULL;
  KSDATAFORMAT *format = NULL;
  guint8 *vih;

  conn = g_malloc0 (sizeof (KSPIN_CONNECT) + sizeof (KSDATAFORMAT) +
      media_type->format_size);

  conn->Interface.Set = KSINTERFACESETID_Standard;
  conn->Interface.Id = KSINTERFACE_STANDARD_STREAMING;
  conn->Interface.Flags = 0;

  conn->Medium.Set = KSMEDIUMSETID_Standard;
  conn->Medium.Id = KSMEDIUM_TYPE_ANYINSTANCE;
  conn->Medium.Flags = 0;

  conn->PinId = media_type->pin_id;
  conn->PinToHandle = NULL;
  conn->Priority.PriorityClass = KSPRIORITY_NORMAL;
  conn->Priority.PrioritySubClass = KSPRIORITY_NORMAL;

  format = (KSDATAFORMAT *) (conn + 1);
  memcpy (format, media_type->range, sizeof (KSDATAFORMAT));
  format->FormatSize = sizeof (KSDATAFORMAT) + media_type->format_size;

  vih = (guint8 *) (format + 1);
  memcpy (vih, media_type->format, media_type->format_size);

  return conn;
}

gboolean
ks_video_fixate_media_type (const KSDATARANGE * range,
    guint8 * format, gint width, gint height, gint fps_n, gint fps_d)
{
  KS_DATARANGE_VIDEO *vr;
  KS_VIDEOINFOHEADER *vih;
  KS_BITMAPINFOHEADER *bih;
  DWORD dwRate;

  g_return_val_if_fail (format != NULL, FALSE);

  if (IsEqualGUID (&range->Specifier, &FORMAT_VideoInfo)) {
    bih = &((KS_VIDEOINFOHEADER *) format)->bmiHeader;
  } else if (IsEqualGUID (&range->Specifier, &FORMAT_VideoInfo2)) {
    bih = &((KS_VIDEOINFOHEADER2 *) format)->bmiHeader;
  } else if (IsEqualGUID (&range->Specifier, &FORMAT_MPEGVideo)) {
    bih = &((KS_MPEG1VIDEOINFO *) format)->hdr.bmiHeader;
  } else if (IsEqualGUID (&range->Specifier, &FORMAT_MPEG2Video)) {
    bih = &((KS_MPEGVIDEOINFO2 *) format)->hdr.bmiHeader;
  } else {
    return FALSE;
  }

  /* These formats' structures share the most basic stuff */
  vr = (KS_DATARANGE_VIDEO *) range;
  vih = (KS_VIDEOINFOHEADER *) format;

  /* FIXME: Need to figure out how to properly handle ranges */
  if (bih->biWidth != width || bih->biHeight != height)
    return FALSE;

  /* Framerate, clamped because of fraction conversion rounding errors */
  vih->AvgTimePerFrame =
      gst_util_uint64_scale_int_round (NANOSECONDS, fps_d, fps_n);
  vih->AvgTimePerFrame =
      MAX (vih->AvgTimePerFrame, vr->ConfigCaps.MinFrameInterval);
  vih->AvgTimePerFrame =
      MIN (vih->AvgTimePerFrame, vr->ConfigCaps.MaxFrameInterval);

  /* Bitrate, clamped for the same reason as framerate */
  dwRate = (width * height * fps_n) / fps_d;
  vih->dwBitRate = dwRate * bih->biBitCount;
  vih->dwBitRate = MAX (vih->dwBitRate, vr->ConfigCaps.MinBitsPerSecond);
  vih->dwBitRate = MIN (vih->dwBitRate, vr->ConfigCaps.MaxBitsPerSecond);

  return TRUE;
}

static GstStructure *
ks_video_append_var_video_fields (GstStructure * structure)
{
  if (structure) {
    gst_structure_set (structure,
        "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
        "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
        "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
  }

  return structure;
}

GstCaps *
ks_video_get_all_caps (void)
{
  static GstCaps *caps = NULL;

  if (caps == NULL) {
    GstStructure *structure;
    caps = gst_caps_new_empty ();

    /* from Windows SDK 6.0 uuids.h */
    /* RGB formats */
    structure =
        ks_video_append_var_video_fields (ks_video_format_to_structure
        (MEDIASUBTYPE_RGB555, FORMAT_VideoInfo, NULL));
    gst_caps_append_structure (caps, structure);

    structure =
        ks_video_append_var_video_fields (ks_video_format_to_structure
        (MEDIASUBTYPE_RGB565, FORMAT_VideoInfo, NULL));
    gst_caps_append_structure (caps, structure);

    structure =
        ks_video_append_var_video_fields (ks_video_format_to_structure
        (MEDIASUBTYPE_RGB24, FORMAT_VideoInfo, NULL));
    gst_caps_append_structure (caps, structure);

    structure =
        ks_video_append_var_video_fields (ks_video_format_to_structure
        (MEDIASUBTYPE_RGB32, FORMAT_VideoInfo, NULL));
    gst_caps_append_structure (caps, structure);

    /* YUV formats */
    structure =
        ks_video_append_var_video_fields (gst_structure_new_empty
        ("video/x-raw"));
    gst_caps_append_structure (caps, structure);

    /* Other formats */
    structure =
        ks_video_append_var_video_fields (ks_video_format_to_structure
        (MEDIASUBTYPE_MJPG, FORMAT_VideoInfo, NULL));
    gst_caps_append_structure (caps, structure);

    structure =
        ks_video_append_var_video_fields (ks_video_format_to_structure
        (MEDIASUBTYPE_dvsd, FORMAT_VideoInfo, NULL));
    gst_caps_append_structure (caps, structure);

    structure =                 /* no variable video fields (width, height, framerate) */
        ks_video_format_to_structure (MEDIASUBTYPE_dvsd, FORMAT_DvInfo, NULL);
    gst_caps_append_structure (caps, structure);
  }

  return caps;
}
