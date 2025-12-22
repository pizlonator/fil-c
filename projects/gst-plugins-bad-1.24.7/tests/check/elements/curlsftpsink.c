/*
 * very basic unit test for curlsftpsink
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
setup_curlsftpsink (void)
{
  GST_DEBUG ("setup_curlsftpsink");
  sink = gst_check_setup_element ("curlsftpsink");
  srcpad = gst_check_setup_src_pad (sink, &srctemplate);
  fail_unless (gst_pad_set_active (srcpad, TRUE));

  return sink;
}

static void
cleanup_curlsftpsink (GstElement * sink)
{
  GST_DEBUG ("cleanup_curlsftpsink");

  gst_check_teardown_src_pad (sink);
  gst_check_teardown_element (sink);
}

GST_START_TEST (test_properties)
{

  GstElement *sink;

  gchar *res_location = NULL;
  gchar *res_user = NULL;
  gchar *res_passwd = NULL;
  gchar *res_file_name = NULL;
  gint res_timeout = 0;
  gint res_qos_dscp = 0;

  gchar *res_pubkey_file = NULL;
  gchar *res_privkey_file = NULL;
  gchar *res_passphrase = NULL;
  gchar *res_kh_file = NULL;
  gchar *res_host_pubkey_md5 = NULL;
  gchar *res_host_pubkey_sha256 = NULL;
  guint res_auth_type = 0;
  gboolean res_accept_unkh = FALSE;

  gboolean res_create_dirs = FALSE;

  sink = setup_curlsftpsink ();

  /* props from GstCurlBaseSink */
  g_object_set (G_OBJECT (sink), "location", "test_location", NULL);
  g_object_set (G_OBJECT (sink), "user", "test_user", NULL);
  g_object_set (G_OBJECT (sink), "passwd", "test_passwd", NULL);
  g_object_set (G_OBJECT (sink), "file-name", "test_filename", NULL);
  g_object_set (G_OBJECT (sink), "timeout", 123, NULL);
  g_object_set (G_OBJECT (sink), "qos-dscp", 11, NULL); /* DSCP_MIN = 0,
                                                           DSCP_MAX = 63
                                                           gstcurlbasesink.c */

  /* props from GstCurlSshSink */
  g_object_set (G_OBJECT (sink), "ssh-auth-type", CURLSSH_AUTH_PUBLICKEY, NULL);
  g_object_set (G_OBJECT (sink), "ssh-pub-keyfile", "public_key_file", NULL);
  g_object_set (G_OBJECT (sink), "ssh-priv-keyfile", "private_key_file", NULL);
  g_object_set (G_OBJECT (sink), "ssh-knownhosts", "known_hosts", NULL);
  g_object_set (G_OBJECT (sink), "ssh-host-pubkey-md5",
      "00112233445566778899aabbccddeeff", NULL);
#if CURL_AT_LEAST_VERSION(7, 80, 0)
  g_object_set (G_OBJECT (sink), "ssh-host-pubkey-sha256",
      "TQtiu1/zwGEEKG4z/PDfPE/ak47AF9nbWHykx4CWcu9", NULL);
#endif
  g_object_set (G_OBJECT (sink), "ssh-accept-unknownhost", TRUE, NULL);
  g_object_set (G_OBJECT (sink), "ssh-key-passphrase", "SoMePaSsPhRaSe", NULL);

  /* props from GstCurlSftpSink */
  g_object_set (G_OBJECT (sink), "create-dirs", TRUE, NULL);


  /* run a 'get' on all the above props */
  g_object_get (sink, "location", &res_location,
      "user", &res_user, "passwd", &res_passwd, "file-name", &res_file_name,
      "timeout", &res_timeout, "qos-dscp", &res_qos_dscp,
      "ssh-auth-type", &res_auth_type, "ssh-pub-keyfile", &res_pubkey_file,
      "ssh-priv-keyfile", &res_privkey_file, "ssh-knownhosts", &res_kh_file,
      "ssh-host-pubkey-md5", &res_host_pubkey_md5,
      "ssh-accept-unknownhost", &res_accept_unkh,
      "create-dirs", &res_create_dirs, "ssh-key-passphrase", &res_passphrase,
      NULL);

