/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Written by: Ondrej Holy <oholy@redhat.com>,
 *             Felipe Borges <felipeborges@gnome.org>,
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

#include <config.h>

#include <glib/gi18n.h>

#include "cc-mouse-test.h"

struct _CcMouseTest
{
    AdwWindow  parent_instance;

    GtkWidget *arrow_down;
    GtkWidget *arrow_up;
    GtkWidget *primary_click_image;
    GtkWidget *secondary_click_image;
    GtkWidget *double_click_image;
    GtkWidget *image;
    GtkAdjustment *scrolled_window_adjustment;
    GtkWidget *viewport;

    gint double_click_delay;
    guint reset_timeout_id;
    guint primary_timeout_id;
};

G_DEFINE_TYPE (CcMouseTest, cc_mouse_test, ADW_TYPE_WINDOW);

static void
on_scroll_adjustment_changed_cb (GtkAdjustment *adjustment,
                                 gpointer       user_data)
{
    CcMouseTest *self = CC_MOUSE_TEST (user_data);
    gboolean is_bottom, is_top;
    gdouble value;

    value = gtk_adjustment_get_value (adjustment);
    is_top = value == gtk_adjustment_get_lower (adjustment);
    is_bottom = value == (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_page_size (adjustment));

    gtk_widget_set_visible (self->arrow_up, !is_top);
    gtk_widget_set_visible (self->arrow_down, !is_bottom);
}

static gboolean
reset_indicators (CcMouseTest *self)
{
    g_clear_handle_id (&self->reset_timeout_id, g_source_remove);

    gtk_widget_remove_css_class (self->primary_click_image, "success");
    gtk_widget_remove_css_class (self->secondary_click_image, "success");
    gtk_widget_remove_css_class (self->double_click_image, "success");

    return FALSE;
}

static gboolean
primary_click_timeout (CcMouseTest *self)
{
    g_clear_handle_id (&self->primary_timeout_id, g_source_remove);

    gtk_widget_add_css_class (self->primary_click_image, "success");

    return FALSE;
}

static void
on_test_button_clicked_cb (GtkGestureClick *gesture,
                           gint             n_press,
                           gdouble          x,
                           gdouble          y,
                           gpointer         user_data)
{
    CcMouseTest *self = CC_MOUSE_TEST (user_data);
    guint button;
    guint reset_indicators_delay = self->double_click_delay * 2;

    g_clear_handle_id (&self->reset_timeout_id, g_source_remove);
    g_clear_handle_id (&self->primary_timeout_id, g_source_remove);

    reset_indicators (self);

    button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
    if (button == GDK_BUTTON_SECONDARY) {
        gtk_widget_add_css_class (self->secondary_click_image, "success");
    } else if (button == GDK_BUTTON_PRIMARY && n_press == 2) {
        gtk_widget_add_css_class (self->double_click_image, "success");
    } else if (button == GDK_BUTTON_PRIMARY && n_press == 1) {
        self->primary_timeout_id =
            g_timeout_add (self->double_click_delay, (GSourceFunc) primary_click_timeout, self);
        reset_indicators_delay = self->double_click_delay * 3;
    }

    /* Reset the buttons to default state after double_click_delay * 2 for double or secondary
       click, or double_click_delay * 3 for primary click to always indicate equally long. */
    self->reset_timeout_id =
        g_timeout_add (reset_indicators_delay, (GSourceFunc) reset_indicators, self);
}

static void
on_mouse_test_show_cb (CcMouseTest *self)
{
    /* Always scroll back to the top */
    gtk_adjustment_set_value (self->scrolled_window_adjustment, 0);
}

static void
setup_dialog (CcMouseTest *self)
{
    g_autoptr(GtkCssProvider) provider = NULL;

    provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/mouse/mouse-test.css");
    gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                GTK_STYLE_PROVIDER (provider),
                                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
cc_mouse_test_finalize (GObject *object)
{
    CcMouseTest *self = CC_MOUSE_TEST (object);

    g_clear_handle_id (&self->reset_timeout_id, g_source_remove);
    g_clear_handle_id (&self->primary_timeout_id, g_source_remove);

    G_OBJECT_CLASS (cc_mouse_test_parent_class)->finalize (object);
}

static void
cc_mouse_test_class_init (CcMouseTestClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = cc_mouse_test_finalize;

    gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/mouse/cc-mouse-test.ui");

    gtk_widget_class_bind_template_child (widget_class, CcMouseTest, arrow_down);
    gtk_widget_class_bind_template_child (widget_class, CcMouseTest, arrow_up);
    gtk_widget_class_bind_template_child (widget_class, CcMouseTest, primary_click_image);
    gtk_widget_class_bind_template_child (widget_class, CcMouseTest, secondary_click_image);
    gtk_widget_class_bind_template_child (widget_class, CcMouseTest, double_click_image);
    gtk_widget_class_bind_template_child (widget_class, CcMouseTest, image);
    gtk_widget_class_bind_template_child (widget_class, CcMouseTest, scrolled_window_adjustment);
    gtk_widget_class_bind_template_child (widget_class, CcMouseTest, viewport);

    gtk_widget_class_bind_template_callback (widget_class, on_mouse_test_show_cb);
    gtk_widget_class_bind_template_callback (widget_class, on_scroll_adjustment_changed_cb);
    gtk_widget_class_bind_template_callback (widget_class, on_test_button_clicked_cb);
}

static void
cc_mouse_test_init (CcMouseTest *self)
{
    g_autoptr(GSettings) mouse_settings = NULL;

    gtk_widget_init_template (GTK_WIDGET (self));

    mouse_settings = g_settings_new ("org.gnome.desktop.peripherals.mouse");
    self->double_click_delay = g_settings_get_int (mouse_settings, "double-click");

    setup_dialog (self);
}

GtkWidget *
cc_mouse_test_new (void)
{
    return (GtkWidget *) g_object_new (CC_TYPE_MOUSE_TEST, NULL);
}
