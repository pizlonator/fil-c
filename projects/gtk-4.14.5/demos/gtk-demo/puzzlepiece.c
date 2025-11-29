/* Paintable/A simple paintable
 *
 * GdkPaintable is an interface used by GTK for drawings of any sort
 * that do not require layouting or positioning.
 *
 * This demo code gives a simple example on how a paintable can
 * be created.
 *
 * Paintables can be used in many places inside GTK widgets, but the
 * most common usage is inside GtkImage and that's what we're going
 * to do here.
 */

#include <gtk/gtk.h>

#include "puzzlepiece.h"

/* Declare the struct. */
struct _GtkPuzzlePiece
{
  GObject parent_instance;

  GdkPaintable *puzzle;
  guint x;
  guint y;
  guint width;
  guint height;
};

struct _GtkPuzzlePieceClass
{
  GObjectClass parent_class;
};

/* This is the function that draws the puzzle piece.
 * It just draws a rectangular cutout of the puzzle by clipping
 * away the rest.
 */
static void
gtk_puzzle_piece_snapshot (GdkPaintable *paintable,
                           GdkSnapshot  *snapshot,
                           double        width,
                           double        height)
{
  GtkPuzzlePiece *self = GTK_PUZZLE_PIECE (paintable);

  gtk_snapshot_push_clip (snapshot,
                          &GRAPHENE_RECT_INIT (0, 0, width, height));

  gtk_snapshot_translate (snapshot,
                          &GRAPHENE_POINT_INIT (
                              - width * self->x,
                              - height * self->y
                          ));
  gdk_paintable_snapshot (self->puzzle,
                          snapshot,
                          width * self->width,
                          height * self->height);

  gtk_snapshot_pop (snapshot);
}

static GdkPaintableFlags
gtk_puzzle_piece_get_flags (GdkPaintable *paintable)
{
  GtkPuzzlePiece *self = GTK_PUZZLE_PIECE (paintable);

  /* The flags are the same as the ones of the puzzle.
   * If the puzzle changes in some way, so do the pieces.
   */
  return gdk_paintable_get_flags (self->puzzle);
}

static int
gtk_puzzle_piece_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkPuzzlePiece *self = GTK_PUZZLE_PIECE (paintable);

  /* We can compute our width relative to the puzzle.
   * This logic even works for the case where the puzzle
   * has no width, because the 0 return value is unchanged.
   * Round up the value.
   */
  return (gdk_paintable_get_intrinsic_width (self->puzzle) + self->width - 1) / self->width;
}

static int
gtk_puzzle_piece_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkPuzzlePiece *self = GTK_PUZZLE_PIECE (paintable);

  /* Do the same thing we did for the width with the height.
   */
  return (gdk_paintable_get_intrinsic_height (self->puzzle) + self->height - 1) / self->height;
}

static double
gtk_puzzle_piece_get_intrinsic_aspect_ratio (GdkPaintable *paintable)
{
  GtkPuzzlePiece *self = GTK_PUZZLE_PIECE (paintable);

  /* We can compute our aspect ratio relative to the puzzle.
   * This logic again works for the case where the puzzle
   * has no aspect ratio, because the 0 return value is unchanged.
   */
  return gdk_paintable_get_intrinsic_aspect_ratio (self->puzzle) * self->height / self->width;
}

static void
gtk_puzzle_piece_paintable_init (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_puzzle_piece_snapshot;
  iface->get_flags = gtk_puzzle_piece_get_flags;
  iface->get_intrinsic_width = gtk_puzzle_piece_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_puzzle_piece_get_intrinsic_height;
  iface->get_intrinsic_aspect_ratio = gtk_puzzle_piece_get_intrinsic_aspect_ratio;
}

/* When defining the GType, we need to implement the GdkPaintable interface */
G_DEFINE_TYPE_WITH_CODE (GtkPuzzlePiece, gtk_puzzle_piece, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_puzzle_piece_paintable_init))

/* We need to declare a destructor to release our reference to the
 * puzzle paintable and disconnect our signal handlers.
 */
static void
gtk_puzzle_piece_dispose (GObject *object)
{
  GtkPuzzlePiece *self = GTK_PUZZLE_PIECE (object);

  if (self->puzzle)
    {
      g_signal_handlers_disconnect_by_func (self->puzzle, gdk_paintable_invalidate_contents, self);
      g_signal_handlers_disconnect_by_func (self->puzzle, gdk_paintable_invalidate_size, self);
      g_clear_object (&self->puzzle);
    }

  G_OBJECT_CLASS (gtk_puzzle_piece_parent_class)->dispose (object);
}

/* Here's the boilerplate for the GObject declaration.
 */
static void
gtk_puzzle_piece_class_init (GtkPuzzlePieceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_puzzle_piece_dispose;
}

static void
gtk_puzzle_piece_init (GtkPuzzlePiece *self)
{
}

/* And finally, we add a constructor.
 * It is declared in the header so that the other examples
 * can use it.
 */
GdkPaintable *
gtk_puzzle_piece_new (GdkPaintable *puzzle,
                      guint         x,
                      guint         y,
                      guint         width,
                      guint         height)
{
  GtkPuzzlePiece *self;

  /* These are sanity checks, so that we get warnings if we accidentally
   * do anything stupid. */
  g_return_val_if_fail (GDK_IS_PAINTABLE (puzzle), NULL);
  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);
  g_return_val_if_fail (x < width, NULL);
  g_return_val_if_fail (y < height, NULL);

  self = g_object_new (GTK_TYPE_PUZZLE_PIECE, NULL);

  self->puzzle = g_object_ref (puzzle);
  g_signal_connect_swapped (puzzle, "invalidate-contents", G_CALLBACK (gdk_paintable_invalidate_contents), self);
  g_signal_connect_swapped (puzzle, "invalidate-size", G_CALLBACK (gdk_paintable_invalidate_size), self);
  self->x = x;
  self->y = y;
  self->width = width;
  self->height = height;

  return GDK_PAINTABLE (self);
}

/* Here are the accessors that we need to inspect the puzzle
 * pieces in other code.
 */
GdkPaintable *
gtk_puzzle_piece_get_puzzle (GtkPuzzlePiece *self)
{
  /* Add sanity checks here, too.
   * If you make a habit out of this, you can always rely
   * on your code having sanity checks, which makes it
   * way easier to debug.
   */
  g_return_val_if_fail (GTK_IS_PUZZLE_PIECE (self), NULL);

  return self->puzzle;
}

guint
gtk_puzzle_piece_get_x (GtkPuzzlePiece *self)
{
  g_return_val_if_fail (GTK_IS_PUZZLE_PIECE (self), 0);

  return self->x;
}

guint
gtk_puzzle_piece_get_y (GtkPuzzlePiece *self)
{
  g_return_val_if_fail (GTK_IS_PUZZLE_PIECE (self), 0);

  return self->y;
}

