/* gtp-static-init.c
 *
 * Copyright 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "gtp-static-init.h"

struct _GtpStaticInit
{
  CcPanel parent;
};

G_DEFINE_TYPE (GtpStaticInit, gtp_static_init, CC_TYPE_PANEL)

void
gtp_static_init_func (void)
{
  g_message ("GtpStaticInit: running outside the panel instance");
}

static void
gtp_static_init_class_init (GtpStaticInitClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/tests/panels/gtp-static-init.ui");

}

static void
gtp_static_init_init (GtpStaticInit *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
