/*
 * Copyright (C) 2023 Red Hat Inc.
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>
#include <gdk/gdksurfaceprivate.h>
#include <gdk/gdksubsurfaceprivate.h>
#include <gdk/gdkdebugprivate.h>
#include <gsk/gskrendernodeparserprivate.h>
#include <gsk/gskrendernodeprivate.h>
#include <gsk/gskoffloadprivate.h>
#include "gskrendernodeattach.h"

static char *
test_get_sibling_file (const char *node_file,
                       const char *old_ext,
                       const char *new_ext)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (node_file, old_ext))
    g_string_append_len (file, node_file, strlen (node_file) - 5);
  else
    g_string_append (file, node_file);

  g_string_append (file, new_ext);

  if (!g_file_test (file->str, G_FILE_TEST_EXISTS))
    {
      g_string_free (file, TRUE);
      return NULL;
    }

  return g_string_free (file, FALSE);
}

static GBytes *
diff_with_file (const char  *file1,
                GBytes      *input,
                GError     **error)
{
  char *buffer;
  gsize len;
  static const char msg[] = "The output is not as expected";

  g_file_get_contents (file1, &buffer, &len, NULL);
  if (strcmp (buffer, (char *) g_bytes_get_data (input, NULL)) == 0)
    {
      g_free (buffer);
      return NULL;
    }

  g_free (buffer);
  return g_bytes_new_static (msg, strlen (msg) + 1);
}

static void
append_error_value (GString *string,
                    GType    enum_type,
                    guint    value)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (enum_type);
  enum_value = g_enum_get_value (enum_class, value);

  g_string_append (string, enum_value->value_name);

  g_type_class_unref (enum_class);
}

static void
deserialize_error_func (const GskParseLocation *start,
                        const GskParseLocation *end,
                        const GError           *error,
                        gpointer                user_data)
{
  GString *errors = user_data;
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

  g_string_append_printf (errors, "%s: error: ", string->str);
  g_string_free (string, TRUE);

  if (error->domain == GTK_CSS_PARSER_ERROR)
    append_error_value (errors, GTK_TYPE_CSS_PARSER_ERROR, error->code);
  else if (error->domain == GTK_CSS_PARSER_WARNING)
    append_error_value (errors, GTK_TYPE_CSS_PARSER_WARNING, error->code);
  else
    g_string_append_printf (errors,
                            "%s %u\n",
                            g_quark_to_string (error->domain),
                            error->code);

  g_string_append_c (errors, '\n');
}

static GskRenderNode *
node_from_file (GFile *file)
{
  GBytes *bytes;
  GError *error = NULL;
  GString *errors = NULL;
  GskRenderNode *node = NULL;

  bytes = g_file_load_bytes (file, NULL, NULL, &error);
  if (error)
    {
      g_print ("Error loading file: %s\n", error->message);
      g_clear_error (&error);
      return NULL;
    }

  errors = g_string_new ("");
  node = gsk_render_node_deserialize (bytes,
                                      deserialize_error_func,
                                      errors);
  if (errors->len > 0)
    {
      g_print ("Error loading file: %s\n", errors->str);
      g_string_free (errors, TRUE);
      g_bytes_unref (bytes);
      return NULL;
    }

  g_string_free (errors, TRUE);
  g_bytes_unref (bytes);

  return node;
}

static GskRenderNode *
node_from_path (const char *path)
{
  GFile *file = g_file_new_for_path (path);
  GskRenderNode *node = node_from_file (file);
  g_object_unref (file);
  return node;
}

static GBytes *
collect_offload_info (GdkSurface *surface,
                      GskOffload *offload)
{
  GString *s;
  GBytes *bytes;

  s = g_string_new ("");

  for (unsigned int i = 0; i < gdk_surface_get_n_subsurfaces (surface); i++)
    {
      GdkSubsurface *subsurface = gdk_surface_get_subsurface (surface, i);

      g_object_set_data (G_OBJECT (subsurface), "pos", GUINT_TO_POINTER (i));
    }

  for (unsigned int i = 0; i < gdk_surface_get_n_subsurfaces (surface); i++)
    {
      GdkSubsurface *subsurface;
      GskOffloadInfo *info;
      char above[20];

      subsurface = gdk_surface_get_subsurface (surface, i);
      info = gsk_offload_get_subsurface_info (offload, subsurface);

      if (info->place_above)
        g_snprintf (above, sizeof (above), "%d",
                    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (info->place_above), "pos")));
      else
        g_snprintf (above, sizeof (above), "-");

      /* NOTE: We look at can_offload here, not is_offloaded, since we don't have
       * dmabuf textures here, so attaching them to subsurfaces won't succeed.
       */
      if (info->can_offload)
        {
          g_string_append_printf (s, "%u: offloaded, %s%sabove: %s, ",
                                  i,
                                  info->was_offloaded ? "was offloaded, " : "",
                                  gdk_subsurface_is_above_parent (subsurface) ? "raised, " : "",
                                  above);
          g_string_append_printf (s, "texture: %dx%d, ",
                                  gdk_texture_get_width (info->texture),
                                  gdk_texture_get_height (info->texture));
          g_string_append_printf (s, "source: %g %g %g %g, ",
                                  info->source.origin.x, info->source.origin.y,
                                  info->source.size.width, info->source.size.height);
          g_string_append_printf (s, "dest: %g %g %g %g\n",
                                  info->dest.origin.x, info->dest.origin.y,
                                  info->dest.size.width, info->dest.size.height);
        }
      else
        g_string_append_printf (s, "%u: %snot offloaded\n",
                                i,
                                info->was_offloaded ? "was offloaded, " : "");
    }

  bytes = g_bytes_new (s->str, s->len + 1);

  g_string_free (s, TRUE);

  return bytes;
}

