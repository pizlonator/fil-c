#include <girepository.h>

int
main (void)
{
  GIRepository *repository;
  GError *error = NULL;
  GIBaseInfo *base_info;
  GIArgument in_args[5];
  GIArgument retval;

  repository = g_irepository_get_default ();
  g_irepository_require (repository, "GLib", "2.0", 0, &error);
  if (error)
    {
      g_error ("ERROR: %s\n", error->message);
      return 1;
    }

  base_info = g_irepository_find_by_name (repository, "GLib", "assertion_message");
  if (!base_info)
    {
      g_error ("ERROR: %s\n", "Could not find GLib.assertion_message");
      return 1;
    }

  in_args[0].v_pointer = (gpointer)"domain";
  in_args[1].v_pointer = (gpointer)"glib-print.c";
  in_args[2].v_int = 42;
  in_args[3].v_pointer = (gpointer)"main";
  in_args[4].v_pointer = (gpointer)"hello world";

  if (!g_function_info_invoke ((GIFunctionInfo *) base_info,
                               (const GIArgument *) &in_args,
                               5,
                               NULL,
                               0,
                               &retval,
                               &error))
    {
      g_error ("ERROR: %s\n", error->message);
      return 1;
    }

  g_base_info_unref (base_info);

  return 0;
}
