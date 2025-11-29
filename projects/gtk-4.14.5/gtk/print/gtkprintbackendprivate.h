/* GTK - The GIMP Toolkit
 * gtkprintbackend.h: Abstract printer backend interfaces
 * Copyright (C) 2006, Red Hat, Inc.
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

#pragma once

/* This is a "semi-private" header; it is meant only for
 * alternate GtkPrintDialog backend modules; no stability guarantees
 * are made at this point
 */
#ifndef GTK_PRINT_BACKEND_ENABLE_UNSUPPORTED
#error "GtkPrintBackend is not supported API for general use"
#endif

#include <gtk/gtk.h>
#include "gtkunixprint.h"
#include "gtkprinteroptionsetprivate.h"

G_BEGIN_DECLS

typedef struct _GtkPrintBackendClass    GtkPrintBackendClass;
typedef struct _GtkPrintBackendPrivate  GtkPrintBackendPrivate;

#define GTK_PRINT_BACKEND_ERROR (gtk_print_backend_error_quark ())

typedef enum
{
  /* TODO: add specific errors */
  GTK_PRINT_BACKEND_ERROR_GENERIC
} GtkPrintBackendError;

GDK_AVAILABLE_IN_ALL
GQuark     gtk_print_backend_error_quark      (void);

#define GTK_TYPE_PRINT_BACKEND                  (gtk_print_backend_get_type ())
#define GTK_PRINT_BACKEND(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PRINT_BACKEND, GtkPrintBackend))
#define GTK_PRINT_BACKEND_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINT_BACKEND, GtkPrintBackendClass))
#define GTK_IS_PRINT_BACKEND(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PRINT_BACKEND))
#define GTK_IS_PRINT_BACKEND_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINT_BACKEND))
#define GTK_PRINT_BACKEND_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINT_BACKEND, GtkPrintBackendClass))

typedef enum 
{
  GTK_PRINT_BACKEND_STATUS_UNKNOWN,
  GTK_PRINT_BACKEND_STATUS_OK,
  GTK_PRINT_BACKEND_STATUS_UNAVAILABLE
} GtkPrintBackendStatus;

struct _GtkPrintBackend
{
  GObject parent_instance;

  GtkPrintBackendPrivate *priv;
};

struct _GtkPrintBackendClass
{
  GObjectClass parent_class;

  /* Global backend methods: */
  void                   (*request_printer_list)            (GtkPrintBackend        *backend);
  void                   (*print_stream)                    (GtkPrintBackend        *backend,
							     GtkPrintJob            *job,
							     GIOChannel             *data_io,
							     GtkPrintJobCompleteFunc callback,
							     gpointer                user_data,
							     GDestroyNotify          dnotify);

  /* Printer methods: */
  void                  (*printer_request_details)           (GtkPrinter          *printer);
  cairo_surface_t *     (*printer_create_cairo_surface)      (GtkPrinter          *printer,
							      GtkPrintSettings    *settings,
							      double               height,
							      double               width,
							      GIOChannel          *cache_io);
  GtkPrinterOptionSet * (*printer_get_options)               (GtkPrinter          *printer,
							      GtkPrintSettings    *settings,
							      GtkPageSetup        *page_setup,
							      GtkPrintCapabilities capabilities);
  gboolean              (*printer_mark_conflicts)            (GtkPrinter          *printer,
							      GtkPrinterOptionSet *options);
  void                  (*printer_get_settings_from_options) (GtkPrinter          *printer,
							      GtkPrinterOptionSet *options,
							      GtkPrintSettings    *settings);
  void                  (*printer_prepare_for_print)         (GtkPrinter          *printer,
							      GtkPrintJob         *print_job,
							      GtkPrintSettings    *settings,
							      GtkPageSetup        *page_setup);
  GList  *              (*printer_list_papers)               (GtkPrinter          *printer);
  GtkPageSetup *        (*printer_get_default_page_size)     (GtkPrinter          *printer);
  gboolean              (*printer_get_hard_margins)          (GtkPrinter          *printer,
							      double              *top,
							      double              *bottom,
							      double              *left,
							      double              *right);
  GtkPrintCapabilities  (*printer_get_capabilities)          (GtkPrinter          *printer);

  /* Signals */
  void                  (*printer_list_changed)              (GtkPrintBackend     *backend);
  void                  (*printer_list_done)                 (GtkPrintBackend     *backend);
  void                  (*printer_added)                     (GtkPrintBackend     *backend,
							      GtkPrinter          *printer);
  void                  (*printer_removed)                   (GtkPrintBackend     *backend,
							      GtkPrinter          *printer);
  void                  (*printer_status_changed)            (GtkPrintBackend     *backend,
							      GtkPrinter          *printer);
  void                  (*request_password)                  (GtkPrintBackend     *backend,
                                                              gpointer             auth_info_required,
                                                              gpointer             auth_info_default,
                                                              gpointer             auth_info_display,
                                                              gpointer             auth_info_visible,
                                                              const char          *prompt,
                                                              gboolean             can_store_auth_info);

