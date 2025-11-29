/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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
#include "gdkdisplay.h"
#include "gdkcursor.h"
#include "gdkwin32.h"
#include "gdktextureprivate.h"

#include "gdkdisplay-win32.h"

#include "xcursors.h"

#include <stdint.h>

static struct {
  char *name;
  char *id;
} default_cursors[] = {
  /* -- Win32 cursor names: -- */
  { "appstarting", IDC_APPSTARTING },
  { "arrow", IDC_ARROW },
  { "cross", IDC_CROSS },
  { "dnd-move", IDC_ARROW },
  { "hand",  IDC_HAND },
  { "help",  IDC_HELP },
  { "ibeam", IDC_IBEAM },
  /* -- X11 cursor names: -- */
  { "left_ptr_watch", IDC_APPSTARTING },
  { "sizeall", IDC_SIZEALL },
  { "sizenesw", IDC_SIZENESW },
  { "sizens", IDC_SIZENS },
  { "sizenwse", IDC_SIZENWSE },
  { "sizewe", IDC_SIZEWE },
  { "uparrow", IDC_UPARROW },
  { "wait", IDC_WAIT },
  /* -- CSS cursor names: -- */
  { "default", IDC_ARROW },
  { "pointer", IDC_HAND },
  { "progress", IDC_APPSTARTING },
  { "crosshair", IDC_CROSS },
  { "text", IDC_IBEAM },
  { "move", IDC_SIZEALL },
  { "not-allowed", IDC_NO },
  { "all-scroll", IDC_SIZEALL },
  { "ew-resize", IDC_SIZEWE },
  { "e-resize", IDC_SIZEWE },
  { "w-resize", IDC_SIZEWE },
  { "col-resize", IDC_SIZEWE },
  { "ns-resize", IDC_SIZENS },
  { "n-resize", IDC_SIZENS },
  { "s-resize", IDC_SIZENS },
  { "row-resize", IDC_SIZENS },
  { "nesw-resize", IDC_SIZENESW },
  { "ne-resize", IDC_SIZENESW },
  { "sw-resize", IDC_SIZENESW },
  { "nwse-resize", IDC_SIZENWSE },
  { "nw-resize", IDC_SIZENWSE },
  { "se-resize", IDC_SIZENWSE }
};

typedef struct _GdkWin32HCursorTableEntry GdkWin32HCursorTableEntry;

struct _GdkWin32HCursorTableEntry
{
  HCURSOR  handle;
  guint64  refcount;
  gboolean destroyable;
};

struct _GdkWin32HCursor
{
  GObject          parent_instance;

  /* Do not do any modifications to the handle
   * (i.e. do not call DestroyCursor() on it).
   * It's a "read-only" copy, the original is
   * stored in the display instance */
  HANDLE           readonly_handle;

  /* This is a way to access the real handle
   * stored in the display.
   * TODO: make it a weak reference */
  GdkWin32Display *display;

  /* A copy of the "destoyable" attribute of
   * the handle */
  gboolean         readonly_destroyable;
};

struct _GdkWin32HCursorClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,
  PROP_DISPLAY,
  PROP_HANDLE,
  PROP_DESTROYABLE,
  NUM_PROPERTIES
};

G_DEFINE_TYPE (GdkWin32HCursor, gdk_win32_hcursor, G_TYPE_OBJECT)

static void
gdk_win32_hcursor_init (GdkWin32HCursor *win32_hcursor)
{
}

static void
gdk_win32_hcursor_finalize (GObject *gobject)
{
  GdkWin32HCursor *win32_hcursor = GDK_WIN32_HCURSOR (gobject);

  if (win32_hcursor->display)
    _gdk_win32_display_hcursor_unref (win32_hcursor->display, win32_hcursor->readonly_handle);

  g_clear_object (&win32_hcursor->display);

  G_OBJECT_CLASS (gdk_win32_hcursor_parent_class)->finalize (G_OBJECT (win32_hcursor));
}

