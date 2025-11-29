#include <gtk/gtk.h>
#include "gtk/gtkprivate.h"

#if defined(G_OS_WIN32) || defined(G_WITH_CYGWIN)
#define DO_ESCAPE 0
#else  
#define DO_ESCAPE 1
#endif

typedef struct {
  const char *pat;
  const char *str;
  gboolean no_leading_period;
  gboolean ci;
  gboolean result;
} TestCase;

static TestCase tests[] = {
  { "[a-]", "-", TRUE, FALSE, TRUE },

  { "a", "a", TRUE, FALSE, TRUE },
  { "a", "b", TRUE, FALSE, FALSE },

  /* Test what ? matches */
  { "?", "a", TRUE, FALSE, TRUE },
  { "?", ".", TRUE, FALSE, FALSE },
  { "a?", "a.", TRUE, FALSE, TRUE },
  { "a/?", "a/b", TRUE, FALSE, TRUE },
  { "a/?", "a/.", TRUE, FALSE, FALSE },
  { "?", "/", TRUE, FALSE, FALSE },

  /* Test what * matches */
  { "*", "a", TRUE, FALSE, TRUE },
  { "*", ".", TRUE, FALSE, FALSE },
  { "a*", "a.", TRUE, FALSE, TRUE },
  { "a/*", "a/b", TRUE, FALSE, TRUE },
  { "a/*", "a/.", TRUE, FALSE, FALSE },
  { "*", "/", TRUE, FALSE, FALSE },

  /* Range tests */
  { "[ab]", "a", TRUE, FALSE, TRUE },
  { "[ab]", "c", TRUE, FALSE, FALSE },
  { "[^ab]", "a", TRUE, FALSE, FALSE },
  { "[!ab]", "a", TRUE, FALSE, FALSE },
  { "[^ab]", "c", TRUE, FALSE, TRUE },
  { "[!ab]", "c", TRUE, FALSE, TRUE },
  { "[a-c]", "b", TRUE, FALSE, TRUE },
  { "[a-c]", "d", TRUE, FALSE, FALSE },
  { "[a-]", "-", TRUE, FALSE, TRUE },
  { "[]]", "]", TRUE, FALSE, TRUE },
  { "[^]]", "a", TRUE, FALSE, TRUE },
  { "[!]]", "a", TRUE, FALSE, TRUE },

  /* Various unclosed ranges */
  { "[ab", "a", TRUE, FALSE, FALSE },
  { "[a-", "a", TRUE, FALSE, FALSE },
  { "[ab", "c", TRUE, FALSE, FALSE },
  { "[a-", "c", TRUE, FALSE, FALSE },
  { "[^]", "a", TRUE, FALSE, FALSE },

  /* Ranges and special no-wildcard matches */
  { "[.]", ".", TRUE, FALSE, FALSE },
  { "a[.]", "a.", TRUE, FALSE, TRUE },
  { "a/[.]", "a/.", TRUE, FALSE, FALSE },
  { "[/]", "/", TRUE, FALSE, FALSE },
  { "[^/]", "a", TRUE, FALSE, TRUE },
  
  /* Basic tests of * (and combinations of * and ?) */
  { "a*b", "ab", TRUE, FALSE, TRUE },
  { "a*b", "axb", TRUE, FALSE, TRUE },
  { "a*b", "axxb", TRUE, FALSE, TRUE },
  { "a**b", "ab", TRUE, FALSE, TRUE },
  { "a**b", "axb", TRUE, FALSE, TRUE },
  { "a**b", "axxb", TRUE, FALSE, TRUE },
  { "a*?*b", "ab", TRUE, FALSE, FALSE },
  { "a*?*b", "axb", TRUE, FALSE, TRUE },
  { "a*?*b", "axxb", TRUE, FALSE, TRUE },

  /* Test of  *[range] */
  { "a*[cd]", "ac", TRUE, FALSE, TRUE },
  { "a*[cd]", "axc", TRUE, FALSE, TRUE },
  { "a*[cd]", "axx", TRUE, FALSE, FALSE },

  { "a/[.]", "a/.", TRUE, FALSE, FALSE },
  { "a*[.]", "a/.", TRUE, FALSE, FALSE },


  /* Test of UTF-8 */

  { "ä", "ä", TRUE, FALSE, TRUE },
  { "?", "ä", TRUE, FALSE, TRUE },
  { "*ö", "äö", TRUE, FALSE, TRUE },
  { "*ö", "ääö", TRUE, FALSE, TRUE },
  { "[ä]", "ä", TRUE, FALSE, TRUE },
  { "[ä-ö]", "é", TRUE, FALSE, TRUE },
  { "[ä-ö]", "a", TRUE, FALSE, FALSE },

  /* ci patterns */
  { "*.txt", "a.TXT", TRUE, TRUE, TRUE },
  { "*.txt", "a.TxT", TRUE, TRUE, TRUE },
  { "*.txt", "a.txT", TRUE, TRUE, TRUE },
  { "*ö", "äÖ", TRUE, TRUE, TRUE },

#ifdef DO_ESCAPE
  /* Tests of escaping */
  { "\\\\", "\\", TRUE, FALSE, TRUE },
  { "\\?", "?", TRUE, FALSE, TRUE },
  { "\\?", "a", TRUE, FALSE, FALSE },
  { "\\*", "*", TRUE, FALSE, TRUE },
  { "\\*", "a", TRUE, FALSE, FALSE },
  { "\\[a-b]", "[a-b]", TRUE, FALSE, TRUE },
  { "[\\\\]", "\\", TRUE, FALSE, TRUE },
  { "[\\^a]", "a", TRUE, FALSE, TRUE },
  { "[a\\-c]", "b", TRUE, FALSE, FALSE },
  { "[a\\-c]", "-", TRUE, FALSE, TRUE },
  { "[a\\]", "a", TRUE, FALSE, FALSE },
#endif /* DO_ESCAPE */
};

static void
test_fnmatch (gconstpointer data)
{
  const TestCase *test = data;

  g_assert_true (_gtk_fnmatch (test->pat, test->str, test->no_leading_period, test->ci) == test->result);
}

typedef struct {
  const char *glob;
  const char *ci;
} CITest;

static CITest citests[] = {
  { "*.txt", "*.[tT][xX][tT]" },
  { "*.TXT", "*.[tT][xX][tT]" },
  { "*?[]-abc]t", "*?[]-abc][tT]" },
#ifdef DO_ESCAPE
  /* Tests of escaping */
  { "\\\\", "\\\\" },
  { "\\??", "\\??" },
  { "\\**", "\\**" },
  { "\\[", "\\[" },
  { "\\[a-", "\\[[aA]-" },
  { "\\[]", "\\[]" },
#endif
};

static void
test_ci_glob (gconstpointer data)
{
  const CITest *test = data;
  char *ci;

  ci = _gtk_make_ci_glob_pattern (test->glob);
  g_assert_cmpstr (ci, ==, test->ci);
  g_free (ci);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  for (int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      char *path = g_strdup_printf ("/fnmatch/test%d", i);
      g_test_add_data_func (path, &tests[i], test_fnmatch);
      g_free (path);
    }

  for (int i = 0; i < G_N_ELEMENTS (citests); i++)
    {
      char *path = g_strdup_printf ("/ci-glob/test%d", i);
      g_test_add_data_func (path, &citests[i], test_ci_glob);
      g_free (path);
    }

  return g_test_run ();
}
