/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */
#include "config.h"

#include "gdkwin32keys.h"

#include "gdk.h"

#include "gdkprivate-win32.h"
#include "gdkdebugprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkkeysyms.h"
#include "gdkkeysprivate.h"
#include "gdkkeys-win32.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#define GDK_MOD2_MASK (1 << 4)

/* GdkWin32Keymap */

struct _GdkWin32KeymapClass
{
  GdkKeymapClass parent_class;
};

struct _GdkWin32Keymap
{
  GdkKeymap parent_instance;

  /* Array of HKL */
  GArray *layout_handles;

  /* Array of GdkWin32KeymapLayoutInfo */
  GArray *layout_infos;

  /* Index of a handle in layout_handles,
   * at any point it should be the same handle as GetKeyboardLayout(0) returns,
   * but GDK caches it to avoid calling GetKeyboardLayout (0) every time.
   */
  guint8 active_layout;

  guint current_serial;

  /* Pointer to the implementation to be used. See comment in gdkkeys-win32.h.
   * (we will dynamically choose at runtime for 32-bit builds based on whether
   * we are running under WOW64)
   */
  const GdkWin32KeymapImpl *gdkwin32_keymap_impl;
};

G_DEFINE_TYPE (GdkWin32Keymap, gdk_win32_keymap, GDK_TYPE_KEYMAP)

guint _gdk_keymap_serial = 0;
static GdkKeymap *default_keymap = NULL;

static void update_keymap              (GdkWin32Keymap *gdk_keymap);
static void clear_keyboard_layout_info (gpointer        data);

static void
gdk_win32_keymap_init (GdkWin32Keymap *keymap)
{
  /* Regular implementation (32 bit & 64 bit) */
  extern const GdkWin32KeymapImpl gdkwin32_keymap_impl;
  /* Implementation for 32 bit applications running on a 64 bit host (WOW64). */
#ifndef _WIN64
  extern const GdkWin32KeymapImpl gdkwin32_keymap_impl_wow64;
#endif

  keymap->layout_infos = g_array_new (FALSE, TRUE,
                                      sizeof (GdkWin32KeymapLayoutInfo));
  g_array_set_clear_func (keymap->layout_infos,
                          clear_keyboard_layout_info);

  keymap->layout_handles = g_array_new (FALSE, FALSE,
                                        sizeof (GdkWin32KeymapLayoutInfo));
  keymap->active_layout = 0;

  keymap->gdkwin32_keymap_impl = &gdkwin32_keymap_impl;
#ifndef _WIN64
  if (_gdk_win32_check_processor (GDK_WIN32_WOW64))
    keymap->gdkwin32_keymap_impl = &gdkwin32_keymap_impl_wow64;
#endif

  update_keymap (keymap);
}

static void
gdk_win32_keymap_finalize (GObject *object)
{
  GdkWin32Keymap *keymap = GDK_WIN32_KEYMAP (object);

  g_clear_pointer (&keymap->layout_handles, g_array_unref);
  g_clear_pointer (&keymap->layout_infos, g_array_unref);

  G_OBJECT_CLASS (gdk_win32_keymap_parent_class)->finalize (object);
}

/* Convenience wrapper functions */

static gboolean
load_layout_dll (GdkWin32Keymap           *keymap,
                 const char               *dll,
                 GdkWin32KeymapLayoutInfo *info)
{
  return keymap->gdkwin32_keymap_impl->load_layout_dll (dll, info);
}

static void
init_vk_lookup_table (GdkWin32Keymap           *keymap,
                      GdkWin32KeymapLayoutInfo *info)
{
  keymap->gdkwin32_keymap_impl->init_vk_lookup_table (info);
}

static BYTE
keystate_to_modbits (GdkWin32Keymap           *keymap,
                     GdkWin32KeymapLayoutInfo *info,
                     const BYTE                keystate[256])
{
  return keymap->gdkwin32_keymap_impl->keystate_to_modbits (info, keystate);
}

static BYTE
modbits_to_level (GdkWin32Keymap           *keymap,
                  GdkWin32KeymapLayoutInfo *info,
                  BYTE                      modbits)
{
  return keymap->gdkwin32_keymap_impl->modbits_to_level (info, modbits);
}

static WCHAR
vk_to_char_fuzzy (GdkWin32Keymap           *keymap,
                  GdkWin32KeymapLayoutInfo *info,
                  BYTE                      mod_bits,
                  BYTE                      lock_bits,
                  BYTE                     *consumed_mod_bits,
                  gboolean                 *is_dead,
                  BYTE                      vk)
{
  return keymap->gdkwin32_keymap_impl->vk_to_char_fuzzy (info, mod_bits, lock_bits,
                                                         consumed_mod_bits, is_dead, vk);
}

