/* GTK - The GIMP Toolkit
 * gtkprintbackendprivate.h: Abstract printer backend interfaces
 * Copyright (C) 2006, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <string.h>
#include <glib.h>

#include "gtkprinteroptionsetprivate.h"

/*****************************************
 *         GtkPrinterOptionSet    *
 *****************************************/

enum {
  CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* ugly side-effect of aliasing */
#undef gtk_printer_option_set

G_DEFINE_TYPE (GtkPrinterOptionSet, gtk_printer_option_set, G_TYPE_OBJECT)

static void
gtk_printer_option_set_finalize (GObject *object)
{
  GtkPrinterOptionSet *set = GTK_PRINTER_OPTION_SET (object);

  g_hash_table_destroy (set->hash);
  g_ptr_array_foreach (set->array, (GFunc)g_object_unref, NULL);
  g_ptr_array_free (set->array, TRUE);
  
  G_OBJECT_CLASS (gtk_printer_option_set_parent_class)->finalize (object);
}

static void
gtk_printer_option_set_init (GtkPrinterOptionSet *set)
{
  set->array = g_ptr_array_new ();
  set->hash = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
gtk_printer_option_set_class_init (GtkPrinterOptionSetClass *class)
{
  GObjectClass *gobject_class = (GObjectClass *)class;

  gobject_class->finalize = gtk_printer_option_set_finalize;

  signals[CHANGED] =
    g_signal_new ("changed",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrinterOptionSetClass, changed),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);
}


static void
emit_changed (GtkPrinterOptionSet *set)
{
  g_signal_emit (set, signals[CHANGED], 0);
}

GtkPrinterOptionSet *
gtk_printer_option_set_new (void)
{
  return g_object_new (GTK_TYPE_PRINTER_OPTION_SET, NULL);
}

void
gtk_printer_option_set_remove (GtkPrinterOptionSet *set,
			       GtkPrinterOption    *option)
{
  int i;
  
  for (i = 0; i < set->array->len; i++)
    {
      if (g_ptr_array_index (set->array, i) == option)
	{
	  g_ptr_array_remove_index (set->array, i);
	  g_hash_table_remove (set->hash, option->name);
	  g_signal_handlers_disconnect_by_func (option, emit_changed, set);

	  g_object_unref (option);
	  break;
	}
    }
}

void
gtk_printer_option_set_add (GtkPrinterOptionSet *set,
			    GtkPrinterOption    *option)
{
  g_object_ref (option);
  
  if (gtk_printer_option_set_lookup (set, option->name))
    gtk_printer_option_set_remove (set, option);
    
  g_ptr_array_add (set->array, option);
  g_hash_table_insert (set->hash, option->name, option);
  g_signal_connect_object (option, "changed", G_CALLBACK (emit_changed), set, G_CONNECT_SWAPPED);
}

GtkPrinterOption *
gtk_printer_option_set_lookup (GtkPrinterOptionSet *set,
			       const char          *name)
{
  gpointer ptr;

  ptr = g_hash_table_lookup (set->hash, name);

  return GTK_PRINTER_OPTION (ptr);
}

void
gtk_printer_option_set_clear_conflicts (GtkPrinterOptionSet *set)
{
  gtk_printer_option_set_foreach (set,
				  (GtkPrinterOptionSetFunc)gtk_printer_option_clear_has_conflict,
				  NULL);
}

/**
 * gtk_printer_option_set_get_groups:
 * @set: a `GtkPrinterOptionSet`
 *
 * Gets the groups in this set.
 *
 * Returns: (element-type utf8) (transfer full): a list of group names.
 */
GList *
gtk_printer_option_set_get_groups (GtkPrinterOptionSet *set)
{
  GtkPrinterOption *option;
  GList *list = NULL;
  int i;

  for (i = 0; i < set->array->len; i++)
    {
      option = g_ptr_array_index (set->array, i);

      if (g_list_find_custom (list, option->group, (GCompareFunc)g_strcmp0) == NULL)
	list = g_list_prepend (list, g_strdup (option->group));
    }

  return g_list_reverse (list);
}

void
gtk_printer_option_set_foreach_in_group (GtkPrinterOptionSet     *set,
					 const char              *group,
					 GtkPrinterOptionSetFunc  func,
					 gpointer                 user_data)
{
  GtkPrinterOption *option;
  int i;

  for (i = 0; i < set->array->len; i++)
    {
      option = g_ptr_array_index (set->array, i);

      if (group == NULL || g_strcmp0 (group, option->group) == 0)
	func (option, user_data);
    }
}

void
gtk_printer_option_set_foreach (GtkPrinterOptionSet *set,
				GtkPrinterOptionSetFunc func,
				gpointer user_data)
{
  gtk_printer_option_set_foreach_in_group (set, NULL, func, user_data);
}
