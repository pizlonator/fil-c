/* GTK - The GIMP Toolkit
 * gtkpagesetup.c: Page Setup
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

#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "gtkpagesetup.h"
#include "gtkprintutilsprivate.h"
#include "gtkprintoperation.h" /* for GtkPrintError */

/**
 * GtkPageSetup:
 *
 * A `GtkPageSetup` object stores the page size, orientation and margins.
 *
 * The idea is that you can get one of these from the page setup dialog
 * and then pass it to the `GtkPrintOperation` when printing.
 * The benefit of splitting this out of the `GtkPrintSettings` is that
 * these affect the actual layout of the page, and thus need to be set
 * long before user prints.
 *
 * ## Margins
 *
 * The margins specified in this object are the “print margins”, i.e. the
 * parts of the page that the printer cannot print on. These are different
 * from the layout margins that a word processor uses; they are typically
 * used to determine the minimal size for the layout margins.
 *
 * To obtain a `GtkPageSetup` use [ctor@Gtk.PageSetup.new] to get the defaults,
 * or use [func@Gtk.print_run_page_setup_dialog] to show the page setup dialog
 * and receive the resulting page setup.
 *
 * ## A page setup dialog
 *
 * ```c
 * static GtkPrintSettings *settings = NULL;
 * static GtkPageSetup *page_setup = NULL;
 *
 * static void
 * do_page_setup (void)
 * {
 *   GtkPageSetup *new_page_setup;
 *
 *   if (settings == NULL)
 *     settings = gtk_print_settings_new ();
 *
 *   new_page_setup = gtk_print_run_page_setup_dialog (GTK_WINDOW (main_window),
 *                                                     page_setup, settings);
 *
 *   if (page_setup)
 *     g_object_unref (page_setup);
 *
 *   page_setup = new_page_setup;
 * }
 * ```
 */

#define KEYFILE_GROUP_NAME "Page Setup"

typedef struct _GtkPageSetupClass GtkPageSetupClass;

#define GTK_IS_PAGE_SETUP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PAGE_SETUP))
#define GTK_PAGE_SETUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PAGE_SETUP, GtkPageSetupClass))
#define GTK_PAGE_SETUP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PAGE_SETUP, GtkPageSetupClass))

struct _GtkPageSetup
{
  GObject parent_instance;

  GtkPageOrientation orientation;
  GtkPaperSize *paper_size;
  /* These are stored in mm */
  double top_margin, bottom_margin, left_margin, right_margin;
};

struct _GtkPageSetupClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GtkPageSetup, gtk_page_setup, G_TYPE_OBJECT)

static void
gtk_page_setup_finalize (GObject *object)
{
  GtkPageSetup *setup = GTK_PAGE_SETUP (object);
  
  gtk_paper_size_free (setup->paper_size);
  
  G_OBJECT_CLASS (gtk_page_setup_parent_class)->finalize (object);
}

static void
gtk_page_setup_init (GtkPageSetup *setup)
{
  setup->paper_size = gtk_paper_size_new (NULL);
  setup->orientation = GTK_PAGE_ORIENTATION_PORTRAIT;
  setup->top_margin = gtk_paper_size_get_default_top_margin (setup->paper_size, GTK_UNIT_MM);
  setup->bottom_margin = gtk_paper_size_get_default_bottom_margin (setup->paper_size, GTK_UNIT_MM);
  setup->left_margin = gtk_paper_size_get_default_left_margin (setup->paper_size, GTK_UNIT_MM);
  setup->right_margin = gtk_paper_size_get_default_right_margin (setup->paper_size, GTK_UNIT_MM);
}

static void
gtk_page_setup_class_init (GtkPageSetupClass *class)
{
  GObjectClass *gobject_class = (GObjectClass *)class;

  gobject_class->finalize = gtk_page_setup_finalize;
}

/**
 * gtk_page_setup_new:
 *
 * Creates a new `GtkPageSetup`.
 *
 * Returns: a new `GtkPageSetup`.
 */
