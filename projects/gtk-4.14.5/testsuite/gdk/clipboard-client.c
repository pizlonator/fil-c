#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#endif

#ifdef G_OS_WIN32
#include <io.h>
#include <process.h>
#endif

G_GNUC_NORETURN static void
got_string_cb (GObject      *source,
               GAsyncResult *result,
               gpointer      data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (source);
  GError *error = NULL;
  char *text;

  text = gdk_clipboard_read_text_finish (clipboard, result, &error);
  if (text)
    {
      g_print ("%s", text);
      g_free (text);
    }
  else
    {
      g_print ("ERROR: %s", error->message);
      g_clear_error (&error);
    }

  exit (0);
}

G_GNUC_NORETURN static void
got_text_cb (GObject      *source,
             GAsyncResult *result,
             gpointer      data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (source);
  GError *error = NULL;
  char *text;

  text = gdk_clipboard_read_text_finish (clipboard, result, &error);
  if (text)
    {
      int fd;
      char *name;

      fd = g_file_open_tmp ("XXXXXX.out", &name, &error);
      if (error)
        g_error ("Failed to create tmp file: %s", error->message);
      close (fd);
      g_file_set_contents (name, text, -1, &error);
      g_print ("%s", name);
      g_free (text);
      g_free (name);
    }
  else
    {
      g_print ("ERROR: %s", error->message);
      g_clear_error (&error);
    }

  exit (0);
}

G_GNUC_NORETURN static void
got_texture_cb (GObject      *source,
                GAsyncResult *result,
                gpointer      data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (source);
  GError *error = NULL;
  GdkTexture *texture;

  texture = gdk_clipboard_read_texture_finish (clipboard, result, &error);
  if (texture)
    {
      int fd;
      char *name;

      fd = g_file_open_tmp ("XXXXXX.out", &name, &error);
      if (error)
        g_error ("Failed to create tmp file: %s", error->message);
      close (fd);
      gdk_texture_save_to_png (texture, name);
      g_print ("%s", name);
      g_object_unref (texture);
      g_free (name);
    }
  else
    {
      g_print ("ERROR: %s", error->message);
      g_clear_error (&error);
    }

  exit (0);
}

G_GNUC_NORETURN static void
got_file (GObject      *source,
          GAsyncResult *result,
          gpointer      data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (source);
  GError *error = NULL;
  const GValue *value;

  value = gdk_clipboard_read_value_finish (clipboard, result, &error);
  if (value)
    {
      GFile *file = g_value_get_object (value);
      char *path = g_file_get_path (file);
      g_print ("%s", path);
      g_free (path);
    }
  else
    {
      g_print ("ERROR: %s", error->message);
      g_clear_error (&error);
    }

  exit (0);
}

G_GNUC_NORETURN static void
got_files (GObject      *source,
           GAsyncResult *result,
           gpointer      data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (source);
  GError *error = NULL;
  const GValue *value;

  value = gdk_clipboard_read_value_finish (clipboard, result, &error);
  if (value)
    {
      GSList *files = g_value_get_boxed (value);
      for (GSList *l = files; l; l = l->next)
        {
          GFile *file = l->data;
          char *path = g_file_get_path (file);
          if (l != files)
            g_print (":");
          g_print ("%s", path);
          g_free (path);
        }
    }
  else
    {
      g_print ("ERROR: %s", error->message);
      g_clear_error (&error);
    }

  exit (0);
}

G_GNUC_NORETURN static void
got_color (GObject      *source,
           GAsyncResult *result,
           gpointer      data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (source);
  GError *error = NULL;
  const GValue *value;

  value = gdk_clipboard_read_value_finish (clipboard, result, &error);
  if (value)
    {
      GdkRGBA *color = g_value_get_boxed (value);
      char *s = gdk_rgba_to_string (color);
      g_print ("%s", s);
      g_free (s);
    }
  else
    {
      g_print ("ERROR: %s", error->message);
      g_clear_error (&error);
    }

  exit (0);
}

static const char *action;
static const char *type;
static const char *value;
static gulong handler;

