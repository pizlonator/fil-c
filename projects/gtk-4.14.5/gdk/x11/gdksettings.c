/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

static const struct {
  const char *xname, *gdkname;
} gdk_settings_map[] = {
  {"Net/DoubleClickTime",     "gtk-double-click-time"},
  {"Net/DoubleClickDistance", "gtk-double-click-distance"},
  {"Net/DndDragThreshold",    "gtk-dnd-drag-threshold"},
  {"Net/CursorBlink",         "gtk-cursor-blink"},
  {"Net/CursorBlinkTime",     "gtk-cursor-blink-time"},
  {"Net/ThemeName",           "gtk-theme-name"},
  {"Net/IconThemeName",       "gtk-icon-theme-name"},
  {"Gtk/ColorPalette",        "gtk-color-palette"},
  {"Gtk/FontName",            "gtk-font-name"},
  {"Gtk/KeyThemeName",        "gtk-key-theme-name"},
  {"Gtk/CursorThemeName",     "gtk-cursor-theme-name"},
  {"Gtk/CursorThemeSize",     "gtk-cursor-theme-size"},
  {"Gtk/ColorScheme",         "gtk-color-scheme"},
  {"Gtk/EnableAnimations",    "gtk-enable-animations"},
  {"Gtk/ShowStatusStates",    "gtk-show-status-shapes"},
  {"Xft/Antialias",           "gtk-xft-antialias"},
  {"Xft/Hinting",             "gtk-xft-hinting"},
  {"Xft/HintStyle",           "gtk-xft-hintstyle"},
  {"Xft/RGBA",                "gtk-xft-rgba"},
  {"Xft/DPI",                 "gtk-xft-dpi"},
  {"Gtk/EnableAccels",        "gtk-enable-accels"},
  {"Gtk/ScrolledWindowPlacement", "gtk-scrolled-window-placement"},
  {"Gtk/IMModule",            "gtk-im-module"},
  {"Fontconfig/Timestamp",    "gtk-fontconfig-timestamp"},
  {"Net/SoundThemeName",      "gtk-sound-theme-name"},
  {"Net/EnableInputFeedbackSounds", "gtk-enable-input-feedback-sounds"},
  {"Net/EnableEventSounds",   "gtk-enable-event-sounds"},
  {"Gtk/CursorBlinkTimeout",  "gtk-cursor-blink-timeout"},
  {"Gtk/ShellShowsAppMenu",   "gtk-shell-shows-app-menu"},
  {"Gtk/ShellShowsMenubar",   "gtk-shell-shows-menubar"},
  {"Gtk/ShellShowsDesktop",   "gtk-shell-shows-desktop"},
  {"Gtk/SessionBusId",        "gtk-session-bus-id"},
  {"Gtk/DecorationLayout",    "gtk-decoration-layout"},
  {"Gtk/TitlebarDoubleClick", "gtk-titlebar-double-click"},
  {"Gtk/TitlebarMiddleClick", "gtk-titlebar-middle-click"},
  {"Gtk/TitlebarRightClick", "gtk-titlebar-right-click"},
  {"Gtk/DialogsUseHeader",    "gtk-dialogs-use-header"},
  {"Gtk/EnablePrimaryPaste",  "gtk-enable-primary-paste"},
  {"Gtk/PrimaryButtonWarpsSlider", "gtk-primary-button-warps-slider"},
  {"Gtk/RecentFilesMaxAge",   "gtk-recent-files-max-age"},
  {"Gtk/RecentFilesEnabled",  "gtk-recent-files-enabled"},
  {"Gtk/KeynavUseCaret",      "gtk-keynav-use-caret"},
  {"Gtk/OverlayScrolling",    "gtk-overlay-scrolling"},

  /* These are here in order to be recognized, but are not sent to
     gtk as they are handled internally by gdk: */
  {"Gdk/WindowScalingFactor", "gdk-window-scaling-factor"},
  {"Gdk/UnscaledDPI",         "gdk-unscaled-dpi"}
};

static const char *
gdk_from_xsettings_name (const char *xname)
{
  static GHashTable *hash = NULL;
  guint i;

  if (G_UNLIKELY (hash == NULL))
  {
    hash = g_hash_table_new (g_str_hash, g_str_equal);

    for (i = 0; i < G_N_ELEMENTS (gdk_settings_map); i++)
      g_hash_table_insert (hash, (gpointer)gdk_settings_map[i].xname,
                                 (gpointer)gdk_settings_map[i].gdkname);
  }

  return g_hash_table_lookup (hash, xname);
}

