/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2007, 2008, 2010, 2011 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "terminal-pcre2.hh"
#include "terminal-regex.hh"
#include "terminal-screen.hh"
#include "terminal-client-utils.hh"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <uuid.h>

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
#include <sys/sysctl.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "terminal-accels.hh"
#include "terminal-app.hh"
#include "terminal-debug.hh"
#include "terminal-defines.hh"
#include "terminal-enums.hh"
#include "terminal-intl.hh"
#include "terminal-marshal.h"
#include "terminal-schemas.hh"
#include "terminal-screen-container.hh"
#include "terminal-util.hh"
#include "terminal-window.hh"
#include "terminal-info-bar.hh"
#include "terminal-libgsystem.hh"

#include "eggshell.hh"

#define URL_MATCH_CURSOR_NAME "pointer"

namespace {

typedef struct {
  volatile int refcount;
  char **argv; /* as passed */
  char **exec_argv; /* as processed */
  char **envv;
  char *cwd;
  gboolean as_shell;

  VtePtyFlags pty_flags;
  GSpawnFlags spawn_flags;

  /* FD passing */
  GUnixFDList *fd_list;
  int n_fd_map;
  int* fd_map;

  /* async exec callback */
  TerminalScreenExecCallback callback;
  gpointer callback_data;
  GDestroyNotify callback_data_destroy_notify;

  /* Cancellable */
  GCancellable *cancellable;
} ExecData;

} // anon namespace

typedef struct
{
  int tag;
  TerminalURLFlavor flavor;
} TagData;

struct _TerminalScreenPrivate
{
  char *uuid;
  gboolean registered; /* D-Bus interface is registered */

  GSettings *profile; /* never nullptr */
  guint profile_changed_id;
  guint profile_forgotten_id;
  int child_pid;
  GSList *match_tags;
  gboolean exec_on_realize;
  guint idle_exec_source;
  ExecData *exec_data;

  GIcon* icon;
};

enum
{
  PROFILE_SET,
  SHOW_POPUP_MENU,
  MATCH_CLICKED,
  CLOSE_SCREEN,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_PROFILE,
  PROP_ICON,
  PROP_TITLE,
};

enum
{
  TARGET_COLOR,
  TARGET_BGIMAGE,
  TARGET_RESET_BG,
  TARGET_MOZ_URL,
  TARGET_NETSCAPE_URL,
  TARGET_TAB
};

static void terminal_screen_constructed (GObject             *object);
static void terminal_screen_dispose     (GObject             *object);
static void terminal_screen_finalize    (GObject             *object);
static void terminal_screen_drag_data_received (GtkWidget        *widget,
                                                GdkDragContext   *context,
                                                gint              x,
                                                gint              y,
                                                GtkSelectionData *selection_data,
                                                guint             info,
                                                guint             time);
static void terminal_screen_set_font (TerminalScreen *screen);
static void terminal_screen_system_font_changed_cb (GSettings *,
                                                    const char*,
                                                    TerminalScreen *screen);
static gboolean terminal_screen_popup_menu (GtkWidget *widget);
static gboolean terminal_screen_button_press (GtkWidget *widget,
                                              GdkEventButton *event);
static void terminal_screen_child_exited  (VteTerminal *terminal,
                                           int status);

static void terminal_screen_window_title_changed      (VteTerminal *vte_terminal,
                                                       TerminalScreen *screen);

static void update_color_scheme                      (TerminalScreen *screen);

static char* terminal_screen_check_hyperlink   (TerminalScreen            *screen,
                                                GdkEvent                  *event);
static void terminal_screen_check_extra (TerminalScreen *screen,
                                         GdkEvent       *event,
                                         char           **number_info,
                                         char           **timestamp_info);
static char* terminal_screen_check_match       (TerminalScreen            *screen,
                                                GdkEvent                  *event,
                                                int                  *flavor);

static void terminal_screen_show_info_bar (TerminalScreen *screen,
                                           GError *error,
                                           gboolean show_relaunch);


static char**terminal_screen_get_child_environment (TerminalScreen *screen,
                                                    char **initial_envv,
                                                    char **path,
                                                    char **shell);

static gboolean terminal_screen_get_child_command (TerminalScreen *screen,
                                                   char          **exec_argv,
                                                   const char     *path_env,
                                                   const char     *shell_env,
                                                   gboolean        shell,
                                                   gboolean       *preserve_cwd_p,
                                                   GSpawnFlags    *spawn_flags_p,
                                                   char         ***argv_p,
                                                   GError        **err);

static void terminal_screen_queue_idle_exec (TerminalScreen *screen);

static guint signals[LAST_SIGNAL];

typedef struct {
  const char *pattern;
  TerminalURLFlavor flavor;
} TerminalRegexPattern;

static const TerminalRegexPattern url_regex_patterns[] = {
  { REGEX_URL_AS_IS, FLAVOR_AS_IS },
  { REGEX_URL_HTTP,  FLAVOR_DEFAULT_TO_HTTP },
  { REGEX_URL_FILE,  FLAVOR_AS_IS },
  { REGEX_URL_VOIP,  FLAVOR_VOIP_CALL },
  { REGEX_EMAIL,     FLAVOR_EMAIL },
  { REGEX_NEWS_MAN,  FLAVOR_AS_IS },
};

static const TerminalRegexPattern extra_regex_patterns[] = {
  { "(0[Xx][[:xdigit:]]+|[[:digit:]]+)", FLAVOR_NUMBER },
};

static VteRegex **url_regexes;
static VteRegex **extra_regexes;
static TerminalURLFlavor *url_regex_flavors;
static TerminalURLFlavor *extra_regex_flavors;
static guint n_url_regexes;
static guint n_extra_regexes;

/* See bug #697024 */
#ifndef __linux__

#undef dup3
#define dup3 fake_dup3

static int
fake_dup3 (int fd, int fd2, int flags)
{
  if (dup2 (fd, fd2) == -1)
    return -1;

  return fcntl (fd2, F_SETFD, flags);
}
#endif /* !__linux__ */

static char*
strv_to_string (char **strv)
{
  return strv ? g_strjoinv (" ", strv) : g_strdup ("(null)");
}

static char*
exec_data_to_string (ExecData *data)
{
  gs_free char *str1 = nullptr;
  gs_free char *str2 = nullptr;
  return data ? g_strdup_printf ("data %p argv:[%s] exec-argv:[%s] envv:%p(%u) as-shell:%s cwd:%s",
                                 data,
                                 (str1 = strv_to_string (data->argv)),
                                 (str2 = strv_to_string (data->exec_argv)),
                                 data->envv, data->envv ? g_strv_length (data->envv) : 0,
                                 data->as_shell ? "true" : "false",
                                 data->cwd)
  : g_strdup ("(null)");
}

static ExecData*
exec_data_new (void)
{
  ExecData *data = g_new0 (ExecData, 1);
  data->refcount = 1;

  return data;
}

static ExecData *
exec_data_clone (ExecData *data,
                 gboolean preserve_argv)
{
  if (data == nullptr)
    return nullptr;

  ExecData *clone = exec_data_new ();
  clone->envv = g_strdupv (data->envv);
  clone->cwd = g_strdup (data->cwd);

  /* If FDs were passed, cannot repeat argv. Return data only for env and cwd */
  if (!preserve_argv ||
      data->fd_list != nullptr) {
    clone->as_shell = TRUE;
    return clone;
  }

  clone->argv = g_strdupv (data->argv);
  clone->as_shell = data->as_shell;

  return clone;
}

static void
exec_data_callback (ExecData *data,
                    GError *error,
                    TerminalScreen *screen)
{
  if (data->callback)
    data->callback (screen, error, data->callback_data);
}

static ExecData*
exec_data_ref (ExecData *data)
{
  data->refcount++;
  return data;
}

static void
exec_data_unref (ExecData *data)
{
  if (data == nullptr)
    return;

  if (--data->refcount > 0)
    return;

  g_strfreev (data->argv);
  g_strfreev (data->exec_argv);
  g_strfreev (data->envv);
  g_free (data->cwd);
  g_clear_object (&data->fd_list);
  g_free (data->fd_map);

  if (data->callback_data_destroy_notify && data->callback_data)
    data->callback_data_destroy_notify (data->callback_data);

  g_clear_object (&data->cancellable);

  g_free (data);
}

GS_DEFINE_CLEANUP_FUNCTION0(ExecData*, _terminal_local_unref_exec_data, exec_data_unref)
#define terminal_unref_exec_data __attribute__((__cleanup__(_terminal_local_unref_exec_data)))

static void
terminal_screen_clear_exec_data (TerminalScreen *screen,
                                 gboolean cancelled)
{
  TerminalScreenPrivate *priv = screen->priv;

  if (priv->exec_data == nullptr)
    return;

  if (cancelled) {
    gs_free_error GError *err = nullptr;
    g_set_error_literal (&err, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                         "Spawning was cancelled");
    exec_data_callback (priv->exec_data, err, screen);
  }

  exec_data_unref (priv->exec_data);
  priv->exec_data = nullptr;
}

G_DEFINE_TYPE (TerminalScreen, terminal_screen, VTE_TYPE_TERMINAL)

static void
free_tag_data (TagData *tagdata)
{
  g_slice_free (TagData, tagdata);
}

