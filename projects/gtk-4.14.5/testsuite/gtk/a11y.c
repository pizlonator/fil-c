#include <gtk/gtk.h>
#include "gtk/gtkaccessibleprivate.h"

static void
test_role_to_name (void)
{
  GEnumClass *class;

  class = g_type_class_ref (GTK_TYPE_ACCESSIBLE_ROLE);

  for (unsigned int i = 0; i < class->n_values; i++)
    {
      GtkAccessibleRole role = class->values[i].value;

      g_assert_nonnull (gtk_accessible_role_to_name (role, "gtk40"));
    }

  g_type_class_unref (class);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/a11y/role-to-name", test_role_to_name);

  return g_test_run ();
}
