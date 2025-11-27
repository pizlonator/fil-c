#include "config.h"

#include "gettype.h"

G_DEFINE_TYPE (GetTypeObject, gettype_object, G_TYPE_OBJECT);

static void
gettype_object_class_init (GetTypeObjectClass * klass)
{
}

static void
gettype_object_init (GetTypeObject * object)
{
}

GetTypeObject *
gettype_object_new (void)
{
  return g_object_new (GETTYPE_TYPE_OBJECT, NULL);
}

/**
 * gettype_object_nonmeta1_get_type:
 * @obj: self
 *
 * This shouldn't be scanned as a *_get_type function because it takes
 * arguments.
 *
 * Returns: 0
 */
GType
gettype_object_nonmeta1_get_type (GetTypeObject * obj)
{
  return 0;
}

/**
 * gettype_object_nonmeta2_get_type:
 *
 * This shouldn't be scanned as a *_get_type function because it doesn't return
 * a GType. It will generate a warning.
 *
 * Returns: true
 */
gboolean
gettype_object_nonmeta2_get_type (void)
{
  return TRUE;
}

/**
 * gettype_object_nonmeta_get_gtype:
 *
 * This shouldn't be scanned as a *_get_type function because it doesn't return
 * a GType. It will generate a warning.
 *
 * Returns: true
 */
gboolean
gettype_object_nonmeta_get_gtype (void)
{
  return TRUE;
}