static void
precompile_regexes (const TerminalRegexPattern *regex_patterns,
                    guint n_regexes,
                    VteRegex ***regexes,
                    TerminalURLFlavor **regex_flavors)
{
  guint i;

  *regexes = g_new0 (VteRegex*, n_regexes);
  *regex_flavors = g_new0 (TerminalURLFlavor, n_regexes);

  for (i = 0; i < n_regexes; ++i)
    {
      GError *error = nullptr;

      (*regexes)[i] = vte_regex_new_for_match (regex_patterns[i].pattern, -1,
                                               PCRE2_UTF | PCRE2_NO_UTF_CHECK | PCRE2_UCP | PCRE2_MULTILINE,
                                               &error);
      g_assert_no_error (error);

      if (!vte_regex_jit ((*regexes)[i], PCRE2_JIT_COMPLETE, &error) ||
          !vte_regex_jit ((*regexes)[i], PCRE2_JIT_PARTIAL_SOFT, &error)) {
        g_printerr ("Failed to JIT regex '%s': %s\n", regex_patterns[i].pattern, error->message);
        g_clear_error (&error);
      }

      (*regex_flavors)[i] = regex_patterns[i].flavor;
    }
}

static void
terminal_screen_class_enable_menu_bar_accel_notify_cb (GSettings *settings,
                                                       const char *key,
                                                       TerminalScreenClass *klass)
{
  static gboolean is_enabled = TRUE; /* the binding is enabled by default since GtkWidgetClass installs it */
  gboolean enable;
  GtkBindingSet *binding_set;

  enable = g_settings_get_boolean (settings, key);

  /* Only remove the 'skip' entry when we have added it previously! */
  if (enable == is_enabled)
    return;

  is_enabled = enable;

  binding_set = gtk_binding_set_by_class (klass);
  if (enable)
    gtk_binding_entry_remove (binding_set, GDK_KEY_F10, GDK_SHIFT_MASK);
  else
    gtk_binding_entry_skip (binding_set, GDK_KEY_F10, GDK_SHIFT_MASK);
}

static TerminalWindow *
terminal_screen_get_window (TerminalScreen *screen)
{
  GtkWidget *widget = GTK_WIDGET (screen);
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (widget);
  if (!gtk_widget_is_toplevel (toplevel))
    return nullptr;

  return TERMINAL_WINDOW (toplevel);
}

static void
terminal_screen_realize (GtkWidget *widget)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);

  GTK_WIDGET_CLASS (terminal_screen_parent_class)->realize (widget);

  terminal_screen_set_font (screen);

  TerminalScreenPrivate *priv = screen->priv;
  if (priv->exec_on_realize)
    terminal_screen_queue_idle_exec (screen);

  priv->exec_on_realize = FALSE;

}

static void
terminal_screen_update_style (TerminalScreen *screen)
{
  update_color_scheme (screen);
  terminal_screen_set_font (screen);
}

static void
terminal_screen_style_updated (GtkWidget *widget)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);

  GTK_WIDGET_CLASS (terminal_screen_parent_class)->style_updated (widget);

  terminal_screen_update_style (screen);
}

static void
terminal_screen_init (TerminalScreen *screen)
{
  const GtkTargetEntry target_table[] = {
    { (char *) "GTK_NOTEBOOK_TAB", GTK_TARGET_SAME_APP, TARGET_TAB },
    { (char *) "application/x-color", 0, TARGET_COLOR },
    { (char *) "x-special/gnome-reset-background", 0, TARGET_RESET_BG },
    { (char *) "text/x-moz-url",  0, TARGET_MOZ_URL },
    { (char *) "_NETSCAPE_URL", 0, TARGET_NETSCAPE_URL }
  };
  VteTerminal *terminal = VTE_TERMINAL (screen);
  TerminalScreenPrivate *priv;
  TerminalApp *app;
  GtkTargetList *target_list;
  GtkTargetEntry *targets;
  int n_targets;
  guint i;
  uuid_t u;
  char uuidstr[37];

  priv = screen->priv = G_TYPE_INSTANCE_GET_PRIVATE (screen, TERMINAL_TYPE_SCREEN, TerminalScreenPrivate);

  uuid_generate (u);
  uuid_unparse (u, uuidstr);
  priv->uuid = g_strdup (uuidstr);

  vte_terminal_set_mouse_autohide (terminal, TRUE);

  priv->child_pid = -1;

  vte_terminal_set_allow_hyperlink (terminal, TRUE);

  for (i = 0; i < n_url_regexes; ++i)
    {
      TagData *tag_data;

      tag_data = g_slice_new (TagData);
      tag_data->flavor = url_regex_flavors[i];
      tag_data->tag = vte_terminal_match_add_regex (terminal, url_regexes[i], 0);
      vte_terminal_match_set_cursor_name (terminal, tag_data->tag, URL_MATCH_CURSOR_NAME);

      priv->match_tags = g_slist_prepend (priv->match_tags, tag_data);
    }

  /* Setup DND */
  target_list = gtk_target_list_new (nullptr, 0);
  gtk_target_list_add_uri_targets (target_list, 0);
  gtk_target_list_add_text_targets (target_list, 0);
  gtk_target_list_add_table (target_list, target_table, G_N_ELEMENTS (target_table));

  targets = gtk_target_table_new_from_list (target_list, &n_targets);

  gtk_drag_dest_set (GTK_WIDGET (screen),
                     GtkDestDefaults(GTK_DEST_DEFAULT_MOTION |
				     GTK_DEST_DEFAULT_HIGHLIGHT |
				     GTK_DEST_DEFAULT_DROP),
                     targets, n_targets,
                     GdkDragAction(GDK_ACTION_COPY | GDK_ACTION_MOVE));

  gtk_target_table_free (targets, n_targets);
  gtk_target_list_unref (target_list);

  g_signal_connect (screen, "window-title-changed",
                    G_CALLBACK (terminal_screen_window_title_changed),
                    screen);

  app = terminal_app_get ();
  g_signal_connect (terminal_app_get_desktop_interface_settings (app), "changed::" MONOSPACE_FONT_KEY_NAME,
                    G_CALLBACK (terminal_screen_system_font_changed_cb), screen);

}

