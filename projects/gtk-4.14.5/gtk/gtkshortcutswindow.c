/* gtkshortcutswindow.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkshortcutswindowprivate.h"

#include "gtkbox.h"
#include "gtkbuildable.h"
#include "gtkgrid.h"
#include "gtkheaderbar.h"
#include <glib/gi18n-lib.h>
#include "gtklabel.h"
#include "gtklistbox.h"
#include "gtkmain.h"
#include "gtkmenubutton.h"
#include "gtkpopover.h"
#include "gtkprivate.h"
#include "gtkscrolledwindow.h"
#include "gtksearchbar.h"
#include "gtksearchentry.h"
#include "gtkshortcutssection.h"
#include "gtkshortcutsgroup.h"
#include "gtkshortcutsshortcutprivate.h"
#include "gtksizegroup.h"
#include "gtkstack.h"
#include "gtktogglebutton.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

/**
 * GtkShortcutsWindow:
 *
 * A `GtkShortcutsWindow` shows information about the keyboard shortcuts
 * and gestures of an application.
 *
 * The shortcuts can be grouped, and you can have multiple sections in this
 * window, corresponding to the major modes of your application.
 *
 * Additionally, the shortcuts can be filtered by the current view, to avoid
 * showing information that is not relevant in the current application context.
 *
 * The recommended way to construct a `GtkShortcutsWindow` is with
 * [class@Gtk.Builder], by using the `<child>` tag to populate a
 * `GtkShortcutsWindow` with one or more [class@Gtk.ShortcutsSection] objects,
 * which contain one or more [class@Gtk.ShortcutsGroup] instances, which, in turn,
 * contain [class@Gtk.ShortcutsShortcut] instances.
 *
 * If you need to add a section programmatically, use [method@Gtk.ShortcutsWindow.add_section]
 * instead of [method@Gtk.Window.set_child], as the shortcuts window manages
 * its children directly.
 *
 * # A simple example:
 *
 * ![](gedit-shortcuts.png)
 *
 * This example has as single section. As you can see, the shortcut groups
 * are arranged in columns, and spread across several pages if there are too
 * many to find on a single page.
 *
 * The .ui file for this example can be found [here](https://gitlab.gnome.org/GNOME/gtk/tree/main/demos/gtk-demo/shortcuts-gedit.ui).
 *
 * # An example with multiple views:
 *
 * ![](clocks-shortcuts.png)
 *
 * This example shows a `GtkShortcutsWindow` that has been configured to show only
 * the shortcuts relevant to the "stopwatch" view.
 *
 * The .ui file for this example can be found [here](https://gitlab.gnome.org/GNOME/gtk/tree/main/demos/gtk-demo/shortcuts-clocks.ui).
 *
 * # An example with multiple sections:
 *
 * ![](builder-shortcuts.png)
 *
 * This example shows a `GtkShortcutsWindow` with two sections, "Editor Shortcuts"
 * and "Terminal Shortcuts".
 *
 * The .ui file for this example can be found [here](https://gitlab.gnome.org/GNOME/gtk/tree/main/demos/gtk-demo/shortcuts-builder.ui).
 *
 * ## CSS nodes
 *
 * `GtkShortcutsWindow` has a single CSS node with the name `window` and style
 * class `.shortcuts`.
 */

struct _GtkShortcutsWindow
{
  GtkWindow       parent_instance;

  GHashTable     *keywords;
  char           *initial_section;
  char           *last_section_name;
  char           *view_name;
  GtkSizeGroup   *search_text_group;
  GtkSizeGroup   *search_image_group;
  GHashTable     *search_items_hash;

  GtkStack       *stack;
  GtkStack       *title_stack;
  GtkMenuButton  *menu_button;
  GtkSearchBar   *search_bar;
  GtkSearchEntry *search_entry;
  GtkHeaderBar   *header_bar;
  GtkWidget      *main_box;
  GtkPopover     *popover;
  GtkListBox     *list_box;
  GtkBox         *search_gestures;
  GtkBox         *search_shortcuts;