GtkPageSetup *
gtk_page_setup_new (void)
{
  return g_object_new (GTK_TYPE_PAGE_SETUP, NULL);
}

/**
 * gtk_page_setup_copy:
 * @other: the `GtkPageSetup` to copy
 *
 * Copies a `GtkPageSetup`.
 *
 * Returns: (transfer full): a copy of @other
 */
GtkPageSetup *
gtk_page_setup_copy (GtkPageSetup *other)
{
  GtkPageSetup *copy;

  copy = gtk_page_setup_new ();
  copy->orientation = other->orientation;
  gtk_paper_size_free (copy->paper_size);
  copy->paper_size = gtk_paper_size_copy (other->paper_size);
  copy->top_margin = other->top_margin;
  copy->bottom_margin = other->bottom_margin;
  copy->left_margin = other->left_margin;
  copy->right_margin = other->right_margin;

  return copy;
}

/**
 * gtk_page_setup_get_orientation:
 * @setup: a `GtkPageSetup`
 *
 * Gets the page orientation of the `GtkPageSetup`.
 *
 * Returns: the page orientation
 */
GtkPageOrientation
gtk_page_setup_get_orientation (GtkPageSetup *setup)
{
  return setup->orientation;
}

/**
 * gtk_page_setup_set_orientation:
 * @setup: a `GtkPageSetup`
 * @orientation: a `GtkPageOrientation` value
 *
 * Sets the page orientation of the `GtkPageSetup`.
 */
void
gtk_page_setup_set_orientation (GtkPageSetup       *setup,
				GtkPageOrientation  orientation)
{
  setup->orientation = orientation;
}

/**
 * gtk_page_setup_get_paper_size:
 * @setup: a `GtkPageSetup`
 *
 * Gets the paper size of the `GtkPageSetup`.
 *
 * Returns: (transfer none): the paper size
 */
GtkPaperSize *
gtk_page_setup_get_paper_size (GtkPageSetup *setup)
{
  g_return_val_if_fail (GTK_IS_PAGE_SETUP (setup), NULL);

  return setup->paper_size;
}

/**
 * gtk_page_setup_set_paper_size:
 * @setup: a `GtkPageSetup`
 * @size: a `GtkPaperSize`
 *
 * Sets the paper size of the `GtkPageSetup` without
 * changing the margins.
 *
 * See [method@Gtk.PageSetup.set_paper_size_and_default_margins].
 */
void
gtk_page_setup_set_paper_size (GtkPageSetup *setup,
			       GtkPaperSize *size)
{
  GtkPaperSize *old_size;

  g_return_if_fail (GTK_IS_PAGE_SETUP (setup));
  g_return_if_fail (size != NULL);

  old_size = setup->paper_size;

  setup->paper_size = gtk_paper_size_copy (size);

  if (old_size)
    gtk_paper_size_free (old_size);
}

/**
 * gtk_page_setup_set_paper_size_and_default_margins:
 * @setup: a `GtkPageSetup`
 * @size: a `GtkPaperSize`
 *
 * Sets the paper size of the `GtkPageSetup` and modifies
 * the margins according to the new paper size.
 */
void
gtk_page_setup_set_paper_size_and_default_margins (GtkPageSetup *setup,
						   GtkPaperSize *size)
{
  gtk_page_setup_set_paper_size (setup, size);
  setup->top_margin = gtk_paper_size_get_default_top_margin (setup->paper_size, GTK_UNIT_MM);
  setup->bottom_margin = gtk_paper_size_get_default_bottom_margin (setup->paper_size, GTK_UNIT_MM);
  setup->left_margin = gtk_paper_size_get_default_left_margin (setup->paper_size, GTK_UNIT_MM);
  setup->right_margin = gtk_paper_size_get_default_right_margin (setup->paper_size, GTK_UNIT_MM);
}

