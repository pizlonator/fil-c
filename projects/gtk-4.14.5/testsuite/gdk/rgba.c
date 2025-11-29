#include <locale.h>
#include <gdk/gdk.h>

static void
test_color_parse (void)
{
  GdkRGBA color;
  GdkRGBA expected;
  gboolean res;

  res = gdk_rgba_parse (&color, "foo");
  g_assert_true (!res);

  res = gdk_rgba_parse (&color, "");
  g_assert_true (!res);

  expected.red = 100/255.;
  expected.green = 90/255.;
  expected.blue = 80/255.;
  expected.alpha = 0.1;
  res = gdk_rgba_parse (&color, "rgba(100,90,80,0.1)");
  g_assert_true (res);
  g_assert_true (gdk_rgba_equal (&color, &expected));

  expected.red = 0.4;
  expected.green = 0.3;
  expected.blue = 0.2;
  expected.alpha = 0.1;
  res = gdk_rgba_parse (&color, "rgba(40%,30%,20%,0.1)");
  g_assert_true (res);
  g_assert_true (gdk_rgba_equal (&color, &expected));

  res = gdk_rgba_parse (&color, "rgba(  40 % ,  30 %  ,   20 % ,  0.1    )");
  g_assert_true (res);
  g_assert_true (gdk_rgba_equal (&color, &expected));

  expected.red = 1.0;
  expected.green = 0.0;
  expected.blue = 0.0;
  expected.alpha = 1.0;
  res = gdk_rgba_parse (&color, "red");
  g_assert_true (res);
  g_assert_true (gdk_rgba_equal (&color, &expected));

  expected.red = 0.0;
  expected.green = 0x8080 / 65535.;
  expected.blue = 1.0;
  expected.alpha = 1.0;
  res = gdk_rgba_parse (&color, "#0080ff");
  g_assert_true (res);
  g_assert_true (gdk_rgba_equal (&color, &expected));

  expected.red = 0.0;
  expected.green = 0.0;
  expected.blue = 0.0;
  expected.alpha = 1.0;
  res = gdk_rgba_parse (&color, "rgb(0,0,0)");
  g_assert_true (res);
  g_assert_true (gdk_rgba_equal (&color, &expected));

  expected.red = 0.0;
  expected.green = 0x8080 / 65535.;
  expected.blue = 1.0;
  expected.alpha = 0x8888 / 65535.;
  res = gdk_rgba_parse (&color, "#0080ff88");
  g_assert_true (res);
  g_assert_true (gdk_rgba_equal (&color, &expected));

  expected.red = 1.0;
  expected.green = 0.0;
  expected.blue = 0.0;
  expected.alpha = 1.0;
  res = gdk_rgba_parse (&color, "hsl (0, 100%, 50%)");
  g_assert_true (res);
  g_assert_true (gdk_rgba_equal (&color, &expected));

  expected.red = 0.0;
  expected.green = 1.0;
  expected.blue = 0.0;
  expected.alpha = 0.1;
  res = gdk_rgba_parse (&color, "hsla (120, 255, 50%, 0.1)");
  g_assert_true (res);
  g_assert_true (gdk_rgba_equal (&color, &expected));

  expected.red = 0.0;
  expected.green = 0.5;
  expected.blue = 0.5;
  expected.alpha = 1.0;
  res = gdk_rgba_parse (&color, "hsl(180, 100%, 25%)");
  g_assert_true (res);
  g_assert_true (gdk_rgba_equal (&color, &expected));
}

static void
test_color_to_string (void)
{
  GdkRGBA rgba;
  GdkRGBA out;
  char *res;
  char *res_de;
  char *res_en;
  char *orig;

  /* Using /255. values for the r, g, b components should
   * make sure they round-trip exactly without rounding
   * from the double => integer => double conversions */
  rgba.red = 1.0;
  rgba.green = 128/255.;
  rgba.blue = 64/255.;
  rgba.alpha = 0.5;

  orig = g_strdup (setlocale (LC_ALL, NULL));
  res = gdk_rgba_to_string (&rgba);
  gdk_rgba_parse (&out, res);
  g_assert_true (gdk_rgba_equal (&rgba, &out));

  setlocale (LC_ALL, "de_DE.utf-8");
  res_de = gdk_rgba_to_string (&rgba);
  g_assert_cmpstr (res, ==, res_de);

  setlocale (LC_ALL, "en_US.utf-8");
  res_en = gdk_rgba_to_string (&rgba);
  g_assert_cmpstr (res, ==, res_en);

  g_free (res);
  g_free (res_de);
  g_free (res_en);

  setlocale (LC_ALL, orig);
  g_free (orig);
}

static void
test_color_copy (void)
{
  GdkRGBA rgba;
  GdkRGBA *out;

  rgba.red = 0.0;
  rgba.green = 0.1;
  rgba.blue = 0.6;
  rgba.alpha = 0.9;

  out = gdk_rgba_copy (&rgba);
  g_assert_true (gdk_rgba_equal (&rgba, out));

  gdk_rgba_free (out);
}

static void
test_color_parse_nonsense (void)
{
  GdkRGBA color;
  gboolean res;

  /*http://bugzilla.gnome.org/show_bug.cgi?id=667485 */

  res = gdk_rgba_parse (&color, "rgb(,,)");
  g_assert_false (res);

  res = gdk_rgba_parse (&color, "rgb(%,%,%)");
  g_assert_false (res);

  res = gdk_rgba_parse (&color, "rgb(nan,nan,nan)");
  g_assert_false (res);

  res = gdk_rgba_parse (&color, "rgb(inf,inf,inf)");
  g_assert_false (res);

  res = gdk_rgba_parse (&color, "rgb(1p12,0,0)");
  g_assert_false (res);

  res = gdk_rgba_parse (&color, "rgb(5d1%,1,1)");
  g_assert_false (res);

  res = gdk_rgba_parse (&color, "rgb(0,0,0)moo");
  g_assert_false (res);

  res = gdk_rgba_parse (&color, "rgb(0,0,0)  moo");
  g_assert_false (res);

  res = gdk_rgba_parse (&color, "#XGB");
  g_assert_false (res);

  res = gdk_rgba_parse (&color, "#XGBQ");
  g_assert_false (res);

  res = gdk_rgba_parse (&color, "#AAAAXGBQ");
  g_assert_false (res);
}

static void
test_color_hash (void)
{
  GdkRGBA color1;
  GdkRGBA color2;
  guint hash1, hash2;

  gdk_rgba_parse (&color1, "hsla (120, 255, 50%, 0.1)");
  gdk_rgba_parse (&color2, "rgb(0,0,0)");

  hash1 = gdk_rgba_hash (&color1);
  hash2 = gdk_rgba_hash (&color2);

  g_assert_cmpuint (hash1, !=, 0);
  g_assert_cmpuint (hash2, !=, 0);
  g_assert_cmpuint (hash1, !=, hash2);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/rgba/parse", test_color_parse);
  g_test_add_func ("/rgba/parse/nonsense", test_color_parse_nonsense);
  g_test_add_func ("/rgba/to-string", test_color_to_string);
  g_test_add_func ("/rgba/copy", test_color_copy);
  g_test_add_func ("/rgba/hash", test_color_hash);

  return g_test_run ();
}
