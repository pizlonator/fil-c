/* GTK - The GIMP Toolkit
 * gtkprintsettings.c: Print Settings
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
#include <stdlib.h>

#include <glib/gprintf.h>

#include <gtk/gtk.h>

#include "gtkprintsettings.h"
#include "gtkprintutilsprivate.h"


/**
 * GtkPrintSettings:
 *
 * A `GtkPrintSettings` object represents the settings of a print dialog in
 * a system-independent way.
 *
 * The main use for this object is that once you’ve printed you can get a
 * settings object that represents the settings the user chose, and the next
 * time you print you can pass that object in so that the user doesn’t have
 * to re-set all his settings.
 *
 * Its also possible to enumerate the settings so that you can easily save
 * the settings for the next time your app runs, or even store them in a
 * document. The predefined keys try to use shared values as much as possible
 * so that moving such a document between systems still works.
 */

typedef struct _GtkPrintSettingsClass GtkPrintSettingsClass;

#define GTK_IS_PRINT_SETTINGS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINT_SETTINGS))
#define GTK_PRINT_SETTINGS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINT_SETTINGS, GtkPrintSettingsClass))
#define GTK_PRINT_SETTINGS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINT_SETTINGS, GtkPrintSettingsClass))

struct _GtkPrintSettings
{
  GObject parent_instance;

  GHashTable *hash;
};

struct _GtkPrintSettingsClass
{
  GObjectClass parent_class;
};

#define KEYFILE_GROUP_NAME "Print Settings"

G_DEFINE_TYPE (GtkPrintSettings, gtk_print_settings, G_TYPE_OBJECT)

static void
gtk_print_settings_finalize (GObject *object)
{
  GtkPrintSettings *settings = GTK_PRINT_SETTINGS (object);

  g_hash_table_destroy (settings->hash);

  G_OBJECT_CLASS (gtk_print_settings_parent_class)->finalize (object);
}

static void
gtk_print_settings_init (GtkPrintSettings *settings)
{
  settings->hash = g_hash_table_new_full (g_str_hash, g_str_equal,
					  g_free, g_free);
}

static void
gtk_print_settings_class_init (GtkPrintSettingsClass *class)
{
  GObjectClass *gobject_class = (GObjectClass *)class;

  gobject_class->finalize = gtk_print_settings_finalize;
}

/**
 * gtk_print_settings_new:
 *
 * Creates a new `GtkPrintSettings` object.
 *
 * Returns: a new `GtkPrintSettings` object
 */
GtkPrintSettings *
gtk_print_settings_new (void)
{
  return g_object_new (GTK_TYPE_PRINT_SETTINGS, NULL);
}

static void
copy_hash_entry  (gpointer  key,
		  gpointer  value,
		  gpointer  user_data)
{
  GtkPrintSettings *settings = user_data;

  g_hash_table_insert (settings->hash, 
		       g_strdup (key), 
		       g_strdup (value));
}



/**
 * gtk_print_settings_copy:
 * @other: a `GtkPrintSettings`
 *
 * Copies a `GtkPrintSettings` object.
 *
 * Returns: (transfer full): a newly allocated copy of @other
 */
GtkPrintSettings *
gtk_print_settings_copy (GtkPrintSettings *other)
{
  GtkPrintSettings *settings;

  if (other == NULL)
    return NULL;
  
  g_return_val_if_fail (GTK_IS_PRINT_SETTINGS (other), NULL);

  settings = gtk_print_settings_new ();

  g_hash_table_foreach (other->hash,
			copy_hash_entry,
			settings);

  return settings;
}

/**
 * gtk_print_settings_get:
 * @settings: a `GtkPrintSettings`
 * @key: a key
 *
 * Looks up the string value associated with @key.
 *
 * Returns: (nullable): the string value for @key
 */
const char *
gtk_print_settings_get (GtkPrintSettings *settings,
			const char       *key)
{
  return g_hash_table_lookup (settings->hash, key);
}

/**
 * gtk_print_settings_set:
 * @settings: a `GtkPrintSettings`
 * @key: a key
 * @value: (nullable): a string value
 *
 * Associates @value with @key.
 */
void
gtk_print_settings_set (GtkPrintSettings *settings,
			const char       *key,
			const char       *value)
{
  if (value == NULL)
    gtk_print_settings_unset (settings, key);
  else
    g_hash_table_insert (settings->hash, 
			 g_strdup (key), 
			 g_strdup (value));
}

/**
 * gtk_print_settings_unset:
 * @settings: a `GtkPrintSettings`
 * @key: a key
 *
 * Removes any value associated with @key.
 *
 * This has the same effect as setting the value to %NULL.
 */
void
gtk_print_settings_unset (GtkPrintSettings *settings,
			  const char       *key)
{
  g_hash_table_remove (settings->hash, key);
}

/**
 * gtk_print_settings_has_key:
 * @settings: a `GtkPrintSettings`
 * @key: a key
 *
 * Returns %TRUE, if a value is associated with @key.
 *
 * Returns: %TRUE, if @key has a value
 */
gboolean        
gtk_print_settings_has_key (GtkPrintSettings *settings,
			    const char       *key)
{
  return gtk_print_settings_get (settings, key) != NULL;
}


/**
 * gtk_print_settings_get_bool:
 * @settings: a `GtkPrintSettings`
 * @key: a key
 *
 * Returns the boolean represented by the value
 * that is associated with @key.
 *
 * The string “true” represents %TRUE, any other
 * string %FALSE.
 *
 * Returns: %TRUE, if @key maps to a true value.
 */
gboolean
gtk_print_settings_get_bool (GtkPrintSettings *settings,
			     const char       *key)
{
  const char *val;

  val = gtk_print_settings_get (settings, key);
  if (g_strcmp0 (val, "true") == 0)
    return TRUE;
  
  return FALSE;
}

/**
 * gtk_print_settings_get_bool_with_default:
 * @settings: a `GtkPrintSettings`
 * @key: a key
 * @default_val: the default value
 *
 * Returns the boolean represented by the value
 * that is associated with @key, or @default_val
 * if the value does not represent a boolean.
 *
 * The string “true” represents %TRUE, the string
 * “false” represents %FALSE.
 *
 * Returns: the boolean value associated with @key
 */
static gboolean
gtk_print_settings_get_bool_with_default (GtkPrintSettings *settings,
					  const char       *key,
					  gboolean          default_val)
{
  const char *val;

  val = gtk_print_settings_get (settings, key);
  if (g_strcmp0 (val, "true") == 0)
    return TRUE;

  if (g_strcmp0 (val, "false") == 0)
    return FALSE;
  
  return default_val;
}

