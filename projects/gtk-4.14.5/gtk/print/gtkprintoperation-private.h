/* GTK - The GIMP Toolkit
 * gtkprintoperation.h: Print Operation
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

#include "gtkprintoperation.h"

G_BEGIN_DECLS

/* Page drawing states */
typedef enum
{
  GTK_PAGE_DRAWING_STATE_READY,
  GTK_PAGE_DRAWING_STATE_DRAWING,
  GTK_PAGE_DRAWING_STATE_DEFERRED_DRAWING
} GtkPageDrawingState;

struct _GtkPrintOperationPrivate
{
  GtkPrintOperationAction action;
  GtkPrintStatus status;
  GError *error;
  char *status_string;
  GtkPageSetup *default_page_setup;
  GtkPrintSettings *print_settings;
  char *job_name;
  int nr_of_pages;
  int nr_of_pages_to_print;
  int page_position;
  int current_page;
  GtkUnit unit;
  char *export_filename;
  guint use_full_page      : 1;
  guint track_print_status : 1;
  guint show_progress      : 1;
  guint cancelled          : 1;
  guint allow_async        : 1;
  guint is_sync            : 1;
  guint support_selection  : 1;
  guint has_selection      : 1;
  guint embed_page_setup   : 1;

  GtkPageDrawingState      page_drawing_state;

  guint print_pages_idle_id;
  guint show_progress_timeout_id;

  GtkPrintContext *print_context;
  
  GtkPrintPages print_pages;
  GtkPageRange *page_ranges;
  int num_page_ranges;
  
  int manual_num_copies;
  guint manual_collation   : 1;
  guint manual_reverse     : 1;
  guint manual_orientation : 1;
  double manual_scale;
  GtkPageSet manual_page_set;
  guint manual_number_up;
  GtkNumberUpLayout manual_number_up_layout;

  GtkWidget *custom_widget;
  char *custom_tab_label;
  
  gpointer platform_data;
  GDestroyNotify free_platform_data;

  GMainLoop *rloop; /* recursive mainloop */

  void (*start_page) (GtkPrintOperation *operation,
		      GtkPrintContext   *print_context,
		      GtkPageSetup      *page_setup);
  void (*end_page)   (GtkPrintOperation *operation,
		      GtkPrintContext   *print_context);
  void (*end_run)    (GtkPrintOperation *operation,
		      gboolean           wait,
		      gboolean           cancelled);
};


typedef void (* GtkPrintOperationPrintFunc) (GtkPrintOperation      *op,
					     GtkWindow              *parent,
					     gboolean                do_print,
					     GtkPrintOperationResult result);

GtkPrintOperationResult _gtk_print_operation_platform_backend_run_dialog             (GtkPrintOperation           *operation,
										      gboolean                     show_dialog,
										      GtkWindow                   *parent,
										      gboolean                    *do_print);
void                    _gtk_print_operation_platform_backend_run_dialog_async       (GtkPrintOperation           *op,
										      gboolean                     show_dialog,
										      GtkWindow                   *parent,
										      GtkPrintOperationPrintFunc   print_cb);
void                    _gtk_print_operation_platform_backend_launch_preview         (GtkPrintOperation           *op,
										      cairo_surface_t             *surface,
										      GtkWindow                   *parent,
										      const char                  *filename);
cairo_surface_t *       _gtk_print_operation_platform_backend_create_preview_surface (GtkPrintOperation           *op,
										      GtkPageSetup                *page_setup,
										      double                      *dpi_x,
										      double                      *dpi_y,
										      char                        **target);
void                    _gtk_print_operation_platform_backend_resize_preview_surface (GtkPrintOperation           *op,
										      GtkPageSetup                *page_setup,
										      cairo_surface_t             *surface);
void                    _gtk_print_operation_platform_backend_preview_start_page     (GtkPrintOperation *op,
										      cairo_surface_t *surface,
										      cairo_t *cr);
void                    _gtk_print_operation_platform_backend_preview_end_page       (GtkPrintOperation *op,
										      cairo_surface_t *surface,
										      cairo_t *cr);

void _gtk_print_operation_set_status (GtkPrintOperation *op,
				      GtkPrintStatus     status,
				      const char        *string);

/* GtkPrintContext private functions: */

GtkPrintContext *_gtk_print_context_new                             (GtkPrintOperation *op);
void             _gtk_print_context_set_page_setup                  (GtkPrintContext   *context,
								     GtkPageSetup      *page_setup);
void             _gtk_print_context_translate_into_margin           (GtkPrintContext   *context);
void             _gtk_print_context_rotate_according_to_orientation (GtkPrintContext   *context);
void             _gtk_print_context_reverse_according_to_orientation (GtkPrintContext *context);
void             _gtk_print_context_set_hard_margins                (GtkPrintContext   *context,
								     double             top,
								     double             bottom,
								     double             left,
								     double             right);

G_END_DECLS

