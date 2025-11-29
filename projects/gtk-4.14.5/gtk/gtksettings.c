/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.Free
 */


#include "config.h"

#include "gtksettingsprivate.h"

#include "gtkcssproviderprivate.h"
#include "gtkprivate.h"
#include "gtkscrolledwindow.h"
#include "deprecated/gtkstylecontextprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtktypebuiltins.h"
#include "gtkversion.h"
#include "gtkwidgetprivate.h"

#include "gdk/gdkdisplayprivate.h"

#include <string.h>

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#include <pango/pangofc-fontmap.h>
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#include <pango/pangofc-fontmap.h>
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gdkbroadway.h"
#endif

#ifdef GDK_WINDOWING_MACOS
#include "macos/gdkmacos.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#endif

#ifdef GDK_WINDOWING_MACOS
#define PRINT_PREVIEW_COMMAND "open -b com.apple.Preview %f"
#else
#define PRINT_PREVIEW_COMMAND "evince --unlink-tempfile --preview --print-settings %s %f"
#endif

/**
 * GtkSettings:
 *
 * `GtkSettings` provides a mechanism to share global settings between
 * applications.
 *
 * On the X window system, this sharing is realized by an
 * [XSettings](http://www.freedesktop.org/wiki/Specifications/xsettings-spec)
 * manager that is usually part of the desktop environment, along with
 * utilities that let the user change these settings.
 *
 * On Wayland, the settings are obtained either via a settings portal,
 * or by reading desktop settings from [class@Gio.Settings].
 *
 * On macOS, the settings are obtained from `NSUserDefaults`.
 *
 * In the absence of these sharing mechanisms, GTK reads default values for
 * settings from `settings.ini` files in `/etc/gtk-4.0`, `$XDG_CONFIG_DIRS/gtk-4.0`
 * and `$XDG_CONFIG_HOME/gtk-4.0`. These files must be valid key files (see
 * `GKeyFile`), and have a section called Settings. Themes can also provide
 * default values for settings by installing a `settings.ini` file
 * next to their `gtk.css` file.
 *
 * Applications can override system-wide settings by setting the property
 * of the `GtkSettings` object with g_object_set(). This should be restricted
 * to special cases though; `GtkSettings` are not meant as an application
 * configuration facility.
 *
 * There is one `GtkSettings` instance per display. It can be obtained with
 * [func@Gtk.Settings.get_for_display], but in many cases, it is more
 * convenient to use [method@Gtk.Widget.get_settings].
 */

/* --- typedefs --- */
typedef struct _GtkSettingsValue GtkSettingsValue;

/*< private >
 * GtkSettingsValue:
 * @origin: Origin should be something like “filename:linenumber” for
 *    rc files, or e.g. “XProperty” for other sources.
 * @value: Valid types are LONG, DOUBLE and STRING corresponding to
 *    the token parsed, or a GSTRING holding an unparsed statement
 */
struct _GtkSettingsValue
{
  /* origin should be something like "filename:linenumber" for rc files,
   * or e.g. "XProperty" for other sources
   */
  char *origin;

  /* valid types are LONG, DOUBLE and STRING corresponding to the token parsed,
   * or a GSTRING holding an unparsed statement
   */
  GValue value;

  /* the settings source */
  GtkSettingsSource source;
};

typedef struct _GtkSettingsClass GtkSettingsClass;
typedef struct _GtkSettingsPropertyValue GtkSettingsPropertyValue;

struct _GtkSettings
{
  GObject parent_instance;

  GData *queued_settings;      /* of type GtkSettingsValue* */
  GtkSettingsPropertyValue *property_values;
  GdkDisplay *display;
  GSList *style_cascades;
  GtkCssProvider *theme_provider;
  int font_size;
  gboolean font_size_absolute;
  char *font_family;
  cairo_font_options_t *font_options;
};

struct _GtkSettingsClass
{
  GObjectClass parent_class;
};

struct _GtkSettingsPropertyValue
{
  GValue value;
  GtkSettingsSource source;
};

enum {
  PROP_0,
  PROP_DOUBLE_CLICK_TIME,
  PROP_DOUBLE_CLICK_DISTANCE,
  PROP_CURSOR_BLINK,
  PROP_CURSOR_BLINK_TIME,
  PROP_CURSOR_BLINK_TIMEOUT,
  PROP_SPLIT_CURSOR,
  PROP_CURSOR_ASPECT_RATIO,
  PROP_THEME_NAME,
  PROP_ICON_THEME_NAME,
  PROP_DND_DRAG_THRESHOLD,
  PROP_FONT_NAME,
  PROP_XFT_ANTIALIAS,
  PROP_XFT_HINTING,
  PROP_XFT_HINTSTYLE,
  PROP_XFT_RGBA,
  PROP_XFT_DPI,
  PROP_HINT_FONT_METRICS,
  PROP_CURSOR_THEME_NAME,
  PROP_CURSOR_THEME_SIZE,
  PROP_ALTERNATIVE_BUTTON_ORDER,
  PROP_ALTERNATIVE_SORT_ARROWS,
  PROP_ENABLE_ANIMATIONS,
  PROP_ERROR_BELL,
  PROP_STATUS_SHAPES,
  PROP_PRINT_BACKENDS,
  PROP_PRINT_PREVIEW_COMMAND,
  PROP_ENABLE_ACCELS,
  PROP_IM_MODULE,
  PROP_RECENT_FILES_MAX_AGE,
  PROP_FONTCONFIG_TIMESTAMP,
  PROP_SOUND_THEME_NAME,
  PROP_ENABLE_INPUT_FEEDBACK_SOUNDS,
  PROP_ENABLE_EVENT_SOUNDS,
  PROP_PRIMARY_BUTTON_WARPS_SLIDER,
  PROP_APPLICATION_PREFER_DARK_THEME,
  PROP_ENTRY_SELECT_ON_FOCUS,
  PROP_ENTRY_PASSWORD_HINT_TIMEOUT,
  PROP_LABEL_SELECT_ON_FOCUS,
  PROP_SHELL_SHOWS_APP_MENU,
  PROP_SHELL_SHOWS_MENUBAR,
  PROP_SHELL_SHOWS_DESKTOP,
  PROP_DECORATION_LAYOUT,
  PROP_TITLEBAR_DOUBLE_CLICK,
  PROP_TITLEBAR_MIDDLE_CLICK,
  PROP_TITLEBAR_RIGHT_CLICK,
  PROP_DIALOGS_USE_HEADER,
  PROP_ENABLE_PRIMARY_PASTE,
  PROP_RECENT_FILES_ENABLED,
  PROP_LONG_PRESS_TIME,
  PROP_KEYNAV_USE_CARET,
  PROP_OVERLAY_SCROLLING,

  NUM_PROPERTIES
};

static GParamSpec *pspecs[NUM_PROPERTIES] = { NULL, };

/* --- prototypes --- */
static void     gtk_settings_provider_iface_init (GtkStyleProviderInterface *iface);

static void     gtk_settings_finalize            (GObject               *object);
static void     gtk_settings_get_property        (GObject               *object,
                                                  guint                  property_id,
                                                  GValue                *value,
                                                  GParamSpec            *pspec);
static void     gtk_settings_set_property        (GObject               *object,
                                                  guint                  property_id,
                                                  const GValue          *value,
                                                  GParamSpec            *pspec);
static void     gtk_settings_notify              (GObject               *object,
                                                  GParamSpec            *pspec);
static void    settings_update_double_click      (GtkSettings           *settings);

