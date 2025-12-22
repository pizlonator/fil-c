/* GStreamer
 * Copyright (C) 2007 Sebastien Moutte <sebastien@moutte.net>
 *
 * gstdshow.cpp:
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

#include <gst/video/video-format.h>

#include "gstdshow.h"
#include "gstdshowfakesink.h"
#include "gstdshowvideosrc.h"

GST_DEBUG_CATEGORY_EXTERN (dshowsrcwrapper_debug);
#define GST_CAT_DEFAULT dshowsrcwrapper_debug

gchar *
wchar_to_gchar (WCHAR * w)
{
  return g_utf16_to_utf8 ((const gunichar2 *) w, wcslen (w), NULL, NULL, NULL);
}

const GUID DSHOW_MEDIASUBTYPE_I420
    = { 0x30323449, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B,
    0x71}
};

void
gst_dshow_free_mediatype (AM_MEDIA_TYPE * pmt)
{
  if (pmt != NULL) {
    if (pmt->cbFormat != 0) {
      CoTaskMemFree ((PVOID) pmt->pbFormat);
      pmt->cbFormat = 0;
      pmt->pbFormat = NULL;
    }
    if (pmt->pUnk != NULL) {
      /* Unnecessary because pUnk should not be used, but safest. */
      pmt->pUnk->Release ();
      pmt->pUnk = NULL;
    }

    CoTaskMemFree (pmt);
  }
}

void
gst_dshow_free_pin_mediatype (gpointer pt)
{
  GstCapturePinMediaType *pin_mediatype = (GstCapturePinMediaType *) pt;
  if (pin_mediatype) {
    if (pin_mediatype->capture_pin) {
      pin_mediatype->capture_pin->Release ();
      pin_mediatype->capture_pin = NULL;
    }
    if (pin_mediatype->mediatype) {
      gst_dshow_free_mediatype (pin_mediatype->mediatype);
      pin_mediatype->mediatype = NULL;
    }
  }
}

GstCapturePinMediaType *
gst_dshow_new_pin_mediatype (IPin * pin)
{
  GstCapturePinMediaType *pin_mediatype = g_new0 (GstCapturePinMediaType, 1);

  pin->AddRef ();
  pin_mediatype->capture_pin = pin;

  return pin_mediatype;
}

GstCapturePinMediaType *
gst_dshow_new_pin_mediatype_from_enum_mediatypes (IPin * pin, IEnumMediaTypes *enum_mediatypes)
{
  GstCapturePinMediaType *pin_mediatype = gst_dshow_new_pin_mediatype (pin);
  VIDEOINFOHEADER *video_info = NULL;

  HRESULT hres = enum_mediatypes->Next (1, &pin_mediatype->mediatype, NULL);
  if (hres != S_OK || !pin_mediatype->mediatype) {
    gst_dshow_free_pin_mediatype (pin_mediatype);
    return NULL;
  }

  /* FORMAT_None or GUID_NULL format type does hold format data */
  if (pin_mediatype->mediatype->formattype == FORMAT_None ||
      pin_mediatype->mediatype->formattype == GUID_NULL) {
    return NULL;
  }

  video_info = (VIDEOINFOHEADER *) pin_mediatype->mediatype->pbFormat;
  if (!video_info)
    return NULL;

  pin_mediatype->defaultWidth = video_info->bmiHeader.biWidth;
  pin_mediatype->defaultHeight = video_info->bmiHeader.biHeight;
  if (video_info->AvgTimePerFrame > 0) {
    pin_mediatype->defaultFPS = (gint) (10000000 / video_info->AvgTimePerFrame);
  } else {
    pin_mediatype->defaultFPS = 30;
  }
  pin_mediatype->granularityWidth = 1;
  pin_mediatype->granularityHeight = 1;

  return pin_mediatype;
}

