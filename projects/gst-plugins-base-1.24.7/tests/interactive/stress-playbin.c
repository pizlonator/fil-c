#include <gst/gst.h>
#include <stdlib.h>
#include <string.h>

#define TEST_RUNTIME 120.0      /* how long to run the test, in seconds */

static void
play_file (const gint delay, const gchar * uri)
{
  GstStateChangeReturn sret;
  GstMessage *msg;
  GstElement *play;
  guint wait_nanosecs;

  play = gst_element_factory_make ("playbin", "playbin");

  g_object_set (play, "uri", uri, NULL);
  g_printerr ("Playing %s\n", uri);
  sret = gst_element_set_state (play, GST_STATE_PLAYING);
  if (sret != GST_STATE_CHANGE_ASYNC && sret != GST_STATE_CHANGE_SUCCESS) {
    g_printerr ("ERROR: state change failed, sret=%d\n", sret);
    goto next;
  }

  wait_nanosecs = g_random_int_range (0, GST_MSECOND * delay);
  msg = gst_bus_poll (GST_ELEMENT_BUS (play),
      GST_MESSAGE_ERROR | GST_MESSAGE_EOS, wait_nanosecs);
  if (msg) {
    switch (GST_MESSAGE_TYPE (msg)) {
      case GST_MESSAGE_ERROR:
      {
        GError *gerror;
        gchar *debug;

        gst_message_parse_error (msg, &gerror, &debug);
        gst_object_default_error (GST_MESSAGE_SRC (msg), gerror, debug);
        g_clear_error (&gerror);
        g_free (debug);
        break;
      }
      case GST_MESSAGE_EOS:
        g_printerr ("Got EOS\n");
        break;
      default:
        g_printerr ("Got unexpected %s message\n", GST_MESSAGE_TYPE_NAME (msg));
        break;
    }
    gst_message_unref (msg);
    goto next;
  }

  /* on to the next one */
  g_print (".");

next:
  gst_element_set_state (play, GST_STATE_NULL);
  gst_object_unref (play);
}

static void
check_arg (GPtrArray * files, const gchar * arg)
{
  GDir *dir;

  if ((dir = g_dir_open (arg, 0, NULL))) {
    const gchar *entry;

    while ((entry = g_dir_read_name (dir))) {
      gchar *path;

      path = g_strconcat (arg, G_DIR_SEPARATOR_S, entry, NULL);
      check_arg (files, path);
      g_free (path);
    }

    g_dir_close (dir);
    return;
  } else if (g_file_test (arg, G_FILE_TEST_EXISTS)) {
    /* hack: technically an URI is not just file:// + path, but it'll do here */
    g_ptr_array_add (files, g_strdup_printf ("file://%s", arg));
  }
}

int
main (int argc, char **argv)
{
  GPtrArray *files;
  gchar **args = NULL;
  guint num, i;
  GError *err = NULL;
  gint run = 100;
  GOptionContext *ctx;
  GOptionEntry options[] = {
    {"runtime", '\000', 0, G_OPTION_ARG_INT, &run, "maximum play time (ms)",
        NULL},
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &args, NULL},
    {NULL}
  };
  GTimer *timer;

  ctx = g_option_context_new ("FILES OR DIRECTORIES WITH AUDIO FILES");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());
  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", GST_STR_NULL (err->message));
    g_option_context_free (ctx);
    g_clear_error (&err);
    exit (1);
  }
  g_option_context_free (ctx);

  if (args == NULL || *args == NULL) {
    g_print ("Please provide one or more directories with audio files\n\n");
    return 1;
  }

  files = g_ptr_array_new ();

  num = g_strv_length (args);
  for (i = 0; i < num; ++i) {
    if (g_path_is_absolute (args[i])) {
      check_arg (files, args[i]);
    } else {
      g_warning ("Argument '%s' is not an absolute file path", args[i]);
    }
  }

  if (files->len == 0) {
    g_print ("Did not find any files\n\n");
    return 1;
  }

  timer = g_timer_new ();

  while (g_timer_elapsed (timer, NULL) < TEST_RUNTIME) {
    gint32 idx;

    idx = g_random_int_range (0, files->len);
    play_file (run, (const gchar *) g_ptr_array_index (files, idx));
  }

  g_strfreev (args);
  g_timer_destroy (timer);

  return 0;
}
