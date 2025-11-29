/* GtkPrinter
 * Copyright (C) 2006 John (J5) Palmieri  <johnp@redhat.com>
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gtkprinter.h"
#include "gtkprinterprivate.h"
#include "gtkprintbackendprivate.h"
#include "gtkprintjob.h"

/**
 * GtkPrinter:
 *
 * A `GtkPrinter` object represents a printer.
 *
 * You only need to deal directly with printers if you use the
 * non-portable [class@Gtk.PrintUnixDialog] API.
 *
 * A `GtkPrinter` allows to get status information about the printer,
 * such as its description, its location, the number of queued jobs,
 * etc. Most importantly, a `GtkPrinter` object can be used to create
 * a [class@Gtk.PrintJob] object, which lets you print to the printer.
 */


static void gtk_printer_finalize     (GObject *object);

struct _GtkPrinterPrivate
{
  char *name;
  char *location;
  char *description;
  char *icon_name;

  guint is_active         : 1;
  guint is_paused         : 1;
  guint is_accepting_jobs : 1;
  guint is_new            : 1;
  guint is_virtual        : 1;
  guint is_default        : 1;
  guint has_details       : 1;
  guint accepts_pdf       : 1;
  guint accepts_ps        : 1;

  char *state_message;
  int job_count;

  GtkPrintBackend *backend;
};

enum {
  DETAILS_ACQUIRED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_BACKEND,
  PROP_IS_VIRTUAL,
  PROP_STATE_MESSAGE,
  PROP_LOCATION,
  PROP_ICON_NAME,
  PROP_JOB_COUNT,
  PROP_ACCEPTS_PDF,
  PROP_ACCEPTS_PS,
  PROP_PAUSED,
  PROP_ACCEPTING_JOBS
};

static guint signals[LAST_SIGNAL] = { 0 };

static void gtk_printer_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec);
static void gtk_printer_get_property (GObject      *object,
				      guint         prop_id,
				      GValue       *value,
				      GParamSpec   *pspec);

G_DEFINE_TYPE_WITH_PRIVATE (GtkPrinter, gtk_printer, G_TYPE_OBJECT)

