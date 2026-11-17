/*
 * Copyright © 2015 Christian Persch
 * Copyright © 2005 Paolo Maggi
 * Copyright © 2010 Red Hat (Red Hat author: Behdad Esfahbod)
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "terminal-pcre2.hh"
#include "terminal-search-popover.hh"
#include "terminal-intl.hh"
#include "terminal-window.hh"
#include "terminal-app.hh"
#include "terminal-libgsystem.hh"

typedef struct _TerminalSearchPopoverPrivate TerminalSearchPopoverPrivate;

struct _TerminalSearchPopover
{
  GtkWindow parent_instance;
};

struct _TerminalSearchPopoverClass
{
  GtkWindowClass parent_class;

  /* Signals */
  void (* search) (TerminalSearchPopover *popover,
                   gboolean backward);
};

struct _TerminalSearchPopoverPrivate
{
  GtkWidget *search_entry;
  GtkWidget *search_prev_button;
  GtkWidget *search_next_button;
  GtkWidget *reveal_button;
  GtkWidget *close_button;
  GtkWidget *revealer;
  GtkWidget *match_case_checkbutton;
  GtkWidget *entire_word_checkbutton;
  GtkWidget *regex_checkbutton;
  GtkWidget *wrap_around_checkbutton;

  gboolean search_text_changed;

  /* Cached regex */
  gboolean regex_caseless;
  char *regex_pattern;
  VteRegex *regex;
};

enum {
  PROP_0,
  PROP_REGEX,
  PROP_WRAP_AROUND,
  LAST_PROP
};

enum {
  SEARCH,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static GParamSpec *pspecs[LAST_PROP];
static GtkListStore *history_store;

G_DEFINE_TYPE_WITH_PRIVATE (TerminalSearchPopover, terminal_search_popover, GTK_TYPE_WINDOW)

#define PRIV(obj) ((TerminalSearchPopoverPrivate *) terminal_search_popover_get_instance_private ((TerminalSearchPopover *)(obj)))

/* history */

#define HISTORY_MIN_ITEM_LEN (3)
#define HISTORY_LENGTH (10)

static gboolean
history_enabled (void)
{
  gboolean enabled;

  /* not quite an exact setting for this, but close enough… */
  g_object_get (gtk_settings_get_default (), "gtk-recent-files-enabled", &enabled, nullptr);
  if (!enabled)
    return FALSE;

  if (history_store == nullptr) {
    history_store = gtk_list_store_new (1, G_TYPE_STRING);
    g_object_set_data_full (G_OBJECT (terminal_app_get ()), "search-history-store",
                            history_store, (GDestroyNotify) g_object_unref);
  }

  return TRUE;
}

static gboolean
history_remove_item (const char  *text)
{
  GtkTreeModel *model = GTK_TREE_MODEL (history_store);
  GtkTreeIter iter;

  if (!gtk_tree_model_get_iter_first (model, &iter))
    return FALSE;

  do {
    gs_free gchar *item_text;

    gtk_tree_model_get (model, &iter, 0, &item_text, -1);

    if (item_text != nullptr && strcmp (item_text, text) == 0) {
      gtk_list_store_remove (history_store, &iter);
      return TRUE;
    }
  } while (gtk_tree_model_iter_next (model, &iter));

  return FALSE;
}

static void
history_clamp (int max)
{
  GtkTreePath *path;
  GtkTreeIter iter;

  /* -1 because TreePath counts from 0 */
  path = gtk_tree_path_new_from_indices (max - 1, -1);

  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (history_store), &iter, path))
    while (1)
      if (!gtk_list_store_remove (history_store, &iter))
	break;

  gtk_tree_path_free (path);
}

static void
history_insert_item (const char *text)
{
  GtkTreeIter iter;

  if (!history_enabled () || text == nullptr)
    return;

  if (g_utf8_strlen (text, -1) <= HISTORY_MIN_ITEM_LEN)
    return;

  /* remove the text from the store if it was already
   * present. If it wasn't, clamp to max history - 1
   * before inserting the new row, otherwise appending
   * would not work */
  if (!history_remove_item (text))
    history_clamp (HISTORY_LENGTH - 1);

  gtk_list_store_insert_with_values (history_store, &iter, 0,
                                     0, text,
                                     -1);
}

/* helper functions */

