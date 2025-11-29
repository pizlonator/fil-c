/* GTK - The GIMP Toolkit
 * gtkpapersize.c: Paper Size
 * Copyright (C) 2006, Red Hat, Inc.
 * Copyright © 2006, 2007 Christian Persch
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
#include <locale.h>
#if defined(HAVE__NL_PAPER_HEIGHT) && defined(HAVE__NL_PAPER_WIDTH)
#include <langinfo.h>
#endif
#include <math.h>

#include <glib/gi18n-lib.h>

#include "gtkpapersize.h"
#include "gtkprintutilsprivate.h"
#include "gtkprintoperation.h"  /* for GtkPrintError */

/* _gtk_load_custom_papers() only on Unix so far  */
#ifdef G_OS_UNIX
#include "gtkcustompaperunixdialog.h"
#endif

#include "paper_names_offsets.c"


/**
 * GtkPaperSize:
 *
 * `GtkPaperSize` handles paper sizes.
 *
 * It uses the standard called
 * [PWG 5101.1-2002 PWG: Standard for Media Standardized Names](http://www.pwg.org/standards.html)
 * to name the paper sizes (and to get the data for the page sizes).
 * In addition to standard paper sizes, `GtkPaperSize` allows to
 * construct custom paper sizes with arbitrary dimensions.
 *
 * The `GtkPaperSize` object stores not only the dimensions (width
 * and height) of a paper size and its name, it also provides
 * default print margins.
 */


struct _GtkPaperSize
{
  const PaperInfo *info;

  /* If these are not set we fall back to info */
  char *name;
  char *display_name;
  char *ppd_name;

  double width, height; /* Stored in mm */
  gboolean is_custom;
  gboolean is_ipp;
};

G_DEFINE_BOXED_TYPE (GtkPaperSize, gtk_paper_size,
                     gtk_paper_size_copy,
                     gtk_paper_size_free)

static const PaperInfo *
lookup_paper_info (const char *name)
{
  int lower = 0;
  int upper = G_N_ELEMENTS (standard_names_offsets) - 1;
  int mid;
  int cmp;

  do
    {
       mid = (lower + upper) / 2;
       cmp = strcmp (name, paper_names + standard_names_offsets[mid].name);
       if (cmp < 0)
         upper = mid - 1;
       else if (cmp > 0)
         lower = mid + 1;
       else
         return &standard_names_offsets[mid];
    }
  while (lower <= upper);

  return NULL;
}

static gboolean
parse_media_size (const char *size,
                  double      *width_mm,
                  double      *height_mm)
{
  const char *p;
  char *e;
  double short_dim, long_dim;

  p = size;

  short_dim = g_ascii_strtod (p, &e);

  if (p == e || *e != 'x')
    return FALSE;

  p = e + 1; /* Skip x */

  long_dim = g_ascii_strtod (p, &e);

  if (p == e)
    return FALSE;

  p = e;

  if (strcmp (p, "in") == 0)
    {
      short_dim = short_dim * MM_PER_INCH;
      long_dim = long_dim * MM_PER_INCH;
    }
  else if (strcmp (p, "mm") != 0)
    return FALSE;

  if (width_mm)
    *width_mm = short_dim;
  if (height_mm)
    *height_mm = long_dim;

  return TRUE;
}

static gboolean
parse_full_media_size_name (const char   *full_name,
                            char        **name,
                            double       *width_mm,
                            double       *height_mm)
{
  const char *p;
  const char *end_of_name;

  /* From the spec:
   media-size-self-describing-name =
        ( class-in "_" size-name "_" short-dim "x" long-dim "in" ) |
        ( class-mm "_" size-name "_" short-dim "x" long-dim "mm" )
   class-in = "custom" | "na" | "asme" | "roc" | "oe"
   class-mm = "custom" | "iso" | "jis" | "jpn" | "prc" | "om"
   size-name = ( lowalpha | digit ) *( lowalpha | digit | "-" )
   short-dim = dim
   long-dim = dim
   dim = integer-part [fraction-part] | "0" fraction-part
   integer-part = non-zero-digit *digit
   fraction-part = "." *digit non-zero-digit
   lowalpha = "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" |
              "j" | "k" | "l" | "m" | "n" | "o" | "p" | "q" | "r" |
              "s" | "t" | "u" | "v" | "w" | "x" | "y" | "z"
   non-zero-digit = "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
   digit    = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
 */

  p = strchr (full_name, '_');
  if (p == NULL)
    return FALSE;

  p++; /* Skip _ */

  p = strchr (p, '_');
  if (p == NULL)
    return FALSE;

  end_of_name = p;

  p++; /* Skip _ */

  if (!parse_media_size (p, width_mm, height_mm))
    return FALSE;

  if (name)
    *name = g_strndup (full_name, end_of_name - full_name);

  return TRUE;
}