static void
gdk_win32_hcursor_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GdkWin32HCursor *win32_hcursor;

  win32_hcursor = GDK_WIN32_HCURSOR (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_set_object (&win32_hcursor->display, g_value_get_object (value));
      break;

    case PROP_DESTROYABLE:
      win32_hcursor->readonly_destroyable = g_value_get_boolean (value);
      break;

    case PROP_HANDLE:
      win32_hcursor->readonly_handle = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
gdk_win32_hcursor_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GdkWin32HCursor *win32_hcursor;

  win32_hcursor = GDK_WIN32_HCURSOR (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, win32_hcursor->display);
      break;

    case PROP_DESTROYABLE:
      g_value_set_boolean (value, win32_hcursor->readonly_destroyable);
      break;

    case PROP_HANDLE:
      g_value_set_pointer (value, win32_hcursor->readonly_handle);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_win32_hcursor_constructed (GObject *object)
{
  GdkWin32HCursor *win32_hcursor;

  win32_hcursor = GDK_WIN32_HCURSOR (object);

  g_assert_nonnull (win32_hcursor->display);
  g_assert_nonnull (win32_hcursor->readonly_handle);

  _gdk_win32_display_hcursor_ref (win32_hcursor->display,
                                  win32_hcursor->readonly_handle,
                                  win32_hcursor->readonly_destroyable);
}

static GParamSpec *hcursor_props[NUM_PROPERTIES] = { NULL, };

static void
gdk_win32_hcursor_class_init (GdkWin32HCursorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_win32_hcursor_finalize;
  object_class->constructed = gdk_win32_hcursor_constructed;
  object_class->get_property = gdk_win32_hcursor_get_property;
  object_class->set_property = gdk_win32_hcursor_set_property;

  hcursor_props[PROP_DISPLAY] =
      g_param_spec_object ("display", NULL, NULL,
                           GDK_TYPE_DISPLAY,
                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  hcursor_props[PROP_HANDLE] =
      g_param_spec_pointer ("handle", NULL, NULL,
                            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  hcursor_props[PROP_DESTROYABLE] =
      g_param_spec_boolean ("destroyable", NULL, NULL,
                            TRUE,
                            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, hcursor_props);
}

GdkWin32HCursor *
gdk_win32_hcursor_new (GdkWin32Display *display,
                       HCURSOR          handle,
                       gboolean         destroyable)
{
  return g_object_new (GDK_TYPE_WIN32_HCURSOR,
                       "display", display,
                       "handle", handle,
                       "destroyable", destroyable,
                       NULL);
}

void
_gdk_win32_display_hcursor_ref (GdkWin32Display *display,
                                HCURSOR          handle,
                                gboolean         destroyable)
{
  GdkWin32HCursorTableEntry *entry;

  entry = g_hash_table_lookup (display->cursor_reftable, handle);

  if (entry)
    {
      if (entry->destroyable != destroyable)
        g_warning ("Destroyability metadata for cursor handle 0x%p does not match", handle);

      entry->refcount += 1;

      return;
    }

  entry = g_new0 (GdkWin32HCursorTableEntry, 1);
  entry->handle = handle;
  entry->destroyable = destroyable;
  entry->refcount = 1;

  g_hash_table_insert (display->cursor_reftable, handle, entry);
  display->cursors_for_destruction = g_list_remove_all (display->cursors_for_destruction, handle);
}

static gboolean
delayed_cursor_destruction (gpointer user_data)
{
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (user_data);
  HANDLE current_hcursor = GetCursor ();
  GList *p;

  win32_display->idle_cursor_destructor_id = 0;

  for (p = win32_display->cursors_for_destruction; p; p = p->next)
    {
      HCURSOR handle = (HCURSOR) p->data;

      if (handle == NULL)
        continue;

      if (current_hcursor == handle)
        {
          SetCursor (NULL);
          current_hcursor = NULL;
        }

      if (!DestroyCursor (handle))
        g_warning (G_STRLOC ": DestroyCursor (%p) failed: %lu", handle, GetLastError ());
    }

  g_list_free (win32_display->cursors_for_destruction);
  win32_display->cursors_for_destruction = NULL;

  return G_SOURCE_REMOVE;
}

void
_gdk_win32_display_hcursor_unref (GdkWin32Display *display,
                                  HCURSOR          handle)
{
  GdkWin32HCursorTableEntry *entry;
  gboolean destroyable;

  entry = g_hash_table_lookup (display->cursor_reftable, handle);

  if (!entry)
    {
      g_warning ("Trying to forget cursor handle 0x%p that is not in the table", handle);

      return;
    }

  entry->refcount -= 1;

  if (entry->refcount > 0)
    return;

  destroyable = entry->destroyable;

  g_hash_table_remove (display->cursor_reftable, handle);
  g_free (entry);

  if (!destroyable)
    return;

  /* GDK tends to destroy a cursor first, then set a new one.
   * This results in repeated oscillations between SetCursor(NULL)
   * and SetCursor(hcursor). To avoid that, delay cursor destruction a bit
   * to let GDK set a new one first. That way cursors are switched
   * seamlessly, without a NULL cursor between them.
   * If GDK sets the new cursor to the same handle the old cursor had,
   * the cursor handle is taken off the destruction list.
   */
  if (g_list_find (display->cursors_for_destruction, handle) == NULL)
    {
      display->cursors_for_destruction = g_list_prepend (display->cursors_for_destruction, handle);

      if (display->idle_cursor_destructor_id == 0)
        display->idle_cursor_destructor_id = g_idle_add (delayed_cursor_destruction, display);
    }
}

#ifdef gdk_win32_hcursor_get_handle
#undef gdk_win32_hcursor_get_handle
#endif

HCURSOR
gdk_win32_hcursor_get_handle (GdkWin32HCursor *cursor)
{
  return cursor->readonly_handle;
}

static HCURSOR
hcursor_from_x_cursor (int          i,
                       const char *name)
{
  int j, x, y, ofs;
  HCURSOR rv;
  int w, h;
  uint8_t *and_plane;
  uint8_t *xor_plane;

  w = GetSystemMetrics (SM_CXCURSOR);
  h = GetSystemMetrics (SM_CYCURSOR);

  and_plane = g_malloc ((w/8) * h);
  memset (and_plane, 0xff, (w/8) * h);
  xor_plane = g_malloc ((w/8) * h);
  memset (xor_plane, 0, (w/8) * h);

  if (strcmp (name, "none") != 0)
    {

#define SET_BIT(v,b)  (v |= (1 << b))
#define RESET_BIT(v,b)  (v &= ~(1 << b))

      for (j = 0, y = 0; y < cursors[i].height && y < h ; y++)
	{
	  ofs = (y * w) / 8;
	  j = y * cursors[i].width;

	  for (x = 0; x < cursors[i].width && x < w ; x++, j++)
	    {
	      int pofs = ofs + x / 8;
              uint8_t data = (cursors[i].data[j/4] & (0xc0 >> (2 * (j%4)))) >> (2 * (3 - (j%4)));
	      int bit = 7 - (j % cursors[i].width) % 8;

	      if (data)
		{
		  RESET_BIT (and_plane[pofs], bit);

		  if (data == 1)
		    SET_BIT (xor_plane[pofs], bit);
		}
	    }
	}

#undef SET_BIT
#undef RESET_BIT

      rv = CreateCursor (NULL, cursors[i].hotx, cursors[i].hoty,
			 w, h, and_plane, xor_plane);
    }
  else
    {
      rv = CreateCursor (NULL, 0, 0, w, h, and_plane, xor_plane);
    }

  if (rv == NULL)
    WIN32_API_FAILED ("CreateCursor");

  g_free (and_plane);
  g_free (xor_plane);

  return rv;
}

static GdkWin32HCursor *
win32_cursor_create_win32hcursor (GdkWin32Display *display,
                                  Win32Cursor     *cursor,
                                  const char      *name)
{
  GdkWin32HCursor *result;

  switch (cursor->load_type)
    {
      case GDK_WIN32_CURSOR_LOAD_FROM_FILE:
        result = gdk_win32_hcursor_new (display,
                                        LoadImageW (NULL,
                                                    cursor->resource_name,
                                                    IMAGE_CURSOR,
                                                    cursor->width,
                                                    cursor->height,
                                                    cursor->load_flags),
                                        cursor->load_flags & LR_SHARED ? FALSE : TRUE);
        break;
      case GDK_WIN32_CURSOR_LOAD_FROM_RESOURCE_NULL:
        result = gdk_win32_hcursor_new (display,
                                        LoadImageA (NULL,
                                                    (const char *) cursor->resource_name,
                                                    IMAGE_CURSOR,
                                                    cursor->width,
                                                    cursor->height,
                                                    cursor->load_flags),
                                        cursor->load_flags & LR_SHARED ? FALSE : TRUE);
        break;
      case GDK_WIN32_CURSOR_LOAD_FROM_RESOURCE_THIS:
        result = gdk_win32_hcursor_new (display,
                                        LoadImageA (GetModuleHandle (NULL),
                                                    (const char *) cursor->resource_name,
                                                    IMAGE_CURSOR,
                                                    cursor->width,
                                                    cursor->height,
                                                    cursor->load_flags),
                                        cursor->load_flags & LR_SHARED ? FALSE : TRUE);
        break;
      case GDK_WIN32_CURSOR_LOAD_FROM_RESOURCE_GTK:
        result = gdk_win32_hcursor_new (display,
                                        LoadImageA (this_module (),
                                                    (const char *) cursor->resource_name,
                                                    IMAGE_CURSOR,
                                                    cursor->width,
                                                    cursor->height,
                                                    cursor->load_flags),
                                        cursor->load_flags & LR_SHARED ? FALSE : TRUE);
        break;
      case GDK_WIN32_CURSOR_CREATE:
        result = gdk_win32_hcursor_new (display,
                                        hcursor_from_x_cursor (cursor->xcursor_number,
                                                               name),
                                        TRUE);
        break;
      default:
        result = NULL;
    }

  return result;
}

static Win32Cursor *
win32_cursor_new (GdkWin32CursorLoadType load_type,
                  gpointer               resource_name,
                  int                    width,
                  int                    height,
                  guint                  load_flags,
                  int                    xcursor_number)
{
  Win32Cursor *result;

  result = g_new (Win32Cursor, 1);
  result->load_type = load_type;
  result->resource_name = resource_name;
  result->width = width;
  result->height = height;
  result->load_flags = load_flags;
  result->xcursor_number = xcursor_number;

  return result;
}


static void
win32_cursor_destroy (gpointer data)
{
  Win32Cursor *cursor = data;

  /* resource_name could be a resource ID (uint16_t stored as a pointer),
   * which shouldn't be freed.
   */
  if (cursor->load_type == GDK_WIN32_CURSOR_LOAD_FROM_FILE)
    g_free (cursor->resource_name);

  g_free (cursor);
}

static void
win32_cursor_theme_load_from (Win32CursorTheme *theme,
                              int               size,
                              const char       *dir)
{
  GDir *gdir;
  const char *filename;
  HCURSOR hcursor;

  gdir = g_dir_open (dir, 0, NULL);

  if (gdir == NULL)
    return;

  while ((filename = g_dir_read_name (gdir)) != NULL)
    {
      char *fullname;
      gunichar2 *filenamew;
      char *cursor_name;
      char *dot;
      Win32Cursor *cursor;

      fullname = g_build_filename (dir, filename, NULL);
      filenamew = g_utf8_to_utf16 (fullname, -1, NULL, NULL, NULL);
      g_free (fullname);

      if (filenamew == NULL)
        continue;

      hcursor = LoadImageW (NULL, filenamew, IMAGE_CURSOR, size, size,
                            LR_LOADFROMFILE | (size == 0 ? LR_DEFAULTSIZE : 0));

      if (hcursor == NULL)
        {
          g_free (filenamew);
          continue;
        }

      DestroyCursor (hcursor);
      dot = strchr (filename, '.');

      cursor_name = dot ? g_strndup (filename, dot - filename) : g_strdup (filename);

      cursor = win32_cursor_new (GDK_WIN32_CURSOR_LOAD_FROM_FILE,
                                 filenamew,
                                 size,
                                 size,
                                 LR_LOADFROMFILE | (size == 0 ? LR_DEFAULTSIZE : 0),
                                 0);

      g_hash_table_insert (theme->named_cursors, cursor_name, cursor);
    }
}

static void
win32_cursor_theme_load_from_dirs (Win32CursorTheme *theme,
                                   const char       *name,
                                   int               size)
{
  char *theme_dir;
  const char * const *dirs;
  int i;

  dirs = g_get_system_data_dirs ();

  /* <prefix>/share/icons */
  for (i = 0; dirs[i]; i++)
    {
      theme_dir = g_build_filename (dirs[i], "icons", name, "cursors", NULL);
      win32_cursor_theme_load_from (theme, size, theme_dir);
      g_free (theme_dir);
    }

  /* ~/.icons */
  theme_dir = g_build_filename (g_get_home_dir (), "icons", name, "cursors", NULL);
  win32_cursor_theme_load_from (theme, size, theme_dir);
  g_free (theme_dir);
}

static void
win32_cursor_theme_load_system (Win32CursorTheme *theme,
                                int               size)
{
  int              i;
  HCURSOR          shared_hcursor;
  HCURSOR          x_hcursor;
  Win32Cursor     *cursor;

  for (i = 0; i < G_N_ELEMENTS (cursors); i++)
    {
      if (cursors[i].name == NULL)
        break;

      shared_hcursor = NULL;
      x_hcursor = NULL;

      /* Prefer W32 cursors */
      if (cursors[i].builtin)
        shared_hcursor = LoadImageA (NULL, cursors[i].builtin, IMAGE_CURSOR,
                                     size, size,
                                     LR_SHARED | (size == 0 ? LR_DEFAULTSIZE : 0));

      /* Fall back to X cursors, but only if we've got no theme cursor */
      if (shared_hcursor == NULL && g_hash_table_lookup (theme->named_cursors, cursors[i].name) == NULL)
        x_hcursor = hcursor_from_x_cursor (i, cursors[i].name);

      if (shared_hcursor == NULL && x_hcursor == NULL)
        continue;
      else if (x_hcursor != NULL)
        DestroyCursor (x_hcursor);

      cursor = win32_cursor_new (shared_hcursor ? GDK_WIN32_CURSOR_LOAD_FROM_RESOURCE_NULL : GDK_WIN32_CURSOR_CREATE,
                                 (gpointer) cursors[i].builtin,
                                 size,
                                 size,
                                 LR_SHARED | (size == 0 ? LR_DEFAULTSIZE : 0),
                                 x_hcursor ? i : 0);
      g_hash_table_insert (theme->named_cursors,
                           g_strdup (cursors[i].name),
                           cursor);
    }

  for (i = 0; i < G_N_ELEMENTS (default_cursors); i++)
    {
      if (default_cursors[i].name == NULL)
        break;

      shared_hcursor = LoadImageA (NULL, default_cursors[i].id, IMAGE_CURSOR, size, size,
                                   LR_SHARED | (size == 0 ? LR_DEFAULTSIZE : 0));

      if (shared_hcursor == NULL)
        continue;

      cursor = win32_cursor_new (GDK_WIN32_CURSOR_LOAD_FROM_RESOURCE_NULL,
                                 (gpointer) default_cursors[i].id,
                                 size,
                                 size,
                                 LR_SHARED | (size == 0 ? LR_DEFAULTSIZE : 0),
                                 0);
      g_hash_table_insert (theme->named_cursors,
                           g_strdup (default_cursors[i].name),
                           cursor);
    }
}

Win32CursorTheme *
win32_cursor_theme_load (const char *name,
                         int         size)
{
  Win32CursorTheme *result = g_new0 (Win32CursorTheme, 1);

  result->named_cursors = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 win32_cursor_destroy);

  if (strcmp (name, "system") == 0)
    {
      win32_cursor_theme_load_from_dirs (result, "Adwaita", size);
      win32_cursor_theme_load_system (result, size);
    }
  else
    {
      win32_cursor_theme_load_from_dirs (result, name, size);
    }

  if (g_hash_table_size (result->named_cursors) > 0)
    return result;

  win32_cursor_theme_destroy (result);
  return NULL;
}

void
win32_cursor_theme_destroy (Win32CursorTheme *theme)
{
  g_hash_table_destroy (theme->named_cursors);
  g_free (theme);
}

Win32Cursor *
win32_cursor_theme_get_cursor (Win32CursorTheme *theme,
                               const char       *name)
{
  return g_hash_table_lookup (theme->named_cursors, name);
}

static void
gdk_win32_cursor_remove_from_cache (gpointer data, GObject *cursor)
{
  GdkDisplay *display = data;
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (display);

  /* Unrefs the GdkWin32HCursor value object automatically */
  g_hash_table_remove (win32_display->cursors, cursor);
}

void
_gdk_win32_display_finalize_cursors (GdkWin32Display *display)
{
  GHashTableIter iter;
  gpointer cursor;

  if (display->cursors)
    {
      g_hash_table_iter_init (&iter, display->cursors);
      while (g_hash_table_iter_next (&iter, &cursor, NULL))
        g_object_weak_unref (G_OBJECT (cursor),
                             gdk_win32_cursor_remove_from_cache,
                             GDK_DISPLAY (display));
      g_hash_table_unref (display->cursors);
    }

  g_free (display->cursor_theme_name);

  g_list_free (display->cursors_for_destruction);
  display->cursors_for_destruction = NULL;

  if (display->cursor_theme)
    win32_cursor_theme_destroy (display->cursor_theme);
}

void
_gdk_win32_display_init_cursors (GdkWin32Display *display)
{
  display->cursors = g_hash_table_new_full (gdk_cursor_hash,
                                            gdk_cursor_equal,
                                            NULL,
                                            g_object_unref);
  display->cursor_reftable = g_hash_table_new (NULL, NULL);
  display->cursor_theme_name = g_strdup ("system");
}

/* This is where we use the names mapped to the equivalents that Windows defines by default */
static GdkWin32HCursor *
win32hcursor_idc_from_name (GdkWin32Display *display,
                            const char      *name)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (default_cursors); i++)
    {
      if (strcmp (default_cursors[i].name, name) != 0)
        continue;

      return gdk_win32_hcursor_new (display,
                                    LoadImageA (NULL, default_cursors[i].id, IMAGE_CURSOR, 0, 0,
                                                LR_SHARED | LR_DEFAULTSIZE),
                                    FALSE);
    }

  return NULL;
}

