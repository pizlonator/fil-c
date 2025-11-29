/* Sliding Puzzle
 * #Keywords: GdkPaintable, GdkGesture, GtkShortcutController, game
 *
 * This demo demonstrates how to use gestures and paintables to create a
 * small sliding puzzle game.
 */

#include "config.h"
#include <gtk/gtk.h>

/* Include the header for the puzzle piece */
#include "puzzlepiece.h"
#include "paintable.h"


static GtkWidget *window = NULL;
static GtkWidget *frame = NULL;
static GtkWidget *choices = NULL;
static GtkWidget *size_spin = NULL;
static GdkPaintable *puzzle = NULL;

static gboolean solved = TRUE;
static guint width = 3;
static guint height = 3;
static guint pos_x;
static guint pos_y;

static void
ended (GObject *object)
{
  g_object_unref (object);
}

static void
celebrate (gboolean win)
{
  char *path;
  GtkMediaStream *stream;

  if (win)
    path = g_build_filename (GTK_DATADIR, "sounds", "freedesktop", "stereo", "complete.oga", NULL);
  else
    path = g_build_filename (GTK_DATADIR, "sounds", "freedesktop", "stereo", "suspend-error.oga", NULL);
  stream = gtk_media_file_new_for_filename (path);
  gtk_media_stream_set_volume (stream, 1.0);
  gtk_media_stream_play (stream);

  g_signal_connect (stream, "notify::ended", G_CALLBACK (ended), NULL);
  g_free (path);
}

static gboolean
move_puzzle (GtkWidget *grid,
             int        dx,
             int        dy)
{
  GtkWidget *pos, *next;
  GdkPaintable *piece;
  guint next_x, next_y;

  /* We don't move anything if the puzzle is solved */
  if (solved)
    return FALSE;

  /* Return FALSE if we can't move to where the call 
   * wants us to move.
   */
  if ((dx < 0 && pos_x < -dx) ||
      dx + pos_x >= width ||
      (dy < 0 && pos_y < -dy) ||
      dy + pos_y >= height)
    return FALSE;

  /* Compute the new position */
  next_x = pos_x + dx;
  next_y = pos_y + dy;

  /* Get the current and next image */
  pos = gtk_grid_get_child_at (GTK_GRID (grid), pos_x, pos_y);
  next = gtk_grid_get_child_at (GTK_GRID (grid), next_x, next_y);

  /* Move the displayed piece. */
  piece = gtk_picture_get_paintable (GTK_PICTURE (next));
  gtk_picture_set_paintable (GTK_PICTURE (pos), piece);
  gtk_picture_set_paintable (GTK_PICTURE (next), NULL);

  /* Update the current position */
  pos_x = next_x;
  pos_y = next_y;

  /* Return TRUE because we successfully moved the piece */
  return TRUE;
}

static void
shuffle_puzzle (GtkWidget *grid)
{
  guint i, n_steps;

  /* Do this many random moves */
  n_steps = width * height * 50;

  for (i = 0; i < n_steps; i++)
    {
      /* Get a random number for the direction to move in */
      switch (g_random_int_range (0, 4))
        {
        case 0:
          /* left */
          move_puzzle (grid, -1, 0);
          break;

        case 1:
          /* up */
          move_puzzle (grid, 0, -1);
          break;

        case 2:
          /* right */
          move_puzzle (grid, 1, 0);
          break;

        case 3:
          /* down */
          move_puzzle (grid, 0, 1);
          break;

        default:
          g_assert_not_reached ();
          continue;
        }
    }
}

