/* gdkinput-dmanipulation.c
 *
 * Copyright © 2022 the GTK team
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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/* {{{ */

#ifdef WINVER
#undef WINVER
#endif
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define WINVER 0x0603
#define _WIN32_WINNT 0x0603
#define COBJMACROS
#include "config.h"

#include <gdk/gdk.h>
#include "gdkwin32.h"
#include "gdkprivate-win32.h"
#include "gdkdevicemanager-win32.h"
#include "gdkdevice-virtual.h"
#include "gdkdeviceprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkeventsprivate.h"
#include "gdkseatdefaultprivate.h"
#include "gdkinput-dmanipulation.h"
#include "winpointer.h"

#include <windows.h>
#include <directmanipulation.h>

typedef BOOL
(WINAPI *getPointerType_t)(UINT32 pointerId, POINTER_INPUT_TYPE *pointerType);
static getPointerType_t getPointerType;

static IDirectManipulationManager *dmanipulation_manager;

typedef struct
{
  IDirectManipulationViewportEventHandlerVtbl *vtable;
  LONG reference_count;

  enum {
    GESTURE_PAN,
    GESTURE_ZOOM,
  } gesture;

  GdkTouchpadGesturePhase phase;
  gpointer sequence;

  float scale;
  float pan_x;
  float pan_y;

  GdkSurface *surface;
  GdkDevice *device;
}
DManipEventHandler;

static void dmanip_event_handler_running_state_clear (DManipEventHandler *handler);
static void dmanip_event_handler_free (DManipEventHandler *handler);

static void reset_viewport (IDirectManipulationViewport *viewport);

static gpointer util_get_next_sequence (void);
static GdkModifierType util_get_modifier_state (void);
static gboolean util_handler_free (gpointer);

/* }}} */
/* {{{ ViewportEventHandler */

static STDMETHODIMP_ (ULONG)
DManipEventHandler_AddRef (IDirectManipulationViewportEventHandler *self_)
{
  DManipEventHandler *self = (DManipEventHandler*) self_;

  return (ULONG) InterlockedIncrement (&self->reference_count);
}

static STDMETHODIMP_ (ULONG)
DManipEventHandler_Release (IDirectManipulationViewportEventHandler *self_)
{
  DManipEventHandler *self = (DManipEventHandler*) self_;

  /* NOTE: This may run from a worker thread */

  LONG new_reference_count = InterlockedDecrement (&self->reference_count);

  if (new_reference_count <= 0)
    {
      /* For safety, schedule the cleanup to be executed
       * on the main thread */
      g_idle_add (util_handler_free, self);

      return 0;
    }

  return (ULONG) new_reference_count;
}

static STDMETHODIMP
DManipEventHandler_QueryInterface (IDirectManipulationViewportEventHandler *self_,
                                   REFIID riid,
                                   void **ppvObject)
{
  DManipEventHandler *self = (DManipEventHandler*) self_;

  if G_UNLIKELY (!self || !ppvObject)
    return E_POINTER;

  *ppvObject = NULL;

  if (IsEqualGUID (riid, &IID_IUnknown))
    *ppvObject = self;
  else if (IsEqualGUID (riid, &IID_IDirectManipulationViewportEventHandler))
    *ppvObject = self;

  if (*ppvObject == NULL)
    return E_NOINTERFACE;

  DManipEventHandler_AddRef (self_);
  return S_OK;
}

/* NOTE:
 *
 * All DManipEventHandler callbacks are fired from the main thread */

static STDMETHODIMP
DManipEventHandler_OnViewportUpdated (IDirectManipulationViewportEventHandler *self_,
                                      IDirectManipulationViewport *viewport)
{
  return S_OK;
}