static void
terminal_screen_get_property (GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
  TerminalScreen *screen = TERMINAL_SCREEN (object);

  switch (prop_id)
    {
      case PROP_PROFILE:
        g_value_set_object (value, terminal_screen_get_profile (screen));
        break;
      case PROP_ICON:
        g_value_set_object(value, screen->priv->icon);
        break;
      case PROP_TITLE:
        g_value_set_string (value, terminal_screen_get_title (screen));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
terminal_screen_set_property (GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  TerminalScreen *screen = TERMINAL_SCREEN (object);

  switch (prop_id)
    {
      case PROP_PROFILE:
        terminal_screen_set_profile (screen, (GSettings*)g_value_get_object (value));
        break;
      case PROP_ICON:
      case PROP_TITLE:
        /* not writable */
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
terminal_screen_class_init (TerminalScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
  VteTerminalClass *terminal_class = VTE_TERMINAL_CLASS (klass);
  GSettings *settings;

  object_class->constructed = terminal_screen_constructed;
  object_class->dispose = terminal_screen_dispose;
  object_class->finalize = terminal_screen_finalize;
  object_class->get_property = terminal_screen_get_property;
  object_class->set_property = terminal_screen_set_property;

  widget_class->realize = terminal_screen_realize;
  widget_class->style_updated = terminal_screen_style_updated;
  widget_class->drag_data_received = terminal_screen_drag_data_received;
  widget_class->button_press_event = terminal_screen_button_press;
  widget_class->popup_menu = terminal_screen_popup_menu;

  terminal_class->child_exited = terminal_screen_child_exited;

  signals[PROFILE_SET] =
    g_signal_new (I_("profile-set"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalScreenClass, profile_set),
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, G_TYPE_SETTINGS);
  
  signals[SHOW_POPUP_MENU] =
    g_signal_new (I_("show-popup-menu"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalScreenClass, show_popup_menu),
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);

  signals[MATCH_CLICKED] =
    g_signal_new (I_("match-clicked"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalScreenClass, match_clicked),
                  g_signal_accumulator_true_handled, nullptr,
                  _terminal_marshal_BOOLEAN__STRING_INT_UINT,
                  G_TYPE_BOOLEAN,
                  3, G_TYPE_STRING, G_TYPE_INT, G_TYPE_UINT);
  
  signals[CLOSE_SCREEN] =
    g_signal_new (I_("close-screen"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalScreenClass, close_screen),
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  g_object_class_install_property
    (object_class,
     PROP_PROFILE,
     g_param_spec_object ("profile", nullptr, nullptr,
                          G_TYPE_SETTINGS,
                          GParamFlags(G_PARAM_READWRITE |
				      G_PARAM_STATIC_NAME |
				      G_PARAM_STATIC_NICK |
				      G_PARAM_STATIC_BLURB)));

  g_object_class_install_property
    (object_class,
     PROP_ICON,
     g_param_spec_object ("icon", nullptr, nullptr,
                          G_TYPE_ICON,
                          GParamFlags(G_PARAM_READABLE |
				      G_PARAM_STATIC_STRINGS |
                                      G_PARAM_EXPLICIT_NOTIFY)));

  g_object_class_install_property
    (object_class,
     PROP_TITLE,
     g_param_spec_string ("title", nullptr, nullptr,
                          nullptr,
                          GParamFlags(G_PARAM_READABLE |
				      G_PARAM_STATIC_STRINGS |
                                      G_PARAM_EXPLICIT_NOTIFY)));

  g_type_class_add_private (object_class, sizeof (TerminalScreenPrivate));

  n_url_regexes = G_N_ELEMENTS (url_regex_patterns);
  precompile_regexes (url_regex_patterns, n_url_regexes, &url_regexes, &url_regex_flavors);
  n_extra_regexes = G_N_ELEMENTS (extra_regex_patterns);
  precompile_regexes (extra_regex_patterns, n_extra_regexes, &extra_regexes, &extra_regex_flavors);

  /* This fixes bug #329827 */
  settings = terminal_app_get_global_settings (terminal_app_get ());
  terminal_screen_class_enable_menu_bar_accel_notify_cb (settings, TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY, klass);
  g_signal_connect (settings, "changed::" TERMINAL_SETTING_ENABLE_MENU_BAR_ACCEL_KEY,
                    G_CALLBACK (terminal_screen_class_enable_menu_bar_accel_notify_cb), klass);
}

static void
terminal_screen_constructed (GObject *object)
{
  TerminalScreen *screen = TERMINAL_SCREEN (object);
  TerminalScreenPrivate *priv = screen->priv;

  G_OBJECT_CLASS (terminal_screen_parent_class)->constructed (object);

  terminal_app_register_screen (terminal_app_get (), screen);
  priv->registered = TRUE;
}

static void
terminal_screen_dispose (GObject *object)
{
  TerminalScreen *screen = TERMINAL_SCREEN (object);
  TerminalScreenPrivate *priv = screen->priv;
  GtkSettings *settings;

  /* Unset child PID so that when an eventual child-exited signal arrives,
   * we don't emit "close".
   */
  priv->child_pid = -1;

  settings = gtk_widget_get_settings (GTK_WIDGET (screen));
  g_signal_handlers_disconnect_matched (settings, G_SIGNAL_MATCH_DATA,
                                        0, 0, nullptr, nullptr,
                                        screen);

  if (priv->idle_exec_source != 0)
    {
      g_source_remove (priv->idle_exec_source);
      priv->idle_exec_source = 0;
    }

  terminal_screen_clear_exec_data (screen, TRUE);

  g_clear_object(&screen->priv->icon);

  G_OBJECT_CLASS (terminal_screen_parent_class)->dispose (object);

  /* Unregister *after* chaining up to the parent's dispose,
   * since that will terminate the child process if there still
   * is any, and we need to get the dbus signal out
   * from the TerminalReceiver.
   */
  if (priv->registered) {
    terminal_app_unregister_screen (terminal_app_get (), screen);
    priv->registered = FALSE;
  }
}

static void
terminal_screen_finalize (GObject *object)
{
  TerminalScreen *screen = TERMINAL_SCREEN (object);
  TerminalScreenPrivate *priv = screen->priv;

  g_signal_handlers_disconnect_by_func (terminal_app_get_desktop_interface_settings (terminal_app_get ()),
                                        (void*)terminal_screen_system_font_changed_cb,
                                        screen);

  terminal_screen_set_profile (screen, nullptr);

  g_slist_free_full (priv->match_tags, (GDestroyNotify) free_tag_data);

  g_free (priv->uuid);

  G_OBJECT_CLASS (terminal_screen_parent_class)->finalize (object);
}

TerminalScreen *
terminal_screen_new (GSettings       *profile,
                     const char      *title,
                     double           zoom)
{
  g_return_val_if_fail (G_IS_SETTINGS (profile), nullptr);

  TerminalScreen *screen = (TerminalScreen*)g_object_new (TERMINAL_TYPE_SCREEN, nullptr);

  terminal_screen_set_profile (screen, profile);

  vte_terminal_set_size (VTE_TERMINAL (screen),
                         g_settings_get_int (profile, TERMINAL_PROFILE_DEFAULT_SIZE_COLUMNS_KEY),
                         g_settings_get_int (profile, TERMINAL_PROFILE_DEFAULT_SIZE_ROWS_KEY));

  /* If given an initial title, strip it of control characters and
   * feed it to the terminal.
   */
  if (title != nullptr) {
    GString *seq;
    const char *p;

    seq = g_string_new ("\033]0;");
    for (p = title; *p; p = g_utf8_next_char (p)) {
      gunichar c = g_utf8_get_char (p);
      if (c < 0x20 || (c >= 0x7f && c <= 0x9f))
        continue;
      else if (c == ';')
        break;

      g_string_append_unichar (seq, c);
    }
    g_string_append (seq, "\033\\");

    vte_terminal_feed (VTE_TERMINAL (screen), seq->str, seq->len);
    g_string_free (seq, TRUE);
  }

  vte_terminal_set_font_scale (VTE_TERMINAL (screen), zoom);
  terminal_screen_set_font (screen);

  return screen;
}

static gboolean
terminal_screen_reexec_from_exec_data (TerminalScreen *screen,
                                       ExecData *data,
                                       char **envv,
                                       const char *cwd,
                                       GCancellable *cancellable,
                                       GError **error)
{
  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_PROCESSES) {
    gs_free char *str = exec_data_to_string (data);
    _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                           "[screen %p] reexec_from_data: envv:%p(%u) cwd:%s data:[%s]\n",
                           screen,
                           envv, envv ? g_strv_length (envv) : 0,
                           cwd,
                           str);
  }

  return terminal_screen_exec (screen,
                               data ? data->argv : nullptr,
                               envv ? envv : data ? data->envv : nullptr,
                               data ? data->as_shell : TRUE,
                               /* If we have command line args, must always pass the cwd from the command line, too */
                               data && data->argv ? data->cwd : cwd ? cwd : data ? data->cwd : nullptr,
                               nullptr /* fd list */, nullptr /* fd array */,
                               nullptr, nullptr, nullptr, /* callback + data + destroy notify */
                               cancellable,
                               error);
}

gboolean
terminal_screen_reexec_from_screen (TerminalScreen *screen,
                                    TerminalScreen *parent_screen,
                                    GCancellable *cancellable,
                                    GError **error)
{
  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), FALSE);

  if (parent_screen == nullptr)
    return TRUE;

  g_return_val_if_fail (TERMINAL_IS_SCREEN (parent_screen), FALSE);

  terminal_unref_exec_data ExecData* data = exec_data_clone (parent_screen->priv->exec_data, FALSE);
  gs_free char* cwd = terminal_screen_get_current_dir (parent_screen);

  _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                         "[screen %p] reexec_from_screen: parent:%p cwd:%s\n",
                         screen,
                         parent_screen,
                         cwd);

  return terminal_screen_reexec_from_exec_data (screen,
                                                data,
                                                nullptr /* envv */,
                                                cwd,
                                                cancellable,
                                                error);
}

gboolean
terminal_screen_reexec (TerminalScreen *screen,
                        char **envv,
                        const char *cwd,
                        GCancellable *cancellable,
                        GError **error)
{
  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), FALSE);


  _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                         "[screen %p] reexec: envv:%p(%u) cwd:%s\n",
                         screen,
                         envv, envv ? g_strv_length (envv) : 0,
                         cwd);

  return terminal_screen_reexec_from_exec_data (screen,
                                                screen->priv->exec_data,
                                                envv,
                                                cwd,
                                                cancellable,
                                                error);
}

gboolean
terminal_screen_exec (TerminalScreen *screen,
                      char **argv,
                      char **initial_envv,
                      gboolean as_shell,
                      const char *cwd,
                      GUnixFDList *fd_list,
                      GVariant *fd_array,
                      TerminalScreenExecCallback callback,
                      gpointer user_data,
                      GDestroyNotify destroy_notify,
                      GCancellable *cancellable,
                      GError **error)
{
  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (cancellable == nullptr || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == nullptr || *error == nullptr, FALSE);
  g_return_val_if_fail (gtk_widget_get_parent (GTK_WIDGET (screen)) != nullptr, FALSE);

  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_PROCESSES) {
    gs_free char *argv_str = nullptr;
    _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                           "[screen %p] exec: argv:[%s] envv:%p(%u) as-shell:%s cwd:%s\n",
                           screen,
                           (argv_str = strv_to_string(argv)),
                           initial_envv, initial_envv ? g_strv_length (initial_envv) : 0,
                           as_shell ? "true":"false",
                           cwd);
  }

  TerminalScreenPrivate *priv = screen->priv;

  ExecData *data = exec_data_new ();
  data->callback = callback;
  data->callback_data = user_data;
  data->callback_data_destroy_notify = destroy_notify;

  GError *err = nullptr;
  if (priv->child_pid != -1) {
    g_set_error_literal (&err, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                         "Cannot launch a new child process while the terminal is still running another child process");

    terminal_screen_show_info_bar (screen, err, FALSE);
    g_propagate_error (error, err);
    exec_data_unref (data); /* frees the callback data */
    return FALSE;
  }

  gs_free char *path = nullptr;
  gs_free char *shell = nullptr;
  gs_strfreev char **envv = terminal_screen_get_child_environment (screen,
                                                                  initial_envv,
                                                                  &path,
                                                                  &shell);

  gboolean preserve_cwd = FALSE;
  GSpawnFlags spawn_flags = GSpawnFlags(G_SPAWN_SEARCH_PATH_FROM_ENVP |
					VTE_SPAWN_NO_PARENT_ENVV);
  gs_strfreev char **exec_argv = nullptr;
  if (!terminal_screen_get_child_command (screen,
                                          argv,
                                          path,
                                          shell,
                                          as_shell,
                                          &preserve_cwd,
                                          &spawn_flags,
                                          &exec_argv,
                                          &err)) {
    terminal_screen_show_info_bar (screen, err, FALSE);
    g_propagate_error (error, err);
    exec_data_unref (data); /* frees the callback data */
    return FALSE;
  }

  if (!preserve_cwd) {
    cwd = g_get_home_dir ();
    envv = g_environ_unsetenv (envv, "PWD");
  }

  data->fd_list = (GUnixFDList*)(fd_list ? g_object_ref(fd_list) : nullptr);

  if (fd_array) {
    g_assert_nonnull(fd_list);
    int n_fds = g_unix_fd_list_get_length(fd_list);

    gsize fd_array_data_len;
    const int *fd_array_data = (int const*)g_variant_get_fixed_array (fd_array, &fd_array_data_len, 2 * sizeof (int));

    data->n_fd_map = fd_array_data_len;
    data->fd_map = g_new (int, data->n_fd_map);
    for (gsize i = 0; i < fd_array_data_len; i++) {
      const int fd = fd_array_data[2 * i];
      const int idx = fd_array_data[2 * i + 1];
      g_assert_cmpint(idx, >=, 0);
      g_assert_cmpuint(idx, <, n_fds);

      data->fd_map[idx] = fd;
    }
  } else {
    data->n_fd_map = 0;
    data->fd_map = nullptr;
  }

  data->argv = g_strdupv (argv);
  data->exec_argv = g_strdupv (exec_argv);
  data->cwd = g_strdup (cwd);
  data->envv = g_strdupv (envv);
  data->as_shell = as_shell;
  data->pty_flags = VTE_PTY_DEFAULT;
  data->spawn_flags = spawn_flags;
  data->cancellable = (GCancellable*)(cancellable ? g_object_ref (cancellable) : nullptr);

  terminal_screen_clear_exec_data (screen, TRUE);
  priv->exec_data = data;

  terminal_screen_queue_idle_exec (screen);

  return TRUE;
}

