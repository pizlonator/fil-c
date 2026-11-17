/*
 * cc-region-page.c
 *
 * Copyright (C) 2010 Intel, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Sergey Udaltsov <svu@gnome.org>
 *   Gotam Gorabh <gautamy672@gmail.com>
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-region-page"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "cc-common-language.h"
#include "cc-format-chooser.h"
#include "cc-language-chooser.h"
#include "cc-list-row.h"
#include "cc-region-page.h"

#include <act/act.h>
#include <errno.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>
#include <libgnome-desktop/gnome-xkb-info.h>

#include <locale.h>
#include <polkit/polkit.h>

#define GNOME_SYSTEM_LOCALE_DIR "org.gnome.system.locale"
#define KEY_REGION "region"

#define DEFAULT_LOCALE "en_US.utf-8"

typedef enum {
        USER,
        SYSTEM,
} CcLocaleTarget;

struct _CcRegionPage {
        AdwNavigationPage parent_instance;

        CcListRow       *formats_row;
        AdwBanner       *banner;
        GtkSizeGroup    *input_size_group;
        CcListRow       *login_formats_row;
        GtkWidget       *login_group;
        CcListRow       *login_language_row;
        GtkListBoxRow   *language_row;

        gboolean         login_auto_apply;
        GPermission     *permission;
        GDBusProxy      *localed;
        GDBusProxy      *session;

        ActUserManager  *user_manager;
        ActUser         *user;
        GSettings       *locale_settings;

        gchar           *language;
        gchar           *region;
        gchar           *system_language;
        gchar           *system_region;

        GCancellable    *cancellable;
};

G_DEFINE_TYPE (CcRegionPage, cc_region_page, ADW_TYPE_NAVIGATION_PAGE)

/* Auxiliary methods */

static GFile *
get_needs_restart_file (void)
{
        g_autofree gchar *path = NULL;

        path = g_build_filename (g_get_user_runtime_dir (),
                                 "gnome-control-center-region-needs-restart",
                                 NULL);
        return g_file_new_for_path (path);
}

static void
restart_now (CcRegionPage *self)
{
        g_autoptr(GFile) file = NULL;

        file = get_needs_restart_file ();
        g_file_delete (file, NULL, NULL);

        g_dbus_proxy_call (self->session,
                           "Logout",
                           g_variant_new ("(u)", 0),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);
}

static void
set_restart_notification_visible (CcRegionPage *self,
                                  const gchar  *locale,
                                  gboolean      visible)
{
        locale_t new_locale;
        locale_t current_locale;
        g_autoptr(GFile) file = NULL;
        g_autoptr(GFileOutputStream) output_stream = NULL;
        g_autoptr(GError) error = NULL;

        if (locale) {
                new_locale = newlocale (LC_MESSAGES_MASK, locale, (locale_t) 0);
                if (new_locale != (locale_t) 0) {
                        current_locale = uselocale (new_locale);
                        uselocale (current_locale);
                        freelocale (new_locale);
                } else {
                        g_warning ("Failed to create locale %s: %s", locale, g_strerror (errno));
                }
        }

        adw_banner_set_revealed (self->banner, visible);

        file = get_needs_restart_file ();

        if (!visible) {
                g_file_delete (file, NULL, NULL);
                return;
        }

        output_stream = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (output_stream == NULL) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
                        g_warning ("Unable to create %s: %s", g_file_get_path (file), error->message);
        }
}

typedef struct {
        CcRegionPage *self;
        int category;
        gchar *target_locale;
} MaybeNotifyData;

static void
maybe_notify_data_free (MaybeNotifyData *data)
{
        g_free (data->target_locale);
        g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MaybeNotifyData, maybe_notify_data_free)

