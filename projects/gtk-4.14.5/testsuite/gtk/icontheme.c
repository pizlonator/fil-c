#include <gtk/gtk.h>

#include <string.h>

#define SCALABLE_IMAGE_SIZE (128)

static GtkIconTheme *
get_test_icontheme (gboolean force_reload)
{
  static GtkIconTheme *icon_theme = NULL;
  const char *current_dir[2];

  if (force_reload)
    g_clear_object (&icon_theme);

  if (icon_theme)
    return icon_theme;

  icon_theme = gtk_icon_theme_new ();
  gtk_icon_theme_set_theme_name (icon_theme, "icons");
  current_dir[0] = g_test_get_dir (G_TEST_DIST);
  current_dir[1] = NULL;
  gtk_icon_theme_set_search_path (icon_theme, current_dir);

  return icon_theme;
}

static char *
lookup_flags_to_string (GtkIconLookupFlags flags)
{
  GValue flags_value = { 0, }, string_value = { 0, };
  char *result;

  g_value_init (&flags_value, GTK_TYPE_ICON_LOOKUP_FLAGS);
  g_value_init (&string_value, G_TYPE_STRING);

  g_value_set_flags (&flags_value, flags);
  if (!g_value_transform (&flags_value, &string_value))
    {
      g_assert_not_reached ();
    }

  result = g_value_dup_string (&string_value);

  g_value_unset (&flags_value);
  g_value_unset (&string_value);

  return result;
}

static void
assert_icon_lookup_size (const char         *icon_name,
                         int                 size,
                         GtkTextDirection    direction,
                         GtkIconLookupFlags  flags,
                         gboolean            fallbacks,
                         const char         *filename,
                         int                 pixbuf_size)
{
  GtkIconPaintable *info;
  GFile *file;
  char *uri = NULL;

  if (fallbacks)
    {
      GThemedIcon *fallback_icons = G_THEMED_ICON (g_themed_icon_new_with_default_fallbacks (icon_name));
      const char **fallback_names = (const char **) g_themed_icon_get_names (fallback_icons);
      info = gtk_icon_theme_lookup_icon (get_test_icontheme (FALSE), icon_name, &fallback_names[1], size, 1, direction, flags);
      g_object_unref (fallback_icons);
    }
  else
    {
      info = gtk_icon_theme_lookup_icon (get_test_icontheme (FALSE), icon_name, NULL, size, 1, direction, flags);
    }

  if (info == NULL)
    {
      g_error ("Could not look up an icon for \"%s\" with flags %s at size %d",
               icon_name, lookup_flags_to_string (flags), size);
      return;
    }

  file = gtk_icon_paintable_get_file (info);
  if (file)
    {
      uri = g_file_get_uri (file);
      g_object_unref (file);
    }

  if (filename)
    {
      if (uri == NULL || !g_str_has_suffix (uri, filename))
        {
          g_error ("Icon for \"%s\" with flags %s at size %d should be \"...%s\" but is \"...%s\"",
                   icon_name, lookup_flags_to_string (flags), size,
                   filename, uri);
          return;
        }
    }
  else
    {
      g_assert_null (uri);
    }

  g_free (uri);

  g_assert_cmpint (gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (info)), ==, size);

  g_object_unref (info);
}

static void
assert_icon_lookup (const char         *icon_name,
                    int                 size,
                    GtkTextDirection    direction,
                    GtkIconLookupFlags  flags,
                    gboolean            fallbacks,
                    const char         *filename)
{
  assert_icon_lookup_size (icon_name, size, direction, flags, fallbacks, filename, -1);
}

static void
assert_icon_lookup_fails (const char         *icon_name,
                          int                 size,
                          GtkTextDirection    direction,
                          GtkIconLookupFlags  flags)
{
  GtkIconPaintable *info;

  info = gtk_icon_theme_lookup_icon (get_test_icontheme (FALSE), icon_name, NULL, size, 1, direction, flags);

  /* We never truly *fail*, but check that we got the image-missing fallback */
  g_assert_nonnull (info);
  g_assert_cmpstr (gtk_icon_paintable_get_icon_name (info), ==, "image-missing");
}