GstCapturePinMediaType *
gst_dshow_new_pin_mediatype_from_streamcaps (IPin * pin, gint id, IAMStreamConfig * streamcaps)
{
  GstCapturePinMediaType *pin_mediatype = gst_dshow_new_pin_mediatype (pin);
  VIDEOINFOHEADER *video_info = NULL;

  HRESULT hres = streamcaps->GetStreamCaps (id, &pin_mediatype->mediatype,
      (BYTE *) & pin_mediatype->vscc);
  if (FAILED (hres) || !pin_mediatype->mediatype) {
    gst_dshow_free_pin_mediatype (pin_mediatype);
    return NULL;
  }

  video_info = (VIDEOINFOHEADER *) pin_mediatype->mediatype->pbFormat;

  pin_mediatype->defaultWidth = video_info->bmiHeader.biWidth;
  pin_mediatype->defaultHeight = video_info->bmiHeader.biHeight;
  if (video_info->AvgTimePerFrame > 0) {
    pin_mediatype->defaultFPS = (gint) (10000000 / video_info->AvgTimePerFrame);
  } else {
    pin_mediatype->defaultFPS = 30;
  }
  pin_mediatype->granularityWidth = pin_mediatype->vscc.OutputGranularityX;
  pin_mediatype->granularityHeight = pin_mediatype->vscc.OutputGranularityY;

  return pin_mediatype;
}

void
gst_dshow_free_pins_mediatypes (GList * pins_mediatypes)
{
  g_list_free_full (pins_mediatypes,
      (GDestroyNotify) gst_dshow_free_pin_mediatype);
}

gboolean
gst_dshow_check_mediatype (AM_MEDIA_TYPE * media_type, const GUID sub_type,
    const GUID format_type)
{
  RPC_STATUS rpcstatus;

  g_return_val_if_fail (media_type != NULL, FALSE);

  return
      UuidCompare (&media_type->subtype, (UUID *) & sub_type,
      &rpcstatus) == 0 && rpcstatus == RPC_S_OK &&
      //IsEqualGUID (&media_type->subtype, &sub_type)
      UuidCompare (&media_type->formattype, (UUID *) & format_type,
      &rpcstatus) == 0 && rpcstatus == RPC_S_OK;
}

gboolean
gst_dshow_get_pin_from_filter (IBaseFilter * filter, PIN_DIRECTION pindir,
    IPin ** pin)
{
  gboolean ret = FALSE;
  IEnumPins *enumpins = NULL;
  IPin *pintmp = NULL;
  HRESULT hres;
  *pin = NULL;

  hres = filter->EnumPins (&enumpins);
  if (FAILED (hres)) {
    return ret;
  }

  while (enumpins->Next (1, &pintmp, NULL) == S_OK) {
    PIN_DIRECTION pindirtmp;
    hres = pintmp->QueryDirection (&pindirtmp);
    if (hres == S_OK && pindir == pindirtmp) {
      *pin = pintmp;
      ret = TRUE;
      break;
    }
    pintmp->Release ();
  }
  enumpins->Release ();

  return ret;
}