static void
gtk_printer_class_init (GtkPrinterClass *class)
{
  GObjectClass *object_class;
  object_class = (GObjectClass *) class;

  object_class->finalize = gtk_printer_finalize;

  object_class->set_property = gtk_printer_set_property;
  object_class->get_property = gtk_printer_get_property;

  /**
   * GtkPrinter:name: (attributes org.gtk.Property.get=gtk_printer_get_name)
   *
   * The name of the printer.
   */
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_NAME,
                                   g_param_spec_string ("name", NULL, NULL,
						        "",
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtkPrinter:backend: (attributes org.gtk.Property.get=gtk_printer_get_backend)
   *
   * The backend for the printer.
   */
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_BACKEND,
                                   g_param_spec_object ("backend", NULL, NULL,
						        GTK_TYPE_PRINT_BACKEND,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtkPrinter:is-virtual: (attributes org.gtk.Property.get=gtk_printer_is_virtual)
   *
   * %FALSE if this represents a real hardware device.
   */
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_IS_VIRTUAL,
                                   g_param_spec_boolean ("is-virtual", NULL, NULL,
							 FALSE,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtkPrinter:accepts-pdf: (attributes org.gtk.Property.get=gtk_printer_accepts_pdf)
   *
   * %TRUE if this printer can accept PDF.
   */
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_ACCEPTS_PDF,
                                   g_param_spec_boolean ("accepts-pdf", NULL, NULL,
							 FALSE,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtkPrinter:accepts-ps: (attributes org.gtk.Property.get=gtk_printer_accepts_ps)
   *
   * %TRUE if this printer can accept PostScript.
   */
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_ACCEPTS_PS,
                                   g_param_spec_boolean ("accepts-ps", NULL, NULL,
							 TRUE,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtkPrinter:state-message: (attributes org.gtk.Property.get=gtk_printer_get_state_message)
   *
   * String giving the current status of the printer.
   */
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_STATE_MESSAGE,
                                   g_param_spec_string ("state-message", NULL, NULL,
						        "",
							G_PARAM_READABLE));

  /**
   * GtkPrinter:location: (attributes org.gtk.Property.get=gtk_printer_get_location)
   *
   * Information about the location of the printer.
   */
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_LOCATION,
                                   g_param_spec_string ("location", NULL, NULL,
						        "",
							G_PARAM_READABLE));

  /**
   * GtkPrinter:icon-name: (attributes org.gtk.Property.get=gtk_printer_get_icon_name)
   *
   * Icon name to use for the printer.
   */
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name", NULL, NULL,
						        "printer",
							G_PARAM_READABLE));

  /**
   * GtkPrinter:job-count: (attributes org.gtk.Property.get=gtk_printer_get_job_count)
   *
   * Number of jobs queued in the printer.
   */
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_JOB_COUNT,
				   g_param_spec_int ("job-count", NULL, NULL,
 						     0,
 						     G_MAXINT,
 						     0,
 						     G_PARAM_READABLE));

  /**
   * GtkPrinter:paused: (attributes org.gtk.Property.get=gtk_printer_is_paused)
   *
   * %TRUE if this printer is paused.
   *
   * A paused printer still accepts jobs, but it does
   * not print them.
   */
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_PAUSED,
                                   g_param_spec_boolean ("paused", NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));

  /**
   * GtkPrinter:accepting-jobs: (attributes org.gtk.Property.get=gtk_printer_is_accepting_jobs)
   *
   * %TRUE if the printer is accepting jobs.
   */
  g_object_class_install_property (G_OBJECT_CLASS (class),
                                   PROP_ACCEPTING_JOBS,
                                   g_param_spec_boolean ("accepting-jobs", NULL, NULL,
							 TRUE,
							 G_PARAM_READABLE));

  /**
   * GtkPrinter::details-acquired:
   * @printer: the `GtkPrinter` on which the signal is emitted
   * @success: %TRUE if the details were successfully acquired
   *
   * Emitted in response to a request for detailed information
   * about a printer from the print backend.
   *
   * The @success parameter indicates if the information was
   * actually obtained.
   */
  signals[DETAILS_ACQUIRED] =
    g_signal_new ("details-acquired",
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrinterClass, details_acquired),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void
gtk_printer_init (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  priv->name = NULL;
  priv->location = NULL;
  priv->description = NULL;
  priv->icon_name = g_strdup ("printer");

  priv->is_active = TRUE;
  priv->is_paused = FALSE;
  priv->is_accepting_jobs = TRUE;
  priv->is_new = TRUE;
  priv->has_details = FALSE;
  priv->accepts_pdf = FALSE;
  priv->accepts_ps = TRUE;

  priv->state_message = NULL;  
  priv->job_count = 0;
}

static void
gtk_printer_finalize (GObject *object)
{
  GtkPrinter *printer = GTK_PRINTER (object);
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_free (priv->name);
  g_free (priv->location);
  g_free (priv->description);
  g_free (priv->state_message);
  g_free (priv->icon_name);

  if (priv->backend)
    g_object_unref (priv->backend);

  G_OBJECT_CLASS (gtk_printer_parent_class)->finalize (object);
}