static GtkPaperSize *
gtk_paper_size_new_from_info (const PaperInfo *info)
{
  GtkPaperSize *size;

  size = g_new0 (GtkPaperSize, 1);
  size->info = info;
  size->width = info->width;
  size->height = info->height;

  return size;
}

/**
 * gtk_paper_size_new:
 * @name: (nullable): a paper size name
 *
 * Creates a new `GtkPaperSize` object by parsing a
 * [PWG 5101.1-2002](ftp://ftp.pwg.org/pub/pwg/candidates/cs-pwgmsn10-20020226-5101.1.pdf)
 * paper name.
 *
 * If @name is %NULL, the default paper size is returned,
 * see [func@Gtk.PaperSize.get_default].
 *
 * Returns: a new `GtkPaperSize`, use [method@Gtk.PaperSize.free]
 * to free it
 */
GtkPaperSize *
gtk_paper_size_new (const char *name)
{
  GtkPaperSize *size;
  char *short_name;
  double width, height;
  const PaperInfo *info;

  if (name == NULL)
    name = gtk_paper_size_get_default ();

  if (parse_full_media_size_name (name, &short_name, &width, &height))
    {
      info = lookup_paper_info (short_name);
      if (info != NULL && info->width == width && info->height == height)
        {
          size = gtk_paper_size_new_from_info (info);
          g_free (short_name);
        }
      else
        {
          size = g_new0 (GtkPaperSize, 1);

          size->width = width;
          size->height = height;
          size->name = short_name;
          size->display_name = g_strdup (short_name);
          if (strncmp (short_name, "custom", 6) == 0)
            size->is_custom = TRUE;
        }
    }
  else
    {
      info = lookup_paper_info (name);
      if (info != NULL)
        size = gtk_paper_size_new_from_info (info);
      else
        {
          g_warning ("Unknown paper size %s", name);
          size = g_new0 (GtkPaperSize, 1);
          size->name = g_strdup (name);
          size->display_name = g_strdup (name);
          /* Default to A4 size */
          size->width = 210;
          size->height = 297;
        }
    }

  return size;
}

static char *
improve_displayname (const char *name)
{
  char *p, *p1, *p2, *s;

  p = strrchr (name, 'x');
  if (p && p != name &&
      g_ascii_isdigit (*(p - 1)) &&
      g_ascii_isdigit (*(p + 1)))
    {
      p1 = g_strndup (name, p - name);
      p2 = g_strdup (p + 1);
      s = g_strconcat (p1, "×", p2, NULL);
      g_free (p1);
      g_free (p2);
    }
  else
    s = g_strdup (name);

  return s;
}

/**
 * gtk_paper_size_new_from_ppd:
 * @ppd_name: a PPD paper name
 * @ppd_display_name: the corresponding human-readable name
 * @width: the paper width, in points
 * @height: the paper height in points
 *
 * Creates a new `GtkPaperSize` object by using
 * PPD information.
 *
 * If @ppd_name is not a recognized PPD paper name,
 * @ppd_display_name, @width and @height are used to
 * construct a custom `GtkPaperSize` object.
 *
 * Returns: a new `GtkPaperSize`, use [method@Gtk.PaperSize.free]
 * to free it
 */