#if CURL_AT_LEAST_VERSION(7, 80, 0)
  g_object_get (sink, "ssh-host-pubkey-sha256", &res_host_pubkey_sha256, NULL);
#endif

  fail_unless (strncmp (res_location, "test_location", strlen ("test_location"))
      == 0);
  fail_unless (strncmp (res_user, "test_user", strlen ("test_user")) == 0);
  fail_unless (strncmp (res_passwd, "test_passwd", strlen ("test_passwd"))
      == 0);
  fail_unless (strncmp (res_file_name, "test_filename",
          strlen ("test_filename")) == 0);
  fail_unless (res_timeout == 123);
  fail_unless (res_qos_dscp == 11);

  fail_unless (res_auth_type == CURLSSH_AUTH_PUBLICKEY);
  fail_unless (strncmp (res_pubkey_file, "public_key_file",
          strlen ("public_key_file")) == 0);
  fail_unless (strncmp (res_privkey_file, "private_key_file",
          strlen ("private_key_file")) == 0);
  fail_unless (strncmp (res_kh_file, "known_hosts", strlen ("known_hosts"))
      == 0);
  fail_unless (strncmp (res_host_pubkey_md5, "00112233445566778899aabbccddeeff",
          strlen ("00112233445566778899aabbccddeeff")) == 0);
#if CURL_AT_LEAST_VERSION(7, 80, 0)
  fail_unless (strncmp (res_host_pubkey_sha256,
          "TQtiu1/zwGEEKG4z/PDfPE/ak47AF9nbWHykx4CWcu9",
          strlen ("TQtiu1/zwGEEKG4z/PDfPE/ak47AF9nbWHykx4CWcu9")) == 0);
#endif
  fail_unless (strncmp (res_passphrase, "SoMePaSsPhRaSe",
          strlen ("SoMePaSsPhRaSe")) == 0);
  fail_unless (res_accept_unkh == TRUE);
  fail_unless (res_create_dirs == TRUE);

  g_free (res_location);
  g_free (res_user);
  g_free (res_passwd);
  g_free (res_file_name);
  g_free (res_pubkey_file);
  g_free (res_privkey_file);
  g_free (res_passphrase);
  g_free (res_kh_file);
  g_free (res_host_pubkey_md5);
  g_free (res_host_pubkey_sha256);

  /* ------- change properties ------------- */

  /* props from GstCurlBaseSink */
  g_object_set (G_OBJECT (sink), "location", "new_location", NULL);
  g_object_set (G_OBJECT (sink), "user", "new_user", NULL);
  g_object_set (G_OBJECT (sink), "passwd", "new_passwd", NULL);
  g_object_set (G_OBJECT (sink), "file-name", "new_filename", NULL);
  g_object_set (G_OBJECT (sink), "timeout", 321, NULL);
  g_object_set (G_OBJECT (sink), "qos-dscp", 22, NULL);

  /* props from GstCurlSshSink */
  g_object_set (G_OBJECT (sink), "ssh-auth-type", CURLSSH_AUTH_PASSWORD, NULL);
  g_object_set (G_OBJECT (sink), "ssh-pub-keyfile", "/xxx/pub_key", NULL);
  g_object_set (G_OBJECT (sink), "ssh-priv-keyfile", "/yyy/pvt_key", NULL);
  g_object_set (G_OBJECT (sink), "ssh-knownhosts", "/zzz/known_hosts", NULL);
  g_object_set (G_OBJECT (sink), "ssh-host-pubkey-md5",
      "ffeeddccbbaa99887766554433221100", NULL);
#if CURL_AT_LEAST_VERSION(7, 80, 0)
  g_object_set (G_OBJECT (sink), "ssh-host-pubkey-sha256",
      "TUtitut/wGEEKG4z/PDfPE/ak47AF7nbWHykAxCWcu5", NULL);