static char *
region_to_string (cairo_region_t *region)
{
  GString *s = g_string_new ("");

  for (int i = 0; i < cairo_region_num_rectangles (region); i++)
    {
      cairo_rectangle_int_t r;

      cairo_region_get_rectangle (region, i, &r);
      g_string_append_printf (s, "%d %d %d %d\n", r.x, r.y, r.width, r.height);
    }

  return g_string_free (s, FALSE);
}

static gboolean
region_contains_region (cairo_region_t *region1,
                        cairo_region_t *region2)
{
  for (int i = 0; i < cairo_region_num_rectangles (region2); i++)
    {
      cairo_rectangle_int_t r;

      cairo_region_get_rectangle (region2, i, &r);
      if (cairo_region_contains_rectangle (region1, &r) != CAIRO_REGION_OVERLAP_IN)
        return FALSE;
    }

  return TRUE;
}

static cairo_region_t *
region_from_string (const char *str)
{
  cairo_region_t *region;
  char **s;
  int len;

  region = cairo_region_create ();

  s = g_strsplit (str, "\n", 0);
  len = g_strv_length (s);

  for (int i = 0; i < len; i++)
    {
      cairo_rectangle_int_t r;

      if (s[i][0] == '\0')
        continue;

      if (sscanf (s[i], "%d %d %d %d", &r.x, &r.y, &r.width, &r.height) == 4)
        cairo_region_union_rectangle (region, &r);
      else
        g_error ("failed to parse region");
    }

  g_strfreev (s);

  return region;
}