GtkPaperSize *
gtk_paper_size_new_from_ppd (const char *ppd_name,
                             const char *ppd_display_name,
                             double       width,
                             double       height)
{
  char *name;
  const char *lookup_ppd_name;
  char *freeme;
  GtkPaperSize *size;
  int i;
  char *display_name;

  lookup_ppd_name = ppd_name;

  freeme = NULL;
  /* Strip out Traverse suffix in matching. */
  if (g_str_has_suffix (ppd_name, ".Transverse"))
    {
      lookup_ppd_name = freeme =
        g_strndup (ppd_name, strlen (ppd_name) - strlen (".Transverse"));
    }

  for (i = 0; i < G_N_ELEMENTS (standard_names_offsets); i++)
    {
      if (standard_names_offsets[i].ppd_name != -1 &&
          strcmp (paper_names + standard_names_offsets[i].ppd_name, lookup_ppd_name) == 0)
        {
          size = gtk_paper_size_new_from_info (&standard_names_offsets[i]);
          goto out;
        }
    }

  for (i = 0; i < G_N_ELEMENTS (extra_ppd_names_offsets); i++)
    {
      if (strcmp (paper_names + extra_ppd_names_offsets[i].ppd_name, lookup_ppd_name) == 0)
        {
          size = gtk_paper_size_new (paper_names + extra_ppd_names_offsets[i].standard_name);
          goto out;
        }
    }

  name = g_strconcat ("ppd_", ppd_name, NULL);
  display_name = improve_displayname (ppd_display_name);
  size = gtk_paper_size_new_custom (name, display_name, width, height, GTK_UNIT_POINTS);
  g_free (display_name);
  g_free (name);

 out:

  if (size->info == NULL ||
      size->info->ppd_name == -1 ||
      strcmp (paper_names + size->info->ppd_name, ppd_name) != 0)
    size->ppd_name = g_strdup (ppd_name);

  g_free (freeme);

  return size;
}

/* Tolerance of paper size in points according to PostScript Language Reference */
#define PAPER_SIZE_TOLERANCE 5

/**
 * gtk_paper_size_new_from_ipp:
 * @ipp_name: an IPP paper name
 * @width: the paper width, in points
 * @height: the paper height in points
 *
 * Creates a new `GtkPaperSize` object by using
 * IPP information.
 *
 * If @ipp_name is not a recognized paper name,
 * @width and @height are used to
 * construct a custom `GtkPaperSize` object.
 *
 * Returns: a new `GtkPaperSize`, use [method@Gtk.PaperSize.free]
 * to free it
 */
GtkPaperSize *
gtk_paper_size_new_from_ipp (const char *ipp_name,
                             double       width,
                             double       height)
{
  GtkPaperSize *size;
  const char   *name = NULL;
  gboolean      found = FALSE;
  float         x_dimension;
  float         y_dimension;
  char         *display_name = NULL;
  int           i;

  /* Find paper size according to its name */
  for (i = 0; i < G_N_ELEMENTS (standard_names_offsets); i++)
    {
      if (standard_names_offsets[i].name != -1)
        name = paper_names + standard_names_offsets[i].name;
      if (name != NULL &&
          /* Given paper size name is equal to a name
             from the standard paper size names list. */
          ((g_strcmp0 (ipp_name, name) == 0) ||
           /* Given paper size name is prefixed by a name
              from the standard paper size names list +
              it consists of size in its name (e.g. iso_a4_210x297mm). */
            (g_str_has_prefix (ipp_name, name) &&
             strlen (ipp_name) > strlen (name) + 2 &&
             ipp_name[strlen (ipp_name)] == '_' &&
             g_ascii_isdigit (ipp_name[strlen (ipp_name) + 1]) &&
             (g_str_has_suffix (ipp_name, "mm") ||
              g_str_has_suffix (ipp_name, "in")))))
        {
          display_name = g_strdup (g_dpgettext2 (GETTEXT_PACKAGE,
                                                 "paper size",
                                                 paper_names + standard_names_offsets[i].display_name));
          found = TRUE;
          break;
        }
    }

  /* Find paper size according to its size */
  if (display_name == NULL)
    {
      for (i = 0; i < G_N_ELEMENTS (standard_names_offsets); i++)
        {
          x_dimension = _gtk_print_convert_from_mm (standard_names_offsets[i].width, GTK_UNIT_POINTS);
          y_dimension = _gtk_print_convert_from_mm (standard_names_offsets[i].height, GTK_UNIT_POINTS);

          if (fabs (x_dimension - width) <= PAPER_SIZE_TOLERANCE &&
              fabs (y_dimension - height) <= PAPER_SIZE_TOLERANCE)
            {
              display_name = g_strdup (g_dpgettext2 (GETTEXT_PACKAGE,
                                                     "paper size",
                                                     paper_names + standard_names_offsets[i].display_name));
              found = TRUE;
              break;
            }
        }
    }

  /* Fallback to name of the paper size as given in "ipp_name" parameter */
  if (display_name == NULL)
    display_name = g_strdup (ipp_name);

  size = gtk_paper_size_new_custom (ipp_name, display_name, width, height, GTK_UNIT_POINTS);
  size->is_custom = !found;
  size->is_ipp = found;

  g_free (display_name);

  return size;
}

