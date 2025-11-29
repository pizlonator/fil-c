#include <errno.h>
#include <stdio.h>
#include <sysprof-capture.h>
#include <gio/gio.h>

typedef struct {
  const char *group;
  gint64 value;
} Data;

static bool
callback (const SysprofCaptureFrame *frame,
          gpointer              user_data)
{
  Data *data = user_data;

  if (frame->type == SYSPROF_CAPTURE_FRAME_MARK)
    {
      SysprofCaptureMark *mark = (SysprofCaptureMark *)frame;
      if (strcmp (mark->group, "gtk") == 0 &&
          strcmp (mark->name, data->group) == 0)
        {
          data->value = mark->duration;
          return FALSE;
        }
    }

  return TRUE;
}

#define MILLISECONDS(v) ((v) / (1000.0 * G_TIME_SPAN_MILLISECOND))

static int opt_rep = 10;

static GOptionEntry options[] = {
  { "runs", 'r', G_OPTION_FLAG_NONE, G_OPTION_ARG_INT, &opt_rep, "Number of runs", "COUNT" },
  { NULL, }
};

int
main (int argc, char *argv[])
{
  GOptionContext *context;
  GError *error = NULL;
  Data data;
  SysprofCaptureFrameType type;
  char fd_str[20];
  gint64 *values;
  gint64 min, max, total;
  int i;

  context = g_option_context_new ("COMMANDLINE");
  g_option_context_add_main_entries (context, options, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    g_error ("Parsing options: %s", error->message);

  if (argc < 2)
    {
      g_print ("Usage: testperf [OPTIONS] COMMANDLINE\n");
      exit (1);
    }

  if (opt_rep < 1)
    g_error ("COUNT must be a positive number");

  values = g_new (gint64, opt_rep);

  for (i = 0; i < opt_rep; i++)
    {
      GSubprocessLauncher *launcher;
      GSubprocess *subprocess;
      int fd;
      char *name;
      SysprofCaptureReader *reader;
      SysprofCaptureCursor *cursor;
      SysprofCaptureCondition *condition;

      fd = g_file_open_tmp ("gtk.XXXXXX.syscap", &name, &error);
      if (error)
        g_error ("Create syscap file: %s", error->message);

      launcher = g_subprocess_launcher_new (0);
      g_subprocess_launcher_take_fd (launcher, fd, fd);
      g_snprintf (fd_str, sizeof (fd_str), "%d", fd);
      g_subprocess_launcher_setenv (launcher, "GTK_TRACE_FD", fd_str, TRUE);
      g_subprocess_launcher_setenv (launcher, "GTK_DEBUG_AUTO_QUIT", "1", TRUE);

      subprocess = g_subprocess_launcher_spawnv (launcher, (const char *const *)argv + 1, &error);
      if (error)
        g_error ("Launch child: %s", error->message);

      if (!g_subprocess_wait (subprocess, NULL, &error))
        g_error ("Run child: %s", error->message);

      g_object_unref (subprocess);
      g_object_unref (launcher);

      reader = sysprof_capture_reader_new (name);
      if (!reader)
        g_error ("Opening syscap file: %s", g_strerror (errno));

      data.group = "style";
      data.value = 0;

      cursor = sysprof_capture_cursor_new (reader);

      type = SYSPROF_CAPTURE_FRAME_MARK;
      condition = sysprof_capture_condition_new_where_type_in (1, &type);
      sysprof_capture_cursor_add_condition (cursor, condition);

      sysprof_capture_cursor_foreach (cursor, callback, &data);

      values[i] = data.value;

      sysprof_capture_cursor_unref (cursor);
      sysprof_capture_reader_unref (reader);

      remove (name);

      g_free (name);
    }

  min = G_MAXINT64;
  max = 0;
  total = 0;

  for (i = 0; i < opt_rep; i++)
    {
      if (min > values[i])
        min = values[i];
      if (max < values[i])
        max = values[i];
      total += values[i];
    }

  g_print ("%d runs, min %g, max %g, avg %g\n",
           opt_rep,
           MILLISECONDS (min),
           MILLISECONDS (max),
           MILLISECONDS (total / opt_rep));
}