static GdkWin32HCursor *
win32hcursor_x_from_name (GdkWin32Display *display,
                          const char      *name)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (cursors); i++)
    if (cursors[i].name == NULL || strcmp (cursors[i].name, name) == 0)
      return gdk_win32_hcursor_new (display, hcursor_from_x_cursor (i, name), TRUE);

  return NULL;
}

static GdkWin32HCursor *
win32hcursor_from_theme (GdkWin32Display *display,
                         const char      *name)
{
  Win32CursorTheme *theme;
  Win32Cursor *theme_cursor;
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (display);

  if (name == NULL)
    return NULL;

  theme = _gdk_win32_display_get_cursor_theme (win32_display);
  theme_cursor = win32_cursor_theme_get_cursor (theme, name);

  if (theme_cursor == NULL)
    return NULL;

  return win32_cursor_create_win32hcursor (win32_display, theme_cursor, name);
}

static GdkWin32HCursor *
win32hcursor_from_name (GdkWin32Display *display,
                        const char      *name)
{
  GdkWin32HCursor *win32hcursor;

  /* Try current theme first */
  win32hcursor = win32hcursor_from_theme (display, name);

  if (win32hcursor != NULL)
    return win32hcursor;

  win32hcursor = win32hcursor_idc_from_name (display, name);

  if (win32hcursor != NULL)
    return win32hcursor;

  win32hcursor = win32hcursor_x_from_name (display, name);

  return win32hcursor;
}

