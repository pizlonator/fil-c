/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2014 Red Hat, Inc.
 */

#include "config.h"

#include "ui-helpers.h"

void
widget_set_error (GtkWidget *widget)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));

	gtk_widget_add_css_class (widget, "error");
}

void
widget_unset_error (GtkWidget *widget)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));

	gtk_widget_remove_css_class (widget, "error");
}
