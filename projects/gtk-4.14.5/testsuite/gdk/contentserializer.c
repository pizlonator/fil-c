#include <stdlib.h>

#include <gtk/gtk.h>

typedef gboolean (* ValueCompareFunc) (GValue *v1, GValue *v2);

typedef struct {
  GOutputStream *ostream;
  GInputStream *istream;
  const char *mime_type;
  GValue value;
  ValueCompareFunc compare;
  gboolean done;
} TestData;

static gboolean
compare_string_values (GValue *v1, GValue *v2)
{
  return G_VALUE_TYPE (v1) == G_TYPE_STRING &&
         G_VALUE_TYPE (v2) == G_TYPE_STRING &&
         strcmp (g_value_get_string (v1), g_value_get_string (v2)) == 0;
}

static gboolean
compare_rgba_values (GValue *v1, GValue *v2)
{
  return G_VALUE_TYPE (v1) == GDK_TYPE_RGBA &&
         G_VALUE_TYPE (v2) == GDK_TYPE_RGBA &&
         gdk_rgba_equal ((GdkRGBA *)g_value_get_boxed (v1),
                         (GdkRGBA *)g_value_get_boxed (v2));
}

static gboolean
textures_equal (GdkTexture *t1,  GdkTexture *t2)
{
  guchar *d1, *d2;
  int width, height;
  gboolean ret;

  width = gdk_texture_get_width (t1);
  height = gdk_texture_get_height (t1);

  if (width != gdk_texture_get_width (t2))
    return FALSE;
  if (height != gdk_texture_get_height (t2))
    return FALSE;

  d1 = g_malloc (width * height * 4);
  d2 = g_malloc (width * height * 4);

  gdk_texture_download (t1, d1, width * 4);
  gdk_texture_download (t2, d2, width * 4);

  ret = memcmp (d1, d2, width * height * 4) == 0;

  if (!ret)
    {
      gdk_texture_save_to_png (t1, "texture1.png");
      gdk_texture_save_to_png (t2, "texture2.png");
    }
  g_free (d1);
  g_free (d2);

  return ret;
}

static gboolean
compare_texture_values (GValue *v1, GValue *v2)
{
  return G_VALUE_TYPE (v1) == GDK_TYPE_TEXTURE &&
         G_VALUE_TYPE (v2) == GDK_TYPE_TEXTURE &&
         textures_equal ((GdkTexture *)g_value_get_object (v1),
                         (GdkTexture *)g_value_get_object (v2));
}

static gboolean
compare_file_values (GValue *v1, GValue *v2)
{
  return G_VALUE_TYPE (v1) == G_TYPE_FILE &&
         G_VALUE_TYPE (v2) == G_TYPE_FILE &&
         g_file_equal ((GFile *)g_value_get_object (v1),
                       (GFile *)g_value_get_object (v2));
}

static gboolean
compare_file_list_values (GValue *v1, GValue *v2)
{
  GSList *s1, *s2, *l1, *l2;

  if (G_VALUE_TYPE (v1) != GDK_TYPE_FILE_LIST ||
      G_VALUE_TYPE (v2) != GDK_TYPE_FILE_LIST)
    return FALSE;

  s1 = g_value_get_boxed (v1);
  s2 = g_value_get_boxed (v2);

  if (g_slist_length (s1) != g_slist_length (s2))
    return FALSE;

  for (l1 = s1, l2 = s2; l1 != NULL; l1 = l1->next, l2 = l2->next)
    {
      GFile *f1 = l1->data;
      GFile *f2 = l2->data;
      if (!g_file_equal (f1, f2))
        return FALSE;
    }

  return TRUE;
}

static void
deserialize_done (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GError *error = NULL;
  gboolean res;
  TestData *data = user_data;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_VALUE_TYPE (&data->value));

  res = gdk_content_deserialize_finish (result, &value, &error);
  g_assert_true (res);
  g_assert_no_error (error);

  g_assert_true (data->compare (&data->value, &value));

  g_value_unset (&value);

  data->done = TRUE;
  g_main_context_wakeup (NULL);
}

static void
serialize_done (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
  GError *error = NULL;
  gboolean res;
  TestData *data = user_data;
  gpointer serialized_data;
  gsize serialized_len;

  res = gdk_content_serialize_finish (result, &error);
  g_assert_true (res);
  g_assert_no_error (error);

  serialized_data = g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (data->ostream));
  serialized_len = g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (data->ostream));
  data->istream = g_memory_input_stream_new_from_data (serialized_data, serialized_len, NULL);

  gdk_content_deserialize_async (data->istream,
                                 data->mime_type,
                                 G_VALUE_TYPE (&data->value),
                                 G_PRIORITY_DEFAULT,
                                 NULL,
                                 deserialize_done,
                                 data);
}

