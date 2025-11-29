/* Pango/Font Rendering
 *
 * Demonstrates various aspects of font rendering,
 * such as hinting, antialiasing and grid alignment.
 *
 * The demo lets you explore font rendering options
 * interactively to get a feeling for they affect the
 * shape and positioning of the glyphs.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;
static GtkWidget *font_button = NULL;
static GtkWidget *entry = NULL;
static GtkWidget *image = NULL;
static GtkWidget *hinting = NULL;
static GtkWidget *anti_alias = NULL;
static GtkWidget *hint_metrics = NULL;
static GtkWidget *up_button = NULL;
static GtkWidget *down_button = NULL;
static GtkWidget *text_radio = NULL;
static GtkWidget *show_grid = NULL;
static GtkWidget *show_extents = NULL;
static GtkWidget *show_pixels = NULL;
static GtkWidget *show_outlines = NULL;

static PangoContext *context;

static int scale = 7;
static double pixel_alpha = 1.0;
static double outline_alpha = 0.0;

static void
update_image (void)
{
  const char *text;
  PangoFontDescription *desc;
  PangoLayout *layout;
  PangoRectangle ink, logical;
  int baseline;
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkPixbuf *pixbuf;
  GdkPixbuf *pixbuf2;
  GdkTexture *texture;
  cairo_font_options_t *fopt;
  cairo_hint_style_t hintstyle;
  cairo_hint_metrics_t hintmetrics;
  cairo_antialias_t antialias;
  cairo_path_t *path;

  if (!context)
    context = gtk_widget_create_pango_context (image);

  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  desc = gtk_font_dialog_button_get_font_desc (GTK_FONT_DIALOG_BUTTON (font_button));

  fopt = cairo_font_options_copy (pango_cairo_context_get_font_options (context));

  switch (gtk_drop_down_get_selected (GTK_DROP_DOWN (hinting)))
    {
    case 0:
      hintstyle = CAIRO_HINT_STYLE_NONE;
      break;
    case 1:
      hintstyle = CAIRO_HINT_STYLE_SLIGHT;
      break;
    case 2:
      hintstyle = CAIRO_HINT_STYLE_MEDIUM;
      break;
    case 3:
      hintstyle = CAIRO_HINT_STYLE_FULL;
      break;
    default:
      hintstyle = CAIRO_HINT_STYLE_DEFAULT;
      break;
    }
  cairo_font_options_set_hint_style (fopt, hintstyle);

  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (hint_metrics)))
    hintmetrics = CAIRO_HINT_METRICS_ON;
  else
    hintmetrics = CAIRO_HINT_METRICS_OFF;
  cairo_font_options_set_hint_metrics (fopt, hintmetrics);

  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (anti_alias)))
    antialias = CAIRO_ANTIALIAS_GRAY;
  else
    antialias = CAIRO_ANTIALIAS_NONE;
  cairo_font_options_set_antialias (fopt, antialias);

  pango_context_set_round_glyph_positions (context, hintmetrics == CAIRO_HINT_METRICS_ON);
  pango_cairo_context_set_font_options (context, fopt);
  cairo_font_options_destroy (fopt);
  pango_context_changed (context);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (text_radio)))
    {
      layout = pango_layout_new (context);
      pango_layout_set_font_description (layout, desc);
      pango_layout_set_text (layout, text, -1);
      pango_layout_get_extents (layout, &ink, &logical);
      baseline = pango_layout_get_baseline (layout);

      pango_extents_to_pixels (&ink, NULL);

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ink.width + 20, ink.height + 20);
      cr = cairo_create (surface);
      cairo_set_source_rgb (cr, 1, 1, 1);
      cairo_paint (cr);

      cairo_set_source_rgba (cr, 0, 0, 0, pixel_alpha);

      cairo_move_to (cr, 10, 10);
      pango_cairo_show_layout (cr, layout);

      pango_cairo_layout_path (cr, layout);
      path = cairo_copy_path (cr);

      cairo_destroy (cr);
      g_object_unref (layout);

      pixbuf = gdk_pixbuf_new_from_data (cairo_image_surface_get_data (surface),
                                         GDK_COLORSPACE_RGB, TRUE, 8,
                                         cairo_image_surface_get_width (surface),
                                         cairo_image_surface_get_height (surface),
                                         cairo_image_surface_get_stride (surface),
                                         NULL, NULL);

      pixbuf2 = gdk_pixbuf_scale_simple (pixbuf,
                                         gdk_pixbuf_get_width (pixbuf) * scale,
                                         gdk_pixbuf_get_height (pixbuf) * scale,
                                         GDK_INTERP_NEAREST);

      g_object_unref (pixbuf);
      cairo_surface_destroy (surface);

      surface = cairo_image_surface_create_for_data (gdk_pixbuf_get_pixels (pixbuf2),
                                                     CAIRO_FORMAT_ARGB32,
                                                     gdk_pixbuf_get_width (pixbuf2),
                                                     gdk_pixbuf_get_height (pixbuf2),
                                                     gdk_pixbuf_get_rowstride (pixbuf2));

      cr = cairo_create (surface);
      cairo_set_line_width (cr, 1);

      if (gtk_check_button_get_active (GTK_CHECK_BUTTON (show_grid)))
        {
          int i;
          cairo_set_source_rgba (cr, 0.2, 0, 0, 0.2);
          for (i = 1; i < ink.height + 20; i++)
            {
              cairo_move_to (cr, 0, scale * i - 0.5);
              cairo_line_to (cr, scale * (ink.width + 20), scale * i - 0.5);
              cairo_stroke (cr);
            }
          for (i = 1; i < ink.width + 20; i++)
            {
              cairo_move_to (cr, scale * i - 0.5, 0);
              cairo_line_to (cr, scale * i - 0.5, scale * (ink.height + 20));
              cairo_stroke (cr);
            }
        }

      if (gtk_check_button_get_active (GTK_CHECK_BUTTON (show_extents)))
        {
          cairo_set_source_rgb (cr, 0, 0, 1);

          cairo_rectangle (cr,
                           scale * (10 + pango_units_to_double (logical.x)) - 0.5,
                           scale * (10 + pango_units_to_double (logical.y)) - 0.5,
                           scale * pango_units_to_double (logical.width) + 1,
                           scale * pango_units_to_double (logical.height) + 1);
          cairo_stroke (cr);
          cairo_move_to (cr, scale * (10 + pango_units_to_double (logical.x)) - 0.5,
                             scale * (10 + pango_units_to_double (baseline)) - 0.5);
          cairo_line_to (cr, scale * (10 + pango_units_to_double (logical.x + logical.width)) + 1,
                             scale * (10 + pango_units_to_double (baseline)) - 0.5);
          cairo_stroke (cr);
          cairo_set_source_rgb (cr, 1, 0, 0);
          cairo_rectangle (cr,
                           scale * (10 + ink.x) - 0.5,
                           scale * (10 + ink.y) - 0.5,
                           scale * ink.width + 1,
                           scale * ink.height + 1);
          cairo_stroke (cr);
        }

      for (int i = 0; i < path->num_data; i += path->data[i].header.length)
        {
          cairo_path_data_t *data = &path->data[i];
          switch (data->header.type)
            {
            case CAIRO_PATH_CURVE_TO:
              data[3].point.x *= scale; data[3].point.y *= scale;
              data[2].point.x *= scale; data[2].point.y *= scale;
              data[1].point.x *= scale; data[1].point.y *= scale;
              break;
            case CAIRO_PATH_LINE_TO:
            case CAIRO_PATH_MOVE_TO:
              data[1].point.x *= scale; data[1].point.y *= scale;
              break;
            case CAIRO_PATH_CLOSE_PATH:
              break;
            default:
              g_assert_not_reached ();
            }
        }

      cairo_set_source_rgba (cr, 0, 0, 0, outline_alpha);
      cairo_move_to (cr, scale * 20 - 0.5, scale * 20 - 0.5);
      cairo_append_path (cr, path);
      cairo_stroke (cr);

      cairo_surface_destroy (surface);
      cairo_destroy (cr);

      cairo_path_destroy (path);
    }
  else
    {
      PangoLayoutIter *iter;
      PangoLayoutRun *run;
      PangoGlyphInfo *g;
      int i, j;
      GString *str;
      gunichar ch;

      if (*text == '\0')
        text = " ";

      ch = g_utf8_get_char (text);
      str = g_string_new ("");
      layout = pango_layout_new (context);

retry:
      for (i = 0; i < 4; i++)
        {
          g_string_append_unichar (str, ch);
          g_string_append_unichar (str, 0x200c);
        }

      pango_layout_set_font_description (layout, desc);
      pango_layout_set_text (layout, str->str, -1);
      pango_layout_get_extents (layout, &ink, &logical);
      pango_extents_to_pixels (&logical, NULL);

      iter = pango_layout_get_iter (layout);
      run = pango_layout_iter_get_run (iter);

      if (run->glyphs->num_glyphs < 8)
        {
          /* not a good char to use */
          g_string_truncate (str, 0);
          ch = 'a';
          goto retry;
        }

      g_string_free (str, TRUE);

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, logical.width * 3 / 2, 4*logical.height);
      cr = cairo_create (surface);
      cairo_set_source_rgb (cr, 1, 1, 1);
      cairo_paint (cr);

      cairo_set_source_rgb (cr, 0, 0, 0);
      for (i = 0; i < 4; i++)
        {
          g = &(run->glyphs->glyphs[2*i]);
          g->geometry.width = PANGO_UNITS_ROUND (g->geometry.width * 3 / 2);
        }

      for (j = 0; j < 4; j++)
        {
          for (i = 0; i < 4; i++)
            {
              g = &(run->glyphs->glyphs[2*i]);
              g->geometry.x_offset = i * (PANGO_SCALE / 4);
              g->geometry.y_offset = j * (PANGO_SCALE / 4);
            }

          cairo_move_to (cr, 0, j * logical.height);
          pango_cairo_show_layout (cr, layout);
        }

      cairo_destroy (cr);
      pango_layout_iter_free (iter);
      g_object_unref (layout);

      pixbuf = gdk_pixbuf_new_from_data (cairo_image_surface_get_data (surface),
                                         GDK_COLORSPACE_RGB, TRUE, 8,
                                         cairo_image_surface_get_width (surface),
                                         cairo_image_surface_get_height (surface),
                                         cairo_image_surface_get_stride (surface),
                                         NULL, NULL);

      pixbuf2 = gdk_pixbuf_scale_simple (pixbuf,
                                         gdk_pixbuf_get_width (pixbuf) * scale,
                                         gdk_pixbuf_get_height (pixbuf) * scale,
                                         GDK_INTERP_NEAREST);
      g_object_unref (pixbuf);
      cairo_surface_destroy (surface);
    }

  texture = gdk_texture_new_for_pixbuf (pixbuf2);
  gtk_picture_set_paintable (GTK_PICTURE (image), GDK_PAINTABLE (texture));
  g_object_unref (pixbuf2);
  g_object_unref (texture);
}