gboolean
gst_dshow_find_filter (CLSID input_majortype, CLSID input_subtype,
    CLSID output_majortype, CLSID output_subtype,
    gchar * prefered_filter_name, IBaseFilter ** filter)
{
  gboolean ret = FALSE;
  HRESULT hres;
  GUID arrayInTypes[2];
  GUID arrayOutTypes[2];
  IFilterMapper2 *mapper = NULL;
  IEnumMoniker *enum_moniker = NULL;
  IMoniker *moniker = NULL;
  ULONG fetched;
  gchar *prefered_filter_upper = NULL;
  gboolean exit = FALSE;

  /* initialize output parameter */
  if (filter)
    *filter = NULL;

  /* create a private copy of preferred filter substring in upper case */
  if (prefered_filter_name) {
    prefered_filter_upper = g_strdup (prefered_filter_name);
    _strupr (prefered_filter_upper);
  }

  hres = CoCreateInstance (CLSID_FilterMapper2, NULL, CLSCTX_INPROC,
      IID_IFilterMapper2, (void **) &mapper);
  if (FAILED (hres))
    goto clean;

  memcpy (&arrayInTypes[0], &input_majortype, sizeof (CLSID));
  memcpy (&arrayInTypes[1], &input_subtype, sizeof (CLSID));
  memcpy (&arrayOutTypes[0], &output_majortype, sizeof (CLSID));
  memcpy (&arrayOutTypes[1], &output_subtype, sizeof (CLSID));

  hres =
      mapper->EnumMatchingFilters (&enum_moniker, 0, FALSE,
      MERIT_DO_NOT_USE + 1, TRUE, 1, arrayInTypes, NULL, NULL, FALSE, TRUE, 1,
      arrayOutTypes, NULL, NULL);
  if (FAILED (hres))
    goto clean;

  enum_moniker->Reset ();

  while (hres = enum_moniker->Next (1, &moniker, &fetched), hres == S_OK
      && !exit) {
    IBaseFilter *filter_temp = NULL;
    IPropertyBag *property_bag = NULL;
    gchar *friendly_name = NULL;

    hres =
        moniker->BindToStorage (NULL, NULL, IID_IPropertyBag,
        (void **) &property_bag);
    if (SUCCEEDED (hres) && property_bag) {
      VARIANT varFriendlyName;
      VariantInit (&varFriendlyName);

      hres = property_bag->Read (L"FriendlyName", &varFriendlyName, NULL);
      if (hres == S_OK && varFriendlyName.bstrVal) {
        friendly_name = wchar_to_gchar (varFriendlyName.bstrVal);
        if (friendly_name)
          _strupr (friendly_name);
        SysFreeString (varFriendlyName.bstrVal);
      }
      property_bag->Release ();
    }

    hres =
        moniker->BindToObject (NULL, NULL, IID_IBaseFilter,
        (void **) &filter_temp);
    if (SUCCEEDED (hres) && filter_temp) {
      ret = TRUE;
      if (filter) {
        if (*filter)
          (*filter)->Release ();

        *filter = filter_temp;
        (*filter)->AddRef ();

        if (prefered_filter_upper && friendly_name &&
            strstr (friendly_name, prefered_filter_upper))
          exit = TRUE;
      }

      /* if we just want to know if the formats are supported OR
         if we don't care about what will be the filter used
         => we can stop enumeration */
      if (!filter || !prefered_filter_upper)
        exit = TRUE;

      filter_temp->Release ();
    }

    g_free (friendly_name);
    moniker->Release ();
  }

clean:
  g_free (prefered_filter_upper);
  if (enum_moniker)
    enum_moniker->Release ();
  if (mapper)
    mapper->Release ();

  return ret;
}

void
gst_dshow_device_entry_free (DshowDeviceEntry * entry)
{
  if (entry) {
    g_free (entry->device);
    entry->device = NULL;
    g_free (entry->device_name);
    entry->device_name = NULL;
    if (entry->caps) {
      gst_caps_unref (entry->caps);
      entry->caps = NULL;
    }
    if (entry->moniker) {
      entry->moniker->Release ();
      entry->moniker = NULL;
    }
  }
}

void
gst_dshow_device_list_free (GList * devices)
{
  GList *cur;

  for (cur = devices; cur != NULL; cur = cur->next)
    gst_dshow_device_entry_free ((DshowDeviceEntry *) cur->data);

  g_list_free (devices);
}