/* Create a blank cursor */
static GdkWin32HCursor *
create_blank_win32hcursor (GdkWin32Display *display)
{
  int w, h;
  uint8_t *and_plane, *xor_plane;
  HCURSOR rv;

  w = GetSystemMetrics (SM_CXCURSOR);
  h = GetSystemMetrics (SM_CYCURSOR);

  and_plane = g_malloc ((w/8) * h);
  memset (and_plane, 0xff, (w/8) * h);
  xor_plane = g_malloc ((w/8) * h);
  memset (xor_plane, 0, (w/8) * h);

  rv = CreateCursor (NULL, 0, 0, w, h, and_plane, xor_plane);
  if (rv == NULL)
    WIN32_API_FAILED ("CreateCursor");

  return gdk_win32_hcursor_new (display, rv, TRUE);
}

static GdkWin32HCursor *
gdk_win32hcursor_create_for_name (GdkWin32Display  *display,
                                  const char *name)
{
  const HINSTANCE hinstance = GetModuleHandle (NULL);
  GdkWin32HCursor *win32hcursor = NULL;

  /* Blank cursor case */
  if (strcmp (name, "none") == 0)
    return create_blank_win32hcursor (display);

  win32hcursor = win32hcursor_from_name (display, name);

  if (win32hcursor)
    return win32hcursor;

  /* Allow to load named cursor resources linked into the executable.
   * Cursors obtained with LoadCursor() cannot be destroyed.
   */
  return gdk_win32_hcursor_new (display, LoadCursor (hinstance, name), FALSE);
}