/*
 * Return the keyboard layout according to the user's keyboard layout
 * substitution preferences.
 *
 * The result is heap-allocated and should be freed with g_free().
 */
static char*
get_keyboard_layout_substituted_name (const char *layout_name)
{
  HKEY     hkey     = 0;
  DWORD    var_type = REG_SZ;
  char    *result   = NULL;
  DWORD    buf_len  = 0;
  LSTATUS  status;

  static const char *substitute_path = "Keyboard Layout\\Substitutes";

  status = RegOpenKeyExA (HKEY_CURRENT_USER, substitute_path, 0,
                          KEY_QUERY_VALUE, &hkey);
  if (status != ERROR_SUCCESS)
    {
      /* No substitute set for this value, not sure if this is a normal case */
      g_warning("Could not open registry key '%s'. Error code: %d",
                substitute_path, (int)status);

      goto fail1;
    }

  status = RegQueryValueExA (hkey, layout_name, 0, &var_type, 0, &buf_len);
  if (status != ERROR_SUCCESS)
    {
      g_debug("Could not query registry key '%s\\%s'. Error code: %d",
              substitute_path, layout_name, (int)status);
      goto fail2;
    }

  /* Allocate buffer */
  result = (char*) g_malloc (buf_len);

  /* Retrieve substitute name */
  status = RegQueryValueExA (hkey, layout_name, 0, &var_type,
                             (LPBYTE) result, &buf_len);
  if (status != ERROR_SUCCESS)
    {
      g_warning("Could not obtain registry value at key '%s\\%s'. "
                "Error code: %d",
                substitute_path, layout_name, (int)status);
      goto fail3;
    }

  RegCloseKey (hkey);
  return result;

fail3:
  g_free (result);
fail2:
  RegCloseKey (hkey);
fail1:
  return NULL;
}

static char*
_get_keyboard_layout_file (const char *layout_name)
{
  HKEY     hkey              = 0;
  DWORD    var_type          = REG_SZ;
  char    *result            = NULL;
  DWORD    file_name_len     = 0;
  int      dir_len           = 0;
  int      buf_len           = 0;
  LSTATUS  status;

  static const char prefix[] = "SYSTEM\\CurrentControlSet\\Control\\"
                               "Keyboard Layouts\\";
  char kbdKeyPath[sizeof (prefix) + KL_NAMELENGTH];

  g_snprintf (kbdKeyPath, sizeof (prefix) + KL_NAMELENGTH, "%s%s",
              prefix, layout_name);

  status = RegOpenKeyExA (HKEY_LOCAL_MACHINE, (LPCSTR) kbdKeyPath, 0,
                          KEY_QUERY_VALUE, &hkey);
  if (status != ERROR_SUCCESS)
    {
      g_warning("Could not open registry key '%s'. Error code: %d",
                kbdKeyPath, (int)status);
      goto fail1;
    }

  /* Get sizes */
  status = RegQueryValueExA (hkey, "Layout File", 0, &var_type, 0,
                             &file_name_len);
  if (status != ERROR_SUCCESS)
    {
      g_warning("Could not query registry key '%s\\Layout File'. Error code: %d",
                kbdKeyPath, (int)status);
      goto fail2;
    }

  dir_len = GetSystemDirectoryA (0, 0); /* includes \0 */
  if (dir_len == 0)
    {
      g_warning("GetSystemDirectoryA failed. Error: %d", (int)GetLastError());
      goto fail2;
    }

  /* Allocate buffer */
  buf_len = dir_len + (int) strlen ("\\") + file_name_len;
  result = (char*) g_malloc (buf_len);

  /* Append system directory. The -1 is because dir_len includes \0 */
  if (GetSystemDirectoryA (&result[0], dir_len) != dir_len - 1)
    goto fail3;

  /* Append directory separator */
  result[dir_len - 1] = '\\';

  /* Append file name */
  status = RegQueryValueExA (hkey, "Layout File", 0, &var_type,
                             (LPBYTE) &result[dir_len], &file_name_len);
  if (status != ERROR_SUCCESS)
    {
      goto fail3;
    }

  result[dir_len + file_name_len] = '\0';

  RegCloseKey (hkey);
  return result;

fail3:
  g_free (result);
fail2:
  RegCloseKey (hkey);
fail1:
  return NULL;
}

/*
 * Get the file path of the keyboard layout dll.
 * The result is heap-allocated and should be freed with g_free().
 */
static char*
get_keyboard_layout_file (const char *layout_name)
{
  char *result = _get_keyboard_layout_file (layout_name);

  /* If we could not retrieve a path, it may be that we need to perform layout
   * substitution
   */
  if (result == NULL)
    {
      char *substituted = get_keyboard_layout_substituted_name (layout_name);
      result = _get_keyboard_layout_file (substituted);
      g_free (substituted);
    }

  return result;
}