/**
 * gtk_print_settings_set_bool:
 * @settings: a `GtkPrintSettings`
 * @key: a key
 * @value: a boolean
 *
 * Sets @key to a boolean value.
 */
void
gtk_print_settings_set_bool (GtkPrintSettings *settings,
			     const char       *key,
			     gboolean          value)
{
  if (value)
    gtk_print_settings_set (settings, key, "true");
  else
    gtk_print_settings_set (settings, key, "false");
}

/**
 * gtk_print_settings_get_double_with_default:
 * @settings: a `GtkPrintSettings`
 * @key: a key
 * @def: the default value
 *
 * Returns the floating point number represented by
 * the value that is associated with @key, or @default_val
 * if the value does not represent a floating point number.
 *
 * Floating point numbers are parsed with g_ascii_strtod().
 *
 * Returns: the floating point number associated with @key
 */
double
gtk_print_settings_get_double_with_default (GtkPrintSettings *settings,
					    const char       *key,
					    double            def)
{
  const char *val;

  val = gtk_print_settings_get (settings, key);
  if (val == NULL)
    return def;

  return g_ascii_strtod (val, NULL);
}

/**
 * gtk_print_settings_get_double:
 * @settings: a `GtkPrintSettings`
 * @key: a key
 *
 * Returns the double value associated with @key, or 0.
 *
 * Returns: the double value of @key
 */
double
gtk_print_settings_get_double (GtkPrintSettings *settings,
			       const char       *key)
{
  return gtk_print_settings_get_double_with_default (settings, key, 0.0);
}

/**
 * gtk_print_settings_set_double:
 * @settings: a `GtkPrintSettings`
 * @key: a key
 * @value: a double value
 *
 * Sets @key to a double value.
 */
void
gtk_print_settings_set_double (GtkPrintSettings *settings,
			       const char       *key,
			       double            value)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];
  
  g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, value);
  gtk_print_settings_set (settings, key, buf);
}

/**
 * gtk_print_settings_get_length:
 * @settings: a `GtkPrintSettings`
 * @key: a key
 * @unit: the unit of the return value
 *
 * Returns the value associated with @key, interpreted
 * as a length.
 *
 * The returned value is converted to @units.
 *
 * Returns: the length value of @key, converted to @unit
 */
double
gtk_print_settings_get_length (GtkPrintSettings *settings,
			       const char       *key,
			       GtkUnit           unit)
{
  double length = gtk_print_settings_get_double (settings, key);
  return _gtk_print_convert_from_mm (length, unit);
}

/**
 * gtk_print_settings_set_length:
 * @settings: a `GtkPrintSettings`
 * @key: a key
 * @value: a length
 * @unit: the unit of @length
 *
 * Associates a length in units of @unit with @key.
 */
void
gtk_print_settings_set_length (GtkPrintSettings *settings,
			       const char       *key,
			       double            value, 
			       GtkUnit           unit)
{
  gtk_print_settings_set_double (settings, key,
				 _gtk_print_convert_to_mm (value, unit));
}

/**
 * gtk_print_settings_get_int_with_default:
 * @settings: a `GtkPrintSettings`
 * @key: a key
 * @def: the default value
 *
 * Returns the value of @key, interpreted as
 * an integer, or the default value.
 *
 * Returns: the integer value of @key
 */
int
gtk_print_settings_get_int_with_default (GtkPrintSettings *settings,
					 const char       *key,
					 int               def)
{
  const char *val;

  val = gtk_print_settings_get (settings, key);
  if (val == NULL)
    return def;

  return atoi (val);
}

/**
 * gtk_print_settings_get_int:
 * @settings: a `GtkPrintSettings`
 * @key: a key
 *
 * Returns the integer value of @key, or 0.
 *
 * Returns: the integer value of @key
 */
int
gtk_print_settings_get_int (GtkPrintSettings *settings,
			    const char       *key)
{
  return gtk_print_settings_get_int_with_default (settings, key, 0);
}

/**
 * gtk_print_settings_set_int:
 * @settings: a `GtkPrintSettings`
 * @key: a key
 * @value: an integer
 *
 * Sets @key to an integer value.
 */
void
gtk_print_settings_set_int (GtkPrintSettings *settings,
			    const char       *key,
			    int               value)
{
  char buf[128];
  g_sprintf (buf, "%d", value);
  gtk_print_settings_set (settings, key, buf);
}

/**
 * gtk_print_settings_foreach:
 * @settings: a `GtkPrintSettings`
 * @func: (scope call): the function to call
 * @user_data: (closure): user data for @func
 *
 * Calls @func for each key-value pair of @settings.
 */
void
gtk_print_settings_foreach (GtkPrintSettings    *settings,
			    GtkPrintSettingsFunc func,
			    gpointer             user_data)
{
  g_hash_table_foreach (settings->hash, (GHFunc)func, user_data);
}

/**
 * gtk_print_settings_get_printer:
 * @settings: a `GtkPrintSettings`
 *
 * Convenience function to obtain the value of
 * %GTK_PRINT_SETTINGS_PRINTER.
 *
 * Returns: (nullable): the printer name
 */
const char *
gtk_print_settings_get_printer (GtkPrintSettings *settings)
{
  return gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_PRINTER);
}


/**
 * gtk_print_settings_set_printer:
 * @settings: a `GtkPrintSettings`
 * @printer: the printer name
 *
 * Convenience function to set %GTK_PRINT_SETTINGS_PRINTER
 * to @printer.
 */
void
gtk_print_settings_set_printer (GtkPrintSettings *settings,
				const char       *printer)
{
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PRINTER, printer);
}

/**
 * gtk_print_settings_get_orientation:
 * @settings: a `GtkPrintSettings`
 *
 * Get the value of %GTK_PRINT_SETTINGS_ORIENTATION,
 * converted to a `GtkPageOrientation`.
 *
 * Returns: the orientation
 */
GtkPageOrientation
gtk_print_settings_get_orientation (GtkPrintSettings *settings)
{
  const char *val;

  val = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_ORIENTATION);

  if (val == NULL || strcmp (val, "portrait") == 0)
    return GTK_PAGE_ORIENTATION_PORTRAIT;

  if (strcmp (val, "landscape") == 0)
    return GTK_PAGE_ORIENTATION_LANDSCAPE;
  
  if (strcmp (val, "reverse_portrait") == 0)
    return GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT;
  
  if (strcmp (val, "reverse_landscape") == 0)
    return GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE;
  
  return GTK_PAGE_ORIENTATION_PORTRAIT;
}

