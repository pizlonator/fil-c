/* GTK - The GIMP Toolkit
 * Copyright (C) 2022 the GTK team
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

#include "gtkcolorpickerwin32private.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

GList *pickers;
HHOOK  hook;

static void remove_hook (void);

extern IMAGE_DOS_HEADER __ImageBase;
#define this_hmodule ((HMODULE)&__ImageBase)

struct _GtkColorPickerWin32
{
  GObject parent_instance;

  GTask *task;
  POINT  point;
};

struct _GtkColorPickerWin32Class
{
  GObjectClass parent_class;
};

static GInitableIface *initable_parent_iface;
static void gtk_color_picker_win32_initable_iface_init (GInitableIface *iface);
static void gtk_color_picker_win32_iface_init (GtkColorPickerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorPickerWin32, gtk_color_picker_win32, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gtk_color_picker_win32_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_PICKER, gtk_color_picker_win32_iface_init))

static gboolean
gtk_color_picker_win32_initable_init (GInitable     *initable,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
  return TRUE;
}

static void
gtk_color_picker_win32_initable_iface_init (GInitableIface *iface)
{
  initable_parent_iface = g_type_interface_peek_parent (iface);
  iface->init = gtk_color_picker_win32_initable_init;
}

static void
gtk_color_picker_win32_init (GtkColorPickerWin32 *picker)
{
}

static void
gtk_color_picker_win32_class_init (GtkColorPickerWin32Class *class)
{
}

GtkColorPicker *
gtk_color_picker_win32_new (void)
{
  return GTK_COLOR_PICKER (g_initable_new (GTK_TYPE_COLOR_PICKER_WIN32, NULL, NULL, NULL));
}

static void
on_task_completed (GObject    *object,
                   GParamSpec *pspec,
                   gpointer    user_data)
{
  gpointer source = g_task_get_source_object (G_TASK (object));
  GtkColorPickerWin32 *picker = GTK_COLOR_PICKER_WIN32 (source);

  g_clear_object (&picker->task);
}

static void
pick_color (GTask         *task,
            gpointer       source_object,
            gpointer       task_data,
            GCancellable  *cancellable)
{
  GtkColorPickerWin32 *picker = GTK_COLOR_PICKER_WIN32 (source_object);
  GdkRGBA rgba = (GdkRGBA) { 1.0, 1.0, 1.0, 1.0 };
  HDC hdc = GetDC(HWND_DESKTOP);

  if (hdc)
    {
      COLORREF color = GetPixel(hdc, picker->point.x, picker->point.y);

      rgba = (GdkRGBA){
        (double) GetRValue (color) / 255.0,
        (double) GetGValue (color) / 255.0,
        (double) GetBValue (color) / 255.0,
        1.0,
      };

      ReleaseDC (HWND_DESKTOP, hdc);
    }

  g_task_return_pointer (task,
                         gdk_rgba_copy (&rgba),
                         (GDestroyNotify) gdk_rgba_free);
}

static void
picked (GtkColorPickerWin32 *picker)
{
  g_task_run_in_thread (picker->task, pick_color);
}

static LRESULT CALLBACK
mouse_proc (int    nCode,
            WPARAM wParam,
            LPARAM lParam)
{
  if (nCode == HC_ACTION)
    {
      MSLLHOOKSTRUCT *info = (MSLLHOOKSTRUCT*) lParam;

      switch (wParam)
        {
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_XBUTTONDOWN:
          {
            GtkColorPickerWin32 *picker = GTK_COLOR_PICKER_WIN32 (pickers->data);

            if (!pickers)
              break;

            /* A low-level mouse hook always receives screen points in
             * per-monitor DPI aware screen coordinates, regardless of
             * the DPI awareness setting of the application. */
            picker->point = info->pt;

            picked (picker);

            pickers = g_list_delete_link (pickers, pickers);

            /* It's safe to remove a hook from within its callback */
            if (!pickers)
              remove_hook ();

            return 1;
          }
        break;
        default:
        break;
        }
    }

  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static gboolean
ensure_mouse_hook (void)
{
  if (!hook)
    {
      hook = SetWindowsHookEx (WH_MOUSE_LL, mouse_proc, this_hmodule, 0);
      if (!hook)
        {
          g_warning ("SetWindowsHookEx failed with error code "
                     "%"G_GUINT32_FORMAT, (unsigned) GetLastError ());
          return FALSE;
        }
    }

  return TRUE;
}

static void
remove_hook (void)
{
  if (hook)
    {
      UnhookWindowsHookEx (hook);
      hook = NULL;
    }
}

static void
gtk_color_picker_win32_pick (GtkColorPicker      *cp,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  GtkColorPickerWin32 *picker = GTK_COLOR_PICKER_WIN32 (cp);

  if (picker->task)
    return;

  picker->task = g_task_new (picker, NULL, callback, user_data);
  g_task_set_name (picker->task, "GtkColorPicker");
  g_signal_connect (picker->task, "notify::completed",
                    G_CALLBACK (on_task_completed),
                    NULL);

  if (!ensure_mouse_hook ())
    {
      g_task_return_new_error (picker->task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Cannot capture the mouse pointer");
      g_object_unref (picker->task);
      return;
    }

  pickers = g_list_prepend (pickers, cp);
}

static GdkRGBA *
gtk_color_picker_win32_pick_finish (GtkColorPicker  *cp,
                                    GAsyncResult    *res,
                                    GError         **error)
{
  g_return_val_if_fail (g_task_is_valid (res, cp), NULL);

  return g_task_propagate_pointer (G_TASK (res), error);
}

static void
gtk_color_picker_win32_iface_init (GtkColorPickerInterface *iface)
{
  iface->pick = gtk_color_picker_win32_pick;
  iface->pick_finish = gtk_color_picker_win32_pick_finish;
}