#endif
  g_object_set (G_OBJECT (sink), "ssh-accept-unknownhost", FALSE, NULL);
  g_object_set (G_OBJECT (sink), "ssh-key-passphrase", "OtherPASSphrase", NULL);

  /* props from GstCurlSftpSink */
  g_object_set (G_OBJECT (sink), "create-dirs", FALSE, NULL);


  /* run a 'get' on all the above props */
  g_object_get (sink, "location", &res_location, "user", &res_user,
      "passwd", &res_passwd, "file-name", &res_file_name,
      "timeout", &res_timeout, "qos-dscp", &res_qos_dscp,
      "ssh-auth-type", &res_auth_type, "ssh-pub-keyfile", &res_pubkey_file,
      "ssh-priv-keyfile", &res_privkey_file, "ssh-knownhosts", &res_kh_file,
      "ssh-accept-unknownhost", &res_accept_unkh,
      "ssh-host-pubkey-md5", &res_host_pubkey_md5,
      "ssh-key-passphrase", &res_passphrase, "create-dirs", &res_create_dirs,
      NULL);

#if CURL_AT_LEAST_VERSION(7, 80, 0)
  g_object_get (sink, "ssh-host-pubkey-sha256", &res_host_pubkey_sha256, NULL);
#endif

  fail_unless (strncmp (res_location, "new_location", strlen ("new_location"))
      == 0);
  fail_unless (strncmp (res_user, "new_user", strlen ("new_user")) == 0);
  fail_unless (strncmp (res_passwd, "new_passwd", strlen ("new_passwd"))
      == 0);
  fail_unless (strncmp (res_file_name, "new_filename",
          strlen ("new_filename")) == 0);
  fail_unless (res_timeout == 321);
  fail_unless (res_qos_dscp == 22);

  fail_unless (res_auth_type == CURLSSH_AUTH_PASSWORD);
  fail_unless (strncmp (res_pubkey_file, "/xxx/pub_key",
          strlen ("/xxx/pub_key")) == 0);
  fail_unless (strncmp (res_privkey_file, "/yyy/pvt_key",
          strlen ("/yyy/pvt_key")) == 0);
  fail_unless (strncmp (res_kh_file, "/zzz/known_hosts",
          strlen ("/zzz/known_host")) == 0);
  fail_unless (strncmp (res_host_pubkey_md5, "ffeeddccbbaa99887766554433221100",
          strlen ("ffeeddccbbaa99887766554433221100")) == 0);
#if CURL_AT_LEAST_VERSION(7, 80, 0)
  fail_unless (strncmp (res_host_pubkey_sha256,
          "TUtitut/wGEEKG4z/PDfPE/ak47AF7nbWHykAxCWcu5",
          strlen ("TUtitut/wGEEKG4z/PDfPE/ak47AF7nbWHykAxCWcu5")) == 0);
#endif
  fail_unless (strncmp (res_passphrase, "OtherPASSphrase",
          strlen ("OtherPASSphrase")) == 0);
  fail_unless (res_accept_unkh == FALSE);
  fail_unless (res_create_dirs == FALSE);

  g_free (res_location);
  g_free (res_user);
  g_free (res_passwd);
  g_free (res_file_name);
  g_free (res_pubkey_file);
  g_free (res_privkey_file);
  g_free (res_passphrase);
  g_free (res_kh_file);
  g_free (res_host_pubkey_md5);
  g_free (res_host_pubkey_256);

  cleanup_curlsftpsink (sink);
}

GST_END_TEST;

static Suite *
curlsink_suite (void)
{
  Suite *s = suite_create ("curlsftpsink");
  TCase *tc_chain = tcase_create ("sftpsink props");

  suite_add_tcase (s, tc_chain);
  tcase_set_timeout (tc_chain, 20);
  tcase_add_test (tc_chain, test_properties);

  return s;
}

GST_CHECK_MAIN (curlsink);