static STDMETHODIMP
DManipEventHandler_OnContentUpdated (IDirectManipulationViewportEventHandler *self_,
                                     IDirectManipulationViewport *viewport,
                                     IDirectManipulationContent *content)
{
  DManipEventHandler *self = (DManipEventHandler*) self_;
  float transform[6] = {1., 0., 0., 1., 0., 0.};
  HRESULT hr;

  hr = IDirectManipulationContent_GetContentTransform (content, transform,
                                                       G_N_ELEMENTS (transform));
  HR_CHECK_RETURN_VAL (hr, E_FAIL);

  switch (self->gesture)
    {
    case GESTURE_PAN:
      {
        GdkWin32Surface *surface_win32;
        GdkModifierType state;
        uint32_t time;
        int scale;
        float pan_x;
        float pan_y;
        GdkEvent *event;

        pan_x = transform[4];
        pan_y = transform[5];

        surface_win32 = GDK_WIN32_SURFACE (self->surface);
        scale = surface_win32->surface_scale;
        state = util_get_modifier_state ();
        time = (uint32_t) GetMessageTime ();

        event = gdk_scroll_event_new (self->surface,
                                      self->device,
                                      NULL, time, state,
                                      (self->pan_x - pan_x) / scale,
                                      (self->pan_y - pan_y) / scale,
                                      FALSE,
                                      GDK_SCROLL_UNIT_SURFACE);
        _gdk_win32_append_event (event);

        self->pan_x = pan_x;
        self->pan_y = pan_y;
      }
    break;
    case GESTURE_ZOOM:
      {
        GdkModifierType state;
        uint32_t time;
        POINT cursor = {0, 0};
        float scale;
        GdkEvent *event;

        scale = transform[0];

        state = util_get_modifier_state ();
        time = (uint32_t) GetMessageTime ();
        _gdk_win32_get_cursor_pos (&cursor);

        ScreenToClient (GDK_SURFACE_HWND (self->surface), &cursor);

        if (!self->sequence)
          self->sequence = util_get_next_sequence ();

        event = gdk_touchpad_event_new_pinch (self->surface, self->sequence, self->device,
                                              time, state, self->phase, cursor.x, cursor.y,
                                              2, 0.0, 0.0, scale, 0.0);
        _gdk_win32_append_event (event);

        self->scale = scale;
        self->phase = GDK_TOUCHPAD_GESTURE_PHASE_UPDATE;
      }
    break;
    default:
      g_assert_not_reached ();
    break;
    }

  return S_OK;
}

static STDMETHODIMP
DManipEventHandler_OnViewportStatusChanged (IDirectManipulationViewportEventHandler *self_,
                                            IDirectManipulationViewport *viewport,
                                            DIRECTMANIPULATION_STATUS current,
                                            DIRECTMANIPULATION_STATUS previous)
{
  DManipEventHandler *self = (DManipEventHandler*) self_;

  if (previous == DIRECTMANIPULATION_RUNNING)
    {
      switch (self->gesture)
        {
        case GESTURE_PAN:
          {
            GdkModifierType state;
            uint32_t time;
            GdkEvent *event;

            state = util_get_modifier_state ();
            time = (uint32_t) GetMessageTime ();

            event = gdk_scroll_event_new (self->surface, self->device,
                                          NULL, time, state,
                                          0.0, 0.0, TRUE,
                                          GDK_SCROLL_UNIT_SURFACE);
            _gdk_win32_append_event (event);
          }
        break;
        case GESTURE_ZOOM:
          {
            GdkModifierType state;
            uint32_t time;
            POINT cursor = {0, 0};
            GdkEvent *event;

            if (self->phase == GDK_TOUCHPAD_GESTURE_PHASE_BEGIN)
              break;

            state = util_get_modifier_state ();
            time = (uint32_t) GetMessageTime ();
            _gdk_win32_get_cursor_pos (&cursor);

            ScreenToClient (GDK_SURFACE_HWND (self->surface), &cursor);

            event = gdk_touchpad_event_new_pinch (self->surface, self->sequence, self->device,
                                                  time, state, GDK_TOUCHPAD_GESTURE_PHASE_END,
                                                  cursor.x, cursor.y, 2, 0., 0., self->scale,
                                                  0.);
            _gdk_win32_append_event (event);
          }
        break;
        default:
          g_assert_not_reached ();
        break;
        }

      dmanip_event_handler_running_state_clear (self);
      reset_viewport (viewport);
    }

  return S_OK;
}

static void
dmanip_event_handler_running_state_clear (DManipEventHandler *handler)
{
  handler->scale = 1.0;
  handler->pan_x = 0.0;
  handler->pan_y = 0.0;

  handler->phase = GDK_TOUCHPAD_GESTURE_PHASE_BEGIN;
  handler->sequence = NULL;
}

