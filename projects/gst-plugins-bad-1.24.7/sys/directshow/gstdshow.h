/* GStreamer
 * Copyright (C) 2007 Sebastien Moutte <sebastien@moutte.net>
 *
 * gstdshow.h:
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

#ifndef _GSTDSHOW_
#define _GSTDSHOW_

#include <streams.h>
#include <windows.h>
#include <tchar.h>
#include <objbase.h>
#include <dshow.h>
#include <Rpc.h>

#include <gst/gst.h>
#include <gst/video/video.h>

typedef struct _DshowDeviceEntry DshowDeviceEntry;

struct _DshowDeviceEntry
{
  gchar *device;
  gchar *device_name;
  gint device_index;
  GstCaps *caps;
  IMoniker *moniker;
};

typedef struct _GstCapturePinMediaType
{
  AM_MEDIA_TYPE *mediatype;
  IPin *capture_pin;

  VIDEO_STREAM_CONFIG_CAPS vscc;

  //default caps
  gint defaultWidth;
  gint defaultHeight;
  gint defaultFPS;

  gint granularityWidth;        //will be removed when GST_TYPE_INT_RANGE_STEP exits
  gint granularityHeight;       //will be removed when GST_TYPE_INT_RANGE_STEP exits
} GstCapturePinMediaType;

/* free memory of the input pin mediatype */
void gst_dshow_free_pin_mediatype (gpointer pt);

/* free memory of the input dshow mediatype */
void gst_dshow_free_mediatype (AM_MEDIA_TYPE * pmt);

/* create a new capture media type that handles dshow video caps of a capture pin */
GstCapturePinMediaType *gst_dshow_new_pin_mediatype (IPin * pin);

/* create a new capture media type from enum mediatype */
GstCapturePinMediaType * gst_dshow_new_pin_mediatype_from_enum_mediatypes (IPin * pin,
    IEnumMediaTypes *enum_mediatypes);

/* create a new capture media type from streamcaps */
GstCapturePinMediaType *gst_dshow_new_pin_mediatype_from_streamcaps (IPin * pin,
    gint id, IAMStreamConfig * streamcaps);

/* free the memory of all mediatypes of the input list if pin mediatype */
void gst_dshow_free_pins_mediatypes (GList * mediatypes);

/* allow to know what kind of media type we have */
gboolean gst_dshow_check_mediatype (AM_MEDIA_TYPE * media_type,
    const GUID sub_type, const GUID format_type);

/* get a pin from directshow filter */
gboolean gst_dshow_get_pin_from_filter (IBaseFilter * filter,
    PIN_DIRECTION pindir, IPin ** pin);

/* find and return a filter according to the input and output types */
gboolean gst_dshow_find_filter (CLSID input_majortype, CLSID input_subtype,
    CLSID output_majortype, CLSID output_subtype,
    gchar * prefered_filter_name, IBaseFilter ** filter);

/* get the dshow device path from device friendly name.
If friendly name is not set, it will return the first available device */
gchar *gst_dshow_getdevice_from_devicename (const GUID * device_category,
    gchar ** device_name, gint * device_index);

/* show the capture filter property page (generally used to setup the device). the page is modal*/
gboolean gst_dshow_show_propertypage (IBaseFilter * base_filter);

/* translate GUID format to gsteamer video format */
GstVideoFormat gst_dshow_guid_to_gst_video_format (AM_MEDIA_TYPE *mediatype);

/* check if IPin is connected */
gboolean gst_dshow_is_pin_connected (IPin *pin);

/* transform a dshow video caps to a gstreamer video caps */
GstCaps *gst_dshow_new_video_caps (GstVideoFormat video_format,
    const gchar * name, GstCapturePinMediaType * pin_mediatype);

/* configure the latency of the capture source */
bool gst_dshow_configure_latency (IPin *pCapturePin, guint bufSizeMS);

/* enumerate devices of a given category (i.e., audio or video) */
void gst_dshow_device_entry_free (DshowDeviceEntry *entry);
void gst_dshow_device_list_free (GList * devices);
GList * gst_dshow_enumerate_devices (const GUID * device_category, gboolean getcaps);

DshowDeviceEntry * gst_dshow_select_device (const GUID * device_category,
    const gchar *device, const gchar *device_name, const gint device_index);

/* create capture filter from moniker */
IBaseFilter *gst_dshow_create_capture_filter (IMoniker *moniker);

#endif /* _GSTDSHOW_ */
