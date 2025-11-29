/* -*- mode: C; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#include <gtk/gtk.h>

#include "frame-stats.h"


static gboolean no_auto_scroll = FALSE;
static gint n_columns = 20;
static double scroll_pages = 0;


/* This is our dummy item for the model. */
#define DATA_TABLE_TYPE_ITEM (data_table_item_get_type ())
G_DECLARE_FINAL_TYPE (DataTableItem, data_table_item, DATA_TABLE, ITEM, GObject)

struct _DataTableItem
{
  GObject parent_instance;
  int data;
};

struct _DataTableItemClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (DataTableItem, data_table_item, G_TYPE_OBJECT)

static void data_table_item_init (DataTableItem *item) {}

static void data_table_item_class_init (DataTableItemClass *class) {}

static DataTableItem *
data_table_item_new (int data)
{
  DataTableItem *item = g_object_new (DATA_TABLE_TYPE_ITEM, NULL);
  item->data = data;
  return item;
}


static void
set_adjustment_to_fraction (GtkAdjustment *adjustment,
                            double         fraction)
{
  double upper = gtk_adjustment_get_upper (adjustment);
  double lower = gtk_adjustment_get_lower (adjustment);
  double page_size = gtk_adjustment_get_page_size (adjustment);

  gtk_adjustment_set_value (adjustment,
                            (1 - fraction) * lower +
                            fraction * (upper - page_size));
}

static void
move_adjustment_by_pages (GtkAdjustment *adjustment,
                          double         n_pages)
{
  double page_size = gtk_adjustment_get_page_size (adjustment);
  double value = gtk_adjustment_get_value (adjustment);

  value += page_size * n_pages;
  /* the adjustment will clamp properly */
  gtk_adjustment_set_value (adjustment, value);
}

static gboolean
scroll_column_view (GtkWidget     *column_view,
                    GdkFrameClock *frame_clock,
                    gpointer       user_data)
{
  GtkAdjustment *vadjustment;

  vadjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (column_view));

  if (scroll_pages == 0.0)
    set_adjustment_to_fraction (vadjustment, g_random_double ());
  else
    move_adjustment_by_pages (vadjustment, (g_random_double () * 2 - 1) * scroll_pages);

  return TRUE;
}

enum WidgetType
{
  WIDGET_TYPE_NONE,
  WIDGET_TYPE_LABEL,
  WIDGET_TYPE_TEXT,
  WIDGET_TYPE_INSCRIPTION,
};

static enum WidgetType widget_type = WIDGET_TYPE_INSCRIPTION;

static void
setup (GtkSignalListItemFactory *factory,
       GObject                  *listitem)
{
  GtkWidget *widget;

  switch (widget_type)
    {
    case WIDGET_TYPE_NONE:
      /* It's actually a box, just to request size similar to labels. */
      widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_set_size_request (widget, 50, 18);
      break;
    case WIDGET_TYPE_LABEL:
      widget = gtk_label_new ("");
      break;
    case WIDGET_TYPE_TEXT:
      widget = gtk_text_new ();
      break;
    case WIDGET_TYPE_INSCRIPTION:
      widget = gtk_inscription_new ("");
      gtk_inscription_set_min_chars (GTK_INSCRIPTION (widget), 6);
      break;
    default:
      g_assert_not_reached ();
    }

  gtk_list_item_set_child (GTK_LIST_ITEM (listitem), widget);
}