static void
maybe_notify_finish (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        g_autoptr(MaybeNotifyData) mnd = data;
        CcRegionPage *self = mnd->self;
        g_autoptr(GError) error = NULL;
        g_autoptr(GVariant) retval = NULL;
        g_autofree gchar *current_lang_code = NULL;
        g_autofree gchar *current_country_code = NULL;
        g_autofree gchar *target_lang_code = NULL;
        g_autofree gchar *target_country_code = NULL;
        const gchar *current_locale = NULL;

        retval = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
        if (!retval) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to get locale: %s\n", error->message);
                return;
        }

        g_variant_get (retval, "(&s)", &current_locale);

        if (!gnome_parse_locale (current_locale,
                                 &current_lang_code,
                                 &current_country_code,
                                 NULL,
                                 NULL))
                return;

        if (!gnome_parse_locale (mnd->target_locale,
                                 &target_lang_code,
                                 &target_country_code,
                                 NULL,
                                 NULL))
                return;

        if (g_str_equal (current_lang_code, target_lang_code) == FALSE ||
            g_str_equal (current_country_code, target_country_code) == FALSE)
                set_restart_notification_visible (self,
                                                  mnd->category == LC_MESSAGES ? mnd->target_locale : NULL,
                                                  TRUE);
        else
                set_restart_notification_visible (self,
                                                  mnd->category == LC_MESSAGES ? mnd->target_locale : NULL,
                                                  FALSE);
}

static void
maybe_notify (CcRegionPage *self,
              int           category,
              const gchar  *target_locale)
{
        MaybeNotifyData *mnd;

        mnd = g_new0 (MaybeNotifyData, 1);
        mnd->self = self;
        mnd->category = category;
        mnd->target_locale = g_strdup (target_locale);

        g_dbus_proxy_call (self->session,
                           "GetLocale",
                           g_variant_new ("(i)", category),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           self->cancellable,
                           maybe_notify_finish,
                           mnd);
}

static void
set_localed_locale (CcRegionPage *self)
{
        g_autoptr(GVariantBuilder) b = NULL;
        g_autofree gchar *lang_value = NULL;

        b = g_variant_builder_new (G_VARIANT_TYPE ("as"));
        lang_value = g_strconcat ("LANG=", self->system_language, NULL);
        g_variant_builder_add (b, "s", lang_value);

        if (self->system_region != NULL) {
                g_autofree gchar *time_value = NULL;
                g_autofree gchar *numeric_value = NULL;
                g_autofree gchar *monetary_value = NULL;
                g_autofree gchar *measurement_value = NULL;
                g_autofree gchar *paper_value = NULL;
                time_value = g_strconcat ("LC_TIME=", self->system_region, NULL);
                g_variant_builder_add (b, "s", time_value);
                numeric_value = g_strconcat ("LC_NUMERIC=", self->system_region, NULL);
                g_variant_builder_add (b, "s", numeric_value);
                monetary_value = g_strconcat ("LC_MONETARY=", self->system_region, NULL);
                g_variant_builder_add (b, "s", monetary_value);
                measurement_value = g_strconcat ("LC_MEASUREMENT=", self->system_region, NULL);
                g_variant_builder_add (b, "s", measurement_value);
                paper_value = g_strconcat ("LC_PAPER=", self->system_region, NULL);
                g_variant_builder_add (b, "s", paper_value);
        }
        g_dbus_proxy_call (self->localed,
                           "SetLocale",
                           g_variant_new ("(asb)", b, TRUE),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);
}

static void
set_system_language (CcRegionPage *self,
                     const gchar  *language)
{
        if (g_strcmp0 (language, self->system_language) == 0)
                return;

        g_free (self->system_language);
        self->system_language = g_strdup (language);

        set_localed_locale (self);
}

static void update_user_language_row (CcRegionPage *self);

static void
update_language (CcRegionPage   *self,
                 CcLocaleTarget  target,
                 const gchar    *language)
{
        switch (target) {
        case USER:
                if (g_strcmp0 (language, self->language) == 0)
                        return;
                act_user_set_language (self->user, language);
                if (self->login_auto_apply)
                        set_system_language (self, language);
                maybe_notify (self, LC_MESSAGES, language);

                g_set_str (&self->language, language);
                update_user_language_row (self);

                break;

        case SYSTEM:
                set_system_language (self, language);
                break;
        }
}