static void    settings_update_cursor_theme      (GtkSettings           *settings);
static void    settings_update_font_options      (GtkSettings           *settings);
static void    settings_update_font_values       (GtkSettings           *settings);
static gboolean settings_update_fontconfig       (GtkSettings           *settings);
static void    settings_update_theme             (GtkSettings           *settings);
static gboolean settings_update_xsetting         (GtkSettings           *settings,
                                                  GParamSpec            *pspec,
                                                  gboolean               force);
static void    settings_update_xsettings         (GtkSettings           *settings);

static void gtk_settings_load_from_key_file      (GtkSettings           *settings,
                                                  const char            *path,
                                                  GtkSettingsSource      source);
static void settings_update_provider             (GdkDisplay            *display,
                                                  GtkCssProvider       **old,
                                                  GtkCssProvider        *new);

/* --- variables --- */
static GQuark            quark_gtk_settings = 0;

static GPtrArray *display_settings;


G_DEFINE_TYPE_EXTENDED (GtkSettings, gtk_settings, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER,
                                               gtk_settings_provider_iface_init));

/* --- functions --- */
static void
gtk_settings_init (GtkSettings *settings)
{
  guint i = 0;
  char *path;
  const char * const *config_dirs;

  g_datalist_init (&settings->queued_settings);

  settings->style_cascades = g_slist_prepend (NULL, _gtk_style_cascade_new ());
  settings->theme_provider = gtk_css_provider_new ();

  settings->property_values = g_new0 (GtkSettingsPropertyValue, NUM_PROPERTIES - 1);
  g_object_freeze_notify (G_OBJECT (settings));

  for (i = 1; i < NUM_PROPERTIES; i++)
    {
      GParamSpec *pspec = pspecs[i];
      GType value_type = G_PARAM_SPEC_VALUE_TYPE (pspec);

      g_value_init (&settings->property_values[i - 1].value, value_type);
      g_param_value_set_default (pspec, &settings->property_values[i - 1].value);

      g_object_notify_by_pspec (G_OBJECT (settings), pspec);
      settings->property_values[i - 1].source = GTK_SETTINGS_SOURCE_DEFAULT;
    }

  path = g_build_filename (_gtk_get_data_prefix (), "share", "gtk-4.0", "settings.ini", NULL);
  gtk_settings_load_from_key_file (settings, path, GTK_SETTINGS_SOURCE_DEFAULT);
  g_free (path);

  path = g_build_filename (_gtk_get_sysconfdir (), "gtk-4.0", "settings.ini", NULL);
  gtk_settings_load_from_key_file (settings, path, GTK_SETTINGS_SOURCE_DEFAULT);
  g_free (path);

  config_dirs = g_get_system_config_dirs ();
  for (i = 0; config_dirs[i] != NULL; i++)
    {
      path = g_build_filename (config_dirs[i], "gtk-4.0", "settings.ini", NULL);
      gtk_settings_load_from_key_file (settings, path, GTK_SETTINGS_SOURCE_DEFAULT);
      g_free (path);
    }

  path = g_build_filename (g_get_user_config_dir (), "gtk-4.0", "settings.ini", NULL);
  gtk_settings_load_from_key_file (settings, path, GTK_SETTINGS_SOURCE_DEFAULT);
  g_free (path);

  g_object_thaw_notify (G_OBJECT (settings));

  /* ensure that derived fields are initialized */
  if (settings->font_size == 0)
    settings_update_font_values (settings);
}

