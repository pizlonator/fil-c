/*
 * Unittest for curlhttpsink
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
setup_curlhttpsink (void)
{
  GST_DEBUG ("setup_curlhttpsink");
  sink = gst_check_setup_element ("curlhttpsink");
  srcpad = gst_check_setup_src_pad (sink, &srctemplate);
  fail_unless (gst_pad_set_active (srcpad, TRUE));

  return sink;
}

static void
cleanup_curlhttpsink (GstElement * sink)
{
  GST_DEBUG ("cleanup_curlhttpsink");

  gst_check_teardown_src_pad (sink);
  gst_check_teardown_element (sink);
}


GST_START_TEST (test_properties)
{
  GstElement *sink;
  gchar *res_location = NULL;
  gchar *res_file_name = NULL;
  gchar *res_user;
  gchar *res_passwd;
  gchar *res_proxy;
  guint res_proxy_port;
  gchar *res_proxy_user;
  gchar *res_proxy_passwd;
  gchar *res_content_type;
  gboolean res_use_content_length;

  sink = setup_curlhttpsink ();

  g_object_set (G_OBJECT (sink),
      "location", "mylocation",
      "file-name", "myfile",
      "user", "user",
      "passwd", "passwd",
      "proxy", "myproxy",
      "proxy-port", 7777,
      "proxy-user", "proxy_user",
      "proxy-passwd", "proxy_passwd",
      "content-type", "image/jpeg", "use-content-length", TRUE, NULL);

  g_object_get (sink,
      "location", &res_location,
      "file-name", &res_file_name,
      "user", &res_user,
      "passwd", &res_passwd,
      "proxy", &res_proxy,
      "proxy-port", &res_proxy_port,
      "proxy-user", &res_proxy_user,
      "proxy-passwd", &res_proxy_passwd,
      "content-type", &res_content_type,
      "use-content-length", &res_use_content_length, NULL);

  fail_unless (strncmp (res_location, "mylocation", strlen ("mylocation"))
      == 0);
  fail_unless (strncmp (res_file_name, "myfile", strlen ("myfile"))
      == 0);
  fail_unless (strncmp (res_user, "user", strlen ("user")) == 0);
  fail_unless (strncmp (res_passwd, "passwd", strlen ("passwd")) == 0);
  fail_unless (strncmp (res_proxy, "myproxy", strlen ("myproxy")) == 0);
  fail_unless (res_proxy_port == 7777);
  fail_unless (strncmp (res_proxy_user, "proxy_user", strlen ("proxy_user"))
      == 0);
  fail_unless (strncmp (res_proxy_passwd, "proxy_passwd",
          strlen ("proxy_passwd")) == 0);
  fail_unless (strncmp (res_content_type, "image/jpeg", strlen ("image/jpeg"))
      == 0);
  fail_unless (res_use_content_length == TRUE);

  g_free (res_location);
  g_free (res_file_name);
  g_free (res_user);
  g_free (res_passwd);
  g_free (res_proxy);
  g_free (res_proxy_user);
  g_free (res_proxy_passwd);
  g_free (res_content_type);

  /* new properties */
  g_object_set (G_OBJECT (sink), "location", "newlocation", NULL);
  g_object_get (sink, "location", &res_location, NULL);
  fail_unless (strncmp (res_location, "newlocation", strlen ("newlocation"))
      == 0);
  g_free (res_location);

  g_object_set (G_OBJECT (sink), "file-name", "newfile", NULL);
  g_object_get (sink, "file-name", &res_file_name, NULL);
  fail_unless (strncmp (res_file_name, "newfile", strlen ("newfile"))
      == 0);
  g_free (res_file_name);

  cleanup_curlhttpsink (sink);
}

GST_END_TEST;

static Suite *
curlsink_suite (void)
{
  Suite *s = suite_create ("curlhttpsink");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_set_timeout (tc_chain, 20);
  tcase_add_test (tc_chain, test_properties);

  return s;
}

GST_CHECK_MAIN (curlsink);
