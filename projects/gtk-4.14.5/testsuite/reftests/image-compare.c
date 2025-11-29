#include "config.h"

#include <stdlib.h>

#include "reftest-compare.h"

static char *opt_filename;
static gboolean opt_quiet;

int
main (int argc, char **argv)
{
  GdkTexture *image1;
  GdkTexture *image2;
  GdkTexture *diff;
  GOptionEntry entries[] = {
    {"output", 'o', 0, G_OPTION_ARG_FILENAME, &opt_filename, "Output location", "FILE" },
    {"quiet", 'q', 0, G_OPTION_ARG_NONE, &opt_quiet, "Don't talk", NULL },
    { NULL, }
  };
  GOptionContext *context;
  GError *error = NULL;

  context = g_option_context_new ("FILE1 FILE2");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      if (error != NULL)
        g_printerr ("%s\n", error->message);
      else
        g_printerr ("Option parse error\n");
      exit (1);
    }

  if (argc < 3)
    {
      g_printerr ("Must specify two files\n");
      exit (1);
    }

  image1 = gdk_texture_new_from_filename (argv[1], &error);
  if (image1 == NULL)
    {
      g_printerr ("Error loading %s: %s\n", argv[1], error->message);
      exit (1);
    }
  image2 = gdk_texture_new_from_filename (argv[2], &error);
  if (image2 == NULL)
    {
      g_printerr ("Error loading %s: %s\n", argv[2], error->message);
      exit (1);
    }

  diff = reftest_compare_textures (image1, image2);

  if (opt_filename && diff)
    {
      if (!gdk_texture_save_to_png (diff, opt_filename))
        {
          g_printerr ("Could not save diff image to %s\n", opt_filename);
          exit (1);
        }
    }

  if (!opt_quiet)
    {
      if (diff)
        {
          if (opt_filename)
            g_print ("Differences witten to %s.\n", opt_filename);
          else
            g_print ("The images are different.\n");
        }
      else
        g_print ("No differences.\n");
    }

  return diff != NULL ? 1 : 0;
}
