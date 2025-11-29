/* gtkentrycompletion.c
 * Copyright (C) 2003  Kristian Rietveld  <kris@gtk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * GtkEntryCompletion:
 *
 * `GtkEntryCompletion` is an auxiliary object to provide completion functionality
 * for `GtkEntry`.
 *
 * It implements the [iface@Gtk.CellLayout] interface, to allow the user
 * to add extra cells to the `GtkTreeView` with completion matches.
 *
 * “Completion functionality” means that when the user modifies the text
 * in the entry, `GtkEntryCompletion` checks which rows in the model match
 * the current content of the entry, and displays a list of matches.
 * By default, the matching is done by comparing the entry text
 * case-insensitively against the text column of the model (see
 * [method@Gtk.EntryCompletion.set_text_column]), but this can be overridden
 * with a custom match function (see [method@Gtk.EntryCompletion.set_match_func]).
 *
 * When the user selects a completion, the content of the entry is
 * updated. By default, the content of the entry is replaced by the
 * text column of the model, but this can be overridden by connecting
 * to the [signal@Gtk.EntryCompletion::match-selected] signal and updating the
 * entry in the signal handler. Note that you should return %TRUE from
 * the signal handler to suppress the default behaviour.
 *
 * To add completion functionality to an entry, use
 * [method@Gtk.Entry.set_completion].
 *
 * `GtkEntryCompletion` uses a [class@Gtk.TreeModelFilter] model to
 * represent the subset of the entire model that is currently matching.
 * While the `GtkEntryCompletion` signals
 * [signal@Gtk.EntryCompletion::match-selected] and
 * [signal@Gtk.EntryCompletion::cursor-on-match] take the original model
 * and an iter pointing to that model as arguments, other callbacks and
 * signals (such as `GtkCellLayoutDataFunc` or
 * [signal@Gtk.CellArea::apply-attributes)]
 * will generally take the filter model as argument. As long as you are
 * only calling [method@Gtk.TreeModel.get], this will make no difference to
 * you. If for some reason, you need the original model, use
 * [method@Gtk.TreeModelFilter.get_model]. Don’t forget to use
 * [method@Gtk.TreeModelFilter.convert_iter_to_child_iter] to obtain a
 * matching iter.
 *
 * Deprecated: 4.10
 */

#include "config.h"

#include "gtkentrycompletion.h"

#include "gtkentryprivate.h"
#include "gtktextprivate.h"
#include "gtkcelllayout.h"
#include "gtkcellareabox.h"

#include "gtkcellrenderertext.h"
#include "gtktreeselection.h"
#include "gtktreeview.h"
#include "gtkscrolledwindow.h"
#include "gtksizerequest.h"
#include "gtkbox.h"
#include "gtkpopover.h"
#include "gtkentry.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkeventcontrollerfocus.h"
#include "gtkeventcontrollerkey.h"
#include "gtkgestureclick.h"

#include "gtkprivate.h"
#include "gtkwindowprivate.h"
#include "gtkwidgetprivate.h"
#include "gtknative.h"

#include <string.h>

#define PAGE_STEP 14
#define COMPLETION_TIMEOUT 100

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/* signals */
enum
{
  INSERT_PREFIX,
  MATCH_SELECTED,
  CURSOR_ON_MATCH,
  NO_MATCHES,
  LAST_SIGNAL
};

/* properties */
enum
{
  PROP_0,
  PROP_MODEL,
  PROP_MINIMUM_KEY_LENGTH,
  PROP_TEXT_COLUMN,
  PROP_INLINE_COMPLETION,
  PROP_POPUP_COMPLETION,
  PROP_POPUP_SET_WIDTH,
  PROP_POPUP_SINGLE_MATCH,
  PROP_INLINE_SELECTION,
  PROP_CELL_AREA,
  NUM_PROPERTIES
};


static void     gtk_entry_completion_cell_layout_init    (GtkCellLayoutIface      *iface);
static GtkCellArea* gtk_entry_completion_get_area        (GtkCellLayout           *cell_layout);

static void     gtk_entry_completion_constructed         (GObject      *object);
static void     gtk_entry_completion_set_property        (GObject      *object,
                                                          guint         prop_id,
                                                          const GValue *value,
                                                          GParamSpec   *pspec);
static void     gtk_entry_completion_get_property        (GObject      *object,
                                                          guint         prop_id,
                                                          GValue       *value,
                                                          GParamSpec   *pspec);
static void     gtk_entry_completion_finalize            (GObject      *object);
static void     gtk_entry_completion_dispose             (GObject      *object);

static gboolean gtk_entry_completion_visible_func        (GtkTreeModel       *model,
                                                          GtkTreeIter        *iter,
                                                          gpointer            data);
static void     gtk_entry_completion_list_activated      (GtkTreeView        *treeview,
                                                          GtkTreePath        *path,
                                                          GtkTreeViewColumn  *column,
                                                          gpointer            user_data);
static void     gtk_entry_completion_selection_changed   (GtkTreeSelection   *selection,
                                                          gpointer            data);

static gboolean gtk_entry_completion_match_selected      (GtkEntryCompletion *completion,
                                                          GtkTreeModel       *model,
                                                          GtkTreeIter        *iter);
static gboolean gtk_entry_completion_real_insert_prefix  (GtkEntryCompletion *completion,
                                                          const char         *prefix);
static gboolean gtk_entry_completion_cursor_on_match     (GtkEntryCompletion *completion,
                                                          GtkTreeModel       *model,
                                                          GtkTreeIter        *iter);
static gboolean gtk_entry_completion_insert_completion   (GtkEntryCompletion *completion,
                                                          GtkTreeModel       *model,
                                                          GtkTreeIter        *iter);
static void     gtk_entry_completion_insert_completion_text (GtkEntryCompletion *completion,
                                                             const char *text);
static void     connect_completion_signals                  (GtkEntryCompletion *completion);
static void     disconnect_completion_signals               (GtkEntryCompletion *completion);


static GParamSpec *entry_completion_props[NUM_PROPERTIES] = { NULL, };

static guint entry_completion_signals[LAST_SIGNAL] = { 0 };

/* GtkBuildable */
static void     gtk_entry_completion_buildable_init      (GtkBuildableIface  *iface);

G_DEFINE_TYPE_WITH_CODE (GtkEntryCompletion, gtk_entry_completion, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
                                                gtk_entry_completion_cell_layout_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_entry_completion_buildable_init))