/**
 * gtk_page_setup_get_top_margin:
 * @setup: a `GtkPageSetup`
 * @unit: the unit for the return value
 *
 * Gets the top margin in units of @unit.
 *
 * Returns: the top margin
 */
double
gtk_page_setup_get_top_margin (GtkPageSetup *setup,
			       GtkUnit       unit)
{
  return _gtk_print_convert_from_mm (setup->top_margin, unit);
}

/**
 * gtk_page_setup_set_top_margin:
 * @setup: a `GtkPageSetup`
 * @margin: the new top margin in units of @unit
 * @unit: the units for @margin
 *
 * Sets the top margin of the `GtkPageSetup`.
 */
void
gtk_page_setup_set_top_margin (GtkPageSetup *setup,
			       double        margin,
			       GtkUnit       unit)
{
  setup->top_margin = _gtk_print_convert_to_mm (margin, unit);
}

/**
 * gtk_page_setup_get_bottom_margin:
 * @setup: a `GtkPageSetup`
 * @unit: the unit for the return value
 *
 * Gets the bottom margin in units of @unit.
 *
 * Returns: the bottom margin
 */
double
gtk_page_setup_get_bottom_margin (GtkPageSetup *setup,
				  GtkUnit       unit)
{
  return _gtk_print_convert_from_mm (setup->bottom_margin, unit);
}

/**
 * gtk_page_setup_set_bottom_margin:
 * @setup: a `GtkPageSetup`
 * @margin: the new bottom margin in units of @unit
 * @unit: the units for @margin
 *
 * Sets the bottom margin of the `GtkPageSetup`.
 */
void
gtk_page_setup_set_bottom_margin (GtkPageSetup *setup,
				  double        margin,
				  GtkUnit       unit)
{
  setup->bottom_margin = _gtk_print_convert_to_mm (margin, unit);
}

/**
 * gtk_page_setup_get_left_margin:
 * @setup: a `GtkPageSetup`
 * @unit: the unit for the return value
 *
 * Gets the left margin in units of @unit.
 *
 * Returns: the left margin
 */
double
gtk_page_setup_get_left_margin (GtkPageSetup *setup,
				GtkUnit       unit)
{
  return _gtk_print_convert_from_mm (setup->left_margin, unit);
}

/**
 * gtk_page_setup_set_left_margin:
 * @setup: a `GtkPageSetup`
 * @margin: the new left margin in units of @unit
 * @unit: the units for @margin
 *
 * Sets the left margin of the `GtkPageSetup`.
 */
void
gtk_page_setup_set_left_margin (GtkPageSetup *setup,
				double        margin,
				GtkUnit       unit)
{
  setup->left_margin = _gtk_print_convert_to_mm (margin, unit);
}

/**
 * gtk_page_setup_get_right_margin:
 * @setup: a `GtkPageSetup`
 * @unit: the unit for the return value
 *
 * Gets the right margin in units of @unit.
 *
 * Returns: the right margin
 */
double
gtk_page_setup_get_right_margin (GtkPageSetup *setup,
				 GtkUnit       unit)
{
  return _gtk_print_convert_from_mm (setup->right_margin, unit);
}

/**
 * gtk_page_setup_set_right_margin:
 * @setup: a `GtkPageSetup`
 * @margin: the new right margin in units of @unit
 * @unit: the units for @margin
 *
 * Sets the right margin of the `GtkPageSetup`.
 */
void
gtk_page_setup_set_right_margin (GtkPageSetup *setup,
				 double        margin,
				 GtkUnit       unit)
{
  setup->right_margin = _gtk_print_convert_to_mm (margin, unit);
}

/**
 * gtk_page_setup_get_paper_width:
 * @setup: a `GtkPageSetup`
 * @unit: the unit for the return value
 *
 * Returns the paper width in units of @unit.
 *
 * Note that this function takes orientation,
 * but not margins into consideration.
 * See [method@Gtk.PageSetup.get_page_width].
 *
 * Returns: the paper width.
 */