static void
update_sensitivity (TerminalSearchPopover *popover)
{
  TerminalSearchPopoverPrivate *priv = PRIV (popover);
  gboolean can_search;

  can_search = priv->regex != nullptr;

  gtk_widget_set_sensitive (priv->search_prev_button, can_search);
  gtk_widget_set_sensitive (priv->search_next_button, can_search);
}

static void
perform_search (TerminalSearchPopover *popover,
                gboolean backward)
{
  TerminalSearchPopoverPrivate *priv = PRIV (popover);

  if (priv->regex == nullptr)
    return;

  /* Add to search history */
  if (priv->search_text_changed) {
    const char *search_text;

    search_text = gtk_entry_get_text (GTK_ENTRY (priv->search_entry));
    history_insert_item (search_text);

    priv->search_text_changed = FALSE;
  }

  g_signal_emit (popover, signals[SEARCH], 0, backward);
}

static void
previous_match_cb (GtkWidget *widget,
                  TerminalSearchPopover *popover)
{
  perform_search (popover, TRUE);
}

static void
next_match_cb (GtkWidget *widget,
               TerminalSearchPopover *popover)
{
  perform_search (popover, FALSE);
}

static void
close_clicked_cb (GtkWidget *widget,
                  GtkWidget *popover)
{
  gtk_widget_hide (popover);
}

static void
search_button_clicked_cb (GtkWidget *button,
                          TerminalSearchPopover *popover)
{
  TerminalSearchPopoverPrivate *priv = PRIV (popover);

  perform_search (popover, button == priv->search_prev_button);
}

static gboolean
key_press_cb (GtkWidget *popover,
              GdkEventKey *event,
              gpointer user_data G_GNUC_UNUSED)
{
  if (event->keyval == GDK_KEY_Escape) {
    gtk_widget_hide (popover);
    return TRUE;
  }
  return FALSE;
}

static void
update_regex (TerminalSearchPopover *popover)
{
  TerminalSearchPopoverPrivate *priv = PRIV (popover);
  const char *search_text;
  gboolean caseless;
  gs_free char *pattern;
  gs_free_error GError *error = nullptr;

  search_text = gtk_entry_get_text (GTK_ENTRY (priv->search_entry));

  caseless = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->match_case_checkbutton));

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->regex_checkbutton))) {
    pattern = g_strdup (search_text);
  } else {
    pattern = g_regex_escape_string (search_text, -1);
  }

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->entire_word_checkbutton))) {
    char *new_pattern;
    new_pattern = g_strdup_printf ("\\b%s\\b", pattern);
    g_free (pattern);
    pattern = new_pattern;
  }

  if (priv->regex_caseless == caseless &&
      g_strcmp0 (priv->regex_pattern, pattern) == 0)
    return;

  if (priv->regex) {
    vte_regex_unref (priv->regex);
  }

  g_clear_pointer (&priv->regex_pattern, g_free);

  /* FIXME: if comping the regex fails, show the error message somewhere */
  if (search_text[0] != '\0') {
    guint32 compile_flags;

    compile_flags = PCRE2_UTF | PCRE2_NO_UTF_CHECK | PCRE2_UCP | PCRE2_MULTILINE;
    if (caseless)
      compile_flags |= PCRE2_CASELESS;

    priv->regex = vte_regex_new_for_search (pattern, -1, compile_flags, &error);
    if (priv->regex != nullptr &&
        (!vte_regex_jit (priv->regex, PCRE2_JIT_COMPLETE, nullptr) ||
         !vte_regex_jit (priv->regex, PCRE2_JIT_PARTIAL_SOFT, nullptr))) {
    }

    if (priv->regex != nullptr)
      gs_transfer_out_value (&priv->regex_pattern, &pattern);
  } else {
    priv->regex = nullptr;
  }

  priv->regex_caseless = caseless;

  update_sensitivity (popover);

  g_object_notify_by_pspec (G_OBJECT (popover), pspecs[PROP_REGEX]);
}

static void
search_text_changed_cb (GtkToggleButton *button,
                        TerminalSearchPopover *popover)
{
  TerminalSearchPopoverPrivate *priv = PRIV (popover);

  update_regex (popover);
  priv->search_text_changed = TRUE;
}

static void
search_parameters_changed_cb (GtkToggleButton *button,
                              TerminalSearchPopover *popover)
{
  update_regex (popover);
}