static void
gtk_entry_completion_class_init (GtkEntryCompletionClass *klass)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *)klass;

  object_class->constructed = gtk_entry_completion_constructed;
  object_class->set_property = gtk_entry_completion_set_property;
  object_class->get_property = gtk_entry_completion_get_property;
  object_class->dispose = gtk_entry_completion_dispose;
  object_class->finalize = gtk_entry_completion_finalize;

  klass->match_selected = gtk_entry_completion_match_selected;
  klass->insert_prefix = gtk_entry_completion_real_insert_prefix;
  klass->cursor_on_match = gtk_entry_completion_cursor_on_match;
  klass->no_matches = NULL;

  /**
   * GtkEntryCompletion::insert-prefix:
   * @widget: the object which received the signal
   * @prefix: the common prefix of all possible completions
   *
   * Emitted when the inline autocompletion is triggered.
   *
   * The default behaviour is to make the entry display the
   * whole prefix and select the newly inserted part.
   *
   * Applications may connect to this signal in order to insert only a
   * smaller part of the @prefix into the entry - e.g. the entry used in
   * the `GtkFileChooser` inserts only the part of the prefix up to the
   * next '/'.
   *
   * Returns: %TRUE if the signal has been handled
   */
  entry_completion_signals[INSERT_PREFIX] =
    g_signal_new (I_("insert-prefix"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEntryCompletionClass, insert_prefix),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__STRING,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_STRING);

  /**
   * GtkEntryCompletion::match-selected:
   * @widget: the object which received the signal
   * @model: the `GtkTreeModel` containing the matches
   * @iter: a `GtkTreeIter` positioned at the selected match
   *
   * Emitted when a match from the list is selected.
   *
   * The default behaviour is to replace the contents of the
   * entry with the contents of the text column in the row
   * pointed to by @iter.
   *
   * Note that @model is the model that was passed to
   * [method@Gtk.EntryCompletion.set_model].
   *
   * Returns: %TRUE if the signal has been handled
   */
  entry_completion_signals[MATCH_SELECTED] =
    g_signal_new (I_("match-selected"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEntryCompletionClass, match_selected),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__OBJECT_BOXED,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_TREE_MODEL,
                  GTK_TYPE_TREE_ITER);

  /**
   * GtkEntryCompletion::cursor-on-match:
   * @widget: the object which received the signal
   * @model: the `GtkTreeModel` containing the matches
   * @iter: a `GtkTreeIter` positioned at the selected match
   *
   * Emitted when a match from the cursor is on a match of the list.
   *
   * The default behaviour is to replace the contents
   * of the entry with the contents of the text column in the row
   * pointed to by @iter.
   *
   * Note that @model is the model that was passed to
   * [method@Gtk.EntryCompletion.set_model].
   *
   * Returns: %TRUE if the signal has been handled
   */
  entry_completion_signals[CURSOR_ON_MATCH] =
    g_signal_new (I_("cursor-on-match"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEntryCompletionClass, cursor_on_match),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__OBJECT_BOXED,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_TREE_MODEL,
                  GTK_TYPE_TREE_ITER);

  /**
   * GtkEntryCompletion::no-matches:
   * @widget: the object which received the signal
   *
   * Emitted when the filter model has zero
   * number of rows in completion_complete method.
   *
   * In other words when `GtkEntryCompletion` is out of suggestions.
   */
  entry_completion_signals[NO_MATCHES] =
    g_signal_new (I_("no-matches"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkEntryCompletionClass, no_matches),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  entry_completion_props[PROP_MODEL] =
      g_param_spec_object ("model", NULL, NULL,
                           GTK_TYPE_TREE_MODEL,
                           GTK_PARAM_READWRITE);

  entry_completion_props[PROP_MINIMUM_KEY_LENGTH] =
      g_param_spec_int ("minimum-key-length", NULL, NULL,
                        0, G_MAXINT, 1,
                        GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntryCompletion:text-column: (attributes org.gtk.Property.get=gtk_entry_completion_get_text_column org.gtk.Property.set=gtk_entry_completion_set_text_column)
   *
   * The column of the model containing the strings.
   *
   * Note that the strings must be UTF-8.
   */
  entry_completion_props[PROP_TEXT_COLUMN] =
    g_param_spec_int ("text-column", NULL, NULL,
                      -1, G_MAXINT, -1,
                      GTK_PARAM_READWRITE);

  /**
   * GtkEntryCompletion:inline-completion: (attributes org.gtk.Property.get=gtk_entry_completion_get_inline_completion org.gtk.Property.set=gtk_entry_completion_set_inline_completion)
   *
   * Determines whether the common prefix of the possible completions
   * should be inserted automatically in the entry.
   *
   * Note that this requires text-column to be set, even if you are
   * using a custom match function.
   */
  entry_completion_props[PROP_INLINE_COMPLETION] =
      g_param_spec_boolean ("inline-completion", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntryCompletion:popup-completion: (attributes org.gtk.Property.get=gtk_entry_completion_get_popup_completion org.gtk.Property.set=gtk_entry_completion_set_popup_completion)
   *
   * Determines whether the possible completions should be
   * shown in a popup window.
   */
  entry_completion_props[PROP_POPUP_COMPLETION] =
      g_param_spec_boolean ("popup-completion", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntryCompletion:popup-set-width: (attributes org.gtk.Property.get=gtk_entry_completion_get_popup_set_width org.gtk.Property.set=gtk_entry_completion_set_popup_set_width)
   *
   * Determines whether the completions popup window will be
   * resized to the width of the entry.
   */
  entry_completion_props[PROP_POPUP_SET_WIDTH] =
      g_param_spec_boolean ("popup-set-width", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntryCompletion:popup-single-match: (attributes org.gtk.Property.get=gtk_entry_completion_get_popup_single_match org.gtk.Property.set=gtk_entry_completion_set_popup_single_match)
   *
   * Determines whether the completions popup window will shown
   * for a single possible completion.
   *
   * You probably want to set this to %FALSE if you are using
   * [property@Gtk.EntryCompletion:inline-completion].
   */
  entry_completion_props[PROP_POPUP_SINGLE_MATCH] =
      g_param_spec_boolean ("popup-single-match", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntryCompletion:inline-selection: (attributes org.gtk.Property.get=gtk_entry_completion_get_inline_selection org.gtk.Property.set=gtk_entry_completion_set_inline_selection)
   *
   * Determines whether the possible completions on the popup
   * will appear in the entry as you navigate through them.
   */
  entry_completion_props[PROP_INLINE_SELECTION] =
      g_param_spec_boolean ("inline-selection", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEntryCompletion:cell-area:
   *
   * The `GtkCellArea` used to layout cell renderers in the treeview column.
   *
   * If no area is specified when creating the entry completion with
   * [ctor@Gtk.EntryCompletion.new_with_area], a horizontally oriented
   * [class@Gtk.CellAreaBox] will be used.
   */
  entry_completion_props[PROP_CELL_AREA] =
      g_param_spec_object ("cell-area", NULL, NULL,
                           GTK_TYPE_CELL_AREA,
                           GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, entry_completion_props);
}


static void
gtk_entry_completion_buildable_custom_tag_end (GtkBuildable *buildable,
                                                GtkBuilder   *builder,
                                                GObject      *child,
                                                const char   *tagname,
                                                gpointer      data)
{
  /* Just ignore the boolean return from here */
  _gtk_cell_layout_buildable_custom_tag_end (buildable, builder, child, tagname, data);
}

static void
gtk_entry_completion_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = _gtk_cell_layout_buildable_add_child;
  iface->custom_tag_start = _gtk_cell_layout_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_entry_completion_buildable_custom_tag_end;
}

static void
gtk_entry_completion_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->get_area = gtk_entry_completion_get_area;
}

static void
gtk_entry_completion_init (GtkEntryCompletion *completion)
{
  completion->minimum_key_length = 1;
  completion->text_column = -1;
  completion->has_completion = FALSE;
  completion->inline_completion = FALSE;
  completion->popup_completion = TRUE;
  completion->popup_set_width = TRUE;
  completion->popup_single_match = TRUE;
  completion->inline_selection = FALSE;

  completion->filter_model = NULL;
  completion->insert_text_signal_group = NULL;
}

static gboolean
propagate_to_entry (GtkEventControllerKey *key,
                    guint                  keyval,
                    guint                  keycode,
                    GdkModifierType        modifiers,
                    GtkEntryCompletion    *completion)
{
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->entry));

  return gtk_event_controller_key_forward (key, GTK_WIDGET (text));
}

static void
gtk_entry_completion_constructed (GObject *object)
{
  GtkEntryCompletion        *completion = GTK_ENTRY_COMPLETION (object);
  GtkTreeSelection          *sel;
  GtkEventController        *controller;

  G_OBJECT_CLASS (gtk_entry_completion_parent_class)->constructed (object);

  if (!completion->cell_area)
    {
      completion->cell_area = gtk_cell_area_box_new ();
      g_object_ref_sink (completion->cell_area);
    }

  /* completions */
  completion->tree_view = gtk_tree_view_new ();
  g_signal_connect (completion->tree_view, "row-activated",
                    G_CALLBACK (gtk_entry_completion_list_activated),
                    completion);

  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (completion->tree_view), FALSE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (completion->tree_view), FALSE);
  gtk_tree_view_set_hover_selection (GTK_TREE_VIEW (completion->tree_view), TRUE);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (completion->tree_view), TRUE);

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->tree_view));
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);
  gtk_tree_selection_unselect_all (sel);
  g_signal_connect (sel, "changed",
                    G_CALLBACK (gtk_entry_completion_selection_changed),
                    completion);
  completion->first_sel_changed = TRUE;

  completion->column = gtk_tree_view_column_new_with_area (completion->cell_area);
  gtk_tree_view_append_column (GTK_TREE_VIEW (completion->tree_view), completion->column);

  completion->scrolled_window = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (completion->scrolled_window),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);

  /* a nasty hack to get the completions treeview to size nicely */
  gtk_widget_set_size_request (gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (completion->scrolled_window)),
                               -1, 0);

  /* pack it all */
  completion->popup_window = gtk_popover_new ();
  gtk_popover_set_position (GTK_POPOVER (completion->popup_window), GTK_POS_BOTTOM);
  gtk_popover_set_autohide (GTK_POPOVER (completion->popup_window), FALSE);
  gtk_popover_set_has_arrow (GTK_POPOVER (completion->popup_window), FALSE);
  gtk_widget_add_css_class (completion->popup_window, "entry-completion");

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (propagate_to_entry), completion);
  g_signal_connect (controller, "key-released",
                    G_CALLBACK (propagate_to_entry), completion);
  gtk_widget_add_controller (completion->popup_window, controller);

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  g_signal_connect_swapped (controller, "released",
                            G_CALLBACK (_gtk_entry_completion_popdown),
                            completion);
  gtk_widget_add_controller (completion->popup_window, controller);

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (completion->scrolled_window),
                                 completion->tree_view);
  gtk_widget_set_hexpand (completion->scrolled_window, TRUE);
  gtk_widget_set_vexpand (completion->scrolled_window, TRUE);
  gtk_popover_set_child (GTK_POPOVER (completion->popup_window), completion->scrolled_window);
}


