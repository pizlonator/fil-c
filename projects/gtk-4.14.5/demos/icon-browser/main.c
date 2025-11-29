#include <gtk/gtk.h>
#include <iconbrowserapp.h>

int
main (int argc, char *argv[])
{
  return g_application_run (G_APPLICATION (icon_browser_app_new ()), argc, argv);
}