static void
gtk_printer_set_property (GObject         *object,
			  guint            prop_id,
			  const GValue    *value,
			  GParamSpec      *pspec)
{
  GtkPrinter *printer = GTK_PRINTER (object);
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  switch (prop_id)
    {
    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;
    
    case PROP_BACKEND:
      priv->backend = GTK_PRINT_BACKEND (g_value_dup_object (value));
      break;

    case PROP_IS_VIRTUAL:
      priv->is_virtual = g_value_get_boolean (value);
      break;

    case PROP_ACCEPTS_PDF:
      priv->accepts_pdf = g_value_get_boolean (value);
      break;

    case PROP_ACCEPTS_PS:
      priv->accepts_ps = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_printer_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
  GtkPrinter *printer = GTK_PRINTER (object);
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  switch (prop_id)
    {
    case PROP_NAME:
      if (priv->name)
	g_value_set_string (value, priv->name);
      else
	g_value_set_static_string (value, "");
      break;
    case PROP_BACKEND:
      g_value_set_object (value, priv->backend);
      break;
    case PROP_STATE_MESSAGE:
      if (priv->state_message)
	g_value_set_string (value, priv->state_message);
      else
	g_value_set_static_string (value, "");
      break;
    case PROP_LOCATION:
      if (priv->location)
	g_value_set_string (value, priv->location);
      else
	g_value_set_static_string (value, "");
      break;
    case PROP_ICON_NAME:
      if (priv->icon_name)
	g_value_set_string (value, priv->icon_name);
      else
	g_value_set_static_string (value, "printer");
      break;
    case PROP_JOB_COUNT:
      g_value_set_int (value, priv->job_count);
      break;
    case PROP_IS_VIRTUAL:
      g_value_set_boolean (value, priv->is_virtual);
      break;
    case PROP_ACCEPTS_PDF:
      g_value_set_boolean (value, priv->accepts_pdf);
      break;
    case PROP_ACCEPTS_PS:
      g_value_set_boolean (value, priv->accepts_ps);
      break;
    case PROP_PAUSED:
      g_value_set_boolean (value, priv->is_paused);
      break;
    case PROP_ACCEPTING_JOBS:
      g_value_set_boolean (value, priv->is_accepting_jobs);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_printer_new:
 * @name: the name of the printer
 * @backend: a `GtkPrintBackend`
 * @virtual_: whether the printer is virtual
 *
 * Creates a new `GtkPrinter`.
 *
 * Returns: a new `GtkPrinter`
 */
GtkPrinter *
gtk_printer_new (const char      *name,
		 GtkPrintBackend *backend,
		 gboolean         virtual_)
{
  GObject *result;
  
  result = g_object_new (GTK_TYPE_PRINTER,
			 "name", name,
			 "backend", backend,
			 "is-virtual", virtual_,
                         NULL);

  return (GtkPrinter *) result;
}

/**
 * gtk_printer_get_backend: (attributes org.gtk.Method.get_property=backend)
 * @printer: a `GtkPrinter`
 *
 * Returns the backend of the printer.
 *
 * Returns: (transfer none): the backend of @printer
 */
GtkPrintBackend *
gtk_printer_get_backend (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);
  
  return priv->backend;
}

/**
 * gtk_printer_get_name: (attributes org.gtk.Method.get_property=name)
 * @printer: a `GtkPrinter`
 *
 * Returns the name of the printer.
 *
 * Returns: the name of @printer
 */
const char *
gtk_printer_get_name (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);

  return priv->name;
}

/**
 * gtk_printer_get_description:
 * @printer: a `GtkPrinter`
 *
 * Gets the description of the printer.
 *
 * Returns: the description of @printer
 */
const char *
gtk_printer_get_description (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);
  
  return priv->description;
}

gboolean
gtk_printer_set_description (GtkPrinter  *printer,
			     const char *description)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), FALSE);

  if (g_strcmp0 (priv->description, description) == 0)
    return FALSE;

  g_free (priv->description);
  priv->description = g_strdup (description);
  
  return TRUE;
}

/**
 * gtk_printer_get_state_message: (attributes org.gtk.Method.get_property=state-message)
 * @printer: a `GtkPrinter`
 *
 * Returns the state message describing the current state
 * of the printer.
 *
 * Returns: the state message of @printer
 */
const char *
gtk_printer_get_state_message (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);

  return priv->state_message;
}

gboolean
gtk_printer_set_state_message (GtkPrinter  *printer,
			       const char *message)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), FALSE);

  if (g_strcmp0 (priv->state_message, message) == 0)
    return FALSE;

  g_free (priv->state_message);
  priv->state_message = g_strdup (message);
  g_object_notify (G_OBJECT (printer), "state-message");

  return TRUE;
}

/**
 * gtk_printer_get_location: (attributes org.gtk.Method.get_property=location)
 * @printer: a `GtkPrinter`
 *
 * Returns a description of the location of the printer.
 *
 * Returns: the location of @printer
 */
const char *
gtk_printer_get_location (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);

  return priv->location;
}

gboolean
gtk_printer_set_location (GtkPrinter  *printer,
			  const char *location)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), FALSE);

  if (g_strcmp0 (priv->location, location) == 0)
    return FALSE;

  g_free (priv->location);
  priv->location = g_strdup (location);
  g_object_notify (G_OBJECT (printer), "location");
  
  return TRUE;
}

