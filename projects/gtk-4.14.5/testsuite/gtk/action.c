/* Copyright (C) 2019 Red Hat, Inc.
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
#include <gtk/gtk.h>

static void
activate (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
  int *activated = user_data;
  (*activated)++;
}

/* Test that inheriting actions along the widget
 * hierarchy works as expected. Since GtkWidget does
 * not expose the actions, we test this by observing
 * the effect of activating them.
 */
static void
test_inheritance (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *button;
  GSimpleActionGroup *win_actions;
  GSimpleActionGroup *box_actions;
  GActionEntry entries[] = {
    { "action", activate, NULL, NULL, NULL },
  };
  gboolean found;
  int win_activated;
  int box_activated;

  win_activated = 0;
  box_activated = 0;

  /* Our hierarchy looks like this:
   *
   * window    win.action
   *   |
   *  box      box.action
   *   |
   * button
   */
  window = gtk_window_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  button = gtk_button_new ();

  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_box_append (GTK_BOX (box), button);

  win_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (win_actions),
                                   entries, G_N_ELEMENTS (entries),
                                   &win_activated);

  box_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (box_actions),
                                   entries, G_N_ELEMENTS (entries),
                                   &box_activated);

  gtk_widget_insert_action_group (window, "win", G_ACTION_GROUP (win_actions));
  gtk_widget_insert_action_group (box, "box", G_ACTION_GROUP (box_actions));

  g_assert_cmpint (win_activated, ==, 0);
  g_assert_cmpint (box_activated, ==, 0);

  found = gtk_widget_activate_action (button, "win.action", NULL);

  g_assert_true (found);
  g_assert_cmpint (win_activated, ==, 1);
  g_assert_cmpint (box_activated, ==, 0);

  found = gtk_widget_activate_action (box, "win.action", NULL);

  g_assert_true (found);
  g_assert_cmpint (win_activated, ==, 2);
  g_assert_cmpint (box_activated, ==, 0);

  found = gtk_widget_activate_action (button, "box.action", NULL);

  g_assert_true (found);
  g_assert_cmpint (win_activated, ==, 2);
  g_assert_cmpint (box_activated, ==, 1);

  found = gtk_widget_activate_action (window, "box.action", NULL);

  g_assert_false (found);
  g_assert_cmpint (win_activated, ==, 2);
  g_assert_cmpint (box_activated, ==, 1);

  gtk_window_destroy (GTK_WINDOW (window));
  g_object_unref (win_actions);
  g_object_unref (box_actions);
}