static void
gtk_entry_completion_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (object);
  GtkCellArea *area;

  switch (prop_id)
    {
      case PROP_MODEL:
        gtk_entry_completion_set_model (completion,
                                        g_value_get_object (value));
        break;

      case PROP_MINIMUM_KEY_LENGTH:
        gtk_entry_completion_set_minimum_key_length (completion,
                                                     g_value_get_int (value));
        break;

      case PROP_TEXT_COLUMN:
        completion->text_column = g_value_get_int (value);
        break;

      case PROP_INLINE_COMPLETION:
	gtk_entry_completion_set_inline_completion (completion,
						    g_value_get_boolean (value));
        break;

      case PROP_POPUP_COMPLETION:
	gtk_entry_completion_set_popup_completion (completion,
						   g_value_get_boolean (value));
        break;

      case PROP_POPUP_SET_WIDTH:
	gtk_entry_completion_set_popup_set_width (completion,
						  g_value_get_boolean (value));
        break;

      case PROP_POPUP_SINGLE_MATCH:
	gtk_entry_completion_set_popup_single_match (completion,
						     g_value_get_boolean (value));
        break;

      case PROP_INLINE_SELECTION:
	gtk_entry_completion_set_inline_selection (completion,
						   g_value_get_boolean (value));
        break;

      case PROP_CELL_AREA:
        /* Construct-only, can only be assigned once */
        area = g_value_get_object (value);
        if (area)
          {
            if (completion->cell_area != NULL)
              {
                g_warning ("cell-area has already been set, ignoring construct property");
                g_object_ref_sink (area);
                g_object_unref (area);
              }
            else
              completion->cell_area = g_object_ref_sink (area);
          }
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_entry_completion_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (object);

  switch (prop_id)
    {
      case PROP_MODEL:
        g_value_set_object (value,
                            gtk_entry_completion_get_model (completion));
        break;

      case PROP_MINIMUM_KEY_LENGTH:
        g_value_set_int (value, gtk_entry_completion_get_minimum_key_length (completion));
        break;

      case PROP_TEXT_COLUMN:
        g_value_set_int (value, gtk_entry_completion_get_text_column (completion));
        break;

      case PROP_INLINE_COMPLETION:
        g_value_set_boolean (value, gtk_entry_completion_get_inline_completion (completion));
        break;

      case PROP_POPUP_COMPLETION:
        g_value_set_boolean (value, gtk_entry_completion_get_popup_completion (completion));
        break;

      case PROP_POPUP_SET_WIDTH:
        g_value_set_boolean (value, gtk_entry_completion_get_popup_set_width (completion));
        break;

      case PROP_POPUP_SINGLE_MATCH:
        g_value_set_boolean (value, gtk_entry_completion_get_popup_single_match (completion));
        break;

      case PROP_INLINE_SELECTION:
        g_value_set_boolean (value, gtk_entry_completion_get_inline_selection (completion));
        break;

      case PROP_CELL_AREA:
        g_value_set_object (value, completion->cell_area);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_entry_completion_finalize (GObject *object)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (object);

  g_free (completion->case_normalized_key);
  g_free (completion->completion_prefix);

  if (completion->match_notify)
    (* completion->match_notify) (completion->match_data);

  G_OBJECT_CLASS (gtk_entry_completion_parent_class)->finalize (object);
}

static void
gtk_entry_completion_dispose (GObject *object)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (object);

  if (completion->entry)
    gtk_entry_set_completion (GTK_ENTRY (completion->entry), NULL);

  g_clear_object (&completion->cell_area);

  G_OBJECT_CLASS (gtk_entry_completion_parent_class)->dispose (object);
}

/* implement cell layout interface (only need to return the underlying cell area) */
static GtkCellArea*
gtk_entry_completion_get_area (GtkCellLayout *cell_layout)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (cell_layout);

  if (G_UNLIKELY (!completion->cell_area))
    {
      completion->cell_area = gtk_cell_area_box_new ();
      g_object_ref_sink (completion->cell_area);
    }

  return completion->cell_area;
}

/* all those callbacks */
static gboolean
gtk_entry_completion_default_completion_func (GtkEntryCompletion *completion,
                                              const char         *key,
                                              GtkTreeIter        *iter,
                                              gpointer            user_data)
{
  char *item = NULL;
  char *normalized_string;
  char *case_normalized_string;

  gboolean ret = FALSE;

  GtkTreeModel *model;

  model = gtk_tree_model_filter_get_model (completion->filter_model);

  g_return_val_if_fail (gtk_tree_model_get_column_type (model, completion->text_column) == G_TYPE_STRING,
                        FALSE);

  gtk_tree_model_get (model, iter,
                      completion->text_column, &item,
                      -1);

  if (item != NULL)
    {
      normalized_string = g_utf8_normalize (item, -1, G_NORMALIZE_ALL);

      if (normalized_string != NULL)
        {
          case_normalized_string = g_utf8_casefold (normalized_string, -1);

          if (!strncmp (key, case_normalized_string, strlen (key)))
            ret = TRUE;

          g_free (case_normalized_string);
        }
      g_free (normalized_string);
    }
  g_free (item);

  return ret;
}

