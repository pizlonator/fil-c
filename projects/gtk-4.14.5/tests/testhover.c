#include <gtk/gtk.h>

#if 0
#define gtk_event_controller_motion_new gtk_drop_controller_motion_new
#define gtk_event_controller_motion_contains_pointer gtk_drop_controller_motion_contains_pointer
#define gtk_event_controller_motion_is_pointer gtk_drop_controller_motion_is_pointer
#undef GTK_EVENT_CONTROLLER_MOTION
#define GTK_EVENT_CONTROLLER_MOTION GTK_DROP_CONTROLLER_MOTION
#endif

static void
quit_cb (GtkWidget *widget,
         gpointer   unused)
{
  g_main_context_wakeup (NULL);
}

static void
enter_annoy_cb (GtkEventController *controller,
                double              x,
                double              y)
{
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  GtkWindow *window = GTK_WINDOW (gtk_widget_get_root (widget));

  g_print ("%15s ENTER %s %g, %g\n",
           gtk_window_get_title (window),
           gtk_event_controller_motion_contains_pointer (GTK_EVENT_CONTROLLER_MOTION (controller))
           ? gtk_event_controller_motion_is_pointer (GTK_EVENT_CONTROLLER_MOTION (controller))
             ? "IS     "
             : "CONTAIN"
             : "       ",
           x, y);
}

static void
motion_annoy_cb (GtkEventController *controller,
                 double              x,
                 double              y)
{
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  GtkWindow *window = GTK_WINDOW (gtk_widget_get_root (widget));

  g_print ("%15s MOVE  %s %g, %g\n",
           gtk_window_get_title (window),
           gtk_event_controller_motion_contains_pointer (GTK_EVENT_CONTROLLER_MOTION (controller))
           ? gtk_event_controller_motion_is_pointer (GTK_EVENT_CONTROLLER_MOTION (controller))
             ? "IS     "
             : "CONTAIN"
             : "       ",
           x, y);
}

static void
leave_annoy_cb (GtkEventController *controller)
{
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  GtkWindow *window = GTK_WINDOW (gtk_widget_get_root (widget));

  g_print ("%15s LEAVE %s\n",
           gtk_window_get_title (window),
           gtk_event_controller_motion_contains_pointer (GTK_EVENT_CONTROLLER_MOTION (controller))
           ? gtk_event_controller_motion_is_pointer (GTK_EVENT_CONTROLLER_MOTION (controller))
             ? "IS     "
             : "CONTAIN"
             : "       ");
}

static GtkEventController *
annoying_event_controller_motion_new (void)
{
  GtkEventController *controller = gtk_event_controller_motion_new ();

  g_signal_connect (controller, "enter", G_CALLBACK (enter_annoy_cb), NULL);
  g_signal_connect (controller, "motion", G_CALLBACK (motion_annoy_cb), NULL);
  g_signal_connect (controller, "leave", G_CALLBACK (leave_annoy_cb), NULL);

  return controller;
}

/*** TEST 1: remove()/add() ***/

static void
enter1_cb (GtkEventController *controller)
{
  GtkWidget *box = gtk_event_controller_get_widget (controller);

  gtk_box_remove (GTK_BOX (box), gtk_widget_get_first_child (box));
  gtk_box_append (GTK_BOX (box), gtk_label_new ("HOVER!"));
}

static void
leave1_cb (GtkEventController *controller)
{
  GtkWidget *box = gtk_event_controller_get_widget (controller);

  gtk_box_remove (GTK_BOX (box), gtk_widget_get_first_child (box));
  gtk_box_append (GTK_BOX (box), gtk_image_new_from_icon_name ("start-here"));
}

static void
test1 (void)
{
  GtkWidget *win;
  GtkWidget *box;
  GtkEventController *controller;
  win = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (win), 400, 300);
  gtk_window_set_title (GTK_WINDOW (win), "add/remove");

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  gtk_window_set_child (GTK_WINDOW (win), box);
  controller = annoying_event_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (enter1_cb), NULL);
  g_signal_connect (controller, "leave", G_CALLBACK (leave1_cb), NULL);
  gtk_widget_add_controller (box, controller);

  gtk_box_append (GTK_BOX (box), gtk_image_new_from_icon_name ("start-here"));

  gtk_window_present (GTK_WINDOW (win));

  g_signal_connect (win, "destroy", G_CALLBACK (quit_cb), NULL);
}

/*** TEST 2: hide()/show() ***/

static void
enter2_cb (GtkEventController *controller)
{
  GtkWidget *box = gtk_event_controller_get_widget (controller);

  gtk_widget_set_visible (gtk_widget_get_first_child (box), FALSE);
  gtk_widget_set_visible (gtk_widget_get_last_child (box), TRUE);
}

