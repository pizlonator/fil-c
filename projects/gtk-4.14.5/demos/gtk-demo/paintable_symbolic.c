/* Paintable/Symbolic Paintable
 *
 * GdkPaintables can be made to follow the theme's colors. GTK calls
 * icons that do this symbolic icons, paintables that want to have
 * the same effect can implement the GtkSymbolicPaintable interface.
 *
 * We will adapt the original paintable example by adding the ability
 * to recolor the paintable based on the symbolic colors.
 */

#include <gtk/gtk.h>

#include "paintable.h"

static GtkWidget *window = NULL;

/* First, add the boilerplate for the object itself.
 * This part would normally go in the header.
 */
#define GTK_TYPE_NUCLEAR_SYMBOLIC (gtk_nuclear_symbolic_get_type ())
G_DECLARE_FINAL_TYPE (GtkNuclearSymbolic, gtk_nuclear_symbolic, GTK, NUCLEAR_SYMBOLIC, GObject)

/* Declare a few warning levels, so we can pick colors based on them */
typedef enum
{
  WARNING_NONE,
  WARNING_ALERT,
  WARNING_EMERGENCY
} WarningLevel;

/* Declare the struct. */
struct _GtkNuclearSymbolic
{
  GObject parent_instance;

  WarningLevel warning_level;
};

struct _GtkNuclearSymbolicClass
{
  GObjectClass parent_class;
};

/* Add a function to draw the nuclear icon in the given colors */
static void
gtk_nuclear_symbolic_snapshot_symbolic (GtkSymbolicPaintable *paintable,
                                        GdkSnapshot          *snapshot,
                                        double                width,
                                        double                height,
                                        const GdkRGBA        *colors,
                                        gsize                 n_colors)
{
  GtkNuclearSymbolic *self = GTK_NUCLEAR_SYMBOLIC (paintable);
  static const GdkRGBA transparent = { 0, };
  const GdkRGBA *bg_color;

  /* select the right background color from the warning level */
  switch (self->warning_level)
  {
    case WARNING_NONE:
      bg_color = &transparent;
      break;
    case WARNING_ALERT:
      bg_color = &colors[GTK_SYMBOLIC_COLOR_WARNING];
      break;
    case WARNING_EMERGENCY:
      bg_color = &colors[GTK_SYMBOLIC_COLOR_ERROR];
      break;
    default:
      /* This should never happen, but we better do defensive coding
       * with this critical icon */
      g_assert_not_reached ();
      bg_color = &transparent;
      break;
  }

  /* Draw the icon with the selected warning color */
  gtk_nuclear_snapshot (snapshot,
                        &colors[GTK_SYMBOLIC_COLOR_FOREGROUND],
                        bg_color,
                        width, height,
                        0);
}

static void
gtk_nuclear_symbolic_symbolic_paintable_init (GtkSymbolicPaintableInterface *iface)
{
  iface->snapshot_symbolic = gtk_nuclear_symbolic_snapshot_symbolic;
}

/* We need to implement the functionality required by the GdkPaintable interface */
static void
gtk_nuclear_symbolic_snapshot (GdkPaintable *paintable,
                                GdkSnapshot  *snapshot,
                                double        width,
                                double        height)
{
  /* Calling this function without passing a color is a neat trick
   * to make GTK use default colors and otherwise forward the call
   * to the snapshotting function above.
   */
  gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable),
                                            snapshot,
                                            width, height,
                                            NULL, 0);
}

static GdkPaintableFlags
gtk_nuclear_symbolic_get_flags (GdkPaintable *paintable)
{
  /* This image has a static size, but the contents may change:
   * We draw different things when the warning level changes.
   */
  return GDK_PAINTABLE_STATIC_SIZE;
}

static void
gtk_nuclear_symbolic_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_nuclear_symbolic_snapshot;
  iface->get_flags = gtk_nuclear_symbolic_get_flags;
}

/* When defining the GType, we need to implement bot the GdkPaintable
 * and the GtkSymbolicPaintable interface */
G_DEFINE_TYPE_WITH_CODE (GtkNuclearSymbolic, gtk_nuclear_symbolic, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_nuclear_symbolic_paintable_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SYMBOLIC_PAINTABLE,
                                                gtk_nuclear_symbolic_symbolic_paintable_init))

static void
gtk_nuclear_symbolic_class_init (GtkNuclearSymbolicClass *klass)
{
}

static void
gtk_nuclear_symbolic_init (GtkNuclearSymbolic *nuclear)
{
}

/* And finally, we add the simple constructor we declared in the header. */
GdkPaintable *
gtk_nuclear_symbolic_new (void)
{
  return g_object_new (GTK_TYPE_NUCLEAR_SYMBOLIC, NULL);
}

/* Add some fun feature to the button */
static void
nuclear_button_clicked (GtkButton          *button,
                        GtkNuclearSymbolic *nuclear)
{
  if (nuclear->warning_level >= WARNING_EMERGENCY)
    {
      /* On maximum warning level, reset the warning */
      nuclear->warning_level = WARNING_NONE;
      /* And sometimes (but not always to confuse people)
       * close the window.
       */
      if (g_random_boolean ())
        gtk_window_close (GTK_WINDOW (window));
    }
  else
    {
      /* Otherwise just increase the warning level */
      nuclear->warning_level++;
    }

  /* Don't forget to emit the signal causing the paintable to redraw.
   * Changing the warning level changes the background color after all.
   */
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (nuclear));
}

GtkWidget *
do_paintable_symbolic (GtkWidget *do_widget)
{
  GdkPaintable *nuclear;
  GtkWidget *image, *button;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Don't click!");
      gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      button = gtk_button_new ();
      gtk_window_set_child (GTK_WINDOW (window), button);

      nuclear = gtk_nuclear_symbolic_new ();
      image = gtk_image_new_from_paintable (nuclear);

      gtk_button_set_child (GTK_BUTTON (button), image);
      g_signal_connect (button, "clicked", G_CALLBACK (nuclear_button_clicked), nuclear);
      g_object_unref (nuclear);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