static gboolean fading = FALSE;
static double start_pixel_alpha;
static double end_pixel_alpha;
static double start_outline_alpha;
static double end_outline_alpha;
static gint64 start_time;
static gint64 end_time;

static double
ease_out_cubic (double t)
{
  double p = t - 1;
  return p * p * p + 1;
}

static gboolean
change_alpha (GtkWidget     *widget,
              GdkFrameClock *clock,
              gpointer       user_data)
{
  gint64 now = g_get_monotonic_time ();
  double t;

  t = ease_out_cubic ((now - start_time) / (double) (end_time - start_time));

  pixel_alpha = start_pixel_alpha + (end_pixel_alpha - start_pixel_alpha) * t;
  outline_alpha = start_outline_alpha + (end_outline_alpha - start_outline_alpha) * t;

  update_image ();

  if (now >= end_time)
    {
      fading = FALSE;
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static void
start_alpha_fade (void)
{
  gboolean pixels;
  gboolean outlines;

  if (fading)
    return;

  pixels = gtk_check_button_get_active (GTK_CHECK_BUTTON (show_pixels));
  outlines = gtk_check_button_get_active (GTK_CHECK_BUTTON (show_outlines));

  start_pixel_alpha = pixel_alpha;
  if (pixels && outlines)
    end_pixel_alpha = 0.5;
  else if (pixels)
    end_pixel_alpha = 1;
  else
    end_pixel_alpha = 0;

  start_outline_alpha = outline_alpha;
  if (outlines)
    end_outline_alpha = 1.0;
  else
    end_outline_alpha = 0.0;

  start_time = g_get_monotonic_time ();
  end_time = start_time + G_TIME_SPAN_SECOND / 2;

  fading = TRUE;
  gtk_widget_add_tick_callback (window, change_alpha, NULL, NULL);
}

static void
update_buttons (void)
{
  gtk_widget_set_sensitive (up_button, scale < 32);
  gtk_widget_set_sensitive (down_button, scale > 1);
}

static gboolean
scale_up (GtkWidget *widget,
          GVariant  *args,
          gpointer   user_data)
{
  scale += 1;
  update_buttons ();
  update_image ();
  return TRUE;
}

static gboolean
scale_down (GtkWidget *widget,
            GVariant  *args,
            gpointer   user_data)
{
  scale -= 1;
  update_buttons ();
  update_image ();
  return TRUE;
}

GtkWidget *
do_fontrendering (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkBuilder *builder;

      builder = gtk_builder_new_from_resource ("/fontrendering/fontrendering.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      font_button = GTK_WIDGET (gtk_builder_get_object (builder, "font_button"));
      up_button = GTK_WIDGET (gtk_builder_get_object (builder, "up_button"));
      down_button = GTK_WIDGET (gtk_builder_get_object (builder, "down_button"));
      entry = GTK_WIDGET (gtk_builder_get_object (builder, "entry"));
      image = GTK_WIDGET (gtk_builder_get_object (builder, "image"));
      hinting = GTK_WIDGET (gtk_builder_get_object (builder, "hinting"));
      anti_alias = GTK_WIDGET (gtk_builder_get_object (builder, "antialias"));
      hint_metrics = GTK_WIDGET (gtk_builder_get_object (builder, "hint_metrics"));
      text_radio = GTK_WIDGET (gtk_builder_get_object (builder, "text_radio"));
      show_grid = GTK_WIDGET (gtk_builder_get_object (builder, "show_grid"));
      show_extents = GTK_WIDGET (gtk_builder_get_object (builder, "show_extents"));
      show_pixels = GTK_WIDGET (gtk_builder_get_object (builder, "show_pixels"));
      show_outlines = GTK_WIDGET (gtk_builder_get_object (builder, "show_outlines"));

      g_signal_connect (up_button, "clicked", G_CALLBACK (scale_up), NULL);
      g_signal_connect (down_button, "clicked", G_CALLBACK (scale_down), NULL);
      g_signal_connect (entry, "notify::text", G_CALLBACK (update_image), NULL);
      g_signal_connect (font_button, "notify::font-desc", G_CALLBACK (update_image), NULL);
      g_signal_connect (hinting, "notify::selected", G_CALLBACK (update_image), NULL);
      g_signal_connect (anti_alias, "notify::active", G_CALLBACK (update_image), NULL);
      g_signal_connect (hint_metrics, "notify::active", G_CALLBACK (update_image), NULL);
      g_signal_connect (text_radio, "notify::active", G_CALLBACK (update_image), NULL);
      g_signal_connect (show_grid, "notify::active", G_CALLBACK (update_image), NULL);
      g_signal_connect (show_extents, "notify::active", G_CALLBACK (update_image), NULL);
      g_signal_connect (show_pixels, "notify::active", G_CALLBACK (start_alpha_fade), NULL);
      g_signal_connect (show_outlines, "notify::active", G_CALLBACK (start_alpha_fade), NULL);

      update_image ();

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