static void
clear_keyboard_layout_info (gpointer data)
{
  GdkWin32KeymapLayoutInfo *layout_info = (GdkWin32KeymapLayoutInfo*) data;

  g_free (layout_info->file);

  if (layout_info->key_entries != NULL)
    g_array_unref (layout_info->key_entries);

  if (layout_info->reverse_lookup_table != NULL)
    g_hash_table_destroy (layout_info->reverse_lookup_table);

  if (layout_info->lib != NULL)
    FreeLibrary (layout_info->lib);

  memset (layout_info, 0, sizeof (GdkWin32KeymapLayoutInfo));
}

#define DEFINE_SPECIAL(map)                 \
  map (VK_CANCEL,     GDK_KEY_Cancel)       \
  map (VK_BACK,       GDK_KEY_BackSpace)    \
  map (VK_CLEAR,      GDK_KEY_Clear)        \
  map (VK_RETURN,     GDK_KEY_Return)       \
  map (VK_LSHIFT,     GDK_KEY_Shift_L)      \
  map (VK_LCONTROL,   GDK_KEY_Control_L)    \
  map (VK_LMENU,      GDK_KEY_Alt_L)        \
  map (VK_PAUSE,      GDK_KEY_Pause)        \
  map (VK_ESCAPE,     GDK_KEY_Escape)       \
  map (VK_PRIOR,      GDK_KEY_Prior)        \
  map (VK_NEXT,       GDK_KEY_Next)         \
  map (VK_END,        GDK_KEY_End)          \
  map (VK_HOME,       GDK_KEY_Home)         \
  map (VK_LEFT,       GDK_KEY_Left)         \
  map (VK_UP,         GDK_KEY_Up)           \
  map (VK_RIGHT,      GDK_KEY_Right)        \
  map (VK_DOWN,       GDK_KEY_Down)         \
  map (VK_SELECT,     GDK_KEY_Select)       \
  map (VK_PRINT,      GDK_KEY_Print)        \
  map (VK_EXECUTE,    GDK_KEY_Execute)      \
  map (VK_INSERT,     GDK_KEY_Insert)       \
  map (VK_DELETE,     GDK_KEY_Delete)       \
  map (VK_HELP,       GDK_KEY_Help)         \
  map (VK_LWIN,       GDK_KEY_Meta_L)       \
  map (VK_RWIN,       GDK_KEY_Meta_R)       \
  map (VK_APPS,       GDK_KEY_Menu)         \
  map (VK_DECIMAL,    GDK_KEY_KP_Decimal)   \
  map (VK_MULTIPLY,   GDK_KEY_KP_Multiply)  \
  map (VK_ADD,        GDK_KEY_KP_Add)       \
  map (VK_SEPARATOR,  GDK_KEY_KP_Separator) \
  map (VK_SUBTRACT,   GDK_KEY_KP_Subtract)  \
  map (VK_DIVIDE,     GDK_KEY_KP_Divide)    \
  map (VK_NUMPAD0,    GDK_KEY_KP_0)         \
  map (VK_NUMPAD1,    GDK_KEY_KP_1)         \
  map (VK_NUMPAD2,    GDK_KEY_KP_2)         \
  map (VK_NUMPAD3,    GDK_KEY_KP_3)         \
  map (VK_NUMPAD4,    GDK_KEY_KP_4)         \
  map (VK_NUMPAD5,    GDK_KEY_KP_5)         \
  map (VK_NUMPAD6,    GDK_KEY_KP_6)         \
  map (VK_NUMPAD7,    GDK_KEY_KP_7)         \
  map (VK_NUMPAD8,    GDK_KEY_KP_8)         \
  map (VK_NUMPAD9,    GDK_KEY_KP_9)         \
  map (VK_F1,         GDK_KEY_F1)           \
  map (VK_F2,         GDK_KEY_F2)           \
  map (VK_F3,         GDK_KEY_F3)           \
  map (VK_F4,         GDK_KEY_F4)           \
  map (VK_F5,         GDK_KEY_F5)           \
  map (VK_F6,         GDK_KEY_F6)           \
  map (VK_F7,         GDK_KEY_F7)           \
  map (VK_F8,         GDK_KEY_F8)           \
  map (VK_F9,         GDK_KEY_F9)           \
  map (VK_F10,        GDK_KEY_F10)          \
  map (VK_F11,        GDK_KEY_F11)          \
  map (VK_F12,        GDK_KEY_F12)          \
  map (VK_F13,        GDK_KEY_F13)          \
  map (VK_F14,        GDK_KEY_F14)          \
  map (VK_F15,        GDK_KEY_F15)          \
  map (VK_F16,        GDK_KEY_F16)          \
  map (VK_F17,        GDK_KEY_F17)          \
  map (VK_F18,        GDK_KEY_F18)          \
  map (VK_F19,        GDK_KEY_F19)          \
  map (VK_F20,        GDK_KEY_F20)          \
  map (VK_F21,        GDK_KEY_F21)          \
  map (VK_F22,        GDK_KEY_F22)          \
  map (VK_F23,        GDK_KEY_F23)          \
  map (VK_F24,        GDK_KEY_F24)          \
  map (VK_NUMLOCK,    GDK_KEY_Num_Lock)     \
  map (VK_SCROLL,     GDK_KEY_Scroll_Lock)  \
  map (VK_RSHIFT,     GDK_KEY_Shift_R)      \
  map (VK_RCONTROL,   GDK_KEY_Control_R)    \
  map (VK_RMENU,      GDK_KEY_Alt_R)        \
  map (VK_CAPITAL,    GDK_KEY_Caps_Lock)