  GtkWindow      *window;
  gulong          keys_changed_id;
};

typedef struct
{
  GtkWindowClass parent_class;

  void (*close)  (GtkShortcutsWindow *self);
  void (*search) (GtkShortcutsWindow *self);
} GtkShortcutsWindowClass;

typedef struct
{
  GtkShortcutsWindow *self;
  GtkBuilder        *builder;
  GQueue            *stack;
  char              *property_name;
  guint              translatable : 1;
} ViewsParserData;

static void gtk_shortcuts_window_buildable_iface_init (GtkBuildableIface *iface);


G_DEFINE_TYPE_WITH_CODE (GtkShortcutsWindow, gtk_shortcuts_window, GTK_TYPE_WINDOW,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_shortcuts_window_buildable_iface_init))


enum {
  CLOSE,
  SEARCH,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_SECTION_NAME,
  PROP_VIEW_NAME,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];
static guint signals[LAST_SIGNAL];


static gboolean
more_than_three_children (GtkWidget *widget)
{
  GtkWidget *child;
  int i;

  for (child = gtk_widget_get_first_child (widget), i = 0;
       child != NULL;
       child = gtk_widget_get_next_sibling (child), i++)
    {
      if (i == 3)
        return TRUE;
    }

  return FALSE;
}

static void
update_title_stack (GtkShortcutsWindow *self)
{
  GtkWidget *visible_child;

  visible_child = gtk_stack_get_visible_child (self->stack);

  if (GTK_IS_SHORTCUTS_SECTION (visible_child))
    {
      if (more_than_three_children (GTK_WIDGET (self->stack)))
        {
          char *title;

          gtk_stack_set_visible_child_name (self->title_stack, "sections");
          g_object_get (visible_child, "title", &title, NULL);
          gtk_menu_button_set_label (self->menu_button, title);
          g_free (title);
        }
      else
        {
          gtk_stack_set_visible_child_name (self->title_stack, "title");
        }
    }
  else if (visible_child != NULL)
    {
      gtk_stack_set_visible_child_name (self->title_stack, "search");
    }
}

static void
gtk_shortcuts_window_add_search_item (GtkWidget *child, gpointer data)
{
  GtkShortcutsWindow *self = data;
  GtkWidget *item;
  char *accelerator = NULL;
  char *title = NULL;
  char *hash_key = NULL;
  GIcon *icon = NULL;
  gboolean icon_set = FALSE;
  gboolean subtitle_set = FALSE;
  GtkTextDirection direction;
  GtkShortcutType shortcut_type;
  char *action_name = NULL;
  char *subtitle;
  char *str;
  char *keywords;

  if (GTK_IS_SHORTCUTS_SHORTCUT (child))
    {
      GEnumClass *class;
      GEnumValue *value;

      g_object_get (child,
                    "accelerator", &accelerator,
                    "title", &title,
                    "direction", &direction,
                    "icon-set", &icon_set,
                    "subtitle-set", &subtitle_set,
                    "shortcut-type", &shortcut_type,
                    "action-name", &action_name,
                    NULL);

      class = G_ENUM_CLASS (g_type_class_ref (GTK_TYPE_SHORTCUT_TYPE));
      value = g_enum_get_value (class, shortcut_type);

      hash_key = g_strdup_printf ("%s-%s-%s", title, value->value_nick, accelerator);

      g_type_class_unref (class);

      if (g_hash_table_contains (self->search_items_hash, hash_key))
        {
          g_free (hash_key);
          g_free (title);
          g_free (accelerator);
          return;
        }

      g_hash_table_insert (self->search_items_hash, hash_key, GINT_TO_POINTER (1));

      item = g_object_new (GTK_TYPE_SHORTCUTS_SHORTCUT,
                           "accelerator", accelerator,
                           "title", title,
                           "direction", direction,
                           "shortcut-type", shortcut_type,
                           "accel-size-group", self->search_image_group,
                           "title-size-group", self->search_text_group,
                           "action-name", action_name,
                           NULL);
      if (icon_set)
        {
          g_object_get (child, "icon", &icon, NULL);
          g_object_set (item, "icon", icon, NULL);
          g_clear_object (&icon);
        }
      if (subtitle_set)
        {
          g_object_get (child, "subtitle", &subtitle, NULL);
          g_object_set (item, "subtitle", subtitle, NULL);
          g_free (subtitle);
        }
      str = g_strdup_printf ("%s %s", accelerator, title);
      keywords = g_utf8_strdown (str, -1);

      g_hash_table_insert (self->keywords, item, keywords);
      if (shortcut_type == GTK_SHORTCUT_ACCELERATOR)
        gtk_box_append (GTK_BOX (self->search_shortcuts), item);
      else
        gtk_box_append (GTK_BOX (self->search_gestures), item);

      g_free (title);
      g_free (accelerator);
      g_free (str);
      g_free (action_name);
    }
  else
    {
      GtkWidget *widget;

      for (widget = gtk_widget_get_first_child (child);
           widget != NULL;
           widget = gtk_widget_get_next_sibling (widget))
        gtk_shortcuts_window_add_search_item (widget, self);
    }
}