static gboolean
gtk_entry_completion_visible_func (GtkTreeModel *model,
                                   GtkTreeIter  *iter,
                                   gpointer      data)
{
  gboolean ret = FALSE;

  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (data);

  if (!completion->case_normalized_key)
    return ret;

  if (completion->match_func)
    ret = (* completion->match_func) (completion,
                                            completion->case_normalized_key,
                                            iter,
                                            completion->match_data);
  else if (completion->text_column >= 0)
    ret = gtk_entry_completion_default_completion_func (completion,
                                                        completion->case_normalized_key,
                                                        iter,
                                                        NULL);

  return ret;
}

static void
gtk_entry_completion_list_activated (GtkTreeView       *treeview,
                                     GtkTreePath       *path,
                                     GtkTreeViewColumn *column,
                                     gpointer           user_data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);
  GtkTreeIter iter;
  gboolean entry_set;
  GtkTreeModel *model;
  GtkTreeIter child_iter;
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->entry));

  gtk_tree_model_get_iter (GTK_TREE_MODEL (completion->filter_model), &iter, path);
  gtk_tree_model_filter_convert_iter_to_child_iter (completion->filter_model,
                                                    &child_iter,
                                                    &iter);
  model = gtk_tree_model_filter_get_model (completion->filter_model);

  g_signal_handler_block (text, completion->changed_id);
  g_signal_emit (completion, entry_completion_signals[MATCH_SELECTED],
                 0, model, &child_iter, &entry_set);
  g_signal_handler_unblock (text, completion->changed_id);

  _gtk_entry_completion_popdown (completion);
}

static void
gtk_entry_completion_selection_changed (GtkTreeSelection *selection,
                                        gpointer          data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (data);

  if (completion->first_sel_changed)
    {
      completion->first_sel_changed = FALSE;
      if (gtk_widget_is_focus (completion->tree_view))
        gtk_tree_selection_unselect_all (selection);
    }
}

/* public API */

/**
 * gtk_entry_completion_new:
 *
 * Creates a new `GtkEntryCompletion` object.
 *
 * Returns: A newly created `GtkEntryCompletion` object
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
GtkEntryCompletion *
gtk_entry_completion_new (void)
{
  GtkEntryCompletion *completion;

  completion = g_object_new (GTK_TYPE_ENTRY_COMPLETION, NULL);

  return completion;
}

/**
 * gtk_entry_completion_new_with_area:
 * @area: the `GtkCellArea` used to layout cells
 *
 * Creates a new `GtkEntryCompletion` object using the
 * specified @area.
 *
 * The `GtkCellArea` is used to layout cells in the underlying
 * `GtkTreeViewColumn` for the drop-down menu.
 *
 * Returns: A newly created `GtkEntryCompletion` object
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
GtkEntryCompletion *
gtk_entry_completion_new_with_area (GtkCellArea *area)
{
  GtkEntryCompletion *completion;

  completion = g_object_new (GTK_TYPE_ENTRY_COMPLETION, "cell-area", area, NULL);

  return completion;
}

/**
 * gtk_entry_completion_get_entry:
 * @completion: a `GtkEntryCompletion`
 *
 * Gets the entry @completion has been attached to.
 *
 * Returns: (transfer none): The entry @completion has been attached to
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
GtkWidget *
gtk_entry_completion_get_entry (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), NULL);

  return completion->entry;
}

/**
 * gtk_entry_completion_set_model: (attributes org.gtk.Method.set_property=model)
 * @completion: a `GtkEntryCompletion`
 * @model: (nullable): the `GtkTreeModel`
 *
 * Sets the model for a `GtkEntryCompletion`.
 *
 * If @completion already has a model set, it will remove it
 * before setting the new model. If model is %NULL, then it
 * will unset the model.
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
void
gtk_entry_completion_set_model (GtkEntryCompletion *completion,
                                GtkTreeModel       *model)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (model == NULL || GTK_IS_TREE_MODEL (model));

  if (!model)
    {
      gtk_tree_view_set_model (GTK_TREE_VIEW (completion->tree_view),
                               NULL);
      _gtk_entry_completion_popdown (completion);
      completion->filter_model = NULL;
      return;
    }

  /* code will unref the old filter model (if any) */
  completion->filter_model =
    GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (model, NULL));
  gtk_tree_model_filter_set_visible_func (completion->filter_model,
                                          gtk_entry_completion_visible_func,
                                          completion,
                                          NULL);

  gtk_tree_view_set_model (GTK_TREE_VIEW (completion->tree_view),
                           GTK_TREE_MODEL (completion->filter_model));
  g_object_unref (completion->filter_model);

  g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_MODEL]);

  if (gtk_widget_get_visible (completion->popup_window))
    _gtk_entry_completion_resize_popup (completion);
}

/**
 * gtk_entry_completion_get_model: (attributes org.gtk.Method.get_property=model)
 * @completion: a `GtkEntryCompletion`
 *
 * Returns the model the `GtkEntryCompletion` is using as data source.
 *
 * Returns %NULL if the model is unset.
 *
 * Returns: (nullable) (transfer none): A `GtkTreeModel`
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
GtkTreeModel *
gtk_entry_completion_get_model (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), NULL);

  if (!completion->filter_model)
    return NULL;

  return gtk_tree_model_filter_get_model (completion->filter_model);
}

/**
 * gtk_entry_completion_set_match_func:
 * @completion: a `GtkEntryCompletion`
 * @func: the `GtkEntryCompletion`MatchFunc to use
 * @func_data: user data for @func
 * @func_notify: destroy notify for @func_data.
 *
 * Sets the match function for @completion to be @func.
 *
 * The match function is used to determine if a row should or
 * should not be in the completion list.
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
void
gtk_entry_completion_set_match_func (GtkEntryCompletion          *completion,
                                     GtkEntryCompletionMatchFunc  func,
                                     gpointer                     func_data,
                                     GDestroyNotify               func_notify)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));

  if (completion->match_notify)
    (* completion->match_notify) (completion->match_data);

  completion->match_func = func;
  completion->match_data = func_data;
  completion->match_notify = func_notify;
}

/**
 * gtk_entry_completion_set_minimum_key_length:
 * @completion: a `GtkEntryCompletion`
 * @length: the minimum length of the key in order to start completing
 *
 * Requires the length of the search key for @completion to be at least
 * @length.
 *
 * This is useful for long lists, where completing using a small
 * key takes a lot of time and will come up with meaningless results anyway
 * (ie, a too large dataset).
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
void
gtk_entry_completion_set_minimum_key_length (GtkEntryCompletion *completion,
                                             int                 length)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (length >= 0);

  if (completion->minimum_key_length != length)
    {
      completion->minimum_key_length = length;

      g_object_notify_by_pspec (G_OBJECT (completion),
                                entry_completion_props[PROP_MINIMUM_KEY_LENGTH]);
    }
}

/**
 * gtk_entry_completion_get_minimum_key_length:
 * @completion: a `GtkEntryCompletion`
 *
 * Returns the minimum key length as set for @completion.
 *
 * Returns: The currently used minimum key length
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
int
gtk_entry_completion_get_minimum_key_length (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), 0);

  return completion->minimum_key_length;
}

/**
 * gtk_entry_completion_complete:
 * @completion: a `GtkEntryCompletion`
 *
 * Requests a completion operation, or in other words a refiltering of the
 * current list with completions, using the current key.
 *
 * The completion list view will be updated accordingly.
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
void
gtk_entry_completion_complete (GtkEntryCompletion *completion)
{
  char *tmp;
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (GTK_IS_ENTRY (completion->entry));

  if (!completion->filter_model)
    return;

  g_free (completion->case_normalized_key);

  tmp = g_utf8_normalize (gtk_editable_get_text (GTK_EDITABLE (completion->entry)),
                          -1, G_NORMALIZE_ALL);
  completion->case_normalized_key = g_utf8_casefold (tmp, -1);
  g_free (tmp);

  gtk_tree_model_filter_refilter (completion->filter_model);

  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (completion->filter_model), &iter))
    g_signal_emit (completion, entry_completion_signals[NO_MATCHES], 0);

  if (gtk_widget_get_visible (completion->popup_window))
    _gtk_entry_completion_resize_popup (completion);
}

/**
 * gtk_entry_completion_set_text_column: (attributes org.gtk.Method.set_property=text-column)
 * @completion: a `GtkEntryCompletion`
 * @column: the column in the model of @completion to get strings from
 *
 * Convenience function for setting up the most used case of this code: a
 * completion list with just strings.
 *
 * This function will set up @completion
 * to have a list displaying all (and just) strings in the completion list,
 * and to get those strings from @column in the model of @completion.
 *
 * This functions creates and adds a `GtkCellRendererText` for the selected
 * column. If you need to set the text column, but don't want the cell
 * renderer, use g_object_set() to set the
 * [property@Gtk.EntryCompletion:text-column] property directly.
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
void
gtk_entry_completion_set_text_column (GtkEntryCompletion *completion,
                                      int                 column)
{
  GtkCellRenderer *cell;

  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));
  g_return_if_fail (column >= 0);

  if (completion->text_column == column)
    return;

  completion->text_column = column;

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion),
                              cell, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (completion),
                                 cell,
                                 "text", column);

  g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_TEXT_COLUMN]);
}

/**
 * gtk_entry_completion_get_text_column: (attributes org.gtk.Method.get_property=text-column)
 * @completion: a `GtkEntryCompletion`
 *
 * Returns the column in the model of @completion to get strings from.
 *
 * Returns: the column containing the strings
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
int
gtk_entry_completion_get_text_column (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), -1);

  return completion->text_column;
}

/* private */