#define DEFINE_DEAD(map)                                                      \
  map ('"',                          /* 0x022 */ GDK_KEY_dead_diaeresis)      \
  map ('\'',                         /* 0x027 */ GDK_KEY_dead_acute)          \
  map (GDK_KEY_asciicircum,          /* 0x05e */ GDK_KEY_dead_circumflex)     \
  map (GDK_KEY_grave,                /* 0x060 */ GDK_KEY_dead_grave)          \
  map (GDK_KEY_asciitilde,           /* 0x07e */ GDK_KEY_dead_tilde)          \
  map (GDK_KEY_diaeresis,            /* 0x0a8 */ GDK_KEY_dead_diaeresis)      \
  map (GDK_KEY_degree,               /* 0x0b0 */ GDK_KEY_dead_abovering)      \
  map (GDK_KEY_acute,                /* 0x0b4 */ GDK_KEY_dead_acute)          \
  map (GDK_KEY_periodcentered,       /* 0x0b7 */ GDK_KEY_dead_abovedot)       \
  map (GDK_KEY_cedilla,              /* 0x0b8 */ GDK_KEY_dead_cedilla)        \
  map (GDK_KEY_breve,                /* 0x1a2 */ GDK_KEY_dead_breve)          \
  map (GDK_KEY_ogonek,               /* 0x1b2 */ GDK_KEY_dead_ogonek)         \
  map (GDK_KEY_caron,                /* 0x1b7 */ GDK_KEY_dead_caron)          \
  map (GDK_KEY_doubleacute,          /* 0x1bd */ GDK_KEY_dead_doubleacute)    \
  map (GDK_KEY_abovedot,             /* 0x1ff */ GDK_KEY_dead_abovedot)       \
  map (0x1000384,              /* Greek tonos */ GDK_KEY_dead_acute)          \
  map (GDK_KEY_Greek_accentdieresis, /* 0x7ae */ GDK_KEY_Greek_accentdieresis)


static guint
vk_and_mod_bits_to_gdk_keysym (GdkWin32Keymap     *keymap,
                               GdkWin32KeymapLayoutInfo *info,
                               guint               vk,
                               BYTE                mod_bits,
                               BYTE                lock_bits,
                               BYTE               *consumed_mod_bits)

{
  gboolean is_dead = FALSE;
  gunichar c;
  guint    sym;

  if (consumed_mod_bits)
    *consumed_mod_bits = 0;

  /* Handle special key: Tab */
  if (vk == VK_TAB)
    {
      if (consumed_mod_bits)
        *consumed_mod_bits = mod_bits & KBDSHIFT;
      return (mod_bits & KBDSHIFT) ? GDK_KEY_ISO_Left_Tab : GDK_KEY_Tab;
    }

  /* Handle other special keys */
  switch (vk)
    {
      #define MAP(a_vk, a_gdk) case a_vk: return a_gdk;

      DEFINE_SPECIAL (MAP)

      /* Non-bijective mappings: */
      MAP (VK_SHIFT,    GDK_KEY_Shift_L)
      MAP (VK_CONTROL,  GDK_KEY_Control_L)
      MAP (VK_MENU,     GDK_KEY_Alt_L)
      MAP (VK_SNAPSHOT, GDK_KEY_Print)

      #undef MAP
    }

  /* Handle regular keys (including dead keys) */
  c = vk_to_char_fuzzy (keymap, info, mod_bits, lock_bits,
                        consumed_mod_bits, &is_dead, vk);

  if (c == WCH_NONE)
    return GDK_KEY_VoidSymbol;

  sym = gdk_unicode_to_keyval (c);

  if (is_dead)
    {
      switch (sym)
	{
	  #define MAP(a_nondead, a_dead) case a_nondead: return a_dead;
	  DEFINE_DEAD (MAP)
	  #undef MAP
	}
    }

  return sym;
}