/**
 * gtk_printer_get_icon_name: (attributes org.gtk.Method.get_property=icon-name)
 * @printer: a `GtkPrinter`
 *
 * Gets the name of the icon to use for the printer.
 *
 * Returns: the icon name for @printer
 */
const char *
gtk_printer_get_icon_name (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);

  return priv->icon_name;
}

void
gtk_printer_set_icon_name (GtkPrinter  *printer,
			   const char *icon)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_if_fail (GTK_IS_PRINTER (printer));

  g_free (priv->icon_name);
  priv->icon_name = g_strdup (icon);
  g_object_notify (G_OBJECT (printer), "icon-name");
}

/**
 * gtk_printer_get_job_count: (attributes org.gtk.Method.get_property=job-count)
 * @printer: a `GtkPrinter`
 *
 * Gets the number of jobs currently queued on the printer.
 *
 * Returns: the number of jobs on @printer
 */
int
gtk_printer_get_job_count (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), 0);

  return priv->job_count;
}

gboolean
gtk_printer_set_job_count (GtkPrinter *printer,
			   int         count)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), FALSE);

  if (priv->job_count == count)
    return FALSE;

  priv->job_count = count;
  
  g_object_notify (G_OBJECT (printer), "job-count");
  
  return TRUE;
}

/**
 * gtk_printer_has_details:
 * @printer: a `GtkPrinter`
 *
 * Returns whether the printer details are available.
 *
 * Returns: %TRUE if @printer details are available
 */
gboolean
gtk_printer_has_details (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), FALSE);

  return priv->has_details;
}

void
gtk_printer_set_has_details (GtkPrinter *printer,
			     gboolean    val)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  priv->has_details = val;
}

/**
 * gtk_printer_is_active:
 * @printer: a `GtkPrinter`
 *
 * Returns whether the printer is currently active (i.e.
 * accepts new jobs).
 *
 * Returns: %TRUE if @printer is active
 */
gboolean
gtk_printer_is_active (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), TRUE);
  
  return priv->is_active;
}

void
gtk_printer_set_is_active (GtkPrinter *printer,
			   gboolean val)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_if_fail (GTK_IS_PRINTER (printer));

  priv->is_active = val;
}

/**
 * gtk_printer_is_paused: (attributes org.gtk.Method.get_property=paused)
 * @printer: a `GtkPrinter`
 *
 * Returns whether the printer is currently paused.
 *
 * A paused printer still accepts jobs, but it is not
 * printing them.
 *
 * Returns: %TRUE if @printer is paused
 */
gboolean
gtk_printer_is_paused (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), TRUE);
  
  return priv->is_paused;
}

gboolean
gtk_printer_set_is_paused (GtkPrinter *printer,
			   gboolean    val)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), FALSE);

  if (val == priv->is_paused)
    return FALSE;

  priv->is_paused = val;

  return TRUE;
}

/**
 * gtk_printer_is_accepting_jobs: (attributes org.gtk.Method.get_property=accepting-jobs)
 * @printer: a `GtkPrinter`
 *
 * Returns whether the printer is accepting jobs
 *
 * Returns: %TRUE if @printer is accepting jobs
 */
gboolean
gtk_printer_is_accepting_jobs (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), TRUE);
  
  return priv->is_accepting_jobs;
}

gboolean
gtk_printer_set_is_accepting_jobs (GtkPrinter *printer,
				   gboolean val)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), FALSE);

  if (val == priv->is_accepting_jobs)
    return FALSE;

  priv->is_accepting_jobs = val;

  return TRUE;
}

/**
 * gtk_printer_is_virtual: (attributes org.gtk.Method.get_property=is-virtual)
 * @printer: a `GtkPrinter`
 *
 * Returns whether the printer is virtual (i.e. does not
 * represent actual printer hardware, but something like
 * a CUPS class).
 *
 * Returns: %TRUE if @printer is virtual
 */
gboolean
gtk_printer_is_virtual (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), TRUE);
  
  return priv->is_virtual;
}

