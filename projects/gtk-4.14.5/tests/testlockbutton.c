/* testlockbutton.c
 * Copyright (C) 2011 Red Hat, Inc.
 * Authors: Matthias Clasen
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

#include <gtk/gtk.h>
#include <gio/gio.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/* a fake permission implementation */

#define G_TYPE_TEST_PERMISSION      (g_test_permission_get_type ())
#define G_TEST_PERMISSION(inst)     (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                                     G_TYPE_TEST_PERMISSION,             \
                                     GTestPermission))
#define G_IS_TEST_PERMISSION(inst)  (G_TYPE_CHECK_INSTANCE_TYPE ((inst), \
                                     G_TYPE_TEST_PERMISSION))

typedef struct _GTestPermission GTestPermission;
typedef struct _GTestPermissionClass GTestPermissionClass;

struct _GTestPermission
{
  GPermission parent;

  gboolean success;
};

struct _GTestPermissionClass
{
  GPermissionClass parent_class;
};

static GType g_test_permission_get_type (void);
G_DEFINE_TYPE (GTestPermission, g_test_permission, G_TYPE_PERMISSION)

static void
g_test_permission_init (GTestPermission *test)
{
}

static gboolean
update_allowed (GTestPermission  *test,
                gboolean          allowed,
                GError          **error)
{
  gboolean can_acquire, can_release;

  g_object_get (test,
                "can-acquire", &can_acquire,
                "can-release", &can_release,
                NULL);

  if (test->success)
    {
      g_permission_impl_update (G_PERMISSION (test),
                                allowed, can_acquire, can_release);
      return TRUE;
    }
  else
    {
      g_set_error_literal (error,
                           G_IO_ERROR, G_IO_ERROR_FAILED, "Sorry, no luck");
      return FALSE;
    }
}

static gboolean
acquire (GPermission   *permission,
         GCancellable  *cancellable,
         GError       **error)
{
  GTestPermission *test = G_TEST_PERMISSION (permission);
  return update_allowed (test, TRUE, error);
}

static void
acquire_async (GPermission         *permission,
               GCancellable        *cancellable,
               GAsyncReadyCallback  callback,
               gpointer             user_data)
{
  GTask *result;
  g_print ("GTestPermission::acquire_async\n");
  result = g_task_new ((GObject*)permission,
                       cancellable,
                       callback,
                       user_data);
  g_task_return_boolean (result, TRUE);
  g_object_unref (result);
}

static gboolean
acquire_finish (GPermission   *permission,
                GAsyncResult  *result,
                GError       **error)
{
  GTestPermission *test = G_TEST_PERMISSION (permission);
  g_print ("GTestPermission::acquire_finish\n");
  return update_allowed (test, TRUE, error);
}

static gboolean
release (GPermission   *permission,
         GCancellable  *cancellable,
         GError       **error)
{
  GTestPermission *test = G_TEST_PERMISSION (permission);
  return update_allowed (test, FALSE, error);
}

static void
release_async (GPermission         *permission,
               GCancellable        *cancellable,
               GAsyncReadyCallback  callback,
               gpointer             user_data)
{
  GTask *result;
  result = g_task_new ((GObject*)permission,
                       cancellable,
                       callback,
                       user_data);
  g_task_return_boolean (result, TRUE);
  g_object_unref (result);
}

static gboolean
release_finish (GPermission   *permission,
                GAsyncResult  *result,
                GError       **error)
{
  GTestPermission *test = G_TEST_PERMISSION (permission);
  return update_allowed (test, FALSE, error);
}

static void
g_test_permission_class_init (GTestPermissionClass *class)
{
  GPermissionClass *permission_class = G_PERMISSION_CLASS (class);

  permission_class->acquire = acquire;
  permission_class->acquire_async = acquire_async;
  permission_class->acquire_finish = acquire_finish;

  permission_class->release = release;
  permission_class->release_async = release_async;
  permission_class->release_finish = release_finish;
}

static void
g_test_permission_set_success (GTestPermission *permission,
                               gboolean         success)
{
  permission->success = success;
}

static GtkWidget *allowed_button;
static GtkWidget *can_acquire_button;
static GtkWidget *can_release_button;
static GtkWidget *success_button;

static void
update_clicked (GtkButton *button, GtkLockButton *lockbutton)
{
  GPermission *permission;
  gboolean allowed, can_acquire, can_release;
  gboolean success;

  permission = gtk_lock_button_get_permission (lockbutton);

  allowed = gtk_check_button_get_active (GTK_CHECK_BUTTON (allowed_button));
  can_acquire = gtk_check_button_get_active (GTK_CHECK_BUTTON (can_acquire_button));
  can_release = gtk_check_button_get_active (GTK_CHECK_BUTTON (can_release_button));
  success = gtk_check_button_get_active (GTK_CHECK_BUTTON (success_button));
  g_permission_impl_update (permission, allowed, can_acquire, can_release);
  g_test_permission_set_success (G_TEST_PERMISSION (permission), success);
}

static GtkWidget *content;

static void
permission_changed (GPermission *permission,
                    GParamSpec  *pspec)
{
  gboolean allowed, can_acquire, can_release;

  g_object_get (permission,
                "allowed", &allowed,
                "can-acquire", &can_acquire,
                "can-release", &can_release,
                NULL);

  gtk_check_button_set_active (GTK_CHECK_BUTTON (allowed_button), allowed);
  gtk_check_button_set_active (GTK_CHECK_BUTTON (can_acquire_button), can_acquire);
  gtk_check_button_set_active (GTK_CHECK_BUTTON (can_release_button), can_release);

  gtk_widget_set_sensitive (content, allowed);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *dialog;
  GtkWidget *button;
  GtkWidget *box;
  GtkWidget *update;
  GPermission *permission;

  gtk_init ();

  permission = g_object_new (G_TYPE_TEST_PERMISSION, NULL);

  window = gtk_window_new ();
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_window_set_child (GTK_WINDOW (window), box);

  allowed_button = gtk_check_button_new_with_label ("Allowed");
  gtk_box_append (GTK_BOX (box), allowed_button);
  can_acquire_button = gtk_check_button_new_with_label ("Can acquire");
  gtk_box_append (GTK_BOX (box), can_acquire_button);
  can_release_button = gtk_check_button_new_with_label ("Can release");
  gtk_box_append (GTK_BOX (box), can_release_button);
  success_button = gtk_check_button_new_with_label ("Will succeed");
  gtk_box_append (GTK_BOX (box), success_button);
  update = gtk_button_new_with_label ("Update");
  gtk_box_append (GTK_BOX (box), update);
  g_signal_connect (permission, "notify",
                    G_CALLBACK (permission_changed), NULL);

  button = gtk_lock_button_new (permission);

  g_signal_connect (update, "clicked",
                    G_CALLBACK (update_clicked), button);

  dialog = gtk_dialog_new_with_buttons ("Dialog", NULL, 0,
                                        "Close", GTK_RESPONSE_CLOSE,
                                        "Some other action", GTK_RESPONSE_APPLY,
                                        NULL);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_box_append (GTK_BOX (content), gtk_check_button_new_with_label ("Control 1"));
  gtk_box_append (GTK_BOX (content), gtk_check_button_new_with_label ("Control 2"));
  gtk_widget_set_sensitive (content, FALSE);

  gtk_box_append (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), content);
  gtk_box_append (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), button);

  gtk_window_present (GTK_WINDOW (window));
  gtk_window_present (GTK_WINDOW (dialog));

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