static HICON
pixbuf_to_hicon (GdkPixbuf *pixbuf,
                 gboolean   is_icon,
                 int        x,
                 int        y);

HICON
_gdk_win32_create_hicon_for_texture (GdkTexture *texture,
                                     gboolean    is_icon,
                                     int         x,
                                     int         y)
{
  cairo_surface_t *surface;
  GdkPixbuf *pixbuf;
  int width, height;
  HICON icon;

  surface = gdk_texture_download_surface (texture);
  width = cairo_image_surface_get_width (surface);
  height = cairo_image_surface_get_height (surface);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);
G_GNUC_END_IGNORE_DEPRECATIONS

  icon = pixbuf_to_hicon (pixbuf, is_icon, x, y);

  g_object_unref (pixbuf);

  return icon;
}

static GdkWin32HCursor *
gdk_win32hcursor_create_for_texture (GdkWin32Display *display,
                                     GdkTexture      *texture,
                                     int              x,
                                     int              y)
{
  HICON icon = _gdk_win32_create_hicon_for_texture (texture, FALSE, x, y);

  return gdk_win32_hcursor_new (display, (HCURSOR) icon, TRUE);
}


static gboolean
_gdk_win32_cursor_update (GdkWin32Display  *win32_display,
                          GdkCursor        *cursor,
                          GdkWin32HCursor  *win32_hcursor,
                          GList           **update_cursors,
                          GList           **update_win32hcursors)
{
  GdkWin32HCursor  *win32hcursor_new = NULL;
  Win32CursorTheme *theme;
  Win32Cursor      *theme_cursor;

  const char *name = gdk_cursor_get_name (cursor);

  /* Do nothing if this is not a named cursor. */
  if (name == NULL)
    return FALSE;

  theme = _gdk_win32_display_get_cursor_theme (win32_display);
  theme_cursor = win32_cursor_theme_get_cursor (theme, name);

  if (theme_cursor != NULL)
    win32hcursor_new = win32_cursor_create_win32hcursor (win32_display, theme_cursor, name);

  if (win32hcursor_new == NULL)
    {
      g_warning (G_STRLOC ": Unable to load %s from the cursor theme", name);

      win32hcursor_new = win32hcursor_idc_from_name (win32_display, name);

      if (win32hcursor_new == NULL)
        win32hcursor_new = win32hcursor_x_from_name (win32_display, name);

      if (win32hcursor_new == NULL)
        return FALSE;
    }

  if (GetCursor () == win32_hcursor->readonly_handle)
    SetCursor (win32hcursor_new->readonly_handle);

  /* Don't modify the hash table mid-iteration, put everything into a list
   * and update the table later on.
   */
  *update_cursors = g_list_prepend (*update_cursors, cursor);
  *update_win32hcursors = g_list_prepend (*update_win32hcursors, win32hcursor_new);

  return TRUE;
}

