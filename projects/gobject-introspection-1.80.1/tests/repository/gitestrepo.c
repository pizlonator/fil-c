
#include "girepository.h"

#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>

void test_constructor_return_type(GIBaseInfo* object_info);

void
test_constructor_return_type(GIBaseInfo* object_info)
{
  GIFunctionInfo* constructor;
  GITypeInfo* return_type;
  GIBaseInfo *return_info;
  const gchar* class_name;
  const gchar* return_name;

  class_name = g_registered_type_info_get_type_name ((GIRegisteredTypeInfo*) object_info);
  g_assert (class_name);

  constructor = g_object_info_find_method((GIObjectInfo*)object_info, "new");
  g_assert (constructor);

  return_type = g_callable_info_get_return_type ((GICallableInfo*)constructor);
  g_assert (return_type);
  g_assert (g_type_info_get_tag (return_type) == GI_TYPE_TAG_INTERFACE);

  return_info = g_type_info_get_interface (return_type);
  g_assert (return_info);

  return_name = g_registered_type_info_get_type_name ((GIRegisteredTypeInfo*) return_info);
  g_assert (strcmp (class_name, return_name) == 0);
  g_base_info_unref ((GIBaseInfo*)return_info);
}

static void
test_type_info_get_name (GIRepository *repo)
{
  GIBaseInfo *base;
  GIVFuncInfo *vfunc;
  GITypeInfo *typeinfo;

  base = g_irepository_find_by_name (repo, "Gio", "File");
  g_assert_nonnull (base);
  g_assert_true (GI_IS_INTERFACE_INFO (base));
  vfunc = g_interface_info_find_vfunc ((GIInterfaceInfo*)base, "read_async");
  g_assert_nonnull (vfunc);
  g_base_info_unref ((GIBaseInfo*)base);

  typeinfo = g_callable_info_get_return_type (vfunc);
  g_assert_nonnull (typeinfo);
  g_base_info_unref ((GIBaseInfo*)vfunc);

  /* https://gitlab.gnome.org/GNOME/gobject-introspection/issues/96 */
  g_assert_null (g_base_info_get_name (typeinfo));
  g_base_info_unref ((GIBaseInfo*)typeinfo);
}

static void
test_get_gtype_interfaces (GIRepository *repo)
{
  GIInterfaceInfo **interfaces;
  guint n_interfaces, ix;
  gboolean found_initable = FALSE, found_async_initable = FALSE;

  g_irepository_get_object_gtype_interfaces (repo, G_TYPE_DBUS_CONNECTION, &n_interfaces, &interfaces);

  g_assert_cmpuint (n_interfaces, ==, 2);

  for (ix = 0; ix < n_interfaces; ix++)
    {
      const gchar *name = g_base_info_get_name(*(interfaces + ix));
      if (strcmp (name, "Initable") == 0)
        found_initable = TRUE;
      else if (strcmp (name, "AsyncInitable") == 0)
        found_async_initable = TRUE;
    }

  g_assert_true (found_initable);
  g_assert_true (found_async_initable);
}

int
main(int argc, char **argv)
{
  GIRepository *repo;
  GITypelib *ret;
  GError *error = NULL;
  GIBaseInfo *info;
  GIBaseInfo *siginfo;
  GIEnumInfo *errorinfo;
  GType gtype;
  const char *prefix;

  repo = g_irepository_get_default ();

  ret = g_irepository_require (repo, "Gio", NULL, 0, &error);
  if (!ret)
    g_error ("%s", error->message);

  prefix = g_irepository_get_c_prefix (repo, "Gio");
  g_assert (prefix != NULL);
  g_assert_cmpstr (prefix, ==, "G");

  info = g_irepository_find_by_name (repo, "Gio", "Cancellable");
  g_assert (info != NULL);
  g_assert (g_base_info_get_type (info) == GI_INFO_TYPE_OBJECT);

  gtype = g_registered_type_info_get_g_type ((GIRegisteredTypeInfo *)info);
  g_assert (g_type_is_a (gtype, G_TYPE_OBJECT));

  info = g_irepository_find_by_gtype (repo, g_type_from_name ("GCancellable"));
  g_assert (info != NULL);

  g_print ("Successfully found GCancellable\n");

  test_constructor_return_type (info);

  info = g_irepository_find_by_name (repo, "Gio", "ThisDoesNotExist");
  g_assert (info == NULL);

  info = g_irepository_find_by_name (repo, "Gio", "FileMonitor");
  g_assert (info != NULL);
  siginfo = g_object_info_find_signal ((GIObjectInfo*) info, "changed");
  g_assert (siginfo != NULL);
  g_base_info_unref (siginfo);

  /* Test notify on gobject */

  info = g_irepository_find_by_name (repo, "GObject", "Object");
  g_assert (info != NULL);
  siginfo = g_object_info_find_signal (info, "notify");
  g_assert (siginfo != NULL);
  g_base_info_unref (siginfo);
  g_base_info_unref (info);

  /* vfunc tests */
  {
    GIVFuncInfo *vfunc;
    GIFunctionInfo *invoker;

    /* Test vfunc with invoker on interface */
    info = g_irepository_find_by_name (repo, "Gio", "File");
    g_assert (info != NULL);

    vfunc = g_interface_info_find_vfunc ((GIInterfaceInfo*)info, "read_async");
    g_assert (vfunc != NULL);

    invoker = g_vfunc_info_get_invoker (vfunc);
    g_assert (invoker != NULL);

    g_assert (strcmp (g_base_info_get_name ((GIBaseInfo*)invoker), "read_async") == 0);

    /* Test vfunc with no known invoker on object */
    info = g_irepository_find_by_name (repo, "GObject", "Object");
    g_assert (info != NULL);

    vfunc = g_object_info_find_vfunc ((GIObjectInfo*)info, "dispose");
    g_assert (vfunc != NULL);

    /* Ok, maybe we should mark g_object_run_dispose as the invoker for
     * dispose, but...eh.  It's pretty special however you cut it.
     */
    invoker = g_vfunc_info_get_invoker (vfunc);
    g_assert (invoker == NULL);

    /* Test vfunc with invoker on object */
    info = g_irepository_find_by_name (repo, "Gio", "AppLaunchContext");
    g_assert (info != NULL);

    vfunc = g_object_info_find_vfunc ((GIObjectInfo*)info, "get_display");
    g_assert (vfunc != NULL);

    invoker = g_vfunc_info_get_invoker (vfunc);
    g_assert (invoker != NULL);
    g_assert (strcmp (g_base_info_get_name ((GIBaseInfo*)invoker), "get_display") == 0);

    /* And let's be sure we can find the method directly */

    invoker = g_object_info_find_method ((GIObjectInfo*)info, "get_display");
    g_assert (invoker != NULL);
    g_assert (strcmp (g_base_info_get_name ((GIBaseInfo*)invoker), "get_display") == 0);
  }

  test_type_info_get_name (repo);
  test_get_gtype_interfaces (repo);

  /* Error quark tests */
  errorinfo = g_irepository_find_by_error_domain (repo, G_RESOLVER_ERROR);
  g_assert (errorinfo != NULL);
  g_assert (g_base_info_get_type ((GIBaseInfo *)errorinfo) == GI_INFO_TYPE_ENUM);
  g_assert (strcmp (g_base_info_get_name ((GIBaseInfo*)errorinfo), "ResolverError") == 0);

  exit(0);
}
