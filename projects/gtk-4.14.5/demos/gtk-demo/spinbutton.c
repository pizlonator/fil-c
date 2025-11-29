/* Spin Buttons
 * #Keywords: GtkEntry
 *
 * GtkSpinButton provides convenient ways to input data
 * that can be seen as a value in a range. The examples
 * here show that this does not necessarily mean numeric
 * values, and it can include custom formatting.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdlib.h>

G_MODULE_EXPORT int
spinbutton_hex_spin_input (GtkSpinButton *spin_button,
                           double        *new_val)
{
  const char *buf;
  char *err;
  double res;

  buf = gtk_editable_get_text (GTK_EDITABLE (spin_button));
  res = strtol (buf, &err, 16);
  *new_val = res;
  if (*err)
    return GTK_INPUT_ERROR;
  else
    return TRUE;
}

G_MODULE_EXPORT int
spinbutton_hex_spin_output (GtkSpinButton *spin_button)
{
  GtkAdjustment *adjustment;
  char *buf;
  double val;

  adjustment = gtk_spin_button_get_adjustment (spin_button);
  val = gtk_adjustment_get_value (adjustment);
  if (fabs (val) < 1e-5)
    buf = g_strdup ("0x00");
  else
    buf = g_strdup_printf ("0x%.2X", (int) val);
  if (strcmp (buf, gtk_editable_get_text (GTK_EDITABLE (spin_button))))
    gtk_editable_set_text (GTK_EDITABLE (spin_button), buf);
  g_free (buf);

  return TRUE;
}

G_MODULE_EXPORT int
spinbutton_time_spin_input (GtkSpinButton *spin_button,
                            double        *new_val)
{
  const char *text;
  char **str;
  gboolean found = FALSE;
  int hours;
  int minutes;
  char *endh;
  char *endm;

  text = gtk_editable_get_text (GTK_EDITABLE (spin_button));
  str = g_strsplit (text, ":", 2);

  if (g_strv_length (str) == 2)
    {
      hours = strtol (str[0], &endh, 10);
      minutes = strtol (str[1], &endm, 10);
      if (!*endh && !*endm &&
          0 <= hours && hours < 24 &&
          0 <= minutes && minutes < 60)
        {
          *new_val = hours * 60 + minutes;
          found = TRUE;
        }
    }

  g_strfreev (str);

  if (!found)
    {
      *new_val = 0.0;
      return GTK_INPUT_ERROR;
    }

  return TRUE;
}

G_MODULE_EXPORT int
spinbutton_time_spin_output (GtkSpinButton *spin_button)
{
  GtkAdjustment *adjustment;
  char *buf;
  double hours;
  double minutes;

  adjustment = gtk_spin_button_get_adjustment (spin_button);
  hours = gtk_adjustment_get_value (adjustment) / 60.0;
  minutes = (hours - floor (hours)) * 60.0;
  buf = g_strdup_printf ("%02.0f:%02.0f", floor (hours), floor (minutes + 0.5));
  if (strcmp (buf, gtk_editable_get_text (GTK_EDITABLE (spin_button))))
    gtk_editable_set_text (GTK_EDITABLE (spin_button), buf);
  g_free (buf);

  return TRUE;
}

static const char *month[12] = {
  "January",
  "February",
  "March",
  "April",
  "May",
  "June",
  "July",
  "August",
  "September",
  "October",
  "November",
  "December"
};

G_MODULE_EXPORT int
spinbutton_month_spin_input (GtkSpinButton *spin_button,
                             double        *new_val)
{
  int i;
  char *tmp1, *tmp2;
  gboolean found = FALSE;

  for (i = 1; i <= 12; i++)
    {
      tmp1 = g_ascii_strup (month[i - 1], -1);
      tmp2 = g_ascii_strup (gtk_editable_get_text (GTK_EDITABLE (spin_button)), -1);
      if (strstr (tmp1, tmp2) == tmp1)
        found = TRUE;
      g_free (tmp1);
      g_free (tmp2);
      if (found)
        break;
    }
  if (!found)
    {
      *new_val = 0.0;
      return GTK_INPUT_ERROR;
    }
  *new_val = (double) i;

  return TRUE;
}

G_MODULE_EXPORT int
spinbutton_month_spin_output (GtkSpinButton *spin_button)
{
  GtkAdjustment *adjustment;
  double value;
  int i;

  adjustment = gtk_spin_button_get_adjustment (spin_button);
  value = gtk_adjustment_get_value (adjustment);
  for (i = 1; i <= 12; i++)
    if (fabs (value - (double)i) < 1e-5)
      {
        if (strcmp (month[i-1], gtk_editable_get_text (GTK_EDITABLE (spin_button))))
          gtk_editable_set_text (GTK_EDITABLE (spin_button), month[i-1]);
      }

  return TRUE;
}

static gboolean
value_to_label (GBinding     *binding,
                const GValue *from,
                GValue       *to,
                gpointer      user_data)
{
  g_value_take_string (to, g_strdup_printf ("%g", g_value_get_double (from)));
  return TRUE;
}

GtkWidget *
do_spinbutton (GtkWidget *do_widget)
{
  static GtkWidget *window;

  if (!window)
    {
      GtkBuilder *builder;
      GtkBuilderScope *scope;
      GtkAdjustment *adj;
      GtkWidget *label;

      scope = gtk_builder_cscope_new ();
      builder = gtk_builder_new ();
      gtk_builder_cscope_add_callback (GTK_BUILDER_CSCOPE (scope), spinbutton_hex_spin_input);
      gtk_builder_cscope_add_callback (GTK_BUILDER_CSCOPE (scope), spinbutton_hex_spin_output);
      gtk_builder_cscope_add_callback (GTK_BUILDER_CSCOPE (scope), spinbutton_time_spin_input);
      gtk_builder_cscope_add_callback (GTK_BUILDER_CSCOPE (scope), spinbutton_time_spin_output);
      gtk_builder_cscope_add_callback (GTK_BUILDER_CSCOPE (scope), spinbutton_month_spin_input);
      gtk_builder_cscope_add_callback (GTK_BUILDER_CSCOPE (scope), spinbutton_month_spin_output);
      gtk_builder_set_scope (builder, scope);
      gtk_builder_add_from_resource (builder, "/spinbutton/spinbutton.ui", NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Spin Buttons");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      adj = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "basic_adjustment"));
      label = GTK_WIDGET (gtk_builder_get_object (builder, "basic_label"));
      g_object_bind_property_full (adj, "value",
                                   label, "label",
                                   G_BINDING_SYNC_CREATE,
                                   value_to_label,
                                   NULL,
                                   NULL, NULL);
      adj = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "hex_adjustment"));
      label = GTK_WIDGET (gtk_builder_get_object (builder, "hex_label"));
      g_object_bind_property_full (adj, "value",
                                   label, "label",
                                   G_BINDING_SYNC_CREATE,
                                   value_to_label,
                                   NULL,
                                   NULL, NULL);
      adj = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "time_adjustment"));
      label = GTK_WIDGET (gtk_builder_get_object (builder, "time_label"));
      g_object_bind_property_full (adj, "value",
                                   label, "label",
                                   G_BINDING_SYNC_CREATE,
                                   value_to_label,
                                   NULL,
                                   NULL, NULL);
      adj = GTK_ADJUSTMENT (gtk_builder_get_object (builder, "month_adjustment"));
      label = GTK_WIDGET (gtk_builder_get_object (builder, "month_label"));
      g_object_bind_property_full (adj, "value",
                                   label, "label",
                                   G_BINDING_SYNC_CREATE,
                                   value_to_label,
                                   NULL,
                                   NULL, NULL);

      g_object_unref (builder);
      g_object_unref (scope);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