/* some nasty size requisition */
void
_gtk_entry_completion_resize_popup (GtkEntryCompletion *completion)
{
  graphene_rect_t bounds;
  int matches, items, height;
  GdkSurface *surface;
  GtkRequisition entry_req;
  GtkRequisition tree_req;
  int width;

  surface = gtk_native_get_surface (gtk_widget_get_native (completion->entry));

  if (!surface)
    return;

  if (!completion->filter_model)
    return;

  if (!gtk_widget_compute_bounds (completion->entry,
                                  GTK_WIDGET (gtk_widget_get_native (completion->entry)),
                                  &bounds))
    graphene_rect_init (&bounds, 0, 0, 0, 0);

  gtk_widget_get_preferred_size (completion->entry, &entry_req, NULL);

  matches = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (completion->filter_model), NULL);

  /* Call get preferred size on the on the tree view to force it to validate its
   * cells before calling into the cell size functions.
   */
  gtk_widget_get_preferred_size (completion->tree_view, &tree_req, NULL);
  gtk_tree_view_column_cell_get_size (completion->column, NULL, NULL, NULL, &height);

  gtk_widget_realize (completion->tree_view);

  items = MIN (matches, 10);

  if (items <= 0)
    gtk_widget_hide (completion->scrolled_window);
  else
    gtk_widget_show (completion->scrolled_window);

  if (completion->popup_set_width)
    width = ceilf (bounds.size.width);
  else
    width = -1;

  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (completion->tree_view));
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (completion->scrolled_window), width);
  gtk_widget_set_size_request (completion->popup_window, width, -1);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (completion->scrolled_window), items * height);

  gtk_popover_present (GTK_POPOVER (completion->popup_window));
}

static void
gtk_entry_completion_popup (GtkEntryCompletion *completion)
{
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->entry));

  if (gtk_widget_get_mapped (completion->popup_window))
    return;

  if (!gtk_widget_get_mapped (GTK_WIDGET (text)))
    return;

  if (!gtk_widget_has_focus (GTK_WIDGET (text)))
    return;

  /* default on no match */
  completion->current_selected = -1;

  gtk_widget_realize (completion->popup_window);

  _gtk_entry_completion_resize_popup (completion);

  if (completion->filter_model)
    {
      GtkTreePath *path;

      path = gtk_tree_path_new_from_indices (0, -1);
      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (completion->tree_view), path,
                                    NULL, FALSE, 0.0, 0.0);
      gtk_tree_path_free (path);
    }

  gtk_popover_popup (GTK_POPOVER (completion->popup_window));
}

void
_gtk_entry_completion_popdown (GtkEntryCompletion *completion)
{
  if (!gtk_widget_get_mapped (completion->popup_window))
    return;

  gtk_popover_popdown (GTK_POPOVER (completion->popup_window));
}

static gboolean
gtk_entry_completion_match_selected (GtkEntryCompletion *completion,
                                     GtkTreeModel       *model,
                                     GtkTreeIter        *iter)
{
  g_assert (completion->entry != NULL);

  char *str = NULL;

  gtk_tree_model_get (model, iter, completion->text_column, &str, -1);
  gtk_editable_set_text (GTK_EDITABLE (completion->entry), str ? str : "");

  /* move cursor to the end */
  gtk_editable_set_position (GTK_EDITABLE (completion->entry), -1);

  g_free (str);

  return TRUE;
}

static gboolean
gtk_entry_completion_cursor_on_match (GtkEntryCompletion *completion,
                                      GtkTreeModel       *model,
                                      GtkTreeIter        *iter)
{
  g_assert (completion->entry != NULL);

  gtk_entry_completion_insert_completion (completion, model, iter);

  return TRUE;
}

/**
 * gtk_entry_completion_compute_prefix:
 * @completion: the entry completion
 * @key: The text to complete for
 *
 * Computes the common prefix that is shared by all rows in @completion
 * that start with @key.
 *
 * If no row matches @key, %NULL will be returned.
 * Note that a text column must have been set for this function to work,
 * see [method@Gtk.EntryCompletion.set_text_column] for details.
 *
 * Returns: (nullable) (transfer full): The common prefix all rows
 *   starting with @key
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
char *
gtk_entry_completion_compute_prefix (GtkEntryCompletion *completion,
                                     const char         *key)
{
  GtkTreeIter iter;
  char *prefix = NULL;
  gboolean valid;

  if (completion->text_column < 0)
    return NULL;

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (completion->filter_model),
                                         &iter);

  while (valid)
    {
      char *text;

      gtk_tree_model_get (GTK_TREE_MODEL (completion->filter_model),
                          &iter, completion->text_column, &text,
                          -1);

      if (text && g_str_has_prefix (text, key))
        {
          if (!prefix)
            prefix = g_strdup (text);
          else
            {
              char *p = prefix;
              char *q = text;

              while (*p && *p == *q)
                {
                  p++;
                  q++;
                }

              *p = '\0';

              if (p > prefix)
                {
                  /* strip a partial multibyte character */
                  q = g_utf8_find_prev_char (prefix, p);
                  switch (g_utf8_get_char_validated (q, p - q))
                    {
                    case (gunichar)-2:
                    case (gunichar)-1:
                      *q = 0;
                      break;
                    default: ;
                    }
                }
            }
        }

      g_free (text);
      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (completion->filter_model),
                                        &iter);
    }

  return prefix;
}


static gboolean
gtk_entry_completion_real_insert_prefix (GtkEntryCompletion *completion,
                                         const char         *prefix)
{
  g_assert (completion->entry != NULL);

  if (prefix)
    {
      int key_len;
      int prefix_len;
      const char *key;

      prefix_len = g_utf8_strlen (prefix, -1);

      key = gtk_editable_get_text (GTK_EDITABLE (completion->entry));
      key_len = g_utf8_strlen (key, -1);

      if (prefix_len > key_len)
        {
          int pos = prefix_len;

          gtk_editable_insert_text (GTK_EDITABLE (completion->entry),
                                    prefix + strlen (key), -1, &pos);
          gtk_editable_select_region (GTK_EDITABLE (completion->entry),
                                      key_len, prefix_len);

          completion->has_completion = TRUE;
        }
    }

  return TRUE;
}