static cairo_region_t *
region_from_file (const char *path)
{
  char *buffer;
  gsize len;
  GError *error = NULL;
  cairo_region_t *region;

  if (!g_file_get_contents (path, &buffer, &len, &error))
    {
      g_error ("Failed to read region file: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  region = region_from_string (buffer);
  g_free (buffer);

  return region;
}

static void
notify_width (GdkSurface *surface,
              GParamSpec *pspec,
              gpointer    data)
{
  gboolean *done = data;

  *done = TRUE;
}

static void
compute_size (GdkToplevel     *toplevel,
              GdkToplevelSize *size,
              gpointer         data)
{
  g_signal_connect (toplevel, "notify::width",
                    G_CALLBACK (notify_width), data);
  gdk_toplevel_size_set_size (size, 800, 600);
}

static GdkSurface *
make_toplevel (void)
{
  GdkSurface *surface;
  GdkToplevelLayout *layout;
  gboolean done;

  surface = gdk_surface_new_toplevel (gdk_display_get_default ());

  done = FALSE;
  g_signal_connect (surface, "compute-size", G_CALLBACK (compute_size), &done);

  layout = gdk_toplevel_layout_new ();
  gdk_toplevel_present (GDK_TOPLEVEL (surface), layout);
  gdk_toplevel_layout_unref (layout);
  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return surface;
}

static gboolean
parse_node_file (GFile *file, const char *generate)
{
  char *reference_file;
  GdkSurface *surface;
  GdkSubsurface *subsurface;
  GskOffload *offload;
  GskRenderNode *node, *tmp;
  GBytes *offload_state, *diff;
  GError *error = NULL;
  gboolean result = TRUE;
  cairo_region_t *clip, *region;
  char *path;
  GskRenderNode *node2;
  const char *generate_values[] = { "offload", "offload2", "diff", NULL };

  if (generate && !g_strv_contains (generate_values, generate))
    {
      g_print ("Allowed --generate values are: ");
      for (int i = 0; generate_values[i]; i++)
        g_print ("%s ", generate_values[i]);
      g_print ("\n");
      return FALSE;
    }

  surface = make_toplevel ();

  if (gdk_surface_get_scale (surface) != 1.0)
    {
      g_print ("Offload tests don't work with fractional scales");
      exit (77);
    }

  subsurface = gdk_surface_create_subsurface (surface);
  if (subsurface == NULL)
    exit (77); /* subsurfaces aren't supported, skip these tests */
  g_clear_object (&subsurface);

  node = node_from_file (file);
  if (node == NULL)
    return FALSE;

  tmp = gsk_render_node_attach (node, surface);
  gsk_render_node_unref (node);
  node = tmp;

  region = cairo_region_create ();
  offload = gsk_offload_new (surface, node, region);
  offload_state = collect_offload_info (surface, offload);
  gsk_offload_free (offload);
  cairo_region_destroy (region);

  if (g_strcmp0 (generate, "offload") == 0)
    {
      g_print ("%s", (char *)g_bytes_get_data (offload_state, NULL));
      g_bytes_unref (offload_state);
      return TRUE;
    }

  reference_file = test_get_sibling_file (g_file_peek_path (file), ".node", ".offload");
  if (reference_file == NULL)
    return FALSE;

  diff = diff_with_file (reference_file, offload_state, &error);
  g_assert_no_error (error);
  if (diff && g_bytes_get_size (diff) > 0)
    {
      g_print ("Resulting .offload file doesn't match reference:\n%s\n",
               (const char *) g_bytes_get_data (diff, NULL));
      result = FALSE;
    }

  g_clear_pointer (&offload_state, g_bytes_unref);
  g_clear_pointer (&diff, g_bytes_unref);
  g_clear_pointer (&reference_file, g_free);

  path = test_get_sibling_file (g_file_peek_path (file), ".node", ".node2");
  if (path)
    {
      node2 = node_from_path (path);
      tmp = gsk_render_node_attach (node2, surface);
      gsk_render_node_unref (node2);
      node2 = tmp;

      clip = cairo_region_create ();
      offload = gsk_offload_new (surface, node2, clip);
      offload_state = collect_offload_info (surface, offload);

      if (g_strcmp0 (generate, "offload2") == 0)
        {
          g_print ("%s", (char *)g_bytes_get_data (offload_state, NULL));
          g_bytes_unref (offload_state);
          return TRUE;
        }

      reference_file = test_get_sibling_file (g_file_peek_path (file), ".node", ".offload2");
      if (reference_file == NULL)
        return FALSE;

      diff = diff_with_file (reference_file, offload_state, &error);
      g_assert_no_error (error);
      if (diff && g_bytes_get_size (diff) > 0)
        {
          g_print ("Resulting .offload2 file doesn't match reference:\n%s\n",
                   (const char *) g_bytes_get_data (diff, NULL));
          result = FALSE;
        }

      g_clear_pointer (&offload_state, g_bytes_unref);
      g_clear_pointer (&diff, g_bytes_unref);
      g_clear_pointer (&reference_file, g_free);

      gsk_render_node_diff (node, node2, &(GskDiffData) { clip, surface });

      if (g_strcmp0 (generate, "diff") == 0)
        {
          char *out = region_to_string (clip);
          g_print ("%s", out);
          cairo_region_destroy (clip);
          g_free (out);
          return TRUE;
        }

      reference_file = test_get_sibling_file (g_file_peek_path (file), ".node", ".diff");
      region = region_from_file (reference_file);
      if (!region_contains_region (clip, region))
        {
          g_print ("Resulting region doesn't include reference:\n");
          g_print ("%s\n", region_to_string (clip));
          result = FALSE;
        }

      g_clear_pointer (&region, cairo_region_destroy);
      g_clear_pointer (&reference_file, g_free);
      g_clear_pointer (&clip, cairo_region_destroy);
      g_clear_pointer (&offload, gsk_offload_free);
      g_clear_pointer (&node2, gsk_render_node_unref);
    }

  g_clear_pointer (&path, g_free);

  g_clear_pointer (&node, gsk_render_node_unref);
  gdk_surface_destroy (surface);

  return result;
}

static gboolean
test_file (GFile *file)
{
  if (g_test_verbose ())
    g_test_message ("%s", g_file_peek_path (file));

  return parse_node_file (file, NULL);
}

static int
compare_files (gconstpointer a, gconstpointer b)
{
  GFile *file1 = G_FILE (a);
  GFile *file2 = G_FILE (b);
  char *path1, *path2;
  int result;

  path1 = g_file_get_path (file1);
  path2 = g_file_get_path (file2);

  result = strcmp (path1, path2);

  g_free (path1);
  g_free (path2);

  return result;
}

static gboolean
test_files_in_directory (GFile *dir)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GList *l, *files;
  GError *error = NULL;
  gboolean result = TRUE;

  enumerator = g_file_enumerate_children (dir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
  g_assert_no_error (error);
  files = NULL;

  while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)))
    {
      const char *filename;

      filename = g_file_info_get_name (info);

      if (!g_str_has_suffix (filename, ".node"))
        {
          g_object_unref (info);
          continue;
        }

      files = g_list_prepend (files, g_file_get_child (dir, filename));

      g_object_unref (info);
    }

  g_assert_no_error (error);
  g_object_unref (enumerator);

  files = g_list_sort (files, compare_files);
  for (l = files; l; l = l->next)
    {
      result &= test_file (l->data);
    }
  g_list_free_full (files, g_object_unref);

  return result;
}

