/* GStreamer
 * Copyright <2006, 2007, 2008, 2009, 2010> Fluendo <support@fluendo.com>
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

#ifndef _GST_DSHOW_UTIL_H_
#define _GST_DSHOW_UTIL_H_

#include <windows.h>
#include <tchar.h>
#include <comdef.h>
#include <objbase.h>
#include <dshow.h>
#include <Rpc.h>
#include <streams.h>
#include <strmif.h>

#include <glib.h>

_COM_SMARTPTR_TYPEDEF(IBaseFilter, __uuidof(IBaseFilter));
_COM_SMARTPTR_TYPEDEF(IFilterGraph, __uuidof(IFilterGraph));
_COM_SMARTPTR_TYPEDEF(IFilterMapper2, __uuidof(IFilterMapper2));
_COM_SMARTPTR_TYPEDEF(IEnumMediaTypes, __uuidof(IEnumMediaTypes));
_COM_SMARTPTR_TYPEDEF(IEnumMoniker, __uuidof(IEnumMoniker));
_COM_SMARTPTR_TYPEDEF(IEnumPins, __uuidof(IEnumPins));
_COM_SMARTPTR_TYPEDEF(IMediaFilter, __uuidof(IMediaFilter));
_COM_SMARTPTR_TYPEDEF(IMoniker, __uuidof(IMoniker));
_COM_SMARTPTR_TYPEDEF(IPin, __uuidof(IPin));

typedef struct {
  const GUID *filter_guid;  /* The filter GUID, or DMO GUID */
  const GUID *dmo_category; /* If non-NULL, the filter is a DMO of this
                               category */
} PreferredFilter;

/* get a pin from directshow filter */
IPin *gst_dshow_util_get_pin_from_filter (IBaseFilter *filter, PIN_DIRECTION pindir);

/* find and return a filter according to the input and output types */
IBaseFilter *
gst_dshow_util_find_filter(CLSID input_majortype, CLSID input_subtype,
                      CLSID output_majortype, CLSID output_subtype,
                      PreferredFilter *preferred_filters);

#define DSHOW_CODEC_QDATA g_quark_from_string ("dshow-codec")

#endif /* _GST_DSHOW_UTIL_H_ */