static int
gdk_keysym_to_key_entry_index (GdkWin32KeymapLayoutInfo *info,
                               guint               sym)
{
  gunichar c;
  gintptr  index;

  if (info->reverse_lookup_table == NULL)
    return -1;

  /* Special cases */
  if (sym == GDK_KEY_Tab)
    return VK_TAB;
  if (sym == GDK_KEY_ISO_Left_Tab)
    return 256;

  /* Generic non-printable keys */
  switch (sym)
    {
      #define MAP(a_vk, a_gdk) case a_gdk: return a_vk;
      DEFINE_SPECIAL (MAP)
      #undef MAP
    }

  /* Fix up dead keys */
  #define MAP(a_nondead, a_dead) \
    if (sym == a_dead)           \
      sym = a_nondead;
  DEFINE_DEAD (MAP)
  #undef MAP

  /* Try converting to Unicode and back */
  c = gdk_keyval_to_unicode (sym);

  index = -1;
  if (g_hash_table_lookup_extended (info->reverse_lookup_table,
                                    GINT_TO_POINTER (c),
                                    NULL, (gpointer*) &index))
    {
      return index;
    }
  else
    {
      return -1;
    }
}

static GdkModifierType
mod_bits_to_gdk_mod_mask (BYTE mod_bits)
{
  GdkModifierType result = 0;
  if (mod_bits & KBDSHIFT)
    result |= GDK_SHIFT_MASK;
  if (mod_bits & KBDCTRL)
    result |= GDK_CONTROL_MASK;
  if (mod_bits & KBDALT)
    result |= GDK_ALT_MASK;
  return result;
}

static BYTE
gdk_mod_mask_to_mod_bits (GdkModifierType mod_mask)
{
  BYTE result = 0;
  if (mod_mask & GDK_SHIFT_MASK)
    result |= KBDSHIFT;
  if (mod_mask & GDK_CONTROL_MASK)
    result |= KBDCTRL;
  if (mod_mask & GDK_ALT_MASK)
    result |= KBDALT;
  return result;
}

static void
update_keymap (GdkWin32Keymap *keymap)
{
  HKL  current_layout;
  BOOL changed = FALSE;
  int  n_layouts;
  int  i;

  if (keymap->current_serial == _gdk_keymap_serial &&
      keymap->layout_handles->len > 0)
    {
      return;
    }

  n_layouts = GetKeyboardLayoutList (0, 0);
  g_array_set_size (keymap->layout_handles, n_layouts);
  n_layouts = GetKeyboardLayoutList (n_layouts,
		                     &g_array_index(keymap->layout_handles,
				                    HKL, 0));

  g_array_set_size (keymap->layout_infos, n_layouts);

  current_layout = GetKeyboardLayout (0);

  for (i = 0; i < n_layouts; ++i)
    {
      GdkWin32KeymapLayoutInfo *info = &g_array_index(keymap->layout_infos,
                                                      GdkWin32KeymapLayoutInfo, i);
      HKL hkl = g_array_index(keymap->layout_handles, HKL, i);

      if (info->handle != hkl)
        {
          changed = TRUE;

          /* Free old data */
          clear_keyboard_layout_info (info);

          /* Load new data */
          info->handle = hkl;
          ActivateKeyboardLayout (hkl, 0);
          GetKeyboardLayoutNameA (info->name);

          info->file = get_keyboard_layout_file (info->name);

          if (info->file != NULL && load_layout_dll (keymap, info->file, info))
            {
              info->key_entries = g_array_new (FALSE, FALSE,
                                               sizeof (GdkWin32KeymapKeyEntry));

              info->reverse_lookup_table = g_hash_table_new (g_direct_hash,
                                                             g_direct_equal);
              init_vk_lookup_table (keymap, info);
            }
          else
            {
              g_warning("Failed to load keyboard layout DLL for layout %s: %s",
                        info->name, info->file);
            }
        }

      if (info->handle == current_layout)
        keymap->active_layout = i;
    }

  if (changed)
    ActivateKeyboardLayout (current_layout, 0);

  keymap->current_serial = _gdk_keymap_serial;
}

guint8
_gdk_win32_keymap_get_rshift_scancode (GdkWin32Keymap *keymap)
{
  return MapVirtualKey (VK_RSHIFT, MAPVK_VK_TO_VSC);
}

