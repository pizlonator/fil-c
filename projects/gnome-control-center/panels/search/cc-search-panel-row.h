/*
 * Copyright © 2019 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Felipe Borges <felipeborges@gnome.org>
 */

#pragma once

#include <gio/gdesktopappinfo.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_SEARCH_PANEL_ROW (cc_search_panel_row_get_type())

G_DECLARE_FINAL_TYPE (CcSearchPanelRow, cc_search_panel_row, CC, SEARCH_PANEL_ROW, AdwActionRow)


CcSearchPanelRow *cc_search_panel_row_new          (GAppInfo *app_info);

GAppInfo         *cc_search_panel_row_get_app_info (CcSearchPanelRow *row);

GtkWidget        *cc_search_panel_row_get_switch   (CcSearchPanelRow *row);

G_END_DECLS
