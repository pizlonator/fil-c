/* GTK - The GIMP Toolkit
 * Copyright (C) 2001 CodeFactory AB
 * Copyright (C) 2001, 2002 Anders Carlsson
 * Copyright (C) 2003, 2004 Matthias Clasen <mclasen@redhat.com>
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

/*
 * Author: Anders Carlsson <andersca@gnome.org>
 *
 * Modified by the GTK+ Team and others 1997-2004.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <string.h>

#include <cairo-gobject.h>

#include "gtkaboutdialog.h"
#include "gtkbutton.h"
#include "gtkgrid.h"
#include "gtkbox.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkstack.h"
#include "gtktextview.h"
#include "gtktypebuiltins.h"
#include "gtkstack.h"
#include "gtksettings.h"
#include "gtkurilauncher.h"
#include "gtkprivate.h"
#include <glib/gi18n-lib.h>
#include "gtkeventcontrollermotion.h"
#include "gtkeventcontrollerkey.h"
#include "gtkgestureclick.h"
#include "gtkcssnodeprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkcsscolorvalueprivate.h"


/**
 * GtkAboutDialog:
 *
 * The `GtkAboutDialog` offers a simple way to display information about
 * a program.
 *
 * The shown information includes the programs' logo, name, copyright,
 * website and license. It is also possible to give credits to the authors,
 * documenters, translators and artists who have worked on the program.
 *
 * An about dialog is typically opened when the user selects the `About`
 * option from the `Help` menu. All parts of the dialog are optional.
 *
 * ![An example GtkAboutDialog](aboutdialog.png)
 *
 * About dialogs often contain links and email addresses. `GtkAboutDialog`
 * displays these as clickable links. By default, it calls [method@Gtk.FileLauncher.launch]
 * when a user clicks one. The behaviour can be overridden with the
 * [signal@Gtk.AboutDialog::activate-link] signal.
 *
 * To specify a person with an email address, use a string like
 * `Edgar Allan Poe <edgar@poe.com>`. To specify a website with a title,
 * use a string like `GTK team https://www.gtk.org`.
 *
 * To make constructing a `GtkAboutDialog` as convenient as possible, you can
 * use the function [func@Gtk.show_about_dialog] which constructs and shows
 * a dialog and keeps it around so that it can be shown again.
 *
 * Note that GTK sets a default title of `_("About %s")` on the dialog
 * window (where `%s` is replaced by the name of the application, but in
 * order to ensure proper translation of the title, applications should
 * set the title property explicitly when constructing a `GtkAboutDialog`,
 * as shown in the following example:
 *
 * ```c
 * GFile *logo_file = g_file_new_for_path ("./logo.png");
 * GdkTexture *example_logo = gdk_texture_new_from_file (logo_file, NULL);
 * g_object_unref (logo_file);
 *
 * gtk_show_about_dialog (NULL,
 *                        "program-name", "ExampleCode",
 *                        "logo", example_logo,
 *                        "title", _("About ExampleCode"),
 *                        NULL);
 * ```
 *
 * ## CSS nodes
 *
 * `GtkAboutDialog` has a single CSS node with the name `window` and style
 * class `.aboutdialog`.

 */

typedef struct
{
  const char *name;
  const char *url;
} LicenseInfo;

/* LicenseInfo for each GtkLicense type; keep in the same order as the enumeration */
static const LicenseInfo gtk_license_info [] = {
  { N_("License"), NULL },
  { N_("Custom License") , NULL },
  { N_("GNU General Public License, version 2 or later"), "https://www.gnu.org/licenses/old-licenses/gpl-2.0.html" },
  { N_("GNU General Public License, version 3 or later"), "https://www.gnu.org/licenses/gpl-3.0.html" },
  { N_("GNU Lesser General Public License, version 2.1 or later"), "https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html" },
  { N_("GNU Lesser General Public License, version 3 or later"), "https://www.gnu.org/licenses/lgpl-3.0.html" },
  { N_("BSD 2-Clause License"), "https://opensource.org/licenses/bsd-license.php" },
  { N_("The MIT License (MIT)"), "https://opensource.org/licenses/mit-license.php" },
  { N_("Artistic License 2.0"), "https://opensource.org/licenses/artistic-license-2.0.php" },
  { N_("GNU General Public License, version 2 only"), "https://www.gnu.org/licenses/old-licenses/gpl-2.0.html" },
  { N_("GNU General Public License, version 3 only"), "https://www.gnu.org/licenses/gpl-3.0.html" },
  { N_("GNU Lesser General Public License, version 2.1 only"), "https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html" },
  { N_("GNU Lesser General Public License, version 3 only"), "https://www.gnu.org/licenses/lgpl-3.0.html" },
  { N_("GNU Affero General Public License, version 3 or later"), "https://www.gnu.org/licenses/agpl-3.0.html" },
  { N_("GNU Affero General Public License, version 3 only"), "https://www.gnu.org/licenses/agpl-3.0.html" },
  { N_("BSD 3-Clause License"), "https://opensource.org/licenses/BSD-3-Clause" },
  { N_("Apache License, Version 2.0"), "https://opensource.org/licenses/Apache-2.0" },
  { N_("Mozilla Public License 2.0"), "https://opensource.org/licenses/MPL-2.0" },
  { N_("BSD Zero-Clause License"), "https://opensource.org/license/0bsd" }
};
/* Keep this static assertion updated with the last element of the
 * enumeration, and make sure it matches the last element of the array */
G_STATIC_ASSERT (G_N_ELEMENTS (gtk_license_info) - 1 == GTK_LICENSE_0BSD);

typedef struct
{
  char *heading;
  char **people;
} CreditSection;

typedef struct _GtkAboutDialogClass GtkAboutDialogClass;

struct _GtkAboutDialog
{
  GtkWindow parent_instance;

  char *name;
  char *version;
  char *copyright;
  char *comments;
  char *website_url;
  char *website_text;
  char *translator_credits;
  char *license;
  char *system_information;

  char **authors;
  char **documenters;
  char **artists;

  GSList *credit_sections;

  gboolean credits_page_initialized;
  gboolean license_page_initialized;
  gboolean system_page_initialized;

  GtkWidget *stack;
  GtkWidget *stack_switcher;

  GtkWidget *logo_image;
  GtkWidget *name_label;
  GtkWidget *version_label;
  GtkWidget *comments_label;
  GtkWidget *copyright_label;
  GtkWidget *license_label;
  GtkWidget *website_label;

  GtkWidget *credits_page;
  GtkWidget *license_page;
  GtkWidget *system_page;

  GtkWidget *credits_grid;
  GtkWidget *license_view;
  GtkWidget *system_view;

  GPtrArray *visited_links;

  GtkCssNode *link_node;
  GtkCssNode *visited_link_node;

  GtkLicense license_type;

  guint hovering_over_link : 1;
  guint wrap_license : 1;
  guint in_child_changed : 1;
};

struct _GtkAboutDialogClass
{
  GtkWindowClass parent_class;

  gboolean (*activate_link) (GtkAboutDialog *dialog,
                             const char     *uri);
};

enum
{
  PROP_0,
  PROP_NAME,
  PROP_VERSION,
  PROP_COPYRIGHT,
  PROP_COMMENTS,
  PROP_WEBSITE,
  PROP_WEBSITE_LABEL,
  PROP_LICENSE,
  PROP_SYSTEM_INFORMATION,
  PROP_AUTHORS,
  PROP_DOCUMENTERS,
  PROP_TRANSLATOR_CREDITS,
  PROP_ARTISTS,
  PROP_LOGO,
  PROP_LOGO_ICON_NAME,
  PROP_WRAP_LICENSE,
  PROP_LICENSE_TYPE,
  LAST_PROP
};

static void                 gtk_about_dialog_finalize       (GObject            *object);
static void                 gtk_about_dialog_get_property   (GObject            *object,
                                                             guint               prop_id,
                                                             GValue             *value,
                                                             GParamSpec         *pspec);
static void                 gtk_about_dialog_set_property   (GObject            *object,
                                                             guint               prop_id,
                                                             const GValue       *value,
                                                             GParamSpec         *pspec);
static void                 update_name_version             (GtkAboutDialog     *about);
static void                 follow_if_link                  (GtkAboutDialog     *about,
                                                             GtkTextView        *text_view,
                                                             GtkTextIter        *iter);
static void                 set_cursor_if_appropriate       (GtkAboutDialog     *about,
                                                             GtkTextView        *text_view,
                                                             int                 x,
                                                             int                 y);