/**
 * gtk_printer_accepts_pdf: (attributes org.gtk.Method.get_property=accepts-pdf)
 * @printer: a `GtkPrinter`
 *
 * Returns whether the printer accepts input in
 * PDF format.
 *
 * Returns: %TRUE if @printer accepts PDF
 */
gboolean
gtk_printer_accepts_pdf (GtkPrinter *printer)
{ 
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), TRUE);
  
  return priv->accepts_pdf;
}

void
gtk_printer_set_accepts_pdf (GtkPrinter *printer,
			     gboolean val)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_if_fail (GTK_IS_PRINTER (printer));

  priv->accepts_pdf = val;
}

/**
 * gtk_printer_accepts_ps: (attributes org.gtk.Method.get_property=accepts-ps)
 * @printer: a `GtkPrinter`
 *
 * Returns whether the printer accepts input in
 * PostScript format.
 *
 * Returns: %TRUE if @printer accepts PostScript
 */
gboolean
gtk_printer_accepts_ps (GtkPrinter *printer)
{ 
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), TRUE);
  
  return priv->accepts_ps;
}

void
gtk_printer_set_accepts_ps (GtkPrinter *printer,
			    gboolean val)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_if_fail (GTK_IS_PRINTER (printer));

  priv->accepts_ps = val;
}

gboolean
gtk_printer_is_new (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), FALSE);
  
  return priv->is_new;
}

void
gtk_printer_set_is_new (GtkPrinter *printer,
			gboolean val)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_if_fail (GTK_IS_PRINTER (printer));

  priv->is_new = val;
}


/**
 * gtk_printer_is_default:
 * @printer: a `GtkPrinter`
 *
 * Returns whether the printer is the default printer.
 *
 * Returns: %TRUE if @printer is the default
 */
gboolean
gtk_printer_is_default (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_val_if_fail (GTK_IS_PRINTER (printer), FALSE);
  
  return priv->is_default;
}

void
gtk_printer_set_is_default (GtkPrinter *printer,
			    gboolean    val)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);

  g_return_if_fail (GTK_IS_PRINTER (printer));

  priv->is_default = val;
}

/**
 * gtk_printer_request_details:
 * @printer: a `GtkPrinter`
 *
 * Requests the printer details.
 *
 * When the details are available, the
 * [signal@Gtk.Printer::details-acquired] signal
 * will be emitted on @printer.
 */
void
gtk_printer_request_details (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);
  GtkPrintBackendClass *backend_class;

  g_return_if_fail (GTK_IS_PRINTER (printer));

  backend_class = GTK_PRINT_BACKEND_GET_CLASS (priv->backend);
  backend_class->printer_request_details (printer);
}

GtkPrinterOptionSet *
_gtk_printer_get_options (GtkPrinter           *printer,
			  GtkPrintSettings     *settings,
			  GtkPageSetup         *page_setup,
			  GtkPrintCapabilities  capabilities)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (priv->backend);

  return backend_class->printer_get_options (printer, settings, page_setup, capabilities);
}

gboolean
_gtk_printer_mark_conflicts (GtkPrinter          *printer,
			     GtkPrinterOptionSet *options)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (priv->backend);

  return backend_class->printer_mark_conflicts (printer, options);
}
  
void
_gtk_printer_get_settings_from_options (GtkPrinter          *printer,
					GtkPrinterOptionSet *options,
					GtkPrintSettings    *settings)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (priv->backend);

  backend_class->printer_get_settings_from_options (printer, options, settings);
}

void
_gtk_printer_prepare_for_print (GtkPrinter       *printer,
				GtkPrintJob      *print_job,
				GtkPrintSettings *settings,
				GtkPageSetup     *page_setup)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (priv->backend);

  backend_class->printer_prepare_for_print (printer, print_job, settings, page_setup);
}

cairo_surface_t *
_gtk_printer_create_cairo_surface (GtkPrinter       *printer,
				   GtkPrintSettings *settings,
				   double            width, 
				   double            height,
				   GIOChannel       *cache_io)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (priv->backend);

  return backend_class->printer_create_cairo_surface (printer, settings,
						      width, height, cache_io);
}

