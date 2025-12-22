/*
 * Unittest for curlfilesink
 */

#include <gst/check/gstcheck.h>
#include <glib/gstdio.h>
#include <curl/curl.h>
#include <unistd.h>

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstPad *srcpad;

static GstElement *sink;

static GstElement *
setup_curlfilesink (void)
{
  GST_DEBUG ("setup_curlfilesink");
  sink = gst_check_setup_element ("curlfilesink");
  srcpad = gst_check_setup_src_pad (sink, &srctemplate);
  fail_unless (gst_pad_set_active (srcpad, TRUE));

  return sink;
}

static void
cleanup_curlfilesink (GstElement * sink)
{
  GST_DEBUG ("cleanup_curlfilesink");

  gst_check_teardown_src_pad (sink);
  gst_check_teardown_element (sink);
}

static void
test_verify_file_data (const gchar * dir, gchar * file_name,
    const gchar * expected_file_content)
{
  GError *err = NULL;
  gchar *res_file_content = NULL;
  gchar *path = NULL;

  path = g_strdup_printf ("%s/%s", dir, file_name);
  g_free (file_name);

  if (!g_file_get_contents (path, &res_file_content, NULL, &err)) {
    GST_WARNING ("Error loading file: %s (%s)", file_name, err->message);
    g_error_free (err);
  }

  fail_unless (res_file_content != NULL);

  fail_unless (strncmp (res_file_content, expected_file_content,
          strlen (expected_file_content)) == 0);
  g_free (res_file_content);
  g_unlink (path);
  g_free (path);
}

static void
test_set_and_play_buffer (const gchar * _data)
{
  gpointer data = (gpointer) _data;
  GstBuffer *buffer;
  gint num_bytes;

  num_bytes = strlen (data);
  buffer = gst_buffer_new ();
  gst_buffer_insert_memory (buffer, 0,
      gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
          data, num_bytes, 0, num_bytes, data, NULL));

  fail_unless_equals_int (gst_pad_push (srcpad, buffer), GST_FLOW_OK);
}

static void
test_set_and_fail_to_play_buffer (const gchar * _data)
{
  gpointer data = (gpointer) _data;
  GstBuffer *buffer;
  gint num_bytes;

  num_bytes = strlen (data);
  buffer = gst_buffer_new ();
  gst_buffer_insert_memory (buffer, 0,
      gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,
          data, num_bytes, 0, num_bytes, data, NULL));

  fail_unless_equals_int (gst_pad_push (srcpad, buffer), GST_FLOW_ERROR);
}

GST_START_TEST (test_properties)
{
  GstElement *sink;
  GstCaps *caps;
  const gchar *location = "file:///tmp/";
  const gchar *file_contents = "line 1\r\n";
  gchar *file_name = g_strdup_printf ("curlfilesink_%d", g_random_int ());
  gchar *res_location = NULL;
  gchar *res_file_name = NULL;
  gboolean res_create_dirs = FALSE;
  gchar *path = NULL;

  sink = setup_curlfilesink ();

  g_object_set (G_OBJECT (sink), "location", "mylocation", NULL);
  g_object_set (G_OBJECT (sink), "file-name", "myfile", NULL);
  g_object_set (G_OBJECT (sink), "create-dirs", TRUE, NULL);

  g_object_get (sink,
      "location", &res_location,
      "file-name", &res_file_name, "create-dirs", &res_create_dirs, NULL);

  fail_unless (strncmp (res_location, "mylocation", strlen ("mylocation"))
      == 0);
  fail_unless (strncmp (res_file_name, "myfile", strlen ("myfile"))
      == 0);
  fail_unless (res_create_dirs == TRUE);
  g_free (res_location);
  g_free (res_file_name);

  /* change properties */
  g_object_set (G_OBJECT (sink), "location", location, NULL);
  g_object_set (G_OBJECT (sink), "file-name", file_name, NULL);
  g_object_set (G_OBJECT (sink), "create-dirs", FALSE, NULL);

  g_object_get (sink,
      "location", &res_location,
      "file-name", &res_file_name, "create-dirs", &res_create_dirs, NULL);

  fail_unless (strncmp (res_location, location, strlen (location))
      == 0);
  fail_unless (strncmp (res_file_name, file_name, strlen (file_name))
      == 0);
  fail_unless (res_create_dirs == FALSE);
  g_free (res_location);
  g_free (res_file_name);

  /* start playing */
  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);
  caps = gst_caps_from_string ("application/x-gst-check");
  gst_check_setup_events (srcpad, sink, caps, GST_FORMAT_BYTES);

  /* setup buffer */
  test_set_and_play_buffer (file_contents);

  /* try to change location property while in PLAYING state */
  g_object_set (G_OBJECT (sink), "location", "newlocation", NULL);
  g_object_get (sink, "location", &res_location, NULL);

  /* verify that location has not been altered */
  fail_unless (strncmp (res_location, location, strlen (location))
      == 0);
  g_free (res_location);

  /* eos */
  fail_unless (gst_pad_push_event (srcpad, gst_event_new_eos ()));
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);

  gst_caps_unref (caps);
  cleanup_curlfilesink (sink);

  path = g_strdup_printf ("/tmp/%s", file_name);
  g_unlink (path);
  g_free (file_name);
  g_free (path);
}