static void                 populate_credits_page           (GtkAboutDialog     *about);
static void                 populate_license_page           (GtkAboutDialog     *about);
static void                 populate_system_page            (GtkAboutDialog     *about);
static gboolean             gtk_about_dialog_activate_link  (GtkAboutDialog     *about,
                                                             const char         *uri);
static gboolean             emit_activate_link              (GtkAboutDialog     *about,
							     const char         *uri);
static gboolean             text_view_key_pressed           (GtkEventController *controller,
                                                             guint               keyval,
                                                             guint               keycode,
                                                             GdkModifierType     state,
							     GtkAboutDialog     *about);
static void                 text_view_released              (GtkGestureClick *press,
                                                             int                   n,
                                                             double                x,
                                                             double                y,
							     GtkAboutDialog       *about);
static void                 text_view_motion                (GtkEventControllerMotion *motion,
                                                             double                    x,
                                                             double                    y,
							     GtkAboutDialog           *about);

enum {
  ACTIVATE_LINK,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *props[LAST_PROP] = { NULL, };

G_DEFINE_TYPE (GtkAboutDialog, gtk_about_dialog, GTK_TYPE_WINDOW)

static gboolean
stack_visible_child_notify (GtkStack       *stack,
                            GParamSpec     *pspec,
                            GtkAboutDialog *about)
{
  GtkWidget *child;

  child = gtk_stack_get_visible_child (stack);
  if (child == about->credits_page)
    {
      if (!about->credits_page_initialized)
        {
          populate_credits_page (about);
          about->credits_page_initialized = TRUE;
        }
    }
  else if (child == about->license_page)
    {
      if (!about->license_page_initialized)
        {
          populate_license_page (about);
          about->license_page_initialized = TRUE;
        }
    }
  else if (child == about->system_page)
    {
      if (!about->system_page_initialized)
        {
          populate_system_page (about);
          about->system_page_initialized = TRUE;
        }
    }

  return FALSE;
}

static void
gtk_about_dialog_map (GtkWidget *widget)
{
  GtkAboutDialog *about = GTK_ABOUT_DIALOG (widget);

  if (gtk_widget_get_visible (about->stack_switcher))
    gtk_widget_grab_focus (gtk_widget_get_first_child (about->stack_switcher));

  GTK_WIDGET_CLASS (gtk_about_dialog_parent_class)->map (widget);
}

static void
gtk_about_dialog_class_init (GtkAboutDialogClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;

  object_class->set_property = gtk_about_dialog_set_property;
  object_class->get_property = gtk_about_dialog_get_property;

  object_class->finalize = gtk_about_dialog_finalize;

  widget_class->map = gtk_about_dialog_map;

  klass->activate_link = gtk_about_dialog_activate_link;

  /**
   * GtkAboutDialog::activate-link:
   * @label: The object on which the signal was emitted
   * @uri: the URI that is activated
   *
   * Emitted every time a URL is activated.
   *
   * Applications may connect to it to override the default behaviour,
   * which is to call [method@Gtk.FileLauncher.launch].
   *
   * Returns: `TRUE` if the link has been activated
   */
  signals[ACTIVATE_LINK] =
    g_signal_new (I_("activate-link"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkAboutDialogClass, activate_link),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__STRING,
                  G_TYPE_BOOLEAN, 1, G_TYPE_STRING);
  g_signal_set_va_marshaller (signals[ACTIVATE_LINK],
                              G_TYPE_FROM_CLASS (object_class),
                              _gtk_marshal_BOOLEAN__STRINGv);

  /**
   * GtkAboutDialog:program-name: (attributes org.gtk.Property.get=gtk_about_dialog_get_program_name org.gtk.Property.set=gtk_about_dialog_set_program_name)
   *
   * The name of the program.
   *
   * If this is not set, it defaults to the value returned by
   * `g_get_application_name()`.
   */
  props[PROP_NAME] =
    g_param_spec_string ("program-name", NULL, NULL,
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:version: (attributes org.gtk.Property.get=gtk_about_dialog_get_version org.gtk.Property.set=gtk_about_dialog_set_version)
   *
   * The version of the program.
   */
  props[PROP_VERSION] =
    g_param_spec_string ("version", NULL, NULL,
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:copyright: (attributes org.gtk.Property.get=gtk_about_dialog_get_copyright org.gtk.Property.set=gtk_about_dialog_set_copyright)
   *
   * Copyright information for the program.
   */
  props[PROP_COPYRIGHT] =
    g_param_spec_string ("copyright", NULL, NULL,
                        NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:comments: (attributes org.gtk.Property.get=gtk_about_dialog_get_comments org.gtk.Property.set=gtk_about_dialog_set_comments)
   *
   * Comments about the program.
   *
   * This string is displayed in a label in the main dialog, thus it
   * should be a short explanation of the main purpose of the program,
   * not a detailed list of features.
   */
  props[PROP_COMMENTS] =
    g_param_spec_string ("comments", NULL, NULL,
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:license: (attributes org.gtk.Property.get=gtk_about_dialog_get_license org.gtk.Property.set=gtk_about_dialog_set_license)
   *
   * The license of the program, as free-form text.
   *
   * This string is displayed in a text view in a secondary dialog, therefore
   * it is fine to use a long multi-paragraph text. Note that the text is only
   * wrapped in the text view if the "wrap-license" property is set to `TRUE`;
   * otherwise the text itself must contain the intended linebreaks.
   *
   * When setting this property to a non-`NULL` value, the
   * [property@Gtk.AboutDialog:license-type] property is set to
   * `GTK_LICENSE_CUSTOM` as a side effect.
   *
   * The text may contain links in this format `<http://www.some.place/>`
   * and email references in the form `<mail-to@some.body>`, and these will
   * be converted into clickable links.
   */
  props[PROP_LICENSE] =
    g_param_spec_string ("license", NULL, NULL,
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:system-information: (attributes org.gtk.Property.get=gtk_about_dialog_get_system_information org.gtk.Property.set=gtk_about_dialog_set_system_information)
   *
   * Information about the system on which the program is running.
   *
   * This information is displayed in a separate page, therefore it is fine
   * to use a long multi-paragraph text. Note that the text should contain
   * the intended linebreaks.
   *
   * The text may contain links in this format `<http://www.some.place/>`
   * and email references in the form `<mail-to@some.body>`, and these will
   * be converted into clickable links.
   */
  props[PROP_SYSTEM_INFORMATION] =
    g_param_spec_string ("system-information", NULL, NULL,
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:license-type: (attributes org.gtk.Property.get=gtk_about_dialog_get_license_type org.gtk.Property.set=gtk_about_dialog_set_license_type)
   *
   * The license of the program.
   *
   * The `GtkAboutDialog` will automatically fill out a standard disclaimer
   * and link the user to the appropriate online resource for the license
   * text.
   *
   * If `GTK_LICENSE_UNKNOWN` is used, the link used will be the same
   * specified in the [property@Gtk.AboutDialog:website] property.
   *
   * If `GTK_LICENSE_CUSTOM` is used, the current contents of the
   * [property@Gtk.AboutDialog:license] property are used.
   *
   * For any other [enum@Gtk.License] value, the contents of the
   * [property@Gtk.AboutDialog:license] property are also set by this property as
   * a side effect.
   */
  props[PROP_LICENSE_TYPE] =
    g_param_spec_enum ("license-type", NULL, NULL,
                       GTK_TYPE_LICENSE,
                       GTK_LICENSE_UNKNOWN,
                       GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:website: (attributes org.gtk.Property.get=gtk_about_dialog_get_website org.gtk.Property.set=gtk_about_dialog_set_website)
   *
   * The URL for the link to the website of the program.
   *
   * This should be a string starting with `http://` or `https://`.
   */
  props[PROP_WEBSITE] =
    g_param_spec_string ("website", NULL, NULL,
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:website-label: (attributes org.gtk.Property.get=gtk_about_dialog_get_website_label org.gtk.Property.set=gtk_about_dialog_set_website_label)
   *
   * The label for the link to the website of the program.
   */
  props[PROP_WEBSITE_LABEL] =
    g_param_spec_string ("website-label", NULL, NULL,
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:authors: (attributes org.gtk.Property.get=gtk_about_dialog_get_authors org.gtk.Property.set=gtk_about_dialog_set_authors)
   *
   * The authors of the program, as a `NULL`-terminated array of strings.
   *
   * Each string may contain email addresses and URLs, which will be displayed
   * as links, see the introduction for more details.
   */
  props[PROP_AUTHORS] =
    g_param_spec_boxed ("authors", NULL, NULL,
                        G_TYPE_STRV,
                        GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:documenters: (attributes org.gtk.Property.get=gtk_about_dialog_get_documenters org.gtk.Property.set=gtk_about_dialog_set_documenters)
   *
   * The people documenting the program, as a `NULL`-terminated array of strings.
   *
   * Each string may contain email addresses and URLs, which will be displayed
   * as links, see the introduction for more details.
   */
  props[PROP_DOCUMENTERS] =
    g_param_spec_boxed ("documenters", NULL, NULL,
                        G_TYPE_STRV,
                        GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:artists: (attributes org.gtk.Property.get=gtk_about_dialog_get_artists org.gtk.Property.set=gtk_about_dialog_set_artists)
   *
   * The people who contributed artwork to the program, as a `NULL`-terminated
   * array of strings.
   *
   * Each string may contain email addresses and URLs, which will be displayed
   * as links.
   */
  props[PROP_ARTISTS] =
    g_param_spec_boxed ("artists", NULL, NULL,
                        G_TYPE_STRV,
                        GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:translator-credits: (attributes org.gtk.Property.get=gtk_about_dialog_get_translator_credits org.gtk.Property.set=gtk_about_dialog_set_translator_credits)
   *
   * Credits to the translators.
   *
   * This string should be marked as translatable.
   *
   * The string may contain email addresses and URLs, which will be displayed
   * as links, see the introduction for more details.
   */
  props[PROP_TRANSLATOR_CREDITS] =
    g_param_spec_string ("translator-credits", NULL, NULL,
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:logo: (attributes org.gtk.Property.get=gtk_about_dialog_get_logo org.gtk.Property.set=gtk_about_dialog_set_logo)
   *
   * A logo for the about box.
   *
   * If it is `NULL`, the default window icon set with
   * [func@Gtk.Window.set_default_icon_name] will be used.
   */
  props[PROP_LOGO] =
    g_param_spec_object ("logo", NULL, NULL,
                         GDK_TYPE_PAINTABLE,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:logo-icon-name: (attributes org.gtk.Property.get=gtk_about_dialog_get_logo_icon_name org.gtk.Property.set=gtk_about_dialog_set_logo_icon_name)
   *
   * A named icon to use as the logo for the about box.
   *
   * This property overrides the [property@Gtk.AboutDialog:logo] property.
   */
  props[PROP_LOGO_ICON_NAME] =
    g_param_spec_string ("logo-icon-name", NULL, NULL,
                         NULL,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkAboutDialog:wrap-license: (attributes org.gtk.Property.get=gtk_about_dialog_get_wrap_license org.gtk.Property.set=gtk_about_dialog_set_wrap_license)
   *
   * Whether to wrap the text in the license dialog.
   */
  props[PROP_WRAP_LICENSE] =
    g_param_spec_boolean ("wrap-license", NULL, NULL,
                          FALSE,
                          GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  /*
   * Key bindings
   */

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/ui/gtkaboutdialog.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkAboutDialog, stack);
  gtk_widget_class_bind_template_child (widget_class, GtkAboutDialog, stack_switcher);
  gtk_widget_class_bind_template_child (widget_class, GtkAboutDialog, logo_image);
  gtk_widget_class_bind_template_child (widget_class, GtkAboutDialog, name_label);
  gtk_widget_class_bind_template_child (widget_class, GtkAboutDialog, version_label);
  gtk_widget_class_bind_template_child (widget_class, GtkAboutDialog, comments_label);
  gtk_widget_class_bind_template_child (widget_class, GtkAboutDialog, copyright_label);
  gtk_widget_class_bind_template_child (widget_class, GtkAboutDialog, license_label);
  gtk_widget_class_bind_template_child (widget_class, GtkAboutDialog, website_label);
  gtk_widget_class_bind_template_child (widget_class, GtkAboutDialog, credits_page);
  gtk_widget_class_bind_template_child (widget_class, GtkAboutDialog, license_page);
  gtk_widget_class_bind_template_child (widget_class, GtkAboutDialog, system_page);
  gtk_widget_class_bind_template_child (widget_class, GtkAboutDialog, credits_grid);
  gtk_widget_class_bind_template_child (widget_class, GtkAboutDialog, license_view);
  gtk_widget_class_bind_template_child (widget_class, GtkAboutDialog, system_view);

  gtk_widget_class_bind_template_callback (widget_class, emit_activate_link);
  gtk_widget_class_bind_template_callback (widget_class, text_view_released);
  gtk_widget_class_bind_template_callback (widget_class, text_view_motion);
  gtk_widget_class_bind_template_callback (widget_class, text_view_key_pressed);
  gtk_widget_class_bind_template_callback (widget_class, stack_visible_child_notify);
}

static gboolean
emit_activate_link (GtkAboutDialog *about,
                    const char     *uri)
{
  gboolean handled = FALSE;

  g_signal_emit (about, signals[ACTIVATE_LINK], 0, uri, &handled);

  return TRUE;
}

static void
update_stack_switcher_visibility (GtkAboutDialog *about)
{
  GtkStackPage *page;
  gboolean any_visible = FALSE;

  page = gtk_stack_get_page (GTK_STACK (about->stack), about->credits_page);
  any_visible |= gtk_stack_page_get_visible (page);
  page = gtk_stack_get_page (GTK_STACK (about->stack), about->license_page);
  any_visible |= gtk_stack_page_get_visible (page);
  page = gtk_stack_get_page (GTK_STACK (about->stack), about->system_page);
  any_visible |= gtk_stack_page_get_visible (page);

  gtk_widget_set_visible (about->stack_switcher, any_visible);
}

static void
update_license_button_visibility (GtkAboutDialog *about)
{
  GtkStackPage *page;

  page = gtk_stack_get_page (GTK_STACK (about->stack), about->license_page);
  gtk_stack_page_set_visible (page,
                              about->license_type == GTK_LICENSE_CUSTOM &&
                              about->license != NULL &&
                              about->license[0] != '\0');

  update_stack_switcher_visibility (about);
}

static void
update_system_button_visibility (GtkAboutDialog *about)
{
  GtkStackPage *page;

  page = gtk_stack_get_page (GTK_STACK (about->stack), about->system_page);
  gtk_stack_page_set_visible (page,
                              about->system_information != NULL &&
                              about->system_information[0] != '\0');

  update_stack_switcher_visibility (about);
}

static void
update_credits_button_visibility (GtkAboutDialog *about)
{
  gboolean show;
  GtkStackPage *page;

  page = gtk_stack_get_page (GTK_STACK (about->stack), about->credits_page);

  show = (about->authors != NULL ||
          about->documenters != NULL ||
          about->artists != NULL ||
          about->credit_sections != NULL ||
          (about->translator_credits != NULL &&
           strcmp (about->translator_credits, "translator_credits") &&
           strcmp (about->translator_credits, "translator-credits")));
  gtk_stack_page_set_visible (page, show);

  update_stack_switcher_visibility (about);
}

static void
gtk_about_dialog_init (GtkAboutDialog *about)
{
  GtkCssNode *node;
  GtkStateFlags state;

  /* Data */
  about->name = NULL;
  about->version = NULL;
  about->copyright = NULL;
  about->comments = NULL;
  about->website_url = NULL;
  about->website_text = NULL;
  about->translator_credits = NULL;
  about->license = NULL;
  about->authors = NULL;
  about->documenters = NULL;
  about->artists = NULL;

  about->hovering_over_link = FALSE;
  about->wrap_license = FALSE;

  about->license_type = GTK_LICENSE_UNKNOWN;

  about->visited_links = g_ptr_array_new_with_free_func (g_free);

  gtk_widget_init_template (GTK_WIDGET (about));

  gtk_stack_set_visible_child_name (GTK_STACK (about->stack), "main");
  update_stack_switcher_visibility (about);

  /* force defaults */
  gtk_about_dialog_set_program_name (about, NULL);
  gtk_about_dialog_set_logo (about, NULL);

  node = gtk_widget_get_css_node (GTK_WIDGET (about));
  state = gtk_css_node_get_state (node);

  about->link_node = gtk_css_node_new ();
  gtk_css_node_set_name (about->link_node, g_quark_from_static_string ("link"));
  gtk_css_node_set_parent (about->link_node, node);
  gtk_css_node_set_state (about->link_node, state | GTK_STATE_FLAG_LINK);
  g_object_unref (about->link_node);

  about->visited_link_node = gtk_css_node_new ();
  gtk_css_node_set_name (about->visited_link_node, g_quark_from_static_string ("link"));
  gtk_css_node_set_parent (about->visited_link_node, node);
  gtk_css_node_set_state (about->visited_link_node, state | GTK_STATE_FLAG_VISITED);
  g_object_unref (about->visited_link_node);
}

static void
destroy_credit_section (gpointer data)
{
  CreditSection *cs = data;
  g_free (cs->heading);
  g_strfreev (cs->people);
  g_free (data);
}

static void
gtk_about_dialog_finalize (GObject *object)
{
  GtkAboutDialog *about = GTK_ABOUT_DIALOG (object);

  g_free (about->name);
  g_free (about->version);
  g_free (about->copyright);
  g_free (about->comments);
  g_free (about->license);
  g_free (about->website_url);
  g_free (about->website_text);
  g_free (about->translator_credits);
  g_free (about->system_information);

  g_strfreev (about->authors);
  g_strfreev (about->documenters);
  g_strfreev (about->artists);

  g_slist_free_full (about->credit_sections, destroy_credit_section);
  g_ptr_array_unref (about->visited_links);

  G_OBJECT_CLASS (gtk_about_dialog_parent_class)->finalize (object);
}

static void
gtk_about_dialog_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkAboutDialog *about = GTK_ABOUT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_NAME:
      gtk_about_dialog_set_program_name (about, g_value_get_string (value));
      break;
    case PROP_VERSION:
      gtk_about_dialog_set_version (about, g_value_get_string (value));
      break;
    case PROP_COMMENTS:
      gtk_about_dialog_set_comments (about, g_value_get_string (value));
      break;
    case PROP_WEBSITE:
      gtk_about_dialog_set_website (about, g_value_get_string (value));
      break;
    case PROP_WEBSITE_LABEL:
      gtk_about_dialog_set_website_label (about, g_value_get_string (value));
      break;
    case PROP_LICENSE:
      gtk_about_dialog_set_license (about, g_value_get_string (value));
      break;
    case PROP_SYSTEM_INFORMATION:
      gtk_about_dialog_set_system_information (about, g_value_get_string (value));
      break;
    case PROP_LICENSE_TYPE:
      gtk_about_dialog_set_license_type (about, g_value_get_enum (value));
      break;
    case PROP_COPYRIGHT:
      gtk_about_dialog_set_copyright (about, g_value_get_string (value));
      break;
    case PROP_LOGO:
      gtk_about_dialog_set_logo (about, g_value_get_object (value));
      break;
    case PROP_AUTHORS:
      gtk_about_dialog_set_authors (about, (const char **)g_value_get_boxed (value));
      break;
    case PROP_DOCUMENTERS:
      gtk_about_dialog_set_documenters (about, (const char **)g_value_get_boxed (value));
      break;
    case PROP_ARTISTS:
      gtk_about_dialog_set_artists (about, (const char **)g_value_get_boxed (value));
      break;
    case PROP_TRANSLATOR_CREDITS:
      gtk_about_dialog_set_translator_credits (about, g_value_get_string (value));
      break;
    case PROP_LOGO_ICON_NAME:
      gtk_about_dialog_set_logo_icon_name (about, g_value_get_string (value));
      break;
    case PROP_WRAP_LICENSE:
      gtk_about_dialog_set_wrap_license (about, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_about_dialog_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkAboutDialog *about = GTK_ABOUT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, about->name);
      break;
    case PROP_VERSION:
      g_value_set_string (value, about->version);
      break;
    case PROP_COPYRIGHT:
      g_value_set_string (value, about->copyright);
      break;
    case PROP_COMMENTS:
      g_value_set_string (value, about->comments);
      break;
    case PROP_WEBSITE:
      g_value_set_string (value, about->website_url);
      break;
    case PROP_WEBSITE_LABEL:
      g_value_set_string (value, about->website_text);
      break;
    case PROP_LICENSE:
      g_value_set_string (value, about->license);
      break;
    case PROP_SYSTEM_INFORMATION:
      g_value_set_string (value, about->system_information);
      break;
    case PROP_LICENSE_TYPE:
      g_value_set_enum (value, about->license_type);
      break;
    case PROP_TRANSLATOR_CREDITS:
      g_value_set_string (value, about->translator_credits);
      break;
    case PROP_AUTHORS:
      g_value_set_boxed (value, about->authors);
      break;
    case PROP_DOCUMENTERS:
      g_value_set_boxed (value, about->documenters);
      break;
    case PROP_ARTISTS:
      g_value_set_boxed (value, about->artists);
      break;
    case PROP_LOGO:
      if (gtk_image_get_storage_type (GTK_IMAGE (about->logo_image)) == GTK_IMAGE_PAINTABLE)
        g_value_set_object (value, gtk_image_get_paintable (GTK_IMAGE (about->logo_image)));
      else
        g_value_set_object (value, NULL);
      break;
    case PROP_LOGO_ICON_NAME:
      if (gtk_image_get_storage_type (GTK_IMAGE (about->logo_image)) == GTK_IMAGE_ICON_NAME)
        g_value_set_string (value, gtk_image_get_icon_name (GTK_IMAGE (about->logo_image)));
      else
        g_value_set_string (value, NULL);
      break;
    case PROP_WRAP_LICENSE:
      g_value_set_boolean (value, about->wrap_license);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gtk_about_dialog_activate_link (GtkAboutDialog *about,
                                const char     *uri)
{
  GtkUriLauncher *launcher;

  launcher = gtk_uri_launcher_new (uri);

  gtk_uri_launcher_launch (launcher, GTK_WINDOW (about), NULL, NULL, NULL);

  g_object_unref (launcher);

  return TRUE;
}

static void
update_website (GtkAboutDialog *about)
{
  gtk_widget_set_visible (about->website_label, TRUE);

  if (about->website_url)
    {
      char *markup;

      if (about->website_text)
        {
          char *escaped;

          escaped = g_markup_escape_text (about->website_text, -1);
          markup = g_strdup_printf ("<a href=\"%s\">%s</a>",
                                    about->website_url, escaped);
          g_free (escaped);
        }
      else
        {
          markup = g_strdup_printf ("<a href=\"%s\">%s</a>",
                                    about->website_url, _("Website"));
        }

      gtk_label_set_markup (GTK_LABEL (about->website_label), markup);
      g_free (markup);
    }
  else
    {
      if (about->website_text)
        gtk_label_set_text (GTK_LABEL (about->website_label), about->website_text);
      else
        gtk_widget_set_visible (about->website_label, FALSE);
    }
}

/**
 * gtk_about_dialog_get_program_name: (attributes org.gtk.Method.get_property=program-name)
 * @about: a `GtkAboutDialog`
 *
 * Returns the program name displayed in the about dialog.
 *
 * Returns: (nullable): The program name
 */
const char *
gtk_about_dialog_get_program_name (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return about->name;
}

static void
update_name_version (GtkAboutDialog *about)
{
  char *title_string, *name_string;

  title_string = g_strdup_printf (_("About %s"), about->name);
  gtk_window_set_title (GTK_WINDOW (about), title_string);
  g_free (title_string);

  gtk_widget_set_visible (about->version_label, about->version != NULL);
  if (about->version != NULL)
    gtk_label_set_markup (GTK_LABEL (about->version_label), about->version);

  name_string = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>",
                                         about->name);
  gtk_label_set_markup (GTK_LABEL (about->name_label), name_string);
  g_free (name_string);
}

/**
 * gtk_about_dialog_set_program_name: (attributes org.gtk.Method.set_property=program-name)
 * @about: a `GtkAboutDialog`
 * @name: (nullable): the program name
 *
 * Sets the name to display in the about dialog.
 *
 * If `name` is not set, the string returned
 * by `g_get_application_name()` is used.
 */
void
gtk_about_dialog_set_program_name (GtkAboutDialog *about,
                                   const char     *name)
{
  char *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  tmp = about->name;
  about->name = g_strdup (name ? name : g_get_application_name ());
  g_free (tmp);

  update_name_version (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_NAME]);
}


/**
 * gtk_about_dialog_get_version: (attributes org.gtk.Method.get_property=version)
 * @about: a `GtkAboutDialog`
 *
 * Returns the version string.
 *
 * Returns: (nullable): The version string
 */
const char *
gtk_about_dialog_get_version (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return about->version;
}

/**
 * gtk_about_dialog_set_version: (attributes org.gtk.Method.set_property=version)
 * @about: a `GtkAboutDialog`
 * @version: (nullable): the version string
 *
 * Sets the version string to display in the about dialog.
 */
void
gtk_about_dialog_set_version (GtkAboutDialog *about,
                              const char     *version)
{
  char *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  tmp = about->version;
  about->version = g_strdup (version);
  g_free (tmp);

  update_name_version (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_VERSION]);
}

/**
 * gtk_about_dialog_get_copyright: (attributes org.gtk.Method.get_property=copyright)
 * @about: a `GtkAboutDialog`
 *
 * Returns the copyright string.
 *
 * Returns: (nullable): The copyright string
 */
const char *
gtk_about_dialog_get_copyright (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return about->copyright;
}

/**
 * gtk_about_dialog_set_copyright: (attributes org.gtk.Method.set_property=copyright)
 * @about: a `GtkAboutDialog`
 * @copyright: (nullable): the copyright string
 *
 * Sets the copyright string to display in the about dialog.
 *
 * This should be a short string of one or two lines.
 */
void
gtk_about_dialog_set_copyright (GtkAboutDialog *about,
                                const char     *copyright)
{
  char *copyright_string, *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  tmp = about->copyright;
  about->copyright = g_strdup (copyright);
  g_free (tmp);

  gtk_widget_set_visible (about->copyright_label, about->copyright != NULL);
  if (about->copyright != NULL)
    {
      copyright_string = g_markup_printf_escaped ("<span size=\"small\">%s</span>",
                                                  about->copyright);
      gtk_label_set_markup (GTK_LABEL (about->copyright_label), copyright_string);
      g_free (copyright_string);
    }

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_COPYRIGHT]);
}

/**
 * gtk_about_dialog_get_comments: (attributes org.gtk.Method.set_property=comments)
 * @about: a `GtkAboutDialog`
 *
 * Returns the comments string.
 *
 * Returns: (nullable): The comments
 */
const char *
gtk_about_dialog_get_comments (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return about->comments;
}

/**
 * gtk_about_dialog_set_comments: (attributes org.gtk.Method.set_property=comments)
 * @about: a `GtkAboutDialog`
 * @comments: (nullable): a comments string
 *
 * Sets the comments string to display in the about dialog.
 *
 * This should be a short string of one or two lines.
 */
void
gtk_about_dialog_set_comments (GtkAboutDialog *about,
                               const char     *comments)
{
  char *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  tmp = about->comments;
  if (comments)
    {
      about->comments = g_strdup (comments);
      gtk_label_set_text (GTK_LABEL (about->comments_label), about->comments);
    }
  else
    {
      about->comments = NULL;
    }

  gtk_widget_set_visible (about->comments_label, about->comments != NULL);
  g_free (tmp);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_COMMENTS]);
}

/**
 * gtk_about_dialog_get_license: (attributes org.gtk.Method.get_property=license)
 * @about: a `GtkAboutDialog`
 *
 * Returns the license information.
 *
 * Returns: (nullable): The license information
 */
const char *
gtk_about_dialog_get_license (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return about->license;
}

/**
 * gtk_about_dialog_set_license: (attributes org.gtk.Method.set_property=license)
 * @about: a `GtkAboutDialog`
 * @license: (nullable): the license information
 *
 * Sets the license information to be displayed in the
 * about dialog.
 *
 * If `license` is `NULL`, the license page is hidden.
 */
void
gtk_about_dialog_set_license (GtkAboutDialog *about,
                              const char     *license)
{
  char *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  tmp = about->license;
  if (license)
    {
      about->license = g_strdup (license);
      about->license_type = GTK_LICENSE_CUSTOM;
    }
  else
    {
      about->license = NULL;
      about->license_type = GTK_LICENSE_UNKNOWN;
    }
  g_free (tmp);

  gtk_widget_set_visible (about->license_label, FALSE);

  update_license_button_visibility (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LICENSE]);
  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LICENSE_TYPE]);
}

/**
 * gtk_about_dialog_get_system_information: (attributes org.gtk.Method.get_property=system-information)
 * @about: a `GtkAboutDialog`
 *
 * Returns the system information that is shown in the about dialog.
 *
 * Returns: (nullable): the system information
 */
const char *
gtk_about_dialog_get_system_information (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return about->system_information;
}

/**
 * gtk_about_dialog_set_system_information: (attributes org.gtk.Method.set_property=system-information)
 * @about: a `GtkAboutDialog`
 * @system_information: (nullable): system information
 *
 * Sets the system information to be displayed in the about
 * dialog.
 *
 * If `system_information` is `NULL`, the system information
 * page is hidden.
 *
 * See [property@Gtk.AboutDialog:system-information].
 */
void
gtk_about_dialog_set_system_information (GtkAboutDialog *about,
                                         const char     *system_information)
{
  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  g_free (about->system_information);
  about->system_information = g_strdup (system_information);
  update_system_button_visibility (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_SYSTEM_INFORMATION]);
}

/**
 * gtk_about_dialog_get_wrap_license: (attributes org.gtk.Method.get_property=wrap-license)
 * @about: a `GtkAboutDialog`
 *
 * Returns whether the license text in the about dialog is
 * automatically wrapped.
 *
 * Returns: `TRUE` if the license text is wrapped
 */
gboolean
gtk_about_dialog_get_wrap_license (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), FALSE);

  return about->wrap_license;
}

/**
 * gtk_about_dialog_set_wrap_license: (attributes org.gtk.Method.set_property=wrap-license)
 * @about: a `GtkAboutDialog`
 * @wrap_license: whether to wrap the license
 *
 * Sets whether the license text in the about dialog should be
 * automatically wrapped.
 */
void
gtk_about_dialog_set_wrap_license (GtkAboutDialog *about,
                                   gboolean        wrap_license)
{
  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  wrap_license = wrap_license != FALSE;

  if (about->wrap_license != wrap_license)
    {
       about->wrap_license = wrap_license;

       g_object_notify_by_pspec (G_OBJECT (about), props[PROP_WRAP_LICENSE]);
    }
}

/**
 * gtk_about_dialog_get_website: (attributes org.gtk.Method.get_property=website)
 * @about: a `GtkAboutDialog`
 *
 * Returns the website URL.
 *
 * Returns: (nullable) (transfer none): The website URL
 */
const char *
gtk_about_dialog_get_website (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return about->website_url;
}

/**
 * gtk_about_dialog_set_website: (attributes org.gtk.Method.set_property=website)
 * @about: a `GtkAboutDialog`
 * @website: (nullable): a URL string starting with `http://`
 *
 * Sets the URL to use for the website link.
 */
void
gtk_about_dialog_set_website (GtkAboutDialog *about,
                              const char     *website)
{
  char *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  tmp = about->website_url;
  about->website_url = g_strdup (website);
  g_free (tmp);

  update_website (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_WEBSITE]);
}

/**
 * gtk_about_dialog_get_website_label: (attributes org.gtk.Method.get_property=website-label)
 * @about: a `GtkAboutDialog`
 *
 * Returns the label used for the website link.
 *
 * Returns: (nullable) (transfer none): The label used for the website link
 */
const char *
gtk_about_dialog_get_website_label (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return about->website_text;
}

/**
 * gtk_about_dialog_set_website_label: (attributes org.gtk.Method.set_property=website-label)
 * @about: a `GtkAboutDialog`
 * @website_label: the label used for the website link
 *
 * Sets the label to be used for the website link.
 */
void
gtk_about_dialog_set_website_label (GtkAboutDialog *about,
                                    const char     *website_label)
{
  char *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  tmp = about->website_text;
  about->website_text = g_strdup (website_label);
  g_free (tmp);

  update_website (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_WEBSITE_LABEL]);
}

/**
 * gtk_about_dialog_get_authors: (attributes org.gtk.Method.get_property=authors)
 * @about: a `GtkAboutDialog`
 *
 * Returns the names of the authors which are displayed
 * in the credits page.
 *
 * Returns: (array zero-terminated=1) (transfer none): A
 *   `NULL`-terminated string array containing the authors
 */
const char * const *
gtk_about_dialog_get_authors (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return (const char * const *) about->authors;
}

/**
 * gtk_about_dialog_set_authors: (attributes org.gtk.Method.set_property=authors)
 * @about: a `GtkAboutDialog`
 * @authors: (array zero-terminated=1): the authors of the application
 *
 * Sets the names of the authors which are displayed
 * in the "Credits" page of the about dialog.
 */
void
gtk_about_dialog_set_authors (GtkAboutDialog  *about,
                              const char     **authors)
{
  char **tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  tmp = about->authors;
  about->authors = g_strdupv ((char **)authors);
  g_strfreev (tmp);

  update_credits_button_visibility (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_AUTHORS]);
}

