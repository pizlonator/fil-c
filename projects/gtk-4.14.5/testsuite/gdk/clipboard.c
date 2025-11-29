#include <stdlib.h>

#include <gtk/gtk.h>

static void
text_received (GObject *source,
               GAsyncResult *res,
               gpointer data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (source);
  gboolean *done = data;
  GError *error = NULL;
  char *text;

  text = gdk_clipboard_read_text_finish (clipboard, res, &error);

  g_assert_no_error (error);
  g_assert_cmpstr (text, ==, "testing, 1, 2");

  g_free (text);

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

static void
test_clipboard_basic (void)
{
  GdkDisplay *display;
  GdkClipboard *clipboard;
  GdkContentFormats *formats;
  GdkContentProvider *content;
  gboolean done;
  gboolean ret;
  GValue value = G_VALUE_INIT;
  GError *error = NULL;

  display = gdk_display_get_default ();
  clipboard = gdk_display_get_clipboard (display);

  g_assert_true (gdk_clipboard_get_display (clipboard) == display);

  gdk_clipboard_set_text (clipboard, "testing, 1, 2");

  g_assert_true (gdk_clipboard_is_local (clipboard));

  formats = gdk_clipboard_get_formats (clipboard);

  g_assert_true (gdk_content_formats_contain_gtype (formats, G_TYPE_STRING));
  g_assert_true (gdk_content_formats_contain_mime_type (formats, "text/plain"));

  done = FALSE;

  gdk_clipboard_read_text_async (clipboard, NULL, text_received, &done);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  content = gdk_clipboard_get_content (clipboard);
  g_assert_nonnull (content);

  g_value_init (&value, G_TYPE_STRING);
  ret = gdk_content_provider_get_value (content, &value, &error);
  g_assert_true (ret);
  g_assert_no_error (error);
  g_assert_true (G_VALUE_TYPE (&value) == G_TYPE_STRING);
  g_assert_cmpstr (g_value_get_string (&value), ==, "testing, 1, 2");

  g_value_unset (&value);
}

static void
read_upto_done (GObject      *source,
                GAsyncResult *result,
                gpointer      data)
{
  GDataInputStream *out = G_DATA_INPUT_STREAM (source);
  gboolean *done = data;
  char *str;
  GError *error = NULL;

  str = g_data_input_stream_read_upto_finish (out, result, NULL, &error);
  g_assert_no_error (error);

  if (g_test_verbose ())
    g_test_message ("src formats: %s", str);

  g_free (str);

  *done = TRUE;
  g_main_context_wakeup (NULL);
}

static void
compare_textures (GdkTexture *t1,
                  GdkTexture *t2)
{
  int width;
  int height;
  int stride;
  guchar *d1;
  guchar *d2;

  width = gdk_texture_get_width (t1);
  height = gdk_texture_get_height (t1);
  stride = 4 * width;

  g_assert_cmpint (width, ==, gdk_texture_get_width (t2));
  g_assert_cmpint (height, ==, gdk_texture_get_height (t2));

  d1 = g_malloc (stride * height);
  d2 = g_malloc (stride * height);

  gdk_texture_download (t1, d1, stride);
  gdk_texture_download (t2, d2, stride);

  g_assert_cmpmem (d1, stride * height, d2, stride * height);

  g_free (d1);
  g_free (d2);
}

static void
compare_files (const char *file1,
               const char *file2)
{
  char *m1, *m2;
  gsize l1, l2;
  GError *error = NULL;

  g_file_get_contents (file1, &m1, &l1, &error);
  g_assert_no_error (error);
  g_file_get_contents (file2, &m2, &l2, &error);
  g_assert_no_error (error);

  if (l1 != l2)
    g_test_fail_printf ("file length mismatch: %s %s\n", file1, file2);
  else if (memcmp (m1, m2, l1) != 0)
    g_test_fail_printf ("file mismatch: %s %s\n", file1, file2);

  g_free (m1);
  g_free (m2);
}

static void
test_clipboard_roundtrip (const char *type,
                          const char *value,
                          const char *result)
{
  GSubprocess *source, *target;
  char *clipboard_client;
  GError *error = NULL;
  GDataInputStream *out;
  gboolean done = FALSE;
  char *stdout_buf = NULL;
  char *stderr_buf = NULL;

  if (gdk_display_get_default_seat (gdk_display_get_default ()) == NULL)
    {
      g_test_skip ("we have no seat, so focus won't work");
      return;
    }

  clipboard_client = g_test_build_filename (G_TEST_BUILT, "/clipboard-client", NULL);

  source = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                             &error,
                             clipboard_client,
                             "set", type, value, NULL);
  g_assert_no_error (error);

  /* Wait until the first child has claimed the clipboard */
  out = g_data_input_stream_new (g_subprocess_get_stdout_pipe (source));
  g_data_input_stream_read_upto_async (out, "\n", 1, 0, NULL, read_upto_done, &done);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  g_object_unref (out);

  target = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE,
                             &error,
                             clipboard_client,
                             "get", type, NULL);

  g_free (clipboard_client);

  if (!target)
    {
      g_test_fail ();
      g_error_free (error);

      g_subprocess_force_exit (source);
      g_object_unref (source);

      return;
    }

  g_subprocess_communicate_utf8 (target, NULL, NULL, &stdout_buf, &stderr_buf, &error);

  g_subprocess_force_exit (source);
  g_object_unref (source);

  g_assert_no_error (error);

  if (result)
    g_assert_cmpstr (stdout_buf, ==, result);
  else if (g_str_has_prefix (stdout_buf, "ERROR"))
    {
      g_test_fail_printf ("dest error: %s", stdout_buf);
    }
  else if (g_str_equal (type, "image"))
    {
      GFile *f1, *f2;
      GdkTexture *t1, *t2;

      f1 = g_file_new_for_path (value);
      f2 = g_file_new_for_path (stdout_buf);

      t1 = gdk_texture_new_from_file (f1, &error);
      g_assert_no_error (error);
      t2 = gdk_texture_new_from_file (f2, &error);
      g_assert_no_error (error);

      compare_textures (t1, t2);

      g_object_unref (t1);
      g_object_unref (t2);
      g_object_unref (f1);
      g_object_unref (f2);
    }
  else if (g_str_equal (type, "file"))
    {
      compare_files (value, stdout_buf);
    }
  else if (g_str_equal (type, "files"))
    {
      char **in_files, **out_files;

      in_files = g_strsplit (value, ":", -1);
      out_files = g_strsplit (stdout_buf, ":", -1);

      g_assert_cmpint (g_strv_length (in_files), ==, g_strv_length (out_files));
      for (int i = 0; in_files[i]; i++)
        compare_files (in_files[i], out_files[i]);

      g_strfreev (in_files);
      g_strfreev (out_files);
    }

  g_assert_null (stderr_buf);
  g_free (stdout_buf);
  g_object_unref (target);
}