static void
set_system_region (CcRegionPage *self,
                   const gchar  *region)
{
        if (g_strcmp0 (region, self->system_region) == 0)
                return;

        g_free (self->system_region);
        self->system_region = g_strdup (region);

        set_localed_locale (self);
}

static void
update_region (CcRegionPage   *self,
               CcLocaleTarget  target,
               const gchar    *region)
{
        switch (target) {
        case USER:
                if (g_strcmp0 (region, self->region) == 0)
                        return;
                if (region == NULL || region[0] == '\0')
                        g_settings_reset (self->locale_settings, KEY_REGION);
                else
                        g_settings_set_string (self->locale_settings, KEY_REGION, region);
                if (self->login_auto_apply)
                        set_system_region (self, region);

                if (region == NULL || region[0] == '\0') {
                        // Formats (region) are being reset as part of changing the language,
                        // and that already triggers the notification check.
                        return;
                }

                maybe_notify (self, LC_TIME, region);
                break;

        case SYSTEM:
                set_system_region (self, region);
                break;
        }
}

static void
language_response (CcRegionPage      *self,
                   gint               response_id,
                   CcLanguageChooser *chooser)
{
        const gchar *language;

        if (response_id == GTK_RESPONSE_OK) {
                CcLocaleTarget target;

                language = cc_language_chooser_get_language (chooser);
                target = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (chooser), "target"));
                update_language (self, target, language);

                /* Keep format strings consistent with the user's language */
                update_region (self, target, NULL);
        }

        gtk_window_destroy (GTK_WINDOW (chooser));
}

static const gchar *
get_effective_language (CcRegionPage   *self,
                        CcLocaleTarget  target)
{
        switch (target) {
        case USER:
                return self->language;
        case SYSTEM:
                return self->system_language;
        default:
                g_assert_not_reached ();
        }
}

static void
show_language_chooser (CcRegionPage   *self,
                       CcLocaleTarget  target)
{
        CcLanguageChooser *chooser;
        GtkNative *native;

        chooser = cc_language_chooser_new ();
        cc_language_chooser_set_language (chooser, get_effective_language (self, target));
        g_object_set_data (G_OBJECT (chooser), "target", GINT_TO_POINTER (target));
        g_signal_connect_object (chooser, "response",
                                 G_CALLBACK (language_response), self, G_CONNECT_SWAPPED);

        native = gtk_widget_get_native (GTK_WIDGET (self));
        gtk_window_set_transient_for (GTK_WINDOW (chooser),
                                      GTK_WINDOW (native));
        gtk_window_present (GTK_WINDOW (chooser));
}

static const gchar *
get_effective_region (CcRegionPage   *self,
                      CcLocaleTarget  target)
{
        const gchar *region = NULL;

        switch (target) {
        case USER:
                region = self->region;
                break;

        case SYSTEM:
                region = self->system_region;
                break;
        }

        /* Region setting might be empty - show the language because
         * that's what LC_TIME and others will effectively be when the
         * user logs in again. */
        if (region == NULL || region[0] == '\0')
                region = get_effective_language (self, target);

        return region;
}

static void
format_response (CcRegionPage    *self,
                 gint             response_id,
                 CcFormatChooser *chooser)
{
        const gchar *region;

        if (response_id == GTK_RESPONSE_OK) {
                CcLocaleTarget target;

                region = cc_format_chooser_get_region (chooser);
                target = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (chooser), "target"));

                update_region (self, target, region);
        }

        gtk_window_destroy (GTK_WINDOW (chooser));
}

static void
show_region_chooser (CcRegionPage   *self,
                     CcLocaleTarget  target)
{
        CcFormatChooser *chooser;
        GtkNative *native;

        chooser = cc_format_chooser_new ();
        cc_format_chooser_set_region (chooser, get_effective_region (self, target));
        g_object_set_data (G_OBJECT (chooser), "target", GINT_TO_POINTER (target));
        g_signal_connect_object (chooser, "response",
                                 G_CALLBACK (format_response), self, G_CONNECT_SWAPPED);

        native = gtk_widget_get_native (GTK_WIDGET (self));
        gtk_window_set_transient_for (GTK_WINDOW (chooser),
                                      GTK_WINDOW (native));
        gtk_window_present (GTK_WINDOW (chooser));
}