GList *
gst_dshow_enumerate_devices (const GUID * device_category, gboolean getcaps)
{
  GList *result = NULL;
  ICreateDevEnum *devices_enum = NULL;
  IEnumMoniker *enum_moniker = NULL;
  IMoniker *moniker = NULL;
  HRESULT hres = S_FALSE;
  ULONG fetched;
  gint devidx = -1;

  hres = CoCreateInstance (CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
      IID_ICreateDevEnum, (void **) &devices_enum);
  if (hres != S_OK) {
    GST_ERROR ("Failed to create System Device Enumerator");
    goto clean;
  }

  hres = devices_enum->CreateClassEnumerator (*device_category,
      &enum_moniker, 0);
  if (hres != S_OK || !enum_moniker) {
    GST_ERROR ("Failed to create audio/video class device enumerator");
    goto clean;
  }

  enum_moniker->Reset ();

  while (enum_moniker->Next (1, &moniker, &fetched) == S_OK) {
    IPropertyBag *property_bag = NULL;
    hres = moniker->BindToStorage (NULL, NULL, IID_IPropertyBag,
        (void **) &property_bag);
    if (SUCCEEDED (hres) && property_bag) {
      VARIANT varFriendlyName;
      VariantInit (&varFriendlyName);

      hres = property_bag->Read (L"FriendlyName", &varFriendlyName, NULL);
      if (hres == S_OK && varFriendlyName.bstrVal) {
        gchar *friendly_name = wchar_to_gchar (varFriendlyName.bstrVal);

        devidx++;
        GST_DEBUG ("Found device idx=%d: device-name='%s'",
            devidx, friendly_name);

        WCHAR *wszDisplayName = NULL;
        hres = moniker->GetDisplayName (NULL, NULL, &wszDisplayName);
        if (hres == S_OK && wszDisplayName) {
          DshowDeviceEntry *entry = g_new0 (DshowDeviceEntry, 1);
          gchar *device_path = NULL;
          GstCaps *caps = NULL;

          device_path = wchar_to_gchar (wszDisplayName);
          CoTaskMemFree (wszDisplayName);

          /* getting caps can be slow, so make it optional when enumerating */
          if (getcaps) {
            IBindCtx *lpbc = NULL;
            hres = CreateBindCtx (0, &lpbc);
            if (SUCCEEDED (hres)) {
              IBaseFilter *video_cap_filter = NULL;
              hres = moniker->BindToObject (lpbc, NULL, IID_IBaseFilter,
                (LPVOID *) & video_cap_filter);
              if (video_cap_filter) {
                caps = gst_dshowvideosrc_getcaps_from_capture_filter (video_cap_filter, NULL);
                video_cap_filter->Release ();
              }
              lpbc->Release ();
            }
          }

          entry->device = device_path;
          entry->device_name = friendly_name;
          entry->device_index = devidx;
          entry->caps = caps;
          entry->moniker = moniker;
          moniker = NULL;
          result = g_list_append (result, entry);
        } else {
          g_free (friendly_name);
        }
        SysFreeString (varFriendlyName.bstrVal);
      }
      property_bag->Release ();
    }
    if (moniker) {
      moniker->Release ();
    }
  }

clean:
  if (enum_moniker) {
    enum_moniker->Release ();
  }

  if (devices_enum) {
    devices_enum->Release ();
  }

  return result;
}

DshowDeviceEntry *
gst_dshow_select_device (const GUID * device_category,
    const gchar * device, const gchar * device_name, const gint device_index)
{
  GList *devices = NULL;
  GList *item = NULL;
  DshowDeviceEntry *selected = NULL;

  GST_DEBUG ("Trying to select device-index=%d, device-name='%s', device='%s'",
    device_index, device_name, device);

  devices = gst_dshow_enumerate_devices (&CLSID_VideoInputDeviceCategory, FALSE);

  for (item = devices; item != NULL; item = item->next) {
    DshowDeviceEntry *entry = (DshowDeviceEntry *) item->data;

    /* device will be used first, then device-name, then device-index */
    if (device && g_strcmp0 (device, entry->device) == 0) {
      selected = entry;
      break;
    } else if (!device && device_name && g_strcmp0 (device_name, entry->device_name) == 0) {
      selected = entry;
      break;
    } else if (!device && !device_name && device_index == entry->device_index) {
      selected = entry;
      break;
    }
  }

  if (selected) {
    devices = g_list_remove (devices, selected);
    GST_DEBUG ("Selected device-index=%d, device-name='%s', device='%s'",
      selected->device_index, selected->device_name, selected->device);
  } else {
    GST_DEBUG ("No matching device found");
  }

  gst_dshow_device_list_free (devices);

  return selected;
}

