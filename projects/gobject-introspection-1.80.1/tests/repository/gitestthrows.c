
#include "girepository.h"


static void
test_invoke_gerror (void)
{
  GIRepository *repo;
  GITypelib *ret;
  GIBaseInfo *info;
  GIArgument in_arg[1];
  GIArgument ret_arg;
  GError *error;
  gboolean invoke_return;

  repo = g_irepository_get_default ();

  error = NULL;
  ret = g_irepository_require (repo, "GLib", NULL, 0, &error);
  g_assert (ret != NULL);
  g_assert (error == NULL);

  info = g_irepository_find_by_name (repo, "GLib", "file_read_link");
  g_assert (info != NULL);
  g_assert (g_base_info_get_type (info) == GI_INFO_TYPE_FUNCTION);
  g_assert (g_function_info_get_flags ((GIFunctionInfo *)info) & GI_FUNCTION_THROWS);

  in_arg[0].v_string = g_strdup ("non-existent-file/hope");
  error = NULL;
  invoke_return = g_function_info_invoke ((GIFunctionInfo *)info,
                                          in_arg,
                                          1,
                                          NULL,
                                          0,
                                          &ret_arg,
                                          &error);
  g_free (in_arg[0].v_string);

  g_assert (invoke_return == FALSE);
  g_assert (error != NULL);
  g_assert (error->domain == G_FILE_ERROR);
  g_assert (error->code == G_FILE_ERROR_NOENT);
}

static void
test_vfunc_can_throw_gerror (void)
{
  GIRepository *repo;
  GITypelib *ret;
  GIBaseInfo *object_info;
  GIFunctionInfo *invoker_info;
  GIVFuncInfo *vfunc_info;
  GError *error = NULL;

  repo = g_irepository_get_default ();

  ret = g_irepository_require (repo, "GIMarshallingTests", NULL, 0, &error);
  g_assert_nonnull (ret);
  g_assert_no_error (error);

  object_info = g_irepository_find_by_name (repo, "GIMarshallingTests", "Object");
  g_assert_nonnull (object_info);
  g_assert (g_base_info_get_type (object_info) == GI_INFO_TYPE_OBJECT);

  invoker_info = g_object_info_find_method ((GIObjectInfo *)object_info,
                                            "vfunc_meth_with_error");
  g_assert_nonnull (invoker_info);
  g_assert (g_function_info_get_flags (invoker_info) & GI_FUNCTION_THROWS);
  g_assert (g_callable_info_can_throw_gerror ((GICallableInfo *)invoker_info));

  vfunc_info = g_object_info_find_vfunc ((GIObjectInfo *)object_info,
                                         "vfunc_meth_with_err");
  g_assert_nonnull (vfunc_info);
  g_assert (g_vfunc_info_get_flags (vfunc_info) & GI_VFUNC_THROWS);
  g_assert (g_callable_info_can_throw_gerror ((GICallableInfo *)vfunc_info));

  g_base_info_unref (vfunc_info);
  g_base_info_unref (invoker_info);
  g_base_info_unref (object_info);
}

static void
test_callback_can_throw_gerror (void)
{
  GIRepository *repo;
  GITypelib *ret;
  GIStructInfo *class_info;
  GIFieldInfo *field_info;
  GITypeInfo *field_type;
  GICallbackInfo *callback_info;
  GError *error = NULL;

  repo = g_irepository_get_default ();

  ret = g_irepository_require (repo, "GIMarshallingTests", NULL, 0, &error);
  g_assert_nonnull (ret);
  g_assert_no_error (error);

  class_info = g_irepository_find_by_name (repo, "GIMarshallingTests", "ObjectClass");
  g_assert_nonnull (class_info);
  g_assert (g_base_info_get_type (class_info) == GI_INFO_TYPE_STRUCT);

  field_info = g_struct_info_find_field (class_info, "vfunc_meth_with_err");
  g_assert_nonnull (field_info);
  g_assert (g_base_info_get_type (field_info) == GI_INFO_TYPE_FIELD);

  field_type = g_field_info_get_type (field_info);
  g_assert_nonnull (field_type);
  g_assert (g_base_info_get_type (field_type) == GI_INFO_TYPE_TYPE);
  g_assert (g_type_info_get_tag (field_type) == GI_TYPE_TAG_INTERFACE);

  callback_info = g_type_info_get_interface (field_type);
  g_assert_nonnull (callback_info);
  g_assert (g_callable_info_can_throw_gerror ((GICallableInfo *)callback_info));

  g_base_info_unref (callback_info);
  g_base_info_unref (field_type);
  g_base_info_unref (field_info);
  g_base_info_unref (class_info);
}

int
main(int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/girepository/throws/invoke-gerror", test_invoke_gerror);
  g_test_add_func ("/girepository/throws/vfunc-can-throw-gerror",
                   test_vfunc_can_throw_gerror);
  g_test_add_func ("/girepository/throws/callback-can-throw-gerror",
                   test_callback_can_throw_gerror);

  return g_test_run ();
}
