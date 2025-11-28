/* SPDX-License-Identifier: MIT */

#include <graphene.h>
#include <mutest.h>

static void
size_zero (mutest_spec_t *spec)
{
  const graphene_size_t *zero = graphene_size_zero ();
  graphene_size_t *s;

  mutest_expect ("graphene_size_zero() to not be null",
                 mutest_pointer ((void *) zero),
                 mutest_not, mutest_to_be_null,
                 NULL);

  mutest_expect ("graphene_size_zero().width is zero",
                 mutest_float_value (zero->width),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);
  mutest_expect ("graphene_size_zero().height is zero",
                 mutest_float_value (zero->width),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);

  mutest_expect ("graphene_size_zero() to return the same pointer",
                 mutest_bool_value (zero == graphene_size_zero ()),
                 mutest_to_be_true,
                 NULL);

  s = graphene_size_init_from_size (graphene_size_alloc (), graphene_size_zero ());
  mutest_expect ("graphene_size_zero() to return its own pointer",
                 mutest_bool_value (zero == graphene_size_zero ()),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("graphene_size_zero() to match a zero size",
                 mutest_bool_value (graphene_size_equal (s, zero)),
                 mutest_to_be_true,
                 NULL);

  graphene_size_free (s);
}

static void
size_equal (mutest_spec_t *spec)
{
  const graphene_size_t *zero = graphene_size_zero ();

  mutest_expect ("graphene_size_equal() works on the same pointer",
                 mutest_bool_value (graphene_size_equal (zero, zero)),
                 mutest_to_be_true,
                 NULL);

  mutest_expect ("graphene_size_equal() works when b is null",
                 mutest_bool_value (graphene_size_equal (zero, NULL)),
                 mutest_to_be_false,
                 NULL);

  mutest_expect ("graphene_size_equal() works when a is null",
                 mutest_bool_value (graphene_size_equal (NULL, zero)),
                 mutest_to_be_false,
                 NULL);
}

static void
size_init (mutest_spec_t *spec)
{
  graphene_size_t s;

  mutest_expect ("graphene_size_init() does not return null",
                 mutest_pointer (graphene_size_init (&s, 10.f, 20.f)),
                 mutest_not, mutest_to_be_null,
                 NULL);

  mutest_expect ("graphene_size_init() sets the width",
                 mutest_float_value (s.width),
                 mutest_to_be_close_to, 10.0, 0.0001,
                 NULL);

  mutest_expect ("graphene_size_init() sets the height",
                 mutest_float_value (s.height),
                 mutest_to_be_close_to, 20.0, 0.0001,
                 NULL);
}

static void
size_scale (mutest_spec_t *spec)
{
  graphene_size_t s, r;

  graphene_size_init (&s, 10.f, 10.f);
  graphene_size_scale (&s, .5f, &r);

  mutest_expect ("graphene_size_scale() changes the width",
                 mutest_float_value (r.width),
                 mutest_to_be_in_range, s.width * 0.5, s.width,
                 mutest_to_be_close_to, (s.width * 0.5), 0.0001,
                 NULL);
  mutest_expect ("graphene_size_scale() changes the height",
                 mutest_float_value (r.height),
                 mutest_to_be_in_range, s.height * 0.5, s.height,
                 mutest_to_be_close_to, (s.height * 0.5), 0.0001,
                 NULL);
}

static void
size_interpolate (mutest_spec_t *spec)
{
  graphene_size_t a, b, half, res;

  graphene_size_init (&a, 10.f, 10.f);
  graphene_size_init (&b, 20.f, 20.f);
  graphene_size_init (&half, 15.f, 15.f);

  graphene_size_interpolate (&a, &b, 0.0, &res);
  mutest_expect ("lerp(a, b, 0.0) = a",
                 mutest_bool_value (graphene_size_equal (&res, &a)),
                 mutest_to_be_true,
                 NULL);

  graphene_size_interpolate (&a, &b, 0.5, &res);
  mutest_expect ("lerp(a, b, 0.5) = half",
                 mutest_bool_value (graphene_size_equal (&res, &half)),
                 mutest_to_be_true,
                 NULL);

  graphene_size_interpolate (&a, &b, 1.0, &res);
  mutest_expect ("lerp(a, b, 1.0) = b",
                 mutest_bool_value (graphene_size_equal (&res, &b)),
                 mutest_to_be_true,
                 NULL);
}

static void
size_check (mutest_suite_t *suite)
{
  mutest_it ("initializes components", size_init);
  mutest_it ("has a valid zero size constant", size_zero);
  mutest_it ("checks for null pointers in equal()", size_equal);
  mutest_it ("scales the size components", size_scale);
  mutest_it ("interpolates size components", size_interpolate);
}

MUTEST_MAIN (
  mutest_describe ("graphene_size_t", size_check);
)
