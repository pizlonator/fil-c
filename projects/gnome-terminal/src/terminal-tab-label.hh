/*
 *  Copyright © 2008 Christian Persch
 *
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

#ifndef TERMINAL_TAB_LABEL_H
#define TERMINAL_TAB_LABEL_H

#include <gtk/gtk.h>

#include "terminal-screen.hh"

G_BEGIN_DECLS

#define TERMINAL_TYPE_TAB_LABEL         (terminal_tab_label_get_type ())
#define TERMINAL_TAB_LABEL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TERMINAL_TYPE_TAB_LABEL, TerminalTabLabel))
#define TERMINAL_TAB_LABEL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TERMINAL_TYPE_TAB_LABEL, TerminalTabLabelClass))
#define TERMINAL_IS_TAB_LABEL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TERMINAL_TYPE_TAB_LABEL))
#define TERMINAL_IS_TAB_LABEL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TERMINAL_TYPE_TAB_LABEL))
#define TERMINAL_TAB_LABEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TERMINAL_TYPE_TAB_LABEL, TerminalTabLabelClass))

typedef struct _TerminalTabLabel        TerminalTabLabel;
typedef struct _TerminalTabLabelClass   TerminalTabLabelClass;
typedef struct _TerminalTabLabelPrivate TerminalTabLabelPrivate;

struct _TerminalTabLabel
{
  GtkBox parent_instance;

  /*< private >*/
  TerminalTabLabelPrivate *priv;
};

struct _TerminalTabLabelClass
{
  GtkBoxClass parent_class;

  /* Signals */
  void (* close_button_clicked) (TerminalTabLabel *tab_label);
};

GType           terminal_tab_label_get_type   (void);

GtkWidget *     terminal_tab_label_new        (TerminalScreen *screen);

void            terminal_tab_label_set_bold   (TerminalTabLabel *tab_label,
                                               gboolean bold);

TerminalScreen *terminal_tab_label_get_screen (TerminalTabLabel *tab_label);

G_END_DECLS

#endif /* !TERMINAL_TAB_LABEL_H */
