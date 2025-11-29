/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkscreen-x11.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>

static void
insert_atom_pair (GdkDisplay *display,
                  const char *string,
		  Atom        xatom)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);  
  char *s;
  
  if (!display_x11->atom_from_string)
    {
      display_x11->atom_from_string = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
      display_x11->atom_to_string = g_hash_table_new (NULL, NULL);
    }
  
  s = g_strdup (string);
  g_hash_table_insert (display_x11->atom_from_string, s, GUINT_TO_POINTER (xatom));
  g_hash_table_insert (display_x11->atom_to_string, GUINT_TO_POINTER (xatom), s);
}

static Atom
lookup_cached_xatom (GdkDisplay *display,
		     const char *string)
{
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  if (display_x11->atom_from_string)
    return GPOINTER_TO_UINT (g_hash_table_lookup (display_x11->atom_from_string, string));

  return None;
}

/**
 * gdk_x11_get_xatom_by_name_for_display:
 * @display: (type GdkX11Display): a `GdkDisplay`
 * @atom_name: a string
 * 
 * Returns the X atom for a `GdkDisplay` corresponding to @atom_name.
 * This function caches the result, so if called repeatedly it is much
 * faster than XInternAtom(), which is a round trip to the server each time.
 * 
 * Returns: a X atom for a `GdkDisplay`
 **/
Atom
gdk_x11_get_xatom_by_name_for_display (GdkDisplay  *display,
				       const char *atom_name)
{
  Atom xatom = None;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), None);

  if (atom_name == NULL)
    return None;

  if (gdk_display_is_closed (display))
    return None;

  xatom = lookup_cached_xatom (display, atom_name);

  if (!xatom)
    {
      xatom = XInternAtom (GDK_DISPLAY_XDISPLAY (display), atom_name, FALSE);
      insert_atom_pair (display, atom_name, xatom);
    }

  return xatom;
}

void
_gdk_x11_precache_atoms (GdkDisplay          *display,
			 const char * const *atom_names,
			 int                  n_atoms)
{
  Atom *xatoms;
  const char **xatom_names;
  int n_xatoms;
  int i;

  xatoms = g_new (Atom, n_atoms);
  xatom_names = g_new (const char *, n_atoms);

  n_xatoms = 0;
  for (i = 0; i < n_atoms; i++)
    {
      if (lookup_cached_xatom (display, atom_names[i]) == None)
	{
	  xatom_names[n_xatoms] = atom_names[i];
	  n_xatoms++;
	}
    }

  if (n_xatoms)
    XInternAtoms (GDK_DISPLAY_XDISPLAY (display),
                  (char **) xatom_names, n_xatoms, False, xatoms);

  for (i = 0; i < n_xatoms; i++)
    insert_atom_pair (display, xatom_names[i], xatoms[i]);

  g_free (xatoms);
  g_free (xatom_names);
}

/**
 * gdk_x11_get_xatom_name_for_display:
 * @display: (type GdkX11Display): the `GdkDisplay` where @xatom is defined
 * @xatom: an X atom 
 * 
 * Returns the name of an X atom for its display. This
 * function is meant mainly for debugging, so for convenience, unlike
 * XAtomName() and the result doesn’t need to
 * be freed. 
 *
 * Returns: name of the X atom; this string is owned by GDK,
 *   so it shouldn’t be modified or freed. 
 **/
const char *
gdk_x11_get_xatom_name_for_display (GdkDisplay *display,
				    Atom        xatom)

{
  GdkX11Display *display_x11;
  const char *string;
  
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (xatom == None)
    return NULL;

  if (gdk_display_is_closed (display))
    return NULL;

  display_x11 = GDK_X11_DISPLAY (display);
  
  if (display_x11->atom_to_string)
    string = g_hash_table_lookup (display_x11->atom_to_string,
				  GUINT_TO_POINTER (xatom));
  else
    string = NULL;
  
  if (!string)
    {
      /* If this atom doesn't exist, we'll die with an X error unless
       * we take precautions
       */
      char *name;
      gdk_x11_display_error_trap_push (display);
      name = XGetAtomName (GDK_DISPLAY_XDISPLAY (display), xatom);
      if (gdk_x11_display_error_trap_pop (display))
	{
	  g_warning (G_STRLOC " invalid X atom: %ld", xatom);
	}
      else
	{
	  insert_atom_pair (display, name, xatom);
	  XFree (name);
          string = g_hash_table_lookup (display_x11->atom_to_string,
                                        GUINT_TO_POINTER (xatom));
	}
    }

  return string;
}

Atom
_gdk_x11_get_xatom_for_display_printf (GdkDisplay    *display,
                                       const char    *format,
                                       ...)
{
  va_list args;
  char *atom_name;
  Atom atom;

  va_start (args, format);
  atom_name = g_strdup_vprintf (format, args);
  va_end (args);

  atom = gdk_x11_get_xatom_by_name_for_display (display, atom_name);

  g_free (atom_name);

  return atom;
}

