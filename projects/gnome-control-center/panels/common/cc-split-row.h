/* cc-vertical-row.h
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

#define CC_TYPE_SPLIT_ROW (cc_split_row_get_type())
G_DECLARE_FINAL_TYPE (CcSplitRow, cc_split_row, CC, SPLIT_ROW, CcVerticalRow)

struct _CcSplitRowClass
{
  CcVerticalRowClass parent_class;

  /*< private >*/
  gpointer padding[4];
};

void         cc_split_row_set_default_illustration_resource (CcSplitRow  *self,
                                                             const gchar *resource_path);

const gchar *cc_split_row_get_default_illustration_resource (CcSplitRow *self);

void         cc_split_row_set_alternative_illustration_resource (CcSplitRow  *self,
                                                                 const gchar *resource_path);

const gchar *cc_split_row_get_alternative_illustration_resource (CcSplitRow *self);

void         cc_split_row_set_use_default (CcSplitRow *self,
                                           gboolean    use_default);

gboolean     cc_split_row_get_use_default (CcSplitRow *self);

void         cc_split_row_set_compact (CcSplitRow *self,
                                       gboolean    compact);

gboolean     cc_split_row_get_compact (CcSplitRow *self);

const gchar *cc_split_row_get_default_option_title (CcSplitRow *self);

void         cc_split_row_set_default_option_title (CcSplitRow  *self,
                                                    const gchar *title);
const gchar *cc_split_row_get_default_option_subtitle (CcSplitRow *self);

void         cc_split_row_set_default_option_subtitle (CcSplitRow  *self,
                                                       const gchar *subtitle);
const gchar *cc_split_row_get_alternative_option_title (CcSplitRow *self);

void         cc_split_row_set_alternative_option_title (CcSplitRow  *self,
                                                        const gchar *title);
const gchar *cc_split_row_get_alternative_option_subtitle (CcSplitRow *self);

void         cc_split_row_set_alternative_option_subtitle (CcSplitRow  *self,
                                                           const gchar *subtitle);

G_END_DECLS