const char*
terminal_screen_get_title (TerminalScreen *screen)
{
  return vte_terminal_get_window_title (VTE_TERMINAL (screen));
}

static void
terminal_screen_profile_changed_cb (GSettings     *profile,
                                    const char    *prop_name,
                                    TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  GObject *object = G_OBJECT (screen);
  VteTerminal *vte_terminal = VTE_TERMINAL (screen);
  TerminalWindow *window;

  g_object_freeze_notify (object);

  if ((window = terminal_screen_get_window (screen)))
    {
      /* We need these in line for the set_size in
       * update_on_realize
       */
      terminal_window_update_geometry (window);
    }

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SCROLLBAR_POLICY_KEY))
    _terminal_screen_update_scrollbar (screen);

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_ENCODING_KEY))
    {
      gs_free char *charset = g_settings_get_string (profile, TERMINAL_PROFILE_ENCODING_KEY);
      const char *encoding = terminal_util_translate_encoding (charset);
      if (encoding != nullptr)
        vte_terminal_set_encoding (vte_terminal, encoding, nullptr);
    }

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_CJK_UTF8_AMBIGUOUS_WIDTH_KEY))
    {
      int width;

      width = g_settings_get_enum (profile, TERMINAL_PROFILE_CJK_UTF8_AMBIGUOUS_WIDTH_KEY);
      vte_terminal_set_cjk_ambiguous_width (vte_terminal, width);
    }

  if (gtk_widget_get_realized (GTK_WIDGET (screen)) &&
      (!prop_name ||
       prop_name == I_(TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY) ||
       prop_name == I_(TERMINAL_PROFILE_FONT_KEY) ||
       prop_name == I_(TERMINAL_PROFILE_CELL_WIDTH_SCALE_KEY) ||
       prop_name == I_(TERMINAL_PROFILE_CELL_HEIGHT_SCALE_KEY)))
    terminal_screen_set_font (screen);

  if (!prop_name ||
      prop_name == I_(TERMINAL_PROFILE_USE_THEME_COLORS_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_FOREGROUND_COLOR_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_BACKGROUND_COLOR_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_BOLD_COLOR_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_CURSOR_COLORS_SET_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_CURSOR_BACKGROUND_COLOR_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_CURSOR_FOREGROUND_COLOR_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_HIGHLIGHT_COLORS_SET_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_HIGHLIGHT_BACKGROUND_COLOR_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_HIGHLIGHT_FOREGROUND_COLOR_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_PALETTE_KEY))
    update_color_scheme (screen);

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_AUDIBLE_BELL_KEY))
      vte_terminal_set_audible_bell (vte_terminal, g_settings_get_boolean (profile, TERMINAL_PROFILE_AUDIBLE_BELL_KEY));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SCROLL_ON_INSERT_KEY))
    vte_terminal_set_scroll_on_insert(vte_terminal,
                                      g_settings_get_boolean(profile, TERMINAL_PROFILE_SCROLL_ON_INSERT_KEY));
  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE_KEY))
    vte_terminal_set_scroll_on_keystroke (vte_terminal,
                                          g_settings_get_boolean (profile, TERMINAL_PROFILE_SCROLL_ON_KEYSTROKE_KEY));
  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_SCROLL_ON_OUTPUT_KEY))
    vte_terminal_set_scroll_on_output (vte_terminal,
                                       g_settings_get_boolean (profile, TERMINAL_PROFILE_SCROLL_ON_OUTPUT_KEY));
  if (!prop_name ||
      prop_name == I_(TERMINAL_PROFILE_SCROLLBACK_LINES_KEY) ||
      prop_name == I_(TERMINAL_PROFILE_SCROLLBACK_UNLIMITED_KEY))
    {
      glong lines = g_settings_get_boolean (profile, TERMINAL_PROFILE_SCROLLBACK_UNLIMITED_KEY) ?
		    -1 : g_settings_get_int (profile, TERMINAL_PROFILE_SCROLLBACK_LINES_KEY);
      vte_terminal_set_scrollback_lines (vte_terminal, lines);
    }

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_BACKSPACE_BINDING_KEY))
  vte_terminal_set_backspace_binding (vte_terminal,
                                      VteEraseBinding(g_settings_get_enum (profile, TERMINAL_PROFILE_BACKSPACE_BINDING_KEY)));
  
  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_DELETE_BINDING_KEY))
  vte_terminal_set_delete_binding (vte_terminal,
                                   VteEraseBinding(g_settings_get_enum (profile, TERMINAL_PROFILE_DELETE_BINDING_KEY)));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_ENABLE_BIDI_KEY))
    vte_terminal_set_enable_bidi (vte_terminal,
                                  g_settings_get_boolean (profile, TERMINAL_PROFILE_ENABLE_BIDI_KEY));
  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_ENABLE_SHAPING_KEY))
    vte_terminal_set_enable_shaping (vte_terminal,
                                     g_settings_get_boolean (profile, TERMINAL_PROFILE_ENABLE_SHAPING_KEY));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_ENABLE_SIXEL_KEY))
    vte_terminal_set_enable_sixel (vte_terminal,
                                   g_settings_get_boolean (profile, TERMINAL_PROFILE_ENABLE_SIXEL_KEY));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_BOLD_IS_BRIGHT_KEY))
    vte_terminal_set_bold_is_bright (vte_terminal,
                                     g_settings_get_boolean (profile, TERMINAL_PROFILE_BOLD_IS_BRIGHT_KEY));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_CURSOR_BLINK_MODE_KEY))
    vte_terminal_set_cursor_blink_mode (vte_terminal,
                                        VteCursorBlinkMode(g_settings_get_enum (priv->profile, TERMINAL_PROFILE_CURSOR_BLINK_MODE_KEY)));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_CURSOR_SHAPE_KEY))
    vte_terminal_set_cursor_shape (vte_terminal,
                                   VteCursorShape(g_settings_get_enum (priv->profile, TERMINAL_PROFILE_CURSOR_SHAPE_KEY)));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_REWRAP_ON_RESIZE_KEY))
    vte_terminal_set_rewrap_on_resize (vte_terminal,
                                       g_settings_get_boolean (profile, TERMINAL_PROFILE_REWRAP_ON_RESIZE_KEY));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_TEXT_BLINK_MODE_KEY))
    vte_terminal_set_text_blink_mode (vte_terminal,
                                      VteTextBlinkMode(g_settings_get_enum (profile, TERMINAL_PROFILE_TEXT_BLINK_MODE_KEY)));

  if (!prop_name || prop_name == I_(TERMINAL_PROFILE_WORD_CHAR_EXCEPTIONS_KEY))
    {
      gs_free char *word_char_exceptions;
      g_settings_get (profile, TERMINAL_PROFILE_WORD_CHAR_EXCEPTIONS_KEY, "ms", &word_char_exceptions);
      vte_terminal_set_word_char_exceptions (vte_terminal, word_char_exceptions);
    }

  g_object_thaw_notify (object);
}