static void
test_content_roundtrip (const GValue     *value,
                        const char       *mime_type,
                        ValueCompareFunc  compare)
{
  TestData data = { 0, };

  data.ostream = g_memory_output_stream_new_resizable ();
  data.mime_type = mime_type;
  g_value_init (&data.value, G_VALUE_TYPE (value));
  g_value_copy (value, &data.value);
  data.compare = compare;
  data.done = FALSE;

  gdk_content_serialize_async (data.ostream,
                               data.mime_type,
                               &data.value,
                               G_PRIORITY_DEFAULT,
                               NULL,
                               serialize_done,
                               &data);

  while (!data.done)
    g_main_context_iteration (NULL, TRUE);

  g_object_unref (data.ostream);
  g_object_unref (data.istream);
  g_value_unset (&data.value);
}

static void
test_content_text_plain_utf8 (void)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, "ABCDEF12345");
  test_content_roundtrip (&value, "text/plain;charset=utf-8", compare_string_values);
  g_value_unset (&value);
}

static void
test_content_text_plain (void)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, "ABCDEF12345");
  test_content_roundtrip (&value, "text/plain", compare_string_values);
  g_value_unset (&value);
}

static void
test_content_color (void)
{
  GdkRGBA color;
  GValue value = G_VALUE_INIT;

  gdk_rgba_parse (&color, "magenta");
  g_value_init (&value, GDK_TYPE_RGBA);
  g_value_set_boxed (&value, &color);
  test_content_roundtrip (&value, "application/x-color", compare_rgba_values);
  g_value_unset (&value);
}

static void
test_content_texture (gconstpointer data)
{
  const char *mimetype = data;
  GValue value = G_VALUE_INIT;
  char *path;
  GFile *file;
  GdkTexture *texture;
  GError *error = NULL;

  path = g_test_build_filename (G_TEST_DIST, "image-data", "image.png", NULL);
  file = g_file_new_for_path (path);
  texture = gdk_texture_new_from_file (file, &error);
  g_assert_no_error (error);
  g_object_unref (file);
  g_free (path);

  g_value_init (&value, GDK_TYPE_TEXTURE);
  g_value_set_object (&value, texture);
  test_content_roundtrip (&value, mimetype, compare_texture_values);
  g_value_unset (&value);
  g_object_unref (texture);
}

static void
test_content_file (void)
{
  GFile *file;
  GValue value = G_VALUE_INIT;

  file = g_file_new_for_path ("/etc/passwd");
  g_value_init (&value, G_TYPE_FILE);
  g_value_set_object (&value, file);
  test_content_roundtrip (&value, "text/uri-list", compare_file_values);
  g_value_unset (&value);
  g_object_unref (file);
}

static void
test_content_files (void)
{
  GSList *files;
  GValue value = G_VALUE_INIT;

  files = NULL;
  files = g_slist_append (files, g_file_new_for_path ("/etc/passwd"));
  files = g_slist_append (files, g_file_new_for_path ("/boot/ostree"));

  g_value_init (&value, GDK_TYPE_FILE_LIST);
  g_value_set_boxed (&value, files);
  test_content_roundtrip (&value, "text/uri-list", compare_file_list_values);
  g_value_unset (&value);
  g_slist_free_full (files, g_object_unref);
}

#define MY_TYPE_INT_LIST (my_int_list_get_type ())
GType my_int_list_get_type (void) G_GNUC_CONST;

typedef gpointer MyIntList;

static gpointer
my_int_list_copy (gpointer list)
{
  int size;
  gpointer copy;

  size = *(int *)list;
  copy = g_malloc ((size + 1) * sizeof (int));
  memcpy (copy, list, (size + 1) * sizeof (int));

  return copy;
}

static void
my_int_list_free (gpointer list)
{
  g_free (list);
}

G_DEFINE_BOXED_TYPE (MyIntList, my_int_list, my_int_list_copy, my_int_list_free)

static void
int_list_serializer_finish (GObject      *source,
                            GAsyncResult *result,
                            gpointer      serializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;

  if (!g_output_stream_write_all_finish (stream, result, NULL, &error))
    gdk_content_serializer_return_error (serializer, error);
  else
    gdk_content_serializer_return_success (serializer);
}

