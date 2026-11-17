/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2020 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

#include "pp-job.h"

G_BEGIN_DECLS

#define PP_TYPE_JOB_ROW (pp_job_row_get_type())
G_DECLARE_FINAL_TYPE (PpJobRow, pp_job_row, PP, JOB_ROW, GtkListBoxRow)

PpJobRow* pp_job_row_new     (PpJob    *job);

PpJob*    pp_job_row_get_job (PpJobRow *self);

G_END_DECLS