static void
update_color_scheme (TerminalScreen *screen)
{
  GtkWidget *widget = GTK_WIDGET (screen);
  TerminalScreenPrivate *priv = screen->priv;
  GSettings *profile = priv->profile;
  gs_free GdkRGBA *colors;
  gsize n_colors;
  GdkRGBA fg, bg, bold, theme_fg, theme_bg;
  GdkRGBA cursor_bg, cursor_fg;
  GdkRGBA highlight_bg, highlight_fg;
  GdkRGBA *boldp;
  GdkRGBA *cursor_bgp = nullptr, *cursor_fgp = nullptr;
  GdkRGBA *highlight_bgp = nullptr, *highlight_fgp = nullptr;
  GtkStyleContext *context;
  gboolean use_theme_colors;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_get_color (context, gtk_style_context_get_state (context), &theme_fg);
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_style_context_get_background_color (context, gtk_style_context_get_state (context), &theme_bg);
  G_GNUC_END_IGNORE_DEPRECATIONS

  use_theme_colors = g_settings_get_boolean (profile, TERMINAL_PROFILE_USE_THEME_COLORS_KEY);
  if (use_theme_colors ||
      (!terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_FOREGROUND_COLOR_KEY, &fg) ||
       !terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_BACKGROUND_COLOR_KEY, &bg)))
    {
      fg = theme_fg;
      bg = theme_bg;
    }

  if (!g_settings_get_boolean (profile, TERMINAL_PROFILE_BOLD_COLOR_SAME_AS_FG_KEY) &&
      !use_theme_colors &&
      terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_BOLD_COLOR_KEY, &bold))
    boldp = &bold;
  else
    boldp = nullptr;

  if (g_settings_get_boolean (profile, TERMINAL_PROFILE_CURSOR_COLORS_SET_KEY) &&
      !use_theme_colors)
    {
      if (terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_CURSOR_BACKGROUND_COLOR_KEY, &cursor_bg))
        cursor_bgp = &cursor_bg;
      if (terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_CURSOR_FOREGROUND_COLOR_KEY, &cursor_fg))
        cursor_fgp = &cursor_fg;
    }

  if (g_settings_get_boolean (profile, TERMINAL_PROFILE_HIGHLIGHT_COLORS_SET_KEY) &&
      !use_theme_colors)
    {
      if (terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_HIGHLIGHT_BACKGROUND_COLOR_KEY, &highlight_bg))
        highlight_bgp = &highlight_bg;
      if (terminal_g_settings_get_rgba (profile, TERMINAL_PROFILE_HIGHLIGHT_FOREGROUND_COLOR_KEY, &highlight_fg))
        highlight_fgp = &highlight_fg;
    }

  colors = terminal_g_settings_get_rgba_palette (priv->profile, TERMINAL_PROFILE_PALETTE_KEY, &n_colors);
  vte_terminal_set_colors (VTE_TERMINAL (screen), &fg, &bg,
                           colors, n_colors);
  vte_terminal_set_color_bold (VTE_TERMINAL (screen), boldp);
  vte_terminal_set_color_cursor (VTE_TERMINAL (screen), cursor_bgp);
  vte_terminal_set_color_cursor_foreground (VTE_TERMINAL (screen), cursor_fgp);
  vte_terminal_set_color_highlight (VTE_TERMINAL (screen), highlight_bgp);
  vte_terminal_set_color_highlight_foreground (VTE_TERMINAL (screen), highlight_fgp);
}

static void
terminal_screen_set_font (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  GSettings *profile = priv->profile;
  PangoFontDescription *desc;
  int size;

  if (g_settings_get_boolean (profile, TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY))
    {
      desc = terminal_app_get_system_font (terminal_app_get ());
    }
  else
    {
      gs_free char *font;
      font = g_settings_get_string (profile, TERMINAL_PROFILE_FONT_KEY);
      desc = pango_font_description_from_string (font);
    }

  size = pango_font_description_get_size (desc);
  /* Sanity check */
  if (size == 0) {
    if (pango_font_description_get_size_is_absolute (desc))
      pango_font_description_set_absolute_size (desc, 10);
    else
      pango_font_description_set_size (desc, 10);
  }

  vte_terminal_set_font (VTE_TERMINAL (screen), desc);

  pango_font_description_free (desc);

  vte_terminal_set_cell_width_scale (VTE_TERMINAL (screen),
                                     g_settings_get_double (profile, TERMINAL_PROFILE_CELL_WIDTH_SCALE_KEY));
  vte_terminal_set_cell_height_scale (VTE_TERMINAL (screen),
                                      g_settings_get_double (profile, TERMINAL_PROFILE_CELL_HEIGHT_SCALE_KEY));
}

static void
terminal_screen_system_font_changed_cb (GSettings      *settings,
                                        const char     *key,
                                        TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  if (!gtk_widget_get_realized (GTK_WIDGET (screen)))
    return;

  if (!g_settings_get_boolean (priv->profile, TERMINAL_PROFILE_USE_SYSTEM_FONT_KEY))
    return;

  terminal_screen_set_font (screen);
}

void
terminal_screen_set_profile (TerminalScreen *screen,
                             GSettings *profile)
{
  TerminalScreenPrivate *priv = screen->priv;
  GSettings*old_profile;

  old_profile = priv->profile;
  if (profile == old_profile)
    return;

  if (priv->profile_changed_id)
    {
      g_signal_handler_disconnect (G_OBJECT (priv->profile),
                                   priv->profile_changed_id);
      priv->profile_changed_id = 0;
    }

  priv->profile = profile;
  if (profile)
    {
      g_object_ref (profile);
      priv->profile_changed_id =
        g_signal_connect (profile, "changed",
                          G_CALLBACK (terminal_screen_profile_changed_cb),
                          screen);
      terminal_screen_profile_changed_cb (profile, nullptr, screen);

      g_signal_emit (G_OBJECT (screen), signals[PROFILE_SET], 0, old_profile);
    }

  if (old_profile)
    g_object_unref (old_profile);

  g_object_notify (G_OBJECT (screen), "profile");
}

GSettings*
terminal_screen_get_profile (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  return priv->profile;
}

GSettings*
terminal_screen_ref_profile (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  if (priv->profile != nullptr)
    return (GSettings*)g_object_ref (priv->profile);
  return nullptr;
}

static gboolean
should_preserve_cwd (TerminalPreserveWorkingDirectory preserve_cwd,
                     const char *path,
                     const char *arg0)
{
  switch (preserve_cwd) {
  case TERMINAL_PRESERVE_WORKING_DIRECTORY_SAFE: {
    gs_free char *resolved_arg0 = terminal_util_find_program_in_path (path, arg0);
    return resolved_arg0 != nullptr &&
      terminal_util_get_is_shell (resolved_arg0);
  }

  case TERMINAL_PRESERVE_WORKING_DIRECTORY_ALWAYS:
    return TRUE;

  case TERMINAL_PRESERVE_WORKING_DIRECTORY_NEVER:
  default:
    return FALSE;
  }
}

static gboolean
terminal_screen_get_child_command (TerminalScreen *screen,
                                   char          **argv,
                                   const char     *path_env,
                                   const char     *shell_env,
                                   gboolean        as_shell,
                                   gboolean       *preserve_cwd_p,
                                   GSpawnFlags    *spawn_flags_p,
                                   char         ***exec_argv_p,
                                   GError        **err)
{
  TerminalScreenPrivate *priv = screen->priv;
  GSettings *profile = priv->profile;
  TerminalPreserveWorkingDirectory preserve_cwd;
  char **exec_argv;

  g_assert (spawn_flags_p != nullptr && exec_argv_p != nullptr && preserve_cwd_p != nullptr);

  *exec_argv_p = exec_argv = nullptr;

  preserve_cwd = TerminalPreserveWorkingDirectory
    (g_settings_get_enum (profile, TERMINAL_PROFILE_PRESERVE_WORKING_DIRECTORY_KEY));

  if (argv)
    {
      exec_argv = g_strdupv (argv);

      /* argv and cwd come from the command line client, so it must always be used */
      *preserve_cwd_p = TRUE;
      *spawn_flags_p = GSpawnFlags(*spawn_flags_p | G_SPAWN_SEARCH_PATH_FROM_ENVP);
    }
  else if (g_settings_get_boolean (profile, TERMINAL_PROFILE_USE_CUSTOM_COMMAND_KEY))
    {
      gs_free char *exec_argv_str;

      exec_argv_str = g_settings_get_string (profile, TERMINAL_PROFILE_CUSTOM_COMMAND_KEY);
      if (!g_shell_parse_argv (exec_argv_str, nullptr, &exec_argv, err))
        return FALSE;

      *preserve_cwd_p = should_preserve_cwd (preserve_cwd, path_env, exec_argv[0]);
      *spawn_flags_p = GSpawnFlags(*spawn_flags_p | G_SPAWN_SEARCH_PATH_FROM_ENVP);
    }
  else if (as_shell)
    {
      const char *only_name;
      char *shell;
      int argc = 0;

      shell = egg_shell (shell_env);

      only_name = strrchr (shell, '/');
      if (only_name != nullptr)
        only_name++;
      else {
        only_name = shell;
        *spawn_flags_p = GSpawnFlags(*spawn_flags_p | G_SPAWN_SEARCH_PATH_FROM_ENVP);
      }

      exec_argv = g_new (char*, 3);

      exec_argv[argc++] = shell;

      if (g_settings_get_boolean (profile, TERMINAL_PROFILE_LOGIN_SHELL_KEY))
        exec_argv[argc++] = g_strconcat ("-", only_name, nullptr);
      else
        exec_argv[argc++] = g_strdup (only_name);

      exec_argv[argc++] = nullptr;

      *preserve_cwd_p = should_preserve_cwd (preserve_cwd, path_env, shell);
      *spawn_flags_p = GSpawnFlags(*spawn_flags_p | G_SPAWN_FILE_AND_ARGV_ZERO);
    }

  else
    {
      g_set_error_literal (err, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                           _("No command supplied nor shell requested"));
      return FALSE;
    }

  *exec_argv_p = exec_argv;

  return TRUE;
}

static gboolean
remove_prefixed_cb(void* key,
                   void* value,
                   void* user_data)
{
  auto const env = reinterpret_cast<char const*>(key);
  auto const prefix = reinterpret_cast<char const*>(user_data);

  if (terminal_client_get_environment_prefix_filters_is_excluded(env))
    return false;

  return g_str_has_prefix(env, prefix);
}

