/*
 * Copyright © 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright © 1998-2002 Tor Lillqvist
 * Copyright © 2005-2008 Imendio AB
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include "config.h"

#include <AppKit/AppKit.h>

#include "gdkdisplayprivate.h"

#include "gdkmacosdisplay-private.h"
#include "gdkmacosutils-private.h"

typedef struct
{
  const char *font_name;
  int         xft_dpi;
  int         double_click_time;
  int         cursor_blink_time;
  guint       enable_animations : 1;
  guint       shell_shows_desktop : 1;
  guint       shell_shows_menubar : 1;
  guint       primary_button_warps_slider : 1;
} GdkMacosSettings;

static GdkMacosSettings current_settings;
static gboolean current_settings_initialized;

static void
_gdk_macos_settings_load (GdkMacosSettings *settings)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
  NSString *name;
  NSInteger ival;
  float fval;
  char *str;
  int pt_size;

  g_assert (settings != NULL);

  settings->shell_shows_desktop = TRUE;
  settings->shell_shows_menubar = TRUE;
  settings->enable_animations = TRUE;
  settings->xft_dpi = 72 * 1024;

  ival = [defaults integerForKey:@"NSTextInsertionPointBlinkPeriod"];
  if (ival > 0)
    settings->cursor_blink_time = ival;
  else
    settings->cursor_blink_time = 1000;

  settings->primary_button_warps_slider =
      [[NSUserDefaults standardUserDefaults] boolForKey:@"AppleScrollerPagingBehavior"] == YES;

  fval = [defaults floatForKey:@"com.apple.mouse.doubleClickThreshold"];
  if (fval == 0.0)
    fval = 0.5;
  settings->double_click_time = fval * 1000;

  name = [[NSFont systemFontOfSize:0] familyName];
  pt_size = (int)[[NSFont userFontOfSize:0] pointSize];
  /* Let's try to use the "views" font size (12pt) by default. This is
   * used for lists/text/other "content" which is the largest parts of
   * apps, using the "regular control" size (13pt) looks a bit out of
   * place. We might have to tweak this.
   *
   * The size has to be hardcoded as there doesn't seem to be a way to
   * get the views font size programmatically.
   */
  str = g_strdup_printf ("%s %d", [name UTF8String], pt_size);
  settings->font_name = g_intern_string (str);
  g_free (str);

  GDK_END_MACOS_ALLOC_POOL;
}

gboolean
_gdk_macos_display_get_setting (GdkMacosDisplay *self,
                                const char      *setting,
                                GValue          *value)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  gboolean ret = FALSE;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (self), FALSE);
  g_return_val_if_fail (setting != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  if (!current_settings_initialized)
    {
      _gdk_macos_settings_load (&current_settings);
      current_settings_initialized = TRUE;
    }

  if (FALSE) {}
  else if (strcmp (setting, "gtk-enable-animations") == 0)
    {
      g_value_set_boolean (value, current_settings.enable_animations);
      ret = TRUE;
    }
  else if (strcmp (setting, "gtk-xft-dpi") == 0)
    {
      g_value_set_int (value, current_settings.xft_dpi);
      ret = TRUE;
    }
  else if (strcmp (setting, "gtk-cursor-blink-time") == 0)
    {
      g_value_set_int (value, current_settings.cursor_blink_time);
      ret = TRUE;
    }
  else if (strcmp (setting, "gtk-double-click-time") == 0)
    {
      g_value_set_int (value, current_settings.double_click_time);
      ret = TRUE;
    }
  else if (strcmp (setting, "gtk-font-name") == 0)
    {
      g_value_set_static_string (value, current_settings.font_name);
      ret = TRUE;
    }
  else if (strcmp (setting, "gtk-primary-button-warps-slider") == 0)
    {
      g_value_set_boolean (value, current_settings.primary_button_warps_slider);
      ret = TRUE;
    }
  else if (strcmp (setting, "gtk-shell-shows-desktop") == 0)
    {
      g_value_set_boolean (value, current_settings.shell_shows_desktop);
      ret = TRUE;
    }
  else if (strcmp (setting, "gtk-shell-shows-menubar") == 0)
    {
      g_value_set_boolean (value, current_settings.shell_shows_menubar);
      ret = TRUE;
    }

  GDK_END_MACOS_ALLOC_POOL;

  return ret;
}

void
_gdk_macos_display_reload_settings (GdkMacosDisplay *self)
{
  GdkMacosSettings old_settings;

  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));

  old_settings = current_settings;
  _gdk_macos_settings_load (&current_settings);
  current_settings_initialized = TRUE;

  if (old_settings.xft_dpi != current_settings.xft_dpi)
    gdk_display_setting_changed (GDK_DISPLAY (self), "gtk-xft-dpi");

  if (old_settings.double_click_time != current_settings.double_click_time)
    gdk_display_setting_changed (GDK_DISPLAY (self), "gtk-double-click-time");

  if (old_settings.enable_animations != current_settings.enable_animations)
    gdk_display_setting_changed (GDK_DISPLAY (self), "gtk-enable-animations");

  if (old_settings.font_name != current_settings.font_name)
    gdk_display_setting_changed (GDK_DISPLAY (self), "gtk-font-name");

  if (old_settings.primary_button_warps_slider != current_settings.primary_button_warps_slider)
    gdk_display_setting_changed (GDK_DISPLAY (self), "gtk-primary-button-warps-slider");

  if (old_settings.shell_shows_menubar != current_settings.shell_shows_menubar)
    gdk_display_setting_changed (GDK_DISPLAY (self), "gtk-shell-shows-menubar");

  if (old_settings.shell_shows_desktop != current_settings.shell_shows_desktop)
    gdk_display_setting_changed (GDK_DISPLAY (self), "gtk-shell-shows-desktop");
}