static DManipEventHandler*
dmanip_event_handler_new (GdkSurface *surface,
                          int gesture)
{
  static IDirectManipulationViewportEventHandlerVtbl vtable = {
    DManipEventHandler_QueryInterface,
    DManipEventHandler_AddRef,
    DManipEventHandler_Release,
    DManipEventHandler_OnViewportStatusChanged,
    DManipEventHandler_OnViewportUpdated,
    DManipEventHandler_OnContentUpdated,
  };
  DManipEventHandler *handler;

  handler = g_new0 (DManipEventHandler, 1);
  handler->vtable = &vtable;
  handler->reference_count = 1;

  handler->gesture = gesture;

  handler->surface = surface;
  handler->device = _gdk_device_manager->core_pointer;

  dmanip_event_handler_running_state_clear (handler);

  return handler;
}

static void
dmanip_event_handler_free (DManipEventHandler *handler)
{
  g_free (handler);
}


/* }}} */
/* {{{ Viewport utils */


static void
reset_viewport (IDirectManipulationViewport *viewport)
{
  IDirectManipulationContent *content = NULL;
  REFIID iid = &IID_IDirectManipulationContent;
  float identity[6] = {1., 0., 0., 1., 0., 0.};
  HRESULT hr;

  hr = IDirectManipulationViewport_GetPrimaryContent (viewport, iid, (void**)&content);
  HR_CHECK_GOTO (hr, failed);

  hr = IDirectManipulationContent_SyncContentTransform (content, identity,
                                                        G_N_ELEMENTS (identity));
  HR_CHECK_GOTO (hr, failed);

failed:
  IUnknown_Release (content);
}

static void
close_viewport (IDirectManipulationViewport **p_viewport)
{
  IDirectManipulationViewport *viewport = *p_viewport;

  if (viewport)
    {
      IDirectManipulationViewport_Abandon (viewport);
      IUnknown_Release (viewport);

      *p_viewport = NULL;
    }
}

static void
create_viewport (GdkSurface *surface,
                 int gesture,
                 IDirectManipulationViewport **pViewport)
{
  DIRECTMANIPULATION_CONFIGURATION configuration = 0;
  HWND hwnd = GDK_SURFACE_HWND (surface);
  IDirectManipulationViewportEventHandler *handler = NULL;
  DWORD cookie = 0;
  HRESULT hr;

  hr = IDirectManipulationManager_CreateViewport (dmanipulation_manager, NULL, hwnd,
                                                  &IID_IDirectManipulationViewport,
                                                  (void**) pViewport);
  HR_CHECK_GOTO (hr, failed);

  switch (gesture)
    {
    case GESTURE_PAN:
      configuration = DIRECTMANIPULATION_CONFIGURATION_INTERACTION |
                      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_X |
                      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y;
    break;
    case GESTURE_ZOOM:
      configuration = DIRECTMANIPULATION_CONFIGURATION_INTERACTION |
                      DIRECTMANIPULATION_CONFIGURATION_SCALING;
    break;
    default:
      g_assert_not_reached ();
    break;
    }

  handler = (IDirectManipulationViewportEventHandler*)
    dmanip_event_handler_new (surface, gesture);

  hr = IDirectManipulationViewport_AddEventHandler (*pViewport, hwnd, handler, &cookie);
  HR_CHECK_GOTO (hr, failed);

  hr = IDirectManipulationViewport_ActivateConfiguration (*pViewport, configuration);
  HR_CHECK_GOTO (hr, failed);

  hr = IDirectManipulationViewport_SetViewportOptions (*pViewport,
         DIRECTMANIPULATION_VIEWPORT_OPTIONS_DISABLEPIXELSNAPPING);

  hr = IDirectManipulationViewport_Enable (*pViewport);
  HR_CHECK_GOTO (hr, failed);

  // drop our initial reference
  IUnknown_Release (handler);
  return;

failed:
  if (handler)
    IUnknown_Release (handler);

  close_viewport (pViewport);
}


/* }}} */
/* {{{ Public */