static void
wrap_around_toggled_cb (GtkToggleButton *button,
                        TerminalSearchPopover *popover)
{
  g_object_notify_by_pspec (G_OBJECT (popover), pspecs[PROP_WRAP_AROUND]);
}

/* public functions */

/* Class implementation */

static void
terminal_search_popover_grab_focus (GtkWidget *widget)
{
  TerminalSearchPopover *popover = TERMINAL_SEARCH_POPOVER (widget);
  TerminalSearchPopoverPrivate *priv = PRIV (popover);

  gtk_widget_grab_focus (priv->search_entry);
}

static void
terminal_search_popover_init (TerminalSearchPopover *popover)
{
  TerminalSearchPopoverPrivate *priv = PRIV (popover);
  GtkWidget *widget = GTK_WIDGET (popover);

  priv->regex_pattern = 0;
  priv->regex_caseless = TRUE;

  gtk_widget_init_template (widget);

  /* Make the search entry reasonably wide */
  gtk_widget_set_size_request (priv->search_entry, 300, -1);

  /* Add entry completion with history */
#if 0
  g_object_set (G_OBJECT (priv->search_entry),
		"model", history_store,
		"entry-text-column", 0,
		nullptr);
#endif

  if (history_enabled ()) {
    gs_unref_object GtkEntryCompletion *completion;

    completion = gtk_entry_completion_new ();
    gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (history_store));
    gtk_entry_completion_set_text_column (completion, 0);
    gtk_entry_completion_set_minimum_key_length (completion, HISTORY_MIN_ITEM_LEN);
    gtk_entry_completion_set_popup_completion (completion, FALSE);
    gtk_entry_completion_set_inline_completion (completion, TRUE);
    gtk_entry_set_completion (GTK_ENTRY (priv->search_entry), completion);
  }

#if 0
  gtk_popover_set_default_widget (GTK_POPOVER (popover), priv->search_prev_button);
#else
  GtkWindow *window = GTK_WINDOW (popover);
  gtk_window_set_default (window, priv->search_prev_button);
#endif

  g_signal_connect (priv->search_entry, "previous-match", G_CALLBACK (previous_match_cb), popover);
  g_signal_connect (priv->search_entry, "next-match", G_CALLBACK (next_match_cb), popover);

  g_signal_connect (priv->search_prev_button, "clicked", G_CALLBACK (search_button_clicked_cb), popover);
  g_signal_connect (priv->search_next_button, "clicked", G_CALLBACK (search_button_clicked_cb), popover);

  g_signal_connect (priv->close_button, "clicked", G_CALLBACK (close_clicked_cb), popover);

  g_object_bind_property (priv->reveal_button, "active",
                          priv->revealer, "reveal-child",
                          G_BINDING_DEFAULT);

  update_sensitivity (popover);

  g_signal_connect (priv->search_entry, "search-changed", G_CALLBACK (search_text_changed_cb), popover);
  g_signal_connect (priv->match_case_checkbutton, "toggled", G_CALLBACK (search_parameters_changed_cb), popover);
  g_signal_connect (priv->entire_word_checkbutton, "toggled", G_CALLBACK (search_parameters_changed_cb), popover);
  g_signal_connect (priv->regex_checkbutton, "toggled", G_CALLBACK (search_parameters_changed_cb), popover);

  g_signal_connect (priv->wrap_around_checkbutton, "toggled", G_CALLBACK (wrap_around_toggled_cb), popover);

  g_signal_connect (popover, "key-press-event", G_CALLBACK (key_press_cb), nullptr);

  if (terminal_app_get_dialog_use_headerbar (terminal_app_get ())) {
    GtkWidget *headerbar;

    headerbar = (GtkWidget*)g_object_new (GTK_TYPE_HEADER_BAR,
					  "title", gtk_window_get_title (window),
					  "has-subtitle", FALSE,
					  "show-close-button", TRUE,
					  "visible", TRUE,
					  nullptr);
    gtk_style_context_add_class (gtk_widget_get_style_context (headerbar),
                                 "default-decoration");
    gtk_window_set_titlebar (window, headerbar);
  }
}

static void
terminal_search_popover_finalize (GObject *object)
{
  TerminalSearchPopover *popover = TERMINAL_SEARCH_POPOVER (object);
  TerminalSearchPopoverPrivate *priv = PRIV (popover);

  if (priv->regex) {
    vte_regex_unref (priv->regex);
  }

  g_free (priv->regex_pattern);

  G_OBJECT_CLASS (terminal_search_popover_parent_class)->finalize (object);
}

