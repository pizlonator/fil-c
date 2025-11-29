/*
 * Copyright © 2001, 2007 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL RED HAT
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Owen Taylor, Red Hat, Inc.
 */

#include "config.h"

#include "xsettings-client.h"

#include <gdk/x11/gdkx11display.h>
#include <gdk/x11/gdkx11property.h>
#include <gdk/x11/gdkx11screen.h>
#include <gdk/x11/gdkx11surface.h>
#include <gdk/x11/gdkprivate-x11.h>
#include <gdk/x11/gdkdisplay-x11.h>
#include <gdk/x11/gdkscreen-x11.h>

#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xmd.h>		/* For CARD16 */

typedef enum
{
  GDK_SETTING_ACTION_NEW,
  GDK_SETTING_ACTION_CHANGED,
  GDK_SETTING_ACTION_DELETED
} GdkSettingAction;

#include "gdksettings.c"

/* Types of settings possible. Enum values correspond to
 * protocol values.
 */
typedef enum 
{
  XSETTINGS_TYPE_INT     = 0,
  XSETTINGS_TYPE_STRING  = 1,
  XSETTINGS_TYPE_COLOR   = 2
} XSettingsType;

typedef struct _XSettingsBuffer  XSettingsBuffer;

struct _XSettingsBuffer
{
  char byte_order;
  size_t len;
  unsigned char *data;
  unsigned char *pos;
};

static void
gdk_xsettings_notify (GdkX11Screen     *x11_screen,
                      const char       *name,
		      GdkSettingAction  action)
{
  gdk_display_setting_changed (x11_screen->display, name);
}

static gboolean
value_equal (const GValue *value_a,
             const GValue *value_b)
{
  if (G_VALUE_TYPE (value_a) != G_VALUE_TYPE (value_b))
    return FALSE;

  switch (G_VALUE_TYPE (value_a))
    {
    case G_TYPE_INT:
      return g_value_get_int (value_a) == g_value_get_int (value_b);
    case XSETTINGS_TYPE_COLOR:
      return gdk_rgba_equal (g_value_get_boxed (value_a), g_value_get_boxed (value_b));
    case G_TYPE_STRING:
      return g_str_equal (g_value_get_string (value_a), g_value_get_string (value_b));
    default:
      g_warning ("unable to compare values of type %s", g_type_name (G_VALUE_TYPE (value_a)));
      return FALSE;
    }
}

static void
notify_changes (GdkX11Screen *x11_screen,
		GHashTable   *old_list)
{
  GHashTableIter iter;
  GValue *setting, *old_setting;
  const char *name;

  if (x11_screen->xsettings != NULL)
    {
      g_hash_table_iter_init (&iter, x11_screen->xsettings);
      while (g_hash_table_iter_next (&iter, (gpointer *) &name, (gpointer*) &setting))
	{
	  old_setting = old_list ? g_hash_table_lookup (old_list, name) : NULL;

	  if (old_setting == NULL)
	    gdk_xsettings_notify (x11_screen, name, GDK_SETTING_ACTION_NEW);
	  else if (!value_equal (setting, old_setting))
	    gdk_xsettings_notify (x11_screen, name, GDK_SETTING_ACTION_CHANGED);
	    
	  /* remove setting from old_list */
	  if (old_setting != NULL)
	    g_hash_table_remove (old_list, name);
	}
    }

  if (old_list != NULL)
    {
      /* old_list now contains only deleted settings */
      g_hash_table_iter_init (&iter, old_list);
      while (g_hash_table_iter_next (&iter, (gpointer *) &name, (gpointer*) &old_setting))
	gdk_xsettings_notify (x11_screen, name, GDK_SETTING_ACTION_DELETED);
    }
}

#define BYTES_LEFT(buffer) ((buffer)->data + (buffer)->len - (buffer)->pos)

#define return_if_fail_bytes(buffer, n_bytes) G_STMT_START{ \
  if (BYTES_LEFT (buffer) < (n_bytes)) \
    { \
      g_warning ("Invalid XSETTINGS property (read off end: Expected %u bytes, only %"G_GSIZE_FORMAT" left", \
                 (n_bytes), BYTES_LEFT (buffer)); \
      return FALSE; \
    } \
}G_STMT_END