static char**
terminal_screen_get_child_environment (TerminalScreen *screen,
                                       char **initial_envv,
                                       char **path,
                                       char **shell)
{
  TerminalApp *app = terminal_app_get ();
  char **env;
  gs_strfreev char** current_environ = nullptr;
  char *e, *v;
  GHashTable *env_table;
  GHashTableIter iter;
  GPtrArray *retval;
  guint i;

  env_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  if (initial_envv)
    env = initial_envv;
  else {
    env = current_environ = g_get_environ ();
    /* Remove this variable which we set in server.c:main() */
    env = g_environ_unsetenv (env, "G_ENABLE_DIAGNOSTIC");
  }

  for (i = 0; env[i]; ++i)
    {
      v = strchr (env[i], '=');
      if (v)
          g_hash_table_replace (env_table, g_strndup (env[i], v - env[i]), g_strdup (v + 1));
        else
          g_hash_table_replace (env_table, g_strdup (env[i]), nullptr);
    }

  /* Remove unwanted env variables */
  auto const filters = terminal_client_get_environment_filters ();
  for (i = 0; filters[i]; ++i)
    g_hash_table_remove (env_table, filters[i]);

  auto const pfilters = terminal_client_get_environment_prefix_filters ();
  for (i = 0; pfilters[i]; ++i) {
    g_hash_table_foreach_remove (env_table,
                                 GHRFunc(remove_prefixed_cb),
                                 (void*)pfilters[i]);
  }

  terminal_util_add_proxy_env (env_table);

  /* Add gnome-terminal private env vars used to communicate back to g-t-server */
  GDBusConnection *connection = g_application_get_dbus_connection (G_APPLICATION (app));
  g_hash_table_replace (env_table, g_strdup (TERMINAL_ENV_SERVICE_NAME),
                        g_strdup (g_dbus_connection_get_unique_name (connection)));

  g_hash_table_replace (env_table, g_strdup (TERMINAL_ENV_SCREEN),
                        terminal_app_dup_screen_object_path (app, screen));

  /* Convert to strv */
  retval = g_ptr_array_sized_new (g_hash_table_size (env_table));
  g_hash_table_iter_init (&iter, env_table);
  while (g_hash_table_iter_next (&iter, (gpointer *) &e, (gpointer *) &v))
    g_ptr_array_add (retval, g_strdup_printf ("%s=%s", e, v ? v : ""));
  g_ptr_array_add (retval, nullptr);

  *path = g_strdup ((char const*)g_hash_table_lookup (env_table, "PATH"));
  *shell = g_strdup ((char const*)g_hash_table_lookup (env_table, "SHELL"));

  g_hash_table_destroy (env_table);
  return (char **) g_ptr_array_free (retval, FALSE);
}

enum {
  RESPONSE_RELAUNCH,
  RESPONSE_EDIT_PREFERENCES
};

static void
info_bar_response_cb (GtkWidget *info_bar,
                      int response,
                      TerminalScreen *screen)
{
  gtk_widget_grab_focus (GTK_WIDGET (screen));

  switch (response) {
    case GTK_RESPONSE_CANCEL:
      gtk_widget_destroy (info_bar);
      g_signal_emit (screen, signals[CLOSE_SCREEN], 0);
      break;
    case RESPONSE_RELAUNCH:
      gtk_widget_destroy (info_bar);
      terminal_screen_reexec (screen, nullptr, nullptr, nullptr, nullptr);
      break;
    case RESPONSE_EDIT_PREFERENCES:
      terminal_app_edit_preferences (terminal_app_get (),
                                     terminal_screen_get_profile (screen),
                                     "custom-command-entry",
                                     gtk_get_current_event_time());
      break;
    default:
      gtk_widget_destroy (info_bar);
      break;
  }
}

static void
terminal_screen_show_info_bar (TerminalScreen *screen,
                               GError *error,
                               gboolean show_relaunch)
{
  GtkWidget *info_bar;

  if (!gtk_widget_get_parent (GTK_WIDGET (screen)))
    return;

  info_bar = terminal_info_bar_new (GTK_MESSAGE_ERROR,
                                    _("_Preferences"), RESPONSE_EDIT_PREFERENCES,
                                    !show_relaunch ? nullptr : _("_Relaunch"), RESPONSE_RELAUNCH,
                                    nullptr);
  terminal_info_bar_format_text (TERMINAL_INFO_BAR (info_bar),
                                 _("There was an error creating the child process for this terminal"));
  terminal_info_bar_format_text (TERMINAL_INFO_BAR (info_bar),
                                 "%s", error->message);
  g_signal_connect (info_bar, "response",
                    G_CALLBACK (info_bar_response_cb), screen);

  gtk_widget_set_halign (info_bar, GTK_ALIGN_FILL);
  gtk_widget_set_valign (info_bar, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (terminal_screen_container_get_from_screen (screen)),
                           info_bar);
  gtk_info_bar_set_default_response (GTK_INFO_BAR (info_bar), GTK_RESPONSE_CANCEL);
  gtk_widget_show (info_bar);
}

static void
spawn_result_cb (VteTerminal *terminal,
                 GPid pid,
                 GError *error,
                 gpointer user_data)
{
  TerminalScreen *screen = TERMINAL_SCREEN (terminal);
  ExecData *exec_data = (ExecData*)user_data;

  /* Terminal was destroyed while the spawn operation was in progress; nothing to do. */
  if (terminal == nullptr)
    goto out;

  {
  TerminalScreenPrivate *priv = screen->priv;

  priv->child_pid = pid;

  if (error) {
     // FIXMEchpe should be unnecessary, vte already does this internally
    vte_terminal_set_pty (terminal, nullptr);

    gboolean can_reexec = TRUE; /* FIXME */
    terminal_screen_show_info_bar (screen, error, can_reexec);
  }

  /* Retain info for reexec, if possible */
  ExecData *new_exec_data = exec_data_clone (exec_data, TRUE);
  terminal_screen_clear_exec_data (screen, FALSE);
  priv->exec_data = new_exec_data;
  }

out:

  /* Must do this even if the terminal was destroyed */
  exec_data_callback (exec_data, error, screen);

  exec_data_unref (exec_data);
}

static gboolean
idle_exec_cb (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  priv->idle_exec_source = 0;

  ExecData *data = priv->exec_data;
  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_PROCESSES) {
    gs_free char *str = exec_data_to_string (data);
    _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                           "[screen %p] now launching the child process: %s\n",
                           screen, str);
  }

  int n_fds;
  int *fds;
  if (data->fd_list) {
    fds = g_unix_fd_list_steal_fds(data->fd_list, &n_fds);
  } else {
    fds = nullptr;
    n_fds = 0;
  }

  VteTerminal *terminal = VTE_TERMINAL (screen);
  vte_terminal_spawn_with_fds_async (terminal,
                                     data->pty_flags,
                                     data->cwd,
                                     (char const* const*)data->exec_argv,
                                     (char const* const*)data->envv,
                                     fds, n_fds,
                                     data->fd_map, data->n_fd_map,
                                     data->spawn_flags,
                                     nullptr, nullptr, nullptr, /* child setup, data, destroy */
                                     -1,
                                     data->cancellable,
                                     spawn_result_cb,
                                     exec_data_ref (data));

  return FALSE; /* don't run again */
}

static void
terminal_screen_queue_idle_exec (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;

  if (priv->idle_exec_source != 0)
    return;

  if (!gtk_widget_get_realized (GTK_WIDGET (screen))) {
    priv->exec_on_realize = TRUE;
    return;
  }

  _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                         "[screen %p] scheduling launching the child process on idle\n",
                         screen);

  priv->idle_exec_source = g_idle_add ((GSourceFunc) idle_exec_cb, screen);
}

static TerminalScreenPopupInfo *
terminal_screen_popup_info_new (TerminalScreen *screen)
{
  TerminalScreenPopupInfo *info;

  info = g_slice_new0 (TerminalScreenPopupInfo);
  info->ref_count = 1;

  return info;
}

TerminalScreenPopupInfo *
terminal_screen_popup_info_ref (TerminalScreenPopupInfo *info)
{
  g_return_val_if_fail (info != nullptr, nullptr);

  info->ref_count++;
  return info;
}

void
terminal_screen_popup_info_unref (TerminalScreenPopupInfo *info)
{
  g_return_if_fail (info != nullptr);

  if (--info->ref_count > 0)
    return;

  g_free (info->hyperlink);
  g_free (info->url);
  g_free (info->number_info);
  g_free (info->timestamp_info);
  g_slice_free (TerminalScreenPopupInfo, info);
}

static gboolean
terminal_screen_popup_menu (GtkWidget *widget)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);
  TerminalScreenPopupInfo *info;

  info = terminal_screen_popup_info_new (screen);
  info->button = 0;
  info->timestamp = gtk_get_current_event_time ();

  g_signal_emit (screen, signals[SHOW_POPUP_MENU], 0, info);
  terminal_screen_popup_info_unref (info);

  return TRUE;
}

static void
terminal_screen_do_popup (TerminalScreen *screen,
                          GdkEventButton *event,
                          char *hyperlink,
                          char *url,
                          int url_flavor,
                          char *number_info,
                          char *timestamp_info)
{
  TerminalScreenPopupInfo *info;

  info = terminal_screen_popup_info_new (screen);
  info->button = event->button;
  info->state = event->state & gtk_accelerator_get_default_mod_mask ();
  info->timestamp = event->time;
  info->hyperlink = hyperlink; /* adopted */
  info->url = url; /* adopted */
  info->url_flavor = TerminalURLFlavor(url_flavor);
  info->number_info = number_info; /* adopted */
  info->timestamp_info = timestamp_info; /* adopted */

  g_signal_emit (screen, signals[SHOW_POPUP_MENU], 0, info);
  terminal_screen_popup_info_unref (info);
}

