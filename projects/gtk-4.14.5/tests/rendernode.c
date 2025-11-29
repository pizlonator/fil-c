#include <gtk/gtk.h>

static gboolean benchmark = FALSE;
static gboolean dump_variant = FALSE;
static gboolean fallback = FALSE;
static int runs = 1;

static GOptionEntry options[] = {
  { "benchmark", 'b', 0, G_OPTION_ARG_NONE, &benchmark, "Time operations", NULL },
  { "dump-variant", 'd', 0, G_OPTION_ARG_NONE, &dump_variant, "Dump GVariant structure", NULL },
  { "fallback", '\0', 0, G_OPTION_ARG_NONE, &fallback, "Draw node without a renderer", NULL },
  { "runs", 'r', 0, G_OPTION_ARG_INT, &runs, "Render the test N times", "N" },
  { NULL }
};

static void
deserialize_error_func (const GskParseLocation *start,
                        const GskParseLocation *end,
                        const GError           *error,
                        gpointer                user_data)
{
  GString *string = g_string_new ("<data>");

  g_string_append_printf (string, ":%zu:%zu",
                          start->lines + 1, start->line_chars + 1);
  if (start->lines != end->lines || start->line_chars != end->line_chars)
    {
      g_string_append (string, "-");
      if (start->lines != end->lines)
        g_string_append_printf (string, "%zu:", end->lines + 1);
      g_string_append_printf (string, "%zu", end->line_chars + 1);
    }

  g_warning ("Error at %s: %s", string->str, error->message);

  g_string_free (string, TRUE);
}


int
main(int argc, char **argv)
{
  cairo_surface_t *surface;
  GskRenderNode *node;
  GError *error = NULL;
  GBytes *bytes;
  gint64 start, end;
  char *contents;
  gsize len;
  int run;
  GOptionContext *context;
  GdkTexture *texture;

  context = g_option_context_new ("NODE-FILE PNG-FILE");
  g_option_context_add_main_entries (context, options, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  gtk_init ();

  if (runs < 1)
    {
      g_printerr ("Number of runs given with -r/--runs must be at least 1 and not %d.\n", runs);
      return 1;
    }
  if (!(argc == 3 || (argc == 2 && (dump_variant || benchmark))))
    {
      g_printerr ("Usage: %s [OPTIONS] NODE-FILE PNG-FILE\n", argv[0]);
      return 1;
    }

  if (!g_file_get_contents (argv[1], &contents, &len, &error))
    {
      g_printerr ("Could not open node file: %s\n", error->message);
      return 1;
    }

  bytes = g_bytes_new_take (contents, len);
  if (dump_variant)
    {
      GVariant *variant = g_variant_new_from_bytes (G_VARIANT_TYPE ("(suuv)"), bytes, FALSE);
      char *s;

      s = g_variant_print (variant, FALSE);
      g_print ("%s\n", s);
      g_free (s);
      g_variant_unref (variant);
    }

  start = g_get_monotonic_time ();
  node = gsk_render_node_deserialize (bytes, deserialize_error_func, NULL);
  end = g_get_monotonic_time ();
  if (benchmark)
    {
      char *bytes_string = g_format_size (g_bytes_get_size (bytes));
      g_print ("Loaded %s in %.4gs\n", bytes_string, (double) (end - start) / G_USEC_PER_SEC);
      g_free (bytes_string);
    }
  g_bytes_unref (bytes);

  if (node == NULL)
    {
      return 1;
    }

  if (fallback)
    {
      graphene_rect_t bounds;
      cairo_t *cr;
      int width, height, stride;
      guchar *pixels;

      gsk_render_node_get_bounds (node, &bounds);
      width = ceil (bounds.size.width);
      height = ceil (bounds.size.height);
      stride = width * 4;
      pixels = g_malloc0_n (stride, height);

      surface = cairo_image_surface_create_for_data (pixels, CAIRO_FORMAT_ARGB32, width, height, stride);
      cr = cairo_create (surface);

      cairo_translate (cr, - bounds.origin.x, - bounds.origin.y);
      for (run = 0; run < runs; run++)
        {
          if (run > 0)
            {
              cairo_save (cr);
              cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
              cairo_paint (cr);
              cairo_restore (cr);
            }
          start = g_get_monotonic_time ();
          gsk_render_node_draw (node, cr);
          end = g_get_monotonic_time ();
          if (benchmark)
            g_print ("Run %d: Rendered fallback in %.4gs\n", run, (double) (end - start) / G_USEC_PER_SEC);
        }

      cairo_destroy (cr);
      cairo_surface_destroy (surface);

      bytes = g_bytes_new_take (pixels, stride * height);
      texture = gdk_memory_texture_new (width, height,
                                        GDK_MEMORY_DEFAULT,
                                        bytes,
                                        stride);
      g_bytes_unref (bytes);
    }
  else
    {
      GskRenderer *renderer;
      GdkSurface *window;

      window = gdk_surface_new_toplevel (gdk_display_get_default());
      renderer = gsk_renderer_new_for_surface (window);
      texture = NULL; /* poor gcc can't see that runs > 0 */

      for (run = 0; run < runs; run++)
        {
          if (run > 0)
            g_object_unref (texture);
          start = g_get_monotonic_time ();
          texture = gsk_renderer_render_texture (renderer, node, NULL);
          end = g_get_monotonic_time ();
          if (benchmark)
            g_print ("Run %u: Rendered using %s in %.4gs\n", run, G_OBJECT_TYPE_NAME (renderer), (double) (end - start) / G_USEC_PER_SEC);
        }

      gsk_renderer_unrealize (renderer);
      g_object_unref (window);
      g_object_unref (renderer);
    }

  gsk_render_node_unref (node);

  if (argc > 2)
    {
      if (!gdk_texture_save_to_png (texture, argv[2]))
        {
          g_object_unref (texture);
          g_print ("Failed to save PNG file\n");
          return 1;
        }
    }

  g_object_unref (texture);

  return 0;
}