/**
 * gtk_about_dialog_get_documenters: (attributes org.gtk.Method.get_property=documenters)
 * @about: a `GtkAboutDialog`
 *
 * Returns the name of the documenters which are displayed
 * in the credits page.
 *
 * Returns: (array zero-terminated=1) (transfer none): A
 *   `NULL`-terminated string array containing the documenters
 */
const char * const *
gtk_about_dialog_get_documenters (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return (const char * const *)about->documenters;
}

/**
 * gtk_about_dialog_set_documenters: (attributes org.gtk.Method.set_property=documenters)
 * @about: a `GtkAboutDialog`
 * @documenters: (array zero-terminated=1): the authors of the documentation
 *   of the application
 *
 * Sets the names of the documenters which are displayed
 * in the "Credits" page.
 */
void
gtk_about_dialog_set_documenters (GtkAboutDialog *about,
                                  const char    **documenters)
{
  char **tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  tmp = about->documenters;
  about->documenters = g_strdupv ((char **)documenters);
  g_strfreev (tmp);

  update_credits_button_visibility (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_DOCUMENTERS]);
}

/**
 * gtk_about_dialog_get_artists: (attributes org.gtk.Method.get_property=artists)
 * @about: a `GtkAboutDialog`
 *
 * Returns the names of the artists which are displayed
 * in the credits page.
 *
 * Returns: (array zero-terminated=1) (transfer none): A
 *   `NULL`-terminated string array containing the artists
 */
