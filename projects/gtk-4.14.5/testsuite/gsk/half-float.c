#include <gtk/gtk.h>

#include "gsk/gl/fp16private.h"

static void
test_constants (void)
{
  struct {
    float f;
    guint16 h;
  } tests[] = {
    { 0.0, FP16_ZERO },
    { 1.0, FP16_ONE },
    { -1.0, FP16_MINUS_ONE },
  };

  for (int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      float f[4];
      guint16 h[4];

      memset (f, 0, sizeof (f));
      f[0] = tests[i].f;
      float_to_half4 (f, h);
      g_assert_cmpuint (h[0], ==, tests[i].h);


      memset (h, 0, sizeof (h));
      h[0] = tests[i].h;
      half_to_float4 (h, f);
      g_assert_cmpfloat (f[0], ==, tests[i].f);
    }
}

static void
test_constants_c (void)
{
  struct {
    float f;
    guint16 h;
  } tests[] = {
    { 0.0, FP16_ZERO },
    { 1.0, FP16_ONE },
    { -1.0, FP16_MINUS_ONE },
  };

  for (int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      float f[4];
      guint16 h[4];

      memset (f, 0, sizeof (f));
      f[0] = tests[i].f;
      float_to_half4_c (f, h);
      g_assert_cmpuint (h[0], ==, tests[i].h);


      memset (h, 0, sizeof (h));
      h[0] = tests[i].h;
      half_to_float4_c (h, f);
      g_assert_cmpfloat (f[0], ==, tests[i].f);
    }
}

static float
random_representable_float (void)
{
  guint16 h[4];
  float f[4];
  do
    {
      /* generate a random float thats representable as fp16 */
      memset (h, 0, sizeof (h));
      h[0] = g_test_rand_int_range (G_MININT16, G_MAXINT16);
      half_to_float4 (h, f);
    }
  while (!isnormal (f[0])); /* skip nans and infs since they don't compare well */

  return f[0];
}

static void
test_roundtrip (void)
{
  for (int i = 0; i < 100; i++)
    {
      float f[4];
      float f2[4];
      guint16 h[4];

      f2[0] = random_representable_float ();
      memset (f, 0, sizeof (f));
      f[0] = f2[0];

      float_to_half4 (f, h);
      half_to_float4 (h, f2);

      g_assert_cmpfloat (f[0], ==, f2[0]);
    }
}

static void
test_roundtrip_c (void)
{
  for (int i = 0; i < 100; i++)
    {
      float f[4];
      float f2[4];
      guint16 h[4];

      f2[0] = random_representable_float ();
      memset (f, 0, sizeof (f));
      f[0] = f2[0];

      float_to_half4_c (f, h);
      half_to_float4_c (h, f2);

      g_assert_cmpfloat (f[0], ==, f2[0]);
    }
}

/* Test that the array version work as expected,
 * in particular with unaligned boundaries.
 */
static void
test_many (void)
{
  for (int i = 0; i < 100; i++)
    {
      int size = g_test_rand_int_range (100, 200);
      int offset = g_test_rand_int_range (0, 20);

      guint16 *h = g_new0 (guint16, size);
      float *f = g_new0 (float, size);
      float *f2 = g_new0 (float, size);

      for (int j = offset; j < size; j++)
        f[j] = random_representable_float ();

      float_to_half (f + offset, h + offset, size - offset);
      half_to_float (h + offset, f2 + offset, size - offset);

      for (int j = offset; j < size; j++)
        g_assert_cmpfloat (f[j], ==, f2[j]);
    }
}

static void
test_many_c (void)
{
  for (int i = 0; i < 100; i++)
    {
      int size = g_test_rand_int_range (100, 200);
      int offset = g_test_rand_int_range (0, 20);

      guint16 *h = g_new0 (guint16, size);
      float *f = g_new0 (float, size);
      float *f2 = g_new0 (float, size);

      for (int j = offset; j < size; j++)
        f[j] = random_representable_float ();

      float_to_half_c (f + offset, h + offset, size - offset);
      half_to_float_c (h + offset, f2 + offset, size - offset);

      for (int j = offset; j < size; j++)
        g_assert_cmpfloat (f[j], ==, f2[j]);
    }
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/half-float/constants", test_constants);
  g_test_add_func ("/half-float/roundtrip", test_roundtrip);
  g_test_add_func ("/half-float/many", test_many);
  g_test_add_func ("/half-float/constants/c", test_constants_c);
  g_test_add_func ("/half-float/roundtrip/c", test_roundtrip_c);
  g_test_add_func ("/half-float/many/c", test_many_c);

  return g_test_run ();
}