void gdk_dmanipulation_initialize (void)
{
  if (!getPointerType)
    {
      HMODULE user32_mod;

      user32_mod = LoadLibraryW (L"user32.dll");
      if (!user32_mod)
        {
          WIN32_API_FAILED ("LoadLibraryW");
          return;
        }

      getPointerType = (getPointerType_t)
        GetProcAddress (user32_mod, "GetPointerType");

      if (!getPointerType)
        return;
    }

  if (!gdk_win32_ensure_com ())
        return;

  if (dmanipulation_manager == NULL)
    {
      HRESULT hr;

      hr = CoCreateInstance (&CLSID_DirectManipulationManager,
                             NULL,
                             CLSCTX_INPROC_SERVER,
                             &IID_IDirectManipulationManager,
                             (LPVOID*)&dmanipulation_manager);
      if (FAILED (hr))
        {
          if (hr == REGDB_E_CLASSNOTREG || hr == E_NOINTERFACE);
            /* Not an error,
             * DirectManipulation is not available */
          else HR_LOG (hr);
        }
    }
}

void gdk_dmanipulation_initialize_surface (GdkSurface *surface)
{
  GdkWin32Surface *surface_win32;
  HRESULT hr;

  if (!dmanipulation_manager)
    return;

  surface_win32 = GDK_WIN32_SURFACE (surface);

  create_viewport (surface, GESTURE_PAN,
                   &surface_win32->dmanipulation_viewport_pan);

  create_viewport (surface, GESTURE_ZOOM,
                   &surface_win32->dmanipulation_viewport_zoom);

  hr = IDirectManipulationManager_Activate (dmanipulation_manager,
                                            GDK_SURFACE_HWND (surface));
  HR_CHECK_RETURN (hr);
}

void gdk_dmanipulation_finalize_surface (GdkSurface *surface)
{
  GdkWin32Surface *surface_win32 = GDK_WIN32_SURFACE (surface);

  close_viewport (&surface_win32->dmanipulation_viewport_zoom);
  close_viewport (&surface_win32->dmanipulation_viewport_pan);
}

void gdk_dmanipulation_maybe_add_contact (GdkSurface *surface,
                                          MSG *msg)
{
  POINTER_INPUT_TYPE type = PT_POINTER;
  UINT32 pointer_id = GET_POINTERID_WPARAM (msg->wParam);

  if (!dmanipulation_manager)
    return;

  if (!getPointerType)
    return;

  if G_UNLIKELY (!getPointerType (pointer_id, &type))
    {
      WIN32_API_FAILED_LOG_ONCE ("GetPointerType");
      return;
    }

  if (type == PT_TOUCHPAD)
    {
      GdkWin32Surface *surface_win32 = GDK_WIN32_SURFACE (surface);
      HRESULT hr;

      hr = IDirectManipulationViewport_SetContact (surface_win32->dmanipulation_viewport_pan,
                                                   pointer_id);
      HR_CHECK_RETURN (hr);

      hr = IDirectManipulationViewport_SetContact (surface_win32->dmanipulation_viewport_zoom,
                                                   pointer_id);
      HR_CHECK_RETURN (hr);
    }
}


/* }}} */
/* {{{ Utils */


static gpointer
util_get_next_sequence (void)
{
  //TODO: sequence of other input types?
  static unsigned char *sequence_counter = 0;

  if (++sequence_counter == 0)
    sequence_counter++;

  return sequence_counter;
}

static GdkModifierType
util_get_modifier_state (void)
{
  GdkModifierType mask = 0;
  BYTE kbd[256];

  GetKeyboardState (kbd);
  if (kbd[VK_SHIFT] & 0x80)
    mask |= GDK_SHIFT_MASK;
  if (kbd[VK_CAPITAL] & 0x80)
    mask |= GDK_LOCK_MASK;
  if (kbd[VK_CONTROL] & 0x80)
    mask |= GDK_CONTROL_MASK;
  if (kbd[VK_MENU] & 0x80)
    mask |= GDK_ALT_MASK;

  return mask;
}

static gboolean
util_handler_free (gpointer handler)
{
  dmanip_event_handler_free ((DManipEventHandler*)handler);

  return G_SOURCE_REMOVE;
}


/* }}} */