IBaseFilter *
gst_dshow_create_capture_filter (IMoniker *moniker)
{
  HRESULT hres = S_OK;
  IBindCtx *lpbc = NULL;
  IBaseFilter *video_cap_filter = NULL;

  g_assert (moniker != NULL);

  hres = CreateBindCtx (0, &lpbc);
  if (SUCCEEDED (hres)) {
    hres = moniker->BindToObject (lpbc, NULL, IID_IBaseFilter,
        (LPVOID *) & video_cap_filter);
    lpbc->Release ();
  }

  return video_cap_filter;
}

gchar *
gst_dshow_getdevice_from_devicename (const GUID * device_category,
    gchar ** device_name, gint * device_index)
{
  gchar *ret = NULL;
  ICreateDevEnum *devices_enum = NULL;
  IEnumMoniker *enum_moniker = NULL;
  IMoniker *moniker = NULL;
  HRESULT hres = S_FALSE;
  ULONG fetched;
  gboolean bfound = FALSE;
  gint devidx = -1;

  hres = CoCreateInstance (CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
      IID_ICreateDevEnum, (void **) &devices_enum);
  if (hres != S_OK) {
    GST_ERROR ("Failed to create System Device Enumerator");
    goto clean;
  }

  hres = devices_enum->CreateClassEnumerator (*device_category,
      &enum_moniker, 0);
  if (hres != S_OK || !enum_moniker) {
    GST_ERROR ("Failed to create audio/video class device enumerator");
    goto clean;
  }

  enum_moniker->Reset ();

  while (hres = enum_moniker->Next (1, &moniker, &fetched), hres == S_OK
      && !bfound) {
    IPropertyBag *property_bag = NULL;
    hres =
        moniker->BindToStorage (NULL, NULL, IID_IPropertyBag,
        (void **) &property_bag);
    if (SUCCEEDED (hres) && property_bag) {
      VARIANT varFriendlyName;
      VariantInit (&varFriendlyName);

      hres = property_bag->Read (L"FriendlyName", &varFriendlyName, NULL);
      if (hres == S_OK && varFriendlyName.bstrVal) {
        gchar *friendly_name = wchar_to_gchar (varFriendlyName.bstrVal);

        devidx++;
        GST_DEBUG ("Found device idx=%d: device-name='%s'",
            devidx, friendly_name);

        if ((!*device_name || !**device_name) && devidx == *device_index) {
          g_free (*device_name);
          *device_name = g_strdup (friendly_name);
        }

        if ((*device_name && **device_name)
            && _stricmp (*device_name, friendly_name) == 0) {
          WCHAR *wszDisplayName = NULL;
          hres = moniker->GetDisplayName (NULL, NULL, &wszDisplayName);
          if (hres == S_OK && wszDisplayName) {
            *device_index = devidx;
            ret = wchar_to_gchar (wszDisplayName);
            CoTaskMemFree (wszDisplayName);
          }
          bfound = TRUE;
        }
        SysFreeString (varFriendlyName.bstrVal);
      }
      property_bag->Release ();
    }
    moniker->Release ();
  }

clean:
  if (enum_moniker) {
    enum_moniker->Release ();
  }

  if (devices_enum) {
    devices_enum->Release ();
  }

  return ret;
}

