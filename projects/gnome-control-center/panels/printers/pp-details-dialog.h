/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2016  Red Hat, Inc,
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
 * Author: Felipe Borges <feborges@redhat.com>
 */

#pragma once

#include <adwaita.h>
#include "pp-utils.h"

G_BEGIN_DECLS

#define PP_DETAILS_DIALOG_TYPE (pp_details_dialog_get_type ())
G_DECLARE_FINAL_TYPE (PpDetailsDialog, pp_details_dialog, PP, DETAILS_DIALOG, AdwWindow)

PpDetailsDialog *pp_details_dialog_new                  (gchar   *printer_name,
                                                         gchar   *printer_location,
                                                         gchar   *printer_address,
                                                         gchar   *printer_make_and_model,
                                                         gboolean sensitive);

const gchar     *pp_details_dialog_get_printer_name     (PpDetailsDialog *dialog);

const gchar     *pp_details_dialog_get_printer_location (PpDetailsDialog *dialog);

G_END_DECLS