/* Test action inheritance with hierarchy changes */
static void
test_inheritance2 (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GSimpleActionGroup *win_actions;
  GSimpleActionGroup *box1_actions;
  GSimpleActionGroup *box2_actions;
  GActionEntry entries[] = {
    { "action", activate, NULL, NULL, NULL },
  };
  gboolean found;
  int win_activated;
  int box1_activated;
  int box2_activated;

  win_activated = 0;
  box1_activated = 0;
  box2_activated = 0;

  /* Our hierarchy looks like this:
   *
   * window win.action
   *   |
   *  box--------------------+
   *   |                     |
   *  box1   box1.action    box2   box2.action;
   *   |
   * button
   */
  window = gtk_window_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  box1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  button = gtk_button_new ();

  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_box_append (GTK_BOX (box), box1);
  gtk_box_append (GTK_BOX (box), box2);
  gtk_box_append (GTK_BOX (box1), button);

  win_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (win_actions),
                                   entries, G_N_ELEMENTS (entries),
                                   &win_activated);

  box1_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (box1_actions),
                                   entries, G_N_ELEMENTS (entries),
                                   &box1_activated);

  box2_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (box2_actions),
                                   entries, G_N_ELEMENTS (entries),
                                   &box2_activated);

  gtk_widget_insert_action_group (window, "win", G_ACTION_GROUP (win_actions));
  gtk_widget_insert_action_group (box1, "box1", G_ACTION_GROUP (box1_actions));
  gtk_widget_insert_action_group (box2, "box2", G_ACTION_GROUP (box2_actions));

  g_assert_cmpint (win_activated, ==, 0);
  g_assert_cmpint (box1_activated, ==, 0);
  g_assert_cmpint (box2_activated, ==, 0);

  found = gtk_widget_activate_action (button, "win.action", NULL);

  g_assert_true (found);
  g_assert_cmpint (win_activated, ==, 1);
  g_assert_cmpint (box1_activated, ==, 0);
  g_assert_cmpint (box2_activated, ==, 0);

  found = gtk_widget_activate_action (button, "box1.action", NULL);

  g_assert_true (found);
  g_assert_cmpint (win_activated, ==, 1);
  g_assert_cmpint (box1_activated, ==, 1);
  g_assert_cmpint (box2_activated, ==, 0);

  found = gtk_widget_activate_action (button, "box2.action", NULL);

  g_assert_false (found);
  g_assert_cmpint (win_activated, ==, 1);
  g_assert_cmpint (box1_activated, ==, 1);
  g_assert_cmpint (box2_activated, ==, 0);

  g_object_ref (button);
  gtk_box_remove (GTK_BOX (box1), button);
  gtk_box_append (GTK_BOX (box2), button);
  g_object_unref (button);

  found = gtk_widget_activate_action (button, "win.action", NULL);

  g_assert_true (found);
  g_assert_cmpint (win_activated, ==, 2);
  g_assert_cmpint (box1_activated, ==, 1);
  g_assert_cmpint (box2_activated, ==, 0);

  found = gtk_widget_activate_action (button, "box1.action", NULL);

  g_assert_false (found);
  g_assert_cmpint (win_activated, ==, 2);
  g_assert_cmpint (box1_activated, ==, 1);
  g_assert_cmpint (box2_activated, ==, 0);

  found = gtk_widget_activate_action (button, "box2.action", NULL);

  g_assert_true (found);
  g_assert_cmpint (win_activated, ==, 2);
  g_assert_cmpint (box1_activated, ==, 1);
  g_assert_cmpint (box2_activated, ==, 1);

  gtk_window_destroy (GTK_WINDOW (window));

  g_object_unref (win_actions);
  g_object_unref (box1_actions);
  g_object_unref (box2_actions);
}

/* Similar to test_inheritance2, but using the actionable machinery
 */
static void
test_inheritance3 (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GSimpleActionGroup *win_actions;
  GSimpleActionGroup *box1_actions;
  GActionEntry entries[] = {
    { "action", activate, NULL, NULL, NULL },
  };
  int activated;

  /* Our hierarchy looks like this:
   *
   * window win.action
   *   |
   *  box--------------------+
   *   |                     |
   *  box1   box1.action    box2
   *   |
   * button
   */
  window = gtk_window_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  box1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  button = gtk_button_new ();

  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_box_append (GTK_BOX (box), box1);
  gtk_box_append (GTK_BOX (box), box2);
  gtk_box_append (GTK_BOX (box1), button);

  win_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (win_actions),
                                   entries, G_N_ELEMENTS (entries),
                                   &activated);

  box1_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (box1_actions),
                                   entries, G_N_ELEMENTS (entries),
                                   &activated);

  gtk_widget_insert_action_group (window, "win", G_ACTION_GROUP (win_actions));
  gtk_widget_insert_action_group (box1, "box1", G_ACTION_GROUP (box1_actions));

  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "box1.action");

  g_assert_true (gtk_widget_get_sensitive (button));

  g_object_ref (button);
  gtk_box_remove (GTK_BOX (box1), button);
  gtk_box_append (GTK_BOX (box2), button);
  g_object_unref (button);

  g_assert_false (gtk_widget_get_sensitive (button));

  g_object_ref (button);
  gtk_box_remove (GTK_BOX (box2), button);
  gtk_box_append (GTK_BOX (box1), button);
  g_object_unref (button);

  g_assert_true (gtk_widget_get_sensitive (button));

  g_object_ref (button);
  gtk_box_remove (GTK_BOX (box1), button);
  gtk_box_append (GTK_BOX (box2), button);
  g_object_unref (button);

  g_assert_false (gtk_widget_get_sensitive (button));

  g_object_ref (box2);
  gtk_box_remove (GTK_BOX (box), box2);
  gtk_box_append (GTK_BOX (box1), box2);
  g_object_unref (box2);

  g_assert_true (gtk_widget_get_sensitive (button));

  gtk_widget_insert_action_group (box1, "box1", NULL);

  g_assert_false (gtk_widget_get_sensitive (button));

  gtk_widget_insert_action_group (box1, "box1", G_ACTION_GROUP (box1_actions));

  g_assert_true (gtk_widget_get_sensitive (button));

  gtk_window_destroy (GTK_WINDOW (window));

  g_object_unref (win_actions);
  g_object_unref (box1_actions);
}

