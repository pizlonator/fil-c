/*
 * Copyright © 2008, 2010, 2012 Christian Persch
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
 */

#ifndef TERMINAL_NOTEBOOK_H
#define TERMINAL_NOTEBOOK_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TERMINAL_TYPE_NOTEBOOK         (terminal_notebook_get_type ())
#define TERMINAL_NOTEBOOK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TERMINAL_TYPE_NOTEBOOK, TerminalNotebook))
#define TERMINAL_NOTEBOOK_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TERMINAL_TYPE_NOTEBOOK, TerminalNotebookClass))
#define TERMINAL_IS_NOTEBOOK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TERMINAL_TYPE_NOTEBOOK))
#define TERMINAL_IS_NOTEBOOK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TERMINAL_TYPE_NOTEBOOK))
#define TERMINAL_NOTEBOOK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TERMINAL_TYPE_NOTEBOOK, TerminalNotebookClass))

typedef struct _TerminalNotebook        TerminalNotebook;
typedef struct _TerminalNotebookClass   TerminalNotebookClass;
typedef struct _TerminalNotebookPrivate TerminalNotebookPrivate;

struct _TerminalNotebook
{
  GtkNotebook parent_instance;

  /*< private >*/
  TerminalNotebookPrivate *priv;
};

struct _TerminalNotebookClass
{
  GtkNotebookClass parent_class;
};

GType terminal_notebook_get_type (void);

GtkWidget *terminal_notebook_new (void);

void terminal_notebook_set_tab_policy (TerminalNotebook *notebook,
                                       GtkPolicyType policy);
GtkPolicyType terminal_notebook_get_tab_policy (TerminalNotebook *notebook);

GtkWidget *terminal_notebook_get_action_box (TerminalNotebook *notebook,
                                             GtkPackType pack_type);

G_END_DECLS

#endif /* TERMINAL_NOTEBOOK_H */