static void
test_clipboard_string (void)
{
  test_clipboard_roundtrip ("string", "abcdef1230", "abcdef1230");
}

static void
test_clipboard_text (void)
{
  char *filename;

  filename = g_test_build_filename (G_TEST_DIST, "clipboard-data", "test.txt", NULL);

  test_clipboard_roundtrip ("text", filename, NULL);

  g_free (filename);
}

static void
test_clipboard_image (void)
{
  char *filename;

  filename = g_test_build_filename (G_TEST_DIST, "clipboard-data", "image.png", NULL);

  test_clipboard_roundtrip ("image", filename, NULL);
}

static void
test_clipboard_color (void)
{
  test_clipboard_roundtrip ("color", "red", "rgb(255,0,0)");
}

static void
test_clipboard_file (void)
{
  char *filename;

  filename = g_test_build_filename (G_TEST_DIST, "clipboard-data", "test.txt", NULL);

  test_clipboard_roundtrip ("file", filename, NULL);

  g_free (filename);
}

static void
test_clipboard_files (void)
{
  char **filenames;
  char *string;

  filenames = g_new (char *, 3);
  filenames[0] = g_test_build_filename (G_TEST_DIST, "clipboard-data", "image.png", NULL);
  filenames[1] = g_test_build_filename (G_TEST_DIST, "clipboard-data", "test.txt", NULL);
  filenames[2] = NULL;

  string = g_strjoinv (":", filenames);

  test_clipboard_roundtrip ("files", string, NULL);

  g_free (string);
  g_strfreev (filenames);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  gtk_init ();

  g_test_add_func ("/clipboard/basic", test_clipboard_basic);
  g_test_add_func ("/clipboard/string", test_clipboard_string);
  g_test_add_func ("/clipboard/text", test_clipboard_text);
  g_test_add_func ("/clipboard/image", test_clipboard_image);
  g_test_add_func ("/clipboard/color", test_clipboard_color);
  g_test_add_func ("/clipboard/file", test_clipboard_file);
  g_test_add_func ("/clipboard/files", test_clipboard_files);

  return g_test_run ();
}