void
_gdk_win32_display_update_cursors (GdkWin32Display *display)
{
  GHashTableIter iter;
  GdkCursor *cursor;
  GdkWin32HCursor *win32hcursor;
  GList *update_cursors = NULL;
  GList *update_win32hcursors = NULL;

  g_hash_table_iter_init (&iter, display->cursors);

  while (g_hash_table_iter_next (&iter, (gpointer *) &cursor, (gpointer *) &win32hcursor))
    _gdk_win32_cursor_update (display, cursor, win32hcursor, &update_cursors, &update_win32hcursors);

  while (update_cursors != NULL && update_win32hcursors != NULL)
    {
      g_hash_table_replace (display->cursors, update_cursors->data, update_win32hcursors->data);
      update_cursors = g_list_delete_link (update_cursors, update_cursors);
      update_win32hcursors = g_list_delete_link (update_win32hcursors, update_win32hcursors);
    }

  g_assert (update_cursors == NULL && update_win32hcursors == NULL);
}

GdkPixbuf *
gdk_win32_icon_to_pixbuf_libgtk_only (HICON hicon,
                                      double *x_hot,
                                      double *y_hot)
{
  GdkPixbuf *pixbuf = NULL;
  ICONINFO ii;
  struct
  {
    BITMAPINFOHEADER bi;
    RGBQUAD colors[2];
  } bmi;
  HDC hdc;
  uint8_t *pixels, *bits;
  int x, y, w, h;
  gsize rowstride;

  if (!GDI_CALL (GetIconInfo, (hicon, &ii)))
    return NULL;

  if (!(hdc = CreateCompatibleDC (NULL)))
    {
      WIN32_GDI_FAILED ("CreateCompatibleDC");
      goto out0;
    }

  memset (&bmi, 0, sizeof (bmi));
  bmi.bi.biSize = sizeof (bmi.bi);

  if (ii.hbmColor != NULL)
    {
      /* Colour cursor */

      gboolean no_alpha;

      if (!GDI_CALL (GetDIBits, (hdc, ii.hbmColor, 0, 1, NULL, (BITMAPINFO *)&bmi, DIB_RGB_COLORS)))
	goto out1;

      w = bmi.bi.biWidth;
      h = bmi.bi.biHeight;

      bmi.bi.biBitCount = 32;
      bmi.bi.biCompression = BI_RGB;
      bmi.bi.biHeight = -h;

      bits = g_malloc0 (4 * w * h);

      /* color data */
      if (!GDI_CALL (GetDIBits, (hdc, ii.hbmColor, 0, h, bits, (BITMAPINFO *)&bmi, DIB_RGB_COLORS)))
	goto out2;

      pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, w, h);
      pixels = gdk_pixbuf_get_pixels (pixbuf);
      rowstride = gdk_pixbuf_get_rowstride (pixbuf);
      no_alpha = TRUE;
      for (y = 0; y < h; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      pixels[2] = bits[(x+y*w) * 4];
	      pixels[1] = bits[(x+y*w) * 4 + 1];
	      pixels[0] = bits[(x+y*w) * 4 + 2];
	      pixels[3] = bits[(x+y*w) * 4 + 3];
	      if (no_alpha && pixels[3] > 0)
		no_alpha = FALSE;
	      pixels += 4;
	    }
	  pixels += rowstride - w * 4;
	}

      /* mask */
      if (no_alpha &&
	  GDI_CALL (GetDIBits, (hdc, ii.hbmMask, 0, h, bits, (BITMAPINFO *)&bmi, DIB_RGB_COLORS)))
	{
	  pixels = gdk_pixbuf_get_pixels (pixbuf);
	  for (y = 0; y < h; y++)
	    {
	      for (x = 0; x < w; x++)
		{
		  pixels[3] = 255 - bits[(x + y * w) * 4];
		  pixels += 4;
		}
	      pixels += rowstride - w * 4;
	    }
	}
    }
  else
    {
      /* B&W cursor */

      int bpl;

      if (!GDI_CALL (GetDIBits, (hdc, ii.hbmMask, 0, 0, NULL, (BITMAPINFO *)&bmi, DIB_RGB_COLORS)))
	goto out1;

      w = bmi.bi.biWidth;
      h = ABS (bmi.bi.biHeight) / 2;

      bits = g_malloc0 (4 * w * h);

      /* masks */
      if (!GDI_CALL (GetDIBits, (hdc, ii.hbmMask, 0, h*2, bits, (BITMAPINFO *)&bmi, DIB_RGB_COLORS)))
	goto out2;

      pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, w, h);
      pixels = gdk_pixbuf_get_pixels (pixbuf);
      rowstride = gdk_pixbuf_get_rowstride (pixbuf);
      bpl = ((w-1)/32 + 1)*4;
#if 0
      for (y = 0; y < h*2; y++)
	{
	  for (x = 0; x < w; x++)
	    {
	      const int bit = 7 - (x % 8);
	      printf ("%c ", ((bits[bpl*y+x/8])&(1<<bit)) ? ' ' : 'X');
	    }
	  printf ("\n");
	}
#endif

      for (y = 0; y < h; y++)
	{
          const uint8_t *andp, *xorp;
	  if (bmi.bi.biHeight < 0)
	    {
	      andp = bits + bpl*y;
	      xorp = bits + bpl*(h+y);
	    }
	  else
	    {
	      andp = bits + bpl*(h-y-1);
	      xorp = bits + bpl*(h+h-y-1);
	    }
	  for (x = 0; x < w; x++)
	    {
	      const int bit = 7 - (x % 8);
	      if ((*andp) & (1<<bit))
		{
		  if ((*xorp) & (1<<bit))
		    pixels[2] = pixels[1] = pixels[0] = 0xFF;
		  else
		    pixels[2] = pixels[1] = pixels[0] = 0;
		  pixels[3] = 0xFF;
		}
	      else
		{
		  pixels[2] = pixels[1] = pixels[0] = 0;
		  pixels[3] = 0;
		}
	      pixels += 4;
	      if (bit == 0)
		{
		  andp++;
		  xorp++;
		}
	    }
	  pixels += rowstride - w * 4;
	}
    }

  if (x_hot)
    *x_hot = ii.xHotspot;
  if (y_hot)
    *y_hot = ii.yHotspot;

  /* release temporary resources */
 out2:
  g_free (bits);
 out1:
  DeleteDC (hdc);
 out0:
  DeleteObject (ii.hbmColor);
  DeleteObject (ii.hbmMask);

  return pixbuf;
}