static gboolean
check_solved (GtkWidget *grid)
{
  GtkWidget *picture;
  GdkPaintable *piece;
  guint x, y;

  /* Nothing to check if the puzzle is already solved */
  if (solved)
    return TRUE;

  /* If the empty cell isn't in the bottom right,
   * the puzzle is obviously not solved */
  if (pos_x != width - 1 ||
      pos_y != height - 1)
    return FALSE;

  /* Check that all pieces are in the right position */
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          picture = gtk_grid_get_child_at (GTK_GRID (grid), x, y);
          piece = gtk_picture_get_paintable (GTK_PICTURE (picture));

          /* empty cell */
          if (piece == NULL)
            continue;

          if (gtk_puzzle_piece_get_x (GTK_PUZZLE_PIECE (piece)) != x ||
              gtk_puzzle_piece_get_y (GTK_PUZZLE_PIECE (piece)) != y)
            return FALSE;
        }
    }

  /* We solved the puzzle!
   */
  solved = TRUE;

  /* Fill the empty cell to show that we're done.
   */
  picture = gtk_grid_get_child_at (GTK_GRID (grid), 0, 0);
  piece = gtk_picture_get_paintable (GTK_PICTURE (picture));

  piece = gtk_puzzle_piece_new (gtk_puzzle_piece_get_puzzle (GTK_PUZZLE_PIECE (piece)),
                                pos_x, pos_y,
                                width, height);
  picture = gtk_grid_get_child_at (GTK_GRID (grid), pos_x, pos_y);
  gtk_picture_set_paintable (GTK_PICTURE (picture), piece);

  celebrate (TRUE);

  return TRUE;
}

static gboolean
puzzle_key_pressed (GtkWidget *grid,
                    GVariant  *args,
                    gpointer   unused)
{
  int dx, dy;

  g_variant_get (args, "(ii)", &dx, &dy);

  if (!move_puzzle (grid, dx, dy))
    {
      /* Make the error sound and then return TRUE.
       * We handled this key, even though we didn't
       * do anything to the puzzle.
       */
      gtk_widget_error_bell (grid);
      return TRUE;
    }

  check_solved (grid);

  return TRUE;
}

static void
puzzle_button_pressed (GtkGestureClick *gesture,
                       int              n_press,
                       double           x,
                       double           y,
                       GtkWidget       *grid)
{
  GtkWidget *child;
  int l, t, i;
  int pos;

  child = gtk_widget_pick (grid, x, y, GTK_PICK_DEFAULT);

  if (!child)
    {
      gtk_widget_error_bell (grid);
      return;
    }

  gtk_grid_query_child (GTK_GRID (grid), child, &l, &t, NULL, NULL);

  if (l == pos_x && t == pos_y)
    {
      gtk_widget_error_bell (grid);
    }
  else if (l == pos_x)
    {
      pos = pos_y;
      for (i = t; i < pos; i++)
        {
          if (!move_puzzle (grid, 0, -1))
            gtk_widget_error_bell (grid);
        }
      for (i = pos; i < t; i++)
        {
          if (!move_puzzle (grid, 0, 1))
            gtk_widget_error_bell (grid);
        }
    }
  else if (t == pos_y)
    {
      pos = pos_x;
      for (i = l; i < pos; i++)
        {
          if (!move_puzzle (grid, -1, 0))
            gtk_widget_error_bell (grid);
        }
      for (i = pos; i < l; i++)
        {
          if (!move_puzzle (grid, 1, 0))
            gtk_widget_error_bell (grid);
        }
    }
  else
    {
      gtk_widget_error_bell (grid);
    }

  check_solved (grid);
}

static void
add_move_binding (GtkShortcutController *controller,
                  guint                  keyval,
                  guint                  kp_keyval,
                  int                    dx,
                  int                    dy)
{
  GtkShortcut *shortcut;

  shortcut = gtk_shortcut_new_with_arguments (
                 gtk_alternative_trigger_new (gtk_keyval_trigger_new (keyval, 0),
                                              gtk_keyval_trigger_new (kp_keyval, 0)),
                 gtk_callback_action_new (puzzle_key_pressed, NULL, NULL),
                 "(ii)", dx, dy);
  gtk_shortcut_controller_add_shortcut (controller, shortcut);
}

