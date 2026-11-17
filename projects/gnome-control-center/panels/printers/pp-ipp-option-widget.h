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

#include <gtk/gtk.h>
#include <cups/cups.h>
#include <cups/ppd.h>

#include "pp-utils.h"

G_BEGIN_DECLS

#define PP_TYPE_IPP_OPTION_WIDGET (pp_ipp_option_widget_get_type ())
G_DECLARE_FINAL_TYPE (PpIPPOptionWidget, pp_ipp_option_widget, PP, IPP_OPTION_WIDGET, GtkBox)

GtkWidget   *pp_ipp_option_widget_new (IPPAttribute *attr_supported,
                                       IPPAttribute *attr_default,
                                       const gchar  *option_name,
                                       const gchar  *printer);

G_END_DECLS
