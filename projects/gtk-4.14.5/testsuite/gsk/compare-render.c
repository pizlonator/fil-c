#include "config.h"
#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include "../reftests/reftest-compare.h"


static char *arg_output_dir = NULL;
static gboolean plain = FALSE;
static gboolean flip = FALSE;
static gboolean rotate = FALSE;
static gboolean repeat = FALSE;
static gboolean mask = FALSE;
static gboolean replay = FALSE;
static gboolean clip = FALSE;
static gboolean colorflip = FALSE;

extern void
replay_node (GskRenderNode *node, GtkSnapshot *snapshot);

static const char *
get_output_dir (void)
{
  static const char *output_dir = NULL;
  GError *error = NULL;
  GFile *file;

  if (output_dir)
    return output_dir;

  if (arg_output_dir)
    {
      GFile *arg_file = g_file_new_for_commandline_arg (arg_output_dir);
      const char *subdir;

      subdir = g_getenv ("TEST_OUTPUT_SUBDIR");
      if (subdir)
        {
          GFile *child = g_file_get_child (arg_file, subdir);
          g_object_unref (arg_file);
          arg_file = child;
        }

      output_dir = g_file_get_path (arg_file);
      g_object_unref (arg_file);
    }
  else
    {
      output_dir = g_get_tmp_dir ();
    }

  /* Just try to create the output directory.
   * If it already exists, that's exactly what we wanted to check,
   * so we can happily skip that error.
   */
  file = g_file_new_for_path (output_dir);
  if (!g_file_make_directory_with_parents (file, NULL, &error))
    {
      g_object_unref (file);

      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
          g_error ("Failed to create output dir: %s", error->message);
          g_error_free (error);
          return NULL;
        }
      g_error_free (error);
    }
  else
    g_object_unref (file);

  return output_dir;
}

static char *
file_replace_extension (const char *old_file,
                        const char *old_ext,
                        const char *new_ext)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (old_file, old_ext))
    g_string_append_len (file, old_file, strlen (old_file) - strlen (old_ext));
  else
    g_string_append (file, old_file);

  g_string_append (file, new_ext);

  return g_string_free (file, FALSE);
}

static char *
get_output_file (const char *file,
                 const char *orig_ext,
                 const char *new_ext)
{
  const char *dir;
  char *result, *base;
  char *name;

  dir = get_output_dir ();
  base = g_path_get_basename (file);
  name = file_replace_extension (base, orig_ext, new_ext);

  result = g_strconcat (dir, G_DIR_SEPARATOR_S, name, NULL);

  g_free (base);
  g_free (name);

  return result;
}

static void
save_image (GdkTexture *texture,
            const char *test_name,
            const char *extension)
{
  char *filename = get_output_file (test_name, ".node", extension);
  gboolean result;

  g_print ("Storing test result image at %s\n", filename);
  result = gdk_texture_save_to_png (texture, filename);
  g_assert_true (result);
  g_free (filename);
}

static void
save_node (GskRenderNode *node,
           const char    *test_name,
           const char    *extension)
{
  char *filename = get_output_file (test_name, ".node", extension);
  gboolean result;

  g_print ("Storing modified nodes at %s\n", filename);
  result = gsk_render_node_write_to_file (node, filename, NULL);
  g_assert_true (result);
  g_free (filename);
}

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

static const GOptionEntry options[] = {
  { "output", 0, 0, G_OPTION_ARG_FILENAME, &arg_output_dir, "Directory to save image files to", "DIR" },
  { "plain", 0, 0, G_OPTION_ARG_NONE, &plain, "Run test as-is", NULL },
  { "flip", 0, 0, G_OPTION_ARG_NONE, &flip, "Do flipped test", NULL },
  { "rotate", 0, 0, G_OPTION_ARG_NONE, &rotate, "Do rotated test", NULL },
  { "repeat", 0, 0, G_OPTION_ARG_NONE, &repeat, "Do repeated test", NULL },
  { "mask", 0, 0, G_OPTION_ARG_NONE, &mask, "Do masked test", NULL },
  { "replay", 0, 0, G_OPTION_ARG_NONE, &replay, "Do replay test", NULL },
  { "clip", 0, 0, G_OPTION_ARG_NONE, &clip, "Do clip test", NULL },
  { "colorflip", 0, 0, G_OPTION_ARG_NONE, &colorflip, "Swap colors", NULL },
  { NULL }
};