const char * const *
gtk_about_dialog_get_artists (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return (const char * const *)about->artists;
}

/**
 * gtk_about_dialog_set_artists: (attributes org.gtk.Method.set_property=artists)
 * @about: a `GtkAboutDialog`
 * @artists: (array zero-terminated=1): the authors of the artwork
 *   of the application
 *
 * Sets the names of the artists to be displayed
 * in the "Credits" page.
 */
void
gtk_about_dialog_set_artists (GtkAboutDialog *about,
                              const char    **artists)
{
  char **tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  tmp = about->artists;
  about->artists = g_strdupv ((char **)artists);
  g_strfreev (tmp);

  update_credits_button_visibility (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_ARTISTS]);
}

/**
 * gtk_about_dialog_get_translator_credits: (attributes org.gtk.Method.get_property=translator-credits)
 * @about: a `GtkAboutDialog`
 *
 * Returns the translator credits string which is displayed
 * in the credits page.
 *
 * Returns: (nullable): The translator credits string
 */
const char *
gtk_about_dialog_get_translator_credits (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  return about->translator_credits;
}

/**
 * gtk_about_dialog_set_translator_credits: (attributes org.gtk.Method.set_property=translator-credits)
 * @about: a `GtkAboutDialog`
 * @translator_credits: (nullable): the translator credits
 *
 * Sets the translator credits string which is displayed in
 * the credits page.
 *
 * The intended use for this string is to display the translator
 * of the language which is currently used in the user interface.
 * Using `gettext()`, a simple way to achieve that is to mark the
 * string for translation:
 *
 * ```c
 * GtkWidget *about = gtk_about_dialog_new ();
 *  gtk_about_dialog_set_translator_credits (GTK_ABOUT_DIALOG (about),
 *                                           _("translator-credits"));
 * ```
 *
 * It is a good idea to use the customary `msgid` “translator-credits”
 * for this purpose, since translators will already know the purpose of
 * that `msgid`, and since `GtkAboutDialog` will detect if “translator-credits”
 * is untranslated and omit translator credits.
 */
