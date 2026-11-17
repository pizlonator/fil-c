/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* run-passwd.c: this file is part of users-admin, a gnome-system-tools frontend
 * for user administration.
 *
 * Copyright (C) 2002 Diego Gonzalez
 * Copyright (C) 2006 Johannes H. Jensen
 * Copyright (C) 2010 Milan Bouchet-Valat
 *
 * Written by: Diego Gonzalez <diego@pemas.net>
 * Modified by: Johannes H. Jensen <joh@deworks.net>,
 *              Milan Bouchet-Valat <nalimilan@club.fr>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Most of this code originally comes from gnome-about-me-password.c,
 * from gnome-control-center.
 */

#include <config.h>
#include <glib/gi18n.h>

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

#if __sun
#include <sys/types.h>
#include <signal.h>
#endif

#include "run-passwd.h"

/* Passwd states */
typedef enum {
        PASSWD_STATE_NONE,              /* Passwd is not asking for anything */
        PASSWD_STATE_AUTH,              /* Passwd is asking for our current password */
        PASSWD_STATE_NEW,               /* Passwd is asking for our new password */
        PASSWD_STATE_RETYPE,            /* Passwd is asking for our retyped new password */
        PASSWD_STATE_DONE,              /* Passwd succeeded but has not yet exited */
        PASSWD_STATE_ERR                /* Passwd reported an error but has not yet exited */
} PasswdState;

struct PasswdHandler {
        const char *current_password;
        const char *new_password;

        /* Communication with the passwd program */
        GPid backend_pid;

        GIOChannel *backend_stdin;
        GIOChannel *backend_stdout;

        GQueue *backend_stdin_queue;            /* Write queue to backend_stdin */

        /* GMainLoop IDs */
        guint backend_child_watch_id;           /* g_child_watch_add (PID) */
        guint backend_stdout_watch_id;          /* g_io_add_watch (stdout) */

        /* State of the passwd program */
        PasswdState backend_state;
        gboolean    changing_password;

        PasswdCallback auth_cb;
        gpointer       auth_cb_data;

        PasswdCallback chpasswd_cb;
        gpointer       chpasswd_cb_data;
};

/* Buffer size for backend output */
#define BUFSIZE 64


static GQuark
passwd_error_quark (void)
{
        static GQuark q = 0;

        if (q == 0) {
                q = g_quark_from_static_string("passwd_error");
        }

        return q;
}

/* Error handling */
#define PASSWD_ERROR (passwd_error_quark ())


static void
stop_passwd (PasswdHandler *passwd_handler);

static void
free_passwd_resources (PasswdHandler *passwd_handler);

static gboolean
io_watch_stdout (GIOChannel *source, GIOCondition condition, PasswdHandler *passwd_handler);


/*
 * Spawning and closing of backend {{
 */

/* Child watcher */
static void
child_watch_cb (GPid pid, gint status, PasswdHandler *passwd_handler)
{
        if (WIFEXITED (status)) {
                if (WEXITSTATUS (status) >= 255) {
                        g_warning ("Child exited unexpectedly");
                }
                if (WEXITSTATUS (status) == 0) {
                        if (passwd_handler->backend_state == PASSWD_STATE_RETYPE) {
                                passwd_handler->backend_state = PASSWD_STATE_DONE;
                                if (passwd_handler->chpasswd_cb)
                                                passwd_handler->chpasswd_cb (passwd_handler,
                                                                             NULL,
                                                                             passwd_handler->chpasswd_cb_data);
                        }
                }
        }

        free_passwd_resources (passwd_handler);
}

static void
child_setup_cb (gpointer data)
{
	signal (SIGPIPE, SIG_IGN);
	dup2 (fileno (stdout), fileno (stderr));
}

/* Spawn passwd backend
 * Returns: TRUE on success, FALSE otherwise and sets error appropriately */