static void
terminal_search_popover_get_property (GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
  TerminalSearchPopover *popover = TERMINAL_SEARCH_POPOVER (object);

  switch (prop_id) {
  case PROP_REGEX:
    g_value_set_boxed (value, terminal_search_popover_get_regex (popover));
    break;
  case PROP_WRAP_AROUND:
    g_value_set_boolean (value, terminal_search_popover_get_wrap_around (popover));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
terminal_search_popover_set_property (GObject *object,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
  switch (prop_id) {
  case PROP_REGEX:
  case PROP_WRAP_AROUND:
    /* not writable */
    break;
  default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_search_popover_class_init (TerminalSearchPopoverClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->finalize = terminal_search_popover_finalize;
  gobject_class->get_property = terminal_search_popover_get_property;
  gobject_class->set_property = terminal_search_popover_set_property;

  widget_class->grab_focus = terminal_search_popover_grab_focus;

  signals[SEARCH] =
    g_signal_new (I_("search"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalSearchPopoverClass, search),
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  pspecs[PROP_REGEX] =
    g_param_spec_boxed ("regex", nullptr, nullptr,
                        VTE_TYPE_REGEX,
                        GParamFlags(G_PARAM_READABLE |
				    G_PARAM_STATIC_NAME |
				    G_PARAM_STATIC_NICK |
				    G_PARAM_STATIC_BLURB));

  pspecs[PROP_WRAP_AROUND] =
    g_param_spec_boolean ("wrap-around", nullptr, nullptr,
                          FALSE,
                          GParamFlags(G_PARAM_READABLE |
				      G_PARAM_STATIC_NAME |
				      G_PARAM_STATIC_NICK |
				      G_PARAM_STATIC_BLURB));

  g_object_class_install_properties (gobject_class, G_N_ELEMENTS (pspecs), pspecs);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/search-popover.ui");
  gtk_widget_class_bind_template_child_private (widget_class, TerminalSearchPopover, search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, TerminalSearchPopover, search_prev_button);
  gtk_widget_class_bind_template_child_private (widget_class, TerminalSearchPopover, search_next_button);
  gtk_widget_class_bind_template_child_private (widget_class, TerminalSearchPopover, reveal_button);
  gtk_widget_class_bind_template_child_private (widget_class, TerminalSearchPopover, close_button);
  gtk_widget_class_bind_template_child_private (widget_class, TerminalSearchPopover, revealer);
  gtk_widget_class_bind_template_child_private (widget_class, TerminalSearchPopover, match_case_checkbutton);
  gtk_widget_class_bind_template_child_private (widget_class, TerminalSearchPopover, entire_word_checkbutton);
  gtk_widget_class_bind_template_child_private (widget_class, TerminalSearchPopover, regex_checkbutton);
  gtk_widget_class_bind_template_child_private (widget_class, TerminalSearchPopover, wrap_around_checkbutton);
}

/* public API */

/**
 * terminal_search_popover_new:
 *
 * Returns: a new #TerminalSearchPopover
 */
TerminalSearchPopover *
terminal_search_popover_new (GtkWidget *relative_to_widget)
{
  return reinterpret_cast<TerminalSearchPopover*>
    (g_object_new (TERMINAL_TYPE_SEARCH_POPOVER,
#if 0
		   "relative-to", relative_to_widget,
#else
		   "transient-for", gtk_widget_get_toplevel (relative_to_widget),
#endif
		   nullptr));
}

/**
 * terminal_search_popover_get_regex:
 * @popover: a #TerminalSearchPopover
 *
 * Returns: (transfer none): the search regex, or %nullptr
 */
VteRegex *
terminal_search_popover_get_regex (TerminalSearchPopover *popover)
{
  g_return_val_if_fail (TERMINAL_IS_SEARCH_POPOVER (popover), nullptr);

  return PRIV (popover)->regex;
}

/**
 * terminal_search_popover_get_wrap_around:
 * @popover: a #TerminalSearchPopover
 *
 * Returns: (transfer none): whether search should wrap around
 */
gboolean
terminal_search_popover_get_wrap_around (TerminalSearchPopover *popover)
{
  g_return_val_if_fail (TERMINAL_IS_SEARCH_POPOVER (popover), FALSE);

  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (PRIV (popover)->wrap_around_checkbutton));
}