static void
do_icon_lookup (const char         *icon_name,
                int                 size,
                GtkTextDirection    direction,
                GtkIconLookupFlags  flags,
                gboolean            fallbacks)
{
  GtkIconPaintable *info = NULL;

  if (fallbacks)
    {
      GThemedIcon *fallback_icons = G_THEMED_ICON (g_themed_icon_new_with_default_fallbacks (icon_name));
      const char **fallback_names = (const char **) g_themed_icon_get_names (fallback_icons);
      info = gtk_icon_theme_lookup_icon (get_test_icontheme (FALSE), icon_name, &fallback_names[1], size, 1, direction, flags);
      g_object_unref (fallback_icons);
    }
  else
    {
      info = gtk_icon_theme_lookup_icon (get_test_icontheme (FALSE), icon_name, NULL, size, 1, direction, flags);
    }

  if (info)
    g_object_unref (info);
}

static char *
make_lookup_pattern (const char *first_name,
                     ...)
{
  GString *s;
  va_list args;
  char *name;

  s = g_string_new ("");
  g_string_append_printf (s, "*lookup name: %s", first_name);

  va_start (args, first_name);

  while ((name = va_arg (args, char *)) != NULL)
    g_string_append_printf (s, "*lookup name: %s", name);

  va_end (args);

  g_string_append (s, "*");

  return g_string_free (s, FALSE);
}

#define LOOKUP_ORDER_TEST(func,icon_name,size,direction,flags,fallback,...)           \
static void                                                                       \
func (void)                                           \
{                                                                                 \
  char *pattern;                                                                  \
  if (g_test_subprocess ())                                                       \
    {                                                                             \
      guint debug_flags = gtk_get_debug_flags ();                                 \
      gtk_set_debug_flags (debug_flags | GTK_DEBUG_ICONTHEME);                    \
      do_icon_lookup (icon_name, size, direction, flags, fallback);               \
      return;                                                                     \
    }                                                                             \
  g_test_trap_subprocess (NULL, 0, 0);                                            \
  g_test_trap_assert_passed ();                                                   \
  pattern = make_lookup_pattern (__VA_ARGS__, NULL);                              \
  g_test_trap_assert_stderr (pattern);                                            \
  g_free (pattern);                                                               \
}

LOOKUP_ORDER_TEST (test_lookup_order0, "foo-bar-baz", 16, GTK_TEXT_DIR_NONE, 0, TRUE,
                   "foo-bar-baz",
                   "foo-bar",
                   "foo",
                   "foo-bar-baz-symbolic",
                   "foo-bar-symbolic",
                   "foo-symbolic")

LOOKUP_ORDER_TEST (test_lookup_order1, "foo-bar-baz", 16, GTK_TEXT_DIR_RTL, 0, TRUE,
                   "foo-bar-baz-rtl",
                   "foo-bar-baz",
                   "foo-bar-rtl",
                   "foo-bar",
                   "foo-rtl",
                   "foo",
                   "foo-bar-baz-symbolic-rtl",
                   "foo-bar-baz-symbolic",
                   "foo-bar-symbolic-rtl",
                   "foo-bar-symbolic",
                   "foo-symbolic-rtl",
                   "foo-symbolic")

LOOKUP_ORDER_TEST (test_lookup_order2, "foo-bar-baz", 16, GTK_TEXT_DIR_RTL, 0, FALSE,
                   "foo-bar-baz-rtl",
                   "foo-bar-baz")

LOOKUP_ORDER_TEST (test_lookup_order3, "foo-bar-baz-symbolic", 16, GTK_TEXT_DIR_NONE, 0, TRUE,
                   "foo-bar-baz-symbolic",
                   "foo-bar-symbolic",
                   "foo-symbolic",
                   "foo-bar-baz",
                   "foo-bar",
                   "foo")

LOOKUP_ORDER_TEST (test_lookup_order4, "bla-bla", 16, GTK_TEXT_DIR_NONE, GTK_ICON_LOOKUP_FORCE_SYMBOLIC, TRUE,
                   "bla-bla-symbolic",
                   "bla-symbolic",
                   "bla-bla-symbolic", /* awkward */
                   "bla-symbolic", /* awkward */
                   "bla-bla",
                   "bla")

