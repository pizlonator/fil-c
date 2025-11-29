/* testmultidisplay.c
 * Copyright (C) 2008 Christian Kellner
 * Author: Christian Kellner <gicmo@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <gtk/gtk.h>

static gboolean ask_question = FALSE;
static gboolean show_processes = FALSE;
static gboolean anonymous = FALSE;
static gboolean dont_ask_username = FALSE;
static gboolean dont_ask_domain = FALSE;
static gboolean dont_ask_password = FALSE;
static gboolean dont_save_password = FALSE;

static gboolean done = FALSE;

static void
got_reply (GMountOperation       *op,
           GMountOperationResult  result,
           gpointer               user_data)
{
  if (result == G_MOUNT_OPERATION_HANDLED)
    {

      if (ask_question)
        {
          int choice = g_mount_operation_get_choice (op);
          g_print ("User chose: %d\n", choice);
        }
      else
        {
          if (anonymous)
            g_print ("Anymous: %s\n",
                     g_mount_operation_get_anonymous (op) ? "true" : "false");

          if (!dont_ask_username)
            g_print ("Username: %s\n", g_mount_operation_get_username (op));

          if (!dont_ask_domain)
            g_print ("Domain: %s\n", g_mount_operation_get_domain (op));

          if (!dont_ask_password)
            g_print ("Password: %s\n", g_mount_operation_get_password (op));

          if (!dont_save_password)
            {
              GPasswordSave pw_save;

              pw_save = g_mount_operation_get_password_save (op);
              g_print ("Save password: ");
              switch (pw_save)
                {
               case G_PASSWORD_SAVE_NEVER:
                 g_print ("never");
                 break;

               case G_PASSWORD_SAVE_FOR_SESSION:
                 g_print ("session");
                 break;

               case G_PASSWORD_SAVE_PERMANENTLY:
                 g_print ("forever");
                 break;

               default:
                 g_assert_not_reached ();
                }
              g_print ("\n");
            }
        }
    }
  else if (result == G_MOUNT_OPERATION_ABORTED)
    g_print ("Operation aborted.\n");
  else if (G_MOUNT_OPERATION_UNHANDLED)
    g_assert_not_reached ();

  done = TRUE;
  g_main_context_wakeup (NULL);
}

int
main (int argc, char *argv[])
{
  GMountOperation *op;
  gboolean force_rtl = FALSE;
  GError *error = NULL;
  GOptionEntry options[] = {
    { "ask-question", 'q', 0, G_OPTION_ARG_NONE, &ask_question, "Ask a question not a password.", NULL },
    { "show-processes", 0, 0, G_OPTION_ARG_NONE, &show_processes, "Show (pretend) processes.", NULL },
    { "right-to-left", 'r', 0, G_OPTION_ARG_NONE, &force_rtl, "Force right-to-left layout.", NULL },
    { "anonymous", 'a', 0, G_OPTION_ARG_NONE, &anonymous, "Anonymous login allowed.", NULL },
    { "no-username", 'u', 0, G_OPTION_ARG_NONE, &dont_ask_username, "Don't ask for the username.", NULL },
    { "no-password", 'p', 0, G_OPTION_ARG_NONE, &dont_ask_password, "Don't ask for the password.", NULL },
    { "no-domain", 'd', 0, G_OPTION_ARG_NONE, &dont_ask_domain, "Don't ask for the domain.", NULL },
    { "no-pw-save", 's', 0, G_OPTION_ARG_NONE, &dont_save_password, "Don't show password save options.", NULL },
    { NULL }
  };
  GOptionContext *context;

  context = g_option_context_new ("");
  g_option_context_add_main_entries (context, options, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("Failed to parse args: %s\n", error->message);
      g_error_free (error);
      return 1;
    }
  g_option_context_free (context);

  gtk_init ();

  if (force_rtl)
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  op = gtk_mount_operation_new (NULL);

  g_signal_connect (op, "reply", G_CALLBACK (got_reply), NULL);

  if (ask_question)
    {
      static const char *choices[] = {
        "Yes", "No", "Sauerkraut", NULL
      };

      g_signal_emit_by_name (op, "ask_question", "Foo\nbar", choices);
    }
  else if (show_processes)
    {
      static const char *choices[] = {
        "Yes", "No", "Sauerkraut", NULL
      };
      GArray *pids;
      GPid pid;

      pids = g_array_new (TRUE, FALSE, sizeof (GPid));
      pid = (GPid)1000;
      g_array_append_val (pids, pid);
      pid = (GPid)2000;
      g_array_append_val (pids, pid);
      pid = (GPid)3000;
      g_array_append_val (pids, pid);

      g_signal_emit_by_name (op, "show-processes", "Foo\nbar", pids, choices);

      g_array_unref (pids);
    }
  else
    {
      GAskPasswordFlags flags;

      flags = 0;

      if (!dont_ask_password)
          flags |= G_ASK_PASSWORD_NEED_PASSWORD;

      if (!dont_ask_username)
          flags |= G_ASK_PASSWORD_NEED_USERNAME;

      if (!dont_ask_domain)
          flags |= G_ASK_PASSWORD_NEED_DOMAIN;

      if (anonymous)
          flags |= G_ASK_PASSWORD_ANONYMOUS_SUPPORTED;

      if (!dont_save_password)
          flags |= G_ASK_PASSWORD_SAVING_SUPPORTED;

      g_signal_emit_by_name (op, "ask_password",
                             argc > 1 ? argv[1] : "Credentials needed",
                             argc > 2 ? argv[2] : "default user",
                             argc > 3 ? argv[3] : "default domain",
                             flags);
    }

  while (!done)
    g_main_context_iteration (NULL, TRUE);
  return 0;
}
