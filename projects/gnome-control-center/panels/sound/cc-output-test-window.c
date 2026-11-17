/*
 * Copyright (C) 2022 Marco Melorio
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "cc-output-test-wheel.h"
#include "cc-output-test-window.h"

struct _CcOutputTestWindow
{
  AdwWindow          parent_instance;

  CcOutputTestWheel *wheel;
};

G_DEFINE_TYPE (CcOutputTestWindow, cc_output_test_window, ADW_TYPE_WINDOW)

void
cc_output_test_window_class_init (CcOutputTestWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/sound/cc-output-test-window.ui");

  gtk_widget_class_bind_template_child (widget_class, CcOutputTestWindow, wheel);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

  g_type_ensure (CC_TYPE_OUTPUT_TEST_WHEEL);
}

void
cc_output_test_window_init (CcOutputTestWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcOutputTestWindow *
cc_output_test_window_new (GvcMixerStream *stream)
{
  CcOutputTestWindow *self;

  self = g_object_new (CC_TYPE_OUTPUT_TEST_WINDOW, NULL);

  cc_output_test_wheel_set_stream (self->wheel, stream);

  return self;
}