LOOKUP_ORDER_TEST (test_lookup_order5, "bla-bla-symbolic", 16, GTK_TEXT_DIR_NONE, GTK_ICON_LOOKUP_FORCE_SYMBOLIC, TRUE,
                   "bla-bla-symbolic",
                   "bla-symbolic",
                   "bla-bla-symbolic", /* awkward */
                   "bla-symbolic", /* awkward */
                   "bla-bla",
                   "bla")

LOOKUP_ORDER_TEST (test_lookup_order6, "bar-baz", 16, GTK_TEXT_DIR_RTL, GTK_ICON_LOOKUP_FORCE_SYMBOLIC, TRUE,
                   "bar-baz-symbolic-rtl",
                   "bar-baz-symbolic",
                   "bar-symbolic-rtl",
                   "bar-symbolic",
                   "bar-baz-symbolic-rtl", /* awkward */
                   "bar-baz-symbolic", /* awkward */
                   "bar-symbolic-rtl", /* awkward */
                   "bar-symbolic", /* awkward */
                   "bar-baz-rtl",
                   "bar-baz",
                   "bar-rtl",
                   "bar")

LOOKUP_ORDER_TEST (test_lookup_order7, "bar-baz-symbolic", 16, GTK_TEXT_DIR_RTL, GTK_ICON_LOOKUP_FORCE_SYMBOLIC, TRUE,
                   "bar-baz-symbolic-rtl",
                   "bar-baz-symbolic",
                   "bar-symbolic-rtl",
                   "bar-symbolic",
                   "bar-baz-symbolic-rtl", /* awkward */
                   "bar-baz-symbolic", /* awkward */
                   "bar-symbolic-rtl", /* awkward */
                   "bar-symbolic", /* awkward */
                   "bar-baz-rtl",
                   "bar-baz",
                   "bar-rtl",
                   "bar")

LOOKUP_ORDER_TEST (test_lookup_order8, "bar-baz", 16, GTK_TEXT_DIR_LTR, GTK_ICON_LOOKUP_FORCE_SYMBOLIC, TRUE,
                   "bar-baz-symbolic-ltr",
                   "bar-baz-symbolic",
                   "bar-symbolic-ltr",
                   "bar-symbolic",
                   "bar-baz-symbolic-ltr", /* awkward */
                   "bar-baz-symbolic", /* awkward */
                   "bar-symbolic-ltr", /* awkward */
                   "bar-symbolic", /* awkward */
                   "bar-baz-ltr",
                   "bar-baz",
                   "bar-ltr",
                   "bar")

LOOKUP_ORDER_TEST (test_lookup_order9, "bar-baz-symbolic", 16, GTK_TEXT_DIR_LTR, GTK_ICON_LOOKUP_FORCE_SYMBOLIC, TRUE,
                   "bar-baz-symbolic-ltr",
                   "bar-baz-symbolic",
                   "bar-symbolic-ltr",
                   "bar-symbolic",
                   "bar-baz-symbolic-ltr", /* awkward */
                   "bar-baz-symbolic", /* awkward */
                   "bar-symbolic-ltr", /* awkward */
                   "bar-symbolic", /* awkward */
                   "bar-baz-ltr",
                   "bar-baz",
                   "bar-ltr",
                   "bar")