double
gtk_page_setup_get_paper_width (GtkPageSetup *setup,
				GtkUnit       unit)
{
  if (setup->orientation == GTK_PAGE_ORIENTATION_PORTRAIT ||
      setup->orientation == GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT)
    return gtk_paper_size_get_width (setup->paper_size, unit);
  else
    return gtk_paper_size_get_height (setup->paper_size, unit);
}

/**
 * gtk_page_setup_get_paper_height:
 * @setup: a `GtkPageSetup`
 * @unit: the unit for the return value
 *
 * Returns the paper height in units of @unit.
 *
 * Note that this function takes orientation,
 * but not margins into consideration.
 * See [method@Gtk.PageSetup.get_page_height].
 *
 * Returns: the paper height.
 */
double
gtk_page_setup_get_paper_height (GtkPageSetup *setup,
				 GtkUnit       unit)
{
  if (setup->orientation == GTK_PAGE_ORIENTATION_PORTRAIT ||
      setup->orientation == GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT)
    return gtk_paper_size_get_height (setup->paper_size, unit);
  else
    return gtk_paper_size_get_width (setup->paper_size, unit);
}

/**
 * gtk_page_setup_get_page_width:
 * @setup: a `GtkPageSetup`
 * @unit: the unit for the return value
 *
 * Returns the page width in units of @unit.
 *
 * Note that this function takes orientation
 * and margins into consideration.
 * See [method@Gtk.PageSetup.get_paper_width].
 *
 * Returns: the page width.
 */
double
gtk_page_setup_get_page_width (GtkPageSetup *setup,
			       GtkUnit       unit)
{
  double width;

  width = gtk_page_setup_get_paper_width (setup, GTK_UNIT_MM);
  if (setup->orientation == GTK_PAGE_ORIENTATION_PORTRAIT ||
      setup->orientation == GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT)
    width -= setup->left_margin + setup->right_margin;
  else
    width -= setup->top_margin + setup->bottom_margin;

  return _gtk_print_convert_from_mm (width, unit);
}

/**
 * gtk_page_setup_get_page_height:
 * @setup: a `GtkPageSetup`
 * @unit: the unit for the return value
 *
 * Returns the page height in units of @unit.
 *
 * Note that this function takes orientation
 * and margins into consideration.
 * See [method@Gtk.PageSetup.get_paper_height].
 *
 * Returns: the page height.
 */
double
gtk_page_setup_get_page_height (GtkPageSetup *setup,
				GtkUnit       unit)
{
  double height;

  height = gtk_page_setup_get_paper_height (setup, GTK_UNIT_MM);
  if (setup->orientation == GTK_PAGE_ORIENTATION_PORTRAIT ||
      setup->orientation == GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT)
    height -= setup->top_margin + setup->bottom_margin;
  else
    height -= setup->left_margin + setup->right_margin;

  return _gtk_print_convert_from_mm (height, unit);
}

/**
 * gtk_page_setup_load_file:
 * @setup: a `GtkPageSetup`
 * @file_name: (type filename): the filename to read the page setup from
 * @error: (nullable): return location for an error
 *
 * Reads the page setup from the file @file_name.
 *
 * See [method@Gtk.PageSetup.to_file].
 *
 * Returns: %TRUE on success
 */
gboolean
gtk_page_setup_load_file (GtkPageSetup *setup,
                          const char   *file_name,
			  GError      **error)
{
  gboolean retval = FALSE;
  GKeyFile *key_file;

  g_return_val_if_fail (GTK_IS_PAGE_SETUP (setup), FALSE);
  g_return_val_if_fail (file_name != NULL, FALSE);

  key_file = g_key_file_new ();

  if (g_key_file_load_from_file (key_file, file_name, 0, error) &&
      gtk_page_setup_load_key_file (setup, key_file, NULL, error))
    retval = TRUE;

  g_key_file_free (key_file);

  return retval;
}

