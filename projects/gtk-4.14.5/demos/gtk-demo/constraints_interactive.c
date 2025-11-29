/* Constraints/Interactive Constraints
 * #Keywords: GtkConstraintLayout
 *
 * This example shows how constraints can be updated during user interaction.
 * The vertical edge between the buttons can be dragged with the mouse.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE (InteractiveGrid, interactive_grid, INTERACTIVE, GRID, GtkWidget)

struct _InteractiveGrid
{
  GtkWidget parent_instance;

  GtkWidget *button1, *button2;
  GtkWidget *button3;
  GtkConstraintGuide *guide;
  GtkConstraint *constraint;
};

G_DEFINE_TYPE (InteractiveGrid, interactive_grid, GTK_TYPE_WIDGET)

static void
interactive_grid_dispose (GObject *object)
{
  InteractiveGrid *self = INTERACTIVE_GRID (object);

  g_clear_pointer (&self->button1, gtk_widget_unparent);
  g_clear_pointer (&self->button2, gtk_widget_unparent);
  g_clear_pointer (&self->button3, gtk_widget_unparent);

  G_OBJECT_CLASS (interactive_grid_parent_class)->dispose (object);
}

static void
interactive_grid_class_init (InteractiveGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = interactive_grid_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_CONSTRAINT_LAYOUT);
}

static void
build_constraints (InteractiveGrid          *self,
                   GtkConstraintLayout *manager)
{
  self->guide = g_object_new (GTK_TYPE_CONSTRAINT_GUIDE, NULL);
  gtk_constraint_layout_add_guide (manager, self->guide);

  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new_constant (GTK_CONSTRAINT_TARGET (self->guide),
                        GTK_CONSTRAINT_ATTRIBUTE_WIDTH,
                        GTK_CONSTRAINT_RELATION_EQ,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));

  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->button1),
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        1.0,
                        -8.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button1),
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->guide),
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button2),
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->guide),
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button2),
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        GTK_CONSTRAINT_RELATION_EQ,
                        NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        1.0,
                        -8.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->button3),
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        1.0,
                        -8.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));

  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button3),
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->guide),
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));

  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->button1),
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        1.0,
                        -8.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button2),
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->button1),
                        GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button3),
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->button2),
                        GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button3),
                        GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                        GTK_CONSTRAINT_RELATION_EQ,
                        NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                        1.0,
                        -8.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
}

static void
drag_cb (GtkGestureDrag  *drag,
         double           offset_x,
         double           offset_y,
         InteractiveGrid *self)
{
  GtkConstraintLayout *layout = GTK_CONSTRAINT_LAYOUT (gtk_widget_get_layout_manager (GTK_WIDGET (self)));
  double x, y;

  if (self->constraint)
    {
      gtk_constraint_layout_remove_constraint (layout, self->constraint);
      g_clear_object (&self->constraint);
    }

  gtk_gesture_drag_get_start_point (drag, &x, &y);
  self->constraint = gtk_constraint_new_constant (GTK_CONSTRAINT_TARGET (self->guide),
                                                  GTK_CONSTRAINT_ATTRIBUTE_LEFT,
                                                  GTK_CONSTRAINT_RELATION_EQ,
                                                  x + offset_x,
                                                  GTK_CONSTRAINT_STRENGTH_REQUIRED);
  gtk_constraint_layout_add_constraint (layout, g_object_ref (self->constraint));
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
interactive_grid_init (InteractiveGrid *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkGesture *drag;

  self->button1 = gtk_button_new_with_label ("Child 1");
  gtk_widget_set_parent (self->button1, widget);
  gtk_widget_set_name (self->button1, "button1");

  self->button2 = gtk_button_new_with_label ("Child 2");
  gtk_widget_set_parent (self->button2, widget);
  gtk_widget_set_name (self->button2, "button2");

  self->button3 = gtk_button_new_with_label ("Child 3");
  gtk_widget_set_parent (self->button3, widget);
  gtk_widget_set_name (self->button3, "button3");

  GtkLayoutManager *manager = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  build_constraints (self, GTK_CONSTRAINT_LAYOUT (manager));

  drag = gtk_gesture_drag_new ();
  g_signal_connect (drag, "drag-update", G_CALLBACK (drag_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drag));
}

GtkWidget *
do_constraints_interactive (GtkWidget *do_widget)
{
 static GtkWidget *window;

 if (!window)
   {
     GtkWidget *box, *grid;

     window = gtk_window_new ();
     gtk_window_set_display (GTK_WINDOW (window), gtk_widget_get_display (do_widget));
     gtk_window_set_title (GTK_WINDOW (window), "Interactive Constraints");
     gtk_window_set_default_size (GTK_WINDOW (window), 260, -1);
     g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

     box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
     gtk_window_set_child (GTK_WINDOW (window), box);

     grid = g_object_new (interactive_grid_get_type (), NULL);
     gtk_widget_set_hexpand (grid, TRUE);
     gtk_widget_set_vexpand (grid, TRUE);
     gtk_box_append (GTK_BOX (box), grid);
   }

 if (!gtk_widget_get_visible (window))
   gtk_widget_set_visible (window, TRUE);
 else
   gtk_window_destroy (GTK_WINDOW (window));

 return window;
}
