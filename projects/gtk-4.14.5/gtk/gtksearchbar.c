/* GTK - The GIMP Toolkit
 * Copyright (C) 2013 Red Hat, Inc.
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
 * Modified by the GTK+ Team and others 2013.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtksearchbar.h"

#include "gtkbinlayout.h"
#include "gtkbuildable.h"
#include "gtkbutton.h"
#include "gtkcenterbox.h"
#include "gtkentryprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtkprivate.h"
#include "gtkrevealer.h"
#include "gtksearchentryprivate.h"
#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"

/**
 * GtkSearchBar:
 *
 * `GtkSearchBar` is a container made to have a search entry.
 *
 * ![An example GtkSearchBar](search-bar.png)
 *
 * It can also contain additional widgets, such as drop-down menus,
 * or buttons.  The search bar would appear when a search is started
 * through typing on the keyboard, or the application’s search mode
 * is toggled on.
 *
 * For keyboard presses to start a search, the search bar must be told
 * of a widget to capture key events from through
 * [method@Gtk.SearchBar.set_key_capture_widget]. This widget will
 * typically be the top-level window, or a parent container of the
 * search bar. Common shortcuts such as Ctrl+F should be handled as an
 * application action, or through the menu items.
 *
 * You will also need to tell the search bar about which entry you
 * are using as your search entry using [method@Gtk.SearchBar.connect_entry].
 *
 * ## Creating a search bar
 *
 * The following example shows you how to create a more complex search
 * entry.
 *
 * [A simple example](https://gitlab.gnome.org/GNOME/gtk/tree/main/examples/search-bar.c)
 *
 * # CSS nodes
 *
 * ```
 * searchbar
 * ╰── revealer
 *     ╰── box
 *          ├── [child]
 *          ╰── [button.close]
 * ```
 *
 * `GtkSearchBar` has a main CSS node with name searchbar. It has a child
 * node with name revealer that contains a node with name box. The box node
 * contains both the CSS node of the child widget as well as an optional button
 * node which gets the .close style class applied.
 *
 * # Accessibility
 *
 * `GtkSearchBar` uses the %GTK_ACCESSIBLE_ROLE_SEARCH role.
 */

typedef struct _GtkSearchBarClass   GtkSearchBarClass;

struct _GtkSearchBar
{
  GtkWidget parent;

  GtkWidget   *child;
  GtkWidget   *revealer;
  GtkWidget   *box_center;
  GtkWidget   *close_button;

  GtkWidget   *entry;
  gboolean     reveal_child;

  GtkWidget   *capture_widget;
  GtkEventController *capture_widget_controller;
};

struct _GtkSearchBarClass
{
  GtkWidgetClass parent_class;
};

static void gtk_search_bar_buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSearchBar, gtk_search_bar, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_search_bar_buildable_iface_init))

enum {
  PROP_0,
  PROP_SEARCH_MODE_ENABLED,
  PROP_SHOW_CLOSE_BUTTON,
  PROP_CHILD,
  PROP_KEY_CAPTURE_WIDGET,
  LAST_PROPERTY
};

static GParamSpec *widget_props[LAST_PROPERTY] = { NULL, };

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_search_bar_buildable_add_child (GtkBuildable *buildable,
                                    GtkBuilder   *builder,
                                    GObject      *child,
                                    const char   *type)
{
  if (GTK_IS_WIDGET (child))
    gtk_search_bar_set_child (GTK_SEARCH_BAR (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_search_bar_buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_search_bar_buildable_add_child;
}

static void
stop_search_cb (GtkWidget    *entry,
                GtkSearchBar *bar)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (bar->revealer), FALSE);
}

static void
reveal_child_changed_cb (GObject      *object,
                         GParamSpec   *pspec,
                         GtkSearchBar *bar)
{
  gboolean reveal_child;

  g_object_get (object, "reveal-child", &reveal_child, NULL);

  if (reveal_child == bar->reveal_child)
    return;

  bar->reveal_child = reveal_child;

  if (bar->entry)
    {
      if (reveal_child && GTK_IS_ENTRY (bar->entry))
        gtk_entry_grab_focus_without_selecting (GTK_ENTRY (bar->entry));
      else if (reveal_child && GTK_IS_SEARCH_ENTRY (bar->entry))
        gtk_widget_grab_focus (bar->entry);
      else
        gtk_editable_set_text (GTK_EDITABLE (bar->entry), "");
    }

  g_object_notify_by_pspec (G_OBJECT (bar), widget_props[PROP_SEARCH_MODE_ENABLED]);
}