/* this checks a particular bug I've seen: when the action muxer
 * hierarchy is already set up, adding action groups 'in the middle'
 * does not properly update the muxer hierarchy, causing actions
 * to be missed.
 */
static void
test_inheritance4 (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *button;
  GSimpleActionGroup *win_actions;
  GSimpleActionGroup *box_actions;
  GActionEntry entries[] = {
    { "action", activate, NULL, NULL, NULL },
  };
  int activated;

  /* Our hierarchy looks like this:
   *
   * window win.action
   *   |
   *  box
   *   |
   * button
   */
  window = gtk_window_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  button = gtk_button_new ();

  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_box_append (GTK_BOX (box), button);

  win_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (win_actions),
                                   entries, G_N_ELEMENTS (entries),
                                   &activated);

  gtk_widget_insert_action_group (window, "win", G_ACTION_GROUP (win_actions));

  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "box.action");

  /* no box1.action yet, but the action muxers are set up, with windows' muxer
   * being the parent of button's, since box has no muxer yet.
   */
  g_assert_false (gtk_widget_get_sensitive (button));

  box_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (box_actions),
                                   entries, G_N_ELEMENTS (entries),
                                   &activated);

  gtk_widget_insert_action_group (box, "box", G_ACTION_GROUP (box_actions));

  /* now box has a muxer, and buttons muxer should be updated to inherit
   * from it
   */
  g_assert_true (gtk_widget_get_sensitive (button));

  gtk_window_destroy (GTK_WINDOW (window));

  g_object_unref (win_actions);
  g_object_unref (box_actions);
}

static int cut_activated;
static int copy_activated;
static int paste_activated;
static int visibility_changed;

static void
cut_activate (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  cut_activated++;
}

static void
copy_activate (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  copy_activated++;
}

static void
paste_activate (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  paste_activated++;
}

static void
visibility_changed_cb (GObject    *object,
                       GParamSpec *pspec,
                       gpointer    user_data)
{
  visibility_changed++;
}

/* Spot-check that GtkText has the class actions
 * for the context menu. Here we test that the clipboard
 * actions are present, and that toggling visibility
 * via the action works.
 */
static void
test_text (void)
{
  GtkWidget *box;
  GtkWidget *text;
  GSimpleActionGroup *clipboard_actions;
  GActionEntry clipboard_entries[] = {
    { "cut", cut_activate, NULL, NULL, NULL },
    { "copy", copy_activate, NULL, NULL, NULL },
    { "paste", paste_activate, NULL, NULL, NULL },
  };
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  text = gtk_text_new ();

  gtk_box_append (GTK_BOX (box), text);

  clipboard_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (clipboard_actions),
                                   clipboard_entries,
                                   G_N_ELEMENTS (clipboard_entries),
                                   NULL);

  gtk_widget_insert_action_group (box, "clipboard", G_ACTION_GROUP (clipboard_actions));

  gtk_widget_activate_action (text, "cut.clipboard", NULL);
  gtk_widget_activate_action (text, "copy.clipboard", NULL);
  gtk_widget_activate_action (text, "paste.clipboard", NULL);

  g_assert_cmpint (cut_activated, ==, 0);
  g_assert_cmpint (copy_activated, ==, 0);
  g_assert_cmpint (paste_activated, ==, 0);

  g_signal_connect (text, "notify::visibility",
                    G_CALLBACK (visibility_changed_cb), NULL);

  gtk_widget_activate_action (text, "misc.toggle-visibility", NULL);

  g_assert_cmpint (visibility_changed, ==, 1);

  g_object_unref (g_object_ref_sink (box));
  g_object_unref (clipboard_actions);
}

/* Test that inheritance works for individual actions
 * even if they are in groups with the same prefix.
 * This is a change from the way things work in GTK3.
 */