static gboolean
fetch_card16 (XSettingsBuffer *buffer,
	      CARD16          *result)
{
  CARD16 x;

  return_if_fail_bytes (buffer, 2);

  x = *(CARD16 *)buffer->pos;
  buffer->pos += 2;
  
  if (buffer->byte_order == MSBFirst)
    *result = GUINT16_FROM_BE (x);
  else
    *result = GUINT16_FROM_LE (x);

  return TRUE;
}

static gboolean
fetch_ushort (XSettingsBuffer *buffer,
	      unsigned short  *result) 
{
  CARD16 x;
  gboolean r;  

  r = fetch_card16 (buffer, &x);
  if (r)
    *result = x;

  return r;
}

static gboolean
fetch_card32 (XSettingsBuffer *buffer,
	      CARD32          *result)
{
  CARD32 x;

  return_if_fail_bytes (buffer, 4);

  x = *(CARD32 *)buffer->pos;
  buffer->pos += 4;
  
  if (buffer->byte_order == MSBFirst)
    *result = GUINT32_FROM_BE (x);
  else
    *result = GUINT32_FROM_LE (x);
  
  return TRUE;
}

static gboolean
fetch_card8 (XSettingsBuffer *buffer,
	     CARD8           *result)
{
  return_if_fail_bytes (buffer, 1);

  *result = *(CARD8 *)buffer->pos;
  buffer->pos += 1;

  return TRUE;
}

#define XSETTINGS_PAD(n,m) ((n + m - 1) & (~(m-1)))

static gboolean
fetch_string (XSettingsBuffer  *buffer,
              guint             length,
              char            **result)
{
  guint pad_len;

  pad_len = XSETTINGS_PAD (length, 4);
  if (pad_len < length) /* guard against overflow */
    {
      g_warning ("Invalid XSETTINGS property (overflow in string length)");
      return FALSE;
    }

  return_if_fail_bytes (buffer, pad_len);

  *result = g_strndup ((char *) buffer->pos, length);
  buffer->pos += pad_len;

  return TRUE;
}

static void
free_value (gpointer data)
{
  GValue *value = data;

  g_value_unset (value);
  g_free (value);
}