/**
 * gtk_entry_completion_get_completion_prefix:
 * @completion: a `GtkEntryCompletion`
 *
 * Get the original text entered by the user that triggered
 * the completion or %NULL if there’s no completion ongoing.
 *
 * Returns: (nullable): the prefix for the current completion
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
const char *
gtk_entry_completion_get_completion_prefix (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), NULL);

  return completion->completion_prefix;
}

static void
gtk_entry_completion_insert_completion_text (GtkEntryCompletion *completion,
                                             const char         *new_text)
{
  int len;
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->entry));

  if (completion->changed_id > 0)
    g_signal_handler_block (text, completion->changed_id);

  if (completion->insert_text_signal_group != NULL)
    g_signal_group_block (completion->insert_text_signal_group);

  gtk_editable_set_text (GTK_EDITABLE (completion->entry), new_text);

  len = g_utf8_strlen (completion->completion_prefix, -1);
  gtk_editable_select_region (GTK_EDITABLE (completion->entry), len, -1);

  if (completion->changed_id > 0)
    g_signal_handler_unblock (text, completion->changed_id);

  if (completion->insert_text_signal_group != NULL)
    g_signal_group_unblock (completion->insert_text_signal_group);
}

static gboolean
gtk_entry_completion_insert_completion (GtkEntryCompletion *completion,
                                        GtkTreeModel       *model,
                                        GtkTreeIter        *iter)
{
  char *str = NULL;

  if (completion->text_column < 0)
    return FALSE;

  gtk_tree_model_get (model, iter,
                      completion->text_column, &str,
                      -1);

  gtk_entry_completion_insert_completion_text (completion, str);

  g_free (str);

  return TRUE;
}

/**
 * gtk_entry_completion_insert_prefix:
 * @completion: a `GtkEntryCompletion`
 *
 * Requests a prefix insertion.
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
void
gtk_entry_completion_insert_prefix (GtkEntryCompletion *completion)
{
  g_return_if_fail (completion->entry != NULL);

  gboolean done;
  char *prefix;

  if (completion->insert_text_signal_group != NULL)
    g_signal_group_block (completion->insert_text_signal_group);

  prefix = gtk_entry_completion_compute_prefix (completion,
                                                gtk_editable_get_text (GTK_EDITABLE (completion->entry)));

  if (prefix)
    {
      g_signal_emit (completion, entry_completion_signals[INSERT_PREFIX],
                     0, prefix, &done);
      g_free (prefix);
    }

  if (completion->insert_text_signal_group != NULL)
    g_signal_group_unblock (completion->insert_text_signal_group);
}

/**
 * gtk_entry_completion_set_inline_completion: (attributes org.gtk.Method.set_property=inline-completion)
 * @completion: a `GtkEntryCompletion`
 * @inline_completion: %TRUE to do inline completion
 *
 * Sets whether the common prefix of the possible completions should
 * be automatically inserted in the entry.
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
void
gtk_entry_completion_set_inline_completion (GtkEntryCompletion *completion,
                                            gboolean            inline_completion)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));

  inline_completion = inline_completion != FALSE;

  if (completion->inline_completion != inline_completion)
    {
      completion->inline_completion = inline_completion;

      g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_INLINE_COMPLETION]);
    }
}

/**
 * gtk_entry_completion_get_inline_completion: (attributes org.gtk.Method.get_property=inline-completion)
 * @completion: a `GtkEntryCompletion`
 *
 * Returns whether the common prefix of the possible completions should
 * be automatically inserted in the entry.
 *
 * Returns: %TRUE if inline completion is turned on
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
gboolean
gtk_entry_completion_get_inline_completion (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), FALSE);

  return completion->inline_completion;
}

/**
 * gtk_entry_completion_set_popup_completion: (attributes org.gtk.Method.set_property=popup-completion)
 * @completion: a `GtkEntryCompletion`
 * @popup_completion: %TRUE to do popup completion
 *
 * Sets whether the completions should be presented in a popup window.
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
void
gtk_entry_completion_set_popup_completion (GtkEntryCompletion *completion,
                                           gboolean            popup_completion)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));

  popup_completion = popup_completion != FALSE;

  if (completion->popup_completion != popup_completion)
    {
      completion->popup_completion = popup_completion;

      g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_POPUP_COMPLETION]);
    }
}


/**
 * gtk_entry_completion_get_popup_completion: (attributes org.gtk.Method.get_property=popup-completion)
 * @completion: a `GtkEntryCompletion`
 *
 * Returns whether the completions should be presented in a popup window.
 *
 * Returns: %TRUE if popup completion is turned on
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
gboolean
gtk_entry_completion_get_popup_completion (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), TRUE);

  return completion->popup_completion;
}

/**
 * gtk_entry_completion_set_popup_set_width: (attributes org.gtk.Method.set_property=popup-set-width)
 * @completion: a `GtkEntryCompletion`
 * @popup_set_width: %TRUE to make the width of the popup the same as the entry
 *
 * Sets whether the completion popup window will be resized to be the same
 * width as the entry.
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
void
gtk_entry_completion_set_popup_set_width (GtkEntryCompletion *completion,
                                          gboolean            popup_set_width)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));

  popup_set_width = popup_set_width != FALSE;

  if (completion->popup_set_width != popup_set_width)
    {
      completion->popup_set_width = popup_set_width;

      g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_POPUP_SET_WIDTH]);
    }
}

/**
 * gtk_entry_completion_get_popup_set_width: (attributes org.gtk.Method.get_property=popup-set-width)
 * @completion: a `GtkEntryCompletion`
 *
 * Returns whether the completion popup window will be resized to the
 * width of the entry.
 *
 * Returns: %TRUE if the popup window will be resized to the width of
 *   the entry
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
gboolean
gtk_entry_completion_get_popup_set_width (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), TRUE);

  return completion->popup_set_width;
}


/**
 * gtk_entry_completion_set_popup_single_match: (attributes org.gtk.Method.set_property=popup-single-match)
 * @completion: a `GtkEntryCompletion`
 * @popup_single_match: %TRUE if the popup should appear even for a single match
 *
 * Sets whether the completion popup window will appear even if there is
 * only a single match.
 *
 * You may want to set this to %FALSE if you
 * are using [property@Gtk.EntryCompletion:inline-completion].
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
void
gtk_entry_completion_set_popup_single_match (GtkEntryCompletion *completion,
                                             gboolean            popup_single_match)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));

  popup_single_match = popup_single_match != FALSE;

  if (completion->popup_single_match != popup_single_match)
    {
      completion->popup_single_match = popup_single_match;

      g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_POPUP_SINGLE_MATCH]);
    }
}

/**
 * gtk_entry_completion_get_popup_single_match: (attributes org.gtk.Method.get_property=popup-single-match)
 * @completion: a `GtkEntryCompletion`
 *
 * Returns whether the completion popup window will appear even if there is
 * only a single match.
 *
 * Returns: %TRUE if the popup window will appear regardless of the
 *    number of matches
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
gboolean
gtk_entry_completion_get_popup_single_match (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), TRUE);

  return completion->popup_single_match;
}

/**
 * gtk_entry_completion_set_inline_selection: (attributes org.gtk.Method.set_property=inline-selection)
 * @completion: a `GtkEntryCompletion`
 * @inline_selection: %TRUE to do inline selection
 *
 * Sets whether it is possible to cycle through the possible completions
 * inside the entry.
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
void
gtk_entry_completion_set_inline_selection (GtkEntryCompletion *completion,
                                           gboolean inline_selection)
{
  g_return_if_fail (GTK_IS_ENTRY_COMPLETION (completion));

  inline_selection = inline_selection != FALSE;

  if (completion->inline_selection != inline_selection)
    {
      completion->inline_selection = inline_selection;

      g_object_notify_by_pspec (G_OBJECT (completion), entry_completion_props[PROP_INLINE_SELECTION]);
    }
}

/**
 * gtk_entry_completion_get_inline_selection: (attributes org.gtk.Method.get_property=inline-selection)
 * @completion: a `GtkEntryCompletion`
 *
 * Returns %TRUE if inline-selection mode is turned on.
 *
 * Returns: %TRUE if inline-selection mode is on
 *
 * Deprecated: 4.10: GtkEntryCompletion will be removed in GTK 5.
 */
