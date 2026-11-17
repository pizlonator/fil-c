/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
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
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#pragma once

#include <adwaita.h>
#include "pp-utils.h"

G_BEGIN_DECLS

#define PP_TYPE_JOBS_DIALOG (pp_jobs_dialog_get_type ())
G_DECLARE_FINAL_TYPE (PpJobsDialog, pp_jobs_dialog, PP, JOBS_DIALOG, AdwWindow)

PpJobsDialog *pp_jobs_dialog_new               (const gchar  *printer_name);
void          pp_jobs_dialog_update            (PpJobsDialog *dialog);
void          pp_jobs_dialog_authenticate_jobs (PpJobsDialog *dialog);

G_END_DECLS