/**
 * gtk_print_settings_set_orientation:
 * @settings: a `GtkPrintSettings`
 * @orientation: a page orientation
 *
 * Sets the value of %GTK_PRINT_SETTINGS_ORIENTATION.
 */
void
gtk_print_settings_set_orientation (GtkPrintSettings   *settings,
				    GtkPageOrientation  orientation)
{
  const char *val;

  switch (orientation)
    {
    case GTK_PAGE_ORIENTATION_LANDSCAPE:
      val = "landscape";
      break;
    default:
    case GTK_PAGE_ORIENTATION_PORTRAIT:
      val = "portrait";
      break;
    case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
      val = "reverse_landscape";
      break;
    case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
      val = "reverse_portrait";
      break;
    }
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_ORIENTATION, val);
}

/**
 * gtk_print_settings_get_paper_size:
 * @settings: a `GtkPrintSettings`
 *
 * Gets the value of %GTK_PRINT_SETTINGS_PAPER_FORMAT,
 * converted to a `GtkPaperSize`.
 *
 * Returns: (nullable): the paper size
 */
GtkPaperSize *     
gtk_print_settings_get_paper_size (GtkPrintSettings *settings)
{
  const char *val;
  const char *name;
  double w, h;

  val = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_PAPER_FORMAT);
  if (val == NULL)
    return NULL;

  if (g_str_has_prefix (val, "custom-")) 
    {
      name = val + strlen ("custom-");
      w = gtk_print_settings_get_paper_width (settings, GTK_UNIT_MM);
      h = gtk_print_settings_get_paper_height (settings, GTK_UNIT_MM);
      return gtk_paper_size_new_custom (name, name, w, h, GTK_UNIT_MM);
    }

  return gtk_paper_size_new (val);
}

/**
 * gtk_print_settings_set_paper_size:
 * @settings: a `GtkPrintSettings`
 * @paper_size: a paper size
 *
 * Sets the value of %GTK_PRINT_SETTINGS_PAPER_FORMAT,
 * %GTK_PRINT_SETTINGS_PAPER_WIDTH and
 * %GTK_PRINT_SETTINGS_PAPER_HEIGHT.
 */
void
gtk_print_settings_set_paper_size (GtkPrintSettings *settings,
				   GtkPaperSize     *paper_size)
{
  char *custom_name;

  if (paper_size == NULL) 
    {
      gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PAPER_FORMAT, NULL);
      gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PAPER_WIDTH, NULL);
      gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PAPER_HEIGHT, NULL);
    }
  else if (gtk_paper_size_is_custom (paper_size)) 
    {
      custom_name = g_strdup_printf ("custom-%s", 
				     gtk_paper_size_get_name (paper_size));
      gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PAPER_FORMAT, custom_name);
      g_free (custom_name);
      gtk_print_settings_set_paper_width (settings, 
					  gtk_paper_size_get_width (paper_size, 
								    GTK_UNIT_MM),
					  GTK_UNIT_MM);
      gtk_print_settings_set_paper_height (settings, 
					   gtk_paper_size_get_height (paper_size, 
								      GTK_UNIT_MM),
					   GTK_UNIT_MM);
    } 
  else
    gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PAPER_FORMAT, 
			    gtk_paper_size_get_name (paper_size));
}

/**
 * gtk_print_settings_get_paper_width:
 * @settings: a `GtkPrintSettings`
 * @unit: the unit for the return value
 *
 * Gets the value of %GTK_PRINT_SETTINGS_PAPER_WIDTH,
 * converted to @unit.
 *
 * Returns: the paper width, in units of @unit
 */
double
gtk_print_settings_get_paper_width (GtkPrintSettings *settings,
				    GtkUnit           unit)
{
  return gtk_print_settings_get_length (settings, GTK_PRINT_SETTINGS_PAPER_WIDTH, unit);
}

/**
 * gtk_print_settings_set_paper_width:
 * @settings: a `GtkPrintSettings`
 * @width: the paper width
 * @unit: the units of @width
 *
 * Sets the value of %GTK_PRINT_SETTINGS_PAPER_WIDTH.
 */
void
gtk_print_settings_set_paper_width (GtkPrintSettings *settings,
				    double            width, 
				    GtkUnit           unit)
{
  gtk_print_settings_set_length (settings, GTK_PRINT_SETTINGS_PAPER_WIDTH, width, unit);
}

/**
 * gtk_print_settings_get_paper_height:
 * @settings: a `GtkPrintSettings`
 * @unit: the unit for the return value
 *
 * Gets the value of %GTK_PRINT_SETTINGS_PAPER_HEIGHT,
 * converted to @unit.
 *
 * Returns: the paper height, in units of @unit
 */
double
gtk_print_settings_get_paper_height (GtkPrintSettings *settings,
				     GtkUnit           unit)
{
  return gtk_print_settings_get_length (settings, 
					GTK_PRINT_SETTINGS_PAPER_HEIGHT,
					unit);
}

/**
 * gtk_print_settings_set_paper_height:
 * @settings: a `GtkPrintSettings`
 * @height: the paper height
 * @unit: the units of @height
 *
 * Sets the value of %GTK_PRINT_SETTINGS_PAPER_HEIGHT.
 */
void
gtk_print_settings_set_paper_height (GtkPrintSettings *settings,
				     double            height, 
				     GtkUnit           unit)
{
  gtk_print_settings_set_length (settings, 
				 GTK_PRINT_SETTINGS_PAPER_HEIGHT, 
				 height, unit);
}

/**
 * gtk_print_settings_get_use_color:
 * @settings: a `GtkPrintSettings`
 *
 * Gets the value of %GTK_PRINT_SETTINGS_USE_COLOR.
 *
 * Returns: whether to use color
 */
gboolean
gtk_print_settings_get_use_color (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_bool_with_default (settings, 
						   GTK_PRINT_SETTINGS_USE_COLOR,
						   TRUE);
}

/**
 * gtk_print_settings_set_use_color:
 * @settings: a `GtkPrintSettings`
 * @use_color: whether to use color
 *
 * Sets the value of %GTK_PRINT_SETTINGS_USE_COLOR.
 */
void
gtk_print_settings_set_use_color (GtkPrintSettings *settings,
				  gboolean          use_color)
{
  gtk_print_settings_set_bool (settings,
			       GTK_PRINT_SETTINGS_USE_COLOR, 
			       use_color);
}