/**
 * gtk_paper_size_new_custom:
 * @name: the paper name
 * @display_name: the human-readable name
 * @width: the paper width, in units of @unit
 * @height: the paper height, in units of @unit
 * @unit: the unit for @width and @height. not %GTK_UNIT_NONE.
 *
 * Creates a new `GtkPaperSize` object with the
 * given parameters.
 *
 * Returns: a new `GtkPaperSize` object, use [method@Gtk.PaperSize.free]
 * to free it
 */
GtkPaperSize *
gtk_paper_size_new_custom (const char *name,
                           const char *display_name,
                           double       width,
                           double       height,
                           GtkUnit      unit)
{
  GtkPaperSize *size;
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (unit != GTK_UNIT_NONE, NULL);

  size = g_new0 (GtkPaperSize, 1);

  size->name = g_strdup (name);
  size->display_name = g_strdup (display_name);
  size->is_custom = TRUE;

  size->width = _gtk_print_convert_to_mm (width, unit);
  size->height = _gtk_print_convert_to_mm (height, unit);

  return size;
}

/**
 * gtk_paper_size_copy:
 * @other: a `GtkPaperSize`
 *
 * Copies an existing `GtkPaperSize`.
 *
 * Returns: a copy of @other
 */
GtkPaperSize *
gtk_paper_size_copy (GtkPaperSize *other)
{
  GtkPaperSize *size;

  size = g_new0 (GtkPaperSize, 1);

  size->info = other->info;
  if (other->name)
    size->name = g_strdup (other->name);
  if (other->display_name)
    size->display_name = g_strdup (other->display_name);
  if (other->ppd_name)
    size->ppd_name = g_strdup (other->ppd_name);

  size->width = other->width;
  size->height = other->height;
  size->is_custom = other->is_custom;
  size->is_ipp = other->is_ipp;

  return size;
}

/**
 * gtk_paper_size_free:
 * @size: a `GtkPaperSize`
 *
 * Free the given `GtkPaperSize` object.
 */
void
gtk_paper_size_free (GtkPaperSize *size)
{
  g_free (size->name);
  g_free (size->display_name);
  g_free (size->ppd_name);
  g_free (size);
}

/**
 * gtk_paper_size_is_equal:
 * @size1: a `GtkPaperSize` object
 * @size2: another `GtkPaperSize` object
 *
 * Compares two `GtkPaperSize` objects.
 *
 * Returns: %TRUE, if @size1 and @size2
 * represent the same paper size
 */
gboolean
gtk_paper_size_is_equal (GtkPaperSize *size1,
                         GtkPaperSize *size2)
{
  if (size1->info != NULL && size2->info != NULL)
    return size1->info == size2->info;

  return strcmp (gtk_paper_size_get_name (size1),
                 gtk_paper_size_get_name (size2)) == 0;
}

/**
 * gtk_paper_size_get_paper_sizes:
 * @include_custom: whether to include custom paper sizes
 *   as defined in the page setup dialog
 *
 * Creates a list of known paper sizes.
 *
 * Returns:  (element-type GtkPaperSize) (transfer full): a newly allocated list of newly
 *    allocated `GtkPaperSize` objects
 */
