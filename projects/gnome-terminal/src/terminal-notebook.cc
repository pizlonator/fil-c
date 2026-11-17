/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2008, 2010, 2011, 2012 Christian Persch
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

#include <config.h>

#include "terminal-notebook.hh"

#include <gtk/gtk.h>

#include "terminal-debug.hh"
#include "terminal-app.hh"
#include "terminal-intl.hh"
#include "terminal-mdi-container.hh"
#include "terminal-screen-container.hh"
#include "terminal-tab-label.hh"
#include "terminal-schemas.hh"
#include "terminal-libgsystem.hh"

#define TERMINAL_NOTEBOOK_GET_PRIVATE(notebook)(G_TYPE_INSTANCE_GET_PRIVATE ((notebook), TERMINAL_TYPE_NOTEBOOK, TerminalNotebookPrivate))

struct _TerminalNotebookPrivate
{
  TerminalScreen *active_screen;
  GtkPolicyType policy;
};

enum
{
  PROP_0,
  PROP_ACTIVE_SCREEN,
  PROP_TAB_POLICY
};

#define ACTION_AREA_BORDER_WIDTH (2)
#define ACTION_BUTTON_SPACING (6)

/* helper functions */

static void
update_tab_visibility (TerminalNotebook *notebook,
                       int change)
{
  TerminalNotebookPrivate *priv = notebook->priv;
  GtkNotebook *gtk_notebook = GTK_NOTEBOOK (notebook);
  int new_n_pages;
  gboolean show_tabs;

  if (gtk_widget_in_destruction (GTK_WIDGET (notebook)))
    return;

  new_n_pages = gtk_notebook_get_n_pages (gtk_notebook) + change;
  /* Don't do anything if we're going to have zero pages (and thus close the window) */
  if (new_n_pages == 0)
    return;

  switch (priv->policy) {
  case GTK_POLICY_ALWAYS:
    show_tabs = TRUE;
    break;
  case GTK_POLICY_AUTOMATIC:
    show_tabs = new_n_pages > 1;
    break;
  case GTK_POLICY_NEVER:
  case GTK_POLICY_EXTERNAL:
  default:
    show_tabs = FALSE;
    break;
  }

  gtk_notebook_set_show_tabs (gtk_notebook, show_tabs);
}

static void
close_button_clicked_cb (TerminalTabLabel *tab_label,
                         gpointer user_data)
{
  TerminalScreen *screen;
  TerminalNotebook *notebook;

  screen = terminal_tab_label_get_screen (tab_label);

  /* notebook is not passed as user_data because it can change during DND
   * and the close button is not notified about that, see bug 731998.
   */
  notebook = TERMINAL_NOTEBOOK (gtk_widget_get_ancestor (GTK_WIDGET (screen),
                                                         TERMINAL_TYPE_NOTEBOOK));

  if (notebook != nullptr)
    g_signal_emit_by_name (notebook, "screen-close-request", screen);
}


static void
remove_reorder_bindings (GtkBindingSet    *binding_set,
                         guint             keysym)
{
  guint keypad_keysym = keysym - GDK_KEY_Left + GDK_KEY_KP_Left;
  gtk_binding_entry_skip (binding_set, keysym, GDK_MOD1_MASK);
  gtk_binding_entry_skip (binding_set, keypad_keysym, GDK_MOD1_MASK);
}

/* TerminalMdiContainer impl */

static void
terminal_notebook_add_screen (TerminalMdiContainer *container,
                              TerminalScreen *screen,
                              int position)
{
  TerminalNotebook *notebook = TERMINAL_NOTEBOOK (container);
  GtkNotebook *gtk_notebook = GTK_NOTEBOOK (notebook);
  GtkWidget *screen_container, *tab_label;

  g_warn_if_fail (gtk_widget_get_parent (GTK_WIDGET (screen)) == nullptr);

  screen_container = terminal_screen_container_new (screen);
  gtk_widget_show (screen_container);

  update_tab_visibility (notebook, +1);

  tab_label = terminal_tab_label_new (screen);
  g_signal_connect (tab_label, "close-button-clicked",
                    G_CALLBACK (close_button_clicked_cb), nullptr);

  gtk_notebook_insert_page (gtk_notebook,
                            screen_container,
                            tab_label,
                            position);
  gtk_container_child_set (GTK_CONTAINER (notebook),
                           screen_container,
                           "tab-expand", TRUE,
                           "tab-fill", TRUE,
                           nullptr);
  gtk_notebook_set_tab_reorderable (gtk_notebook, screen_container, TRUE);
#if 0
  gtk_notebook_set_tab_detachable (gtk_notebook, screen_container, TRUE);
#endif
}

