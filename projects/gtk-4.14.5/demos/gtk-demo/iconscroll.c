/* Benchmark/Scrolling
 * #Keywords: GtkScrolledWindow
 *
 * This demo scrolls a view with various content.
 */

#include <gtk/gtk.h>

static guint tick_cb;
static GtkAdjustment *hadjustment;
static GtkAdjustment *vadjustment;
static GtkWidget *window = NULL;
static GtkWidget *scrolledwindow;
static int selected;

#define N_WIDGET_TYPES 8


static int hincrement = 5;
static int vincrement = 5;

static gboolean
scroll_cb (GtkWidget *widget,
           GdkFrameClock *frame_clock,
           gpointer data)
{
  double value;

  value = gtk_adjustment_get_value (vadjustment);
  if (value + vincrement <= gtk_adjustment_get_lower (vadjustment) ||
     (value + vincrement >= gtk_adjustment_get_upper (vadjustment) - gtk_adjustment_get_page_size (vadjustment)))
    vincrement = - vincrement;

  gtk_adjustment_set_value (vadjustment, value + vincrement);

  value = gtk_adjustment_get_value (hadjustment);
  if (value + hincrement <= gtk_adjustment_get_lower (hadjustment) ||
     (value + hincrement >= gtk_adjustment_get_upper (hadjustment) - gtk_adjustment_get_page_size (hadjustment)))
    hincrement = - hincrement;

  gtk_adjustment_set_value (hadjustment, value + hincrement);

  return G_SOURCE_CONTINUE;
}

extern GtkWidget *create_icon (void);

static void
populate_icons (void)
{
  GtkWidget *grid;
  int top, left;

  grid = gtk_grid_new ();
  gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (grid, 10);
  gtk_widget_set_margin_end (grid, 10);
  gtk_widget_set_margin_top (grid, 10);
  gtk_widget_set_margin_bottom (grid, 10);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);

  for (top = 0; top < 100; top++)
    for (left = 0; left < 15; left++)
      gtk_grid_attach (GTK_GRID (grid), create_icon (), left, top, 1, 1);

  hincrement = 0;
  vincrement = 5;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), grid);
}

static char *content;
static gsize content_len;

extern void fontify (const char *format, GtkTextBuffer *buffer);

static void
populate_text (gboolean highlight)
{
  GtkWidget *textview;
  GtkTextBuffer *buffer;

  if (!content)
    {
      GBytes *bytes;

      bytes = g_resources_lookup_data ("/sources/font_features.c", 0, NULL);
      content = g_bytes_unref_to_data (bytes, &content_len);
    }

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, content, (int)content_len);

  if (highlight)
    fontify ("c", buffer);

  textview = gtk_text_view_new ();
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (textview), buffer);

  hincrement = 0;
  vincrement = 5;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), textview);
}

static void
populate_emoji_text (void)
{
  GtkWidget *textview;
  GtkTextBuffer *buffer;
  GString *s;
  GtkTextIter iter;

  s = g_string_sized_new (500 * 30 * 4);

  for (int i = 0; i < 500; i++)
    {
      if (i % 2)
        g_string_append (s, "<span underline=\"single\" underline_color=\"red\">x</span>");
      for (int j = 0; j < 30; j++)
        {
          g_string_append (s, "💓");
          g_string_append (s, "<span underline=\"single\" underline_color=\"red\">x</span>");
        }
      g_string_append (s, "\n");
    }

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_get_start_iter (buffer, &iter);
  gtk_text_buffer_insert_markup (buffer, &iter, s->str, s->len);

  g_string_free (s, TRUE);

  textview = gtk_text_view_new ();
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (textview), buffer);

  hincrement = 0;
  vincrement = 5;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), textview);
}

static void
populate_image (void)
{
  GtkWidget *image;

  if (!content)
    {
      GBytes *bytes;

      bytes = g_resources_lookup_data ("/sources/font_features.c", 0, NULL);
      content = g_bytes_unref_to_data (bytes, &content_len);
    }

  image = gtk_picture_new_for_resource ("/sliding_puzzle/portland-rose.jpg");
  gtk_picture_set_can_shrink (GTK_PICTURE (image), FALSE);

  hincrement = 5;
  vincrement = 5;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), image);
}

extern GtkWidget *create_weather_view (void);

