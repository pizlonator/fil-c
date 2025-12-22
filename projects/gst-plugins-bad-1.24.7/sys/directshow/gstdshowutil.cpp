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

#include <dmodshow.h>
#include <dmoreg.h>

#include "gstdshowutil.h"
#include "gstdshowfakesrc.h"

_COM_SMARTPTR_TYPEDEF(IDMOWrapperFilter, __uuidof(IDMOWrapperFilter));

IPin *
gst_dshow_util_get_pin_from_filter (IBaseFilter *filter, PIN_DIRECTION pindir)
{
  IEnumPinsPtr enumpins;
  IPinPtr pin;
  HRESULT hres;

  hres = filter->EnumPins (&enumpins);
  if (FAILED(hres)) {
    return NULL;
  }

  while (enumpins->Next (1, &pin, NULL) == S_OK)
  {
    PIN_DIRECTION pindirtmp;
    hres = pin->QueryDirection (&pindirtmp);
    if (hres == S_OK && pindir == pindirtmp) {
      return pin;
    }
    pin.Release();
  }

  return NULL;
}

IBaseFilter *
gst_dshow_util_find_filter(CLSID input_majortype, CLSID input_subtype,
                      CLSID output_majortype, CLSID output_subtype,
                      PreferredFilter *preferred_filters)
{
  HRESULT hres;
  GUID inTypes[2];
  GUID outTypes[2];
  IFilterMapper2Ptr mapper;
  IEnumMonikerPtr enum_moniker;
  IMonikerPtr moniker;
  ULONG fetched;
  IBaseFilter *filter;

  /* First, see if any of our preferred filters is available.
   * If not, we fall back to the highest-ranked installed filter */
  if (preferred_filters) {
    while (preferred_filters->filter_guid)
    {
      /* If the filter is a DMO, we need to do this a bit differently */
      if (preferred_filters->dmo_category)
      {
        IDMOWrapperFilterPtr wrapper;

        hres = CoCreateInstance (CLSID_DMOWrapperFilter, NULL,
          CLSCTX_INPROC,
          IID_IBaseFilter, (void **)&filter);
        if (SUCCEEDED(hres)) {
          hres = filter->QueryInterface (&wrapper);
          if (SUCCEEDED(hres)) {
            hres = wrapper->Init (*preferred_filters->filter_guid,
                *preferred_filters->dmo_category);
            if (SUCCEEDED(hres))
              return filter;
          }
          filter->Release();
        }
      }
      else
      {
        hres = CoCreateInstance (*preferred_filters->filter_guid,
          NULL, CLSCTX_INPROC,
          IID_IBaseFilter, (void **)&filter);
        if (SUCCEEDED(hres))
          return filter;
      }

      /* Continue to the next filter */
      preferred_filters++;
    }
  }

  hres = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC,
      IID_IFilterMapper2, (void **) &mapper);
  if (FAILED(hres))
    return NULL;

  inTypes[0] = input_majortype;
  inTypes[1] = input_subtype;
  outTypes[0] = output_majortype;
  outTypes[1] = output_subtype;

  hres = mapper->EnumMatchingFilters (&enum_moniker, 0,
          FALSE, MERIT_DO_NOT_USE+1,
          TRUE, 1, inTypes, NULL, NULL, FALSE,
          TRUE, 1, outTypes, NULL, NULL);
  if (FAILED(hres))
    return NULL;

  enum_moniker->Reset ();

  while(enum_moniker->Next (1, &moniker, &fetched) == S_OK)
  {
    hres = moniker->BindToObject(NULL, NULL,
          IID_IBaseFilter, (void**)&filter);
    if(SUCCEEDED(hres)) {
      return filter;
    }
    moniker.Release ();
  }

  return NULL;
}