GList *
gtk_paper_size_get_paper_sizes (gboolean include_custom)
{
  GList *list = NULL;
  guint i;
/* _gtk_load_custom_papers() only on Unix so far  */
#ifdef G_OS_UNIX
  if (include_custom)
    {
      GList *page_setups, *l;

      page_setups = _gtk_load_custom_papers ();
      for (l = page_setups; l != NULL; l = l->next)
        {
          GtkPageSetup *setup = (GtkPageSetup *) l->data;
          GtkPaperSize *size;

          size = gtk_page_setup_get_paper_size (setup);
          list = g_list_prepend (list, gtk_paper_size_copy (size));
        }

      g_list_free_full (page_setups, g_object_unref);
    }
#endif
  for (i = 0; i < G_N_ELEMENTS (standard_names_offsets); ++i)
    {
       GtkPaperSize *size;

       size = gtk_paper_size_new_from_info (&standard_names_offsets[i]);
       list = g_list_prepend (list, size);
    }

  return g_list_reverse (list);
}


/**
 * gtk_paper_size_get_name:
 * @size: a `GtkPaperSize` object
 *
 * Gets the name of the `GtkPaperSize`.
 *
 * Returns: the name of @size
 */
const char *
gtk_paper_size_get_name (GtkPaperSize *size)
{
  if (size->name)
    return size->name;
  g_assert (size->info != NULL);
  return paper_names + size->info->name;
}

/**
 * gtk_paper_size_get_display_name:
 * @size: a `GtkPaperSize` object
 *
 * Gets the human-readable name of the `GtkPaperSize`.
 *
 * Returns: the human-readable name of @size
 */
const char *
gtk_paper_size_get_display_name (GtkPaperSize *size)
{
  const char *display_name;

  if (size->display_name)
    return size->display_name;

  g_assert (size->info != NULL);

  display_name = paper_names + size->info->display_name;
  return g_dpgettext2 (GETTEXT_PACKAGE, "paper size", display_name);
}

/**
 * gtk_paper_size_get_ppd_name:
 * @size: a `GtkPaperSize` object
 *
 * Gets the PPD name of the `GtkPaperSize`, which
 * may be %NULL.
 *
 * Returns: the PPD name of @size
 */
const char *
gtk_paper_size_get_ppd_name (GtkPaperSize *size)
{
  if (size->ppd_name)
    return size->ppd_name;
  if (size->info)
    return paper_names + size->info->ppd_name;
  return NULL;
}

/**
 * gtk_paper_size_get_width:
 * @size: a `GtkPaperSize` object
 * @unit: the unit for the return value, not %GTK_UNIT_NONE
 *
 * Gets the paper width of the `GtkPaperSize`, in
 * units of @unit.
 *
 * Returns: the paper width
 */
double
gtk_paper_size_get_width (GtkPaperSize *size,
                          GtkUnit       unit)
{
  return _gtk_print_convert_from_mm (size->width, unit);
}

/**
 * gtk_paper_size_get_height:
 * @size: a `GtkPaperSize` object
 * @unit: the unit for the return value, not %GTK_UNIT_NONE
 *
 * Gets the paper height of the `GtkPaperSize`, in
 * units of @unit.
 *
 * Returns: the paper height
 */
double
gtk_paper_size_get_height (GtkPaperSize *size,
                           GtkUnit       unit)
{
  return _gtk_print_convert_from_mm (size->height, unit);
}

/**
 * gtk_paper_size_is_custom:
 * @size: a `GtkPaperSize` object
 *
 * Returns %TRUE if @size is not a standard paper size.
 *
 * Returns: whether @size is a custom paper size.
 **/
gboolean
gtk_paper_size_is_custom (GtkPaperSize *size)
{
  return size->is_custom;
}

/**
 * gtk_paper_size_is_ipp:
 * @size: a `GtkPaperSize` object
 *
 * Returns %TRUE if @size is an IPP standard paper size.
 *
 * Returns: whether @size is not an IPP custom paper size.
 **/
gboolean
gtk_paper_size_is_ipp (GtkPaperSize *size)
{
  return size->is_ipp;
}

/**
 * gtk_paper_size_set_size:
 * @size: a custom `GtkPaperSize` object
 * @width: the new width in units of @unit
 * @height: the new height in units of @unit
 * @unit: the unit for @width and @height
 *
 * Changes the dimensions of a @size to @width x @height.
 */