static GskRenderNode *
load_node_file (const char *node_file)
{
  GBytes *bytes;
  gsize len;
  char *contents;
  GError *error = NULL;
  GskRenderNode *node;

  if (!g_file_get_contents (node_file, &contents, &len, &error))
    {
      g_print ("Could not open node file: %s\n", error->message);
      g_clear_error (&error);
      return NULL;
    }

  bytes = g_bytes_new_take (contents, len);
  node = gsk_render_node_deserialize (bytes, deserialize_error_func, NULL);
  g_bytes_unref (bytes);

  g_assert_no_error (error);
  g_assert_nonnull (node);

  return node;
}

static GdkPixbuf *
apply_mask_to_pixbuf (GdkPixbuf *pixbuf)
{
  GdkPixbuf *copy;
  int width, height;

  copy = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
  width = MIN (1000, gdk_pixbuf_get_width (pixbuf));
  height = MIN (1000, gdk_pixbuf_get_height (pixbuf));
  if (width < 25 || height < 25)
    {
      width = MIN (width, 25);
      height = MIN (height, 25);
    }
  if (width != gdk_pixbuf_get_width (pixbuf) ||
      height != gdk_pixbuf_get_height (pixbuf))
    {
      GdkPixbuf *sub;
      sub = gdk_pixbuf_new_subpixbuf (copy, 0, 0, width, height);
      g_object_unref (copy);
      copy = sub;
    }

  for (unsigned int j = 0; j < height; j++)
    {
      guint8 *row = gdk_pixbuf_get_pixels (copy) + j * gdk_pixbuf_get_rowstride (copy);
      for (unsigned int i = 0; i < width; i++)
        {
          guint8 *p = row + i * 4;
          if ((i < 25 && j >= 25) || (i >= 25 && j < 25))
            {
              p[0] = p[1] = p[2] = p[3] = 0;
            }
        }
    }

  return copy;
}

static GdkPixbuf *
apply_colorflip_to_pixbuf (GdkPixbuf *pixbuf)
{
  GdkPixbuf *copy;
  int width, height;

  copy = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  for (unsigned int j = 0; j < height; j++)
    {
      guint8 *row = gdk_pixbuf_get_pixels (copy) + j * gdk_pixbuf_get_rowstride (copy);
      for (unsigned int i = 0; i < width; i++)
        {
          guint8 *p = row + i * 4;
          guint8 q;

          q = p[0];
          p[0] = p[1];
          p[1] = q;
        }
    }

  return copy;
}

static void
make_random_clip (const graphene_rect_t *bounds,
                  cairo_rectangle_int_t *int_clip)
{
  int_clip->width = g_test_rand_int_range (1, (int) floor (bounds->size.width));
  int_clip->height = g_test_rand_int_range (1, (int) floor (bounds->size.height));

  int_clip->x = g_test_rand_int_range ((int) ceil (bounds->origin.x), (int) floor (bounds->origin.x + bounds->size.width - int_clip->width));
  int_clip->y = g_test_rand_int_range ((int) ceil (bounds->origin.y), (int) floor (bounds->origin.y + bounds->size.height - int_clip->height));
}

static void
gsk_rect_from_cairo (graphene_rect_t              *rect,
                      const cairo_rectangle_int_t *int_rect)
{
  rect->origin.x = int_rect->x;
  rect->origin.y = int_rect->y;
  rect->size.width = int_rect->width;
  rect->size.height = int_rect->height;
}