static gboolean
terminal_screen_button_press (GtkWidget      *widget,
                              GdkEventButton *event)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);
  gboolean (* button_press_event) (GtkWidget*, GdkEventButton*) =
    GTK_WIDGET_CLASS (terminal_screen_parent_class)->button_press_event;
  gs_free char *hyperlink = nullptr;
  gs_free char *url = nullptr;
  int url_flavor = 0;
  gs_free char *number_info = nullptr;
  gs_free char *timestamp_info = nullptr;
  guint state;

  state = event->state & gtk_accelerator_get_default_mod_mask ();

  hyperlink = terminal_screen_check_hyperlink (screen, (GdkEvent*)event);
  url = terminal_screen_check_match (screen, (GdkEvent*)event, &url_flavor);
  terminal_screen_check_extra (screen, (GdkEvent*)event, &number_info, &timestamp_info);

  if (hyperlink != nullptr &&
      (event->button == 1 || event->button == 2) &&
      (state & GDK_CONTROL_MASK))
    {
      gboolean handled = FALSE;

      g_signal_emit (screen, signals[MATCH_CLICKED], 0,
                     hyperlink,
                     FLAVOR_AS_IS,
                     state,
                     &handled);
      if (handled)
        return TRUE; /* don't do anything else such as select with the click */
    }

  if (url != nullptr &&
      (event->button == 1 || event->button == 2) &&
      (state & GDK_CONTROL_MASK))
    {
      gboolean handled = FALSE;

      g_signal_emit (screen, signals[MATCH_CLICKED], 0,
                     url,
                     url_flavor,
                     state,
                     &handled);
      if (handled)
        return TRUE; /* don't do anything else such as select with the click */
    }

  if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
      if (!(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)))
        {
          /* on right-click, we should first try to send the mouse event to
           * the client, and popup only if that's not handled. */
          if (button_press_event && button_press_event (widget, event))
            return TRUE;

          terminal_screen_do_popup (screen, event, hyperlink, url, url_flavor, number_info, timestamp_info);
          hyperlink = nullptr; /* adopted to the popup info */
          url = nullptr; /* ditto */
          number_info = nullptr; /* ditto */
          timestamp_info = nullptr; /* ditto */
          return TRUE;
        }
      else if (!(event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)))
        {
          /* do popup on shift+right-click */
          terminal_screen_do_popup (screen, event, hyperlink, url, url_flavor, number_info, timestamp_info);
          hyperlink = nullptr; /* adopted to the popup info */
          url = nullptr; /* ditto */
          number_info = nullptr; /* ditto */
          timestamp_info = nullptr; /* ditto */
          return TRUE;
        }
    }

  /* default behavior is to let the terminal widget deal with it */
  if (button_press_event)
    return button_press_event (widget, event);

  return FALSE;
}

/**
 * terminal_screen_get_current_dir:
 * @screen:
 *
 * Tries to determine the current working directory of the foreground process
 * in @screen's PTY.
 *
 * Returns: a newly allocated string containing the current working directory,
 *   or %nullptr on failure
 */
char *
terminal_screen_get_current_dir (TerminalScreen *screen)
{
  const char *uri;

  uri = vte_terminal_get_current_directory_uri (VTE_TERMINAL (screen));
  if (uri != nullptr)
    return g_filename_from_uri (uri, nullptr, nullptr);

  ExecData *data = screen->priv->exec_data;
  if (data && data->cwd)
    return g_strdup (data->cwd);

  return nullptr;
}

static void
terminal_screen_window_title_changed (VteTerminal *vte_terminal,
                                      TerminalScreen *screen)
{
  g_object_notify (G_OBJECT (screen), "title");
}

static void
terminal_screen_child_exited (VteTerminal *terminal,
                              int status)
{
  TerminalScreen *screen = TERMINAL_SCREEN (terminal);
  TerminalScreenPrivate *priv = screen->priv;
  TerminalExitAction action;

  /* Don't do anything if we don't have a child */
  if (priv->child_pid == -1)
    return;

  /* No need to chain up to VteTerminalClass::child_exited since it's nullptr */

  _terminal_debug_print (TERMINAL_DEBUG_PROCESSES,
                         "[screen %p] child process exited\n",
                         screen);

  priv->child_pid = -1;

  action = TerminalExitAction(g_settings_get_enum (priv->profile, TERMINAL_PROFILE_EXIT_ACTION_KEY));

  switch (action)
    {
    case TERMINAL_EXIT_CLOSE:
      g_signal_emit (screen, signals[CLOSE_SCREEN], 0);
      break;
    case TERMINAL_EXIT_RESTART:
      terminal_screen_reexec (screen, nullptr, nullptr, nullptr, nullptr);
      break;
    case TERMINAL_EXIT_HOLD: {
      GtkWidget *info_bar;

      info_bar = terminal_info_bar_new (GTK_MESSAGE_INFO,
                                        _("_Relaunch"), RESPONSE_RELAUNCH,
                                        nullptr);
      if (WIFEXITED (status)) {
        terminal_info_bar_format_text (TERMINAL_INFO_BAR (info_bar),
                                      _("The child process exited normally with status %d."), WEXITSTATUS (status));
      } else if (WIFSIGNALED (status)) {
        terminal_info_bar_format_text (TERMINAL_INFO_BAR (info_bar),
                                      _("The child process was aborted by signal %d."), WTERMSIG (status));
      } else {
        terminal_info_bar_format_text (TERMINAL_INFO_BAR (info_bar),
                                      _("The child process was aborted."));
      }
      g_signal_connect (info_bar, "response",
                        G_CALLBACK (info_bar_response_cb), screen);

      gtk_widget_set_halign (info_bar, GTK_ALIGN_FILL);
      gtk_widget_set_valign (info_bar, GTK_ALIGN_START);
      gtk_overlay_add_overlay (GTK_OVERLAY (terminal_screen_container_get_from_screen (screen)),
                               info_bar);
      gtk_info_bar_set_default_response (GTK_INFO_BAR (info_bar), RESPONSE_RELAUNCH);
      gtk_widget_show (info_bar);
      break;
    }

    default:
      break;
    }
}

static void
terminal_screen_drag_data_received (GtkWidget        *widget,
                                    GdkDragContext   *context,
                                    gint              x,
                                    gint              y,
                                    GtkSelectionData *selection_data,
                                    guint             info,
                                    guint             timestamp)
{
  TerminalScreen *screen = TERMINAL_SCREEN (widget);
  TerminalScreenPrivate *priv = screen->priv;
  const guchar *selection_data_data;
  GdkAtom selection_data_target;
  gint selection_data_length, selection_data_format;

  selection_data_data = gtk_selection_data_get_data (selection_data);
  selection_data_target = gtk_selection_data_get_target (selection_data);
  selection_data_length = gtk_selection_data_get_length (selection_data);
  selection_data_format = gtk_selection_data_get_format (selection_data);

#if 0
  {
    GList *tmp;

    g_print ("info: %d\n", info);
    tmp = context->targets;
    while (tmp != nullptr)
      {
        GdkAtom atom = GDK_POINTER_TO_ATOM (tmp->data);

        g_print ("Target: %s\n", gdk_atom_name (atom));        
        
        tmp = tmp->next;
      }

    g_print ("Chosen target: %s\n", gdk_atom_name (selection_data->target));
  }
#endif

  if (gtk_targets_include_uri (&selection_data_target, 1))
    {
      gs_strfreev char **uris;
      gs_free char *text = nullptr;
      gsize len;

      uris = gtk_selection_data_get_uris (selection_data);
      if (!uris)
        return;

      terminal_util_transform_uris_to_quoted_fuse_paths (uris);

      text = terminal_util_concat_uris (uris, &len);
      terminal_screen_paste_text (screen, text, len);
    }
  else if (gtk_targets_include_text (&selection_data_target, 1))
    {
      gs_free char *text;

      text = (char *) gtk_selection_data_get_text (selection_data);
      if (text && text[0])
        terminal_screen_paste_text (screen, text, -1);
    }
  else switch (info)
    {
    case TARGET_COLOR:
      {
        guint16 *data = (guint16 *)selection_data_data;
        GdkRGBA color;

        /* We accept drops with the wrong format, since the KDE color
         * chooser incorrectly drops application/x-color with format 8.
         * So just check for the data length.
         */
        if (selection_data_length != 8)
          return;

        color.red = (double) data[0] / 65535.;
        color.green = (double) data[1] / 65535.;
        color.blue = (double) data[2] / 65535.;
        color.alpha = 1.;
        /* FIXME: use opacity from data[3] */

        terminal_g_settings_set_rgba (priv->profile,
                                      TERMINAL_PROFILE_BACKGROUND_COLOR_KEY,
                                      &color);
        g_settings_set_boolean (priv->profile, TERMINAL_PROFILE_USE_THEME_COLORS_KEY, FALSE);
      }
      break;

    case TARGET_MOZ_URL:
      {
        char *utf8_data, *text;
        char *uris[2];
        gsize len;
        
        /* MOZ_URL is in UCS-2 but in format 8. BROKEN!
         *
         * The data contains the URL, a \n, then the
         * title of the web page.
         *
         * Note that some producers (e.g. dolphin) delimit with a \r\n
         * (see issue#293), so we need to handle that, too.
         */
        if (selection_data_format != 8 ||
            selection_data_length == 0 ||
            (selection_data_length % 2) != 0)
          return;

        utf8_data = g_utf16_to_utf8 ((const gunichar2*) selection_data_data,
                                     selection_data_length / 2,
                                     nullptr, nullptr, nullptr);
        if (!utf8_data)
          return;

        uris[0] = g_strdelimit(utf8_data, "\r\n", 0);
        uris[1] = nullptr;
        terminal_util_transform_uris_to_quoted_fuse_paths (uris); /* This may replace uris[0] */

        text = terminal_util_concat_uris (uris, &len);
        terminal_screen_paste_text (screen, text, len);
        g_free (text);
        g_free (uris[0]);
      }
      break;

    case TARGET_NETSCAPE_URL:
      {
        char *utf8_data, *newline, *text;
        char *uris[2];
        gsize len;
        
        /* The data contains the URL, a \n, then the
         * title of the web page.
         */
        if (selection_data_length < 0 || selection_data_format != 8)
          return;

        utf8_data = g_strndup ((char *) selection_data_data, selection_data_length);
        newline = strchr (utf8_data, '\n');
        if (newline)
          *newline = '\0';

        uris[0] = utf8_data;
        uris[1] = nullptr;
        terminal_util_transform_uris_to_quoted_fuse_paths (uris); /* This may replace uris[0] */

        text = terminal_util_concat_uris (uris, &len);
        terminal_screen_paste_text (screen, text, len);
        g_free (text);
        g_free (uris[0]);
      }
      break;

    case TARGET_RESET_BG:
      g_settings_reset (priv->profile, TERMINAL_PROFILE_BACKGROUND_COLOR_KEY);
      break;

    case TARGET_TAB:
      {
        GtkWidget *container;
        TerminalScreen *moving_screen;
        TerminalWindow *source_window;
        TerminalWindow *dest_window;

        container = *(GtkWidget**) selection_data_data;
        if (!GTK_IS_WIDGET (container))
          return;

        moving_screen = terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (container));
        g_warn_if_fail (TERMINAL_IS_SCREEN (moving_screen));
        if (!TERMINAL_IS_SCREEN (moving_screen))
          return;

        source_window = terminal_screen_get_window (moving_screen);
        dest_window = terminal_screen_get_window (screen);
        terminal_window_move_screen (source_window, dest_window, moving_screen, -1);

        gtk_drag_finish (context, TRUE, TRUE, timestamp);
      }
      break;

    default:
      g_assert_not_reached ();
    }
}