static void
start_puzzle (GdkPaintable *paintable)
{
  GtkWidget *picture, *grid;
  GtkEventController *controller;
  guint x, y;
  float aspect_ratio;

  /* Create a new grid */
  grid = gtk_grid_new ();
  gtk_widget_set_focusable (grid, TRUE);
  gtk_aspect_frame_set_child (GTK_ASPECT_FRAME (frame), grid);
  aspect_ratio = gdk_paintable_get_intrinsic_aspect_ratio (paintable);
  if (aspect_ratio == 0.0)
    aspect_ratio = 1.0;
  gtk_aspect_frame_set_ratio (GTK_ASPECT_FRAME (frame), aspect_ratio);
  gtk_aspect_frame_set_obey_child (GTK_ASPECT_FRAME (frame), FALSE);

  /* Add shortcuts so people can use the arrow
   * keys to move the puzzle
   */
  controller = gtk_shortcut_controller_new ();
  gtk_shortcut_controller_set_scope (GTK_SHORTCUT_CONTROLLER (controller),
                                     GTK_SHORTCUT_SCOPE_LOCAL);
  add_move_binding (GTK_SHORTCUT_CONTROLLER (controller),
                    GDK_KEY_Left, GDK_KEY_KP_Left,
                    -1, 0);
  add_move_binding (GTK_SHORTCUT_CONTROLLER (controller),
                    GDK_KEY_Right, GDK_KEY_KP_Right,
                    1, 0);
  add_move_binding (GTK_SHORTCUT_CONTROLLER (controller),
                    GDK_KEY_Up, GDK_KEY_KP_Up,
                    0, -1);
  add_move_binding (GTK_SHORTCUT_CONTROLLER (controller),
                    GDK_KEY_Down, GDK_KEY_KP_Down,
                    0, 1);
  gtk_widget_add_controller (GTK_WIDGET (grid), controller);

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  g_signal_connect (controller, "pressed",
                    G_CALLBACK (puzzle_button_pressed),
                    grid);
  gtk_widget_add_controller (GTK_WIDGET (grid), controller);

  /* Make sure the cells have equal size */
  gtk_grid_set_row_homogeneous (GTK_GRID (grid), TRUE);
  gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);

  /* Reset the variables */
  solved = FALSE;
  pos_x = width - 1;
  pos_y = height - 1;

  /* add a picture for every cell */
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          GdkPaintable *piece;

          /* Don't paint anything for the lsiding part of the video */
          if (x == pos_x && y == pos_y)
            piece = NULL;
          else
            piece = gtk_puzzle_piece_new (paintable,
                                          x, y,
                                          width, height);
          picture = gtk_picture_new_for_paintable (piece);
          gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_FILL);
          gtk_grid_attach (GTK_GRID (grid),
                           picture,
                           x, y,
                           1, 1);
        }
    }

  shuffle_puzzle (grid);
}

static void
reshuffle (void)
{
  GtkWidget *grid;

  if (solved)
    {
      start_puzzle (puzzle);
      grid = gtk_aspect_frame_get_child (GTK_ASPECT_FRAME (frame));
    }
  else
    {
      grid = gtk_aspect_frame_get_child (GTK_ASPECT_FRAME (frame));
      shuffle_puzzle (grid);
    }
  gtk_widget_grab_focus (grid);
}

static void
reconfigure (void)
{
  GtkWidget *popover;
  GtkWidget *grid;
  GtkWidget *child;
  GtkWidget *image;
  GList *selected;

  width = height = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (size_spin));

  selected = gtk_flow_box_get_selected_children (GTK_FLOW_BOX (choices));
  if (selected == NULL)
    child = gtk_widget_get_first_child (choices);
  else
    {
      child = selected->data;
      g_list_free (selected);
    }

  image = gtk_flow_box_child_get_child (GTK_FLOW_BOX_CHILD (child));
  puzzle = gtk_image_get_paintable (GTK_IMAGE (image));

  start_puzzle (puzzle);
  popover = gtk_widget_get_ancestor (size_spin, GTK_TYPE_POPOVER);
  gtk_popover_popdown (GTK_POPOVER (popover));
  grid = gtk_aspect_frame_get_child (GTK_ASPECT_FRAME (frame));
  gtk_widget_grab_focus (grid);
}