static GHashTable *
parse_settings (unsigned char *data,
		size_t         len)
{
  XSettingsBuffer buffer;
  GHashTable *settings = NULL;
  CARD32 serial;
  CARD32 n_entries;
  CARD32 i;
  GValue *value = NULL;
  char *x_name = NULL;
  const char *gdk_name;
  
  buffer.pos = buffer.data = data;
  buffer.len = len;
  
  if (!fetch_card8 (&buffer, (unsigned char *)&buffer.byte_order))
    goto out;

  if (buffer.byte_order != MSBFirst &&
      buffer.byte_order != LSBFirst)
    {
      g_warning ("Invalid XSETTINGS property (unknown byte order %u)", buffer.byte_order);
      goto out;
    }

  buffer.pos += 3;

  if (!fetch_card32 (&buffer, &serial) ||
      !fetch_card32 (&buffer, &n_entries))
    goto out;

  GDK_DEBUG (SETTINGS, "reading %lu settings (serial %lu byte order %u)",
                       (unsigned long)n_entries, (unsigned long)serial, buffer.byte_order);

  for (i = 0; i < n_entries; i++)
    {
      CARD8 type;
      CARD16 name_len;
      CARD32 v_int;
      
      if (!fetch_card8 (&buffer, &type))
	goto out;

      buffer.pos += 1;

      if (!fetch_card16 (&buffer, &name_len))
	goto out;

      if (!fetch_string (&buffer, name_len, &x_name) ||
          /* last change serial (we ignore it) */
          !fetch_card32 (&buffer, &v_int))
	goto out;

      switch (type)
	{
	case XSETTINGS_TYPE_INT:
	  if (!fetch_card32 (&buffer, &v_int))
	    goto out;

          value = g_new0 (GValue, 1);
          g_value_init (value, G_TYPE_INT);
          g_value_set_int (value, (gint32) v_int);

          GDK_DEBUG (SETTINGS, "  %s = %d", x_name, (gint32) v_int);
	  break;
	case XSETTINGS_TYPE_STRING:
          {
            char *s;

            if (!fetch_card32 (&buffer, &v_int) ||
                !fetch_string (&buffer, v_int, &s))
              goto out;
            
            value = g_new0 (GValue, 1);
            g_value_init (value, G_TYPE_STRING);
            g_value_take_string (value, s);

            GDK_DEBUG (SETTINGS, "  %s = \"%s\"", x_name, s);
          }
	  break;
	case XSETTINGS_TYPE_COLOR:
          {
            unsigned short red, green, blue, alpha;
            GdkRGBA rgba;

            if (!fetch_ushort (&buffer, &red) ||
                !fetch_ushort (&buffer, &green) ||
                !fetch_ushort (&buffer, &blue) ||
                !fetch_ushort (&buffer, &alpha))
              goto out;

            rgba.red = red / 65535.0;
            rgba.green = green / 65535.0;
            rgba.blue = blue / 65535.0;
            rgba.alpha = alpha / 65535.0;

            value = g_new0 (GValue, 1);
            g_value_init (value, GDK_TYPE_RGBA);
            g_value_set_boxed (value, &rgba);

            GDK_DEBUG (SETTINGS, "  %s = #%02X%02X%02X%02X", x_name, alpha,red, green, blue);
          }
	  break;
	default:
	  /* Quietly ignore unknown types */
          GDK_DEBUG (SETTINGS, "  %s = ignored (unknown type %u)", x_name, type);
	  break;
	}

      gdk_name = gdk_from_xsettings_name (x_name);
      g_free (x_name);
      x_name = NULL;

      if (gdk_name == NULL)
        {
          GDK_DEBUG (SETTINGS, "    ==> unknown to GTK");
          free_value (value);
        }
      else
        {
          GDK_DEBUG (SETTINGS, "    ==> storing as '%s'", gdk_name);

          if (settings == NULL)
            settings = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              NULL,
                                              free_value);

          if (g_hash_table_lookup (settings, gdk_name) != NULL)
            {
              g_warning ("Invalid XSETTINGS property (Duplicate entry for '%s')", gdk_name);
              goto out;
            }

          g_hash_table_insert (settings, (gpointer) gdk_name, value);
        }

      value = NULL;
    }

  return settings;

 out:

  if (value)
    free_value (value);

  if (settings)
    g_hash_table_unref (settings);

  g_free (x_name);

  return NULL;
}

static void
read_settings (GdkX11Screen *x11_screen,
               gboolean      do_notify)
{
  GdkDisplay *display = x11_screen->display;

  Atom type;
  int format;
  unsigned long n_items;
  unsigned long bytes_after;
  unsigned char *data;
  int result;

  GHashTable *old_list = x11_screen->xsettings;
  GValue value = G_VALUE_INIT;
  GValue *setting, *copy;

  x11_screen->xsettings = NULL;

  if (x11_screen->xsettings_manager_window != 0)
    {
      Atom xsettings_atom = gdk_x11_get_xatom_by_name_for_display (display, "_XSETTINGS_SETTINGS");

      gdk_x11_display_error_trap_push (display);
      result = XGetWindowProperty (gdk_x11_display_get_xdisplay (display),
                                   x11_screen->xsettings_manager_window,
				   xsettings_atom, 0, LONG_MAX,
				   False, xsettings_atom,
				   &type, &format, &n_items, &bytes_after, &data);
      gdk_x11_display_error_trap_pop_ignored (display);
      
      if (result == Success && type != None)
	{
	  if (type != xsettings_atom)
	    {
	      g_warning ("Invalid type for XSETTINGS property: %s", gdk_x11_get_xatom_name_for_display (display, type));
	    }
	  else if (format != 8)
	    {
	      g_warning ("Invalid format for XSETTINGS property: %d", format);
	    }
	  else
	    x11_screen->xsettings = parse_settings (data, n_items);
	  
	  XFree (data);
	}
    }

  /* Since we support scaling we look at the specific Gdk/UnscaledDPI
     setting if it exists and use that instead of Xft/DPI if it is set */
  if (x11_screen->xsettings && !x11_screen->fixed_surface_scale)
    {
      setting = g_hash_table_lookup (x11_screen->xsettings, "gdk-unscaled-dpi");
      if (setting)
	{
	  copy = g_new0 (GValue, 1);
	  g_value_init (copy, G_VALUE_TYPE (setting));
	  g_value_copy (setting, copy);
	  g_hash_table_insert (x11_screen->xsettings, 
			       (gpointer) "gtk-xft-dpi", copy);
	}
    }

  g_value_init (&value, G_TYPE_INT);

  if (!x11_screen->fixed_surface_scale &&
      gdk_display_get_setting (display, "gdk-window-scaling-factor", &value))
    _gdk_x11_screen_set_surface_scale (x11_screen, g_value_get_int (&value));

  /* XSettings gives us the cursor theme size in physical pixel size,
   * while we want logical pixel values instead.
   */
  if (x11_screen->surface_scale > 1 &&
      gdk_display_get_setting (display, "gtk-cursor-theme-size", &value))
    {
      int cursor_theme_size = g_value_get_int (&value);

      copy = g_new0 (GValue, 1);
      g_value_init (copy, G_TYPE_INT);
      g_value_set_int (copy, cursor_theme_size / x11_screen->surface_scale);
      g_hash_table_insert (x11_screen->xsettings,
                           (gpointer) "gtk-cursor-theme-size", copy);
    }

  if (do_notify)
    notify_changes (x11_screen, old_list);
  if (old_list)
    g_hash_table_unref (old_list);
}