/**
 * gtk_print_settings_get_collate:
 * @settings: a `GtkPrintSettings`
 *
 * Gets the value of %GTK_PRINT_SETTINGS_COLLATE.
 *
 * Returns: whether to collate the printed pages
 */
gboolean
gtk_print_settings_get_collate (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_bool_with_default (settings,
                                                   GTK_PRINT_SETTINGS_COLLATE,
                                                   TRUE);
}

/**
 * gtk_print_settings_set_collate:
 * @settings: a `GtkPrintSettings`
 * @collate: whether to collate the output
 *
 * Sets the value of %GTK_PRINT_SETTINGS_COLLATE.
 */
void
gtk_print_settings_set_collate (GtkPrintSettings *settings,
				gboolean          collate)
{
  gtk_print_settings_set_bool (settings,
			       GTK_PRINT_SETTINGS_COLLATE, 
			       collate);
}

/**
 * gtk_print_settings_get_reverse:
 * @settings: a `GtkPrintSettings`
 *
 * Gets the value of %GTK_PRINT_SETTINGS_REVERSE.
 *
 * Returns:  whether to reverse the order of the printed pages
 */
gboolean
gtk_print_settings_get_reverse (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_bool (settings, 
				      GTK_PRINT_SETTINGS_REVERSE);
}

/**
 * gtk_print_settings_set_reverse:
 * @settings: a `GtkPrintSettings`
 * @reverse: whether to reverse the output
 *
 * Sets the value of %GTK_PRINT_SETTINGS_REVERSE.
 */
void
gtk_print_settings_set_reverse (GtkPrintSettings *settings,
				  gboolean        reverse)
{
  gtk_print_settings_set_bool (settings,
			       GTK_PRINT_SETTINGS_REVERSE, 
			       reverse);
}

/**
 * gtk_print_settings_get_duplex:
 * @settings: a `GtkPrintSettings`
 *
 * Gets the value of %GTK_PRINT_SETTINGS_DUPLEX.
 *
 * Returns: whether to print the output in duplex.
 */
GtkPrintDuplex
gtk_print_settings_get_duplex (GtkPrintSettings *settings)
{
  const char *val;

  val = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_DUPLEX);

  if (val == NULL || (strcmp (val, "simplex") == 0))
    return GTK_PRINT_DUPLEX_SIMPLEX;

  if (strcmp (val, "horizontal") == 0)
    return GTK_PRINT_DUPLEX_HORIZONTAL;
  
  if (strcmp (val, "vertical") == 0)
    return GTK_PRINT_DUPLEX_VERTICAL;
  
  return GTK_PRINT_DUPLEX_SIMPLEX;
}

/**
 * gtk_print_settings_set_duplex:
 * @settings: a `GtkPrintSettings`
 * @duplex: a `GtkPrintDuplex` value
 *
 * Sets the value of %GTK_PRINT_SETTINGS_DUPLEX.
 */
void
gtk_print_settings_set_duplex (GtkPrintSettings *settings,
			       GtkPrintDuplex    duplex)
{
  const char *str;

  switch (duplex)
    {
    default:
    case GTK_PRINT_DUPLEX_SIMPLEX:
      str = "simplex";
      break;
    case GTK_PRINT_DUPLEX_HORIZONTAL:
      str = "horizontal";
      break;
    case GTK_PRINT_DUPLEX_VERTICAL:
      str = "vertical";
      break;
    }
  
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_DUPLEX, str);
}

/**
 * gtk_print_settings_get_quality:
 * @settings: a `GtkPrintSettings`
 *
 * Gets the value of %GTK_PRINT_SETTINGS_QUALITY.
 *
 * Returns: the print quality
 */
GtkPrintQuality
gtk_print_settings_get_quality (GtkPrintSettings *settings)
{
  const char *val;

  val = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_QUALITY);

  if (val == NULL || (strcmp (val, "normal") == 0))
    return GTK_PRINT_QUALITY_NORMAL;

  if (strcmp (val, "high") == 0)
    return GTK_PRINT_QUALITY_HIGH;
  
  if (strcmp (val, "low") == 0)
    return GTK_PRINT_QUALITY_LOW;
  
  if (strcmp (val, "draft") == 0)
    return GTK_PRINT_QUALITY_DRAFT;
  
  return GTK_PRINT_QUALITY_NORMAL;
}

/**
 * gtk_print_settings_set_quality:
 * @settings: a `GtkPrintSettings`
 * @quality: a `GtkPrintQuality` value
 * 
 * Sets the value of %GTK_PRINT_SETTINGS_QUALITY.
 */
void
gtk_print_settings_set_quality (GtkPrintSettings *settings,
				GtkPrintQuality   quality)
{
  const char *str;

  switch (quality)
    {
    default:
    case GTK_PRINT_QUALITY_NORMAL:
      str = "normal";
      break;
    case GTK_PRINT_QUALITY_HIGH:
      str = "high";
      break;
    case GTK_PRINT_QUALITY_LOW:
      str = "low";
      break;
    case GTK_PRINT_QUALITY_DRAFT:
      str = "draft";
      break;
    }
  
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_QUALITY, str);
}

/**
 * gtk_print_settings_get_page_set:
 * @settings: a `GtkPrintSettings`
 *
 * Gets the value of %GTK_PRINT_SETTINGS_PAGE_SET.
 *
 * Returns: the set of pages to print
 */
GtkPageSet
gtk_print_settings_get_page_set (GtkPrintSettings *settings)
{
  const char *val;

  val = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_PAGE_SET);

  if (val == NULL || (strcmp (val, "all") == 0))
    return GTK_PAGE_SET_ALL;

  if (strcmp (val, "even") == 0)
    return GTK_PAGE_SET_EVEN;
  
  if (strcmp (val, "odd") == 0)
    return GTK_PAGE_SET_ODD;
  
  return GTK_PAGE_SET_ALL;
}

/**
 * gtk_print_settings_set_page_set:
 * @settings: a `GtkPrintSettings`
 * @page_set: a `GtkPageSet` value
 *
 * Sets the value of %GTK_PRINT_SETTINGS_PAGE_SET.
 */
void
gtk_print_settings_set_page_set (GtkPrintSettings *settings,
				 GtkPageSet        page_set)
{
  const char *str;

  switch (page_set)
    {
    default:
    case GTK_PAGE_SET_ALL:
      str = "all";
      break;
    case GTK_PAGE_SET_EVEN:
      str = "even";
      break;
    case GTK_PAGE_SET_ODD:
      str = "odd";
      break;
    }
  
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PAGE_SET, str);
}

