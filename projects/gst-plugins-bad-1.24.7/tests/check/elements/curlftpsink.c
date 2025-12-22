/*
 * Unittest for curlftpsink
 */

#include <gst/check/gstcheck.h>
#include <glib/gstdio.h>
#include <curl/curl.h>

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstPad *srcpad;

static GstElement *sink;

static GstElement *
setup_curlftpsink (void)
{
  GST_DEBUG ("setup_curlftpsink");
  sink = gst_check_setup_element ("curlftpsink");
  srcpad = gst_check_setup_src_pad (sink, &srctemplate);
  fail_unless (gst_pad_set_active (srcpad, TRUE));

  return sink;
}

static void
cleanup_curlftpsink (GstElement * sink)
{
  GST_DEBUG ("cleanup_curlftpsink");

  gst_check_teardown_src_pad (sink);
  gst_check_teardown_element (sink);
}

GST_START_TEST (test_properties)
{
  GstElement *sink;
  gchar *res_location = NULL;
  gchar *res_file_name = NULL;
  gchar *res_ftp_port = NULL;
  gchar *res_tmp_file_name = NULL;
  gboolean res_create_tmpfile;
  gboolean res_epsv_mode;
  gboolean res_create_dirs;

  sink = setup_curlftpsink ();

  g_object_set (G_OBJECT (sink), "location", "mylocation", NULL);
  g_object_set (G_OBJECT (sink), "file-name", "myfile", NULL);
  g_object_set (G_OBJECT (sink), "ftp-port", "1.2.3.4:0", NULL);
  g_object_set (G_OBJECT (sink), "epsv-mode", FALSE, NULL);
  g_object_set (G_OBJECT (sink), "create-dirs", FALSE, NULL);
  g_object_set (G_OBJECT (sink), "create-tmp-file", FALSE, NULL);
  g_object_set (G_OBJECT (sink), "temp-file-name", "test_tmp_file_", NULL);

  g_object_get (sink,
      "location", &res_location,
      "file-name", &res_file_name,
      "ftp-port", &res_ftp_port,
      "epsv-mode", &res_epsv_mode,
      "create-dirs", &res_create_dirs,
      "create-tmp-file", &res_create_tmpfile,
      "temp-file-name", &res_tmp_file_name, NULL);

  fail_unless (strncmp (res_location, "mylocation", strlen ("mylocation"))
      == 0);
  fail_unless (strncmp (res_file_name, "myfile", strlen ("myfile"))
      == 0);
  fail_unless (strncmp (res_ftp_port, "1.2.3.4:0", strlen ("1.2.3.4:0"))
      == 0);
  fail_unless (strncmp (res_tmp_file_name, "test_tmp_file_",
          strlen ("test_tmp_file_"))
      == 0);
  fail_unless (res_epsv_mode == FALSE);
  fail_unless (res_create_dirs == FALSE);
  fail_unless (res_create_tmpfile == FALSE);

  g_free (res_location);
  g_free (res_file_name);
  g_free (res_ftp_port);
  g_free (res_tmp_file_name);

  /* change properties */
  g_object_set (G_OBJECT (sink), "location", "newlocation", NULL);
  g_object_set (G_OBJECT (sink), "file-name", "newfilename", NULL);
  g_object_set (G_OBJECT (sink), "ftp-port", "", NULL);
  g_object_set (G_OBJECT (sink), "epsv-mode", TRUE, NULL);
  g_object_set (G_OBJECT (sink), "create-dirs", TRUE, NULL);
  g_object_set (G_OBJECT (sink), "create-tmp-file", TRUE, NULL);
  g_object_set (G_OBJECT (sink), "temp-file-name", "test_tmp_file_", NULL);

  g_object_get (sink,
      "location", &res_location,
      "file-name", &res_file_name,
      "ftp-port", &res_ftp_port,
      "epsv-mode", &res_epsv_mode,
      "create-dirs", &res_create_dirs,
      "create-tmp-file", &res_create_tmpfile,
      "temp-file-name", &res_tmp_file_name, NULL);

  fail_unless (strncmp (res_location, "newlocation", strlen ("newlocation"))
      == 0);
  fail_unless (strncmp (res_file_name, "newfilename", strlen ("newfilename"))
      == 0);
  fail_unless (strncmp (res_ftp_port, "", strlen (""))
      == 0);
  fail_unless (strncmp (res_tmp_file_name, "test_tmp_file_",
          strlen ("test_tmp_file_"))
      == 0);
  fail_unless (res_epsv_mode == TRUE);
  fail_unless (res_create_dirs == TRUE);
  fail_unless (res_create_dirs == TRUE);
  fail_unless (res_create_tmpfile == TRUE);

  g_free (res_location);
  g_free (res_file_name);
  g_free (res_ftp_port);
  g_free (res_tmp_file_name);

  cleanup_curlftpsink (sink);
}

GST_END_TEST;

static Suite *
curlsink_suite (void)
{
  Suite *s = suite_create ("curlftpsink");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_set_timeout (tc_chain, 20);
  tcase_add_test (tc_chain, test_properties);

  return s;
}

GST_CHECK_MAIN (curlsink);