static void
terminal_notebook_remove_screen (TerminalMdiContainer *container,
                                 TerminalScreen *screen)
{
  TerminalNotebook *notebook = TERMINAL_NOTEBOOK (container);
  TerminalScreenContainer *screen_container;

  g_warn_if_fail (gtk_widget_is_ancestor (GTK_WIDGET (screen), GTK_WIDGET (notebook)));

  update_tab_visibility (notebook, -1);

  screen_container = terminal_screen_container_get_from_screen (screen);
  gtk_container_remove (GTK_CONTAINER (notebook),
                        GTK_WIDGET (screen_container));
}

static TerminalScreen *
terminal_notebook_get_active_screen (TerminalMdiContainer *container)
{
  TerminalNotebook *notebook = TERMINAL_NOTEBOOK (container);
  GtkNotebook *gtk_notebook = GTK_NOTEBOOK (notebook);
  GtkWidget *widget;

  widget = gtk_notebook_get_nth_page (gtk_notebook, gtk_notebook_get_current_page (gtk_notebook));
  return terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (widget));
}

static void
terminal_notebook_set_active_screen (TerminalMdiContainer *container,
                                     TerminalScreen *screen)
{
  TerminalNotebook *notebook = TERMINAL_NOTEBOOK (container);
  GtkNotebook *gtk_notebook = GTK_NOTEBOOK (notebook);
  TerminalScreenContainer *screen_container;
  GtkWidget *widget;

  screen_container = terminal_screen_container_get_from_screen (screen);
  widget = GTK_WIDGET (screen_container);

  gtk_notebook_set_current_page (gtk_notebook,
                                 gtk_notebook_page_num (gtk_notebook, widget));
}

static GList *
terminal_notebook_list_screen_containers (TerminalMdiContainer *container)
{
  /* We are trusting that GtkNotebook will return pages in order */
  return gtk_container_get_children (GTK_CONTAINER (container));
}

static GList *
terminal_notebook_list_screens (TerminalMdiContainer *container)
{
  GList *list, *l;

  list = terminal_notebook_list_screen_containers (container);
  for (l = list; l != nullptr; l = l->next)
    l->data = terminal_screen_container_get_screen ((TerminalScreenContainer *) l->data);

  return list;
}

static int
terminal_notebook_get_n_screens (TerminalMdiContainer *container)
{
  return gtk_notebook_get_n_pages (GTK_NOTEBOOK (container));
}

static int
terminal_notebook_get_active_screen_num (TerminalMdiContainer *container)
{
  return gtk_notebook_get_current_page (GTK_NOTEBOOK (container));
}

static void
terminal_notebook_set_active_screen_num (TerminalMdiContainer *container,
                                         int position)
{
  GtkNotebook *gtk_notebook = GTK_NOTEBOOK (container);

  gtk_notebook_set_current_page (gtk_notebook, position);
}

static void
terminal_notebook_reorder_screen (TerminalMdiContainer *container,
                                  TerminalScreen *screen,
                                  int new_position)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (container);
  GtkWidget *child;
  int n, pos;

  g_return_if_fail (new_position == 1 || new_position == -1);

  child = GTK_WIDGET (terminal_screen_container_get_from_screen (screen));
  n = gtk_notebook_get_n_pages (notebook);
  pos = gtk_notebook_page_num (notebook, child);

  pos += new_position;
  gtk_notebook_reorder_child (notebook, child,
                              pos < 0 ? n - 1 : pos < n ? pos : 0);
}