/**
 * gtk_printer_list_papers:
 * @printer: a `GtkPrinter`
 *
 * Lists all the paper sizes @printer supports.
 *
 * This will return and empty list unless the printer’s details
 * are available, see [method@Gtk.Printer.has_details] and
 * [method@Gtk.Printer.request_details].
 *
 * Returns: (element-type GtkPageSetup) (transfer full): a newly
 *   allocated list of newly allocated `GtkPageSetup`s.
 */
GList  *
gtk_printer_list_papers (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);
  GtkPrintBackendClass *backend_class;

  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);

  backend_class = GTK_PRINT_BACKEND_GET_CLASS (priv->backend);
  return backend_class->printer_list_papers (printer);
}

/**
 * gtk_printer_get_default_page_size:
 * @printer: a `GtkPrinter`
 *
 * Returns default page size of @printer.
 *
 * Returns: (transfer full): a newly allocated `GtkPageSetup` with default page size
 *   of the printer.
 */
GtkPageSetup *
gtk_printer_get_default_page_size (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);
  GtkPrintBackendClass *backend_class;

  g_return_val_if_fail (GTK_IS_PRINTER (printer), NULL);

  backend_class = GTK_PRINT_BACKEND_GET_CLASS (priv->backend);

  return backend_class->printer_get_default_page_size (printer);
}

/**
 * gtk_printer_get_hard_margins:
 * @printer: a `GtkPrinter`
 * @top: (out): a location to store the top margin in
 * @bottom: (out): a location to store the bottom margin in
 * @left: (out): a location to store the left margin in
 * @right: (out): a location to store the right margin in
 *
 * Retrieve the hard margins of @printer.
 *
 * These are the margins that define the area at the borders
 * of the paper that the printer cannot print to.
 *
 * Note: This will not succeed unless the printer’s details are
 * available, see [method@Gtk.Printer.has_details] and
 * [method@Gtk.Printer.request_details].
 *
 * Returns: %TRUE iff the hard margins were retrieved
 */
gboolean
gtk_printer_get_hard_margins (GtkPrinter *printer,
			      double     *top,
			      double     *bottom,
			      double     *left,
			      double     *right)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (priv->backend);

  return backend_class->printer_get_hard_margins (printer, top, bottom, left, right);
}

/**
 * gtk_printer_get_hard_margins_for_paper_size:
 * @printer: a `GtkPrinter`
 * @paper_size: a `GtkPaperSize`
 * @top: (out): a location to store the top margin in
 * @bottom: (out): a location to store the bottom margin in
 * @left: (out): a location to store the left margin in
 * @right: (out): a location to store the right margin in
 *
 * Retrieve the hard margins of @printer for @paper_size.
 *
 * These are the margins that define the area at the borders
 * of the paper that the printer cannot print to.
 *
 * Note: This will not succeed unless the printer’s details are
 * available, see [method@Gtk.Printer.has_details] and
 * [method@Gtk.Printer.request_details].
 *
 * Return value: %TRUE iff the hard margins were retrieved
 */
gboolean
gtk_printer_get_hard_margins_for_paper_size (GtkPrinter   *printer,
					     GtkPaperSize *paper_size,
					     double       *top,
					     double       *bottom,
					     double       *left,
					     double       *right)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_GET_CLASS (priv->backend);

  return backend_class->printer_get_hard_margins_for_paper_size (printer, paper_size, top, bottom, left, right);
}

/**
 * gtk_printer_get_capabilities:
 * @printer: a `GtkPrinter`
 * 
 * Returns the printer’s capabilities.
 *
 * This is useful when you’re using `GtkPrintUnixDialog`’s
 * manual-capabilities setting and need to know which settings
 * the printer can handle and which you must handle yourself.
 *
 * This will return 0 unless the printer’s details are
 * available, see [method@Gtk.Printer.has_details] and
 * [method@Gtk.Printer.request_details].
 *
 * Returns: the printer’s capabilities
 */
GtkPrintCapabilities
gtk_printer_get_capabilities (GtkPrinter *printer)
{
  GtkPrinterPrivate *priv = gtk_printer_get_instance_private (printer);
  GtkPrintBackendClass *backend_class;

  g_return_val_if_fail (GTK_IS_PRINTER (printer), 0);

  backend_class = GTK_PRINT_BACKEND_GET_CLASS (priv->backend);
  return backend_class->printer_get_capabilities (printer);
}