gboolean
gtk_entry_completion_get_inline_selection (GtkEntryCompletion *completion)
{
  g_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), FALSE);

  return completion->inline_selection;
}


static int
gtk_entry_completion_timeout (gpointer data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (data);

  completion->completion_timeout = 0;

  if (completion->filter_model &&
      g_utf8_strlen (gtk_editable_get_text (GTK_EDITABLE (completion->entry)), -1)
      >= completion->minimum_key_length)
    {
      int matches;
      gboolean popup_single;

      gtk_entry_completion_complete (completion);
      matches = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (completion->filter_model), NULL);
      gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->tree_view)));

      g_object_get (completion, "popup-single-match", &popup_single, NULL);
      if (matches > (popup_single ? 0: 1))
        {
          if (gtk_widget_get_visible (completion->popup_window))
            _gtk_entry_completion_resize_popup (completion);
          else
            gtk_entry_completion_popup (completion);
        }
      else
        _gtk_entry_completion_popdown (completion);
    }
  else if (gtk_widget_get_visible (completion->popup_window))
    _gtk_entry_completion_popdown (completion);
  return G_SOURCE_REMOVE;
}

static inline gboolean
keyval_is_cursor_move (guint keyval)
{
  if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up)
    return TRUE;

  if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down)
    return TRUE;

  if (keyval == GDK_KEY_Page_Up)
    return TRUE;

  if (keyval == GDK_KEY_Page_Down)
    return TRUE;

  return FALSE;
}

static gboolean
gtk_entry_completion_key_pressed (GtkEventControllerKey *controller,
                                  guint                  keyval,
                                  guint                  keycode,
                                  GdkModifierType        state,
                                  gpointer               user_data)
{
  int matches;
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);
  GtkWidget *widget = completion->entry;
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (widget));

  if (!completion->popup_completion)
    return FALSE;

  if (keyval == GDK_KEY_Return ||
      keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_ISO_Enter ||
      keyval == GDK_KEY_Escape)
    {
      if (completion->completion_timeout)
        {
          g_source_remove (completion->completion_timeout);
          completion->completion_timeout = 0;
        }
    }

  if (!gtk_widget_get_mapped (completion->popup_window))
    return FALSE;

  matches = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (completion->filter_model), NULL);

  if (keyval_is_cursor_move (keyval))
    {
      GtkTreePath *path = NULL;

      if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up)
        {
          if (completion->current_selected < 0)
            completion->current_selected = matches - 1;
          else
            completion->current_selected--;
        }
      else if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down)
        {
          if (completion->current_selected < matches - 1)
            completion->current_selected++;
          else
            completion->current_selected = -1;
        }
      else if (keyval == GDK_KEY_Page_Up)
        {
          if (completion->current_selected < 0)
            completion->current_selected = matches - 1;
          else if (completion->current_selected == 0)
            completion->current_selected = -1;
          else if (completion->current_selected < matches)
            {
              completion->current_selected -= PAGE_STEP;
              if (completion->current_selected < 0)
                completion->current_selected = 0;
            }
          else
            {
              completion->current_selected -= PAGE_STEP;
              if (completion->current_selected < matches - 1)
                completion->current_selected = matches - 1;
            }
        }
      else if (keyval == GDK_KEY_Page_Down)
        {
          if (completion->current_selected < 0)
            completion->current_selected = 0;
          else if (completion->current_selected < matches - 1)
            {
              completion->current_selected += PAGE_STEP;
              if (completion->current_selected > matches - 1)
                completion->current_selected = matches - 1;
            }
          else if (completion->current_selected == matches - 1)
            {
              completion->current_selected = -1;
            }
          else
            {
              completion->current_selected += PAGE_STEP;
              if (completion->current_selected > matches - 1)
                completion->current_selected = matches - 1;
            }
        }

      if (completion->current_selected < 0)
        {
          gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->tree_view)));

          if (completion->inline_selection &&
              completion->completion_prefix)
            {
              gtk_editable_set_text (GTK_EDITABLE (completion->entry),
                                     completion->completion_prefix);
              gtk_editable_set_position (GTK_EDITABLE (widget), -1);
            }
        }
      else if (completion->current_selected < matches)
        {
          path = gtk_tree_path_new_from_indices (completion->current_selected, -1);
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (completion->tree_view),
                                    path, NULL, FALSE);

          if (completion->inline_selection)
            {

              GtkTreeIter iter;
              GtkTreeIter child_iter;
              GtkTreeModel *model = NULL;
              GtkTreeSelection *sel;
              gboolean entry_set;

              sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->tree_view));
              if (!gtk_tree_selection_get_selected (sel, &model, &iter))
                return FALSE;
             gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, &iter);
              model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));

              if (completion->completion_prefix == NULL)
                completion->completion_prefix = g_strdup (gtk_editable_get_text (GTK_EDITABLE (completion->entry)));

              g_signal_emit_by_name (completion, "cursor-on-match", model,
                                     &child_iter, &entry_set);
            }
        }

      gtk_tree_path_free (path);

      return TRUE;
    }
  else if (keyval == GDK_KEY_Escape ||
           keyval == GDK_KEY_Left ||
           keyval == GDK_KEY_KP_Left ||
           keyval == GDK_KEY_Right ||
           keyval == GDK_KEY_KP_Right)
    {
      gboolean retval = TRUE;

      gtk_entry_reset_im_context (GTK_ENTRY (widget));
      _gtk_entry_completion_popdown (completion);

      if (completion->current_selected < 0)
        {
          retval = FALSE;
          goto keypress_completion_out;
        }
      else if (completion->inline_selection)
        {
          /* Escape rejects the tentative completion */
          if (keyval == GDK_KEY_Escape)
            {
              if (completion->completion_prefix)
                gtk_editable_set_text (GTK_EDITABLE (completion->entry),
                                       completion->completion_prefix);
              else
                gtk_editable_set_text (GTK_EDITABLE (completion->entry), "");
            }

          /* Move the cursor to the end for Right/Esc */
          if (keyval == GDK_KEY_Right ||
              keyval == GDK_KEY_KP_Right ||
              keyval == GDK_KEY_Escape)
            gtk_editable_set_position (GTK_EDITABLE (widget), -1);
          /* Let the default keybindings run for Left, i.e. either move to the
 *            * previous character or select word if a modifier is used */
          else
            retval = FALSE;
        }

keypress_completion_out:
      if (completion->inline_selection)
        g_clear_pointer (&completion->completion_prefix, g_free);

      return retval;
    }
  else if (keyval == GDK_KEY_Tab ||
           keyval == GDK_KEY_KP_Tab ||
           keyval == GDK_KEY_ISO_Left_Tab)
    {
      gtk_entry_reset_im_context (GTK_ENTRY (widget));
      _gtk_entry_completion_popdown (completion);

      g_clear_pointer (&completion->completion_prefix, g_free);

      return FALSE;
    }
  else if (keyval == GDK_KEY_ISO_Enter ||
           keyval == GDK_KEY_KP_Enter ||
           keyval == GDK_KEY_Return)
    {
      GtkTreeIter iter;
      GtkTreeModel *model = NULL;
      GtkTreeModel *child_model;
      GtkTreeIter child_iter;
      GtkTreeSelection *sel;
      gboolean retval = TRUE;

      gtk_entry_reset_im_context (GTK_ENTRY (widget));
      _gtk_entry_completion_popdown (completion);

      if (completion->current_selected < matches)
        {
          gboolean entry_set;

          sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->tree_view));
          if (gtk_tree_selection_get_selected (sel, &model, &iter))
            {
              gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, &iter);
              child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
              g_signal_handler_block (text, completion->changed_id);
              g_signal_emit_by_name (completion, "match-selected",
                                     child_model, &child_iter, &entry_set);
              g_signal_handler_unblock (text, completion->changed_id);

              if (!entry_set)
                {
                  char *str = NULL;

                  gtk_tree_model_get (model, &iter,
                                      completion->text_column, &str,
                                      -1);

                  gtk_editable_set_text (GTK_EDITABLE (widget), str);

                  /* move the cursor to the end */
                  gtk_editable_set_position (GTK_EDITABLE (widget), -1);
                  g_free (str);
                }
            }
          else
            retval = FALSE;
        }

      g_clear_pointer (&completion->completion_prefix, g_free);

      return retval;
    }

  g_clear_pointer (&completion->completion_prefix, g_free);

  return FALSE;
}