/**
 * gtk_print_settings_get_number_up_layout:
 * @settings: a `GtkPrintSettings`
 *
 * Gets the value of %GTK_PRINT_SETTINGS_NUMBER_UP_LAYOUT.
 *
 * Returns: layout of page in number-up mode
 */
GtkNumberUpLayout
gtk_print_settings_get_number_up_layout (GtkPrintSettings *settings)
{
  GtkNumberUpLayout layout;
  GtkTextDirection  text_direction;
  GEnumClass       *enum_class;
  GEnumValue       *enum_value;
  const char       *val;

  g_return_val_if_fail (GTK_IS_PRINT_SETTINGS (settings), GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM);

  val = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_NUMBER_UP_LAYOUT);
  text_direction = gtk_widget_get_default_direction ();

  if (text_direction == GTK_TEXT_DIR_LTR)
    layout = GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM;
  else
    layout = GTK_NUMBER_UP_LAYOUT_RIGHT_TO_LEFT_TOP_TO_BOTTOM;

  if (val == NULL)
    return layout;

  enum_class = g_type_class_ref (GTK_TYPE_NUMBER_UP_LAYOUT);
  enum_value = g_enum_get_value_by_nick (enum_class, val);
  if (enum_value)
    layout = enum_value->value;
  g_type_class_unref (enum_class);

  return layout;
}

/**
 * gtk_print_settings_set_number_up_layout:
 * @settings: a `GtkPrintSettings`
 * @number_up_layout: a `GtkNumberUpLayout` value
 *
 * Sets the value of %GTK_PRINT_SETTINGS_NUMBER_UP_LAYOUT.
 */
void
gtk_print_settings_set_number_up_layout (GtkPrintSettings  *settings,
					 GtkNumberUpLayout  number_up_layout)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  g_return_if_fail (GTK_IS_PRINT_SETTINGS (settings));

  enum_class = g_type_class_ref (GTK_TYPE_NUMBER_UP_LAYOUT);
  enum_value = g_enum_get_value (enum_class, number_up_layout);
  g_return_if_fail (enum_value != NULL);

  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_NUMBER_UP_LAYOUT, enum_value->value_nick);
  g_type_class_unref (enum_class);
}

/**
 * gtk_print_settings_get_n_copies:
 * @settings: a `GtkPrintSettings`
 * 
 * Gets the value of %GTK_PRINT_SETTINGS_N_COPIES.
 * 
 * Returns: the number of copies to print
 */
int
gtk_print_settings_get_n_copies (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_int_with_default (settings, GTK_PRINT_SETTINGS_N_COPIES, 1);
}

/**
 * gtk_print_settings_set_n_copies:
 * @settings: a `GtkPrintSettings`
 * @num_copies: the number of copies
 *
 * Sets the value of %GTK_PRINT_SETTINGS_N_COPIES.
 */
void
gtk_print_settings_set_n_copies (GtkPrintSettings *settings,
				 int               num_copies)
{
  gtk_print_settings_set_int (settings, GTK_PRINT_SETTINGS_N_COPIES,
			      num_copies);
}

/**
 * gtk_print_settings_get_number_up:
 * @settings: a `GtkPrintSettings`
 *
 * Gets the value of %GTK_PRINT_SETTINGS_NUMBER_UP.
 *
 * Returns: the number of pages per sheet
 */
int
gtk_print_settings_get_number_up (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_int_with_default (settings, GTK_PRINT_SETTINGS_NUMBER_UP, 1);
}

/**
 * gtk_print_settings_set_number_up:
 * @settings: a `GtkPrintSettings`
 * @number_up: the number of pages per sheet
 *
 * Sets the value of %GTK_PRINT_SETTINGS_NUMBER_UP.
 */
void
gtk_print_settings_set_number_up (GtkPrintSettings *settings,
				  int               number_up)
{
  gtk_print_settings_set_int (settings, GTK_PRINT_SETTINGS_NUMBER_UP,
				number_up);
}

/**
 * gtk_print_settings_get_resolution:
 * @settings: a `GtkPrintSettings`
 *
 * Gets the value of %GTK_PRINT_SETTINGS_RESOLUTION.
 *
 * Returns: the resolution in dpi
 */
int
gtk_print_settings_get_resolution (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_int_with_default (settings, GTK_PRINT_SETTINGS_RESOLUTION, 300);
}

/**
 * gtk_print_settings_set_resolution:
 * @settings: a `GtkPrintSettings`
 * @resolution: the resolution in dpi
 *
 * Sets the values of %GTK_PRINT_SETTINGS_RESOLUTION,
 * %GTK_PRINT_SETTINGS_RESOLUTION_X and
 * %GTK_PRINT_SETTINGS_RESOLUTION_Y.
 */
void
gtk_print_settings_set_resolution (GtkPrintSettings *settings,
				   int               resolution)
{
  gtk_print_settings_set_int (settings, GTK_PRINT_SETTINGS_RESOLUTION,
			      resolution);
  gtk_print_settings_set_int (settings, GTK_PRINT_SETTINGS_RESOLUTION_X,
			      resolution);
  gtk_print_settings_set_int (settings, GTK_PRINT_SETTINGS_RESOLUTION_Y,
			      resolution);
}

/**
 * gtk_print_settings_get_resolution_x:
 * @settings: a `GtkPrintSettings`
 *
 * Gets the value of %GTK_PRINT_SETTINGS_RESOLUTION_X.
 *
 * Returns: the horizontal resolution in dpi
 */
int
gtk_print_settings_get_resolution_x (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_int_with_default (settings, GTK_PRINT_SETTINGS_RESOLUTION_X, 300);
}

/**
 * gtk_print_settings_get_resolution_y:
 * @settings: a `GtkPrintSettings`
 *
 * Gets the value of %GTK_PRINT_SETTINGS_RESOLUTION_Y.
 *
 * Returns: the vertical resolution in dpi
 */
int
gtk_print_settings_get_resolution_y (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_int_with_default (settings, GTK_PRINT_SETTINGS_RESOLUTION_Y, 300);
}

/**
 * gtk_print_settings_set_resolution_xy:
 * @settings: a `GtkPrintSettings`
 * @resolution_x: the horizontal resolution in dpi
 * @resolution_y: the vertical resolution in dpi
 *
 * Sets the values of %GTK_PRINT_SETTINGS_RESOLUTION,
 * %GTK_PRINT_SETTINGS_RESOLUTION_X and
 * %GTK_PRINT_SETTINGS_RESOLUTION_Y.
 */