static void
populate_list (void)
{
  GtkWidget *list;

  list = create_weather_view ();

  hincrement = 5;
  vincrement = 0;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), list);
}

extern GtkWidget *create_color_grid (void);
extern GListModel *gtk_color_list_new (guint size);

static void
populate_grid (void)
{
  GtkWidget *list;
  GtkNoSelection *selection;

  list = create_color_grid ();

  selection = gtk_no_selection_new (gtk_color_list_new (2097152));
  gtk_grid_view_set_model (GTK_GRID_VIEW (list), GTK_SELECTION_MODEL (selection));
  g_object_unref (selection);

  hincrement = 0;
  vincrement = 5;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), list);
}

extern GtkWidget *create_ucd_view (GtkWidget *label);

static void
populate_list2 (void)
{
  GtkWidget *list;

  list = create_ucd_view (NULL);

  hincrement = 0;
  vincrement = 5;

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), list);
}

static void
set_widget_type (int type)
{
  if (tick_cb)
    gtk_widget_remove_tick_callback (window, tick_cb);

  if (gtk_scrolled_window_get_child (GTK_SCROLLED_WINDOW (scrolledwindow)))
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), NULL);

  selected = type;

  switch (selected)
    {
    case 0:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling icons");
      populate_icons ();
      break;

    case 1:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling plain text");
      populate_text (FALSE);
      break;

    case 2:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling complex text");
      populate_text (TRUE);
      break;

    case 3:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling text with Emoji");
      populate_emoji_text ();
      break;

    case 4:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling a big image");
      populate_image ();
      break;

    case 5:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling a list");
      populate_list ();
      break;

    case 6:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling a columned list");
      populate_list2 ();
      break;

    case 7:
      gtk_window_set_title (GTK_WINDOW (window), "Scrolling a grid");
      populate_grid ();
      break;

    default:
      g_assert_not_reached ();
    }

  tick_cb = gtk_widget_add_tick_callback (window, scroll_cb, NULL, NULL);
}

G_MODULE_EXPORT void
iconscroll_next_clicked_cb (GtkButton *source,
                            gpointer   user_data)
{
  int new_index;

  if (selected + 1 >= N_WIDGET_TYPES)
    new_index = 0;
  else
    new_index = selected + 1;
 

  set_widget_type (new_index);
}

G_MODULE_EXPORT void
iconscroll_prev_clicked_cb (GtkButton *source,
                            gpointer   user_data)
{
  int new_index;

  if (selected - 1 < 0)
    new_index = N_WIDGET_TYPES - 1;
  else
    new_index = selected - 1;

  set_widget_type (new_index);
}

static gboolean
update_fps (gpointer data)
{
  GtkWidget *label = data;
  GdkFrameClock *frame_clock;
  double fps;
  char *str;

  frame_clock = gtk_widget_get_frame_clock (label);

  fps = gdk_frame_clock_get_fps (frame_clock);
  str = g_strdup_printf ("%.2f fps", fps);
  gtk_label_set_label (GTK_LABEL (label), str);
  g_free (str);

  return G_SOURCE_CONTINUE;
}

static void
remove_timeout (gpointer data)
{
  g_source_remove (GPOINTER_TO_UINT (data));
}

G_MODULE_EXPORT GtkWidget *
do_iconscroll (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkBuilder *builder;
      GtkBuilderScope *scope;
      GtkWidget *label;
      guint id;

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback (GTK_BUILDER_CSCOPE (scope), iconscroll_prev_clicked_cb);
      gtk_builder_cscope_add_callback (GTK_BUILDER_CSCOPE (scope), iconscroll_next_clicked_cb);

      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);

      gtk_builder_add_from_resource (builder, "/iconscroll/iconscroll.ui", NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));

      scrolledwindow = GTK_WIDGET (gtk_builder_get_object (builder, "scrolledwindow"));
      gtk_widget_realize (window);
      hadjustment = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "hadjustment"));
      vadjustment = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "vadjustment"));
      set_widget_type (0);

      label = GTK_WIDGET (gtk_builder_get_object (builder, "fps_label"));
      id = g_timeout_add_full (G_PRIORITY_HIGH, 500, update_fps, label, NULL);
      g_object_set_data_full (G_OBJECT (label), "timeout",
                              GUINT_TO_POINTER (id), remove_timeout);

      g_object_unref (builder);
      g_object_unref (scope);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