static gboolean
spawn_passwd (PasswdHandler *passwd_handler, GError **error)
{
        gchar   *argv[2];
        gchar  **envp;
        gint    my_stdin, my_stdout;

        argv[0] = "/usr/bin/passwd";    /* Is it safe to rely on a hard-coded path? */
        argv[1] = NULL;

        envp = g_get_environ ();
        envp = g_environ_setenv (envp, "LC_ALL", "C", TRUE);

        if (!g_spawn_async_with_pipes (NULL,                            /* Working directory */
                                       argv,                            /* Argument vector */
                                       envp,                            /* Environment */
                                       G_SPAWN_DO_NOT_REAP_CHILD,       /* Flags */
                                       child_setup_cb,                  /* Child setup */
                                       NULL,                            /* Data to child setup */
                                       &passwd_handler->backend_pid,    /* PID */
                                       &my_stdin,                       /* Stdin */
                                       &my_stdout,                      /* Stdout */
                                       NULL,                            /* Stderr */
                                       error)) {                        /* GError */

                /* An error occurred */
                free_passwd_resources (passwd_handler);

                g_strfreev (envp);

                return FALSE;
        }

        g_strfreev (envp);

        /* Open IO Channels */
        passwd_handler->backend_stdin = g_io_channel_unix_new (my_stdin);
        passwd_handler->backend_stdout = g_io_channel_unix_new (my_stdout);

        /* Set raw encoding */
        /* Set nonblocking mode */
        if (g_io_channel_set_encoding (passwd_handler->backend_stdin, NULL, error) != G_IO_STATUS_NORMAL ||
                g_io_channel_set_encoding (passwd_handler->backend_stdout, NULL, error) != G_IO_STATUS_NORMAL ||
                g_io_channel_set_flags (passwd_handler->backend_stdin, G_IO_FLAG_NONBLOCK, error) != G_IO_STATUS_NORMAL ||
                g_io_channel_set_flags (passwd_handler->backend_stdout, G_IO_FLAG_NONBLOCK, error) != G_IO_STATUS_NORMAL ) {

                /* Clean up */
                stop_passwd (passwd_handler);
                return FALSE;
        }

        /* Turn off buffering */
        g_io_channel_set_buffered (passwd_handler->backend_stdin, FALSE);
        g_io_channel_set_buffered (passwd_handler->backend_stdout, FALSE);

        /* Add IO Channel watcher */
        passwd_handler->backend_stdout_watch_id = g_io_add_watch (passwd_handler->backend_stdout,
                                                                  G_IO_IN | G_IO_PRI,
                                                                  (GIOFunc) io_watch_stdout, passwd_handler);

        /* Add child watcher */
        passwd_handler->backend_child_watch_id = g_child_watch_add (passwd_handler->backend_pid, (GChildWatchFunc) child_watch_cb, passwd_handler);

        /* Success! */

        return TRUE;
}

/* Stop passwd backend */
static void
stop_passwd (PasswdHandler *passwd_handler)
{
        /* This is the standard way of returning from the dialog with passwd.
         * If we return this way we can safely kill passwd as it has completed
         * its task.
         */

        if (passwd_handler->backend_pid != -1) {
                kill (passwd_handler->backend_pid, 9);
        }

        /* We must run free_passwd_resources here and not let our child
         * watcher do it, since it will access invalid memory after the
         * dialog has been closed and cleaned up.
         *
         * If we had more than a single thread we'd need to remove
         * the child watch before trying to kill the child.
         */
        free_passwd_resources (passwd_handler);
}