int
main (int argc, char **argv)
{
  gboolean success;

  if (argc < 2)
    {
      const char *basedir;
      GFile *dir;

      gtk_test_init (&argc, &argv);

      basedir = g_test_get_dir (G_TEST_DIST);
      dir = g_file_new_for_path (basedir);
      success = test_files_in_directory (dir);

      g_object_unref (dir);
    }
  else if (g_str_has_prefix (argv[1], "--generate="))
    {
      /* We have up to 3 different result files, the extra
       * argument determines which one is generated. Possible
       * values are offload/offload2/diff.
       */
      if (argc >= 3)
        {
          const char *generate = argv[1] + strlen ("--generate=");
          GFile *file = g_file_new_for_commandline_arg (argv[2]);

          gtk_init ();

          success = parse_node_file (file, generate);

          g_object_unref (file);
        }
      else
        success = FALSE;
    }
  else
    {
      guint i;

      gtk_test_init (&argc, &argv);

      success = TRUE;

      if (argc > 1)
        {
          for (i = 1; i < argc; i++)
            {
              GFile *file = g_file_new_for_commandline_arg (argv[i]);

              success &= test_file (file);

              g_object_unref (file);
            }
        }
      else
        {
          const char *basedir;
          GFile *dir;

          basedir = g_test_get_dir (G_TEST_DIST);
          dir = g_file_new_for_path (basedir);
          success = test_files_in_directory (dir);

          g_object_unref (dir);
        }
    }

  return success ? 0 : 1;
}