static void
do_it (GObject    *object,
       GParamSpec *pspec)
{
  GdkClipboard *clipboard;

  if (object)
    g_signal_handler_disconnect (object, handler);

  clipboard = gdk_display_get_clipboard (gdk_display_get_default ());

  if (strcmp (action, "info") == 0)
    {
      GdkContentFormats *formats;
      char *s;

      formats = gdk_clipboard_get_formats (clipboard);
      s = gdk_content_formats_to_string (formats);
      g_print ("%s\n", s);
      g_free (s);
    }
  else if (strcmp (action, "set") == 0)
    {
      GdkContentFormats *formats;
      char *s;

      if (strcmp (type, "string") == 0)
        {
          gdk_clipboard_set_text (clipboard, value);
        }
      else if (strcmp (type, "text") == 0)
        {
          char *contents;
          gsize len;

          if (!g_file_get_contents (value, &contents, &len, NULL))
            g_error ("Failed to read %s\n", value);

          gdk_clipboard_set_text (clipboard, contents);
          g_free (contents);
        }
      else if (strcmp (type, "image") == 0)
        {
          GFile *file;
          GdkTexture *texture;

          file = g_file_new_for_commandline_arg (value);
          texture = gdk_texture_new_from_file (file, NULL);
          if (!texture)
            g_error ("Failed to read %s\n", value);

          gdk_clipboard_set_texture (clipboard, texture);
          g_object_unref (texture);
          g_object_unref (file);
        }
      else if (strcmp (type, "file") == 0)
        {
          GFile *file;

          file = g_file_new_for_commandline_arg (value);
          gdk_clipboard_set (clipboard, G_TYPE_FILE, file);
          g_object_unref (file);
        }
      else if (strcmp (type, "files") == 0)
        {
          char **strv;
          GSList *files;

          strv = g_strsplit (value, ":", 0);

          files = NULL;
          for (int i = 0; strv[i]; i++)
            files = g_slist_append (files, g_file_new_for_commandline_arg (strv[i]));

          gdk_clipboard_set (clipboard, GDK_TYPE_FILE_LIST, files);

          g_slist_free_full (files, g_object_unref);
          g_strfreev (strv);
        }
      else if (strcmp (type, "color") == 0)
        {
          GdkRGBA color;

          gdk_rgba_parse (&color, value);
          gdk_clipboard_set (clipboard, GDK_TYPE_RGBA, &color);
        }
      else
        g_error ("can't set %s", type);

      formats = gdk_clipboard_get_formats (clipboard);
      s = gdk_content_formats_to_string (formats);
      g_print ("%s\n", s);
      g_free (s);
    }
  else if (strcmp (action, "get") == 0)
    {
      if (strcmp (type, "string") == 0)
        {
          gdk_clipboard_read_text_async (clipboard, NULL, got_string_cb, NULL);
        }
      else if (strcmp (type, "text") == 0)
        {
          gdk_clipboard_read_text_async (clipboard, NULL, got_text_cb, NULL);
        }
      else if (strcmp (type, "image") == 0)
        {
          gdk_clipboard_read_texture_async (clipboard, NULL, got_texture_cb, NULL);
        }
      else if (strcmp (type, "file") == 0)
        {
          gdk_clipboard_read_value_async (clipboard, G_TYPE_FILE, 0, NULL, got_file, NULL);
        }
      else if (strcmp (type, "files") == 0)
        {
          gdk_clipboard_read_value_async (clipboard, GDK_TYPE_FILE_LIST, 0, NULL, got_files, NULL);
        }
      else if (strcmp (type, "color") == 0)
        {
          gdk_clipboard_read_value_async (clipboard, GDK_TYPE_RGBA, 0, NULL, got_color, NULL);
        }
      else
        g_error ("can't get %s", type);
    }
  else
    g_error ("can only set, get or info");
}

int
main (int argc, char *argv[])
{
  gboolean done = FALSE;

  if (argc < 2)
    g_error ("too few arguments");

  action = argv[1];

  if (strcmp (action, "info") == 0)
    {
    }
  else if (strcmp (action, "set") == 0)
    {
      if (argc < 4)
        g_error ("too few arguments for set");

      type = argv[2];
      value = argv[3];
    }
  else if (strcmp (action, "get") == 0)
    {
      if (argc < 3)
        g_error ("too few arguments for get");

      type = argv[2];
    }
  else
    g_error ("can only set or get");

  gtk_init ();

  /* Don't wait for a window manager to give us focus when
   * we may be running on bare wm-less X.
   */
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    {
      GtkWidget *window;

      window = gtk_window_new ();
      gtk_window_present (GTK_WINDOW (window));
      handler = g_signal_connect (window, "notify::is-active", G_CALLBACK (do_it), NULL);
    }
  else
#endif
    do_it (NULL, NULL);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