static void
gtk_entry_completion_changed (GtkWidget *widget,
                              gpointer   user_data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);

  if (!completion->popup_completion)
    return;

  /* (re)install completion timeout */
  if (completion->completion_timeout)
    {
      g_source_remove (completion->completion_timeout);
      completion->completion_timeout = 0;
    }

  if (!gtk_editable_get_text (GTK_EDITABLE (widget)))
    return;

  /* no need to normalize for this test */
  if (completion->minimum_key_length > 0 &&
      strcmp ("", gtk_editable_get_text (GTK_EDITABLE (widget))) == 0)
    {
      if (gtk_widget_get_visible (completion->popup_window))
        _gtk_entry_completion_popdown (completion);
      return;
    }

  completion->completion_timeout =
    g_timeout_add (COMPLETION_TIMEOUT,
                   gtk_entry_completion_timeout,
                   completion);
  gdk_source_set_static_name_by_id (completion->completion_timeout, "[gtk] gtk_entry_completion_timeout");
}

static gboolean
check_completion_callback (GtkEntryCompletion *completion)
{
  completion->check_completion_idle = NULL;

  gtk_entry_completion_complete (completion);
  gtk_entry_completion_insert_prefix (completion);

  return FALSE;
}

static void
clear_completion_callback (GObject            *text,
                           GParamSpec         *pspec,
                           GtkEntryCompletion *completion)
{
  if (!completion->inline_completion)
    return;

  if (pspec->name == I_("cursor-position") ||
      pspec->name == I_("selection-bound"))
    completion->has_completion = FALSE;
}

static gboolean
accept_completion_callback (GtkEntryCompletion *completion)
{
  if (!completion->inline_completion)
    return FALSE;

  if (completion->has_completion)
    gtk_editable_set_position (GTK_EDITABLE (completion->entry),
                               gtk_entry_buffer_get_length (gtk_entry_get_buffer (GTK_ENTRY (completion->entry))));

  return FALSE;
}

static void
text_focus_out (GtkEntryCompletion *completion)
{
  if (!gtk_widget_get_mapped (completion->popup_window))
    accept_completion_callback (completion);
}

static void
completion_inserted_text_callback (GtkEntryBuffer     *buffer,
                                   guint               position,
                                   const char         *text,
                                   guint               length,
                                   GtkEntryCompletion *completion)
{
  if (!completion->inline_completion)
    return;

  /* idle to update the selection based on the file list */
  if (completion->check_completion_idle == NULL)
    {
      completion->check_completion_idle = g_idle_source_new ();
      g_source_set_priority (completion->check_completion_idle, G_PRIORITY_HIGH);
      g_source_set_closure (completion->check_completion_idle,
                            g_cclosure_new_object (G_CALLBACK (check_completion_callback),
                                                   G_OBJECT (completion)));
      g_source_attach (completion->check_completion_idle, NULL);
      g_source_set_static_name (completion->check_completion_idle, "[gtk] check_completion_callback");
    }
}

static void
connect_completion_signals (GtkEntryCompletion *completion)
{
  GtkEventController *controller;
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->entry));

  controller = completion->entry_key_controller = gtk_event_controller_key_new ();
  gtk_event_controller_set_static_name (controller, "gtk-entry-completion");
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (gtk_entry_completion_key_pressed), completion);
  gtk_widget_add_controller (GTK_WIDGET (text), controller);
  controller = completion->entry_focus_controller = gtk_event_controller_focus_new ();
  gtk_event_controller_set_static_name (controller, "gtk-entry-completion");
  g_signal_connect_swapped (controller, "leave", G_CALLBACK (text_focus_out), completion);
  gtk_widget_add_controller (GTK_WIDGET (text), controller);

  completion->changed_id =
    g_signal_connect (text, "changed", G_CALLBACK (gtk_entry_completion_changed), completion);

  completion->insert_text_signal_group = g_signal_group_new (GTK_TYPE_ENTRY_BUFFER);
  g_signal_group_connect (completion->insert_text_signal_group, "inserted-text", G_CALLBACK (completion_inserted_text_callback), completion);
  g_object_bind_property (text, "buffer", completion->insert_text_signal_group, "target", G_BINDING_SYNC_CREATE);

  g_signal_connect (text, "notify", G_CALLBACK (clear_completion_callback), completion);
   g_signal_connect_swapped (text, "activate", G_CALLBACK (accept_completion_callback), completion);
}

static void
disconnect_completion_signals (GtkEntryCompletion *completion)
{
  GtkText *text = gtk_entry_get_text_widget (GTK_ENTRY (completion->entry));

  gtk_widget_remove_controller (GTK_WIDGET (text), completion->entry_key_controller);
  gtk_widget_remove_controller (GTK_WIDGET (text), completion->entry_focus_controller);

  if (completion->changed_id > 0 &&
      g_signal_handler_is_connected (text, completion->changed_id))
    {
      g_signal_handler_disconnect (text, completion->changed_id);
      completion->changed_id = 0;
    }

  g_clear_object (&completion->insert_text_signal_group);

  g_signal_handlers_disconnect_by_func (text, G_CALLBACK (clear_completion_callback), completion);
  g_signal_handlers_disconnect_by_func (text, G_CALLBACK (accept_completion_callback), completion);
}

void
_gtk_entry_completion_disconnect (GtkEntryCompletion *completion)
{
  if (completion->completion_timeout)
    {
      g_source_remove (completion->completion_timeout);
      completion->completion_timeout = 0;
    }
  if (completion->check_completion_idle)
    {
      g_source_destroy (completion->check_completion_idle);
      completion->check_completion_idle = NULL;
    }

  if (gtk_widget_get_mapped (completion->popup_window))
    _gtk_entry_completion_popdown (completion);

  disconnect_completion_signals (completion);

  gtk_widget_unparent (completion->popup_window);

  completion->entry = NULL;
}

void
_gtk_entry_completion_connect (GtkEntryCompletion *completion,
                               GtkEntry           *entry)
{
  completion->entry = GTK_WIDGET (entry);

  gtk_widget_set_parent (completion->popup_window, GTK_WIDGET (entry));

  connect_completion_signals (completion);
}