static GdkPixbuf *
apply_clip_to_pixbuf (GdkPixbuf                   *pixbuf,
                      const cairo_rectangle_int_t *int_clip)
{
  return gdk_pixbuf_new_subpixbuf (pixbuf,
                                   int_clip->x,
                                   int_clip->y,
                                   int_clip->width,
                                   int_clip->height);
}

/*
 * Non-option arguments:
 *   1) .node file to compare
 *   2) .png file to compare the rendered .node file to
 */
int
main (int argc, char **argv)
{
  GdkTexture *reference_texture = NULL;
  GdkTexture *rendered_texture = NULL;
  GskRenderer *renderer;
  GdkSurface *window;
  GskRenderNode *node;
  const char *node_file;
  const char *png_file;
  gboolean success = TRUE;
  GError *error = NULL;
  GOptionContext *context;
  GdkTexture *diff_texture = NULL;

  (g_test_init) (&argc, &argv, NULL);

  context = g_option_context_new ("NODE REF - run GSK node tests");
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_error ("Option parsing failed: %s\n", error->message);
      return 1;
    }
  else if (argc != 3)
    {
      char *help = g_option_context_get_help (context, TRUE, NULL);
      g_print ("%s", help);
      return 1;
    }

  g_option_context_free (context);

  if (!plain && !flip && !rotate && !repeat && !mask && !replay && !clip && !colorflip)
    plain = TRUE;

  gtk_init ();

  node_file = argv[1];
  png_file = argv[2];

  g_print ("Node file: '%s'\n", node_file);
  g_print ("PNG file: '%s'\n", png_file);

  window = gdk_surface_new_toplevel (gdk_display_get_default());
  renderer = gsk_renderer_new_for_surface (window);

  /* Load the render node from the given .node file */
  node = load_node_file (node_file);
  if (!node)
    return 1;

  if (plain)
    {
      /* Render the .node file and download to cairo surface */
      rendered_texture = gsk_renderer_render_texture (renderer, node, NULL);
      g_assert_nonnull (rendered_texture);

      save_image (rendered_texture, node_file, ".out.png");

      /* Load the given reference png file */
      reference_texture = gdk_texture_new_from_filename (png_file, &error);
      if (reference_texture == NULL)
        {
          g_print ("Error loading reference surface: %s\n", error->message);
          g_clear_error (&error);
          save_image (rendered_texture, node_file, ".out.png");
          return 0;
        }

      /* Now compare the two */
      diff_texture = reftest_compare_textures (rendered_texture, reference_texture);
      if (diff_texture)
        {
          save_image (diff_texture, node_file, ".diff.png");
          success = FALSE;
        }

      g_clear_object (&diff_texture);
      g_clear_object (&reference_texture);
      g_clear_object (&rendered_texture);
    }

  if (flip)
    {
      GskRenderNode *node2;
      GdkPixbuf *pixbuf, *pixbuf2;
      GskTransform *transform;

      transform = gsk_transform_scale (NULL, -1, 1);
      node2 = gsk_transform_node_new (node, transform);
      gsk_transform_unref (transform);

      save_node (node2, node_file, "-flipped.node");

      rendered_texture = gsk_renderer_render_texture (renderer, node2, NULL);
      save_image (rendered_texture, node_file, "-flipped.out.png");

      pixbuf = gdk_pixbuf_new_from_file (png_file, &error);
      pixbuf2 = gdk_pixbuf_flip (pixbuf, TRUE);
      reference_texture = gdk_texture_new_for_pixbuf (pixbuf2);
      g_object_unref (pixbuf2);
      g_object_unref (pixbuf);

      save_image (reference_texture, node_file, "-flipped.ref.png");

      diff_texture = reftest_compare_textures (rendered_texture, reference_texture);

      if (diff_texture)
        {
          save_image (diff_texture, node_file, "-flipped.diff.png");
          success = FALSE;
        }

      g_clear_object (&diff_texture);
      g_clear_object (&rendered_texture);
      g_clear_object (&reference_texture);
      gsk_render_node_unref (node2);
    }

  if (repeat)
    {
      GskRenderNode *node2;
      GdkPixbuf *pixbuf, *pixbuf2, *pixbuf3;
      int width, height;
      graphene_rect_t node_bounds;
      graphene_rect_t bounds;

      gsk_render_node_get_bounds (node, &node_bounds);

      node_bounds.size.width = ceil (node_bounds.size.width);
      node_bounds.size.height = ceil (node_bounds.size.height);

      bounds.size.width = MIN (1000, 3 * node_bounds.size.width);
      bounds.size.height = MIN (1000, 3 * node_bounds.size.height);
      bounds.origin.x = node_bounds.origin.x + floorf (node_bounds.size.width / 2);
      bounds.origin.y = node_bounds.origin.y + floorf (node_bounds.size.height / 2);

      node2 = gsk_repeat_node_new (&bounds, node, &node_bounds);
      save_node (node2, node_file, "-repeated.node");

      rendered_texture = gsk_renderer_render_texture (renderer, node2, NULL);
      save_image (rendered_texture, node_file, "-repeated.out.png");

      pixbuf = gdk_pixbuf_new_from_file (png_file, &error);

      width = gdk_pixbuf_get_width (pixbuf);
      height = gdk_pixbuf_get_height (pixbuf);
      pixbuf2 = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (pixbuf),
                                gdk_pixbuf_get_has_alpha (pixbuf),
                                gdk_pixbuf_get_bits_per_sample (pixbuf),
                                width * 4,
                                height * 4);

      for (int i = 0; i < 4; i++)
        {
          for (int j = 0; j < 4; j++)
            {
              gdk_pixbuf_copy_area (pixbuf, 0, 0, width, height, pixbuf2, i * width,  j * height);
            }
        }

      pixbuf3 = gdk_pixbuf_new_subpixbuf (pixbuf2, width / 2, height / 2, MIN (1000, 3 * width), MIN (1000, 3 * height));

      reference_texture = gdk_texture_new_for_pixbuf (pixbuf3);

      g_object_unref (pixbuf3);
      g_object_unref (pixbuf2);
      g_object_unref (pixbuf);

      save_image (reference_texture, node_file, "-repeated.ref.png");

      diff_texture = reftest_compare_textures (rendered_texture, reference_texture);

      if (diff_texture)
        {
          save_image (diff_texture, node_file, "-repeated.diff.png");
          success = FALSE;
        }

      g_clear_object (&diff_texture);
      g_clear_object (&rendered_texture);
      g_clear_object (&reference_texture);
      gsk_render_node_unref (node2);
    }

  if (rotate)
    {
      GskRenderNode *node2;
      GdkPixbuf *pixbuf, *pixbuf2;
      GskTransform *transform;

      transform = gsk_transform_rotate (NULL, 90);
      node2 = gsk_transform_node_new (node, transform);
      gsk_transform_unref (transform);

      save_node (node2, node_file, "-rotated.node");

      rendered_texture = gsk_renderer_render_texture (renderer, node2, NULL);
      save_image (rendered_texture, node_file, "-rotated.out.png");

      pixbuf = gdk_pixbuf_new_from_file (png_file, &error);
      pixbuf2 = gdk_pixbuf_rotate_simple (pixbuf, GDK_PIXBUF_ROTATE_CLOCKWISE);
      reference_texture = gdk_texture_new_for_pixbuf (pixbuf2);
      g_object_unref (pixbuf2);
      g_object_unref (pixbuf);

      save_image (reference_texture, node_file, "-rotated.ref.png");

      diff_texture = reftest_compare_textures (rendered_texture, reference_texture);

      if (diff_texture)
        {
          save_image (diff_texture, node_file, "-rotated.diff.png");
          success = FALSE;
        }

      g_clear_object (&diff_texture);
      g_clear_object (&rendered_texture);
      g_clear_object (&reference_texture);
      gsk_render_node_unref (node2);
    }

  if (mask)
    {
      GskRenderNode *node2;
      GdkPixbuf *pixbuf, *pixbuf2;
      graphene_rect_t bounds;
      GskRenderNode *mask_node;
      GskRenderNode *nodes[2];

      gsk_render_node_get_bounds (node, &bounds);
      nodes[0] = gsk_color_node_new (&(GdkRGBA){ 0, 0, 0, 1},
                                     &GRAPHENE_RECT_INIT (bounds.origin.x, bounds.origin.y, 25, 25));
      if (bounds.size.width > 25 && bounds.size.height > 25)
        {
          nodes[1] = gsk_color_node_new (&(GdkRGBA){ 0, 0, 0, 1},
                                         &GRAPHENE_RECT_INIT (
                                             bounds.origin.x + 25,
                                             bounds.origin.y + 25,
                                             MIN (1000, bounds.size.width) - 25,
                                             MIN (1000, bounds.size.height) - 25));
          mask_node = gsk_container_node_new (nodes, G_N_ELEMENTS (nodes));
          gsk_render_node_unref (nodes[0]);
          gsk_render_node_unref (nodes[1]);
        }
      else
        {
          mask_node = nodes[0];
        }

      node2 = gsk_mask_node_new (node, mask_node, GSK_MASK_MODE_ALPHA);
      gsk_render_node_unref (mask_node);
      save_node (node2, node_file, "-masked.node");

      rendered_texture = gsk_renderer_render_texture (renderer, node2, NULL);
      save_image (rendered_texture, node_file, "-masked.out.png");

      pixbuf = gdk_pixbuf_new_from_file (png_file, &error);
      pixbuf2 = apply_mask_to_pixbuf (pixbuf);
      reference_texture = gdk_texture_new_for_pixbuf (pixbuf2);
      g_object_unref (pixbuf2);
      g_object_unref (pixbuf);

      save_image (reference_texture, node_file, "-masked.ref.png");

      diff_texture = reftest_compare_textures (rendered_texture, reference_texture);

      if (diff_texture)
        {
          save_image (diff_texture, node_file, "-masked.diff.png");
          success = FALSE;
        }

      g_clear_object (&diff_texture);
      g_clear_object (&rendered_texture);
      g_clear_object (&reference_texture);
      gsk_render_node_unref (node2);
    }

  if (replay)
    {
      GskRenderNode *node2;
      GdkTexture *rendered_texture2;
      graphene_rect_t node_bounds, node2_bounds;
      GtkSnapshot *snapshot = gtk_snapshot_new ();

      replay_node (node, snapshot);
      node2 = gtk_snapshot_free_to_node (snapshot);
      /* If the whole render node tree got eliminated, make sure we have
         something to work with nevertheless.  */
      if (!node2)
        node2 = gsk_container_node_new (NULL, 0);

      save_node (node2, node_file, "-replayed.node");
      gsk_render_node_get_bounds (node, &node_bounds);
      gsk_render_node_get_bounds (node2, &node2_bounds);
      /* Check that the node didn't grow.  */
      success = success && graphene_rect_contains_rect (&node_bounds, &node2_bounds);

      rendered_texture = gsk_renderer_render_texture (renderer, node, &node_bounds);
      save_image (rendered_texture, node_file, "-replayed.ref.png");
      rendered_texture2 = gsk_renderer_render_texture (renderer, node2, &node_bounds);
      save_image (rendered_texture2, node_file, "-replayed.out.png");
      g_assert_nonnull (rendered_texture);
      g_assert_nonnull (rendered_texture2);

      diff_texture = reftest_compare_textures (rendered_texture, rendered_texture2);

      if (diff_texture)
        {
          save_image (diff_texture, node_file, "-replayed.diff.png");
          success = FALSE;
        }

      g_clear_object (&diff_texture);
      g_clear_object (&rendered_texture);
      g_clear_object (&rendered_texture2);
      gsk_render_node_unref (node2);
    }

  if (clip)
    {
      GskRenderNode *node2;
      GdkPixbuf *pixbuf, *pixbuf2;
      graphene_rect_t bounds;
      cairo_rectangle_int_t int_clip;
      graphene_rect_t clip_rect;

      gsk_render_node_get_bounds (node, &bounds);

      if (bounds.size.width <= 1 || bounds.size.height <= 1)
        {
          g_test_skip ("Can't make a random clip");
          goto skip_clip;
        }

      make_random_clip (&bounds, &int_clip);
      g_print ("Random clip rectangle %d %d %d %d\n",
               int_clip.x, int_clip.y, int_clip.width, int_clip.height);
      gsk_rect_from_cairo (&clip_rect, &int_clip);
      g_assert_true (graphene_rect_contains_rect (&bounds, &clip_rect));
      g_assert_true (graphene_rect_get_area (&clip_rect) != 0);

      node2 = gsk_clip_node_new (node, &clip_rect);
      save_node (node2, node_file, "-clipped.node");

      rendered_texture = gsk_renderer_render_texture (renderer, node2, NULL);
      save_image (rendered_texture, node_file, "-clipped.out.png");

      pixbuf = gdk_pixbuf_new_from_file (png_file, &error);

      int_clip.x -= (int) bounds.origin.x;
      int_clip.y -= (int) bounds.origin.y;

      pixbuf2 = apply_clip_to_pixbuf (pixbuf, &int_clip);
      reference_texture = gdk_texture_new_for_pixbuf (pixbuf2);
      g_object_unref (pixbuf2);
      g_object_unref (pixbuf);

      save_image (reference_texture, node_file, "-clipped.ref.png");

      diff_texture = reftest_compare_textures (rendered_texture, reference_texture);

      if (diff_texture)
        {
          save_image (diff_texture, node_file, "-clipped.diff.png");
          success = FALSE;
        }

      g_clear_object (&diff_texture);
      g_clear_object (&rendered_texture);
      g_clear_object (&reference_texture);
      gsk_render_node_unref (node2);
    }