static void
test_overlap (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GActionEntry win_entries[] = {
    { "win", activate, NULL, NULL, NULL },
  };
  GActionEntry box_entries[] = {
    { "box", activate, NULL, NULL, NULL },
  };
  GSimpleActionGroup *win_actions;
  GSimpleActionGroup *box_actions;
  int win_activated;
  int box_activated;

  window = gtk_window_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_window_set_child (GTK_WINDOW (window), box);

  win_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (win_actions),
                                   win_entries,
                                   G_N_ELEMENTS (win_entries),
                                   &win_activated);

  box_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (box_actions),
                                   box_entries,
                                   G_N_ELEMENTS (box_entries),
                                   &box_activated);

  gtk_widget_insert_action_group (window, "actions", G_ACTION_GROUP (win_actions));
  gtk_widget_insert_action_group (box, "actions", G_ACTION_GROUP (box_actions));

  win_activated = 0;
  box_activated = 0;

  gtk_widget_activate_action (box, "actions.win", NULL);

  g_assert_cmpint (win_activated, ==, 1);
  g_assert_cmpint (box_activated, ==, 0);

  gtk_widget_activate_action (box, "actions.box", NULL);

  g_assert_cmpint (win_activated, ==, 1);
  g_assert_cmpint (box_activated, ==, 1);

  gtk_window_destroy (GTK_WINDOW (window));
  g_object_unref (win_actions);
  g_object_unref (box_actions);
}

static int toggled;
static int act1;
static int act2;

static void
visibility_toggled (GObject *object,
                    GParamSpec *pspec,
                    gpointer data)
{
  toggled++;
}

static void
activate1 (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
  act1++;
}

static void
activate2 (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
  act2++;
}

/* Test that overlap also works as expected between
 * class action and inserted groups. Class actions
 * take precedence over inserted groups in the same
 * muxer, but inheritance works as normal between
 * muxers.
 */
static void
test_overlap2 (void)
{
  GtkWidget *text;
  GtkWidget *child;
  GSimpleActionGroup *group1;
  GSimpleActionGroup *group2;
  GActionEntry entries1[] = {
    { "toggle-visibility", activate1, NULL, NULL, NULL },
  };
  GActionEntry entries2[] = {
    { "toggle-visibility", activate2, NULL, NULL, NULL },
  };

  text = gtk_text_new ();
  g_signal_connect (text, "notify::visibility",
                    G_CALLBACK (visibility_toggled), NULL);

  child = gtk_label_new ("");
  gtk_widget_set_parent (child, text);

  g_assert_cmpint (toggled, ==, 0);
  g_assert_cmpint (act1, ==, 0);
  g_assert_cmpint (act2, ==, 0);

  gtk_widget_activate_action (child, "misc.toggle-visibility", NULL);

  g_assert_cmpint (toggled, ==, 1);
  g_assert_cmpint (act1, ==, 0);
  g_assert_cmpint (act2, ==, 0);

  group1 = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group1),
                                   entries1,
                                   G_N_ELEMENTS (entries1),
                                   NULL);
  gtk_widget_insert_action_group (text, "misc", G_ACTION_GROUP (group1));
  gtk_widget_activate_action (child, "misc.toggle-visibility", NULL);

  g_assert_cmpint (toggled, ==, 2);
  g_assert_cmpint (act1, ==, 0);
  g_assert_cmpint (act2, ==, 0);

  group2 = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group2),
                                   entries2,
                                   G_N_ELEMENTS (entries2),
                                   NULL);
  gtk_widget_insert_action_group (child, "misc", G_ACTION_GROUP (group2));

  gtk_widget_activate_action (child, "misc.toggle-visibility", NULL);

  g_assert_cmpint (toggled, ==, 2);
  g_assert_cmpint (act1, ==, 0);
  g_assert_cmpint (act2, ==, 1);

  g_object_unref (group1);
  g_object_unref (group2);

  gtk_widget_unparent (child);
  g_object_unref (g_object_ref_sink (text));
}

/* Test that gtk_widget_class_query_action
 * yields the expected results
 */