/**
 * gtk_page_setup_new_from_file:
 * @file_name: (type filename): the filename to read the page setup from
 * @error: (nullable): return location for an error
 *
 * Reads the page setup from the file @file_name.
 *
 * Returns a new `GtkPageSetup` object with the restored
 * page setup, or %NULL if an error occurred.
 * See [method@Gtk.PageSetup.to_file].
 *
 * Returns: the restored `GtkPageSetup`
 */
GtkPageSetup *
gtk_page_setup_new_from_file (const char   *file_name,
			      GError      **error)
{
  GtkPageSetup *setup = gtk_page_setup_new ();

  if (!gtk_page_setup_load_file (setup, file_name, error))
    {
      g_object_unref (setup);
      setup = NULL;
    }

  return setup;
}

/* something like this should really be in gobject! */
static guint
string_to_enum (GType type,
                const char *enum_string)
{
  GEnumClass *enum_class;
  const GEnumValue *value;
  guint retval = 0;

  g_return_val_if_fail (enum_string != NULL, 0);

  enum_class = g_type_class_ref (type);
  value = g_enum_get_value_by_nick (enum_class, enum_string);
  if (value)
    retval = value->value;

  g_type_class_unref (enum_class);

  return retval;
}

/**
 * gtk_page_setup_load_key_file:
 * @setup: a `GtkPageSetup`
 * @key_file: the `GKeyFile` to retrieve the page_setup from
 * @group_name: (nullable): the name of the group in the key_file to read
 *   to use the default name “Page Setup”
 * @error: (nullable): return location for an error
 *
 * Reads the page setup from the group @group_name in the key file
 * @key_file.
 *
 * Returns: %TRUE on success
 */
gboolean
gtk_page_setup_load_key_file (GtkPageSetup *setup,
                              GKeyFile     *key_file,
                              const char   *group_name,
                              GError      **error)
{
  GtkPaperSize *paper_size;
  double top, bottom, left, right;
  char *orientation = NULL, *freeme = NULL;
  gboolean retval = FALSE;
  GError *err = NULL;

  g_return_val_if_fail (GTK_IS_PAGE_SETUP (setup), FALSE);
  g_return_val_if_fail (key_file != NULL, FALSE);

  if (!group_name)
    group_name = KEYFILE_GROUP_NAME;

  if (!g_key_file_has_group (key_file, group_name))
    {
      g_set_error_literal (error,
                           GTK_PRINT_ERROR,
                           GTK_PRINT_ERROR_INVALID_FILE,
                           _("Not a valid page setup file"));
      goto out;
    }

#define GET_DOUBLE(kf, group, name, v) \
  v = g_key_file_get_double (kf, group, name, &err); \
  if (err != NULL) \
    { \
      g_propagate_error (error, err);\
      goto out;\
    }

  GET_DOUBLE (key_file, group_name, "MarginTop", top);
  GET_DOUBLE (key_file, group_name, "MarginBottom", bottom);
  GET_DOUBLE (key_file, group_name, "MarginLeft", left);
  GET_DOUBLE (key_file, group_name, "MarginRight", right);

#undef GET_DOUBLE

  paper_size = gtk_paper_size_new_from_key_file (key_file, group_name, &err);
  if (!paper_size)
    {
      g_propagate_error (error, err);
      goto out;
    }

  gtk_page_setup_set_paper_size (setup, paper_size);
  gtk_paper_size_free (paper_size);

  gtk_page_setup_set_top_margin (setup, top, GTK_UNIT_MM);
  gtk_page_setup_set_bottom_margin (setup, bottom, GTK_UNIT_MM);
  gtk_page_setup_set_left_margin (setup, left, GTK_UNIT_MM);
  gtk_page_setup_set_right_margin (setup, right, GTK_UNIT_MM);

  orientation = g_key_file_get_string (key_file, group_name,
				       "Orientation", NULL);
  if (orientation)
    {
      gtk_page_setup_set_orientation (setup,
				      string_to_enum (GTK_TYPE_PAGE_ORIENTATION,
						      orientation));
      g_free (orientation);
    }

  retval = TRUE;

out:
  g_free (freeme);
  return retval;
}