static void
gtk_settings_class_init (GtkSettingsClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  guint result G_GNUC_UNUSED;

  gobject_class->finalize = gtk_settings_finalize;
  gobject_class->get_property = gtk_settings_get_property;
  gobject_class->set_property = gtk_settings_set_property;
  gobject_class->notify = gtk_settings_notify;

  quark_gtk_settings = g_quark_from_static_string ("gtk-settings");

  /**
   * GtkSettings:gtk-double-click-time:
   *
   * The maximum time to allow between two clicks for them to be considered
   * a double click, in milliseconds.
   */
  pspecs[PROP_DOUBLE_CLICK_TIME] = g_param_spec_int ("gtk-double-click-time", NULL, NULL,
                                                     0, G_MAXINT, 400,
                                                     GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-double-click-distance:
   *
   * The maximum distance allowed between two clicks for them to be considered
   * a double click, in pixels.
   */
  pspecs[PROP_DOUBLE_CLICK_DISTANCE] = g_param_spec_int ("gtk-double-click-distance", NULL, NULL,
                                                         0, G_MAXINT, 5,
                                                         GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-cursor-blink:
   *
   * Whether the cursor should blink.
   *
   * Also see the [property@Gtk.Settings:gtk-cursor-blink-timeout] setting,
   * which allows more flexible control over cursor blinking.
   */
  pspecs[PROP_CURSOR_BLINK] = g_param_spec_boolean ("gtk-cursor-blink", NULL, NULL,
                                                    TRUE,
                                                    GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-cursor-blink-time:
   *
   * Length of the cursor blink cycle, in milliseconds.
   */
  pspecs[PROP_CURSOR_BLINK_TIME] = g_param_spec_int ("gtk-cursor-blink-time", NULL, NULL,
                                                     100, G_MAXINT, 1200,
                                                     GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-cursor-blink-timeout:
   *
   * Time after which the cursor stops blinking, in seconds.
   *
   * The timer is reset after each user interaction.
   *
   * Setting this to zero has the same effect as setting
   * [property@Gtk.Settings:gtk-cursor-blink] to %FALSE.
   */
  pspecs[PROP_CURSOR_BLINK_TIMEOUT] = g_param_spec_int ("gtk-cursor-blink-timeout", NULL, NULL,
                                                        1, G_MAXINT, 10,
                                                        GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-split-cursor:
   *
   * Whether two cursors should be displayed for mixed left-to-right and
   * right-to-left text.
   */
  pspecs[PROP_SPLIT_CURSOR] = g_param_spec_boolean ("gtk-split-cursor", NULL, NULL,
                                                    FALSE,
                                                    GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-cursor-aspect-ratio:
   *
   * The aspect ratio of the text caret.
   */
  pspecs[PROP_CURSOR_ASPECT_RATIO] = g_param_spec_double ("gtk-cursor-aspect-ratio", NULL, NULL,
                                                          0.0, 1.0, 0.04,
                                                          GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-theme-name:
   *
   * Name of the theme to load.
   *
   * See [class@Gtk.CssProvider] for details about how
   * GTK finds the CSS stylesheet for a theme.
   */
  pspecs[PROP_THEME_NAME] = g_param_spec_string ("gtk-theme-name", NULL, NULL,
                                                 DEFAULT_THEME_NAME,
                                                 GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-icon-theme-name:
   *
   * Name of the icon theme to use.
   *
   * See [class@Gtk.IconTheme] for details about how
   * GTK handles icon themes.
   */
  pspecs[PROP_ICON_THEME_NAME] = g_param_spec_string ("gtk-icon-theme-name", NULL, NULL,
                                                      DEFAULT_ICON_THEME,
                                                      GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-dnd-drag-threshold:
   *
   * The number of pixels the cursor can move before dragging.
   */
  pspecs[PROP_DND_DRAG_THRESHOLD] = g_param_spec_int ("gtk-dnd-drag-threshold", NULL, NULL,
                                                      1, G_MAXINT, 8,
                                                      GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-font-name:
   *
   * The default font to use.
   *
   * GTK uses the family name and size from this string.
   */
  pspecs[PROP_FONT_NAME] = g_param_spec_string ("gtk-font-name", NULL, NULL,
                                                "Sans 10",
                                                GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-xft-antialias:
   *
   * Whether to antialias fonts.
   *
   * The values are 0 for no, 1 for yes, or -1 for the system default.
   */
  pspecs[PROP_XFT_ANTIALIAS] = g_param_spec_int ("gtk-xft-antialias", NULL, NULL,
                                                 -1, 1, -1,
                                                 GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-xft-hinting:
   *
   * Whether to enable font hinting.
   *
   * The values are 0 for no, 1 for yes, or -1 for the system default.
   */
  pspecs[PROP_XFT_HINTING] = g_param_spec_int ("gtk-xft-hinting", NULL, NULL,
                                               -1, 1, -1,
                                               GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-xft-hintstyle:
   *
   * What degree of font hinting to use.
   *
   * The possible vaues are hintnone, hintslight,
   * hintmedium, hintfull.
   */
  pspecs[PROP_XFT_HINTSTYLE] = g_param_spec_string ("gtk-xft-hintstyle", NULL, NULL,
                                                    NULL,
                                                    GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-xft-rgba:
   *
   * The type of subpixel antialiasing to use.
   *
   * The possible values are none, rgb, bgr, vrgb, vbgr.
   *
   * Note that GSK does not support subpixel antialiasing, and this
   * setting has no effect on font rendering in GTK.
   */
  pspecs[PROP_XFT_RGBA] = g_param_spec_string ("gtk-xft-rgba", NULL, NULL,
                                               NULL,
                                               GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-xft-dpi:
   *
   * The font resolution, in 1024 * dots/inch.
   *
   * -1 to use the default value.
   */
  pspecs[PROP_XFT_DPI] = g_param_spec_int ("gtk-xft-dpi", NULL, NULL,
                                           -1, 1024*1024, -1,
                                           GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-hint-font-metrics:
   *
   * Whether hinting should be applied to font metrics.
   *
   * Note that this also turns off subpixel positioning of glyphs,
   * since it conflicts with metrics hinting.
   *
   * Since: 4.6
   */
  pspecs[PROP_HINT_FONT_METRICS] = g_param_spec_boolean ("gtk-hint-font-metrics", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-cursor-theme-name:
   *
   * Name of the cursor theme to use.
   *
   * Use %NULL to use the default theme.
   */
  pspecs[PROP_CURSOR_THEME_NAME] = g_param_spec_string ("gtk-cursor-theme-name", NULL, NULL,
                                                        NULL,
                                                        GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-cursor-theme-size:
   *
   * The size to use for cursors.
   *
   * 0 means to use the default size.
   */
  pspecs[PROP_CURSOR_THEME_SIZE] = g_param_spec_int ("gtk-cursor-theme-size", NULL, NULL,
                                                     0, 128, 0,
                                                     GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-alternative-button-order:
   *
   * Whether buttons in dialogs should use the alternative button order.
   */
  pspecs[PROP_ALTERNATIVE_BUTTON_ORDER] = g_param_spec_boolean ("gtk-alternative-button-order", NULL, NULL,
                                                                FALSE,
                                                                GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-alternative-sort-arrows:
   *
   * Controls the direction of the sort indicators in sorted list and tree
   * views.
   *
   * By default an arrow pointing down means the column is sorted
   * in ascending order. When set to %TRUE, this order will be inverted.
   */
  pspecs[PROP_ALTERNATIVE_SORT_ARROWS] = g_param_spec_boolean ("gtk-alternative-sort-arrows", NULL, NULL,
                                                               FALSE,
                                                               GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-enable-animations:
   *
   * Whether to enable toolkit-wide animations.
   */
  pspecs[PROP_ENABLE_ANIMATIONS] = g_param_spec_boolean ("gtk-enable-animations", NULL, NULL,
                                                         TRUE,
                                                         GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-error-bell:
   *
   * When %TRUE, keyboard navigation and other input-related errors
   * will cause a beep.
   *
   * Since the error bell is implemented using gdk_surface_beep(), the
   * windowing system may offer ways to configure the error bell in many
   * ways, such as flashing the window or similar visual effects.
   */
  pspecs[PROP_ERROR_BELL] = g_param_spec_boolean ("gtk-error-bell", NULL, NULL,
                                                  TRUE,
                                                  GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-show-status-shapes:
   *
   * When %TRUE, widgets like switches include shapes to indicate their on/off state.
   *
   * Since: 4.14
   */
  pspecs[PROP_STATUS_SHAPES] = g_param_spec_boolean ("gtk-show-status-shapes", NULL, NULL,
                                                     FALSE,
                                                     GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-print-backends:
   *
   * A comma-separated list of print backends to use in the print
   * dialog.
   *
   * Available print backends depend on the GTK installation,
   * and may include "file", "cups", "lpr" or "papi".
   */
  pspecs[PROP_PRINT_BACKENDS] = g_param_spec_string ("gtk-print-backends", NULL, NULL,
                                                     GTK_PRINT_BACKENDS,
                                                     GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-print-preview-command:
   *
   * A command to run for displaying the print preview.
   *
   * The command should contain a `%f` placeholder, which will get
   * replaced by the path to the pdf file. The command may also
   * contain a `%s` placeholder, which will get replaced by the
   * path to a file containing the print settings in the format
   * produced by [method@Gtk.PrintSettings.to_file].
   *
   * The preview application is responsible for removing the pdf
   * file and the print settings file when it is done.
   */
  pspecs[PROP_PRINT_PREVIEW_COMMAND] = g_param_spec_string ("gtk-print-preview-command", NULL, NULL,
                                                            PRINT_PREVIEW_COMMAND,
                                                            GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-enable-accels:
   *
   * Whether menu items should have visible accelerators which can be
   * activated.
   */
  pspecs[PROP_ENABLE_ACCELS] = g_param_spec_boolean ("gtk-enable-accels", NULL, NULL,
                                                     TRUE,
                                                     GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-im-module:
   *
   * Which IM (input method) module should be used by default.
   *
   * This is the input method that will be used if the user has not
   * explicitly chosen another input method from the IM context menu.
   * This also can be a colon-separated list of input methods, which GTK
   * will try in turn until it finds one available on the system.
   *
   * See [class@Gtk.IMContext].
   */
  pspecs[PROP_IM_MODULE] = g_param_spec_string ("gtk-im-module", NULL, NULL,
                                                NULL,
                                                GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-recent-files-max-age:
   *
   * The maximum age, in days, of the items inside the recently used
   * resources list.
   *
   * Items older than this setting will be excised from the list.
   * If set to 0, the list will always be empty; if set to -1, no
   * item will be removed.
   */
  pspecs[PROP_RECENT_FILES_MAX_AGE] = g_param_spec_int ("gtk-recent-files-max-age", NULL, NULL,
                                                        -1, G_MAXINT,
                                                        30,
                                                        GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-fontconfig-timestamp:
   *
   * Timestamp of the current fontconfig configuration.
   */
  pspecs[PROP_FONTCONFIG_TIMESTAMP] = g_param_spec_uint ("gtk-fontconfig-timestamp", NULL, NULL,
                                                         0, G_MAXUINT, 0,
                                                         GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-sound-theme-name:
   *
   * The XDG sound theme to use for event sounds.
   *
   * See the [Sound Theme Specifications](http://www.freedesktop.org/wiki/Specifications/sound-theme-spec)
   * for more information on event sounds and sound themes.
   *
   * GTK itself does not support event sounds, you have to use
   * a loadable module like the one that comes with libcanberra.
   */
  pspecs[PROP_SOUND_THEME_NAME] = g_param_spec_string ("gtk-sound-theme-name", NULL, NULL,
                                                       "freedesktop",
                                                       GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-enable-input-feedback-sounds:
   *
   * Whether to play event sounds as feedback to user input.
   *
   * See the [Sound Theme Specifications](http://www.freedesktop.org/wiki/Specifications/sound-theme-spec)
   * for more information on event sounds and sound themes.
   *
   * GTK itself does not support event sounds, you have to use a loadable
   * module like the one that comes with libcanberra.
   */
  pspecs[PROP_ENABLE_INPUT_FEEDBACK_SOUNDS] = g_param_spec_boolean ("gtk-enable-input-feedback-sounds", NULL, NULL,
                                                                    TRUE,
                                                                    GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-enable-event-sounds:
   *
   * Whether to play any event sounds at all.
   *
   * See the [Sound Theme Specifications](http://www.freedesktop.org/wiki/Specifications/sound-theme-spec)
   * for more information on event sounds and sound themes.
   *
   * GTK itself does not support event sounds, you have to use a loadable
   * module like the one that comes with libcanberra.
   */
  pspecs[PROP_ENABLE_EVENT_SOUNDS] = g_param_spec_boolean ("gtk-enable-event-sounds", NULL, NULL,
                                                           TRUE,
                                                           GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-primary-button-warps-slider:
   *
   * If the value of this setting is %TRUE, clicking the primary button in a
   * `GtkRange` trough will move the slider, and hence set the range’s value, to
   * the point that you clicked.
   *
   * If it is %FALSE, a primary click will cause the slider/value to move
   * by the range’s page-size towards the point clicked.
   *
   * Whichever action you choose for the primary button, the other action will
   * be available by holding Shift and primary-clicking, or clicking the middle
   * mouse button.
   */
  pspecs[PROP_PRIMARY_BUTTON_WARPS_SLIDER] = g_param_spec_boolean ("gtk-primary-button-warps-slider", NULL, NULL,
                                                                   TRUE,
                                                                   GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-application-prefer-dark-theme:
   *
   * Whether the application prefers to use a dark theme.
   *
   * If a GTK theme includes a dark variant, it will be used
   * instead of the configured theme.
   *
   * Some applications benefit from minimizing the amount of light
   * pollution that interferes with the content. Good candidates for
   * dark themes are photo and video editors that make the actual
   * content get all the attention and minimize the distraction of
   * the chrome.
   *
   * Dark themes should not be used for documents, where large spaces
   * are white/light and the dark chrome creates too much contrast
   * (web browser, text editor...).
   */
  pspecs[PROP_APPLICATION_PREFER_DARK_THEME] = g_param_spec_boolean ("gtk-application-prefer-dark-theme", NULL, NULL,
                                                                     FALSE,
                                                                     GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-entry-select-on-focus:
   *
   * Whether to select the contents of an entry when it is focused.
   */
  pspecs[PROP_ENTRY_SELECT_ON_FOCUS] = g_param_spec_boolean ("gtk-entry-select-on-focus", NULL, NULL,
                                                             TRUE,
                                                             GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-entry-password-hint-timeout:
   *
   * How long to show the last input character in hidden
   * entries.
   *
   * This value is in milliseconds. 0 disables showing the
   * last char. 600 is a good value for enabling it.
   */
  pspecs[PROP_ENTRY_PASSWORD_HINT_TIMEOUT] = g_param_spec_uint ("gtk-entry-password-hint-timeout", NULL, NULL,
                                                                0, G_MAXUINT,
                                                                0,
                                                                GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-label-select-on-focus:
   *
   * Whether to select the contents of a selectable
   * label when it is focused.
   */
  pspecs[PROP_LABEL_SELECT_ON_FOCUS] = g_param_spec_boolean ("gtk-label-select-on-focus", NULL, NULL,
                                                             TRUE,
                                                             GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-shell-shows-app-menu:
   *
   * Set to %TRUE if the desktop environment is displaying
   * the app menu, %FALSE if the app should display it itself.
   */
  pspecs[PROP_SHELL_SHOWS_APP_MENU] = g_param_spec_boolean ("gtk-shell-shows-app-menu", NULL, NULL,
                                                            FALSE,
                                                            GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-shell-shows-menubar:
   *
   * Set to %TRUE if the desktop environment is displaying
   * the menubar, %FALSE if the app should display it itself.
   */
  pspecs[PROP_SHELL_SHOWS_MENUBAR] = g_param_spec_boolean ("gtk-shell-shows-menubar", NULL, NULL,
                                                           FALSE,
                                                           GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-shell-shows-desktop:
   *
   * Set to %TRUE if the desktop environment is displaying
   * the desktop folder, %FALSE if not.
   */
  pspecs[PROP_SHELL_SHOWS_DESKTOP] = g_param_spec_boolean ("gtk-shell-shows-desktop", NULL, NULL,
                                                           TRUE,
                                                           GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-decoration-layout:
   *
   * Determines which buttons should be put in the
   * titlebar of client-side decorated windows, and whether they
   * should be placed on the left or right.
   *
   * The format of the string is button names, separated by commas.
   * A colon separates the buttons that should appear on the left
   * from those on the right. Recognized button names are minimize,
   * maximize, close, icon (the window icon) and menu (a menu button
   * for the fallback app menu).
   *
   * For example, "menu:minimize,maximize,close" specifies a menu
   * on the left, and minimize, maximize and close buttons on the right.
   *
   * Note that buttons will only be shown when they are meaningful.
   * E.g. a menu button only appears when the desktop shell does not
   * show the app menu, and a close button only appears on a window
   * that can be closed.
   *
   * Also note that the setting can be overridden with the
   * [property@Gtk.HeaderBar:decoration-layout] property.
   */
  pspecs[PROP_DECORATION_LAYOUT] = g_param_spec_string ("gtk-decoration-layout", NULL, NULL,
                                                        "menu:minimize,maximize,close",
                                                        GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-titlebar-double-click:
   *
   * Determines the action to take when a double-click
   * occurs on the titlebar of client-side decorated windows.
   *
   * Recognized actions are minimize, toggle-maximize, menu, lower
   * or none.
   */
  pspecs[PROP_TITLEBAR_DOUBLE_CLICK] = g_param_spec_string ("gtk-titlebar-double-click", NULL, NULL,
                                                            "toggle-maximize",
                                                            GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-titlebar-middle-click:
   *
   * Determines the action to take when a middle-click
   * occurs on the titlebar of client-side decorated windows.
   *
   * Recognized actions are minimize, toggle-maximize, menu, lower
   * or none.
   */
  pspecs[PROP_TITLEBAR_MIDDLE_CLICK] = g_param_spec_string ("gtk-titlebar-middle-click", NULL, NULL,
                                                            "none",
                                                            GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-titlebar-right-click:
   *
   * Determines the action to take when a right-click
   * occurs on the titlebar of client-side decorated windows.
   *
   * Recognized actions are minimize, toggle-maximize, menu, lower
   * or none.
   */
  pspecs[PROP_TITLEBAR_RIGHT_CLICK] = g_param_spec_string ("gtk-titlebar-right-click", NULL, NULL,
                                                           "menu",
                                                           GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-dialogs-use-header:
   *
   * Whether builtin GTK dialogs such as the file chooser, the
   * color chooser or the font chooser will use a header bar at
   * the top to show action widgets, or an action area at the bottom.
   *
   * This setting does not affect custom dialogs using `GtkDialog`
   * directly, or message dialogs.
   */
  pspecs[PROP_DIALOGS_USE_HEADER] = g_param_spec_boolean ("gtk-dialogs-use-header", NULL, NULL,
                                                          FALSE,
                                                          GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-enable-primary-paste:
   *
   * Whether a middle click on a mouse should paste the
   * 'PRIMARY' clipboard content at the cursor location.
   */
  pspecs[PROP_ENABLE_PRIMARY_PASTE] = g_param_spec_boolean ("gtk-enable-primary-paste", NULL, NULL,
                                                            TRUE,
                                                            GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-recent-files-enabled:
   *
   * Whether GTK should keep track of items inside the recently used
   * resources list.
   *
   * If set to %FALSE, the list will always be empty.
   */
  pspecs[PROP_RECENT_FILES_ENABLED] = g_param_spec_boolean ("gtk-recent-files-enabled", NULL, NULL,
                                                            TRUE,
                                                            GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-long-press-time:
   *
   * The time for a button or touch press to be considered a “long press”.
   *
   * See [class@Gtk.GestureLongPress].
   */
  pspecs[PROP_LONG_PRESS_TIME] = g_param_spec_uint ("gtk-long-press-time", NULL, NULL,
                                                    0, G_MAXINT, 500,
                                                    GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-keynav-use-caret:
   *
   * Whether GTK should make sure that text can be navigated with
   * a caret, even if it is not editable.
   *
   * This is useful when using a screen reader.
   */
  pspecs[PROP_KEYNAV_USE_CARET] = g_param_spec_boolean ("gtk-keynav-use-caret", NULL, NULL,
                                                        FALSE,
                                                        GTK_PARAM_READWRITE);

  /**
   * GtkSettings:gtk-overlay-scrolling:
   *
   * Whether scrolled windows may use overlaid scrolling indicators.
   *
   * If this is set to %FALSE, scrolled windows will have permanent
   * scrollbars.
   */
  pspecs[PROP_OVERLAY_SCROLLING] = g_param_spec_boolean ("gtk-overlay-scrolling", NULL, NULL,
                                                         TRUE,
                                                         GTK_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, pspecs);
}

static GtkSettings *
gtk_settings_style_provider_get_settings (GtkStyleProvider *provider)
{
  return GTK_SETTINGS (provider);
}

static void
gtk_settings_provider_iface_init (GtkStyleProviderInterface *iface)
{
  iface->get_settings = gtk_settings_style_provider_get_settings;
}

static void
gtk_settings_finalize (GObject *object)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  guint i;

  for (i = 1; i < NUM_PROPERTIES; i++)
    g_value_unset (&settings->property_values[i - 1].value);
  g_free (settings->property_values);

  g_datalist_clear (&settings->queued_settings);

  settings_update_provider (settings->display, &settings->theme_provider, NULL);
  g_slist_free_full (settings->style_cascades, g_object_unref);

  if (settings->font_options)
    cairo_font_options_destroy (settings->font_options);

  g_free (settings->font_family);

  g_object_unref (settings->theme_provider);

  G_OBJECT_CLASS (gtk_settings_parent_class)->finalize (object);
}

GtkStyleCascade *
_gtk_settings_get_style_cascade (GtkSettings *settings,
                                 int          scale)
{
  GtkStyleCascade *new_cascade;
  GSList *list;

  g_return_val_if_fail (GTK_IS_SETTINGS (settings), NULL);

  for (list = settings->style_cascades; list; list = list->next)
    {
      if (_gtk_style_cascade_get_scale (list->data) == scale)
        return list->data;
    }

  /* We are guaranteed to have the special cascade with scale == 1.
   * It's created in gtk_settings_init()
   */
  g_assert (scale != 1);

  new_cascade = _gtk_style_cascade_new ();
  _gtk_style_cascade_set_parent (new_cascade, _gtk_settings_get_style_cascade (settings, 1));
  _gtk_style_cascade_set_scale (new_cascade, scale);

  settings->style_cascades = g_slist_prepend (settings->style_cascades, new_cascade);

  return new_cascade;
}

static void
settings_init_style (GtkSettings *settings)
{
  static GtkCssProvider *css_provider = NULL;
  GtkStyleCascade *cascade;

  /* Add provider for user file */
  if (G_UNLIKELY (!css_provider))
    {
      char *css_path;

      css_provider = gtk_css_provider_new ();
      css_path = g_build_filename (g_get_user_config_dir (),
                                   "gtk-4.0",
                                   "gtk.css",
                                   NULL);

      if (g_file_test (css_path,
                       G_FILE_TEST_IS_REGULAR))
        gtk_css_provider_load_from_path (css_provider, css_path);

      g_free (css_path);
    }

  cascade = _gtk_settings_get_style_cascade (settings, 1);
  _gtk_style_cascade_add_provider (cascade,
                                   GTK_STYLE_PROVIDER (css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_USER);

  _gtk_style_cascade_add_provider (cascade,
                                   GTK_STYLE_PROVIDER (settings),
                                   GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);

  _gtk_style_cascade_add_provider (cascade,
                                   GTK_STYLE_PROVIDER (settings->theme_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);

  settings_update_theme (settings);
}

static void
setting_changed (GdkDisplay       *display,
                 const char       *name,
                 gpointer          data)
{
  GtkSettings *settings = data;
  GParamSpec *pspec;

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), name);

  if (!pspec)
    return;

  if (settings_update_xsetting (settings, pspec, TRUE))
    g_object_notify_by_pspec (G_OBJECT (settings), pspec);
}

static GtkSettings *
gtk_settings_create_for_display (GdkDisplay *display)
{
  GtkSettings *settings;

#ifdef GDK_WINDOWING_MACOS
  if (GDK_IS_MACOS_DISPLAY (display))
    settings = g_object_new (GTK_TYPE_SETTINGS,
                             "gtk-shell-shows-app-menu", TRUE,
                             "gtk-shell-shows-menubar", TRUE,
                             NULL);
  else
#endif
    settings = g_object_new (GTK_TYPE_SETTINGS, NULL);

  settings->display = display;

  g_signal_connect_object (display, "setting-changed", G_CALLBACK (setting_changed), settings, 0);

  g_ptr_array_add (display_settings, settings);

  settings_init_style (settings);
  settings_update_xsettings (settings);
  settings_update_double_click (settings);
  settings_update_cursor_theme (settings);
  settings_update_font_options (settings);
  settings_update_font_values (settings);

  return settings;
}

/**
 * gtk_settings_get_for_display:
 * @display: a `GdkDisplay`
 *
 * Gets the `GtkSettings` object for @display, creating it if necessary.
 *
 * Returns: (transfer none): a `GtkSettings` object
 */
GtkSettings *
gtk_settings_get_for_display (GdkDisplay *display)
{
  int i;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if G_UNLIKELY (display_settings == NULL)
    display_settings = g_ptr_array_new ();

  for (i = 0; i < display_settings->len; i++)
    {
      GtkSettings *settings = g_ptr_array_index (display_settings, i);
      if (settings->display == display)
        return settings;
    }

  return gtk_settings_create_for_display (display);
}

/**
 * gtk_settings_get_default:
 *
 * Gets the `GtkSettings` object for the default display, creating
 * it if necessary.
 *
 * See [func@Gtk.Settings.get_for_display].
 *
 * Returns: (nullable) (transfer none): a `GtkSettings` object. If there is
 *   no default display, then returns %NULL.
 */
GtkSettings*
gtk_settings_get_default (void)
{
  GdkDisplay *display = gdk_display_get_default ();

  if (display)
    return gtk_settings_get_for_display (display);

  g_debug ("%s() returning NULL GtkSettings object. Is a display available?",
           G_STRFUNC);
  return NULL;
}

static void
gtk_settings_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkSettings *settings = GTK_SETTINGS (object);

  g_value_copy (value, &settings->property_values[property_id - 1].value);
  settings->property_values[property_id - 1].source = GTK_SETTINGS_SOURCE_APPLICATION;
}

static void
settings_invalidate_style (GtkSettings *settings)
{
  gtk_style_provider_changed (GTK_STYLE_PROVIDER (settings));
}

static void
settings_update_font_values (GtkSettings *settings)
{
  PangoFontDescription *desc;
  const char *font_name;

  font_name = g_value_get_string (&settings->property_values[PROP_FONT_NAME - 1].value);
  desc = pango_font_description_from_string (font_name);

  if (desc != NULL &&
      (pango_font_description_get_set_fields (desc) & PANGO_FONT_MASK_SIZE) != 0)
    {
      settings->font_size = pango_font_description_get_size (desc);
      settings->font_size_absolute = pango_font_description_get_size_is_absolute (desc);
    }
  else
    {
      settings->font_size = 10 * PANGO_SCALE;
      settings->font_size_absolute = FALSE;
    }

  g_free (settings->font_family);

  if (desc != NULL &&
      (pango_font_description_get_set_fields (desc) & PANGO_FONT_MASK_FAMILY) != 0)
    {
      settings->font_family = g_strdup (pango_font_description_get_family (desc));
    }
  else
    {
      settings->font_family = g_strdup ("Sans");
    }

  if (desc)
    pango_font_description_free (desc);
}

static void
gtk_settings_notify (GObject    *object,
                     GParamSpec *pspec)
{
  GtkSettings *settings = GTK_SETTINGS (object);
  guint property_id = pspec->param_id;

  if (settings->display == NULL) /* initialization */
    return;

  switch (property_id)
    {
    case PROP_DOUBLE_CLICK_TIME:
    case PROP_DOUBLE_CLICK_DISTANCE:
      settings_update_double_click (settings);
      break;
    case PROP_FONT_NAME:
      settings_update_font_values (settings);
      settings_invalidate_style (settings);
      gtk_system_setting_changed (settings->display, GTK_SYSTEM_SETTING_FONT_NAME);
      break;
    case PROP_THEME_NAME:
    case PROP_APPLICATION_PREFER_DARK_THEME:
      settings_update_theme (settings);
      break;
    case PROP_XFT_DPI:
      settings_invalidate_style (settings);
      gtk_system_setting_changed (settings->display, GTK_SYSTEM_SETTING_DPI);
      break;
    case PROP_XFT_ANTIALIAS:
    case PROP_XFT_HINTING:
    case PROP_XFT_HINTSTYLE:
    case PROP_XFT_RGBA:
    case PROP_HINT_FONT_METRICS:
      settings_update_font_options (settings);
      gtk_system_setting_changed (settings->display, GTK_SYSTEM_SETTING_FONT_CONFIG);
      break;
    case PROP_FONTCONFIG_TIMESTAMP:
      if (settings_update_fontconfig (settings))
        gtk_system_setting_changed (settings->display, GTK_SYSTEM_SETTING_FONT_CONFIG);
      break;
    case PROP_ENABLE_ANIMATIONS:
      settings_invalidate_style (settings);
      break;
    case PROP_CURSOR_THEME_NAME:
    case PROP_CURSOR_THEME_SIZE:
      settings_update_cursor_theme (settings);
      break;
    default:
      break;
    }
}

static gboolean
_gtk_settings_parse_convert (const GValue       *src_value,
                             GParamSpec         *pspec,
                             GValue             *dest_value)
{
  gboolean success = FALSE;

  g_return_val_if_fail (G_VALUE_HOLDS (dest_value, G_PARAM_SPEC_VALUE_TYPE (pspec)), FALSE);

  if (G_VALUE_HOLDS (src_value, G_TYPE_GSTRING))
    {
      if (G_VALUE_HOLDS (dest_value, G_TYPE_STRING))
        {
          GString *gstring = g_value_get_boxed (src_value);

          g_value_set_string (dest_value, gstring ? gstring->str : NULL);
          success = !g_param_value_validate (pspec, dest_value);
        }
    }
  else if (g_value_type_transformable (G_VALUE_TYPE (src_value), G_VALUE_TYPE (dest_value)))
    success = g_param_value_convert (pspec, src_value, dest_value, TRUE);

  return success;
}

static void
apply_queued_setting (GtkSettings      *settings,
                      GParamSpec       *pspec,
                      GtkSettingsValue *qvalue)
{
  GValue tmp_value = G_VALUE_INIT;

  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  if (_gtk_settings_parse_convert (&qvalue->value,
                                   pspec, &tmp_value))
    {
      if (settings->property_values[pspec->param_id - 1].source <= qvalue->source)
        {
          g_value_copy (&tmp_value, &settings->property_values[pspec->param_id - 1].value);
          settings->property_values[pspec->param_id - 1].source = qvalue->source;
          g_object_notify_by_pspec (G_OBJECT (settings), pspec);
        }

    }
  else
    {
      char *debug = g_strdup_value_contents (&qvalue->value);

      g_message ("%s: failed to retrieve property '%s' of type '%s' from rc file value \"%s\" of type '%s'",
                 qvalue->origin ? qvalue->origin : "(for origin information, set GTK_DEBUG)",
                 pspec->name,
                 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
                 debug,
                 G_VALUE_TYPE_NAME (&tmp_value));
      g_free (debug);
    }
  g_value_unset (&tmp_value);
}

static void
free_value (gpointer data)
{
  GtkSettingsValue *qvalue = data;

  g_value_unset (&qvalue->value);
  g_free (qvalue->origin);
  g_free (qvalue);
}

static void
gtk_settings_set_property_value_internal (GtkSettings            *settings,
                                          const char             *prop_name,
                                          const GtkSettingsValue *new_value,
                                          GtkSettingsSource       source)
{
  GtkSettingsValue *qvalue;
  GParamSpec *pspec;
  char *name;
  GQuark name_quark;

  if (!G_VALUE_HOLDS_LONG (&new_value->value) &&
      !G_VALUE_HOLDS_DOUBLE (&new_value->value) &&
      !G_VALUE_HOLDS_STRING (&new_value->value) &&
      !G_VALUE_HOLDS (&new_value->value, G_TYPE_GSTRING))
    {
      g_warning (G_STRLOC ": value type invalid (%s)", g_type_name (G_VALUE_TYPE (&new_value->value)));
      return;
    }

  name = g_strdup (prop_name);
  g_strcanon (name, G_CSET_DIGITS "-" G_CSET_a_2_z G_CSET_A_2_Z, '-');
  name_quark = g_quark_from_string (name);
  g_free (name);

  qvalue = g_datalist_id_dup_data (&settings->queued_settings, name_quark, NULL, NULL);
  if (!qvalue)
    {
      qvalue = g_new0 (GtkSettingsValue, 1);
      g_datalist_id_set_data_full (&settings->queued_settings, name_quark, qvalue, free_value);
    }
  else
    {
      g_free (qvalue->origin);
      g_value_unset (&qvalue->value);
    }
  qvalue->origin = g_strdup (new_value->origin);
  g_value_init (&qvalue->value, G_VALUE_TYPE (&new_value->value));
  g_value_copy (&new_value->value, &qvalue->value);
  qvalue->source = source;
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), g_quark_to_string (name_quark));
  if (pspec)
    apply_queued_setting (settings, pspec, qvalue);
}

static void
settings_update_double_click (GtkSettings *settings)
{
  int double_click_time;
  int double_click_distance;

  g_object_get (settings,
                "gtk-double-click-time", &double_click_time,
                "gtk-double-click-distance", &double_click_distance,
                NULL);

  gdk_display_set_double_click_time (settings->display, double_click_time);
  gdk_display_set_double_click_distance (settings->display, double_click_distance);
}

static void
settings_update_cursor_theme (GtkSettings *settings)
{
  char *theme = NULL;
  int size = 0;

  g_object_get (settings,
                "gtk-cursor-theme-name", &theme,
                "gtk-cursor-theme-size", &size,
                NULL);
  if (theme)
    {
      gdk_display_set_cursor_theme (settings->display, theme, size);
      g_free (theme);
    }
}

static void
settings_update_font_options (GtkSettings *settings)
{
  int hinting;
  char *hint_style_str;
  cairo_hint_style_t hint_style;
  int antialias;
  cairo_antialias_t antialias_mode;
  gboolean hint_font_metrics;

  if (settings->font_options)
    cairo_font_options_destroy (settings->font_options);

  g_object_get (settings,
                "gtk-xft-antialias", &antialias,
                "gtk-xft-hinting", &hinting,
                "gtk-xft-hintstyle", &hint_style_str,
                "gtk-hint-font-metrics", &hint_font_metrics,
                NULL);

  settings->font_options = cairo_font_options_create ();

  cairo_font_options_set_hint_metrics (settings->font_options,
                                       hint_font_metrics ? CAIRO_HINT_METRICS_ON
                                                         : CAIRO_HINT_METRICS_OFF);

  hint_style = CAIRO_HINT_STYLE_DEFAULT;
  if (hinting == 0)
    {
      hint_style = CAIRO_HINT_STYLE_NONE;
    }
  else if (hinting == 1)
    {
      if (hint_style_str)
        {
          if (strcmp (hint_style_str, "hintnone") == 0)
            hint_style = CAIRO_HINT_STYLE_NONE;
          else if (strcmp (hint_style_str, "hintslight") == 0)
            hint_style = CAIRO_HINT_STYLE_SLIGHT;
          else if (strcmp (hint_style_str, "hintmedium") == 0)
            hint_style = CAIRO_HINT_STYLE_MEDIUM;
          else if (strcmp (hint_style_str, "hintfull") == 0)
            hint_style = CAIRO_HINT_STYLE_FULL;
        }
    }

  g_free (hint_style_str);

  cairo_font_options_set_hint_style (settings->font_options, hint_style);

  if (antialias == 0)
    antialias_mode = CAIRO_ANTIALIAS_NONE;
  else
    antialias_mode = CAIRO_ANTIALIAS_GRAY;

  cairo_font_options_set_antialias (settings->font_options, antialias_mode);
}

static gboolean
settings_update_fontconfig (GtkSettings *settings)
{
#if defined(GDK_WINDOWING_X11) || defined(GDK_WINDOWING_WAYLAND)
  static guint    last_update_timestamp;
  static gboolean last_update_needed;

  guint timestamp;

  g_object_get (settings,
                "gtk-fontconfig-timestamp", &timestamp,
                NULL);

  /* if timestamp is the same as last_update_timestamp, we already have
   * updated fontconig on this timestamp (another screen requested it perhaps?),
   * just return the cached result.*/

  if (timestamp != last_update_timestamp)
    {
      PangoFontMap *fontmap = pango_cairo_font_map_get_default ();
      gboolean update_needed = FALSE;

      /* bug 547680 */
      if (PANGO_IS_FC_FONT_MAP (fontmap) && !FcConfigUptoDate (NULL))
        {
          pango_fc_font_map_config_changed (PANGO_FC_FONT_MAP (fontmap));
          if (FcInitReinitialize ())
            update_needed = TRUE;
        }

      last_update_timestamp = timestamp;
      last_update_needed = update_needed;
    }

  return last_update_needed;
#else
  return FALSE;
#endif /* GDK_WINDOWING_X11 || GDK_WINDOWING_WAYLAND */
}

static void
settings_update_provider (GdkDisplay      *display,
                          GtkCssProvider **old,
                          GtkCssProvider  *new)
{
  if (display != NULL && *old != new)
    {
      if (*old)
        {
          gtk_style_context_remove_provider_for_display (display,
                                                         GTK_STYLE_PROVIDER (*old));
          g_object_unref (*old);
          *old = NULL;
        }

      if (new)
        {
          gtk_style_context_add_provider_for_display (display,
                                                      GTK_STYLE_PROVIDER (new),
                                                      GTK_STYLE_PROVIDER_PRIORITY_THEME);
          *old = g_object_ref (new);
        }
    }
}

static void
get_theme_name (GtkSettings  *settings,
                char        **theme_name,
                char        **theme_variant)
{
  gboolean prefer_dark;

  *theme_name = NULL;
  *theme_variant = NULL;

  if (g_getenv ("GTK_THEME"))
    *theme_name = g_strdup (g_getenv ("GTK_THEME"));

  if (*theme_name && **theme_name)
    {
      char *p;
      p = strrchr (*theme_name, ':');
      if (p) {
        *p = '\0';
        p++;
        *theme_variant = g_strdup (p);
      }

      return;
    }

  g_free (*theme_name);

  g_object_get (settings,
                "gtk-theme-name", theme_name,
                "gtk-application-prefer-dark-theme", &prefer_dark,
                NULL);

  if (prefer_dark)
    *theme_variant = g_strdup ("dark");

  if (*theme_name && **theme_name)
    return;

  g_free (*theme_name);
  *theme_name = g_strdup (DEFAULT_THEME_NAME);
}

static void
settings_update_theme (GtkSettings *settings)
{
  char *theme_name;
  char *theme_variant;
  const char *theme_dir;
  char *path;

  get_theme_name (settings, &theme_name, &theme_variant);

  gtk_css_provider_load_named (settings->theme_provider,
                               theme_name,
                               theme_variant);

  /* reload per-theme settings */
  theme_dir = _gtk_css_provider_get_theme_dir (settings->theme_provider);
  if (theme_dir)
    {
      path = g_build_filename (theme_dir, "settings.ini", NULL);
      gtk_settings_load_from_key_file (settings, path, GTK_SETTINGS_SOURCE_THEME);
      g_free (path);
    }

  g_free (theme_name);
  g_free (theme_variant);
}

const cairo_font_options_t *
gtk_settings_get_font_options (GtkSettings *settings)
{
  return settings->font_options;
}

GdkDisplay *
_gtk_settings_get_display (GtkSettings *settings)
{
  return settings->display;
}

static void
gvalue_free (gpointer data)
{
  g_value_unset (data);
  g_free (data);
}

static void
gtk_settings_load_from_key_file (GtkSettings       *settings,
                                 const char        *path,
                                 GtkSettingsSource  source)
{
  GError *error;
  GKeyFile *keyfile;
  char **keys;
  gsize n_keys;
  int i;
  char *contents;
  gsize contents_len;

  if (!g_file_get_contents (path, &contents, &contents_len, NULL))
    return;

  error = NULL;
  keys = NULL;

  keyfile = g_key_file_new ();

  if (!g_key_file_load_from_data (keyfile, contents, contents_len, G_KEY_FILE_NONE, &error))
    {
      g_warning ("Failed to parse %s: %s", path, error->message);

      g_error_free (error);

      goto out;
    }

  keys = g_key_file_get_keys (keyfile, "Settings", &n_keys, &error);
  if (error)
    {
      g_warning ("Failed to parse %s: %s", path, error->message);
      g_error_free (error);
      goto out;
    }

  for (i = 0; i < n_keys; i++)
    {
      char *key;
      GParamSpec *pspec;
      GType value_type;
      GtkSettingsValue svalue = { NULL, G_VALUE_INIT, GTK_SETTINGS_SOURCE_DEFAULT };

      key = keys[i];
      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), key);
      if (!pspec)
        {
          g_warning ("Unknown key %s in %s", key, path);
          continue;
        }

      if (pspec->owner_type != G_OBJECT_TYPE (settings))
        continue;

      value_type = G_PARAM_SPEC_VALUE_TYPE (pspec);
      switch (value_type)
        {
        case G_TYPE_BOOLEAN:
          {
            gboolean b_val;

            g_value_init (&svalue.value, G_TYPE_LONG);
            b_val = g_key_file_get_boolean (keyfile, "Settings", key, &error);
            if (!error)
              g_value_set_long (&svalue.value, b_val);
            break;
          }

        case G_TYPE_INT:
        case G_TYPE_UINT:
          {
            int i_val;

            g_value_init (&svalue.value, G_TYPE_LONG);
            i_val = g_key_file_get_integer (keyfile, "Settings", key, &error);
            if (!error)
              g_value_set_long (&svalue.value, i_val);
            break;
          }

        case G_TYPE_DOUBLE:
          {
            double d_val;

            g_value_init (&svalue.value, G_TYPE_DOUBLE);
            d_val = g_key_file_get_double (keyfile, "Settings", key, &error);
            if (!error)
              g_value_set_double (&svalue.value, d_val);
            break;
          }

        default:
          {
            char *s_val;

            g_value_init (&svalue.value, G_TYPE_GSTRING);
            s_val = g_key_file_get_string (keyfile, "Settings", key, &error);
            if (!error)
              g_value_take_boxed (&svalue.value, g_string_new (s_val));
            g_free (s_val);
            break;
          }
        }
      if (error)
        {
          g_warning ("Error setting %s in %s: %s", key, path, error->message);
          g_error_free (error);
          error = NULL;
        }
      else
        {
          GValue *copy;

          copy = g_new0 (GValue, 1);

          g_value_init (copy, G_VALUE_TYPE (&svalue.value));
          g_value_copy (&svalue.value, copy);

          g_param_spec_set_qdata_full (pspec, g_quark_from_string (key),
                                       copy, gvalue_free);

          if (g_getenv ("GTK_DEBUG"))
            svalue.origin = (char *)path;

          gtk_settings_set_property_value_internal (settings, key, &svalue, source);
          g_value_unset (&svalue.value);
        }
    }

 out:
  g_free (contents);
  g_strfreev (keys);
  g_key_file_free (keyfile);
}

static gboolean
settings_update_xsetting (GtkSettings *settings,
                          GParamSpec  *pspec,
                          gboolean     force)
{
  GType value_type;
  GType fundamental_type;
  gboolean retval = FALSE;

  if (settings->property_values[pspec->param_id - 1].source == GTK_SETTINGS_SOURCE_APPLICATION)
    return FALSE;

  if (settings->property_values[pspec->param_id - 1].source == GTK_SETTINGS_SOURCE_XSETTING && !force)
    return FALSE;

  value_type = G_PARAM_SPEC_VALUE_TYPE (pspec);
  fundamental_type = G_TYPE_FUNDAMENTAL (value_type);

  if ((g_value_type_transformable (G_TYPE_INT, value_type) &&
       !(fundamental_type == G_TYPE_ENUM || fundamental_type == G_TYPE_FLAGS)) ||
      g_value_type_transformable (G_TYPE_STRING, value_type) ||
      g_value_type_transformable (GDK_TYPE_RGBA, value_type))
    {
      GValue val = G_VALUE_INIT;

      g_value_init (&val, value_type);

      if (!gdk_display_get_setting (settings->display, pspec->name, &val))
        return FALSE;

      g_param_value_validate (pspec, &val);
      g_value_copy (&val, &settings->property_values[pspec->param_id - 1].value);
      settings->property_values[pspec->param_id - 1].source = GTK_SETTINGS_SOURCE_XSETTING;

      g_value_unset (&val);

      retval = TRUE;
    }
  else
    {
      return FALSE;
    }

  return retval;
}

static void
settings_update_xsettings (GtkSettings *settings)
{
  int i;

  for (i = 0; pspecs[i]; i++)
    settings_update_xsetting (settings, pspecs[i], FALSE);
}

static void
gtk_settings_get_property (GObject     *object,
                           guint        property_id,
                           GValue      *value,
                           GParamSpec  *pspec)
{
  GtkSettings *settings = GTK_SETTINGS (object);

  settings_update_xsetting (settings, pspec, FALSE);

  g_value_copy (&settings->property_values[property_id - 1].value, value);
}

GtkSettingsSource
_gtk_settings_get_setting_source (GtkSettings *settings,
                                  const char *name)
{
  GParamSpec *pspec;

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), name);
  if (!pspec)
    return GTK_SETTINGS_SOURCE_DEFAULT;

  return settings->property_values[pspec->param_id - 1].source;
}

/**
 * gtk_settings_reset_property:
 * @settings: a `GtkSettings` object
 * @name: the name of the setting to reset
 *
 * Undoes the effect of calling g_object_set() to install an
 * application-specific value for a setting.
 *
 * After this call, the setting will again follow the session-wide
 * value for this setting.
 */
void
gtk_settings_reset_property (GtkSettings *settings,
                             const char *name)
{
  GParamSpec *pspec;
  GValue *value;
  GValue tmp_value = G_VALUE_INIT;

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), name);

  g_return_if_fail (pspec != NULL);

  value = g_param_spec_get_qdata (pspec, g_quark_from_string (name));

  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  if (value && _gtk_settings_parse_convert (value, pspec, &tmp_value))
    g_value_copy (&tmp_value, &settings->property_values[pspec->param_id - 1].value);
  else
    g_param_value_set_default (pspec, &settings->property_values[pspec->param_id - 1].value);

  settings->property_values[pspec->param_id - 1].source = GTK_SETTINGS_SOURCE_DEFAULT;
  g_object_notify_by_pspec (G_OBJECT (settings), pspec);
}

gboolean
gtk_settings_get_enable_animations (GtkSettings *settings)
{
  GtkSettingsPropertyValue *svalue = &settings->property_values[PROP_ENABLE_ANIMATIONS - 1];

  if (svalue->source < GTK_SETTINGS_SOURCE_XSETTING)
    {
      GParamSpec *pspec;

      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), "gtk-enable-animations");
      if (settings_update_xsetting (settings, pspec, FALSE))
        g_object_notify_by_pspec (G_OBJECT (settings), pspec);
    }

  return g_value_get_boolean (&svalue->value);
}

int
gtk_settings_get_dnd_drag_threshold (GtkSettings *settings)
{
  GtkSettingsPropertyValue *svalue = &settings->property_values[PROP_DND_DRAG_THRESHOLD - 1];

  if (svalue->source < GTK_SETTINGS_SOURCE_XSETTING)
    {
      GParamSpec *pspec;

      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), "gtk-dnd-drag-threshold");
      if (settings_update_xsetting (settings, pspec, FALSE))
        g_object_notify_by_pspec (G_OBJECT (settings), pspec);
    }

  return g_value_get_int (&svalue->value);
}

static void
settings_update_font_name (GtkSettings *settings)
{
  GtkSettingsPropertyValue *svalue = &settings->property_values[PROP_FONT_NAME - 1];

  if (svalue->source < GTK_SETTINGS_SOURCE_XSETTING)
    {
      GParamSpec *pspec;

      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), "gtk-font-name");
      if (settings_update_xsetting (settings, pspec, FALSE))
        g_object_notify_by_pspec (G_OBJECT (settings), pspec);
    }
}

const char *
gtk_settings_get_font_family (GtkSettings *settings)
{
  settings_update_font_name (settings);

  return settings->font_family;
}

int
gtk_settings_get_font_size (GtkSettings *settings)
{
  settings_update_font_name (settings);

  return settings->font_size;
}

gboolean
gtk_settings_get_font_size_is_absolute (GtkSettings *settings)
{
  settings_update_font_name (settings);

  return settings->font_size_absolute;
}
