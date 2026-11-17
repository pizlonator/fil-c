/* -*- mode: c; style: linux -*-
 * 
 * Copyright (C) 2017 Red Hat, Inc.
 *
 * Written by: Benjamin Berg <bberg@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>
#include "cc-display-config.h"

G_BEGIN_DECLS

#define CC_TYPE_DISPLAY_ARRANGEMENT cc_display_arrangement_get_type ()
G_DECLARE_FINAL_TYPE (CcDisplayArrangement, cc_display_arrangement, CC, DISPLAY_ARRANGEMENT, GtkDrawingArea);

CcDisplayArrangement* cc_display_arrangement_new                 (CcDisplayConfig      *config);

CcDisplayConfig*      cc_display_arrangement_get_config          (CcDisplayArrangement *self);
void                  cc_display_arrangement_set_config          (CcDisplayArrangement *self,
                                                                  CcDisplayConfig      *config);

CcDisplayMonitor*     cc_display_arrangement_get_selected_output (CcDisplayArrangement *arr);
void                  cc_display_arrangement_set_selected_output (CcDisplayArrangement *arr,
                                                                  CcDisplayMonitor     *output);

/* This is a bit of an odd-ball, but it currently makes sense to have it with
 * the arrangement widget where the snapping code lives. */
void                  cc_display_config_snap_outputs             (CcDisplayConfig  *config);

G_END_DECLS