static gboolean
permission_acquired (GPermission *permission, GAsyncResult *res, const gchar *action)
{
        g_autoptr(GError) error = NULL;

        if (!g_permission_acquire_finish (permission, res, &error)) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to acquire permission to %s: %s\n", error->message, action);
                return FALSE;
        }

        return TRUE;
}

static void
choose_language_permission_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
        CcRegionPage *self = user_data;
        if (permission_acquired (G_PERMISSION (source), res, "choose language"))
                show_language_chooser (self, SYSTEM);
}

static void
choose_region_permission_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
        CcRegionPage *self = user_data;
        if (permission_acquired (G_PERMISSION (source), res, "choose region"))
                show_region_chooser (self, SYSTEM);
}

static void
update_user_region_row (CcRegionPage *self)
{
        const gchar *region = get_effective_region (self, USER);
        g_autofree gchar *name = NULL;

        if (region)
                name = gnome_get_country_from_locale (region, region);

        if (!name)
                name = gnome_get_country_from_locale (DEFAULT_LOCALE, DEFAULT_LOCALE);

        adw_action_row_set_subtitle (ADW_ACTION_ROW (self->formats_row), name);
}

static void
update_user_language_row (CcRegionPage *self)
{
        g_autofree gchar *name = NULL;

        if (self->language)
                name = gnome_get_language_from_locale (self->language, self->language);

        if (!name)
                name = gnome_get_language_from_locale (DEFAULT_LOCALE, DEFAULT_LOCALE);

        adw_action_row_set_subtitle (ADW_ACTION_ROW (self->language_row), name);

        /* Formats will change too if not explicitly set. */
        update_user_region_row (self);
}

static void
update_language_from_user (CcRegionPage *self)
{
        const gchar *language = NULL;

        if (act_user_is_loaded (self->user))
                language = act_user_get_language (self->user);

        if (language == NULL || *language == '\0')
                language = setlocale (LC_MESSAGES, NULL);

        g_free (self->language);
        self->language = g_strdup (language);
        update_user_language_row (self);
}

static void
update_region_from_setting (CcRegionPage *self)
{
        g_free (self->region);
        self->region = g_settings_get_string (self->locale_settings, KEY_REGION);
        update_user_region_row (self);
}

static void
setup_language_section (CcRegionPage *self)
{
        self->user = act_user_manager_get_user_by_id (self->user_manager, getuid ());
        g_signal_connect_object (self->user, "notify::language",
                                 G_CALLBACK (update_language_from_user), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->user, "notify::is-loaded",
                                 G_CALLBACK (update_language_from_user), self, G_CONNECT_SWAPPED);

        self->locale_settings = g_settings_new (GNOME_SYSTEM_LOCALE_DIR);
        g_signal_connect_object (self->locale_settings, "changed::" KEY_REGION,
                                 G_CALLBACK (update_region_from_setting), self, G_CONNECT_SWAPPED);

        update_language_from_user (self);
        update_region_from_setting (self);
}

static void
update_login_region (CcRegionPage *self)
{
        g_autofree gchar *name = NULL;

        if (self->system_region)
                name = gnome_get_country_from_locale (self->system_region, self->system_region);
        else if (self->system_language)
                name = gnome_get_country_from_locale (self->system_language, self->system_language);

        if (!name)
                name = gnome_get_country_from_locale (DEFAULT_LOCALE, DEFAULT_LOCALE);

        adw_action_row_set_subtitle (ADW_ACTION_ROW (self->login_formats_row), name);
}

static void
update_login_language (CcRegionPage *self)
{
        g_autofree gchar *name = NULL;

        if (self->system_language)
                name = gnome_get_language_from_locale (self->system_language, self->system_language);

        if (!name)
                name = gnome_get_language_from_locale (DEFAULT_LOCALE, DEFAULT_LOCALE);

        adw_action_row_set_subtitle (ADW_ACTION_ROW (self->login_language_row), name);
        update_login_region (self);
}