void
gtk_print_settings_set_resolution_xy (GtkPrintSettings *settings,
				      int               resolution_x,
				      int               resolution_y)
{
  gtk_print_settings_set_int (settings, GTK_PRINT_SETTINGS_RESOLUTION_X,
			      resolution_x);
  gtk_print_settings_set_int (settings, GTK_PRINT_SETTINGS_RESOLUTION_Y,
			      resolution_y);
  gtk_print_settings_set_int (settings, GTK_PRINT_SETTINGS_RESOLUTION,
			      resolution_x);
}

/**
 * gtk_print_settings_get_printer_lpi:
 * @settings: a `GtkPrintSettings`
 *
 * Gets the value of %GTK_PRINT_SETTINGS_PRINTER_LPI.
 *
 * Returns: the resolution in lpi (lines per inch)
 */
double
gtk_print_settings_get_printer_lpi (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_double_with_default (settings, GTK_PRINT_SETTINGS_PRINTER_LPI, 150.0);
}

/**
 * gtk_print_settings_set_printer_lpi:
 * @settings: a `GtkPrintSettings`
 * @lpi: the resolution in lpi (lines per inch)
 *
 * Sets the value of %GTK_PRINT_SETTINGS_PRINTER_LPI.
 */
void
gtk_print_settings_set_printer_lpi (GtkPrintSettings *settings,
				    double            lpi)
{
  gtk_print_settings_set_double (settings, GTK_PRINT_SETTINGS_PRINTER_LPI,
			         lpi);
}

/**
 * gtk_print_settings_get_scale:
 * @settings: a `GtkPrintSettings`
 *
 * Gets the value of %GTK_PRINT_SETTINGS_SCALE.
 *
 * Returns: the scale in percent
 */
double
gtk_print_settings_get_scale (GtkPrintSettings *settings)
{
  return gtk_print_settings_get_double_with_default (settings,
						     GTK_PRINT_SETTINGS_SCALE,
						     100.0);
}

/**
 * gtk_print_settings_set_scale:
 * @settings: a `GtkPrintSettings`
 * @scale: the scale in percent
 * 
 * Sets the value of %GTK_PRINT_SETTINGS_SCALE.
 */
void
gtk_print_settings_set_scale (GtkPrintSettings *settings,
			      double            scale)
{
  gtk_print_settings_set_double (settings, GTK_PRINT_SETTINGS_SCALE,
				 scale);
}

/**
 * gtk_print_settings_get_print_pages:
 * @settings: a `GtkPrintSettings`
 * 
 * Gets the value of %GTK_PRINT_SETTINGS_PRINT_PAGES.
 * 
 * Returns: which pages to print
 */
GtkPrintPages
gtk_print_settings_get_print_pages (GtkPrintSettings *settings)
{
  const char *val;

  val = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_PRINT_PAGES);

  if (val == NULL || (strcmp (val, "all") == 0))
    return GTK_PRINT_PAGES_ALL;

  if (strcmp (val, "selection") == 0)
    return GTK_PRINT_PAGES_SELECTION;

  if (strcmp (val, "current") == 0)
    return GTK_PRINT_PAGES_CURRENT;
  
  if (strcmp (val, "ranges") == 0)
    return GTK_PRINT_PAGES_RANGES;
  
  return GTK_PRINT_PAGES_ALL;
}

/**
 * gtk_print_settings_set_print_pages:
 * @settings: a `GtkPrintSettings`
 * @pages: a `GtkPrintPages` value
 * 
 * Sets the value of %GTK_PRINT_SETTINGS_PRINT_PAGES.
 */
void
gtk_print_settings_set_print_pages (GtkPrintSettings *settings,
				    GtkPrintPages     pages)
{
  const char *str;

  switch (pages)
    {
    default:
    case GTK_PRINT_PAGES_ALL:
      str = "all";
      break;
    case GTK_PRINT_PAGES_CURRENT:
      str = "current";
      break;
    case GTK_PRINT_PAGES_SELECTION:
      str = "selection";
      break;
    case GTK_PRINT_PAGES_RANGES:
      str = "ranges";
      break;
    }
  
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PRINT_PAGES, str);
}

/**
 * gtk_print_settings_get_page_ranges:
 * @settings: a `GtkPrintSettings`
 * @num_ranges: (out): return location for the length of the returned array
 *
 * Gets the value of %GTK_PRINT_SETTINGS_PAGE_RANGES.
 *
 * Returns: (array length=num_ranges) (transfer full): an array
 *   of `GtkPageRange`s. Use g_free() to free the array when
 *   it is no longer needed.
 */
GtkPageRange *
gtk_print_settings_get_page_ranges (GtkPrintSettings *settings,
				    int              *num_ranges)
{
  const char *val;
  char **range_strs;
  GtkPageRange *ranges;
  int i, n;
  
  val = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_PAGE_RANGES);

  if (val == NULL)
    {
      *num_ranges = 0;
      return NULL;
    }
  
  range_strs = g_strsplit (val, ",", 0);

  for (i = 0; range_strs[i] != NULL; i++)
    ;

  n = i;

  ranges = g_new0 (GtkPageRange, n);

  for (i = 0; i < n; i++)
    {
      int start, end;
      char *str;

      start = (int)strtol (range_strs[i], &str, 10);
      end = start;

      if (*str == '-')
	{
	  str++;
	  end = (int)strtol (str, NULL, 10);
	}

      ranges[i].start = start;
      ranges[i].end = end;
    }

  g_strfreev (range_strs);

  *num_ranges = n;
  return ranges;
}

/**
 * gtk_print_settings_set_page_ranges:
 * @settings: a `GtkPrintSettings`
 * @page_ranges: (array length=num_ranges): an array of `GtkPageRange`s
 * @num_ranges: the length of @page_ranges
 *
 * Sets the value of %GTK_PRINT_SETTINGS_PAGE_RANGES.
 */
void
gtk_print_settings_set_page_ranges  (GtkPrintSettings *settings,
				     GtkPageRange     *page_ranges,
				     int               num_ranges)
{
  GString *s;
  int i;
  
  s = g_string_new ("");

  for (i = 0; i < num_ranges; i++)
    {
      if (page_ranges[i].start == page_ranges[i].end)
	g_string_append_printf (s, "%d", page_ranges[i].start);
      else
	g_string_append_printf (s, "%d-%d",
				page_ranges[i].start,
				page_ranges[i].end);
      if (i < num_ranges - 1)
	g_string_append (s, ",");
    }

  
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_PAGE_RANGES, 
			  s->str);

  g_string_free (s, TRUE);
}