/**
 * gtk_page_setup_new_from_key_file:
 * @key_file: the `GKeyFile` to retrieve the page_setup from
 * @group_name: (nullable): the name of the group in the key_file to read
 *    to use the default name “Page Setup”
 * @error: (nullable): return location for an error
 *
 * Reads the page setup from the group @group_name in the key file
 * @key_file.
 *
 * Returns a new `GtkPageSetup` object with the restored
 * page setup, or %NULL if an error occurred.
 *
 * Returns: the restored `GtkPageSetup`
 */
GtkPageSetup *
gtk_page_setup_new_from_key_file (GKeyFile     *key_file,
				  const char   *group_name,
				  GError      **error)
{
  GtkPageSetup *setup = gtk_page_setup_new ();

  if (!gtk_page_setup_load_key_file (setup, key_file, group_name, error))
    {
      g_object_unref (setup);
      setup = NULL;
    }

  return setup;
}

/**
 * gtk_page_setup_to_file:
 * @setup: a `GtkPageSetup`
 * @file_name: (type filename): the file to save to
 * @error: (nullable): return location for errors
 *
 * This function saves the information from @setup to @file_name.
 *
 * Returns: %TRUE on success
 */
gboolean
gtk_page_setup_to_file (GtkPageSetup  *setup,
		        const char    *file_name,
			GError       **error)
{
  GKeyFile *key_file;
  gboolean retval = FALSE;
  char *data = NULL;
  gsize len;

  g_return_val_if_fail (GTK_IS_PAGE_SETUP (setup), FALSE);
  g_return_val_if_fail (file_name != NULL, FALSE);

  key_file = g_key_file_new ();
  gtk_page_setup_to_key_file (setup, key_file, NULL);

  data = g_key_file_to_data (key_file, &len, error);
  if (!data)
    goto out;

  retval = g_file_set_contents (file_name, data, len, error);

out:
  g_key_file_free (key_file);
  g_free (data);

  return retval;
}

/* something like this should really be in gobject! */
static char *
enum_to_string (GType type,
                guint enum_value)
{
  GEnumClass *enum_class;
  GEnumValue *value;
  char *retval = NULL;

  enum_class = g_type_class_ref (type);

  value = g_enum_get_value (enum_class, enum_value);
  if (value)
    retval = g_strdup (value->value_nick);

  g_type_class_unref (enum_class);

  return retval;
}

/**
 * gtk_page_setup_to_key_file:
 * @setup: a `GtkPageSetup`
 * @key_file: the `GKeyFile` to save the page setup to
 * @group_name: (nullable): the group to add the settings to in @key_file,
 *   or %NULL to use the default name “Page Setup”
 *
 * This function adds the page setup from @setup to @key_file.
 */
void
gtk_page_setup_to_key_file (GtkPageSetup *setup,
			    GKeyFile     *key_file,
			    const char   *group_name)
{
  GtkPaperSize *paper_size;
  char *orientation;

  g_return_if_fail (GTK_IS_PAGE_SETUP (setup));
  g_return_if_fail (key_file != NULL);

  if (!group_name)
    group_name = KEYFILE_GROUP_NAME;

  paper_size = gtk_page_setup_get_paper_size (setup);
  g_assert (paper_size != NULL);

  gtk_paper_size_to_key_file (paper_size, key_file, group_name);

  g_key_file_set_double (key_file, group_name,
			 "MarginTop", gtk_page_setup_get_top_margin (setup, GTK_UNIT_MM));
  g_key_file_set_double (key_file, group_name,
			 "MarginBottom", gtk_page_setup_get_bottom_margin (setup, GTK_UNIT_MM));
  g_key_file_set_double (key_file, group_name,
			 "MarginLeft", gtk_page_setup_get_left_margin (setup, GTK_UNIT_MM));
  g_key_file_set_double (key_file, group_name,
			 "MarginRight", gtk_page_setup_get_right_margin (setup, GTK_UNIT_MM));

  orientation = enum_to_string (GTK_TYPE_PAGE_ORIENTATION,
				gtk_page_setup_get_orientation (setup));
  g_key_file_set_string (key_file, group_name,
			 "Orientation", orientation);
  g_free (orientation);
}

