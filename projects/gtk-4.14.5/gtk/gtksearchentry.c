/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Authors:
 * - Bastien Nocera <bnocera@redhat.com>
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

/*
 * Modified by the GTK+ Team and others 2012.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtksearchentryprivate.h"

#include "gtkaccessibleprivate.h"
#include "gtkeditable.h"
#include "gtkboxlayout.h"
#include "gtkgestureclick.h"
#include "gtktextprivate.h"
#include "gtkimage.h"
#include <glib/gi18n-lib.h>
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkeventcontrollerkey.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtkcsspositionvalueprivate.h"

/**
 * GtkSearchEntry:
 *
 * `GtkSearchEntry` is an entry widget that has been tailored for use
 * as a search entry.
 *
 * The main API for interacting with a `GtkSearchEntry` as entry
 * is the `GtkEditable` interface.
 *
 * ![An example GtkSearchEntry](search-entry.png)
 *
 * It will show an inactive symbolic “find” icon when the search
 * entry is empty, and a symbolic “clear” icon when there is text.
 * Clicking on the “clear” icon will empty the search entry.
 *
 * To make filtering appear more reactive, it is a good idea to
 * not react to every change in the entry text immediately, but
 * only after a short delay. To support this, `GtkSearchEntry`
 * emits the [signal@Gtk.SearchEntry::search-changed] signal which
 * can be used instead of the [signal@Gtk.Editable::changed] signal.
 *
 * The [signal@Gtk.SearchEntry::previous-match],
 * [signal@Gtk.SearchEntry::next-match] and
 * [signal@Gtk.SearchEntry::stop-search] signals can be used to
 * implement moving between search results and ending the search.
 *
 * Often, `GtkSearchEntry` will be fed events by means of being
 * placed inside a [class@Gtk.SearchBar]. If that is not the case,
 * you can use [method@Gtk.SearchEntry.set_key_capture_widget] to
 * let it capture key input from another widget.
 *
 * `GtkSearchEntry` provides only minimal API and should be used with
 * the [iface@Gtk.Editable] API.
 *
 * ## CSS Nodes
 *
 * ```
 * entry.search
 * ╰── text
 * ```
 *
 * `GtkSearchEntry` has a single CSS node with name entry that carries
 * a `.search` style class, and the text node is a child of that.
 *
 * ## Accessibility
 *
 * `GtkSearchEntry` uses the %GTK_ACCESSIBLE_ROLE_SEARCH_BOX role.
 */

enum {
  ACTIVATE,
  SEARCH_CHANGED,
  NEXT_MATCH,
  PREVIOUS_MATCH,
  STOP_SEARCH,
  SEARCH_STARTED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_PLACEHOLDER_TEXT,
  PROP_INPUT_PURPOSE,
  PROP_INPUT_HINTS,
  PROP_ACTIVATES_DEFAULT,
  PROP_SEARCH_DELAY,
  NUM_PROPERTIES,
};

static guint signals[LAST_SIGNAL] = { 0 };

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

typedef struct _GtkSearchEntryClass  GtkSearchEntryClass;

struct _GtkSearchEntry
{
  GtkWidget parent;

  GtkWidget *capture_widget;
  GtkEventController *capture_widget_controller;

  guint search_delay;

  GtkWidget *search_icon;
  GtkWidget *entry;
  GtkWidget *clear_icon;

  guint delayed_changed_id;
  gboolean content_changed;
  gboolean search_stopped;
};

struct _GtkSearchEntryClass
{
  GtkWidgetClass parent_class;

  void (* activate)       (GtkSearchEntry *entry);
  void (* search_changed) (GtkSearchEntry *entry);
  void (* next_match)     (GtkSearchEntry *entry);
  void (* previous_match) (GtkSearchEntry *entry);
  void (* stop_search)    (GtkSearchEntry *entry);
};

static void gtk_search_entry_editable_init (GtkEditableInterface *iface);
static void gtk_search_entry_accessible_init (GtkAccessibleInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSearchEntry, gtk_search_entry, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE,
                                                gtk_search_entry_accessible_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                gtk_search_entry_editable_init))

