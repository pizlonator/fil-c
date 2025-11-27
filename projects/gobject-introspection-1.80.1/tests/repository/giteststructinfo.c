
#include "girepository.h"


static void
test_field_iterators (void)
{
  GIRepository *repo;
  GITypelib *ret;
  GIStructInfo *class_info;
  GIFieldInfo *field_info;
  GError *error = NULL;
  gint i;

  repo = g_irepository_get_default ();

  ret = g_irepository_require (repo, "GIMarshallingTests", NULL, 0, &error);
  g_assert_nonnull (ret);
  g_assert_no_error (error);

  class_info = g_irepository_find_by_name (repo, "GIMarshallingTests", "ObjectClass");
  g_assert_nonnull (class_info);
  g_assert (g_base_info_get_type (class_info) == GI_INFO_TYPE_STRUCT);

  for (i = 0; i < g_struct_info_get_n_fields (class_info); i++)
    {
      const char *field_name;
      GIFieldInfo *found;

      field_info = g_struct_info_get_field (class_info, i);
      g_assert_nonnull (field_info);

      field_name = g_base_info_get_name (field_info);
      g_assert_nonnull (field_name);

      found = g_struct_info_find_field (class_info, field_name);
      g_assert_nonnull (found);
      g_assert_cmpstr (g_base_info_get_name (found), ==, field_name);

      g_base_info_unref (found);
      g_base_info_unref (field_info);
    }

  field_info = g_struct_info_find_field (class_info, "not_a_real_field_name");
  g_assert_null (field_info);

  g_base_info_unref (class_info);
}

int
main(int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/girepository/structinfo/field-iterators", test_field_iterators);

  return g_test_run ();
}
