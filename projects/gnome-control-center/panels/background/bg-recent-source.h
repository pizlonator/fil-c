/* bg-recent-source.h
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "bg-source.h"
#include "cc-background-item.h"

G_BEGIN_DECLS

#define BG_TYPE_RECENT_SOURCE (bg_recent_source_get_type())
G_DECLARE_FINAL_TYPE (BgRecentSource, bg_recent_source, BG, RECENT_SOURCE, BgSource)

BgRecentSource* bg_recent_source_new         (void);

void            bg_recent_source_add_file    (BgRecentSource   *self,
                                              const gchar      *path);

void            bg_recent_source_remove_item (BgRecentSource   *self,
                                              CcBackgroundItem *item);

G_END_DECLS
