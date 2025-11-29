#include <locale.h>
#include <gtk/gtk.h>

static void
test_keysyms_basic (void)
{
  struct {
    guint keyval;
    const char *name;
    const char *other_name;
  } tests[] = {
    { GDK_KEY_space, "space", NULL },
    { GDK_KEY_a, "a", NULL },
    { GDK_KEY_Thorn, "Thorn", "THORN" },
    { GDK_KEY_Hangul_J_RieulTieut, "Hangul_J_RieulTieut", NULL },
    { GDK_KEY_Page_Up, "Page_Up", NULL },
    { GDK_KEY_KP_Multiply, "KP_Multiply", NULL },
    { GDK_KEY_MonBrightnessUp, "MonBrightnessUp", NULL },
    { 0, NULL }
  };
  int i;

 for (i = 0; tests[i].keyval != 0; i++)
   {
     g_assert_cmpstr (gdk_keyval_name (tests[i].keyval), ==, tests[i].name);
     g_assert_cmpuint (gdk_keyval_from_name (tests[i].name), ==, tests[i].keyval);
     if (tests[i].other_name)
       g_assert_cmpuint (gdk_keyval_from_name (tests[i].other_name), ==, tests[i].keyval);
   }
}

static void
test_keysyms_void (void)
{
  g_assert_cmpuint (gdk_keyval_from_name ("NoSuchKeysym"), ==, GDK_KEY_VoidSymbol);
  g_assert_cmpstr (gdk_keyval_name (GDK_KEY_VoidSymbol), ==, "0xffffff");
}

static void
test_keysyms_xf86 (void)
{
  g_assert_cmpuint (gdk_keyval_from_name ("XF86MonBrightnessUp"), ==, GDK_KEY_MonBrightnessUp);
  g_assert_cmpuint (gdk_keyval_from_name ("XF86MonBrightnessDown"), ==, GDK_KEY_MonBrightnessDown);
  g_assert_cmpuint (gdk_keyval_from_name ("XF86KbdBrightnessUp"), ==, GDK_KEY_KbdBrightnessUp);
  g_assert_cmpuint (gdk_keyval_from_name ("XF86KbdBrightnessDown"), ==, GDK_KEY_KbdBrightnessDown);
  g_assert_cmpuint (gdk_keyval_from_name ("XF86Battery"), ==, GDK_KEY_Battery);
  g_assert_cmpuint (gdk_keyval_from_name ("XF86Display"), ==, GDK_KEY_Display);

  g_assert_cmpuint (gdk_keyval_from_name ("MonBrightnessUp"), ==, GDK_KEY_MonBrightnessUp);
  g_assert_cmpuint (gdk_keyval_from_name ("MonBrightnessDown"), ==, GDK_KEY_MonBrightnessDown);
  g_assert_cmpuint (gdk_keyval_from_name ("KbdBrightnessUp"), ==, GDK_KEY_KbdBrightnessUp);
  g_assert_cmpuint (gdk_keyval_from_name ("KbdBrightnessDown"), ==, GDK_KEY_KbdBrightnessDown);
  g_assert_cmpuint (gdk_keyval_from_name ("Battery"), ==, GDK_KEY_Battery);
  g_assert_cmpuint (gdk_keyval_from_name ("Display"), ==, GDK_KEY_Display);
}

#define UNICODE_KEYVAL(wc) ((wc) | 0x01000000)

static void
test_key_case (void)
{
  struct {
    guint lower;
    guint upper;
  } tests[] = {
    { GDK_KEY_a, GDK_KEY_A },
    { GDK_KEY_agrave, GDK_KEY_Agrave },
    { GDK_KEY_thorn, GDK_KEY_Thorn },
    { GDK_KEY_oslash, GDK_KEY_Oslash },
    { GDK_KEY_aogonek, GDK_KEY_Aogonek },
    { GDK_KEY_lstroke, GDK_KEY_Lstroke },
    { GDK_KEY_scaron, GDK_KEY_Scaron },
    { GDK_KEY_zcaron, GDK_KEY_Zcaron },
    { GDK_KEY_racute, GDK_KEY_Racute },
    { GDK_KEY_hstroke, GDK_KEY_Hstroke },
    { GDK_KEY_jcircumflex, GDK_KEY_Jcircumflex },
    { GDK_KEY_cabovedot, GDK_KEY_Cabovedot },
    { GDK_KEY_rcedilla, GDK_KEY_Rcedilla },
    { GDK_KEY_eng, GDK_KEY_ENG },
    { GDK_KEY_amacron, GDK_KEY_Amacron },
    { GDK_KEY_Serbian_dje, GDK_KEY_Serbian_DJE },
    { GDK_KEY_Cyrillic_yu, GDK_KEY_Cyrillic_YU },
    { GDK_KEY_Greek_alphaaccent, GDK_KEY_Greek_ALPHAaccent },
    { GDK_KEY_Greek_omega, GDK_KEY_Greek_OMEGA },
    { GDK_KEY_Greek_sigma, GDK_KEY_Greek_SIGMA },

    { GDK_KEY_space, GDK_KEY_space },
    { GDK_KEY_0, GDK_KEY_0 },
    { GDK_KEY_KP_0, GDK_KEY_KP_0 },

    /* Face Savouring Delicious Food */
    { UNICODE_KEYVAL (0x1f60b), UNICODE_KEYVAL (0x1f60b) },
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      g_assert_true (gdk_keyval_is_lower (tests[i].lower));
      g_assert_true (gdk_keyval_is_upper (tests[i].upper));
      g_assert_cmpuint (gdk_keyval_to_upper (tests[i].lower), ==, tests[i].upper);
      g_assert_cmpuint (gdk_keyval_to_lower (tests[i].upper), ==, tests[i].lower);
    }
}

static void
test_key_unicode (void)
{
  struct {
    guint key;
    gunichar ch;
  } tests[] = {
    { GDK_KEY_a, 'a' },
    { GDK_KEY_A, 'A' },
    { GDK_KEY_EuroSign, 0x20ac },
    { UNICODE_KEYVAL (0x1f60b), 0x1f60b },
  },
  oneway_tests[] = {
    { GDK_KEY_KP_0, '0' },
    { GDK_KEY_KP_Divide, '/' },
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      g_assert_cmpuint (gdk_keyval_to_unicode (tests[i].key), ==, tests[i].ch);
      g_assert_cmpuint (gdk_unicode_to_keyval (tests[i].ch), ==, tests[i].key);
    }

  for (i = 0; i < G_N_ELEMENTS (oneway_tests); i++)
    {
      g_assert_cmpuint (gdk_keyval_to_unicode (tests[i].key), ==, tests[i].ch);
    }
}

int
main (int argc, char *argv[])
{
  setlocale (LC_ALL, "");

  (g_test_init) (&argc, &argv, NULL);
  gtk_init ();

  g_test_add_func ("/keysyms/basic", test_keysyms_basic);
  g_test_add_func ("/keysyms/void", test_keysyms_void);
  g_test_add_func ("/keysyms/xf86", test_keysyms_xf86);
  g_test_add_func ("/keys/case", test_key_case);
  g_test_add_func ("/keys/unicode", test_key_unicode);

  return g_test_run ();
}