skip_clip:

  if (colorflip)
    {
      GskRenderNode *node2;
      GdkPixbuf *pixbuf, *pixbuf2;
      graphene_matrix_t matrix;

      graphene_matrix_init_from_float (&matrix,
                                       (const float []) { 0, 1, 0, 0,
                                                          1, 0, 0, 0,
                                                          0, 0, 1, 0,
                                                          0, 0, 0, 1 });

      node2 = gsk_color_matrix_node_new (node, &matrix, graphene_vec4_zero ());

      save_node (node2, node_file, "-colorflipped.node");

      rendered_texture = gsk_renderer_render_texture (renderer, node2, NULL);
      save_image (rendered_texture, node_file, "-colorflipped.out.png");

      pixbuf = gdk_pixbuf_new_from_file (png_file, &error);
      pixbuf2 = apply_colorflip_to_pixbuf (pixbuf);
      reference_texture = gdk_texture_new_for_pixbuf (pixbuf2);
      g_object_unref (pixbuf2);
      g_object_unref (pixbuf);

      save_image (reference_texture, node_file, "-colorflipped.ref.png");

      diff_texture = reftest_compare_textures (rendered_texture, reference_texture);

      if (diff_texture)
        {
          save_image (diff_texture, node_file, "-colorflipped.diff.png");
          success = FALSE;
        }

      g_clear_object (&diff_texture);
      g_clear_object (&rendered_texture);
      g_clear_object (&reference_texture);
      gsk_render_node_unref (node2);
    }

  gsk_render_node_unref (node);
  gsk_renderer_unrealize (renderer);
  g_object_unref (renderer);
  gdk_surface_destroy (window);

  return success ? 0 : 1;
}