void
gtk_paper_size_set_size (GtkPaperSize *size,
                         double        width,
                         double        height,
                         GtkUnit       unit)
{
  g_return_if_fail (size != NULL);
  g_return_if_fail (size->is_custom);

  size->width = _gtk_print_convert_to_mm (width, unit);
  size->height = _gtk_print_convert_to_mm (height, unit);
}

#define NL_PAPER_GET(x)         \
  ((union { char *string; unsigned int word; })nl_langinfo(x)).word

/**
 * gtk_paper_size_get_default:
 *
 * Returns the name of the default paper size, which
 * depends on the current locale.
 *
 * Returns: the name of the default paper size. The string
 * is owned by GTK and should not be modified.
 */
const char *
gtk_paper_size_get_default (void)
{
  char *locale, *freeme = NULL;
  const char *paper_size;

#if defined(HAVE__NL_PAPER_HEIGHT) && defined(HAVE__NL_PAPER_WIDTH)
  {
    int width = NL_PAPER_GET (_NL_PAPER_WIDTH);
    int height = NL_PAPER_GET (_NL_PAPER_HEIGHT);

    if (width == 210 && height == 297)
      return GTK_PAPER_NAME_A4;

    if (width == 216 && height == 279)
      return GTK_PAPER_NAME_LETTER;
  }
#endif

#ifdef G_OS_WIN32
  freeme = locale = g_win32_getlocale ();
#elif defined(LC_PAPER)
  locale = setlocale(LC_PAPER, NULL);
#else
  locale = setlocale(LC_MESSAGES, NULL);
#endif

  if (!locale)
    return GTK_PAPER_NAME_A4;

  /* CLDR 1.8.1
   * http://unicode.org/repos/cldr-tmp/trunk/diff/supplemental/territory_language_information.html
   */
  if (g_regex_match_simple("[^_.@]{2,3}_(BZ|CA|CL|CO|CR|GT|MX|NI|PA|PH|PR|SV|US|VE)",
                           locale, G_REGEX_ANCHORED, G_REGEX_MATCH_ANCHORED))
    paper_size = GTK_PAPER_NAME_LETTER;
  else
    paper_size = GTK_PAPER_NAME_A4;

  g_free (freeme);
  return paper_size;
}

/* These get the default margins used for the paper size. Its
 * larger than most printers margins, so that it will be within
 * the imageble area on any printer.
 *
 * I’ve taken the actual values used from the OSX page setup dialog.
 * I’m not sure exactly where they got these values for, but might
 * correspond to this (from ghostscript docs):
 *
 * All DeskJets have 0.5 inches (1.27cm) of unprintable bottom margin,
 * due to the mechanical arrangement used to grab the paper. Side margins
 * are approximately 0.25 inches (0.64cm) for U.S. letter paper, and 0.15
 * inches (0.38cm) for A4.
 */

/**
 * gtk_paper_size_get_default_top_margin:
 * @size: a `GtkPaperSize` object
 * @unit: the unit for the return value, not %GTK_UNIT_NONE
 *
 * Gets the default top margin for the `GtkPaperSize`.
 *
 * Returns: the default top margin
 */
double
gtk_paper_size_get_default_top_margin (GtkPaperSize *size,
                                       GtkUnit       unit)
{
  double margin;

  margin = _gtk_print_convert_to_mm (0.25, GTK_UNIT_INCH);
  return _gtk_print_convert_from_mm (margin, unit);
}

/**
 * gtk_paper_size_get_default_bottom_margin:
 * @size: a `GtkPaperSize` object
 * @unit: the unit for the return value, not %GTK_UNIT_NONE
 *
 * Gets the default bottom margin for the `GtkPaperSize`.
 *
 * Returns: the default bottom margin
 */
double
gtk_paper_size_get_default_bottom_margin (GtkPaperSize *size,
                                          GtkUnit       unit)
{
  double margin;
  const char *name;

  margin = _gtk_print_convert_to_mm (0.25, GTK_UNIT_INCH);

  name = gtk_paper_size_get_name (size);
  if (strcmp (name, "na_letter") == 0 ||
      strcmp (name, "na_legal") == 0 ||
      strcmp (name, "iso_a4") == 0)
    margin = _gtk_print_convert_to_mm (0.56, GTK_UNIT_INCH);

  return _gtk_print_convert_from_mm (margin, unit);
}