/* Clean up passwd resources */
static void
free_passwd_resources (PasswdHandler *passwd_handler)
{
        /* Remove the child watcher */
        g_clear_handle_id (&passwd_handler->backend_child_watch_id, g_source_remove);

        /* Close IO channels (internal file descriptors are automatically closed) */
        if (passwd_handler->backend_stdin != NULL) {
                g_autoptr(GError) error = NULL;

                if (g_io_channel_shutdown (passwd_handler->backend_stdin, TRUE, &error) != G_IO_STATUS_NORMAL) {
                        g_warning ("Could not shutdown backend_stdin IO channel: %s", error->message);
                }

                g_clear_pointer (&passwd_handler->backend_stdin, g_io_channel_unref);
        }

        if (passwd_handler->backend_stdout != NULL) {
                g_autoptr(GError) error = NULL;

                if (g_io_channel_shutdown (passwd_handler->backend_stdout, TRUE, &error) != G_IO_STATUS_NORMAL) {
                        g_warning ("Could not shutdown backend_stdout IO channel: %s", error->message);
                }

                g_clear_pointer (&passwd_handler->backend_stdout, g_io_channel_unref);
        }

        /* Remove IO watcher */
        g_clear_handle_id (&passwd_handler->backend_stdout_watch_id, g_source_remove);

        /* Close PID */
        if (passwd_handler->backend_pid != -1) {

                g_spawn_close_pid (passwd_handler->backend_pid);

                passwd_handler->backend_pid = -1;
        }

        /* Clear backend state */
        passwd_handler->backend_state = PASSWD_STATE_NONE;
}

/*
 * }} Spawning and closing of backend
 */

/*
 * Backend communication code {{
 */

/* Write the first element of queue through channel */
static void
io_queue_pop (GQueue *queue, GIOChannel *channel)
{
        g_autofree gchar *buf = NULL;
        gsize   bytes_written;
        g_autoptr(GError) error = NULL;

        buf = g_queue_pop_head (queue);

        if (buf != NULL) {

                if (g_io_channel_write_chars (channel, buf, -1, &bytes_written, &error) != G_IO_STATUS_NORMAL) {
                        g_warning ("Could not write queue element \"%s\" to channel: %s", buf, error->message);
                }

                /* Ensure passwords are cleared from memory */
                memset (buf, 0, strlen (buf));
        }
}

/* Goes through the argument list, checking if one of them occurs in str
 * Returns: TRUE as soon as an element is found to match, FALSE otherwise */
static gboolean
is_string_complete (gchar *str, ...)
{
        va_list ap;
        gchar   *arg;

        if (strlen (str) == 0) {
                return FALSE;
        }

        va_start (ap, str);

        while ((arg = va_arg (ap, char *)) != NULL) {
                if (strstr (str, arg) != NULL) {
                        va_end (ap);
                        return TRUE;
                }
        }

        va_end (ap);

        return FALSE;
}

/*
 * IO watcher for stdout, called whenever there is data to read from the backend.
 * This is where most of the actual IO handling happens.
 */