static void
bind (GtkSignalListItemFactory *factory,
      GObject                  *listitem,
      gpointer                  name)
{
  GtkWidget *widget;
  GObject *item;

  widget = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  item = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));

  char buffer[16] = { 0, };
  g_snprintf (buffer,
              sizeof (buffer),
              "%c%d",
              GPOINTER_TO_INT (name),
              DATA_TABLE_ITEM (item)->data);

  switch (widget_type)
    {
    case WIDGET_TYPE_NONE:
      break;
    case WIDGET_TYPE_LABEL:
      gtk_label_set_label (GTK_LABEL (widget), buffer);
      break;
    case WIDGET_TYPE_TEXT:
      gtk_editable_set_text (GTK_EDITABLE (widget), buffer);
      break;
    case WIDGET_TYPE_INSCRIPTION:
      gtk_inscription_set_text (GTK_INSCRIPTION (widget), buffer);
      break;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
parse_widget_arg (const gchar* option_name,
                  const gchar* value,
                  gpointer data,
                  GError** error)
{
  if (!g_strcmp0 (value, "none"))
    {
      widget_type = WIDGET_TYPE_NONE;
      return TRUE;
    }
  else if (!g_strcmp0 (value, "label"))
    {
      widget_type = WIDGET_TYPE_LABEL;
      return TRUE;
    }
  else if (!g_strcmp0 (value, "text"))
    {
      widget_type = WIDGET_TYPE_TEXT;
      return TRUE;
    }
  else if (!g_strcmp0 (value, "inscription"))
    {
      widget_type = WIDGET_TYPE_INSCRIPTION;
      return TRUE;
    }
  else
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                           "Invalid option value");
      return FALSE;
    }
}

static GOptionEntry options[] = {
  {
    "widget",
    'w',
    G_OPTION_FLAG_NONE,
    G_OPTION_ARG_CALLBACK,
    parse_widget_arg,
    "Cell item widget to use, can be one of: none, label, text, inscription",
    "WIDGET"
  },
  {
    "no-auto-scroll",
    'n',
    G_OPTION_FLAG_NONE,
    G_OPTION_ARG_NONE,
    &no_auto_scroll,
    "Disable automatic scrolling",
    NULL
  },
  {
    "columns",
    'c',
    G_OPTION_FLAG_NONE,
    G_OPTION_ARG_INT,
    &n_columns,
    "Column count",
    "COUNT"
  },
  {
    "pages",
    'p',
    G_OPTION_FLAG_NONE,
    G_OPTION_ARG_DOUBLE,
    &scroll_pages,
    "Maximum number of pages to scroll (or 0 for random)",
    "COUNT"
  },
  { NULL }
};

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *scrolled_window;
  GListStore *store;
  int i;
  GtkMultiSelection *multi_selection;
  GtkWidget *column_view;
  GError *error = NULL;
  gboolean done = FALSE;

  GOptionContext *context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, options, NULL);
  frame_stats_add_options (g_option_context_get_main_group (context));

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  gtk_init ();

  window = gtk_window_new ();
  frame_stats_ensure (GTK_WINDOW (window));
  gtk_window_set_default_size (GTK_WINDOW (window), 1700, 900);

  scrolled_window = gtk_scrolled_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), scrolled_window);

  store = g_list_store_new (DATA_TABLE_TYPE_ITEM);
  for (i = 0; i < 10000; ++i)
    {
      DataTableItem *item = data_table_item_new (i);
      g_list_store_append (store, item);
      g_object_unref (item);
    }

  multi_selection = gtk_multi_selection_new (G_LIST_MODEL (store));
  column_view = gtk_column_view_new (GTK_SELECTION_MODEL (multi_selection));

  gtk_column_view_set_show_column_separators (GTK_COLUMN_VIEW (column_view), TRUE);
  gtk_column_view_set_show_row_separators (GTK_COLUMN_VIEW (column_view), TRUE);
  gtk_widget_add_css_class (column_view, "data-table");

  for (i = 0; i < MIN (n_columns, 127 - 65); ++i)
    {
      const char name[] = { 'A' + i, '\0' };

      GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind), GINT_TO_POINTER (name[0]));

      gtk_column_view_append_column (GTK_COLUMN_VIEW (column_view),
                                     gtk_column_view_column_new (name, factory));
    }

  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window),
                                 column_view);

  if (!no_auto_scroll)
    {
      gtk_widget_add_tick_callback (column_view,
                                    scroll_column_view,
                                    NULL,
                                    NULL);
    }

  gtk_window_present (GTK_WINDOW (window));
  g_signal_connect (window, "destroy",
                    G_CALLBACK (quit_cb), &done);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