/**
 * gtk_page_setup_to_gvariant:
 * @setup: a `GtkPageSetup`
 *
 * Serialize page setup to an a{sv} variant.
 *
 * Return: (transfer none): a new, floating, `GVariant`
 */
GVariant *
gtk_page_setup_to_gvariant (GtkPageSetup *setup)
{
  GtkPaperSize *paper_size;
  GVariant *variant;
  int i;
  GVariantBuilder builder;
  char *orientation;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);

  paper_size = gtk_page_setup_get_paper_size (setup);

  variant = g_variant_ref_sink (gtk_paper_size_to_gvariant (paper_size));
  for (i = 0; i < g_variant_n_children (variant); i++)
    g_variant_builder_add_value (&builder, g_variant_get_child_value (variant, i));
  g_variant_unref (variant);

  g_variant_builder_add (&builder, "{sv}", "MarginTop", g_variant_new_double (gtk_page_setup_get_top_margin (setup, GTK_UNIT_MM)));
  g_variant_builder_add (&builder, "{sv}", "MarginBottom", g_variant_new_double (gtk_page_setup_get_bottom_margin (setup, GTK_UNIT_MM)));
  g_variant_builder_add (&builder, "{sv}", "MarginLeft", g_variant_new_double (gtk_page_setup_get_left_margin (setup, GTK_UNIT_MM)));
  g_variant_builder_add (&builder, "{sv}", "MarginRight", g_variant_new_double (gtk_page_setup_get_right_margin (setup, GTK_UNIT_MM)));

  orientation = enum_to_string (GTK_TYPE_PAGE_ORIENTATION,
                                gtk_page_setup_get_orientation (setup));
  g_variant_builder_add (&builder, "{sv}", "Orientation", g_variant_new_take_string (orientation));

  return g_variant_builder_end (&builder);
}

/**
 * gtk_page_setup_new_from_gvariant:
 * @variant: an a{sv} `GVariant`
 *
 * Desrialize a page setup from an a{sv} variant.
 *
 * The variant must be in the format produced by
 * [method@Gtk.PageSetup.to_gvariant].
 *
 * Returns: (transfer full): a new `GtkPageSetup` object
 */
GtkPageSetup *
gtk_page_setup_new_from_gvariant (GVariant *variant)
{
  GtkPageSetup *setup;
  const char *orientation;
  double margin;
  GtkPaperSize *paper_size;

  g_return_val_if_fail (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARDICT), NULL);

  setup = gtk_page_setup_new ();

  paper_size = gtk_paper_size_new_from_gvariant (variant);
  if (paper_size)
    {
      gtk_page_setup_set_paper_size (setup, paper_size);
      gtk_paper_size_free (paper_size);
    }

  if (g_variant_lookup (variant, "MarginTop", "d", &margin))
    gtk_page_setup_set_top_margin (setup, margin, GTK_UNIT_MM);
  if (g_variant_lookup (variant, "MarginBottom", "d", &margin))
    gtk_page_setup_set_bottom_margin (setup, margin, GTK_UNIT_MM);
  if (g_variant_lookup (variant, "MarginLeft", "d", &margin))
    gtk_page_setup_set_left_margin (setup, margin, GTK_UNIT_MM);
  if (g_variant_lookup (variant, "MarginRight", "d", &margin))
    gtk_page_setup_set_right_margin (setup, margin, GTK_UNIT_MM);

  if (g_variant_lookup (variant, "Orientation", "&s", &orientation))
    gtk_page_setup_set_orientation (setup, string_to_enum (GTK_TYPE_PAGE_ORIENTATION,
                                                           orientation));

  return setup;
}
