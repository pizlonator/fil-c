/* Lists/Minesweeper
 * #Keywords: GtkGridView, GListModel, game
 *
 * This demo shows how to develop a user interface for small game using a
 * grid view.
 *
 * It demonstrates how to use the activate signal and single-press behavior
 * to implement rather different interaction behavior to a typical list.
 */

#include "config.h"
#include <glib/gi18n.h>
#include <gtk/gtk.h>

/*** The cell object ***/

/* Create an object that holds the data for a cell in the game */
typedef struct _SweeperCell SweeperCell;
struct _SweeperCell
{
  GObject parent_instance;

  gboolean is_mine;
  gboolean is_visible;
  guint neighbor_mines;
};

enum {
  CELL_PROP_0,
  CELL_PROP_LABEL,

  N_CELL_PROPS
};

#define SWEEPER_TYPE_CELL (sweeper_cell_get_type ())
G_DECLARE_FINAL_TYPE (SweeperCell, sweeper_cell, SWEEPER, CELL, GObject);

G_DEFINE_TYPE (SweeperCell, sweeper_cell, G_TYPE_OBJECT);
static GParamSpec *cell_properties[N_CELL_PROPS] = { NULL, };

static const char *
sweeper_cell_get_label (SweeperCell *self)
{
  static const char *minecount_labels[10] = { "", "1", "2", "3", "4", "5", "6", "7", "8", "9" };

  if (!self->is_visible)
    return "?";
  
  if (self->is_mine)
    return "💣";

  return minecount_labels[self->neighbor_mines];
}

static void
sweeper_cell_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  SweeperCell *self = SWEEPER_CELL (object);

  switch (property_id)
    {
    case CELL_PROP_LABEL:
      g_value_set_string (value, sweeper_cell_get_label (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
sweeper_cell_class_init (SweeperCellClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = sweeper_cell_get_property;

  cell_properties[CELL_PROP_LABEL] =
    g_param_spec_string ("label",
                         "label",
                         "label to display for this row",
                         NULL,
                         G_PARAM_READABLE);

  g_object_class_install_properties (gobject_class, N_CELL_PROPS, cell_properties);
}

static void
sweeper_cell_init (SweeperCell *self)
{
}

static void
sweeper_cell_reveal (SweeperCell *self)
{
  if (self->is_visible)
    return;

  self->is_visible = TRUE;

  g_object_notify_by_pspec (G_OBJECT (self), cell_properties[CELL_PROP_LABEL]);
}

static SweeperCell *
sweeper_cell_new (void)
{
  return g_object_new (SWEEPER_TYPE_CELL, NULL);
}

/*** The board object ***/

/* Create an object that holds the data for the game */
typedef struct _SweeperGame SweeperGame;
struct _SweeperGame
{
  GObject parent_instance;

  GPtrArray *cells;
  guint width;
  guint height;
  gboolean playing;
  gboolean win;
};

enum {
  GAME_PROP_0,
  GAME_PROP_HEIGHT,
  GAME_PROP_PLAYING,
  GAME_PROP_WIDTH,
  GAME_PROP_WIN,

  N_GAME_PROPS
};

#define SWEEPER_TYPE_GAME (sweeper_game_get_type ())
G_DECLARE_FINAL_TYPE (SweeperGame, sweeper_game, SWEEPER, GAME, GObject);

static GType
sweeper_game_list_model_get_item_type (GListModel *model)
{
  return SWEEPER_TYPE_GAME;
}

static guint
sweeper_game_list_model_get_n_items (GListModel *model)
{
  SweeperGame *self = SWEEPER_GAME (model);

  return self->width * self->height;
}

static gpointer
sweeper_game_list_model_get_item (GListModel *model,
                                  guint       position)
{
  SweeperGame *self = SWEEPER_GAME (model);

  return g_object_ref (g_ptr_array_index (self->cells, position));
}

static void
sweeper_game_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = sweeper_game_list_model_get_item_type;
  iface->get_n_items = sweeper_game_list_model_get_n_items;
  iface->get_item = sweeper_game_list_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (SweeperGame, sweeper_game, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, sweeper_game_list_model_init))

static GParamSpec *game_properties[N_GAME_PROPS] = { NULL, };

static void
sweeper_game_dispose (GObject *object)
{
  SweeperGame *self = SWEEPER_GAME (object);

  g_clear_pointer (&self->cells, g_ptr_array_unref);

  G_OBJECT_CLASS (sweeper_game_parent_class)->dispose (object);
}

static void
sweeper_game_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  SweeperGame *self = SWEEPER_GAME (object);

  switch (property_id)
    {
    case GAME_PROP_HEIGHT:
      g_value_set_uint (value, self->height);
      break;

    case GAME_PROP_PLAYING:
      g_value_set_boolean (value, self->playing);
      break;

    case GAME_PROP_WIDTH:
      g_value_set_uint (value, self->width);
      break;

    case GAME_PROP_WIN:
      g_value_set_boolean (value, self->win);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
sweeper_game_class_init (SweeperGameClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = sweeper_game_dispose;
  gobject_class->get_property = sweeper_game_get_property;

  game_properties[GAME_PROP_HEIGHT] =
    g_param_spec_uint ("height",
                       "height",
                       "height of the game grid",
                       1, G_MAXUINT, 8,
                       G_PARAM_READABLE);

  game_properties[GAME_PROP_PLAYING] =
    g_param_spec_boolean ("playing",
                          "playing",
                          "if the game is still going on",
                          FALSE,
                          G_PARAM_READABLE);

  game_properties[GAME_PROP_WIDTH] =
    g_param_spec_uint ("width",
                       "width",
                       "width of the game grid",
                       1, G_MAXUINT, 8,
                       G_PARAM_READABLE);

  game_properties[GAME_PROP_WIN] =
    g_param_spec_boolean ("win",
                          "win",
                          "if the game was won",
                          FALSE,
                          G_PARAM_READABLE);

  g_object_class_install_properties (gobject_class, N_GAME_PROPS, game_properties);
}

static void
sweeper_game_reset_board (SweeperGame *self,
                          guint        width,
                          guint        height)
{
  guint i;

  g_ptr_array_set_size (self->cells, 0);

  for (i = 0; i < width * height; i++)
    {
      g_ptr_array_add (self->cells, sweeper_cell_new ());
    }

  if (self->width != width)
    {
      self->width = width;
      g_object_notify_by_pspec (G_OBJECT (self), game_properties[GAME_PROP_WIDTH]);
    }
  if (self->height != height)
    {
      self->height = height;
      g_object_notify_by_pspec (G_OBJECT (self), game_properties[GAME_PROP_HEIGHT]);
    }
  if (!self->playing)
    {
      self->playing = TRUE;
      g_object_notify_by_pspec (G_OBJECT (self), game_properties[GAME_PROP_PLAYING]);
    }
  if (self->win)
    {
      self->win = FALSE;
      g_object_notify_by_pspec (G_OBJECT (self), game_properties[GAME_PROP_WIN]);
    }
}

static void
sweeper_game_place_mines (SweeperGame *self,
                          guint        n_mines)
{
  guint i;

  for (i = 0; i < n_mines; i++)
    {
      SweeperCell *cell;

      do {
        cell = g_ptr_array_index (self->cells, g_random_int_range (0, self->cells->len));
      } while (cell->is_mine);

      cell->is_mine = TRUE;
    }
}

static SweeperCell *
get_cell (SweeperGame *self,
          guint        x,
          guint        y)
{
  return g_ptr_array_index (self->cells, y * self->width + x);
}

static void
sweeper_game_count_neighbor_mines (SweeperGame *self,
                                   guint        width,
                                   guint        height)
{
  guint x, y, x2, y2;

  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          SweeperCell *cell = get_cell (self, x, y);

          for (y2 = MAX (1, y) - 1; y2 < MIN (height, y + 2); y2++)
            {
              for (x2 = MAX (1, x) - 1; x2 < MIN (width, x + 2); x2++)
                {
                  SweeperCell *other = get_cell (self, x2, y2);

                  if (other->is_mine)
                    cell->neighbor_mines++;
                }
            }
        }
    }
}

static void
sweeper_game_new_game (SweeperGame *self,
                       guint        width,
                       guint        height,
                       guint        n_mines)
{
  guint n_items_before;

  g_return_if_fail (n_mines <= width * height);

  n_items_before = self->width * self->height;

  g_object_freeze_notify (G_OBJECT (self));

  sweeper_game_reset_board (self, width, height);
  sweeper_game_place_mines (self, n_mines);
  sweeper_game_count_neighbor_mines (self, width, height);

  g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items_before, width * height);

  g_object_thaw_notify (G_OBJECT (self));
}

