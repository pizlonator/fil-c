/*
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>
#include <gtk/gtk-a11y.h>

G_BEGIN_DECLS

#define VTE_TYPE_TERMINAL_ACCESSIBLE            (_vte_terminal_accessible_get_type ())
#define VTE_TERMINAL_ACCESSIBLE(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), VTE_TYPE_TERMINAL_ACCESSIBLE, VteTerminalAccessible))
#define VTE_TERMINAL_ACCESSIBLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VTE_TYPE_TERMINAL_ACCESSIBLE, VteTerminalAccessibleClass))
#define VTE_IS_TERMINAL_ACCESSIBLE(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), VTE_TYPE_TERMINAL_ACCESSIBLE))
#define VTE_IS_TERMINAL_ACCESSIBLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VTE_TYPE_TERMINAL_ACCESSIBLE))
#define VTE_TERMINAL_ACCESSIBLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), VTE_TYPE_TERMINAL_ACCESSIBLE, VteTerminalAccessibleClass))

typedef struct _VteTerminalAccessible      VteTerminalAccessible;
typedef struct _VteTerminalAccessibleClass VteTerminalAccessibleClass;

/**
 * VteTerminalAccessible:
 *
 * The accessible peer for #VteTerminal.
 */
struct _VteTerminalAccessible {
	GtkWidgetAccessible parent;
};

struct _VteTerminalAccessibleClass {
	GtkWidgetAccessibleClass parent_class;
};

GType _vte_terminal_accessible_get_type(void);

void _vte_terminal_accessible_text_modified(VteTerminalAccessible* accessible);

void _vte_terminal_accessible_text_inserted(VteTerminalAccessible* accessible);

void _vte_terminal_accessible_text_deleted (VteTerminalAccessible* accessible);

void _vte_terminal_accessible_text_scrolled(VteTerminalAccessible* accessible,
                                            long delta);

G_END_DECLS