void
gtk_about_dialog_set_translator_credits (GtkAboutDialog *about,
                                         const char     *translator_credits)
{
  char *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  tmp = about->translator_credits;
  about->translator_credits = g_strdup (translator_credits);
  g_free (tmp);

  update_credits_button_visibility (about);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_TRANSLATOR_CREDITS]);
}

/**
 * gtk_about_dialog_get_logo: (attributes org.gtk.Method.get_property=logo)
 * @about: a `GtkAboutDialog`
 *
 * Returns the paintable displayed as logo in the about dialog.
 *
 * Returns: (transfer none) (nullable): the paintable displayed as
 *   logo or `NULL` if the logo is unset or has been set via
 *   [method@Gtk.AboutDialog.set_logo_icon_name]
 */
GdkPaintable *
gtk_about_dialog_get_logo (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  if (gtk_image_get_storage_type (GTK_IMAGE (about->logo_image)) == GTK_IMAGE_PAINTABLE)
    return gtk_image_get_paintable (GTK_IMAGE (about->logo_image));
  else
    return NULL;
}

/**
 * gtk_about_dialog_set_logo: (attributes org.gtk.Method.set_property=logo)
 * @about: a `GtkAboutDialog`
 * @logo: (nullable): a `GdkPaintable`
 *
 * Sets the logo in the about dialog.
 */
