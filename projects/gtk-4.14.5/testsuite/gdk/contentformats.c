#include <stdlib.h>

#include <gtk/gtk.h>

static GType
string_type (void)
{
  return G_TYPE_STRING;
}

static struct {
  GType (* type_func) (void);
  const char *mime_type;
} possible_types[] = {
  /* GTypes go here */
  { string_type, NULL },
  { gdk_file_list_get_type, NULL },
  { gdk_rgba_get_type, NULL },
  { gdk_texture_get_type, NULL },
  /* mime types go here */
  { NULL, "text/plain" },
  { NULL, "text/plain;charset=utf-8" },
  { NULL, "image/png" },
  { NULL, "image/jpeg" },
  { NULL, "application/x-color" },
};

#define assert_printf(...) G_STMT_START{ \
  char *_s = g_strdup_printf (__VA_ARGS__); \
  g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, _s); \
  g_free (_s); \
}G_STMT_END

#define assert_formats_subset(a, b) G_STMT_START{ \
  const GType *_gtypes; \
  const char * const *_mime_types; \
  gsize _i, _n; \
\
  _gtypes = gdk_content_formats_get_gtypes (a, &_n); \
  for (_i = 0; _i < _n; _i++) \
    { \
      if (!gdk_content_formats_contain_gtype (b, _gtypes[_i])) \
        assert_printf (#a " ⊆ " #b ": does not contain GType %s", g_type_name (_gtypes[_i])); \
    } \
\
  _mime_types = gdk_content_formats_get_mime_types (a, &_n); \
  for (_i = 0; _i < _n; _i++) \
    { \
      if (!gdk_content_formats_contain_mime_type (b, _mime_types[_i])) \
        assert_printf (#a " ⊆ " #b ": does not contain mime type %s", _mime_types[_i]); \
    } \
}G_STMT_END

#define assert_formats_equal(a, b) G_STMT_START{\
  assert_formats_subset(a, b); \
  assert_formats_subset(b, a); \
}G_STMT_END

static GdkContentFormats *
create_random_content_formats (void)
{
  GdkContentFormatsBuilder *builder;
  gsize i, n;

  n = g_test_rand_int_range (0, G_N_ELEMENTS (possible_types));
  builder = gdk_content_formats_builder_new ();

  for (i = 0; i < n; i++)
    {
      gsize j = g_test_rand_int_range (0, G_N_ELEMENTS (possible_types));
      if (possible_types[j].type_func)
        gdk_content_formats_builder_add_gtype (builder, possible_types[j].type_func ());
      else if (possible_types[j].mime_type)
        gdk_content_formats_builder_add_mime_type (builder, possible_types[j].mime_type);
      else
        g_assert_not_reached ();
    }

  return gdk_content_formats_builder_free_to_formats (builder);
}

static void
test_print_and_parse (void)
{
  GdkContentFormats *before, *parsed;
  char *string_before, *string_parsed;
  gsize i;

  for (i = 0; i < 100; i++)
    {
      before = create_random_content_formats ();
      string_before = gdk_content_formats_to_string (before);

      parsed = gdk_content_formats_parse (string_before);
      g_assert (parsed);
      assert_formats_equal (before, parsed);

      string_parsed = gdk_content_formats_to_string (parsed);
      g_assert_cmpstr (string_before, ==, string_parsed);

      g_free (string_parsed);
      g_free (string_before);
      gdk_content_formats_unref (parsed);
      gdk_content_formats_unref (before);
    }
}

static void
test_union (void)
{
  GdkContentFormatsBuilder *builder;
  GdkContentFormats *a, *b, *ab, *ab2;
  gsize i;

  for (i = 0; i < 100; i++)
    {
      a = create_random_content_formats ();
      b = create_random_content_formats ();

      ab = gdk_content_formats_union (gdk_content_formats_ref (a), b);
      assert_formats_subset (a, ab);
      assert_formats_subset (b, ab);

      ab2 = gdk_content_formats_union (gdk_content_formats_ref (a), ab);
      assert_formats_equal (ab, ab2);
      gdk_content_formats_unref (ab2);

      builder = gdk_content_formats_builder_new ();
      gdk_content_formats_builder_add_formats (builder, a);
      gdk_content_formats_builder_add_formats (builder, b);
      ab2 = gdk_content_formats_builder_free_to_formats (builder);
      assert_formats_equal (ab, ab2);
      gdk_content_formats_unref (ab2);

      gdk_content_formats_unref (ab);
      gdk_content_formats_unref (a);
      gdk_content_formats_unref (b);
    }
}