/**
 * gtk_paper_size_get_default_left_margin:
 * @size: a `GtkPaperSize` object
 * @unit: the unit for the return value, not %GTK_UNIT_NONE
 *
 * Gets the default left margin for the `GtkPaperSize`.
 *
 * Returns: the default left margin
 */
double
gtk_paper_size_get_default_left_margin (GtkPaperSize *size,
                                        GtkUnit       unit)
{
  double margin;

  margin = _gtk_print_convert_to_mm (0.25, GTK_UNIT_INCH);
  return _gtk_print_convert_from_mm (margin, unit);
}

/**
 * gtk_paper_size_get_default_right_margin:
 * @size: a `GtkPaperSize` object
 * @unit: the unit for the return value, not %GTK_UNIT_NONE
 *
 * Gets the default right margin for the `GtkPaperSize`.
 *
 * Returns: the default right margin
 */
double
gtk_paper_size_get_default_right_margin (GtkPaperSize *size,
                                         GtkUnit       unit)
{
  double margin;

  margin = _gtk_print_convert_to_mm (0.25, GTK_UNIT_INCH);
  return _gtk_print_convert_from_mm (margin, unit);
}

/**
 * gtk_paper_size_new_from_key_file:
 * @key_file: the `GKeyFile` to retrieve the papersize from
 * @group_name: (nullable): the name of the group in the key file to read,
 *   or %NULL to read the first group
 * @error: (nullable): return location for an error
 *
 * Reads a paper size from the group @group_name in the key file
 * @key_file.
 *
 * Returns: a new `GtkPaperSize` object with the restored paper size
 */
GtkPaperSize *
gtk_paper_size_new_from_key_file (GKeyFile     *key_file,
                                  const char   *group_name,
                                  GError      **error)
{
  GtkPaperSize *paper_size = NULL;
  char *name = NULL;
  char *ppd_name = NULL;
  char *display_name = NULL;
  char *freeme = NULL;
  double width, height;
  GError *err = NULL;

  g_return_val_if_fail (key_file != NULL, NULL);

  if (!group_name)
    group_name = freeme = g_key_file_get_start_group (key_file);
  if (!group_name || !g_key_file_has_group (key_file, group_name))
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
    {\
      g_propagate_error (error, err);\
      goto out;\
    }

  GET_DOUBLE (key_file, group_name, "Width", width);
  GET_DOUBLE (key_file, group_name, "Height", height);

#undef GET_DOUBLE

  name = g_key_file_get_string (key_file, group_name,
                                "Name", NULL);
  ppd_name = g_key_file_get_string (key_file, group_name,
                                    "PPDName", NULL);
  display_name = g_key_file_get_string (key_file, group_name,
                                        "DisplayName", NULL);
  /* Fallback for old ~/.gtk-custom-paper entries */
  if (!display_name)
    display_name = g_strdup (name);

  if (ppd_name != NULL)
    paper_size = gtk_paper_size_new_from_ppd (ppd_name,
                                              display_name,
                                              _gtk_print_convert_from_mm (width, GTK_UNIT_POINTS),
                                              _gtk_print_convert_from_mm (height, GTK_UNIT_POINTS));
  else if (name != NULL)
    paper_size = gtk_paper_size_new_custom (name, display_name,
                                            width, height, GTK_UNIT_MM);
  else
    {
      g_set_error_literal (error,
                           GTK_PRINT_ERROR,
                           GTK_PRINT_ERROR_INVALID_FILE,
                           _("Not a valid page setup file"));
      goto out;
    }

  g_assert (paper_size != NULL);

out:
  g_free (ppd_name);
  g_free (name);
  g_free (display_name);
  g_free (freeme);

  return paper_size;
}

/**
 * gtk_paper_size_to_key_file:
 * @size: a `GtkPaperSize`
 * @key_file: the `GKeyFile` to save the paper size to
 * @group_name: the group to add the settings to in @key_file
 *
 * This function adds the paper size from @size to @key_file.
 */
