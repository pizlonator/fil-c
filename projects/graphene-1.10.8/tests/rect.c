/* SPDX-License-Identifier: MIT */

#include <graphene.h>
#include <mutest.h>

static void
rect_init (mutest_spec_t *spec)
{
  graphene_rect_t zero;
  graphene_rect_t *r;
  graphene_rect_t r2;

  r = graphene_rect_init (graphene_rect_alloc (), 0.f, 0.f, 10.f, 10.f);
  mutest_expect ("init to initialize the origin",
                 mutest_bool_value (graphene_point_equal (&r->origin, &GRAPHENE_POINT_INIT (0.f, 0.f))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("init to initialize the size",
                 mutest_bool_value (graphene_size_equal (&r->size, &GRAPHENE_SIZE_INIT (10.f, 10.f))),
                 mutest_to_be_true,
                 NULL);

  graphene_rect_init_from_rect (&r2, r);
  mutest_expect ("initializing from a rectangle creates an equivalent rectangle",
                 mutest_bool_value (graphene_rect_equal (&r2, r)),
                 mutest_to_be_true,
                 NULL);

  zero = GRAPHENE_RECT_INIT_ZERO;
  mutest_expect ("GRAPHENE_RECT_INIT_ZERO initializes a degenerate rectangle",
                 mutest_bool_value (graphene_rect_equal (&zero, graphene_rect_zero ())),
                 mutest_to_be_true,
                 NULL);

  graphene_rect_free (r);
}

static void
rect_normalize (mutest_spec_t *spec)
{
  graphene_rect_t r = GRAPHENE_RECT_INIT (10.f, 10.f, -10.f, -10.f);
  graphene_rect_t s = GRAPHENE_RECT_INIT ( 0.f,  0.f,  10.f,  10.f);
  graphene_rect_t t;

  graphene_rect_normalize_r (&r, &t);

  mutest_expect ("normalizing(10, 10, -10, -10) will put the origin in (0, 0)",
                 mutest_bool_value (graphene_point_equal (&t.origin, &GRAPHENE_POINT_INIT (0, 0))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("normalizing(10, 10, -10, -10) will make the width positive",
                 mutest_float_value (t.size.width),
                 mutest_to_be_greater_than, 0.0,
                 mutest_to_be_close_to, 10.0, 0.0001,
                 NULL);
  mutest_expect ("normalizing(10, 10, -10, -10) will make the height positive",
                 mutest_float_value (t.size.height),
                 mutest_to_be_greater_than, 0.0,
                 mutest_to_be_close_to, 10.0, 0.0001,
                 NULL);
  mutest_expect ("equality to operate on normalized rectangles",
                 mutest_bool_value (graphene_rect_equal (&r, &s)),
                 mutest_to_be_true,
                 NULL);
}

static void
rect_equal (mutest_spec_t *spec)
{
  graphene_rect_t *r, *s;

  r = graphene_rect_init (graphene_rect_alloc (), 0.f, 0.f, 10.f, 10.f);
  s = graphene_rect_init (graphene_rect_alloc (), 1.f, 1.f,  9.f,  9.f);

  mutest_expect ("a rectangle to be equal it itself",
                 mutest_bool_value (graphene_rect_equal (r, r)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("a rectangle to not be equal to null",
                 mutest_bool_value (graphene_rect_equal (r, NULL)),
                 mutest_to_be_false,
                 NULL);
  mutest_expect ("null to not be equal to a rectangle",
                 mutest_bool_value (graphene_rect_equal (NULL, r)),
                 mutest_to_be_false,
                 NULL);
  mutest_expect ("two rectangles with different values to not be equal",
                 mutest_bool_value (graphene_rect_equal (r, s)),
                 mutest_to_be_false,
                 NULL);

  graphene_rect_free (r);
  graphene_rect_free (s);
}

static void
rect_contains_point (mutest_spec_t *spec)
{
  graphene_rect_t r = GRAPHENE_RECT_INIT (0.f, 0.f, 10.f, 10.f);
  graphene_point_t p;

  graphene_rect_get_top_left (&r, &p);
  mutest_expect ("a rectangle to contain its top left corner",
                 mutest_bool_value (graphene_rect_contains_point (&r, &p)),
                 mutest_to_be_true,
                 NULL);

  graphene_rect_get_top_right (&r, &p);
  mutest_expect ("a rectangle to contain its top right corner",
                 mutest_bool_value (graphene_rect_contains_point (&r, &p)),
                 mutest_to_be_true,
                 NULL);

  graphene_rect_get_bottom_right (&r, &p);
  mutest_expect ("a rectangle to contain its bottom right corner",
                 mutest_bool_value (graphene_rect_contains_point (&r, &p)),
                 mutest_to_be_true,
                 NULL);

  graphene_rect_get_bottom_left (&r, &p);
  mutest_expect ("a rectangle to contain its bottom left corner",
                 mutest_bool_value (graphene_rect_contains_point (&r, &p)),
                 mutest_to_be_true,
                 NULL);

  graphene_rect_get_center (&r, &p);
  mutest_expect ("a rectangle to contain its center",
                 mutest_bool_value (graphene_rect_contains_point (&r, &p)),
                 mutest_to_be_true,
                 NULL);

  graphene_point_init (&p, -1.f, 11.f);
  mutest_expect ("a rectangle to not contain a point outside its boundaries",
                 mutest_bool_value (graphene_rect_contains_point (&r, &p)),
                 mutest_to_be_false,
                 NULL);

  const struct {
    graphene_point_t point;
    const char *label;
  } points[] = {
    {
      .point = GRAPHENE_POINT_INIT (250, 150),
      .label = "a rectangle to not contain a point outside its boundaries (right)",
    },
    {
      .point = GRAPHENE_POINT_INIT (150, 50),
      .label = "a rectangle to not contain a point outside its boundaries (below)",
    },
    {
      .point = GRAPHENE_POINT_INIT (50, 150),
      .label = "a rectangle to not contain a point outside its boundaries (left)",
    },
    {
      .point = GRAPHENE_POINT_INIT (150, 250),
      .label = "a rectangle to not contain a point outside its boundaries (above)",
    },
  };
  const unsigned n_points = sizeof (points) / sizeof (points[0]);

  graphene_rect_init (&r, 100, 100, 100, 100);
  for (unsigned i = 0; i < n_points; i++)
    {
      mutest_expect (points[i].label,
                     mutest_bool_value (graphene_rect_contains_point (&r, &(points[i].point))),
                     mutest_to_be, false,
                     NULL);
    }
}

static void
rect_contains_rect (mutest_spec_t *spec)
{
  graphene_rect_t *r, *s;

  r = graphene_rect_init (graphene_rect_alloc (), 0.f, 0.f, 10.f, 10.f);
  s = graphene_rect_init (graphene_rect_alloc (), 1.f, 1.f,  9.f,  9.f);

  mutest_expect ("a rectangle to contain itself",
                 mutest_bool_value (graphene_rect_contains_rect (r, r)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("the smaller rectangle to not contain the larger one",
                 mutest_bool_value (graphene_rect_contains_rect (s, r)),
                 mutest_to_be_false,
                 NULL);
  mutest_expect ("the larger rectangle to contain the smaller one",
                 mutest_bool_value (graphene_rect_contains_rect (r, s)),
                 mutest_to_be_true,
                 NULL);

  graphene_rect_free (r);
  graphene_rect_free (s);
}

static void
rect_intersect (mutest_spec_t *spec)
{
  graphene_rect_t r = GRAPHENE_RECT_INIT (0.f, 0.f, 10.f, 10.f);
  graphene_rect_t s = GRAPHENE_RECT_INIT (5.f, 5.f, 15.f, 15.f);
  graphene_rect_t q = GRAPHENE_RECT_INIT (11.f, 11.f, 2.f, 2.f);
  graphene_rect_t i, j;

  mutest_expect ("intersection between (0, 0, 10, 10) and (5, 5, 15, 15) not be empty",
                 mutest_bool_value (graphene_rect_intersection (&r, &s, &i)),
                 mutest_not, mutest_to_be_false,
                 NULL);

  mutest_expect ("origin of intersection to be at (5, 5)",
                 mutest_bool_value (graphene_point_equal (&i.origin, &GRAPHENE_POINT_INIT (5, 5))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("size of intersection to be (5, 5)",
                 mutest_bool_value (graphene_size_equal (&i.size, &GRAPHENE_SIZE_INIT (5, 5))),
                 mutest_to_be_true,
                 NULL);

  mutest_expect ("intersection between (5, 5, 5, 5) and (11, 11, 2, 2) to be empty",
                 mutest_bool_value (graphene_rect_intersection (&i, &q, &j)),
                 mutest_not, mutest_to_be_true,
                 NULL);
  mutest_expect ("empty intersection to be a degenerate rectangle",
                 mutest_bool_value (graphene_rect_equal (&j, graphene_rect_zero ())),
                 mutest_to_be_true,
                 NULL);
}

static void
rect_union (mutest_spec_t *spec)
{
  graphene_rect_t r = GRAPHENE_RECT_INIT (0.f, 0.f, 10.f, 10.f);
  graphene_rect_t s = GRAPHENE_RECT_INIT (5.f, 5.f, 15.f, 15.f);
  graphene_rect_t u;

  graphene_rect_union (&r, &s, &u);
  mutest_expect ("union to take the minimum normalize origin",
                 mutest_bool_value (graphene_point_equal (&u.origin, &r.origin)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("union to take the maximum normalized size",
                 mutest_bool_value (graphene_size_equal (&u.size, &GRAPHENE_SIZE_INIT (20.f, 20.f))),
                 mutest_to_be_true,
                 NULL);

  mutest_expect ("union rectangle to contain original rectangle A",
                 mutest_bool_value (graphene_rect_contains_rect (&u, &r)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("union rectangle to contain original rectangle B",
                 mutest_bool_value (graphene_rect_contains_rect (&u, &s)),
                 mutest_to_be_true,
                 NULL);
}

static void
rect_offset (mutest_spec_t *spec)
{
  graphene_rect_t r = GRAPHENE_RECT_INIT (0.f, 0.f, 10.f, 10.f);

  graphene_rect_offset (&r, 5.f, 5.f);
  mutest_expect ("offset to move the origin",
                 mutest_bool_value (graphene_point_equal (&r.origin, &GRAPHENE_POINT_INIT (5.f, 5.f))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("offset to keep the size",
                 mutest_bool_value (graphene_size_equal (&r.size, &GRAPHENE_SIZE_INIT (10.f, 10.f))),
                 mutest_to_be_true,
                 NULL);
}

static void
rect_inset (mutest_spec_t *spec)
{
  graphene_rect_t r = GRAPHENE_RECT_INIT (0.f, 0.f, 10.f, 10.f);

  graphene_rect_inset (&r, 2.f, 2.f);
  mutest_expect ("inset(2, 2) to move the origin",
                 mutest_bool_value (graphene_point_equal (&r.origin, &GRAPHENE_POINT_INIT (2.f, 2.f))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("inset(2, 2) to shrink the size",
                 mutest_bool_value (graphene_size_equal (&r.size, &GRAPHENE_SIZE_INIT (6.f, 6.f))),
                 mutest_to_be_true,
                 NULL);

  graphene_rect_inset (&r, 2.f, -2.f);
  mutest_expect ("inset(2, -2) to move back the origin",
                 mutest_float_value (r.origin.y),
                 mutest_to_be_close_to, 0.0, 0.0001,
                 NULL);
  mutest_expect ("inset(2, -2) to shrink the width",
                 mutest_float_value (r.size.width),
                 mutest_to_be_close_to, 2.0, 0.0001,
                 NULL);
  mutest_expect ("inset(2, -2) to expand the height",
                 mutest_float_value (r.size.height),
                 mutest_to_be_close_to, 10.0, 0.0001,
                 NULL);
}

static void
rect_area (void)
{
  graphene_rect_t r = GRAPHENE_RECT_INIT (0.5f, 1.9f,  9.3f, 8.7f);
  graphene_rect_t n;

  graphene_rect_normalize_r (&r, &n);

  mutest_expect ("area to be width × height",
                 mutest_float_value (graphene_rect_get_area (&r)),
                 mutest_to_be_close_to, (n.size.width * n.size.height), 0.1,
                 NULL);
}

static void
rect_round (mutest_spec_t *spec)
{
  graphene_rect_t r = GRAPHENE_RECT_INIT (0.5f, 1.9f,  9.3f, 8.7f);
  graphene_rect_t s = GRAPHENE_RECT_INIT (0.0f, 1.0f, 10.0f, 9.0f);
  graphene_rect_t tmp;

  graphene_rect_round (&r, &tmp);
  mutest_expect ("round() area is larger than or equal to the original rectangle's area",
                 mutest_float_value (graphene_rect_get_area (&tmp)),
                 mutest_to_be_greater_than_or_equal, graphene_rect_get_area (&r),
                 NULL);
  mutest_expect ("round() may not contain the original rectangle",
                 mutest_bool_value (graphene_rect_contains_rect (&tmp, &r)),
                 mutest_not, mutest_to_be, true,
                 NULL);

  /* round_to_pixel() operates in place */
  graphene_rect_init_from_rect (&tmp, &r);
  graphene_rect_round_to_pixel (&tmp);
  mutest_expect ("round_to_pixel() may not contain the original rectangle",
                 mutest_bool_value (graphene_rect_contains_rect (&tmp, &r)),
                 mutest_not, mutest_to_be, true,
                 NULL);
  mutest_expect ("rounding is stable",
                 mutest_bool_value (graphene_rect_equal (&s, &tmp)),
                 mutest_to_be, true,
                 NULL);
}

static void
rect_round_extents (void)
{
  graphene_rect_t r = GRAPHENE_RECT_INIT (2.5f, 1.9f,  9.3f, 8.7f);
  graphene_rect_t s = GRAPHENE_RECT_INIT (2.0f, 1.0f, 10.0f, 10.0f);
  graphene_rect_t rounded;
  graphene_point_t p;

  graphene_rect_round_extents (&r, &rounded);

  graphene_rect_get_top_left (&r, &p);
  mutest_expect ("round_extents()'s result to contain top-left corner of the original rectangle",
                 mutest_bool_value (graphene_rect_contains_point (&rounded, &p)),
                 mutest_to_be, true,
                 NULL);

  graphene_rect_get_top_right (&r, &p);
  mutest_expect ("round_extents()'s result to contain top-right corner of the original rectangle",
                 mutest_bool_value (graphene_rect_contains_point (&rounded, &p)),
                 mutest_to_be, true,
                 NULL);

  graphene_rect_get_bottom_left (&r, &p);
  mutest_expect ("round_extents()'s result to contain bottom-left corner of the original rectangle",
                 mutest_bool_value (graphene_rect_contains_point (&rounded, &p)),
                 mutest_to_be, true,
                 NULL);

  graphene_rect_get_bottom_right (&r, &p);
  mutest_expect ("round_extents()'s result to contain bottom-right corner of the original rectangle",
                 mutest_bool_value (graphene_rect_contains_point (&rounded, &p)),
                 mutest_to_be, true,
                 NULL);

  mutest_expect ("round_extents()'s result to contain the original rectangle",
                 mutest_bool_value (graphene_rect_contains_rect (&rounded, &r)),
                 mutest_to_be, true,
                 NULL);

  mutest_expect ("round_extents()'s rounding to be stable",
                 mutest_bool_value (graphene_rect_equal (&rounded, &s)),
                 mutest_to_be, true,
                 NULL);
}

static void
rect_expand (mutest_spec_t *spec)
{
  graphene_rect_t r = GRAPHENE_RECT_INIT (0.f, 0.f, 100.f, 100.f);
  graphene_point_t p;
  graphene_size_t s;
  graphene_rect_t check;

  graphene_rect_expand (&r, graphene_point_init (&p, -10.f, -10.f), &check);
  graphene_size_init (&s, 110.f, 110.f);
  mutest_expect ("expanding behind the origin changes the origin",
                 mutest_bool_value (graphene_point_equal (&p, &(check.origin))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("expanding behind the origin does not change the size",
                 mutest_bool_value (graphene_size_equal (&s, &(check.size))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("expanded rectangle contains original rectangle",
                 mutest_bool_value (graphene_rect_contains_rect (&check, &r)),
                 mutest_to_be_true,
                 NULL);

  graphene_rect_expand (&r, graphene_point_init (&p, 150.f, 150.f), &check);
  graphene_size_init (&s, 150.f, 150.f);
  mutest_expect ("expanding after the anti-origin does not change the origin",
                 mutest_bool_value (graphene_point_equal (&r.origin, &check.origin)),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("expanding after the anti-origin changes the size",
                 mutest_bool_value (graphene_size_equal (&s, &(check.size))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("expanded rectangle contains original rectangle",
                 mutest_bool_value (graphene_rect_contains_rect (&check, &r)),
                 mutest_to_be_true,
                 NULL);

  graphene_rect_expand (&r, graphene_point_init (&p, 50.f, 150.f), &check);
  mutest_expect ("expanding half in/half out does not change the origin",
                 mutest_bool_value (graphene_point_equal (&r.origin, &(check.origin))),
                 mutest_to_be_true,
                 NULL);
  mutest_expect ("expanding horizontally inside the rectangle to not change width",
                 mutest_float_value (check.size.width),
                 mutest_to_be_close_to, r.size.width, 0.0001,
                 NULL);
  mutest_expect ("expanding vertically outside the rectangle to change the height",
                 mutest_float_value (check.size.height),
                 mutest_to_be_close_to, p.y, 0.0001,
                 NULL);
  mutest_expect ("expanded rectangle contains original rectangle",
                 mutest_bool_value (graphene_rect_contains_rect (&check, &r)),
                 mutest_to_be_true,
                 NULL);
}

static void
rect_interpolate (mutest_spec_t *spec)
{
  graphene_rect_t a = GRAPHENE_RECT_INIT ( 0.f, 0.f, 10.f, 10.f);
  graphene_rect_t b = GRAPHENE_RECT_INIT (10.f, 5.f, 30.f, 20.f);
  graphene_rect_t c;
  graphene_rect_t res;

  graphene_rect_interpolate (&a, &b, 0.0, &res);
  mutest_expect ("lerp(0) to produce the initial rectangle",
                 mutest_bool_value (graphene_rect_equal (&a, &res)),
                 mutest_to_be_true,
                 NULL);

  graphene_rect_interpolate (&a, &b, 1.0, &res);
  mutest_expect ("lerp(1) to produce the final rectangle",
                 mutest_bool_value (graphene_rect_equal (&b, &res)),
                 mutest_to_be_true,
                 NULL);

  c.origin.x = a.origin.x + (b.origin.x - a.origin.x) * 0.25f;
  c.origin.y = a.origin.y + (b.origin.y - a.origin.y) * 0.25f;
  c.size.width = a.size.width + (b.size.width - a.size.width) * 0.25f;
  c.size.height = a.size.height + (b.size.height - a.size.height) * 0.25f;
  graphene_rect_interpolate (&a, &b, 0.25, &res);
  mutest_expect ("lerp(0.25) to produce the expect rectangle",
                 mutest_bool_value (graphene_rect_equal (&c, &res)),
                 mutest_to_be_true,
                 NULL);
}

static void
rect_scale (void)
{
  struct {
    graphene_rect_t unscaled;
    graphene_rect_t scaled;
    float scales[2];
    const char *desc;
  } rects[] = {
    {
      GRAPHENE_RECT_INIT (0.f, 0.f, 10.f, 10.f),
      GRAPHENE_RECT_INIT (0.f, 0.f, 10.f, 10.f),
      { 1.f, 1.f },
      "Scaling by 1 does not change the rectangle",
    },
    {
      GRAPHENE_RECT_INIT (-2.f, -2.f, -2.f, -2.f),
      GRAPHENE_RECT_INIT (-8.f, -20.f, 4.f, 10.f),
      { 2.f, 5.f },
      "Scaling by (2, 5) preserves the origin sign but changes the size sign",
    },
    {
      GRAPHENE_RECT_INIT (-4.f, 2.f, 10.f,  10.f),
      GRAPHENE_RECT_INIT (-1.f, 1.f,  2.5f,  5.f),
      { 0.25f, 0.5f },
      "Scaling by a fraction moves the origin and reduces the size",
    },
    {
      GRAPHENE_RECT_INIT (1.f, 0.f, 10.f, -1.f),
      GRAPHENE_RECT_INIT (0.f, 0.f,  0.f, 10.f),
      { 0.f, -10.f, },
      "Scaling by (0, -10) moves the origin in (0, 0) and flips the negative sign of the size",
    },
  };
  unsigned int n_rects = sizeof (rects) / sizeof (rects[0]);

  for (unsigned int i = 0; i < n_rects; i++)
    {
      graphene_rect_t res;

      graphene_rect_scale (&rects[i].unscaled,
                           rects[i].scales[0],
                           rects[i].scales[1],
                           &res);

      mutest_expect (rects[i].desc,
                     mutest_bool_value (graphene_rect_equal (&res, &rects[i].scaled)),
                     mutest_to_be_true,
                     NULL);
    }
}

static void
rect_suite (mutest_suite_t *suite)
{
  mutest_it ("initializes origin and size", rect_init);
  mutest_it ("normalizes origin and size", rect_normalize);
  mutest_it ("can check for equality", rect_equal);
  mutest_it ("can check for contained points", rect_contains_point);
  mutest_it ("can check for contained rectangles", rect_contains_rect);
  mutest_it ("can compute intersections", rect_intersect);
  mutest_it ("can compute unions", rect_union);
  mutest_it ("can compute the area", rect_area);
  mutest_it ("can offset origin and size", rect_offset);
  mutest_it ("can inset origin and size", rect_inset);
  mutest_it ("can round to the nearest integer", rect_round);
  mutest_it ("can round the extents of a rectangle to the nearest integer", rect_round_extents);
  mutest_it ("can expand", rect_expand);
  mutest_it ("can interpolate", rect_interpolate);
  mutest_it ("can scale", rect_scale);
}

MUTEST_MAIN (
  mutest_describe ("graphene_rect_t", rect_suite);
)
