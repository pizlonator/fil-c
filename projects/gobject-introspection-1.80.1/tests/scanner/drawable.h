#ifndef __REGRESS_TEST_INHERIT_DRAWABLE_H__
#define __REGRESS_TEST_INHERIT_DRAWABLE_H__

#include <glib-object.h>

#include "gitestmacros.h"

typedef struct _RegressTestInheritDrawable RegressTestInheritDrawable;
typedef struct _RegressTestInheritDrawableClass RegressTestInheritDrawableClass;

struct _RegressTestInheritDrawable
{
  GObject parent_instance;
};

struct _RegressTestInheritDrawableClass
{
  GObjectClass parent_class;
};

_GI_TEST_EXTERN
GType regress_test_inherit_drawable_get_type (void) G_GNUC_CONST;


_GI_TEST_EXTERN
void regress_test_inherit_drawable_do_foo (RegressTestInheritDrawable *drawable, int x);

_GI_TEST_EXTERN
void regress_test_inherit_drawable_get_origin (RegressTestInheritDrawable *drawable, int *x, int *y);

_GI_TEST_EXTERN
void regress_test_inherit_drawable_get_size (RegressTestInheritDrawable *drawable, guint *width, guint *height);

_GI_TEST_EXTERN
void regress_test_inherit_drawable_do_foo_maybe_throw (RegressTestInheritDrawable *drawable, int x, GError **error);

typedef struct _RegressTestInheritPixmapObjectClass RegressTestInheritPixmapObjectClass;

struct _RegressTestInheritPixmapObjectClass
{
  RegressTestInheritDrawableClass parent_class;
};

#endif /* __REGRESS_TEST_INHERIT_DRAWABLE_H__ */