/**
 * gtk_printer_compare:
 * @a: a `GtkPrinter`
 * @b: another `GtkPrinter`
 *
 * Compares two printers.
 *
 * Returns: 0 if the printer match, a negative value if @a < @b,
 *   or a positive value if @a > @b
 */
int
gtk_printer_compare (GtkPrinter *a, 
                     GtkPrinter *b)
{
  const char *name_a, *name_b;
  
  g_assert (GTK_IS_PRINTER (a) && GTK_IS_PRINTER (b));
  
  name_a = gtk_printer_get_name (a);
  name_b = gtk_printer_get_name (b);
  if (name_a == NULL  && name_b == NULL)
    return 0;
  else if (name_a == NULL)
    return G_MAXINT;
  else if (name_b == NULL)
    return G_MININT;
  else
    return g_ascii_strcasecmp (name_a, name_b);
}


typedef struct 
{
  GList *backends;
  GtkPrinterFunc func;
  gpointer data;
  GDestroyNotify destroy;
  GMainLoop *loop;
} PrinterList;

static void list_done_cb (GtkPrintBackend *backend, 
			  PrinterList     *printer_list);

static void
stop_enumeration (PrinterList *printer_list)
{
  GList *list, *next;
  GtkPrintBackend *backend;

  for (list = printer_list->backends; list; list = next)
    {
      next = list->next;
      backend = GTK_PRINT_BACKEND (list->data);
      list_done_cb (backend, printer_list);
    }
}

static void 
free_printer_list (PrinterList *printer_list)
{
  if (printer_list->destroy)
    printer_list->destroy (printer_list->data);

  if (printer_list->loop)
    {    
      g_main_loop_quit (printer_list->loop);
      g_main_loop_unref (printer_list->loop);
    }

  g_free (printer_list);
}

static gboolean
list_added_cb (GtkPrintBackend *backend, 
	       GtkPrinter      *printer, 
	       PrinterList     *printer_list)
{
  if (printer_list->func (printer, printer_list->data))
    {
      stop_enumeration (printer_list);
      return TRUE;
    }

  return FALSE;
}

static void
backend_status_changed (GObject    *object,
                        GParamSpec *pspec,
                        gpointer    data)
{
  GtkPrintBackend *backend = GTK_PRINT_BACKEND (object);
  PrinterList *printer_list = data;
  GtkPrintBackendStatus status;

  g_object_get (backend, "status", &status, NULL);
 
  if (status == GTK_PRINT_BACKEND_STATUS_UNAVAILABLE)
    list_done_cb (backend, printer_list);  
}

static gboolean
list_printers_remove_backend (PrinterList     *printer_list,
                              GtkPrintBackend *backend)
{
  printer_list->backends = g_list_remove (printer_list->backends, backend);
  gtk_print_backend_destroy (backend);
  g_object_unref (backend);

  if (printer_list->backends == NULL)
    {
      free_printer_list (printer_list);
      return TRUE;
    }

  return FALSE;
}

static void
list_done_cb (GtkPrintBackend *backend,
	      PrinterList     *printer_list)
{
  g_signal_handlers_disconnect_by_func (backend, list_added_cb, printer_list);
  g_signal_handlers_disconnect_by_func (backend, list_done_cb, printer_list);
  g_signal_handlers_disconnect_by_func (backend, backend_status_changed, printer_list);

  list_printers_remove_backend(printer_list, backend);
}

static gboolean
list_printers_init (PrinterList     *printer_list,
		    GtkPrintBackend *backend)
{
  GList *list, *node;
  GtkPrintBackendStatus status;

  list = gtk_print_backend_get_printer_list (backend);

  for (node = list; node != NULL; node = node->next)
    {
      if (list_added_cb (backend, node->data, printer_list))
        {
          g_list_free (list);
          return TRUE;
        }
    }

  g_list_free (list);

  g_object_get (backend, "status", &status, NULL);
  
  if (status == GTK_PRINT_BACKEND_STATUS_UNAVAILABLE || 
      gtk_print_backend_printer_list_is_done (backend))
    {
      if (list_printers_remove_backend (printer_list, backend))
        return TRUE;
    }
  else
    {
      g_signal_connect (backend, "printer-added", 
			(GCallback) list_added_cb, 
			printer_list);
      g_signal_connect (backend, "printer-list-done", 
			(GCallback) list_done_cb, 
			printer_list);
      g_signal_connect (backend, "notify::status", 
                        (GCallback) backend_status_changed,
                        printer_list);     
    }

  return FALSE;
}