void
gtk_about_dialog_set_logo (GtkAboutDialog *about,
                           GdkPaintable   *logo)
{
  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));
  g_return_if_fail (logo == NULL || GDK_IS_PAINTABLE (logo));

  g_object_freeze_notify (G_OBJECT (about));

  if (gtk_image_get_storage_type (GTK_IMAGE (about->logo_image)) == GTK_IMAGE_ICON_NAME)
    g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LOGO_ICON_NAME]);

  gtk_image_set_from_paintable (GTK_IMAGE (about->logo_image), logo);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LOGO]);

  g_object_thaw_notify (G_OBJECT (about));
}

/**
 * gtk_about_dialog_get_logo_icon_name: (attributes org.gtk.Method.get_property=logo-icon-name)
 * @about: a `GtkAboutDialog`
 *
 * Returns the icon name displayed as logo in the about dialog.
 *
 * Returns: (transfer none) (nullable): the icon name displayed as logo,
 *   or `NULL` if the logo has been set via [method@Gtk.AboutDialog.set_logo]
 */
const char *
gtk_about_dialog_get_logo_icon_name (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  if (gtk_image_get_storage_type (GTK_IMAGE (about->logo_image)) != GTK_IMAGE_ICON_NAME)
    return NULL;

  return gtk_image_get_icon_name (GTK_IMAGE (about->logo_image));
}

/**
 * gtk_about_dialog_set_logo_icon_name: (attributes org.gtk.Method.set_property=logo-icon-name)
 * @about: a `GtkAboutDialog`
 * @icon_name: (nullable): an icon name
 *
 * Sets the icon name to be displayed as logo in the about dialog.
 */
void
gtk_about_dialog_set_logo_icon_name (GtkAboutDialog *about,
                                     const char     *icon_name)
{
  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  g_object_freeze_notify (G_OBJECT (about));

  if (gtk_image_get_storage_type (GTK_IMAGE (about->logo_image)) == GTK_IMAGE_PAINTABLE)
    g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LOGO]);

  gtk_image_set_from_icon_name (GTK_IMAGE (about->logo_image), icon_name);

  g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LOGO_ICON_NAME]);

  g_object_thaw_notify (G_OBJECT (about));
}

static void
follow_if_link (GtkAboutDialog *about,
                GtkTextView    *text_view,
                GtkTextIter    *iter)
{
  GSList *tags = NULL, *tagp = NULL;
  char *uri = NULL;

  tags = gtk_text_iter_get_tags (iter);
  for (tagp = tags; tagp != NULL && !uri; tagp = tagp->next)
    {
      GtkTextTag *tag = tagp->data;

      uri = g_object_get_data (G_OBJECT (tag), "uri");
      if (uri)
        emit_activate_link (about, uri);

      if (uri && !g_ptr_array_find_with_equal_func (about->visited_links, uri, (GCompareFunc)strcmp, NULL))
        {
          GdkRGBA visited_link_color;
          GtkCssStyle *style;

          style = gtk_css_node_get_style (about->visited_link_node);
          visited_link_color = *gtk_css_color_value_get_rgba (style->core->color);
          g_object_set (G_OBJECT (tag), "foreground-rgba", &visited_link_color, NULL);

          g_ptr_array_add (about->visited_links, g_strdup (uri));
        }
    }

  g_slist_free (tags);
}