static void
test_basics (void)
{
  /* just a basic boring lookup so we know everything works */
  assert_icon_lookup ("simple", 16, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/16x16/simple.png");
}

static void
test_generic_fallback (void)
{
  /* simple test for generic fallback */
  assert_icon_lookup ("simple-foo-bar",
                      16,
                      GTK_TEXT_DIR_NONE,
                      0,
                      TRUE,
                      "/icons/16x16/simple.png");

  /* Check generic fallback also works for symbolics falling back to regular items */
  assert_icon_lookup ("simple-foo-bar-symbolic",
                      16,
                      GTK_TEXT_DIR_NONE,
                      0,
                      TRUE,
                      "/icons/16x16/simple.png");

  /* Check we fall back to more generic symbolic icons before falling back to
   * non-symbolics */
  assert_icon_lookup ("everything-justregular-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      0,
                      TRUE,
                      "/icons/scalable/everything-symbolic.svg");
}

static void
test_force_symbolic (void)
{
  /* check forcing symbolic works */
  assert_icon_lookup ("everything",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      FALSE,
                      "/icons/scalable/everything-symbolic.svg");
  /* check forcing symbolic also works for symbolic icons (d'oh) */
  assert_icon_lookup ("everything-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      FALSE,
                      "/icons/scalable/everything-symbolic.svg");

  /* check all the combos for fallbacks on an icon that only exists as symbolic */
  assert_icon_lookup ("everything-justsymbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      FALSE,
                      "/icons/scalable/everything-justsymbolic-symbolic.svg");
  assert_icon_lookup ("everything-justsymbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      TRUE,
                      "/icons/scalable/everything-justsymbolic-symbolic.svg");
  assert_icon_lookup ("everything-justsymbolic-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      FALSE,
                      "/icons/scalable/everything-justsymbolic-symbolic.svg");
  assert_icon_lookup ("everything-justsymbolic-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      TRUE,
                      "/icons/scalable/everything-justsymbolic-symbolic.svg");

  /* check all the combos for fallbacks, this time for an icon that only exists as regular */
  assert_icon_lookup ("everything-justregular",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      FALSE,
                      "/icons/scalable/everything-justregular.svg");
  assert_icon_lookup ("everything-justregular",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      TRUE,
                      "/icons/scalable/everything-symbolic.svg");
  assert_icon_lookup_fails ("everything-justregular-symbolic",
                            SCALABLE_IMAGE_SIZE,
                            GTK_TEXT_DIR_NONE,
                            GTK_ICON_LOOKUP_FORCE_SYMBOLIC);
  assert_icon_lookup ("everything-justregular-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_SYMBOLIC,
                      TRUE,
                      "/icons/scalable/everything-symbolic.svg");
}

static void
test_force_regular (void)
{
  /* check forcing regular works (d'oh) */
  assert_icon_lookup ("everything",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_REGULAR,
                      FALSE,
                      "/icons/scalable/everything.svg");
  /* check forcing regular also works for symbolic icons ) */
  assert_icon_lookup ("everything-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_REGULAR,
                      FALSE,
                      "/icons/scalable/everything.svg");

  /* check all the combos for fallbacks on an icon that only exists as regular */
  assert_icon_lookup ("everything-justregular",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_REGULAR,
                      FALSE,
                      "/icons/scalable/everything-justregular.svg");
  assert_icon_lookup ("everything-justregular",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_REGULAR,
                      TRUE,
                      "/icons/scalable/everything-justregular.svg");
  assert_icon_lookup ("everything-justregular-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_REGULAR,
                      FALSE,
                      "/icons/scalable/everything-justregular.svg");
  assert_icon_lookup ("everything-justregular-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_REGULAR,
                      TRUE,
                      "/icons/scalable/everything-justregular.svg");

  /* check all the combos for fallbacks, this time for an icon that only exists as symbolic */
  assert_icon_lookup_fails ("everything-justsymbolic",
                            SCALABLE_IMAGE_SIZE,
                            GTK_TEXT_DIR_NONE,
                            GTK_ICON_LOOKUP_FORCE_REGULAR);
  assert_icon_lookup ("everything-justsymbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_REGULAR,
                      TRUE,
                      "/icons/scalable/everything.svg");
  assert_icon_lookup ("everything-justsymbolic-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_REGULAR,
                      FALSE,
                      "/icons/scalable/everything-justsymbolic-symbolic.svg");
  assert_icon_lookup ("everything-justsymbolic-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      GTK_ICON_LOOKUP_FORCE_REGULAR,
                      TRUE,
                      "/icons/scalable/everything.svg");
}

static void
test_rtl (void)
{
  assert_icon_lookup ("everything",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_RTL,
                      0,
                      FALSE,
                      "/icons/scalable/everything-rtl.svg");
  assert_icon_lookup ("everything-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_RTL,
                      0,
                      FALSE,
                      "/icons/scalable/everything-symbolic-rtl.svg");

  assert_icon_lookup_fails ("everything-justrtl",
                            SCALABLE_IMAGE_SIZE,
                            GTK_TEXT_DIR_NONE,
                            0);
  assert_icon_lookup_fails ("everything-justrtl",
                            SCALABLE_IMAGE_SIZE,
                            GTK_TEXT_DIR_LTR,
                            0);
  assert_icon_lookup ("everything-justrtl",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_RTL,
                      0,
                      FALSE,
                      "/icons/scalable/everything-justrtl-rtl.svg");

  assert_icon_lookup ("everything-justrtl",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      0,
                      TRUE,
                      "/icons/scalable/everything.svg");
  assert_icon_lookup ("everything-justrtl",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_LTR,
                      0,
                      TRUE,
                      "/icons/scalable/everything.svg");
  assert_icon_lookup ("everything-justrtl",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_RTL,
                      0,
                      TRUE,
                      "/icons/scalable/everything-justrtl-rtl.svg");
}

static void
test_symbolic_single_size (void)
{
  /* Check we properly load a symbolic icon from a sized directory */
  assert_icon_lookup ("only32-symbolic",
                      32,
                      GTK_TEXT_DIR_NONE,
                      0,
                      FALSE,
                      "/icons/32x32/only32-symbolic.svg");
  /* Check that we still properly load it even if a different size is requested */
  assert_icon_lookup ("only32-symbolic",
                      16,
                      GTK_TEXT_DIR_NONE,
                      0,
                      FALSE,
                      "/icons/32x32/only32-symbolic.svg");
  assert_icon_lookup ("only32-symbolic",
                      128,
                      GTK_TEXT_DIR_NONE,
                      0,
                      FALSE,
                      "/icons/32x32/only32-symbolic.svg");
}

static void
test_svg_size (void)
{
  /* Check we properly load a svg icon from a sized directory */
  assert_icon_lookup_size ("twosize-fixed", 48, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/32x32/twosize-fixed.svg", 48);
  assert_icon_lookup_size ("twosize-fixed", 32, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/32x32/twosize-fixed.svg", 32);
  assert_icon_lookup_size ("twosize-fixed", 20, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/32x32/twosize-fixed.svg", 20);
  assert_icon_lookup_size ("twosize-fixed", 16, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/16x16/twosize-fixed.svg", 16);

  /* Check that we still properly load it even if a different size is requested */
  assert_icon_lookup_size ("twosize", 64, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/32x32s/twosize.svg", 64);
  assert_icon_lookup_size ("twosize", 48, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/32x32s/twosize.svg", 48);
  assert_icon_lookup_size ("twosize", 32, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/32x32s/twosize.svg", 32);
  assert_icon_lookup_size ("twosize", 24, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/32x32s/twosize.svg", 24);
  assert_icon_lookup_size ("twosize", 16, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/16x16s/twosize.svg", 16);
  assert_icon_lookup_size ("twosize", 12, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/16x16s/twosize.svg", 12);
  assert_icon_lookup_size ("twosize",  8, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/16x16s/twosize.svg", 8);
}

static void
test_size (void)
{
  assert_icon_lookup_size ("size-test", 12, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/15/size-test.png", 15);
  assert_icon_lookup_size ("size-test", 13, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/15/size-test.png", 15);
  assert_icon_lookup_size ("size-test", 14, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/15/size-test.png", 15);
  assert_icon_lookup_size ("size-test", 15, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/15/size-test.png", 15);
  assert_icon_lookup_size ("size-test", 16, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/16-22/size-test.png", 19);
  assert_icon_lookup_size ("size-test", 17, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/16-22/size-test.png", 19);
  assert_icon_lookup_size ("size-test", 18, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/16-22/size-test.png", 19);
  assert_icon_lookup_size ("size-test", 19, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/16-22/size-test.png", 19);
  /* the next 3 are because we never scale up */
  assert_icon_lookup_size ("size-test", 20, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/25+/size-test.svg", 20);
  assert_icon_lookup_size ("size-test", 21, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/25+/size-test.svg", 21);
  assert_icon_lookup_size ("size-test", 22, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/25+/size-test.svg", 22);

  assert_icon_lookup_size ("size-test", 23, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/25+/size-test.svg", 23);
  assert_icon_lookup_size ("size-test", 23, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/25+/size-test.svg", 23);
  assert_icon_lookup_size ("size-test", 25, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/25+/size-test.svg", 25);
  assert_icon_lookup_size ("size-test", 28, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/25+/size-test.svg", 28);
  /* the next 2 are because we never scale up */
  assert_icon_lookup_size ("size-test", 31, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/35+/size-test.svg", 31);
  assert_icon_lookup_size ("size-test", 34, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/35+/size-test.svg", 34);

  assert_icon_lookup_size ("size-test", 37, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/35+/size-test.svg", 37);
  assert_icon_lookup_size ("size-test", 40, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/35+/size-test.svg", 40);
  assert_icon_lookup_size ("size-test", 45, GTK_TEXT_DIR_NONE, 0, FALSE, "/icons/35+/size-test.svg", 45);
}

static void
test_list (void)
{
  GtkIconTheme *theme;
  char **icons;

  theme = get_test_icontheme (TRUE);
  icons = gtk_icon_theme_get_icon_names (theme);

  g_assert_true (g_strv_contains ((const char * const *)icons, "size-test"));
  g_assert_true (g_strv_contains ((const char * const *)icons, "simple"));
  g_assert_true (g_strv_contains ((const char * const *)icons, "twosize-fixed"));
  g_assert_true (g_strv_contains ((const char * const *)icons, "twosize"));
  g_assert_true (g_strv_contains ((const char * const *)icons, "only32-symbolic"));
  g_assert_true (g_strv_contains ((const char * const *)icons, "everything"));
  g_assert_true (g_strv_contains ((const char * const *)icons, "everything-rtl"));
  g_assert_true (g_strv_contains ((const char * const *)icons, "everything-symbolic"));
  g_assert_true (g_strv_contains ((const char * const *)icons, "everything-justregular"));
  g_assert_true (g_strv_contains ((const char * const *)icons, "everything-justrtl-rtl"));
  g_assert_true (g_strv_contains ((const char * const *)icons, "everything-symbolic-rtl"));
  g_assert_true (g_strv_contains ((const char * const *)icons, "everything-justsymbolic-symbolic"));

  g_assert_true (gtk_icon_theme_has_icon (theme, "size-test"));
  g_assert_true (gtk_icon_theme_has_icon (theme, "simple"));
  g_assert_true (gtk_icon_theme_has_icon (theme, "twosize-fixed"));
  g_assert_true (gtk_icon_theme_has_icon (theme, "twosize"));
  g_assert_true (gtk_icon_theme_has_icon (theme, "only32-symbolic"));
  g_assert_true (gtk_icon_theme_has_icon (theme, "everything"));
  g_assert_true (gtk_icon_theme_has_icon (theme, "everything-rtl"));
  g_assert_true (gtk_icon_theme_has_icon (theme, "everything-symbolic"));
  g_assert_true (gtk_icon_theme_has_icon (theme, "everything-justregular"));
  g_assert_true (gtk_icon_theme_has_icon (theme, "everything-justrtl-rtl"));
  g_assert_true (gtk_icon_theme_has_icon (theme, "everything-symbolic-rtl"));
  g_assert_true (gtk_icon_theme_has_icon (theme, "everything-justsymbolic-symbolic"));

  g_strfreev (icons);
}

static void
test_inherit (void)
{
  assert_icon_lookup ("one-two-three",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      0,
                      TRUE,
                      "/icons/scalable/one-two.svg");
  assert_icon_lookup ("one-two-three",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_RTL,
                      0,
                      TRUE,
                      "/icons/scalable/one-two-rtl.svg");
  assert_icon_lookup ("one-two-three-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      0,
                      TRUE,
                      "/icons2/scalable/one-two-three-symbolic.svg");
  assert_icon_lookup ("one-two-three-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_RTL,
                      0,
                      TRUE,
                      "/icons2/scalable/one-two-three-symbolic.svg");
  assert_icon_lookup ("one-two-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_NONE,
                      0,
                      TRUE,
                      "/icons2/scalable/one-two-symbolic.svg");
  assert_icon_lookup ("one-two-symbolic",
                      SCALABLE_IMAGE_SIZE,
                      GTK_TEXT_DIR_RTL,
                      0,
                      TRUE,
                      "/icons2/scalable/one-two-symbolic-rtl.svg");
}

static void
test_nonsquare_symbolic (void)
{
  int width, height, size;
  GtkIconTheme *icon_theme;
  GtkIconPaintable *info;
  GFile *file;
  GIcon *icon;
  GError *error = NULL;
  GdkPixbuf *pixbuf;
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  graphene_rect_t bounds;

  char *path = g_build_filename (g_test_get_dir (G_TEST_DIST),
                                  "icons",
                                  "scalable",
                                  "nonsquare-symbolic.svg",
                                  NULL);

  /* load the original image for reference */
  pixbuf = gdk_pixbuf_new_from_file (path, &error);

  g_assert_no_error (error);
  g_assert_nonnull (pixbuf);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  size = MAX (width, height);
  g_object_unref (pixbuf);

  g_assert_cmpint (width, !=, height);

  /* now load it through GtkIconTheme */
  icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
  file = g_file_new_for_path (path);
  icon = g_file_icon_new (file);
  info = gtk_icon_theme_lookup_by_gicon (icon_theme, icon,
                                         height, 1, GTK_TEXT_DIR_NONE, 0);
  g_assert_nonnull (info);

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (GDK_PAINTABLE (info), snapshot, size, size);
  node = gtk_snapshot_free_to_node (snapshot);

  /* the original dimensions have been preserved */

  gsk_render_node_get_bounds (node, &bounds);
  g_assert_cmpint (bounds.size.width, ==, width);
  g_assert_cmpint (bounds.size.height, ==, height);

  gsk_render_node_unref (node);
  g_free (path);
  g_object_unref (file);
  g_object_unref (icon);
  g_object_unref (info);
}

static void
require_env (const char *var)
{
  if (g_getenv (var) == NULL)
    g_warning ("Some tests require %s to be set", var);
}

int
main (int argc, char *argv[])
{
  require_env ("G_TEST_SRCDIR");

  gtk_test_init (&argc, &argv);

  g_test_add_func ("/icontheme/basics", test_basics);
  g_test_add_func ("/icontheme/generic-fallback", test_generic_fallback);
  g_test_add_func ("/icontheme/force-symbolic", test_force_symbolic);
  g_test_add_func ("/icontheme/force-regular", test_force_regular);
  g_test_add_func ("/icontheme/rtl", test_rtl);
  g_test_add_func ("/icontheme/symbolic-single-size", test_symbolic_single_size);
  g_test_add_func ("/icontheme/svg-size", test_svg_size);
  g_test_add_func ("/icontheme/size", test_size);
  g_test_add_func ("/icontheme/list", test_list);
  g_test_add_func ("/icontheme/inherit", test_inherit);
  g_test_add_func ("/icontheme/nonsquare-symbolic", test_nonsquare_symbolic);
  g_test_add_func ("/icontheme/lookup_order0", test_lookup_order0);
  g_test_add_func ("/icontheme/lookup_order1", test_lookup_order1);
  g_test_add_func ("/icontheme/lookup_order2", test_lookup_order2);
  g_test_add_func ("/icontheme/lookup_order3", test_lookup_order3);
  g_test_add_func ("/icontheme/lookup_order4", test_lookup_order4);
  g_test_add_func ("/icontheme/lookup_order5", test_lookup_order5);
  g_test_add_func ("/icontheme/lookup_order6", test_lookup_order6);
  g_test_add_func ("/icontheme/lookup_order7", test_lookup_order7);
  g_test_add_func ("/icontheme/lookup_order8", test_lookup_order8);
  g_test_add_func ("/icontheme/lookup_order9", test_lookup_order9);

  return g_test_run();
}