/* Convert a pixbuf to an HICON (or HCURSOR).  Supports alpha under
 * Windows XP, thresholds alpha otherwise.  Also used from
 * gdksurface-win32.c for creating application icons.
 */

static HBITMAP
create_alpha_bitmap (int       size,
                     uint8_t **outdata)
{
  BITMAPV5HEADER bi;
  HDC hdc;
  HBITMAP hBitmap;

  ZeroMemory (&bi, sizeof (BITMAPV5HEADER));
  bi.bV5Size = sizeof (BITMAPV5HEADER);
  bi.bV5Height = bi.bV5Width = size;
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  /* The following mask specification specifies a supported 32 BPP
   * alpha format for Windows XP (BGRA format).
   */
  bi.bV5RedMask   = 0x00FF0000;
  bi.bV5GreenMask = 0x0000FF00;
  bi.bV5BlueMask  = 0x000000FF;
  bi.bV5AlphaMask = 0xFF000000;

  /* Create the DIB section with an alpha channel. */
  hdc = GetDC (NULL);
  if (!hdc)
    {
      WIN32_GDI_FAILED ("GetDC");
      return NULL;
    }
  hBitmap = CreateDIBSection (hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS,
			      (PVOID *) outdata, NULL, (DWORD)0);
  if (hBitmap == NULL)
    WIN32_GDI_FAILED ("CreateDIBSection");
  ReleaseDC (NULL, hdc);

  return hBitmap;
}

static HBITMAP
create_color_bitmap (int       size,
                     uint8_t **outdata,
                     int       bits)
{
  struct {
    BITMAPV4HEADER bmiHeader;
    RGBQUAD bmiColors[2];
  } bmi;
  HDC hdc;
  HBITMAP hBitmap;

  ZeroMemory (&bmi, sizeof (bmi));
  bmi.bmiHeader.bV4Size = sizeof (BITMAPV4HEADER);
  bmi.bmiHeader.bV4Height = bmi.bmiHeader.bV4Width = size;
  bmi.bmiHeader.bV4Planes = 1;
  bmi.bmiHeader.bV4BitCount = bits;
  bmi.bmiHeader.bV4V4Compression = BI_RGB;

  /* when bits is 1, these will be used.
   * bmiColors[0] already zeroed from ZeroMemory()
   */
  bmi.bmiColors[1].rgbBlue = 0xFF;
  bmi.bmiColors[1].rgbGreen = 0xFF;
  bmi.bmiColors[1].rgbRed = 0xFF;

  hdc = GetDC (NULL);
  if (!hdc)
    {
      WIN32_GDI_FAILED ("GetDC");
      return NULL;
    }
  hBitmap = CreateDIBSection (hdc, (BITMAPINFO *)&bmi, DIB_RGB_COLORS,
			      (PVOID *) outdata, NULL, (DWORD)0);
  if (hBitmap == NULL)
    WIN32_GDI_FAILED ("CreateDIBSection");
  ReleaseDC (NULL, hdc);

  return hBitmap;
}

static gboolean
pixbuf_to_hbitmaps_alpha_winxp (GdkPixbuf *pixbuf,
				HBITMAP   *color,
				HBITMAP   *mask)
{
  /* Based on code from
   * http://www.dotnet247.com/247reference/msgs/13/66301.aspx
   */
  HBITMAP hColorBitmap, hMaskBitmap;
  const uint8_t *indata;
  const uint8_t *inrow;
  uint8_t *colordata, *colorrow, *maskdata, *maskbyte;
  int width, height, size, i, i_offset, j, j_offset, rowstride;
  guint maskstride, mask_bit;

  width = gdk_pixbuf_get_width (pixbuf); /* width of icon */
  height = gdk_pixbuf_get_height (pixbuf); /* height of icon */

  /* The bitmaps are created square */
  size = MAX (width, height);

  hColorBitmap = create_alpha_bitmap (size, &colordata);
  if (!hColorBitmap)
    return FALSE;
  hMaskBitmap = create_color_bitmap (size, &maskdata, 1);
  if (!hMaskBitmap)
    {
      DeleteObject (hColorBitmap);
      return FALSE;
    }

  /* MSDN says mask rows are aligned to "LONG" boundaries */
  maskstride = (((size + 31) & ~31) >> 3);

  indata = gdk_pixbuf_read_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);

  if (width > height)
    {
      i_offset = 0;
      j_offset = (width - height) / 2;
    }
  else
    {
      i_offset = (height - width) / 2;
      j_offset = 0;
    }

  for (j = 0; j < height; j++)
    {
      colorrow = colordata + 4*(j+j_offset)*size + 4*i_offset;
      maskbyte = maskdata + (j+j_offset)*maskstride + i_offset/8;
      mask_bit = (0x80 >> (i_offset % 8));
      inrow = indata + (height-j-1)*rowstride;
      for (i = 0; i < width; i++)
	{
	  colorrow[4*i+0] = inrow[4*i+2];
	  colorrow[4*i+1] = inrow[4*i+1];
	  colorrow[4*i+2] = inrow[4*i+0];
	  colorrow[4*i+3] = inrow[4*i+3];
	  if (inrow[4*i+3] == 0)
	    maskbyte[0] |= mask_bit;	/* turn ON bit */
	  else
	    maskbyte[0] &= ~mask_bit;	/* turn OFF bit */
	  mask_bit >>= 1;
	  if (mask_bit == 0)
	    {
	      mask_bit = 0x80;
	      maskbyte++;
	    }
	}
    }

  *color = hColorBitmap;
  *mask = hMaskBitmap;

  return TRUE;
}