void
_terminal_screen_update_scrollbar (TerminalScreen *screen)
{
  TerminalScreenPrivate *priv = screen->priv;
  TerminalScreenContainer *container;
  GtkPolicyType vpolicy;

  container = terminal_screen_container_get_from_screen (screen);
  if (container == nullptr)
    return;

  vpolicy = GtkPolicyType(g_settings_get_enum (priv->profile, TERMINAL_PROFILE_SCROLLBAR_POLICY_KEY));

  terminal_screen_container_set_policy (container, GTK_POLICY_NEVER, vpolicy);
}

void
terminal_screen_get_size (TerminalScreen *screen,
			  int       *width_chars,
			  int       *height_chars)
{
  VteTerminal *terminal = VTE_TERMINAL (screen);

  *width_chars = vte_terminal_get_column_count (terminal);
  *height_chars = vte_terminal_get_row_count (terminal);
}

void
terminal_screen_get_cell_size (TerminalScreen *screen,
			       int                  *cell_width_pixels,
			       int                  *cell_height_pixels)
{
  VteTerminal *terminal = VTE_TERMINAL (screen);

  *cell_width_pixels = vte_terminal_get_char_width (terminal);
  *cell_height_pixels = vte_terminal_get_char_height (terminal);
}

static char*
terminal_screen_check_hyperlink (TerminalScreen *screen,
                                 GdkEvent       *event)
{
  return vte_terminal_hyperlink_check_event (VTE_TERMINAL (screen), event);
}

static char*
terminal_screen_check_match (TerminalScreen *screen,
                             GdkEvent       *event,
                             int       *flavor)
{
  TerminalScreenPrivate *priv = screen->priv;
  GSList *tags;
  int tag;
  char *match;

  match = vte_terminal_match_check_event (VTE_TERMINAL (screen), event, &tag);
  for (tags = priv->match_tags; tags != nullptr; tags = tags->next)
    {
      TagData *tag_data = (TagData*) tags->data;
      if (tag_data->tag == tag)
	{
	  if (flavor)
	    *flavor = tag_data->flavor;
	  return match;
	}
    }

  g_free (match);
  return nullptr;
}

static void
terminal_screen_check_extra (TerminalScreen *screen,
                             GdkEvent       *event,
                             char           **number_info,
                             char           **timestamp_info)
{
  guint i;
  char **matches;
  gboolean flavor_number_found = FALSE;

  matches = g_newa (char *, n_extra_regexes);
  memset(matches, 0, sizeof(char*) * n_extra_regexes);

  if (
      vte_terminal_event_check_regex_simple (VTE_TERMINAL (screen),
                                             event,
                                             extra_regexes,
                                             n_extra_regexes,
                                             0,
                                             matches))
    {
      for (i = 0; i < n_extra_regexes; i++)
        {
          if (matches[i] != nullptr)
            {
              /* Store the first match for each flavor, free all the others */
              switch (extra_regex_flavors[i])
                {
                case FLAVOR_NUMBER:
                  if (!flavor_number_found)
                    {
                      *number_info = terminal_util_number_info (matches[i]);
                      *timestamp_info = terminal_util_timestamp_info (matches[i]);
                      flavor_number_found = TRUE;
                    }
                  g_free (matches[i]);
                  break;
                default:
                  g_free (matches[i]);
                }
            }
        }
    }
}

/**
 * terminal_screen_has_foreground_process:
 * @screen:
 * @process_name: (out) (allow-none): the basename of the program, or %nullptr
 * @cmdline: (out) (allow-none): the full command line, or %nullptr
 *
 * Checks whether there's a foreground process running in
 * this terminal.
 * 
 * Returns: %TRUE iff there's a foreground process running in @screen
 */
gboolean
terminal_screen_has_foreground_process (TerminalScreen *screen,
                                        char           **process_name,
                                        char           **cmdline)
{
  TerminalScreenPrivate *priv = screen->priv;
  gs_free char *command = nullptr;
  gs_free char *data_buf = nullptr;
  gs_free char *basename = nullptr;
  gs_free char *name = nullptr;
  VtePty *pty;
  int fd;
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
  int mib[4];
#else
  char filename[64];
#endif
  char *data;
  gsize i;
  gsize len;
  int fgpid;

  if (priv->child_pid == -1)
    return FALSE;

  pty = vte_terminal_get_pty (VTE_TERMINAL (screen));
  if (pty == nullptr)
    return FALSE;

  fd = vte_pty_get_fd (pty);
  if (fd == -1)
    return FALSE;

  fgpid = tcgetpgrp (fd);
  if (fgpid == -1 || fgpid == priv->child_pid)
    return FALSE;

#if defined(__FreeBSD__) || defined(__DragonFly__)
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_ARGS;
  mib[3] = fgpid;
  if (sysctl (mib, G_N_ELEMENTS (mib), nullptr, &len, nullptr, 0) == -1)
      return TRUE;

  data_buf = (char*)g_malloc0 (len);
  if (sysctl (mib, G_N_ELEMENTS (mib), data_buf, &len, nullptr, 0) == -1)
      return TRUE;
  data = data_buf;
#elif defined(__OpenBSD__)
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC_ARGS;
  mib[2] = fgpid;
  mib[3] = KERN_PROC_ARGV;
  if (sysctl (mib, G_N_ELEMENTS (mib), nullptr, &len, nullptr, 0) == -1)
      return TRUE;

  data_buf = (char*)g_malloc0 (len);
  if (sysctl (mib, G_N_ELEMENTS (mib), data_buf, &len, nullptr, 0) == -1)
      return TRUE;
  data = ((char**)data_buf)[0];
#else
  g_snprintf (filename, sizeof (filename), "/proc/%d/cmdline", fgpid);
  if (!g_file_get_contents (filename, &data_buf, &len, nullptr))
    return TRUE;
  data = data_buf;
#endif

  basename = g_path_get_basename (data);
  if (!basename)
    return TRUE;

  name = g_filename_to_utf8 (basename, -1, nullptr, nullptr, nullptr);
  if (!name)
    return TRUE;

  if (!process_name && !cmdline)
    return TRUE;

  gs_transfer_out_value (process_name, &name);

  if (len > 0 && data[len - 1] == '\0')
    len--;
  for (i = 0; i < len; i++)
    {
      if (data[i] == '\0')
        data[i] = ' ';
    }

  command = g_filename_to_utf8 (data, -1, nullptr, nullptr, nullptr);
  if (!command)
    return TRUE;

  gs_transfer_out_value (cmdline, &command);

  return TRUE;
}

const char *
terminal_screen_get_uuid (TerminalScreen *screen)
{
  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), nullptr);

  return screen->priv->uuid;
}

/**
 * terminal_screen_paste_text:
 * @screen:
 * @text: a NUL-terminated string
 * @len: length of @text, or -1
 *
 * Inserts @text to @terminal as if pasted.
 */
void
terminal_screen_paste_text (TerminalScreen* screen,
                            char const* text,
                            gssize len)
{
  g_return_if_fail (text != nullptr);
  g_return_if_fail (len >= -1);

  /* This is just an API hack until vte 0.69 adds vte_terminal_paste_text_len() */
  /* Note that @text MUST be NUL-terminated */

  vte_terminal_paste_text (VTE_TERMINAL (screen), text);
}

GIcon*
terminal_screen_get_icon(TerminalScreen* screen)
{
  g_return_val_if_fail(TERMINAL_IS_SCREEN(screen), nullptr);

  return screen->priv->icon;
}
