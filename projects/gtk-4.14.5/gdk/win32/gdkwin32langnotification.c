/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2019 Руслан Ижбулатов <lrn1986@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#define COBJMACROS
#include <msctf.h>

#include <gdk/gdk.h>

#include "gdkprivate-win32.h"

struct _GdkWin32ALPNSink
{
  ITfActiveLanguageProfileNotifySink itf_alpn_sink;

  int ref_count;
};

typedef struct _GdkWin32ALPNSink GdkWin32ALPNSink;

static GdkWin32ALPNSink *actlangchangenotify = NULL;
static ITfSource *itf_source = NULL;
static DWORD actlangchangenotify_id = 0;

static ULONG STDMETHODCALLTYPE
alpn_sink_addref (ITfActiveLanguageProfileNotifySink *This)
{
  GdkWin32ALPNSink *alpn_sink = (GdkWin32ALPNSink *) This;
  int ref_count = ++alpn_sink->ref_count;

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
alpn_sink_queryinterface (ITfActiveLanguageProfileNotifySink *This,
                          REFIID                              riid,
                          LPVOID                             *ppvObject)
{
  *ppvObject = NULL;

  if (IsEqualGUID (riid, &IID_IUnknown))
    {
      ITfActiveLanguageProfileNotifySink_AddRef (This);
      *ppvObject = This;
      return S_OK;
    }

  if (IsEqualGUID (riid, &IID_ITfActiveLanguageProfileNotifySink))
    {
      ITfActiveLanguageProfileNotifySink_AddRef (This);
      *ppvObject = This;
      return S_OK;
    }

  return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE
alpn_sink_release (ITfActiveLanguageProfileNotifySink *This)
{
  GdkWin32ALPNSink *alpn_sink = (GdkWin32ALPNSink *) This;
  int ref_count = --alpn_sink->ref_count;

  if (ref_count == 0)
    {
      g_free (This);
    }

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
alpn_sink_on_activated (ITfActiveLanguageProfileNotifySink *This,
                        REFCLSID                            clsid,
                        REFGUID                             guidProfile,
                        BOOL                                fActivated)
{
  _gdk_input_locale_is_ime = fActivated;
  return S_OK;
}

static ITfActiveLanguageProfileNotifySinkVtbl alpn_sink_vtbl = {
  alpn_sink_queryinterface,
  alpn_sink_addref,
  alpn_sink_release,
  alpn_sink_on_activated,
};

static GdkWin32ALPNSink *
alpn_sink_new ()
{
  GdkWin32ALPNSink *result;

  result = g_new0 (GdkWin32ALPNSink, 1);
  result->itf_alpn_sink.lpVtbl = &alpn_sink_vtbl;
  result->ref_count = 0;

  ITfActiveLanguageProfileNotifySink_AddRef (&result->itf_alpn_sink);

  return result;
}


void
_gdk_win32_lang_notification_init ()
{
  HRESULT hr;
  ITfThreadMgr *itf_threadmgr;

  CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);

  if (actlangchangenotify != NULL)
    return;

  hr = CoCreateInstance (&CLSID_TF_ThreadMgr,
                         NULL,
                         CLSCTX_INPROC_SERVER,
                         &IID_ITfThreadMgr,
                         (LPVOID *) &itf_threadmgr);

  if (!SUCCEEDED (hr))
    return;

  hr = ITfThreadMgr_QueryInterface (itf_threadmgr, &IID_ITfSource, (VOID **) &itf_source);
  ITfThreadMgr_Release (itf_threadmgr);

  if (!SUCCEEDED (hr))
    return;

  actlangchangenotify = alpn_sink_new ();

  hr = ITfSource_AdviseSink (itf_source,
                             &IID_ITfActiveLanguageProfileNotifySink,
                             (IUnknown *) actlangchangenotify,
                             &actlangchangenotify_id);

  if (!SUCCEEDED (hr))
    {
      ITfActiveLanguageProfileNotifySink_Release (&actlangchangenotify->itf_alpn_sink);
      actlangchangenotify = NULL;
      ITfSource_Release (itf_source);
      itf_source = NULL;
    }
}

void
_gdk_win32_lang_notification_exit ()
{
  if (actlangchangenotify != NULL && itf_source != NULL)
    {
      ITfSource_UnadviseSink (itf_source, actlangchangenotify_id);
      ITfSource_Release (itf_source);
      ITfActiveLanguageProfileNotifySink_Release (&actlangchangenotify->itf_alpn_sink);
    }

  CoUninitialize ();
}