static Atom
get_selection_atom (GdkX11Screen *x11_screen)
{
  return _gdk_x11_get_xatom_for_display_printf (x11_screen->display, "_XSETTINGS_S%d", x11_screen->screen_num);
}

static void
check_manager_window (GdkX11Screen *x11_screen,
                      gboolean      notify_changes)
{
  GdkDisplay *display;
  Display *xdisplay;

  display = x11_screen->display;
  xdisplay = gdk_x11_display_get_xdisplay (display);

  gdk_x11_display_grab (display);

  if (!(gdk_display_get_debug_flags (display) & GDK_DEBUG_DEFAULT_SETTINGS))
    x11_screen->xsettings_manager_window = XGetSelectionOwner (xdisplay, get_selection_atom (x11_screen));

  if (x11_screen->xsettings_manager_window != 0)
    XSelectInput (xdisplay,
                  x11_screen->xsettings_manager_window,
                  PropertyChangeMask | StructureNotifyMask);

  gdk_x11_display_ungrab (display);

  gdk_display_flush (display);

  read_settings (x11_screen, notify_changes);
}

GdkFilterReturn
gdk_xsettings_root_window_filter (const XEvent *xev,
                                  gpointer      data)
{
  GdkX11Screen *x11_screen = data;
  GdkDisplay *display = x11_screen->display;

  /* The checks here will not unlikely cause us to reread
   * the properties from the manager window a number of
   * times when the manager changes from A->B. But manager changes
   * are going to be pretty rare.
   */
  if (xev->xany.type == ClientMessage &&
      xev->xclient.message_type == gdk_x11_get_xatom_by_name_for_display (display, "MANAGER") &&
      xev->xclient.data.l[1] == get_selection_atom (x11_screen))
    {
      check_manager_window (x11_screen, TRUE);
      return GDK_FILTER_REMOVE;
    }

  return GDK_FILTER_CONTINUE;
}

GdkFilterReturn
gdk_xsettings_manager_window_filter (const XEvent *xev,
                                     gpointer      data)
{
  GdkX11Screen *x11_screen = data;

  if (xev->xany.type == DestroyNotify)
    {
      check_manager_window (x11_screen, TRUE);
      /* let GDK do its cleanup */
      return GDK_FILTER_CONTINUE;
    }
  else if (xev->xany.type == PropertyNotify)
    {
      read_settings (x11_screen, TRUE);
      return GDK_FILTER_REMOVE;
    }

  return GDK_FILTER_CONTINUE;
}

void
_gdk_x11_xsettings_init (GdkX11Screen *x11_screen)
{
  check_manager_window (x11_screen, FALSE);
}

void
_gdk_x11_settings_force_reread (GdkX11Screen *x11_screen)
{
  read_settings (x11_screen, TRUE);
}

void
_gdk_x11_xsettings_finish (GdkX11Screen *x11_screen)
{
  if (x11_screen->xsettings_manager_window)
    x11_screen->xsettings_manager_window = 0;

  if (x11_screen->xsettings)
    {
      g_hash_table_unref (x11_screen->xsettings);
      x11_screen->xsettings = NULL;
    }
}