void
gtk_paper_size_to_key_file (GtkPaperSize *size,
                            GKeyFile     *key_file,
                            const char   *group_name)
{
  const char *name, *ppd_name, *display_name;

  g_return_if_fail (size != NULL);
  g_return_if_fail (key_file != NULL);

  name = gtk_paper_size_get_name (size);
  display_name = gtk_paper_size_get_display_name (size);
  ppd_name = gtk_paper_size_get_ppd_name (size);

  if (ppd_name != NULL)
    g_key_file_set_string (key_file, group_name,
                           "PPDName", ppd_name);
  else
    g_key_file_set_string (key_file, group_name,
                           "Name", name);

  if (display_name)
    g_key_file_set_string (key_file, group_name,
                           "DisplayName", display_name);

  g_key_file_set_double (key_file, group_name,
                         "Width", gtk_paper_size_get_width (size, GTK_UNIT_MM));
  g_key_file_set_double (key_file, group_name,
                         "Height", gtk_paper_size_get_height (size, GTK_UNIT_MM));
}

/**
 * gtk_paper_size_to_gvariant:
 * @paper_size: a `GtkPaperSize`
 *
 * Serialize a paper size to an `a{sv}` variant.
 *
 * Returns: (transfer none): a new, floating, `GVariant`
 */
GVariant *
gtk_paper_size_to_gvariant (GtkPaperSize *paper_size)
{
  const char *name;
  const char *ppd_name;
  const char *display_name;
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);

  name = gtk_paper_size_get_name (paper_size);
  ppd_name = gtk_paper_size_get_ppd_name (paper_size);
  display_name = gtk_paper_size_get_display_name (paper_size);

  if (ppd_name != NULL)
    g_variant_builder_add (&builder, "{sv}", "PPDName", g_variant_new_string (ppd_name));
  else
    g_variant_builder_add (&builder, "{sv}", "Name", g_variant_new_string (name));

  if (display_name != NULL)
    g_variant_builder_add (&builder, "{sv}", "DisplayName", g_variant_new_string (display_name));

  g_variant_builder_add (&builder, "{sv}", "Width", g_variant_new_double (gtk_paper_size_get_width (paper_size, GTK_UNIT_MM)));
  g_variant_builder_add (&builder, "{sv}", "Height", g_variant_new_double (gtk_paper_size_get_height (paper_size, GTK_UNIT_MM)));

  return g_variant_builder_end (&builder);
}

/**
 * gtk_paper_size_new_from_gvariant:
 * @variant: an a{sv} `GVariant`
 *
 * Deserialize a paper size from a `GVariant`.
 *
 * The `GVariant must be in the format produced by
 * [method@Gtk.PaperSize.to_gvariant].
 *
 * Returns: (transfer full): a new `GtkPaperSize` object
 */
GtkPaperSize *
gtk_paper_size_new_from_gvariant (GVariant *variant)
{
  GtkPaperSize *paper_size;
  const char *name;
  const char *ppd_name;
  const char *display_name;
  double width, height;

  g_return_val_if_fail (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARDICT), NULL);

  if (!g_variant_lookup (variant, "Width", "d", &width) ||
      !g_variant_lookup (variant, "Height", "d", &height))
    return NULL;

  if (!g_variant_lookup (variant, "Name", "&s", &name))
    name = NULL;

  if (!g_variant_lookup (variant, "PPDName", "&s", &ppd_name))
    ppd_name = NULL;

  if (!g_variant_lookup (variant, "DisplayName", "&s", &display_name))
    display_name = name;

  if (ppd_name != NULL)
    paper_size = gtk_paper_size_new_from_ppd (ppd_name,
                                              display_name,
                                              _gtk_print_convert_from_mm (width, GTK_UNIT_POINTS),
                                              _gtk_print_convert_from_mm (height, GTK_UNIT_POINTS));
  else if (name != NULL)
    paper_size = gtk_paper_size_new_custom (name, display_name,
                                            width, height, GTK_UNIT_MM);
  else
    paper_size = NULL;

  return paper_size;
}