static void
set_login_button_visibility (CcRegionPage *self)
{
        gboolean has_multiple_users;
        gboolean loaded;

        g_object_get (self->user_manager, "is-loaded", &loaded, NULL);
        if (!loaded)
          return;

        g_object_get (self->user_manager, "has-multiple-users", &has_multiple_users, NULL);

        self->login_auto_apply = !has_multiple_users && g_permission_get_allowed (self->permission);
        gtk_widget_set_visible (self->login_group, !self->login_auto_apply);

        g_signal_handlers_disconnect_by_func (self->user_manager, set_login_button_visibility, self);
}

/* Callbacks */

static void
on_localed_properties_changed (GDBusProxy    *localed_proxy,
                               GVariant      *changed_properties,
                               const gchar  **invalidated_properties,
                               CcRegionPage  *self)
{
        g_autoptr(GVariant) v = NULL;

        v = g_dbus_proxy_get_cached_property (localed_proxy, "Locale");
        if (v) {
                g_autofree const gchar **strv = NULL;
                gsize len;
                gint i;
                const gchar *lang, *messages, *time;

                strv = g_variant_get_strv (v, &len);

                lang = messages = time = NULL;
                for (i = 0; strv[i]; i++) {
                        if (g_str_has_prefix (strv[i], "LANG=")) {
                                lang = strv[i] + strlen ("LANG=");
                        } else if (g_str_has_prefix (strv[i], "LC_MESSAGES=")) {
                                messages = strv[i] + strlen ("LC_MESSAGES=");
                        } else if (g_str_has_prefix (strv[i], "LC_TIME=")) {
                                time = strv[i] + strlen ("LC_TIME=");
                        }
                }
                if (!lang) {
                        lang = setlocale (LC_MESSAGES, NULL);
                }
                if (!messages) {
                        messages = lang;
                }
                g_free (self->system_language);
                self->system_language = g_strdup (messages);
                g_free (self->system_region);
                self->system_region = g_strdup (time);

                update_login_language (self);
        }
}

static void
localed_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        CcRegionPage *self = data;
        GDBusProxy *proxy;
        g_autoptr(GError) error = NULL;

        proxy = g_dbus_proxy_new_finish (res, &error);

        if (!proxy) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to contact localed: %s\n", error->message);
                return;
        }

        self->localed = proxy;

        g_signal_connect_object (self->localed,
                                 "g-properties-changed",
                                 G_CALLBACK (on_localed_properties_changed),
                                 self,
                                 0);

        on_localed_properties_changed (self->localed, NULL, NULL, self);
}

static void
setup_login_permission (CcRegionPage *self)
{
        g_autoptr(GDBusConnection) bus = NULL;
        g_autoptr(GError) error = NULL;
        gboolean can_acquire;
        gboolean loaded;

        self->permission = polkit_permission_new_sync ("org.freedesktop.locale1.set-locale", NULL, NULL, &error);
        if (self->permission == NULL) {
                g_warning ("Could not get 'org.freedesktop.locale1.set-locale' permission: %s",
                           error->message);
                return;
        }

        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
        g_dbus_proxy_new (bus,
                          G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                          NULL,
                          "org.freedesktop.locale1",
                          "/org/freedesktop/locale1",
                          "org.freedesktop.locale1",
                          self->cancellable,
                          (GAsyncReadyCallback) localed_proxy_ready,
                          self);

        g_object_get (self->user_manager, "is-loaded", &loaded, NULL);
        if (loaded)
                set_login_button_visibility (self);
        else
                g_signal_connect_object (self->user_manager, "notify::is-loaded",
                                         G_CALLBACK (set_login_button_visibility), self, G_CONNECT_SWAPPED);

        can_acquire = self->permission &&
                (g_permission_get_allowed (self->permission) ||
                 g_permission_get_can_acquire (self->permission));
        gtk_widget_set_sensitive (self->login_group, can_acquire);
}