static void
append_separator (GString *string)
{
  static const char *separators = "\t\n\f\r ";

  do {
    g_string_append_c (string, separators[g_test_rand_int_range (0, strlen (separators))]);
  } while (g_test_rand_bit ());
}

static char *
fuzzy_print (GdkContentFormats *formats)
{
  GString *string;
  const GType *types;
  const char * const *mime_types;
  gsize i, n;
 
  string = g_string_new ("");

  types = gdk_content_formats_get_gtypes (formats, &n);
  for (i = 0; i < n; i++)
    {
      if (string->len || g_test_rand_bit ())
        append_separator (string);
      g_string_append (string, g_type_name (types[i]));
    }

  mime_types = gdk_content_formats_get_mime_types (formats, &n);
  for (i = 0; i < n; i++)
    {
      if (string->len || g_test_rand_bit ())
        append_separator (string);
      g_string_append (string, mime_types[i]);
    }

  if (g_test_rand_bit ())
    append_separator (string);

  return g_string_free (string, FALSE);
}

static void
test_parse (void)
{
  gsize i;

  for (i = 0; i < 100; i++)
    {
      GdkContentFormats *formats, *parsed;
      char *fuzzy;

      formats = create_random_content_formats ();
      fuzzy = fuzzy_print (formats);
      parsed = gdk_content_formats_parse (fuzzy);
      assert_formats_equal (formats, parsed);

      g_free (fuzzy);
      gdk_content_formats_unref (parsed);
      gdk_content_formats_unref (formats);
    }
}

static void
test_parse_fail (void)
{
  static const char *failures[] = {
    "GtkNonexistingType",
    "text/plain TypeAfterMime",
    "notamimetype",
    "image/png stillnotamimetype",
  };
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (failures); i++)
  {
    g_assert_null (gdk_content_formats_parse (failures[i]));
  }
}

static void
test_match (void)
{
  GdkContentFormatsBuilder *builder;
  GdkContentFormats *formats, *formats2;

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_ref (builder);

  gdk_content_formats_builder_add_gtype (builder, GDK_TYPE_RGBA);
  gdk_content_formats_builder_add_mime_type (builder, "image/png");
  formats = gdk_content_formats_builder_free_to_formats (builder);

  gdk_content_formats_builder_add_gtype (builder, G_TYPE_STRING);
  gdk_content_formats_builder_add_mime_type (builder, "text/plain");
  formats2 = gdk_content_formats_builder_free_to_formats (builder);

  g_assert_false (gdk_content_formats_match (formats, formats2));

  gdk_content_formats_unref (formats2);

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_mime_type (builder, "image/png");
  formats2 = gdk_content_formats_builder_free_to_formats (builder);

  g_assert_true (gdk_content_formats_match (formats, formats2));

  gdk_content_formats_unref (formats2);
  gdk_content_formats_unref (formats);
}

int
main (int argc, char *argv[])
{
  gsize i;

  (g_test_init) (&argc, &argv, NULL);

  gtk_init ();

  /* Ensure all the types we care about to exist */
  for (i = 0; i < G_N_ELEMENTS(possible_types); i++)
    {
      if (possible_types[i].type_func)
        g_type_ensure (possible_types[i].type_func ());
    }

  g_test_add_func ("/contentformats/parse", test_parse);
  g_test_add_func ("/contentformats/parse_fail", test_parse_fail);
  g_test_add_func ("/contentformats/print_and_parse", test_print_and_parse);
  g_test_add_func ("/contentformats/union", test_union);
  g_test_add_func ("/contentformats/match", test_match);

  return g_test_run ();
}