void
_gdk_win32_keymap_set_active_layout (GdkWin32Keymap *keymap,
                                     HKL             hkl)
{
  if (keymap != NULL &&
      keymap->layout_handles->len > 0)
    {
      int group;

      for (group = 0; group < keymap->layout_handles->len; group++)
        if (g_array_index (keymap->layout_handles, HKL, group) == hkl)
          keymap->active_layout = group;
    }
}

guint8
_gdk_win32_keymap_get_active_group (GdkWin32Keymap *keymap)
{
  if (keymap != NULL &&
      keymap->layout_handles->len > 0)
    return keymap->active_layout;

  return 0;
}

GdkKeymap*
_gdk_win32_display_get_keymap (GdkDisplay *display)
{
  g_return_val_if_fail (display == gdk_display_get_default (), NULL);

  if (default_keymap == NULL)
    default_keymap = g_object_new (gdk_win32_keymap_get_type (), NULL);

  return default_keymap;
}

GdkModifierType
_gdk_win32_keymap_get_mod_mask (GdkWin32Keymap *keymap)
{
  GdkWin32KeymapLayoutInfo *layout_info;
  BYTE                      keystate[256] = {0};
  BYTE                      mod_bits;
    
  update_keymap (keymap);

  layout_info = &g_array_index (keymap->layout_infos, GdkWin32KeymapLayoutInfo,
                                keymap->active_layout);
    
  GetKeyboardState (keystate);
    
  mod_bits = keystate_to_modbits (keymap, layout_info, keystate);

  return mod_bits_to_gdk_mod_mask (mod_bits);
}

static PangoDirection
get_hkl_direction (HKL hkl)
{
  switch (PRIMARYLANGID (LOWORD ((DWORD) (gintptr) hkl)))
    {
    case LANG_HEBREW:
    case LANG_ARABIC:
#ifdef LANG_URDU
    case LANG_URDU:
#endif
    case LANG_FARSI:
      /* Others? */
      return PANGO_DIRECTION_RTL;

    default:
      return PANGO_DIRECTION_LTR;
    }
}

static PangoDirection
gdk_win32_keymap_get_direction (GdkKeymap *gdk_keymap)
{
  GdkWin32Keymap *keymap;
  HKL             active_hkl;
  
  g_return_val_if_fail (GDK_IS_KEYMAP (gdk_keymap), PANGO_DIRECTION_LTR);

  keymap = GDK_WIN32_KEYMAP (gdk_keymap);

  update_keymap (keymap);

  if (keymap->layout_handles->len <= 0)
    active_hkl = GetKeyboardLayout (0);
  else
    active_hkl = g_array_index (keymap->layout_handles, HKL,
                                keymap->active_layout);

  return get_hkl_direction (active_hkl);
}

static gboolean
gdk_win32_keymap_have_bidi_layouts (GdkKeymap *gdk_keymap)
{
  GdkWin32Keymap *keymap;
  gboolean        have_rtl = FALSE;
  gboolean        have_ltr = FALSE;
  int             group;

  g_return_val_if_fail (GDK_IS_KEYMAP (gdk_keymap), FALSE);

  keymap = GDK_WIN32_KEYMAP (gdk_keymap);

  update_keymap (keymap);

  for (group = 0; group < keymap->layout_handles->len; group++)
    {
      if (get_hkl_direction (g_array_index (keymap->layout_handles, HKL,
                             group)) == PANGO_DIRECTION_RTL)
        have_rtl = TRUE;
      else
        have_ltr = TRUE;
    }

  return have_ltr && have_rtl;
}

static gboolean
gdk_win32_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  (void) keymap;

  return ((GetKeyState (VK_CAPITAL) & 1) != 0);
}

static gboolean
gdk_win32_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  (void) keymap;

  return ((GetKeyState (VK_NUMLOCK) & 1) != 0);
}

static gboolean
gdk_win32_keymap_get_scroll_lock_state (GdkKeymap *keymap)
{
  (void) keymap;

  return ((GetKeyState (VK_SCROLL) & 1) != 0);
}