/**
 * gtk_print_settings_get_default_source:
 * @settings: a `GtkPrintSettings`
 * 
 * Gets the value of %GTK_PRINT_SETTINGS_DEFAULT_SOURCE.
 * 
 * Returns: (nullable): the default source
 */
const char *
gtk_print_settings_get_default_source (GtkPrintSettings *settings)
{
  return gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_DEFAULT_SOURCE);
}

/**
 * gtk_print_settings_set_default_source:
 * @settings: a `GtkPrintSettings`
 * @default_source: the default source
 * 
 * Sets the value of %GTK_PRINT_SETTINGS_DEFAULT_SOURCE.
 */
void
gtk_print_settings_set_default_source (GtkPrintSettings *settings,
				       const char       *default_source)
{
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_DEFAULT_SOURCE, default_source);
}
     
/**
 * gtk_print_settings_get_media_type:
 * @settings: a `GtkPrintSettings`
 * 
 * Gets the value of %GTK_PRINT_SETTINGS_MEDIA_TYPE.
 *
 * The set of media types is defined in PWG 5101.1-2002 PWG.
 * 
 * Returns: (nullable): the media type
 */
const char *
gtk_print_settings_get_media_type (GtkPrintSettings *settings)
{
  return gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_MEDIA_TYPE);
}

/**
 * gtk_print_settings_set_media_type:
 * @settings: a `GtkPrintSettings`
 * @media_type: the media type
 * 
 * Sets the value of %GTK_PRINT_SETTINGS_MEDIA_TYPE.
 * 
 * The set of media types is defined in PWG 5101.1-2002 PWG.
 */
void
gtk_print_settings_set_media_type (GtkPrintSettings *settings,
				   const char       *media_type)
{
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_MEDIA_TYPE, media_type);
}

/**
 * gtk_print_settings_get_dither:
 * @settings: a `GtkPrintSettings`
 * 
 * Gets the value of %GTK_PRINT_SETTINGS_DITHER.
 * 
 * Returns: (nullable): the dithering that is used
 */
const char *
gtk_print_settings_get_dither (GtkPrintSettings *settings)
{
  return gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_DITHER);
}

/**
 * gtk_print_settings_set_dither:
 * @settings: a `GtkPrintSettings`
 * @dither: the dithering that is used
 * 
 * Sets the value of %GTK_PRINT_SETTINGS_DITHER.
 */
void
gtk_print_settings_set_dither (GtkPrintSettings *settings,
			       const char       *dither)
{
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_DITHER, dither);
}
     
/**
 * gtk_print_settings_get_finishings:
 * @settings: a `GtkPrintSettings`
 * 
 * Gets the value of %GTK_PRINT_SETTINGS_FINISHINGS.
 * 
 * Returns: (nullable): the finishings
 */
const char *
gtk_print_settings_get_finishings (GtkPrintSettings *settings)
{
  return gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_FINISHINGS);
}

/**
 * gtk_print_settings_set_finishings:
 * @settings: a `GtkPrintSettings`
 * @finishings: the finishings
 * 
 * Sets the value of %GTK_PRINT_SETTINGS_FINISHINGS.
 */
void
gtk_print_settings_set_finishings (GtkPrintSettings *settings,
				   const char       *finishings)
{
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_FINISHINGS, finishings);
}
     
/**
 * gtk_print_settings_get_output_bin:
 * @settings: a `GtkPrintSettings`
 * 
 * Gets the value of %GTK_PRINT_SETTINGS_OUTPUT_BIN.
 * 
 * Returns: (nullable): the output bin
 */
const char *
gtk_print_settings_get_output_bin (GtkPrintSettings *settings)
{
  return gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_OUTPUT_BIN);
}

/**
 * gtk_print_settings_set_output_bin:
 * @settings: a `GtkPrintSettings`
 * @output_bin: the output bin
 * 
 * Sets the value of %GTK_PRINT_SETTINGS_OUTPUT_BIN.
 */
void
gtk_print_settings_set_output_bin (GtkPrintSettings *settings,
				   const char       *output_bin)
{
  gtk_print_settings_set (settings, GTK_PRINT_SETTINGS_OUTPUT_BIN, output_bin);
}

/**
 * gtk_print_settings_load_file:
 * @settings: a `GtkPrintSettings`
 * @file_name: (type filename): the filename to read the settings from
 * @error: (nullable): return location for errors
 *
 * Reads the print settings from @file_name.
 *
 * If the file could not be loaded then error is set to either
 * a `GFileError` or `GKeyFileError`.
 *
 * See [method@Gtk.PrintSettings.to_file].
 *
 * Returns: %TRUE on success
 */
gboolean
gtk_print_settings_load_file (GtkPrintSettings *settings,
                              const char       *file_name,
                              GError          **error)
{
  gboolean retval = FALSE;
  GKeyFile *key_file;

  g_return_val_if_fail (GTK_IS_PRINT_SETTINGS (settings), FALSE);
  g_return_val_if_fail (file_name != NULL, FALSE);

  key_file = g_key_file_new ();

  if (g_key_file_load_from_file (key_file, file_name, 0, error) &&
      gtk_print_settings_load_key_file (settings, key_file, NULL, error))
    retval = TRUE;

  g_key_file_free (key_file);

  return retval;
}

/**
 * gtk_print_settings_new_from_file:
 * @file_name: (type filename): the filename to read the settings from
 * @error: (nullable): return location for errors
 *
 * Reads the print settings from @file_name.
 *
 * Returns a new `GtkPrintSettings` object with the restored settings,
 * or %NULL if an error occurred. If the file could not be loaded then
 * error is set to either a `GFileError` or `GKeyFileError`.
 *
 * See [method@Gtk.PrintSettings.to_file].
 *
 * Returns: the restored `GtkPrintSettings`
 */
GtkPrintSettings *
gtk_print_settings_new_from_file (const char   *file_name,
			          GError      **error)
{
  GtkPrintSettings *settings = gtk_print_settings_new ();

  if (!gtk_print_settings_load_file (settings, file_name, error))
    {
      g_object_unref (settings);
      settings = NULL;
    }

  return settings;
}