static gboolean
io_watch_stdout (GIOChannel *source, GIOCondition condition, PasswdHandler *passwd_handler)
{
        static GString *str = NULL;     /* Persistent buffer */

        gchar           buf[BUFSIZE];           /* Temporary buffer */
        gsize           bytes_read;
        g_autoptr(GError) gio_error = NULL;

        gboolean        reinit = FALSE;

        /* Initialize buffer */
        if (str == NULL) {
                str = g_string_new ("");
        }

        if (g_io_channel_read_chars (source, buf, BUFSIZE, &bytes_read, &gio_error)
            != G_IO_STATUS_NORMAL) {
                g_warning ("IO Channel read error: %s", gio_error->message);
                return TRUE;
        }

        str = g_string_append_len (str, buf, bytes_read);

        /* In which state is the backend? */
        switch (passwd_handler->backend_state) {
                case PASSWD_STATE_AUTH:
                        /* Passwd is asking for our current password */

                        if (is_string_complete (str->str, "assword: ", "failure", "wrong", "error", NULL)) {

                                if (strstr (str->str, "assword: ") != NULL &&
                                    strstr (str->str, "incorrect") == NULL &&
                                    strstr (str->str, "urrent") == NULL) {
                                        /* Authentication successful */

                                        passwd_handler->backend_state = PASSWD_STATE_NEW;

                                        /* Trigger callback to update authentication status */
                                        if (passwd_handler->auth_cb)
                                                passwd_handler->auth_cb (passwd_handler,
                                                                         NULL,
                                                                         passwd_handler->auth_cb_data);

                                } else {
                                        /* Authentication failed */
                                        g_autoptr(GError) error = NULL;

                                        error = g_error_new_literal (PASSWD_ERROR, PASSWD_ERROR_AUTH_FAILED,
                                                                     _("Authentication failed"));

                                        passwd_handler->changing_password = FALSE;

                                        /* This error can happen both while authenticating or while changing password:
                                         * if chpasswd_cb is set, this means we're already changing password */
                                        if (passwd_handler->chpasswd_cb)
                                                passwd_handler->chpasswd_cb (passwd_handler,
                                                                             error,
                                                                             passwd_handler->chpasswd_cb_data);
                                        else if (passwd_handler->auth_cb)
                                                passwd_handler->auth_cb (passwd_handler,
                                                                         error,
                                                                         passwd_handler->auth_cb_data);
                                }

                                reinit = TRUE;
                        }
                        break;
                case PASSWD_STATE_NEW:
                        /* Passwd is asking for our new password */

                        if (is_string_complete (str->str, "assword: ", NULL)) {
                                /* Advance to next state */
                                passwd_handler->backend_state = PASSWD_STATE_RETYPE;

                                /* Pop retyped password from queue and into IO channel */
                                io_queue_pop (passwd_handler->backend_stdin_queue, passwd_handler->backend_stdin);

                                reinit = TRUE;
                        }
                        break;
                case PASSWD_STATE_RETYPE:
                        /* Passwd is asking for our retyped new password */

                        if (is_string_complete (str->str,
                                                "successfully",
                                                "short",
                                                "longer",
                                                "palindrome",
                                                "dictionary",
                                                "simple",
                                                "simplistic",
                                                "similar",
                                                "case",
                                                "different",
                                                "wrapped",
                                                "recovered",
                                                "recent",
                                                "unchanged",
                                                "match",
                                                "1 numeric or special",
                                                "failure",
                                                "DIFFERENT",
                                                "BAD PASSWORD",
                                                NULL)) {

                                if (strstr (str->str, "successfully") != NULL) {
                                        /* Hooray! */

                                        passwd_handler->backend_state = PASSWD_STATE_DONE;
                                        /* Trigger callback to update status */
                                        if (passwd_handler->chpasswd_cb)
                                                passwd_handler->chpasswd_cb (passwd_handler,
                                                                             NULL,
                                                                             passwd_handler->chpasswd_cb_data);
                                }
                                else {
                                        /* Ohnoes! */
                                        g_autoptr(GError) error = NULL;

                                        if (strstr (str->str, "recovered") != NULL) {
                                                /* What does this indicate?
                                                 * "Authentication information cannot be recovered?" from libpam? */
                                                error = g_error_new_literal (PASSWD_ERROR, PASSWD_ERROR_UNKNOWN,
                                                                             str->str);
                                        } else if (strstr (str->str, "short") != NULL ||
                                                   strstr (str->str, "longer") != NULL) {
                                                error = g_error_new (PASSWD_ERROR, PASSWD_ERROR_REJECTED,
                                                                     _("The new password is too short"));
                                        } else if (strstr (str->str, "palindrome") != NULL ||
                                                   strstr (str->str, "simple") != NULL ||
                                                   strstr (str->str, "simplistic") != NULL ||
                                                   strstr (str->str, "dictionary") != NULL) {
                                                error = g_error_new (PASSWD_ERROR, PASSWD_ERROR_REJECTED,
                                                                     _("The new password is too simple"));
                                        } else if (strstr (str->str, "similar") != NULL ||
                                                   strstr (str->str, "different") != NULL ||
                                                   strstr (str->str, "case") != NULL ||
                                                   strstr (str->str, "wrapped") != NULL) {
                                                error = g_error_new (PASSWD_ERROR, PASSWD_ERROR_REJECTED,
                                                                     _("The old and new passwords are too similar"));
                                        } else if (strstr (str->str, "recent") != NULL) {
                                                error = g_error_new (PASSWD_ERROR, PASSWD_ERROR_REJECTED,
                                                                     _("The new password has already been used recently."));
                                        } else if (strstr (str->str, "1 numeric or special") != NULL) {
                                                error = g_error_new (PASSWD_ERROR, PASSWD_ERROR_REJECTED,
                                                                     _("The new password must contain numeric or special characters"));
                                        } else if (strstr (str->str, "unchanged") != NULL ||
                                                   strstr (str->str, "match") != NULL) {
                                                error = g_error_new (PASSWD_ERROR, PASSWD_ERROR_REJECTED,
                                                                     _("The old and new passwords are the same"));
                                        } else if (strstr (str->str, "failure") != NULL) {
                                                /* Authentication failure */
                                                error = g_error_new (PASSWD_ERROR, PASSWD_ERROR_AUTH_FAILED,
                                                                     _("Your password has been changed since you initially authenticated!"));
                                        }
                                        else if (strstr (str->str, "DIFFERENT")) {
                                                error = g_error_new (PASSWD_ERROR, PASSWD_ERROR_REJECTED,
                                                                     _("The new password does not contain enough different characters"));
                                        }
                                        else {
                                                error = g_error_new (PASSWD_ERROR, PASSWD_ERROR_UNKNOWN,
                                                                     _("Unknown error"));
                                        }

                                        /* At this point, passwd might have exited, in which case
                                         * child_watch_cb should clean up for us and remove this watcher.
                                         * On some error conditions though, passwd just re-prompts us
                                         * for our new password. */
                                        passwd_handler->backend_state = PASSWD_STATE_ERR;

                                        passwd_handler->changing_password = FALSE;

                                        /* Trigger callback to update status */
                                        if (passwd_handler->chpasswd_cb)
                                                passwd_handler->chpasswd_cb (passwd_handler,
                                                                             error,
                                                                             passwd_handler->chpasswd_cb_data);
                                }

                                reinit = TRUE;

                                /* child_watch_cb should clean up for us now */
                        }
                        break;
                case PASSWD_STATE_NONE:
                        /* Passwd is not asking for anything yet */
                        if (is_string_complete (str->str, "assword: ", NULL)) {

                                /* If the user does not have a password set,
                                 * passwd will immediately ask for the new password,
                                 * so skip the AUTH phase */
                                if (is_string_complete (str->str, "new", "New", NULL)) {
                                        g_autofree gchar *pw = NULL;

                                        passwd_handler->backend_state = PASSWD_STATE_NEW;

                                        /* since passwd didn't ask for our old password
                                         * in this case, simply remove it from the queue */
                                        pw = g_queue_pop_head (passwd_handler->backend_stdin_queue);

                                        /* Pop the IO queue, i.e. send new password */
                                        io_queue_pop (passwd_handler->backend_stdin_queue, passwd_handler->backend_stdin);

                                } else {

                                        passwd_handler->backend_state = PASSWD_STATE_AUTH;

                                        /* Pop the IO queue, i.e. send current password */
                                        io_queue_pop (passwd_handler->backend_stdin_queue, passwd_handler->backend_stdin);
                                }

                                reinit = TRUE;
                        }
                        break;
                default:
                        /* Passwd has returned an error */
                        reinit = TRUE;
                        break;
        }

        if (reinit) {
                g_string_free (str, TRUE);
                str = NULL;
        }

        /* Continue calling us */
        return TRUE;
}