static void
int_list_serializer (GdkContentSerializer *serializer)
{
  GString *str;
  const GValue *value;
  int *data;

  str = g_string_new (NULL);
  value = gdk_content_serializer_get_value (serializer);

  data = g_value_get_boxed (value);
  for (int i = 0; i < data[0] + 1; i++)
    {
      if (i > 0)
        g_string_append_c (str, ' ');
      g_string_append_printf (str, "%d", data[i]);
    }

  g_output_stream_write_all_async (gdk_content_serializer_get_output_stream (serializer),
                                   str->str,
                                   str->len,
                                   gdk_content_serializer_get_priority (serializer),
                                   gdk_content_serializer_get_cancellable (serializer),
                                   int_list_serializer_finish,
                                   serializer);
  gdk_content_serializer_set_task_data (serializer, g_string_free (str, FALSE), g_free);
}

static void
int_list_deserializer_finish (GObject      *source,
                              GAsyncResult *result,
                              gpointer      deserializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;
  gssize written;
  GValue *value;
  char *str;
  char **strv;
  int size;
  int *data;

  written = g_output_stream_splice_finish (stream, result, &error);
  if (written < 0)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  /* write terminating NULL */
  if (g_output_stream_write (stream, "", 1, NULL, &error) < 0 ||
      !g_output_stream_close (stream, NULL, &error))
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  str = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (stream));
  strv = g_strsplit (str, " ", -1);
  size = atoi (strv[0]);
  if (size + 1 != g_strv_length (strv))
    {
      g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_FAILED, "Int list corrupt");
      gdk_content_deserializer_return_error (deserializer, error);
      g_free (str);
      g_strfreev (strv);
      return;
    }

  data = g_malloc ((size + 1) * sizeof (int));
  for (int i = 0; i < size + 1; i++)
    data[i] = atoi (strv[i]);
  g_free (str);
  g_strfreev (strv);

  value = gdk_content_deserializer_get_value (deserializer);
  g_value_take_boxed (value, data);

  gdk_content_deserializer_return_success (deserializer);
}

static void
int_list_deserializer (GdkContentDeserializer *deserializer)
{
  GOutputStream *output;

  output = g_memory_output_stream_new_resizable ();

  g_output_stream_splice_async (output,
                                gdk_content_deserializer_get_input_stream (deserializer),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
                                gdk_content_deserializer_get_priority (deserializer),
                                gdk_content_deserializer_get_cancellable (deserializer),
                                int_list_deserializer_finish,
                                deserializer);
  g_object_unref (output);
}

static gboolean
compare_int_list_values (GValue *v1, GValue *v2)
{
  int *d1, *d2;

  if (G_VALUE_TYPE (v1) != MY_TYPE_INT_LIST ||
      G_VALUE_TYPE (v2) != MY_TYPE_INT_LIST)
    return FALSE;

  d1 = g_value_get_boxed (v1);
  d2 = g_value_get_boxed (v2);

  if (d1[0] != d2[0])
    return FALSE;

  return memcmp (d1, d2, (d1[0] + 1) * sizeof (int)) == 0;
}

static void
test_custom_format (void)
{
  int *data;
  GValue value = G_VALUE_INIT;

  gdk_content_register_serializer (MY_TYPE_INT_LIST,
                                   "application/x-int-list",
                                   int_list_serializer,
                                   NULL, NULL);
  gdk_content_register_deserializer ("application/x-int-list",
                                     MY_TYPE_INT_LIST,
                                     int_list_deserializer,
                                     NULL, NULL);

  data = g_malloc (3 * sizeof (int));
  data[0] = 2;
  data[1] = 3;
  data[2] = 5;

  g_value_init (&value, MY_TYPE_INT_LIST);
  g_value_set_boxed (&value, data);
  test_content_roundtrip (&value, "application/x-int-list", compare_int_list_values);
  g_value_unset (&value);
  g_free (data);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  gtk_init ();

  g_test_add_func ("/content/text_plain_utf8", test_content_text_plain_utf8);
  g_test_add_func ("/content/text_plain", test_content_text_plain);
  g_test_add_func ("/content/color", test_content_color);
  g_test_add_data_func ("/content/texture/png", "image/png", test_content_texture);
  g_test_add_data_func ("/content/texture/tiff", "image/tiff", test_content_texture);
  g_test_add_func ("/content/file", test_content_file);
  g_test_add_func ("/content/files", test_content_files);
  g_test_add_func ("/content/custom", test_custom_format);

  return g_test_run ();
}