  /* not a signal */
  void                  (*set_password)                      (GtkPrintBackend     *backend,
                                                              char               **auth_info_required,
                                                              char               **auth_info,
                                                              gboolean             store_auth_info);

  gboolean              (*printer_get_hard_margins_for_paper_size) (GtkPrinter    *printer,
								    GtkPaperSize  *paper_size,
								    double        *top,
								    double        *bottom,
								    double        *left,
								    double        *right);
};

#define GTK_PRINT_BACKEND_EXTENSION_POINT_NAME "gtk-print-backend"

GDK_AVAILABLE_IN_ALL
GType   gtk_print_backend_get_type       (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GList      *gtk_print_backend_get_printer_list     (GtkPrintBackend         *print_backend);
GDK_AVAILABLE_IN_ALL
GListModel *gtk_print_backend_get_printers         (GtkPrintBackend         *print_backend);
GDK_AVAILABLE_IN_ALL
gboolean    gtk_print_backend_printer_list_is_done (GtkPrintBackend         *print_backend);
GDK_AVAILABLE_IN_ALL
GtkPrinter *gtk_print_backend_find_printer         (GtkPrintBackend         *print_backend,
						    const char              *printer_name);
GDK_AVAILABLE_IN_ALL
void        gtk_print_backend_print_stream         (GtkPrintBackend         *print_backend,
						    GtkPrintJob             *job,
						    GIOChannel              *data_io,
						    GtkPrintJobCompleteFunc  callback,
						    gpointer                 user_data,
						    GDestroyNotify           dnotify);
GDK_AVAILABLE_IN_ALL
GList *     gtk_print_backend_load_modules         (void);
GDK_AVAILABLE_IN_ALL
void        gtk_print_backend_destroy              (GtkPrintBackend         *print_backend);
GDK_AVAILABLE_IN_ALL
void        gtk_print_backend_set_password         (GtkPrintBackend         *backend, 
                                                    char                   **auth_info_required,
                                                    char                   **auth_info,
                                                    gboolean                 can_store_auth_info);

/* Backend-only functions for GtkPrintBackend */

GDK_AVAILABLE_IN_ALL
void        gtk_print_backend_add_printer          (GtkPrintBackend         *print_backend,
						    GtkPrinter              *printer);
GDK_AVAILABLE_IN_ALL
void        gtk_print_backend_remove_printer       (GtkPrintBackend         *print_backend,
						    GtkPrinter              *printer);
GDK_AVAILABLE_IN_ALL
void        gtk_print_backend_set_list_done        (GtkPrintBackend         *backend);


/* Backend-only functions for GtkPrinter */
GDK_AVAILABLE_IN_ALL
gboolean    gtk_printer_is_new                (GtkPrinter      *printer);
GDK_AVAILABLE_IN_ALL
void        gtk_printer_set_accepts_pdf       (GtkPrinter      *printer,
					       gboolean         val);
GDK_AVAILABLE_IN_ALL
void        gtk_printer_set_accepts_ps        (GtkPrinter      *printer,
					       gboolean         val);
GDK_AVAILABLE_IN_ALL
void        gtk_printer_set_is_new            (GtkPrinter      *printer,
					       gboolean         val);
GDK_AVAILABLE_IN_ALL
void        gtk_printer_set_is_active         (GtkPrinter      *printer,
					       gboolean         val);
GDK_AVAILABLE_IN_ALL
gboolean    gtk_printer_set_is_paused         (GtkPrinter      *printer,
					       gboolean         val);
GDK_AVAILABLE_IN_ALL
gboolean    gtk_printer_set_is_accepting_jobs (GtkPrinter      *printer,
					       gboolean         val);
GDK_AVAILABLE_IN_ALL
void        gtk_printer_set_has_details       (GtkPrinter      *printer,
					       gboolean         val);
GDK_AVAILABLE_IN_ALL
void        gtk_printer_set_is_default        (GtkPrinter      *printer,
					       gboolean         val);
GDK_AVAILABLE_IN_ALL
void        gtk_printer_set_icon_name         (GtkPrinter      *printer,
					       const char      *icon);
GDK_AVAILABLE_IN_ALL
gboolean    gtk_printer_set_job_count         (GtkPrinter      *printer,
					       int              count);
GDK_AVAILABLE_IN_ALL
gboolean    gtk_printer_set_location          (GtkPrinter      *printer,
					       const char      *location);
GDK_AVAILABLE_IN_ALL
gboolean    gtk_printer_set_description       (GtkPrinter      *printer,
					       const char      *description);
GDK_AVAILABLE_IN_ALL
gboolean    gtk_printer_set_state_message     (GtkPrinter      *printer,
					       const char      *message);

void        gtk_print_backends_init (void);


G_END_DECLS