/*
 * }} Backend communication code
 */

/* Adds the current password to the IO queue */
static void
authenticate (PasswdHandler *passwd_handler)
{
        gchar   *s;

        s = g_strdup_printf ("%s\n", passwd_handler->current_password);

        g_queue_push_tail (passwd_handler->backend_stdin_queue, s);
}

/* Adds the new password twice to the IO queue */
static void
update_password (PasswdHandler *passwd_handler)
{
        gchar   *s;

        s = g_strdup_printf ("%s\n", passwd_handler->new_password);

        g_queue_push_tail (passwd_handler->backend_stdin_queue, s);
        /* We need to allocate new space because io_queue_pop() g_free()s
         * every element of the queue after it's done */
        g_queue_push_tail (passwd_handler->backend_stdin_queue, g_strdup (s));
}


PasswdHandler *
passwd_init (void)
{
        PasswdHandler *passwd_handler;

        passwd_handler = g_new0 (PasswdHandler, 1);

        /* Initialize backend_pid. -1 means the backend is not running */
        passwd_handler->backend_pid = -1;

        /* Initialize IO Channels */
        passwd_handler->backend_stdin = NULL;
        passwd_handler->backend_stdout = NULL;

        /* Initialize write queue */
        passwd_handler->backend_stdin_queue = g_queue_new ();

        /* Initialize watchers */
        passwd_handler->backend_child_watch_id = 0;
        passwd_handler->backend_stdout_watch_id = 0;

        /* Initialize backend state */
        passwd_handler->backend_state = PASSWD_STATE_NONE;
        passwd_handler->changing_password = FALSE;

        return passwd_handler;
}

