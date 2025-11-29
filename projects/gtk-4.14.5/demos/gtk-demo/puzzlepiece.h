#pragma once

#include <gtk/gtk.h>

/* First, add the boilerplate for the object itself.
 */
#define GTK_TYPE_PUZZLE_PIECE (gtk_puzzle_piece_get_type ())
G_DECLARE_FINAL_TYPE (GtkPuzzlePiece, gtk_puzzle_piece, GTK, PUZZLE_PIECE, GObject)

/* Then, declare all constructors */
GdkPaintable *  gtk_puzzle_piece_new            (GdkPaintable           *puzzle,
                                                 guint                   x,
                                                 guint                   y,
                                                 guint                   width,
                                                 guint                   height);

/* Next, add the getters and setters for object properties */
GdkPaintable *  gtk_puzzle_piece_get_puzzle     (GtkPuzzlePiece         *self);
guint           gtk_puzzle_piece_get_x          (GtkPuzzlePiece         *self);
guint           gtk_puzzle_piece_get_y          (GtkPuzzlePiece         *self);
