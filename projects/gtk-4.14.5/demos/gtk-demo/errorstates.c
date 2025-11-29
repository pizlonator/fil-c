/* Error States
 *
 * GtkLabel and GtkEntry can indicate errors if you set the .error
 * style class on them.
 *
 * This examples shows how this can be used in a dialog for input validation.
 *
 * It also shows how pass callbacks and objects to GtkBuilder with
 * GtkBuilderScope and gtk_builder_expose_object().
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static void
validate_more_details (GtkEntry   *entry,
                       GParamSpec *pspec,
                       GtkEntry   *details)
{
  if (strlen (gtk_editable_get_text (GTK_EDITABLE (entry))) > 0 &&
      strlen (gtk_editable_get_text (GTK_EDITABLE (details))) == 0)
    {
      gtk_widget_set_tooltip_text (GTK_WIDGET (entry), "Must have details first");
      gtk_widget_add_css_class (GTK_WIDGET (entry), "error");
      gtk_accessible_update_state (GTK_ACCESSIBLE (entry),
                                   GTK_ACCESSIBLE_STATE_INVALID, GTK_ACCESSIBLE_INVALID_TRUE,
                                   -1);
    }
  else
    {
      gtk_widget_set_tooltip_text (GTK_WIDGET (entry), "");
      gtk_widget_remove_css_class (GTK_WIDGET (entry), "error");
      gtk_accessible_reset_state (GTK_ACCESSIBLE (entry), GTK_ACCESSIBLE_STATE_INVALID);
    }
}

static gboolean
mode_switch_state_set (GtkSwitch *sw,
                       gboolean   state,
                       GtkWidget *scale)
{
  GtkWidget *label;

  label = GTK_WIDGET (g_object_get_data (G_OBJECT (sw), "error_label"));

  if (!state ||
      (gtk_range_get_value (GTK_RANGE (scale)) > 50))
    {
      gtk_widget_set_visible (label, FALSE);
      gtk_switch_set_state (sw, state);
      gtk_accessible_reset_relation (GTK_ACCESSIBLE (sw), GTK_ACCESSIBLE_RELATION_ERROR_MESSAGE);
      gtk_accessible_reset_state (GTK_ACCESSIBLE (sw), GTK_ACCESSIBLE_STATE_INVALID);
    }
  else
    {
      gtk_widget_set_visible (label, TRUE);
      gtk_accessible_update_relation (GTK_ACCESSIBLE (sw),
                                      GTK_ACCESSIBLE_RELATION_ERROR_MESSAGE, label,
                                      -1);
      gtk_accessible_update_state (GTK_ACCESSIBLE (sw),
                                   GTK_ACCESSIBLE_STATE_INVALID, GTK_ACCESSIBLE_INVALID_TRUE,
                                   -1);
    }

  return TRUE;
}

static void
level_scale_value_changed (GtkRange *range,
                           GtkWidget *sw)
{
  GtkWidget *label;

  label = GTK_WIDGET (g_object_get_data (G_OBJECT (sw), "error_label"));

  if (gtk_switch_get_active (GTK_SWITCH (sw)) &&
      !gtk_switch_get_state (GTK_SWITCH (sw)) &&
      (gtk_range_get_value (range) > 50))
    {
      gtk_widget_set_visible (label, FALSE);
      gtk_switch_set_state (GTK_SWITCH (sw), TRUE);
    }
  else if (gtk_switch_get_state (GTK_SWITCH (sw)) &&
          (gtk_range_get_value (range) <= 50))
    {
      gtk_switch_set_state (GTK_SWITCH (sw), FALSE);
    }

  gtk_accessible_reset_relation (GTK_ACCESSIBLE (sw), GTK_ACCESSIBLE_RELATION_ERROR_MESSAGE);
  gtk_accessible_reset_state (GTK_ACCESSIBLE (sw), GTK_ACCESSIBLE_STATE_INVALID);
}

GtkWidget *
do_errorstates (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *toplevel;
      GtkBuilder *builder;
      GtkBuilderScope *scope;
      GtkWidget *sw, *label;

      toplevel = GTK_WIDGET (gtk_widget_get_root (do_widget));

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback (scope, validate_more_details);
      gtk_builder_cscope_add_callback (scope, mode_switch_state_set);
      gtk_builder_cscope_add_callback (scope, level_scale_value_changed);
      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);
      gtk_builder_expose_object (builder, "toplevel", G_OBJECT (toplevel));
      gtk_builder_add_from_resource (builder, "/errorstates/errorstates.ui", NULL);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "dialog"));

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      sw = GTK_WIDGET (gtk_builder_get_object (builder, "mode_switch"));
      label = GTK_WIDGET (gtk_builder_get_object (builder, "error_label"));
      g_object_set_data (G_OBJECT (sw), "error_label", label);

      g_object_unref (builder);
      g_object_unref (scope);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