static void
terminal_notebook_mdi_iface_init (TerminalMdiContainerInterface *iface)
{
  iface->add_screen = terminal_notebook_add_screen;
  iface->remove_screen = terminal_notebook_remove_screen;
  iface->get_active_screen = terminal_notebook_get_active_screen;
  iface->set_active_screen = terminal_notebook_set_active_screen;
  iface->list_screens = terminal_notebook_list_screens;
  iface->list_screen_containers = terminal_notebook_list_screen_containers;
  iface->get_n_screens = terminal_notebook_get_n_screens;
  iface->get_active_screen_num = terminal_notebook_get_active_screen_num;
  iface->set_active_screen_num = terminal_notebook_set_active_screen_num;
  iface->reorder_screen = terminal_notebook_reorder_screen;
}

G_DEFINE_TYPE_WITH_CODE (TerminalNotebook, terminal_notebook, GTK_TYPE_NOTEBOOK,
                         G_IMPLEMENT_INTERFACE (TERMINAL_TYPE_MDI_CONTAINER, terminal_notebook_mdi_iface_init))

/* GtkNotebookClass impl */

static void
terminal_notebook_switch_page (GtkNotebook     *gtk_notebook,
                               GtkWidget       *child,
                               guint            page_num)
{
  TerminalNotebook *notebook = TERMINAL_NOTEBOOK (gtk_notebook);
  TerminalNotebookPrivate *priv = notebook->priv;
  TerminalScreen *screen, *old_active_screen;

  GTK_NOTEBOOK_CLASS (terminal_notebook_parent_class)->switch_page (gtk_notebook, child, page_num);

  screen = terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (child));

  old_active_screen = priv->active_screen;
  if (screen == old_active_screen)
    return;

  /* Workaround to remove gtknotebook's feature of computing its size based on
   * all pages. When the widget is hidden, its size will not be taken into
   * account.
   * FIXME!
   */
//   if (old_active_screen)
//     gtk_widget_hide (GTK_WIDGET (terminal_screen_container_get_from_screen (old_active_screen)));
  /* Make sure that the widget is no longer hidden due to the workaround */
//   if (child)
//     gtk_widget_show (child);
  if (old_active_screen)
    gtk_widget_hide (GTK_WIDGET (old_active_screen));
  if (screen)
    gtk_widget_show (GTK_WIDGET (screen));

  priv->active_screen = screen;

  g_signal_emit_by_name (notebook, "screen-switched", old_active_screen, screen);
  g_object_notify (G_OBJECT (notebook), "active-screen");
}

static void
terminal_notebook_page_added (GtkNotebook     *gtk_notebook,
                              GtkWidget       *child,
                              guint            page_num)
{
  TerminalNotebook *notebook = TERMINAL_NOTEBOOK (gtk_notebook);
  void (* page_added) (GtkNotebook *, GtkWidget *, guint) =
    GTK_NOTEBOOK_CLASS (terminal_notebook_parent_class)->page_added;

  if (page_added)
    page_added (gtk_notebook, child, page_num);

  update_tab_visibility (notebook, 0);
  g_signal_emit_by_name (gtk_notebook, "screen-added",
                         terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (child)));
}

static void
terminal_notebook_page_removed (GtkNotebook     *gtk_notebook,
                                GtkWidget       *child,
                                guint            page_num)
{
  TerminalNotebook *notebook = TERMINAL_NOTEBOOK (gtk_notebook);
  void (* page_removed) (GtkNotebook *, GtkWidget *, guint) =
    GTK_NOTEBOOK_CLASS (terminal_notebook_parent_class)->page_removed;

  if (page_removed)
    page_removed (gtk_notebook, child, page_num);

  update_tab_visibility (notebook, 0);
  g_signal_emit_by_name (gtk_notebook, "screen-removed",
                         terminal_screen_container_get_screen (TERMINAL_SCREEN_CONTAINER (child)));
}

static void
terminal_notebook_page_reordered (GtkNotebook     *notebook,
                                  GtkWidget       *child,
                                  guint            page_num)
{
  void (* page_reordered) (GtkNotebook *, GtkWidget *, guint) =
    GTK_NOTEBOOK_CLASS (terminal_notebook_parent_class)->page_reordered;

  if (page_reordered)
    page_reordered (notebook, child, page_num);

  g_signal_emit_by_name (notebook, "screens-reordered");
}