/**
 * gtk_print_settings_load_key_file:
 * @settings: a `GtkPrintSettings`
 * @key_file: the `GKeyFile` to retrieve the settings from
 * @group_name: (nullable): the name of the group to use, or %NULL
 *   to use the default “Print Settings”
 * @error: (nullable): return location for errors
 *
 * Reads the print settings from the group @group_name in @key_file.
 *
 * If the file could not be loaded then error is set to either a
 * `GFileError` or `GKeyFileError`.
 *
 * Returns: %TRUE on success
 */
gboolean
gtk_print_settings_load_key_file (GtkPrintSettings *settings,
				  GKeyFile         *key_file,
				  const char       *group_name,
				  GError          **error)
{
  char **keys;
  gsize n_keys, i;
  GError *err = NULL;

  g_return_val_if_fail (GTK_IS_PRINT_SETTINGS (settings), FALSE);
  g_return_val_if_fail (key_file != NULL, FALSE);

  if (!group_name)
    group_name = KEYFILE_GROUP_NAME;

  keys = g_key_file_get_keys (key_file,
			      group_name,
			      &n_keys,
			      &err);
  if (err != NULL)
    {
      g_propagate_error (error, err);
      return FALSE;
    }
   
  for (i = 0 ; i < n_keys; ++i)
    {
      char *value;

      value = g_key_file_get_string (key_file,
				     group_name,
				     keys[i],
				     NULL);
      if (!value)
        continue;

      gtk_print_settings_set (settings, keys[i], value);
      g_free (value);
    }

  g_strfreev (keys);

  return TRUE;
}

/**
 * gtk_print_settings_new_from_key_file:
 * @key_file: the `GKeyFile` to retrieve the settings from
 * @group_name: (nullable): the name of the group to use, or %NULL to use
 *   the default “Print Settings”
 * @error: (nullable): return location for errors
 *
 * Reads the print settings from the group @group_name in @key_file.
 *
 * Returns a new `GtkPrintSettings` object with the restored settings,
 * or %NULL if an error occurred. If the file could not be loaded then
 * error is set to either `GFileError` or `GKeyFileError`.
 *
 * Returns: the restored `GtkPrintSettings`
 */
GtkPrintSettings *
gtk_print_settings_new_from_key_file (GKeyFile     *key_file,
				      const char   *group_name,
				      GError      **error)
{
  GtkPrintSettings *settings = gtk_print_settings_new ();

  if (!gtk_print_settings_load_key_file (settings, key_file,
                                         group_name, error))
    {
      g_object_unref (settings);
      settings = NULL;
    }

  return settings;
}

/**
 * gtk_print_settings_to_file:
 * @settings: a `GtkPrintSettings`
 * @file_name: (type filename): the file to save to
 * @error: (nullable): return location for errors
 *
 * This function saves the print settings from @settings to @file_name.
 *
 * If the file could not be written then error is set to either a
 * `GFileError` or `GKeyFileError`.
 *
 * Returns: %TRUE on success
 */
gboolean
gtk_print_settings_to_file (GtkPrintSettings  *settings,
			    const char        *file_name,
			    GError           **error)
{
  GKeyFile *key_file;
  gboolean retval = FALSE;
  char *data = NULL;
  gsize len;
  GError *err = NULL;

  g_return_val_if_fail (GTK_IS_PRINT_SETTINGS (settings), FALSE);
  g_return_val_if_fail (file_name != NULL, FALSE);

  key_file = g_key_file_new ();
  gtk_print_settings_to_key_file (settings, key_file, NULL);

  data = g_key_file_to_data (key_file, &len, &err);
  if (!data)
    goto out;

  retval = g_file_set_contents (file_name, data, len, &err);

out:
  if (err != NULL)
    g_propagate_error (error, err);

  g_key_file_free (key_file);
  g_free (data);

  return retval;
}

typedef struct {
  GKeyFile *key_file;
  const char *group_name;
} SettingsData;

static void
add_value_to_key_file (const char   *key,
		       const char   *value,
		       SettingsData *data)
{
  g_key_file_set_string (data->key_file, data->group_name, key, value);
}

/**
 * gtk_print_settings_to_key_file:
 * @settings: a `GtkPrintSettings`
 * @key_file: the `GKeyFile` to save the print settings to
 * @group_name: (nullable): the group to add the settings to in @key_file, or
 *   %NULL to use the default “Print Settings”
 *
 * This function adds the print settings from @settings to @key_file.
 */
void
gtk_print_settings_to_key_file (GtkPrintSettings  *settings,
			        GKeyFile          *key_file,
				const char        *group_name)
{
  SettingsData data;

  g_return_if_fail (GTK_IS_PRINT_SETTINGS (settings));
  g_return_if_fail (key_file != NULL);

  if (!group_name)
    group_name = KEYFILE_GROUP_NAME;

  data.key_file = key_file;
  data.group_name = group_name;

  gtk_print_settings_foreach (settings,
			      (GtkPrintSettingsFunc) add_value_to_key_file,
			      &data);
}

static void
add_to_variant (const char *key,
                const char *value,
                gpointer     data)
{
  GVariantBuilder *builder = data;
  g_variant_builder_add (builder, "{sv}", key, g_variant_new_string (value));
}

/**
 * gtk_print_settings_to_gvariant:
 * @settings: a `GtkPrintSettings`
 *
 * Serialize print settings to an a{sv} variant.
 *
 * Returns: (transfer none): a new, floating, `GVariant`
 */
GVariant *
gtk_print_settings_to_gvariant (GtkPrintSettings *settings)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
  gtk_print_settings_foreach (settings, add_to_variant, &builder);

  return g_variant_builder_end (&builder);
}

/**
 * gtk_print_settings_new_from_gvariant:
 * @variant: an a{sv} `GVariant`
 *
 * Deserialize print settings from an a{sv} variant.
 *
 * The variant must be in the format produced by
 * [method@Gtk.PrintSettings.to_gvariant].
 *
 * Returns: (transfer full): a new `GtkPrintSettings` object
 */
GtkPrintSettings *
gtk_print_settings_new_from_gvariant (GVariant *variant)
{
  GtkPrintSettings *settings;
  int i;

  g_return_val_if_fail (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARDICT), NULL);

  settings = gtk_print_settings_new ();

  for (i = 0; i < g_variant_n_children (variant); i++)
    {
      const char *key;
      GVariant *v;

      g_variant_get_child (variant, i, "{&sv}", &key, &v);
      if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING))
        gtk_print_settings_set (settings, key, g_variant_get_string (v, NULL));
      g_variant_unref (v);
    }

  return settings;
}