static void
text_changed (GtkSearchEntry *entry)
{
  entry->content_changed = TRUE;
}

static void
gtk_search_entry_finalize (GObject *object)
{
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (object);

  gtk_editable_finish_delegate (GTK_EDITABLE (entry));

  g_clear_pointer (&entry->search_icon, gtk_widget_unparent);
  g_clear_pointer (&entry->entry, gtk_widget_unparent);
  g_clear_pointer (&entry->clear_icon, gtk_widget_unparent);

  if (entry->delayed_changed_id > 0)
    g_source_remove (entry->delayed_changed_id);

  gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (object), NULL);

  G_OBJECT_CLASS (gtk_search_entry_parent_class)->finalize (object);
}

static void
gtk_search_entry_stop_search (GtkSearchEntry *entry)
{
  entry->search_stopped = TRUE;
}

static void
gtk_search_entry_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (object);
  const char *text;

  if (gtk_editable_delegate_set_property (object, prop_id, value, pspec))
    {
      if (prop_id == NUM_PROPERTIES + GTK_EDITABLE_PROP_EDITABLE)
        {
          gtk_accessible_update_property (GTK_ACCESSIBLE (entry),
                                          GTK_ACCESSIBLE_PROPERTY_READ_ONLY, !g_value_get_boolean (value),
                                          -1);
        }

      return;
    }

  switch (prop_id)
    {
    case PROP_PLACEHOLDER_TEXT:
      text = g_value_get_string (value);
      gtk_text_set_placeholder_text (GTK_TEXT (entry->entry), text);
      gtk_accessible_update_property (GTK_ACCESSIBLE (entry),
                                      GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER, text,
                                      -1);
      break;

    case PROP_INPUT_PURPOSE:
      gtk_search_entry_set_input_purpose (entry, g_value_get_enum (value));
      break;

    case PROP_INPUT_HINTS:
      gtk_search_entry_set_input_hints (entry, g_value_get_flags (value));
      break;

    case PROP_ACTIVATES_DEFAULT:
      if (gtk_text_get_activates_default (GTK_TEXT (entry->entry)) != g_value_get_boolean (value))
        {
          gtk_text_set_activates_default (GTK_TEXT (entry->entry), g_value_get_boolean (value));
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_SEARCH_DELAY:
      gtk_search_entry_set_search_delay (entry, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_search_entry_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (object);

  if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
    return;

  switch (prop_id)
    {
    case PROP_PLACEHOLDER_TEXT:
      g_value_set_string (value, gtk_text_get_placeholder_text (GTK_TEXT (entry->entry)));
      break;

    case PROP_INPUT_PURPOSE:
      g_value_set_enum (value, gtk_text_get_input_purpose (GTK_TEXT (entry->entry)));
      break;

    case PROP_INPUT_HINTS:
      g_value_set_flags (value, gtk_text_get_input_hints (GTK_TEXT (entry->entry)));
      break;

    case PROP_ACTIVATES_DEFAULT:
      g_value_set_boolean (value, gtk_text_get_activates_default (GTK_TEXT (entry->entry)));
      break;

    case PROP_SEARCH_DELAY:
      g_value_set_uint (value, entry->search_delay);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
gtk_search_entry_grab_focus (GtkWidget *widget)
{
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (widget);

  return gtk_text_grab_focus_without_selecting (GTK_TEXT (entry->entry));
}

static gboolean
gtk_search_entry_mnemonic_activate (GtkWidget *widget,
                                    gboolean   group_cycling)
{
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (widget);

  gtk_widget_grab_focus (entry->entry);

  return TRUE;
}

static int
get_spacing (GtkWidget *widget)
{
  GtkCssNode *node = gtk_widget_get_css_node (widget);
  GtkCssStyle *style = gtk_css_node_get_style (node);

  return _gtk_css_position_value_get_x (style->size->border_spacing, 100);
}

static void
gtk_search_entry_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int             *minimum,
                          int             *natural,
                          int             *minimum_baseline,
                          int             *natural_baseline)
{
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (widget);
  int text_min, text_nat;
  int icon_min, icon_nat;
  int spacing;

  spacing = get_spacing (widget);

  gtk_widget_measure (entry->entry,
                      orientation,
                      for_size,
                      &text_min, &text_nat,
                      minimum_baseline, natural_baseline);

  *minimum = text_min;
  *natural = text_nat;

  gtk_widget_measure (entry->search_icon,
                      GTK_ORIENTATION_HORIZONTAL,
                      -1,
                      &icon_min, &icon_nat,
                      NULL, NULL);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum += icon_min + spacing;
      *natural += icon_nat + spacing;
    }
  else
    {
      *minimum = MAX (*minimum, icon_min);
      *natural = MAX (*natural, icon_nat);
    }

  gtk_widget_measure (entry->clear_icon,
                      GTK_ORIENTATION_HORIZONTAL,
                      -1,
                      &icon_min, &icon_nat,
                      NULL, NULL);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum += icon_min + spacing;
      *natural += icon_nat + spacing;
    }
  else
    {
      *minimum = MAX (*minimum, icon_min);
      *natural = MAX (*natural, icon_nat);

      if (G_LIKELY (*minimum_baseline >= 0))
        *minimum_baseline += (*minimum - text_min) / 2;
      if (G_LIKELY (*natural_baseline >= 0))
        *natural_baseline += (*natural - text_nat) / 2;
    }
}

static void
gtk_search_entry_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (widget);
  gboolean is_rtl;
  GtkAllocation text_alloc;
  GtkAllocation icon_alloc;
  int icon_width;
  int spacing;

  spacing = get_spacing (widget);

  is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  text_alloc.x = 0;
  text_alloc.y = 0;
  text_alloc.width = width;
  text_alloc.height = height;

  if (gtk_widget_get_valign (widget) != GTK_ALIGN_BASELINE_FILL &&
      gtk_widget_get_valign (widget) != GTK_ALIGN_BASELINE_CENTER)
    baseline = -1;

  gtk_widget_measure (entry->search_icon,
                      GTK_ORIENTATION_HORIZONTAL,
                      -1,
                      NULL, &icon_width,
                      NULL, NULL);

  icon_alloc.x = is_rtl ? width - icon_width : 0;
  icon_alloc.y = 0;
  icon_alloc.width = icon_width;
  icon_alloc.height = height;

  gtk_widget_size_allocate (entry->search_icon, &icon_alloc, baseline);

  text_alloc.width -= icon_width + spacing;
  text_alloc.x += is_rtl ? 0 : icon_width + spacing;

  if (gtk_widget_get_child_visible (entry->clear_icon))
    {
      gtk_widget_measure (entry->clear_icon,
                          GTK_ORIENTATION_HORIZONTAL,
                          -1,
                          NULL, &icon_width,
                          NULL, NULL);

      icon_alloc.x = is_rtl ? 0 : width - icon_width;
      icon_alloc.y = 0;
      icon_alloc.width = icon_width;
      icon_alloc.height = height;

      gtk_widget_size_allocate (entry->clear_icon, &icon_alloc, baseline);

      text_alloc.width -= icon_width + spacing;
      text_alloc.x += is_rtl ? icon_width + spacing : 0;
    }

  gtk_widget_size_allocate (entry->entry, &text_alloc, baseline);
}

