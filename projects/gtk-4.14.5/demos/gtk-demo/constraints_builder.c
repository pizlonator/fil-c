/* Constraints/Builder
 *
 * GtkConstraintLayouts can be created in .ui files, and constraints can
 * be set up at that time as well, as this example demonstrates. It shows
 * various ways to do spacing and sizing with constraints.
 *
 * Make the window wider to see the rows react differently
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE (ConstraintsGrid, constraints_grid, CONSTRAINTS, GRID, GtkWidget)

struct _ConstraintsGrid
{
  GtkWidget parent_instance;
};

G_DEFINE_TYPE (ConstraintsGrid, constraints_grid, GTK_TYPE_WIDGET)

static void
constraints_grid_init (ConstraintsGrid *grid)
{
}

static void
constraints_grid_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (widget)))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (constraints_grid_parent_class)->dispose (object);
}

static void
constraints_grid_class_init (ConstraintsGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = constraints_grid_dispose;
}

GtkWidget *
do_constraints_builder (GtkWidget *do_widget)
{
 static GtkWidget *window;

 if (!window)
   {
     GtkBuilder *builder;

     g_type_ensure (constraints_grid_get_type ());

     builder = gtk_builder_new_from_resource ("/constraints_builder/constraints_builder.ui");

     window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
     gtk_window_set_display (GTK_WINDOW (window),
                             gtk_widget_get_display (do_widget));
     g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

     g_object_unref (builder);
   }

 if (!gtk_widget_get_visible (window))
   gtk_widget_set_visible (window, TRUE);
 else
   gtk_window_destroy (GTK_WINDOW (window));

 return window;
}