gboolean
gst_dshow_show_propertypage (IBaseFilter * base_filter)
{
  gboolean ret = FALSE;
  ISpecifyPropertyPages *pProp = NULL;
  HRESULT hres =
      base_filter->QueryInterface (IID_ISpecifyPropertyPages, (void **) &pProp);
  if (SUCCEEDED (hres)) {
    /* Get the filter's name and IUnknown pointer. */
    FILTER_INFO FilterInfo;
    CAUUID caGUID;
    IUnknown *pFilterUnk = NULL;
    hres = base_filter->QueryFilterInfo (&FilterInfo);
    base_filter->QueryInterface (IID_IUnknown, (void **) &pFilterUnk);

    /* Show the page. */
    pProp->GetPages (&caGUID);
    pProp->Release ();
    OleCreatePropertyFrame (GetDesktopWindow (), 0, 0, FilterInfo.achName,
        1, &pFilterUnk, caGUID.cElems, caGUID.pElems, 0, 0, NULL);

    pFilterUnk->Release ();
    FilterInfo.pGraph->Release ();
    CoTaskMemFree (caGUID.pElems);
  }
  return ret;
}

GstVideoFormat
gst_dshow_guid_to_gst_video_format (AM_MEDIA_TYPE *mediatype)
{
  if (gst_dshow_check_mediatype (mediatype, DSHOW_MEDIASUBTYPE_I420, FORMAT_VideoInfo))
    return GST_VIDEO_FORMAT_I420;

  if (gst_dshow_check_mediatype (mediatype, MEDIASUBTYPE_RGB24, FORMAT_VideoInfo))
    return GST_VIDEO_FORMAT_BGR;

  if (gst_dshow_check_mediatype (mediatype, MEDIASUBTYPE_YUY2, FORMAT_VideoInfo))
    return GST_VIDEO_FORMAT_YUY2;

  if (gst_dshow_check_mediatype (mediatype, MEDIASUBTYPE_UYVY, FORMAT_VideoInfo))
    return GST_VIDEO_FORMAT_UYVY;

  if (gst_dshow_check_mediatype (mediatype, MEDIASUBTYPE_RGB32, FORMAT_VideoInfo))
    return GST_VIDEO_FORMAT_BGRx;

  if (gst_dshow_check_mediatype (mediatype, MEDIASUBTYPE_RGB565, FORMAT_VideoInfo))
    return GST_VIDEO_FORMAT_BGR16;

  if (gst_dshow_check_mediatype (mediatype, MEDIASUBTYPE_RGB555, FORMAT_VideoInfo))
    return GST_VIDEO_FORMAT_BGR15;

  if (gst_dshow_check_mediatype (mediatype, MEDIASUBTYPE_RGB8, FORMAT_VideoInfo))
    return GST_VIDEO_FORMAT_GRAY8;

  return GST_VIDEO_FORMAT_UNKNOWN;
}

gboolean
gst_dshow_is_pin_connected (IPin * pin)
{
  IPin *tmp_pin = NULL;
  gboolean res;
  HRESULT hres;

  g_assert (pin);
  hres = pin->ConnectedTo (&tmp_pin);
  res = (hres != VFW_E_NOT_CONNECTED);
  if (tmp_pin)
    tmp_pin->Release ();

  return res;
}

