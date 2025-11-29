/* GTK - The GIMP Toolkit
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

#include "gtkversion.h"

/**
 * gtk_get_major_version:
 *
 * Returns the major version number of the GTK library.
 *
 * For example, in GTK version 3.1.5 this is 3.
 *
 * This function is in the library, so it represents the GTK library
 * your code is running against. Contrast with the %GTK_MAJOR_VERSION
 * macro, which represents the major version of the GTK headers you
 * have included when compiling your code.
 *
 * Returns: the major version number of the GTK library
 */
guint
gtk_get_major_version (void)
{
  return GTK_MAJOR_VERSION;
}

/**
 * gtk_get_minor_version:
 *
 * Returns the minor version number of the GTK library.
 *
 * For example, in GTK version 3.1.5 this is 1.
 *
 * This function is in the library, so it represents the GTK library
 * your code is are running against. Contrast with the
 * %GTK_MINOR_VERSION macro, which represents the minor version of the
 * GTK headers you have included when compiling your code.
 *
 * Returns: the minor version number of the GTK library
 */
guint
gtk_get_minor_version (void)
{
  return GTK_MINOR_VERSION;
}

/**
 * gtk_get_micro_version:
 *
 * Returns the micro version number of the GTK library.
 *
 * For example, in GTK version 3.1.5 this is 5.
 *
 * This function is in the library, so it represents the GTK library
 * your code is are running against. Contrast with the
 * %GTK_MICRO_VERSION macro, which represents the micro version of the
 * GTK headers you have included when compiling your code.
 *
 * Returns: the micro version number of the GTK library
 */
guint
gtk_get_micro_version (void)
{
  return GTK_MICRO_VERSION;
}

/**
 * gtk_get_binary_age:
 *
 * Returns the binary age as passed to `libtool`.
 *
 * If `libtool` means nothing to you, don't worry about it.
 *
 * Returns: the binary age of the GTK library
 */
guint
gtk_get_binary_age (void)
{
  return GTK_BINARY_AGE;
}

/**
 * gtk_get_interface_age:
 *
 * Returns the interface age as passed to `libtool`.
 *
 * If `libtool` means nothing to you, don't worry about it.
 *
 * Returns: the interface age of the GTK library
 */
guint
gtk_get_interface_age (void)
{
  return GTK_INTERFACE_AGE;
}

/**
 * gtk_check_version:
 * @required_major: the required major version
 * @required_minor: the required minor version
 * @required_micro: the required micro version
 *
 * Checks that the GTK library in use is compatible with the
 * given version.
 *
 * Generally you would pass in the constants %GTK_MAJOR_VERSION,
 * %GTK_MINOR_VERSION, %GTK_MICRO_VERSION as the three arguments
 * to this function; that produces a check that the library in
 * use is compatible with the version of GTK the application or
 * module was compiled against.
 *
 * Compatibility is defined by two things: first the version
 * of the running library is newer than the version
 * @required_major.required_minor.@required_micro. Second
 * the running library must be binary compatible with the
 * version @required_major.required_minor.@required_micro
 * (same major version.)
 *
 * This function is primarily for GTK modules; the module
 * can call this function to check that it wasn’t loaded
 * into an incompatible version of GTK. However, such a
 * check isn’t completely reliable, since the module may be
 * linked against an old version of GTK and calling the
 * old version of gtk_check_version(), but still get loaded
 * into an application using a newer version of GTK.
 *
 * Returns: (nullable): %NULL if the GTK library is compatible with the
 *   given version, or a string describing the version mismatch.
 *   The returned string is owned by GTK and should not be modified
 *   or freed.
 */
const char *
gtk_check_version (guint required_major,
                   guint required_minor,
                   guint required_micro)
{
  int gtk_effective_micro = 100 * GTK_MINOR_VERSION + GTK_MICRO_VERSION;
  int required_effective_micro = 100 * required_minor + required_micro;

  if (required_major > GTK_MAJOR_VERSION)
    return "GTK version too old (major mismatch)";
  if (required_major < GTK_MAJOR_VERSION)
    return "GTK version too new (major mismatch)";
  if (required_effective_micro < gtk_effective_micro - GTK_BINARY_AGE)
    return "GTK version too new (micro mismatch)";
  if (required_effective_micro > gtk_effective_micro)
    return "GTK version too old (micro mismatch)";
  return NULL;
}
