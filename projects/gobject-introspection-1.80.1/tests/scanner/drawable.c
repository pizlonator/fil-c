/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* This file gets installed, so we can't assume config.h is available */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "drawable.h"

G_DEFINE_ABSTRACT_TYPE (RegressTestInheritDrawable, regress_test_inherit_drawable, G_TYPE_OBJECT);

static void
regress_test_inherit_drawable_class_init (RegressTestInheritDrawableClass *klass)
{

}

static void
regress_test_inherit_drawable_init (RegressTestInheritDrawable *drawable)
{

}

void
regress_test_inherit_drawable_do_foo (RegressTestInheritDrawable *drawable, int x)
{

}

/**
 * regress_test_inherit_drawable_get_origin:
 * @drawable:
 * @x: (out):
 * @y: (out):
 */
void
regress_test_inherit_drawable_get_origin (RegressTestInheritDrawable *drawable, int *x, int *y)
{
  *x = 0;
  *y = 0;
}

/**
 * regress_test_inherit_drawable_get_size:
 * @drawable:
 * @width: (out):
 * @height: (out):
 */
void
regress_test_inherit_drawable_get_size (RegressTestInheritDrawable *drawable, guint *width, guint *height)
{
  *width = 42;
  *height = 42;
}

void
regress_test_inherit_drawable_do_foo_maybe_throw (RegressTestInheritDrawable *drawable, int x, GError **error)
{
  if (x != 42)
    g_set_error(error, 0, 12, "The answer should be 42!");
}