static gboolean
gdk_win32_keymap_get_entries_for_keyval (GdkKeymap     *gdk_keymap,
                                         guint          keyval,
                                         GArray        *retval)
{
  GdkWin32Keymap *keymap;
  int             group;
  guint           len = retval->len;

  g_return_val_if_fail (GDK_IS_KEYMAP (gdk_keymap), FALSE);
  g_return_val_if_fail (keyval != 0, FALSE);

  keymap = GDK_WIN32_KEYMAP (gdk_keymap);

  update_keymap (keymap);

  for (group = 0; group < keymap->layout_handles->len; group++)
    {
      GdkWin32KeymapLayoutInfo *info = &g_array_index (keymap->layout_infos,
                                                       GdkWin32KeymapLayoutInfo,
                                                       group);
      int entry_index = gdk_keysym_to_key_entry_index (info, keyval);

      while (entry_index >= 0)
        {
          GdkWin32KeymapKeyEntry  *entry        = &g_array_index (info->key_entries,
                                                                  GdkWin32KeymapKeyEntry,
                                                                  entry_index);
          BYTE                     base_modbits = entry->mod_bits;
          BYTE                     extra_modbits;
          GdkKeymapKey             gdk_key      = {0};

          /* Add original key combination */
          gdk_key.keycode = entry->vk;
          gdk_key.level   = modbits_to_level (keymap, info, entry->mod_bits);
          gdk_key.group   = group;

          g_array_append_val (retval, gdk_key);

          /* Add combinations with modifiers that do not affect the translation */
          for (extra_modbits = 0;
               extra_modbits <= info->max_modbit_value;
               ++extra_modbits)
            {
              BYTE  modbits;
              guint sym;

              /* We are only interested in masks which are orthogonal to the
               * original mask. */
              if ((extra_modbits | base_modbits) == base_modbits ||
                  (extra_modbits & base_modbits) != 0)
                continue;

              modbits = base_modbits | extra_modbits;

              /* Check if the additional modifiers change the semantics.
               * If they do not, add them. */
              sym = vk_and_mod_bits_to_gdk_keysym (keymap, info, entry->vk,
                                                   modbits, 0, NULL);
              if (sym == keyval || sym == GDK_KEY_VoidSymbol)
                {
                  gdk_key.keycode = entry->vk;
                  gdk_key.level   = modbits_to_level (keymap, info, modbits);
                  gdk_key.group   = group;
                  g_array_append_val (retval, gdk_key);
                }
            }

          entry_index = entry->next;
        }
   }

  return retval->len > len;
}

static gboolean
gdk_win32_keymap_get_entries_for_keycode (GdkKeymap     *gdk_keymap,
                                          guint          hardware_keycode,
                                          GdkKeymapKey **keys,
                                          guint        **keyvals,
                                          int           *n_entries)
{
  GdkWin32Keymap *keymap;
  GArray         *key_array;
  GArray         *keyval_array;
  int             group;
  BYTE            vk;

  g_return_val_if_fail (GDK_IS_KEYMAP (gdk_keymap), FALSE);
  g_return_val_if_fail (n_entries != NULL, FALSE);

  *n_entries = 0;

  if (keys != NULL)
    key_array = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));
  else
    key_array = NULL;

  if (keyvals != NULL)
    keyval_array = g_array_new (FALSE, FALSE, sizeof (guint));
  else
    keyval_array = NULL;

  keymap = GDK_WIN32_KEYMAP (gdk_keymap);
  update_keymap (keymap);

  vk = hardware_keycode;

  for (group = 0; group < keymap->layout_handles->len; group++)
    {
      GdkWin32KeymapLayoutInfo *info = &g_array_index (keymap->layout_infos,
                                                       GdkWin32KeymapLayoutInfo,
                                                       group);
      int level;

      for (level = 0; level <= info->max_level; ++level)
        {
          BYTE         modbits          = info->level_to_modbits[level];
          BYTE         consumed_modbits = 0;
          GdkKeymapKey key              = {0};
          guint        keyval;

          keyval = vk_and_mod_bits_to_gdk_keysym (keymap, info, vk, modbits, 0, &consumed_modbits);

          if (keyval == GDK_KEY_VoidSymbol || consumed_modbits != modbits)
            continue;

          key.keycode = vk;
          key.group = group;
          key.level = level;

          if (key_array)
            g_array_append_val (key_array, key);

          if (keyval_array)
            g_array_append_val (keyval_array, keyval);

          ++(*n_entries);
        }
    }
  
  if (keys != NULL)
    *keys = (GdkKeymapKey*) g_array_free (key_array, FALSE);

  if (keyvals != NULL)
    *keyvals = (guint*) g_array_free (keyval_array, FALSE);

  return *n_entries > 0;
}

static guint
gdk_win32_keymap_lookup_key (GdkKeymap          *gdk_keymap,
                             const GdkKeymapKey *key)
{
  GdkWin32Keymap           *keymap;
  GdkWin32KeymapLayoutInfo *info;

  BYTE                      modbits;
  guint                     sym;

  g_return_val_if_fail (GDK_IS_KEYMAP (gdk_keymap), 0);
  g_return_val_if_fail (key != NULL, 0);

  keymap = GDK_WIN32_KEYMAP (gdk_keymap);
  update_keymap (keymap);

  info = &g_array_index (keymap->layout_infos, GdkWin32KeymapLayoutInfo, key->group);

  if (key->group < 0 || key->group >= keymap->layout_handles->len)
    return 0;
  if (key->level < 0 || key->level > info->max_level)
    return 0;

  modbits = info->level_to_modbits[key->level];
  sym = vk_and_mod_bits_to_gdk_keysym (keymap, info, key->keycode, modbits, 0, NULL);

  if (sym == GDK_KEY_VoidSymbol)
    return 0;
  else
    return sym;
}