static GtkNotebook *
terminal_notebook_create_window (GtkNotebook       *notebook,
                                 GtkWidget         *page,
                                 gint               x,
                                 gint               y)
{
  return GTK_NOTEBOOK_CLASS (terminal_notebook_parent_class)->create_window (notebook, page, x, y);
}

/* GtkWidgetClass impl */

static void
terminal_notebook_grab_focus (GtkWidget *widget)
{
  TerminalScreen *screen;

  screen = terminal_mdi_container_get_active_screen (TERMINAL_MDI_CONTAINER (widget));
  gtk_widget_grab_focus (GTK_WIDGET (screen));
}

/* GObjectClass impl */

static void
terminal_notebook_init (TerminalNotebook *notebook)
{
  TerminalNotebookPrivate *priv;

  priv = notebook->priv = TERMINAL_NOTEBOOK_GET_PRIVATE (notebook);

  priv->active_screen = nullptr;
  priv->policy = GTK_POLICY_AUTOMATIC;
}

static void
terminal_notebook_constructed (GObject *object)
{
  GSettings *settings;
  GtkWidget *widget = GTK_WIDGET (object);
  GtkNotebook *notebook = GTK_NOTEBOOK (object);

  G_OBJECT_CLASS (terminal_notebook_parent_class)->constructed (object);

  settings = terminal_app_get_global_settings (terminal_app_get ());

  update_tab_visibility (TERMINAL_NOTEBOOK (notebook), 0);
  g_settings_bind (settings,
                   TERMINAL_SETTING_TAB_POLICY_KEY,
                   object,
                   "tab-policy",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET |
				      G_SETTINGS_BIND_NO_SENSITIVITY));

  g_settings_bind (settings,
                   TERMINAL_SETTING_TAB_POSITION_KEY,
                   object,
                   "tab-pos",
                   GSettingsBindFlags(G_SETTINGS_BIND_GET |
				      G_SETTINGS_BIND_NO_SENSITIVITY));

  gtk_notebook_set_scrollable (notebook, TRUE);
  gtk_notebook_set_show_border (notebook, FALSE);
  gtk_notebook_set_group_name (notebook, I_("gnome-terminal-window"));

  /* Necessary for scroll events */
  gtk_widget_add_events (widget, GDK_SCROLL_MASK);
}