static gboolean
pixbuf_to_hbitmaps_normal (GdkPixbuf *pixbuf,
			   HBITMAP   *color,
			   HBITMAP   *mask)
{
  /* Based on code from
   * http://www.dotnet247.com/247reference/msgs/13/66301.aspx
   */
  HBITMAP hColorBitmap, hMaskBitmap;
  const uint8_t *indata;
  const uint8_t *inrow;
  uint8_t *colordata, *colorrow, *maskdata, *maskbyte;
  int width, height, size, i, i_offset, j, j_offset, rowstride, nc, bmstride;
  gboolean has_alpha;
  guint maskstride, mask_bit;

  width = gdk_pixbuf_get_width (pixbuf); /* width of icon */
  height = gdk_pixbuf_get_height (pixbuf); /* height of icon */

  /* The bitmaps are created square */
  size = MAX (width, height);

  hColorBitmap = create_color_bitmap (size, &colordata, 24);
  if (!hColorBitmap)
    return FALSE;
  hMaskBitmap = create_color_bitmap (size, &maskdata, 1);
  if (!hMaskBitmap)
    {
      DeleteObject (hColorBitmap);
      return FALSE;
    }

  /* rows are always aligned on 4-byte boundaries */
  bmstride = size * 3;
  if (bmstride % 4 != 0)
    bmstride += 4 - (bmstride % 4);

  /* MSDN says mask rows are aligned to "LONG" boundaries */
  maskstride = (((size + 31) & ~31) >> 3);

  indata = gdk_pixbuf_read_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  nc = gdk_pixbuf_get_n_channels (pixbuf);
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  if (width > height)
    {
      i_offset = 0;
      j_offset = (width - height) / 2;
    }
  else
    {
      i_offset = (height - width) / 2;
      j_offset = 0;
    }

  for (j = 0; j < height; j++)
    {
      colorrow = colordata + (j+j_offset)*bmstride + 3*i_offset;
      maskbyte = maskdata + (j+j_offset)*maskstride + i_offset/8;
      mask_bit = (0x80 >> (i_offset % 8));
      inrow = indata + (height-j-1)*rowstride;
      for (i = 0; i < width; i++)
	{
	  if (has_alpha && inrow[nc*i+3] < 128)
	    {
	      colorrow[3*i+0] = colorrow[3*i+1] = colorrow[3*i+2] = 0;
	      maskbyte[0] |= mask_bit;	/* turn ON bit */
	    }
	  else
	    {
	      colorrow[3*i+0] = inrow[nc*i+2];
	      colorrow[3*i+1] = inrow[nc*i+1];
	      colorrow[3*i+2] = inrow[nc*i+0];
	      maskbyte[0] &= ~mask_bit;	/* turn OFF bit */
	    }
	  mask_bit >>= 1;
	  if (mask_bit == 0)
	    {
	      mask_bit = 0x80;
	      maskbyte++;
	    }
	}
    }

  *color = hColorBitmap;
  *mask = hMaskBitmap;

  return TRUE;
}

static HICON
pixbuf_to_hicon (GdkPixbuf *pixbuf,
		 gboolean   is_icon,
		 int        x,
		 int        y)
{
  ICONINFO ii;
  HICON icon;
  gboolean success;

  if (pixbuf == NULL)
    return NULL;

  if (gdk_pixbuf_get_has_alpha (pixbuf))
    success = pixbuf_to_hbitmaps_alpha_winxp (pixbuf, &ii.hbmColor, &ii.hbmMask);
  else
    success = pixbuf_to_hbitmaps_normal (pixbuf, &ii.hbmColor, &ii.hbmMask);

  if (!success)
    return NULL;

  ii.fIcon = is_icon;
  ii.xHotspot = x;
  ii.yHotspot = y;
  icon = CreateIconIndirect (&ii);
  DeleteObject (ii.hbmColor);
  DeleteObject (ii.hbmMask);
  return icon;
}

/**
 * gdk_win32_display_get_win32hcursor:
 * @display: (type GdkWin32Display): a `GdkDisplay`
 * @cursor: a `GdkCursor`
 *
 * Returns the Win32 HCURSOR wrapper object belonging to a `GdkCursor`,
 * potentially creating the cursor object.
 *
 * Be aware that the returned cursor may not be unique to @cursor.
 * It may for example be shared with its fallback cursor.
 *
 * Returns: a GdkWin32HCursor
 */
GdkWin32HCursor *
gdk_win32_display_get_win32hcursor (GdkWin32Display *display,
                                    GdkCursor       *cursor)
{
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (display);
  GdkWin32HCursor *win32hcursor;
  const char      *cursor_name;
  GdkCursor       *fallback;

  g_return_val_if_fail (cursor != NULL, NULL);

  if (gdk_display_is_closed (GDK_DISPLAY (display)))
    return NULL;

  win32hcursor = g_hash_table_lookup (win32_display->cursors, cursor);

  if (win32hcursor != NULL)
    return win32hcursor;

  cursor_name = gdk_cursor_get_name (cursor);

  if (cursor_name)
    win32hcursor = gdk_win32hcursor_create_for_name (display, cursor_name);
  else
    win32hcursor = gdk_win32hcursor_create_for_texture (display,
                                                        gdk_cursor_get_texture (cursor),
                                                        gdk_cursor_get_hotspot_x (cursor),
                                                        gdk_cursor_get_hotspot_y (cursor));

  if (win32hcursor != NULL)
    {
      g_object_weak_ref (G_OBJECT (cursor), gdk_win32_cursor_remove_from_cache, display);
      g_hash_table_insert (win32_display->cursors, cursor, win32hcursor);

      return win32hcursor;
    }

  fallback = gdk_cursor_get_fallback (cursor);

  if (fallback)
    return gdk_win32_display_get_win32hcursor (display, fallback);

  return NULL;
}