GstCaps *
gst_dshow_new_video_caps (GstVideoFormat video_format, const gchar * name,
    GstCapturePinMediaType * pin_mediatype)
{
  GstCaps *video_caps = NULL;
  GstStructure *video_structure = NULL;
  gint min_w, max_w;
  gint min_h, max_h;
  gint min_fr, max_fr;

  /* raw video format */
  switch (video_format) {
    case GST_VIDEO_FORMAT_BGR:
      video_caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("BGR"));
      break;
    case GST_VIDEO_FORMAT_I420:
      video_caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("I420"));
	  break;
    case GST_VIDEO_FORMAT_YUY2:
      video_caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("YUY2"));
      break;
    case GST_VIDEO_FORMAT_UYVY:
      video_caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("UYVY"));
      break;
    case GST_VIDEO_FORMAT_BGRx:
      video_caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("BGRx"));
      break;
    case GST_VIDEO_FORMAT_BGR16:
      video_caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("BGR16"));
      break;
    case GST_VIDEO_FORMAT_BGR15:
      video_caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("BGR15"));
      break;
    case GST_VIDEO_FORMAT_GRAY8:
      video_caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("GRAY8"));
      break;
    default:
      break;
  }

  /* other video format */
  if (!video_caps) {
    if (g_ascii_strncasecmp (name, "video/x-dv, systemstream=FALSE", 31) == 0) {
      video_caps = gst_caps_new_simple ("video/x-dv",
          "systemstream", G_TYPE_BOOLEAN, FALSE,
          "format", G_TYPE_STRING, "dvsd",
          NULL);
    } else if (g_ascii_strncasecmp (name, "video/x-dv, systemstream=TRUE", 31) == 0) {
      video_caps = gst_caps_new_simple ("video/x-dv",
          "systemstream", G_TYPE_BOOLEAN, TRUE, NULL);
      return video_caps;
    } else if (g_ascii_strncasecmp (name, "image/jpeg", 10) == 0) {
      video_caps = gst_caps_new_simple ("image/jpeg", NULL);
    } else if (g_ascii_strncasecmp (name, "video/x-h264", 12) == 0) {
      video_caps = gst_caps_new_simple ("video/x-h264", NULL);
    }
  }

  if (!video_caps)
    return NULL;

  video_structure = gst_caps_get_structure (video_caps, 0);

  /* Hope GST_TYPE_INT_RANGE_STEP will exits in future gstreamer releases  */
  /* because we could use :  */
  /* "width", GST_TYPE_INT_RANGE_STEP, video_default->minWidth, video_default->maxWidth,  video_default->granularityWidth */
  /* instead of : */
  /* "width", GST_TYPE_INT_RANGE, video_default->minWidth, video_default->maxWidth */

  /* For framerate we do not need a step (granularity) because  */
  /* "The IAMStreamConfig::SetFormat method will set the frame rate to the closest  */
  /* value that the filter supports" as it said in the VIDEO_STREAM_CONFIG_CAPS dshwo doc */

  min_w = pin_mediatype->vscc.MinOutputSize.cx;
  max_w = pin_mediatype->vscc.MaxOutputSize.cx;
  min_h = pin_mediatype->vscc.MinOutputSize.cy;
  max_h = pin_mediatype->vscc.MaxOutputSize.cy;
  min_fr = (gint) (10000000 / pin_mediatype->vscc.MaxFrameInterval);
  max_fr = (gint)(10000000 / pin_mediatype->vscc.MinFrameInterval);

  if (min_w == max_w)
    gst_structure_set (video_structure, "width", G_TYPE_INT, min_w, NULL);
  else
     gst_structure_set (video_structure,
       "width", GST_TYPE_INT_RANGE, min_w, max_w, NULL);

  if (min_h == max_h)
    gst_structure_set (video_structure, "height", G_TYPE_INT, min_h, NULL);
  else
     gst_structure_set (video_structure,
       "height", GST_TYPE_INT_RANGE, min_h, max_h, NULL);

  if (min_fr == max_fr)
    gst_structure_set (video_structure, "framerate",
        GST_TYPE_FRACTION, min_fr, 1, NULL);
  else
     gst_structure_set (video_structure, "framerate",
         GST_TYPE_FRACTION_RANGE, min_fr, 1, max_fr, 1, NULL);

  return video_caps;
}

bool gst_dshow_configure_latency (IPin *pCapturePin, guint bufSizeMS)
{
  HRESULT hr;
  ALLOCATOR_PROPERTIES alloc_prop;
  IAMBufferNegotiation * pNeg = NULL;
  hr = pCapturePin->QueryInterface(IID_IAMBufferNegotiation, (void **)&pNeg);

  if(!SUCCEEDED (hr))
    return FALSE;

  alloc_prop.cbAlign = -1;  // -1 means no preference.
  alloc_prop.cbBuffer = bufSizeMS;
  alloc_prop.cbPrefix = -1;
  alloc_prop.cBuffers = -1;
  hr = pNeg->SuggestAllocatorProperties (&alloc_prop);
  return SUCCEEDED (hr);
}