static void
session_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        CcRegionPage *self = data;
        GDBusProxy *proxy;
        g_autoptr(GError) error = NULL;

        proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

        if (!proxy) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to contact gnome-session: %s\n", error->message);
                return;
        }

        self->session = proxy;
}

static void
on_login_formats_row_activated_cb (CcRegionPage *self)
{
        if (g_permission_get_allowed (self->permission)) {
                show_region_chooser (self, SYSTEM);
        } else if (g_permission_get_can_acquire (self->permission)) {
                g_permission_acquire_async (self->permission,
                                            self->cancellable,
                                            choose_region_permission_cb,
                                            self);
        }
}

static void
on_login_language_row_activated_cb (CcRegionPage *self)
{
        if (g_permission_get_allowed (self->permission)) {
                show_language_chooser (self, SYSTEM);
        } else if (g_permission_get_can_acquire (self->permission)) {
                g_permission_acquire_async (self->permission,
                                            self->cancellable,
                                            choose_language_permission_cb,
                                            self);
        }
}

static void
on_user_formats_row_activated_cb (CcRegionPage *self)
{
        show_region_chooser (self, USER);
}

static void
on_user_language_row_activated_cb (CcRegionPage *self)
{
        show_language_chooser (self, USER);
}

/* GObject overrides */

static void
cc_region_page_finalize (GObject *object)
{
        CcRegionPage *self = CC_REGION_PAGE (object);
        GtkWidget *chooser;

        if (self->user_manager) {
                g_signal_handlers_disconnect_by_data (self->user_manager, self);
                self->user_manager = NULL;
        }

        if (self->user) {
                g_signal_handlers_disconnect_by_data (self->user, self);
                self->user = NULL;
        }

        g_clear_object (&self->permission);
        g_clear_object (&self->localed);
        g_clear_object (&self->session);
        g_clear_object (&self->locale_settings);
        g_free (self->language);
        g_free (self->region);
        g_free (self->system_language);
        g_free (self->system_region);

        chooser = g_object_get_data (G_OBJECT (self), "input-chooser");
        if (chooser)
                gtk_window_destroy (GTK_WINDOW (chooser));

        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);

        G_OBJECT_CLASS (cc_region_page_parent_class)->finalize (object);
}

static void
cc_region_page_class_init (CcRegionPageClass * klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = cc_region_page_finalize;

        g_type_ensure (CC_TYPE_LIST_ROW);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/region/cc-region-page.ui");

        gtk_widget_class_bind_template_child (widget_class, CcRegionPage, formats_row);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPage, banner);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPage, login_formats_row);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPage, login_group);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPage, login_language_row);
        gtk_widget_class_bind_template_child (widget_class, CcRegionPage, language_row);

        gtk_widget_class_bind_template_callback (widget_class, on_login_formats_row_activated_cb);
        gtk_widget_class_bind_template_callback (widget_class, on_login_language_row_activated_cb);
        gtk_widget_class_bind_template_callback (widget_class, on_user_formats_row_activated_cb);
        gtk_widget_class_bind_template_callback (widget_class, on_user_language_row_activated_cb);
        gtk_widget_class_bind_template_callback (widget_class, restart_now);
}

static void
cc_region_page_init (CcRegionPage *self)
{
        g_autoptr(GFile) needs_restart_file = NULL;

        gtk_widget_init_template (GTK_WIDGET (self));

        self->cancellable = g_cancellable_new ();

        self->user_manager = act_user_manager_get_default ();

        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.gnome.SessionManager",
                                  "/org/gnome/SessionManager",
                                  "org.gnome.SessionManager",
                                  self->cancellable,
                                  session_proxy_ready,
                                  self);

        setup_login_permission (self);
        setup_language_section (self);

        needs_restart_file = get_needs_restart_file ();
        if (g_file_query_exists (needs_restart_file, self->cancellable))
                set_restart_notification_visible (self, NULL, TRUE);
}
