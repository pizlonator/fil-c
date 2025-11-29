/* Gtk+ testing utilities
 * Copyright (C) 2007 Imendio AB
 * Authors: Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */


#include "config.h"

#include "gtkspinbutton.h"
#include "gtkmain.h"
#include "gtkbox.h"
#include "gtklabel.h"
#include "gtkbutton.h"
#include "gtktextview.h"
#include "gtkrange.h"

#include <locale.h>
#include <string.h>
#include <math.h>

/* This is a hack.
 * We want to include the same headers as gtktypefuncs.c but we are not
 * allowed to include gtkx.h directly during GTK compilation.
 * So....
 */
#undef GTK_COMPILATION
#include <gtk/gtk.h>
#define GTK_COMPILATION

#ifdef GDK_WINDOWING_BROADWAY
#include <gsk/broadway/gskbroadwayrenderer.h>
#endif

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

/**
 * gtk_test_init:
 * @argcp: Address of the `argc` parameter of the
 *   main() function. Changed if any arguments were handled.
 * @argvp: (inout) (array length=argcp): Address of the `argv`
 *   parameter of main(). Any parameters understood by g_test_init()
 *   or gtk_init() are stripped before return.
 * @...: currently unused
 *
 * This function is used to initialize a GTK test program.
 *
 * It will in turn call g_test_init() and gtk_init() to properly
 * initialize the testing framework and graphical toolkit. It’ll
 * also set the program’s locale to “C”. This is done to make test
 * program environments as deterministic as possible.
 *
 * Like gtk_init() and g_test_init(), any known arguments will be
 * processed and stripped from @argc and @argv.
 */
void
gtk_test_init (int    *argcp,
               char ***argvp,
               ...)
{
  /* g_test_init is defined as a macro that aborts if assertions
   * are disabled. We don't want that, so we call the function.
   */
  (g_test_init) (argcp, argvp, NULL);
  gtk_disable_setlocale();
  setlocale (LC_ALL, "en_US.UTF-8");

  gtk_init ();
}

static gboolean
quit_main_loop_callback (GtkWidget     *widget,
                         GdkFrameClock *frame_clock,
                         gpointer       user_data)
{
  gboolean *done = user_data;

  *done = TRUE;

  g_main_context_wakeup (NULL);

  return G_SOURCE_REMOVE;
}

/**
 * gtk_test_widget_wait_for_draw:
 * @widget: the widget to wait for
 *
 * Enters the main loop and waits for @widget to be “drawn”.
 *
 * In this context that means it waits for the frame clock of
 * @widget to have run a full styling, layout and drawing cycle.
 *
 * This function is intended to be used for syncing with actions that
 * depend on @widget relayouting or on interaction with the display
 * server.
 */
void
gtk_test_widget_wait_for_draw (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  gboolean done = FALSE;

  /* We can do this here because the whole tick procedure does not
   * reenter the main loop. Otherwise we'd need to manually get the
   * frame clock and connect to the after-paint signal.
   */
  gtk_widget_add_tick_callback (widget,
                                quit_main_loop_callback,
                                &done,
                                NULL);

  while (!done)
    g_main_context_iteration (NULL, TRUE);
}

static GType *all_registered_types = NULL;
static guint  n_all_registered_types = 0;

/**
 * gtk_test_list_all_types:
 * @n_types: location to store number of types
 *
 * Return the type ids that have been registered after
 * calling gtk_test_register_all_types().
 *
 * Returns: (array length=n_types zero-terminated=1) (transfer none):
 *    0-terminated array of type ids
 */
const GType*
gtk_test_list_all_types (guint *n_types)
{
  if (n_types)
    *n_types = n_all_registered_types;
  return all_registered_types;
}

/**
 * gtk_test_register_all_types:
 *
 * Force registration of all core GTK object types.
 *
 * This allows to refer to any of those object types via
 * g_type_from_name() after calling this function.
 **/
void
gtk_test_register_all_types (void)
{
  if (!all_registered_types)
    {
      const guint max_gtk_types = 999;
      GType *tp;
      all_registered_types = g_new0 (GType, max_gtk_types);
      tp = all_registered_types;
#include <gtktypefuncs.inc>
      n_all_registered_types = tp - all_registered_types;
      g_assert (n_all_registered_types + 1 < max_gtk_types);
      *tp = 0;
    }
}