static void
test_introspection (void)
{
  GtkWidgetClass *class = g_type_class_ref (GTK_TYPE_TEXT);
  guint i, j;
  guint found;
  GType owner;
  const char *name;
  const GVariantType *params;
  const char *property;
  struct {
    GType owner;
    const char *name;
    const char *params;
    const char *property;
  } expected[] = {
    { GTK_TYPE_TEXT, "misc.toggle-visibility", NULL, "visibility" },
    { GTK_TYPE_TEXT, "misc.insert-emoji", NULL, NULL },
    { GTK_TYPE_TEXT, "selection.select-all", NULL, NULL },
    { GTK_TYPE_TEXT, "selection.delete", NULL, NULL },
    { GTK_TYPE_TEXT, "clipboard.paste", NULL, NULL },
    { GTK_TYPE_TEXT, "clipboard.copy", NULL, NULL },
    { GTK_TYPE_TEXT, "clipboard.cut", NULL, NULL },
    { GTK_TYPE_TEXT, "menu.popup", NULL, NULL },
    { GTK_TYPE_TEXT, "text.redo", NULL, NULL },
    { GTK_TYPE_TEXT, "text.undo", NULL, NULL },
  };

  j = 0;
  found = 0;
  while (gtk_widget_class_query_action (class,
                                        j,
                                        &owner,
                                        &name,
                                        &params,
                                        &property))
    {
      for (i = 0; i < G_N_ELEMENTS (expected); i++)
        {
          if (strcmp (expected[i].name, name) == 0)
            {
              found++;
              g_assert_true (expected[i].owner == owner);
              g_assert_cmpstr (expected[i].name, ==, name);
              g_assert_cmpstr (expected[i].params, ==, params ? g_variant_type_peek_string (params) : NULL);
              g_assert_cmpstr (expected[i].property, ==, property);
              break;
            }
        }
      if (i == G_N_ELEMENTS (expected))
        g_error ("Unexpected GtkText action: %s", name);
      j++;
    }
  g_assert_cmpuint (found, ==, G_N_ELEMENTS (expected));

  g_type_class_unref (class);
}

/* Test that disabled actions don't get activated */
static void
test_enabled (void)
{
  GtkWidget *text;

  text = gtk_text_new ();
  g_signal_connect (text, "notify::visibility",
                    G_CALLBACK (visibility_toggled), NULL);

  toggled = 0;

  gtk_widget_activate_action (text, "misc.toggle-visibility", NULL);

  g_assert_cmpint (toggled, ==, 1);

  gtk_widget_action_set_enabled (text, "misc.toggle-visibility", FALSE);

  gtk_widget_activate_action (text, "misc.toggle-visibility", NULL);

  g_assert_cmpint (toggled, ==, 1);

  g_object_unref (g_object_ref_sink (text));
}

#define MY_TYPE_GTK_ACTIONABLE (my_gtk_actionable_get_type ())
G_DECLARE_FINAL_TYPE          (MyGtkActionable, my_gtk_actionable, MY, GTK_ACTIONABLE, GtkButton)

struct _MyGtkActionable
{
  GtkButton parent_instance;
};

G_DEFINE_FINAL_TYPE (MyGtkActionable, my_gtk_actionable, GTK_TYPE_BUTTON);

static void
test_cb (GtkWidget  *sender,
         const char *name,
         GVariant   *param)
{
}

static void
my_gtk_actionable_init (MyGtkActionable *actionable)
{
  gtk_actionable_set_action_name (GTK_ACTIONABLE (actionable), "test.test");
}

static void
my_gtk_actionable_class_init (MyGtkActionableClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_install_action (widget_class, "test.test", NULL, test_cb);
}

/* Test that actions are correctly notified after reparenting */
static void
test_reparenting (void)
{
  GtkWidget *window, *actionable;

  window = gtk_window_new ();

  actionable = g_object_new (MY_TYPE_GTK_ACTIONABLE, NULL);
  gtk_window_set_child (GTK_WINDOW (window), actionable);
  g_assert_true (gtk_widget_get_sensitive (actionable));

  g_object_ref (actionable);
  gtk_window_set_child (GTK_WINDOW (window), NULL);
  g_assert_false (gtk_widget_get_sensitive (actionable));

  gtk_window_set_child (GTK_WINDOW (window), actionable);
  g_object_unref (actionable);
  g_assert_true (gtk_widget_get_sensitive (actionable));

  g_object_unref (window);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/action/inheritance", test_inheritance);
  g_test_add_func ("/action/inheritance2", test_inheritance2);
  g_test_add_func ("/action/inheritance3", test_inheritance3);
  g_test_add_func ("/action/inheritance4", test_inheritance4);
  g_test_add_func ("/action/text", test_text);
  g_test_add_func ("/action/overlap", test_overlap);
  g_test_add_func ("/action/overlap2", test_overlap2);
  g_test_add_func ("/action/introspection", test_introspection);
  g_test_add_func ("/action/enabled", test_enabled);
  g_test_add_func ("/action/reparenting", test_reparenting);

  return g_test_run();
}