GST_END_TEST;

GST_START_TEST (test_one_file)
{
  GstElement *sink;
  GstCaps *caps;
  const gchar *location = "file:///tmp/";
  gchar *file_name = g_strdup_printf ("curlfilesink_%d", g_random_int ());
  const gchar *file_content = "line 1\r\n";
  gchar *res_location = NULL;
  gchar *res_file_name = NULL;

  sink = setup_curlfilesink ();

  g_object_set (G_OBJECT (sink), "location", location, NULL);
  g_object_set (G_OBJECT (sink), "file-name", file_name, NULL);

  g_object_get (sink,
      "location", &res_location, "file-name", &res_file_name, NULL);

  fail_unless (strncmp (res_location, location, strlen (location))
      == 0);
  fail_unless (strncmp (res_file_name, file_name, strlen (file_name))
      == 0);

  g_free (res_location);
  g_free (res_file_name);

  /* start playing */
  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);
  caps = gst_caps_from_string ("application/x-gst-check");
  gst_check_setup_events (srcpad, sink, caps, GST_FORMAT_BYTES);

  /* setup buffer */
  test_set_and_play_buffer (file_content);

  /* eos */
  fail_unless (gst_pad_push_event (srcpad, gst_event_new_eos ()));
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);

  gst_caps_unref (caps);
  cleanup_curlfilesink (sink);

  /* verify file content */
  test_verify_file_data ("/tmp", file_name, file_content);
}

GST_END_TEST;

GST_START_TEST (test_one_big_file)
{
  GstElement *sink;
  GstCaps *caps;
  const gchar *location = "file:///tmp/";
  gchar *file_name = g_strdup_printf ("curlfilesink_%d", g_random_int ());
  const gchar *file_line1 = "line 1\r\n";
  const gchar *file_line2 = "line 2\r\n";
  const gchar *file_line3 = "line 3\r\n";
  const gchar *expected_file_content = "line 1\r\n" "line 2\r\n" "line 3\r\n";
  gchar *res_location = NULL;
  gchar *res_file_name = NULL;

  sink = setup_curlfilesink ();

  g_object_set (G_OBJECT (sink), "location", location, NULL);
  g_object_set (G_OBJECT (sink), "file-name", file_name, NULL);

  g_object_get (sink,
      "location", &res_location, "file-name", &res_file_name, NULL);

  fail_unless (strncmp (res_location, location, strlen (location))
      == 0);
  fail_unless (strncmp (res_file_name, file_name, strlen (file_name))
      == 0);

  g_free (res_location);
  g_free (res_file_name);

  /* start playing */
  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);
  caps = gst_caps_from_string ("application/x-gst-check");
  gst_check_setup_events (srcpad, sink, caps, GST_FORMAT_BYTES);

  /* setup first buffer */
  test_set_and_play_buffer (file_line1);

  /* setup second buffer */
  test_set_and_play_buffer (file_line2);

  /* setup third buffer */
  test_set_and_play_buffer (file_line3);

  /* eos */
  fail_unless (gst_pad_push_event (srcpad, gst_event_new_eos ()));
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);

  gst_caps_unref (caps);
  cleanup_curlfilesink (sink);

  /* verify file content */
  test_verify_file_data ("/tmp", file_name, expected_file_content);
}

GST_END_TEST;

GST_START_TEST (test_two_files)
{
  GstElement *sink;
  GstCaps *caps;
  const gchar *location = "file:///tmp/";
  gchar *file_name1 = g_strdup_printf ("curlfilesink_%d", g_random_int ());
  gchar *file_name2 = g_strdup_printf ("curlfilesink_%d", g_random_int ());
  const gchar *file_content1 = "file content 1\r\n";
  const gchar *file_content2 = "file content 2\r\n";
  gchar *res_location = NULL;
  gchar *res_file_name = NULL;

  sink = setup_curlfilesink ();

  g_object_set (G_OBJECT (sink), "location", location, NULL);
  g_object_set (G_OBJECT (sink), "file-name", file_name1, NULL);

  g_object_get (sink,
      "location", &res_location, "file-name", &res_file_name, NULL);

  fail_unless (strncmp (res_location, location, strlen (location))
      == 0);
  fail_unless (strncmp (res_file_name, file_name1, strlen (file_name1))
      == 0);

  g_free (res_location);
  g_free (res_file_name);

  /* start playing */
  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);
  caps = gst_caps_from_string ("application/x-gst-check");
  gst_check_setup_events (srcpad, sink, caps, GST_FORMAT_BYTES);

  /* setup first buffer - content of the first file */
  test_set_and_play_buffer (file_content1);

  g_object_set (G_OBJECT (sink), "file-name", file_name2, NULL);
  g_object_get (sink, "file-name", &res_file_name, NULL);
  fail_unless (strncmp (res_file_name, file_name2, strlen (file_name2))
      == 0);
  g_free (res_file_name);

  /* setup second buffer - content of the second file */
  test_set_and_play_buffer (file_content2);

  /* eos */
  fail_unless (gst_pad_push_event (srcpad, gst_event_new_eos ()));
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);

  gst_caps_unref (caps);
  cleanup_curlfilesink (sink);

  /* verify file contents of the first file */
  test_verify_file_data ("/tmp", file_name1, file_content1);
  test_verify_file_data ("/tmp", file_name2, file_content2);
}