void
passwd_destroy (PasswdHandler *passwd_handler)
{
        g_queue_free (passwd_handler->backend_stdin_queue);
        stop_passwd (passwd_handler);
        g_free (passwd_handler);
}

void
passwd_authenticate (PasswdHandler *passwd_handler,
                     const char    *current_password,
                     PasswdCallback cb,
                     const gpointer user_data)
{
        g_autoptr(GError) error = NULL;

        /* Don't stop if we've already started changing password */
        if (passwd_handler->changing_password)
                return;

        /* Clear data from possible previous attempts to change password */
        passwd_handler->new_password = NULL;
        passwd_handler->chpasswd_cb = NULL;
        passwd_handler->chpasswd_cb_data = NULL;
        g_queue_foreach (passwd_handler->backend_stdin_queue, (GFunc) g_free, NULL);
        g_queue_clear (passwd_handler->backend_stdin_queue);

        passwd_handler->current_password = current_password;
        passwd_handler->auth_cb = cb;
        passwd_handler->auth_cb_data = user_data;

        /* Spawn backend */
        stop_passwd (passwd_handler);

        if (!spawn_passwd (passwd_handler, &error)) {
                g_warning ("%s", error->message);
                return;
        }

        authenticate (passwd_handler);

        /* Our IO watcher should now handle the rest */
}

gboolean
passwd_change_password (PasswdHandler *passwd_handler,
                        const char    *new_password,
                        PasswdCallback cb,
                        const gpointer user_data)
{
        passwd_handler->changing_password = TRUE;

        passwd_handler->new_password = new_password;
        passwd_handler->chpasswd_cb = cb;
        passwd_handler->chpasswd_cb_data = user_data;

        /* Stop passwd if an error occurred and it is still running */
        if (passwd_handler->backend_state == PASSWD_STATE_ERR) {

                /* Stop passwd, free resources */
                stop_passwd (passwd_handler);
        }

        /* Check that the backend is still running, or that an error
         * has occurred but it has not yet exited */
        if (passwd_handler->backend_pid == -1) {
                /* If it is not, re-run authentication */
                g_autoptr(GError) error = NULL;

                /* Spawn backend */
                stop_passwd (passwd_handler);

                if (!spawn_passwd (passwd_handler, &error)) {
                        g_warning ("%s", error->message);
                        return FALSE;
                }

                /* Add current and new passwords to queue */
                authenticate (passwd_handler);
                update_password (passwd_handler);
        } else {
                /* Only add new passwords to queue */
                update_password (passwd_handler);
        }

        /* Pop new password through the backend.
         * If user has no password, popping the queue would output current
         * password, while 'passwd' is waiting for the new one. So wait for
         * io_watch_stdout() to remove current password from the queue,
         * and output the new one for us.
         */
        if (passwd_handler->current_password)
                io_queue_pop (passwd_handler->backend_stdin_queue, passwd_handler->backend_stdin);

        /* Our IO watcher should now handle the rest */

        return TRUE;
}