static void
sweeper_game_init (SweeperGame *self)
{
  self->cells = g_ptr_array_new_with_free_func (g_object_unref);

  sweeper_game_new_game (self, 8, 8, 10);
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
  g_signal_connect (stream, "notify::ended", G_CALLBACK (g_object_unref), NULL);
  g_free (path);
}

static void
sweeper_game_end (SweeperGame *self,
                  gboolean     win)
{
  if (self->playing)
    {
      self->playing = FALSE;
      g_object_notify_by_pspec (G_OBJECT (self), game_properties[GAME_PROP_PLAYING]);
      celebrate (win);
    }

  if (self->win != win)
    {
      self->win = win;
      g_object_notify_by_pspec (G_OBJECT (self), game_properties[GAME_PROP_WIN]);
    }
}

static void
sweeper_game_check_finished (SweeperGame *self)
{
  guint i;

  if (!self->playing)
    return;

  for (i = 0; i < self->cells->len; i++)
    {
      SweeperCell *cell = g_ptr_array_index (self->cells, i);

      /* There's still a non-revealed cell that isn't a mine */
      if (!cell->is_visible && !cell->is_mine)
        return;
    }

  sweeper_game_end (self, TRUE);
}

static void
sweeper_game_reveal_cell (SweeperGame *self,
                          guint        position)
{
  SweeperCell *cell;

  if (!self->playing)
    return;

  cell = g_ptr_array_index (self->cells, position);
  sweeper_cell_reveal (cell);

  if (cell->is_mine)
    sweeper_game_end (self, FALSE);

  sweeper_game_check_finished (self);
}

G_MODULE_EXPORT void
minesweeper_cell_clicked_cb (GtkGridView *gridview,
                             guint        pos,
                             SweeperGame *game)
{
  sweeper_game_reveal_cell (game, pos);
}

G_MODULE_EXPORT void
minesweeper_new_game_cb (GtkButton   *button,
                         SweeperGame *game)
{
  sweeper_game_new_game (game, 8, 8, 10);
}

static GtkWidget *window = NULL;

GtkWidget *
do_listview_minesweeper (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkBuilder *builder;

      g_type_ensure (SWEEPER_TYPE_GAME);

      builder = gtk_builder_new_from_resource ("/listview_minesweeper/listview_minesweeper.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