GST_END_TEST;

GST_START_TEST (test_create_dirs)
{
  GstElement *sink;
  GstCaps *caps;
  gchar *tmp_dir = g_strdup ("/tmp/curlfilesink_XXXXXX");
  gchar *sub_dir;
  gchar *sub_sub_dir;
  gchar *location;
  gchar *file_name = g_strdup_printf ("curlfilesink_%d", g_random_int ());
  const gchar *file_content = "line 1\r\n";

  /* create temp dir as base dir (mkdtemp saves dir name in tmp_dir) */
  fail_unless (mkdtemp (tmp_dir) != NULL);

  /* use sub-sub directory as location */
  sub_dir = g_strdup_printf ("%s/a", tmp_dir);
  sub_sub_dir = g_strdup_printf ("%s/b", sub_dir);
  location = g_strdup_printf ("file://%s/", sub_sub_dir);

  sink = setup_curlfilesink ();

  g_object_set (G_OBJECT (sink), "location", location, NULL);
  g_object_set (G_OBJECT (sink), "file-name", file_name, NULL);
  g_object_set (G_OBJECT (sink), "create-dirs", TRUE, NULL);

  /* start playing */
  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);
  caps = gst_caps_from_string ("application/x-gst-check");
  gst_check_setup_events (srcpad, sink, caps, GST_FORMAT_BYTES);

  /* setup buffer */
  test_set_and_play_buffer (file_content);

  /* eos */
  fail_unless (gst_pad_push_event (srcpad, gst_event_new_eos ()));
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);

  gst_caps_unref (caps);
  cleanup_curlfilesink (sink);

  /* verify file content in sub-sub dir created by sink */
  test_verify_file_data (sub_sub_dir, file_name, file_content);

  /* remove directories */
  fail_unless (g_rmdir (sub_sub_dir) == 0);
  fail_unless (g_rmdir (sub_dir) == 0);
  fail_unless (g_rmdir (tmp_dir) == 0);
  g_free (sub_sub_dir);
  g_free (sub_dir);
  g_free (tmp_dir);
  g_free (location);
}

GST_END_TEST;

GST_START_TEST (test_missing_path)
{
  GstElement *sink;
  GstCaps *caps;
  const gchar *location = "file:///missing/path/";
  gchar *file_name = g_strdup_printf ("curlfilesink_%d", g_random_int ());
  const gchar *file_content = "line 1\r\n";
  gchar *res_location = NULL;
  gchar *res_file_name = NULL;

  sink = setup_curlfilesink ();

  g_object_set (G_OBJECT (sink), "location", location, NULL);
  g_object_set (G_OBJECT (sink), "file-name", file_name, NULL);

  g_object_get (sink,
      "location", &res_location, "file-name", &res_file_name, NULL);

  fail_unless (strncmp (res_location, location, strlen (location))
      == 0);
  fail_unless (strncmp (res_file_name, file_name, strlen (file_name))
      == 0);

  g_free (res_location);
  g_free (res_file_name);
  g_free (file_name);

  /* start playing */
  ASSERT_SET_STATE (sink, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);
  caps = gst_caps_from_string ("application/x-gst-check");
  gst_check_setup_events (srcpad, sink, caps, GST_FORMAT_BYTES);

  /* setup & play buffer which should fail due to the missing path */
  test_set_and_fail_to_play_buffer (file_content);

  /* eos */
  fail_unless (gst_pad_push_event (srcpad, gst_event_new_eos ()));
  ASSERT_SET_STATE (sink, GST_STATE_NULL, GST_STATE_CHANGE_SUCCESS);

  gst_caps_unref (caps);
  cleanup_curlfilesink (sink);
}

GST_END_TEST;

static Suite *
curlsink_suite (void)
{
  Suite *s = suite_create ("curlfilesink");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_set_timeout (tc_chain, 20);
  tcase_add_test (tc_chain, test_properties);
  tcase_add_test (tc_chain, test_one_file);
  tcase_add_test (tc_chain, test_one_big_file);
  tcase_add_test (tc_chain, test_two_files);
  tcase_add_test (tc_chain, test_missing_path);
  tcase_add_test (tc_chain, test_create_dirs);

  return s;
}

GST_CHECK_MAIN (curlsink);
