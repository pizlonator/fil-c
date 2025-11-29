#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

static void
test_to_text_list (void)
{
#ifdef GDK_WINDOWING_X11
  GdkDisplay *display;

  display = gdk_display_get_default ();

  if (GDK_IS_X11_DISPLAY (display))
    {
      const char *encoding;
      int format;
      const guchar *text;
      int length;
      char **list;
      int n;

      encoding = g_intern_string ("UTF8_STRING");
      format = 8;
      text = (const guchar*)"abcdef \304\201 \304\205\0ABCDEF \304\200 \304\204";
      length = 25;
      n = gdk_x11_display_text_property_to_text_list (display, encoding, format, text, length, &list);
      g_assert_cmpint (n, ==, 2);
      g_assert_true (g_str_has_prefix (list[0], "abcdef "));
      g_assert_true (g_str_has_prefix (list[1], "ABCDEF "));

      gdk_x11_free_text_list (list);
    }
  else
#endif
    g_test_skip ("No X11 display");
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  gtk_init ();

  g_test_add_func ("/encoding/to-text-list", test_to_text_list);

  return g_test_run ();
}