static void
terminal_notebook_get_property (GObject *object,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  TerminalMdiContainer *mdi_container = TERMINAL_MDI_CONTAINER (object);

  switch (prop_id) {
    case PROP_ACTIVE_SCREEN:
      g_value_set_object (value, terminal_notebook_get_active_screen (mdi_container));
      break;
    case PROP_TAB_POLICY:
      g_value_set_enum (value, terminal_notebook_get_tab_policy (TERMINAL_NOTEBOOK (mdi_container)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_notebook_set_property (GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
  TerminalMdiContainer *mdi_container = TERMINAL_MDI_CONTAINER (object);

  switch (prop_id) {
    case PROP_ACTIVE_SCREEN:
      terminal_notebook_set_active_screen (mdi_container, (TerminalScreen*)g_value_get_object (value));
      break;
    case PROP_TAB_POLICY:
      terminal_notebook_set_tab_policy (TERMINAL_NOTEBOOK (mdi_container), GtkPolicyType(g_value_get_enum (value)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_notebook_class_init (TerminalNotebookClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkNotebookClass *notebook_class = GTK_NOTEBOOK_CLASS (klass);

  g_type_class_add_private (gobject_class, sizeof (TerminalNotebookPrivate));

  gobject_class->constructed = terminal_notebook_constructed;
  gobject_class->get_property = terminal_notebook_get_property;
  gobject_class->set_property = terminal_notebook_set_property;

  g_object_class_override_property (gobject_class, PROP_ACTIVE_SCREEN, "active-screen");

  widget_class->grab_focus = terminal_notebook_grab_focus;

  notebook_class->switch_page = terminal_notebook_switch_page;
  notebook_class->create_window = terminal_notebook_create_window;
  notebook_class->page_added = terminal_notebook_page_added;
  notebook_class->page_removed = terminal_notebook_page_removed;
  notebook_class->page_reordered = terminal_notebook_page_reordered;

  g_object_class_install_property
    (gobject_class,
     PROP_TAB_POLICY,
     g_param_spec_enum ("tab-policy", nullptr, nullptr,
                        GTK_TYPE_POLICY_TYPE,
                        GTK_POLICY_AUTOMATIC,
                        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /* Remove unwanted and interfering keybindings */
  GtkBindingSet *binding_set = gtk_binding_set_by_class (terminal_notebook_parent_class);
  gtk_binding_entry_skip (binding_set, GDK_KEY_Page_Up, GdkModifierType(GDK_CONTROL_MASK));
  gtk_binding_entry_skip (binding_set, GDK_KEY_Page_Up, GdkModifierType(GDK_CONTROL_MASK | GDK_MOD1_MASK));
  gtk_binding_entry_skip (binding_set, GDK_KEY_Page_Down, GdkModifierType(GDK_CONTROL_MASK));
  gtk_binding_entry_skip (binding_set, GDK_KEY_Page_Down, GdkModifierType(GDK_CONTROL_MASK | GDK_MOD1_MASK));
  remove_reorder_bindings (binding_set, GDK_KEY_Up);
  remove_reorder_bindings (binding_set, GDK_KEY_Down);
  remove_reorder_bindings (binding_set, GDK_KEY_Left);
  remove_reorder_bindings (binding_set, GDK_KEY_Right);
  remove_reorder_bindings (binding_set, GDK_KEY_Home);
  remove_reorder_bindings (binding_set, GDK_KEY_Home);
  remove_reorder_bindings (binding_set, GDK_KEY_End);
  remove_reorder_bindings (binding_set, GDK_KEY_End);
}

/* public API */

/**
 * terminal_notebook_new:
 *
 * Returns: (transfer full): a new #TerminalNotebook
 */
GtkWidget *
terminal_notebook_new (void)
{
  return reinterpret_cast<GtkWidget*>
    (g_object_new (TERMINAL_TYPE_NOTEBOOK, nullptr));
}

void
terminal_notebook_set_tab_policy (TerminalNotebook *notebook,
                                  GtkPolicyType policy)
{
  TerminalNotebookPrivate *priv = notebook->priv;

  if (priv->policy == policy)
    return;

  priv->policy = policy;
  update_tab_visibility (notebook, 0);

  g_object_notify (G_OBJECT (notebook), "tab-policy");
}

GtkPolicyType
terminal_notebook_get_tab_policy (TerminalNotebook *notebook)
{
  return notebook->priv->policy;
}

GtkWidget *
terminal_notebook_get_action_box (TerminalNotebook *notebook,
                                  GtkPackType pack_type)
{
  GtkNotebook *gtk_notebook;
  GtkWidget *box, *inner_box;

  g_return_val_if_fail (TERMINAL_IS_NOTEBOOK (notebook), nullptr);

  gtk_notebook = GTK_NOTEBOOK (notebook);
  box = gtk_notebook_get_action_widget (gtk_notebook, pack_type);
  if (box != nullptr) {
    gs_free_list GList *list;

    list = gtk_container_get_children (GTK_CONTAINER (box));
    g_assert (list->data != nullptr);
    return (GtkWidget*)list->data;
  }

  /* Create container for the buttons */
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_set_border_width (GTK_CONTAINER (box), ACTION_AREA_BORDER_WIDTH);

  inner_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, ACTION_BUTTON_SPACING);
  gtk_box_pack_start (GTK_BOX (box), inner_box, TRUE, FALSE, 0);
  gtk_widget_show (inner_box);

  gtk_notebook_set_action_widget (gtk_notebook, box, pack_type);
  gtk_widget_show (box);

  /* FIXME: this appears to be necessary to make the icon buttons contained
   * in the action area render the same way as buttons in the tab labels (e.g.
   * the close button). gtk+ bug?
   */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_style_context_add_region (gtk_widget_get_style_context (box),
                                GTK_STYLE_REGION_TAB,
                                pack_type == GTK_PACK_START ? GTK_REGION_FIRST : GTK_REGION_LAST);
  G_GNUC_END_IGNORE_DEPRECATIONS

  return inner_box;
}