/**
 * gtk_enumerate_printers:
 * @func: a function to call for each printer
 * @data: user data to pass to @func
 * @destroy: function to call if @data is no longer needed
 * @wait: if %TRUE, wait in a recursive mainloop until
 *    all printers are enumerated; otherwise return early
 *
 * Calls a function for all `GtkPrinter`s.
 *
 * If @func returns %TRUE, the enumeration is stopped.
 */
void
gtk_enumerate_printers (GtkPrinterFunc func,
			gpointer       data,
			GDestroyNotify destroy,
			gboolean       wait)
{
  PrinterList *printer_list;
  GList *node, *next;
  GtkPrintBackend *backend;

  printer_list = g_new0 (PrinterList, 1);

  printer_list->func = func;
  printer_list->data = data;
  printer_list->destroy = destroy;

  if (g_module_supported ())
    printer_list->backends = gtk_print_backend_load_modules ();
  
  if (printer_list->backends == NULL)
    {
      free_printer_list (printer_list);
      return;
    }

  for (node = printer_list->backends; node != NULL; node = next)
    {
      next = node->next;
      backend = GTK_PRINT_BACKEND (node->data);
      if (list_printers_init (printer_list, backend))
        return;
    }

  if (wait && printer_list->backends)
    {
      printer_list->loop = g_main_loop_new (NULL, FALSE);
      g_main_loop_run (printer_list->loop);
    }
}


static GtkPrinter *found_printer;

static gboolean
match_printer_name (GtkPrinter *printer,
                    gpointer    data)
{
  if (strcmp (gtk_printer_get_name (printer), (const char *)data) == 0)
    {
      found_printer = g_object_ref (printer);
      return TRUE;
    }

  return FALSE;
}

GtkPrinter *
gtk_printer_find (const char *name)
{
  found_printer = NULL;
  gtk_enumerate_printers (match_printer_name, (gpointer) name, NULL, TRUE);
  return g_steal_pointer (&found_printer);
}

GType
gtk_print_capabilities_get_type (void)
{
  static GType etype = 0;

  if (G_UNLIKELY (etype == 0))
    {
      static const GFlagsValue values[] = {
        { GTK_PRINT_CAPABILITY_PAGE_SET, "GTK_PRINT_CAPABILITY_PAGE_SET", "page-set" },
        { GTK_PRINT_CAPABILITY_COPIES, "GTK_PRINT_CAPABILITY_COPIES", "copies" },
        { GTK_PRINT_CAPABILITY_COLLATE, "GTK_PRINT_CAPABILITY_COLLATE", "collate" },
        { GTK_PRINT_CAPABILITY_REVERSE, "GTK_PRINT_CAPABILITY_REVERSE", "reverse" },
        { GTK_PRINT_CAPABILITY_SCALE, "GTK_PRINT_CAPABILITY_SCALE", "scale" },
        { GTK_PRINT_CAPABILITY_GENERATE_PDF, "GTK_PRINT_CAPABILITY_GENERATE_PDF", "generate-pdf" },
        { GTK_PRINT_CAPABILITY_GENERATE_PS, "GTK_PRINT_CAPABILITY_GENERATE_PS", "generate-ps" },
        { GTK_PRINT_CAPABILITY_PREVIEW, "GTK_PRINT_CAPABILITY_PREVIEW", "preview" },
	{ GTK_PRINT_CAPABILITY_NUMBER_UP, "GTK_PRINT_CAPABILITY_NUMBER_UP", "number-up"},
        { GTK_PRINT_CAPABILITY_NUMBER_UP_LAYOUT, "GTK_PRINT_CAPABILITY_NUMBER_UP_LAYOUT", "number-up-layout" },
        { 0, NULL, NULL }
      };

      etype = g_flags_register_static ("GtkPrintCapabilities", values);
    }

  return etype;
}