static void
close_button_clicked_cb (GtkWidget    *button,
                         GtkSearchBar *bar)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (bar->revealer), FALSE);
}

static void
gtk_search_bar_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (object);

  switch (prop_id)
    {
    case PROP_SEARCH_MODE_ENABLED:
      gtk_search_bar_set_search_mode (bar, g_value_get_boolean (value));
      break;
    case PROP_SHOW_CLOSE_BUTTON:
      gtk_search_bar_set_show_close_button (bar, g_value_get_boolean (value));
      break;
    case PROP_CHILD:
      gtk_search_bar_set_child (bar, g_value_get_object (value));
      break;
    case PROP_KEY_CAPTURE_WIDGET:
      gtk_search_bar_set_key_capture_widget (bar, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_search_bar_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (object);

  switch (prop_id)
    {
    case PROP_SEARCH_MODE_ENABLED:
      g_value_set_boolean (value, gtk_search_bar_get_search_mode (bar));
      break;
    case PROP_SHOW_CLOSE_BUTTON:
      g_value_set_boolean (value, gtk_search_bar_get_show_close_button (bar));
      break;
    case PROP_CHILD:
      g_value_set_object (value, gtk_search_bar_get_child (bar));
      break;
    case PROP_KEY_CAPTURE_WIDGET:
      g_value_set_object (value, gtk_search_bar_get_key_capture_widget (bar));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void gtk_search_bar_set_entry (GtkSearchBar *bar,
                                      GtkEditable  *editable);

static void
gtk_search_bar_dispose (GObject *object)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (object);

  gtk_search_bar_set_key_capture_widget (bar, NULL);
  gtk_search_bar_set_entry (bar, NULL);

  g_clear_pointer (&bar->revealer, gtk_widget_unparent);

  bar->child = NULL;
  bar->box_center = NULL;
  bar->close_button = NULL;

  G_OBJECT_CLASS (gtk_search_bar_parent_class)->dispose (object);
}

static void
gtk_search_bar_compute_expand (GtkWidget *widget,
                               gboolean  *hexpand,
                               gboolean  *vexpand)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (widget);

  if (bar->child)
    {
      *hexpand = gtk_widget_compute_expand (bar->child, GTK_ORIENTATION_HORIZONTAL);
      *vexpand = gtk_widget_compute_expand (bar->child, GTK_ORIENTATION_VERTICAL);
    }
  else
    {
      *hexpand = FALSE;
      *vexpand = FALSE;
    }
}

static void
gtk_search_bar_class_init (GtkSearchBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_search_bar_dispose;
  object_class->set_property = gtk_search_bar_set_property;
  object_class->get_property = gtk_search_bar_get_property;

  widget_class->compute_expand = gtk_search_bar_compute_expand;
  widget_class->focus = gtk_widget_focus_child;

  /**
   * GtkSearchBar:search-mode-enabled: (attributes org.gtk.Property.get=gtk_search_bar_get_search_mode org.gtk.Property.set=gtk_search_bar_set_search_mode)
   *
   * Whether the search mode is on and the search bar shown.
   */
  widget_props[PROP_SEARCH_MODE_ENABLED] = g_param_spec_boolean ("search-mode-enabled", NULL, NULL,
                                                                 FALSE,
                                                                 GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSearchBar:show-close-button: (attributes org.gtk.Property.get=gtk_search_bar_get_show_close_button org.gtk.Property.set=gtk_search_bar_set_show_close_button)
   *
   * Whether to show the close button in the search bar.
   */
  widget_props[PROP_SHOW_CLOSE_BUTTON] = g_param_spec_boolean ("show-close-button", NULL, NULL,
                                                               FALSE,
                                                               GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSearchBar:child: (attributes org.gtk.Property.get=gtk_search_bar_get_child org.gtk.Property.set=gtk_search_bar_set_child)
   *
   * The child widget.
   */
  widget_props[PROP_CHILD] = g_param_spec_object ("child", NULL, NULL,
                                                  GTK_TYPE_WIDGET,
                                                  GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkSearchBar:key-capture-widget: (attributes org.gtk.Property.get=gtk_search_bar_get_key_capture_widget org.gtk.Property.set=gtk_search_bar_set_key_capture_widget)
   *
   * The key capture widget.
   */
  widget_props[PROP_KEY_CAPTURE_WIDGET]
      = g_param_spec_object ("key-capture-widget", NULL, NULL,
                             GTK_TYPE_WIDGET,
                             GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROPERTY, widget_props);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("searchbar"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_SEARCH);
}

static void
gtk_search_bar_init (GtkSearchBar *bar)
{
  bar->revealer = gtk_revealer_new ();
  gtk_revealer_set_reveal_child (GTK_REVEALER (bar->revealer), FALSE);
  gtk_widget_set_hexpand (bar->revealer, TRUE);
  gtk_widget_set_parent (bar->revealer, GTK_WIDGET (bar));

  bar->box_center = gtk_center_box_new ();
  gtk_widget_set_hexpand (bar->box_center, TRUE);

  bar->close_button = gtk_button_new_from_icon_name ("window-close-symbolic");
  gtk_widget_set_valign (bar->close_button, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (bar->close_button, "close");
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (bar->box_center), bar->close_button);
  gtk_widget_set_visible (bar->close_button, FALSE);

  gtk_revealer_set_child (GTK_REVEALER (bar->revealer), bar->box_center);

  g_signal_connect (bar->revealer, "notify::reveal-child",
                    G_CALLBACK (reveal_child_changed_cb), bar);

  g_signal_connect (bar->close_button, "clicked",
                    G_CALLBACK (close_button_clicked_cb), bar);
}

/**
 * gtk_search_bar_new:
 *
 * Creates a `GtkSearchBar`.
 *
 * You will need to tell it about which widget is going to be your text
 * entry using [method@Gtk.SearchBar.connect_entry].
 *
 * Returns: a new `GtkSearchBar`
 */
GtkWidget *
gtk_search_bar_new (void)
{
  return g_object_new (GTK_TYPE_SEARCH_BAR, NULL);
}

static void
gtk_search_bar_set_entry (GtkSearchBar *bar,
                          GtkEditable  *entry)
{
  if (bar->entry != NULL)
    {
      if (GTK_IS_SEARCH_ENTRY (bar->entry))
        {
          gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (bar->entry), NULL);
          g_signal_handlers_disconnect_by_func (bar->entry, stop_search_cb, bar);
        }
      g_object_remove_weak_pointer (G_OBJECT (bar->entry), (gpointer *) &bar->entry);
    }

  bar->entry = GTK_WIDGET (entry);

  if (bar->entry != NULL)
    {
      g_object_add_weak_pointer (G_OBJECT (bar->entry), (gpointer *) &bar->entry);
      if (GTK_IS_SEARCH_ENTRY (bar->entry))
        {
          g_signal_connect (bar->entry, "stop-search",
                            G_CALLBACK (stop_search_cb), bar);
          gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (bar->entry),
                                                   GTK_WIDGET (bar));
        }

    }
}

/**
 * gtk_search_bar_connect_entry:
 * @bar: a `GtkSearchBar`
 * @entry: a `GtkEditable`
 *
 * Connects the `GtkEditable` widget passed as the one to be used in
 * this search bar.
 *
 * The entry should be a descendant of the search bar. Calling this
 * function manually is only required if the entry isn’t the direct
 * child of the search bar (as in our main example).
 */
void
gtk_search_bar_connect_entry (GtkSearchBar *bar,
                              GtkEditable  *entry)
{
  g_return_if_fail (GTK_IS_SEARCH_BAR (bar));
  g_return_if_fail (entry == NULL || GTK_IS_EDITABLE (entry));

  gtk_search_bar_set_entry (bar, entry);
}

/**
 * gtk_search_bar_get_search_mode: (attributes org.gtk.Method.get_property=search-mode-enabled)
 * @bar: a `GtkSearchBar`
 *
 * Returns whether the search mode is on or off.
 *
 * Returns: whether search mode is toggled on
 */
gboolean
gtk_search_bar_get_search_mode (GtkSearchBar *bar)
{
  g_return_val_if_fail (GTK_IS_SEARCH_BAR (bar), FALSE);

  return bar->reveal_child;
}

/**
 * gtk_search_bar_set_search_mode: (attributes org.gtk.Method.set_property=search-mode-enabled)
 * @bar: a `GtkSearchBar`
 * @search_mode: the new state of the search mode
 *
 * Switches the search mode on or off.
 */
void
gtk_search_bar_set_search_mode (GtkSearchBar *bar,
                                gboolean      search_mode)
{
  g_return_if_fail (GTK_IS_SEARCH_BAR (bar));

  gtk_revealer_set_reveal_child (GTK_REVEALER (bar->revealer), search_mode);
}

/**
 * gtk_search_bar_get_show_close_button: (attributes org.gtk.Method.get_property=show-close-button)
 * @bar: a `GtkSearchBar`
 *
 * Returns whether the close button is shown.
 *
 * Returns: whether the close button is shown
 */
gboolean
gtk_search_bar_get_show_close_button (GtkSearchBar *bar)
{
  g_return_val_if_fail (GTK_IS_SEARCH_BAR (bar), FALSE);

  return gtk_widget_get_visible (bar->close_button);
}

/**
 * gtk_search_bar_set_show_close_button: (attributes org.gtk.Method.set_property=show-close-button)
 * @bar: a `GtkSearchBar`
 * @visible: whether the close button will be shown or not
 *
 * Shows or hides the close button.
 *
 * Applications that already have a “search” toggle button should not
 * show a close button in their search bar, as it duplicates the role
 * of the toggle button.
 */
void
gtk_search_bar_set_show_close_button (GtkSearchBar *bar,
                                      gboolean      visible)
{
  g_return_if_fail (GTK_IS_SEARCH_BAR (bar));

  visible = visible != FALSE;

  if (gtk_widget_get_visible (bar->close_button) != visible)
    {
      gtk_widget_set_visible (bar->close_button, visible);
      g_object_notify_by_pspec (G_OBJECT (bar), widget_props[PROP_SHOW_CLOSE_BUTTON]);
    }
}

static void
changed_cb (gboolean *changed)
{
  *changed = TRUE;
}

static gboolean
capture_widget_key_handled (GtkEventControllerKey *controller,
                            guint                  keyval,
                            guint                  keycode,
                            GdkModifierType        state,
                            GtkSearchBar          *bar)
{
  gboolean handled;

  if (!gtk_widget_get_mapped (GTK_WIDGET (bar)))
    return GDK_EVENT_PROPAGATE;

  if (bar->reveal_child)
    return GDK_EVENT_PROPAGATE;

  if (bar->entry == NULL)
    {
      g_warning ("The search bar does not have an entry connected to it. Call gtk_search_bar_connect_entry() to connect one.");
      return GDK_EVENT_PROPAGATE;
    }

  if (GTK_IS_SEARCH_ENTRY (bar->entry))
    {
      /* The search entry was told to listen to events from the search bar, so
       * just forward the event to self, so the search entry has an opportunity
       * to intercept those.
       */
      handled = gtk_event_controller_key_forward (controller, GTK_WIDGET (bar));
    }
  else
    {
      gboolean preedit_changed, buffer_changed;
      guint preedit_change_id, buffer_change_id;
      gboolean res;

      if (gtk_search_entry_is_keynav (keyval, state) ||
          keyval == GDK_KEY_space ||
          keyval == GDK_KEY_Menu)
        return GDK_EVENT_PROPAGATE;

      if (keyval == GDK_KEY_Escape)
        {
          if (gtk_revealer_get_reveal_child (GTK_REVEALER (bar->revealer)))
            {
              stop_search_cb (bar->entry, bar);
              return GDK_EVENT_STOP;
            }

          return GDK_EVENT_PROPAGATE;
        }

      handled = GDK_EVENT_PROPAGATE;
      preedit_changed = buffer_changed = FALSE;
      preedit_change_id = g_signal_connect_swapped (bar->entry, "preedit-changed",
                                                    G_CALLBACK (changed_cb), &preedit_changed);
      buffer_change_id = g_signal_connect_swapped (bar->entry, "changed",
                                                   G_CALLBACK (changed_cb), &buffer_changed);

      res = gtk_event_controller_key_forward (controller, bar->entry);

      g_signal_handler_disconnect (bar->entry, preedit_change_id);
      g_signal_handler_disconnect (bar->entry, buffer_change_id);

      if ((res && buffer_changed) || preedit_changed)
        handled = GDK_EVENT_STOP;
    }

  if (handled == GDK_EVENT_STOP)
    gtk_revealer_set_reveal_child (GTK_REVEALER (bar->revealer), TRUE);

  return handled;
}

/**
 * gtk_search_bar_set_key_capture_widget: (attributes org.gtk.Method.set_property=key-capture-widget)
 * @bar: a `GtkSearchBar`
 * @widget: (nullable) (transfer none): a `GtkWidget`
 *
 * Sets @widget as the widget that @bar will capture key events
 * from.
 *
 * If key events are handled by the search bar, the bar will
 * be shown, and the entry populated with the entered text.
 *
 * Note that despite the name of this function, the events
 * are only 'captured' in the bubble phase, which means that
 * editable child widgets of @widget will receive text input
 * before it gets captured. If that is not desired, you can
 * capture and forward the events yourself with
 * [method@Gtk.EventControllerKey.forward].
 */
void
gtk_search_bar_set_key_capture_widget (GtkSearchBar *bar,
                                       GtkWidget    *widget)
{
  g_return_if_fail (GTK_IS_SEARCH_BAR (bar));
  g_return_if_fail (!widget || GTK_IS_WIDGET (widget));

  if (bar->capture_widget == widget)
    return;

  if (bar->capture_widget)
    {
      gtk_widget_remove_controller (bar->capture_widget,
                                    bar->capture_widget_controller);
      g_object_remove_weak_pointer (G_OBJECT (bar->capture_widget),
                                    (gpointer *) &bar->capture_widget);
    }

  bar->capture_widget = widget;

  if (widget)
    {
      g_object_add_weak_pointer (G_OBJECT (bar->capture_widget),
                                 (gpointer *) &bar->capture_widget);

      bar->capture_widget_controller = gtk_event_controller_key_new ();
      gtk_event_controller_set_propagation_phase (bar->capture_widget_controller,
                                                  GTK_PHASE_BUBBLE);
      g_signal_connect (bar->capture_widget_controller, "key-pressed",
                        G_CALLBACK (capture_widget_key_handled), bar);
      g_signal_connect (bar->capture_widget_controller, "key-released",
                        G_CALLBACK (capture_widget_key_handled), bar);
      gtk_widget_add_controller (widget, bar->capture_widget_controller);
    }

  g_object_notify_by_pspec (G_OBJECT (bar), widget_props[PROP_KEY_CAPTURE_WIDGET]);
}

/**
 * gtk_search_bar_get_key_capture_widget: (attributes org.gtk.Method.get_property=key-capture-widget)
 * @bar: a `GtkSearchBar`
 *
 * Gets the widget that @bar is capturing key events from.
 *
 * Returns: (nullable) (transfer none): The key capture widget.
 **/
GtkWidget *
gtk_search_bar_get_key_capture_widget (GtkSearchBar *bar)
{
  g_return_val_if_fail (GTK_IS_SEARCH_BAR (bar), NULL);

  return bar->capture_widget;
}

/**
 * gtk_search_bar_set_child: (attributes org.gtk.Method.set_property=child)
 * @bar: a `GtkSearchBar`
 * @child: (nullable): the child widget
 *
 * Sets the child widget of @bar.
 */
void
gtk_search_bar_set_child (GtkSearchBar *bar,
                          GtkWidget    *child)
{
  g_return_if_fail (GTK_IS_SEARCH_BAR (bar));
  g_return_if_fail (child == NULL || bar->child == child || gtk_widget_get_parent (child) == NULL);

  if (bar->child == child)
    return;

  if (bar->child)
    {
      if (GTK_IS_EDITABLE (bar->child))
        gtk_search_bar_connect_entry (bar, NULL);

      gtk_center_box_set_center_widget (GTK_CENTER_BOX (bar->box_center), NULL);
    }

   bar->child = child;

  if (bar->child)
    {
      gtk_center_box_set_center_widget (GTK_CENTER_BOX (bar->box_center), child);

      if (GTK_IS_EDITABLE (child))
        gtk_search_bar_connect_entry (bar, GTK_EDITABLE (child));
    }

  g_object_notify_by_pspec (G_OBJECT (bar), widget_props[PROP_CHILD]);
}

/**
 * gtk_search_bar_get_child: (attributes org.gtk.Method.get_property=child)
 * @bar: a `GtkSearchBar`
 *
 * Gets the child widget of @bar.
 *
 * Returns: (nullable) (transfer none): the child widget of @bar
 */
GtkWidget *
gtk_search_bar_get_child (GtkSearchBar *bar)
{
  return bar->child;
}