static void
gtk_search_entry_class_init (GtkSearchEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_search_entry_finalize;
  object_class->get_property = gtk_search_entry_get_property;
  object_class->set_property = gtk_search_entry_set_property;

  widget_class->grab_focus = gtk_search_entry_grab_focus;
  widget_class->focus = gtk_widget_focus_child;
  widget_class->mnemonic_activate = gtk_search_entry_mnemonic_activate;
  widget_class->measure = gtk_search_entry_measure;
  widget_class->size_allocate = gtk_search_entry_size_allocate;

  klass->stop_search = gtk_search_entry_stop_search;

  /**
   * GtkSearchEntry:placeholder-text:
   *
   * The text that will be displayed in the `GtkSearchEntry`
   * when it is empty and unfocused.
   */
  props[PROP_PLACEHOLDER_TEXT] =
      g_param_spec_string ("placeholder-text", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

  /**
   * GtkSearchEntry:input-purpose:
   *
   * The purpose for the `GtkSearchEntry` input used to alter the
   * behaviour of input methods.
   *
   * Since: 4.14
   */
  props[PROP_INPUT_PURPOSE] =
      g_param_spec_enum ("input-purpose", NULL, NULL,
                         GTK_TYPE_INPUT_PURPOSE,
                         GTK_INPUT_PURPOSE_FREE_FORM,
                         GTK_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSearchEntry:input-hints:
   *
   * The hints about input for the `GtkSearchEntry` used to alter the
   * behaviour of input methods.
   *
   * Since: 4.14
   */
  props[PROP_INPUT_HINTS] =
      g_param_spec_flags ("input-hints", NULL, NULL,
                          GTK_TYPE_INPUT_HINTS,
                          GTK_INPUT_HINT_NONE,
                          GTK_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSearchEntry:activates-default:
   *
   * Whether to activate the default widget when Enter is pressed.
   */
  props[PROP_ACTIVATES_DEFAULT] =
      g_param_spec_boolean ("activates-default", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSearchEntry:search-delay:
   *
   * The delay in milliseconds from last keypress to the search
   * changed signal.
   *
   * Since: 4.8
   */
  props[PROP_SEARCH_DELAY] =
      g_param_spec_uint ("search-delay", NULL, NULL,
                         0, G_MAXUINT, 150,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);
  gtk_editable_install_properties (object_class, NUM_PROPERTIES);

  /**
   * GtkSearchEntry::activate:
   * @self: The widget on which the signal is emitted
   *
   * Emitted when the entry is activated.
   *
   * The keybindings for this signal are all forms of the Enter key.
   */
  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkSearchEntryClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkSearchEntry::search-changed:
   * @entry: the entry on which the signal was emitted
   *
   * Emitted with a delay. The length of the delay can be
   * changed with the [property@Gtk.SearchEntry:search-delay]
   * property.
   */
  signals[SEARCH_CHANGED] =
    g_signal_new (I_("search-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkSearchEntryClass, search_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkSearchEntry::next-match:
   * @entry: the entry on which the signal was emitted
   *
   * Emitted when the user initiates a move to the next match
   * for the current search string.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * Applications should connect to it, to implement moving
   * between matches.
   *
   * The default bindings for this signal is Ctrl-g.
   */
  signals[NEXT_MATCH] =
    g_signal_new (I_("next-match"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkSearchEntryClass, next_match),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkSearchEntry::previous-match:
   * @entry: the entry on which the signal was emitted
   *
   * Emitted when the user initiates a move to the previous match
   * for the current search string.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * Applications should connect to it, to implement moving
   * between matches.
   *
   * The default bindings for this signal is Ctrl-Shift-g.
   */
  signals[PREVIOUS_MATCH] =
    g_signal_new (I_("previous-match"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkSearchEntryClass, previous_match),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkSearchEntry::stop-search:
   * @entry: the entry on which the signal was emitted
   *
   * Emitted when the user stops a search via keyboard input.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * Applications should connect to it, to implement hiding
   * the search entry in this case.
   *
   * The default bindings for this signal is Escape.
   */
  signals[STOP_SEARCH] =
    g_signal_new (I_("stop-search"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkSearchEntryClass, stop_search),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkSearchEntry::search-started:
   * @entry: the entry on which the signal was emitted
   *
   * Emitted when the user initiated a search on the entry.
   */
  signals[SEARCH_STARTED] =
    g_signal_new (I_("search-started"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_g, GDK_CONTROL_MASK,
                                       "next-match",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_g, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                       "previous-match",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Escape, 0,
                                       "stop-search",
                                       NULL);

  gtk_widget_class_set_css_name (widget_class, I_("entry"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_SEARCH_BOX);
}

static GtkEditable *
gtk_search_entry_get_delegate (GtkEditable *editable)
{
  return GTK_EDITABLE (GTK_SEARCH_ENTRY (editable)->entry);
}

static void
gtk_search_entry_editable_init (GtkEditableInterface *iface)
{
  iface->get_delegate = gtk_search_entry_get_delegate;
}

static gboolean
gtk_search_entry_accessible_get_platform_state (GtkAccessible              *self,
                                                GtkAccessiblePlatformState  state)
{
  return gtk_editable_delegate_get_accessible_platform_state (GTK_EDITABLE (self), state);
}

static void
gtk_search_entry_accessible_init (GtkAccessibleInterface *iface)
{
  GtkAccessibleInterface *parent_iface = g_type_interface_peek_parent (iface);
  iface->get_at_context = parent_iface->get_at_context;
  iface->get_platform_state = gtk_search_entry_accessible_get_platform_state;
}

static void
gtk_search_entry_icon_press (GtkGestureClick *press,
                             int              n_press,
                             double           x,
                             double           y,
                             GtkSearchEntry  *entry)
{
  gtk_gesture_set_state (GTK_GESTURE (press), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
gtk_search_entry_icon_release (GtkGestureClick *press,
                               int              n_press,
                               double           x,
                               double           y,
                               GtkSearchEntry  *entry)
{
  gtk_editable_set_text (GTK_EDITABLE (entry->entry), "");
}

static gboolean
gtk_search_entry_changed_timeout_cb (gpointer user_data)
{
  GtkSearchEntry *entry = user_data;

  g_signal_emit (entry, signals[SEARCH_CHANGED], 0);
  entry->delayed_changed_id = 0;

  return G_SOURCE_REMOVE;
}

static void
reset_timeout (GtkSearchEntry *entry)
{
  if (entry->delayed_changed_id > 0)
    g_source_remove (entry->delayed_changed_id);
  entry->delayed_changed_id = g_timeout_add (entry->search_delay,
                                            gtk_search_entry_changed_timeout_cb,
                                            entry);
  gdk_source_set_static_name_by_id (entry->delayed_changed_id, "[gtk] gtk_search_entry_changed_timeout_cb");
}

static void
gtk_search_entry_changed (GtkEditable    *editable,
                          GtkSearchEntry *entry)
{
  const char *str;

  /* Update the icons first */
  str = gtk_editable_get_text (GTK_EDITABLE (entry->entry));

  if (str == NULL || *str == '\0')
    {
      gtk_widget_set_child_visible (entry->clear_icon, FALSE);
      gtk_widget_queue_allocate (GTK_WIDGET (entry));

      if (entry->delayed_changed_id > 0)
        {
          g_source_remove (entry->delayed_changed_id);
          entry->delayed_changed_id = 0;
        }
      g_signal_emit (entry, signals[SEARCH_CHANGED], 0);
    }
  else
    {
      gtk_widget_set_child_visible (entry->clear_icon, TRUE);
      gtk_widget_queue_allocate (GTK_WIDGET (entry));

      /* Queue up the timeout */
      reset_timeout (entry);
    }
}

static void
notify_cb (GObject    *object,
           GParamSpec *pspec,
           gpointer    data)
{
  /* The editable interface properties are already forwarded by the editable delegate setup */
  if (g_str_equal (pspec->name, "placeholder-text") ||
      g_str_equal (pspec->name, "activates-default"))
    g_object_notify (data, pspec->name);
}

static void
activate_cb (GtkText  *text,
             gpointer  data)
{
  g_signal_emit (data, signals[ACTIVATE], 0);
}

static void
catchall_click_press (GtkGestureClick *gesture,
                      int              n_press,
                      double           x,
                      double           y,
                      gpointer         user_data)
{
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
gtk_search_entry_init (GtkSearchEntry *entry)
{
  GtkGesture *press, *catchall;

  entry->search_delay = 150;

  /* The search icon is purely presentational */
  entry->search_icon = g_object_new (GTK_TYPE_IMAGE,
                                     "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                                     "icon-name", "system-search-symbolic",
                                     NULL);
  gtk_widget_set_parent (entry->search_icon, GTK_WIDGET (entry));

  entry->entry = gtk_text_new ();
  gtk_widget_set_parent (entry->entry, GTK_WIDGET (entry));
  gtk_widget_set_hexpand (entry->entry, TRUE);
  gtk_editable_init_delegate (GTK_EDITABLE (entry));
  g_signal_connect_swapped (entry->entry, "changed", G_CALLBACK (text_changed), entry);
  g_signal_connect_after (entry->entry, "changed", G_CALLBACK (gtk_search_entry_changed), entry);
  g_signal_connect_swapped (entry->entry, "preedit-changed", G_CALLBACK (text_changed), entry);
  g_signal_connect (entry->entry, "notify", G_CALLBACK (notify_cb), entry);
  g_signal_connect (entry->entry, "activate", G_CALLBACK (activate_cb), entry);

  entry->clear_icon = g_object_new (GTK_TYPE_IMAGE,
                                    "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                                    "icon-name", "edit-clear-symbolic",
                                    NULL);
  gtk_widget_set_tooltip_text (entry->clear_icon, _("Clear Entry"));
  gtk_widget_set_parent (entry->clear_icon, GTK_WIDGET (entry));
  gtk_widget_set_child_visible (entry->clear_icon, FALSE);

  press = gtk_gesture_click_new ();
  g_signal_connect (press, "pressed", G_CALLBACK (gtk_search_entry_icon_press), entry);
  g_signal_connect (press, "released", G_CALLBACK (gtk_search_entry_icon_release), entry);
  gtk_widget_add_controller (entry->clear_icon, GTK_EVENT_CONTROLLER (press));

  catchall = gtk_gesture_click_new ();
  g_signal_connect (catchall, "pressed",
                    G_CALLBACK (catchall_click_press), entry);
  gtk_widget_add_controller (GTK_WIDGET (entry),
                             GTK_EVENT_CONTROLLER (catchall));

  gtk_widget_add_css_class (GTK_WIDGET (entry), I_("search"));

  gtk_accessible_update_property (GTK_ACCESSIBLE (entry),
                                  GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, TRUE,
                                  -1);
}

/**
 * gtk_search_entry_new:
 *
 * Creates a `GtkSearchEntry`.
 *
 * Returns: a new `GtkSearchEntry`
 */
GtkWidget *
gtk_search_entry_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_SEARCH_ENTRY, NULL));
}

gboolean
gtk_search_entry_is_keynav (guint           keyval,
                            GdkModifierType state)
{
  if (keyval == GDK_KEY_Tab       || keyval == GDK_KEY_KP_Tab ||
      keyval == GDK_KEY_Up        || keyval == GDK_KEY_KP_Up ||
      keyval == GDK_KEY_Down      || keyval == GDK_KEY_KP_Down ||
      keyval == GDK_KEY_Left      || keyval == GDK_KEY_KP_Left ||
      keyval == GDK_KEY_Right     || keyval == GDK_KEY_KP_Right ||
      keyval == GDK_KEY_Home      || keyval == GDK_KEY_KP_Home ||
      keyval == GDK_KEY_End       || keyval == GDK_KEY_KP_End ||
      keyval == GDK_KEY_Page_Up   || keyval == GDK_KEY_KP_Page_Up ||
      keyval == GDK_KEY_Page_Down || keyval == GDK_KEY_KP_Page_Down ||
      ((state & (GDK_CONTROL_MASK | GDK_ALT_MASK)) != 0))
        return TRUE;

  /* Other navigation events should get automatically
   * ignored as they will not change the content of the entry
   */
  return FALSE;
}

static gboolean
capture_widget_key_handled (GtkEventControllerKey *controller,
                            guint                  keyval,
                            guint                  keycode,
                            GdkModifierType        state,
                            GtkWidget             *widget)
{
  GtkSearchEntry *entry = GTK_SEARCH_ENTRY (widget);
  gboolean handled, was_empty;

  if (gtk_search_entry_is_keynav (keyval, state) ||
      keyval == GDK_KEY_space ||
      keyval == GDK_KEY_Menu)
    return FALSE;

  entry->content_changed = FALSE;
  entry->search_stopped = FALSE;
  was_empty = (gtk_text_get_text_length (GTK_TEXT (entry->entry)) == 0);

  handled = gtk_event_controller_key_forward (controller, entry->entry);

  if (handled)
    {
      if (was_empty && entry->content_changed && !entry->search_stopped)
        g_signal_emit (entry, signals[SEARCH_STARTED], 0);

      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

/**
 * gtk_search_entry_set_key_capture_widget:
 * @entry: a `GtkSearchEntry`
 * @widget: (nullable) (transfer none): a `GtkWidget`
 *
 * Sets @widget as the widget that @entry will capture key
 * events from.
 *
 * Key events are consumed by the search entry to start or
 * continue a search.
 *
 * If the entry is part of a `GtkSearchBar`, it is preferable
 * to call [method@Gtk.SearchBar.set_key_capture_widget] instead,
 * which will reveal the entry in addition to triggering the
 * search entry.
 *
 * Note that despite the name of this function, the events
 * are only 'captured' in the bubble phase, which means that
 * editable child widgets of @widget will receive text input
 * before it gets captured. If that is not desired, you can
 * capture and forward the events yourself with
 * [method@Gtk.EventControllerKey.forward].
 */
void
gtk_search_entry_set_key_capture_widget (GtkSearchEntry *entry,
                                         GtkWidget      *widget)
{
  g_return_if_fail (GTK_IS_SEARCH_ENTRY (entry));
  g_return_if_fail (!widget || GTK_IS_WIDGET (widget));

  if (entry->capture_widget)
    {
      gtk_widget_remove_controller (entry->capture_widget,
                                    entry->capture_widget_controller);
      g_object_remove_weak_pointer (G_OBJECT (entry->capture_widget),
                                    (gpointer *) &entry->capture_widget);
    }

  entry->capture_widget = widget;

  if (widget)
    {
      g_object_add_weak_pointer (G_OBJECT (entry->capture_widget),
                                 (gpointer *) &entry->capture_widget);

      entry->capture_widget_controller = gtk_event_controller_key_new ();
      gtk_event_controller_set_propagation_phase (entry->capture_widget_controller,
                                                  GTK_PHASE_BUBBLE);
      g_signal_connect (entry->capture_widget_controller, "key-pressed",
                        G_CALLBACK (capture_widget_key_handled), entry);
      g_signal_connect (entry->capture_widget_controller, "key-released",
                        G_CALLBACK (capture_widget_key_handled), entry);
      gtk_widget_add_controller (widget, entry->capture_widget_controller);
    }
}

/**
 * gtk_search_entry_get_key_capture_widget:
 * @entry: a `GtkSearchEntry`
 *
 * Gets the widget that @entry is capturing key events from.
 *
 * Returns: (nullable) (transfer none): The key capture widget.
 */
GtkWidget *
gtk_search_entry_get_key_capture_widget (GtkSearchEntry *entry)
{
  g_return_val_if_fail (GTK_IS_SEARCH_ENTRY (entry), NULL);

  return entry->capture_widget;
}

/**
 * gtk_search_entry_set_search_delay: (attributes org.gtk.Property.set_property=search-delay)
 * @entry: a `GtkSearchEntry`
 * @delay: a delay in milliseconds
 *
 * Set the delay to be used between the last keypress and the
 * [signal@Gtk.SearchEntry::search-changed] signal being emitted.
 *
 * Since: 4.8
 */
void
gtk_search_entry_set_search_delay (GtkSearchEntry *entry,
                                   guint delay)
{
  g_return_if_fail (GTK_IS_SEARCH_ENTRY (entry));

  if (entry->search_delay == delay)
    return;

  entry->search_delay = delay;

  /* Apply the updated timeout */
  reset_timeout (entry);

  g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_SEARCH_DELAY]);
}

/**
 * gtk_search_entry_get_search_delay: (attributes org.gtk.Property.get_property=search-delay)
 * @entry: a `GtkSearchEntry`
 *
 * Get the delay to be used between the last keypress and the
 * [signal@Gtk.SearchEntry::search-changed] signal being emitted.
 *
 * Returns: a delay in milliseconds.
 *
 * Since: 4.8
 */
guint
gtk_search_entry_get_search_delay (GtkSearchEntry *entry)
{
  g_return_val_if_fail (GTK_IS_SEARCH_ENTRY (entry), 0);

  return entry->search_delay;
}

GtkEventController *
gtk_search_entry_get_key_controller (GtkSearchEntry *entry)
{
  return gtk_text_get_key_controller (GTK_TEXT (entry->entry));
}

/**
 * gtk_search_entry_get_placeholder_text:
 * @entry: a `GtkSearchEntry`
 *
 * Gets the placeholder text associated with @entry.
 *
 * Returns: (nullable): The placeholder text.
 *
 * Since: 4.10
 */
const char *
gtk_search_entry_get_placeholder_text (GtkSearchEntry *entry)
{
  g_return_val_if_fail (GTK_IS_SEARCH_ENTRY (entry), NULL);

  return gtk_text_get_placeholder_text (GTK_TEXT (entry->entry));
}

/**
 * gtk_search_entry_set_placeholder_text:
 * @entry: a `GtkSearchEntry`
 * @text: (nullable): the text to set as a placeholder
 *
 * Sets the placeholder text associated with @entry.
 *
 * Since: 4.10
 */
void
gtk_search_entry_set_placeholder_text (GtkSearchEntry *entry,
                                       const char     *text)
{
  g_return_if_fail (GTK_IS_SEARCH_ENTRY (entry));

  gtk_text_set_placeholder_text (GTK_TEXT (entry->entry), text);
}

/**
 * gtk_search_entry_get_input_purpose:
 * @entry: a `GtkSearchEntry`
 *
 * Gets the input purpose of @entry.
 *
 * Returns: The input hints
 *
 * Since: 4.14
 */
GtkInputPurpose
gtk_search_entry_get_input_purpose (GtkSearchEntry *entry)
{
  g_return_val_if_fail (GTK_IS_SEARCH_ENTRY (entry), GTK_INPUT_PURPOSE_FREE_FORM);

  return gtk_text_get_input_purpose (GTK_TEXT (entry->entry));
}

/**
 * gtk_search_entry_set_input_purpose:
 * @entry: a `GtkSearchEntry`
 * @purpose: the new input purpose
 *
 * Sets the input purpose of @entry.
 *
 * Since: 4.14
 */
void
gtk_search_entry_set_input_purpose (GtkSearchEntry  *entry,
                                    GtkInputPurpose  purpose)
{
  g_return_if_fail (GTK_IS_SEARCH_ENTRY (entry));

  if (purpose != gtk_search_entry_get_input_purpose (entry))
    {
      gtk_text_set_input_purpose (GTK_TEXT (entry->entry), purpose);

      g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_INPUT_PURPOSE]);
    }
}

/**
 * gtk_search_entry_get_input_hints:
 * @entry: a `GtkSearchEntry`
 *
 * Gets the input purpose for @entry.
 *
 * Returns: The input hints
 *
 * Since: 4.14
 */
GtkInputHints
gtk_search_entry_get_input_hints (GtkSearchEntry *entry)
{
  g_return_val_if_fail (GTK_IS_SEARCH_ENTRY (entry), GTK_INPUT_HINT_NONE);

  return gtk_text_get_input_hints (GTK_TEXT (entry->entry));
}

/**
 * gtk_search_entry_set_input_hints:
 * @entry: a `GtkSearchEntry`
 * @hints: the new input hints
 *
 * Sets the input hints for @entry.
 *
 * Since: 4.14
 */
void
gtk_search_entry_set_input_hints (GtkSearchEntry *entry,
                                  GtkInputHints   hints)
{
  g_return_if_fail (GTK_IS_SEARCH_ENTRY (entry));

  if (hints != gtk_search_entry_get_input_hints (entry))
    {
      gtk_text_set_input_hints (GTK_TEXT (entry->entry), hints);

      g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_INPUT_HINTS]);
    }
}

GtkText *
gtk_search_entry_get_text_widget (GtkSearchEntry *entry)
{
  return GTK_TEXT (entry->entry);
}
