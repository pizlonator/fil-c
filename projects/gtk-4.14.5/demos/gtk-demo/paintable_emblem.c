/* Paintable/Emblems
 *
 * This demo shows how GdkPaintable can be used to
 * overlay an emblem on top of an icon. The emblem
 * can either be another icon, or an arbitrary
 * paintable.
 */

#include <gtk/gtk.h>

#include "paintable.h"

static GtkWidget *window = NULL;

#define DEMO_TYPE_ICON (demo_icon_get_type ())
G_DECLARE_FINAL_TYPE (DemoIcon, demo_icon, DEMO, ICON, GObject)

struct _DemoIcon
{
  GObject parent_instance;

  GdkPaintable *icon;
  GdkPaintable *emblem;
  GdkPaintableFlags flags;
};

struct _DemoIconClass
{
  GObjectClass parent_class;
};

void
demo_icon_snapshot (GdkPaintable *paintable,
                    GtkSnapshot  *snapshot,
                    double        width,
                    double        height)
{
  DemoIcon *self = DEMO_ICON (paintable);

  gdk_paintable_snapshot (self->icon, snapshot, width, height);
  gtk_snapshot_save (snapshot);
  gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0.5 * width, 0));
  gdk_paintable_snapshot (self->emblem, snapshot, 0.5 * width, 0.5 * height);
  gtk_snapshot_restore (snapshot);
}

static GdkPaintableFlags
demo_icon_get_flags (GdkPaintable *paintable)
{
  DemoIcon *self = DEMO_ICON (paintable);

  return self->flags;
}

static void
demo_icon_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = demo_icon_snapshot;
  iface->get_flags = demo_icon_get_flags;
}

G_DEFINE_TYPE_WITH_CODE (DemoIcon, demo_icon, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                demo_icon_paintable_init))

static void
demo_icon_dispose (GObject *object)
{
  DemoIcon *self = DEMO_ICON (object);

  g_signal_handlers_disconnect_by_func (self->emblem,
                                        gdk_paintable_invalidate_contents,
                                        self);

  g_clear_object (&self->icon);
  g_clear_object (&self->emblem);

  G_OBJECT_CLASS (demo_icon_parent_class)->dispose (object);
}

static void
demo_icon_class_init (DemoIconClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = demo_icon_dispose;
}

static void
demo_icon_init (DemoIcon *self)
{
  self->flags = GDK_PAINTABLE_STATIC_SIZE | GDK_PAINTABLE_STATIC_CONTENTS;
}

GdkPaintable *
demo_icon_new_with_paintable (const char   *icon_name,
                              GdkPaintable *emblem)
{
  GtkIconTheme *theme;
  GtkIconPaintable *icon;
  DemoIcon *self;

  theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());

  icon = gtk_icon_theme_lookup_icon (theme,
                                     icon_name, NULL,
                                     128, 1,
                                     GTK_TEXT_DIR_LTR, 0);

  self = g_object_new (DEMO_TYPE_ICON, NULL);

  self->icon = GDK_PAINTABLE (icon);
  self->emblem = emblem;

  if ((gdk_paintable_get_flags (emblem) & GDK_PAINTABLE_STATIC_CONTENTS) == 0)
    {
      self->flags &= ~GDK_PAINTABLE_STATIC_CONTENTS;

      g_signal_connect_swapped (emblem, "invalidate-contents",
                                G_CALLBACK (gdk_paintable_invalidate_contents), self);
    }

  return GDK_PAINTABLE (self);
}

GdkPaintable *
demo_icon_new (const char *icon_name,
               const char *emblem_name)
{
  GtkIconTheme *theme;
  GtkIconPaintable *emblem;

  theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());

  emblem = gtk_icon_theme_lookup_icon (theme,
                                       emblem_name, NULL,
                                       128, 1,
                                       GTK_TEXT_DIR_LTR, 0);

  return GDK_PAINTABLE (demo_icon_new_with_paintable (icon_name,
                                                      GDK_PAINTABLE (emblem)));
}

GtkWidget *
do_paintable_emblem (GtkWidget *do_widget)
{
  GdkPaintable *icon;
  GtkWidget *grid;
  GtkWidget *image;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Paintable — Emblems");
      gtk_window_set_default_size (GTK_WINDOW (window), 300, 200);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      grid = gtk_grid_new ();

      icon = demo_icon_new ("folder", "starred");
      image = gtk_image_new_from_paintable (icon);
      gtk_widget_set_hexpand (image, TRUE);
      gtk_widget_set_vexpand (image, TRUE);
      gtk_grid_attach (GTK_GRID (grid), image, 0, 0, 1, 1);

      icon = demo_icon_new_with_paintable ("drive-multidisk",
                                           gtk_nuclear_animation_new (FALSE));
      image = gtk_image_new_from_paintable (icon);
      gtk_widget_set_hexpand (image, TRUE);
      gtk_widget_set_vexpand (image, TRUE);
      gtk_grid_attach (GTK_GRID (grid), image, 1, 0, 1, 1);

      gtk_window_set_child (GTK_WINDOW (window), grid);
      g_object_unref (icon);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}