static void
add_choice (GtkWidget    *container,
            GdkPaintable *paintable)
{
  GtkWidget *icon;

  icon = gtk_image_new_from_paintable (paintable);
  gtk_image_set_icon_size (GTK_IMAGE (icon), GTK_ICON_SIZE_LARGE);

  gtk_flow_box_insert (GTK_FLOW_BOX (container), icon, -1);
}

static void
widget_destroyed (gpointer data,
                  GObject *widget)
{
  if (data)
    *(gpointer *) data = NULL;
}


GtkWidget *
do_sliding_puzzle (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *header;
      GtkWidget *restart;
      GtkWidget *tweak;
      GtkWidget *popover;
      GtkWidget *tweaks;
      GtkWidget *apply;
      GtkWidget *label;
      GtkWidget *sw;
      GtkMediaStream *media;

      puzzle = GDK_PAINTABLE (gdk_texture_new_from_resource ("/sliding_puzzle/portland-rose.jpg"));

      tweaks = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (tweaks), 10);
      gtk_grid_set_column_spacing (GTK_GRID (tweaks), 10);
      gtk_widget_set_margin_start (tweaks, 10);
      gtk_widget_set_margin_end (tweaks, 10);
      gtk_widget_set_margin_top (tweaks, 10);
      gtk_widget_set_margin_bottom (tweaks, 10);

      choices = gtk_flow_box_new ();
      gtk_widget_add_css_class (choices, "view");
      add_choice (choices, puzzle);
      add_choice (choices, gtk_nuclear_animation_new (TRUE));
      media = gtk_media_file_new_for_resource ("/images/gtk-logo.webm");
      gtk_media_stream_set_loop (media, TRUE);
      gtk_media_stream_set_muted (media, TRUE);
      gtk_media_stream_play (media);
      add_choice (choices, GDK_PAINTABLE (media));
      sw = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), choices);
      gtk_grid_attach (GTK_GRID (tweaks), sw, 0, 0, 2, 1);

      label = gtk_label_new ("Size");
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_grid_attach (GTK_GRID (tweaks), label, 0, 1, 1, 1);
      size_spin = gtk_spin_button_new_with_range (2, 10, 1);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (size_spin), width);
      gtk_grid_attach (GTK_GRID (tweaks), size_spin, 1, 1, 1, 1);

      apply = gtk_button_new_with_label ("Apply");
      gtk_widget_set_halign (apply, GTK_ALIGN_END);
      gtk_grid_attach (GTK_GRID (tweaks), apply, 1, 2, 1, 1);
      g_signal_connect (apply, "clicked", G_CALLBACK (reconfigure), NULL);

      popover = gtk_popover_new ();
      gtk_popover_set_child (GTK_POPOVER (popover), tweaks);

      tweak = gtk_menu_button_new ();
      gtk_menu_button_set_popover (GTK_MENU_BUTTON (tweak), popover);
      gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (tweak), "emblem-system-symbolic");

      restart = gtk_button_new_from_icon_name ("view-refresh-symbolic");
      g_signal_connect (restart, "clicked", G_CALLBACK (reshuffle), NULL);

      header = gtk_header_bar_new ();
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), restart);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), tweak);
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Sliding Puzzle");
      gtk_window_set_titlebar (GTK_WINDOW (window), header);
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
      g_object_weak_ref (G_OBJECT (window), widget_destroyed, &window);

      frame = gtk_aspect_frame_new (0.5, 0.5, (float) gdk_paintable_get_intrinsic_aspect_ratio (puzzle), FALSE);
      gtk_window_set_child (GTK_WINDOW (window), frame);

      start_puzzle (puzzle);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
