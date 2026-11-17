/* cc-illustrated-row.h
 *
 * Copyright 2018 Purism SPC
 *           2021 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *           2023 Red Hat, Inc
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

#include <adwaita.h>

#include "cc-vertical-row.h"

G_BEGIN_DECLS

#define CC_TYPE_ILLUSTRATED_ROW (cc_illustrated_row_get_type())
G_DECLARE_FINAL_TYPE (CcIllustratedRow, cc_illustrated_row, CC, ILLUSTRATED_ROW, CcVerticalRow)

struct _CcIllustratedRowClass
{
  CcVerticalRowClass parent_class;

  /*< private >*/
  gpointer padding[4];
};

void         cc_illustrated_row_set_resource (CcIllustratedRow *self,
                                              const gchar      *resource_path);

G_END_DECLS