static gboolean
gdk_win32_keymap_translate_keyboard_state (GdkKeymap       *gdk_keymap,
                                           guint            hardware_keycode,
                                           GdkModifierType  state,
                                           int              group,
                                           guint           *keyval,
                                           int             *effective_group,
                                           int             *level,
                                           GdkModifierType *consumed_modifiers)
{
  GdkWin32Keymap           *keymap;
  guint                     tmp_keyval;
  int                       tmp_effective_group;
  int                       tmp_level;
  BYTE                      consumed_mod_bits;

  GdkWin32KeymapLayoutInfo *layout_info;
  guint                     vk;
  BYTE                      mod_bits;
  BYTE                      lock_bits = 0;

  g_return_val_if_fail (GDK_IS_KEYMAP (gdk_keymap), FALSE);

  keymap = GDK_WIN32_KEYMAP (gdk_keymap);
  update_keymap (keymap);

  g_return_val_if_fail (group >= 0 && group < keymap->layout_infos->len, FALSE);

  layout_info = &g_array_index (keymap->layout_infos, GdkWin32KeymapLayoutInfo,
                                group);

  vk = hardware_keycode;
  mod_bits = gdk_mod_mask_to_mod_bits (state);

  if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT)
    mod_bits &= ~KBDSHIFT;
  if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL)
    mod_bits &= ~KBDCTRL;
  if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU)
    mod_bits &= ~KBDALT;
  if (vk == VK_RMENU)
    mod_bits &= ~KBDALTGR;

  /* Translate lock state
   *
   * Right now the only locking modifier is CAPSLOCK. We don't handle KANALOK
   * because GDK doesn't have an equivalent modifier mask to my knowledge (On
   * X11, I believe the same effect is achieved by shifting to a different
   * group. It's just a different concept, that doesn't translate to Windows).
   * But since KANALOK is only used on far-eastern keyboards, which require IME
   * anyway, this is probably fine. The IME input module has actually been the
   * default for all languages (not just far-eastern) for a while now, which
   * means that the keymap is now only used for things like accelerators and
   * keybindings, where you probably don't even want KANALOK to affect the
   * translation.
   */

  if (state & GDK_LOCK_MASK)
    lock_bits |= CAPLOK;

  tmp_keyval = vk_and_mod_bits_to_gdk_keysym (keymap, layout_info, vk, mod_bits,
                                              lock_bits, &consumed_mod_bits);
  tmp_effective_group = group;
  tmp_level = modbits_to_level (keymap, layout_info, consumed_mod_bits);

  /* Determine consumed modifiers */
  
  if (keyval)
    *keyval = tmp_keyval;
  if (effective_group)
    *effective_group = tmp_effective_group;
  if (level)
    *level = tmp_level;
  if (consumed_modifiers)
    *consumed_modifiers = mod_bits_to_gdk_mod_mask (consumed_mod_bits);

  /* Just a diagnostic message to inform the user why their keypresses aren't working.
   * Shouldn't happen under normal circumstances. */
  if (tmp_keyval == GDK_KEY_VoidSymbol && layout_info->tables == NULL)
    g_warning("Failed to translate keypress (keycode: %u) for group %d (%s) because "
              "we could not load the layout.",
              hardware_keycode, group, layout_info->name);

  return tmp_keyval != GDK_KEY_VoidSymbol;
}

static void
gdk_win32_keymap_class_init (GdkWin32KeymapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkKeymapClass *keymap_class = GDK_KEYMAP_CLASS (klass);

  object_class->finalize = gdk_win32_keymap_finalize;

  keymap_class->get_direction = gdk_win32_keymap_get_direction;
  keymap_class->have_bidi_layouts = gdk_win32_keymap_have_bidi_layouts;
  keymap_class->get_caps_lock_state = gdk_win32_keymap_get_caps_lock_state;
  keymap_class->get_num_lock_state = gdk_win32_keymap_get_num_lock_state;
  keymap_class->get_scroll_lock_state = gdk_win32_keymap_get_scroll_lock_state;
  keymap_class->get_entries_for_keyval = gdk_win32_keymap_get_entries_for_keyval;
  keymap_class->get_entries_for_keycode = gdk_win32_keymap_get_entries_for_keycode;
  keymap_class->lookup_key = gdk_win32_keymap_lookup_key;
  keymap_class->translate_keyboard_state = gdk_win32_keymap_translate_keyboard_state;
}
