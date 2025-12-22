/*
 * Unittest for curlsmtpsink
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
setup_curlsmtpsink (void)
{
  GST_DEBUG ("setup_curlsmtpsink");
  sink = gst_check_setup_element ("curlsmtpsink");
  srcpad = gst_check_setup_src_pad (sink, &srctemplate);
  fail_unless (gst_pad_set_active (srcpad, TRUE));

  return sink;
}

static void
cleanup_curlsmtpsink (GstElement * sink)
{
  GST_DEBUG ("cleanup_curlsmtpsink");

  gst_check_teardown_src_pad (sink);
  gst_check_teardown_element (sink);
}

GST_START_TEST (test_properties)
{
  GstElement *sink;
  gchar *res_location = NULL;
  gchar *res_file_name = NULL;
  gchar *res_mail_rcpt = NULL;
  gchar *res_mail_from = NULL;
  gchar *res_subj = NULL;
  gchar *res_msg = NULL;
  gchar *res_usr = NULL;
  gchar *res_passwd = NULL;
  gchar *res_pop_location = NULL;
  gchar *res_pop_usr = NULL;
  gchar *res_pop_passwd = NULL;
  guint res_nbr_attach;
  gboolean res_use_ssl;

  sink = setup_curlsmtpsink ();

  g_object_set (G_OBJECT (sink), "location", "mylocation", NULL);
  g_object_set (G_OBJECT (sink), "file-name", "myfile", NULL);
  g_object_set (G_OBJECT (sink), "user", "usr", NULL);
  g_object_set (G_OBJECT (sink), "passwd", "passwd", NULL);
  g_object_set (G_OBJECT (sink), "mail-rcpt", "rcpt", NULL);
  g_object_set (G_OBJECT (sink), "mail-from", "sender", NULL);
  g_object_set (G_OBJECT (sink), "subject", "subject", NULL);
  g_object_set (G_OBJECT (sink), "message-body", "message", NULL);
  g_object_set (G_OBJECT (sink), "nbr-attachments", 5, NULL);
  g_object_set (G_OBJECT (sink), "use-ssl", TRUE, NULL);
  g_object_set (G_OBJECT (sink), "pop-location", "poploc", NULL);
  g_object_set (G_OBJECT (sink), "pop-user", "popusr", NULL);
  g_object_set (G_OBJECT (sink), "pop-passwd", "poppasswd", NULL);

  g_object_get (sink,
      "location", &res_location,
      "file-name", &res_file_name,
      "user", &res_usr,
      "passwd", &res_passwd,
      "mail-rcpt", &res_mail_rcpt,
      "mail-from", &res_mail_from,
      "subject", &res_subj,
      "message-body", &res_msg,
      "nbr-attachments", &res_nbr_attach,
      "use-ssl", &res_use_ssl,
      "pop-location", &res_pop_location,
      "pop_user", &res_pop_usr, "pop-passwd", &res_pop_passwd, NULL);

  fail_unless (strncmp (res_location, "mylocation", strlen ("mylocation"))
      == 0);
  fail_unless (strncmp (res_file_name, "myfile", strlen ("myfile"))
      == 0);
  fail_unless (strncmp (res_usr, "usr", strlen ("usr"))
      == 0);
  fail_unless (strncmp (res_passwd, "passwd", strlen ("passwd"))
      == 0);
  fail_unless (strncmp (res_mail_rcpt, "rcpt", strlen ("rcpt"))
      == 0);
  fail_unless (strncmp (res_mail_from, "sender", strlen ("sender"))
      == 0);
  fail_unless (strncmp (res_subj, "subject", strlen ("subject"))
      == 0);
  fail_unless (strncmp (res_msg, "message", strlen ("message"))
      == 0);
  fail_unless (strncmp (res_pop_location, "poploc", strlen ("poploc"))
      == 0);
  fail_unless (strncmp (res_pop_usr, "popusr", strlen ("popusr"))
      == 0);
  fail_unless (strncmp (res_pop_passwd, "poppasswd", strlen ("poppasswd"))
      == 0);
  fail_unless (res_nbr_attach == 5);
  fail_unless (res_use_ssl == TRUE);
  g_free (res_location);
  g_free (res_file_name);
  g_free (res_usr);
  g_free (res_passwd);
  g_free (res_mail_rcpt);
  g_free (res_mail_from);
  g_free (res_subj);
  g_free (res_msg);
  g_free (res_pop_location);
  g_free (res_pop_usr);
  g_free (res_pop_passwd);

  /* change properties */
  g_object_set (G_OBJECT (sink), "location", "newlocation", NULL);
  g_object_set (G_OBJECT (sink), "file-name", "newfilename", NULL);
  g_object_set (G_OBJECT (sink), "user", "newusr", NULL);
  g_object_set (G_OBJECT (sink), "passwd", "newpasswd", NULL);
  g_object_set (G_OBJECT (sink), "mail-rcpt", "rcpt1,rcpt2,rcpt3", NULL);
  g_object_set (G_OBJECT (sink), "mail-from", "newsender", NULL);
  g_object_set (G_OBJECT (sink), "subject", "newsubject", NULL);
  g_object_set (G_OBJECT (sink), "message-body", "newmessage", NULL);
  g_object_set (G_OBJECT (sink), "nbr-attachments", 1, NULL);
  g_object_set (G_OBJECT (sink), "use-ssl", FALSE, NULL);
  g_object_set (G_OBJECT (sink), "pop-location", "newpoploc", NULL);
  g_object_set (G_OBJECT (sink), "pop-user", "newpopusr", NULL);
  g_object_set (G_OBJECT (sink), "pop-passwd", "newpoppasswd", NULL);

  g_object_get (sink,
      "location", &res_location,
      "file-name", &res_file_name,
      "user", &res_usr,
      "passwd", &res_passwd,
      "pop_user", &res_pop_usr,
      "pop-passwd", &res_pop_passwd,
      "pop-location", &res_pop_location,
      "nbr-attachments", &res_nbr_attach,
      "subject", &res_subj,
      "use-ssl", &res_use_ssl,
      "message-body", &res_msg,
      "mail-from", &res_mail_from, "mail-rcpt", &res_mail_rcpt, NULL);

  fail_unless (strncmp (res_location, "newlocation", strlen ("newlocation"))
      == 0);
  fail_unless (strncmp (res_file_name, "newfilename", strlen ("newfilename"))
      == 0);
  fail_unless (strncmp (res_usr, "newusr", strlen ("newusr"))
      == 0);
  fail_unless (strncmp (res_passwd, "newpasswd", strlen ("newpasswd"))
      == 0);
  fail_unless (strncmp (res_mail_rcpt, "rcpt1,rcpt2,rcpt3",
          strlen ("rcpt1,rcpt2,rcpt3")) == 0);
  fail_unless (strncmp (res_mail_from, "newsender", strlen ("newsender"))
      == 0);
  fail_unless (strncmp (res_subj, "newsubject", strlen ("newsubject"))
      == 0);
  fail_unless (strncmp (res_msg, "newmessage", strlen ("newmessage"))
      == 0);
  fail_unless (strncmp (res_pop_location, "newpoploc", strlen ("newpoploc"))
      == 0);
  fail_unless (strncmp (res_pop_usr, "newpopusr", strlen ("newpopusr"))
      == 0);
  fail_unless (strncmp (res_pop_passwd, "newpoppasswd", strlen ("newpoppasswd"))
      == 0);

  fail_unless (res_nbr_attach == 1);
  fail_unless (res_use_ssl == FALSE);
  g_free (res_location);
  g_free (res_file_name);
  g_free (res_usr);
  g_free (res_passwd);
  g_free (res_mail_from);
  g_free (res_mail_rcpt);
  g_free (res_subj);
  g_free (res_msg);
  g_free (res_pop_location);
  g_free (res_pop_usr);
  g_free (res_pop_passwd);

  cleanup_curlsmtpsink (sink);
}

GST_END_TEST;

static Suite *
curlsink_suite (void)
{
  Suite *s = suite_create ("curlsmtpsink");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_set_timeout (tc_chain, 20);
  tcase_add_test (tc_chain, test_properties);

  return s;
}

GST_CHECK_MAIN (curlsink);