static void
section_notify_cb (GObject    *section,
                   GParamSpec *pspec,
                   gpointer    data)
{
  GtkShortcutsWindow *self = data;

  if (strcmp (pspec->name, "section-name") == 0)
    {
      char *name;

      g_object_get (section, "section-name", &name, NULL);
      g_object_set (gtk_stack_get_page (self->stack, GTK_WIDGET (section)), "name", name, NULL);
      g_free (name);
    }
  else if (strcmp (pspec->name, "title") == 0)
    {
      char *title;
      GtkWidget *label;

      label = g_object_get_data (section, "gtk-shortcuts-title");
      g_object_get (section, "title", &title, NULL);
      gtk_label_set_label (GTK_LABEL (label), title);
      g_free (title);
    }
}

/**
 * gtk_shortcuts_window_add_section:
 * @self: a `GtkShortcutsWindow`
 * @section: the `GtkShortcutsSection` to add
 *
 * Adds a section to the shortcuts window.
 *
 * This is the programmatic equivalent to using [class@Gtk.Builder] and a
 * `<child>` tag to add the child.
 * 
 * Using [method@Gtk.Window.set_child] is not appropriate as the shortcuts
 * window manages its children internally.
 *
 * Since: 4.14
 */
void
gtk_shortcuts_window_add_section (GtkShortcutsWindow  *self,
                                  GtkShortcutsSection *section)
{
  g_return_if_fail (GTK_IS_SHORTCUTS_WINDOW (self));
  g_return_if_fail (GTK_IS_SHORTCUTS_SECTION (section));
  g_return_if_fail (gtk_widget_get_parent (GTK_WIDGET (section)) == NULL);

  GtkListBoxRow *row;
  char *title;
  char *name;
  const char *visible_section;
  GtkWidget *label;
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (section));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    gtk_shortcuts_window_add_search_item (child, self);

  g_object_get (section,
                "section-name", &name,
                "title", &title,
                NULL);

  g_signal_connect (section, "notify", G_CALLBACK (section_notify_cb), self);

  if (name == NULL)
    name = g_strdup ("shortcuts");

  gtk_stack_add_titled (self->stack, GTK_WIDGET (section), name, title);

  visible_section = gtk_stack_get_visible_child_name (self->stack);
  if (strcmp (visible_section, "internal-search") == 0 ||
      (self->initial_section && strcmp (self->initial_section, visible_section) == 0))
    gtk_stack_set_visible_child (self->stack, GTK_WIDGET (section));

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      NULL);
  g_object_set_data (G_OBJECT (row), "gtk-shortcuts-section", section);
  label = g_object_new (GTK_TYPE_LABEL,
                        "margin-start", 6,
                        "margin-end", 6,
                        "margin-top", 6,
                        "margin-bottom", 6,
                        "label", title,
                        "xalign", 0.5f,
                        NULL);
  g_object_set_data (G_OBJECT (section), "gtk-shortcuts-title", label);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), GTK_WIDGET (label));
  gtk_list_box_insert (GTK_LIST_BOX (self->list_box), GTK_WIDGET (row), -1);

  update_title_stack (self);

  g_free (name);
  g_free (title);
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_shortcuts_window_buildable_add_child (GtkBuildable *buildable,
                                          GtkBuilder   *builder,
                                          GObject      *child,
                                          const char   *type)
{
  if (GTK_IS_SHORTCUTS_SECTION (child))
    gtk_shortcuts_window_add_section (GTK_SHORTCUTS_WINDOW (buildable),
                                      GTK_SHORTCUTS_SECTION (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_shortcuts_window_buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_shortcuts_window_buildable_add_child;
}

static void
gtk_shortcuts_window_set_view_name (GtkShortcutsWindow *self,
                                    const char         *view_name)
{
  GtkWidget *section;

  g_free (self->view_name);
  self->view_name = g_strdup (view_name);

  for (section = gtk_widget_get_first_child (GTK_WIDGET (self->stack));
       section != NULL;
       section = gtk_widget_get_next_sibling (section))
    {
      if (GTK_IS_SHORTCUTS_SECTION (section))
        g_object_set (section, "view-name", self->view_name, NULL);
    }
}

static void
gtk_shortcuts_window_set_section_name (GtkShortcutsWindow *self,
                                       const char         *section_name)
{
  GtkWidget *section = NULL;

  g_free (self->initial_section);
  self->initial_section = g_strdup (section_name);

  if (section_name)
    section = gtk_stack_get_child_by_name (self->stack, section_name);
  if (section)
    gtk_stack_set_visible_child (self->stack, section);
}

static void
update_accels_cb (GtkWidget *widget,
                  gpointer   data)
{
  GtkShortcutsWindow *self = data;

  if (GTK_IS_SHORTCUTS_SHORTCUT (widget))
    gtk_shortcuts_shortcut_update_accel (GTK_SHORTCUTS_SHORTCUT (widget), self->window);
  else
    {
      GtkWidget *child;

      for (child = gtk_widget_get_first_child (GTK_WIDGET (widget));
           child != NULL;
           child = gtk_widget_get_next_sibling (child ))
        update_accels_cb (child, self);
    }
}

static void
update_accels_for_actions (GtkShortcutsWindow *self)
{
  if (self->window)
    {
      GtkWidget *child;

      for (child = gtk_widget_get_first_child (GTK_WIDGET (self));
           child != NULL;
           child = gtk_widget_get_next_sibling (child))
        update_accels_cb (child, self);
    }
}

static void
keys_changed_handler (GtkWindow          *window,
                      GtkShortcutsWindow *self)
{
  update_accels_for_actions (self);
}

void
gtk_shortcuts_window_set_window (GtkShortcutsWindow *self,
                                 GtkWindow          *window)
{
  if (self->keys_changed_id)
    {
      g_signal_handler_disconnect (self->window, self->keys_changed_id);
      self->keys_changed_id = 0;
    }

  self->window = window;

  if (self->window)
    self->keys_changed_id = g_signal_connect (window, "keys-changed",
                                              G_CALLBACK (keys_changed_handler),
                                              self);

  update_accels_for_actions (self);
}

static void
gtk_shortcuts_window__list_box__row_activated (GtkShortcutsWindow *self,
                                               GtkListBoxRow      *row,
                                               GtkListBox         *list_box)
{
  GtkWidget *section;

  section = g_object_get_data (G_OBJECT (row), "gtk-shortcuts-section");
  gtk_stack_set_visible_child (self->stack, section);
  gtk_popover_popdown (self->popover);
}

static gboolean
hidden_by_direction (GtkWidget *widget)
{
  if (GTK_IS_SHORTCUTS_SHORTCUT (widget))
    {
      GtkTextDirection dir;

      g_object_get (widget, "direction", &dir, NULL);
      if (dir != GTK_TEXT_DIR_NONE &&
          dir != gtk_widget_get_direction (widget))
        return TRUE;
    }

  return FALSE;
}

static void
gtk_shortcuts_window__entry__changed (GtkShortcutsWindow *self,
                                     GtkSearchEntry      *search_entry)
{
  char *downcase = NULL;
  GHashTableIter iter;
  const char *text;
  const char *last_section_name;
  gpointer key;
  gpointer value;
  gboolean has_result;

  text = gtk_editable_get_text (GTK_EDITABLE (search_entry));

  if (!text || !*text)
    {
      if (self->last_section_name != NULL)
        {
          gtk_stack_set_visible_child_name (self->stack, self->last_section_name);
          return;

        }
    }

  last_section_name = gtk_stack_get_visible_child_name (self->stack);

  if (g_strcmp0 (last_section_name, "internal-search") != 0 &&
      g_strcmp0 (last_section_name, "no-search-results") != 0)
    {
      g_free (self->last_section_name);
      self->last_section_name = g_strdup (last_section_name);
    }

  downcase = g_utf8_strdown (text, -1);

  g_hash_table_iter_init (&iter, self->keywords);

  has_result = FALSE;
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GtkWidget *widget = key;
      const char *keywords = value;
      gboolean match;

      if (hidden_by_direction (widget))
        match = FALSE;
      else
        match = strstr (keywords, downcase) != NULL;

      gtk_widget_set_visible (widget, match);
      has_result |= match;
    }

  g_free (downcase);

  if (has_result)
    gtk_stack_set_visible_child_name (self->stack, "internal-search");
  else
    gtk_stack_set_visible_child_name (self->stack, "no-search-results");
}

static void
gtk_shortcuts_window__search_mode__changed (GtkShortcutsWindow *self)
{
  if (!gtk_search_bar_get_search_mode (self->search_bar))
    {
      if (self->last_section_name != NULL)
        gtk_stack_set_visible_child_name (self->stack, self->last_section_name);
    }
}

static void
gtk_shortcuts_window_close (GtkShortcutsWindow *self)
{
  gtk_window_close (GTK_WINDOW (self));
}

static void
gtk_shortcuts_window_search (GtkShortcutsWindow *self)
{
  gtk_search_bar_set_search_mode (self->search_bar, TRUE);
}

static void
gtk_shortcuts_window_constructed (GObject *object)
{
  GtkShortcutsWindow *self = (GtkShortcutsWindow *)object;

  G_OBJECT_CLASS (gtk_shortcuts_window_parent_class)->constructed (object);

  if (self->initial_section != NULL)
    gtk_stack_set_visible_child_name (self->stack, self->initial_section);
}

static void
gtk_shortcuts_window_finalize (GObject *object)
{
  GtkShortcutsWindow *self = (GtkShortcutsWindow *)object;

  g_clear_pointer (&self->keywords, g_hash_table_unref);
  g_clear_pointer (&self->initial_section, g_free);
  g_clear_pointer (&self->view_name, g_free);
  g_clear_pointer (&self->last_section_name, g_free);
  g_clear_pointer (&self->search_items_hash, g_hash_table_unref);

  g_clear_object (&self->search_image_group);
  g_clear_object (&self->search_text_group);

  G_OBJECT_CLASS (gtk_shortcuts_window_parent_class)->finalize (object);
}

static void
gtk_shortcuts_window_dispose (GObject *object)
{
  GtkShortcutsWindow *self = (GtkShortcutsWindow *)object;

  if (self->stack)
    g_signal_handlers_disconnect_by_func (self->stack, G_CALLBACK (update_title_stack), self);

  gtk_shortcuts_window_set_window (self, NULL);

  self->stack = NULL;
  self->search_bar = NULL;
  self->main_box = NULL;

  G_OBJECT_CLASS (gtk_shortcuts_window_parent_class)->dispose (object);
}

static void
gtk_shortcuts_window_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkShortcutsWindow *self = (GtkShortcutsWindow *)object;

  switch (prop_id)
    {
    case PROP_SECTION_NAME:
      {
        GtkWidget *child = gtk_stack_get_visible_child (self->stack);

        if (child != NULL)
          {
            char *name = NULL;

            g_object_get (gtk_stack_get_page (self->stack, child),
                                     "name", &name,
                                     NULL);
            g_value_take_string (value, name);
          }
      }
      break;

    case PROP_VIEW_NAME:
      g_value_set_string (value, self->view_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcuts_window_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkShortcutsWindow *self = (GtkShortcutsWindow *)object;

  switch (prop_id)
    {
    case PROP_SECTION_NAME:
      gtk_shortcuts_window_set_section_name (self, g_value_get_string (value));
      break;

    case PROP_VIEW_NAME:
      gtk_shortcuts_window_set_view_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcuts_window_unmap (GtkWidget *widget)
{
  GtkShortcutsWindow *self = (GtkShortcutsWindow *)widget;

  gtk_search_bar_set_search_mode (self->search_bar, FALSE);
  gtk_editable_set_text (GTK_EDITABLE (self->search_entry), "");

  GTK_WIDGET_CLASS (gtk_shortcuts_window_parent_class)->unmap (widget);
}

static void
gtk_shortcuts_window_class_init (GtkShortcutsWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gtk_shortcuts_window_constructed;
  object_class->finalize = gtk_shortcuts_window_finalize;
  object_class->get_property = gtk_shortcuts_window_get_property;
  object_class->set_property = gtk_shortcuts_window_set_property;
  object_class->dispose = gtk_shortcuts_window_dispose;

  widget_class->unmap = gtk_shortcuts_window_unmap;

  klass->close = gtk_shortcuts_window_close;
  klass->search = gtk_shortcuts_window_search;

  /**
   * GtkShortcutsWindow:section-name:
   *
   * The name of the section to show.
   *
   * This should be the section-name of one of the `GtkShortcutsSection`
   * objects that are in this shortcuts window.
   */
  properties[PROP_SECTION_NAME] =
    g_param_spec_string ("section-name", NULL, NULL,
                         "internal-search",
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsWindow:view-name:
   *
   * The view name by which to filter the contents.
   *
   * This should correspond to the [property@Gtk.ShortcutsGroup:view]
   * property of some of the [class@Gtk.ShortcutsGroup] objects that
   * are inside this shortcuts window.
   *
   * Set this to %NULL to show all groups.
   */
  properties[PROP_VIEW_NAME] =
    g_param_spec_string ("view-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  /**
   * GtkShortcutsWindow::close:
   *
   * Emitted when the user uses a keybinding to close the window.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default binding for this signal is the Escape key.
   */
  signals[CLOSE] = g_signal_new (I_("close"),
                                 G_TYPE_FROM_CLASS (klass),
                                 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                 G_STRUCT_OFFSET (GtkShortcutsWindowClass, close),
                                 NULL, NULL, NULL,
                                 G_TYPE_NONE,
                                 0);

  /**
   * GtkShortcutsWindow::search:
   *
   * Emitted when the user uses a keybinding to start a search.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default binding for this signal is Control-F.
   */
  signals[SEARCH] = g_signal_new (I_("search"),
                                 G_TYPE_FROM_CLASS (klass),
                                 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                 G_STRUCT_OFFSET (GtkShortcutsWindowClass, search),
                                 NULL, NULL, NULL,
                                 G_TYPE_NONE,
                                 0);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Escape, 0,
                                       "close",
                                       NULL);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_f, GDK_CONTROL_MASK,
                                       "search",
                                       NULL);

  g_type_ensure (GTK_TYPE_SHORTCUTS_GROUP);
  g_type_ensure (GTK_TYPE_SHORTCUTS_SHORTCUT);

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GENERIC);
}

static void
gtk_shortcuts_window_init (GtkShortcutsWindow *self)
{
  GtkWidget *search_button;
  GtkBox *box;
  GtkWidget *scroller;
  GtkWidget *label;
  GtkWidget *empty;
  GtkWidget *image;
  PangoAttrList *attributes;

  gtk_window_set_resizable (GTK_WINDOW (self), FALSE);

  self->keywords = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  self->search_items_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  self->search_text_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  self->search_image_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  self->header_bar = GTK_HEADER_BAR (gtk_header_bar_new ());
  gtk_window_set_titlebar (GTK_WINDOW (self), GTK_WIDGET (self->header_bar));

  search_button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                                "icon-name", "edit-find-symbolic",
                                NULL);

  gtk_accessible_update_property (GTK_ACCESSIBLE (search_button),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, _("Search Shortcuts"),
                                  -1);

  gtk_header_bar_pack_start (GTK_HEADER_BAR (self->header_bar), search_button);

  self->main_box = g_object_new (GTK_TYPE_BOX,
                           "orientation", GTK_ORIENTATION_VERTICAL,
                           NULL);
  gtk_window_set_child (GTK_WINDOW (self), self->main_box);

  self->search_bar = g_object_new (GTK_TYPE_SEARCH_BAR, NULL);
  g_object_bind_property (self->search_bar, "search-mode-enabled",
                          search_button, "active",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  gtk_box_append (GTK_BOX (self->main_box), GTK_WIDGET (self->search_bar));
  gtk_search_bar_set_key_capture_widget (GTK_SEARCH_BAR (self->search_bar),
                                         GTK_WIDGET (self));

  self->stack = g_object_new (GTK_TYPE_STACK,
                              "hexpand", TRUE,
                              "vexpand", TRUE,
                              "hhomogeneous", TRUE,
                              "vhomogeneous", TRUE,
                              "transition-type", GTK_STACK_TRANSITION_TYPE_CROSSFADE,
                              NULL);
  gtk_box_append (GTK_BOX (self->main_box), GTK_WIDGET (self->stack));

  self->title_stack = g_object_new (GTK_TYPE_STACK,
                                    NULL);
  gtk_header_bar_set_title_widget (self->header_bar, GTK_WIDGET (self->title_stack));

  /* Translators: This is the window title for the shortcuts window in normal mode */
  label = gtk_label_new (_("Shortcuts"));
  gtk_widget_add_css_class (label, "title");
  gtk_stack_add_named (self->title_stack, label, "title");

  /* Translators: This is the window title for the shortcuts window in search mode */
  label = gtk_label_new (_("Search Results"));
  gtk_widget_add_css_class (label, "title");
  gtk_stack_add_named (self->title_stack, label, "search");

  self->menu_button = g_object_new (GTK_TYPE_MENU_BUTTON,
                                    "focus-on-click", FALSE,
                                    NULL);
  gtk_widget_add_css_class (GTK_WIDGET (self->menu_button), "flat");
  gtk_stack_add_named (self->title_stack, GTK_WIDGET (self->menu_button), "sections");

  self->popover = g_object_new (GTK_TYPE_POPOVER,
                                "position", GTK_POS_BOTTOM,
                                NULL);
  gtk_menu_button_set_popover (self->menu_button, GTK_WIDGET (self->popover));

  self->list_box = g_object_new (GTK_TYPE_LIST_BOX,
                                 "selection-mode", GTK_SELECTION_NONE,
                                 NULL);
  g_signal_connect_object (self->list_box,
                           "row-activated",
                           G_CALLBACK (gtk_shortcuts_window__list_box__row_activated),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_popover_set_child (GTK_POPOVER (self->popover), GTK_WIDGET (self->list_box));

  self->search_entry = GTK_SEARCH_ENTRY (gtk_search_entry_new ());
  gtk_search_bar_set_child (GTK_SEARCH_BAR (self->search_bar), GTK_WIDGET (self->search_entry));

  g_object_set (self->search_entry,
                /* Translators: This is placeholder text for the search entry in the shortcuts window */
                "placeholder-text", _("Search Shortcuts"),
                "width-chars", 31,
                "max-width-chars", 40,
                NULL);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self->search_entry),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, _("Search Shortcuts"),
                                  -1);

  gtk_accessible_update_relation (GTK_ACCESSIBLE (self->search_bar),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->search_entry, NULL,
                                  -1);

  g_signal_connect_object (self->search_entry,
                           "search-changed",
                           G_CALLBACK (gtk_shortcuts_window__entry__changed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->search_bar,
                           "notify::search-mode-enabled",
                           G_CALLBACK (gtk_shortcuts_window__search_mode__changed),
                           self,
                           G_CONNECT_SWAPPED);

  scroller = gtk_scrolled_window_new ();
  box = g_object_new (GTK_TYPE_BOX,
                      "halign", GTK_ALIGN_CENTER,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      NULL);
  gtk_widget_add_css_class (GTK_WIDGET (box), "shortcuts-search-results");
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroller), GTK_WIDGET (box));
  gtk_stack_add_named (self->stack, scroller, "internal-search");

  self->search_shortcuts = g_object_new (GTK_TYPE_BOX,
                                         "halign", GTK_ALIGN_CENTER,
                                         "spacing", 6,
                                         "orientation", GTK_ORIENTATION_VERTICAL,
                                         NULL);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (self->search_shortcuts));

  self->search_gestures = g_object_new (GTK_TYPE_BOX,
                                        "halign", GTK_ALIGN_CENTER,
                                        "spacing", 6,
                                        "orientation", GTK_ORIENTATION_VERTICAL,
                                        NULL);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (self->search_gestures));

  empty = g_object_new (GTK_TYPE_GRID,
                        "row-spacing", 12,
                        "margin-start", 12,
                        "margin-end", 12,
                        "margin-top", 12,
                        "margin-bottom", 12,
                        "hexpand", TRUE,
                        "vexpand", TRUE,
                        "halign", GTK_ALIGN_CENTER,
                        "valign", GTK_ALIGN_CENTER,
                        NULL);
  gtk_widget_add_css_class (empty, "dim-label");
  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "edit-find-symbolic",
                        "pixel-size", 72,
                        NULL);
  gtk_grid_attach (GTK_GRID (empty), image, 0, 0, 1, 1);
  attributes = pango_attr_list_new ();
  pango_attr_list_insert (attributes, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  pango_attr_list_insert (attributes, pango_attr_scale_new (1.44));
  label = g_object_new (GTK_TYPE_LABEL,
                        "label", _("No Results Found"),
                        "attributes", attributes,
                        NULL);
  pango_attr_list_unref (attributes);
  gtk_grid_attach (GTK_GRID (empty), label, 0, 1, 1, 1);

  gtk_accessible_update_relation (GTK_ACCESSIBLE (image),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, NULL,
                                  -1);

  label = g_object_new (GTK_TYPE_LABEL,
                        "label", _("Try a different search"),
                        NULL);
  gtk_grid_attach (GTK_GRID (empty), label, 0, 2, 1, 1);

  gtk_stack_add_named (self->stack, empty, "no-search-results");

  g_signal_connect_object (self->stack, "notify::visible-child",
                           G_CALLBACK (update_title_stack), self, G_CONNECT_SWAPPED);

  gtk_widget_add_css_class (GTK_WIDGET (self), "shortcuts");
}