static gboolean
text_view_key_pressed (GtkEventController *controller,
                       guint               keyval,
                       guint               keycode,
                       GdkModifierType     state,
                       GtkAboutDialog     *about)
{
  GtkWidget *text_view;
  GtkTextIter iter;
  GtkTextBuffer *buffer;

  text_view = gtk_event_controller_get_widget (controller);

  switch (keyval)
    {
      case GDK_KEY_Return:
      case GDK_KEY_ISO_Enter:
      case GDK_KEY_KP_Enter:
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
        gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                          gtk_text_buffer_get_insert (buffer));
        follow_if_link (about, GTK_TEXT_VIEW (text_view), &iter);
        break;

      default:
        break;
    }

  return FALSE;
}

static void
text_view_released (GtkGestureClick *gesture,
                    int                   n_press,
                    double                x,
                    double                y,
                    GtkAboutDialog       *about)
{
  GtkWidget *text_view;
  GtkTextIter start, end, iter;
  GtkTextBuffer *buffer;
  int tx, ty;

  if (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture)) != GDK_BUTTON_PRIMARY)
    return;

  text_view = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));

  /* we shouldn't follow a link if the user has selected something */
  gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
  if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end))
    return;

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         x, y, &tx, &ty);

  gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (text_view), &iter, tx, ty);

  follow_if_link (about, GTK_TEXT_VIEW (text_view), &iter);
}

static void
set_cursor_if_appropriate (GtkAboutDialog *about,
                           GtkTextView    *text_view,
                           int             x,
                           int             y)
{
  GSList *tags = NULL, *tagp = NULL;
  GtkTextIter iter;
  gboolean hovering_over_link = FALSE;

  gtk_text_view_get_iter_at_location (text_view, &iter, x, y);

  tags = gtk_text_iter_get_tags (&iter);
  for (tagp = tags;  tagp != NULL;  tagp = tagp->next)
    {
      GtkTextTag *tag = tagp->data;
      char *uri = g_object_get_data (G_OBJECT (tag), "uri");

      if (uri != NULL)
        {
          hovering_over_link = TRUE;
          break;
        }
    }

  if (hovering_over_link != about->hovering_over_link)
    {
      about->hovering_over_link = hovering_over_link;

      if (hovering_over_link)
        gtk_widget_set_cursor_from_name (GTK_WIDGET (text_view), "pointer");
      else
        gtk_widget_set_cursor_from_name (GTK_WIDGET (text_view), "text");
    }

  g_slist_free (tags);
}

static void
text_view_motion (GtkEventControllerMotion *motion,
                  double                    x,
                  double                    y,
                  GtkAboutDialog           *about)
{
  int tx, ty;
  GtkWidget *widget;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (motion));

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (widget),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         x, y, &tx, &ty);

  set_cursor_if_appropriate (about, GTK_TEXT_VIEW (widget), tx, ty);
}

static GtkTextBuffer *
text_buffer_new (GtkAboutDialog  *about,
		 char           **strings)
{
  char **p;
  char *q0, *q1, *q2, *r1, *r2;
  GtkTextBuffer *buffer;
  GdkRGBA color;
  GdkRGBA link_color;
  GdkRGBA visited_link_color;
  GtkTextIter start_iter, end_iter;
  GtkTextTag *tag;
  GtkCssStyle *style;

  style = gtk_css_node_get_style (about->link_node);
  link_color = *gtk_css_color_value_get_rgba (style->core->color);
  style = gtk_css_node_get_style (about->visited_link_node);
  visited_link_color = *gtk_css_color_value_get_rgba (style->core->color);

  buffer = gtk_text_buffer_new (NULL);

  for (p = strings; *p; p++)
    {
      q0  = *p;
      while (*q0)
        {
          q1 = strchr (q0, '<');
          q2 = q1 ? strchr (q1, '>') : NULL;
          r1 = strstr (q0, "http://");
          r2 = strstr (q0, "https://");
          if (!r1 || (r1 && r2 && r2 < r1))
            r1 = r2;
          if (r1)
            {
              r2 = strpbrk (r1, " \n\t>");
              if (!r2)
                r2 = strchr (r1, '\0');
            }
          else
            r2 = NULL;

          if (r1 && r2 && (!q1 || !q2 || (r1 <= q1 + 1)))
            {
              q1 = r1;
              q2 = r2;
            }

          if (q1 && q2)
            {
              GtkTextIter end;
              char *link;
              char *uri;
              const char *link_type;

              if (*q1 == '<')
                {
                  gtk_text_buffer_insert_at_cursor (buffer, q0, q1 - q0 + 1);
                  gtk_text_buffer_get_end_iter (buffer, &end);
                  q1++;
                  link_type = "email";
                }
              else
                {
                  gtk_text_buffer_insert_at_cursor (buffer, q0, q1 - q0);
                  gtk_text_buffer_get_end_iter (buffer, &end);
                  link_type = "uri";
                }

              q0 = q2;

              link = g_strndup (q1, q2 - q1);

              if (g_ptr_array_find_with_equal_func (about->visited_links, link, (GCompareFunc)strcmp, NULL))
                color = visited_link_color;
              else
                color = link_color;

              tag = gtk_text_buffer_create_tag (buffer, NULL,
                                                "foreground-rgba", &color,
                                                "underline", PANGO_UNDERLINE_SINGLE,
                                                NULL);
              if (strcmp (link_type, "email") == 0)
                {
                  char *escaped;

                  escaped = g_uri_escape_string (link, NULL, FALSE);
                  uri = g_strconcat ("mailto:", escaped, NULL);
                  g_free (escaped);
                }
              else
                {
                  uri = g_strdup (link);
                }
              g_object_set_data_full (G_OBJECT (tag), I_("uri"), uri, g_free);
              gtk_text_buffer_insert_with_tags (buffer, &end, link, -1, tag, NULL);

              g_free (link);
            }
          else
            {
              gtk_text_buffer_insert_at_cursor (buffer, q0, -1);
              break;
            }
        }

      if (p[1])
        gtk_text_buffer_insert_at_cursor (buffer, "\n", 1);
    }

  tag = gtk_text_buffer_create_tag (buffer, NULL,
                                    "scale", PANGO_SCALE_SMALL,
                                    NULL);

  gtk_text_buffer_get_start_iter (buffer, &start_iter);
  gtk_text_buffer_get_end_iter (buffer, &end_iter);
  gtk_text_buffer_apply_tag (buffer, tag, &start_iter, &end_iter);

  gtk_text_buffer_set_enable_undo (buffer, FALSE);

  return buffer;
}

static void
add_credits_section (GtkAboutDialog  *about,
                     GtkGrid         *grid,
                     int             *row,
                     char            *title,
                     char           **people)
{
  GtkWidget *label;
  char *markup;
  char **p;
  char *q0, *q1, *q2, *r1, *r2;

  if (people == NULL)
    return;

  markup = g_strdup_printf ("<span size=\"small\">%s</span>", title);
  label = gtk_label_new (markup);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_free (markup);
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_grid_attach (grid, label, 0, *row, 1, 1);

  for (p = people; *p; p++)
    {
      GString *str;

      str = g_string_new ("<span size=\"small\">");

      q0 = *p;
      while (*q0)
        {
          q1 = strchr (q0, '<');
          q2 = q1 ? strchr (q1, '>') : NULL;
          r1 = strstr (q0, "http://");
          r2 = strstr (q0, "https://");
          if (!r1 || (r1 && r2 && r2 < r1))
            r1 = r2;
          if (r1)
            {
              r2 = strpbrk (r1, " \n\t");
              if (!r2)
                r2 = strchr (r1, '\0');
            }
          else
            r2 = NULL;

          if (r1 && r2 && (!q1 || !q2 || (r1 < q1)))
            {
              q1 = r1;
              q2 = r2;
            }
          else if (q1 && (q1[1] == 'a' || q1[1] == 'A') && q1[2] == ' ')
            {
              /* if it is a <a> link leave it for the label to parse */
              q1 = NULL;
            }

          if (q1 && q2)
            {
              char *link;
              char *text;
              char *name;

              if (*q1 == '<')
                {
                  /* email */
                  char *escaped;

                  text = g_strstrip (g_strndup (q0, q1 - q0));
                  name = g_markup_escape_text (text, -1);
                  q1++;
                  link = g_strndup (q1, q2 - q1);
                  q2++;
                  escaped = g_uri_escape_string (link, NULL, FALSE);
                  g_string_append_printf (str,
                                          "<a href=\"mailto:%s\">%s</a>",
                                          escaped,
                                          name[0] ? name : link);
                  g_free (escaped);
                  g_free (link);
                  g_free (text);
                  g_free (name);
                }
              else
                {
                  /* uri */
                  text = g_strstrip (g_strndup (q0, q1 - q0));
                  name = g_markup_escape_text (text, -1);
                  link = g_strndup (q1, q2 - q1);
                  g_string_append_printf (str,
                                          "<a href=\"%s\">%s</a>",
                                          link,
                                          name[0] ? name : link);
                  g_free (link);
                  g_free (text);
                  g_free (name);
                }

              q0 = q2;
            }
          else
            {
              g_string_append (str, q0);
              break;
            }
        }
      g_string_append (str, "</span>");

      label = gtk_label_new (str->str);
      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      gtk_label_set_selectable (GTK_LABEL (label), TRUE);
      g_signal_connect_swapped (label, "activate-link",
                                G_CALLBACK (emit_activate_link), about);
      g_string_free (str, TRUE);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_grid_attach (grid, label, 1, *row, 1, 1);
      gtk_widget_set_visible (label, TRUE);
      (*row)++;
    }

  /* skip one at the end */
  label = gtk_label_new ("");
  gtk_grid_attach (grid, label, 1, *row, 1, 1);
  (*row)++;
}