static void
leave2_cb (GtkEventController *controller)
{
  GtkWidget *box = gtk_event_controller_get_widget (controller);

  gtk_widget_set_visible (gtk_widget_get_first_child (box), TRUE);
  gtk_widget_set_visible (gtk_widget_get_last_child (box), FALSE);
}

static void
test2 (void)
{
  GtkWidget *win;
  GtkWidget *box;
  GtkEventController *controller;
  win = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (win), 400, 300);
  gtk_window_set_title (GTK_WINDOW (win), "show/hide");

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
  gtk_window_set_child (GTK_WINDOW (win), box);
  controller = annoying_event_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (enter2_cb), NULL);
  g_signal_connect (controller, "leave", G_CALLBACK (leave2_cb), NULL);
  gtk_widget_add_controller (box, controller);

  gtk_box_append (GTK_BOX (box), gtk_image_new_from_icon_name ("start-here"));
  gtk_box_append (GTK_BOX (box), gtk_label_new ("HOVER!"));
  gtk_widget_set_visible (gtk_widget_get_last_child (box), FALSE);

  gtk_window_present (GTK_WINDOW (win));

  g_signal_connect (win, "destroy", G_CALLBACK (quit_cb), NULL);
}

/*** TEST 3: set_child_visible() ***/

static void
enter3_cb (GtkEventController *controller)
{
  GtkWidget *stack = gtk_event_controller_get_widget (controller);

  gtk_stack_set_visible_child_name (GTK_STACK (stack), "enter");
}

static void
leave3_cb (GtkEventController *controller)
{
  GtkWidget *stack = gtk_event_controller_get_widget (controller);

  gtk_stack_set_visible_child_name (GTK_STACK (stack), "leave");
}

static void
test3 (void)
{
  GtkWidget *win;
  GtkWidget *stack;
  GtkEventController *controller;
  win = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (win), 400, 300);
  gtk_window_set_title (GTK_WINDOW (win), "child-visible");

  stack = gtk_stack_new ();
  gtk_window_set_child (GTK_WINDOW (win), stack);
  controller = annoying_event_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (enter3_cb), NULL);
  g_signal_connect (controller, "leave", G_CALLBACK (leave3_cb), NULL);
  gtk_widget_add_controller (stack, controller);

  gtk_stack_add_named (GTK_STACK (stack), gtk_image_new_from_icon_name ("start-here"), "leave");
  gtk_stack_add_named (GTK_STACK (stack), gtk_label_new ("HOVER!"), "enter");
  gtk_stack_set_visible_child_name (GTK_STACK (stack), "leave");

  gtk_window_present (GTK_WINDOW (win));

  g_signal_connect (win, "destroy", G_CALLBACK (quit_cb), NULL);
}

/*** TEST 4: move ***/

static void
enter4_cb (GtkEventController *controller)
{
  GtkWidget *fixed = gtk_event_controller_get_widget (controller);

  gtk_fixed_move (GTK_FIXED (fixed), gtk_widget_get_first_child (fixed), -1000, -1000);
  gtk_fixed_move (GTK_FIXED (fixed), gtk_widget_get_last_child (fixed), 0, 0);
}

static void
leave4_cb (GtkEventController *controller)
{
  GtkWidget *fixed = gtk_event_controller_get_widget (controller);

  gtk_fixed_move (GTK_FIXED (fixed), gtk_widget_get_first_child (fixed), 0, 0);
  gtk_fixed_move (GTK_FIXED (fixed), gtk_widget_get_last_child (fixed), -1000, -1000);
}

static void
test4 (void)
{
  GtkWidget *win;
  GtkWidget *fixed;
  GtkEventController *controller;
  win = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (win), 400, 300);
  gtk_window_set_title (GTK_WINDOW (win), "move");

  fixed = gtk_fixed_new ();
  gtk_window_set_child (GTK_WINDOW (win), fixed);
  controller = annoying_event_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (enter4_cb), NULL);
  g_signal_connect (controller, "leave", G_CALLBACK (leave4_cb), NULL);
  gtk_widget_add_controller (fixed, controller);

  gtk_fixed_put (GTK_FIXED (fixed), gtk_image_new_from_icon_name ("start-here"), 0, 0);
  gtk_fixed_put (GTK_FIXED (fixed), gtk_label_new ("HOVER!"), -1000, -1000);

  gtk_window_present (GTK_WINDOW (win));

  g_signal_connect (win, "destroy", G_CALLBACK (quit_cb), NULL);
}

int
main (int argc, char *argv[])
{
  GtkCssProvider *provider;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (provider,
      ":hover {"
      "   box-shadow: inset 0px 0px 0px 1px red;"
      " }"
      " window :not(.title):hover {"
      "   background: yellow;"
      " }"
      " window :not(.title):hover * {"
      "   background: goldenrod;"
      " }");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (provider), 800);
  g_object_unref (provider);

  test1();
  test2();
  test3();
  test4();

  while (gtk_window_list_toplevels ())
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