static void
populate_credits_page (GtkAboutDialog *about)
{
  int row;

  row = 0;

  if (about->authors != NULL)
    add_credits_section (about, GTK_GRID (about->credits_grid), &row, _("Created by"), about->authors);

  if (about->documenters != NULL)
    add_credits_section (about, GTK_GRID (about->credits_grid), &row, _("Documented by"), about->documenters);

  /* Don't show an untranslated gettext msgid */
  if (about->translator_credits != NULL &&
      strcmp (about->translator_credits, "translator_credits") != 0 &&
      strcmp (about->translator_credits, "translator-credits") != 0)
    {
      char **translators;

      translators = g_strsplit (about->translator_credits, "\n", 0);
      add_credits_section (about, GTK_GRID (about->credits_grid), &row, _("Translated by"), translators);
      g_strfreev (translators);
    }

  if (about->artists != NULL)
    add_credits_section (about, GTK_GRID (about->credits_grid), &row, _("Design by"), about->artists);

  if (about->credit_sections != NULL)
    {
      GSList *cs;
      for (cs = about->credit_sections; cs != NULL; cs = cs->next)
	{
	  CreditSection *section = cs->data;
	  add_credits_section (about, GTK_GRID (about->credits_grid), &row, section->heading, section->people);
	}
    }
}

static void
populate_license_page (GtkAboutDialog *about)
{
  GtkTextBuffer *buffer;
  char *strings[2];

  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (about->license_view), about->wrap_license ? GTK_WRAP_WORD : GTK_WRAP_NONE);

  strings[0] = about->license;
  strings[1] = NULL;
  buffer = text_buffer_new (about, strings);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (about->license_view), buffer);
  g_object_unref (buffer);
}

static void
populate_system_page (GtkAboutDialog *about)
{
  GtkTextBuffer *buffer;
  char *strings[2];

  strings[0] = about->system_information;
  strings[1] = NULL;
  buffer = text_buffer_new (about, strings);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (about->system_view), buffer);
  g_object_unref (buffer);
}

/**
 * gtk_about_dialog_new:
 *
 * Creates a new `GtkAboutDialog`.
 *
 * Returns: a newly created `GtkAboutDialog`
 */
GtkWidget *
gtk_about_dialog_new (void)
{
  return g_object_new (GTK_TYPE_ABOUT_DIALOG, NULL);
}

static gboolean
close_cb (GtkAboutDialog *about,
          gpointer        user_data)
{
  gtk_stack_set_visible_child_name (GTK_STACK (about->stack), "main");
  gtk_widget_set_visible (GTK_WIDGET (about), FALSE);

  return TRUE;
}

/**
 * gtk_show_about_dialog:
 * @parent: (nullable): the parent top-level window
 * @first_property_name: the name of the first property
 * @...: value of first property, followed by more pairs of property
 *   name and value, `NULL`-terminated
 *
 * A convenience function for showing an application’s about dialog.
 *
 * The constructed dialog is associated with the parent window and
 * reused for future invocations of this function.
 */
void
gtk_show_about_dialog (GtkWindow   *parent,
                       const char *first_property_name,
                       ...)
{
  static GtkWidget *global_about_dialog = NULL;
  GtkWidget *dialog = NULL;
  va_list var_args;

  if (parent)
    dialog = g_object_get_data (G_OBJECT (parent), "gtk-about-dialog");
  else
    dialog = global_about_dialog;

  if (!dialog)
    {
      dialog = gtk_about_dialog_new ();
      gtk_window_set_hide_on_close (GTK_WINDOW (dialog), TRUE);

      g_object_ref_sink (dialog);

      /* Hide the dialog on close request */
      g_signal_connect (dialog, "close-request",
                        G_CALLBACK (close_cb), NULL);

      va_start (var_args, first_property_name);
      g_object_set_valist (G_OBJECT (dialog), first_property_name, var_args);
      va_end (var_args);

      if (parent)
        {
          gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
          gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
          gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
          g_object_set_data_full (G_OBJECT (parent),
                                  I_("gtk-about-dialog"),
                                  dialog, g_object_unref);
        }
      else
        global_about_dialog = dialog;

    }

  gtk_window_present (GTK_WINDOW (dialog));
}

/**
 * gtk_about_dialog_set_license_type: (attributes org.gtk.Method.set_property=license-type)
 * @about: a `GtkAboutDialog`
 * @license_type: the type of license
 *
 * Sets the license of the application showing the about dialog from a
 * list of known licenses.
 *
 * This function overrides the license set using
 * [method@Gtk.AboutDialog.set_license].
 */
void
gtk_about_dialog_set_license_type (GtkAboutDialog *about,
                                   GtkLicense      license_type)
{
  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));
  g_return_if_fail (license_type >= GTK_LICENSE_UNKNOWN &&
                    license_type < G_N_ELEMENTS (gtk_license_info));

  if (about->license_type != license_type)
    {
      g_object_freeze_notify (G_OBJECT (about));

      about->license_type = license_type;

      gtk_widget_set_visible (about->license_label, TRUE);
      /* custom licenses use the contents of the :license property */
      if (about->license_type != GTK_LICENSE_CUSTOM)
        {
          const char *name;
          const char *url;
          char *license_string;
          GString *str;

          name = _(gtk_license_info[about->license_type].name);
          url = gtk_license_info[about->license_type].url;
          if (url == NULL)
            url = about->website_url;

          str = g_string_sized_new (256);
          /* Translators: this is the license preamble; the string at the end
           * contains the name of the license as link text.
           */
          g_string_append_printf (str, _("This program comes with absolutely no warranty.\nSee the <a href=\"%s\">%s</a> for details."), url, name);

          g_free (about->license);
          about->license = g_string_free (str, FALSE);
          about->wrap_license = TRUE;

          license_string = g_strdup_printf ("<span size=\"small\">%s</span>",
                                            about->license);
          gtk_label_set_markup (GTK_LABEL (about->license_label), license_string);
          g_free (license_string);

          g_object_notify_by_pspec (G_OBJECT (about), props[PROP_WRAP_LICENSE]);
          g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LICENSE]);
        }

      update_license_button_visibility (about);

      g_object_notify_by_pspec (G_OBJECT (about), props[PROP_LICENSE_TYPE]);

      g_object_thaw_notify (G_OBJECT (about));
    }
}

/**
 * gtk_about_dialog_get_license_type: (attributes org.gtk.Method.get_property=license-type)
 * @about: a `GtkAboutDialog`
 *
 * Retrieves the license type.
 *
 * Returns: a [enum@Gtk.License] value
 */
GtkLicense
gtk_about_dialog_get_license_type (GtkAboutDialog *about)
{
  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), GTK_LICENSE_UNKNOWN);

  return about->license_type;
}

/**
 * gtk_about_dialog_add_credit_section:
 * @about: A `GtkAboutDialog`
 * @section_name: The name of the section
 * @people: (array zero-terminated=1): The people who belong to that section
 *
 * Creates a new section in the "Credits" page.
 */
void
gtk_about_dialog_add_credit_section (GtkAboutDialog  *about,
                                     const char      *section_name,
                                     const char     **people)
{
  CreditSection *new_entry;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));
  g_return_if_fail (section_name != NULL);
  g_return_if_fail (people != NULL);

  new_entry = g_new (CreditSection, 1);
  new_entry->heading = g_strdup ((char *)section_name);
  new_entry->people = g_strdupv ((char **)people);

  about->credit_sections = g_slist_append (about->credit_sections, new_entry);
  update_credits_button_visibility (about);
}
