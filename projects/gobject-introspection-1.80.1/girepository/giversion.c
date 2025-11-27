/* Copyright (C) 2018 Christoph Reiter
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <girepository.h>

/**
 * SECTION:giversion
 * @Title: Version Information
 * @Short_description: macros and functions to check the girepository version
 */

/**
 * GI_MAJOR_VERSION:
 *
 * The major version number of the girepository library.
 *
 * Since: 1.60
 */

/**
 * GI_MINOR_VERSION:
 *
 * The minor version number of the girepository library.
 *
 * Since: 1.60
 */

/**
 * GI_MICRO_VERSION:
 *
 * The micro version number of the girepository library.
 *
 * Since: 1.60
 */

/**
 * GI_CHECK_VERSION:
 * @major: the major version to check for
 * @minor: the minor version to check for
 * @micro: the micro version to check for
 *
 * Checks the version of the girepository library that is being compiled
 * against.
 *
 * Returns: %TRUE if the version of the girepository header files
 * is the same as or newer than the passed-in version.
 *
 * Since: 1.60
 */

/**
 * gi_get_major_version:
 *
 * Returns the major version number of the girepository library.
 * (e.g. in version 1.58.2 this is 1.)
 *
 * Returns: the major version number of the girepository library
 *
 * Since: 1.60
 */
guint
gi_get_major_version (void)
{
  return GI_MAJOR_VERSION;
}

/**
 * gi_get_minor_version:
 *
 * Returns the minor version number of the girepository library.
 * (e.g. in version 1.58.2 this is 58.)
 *
 * Returns: the minor version number of the girepository library
 *
 * Since: 1.60
 */
guint
gi_get_minor_version (void)
{
  return GI_MINOR_VERSION;
}

/**
 * gi_get_micro_version:
 *
 * Returns the micro version number of the girepository library.
 * (e.g. in version 1.58.2 this is 2.)
 *
 * Returns: the micro version number of the girepository library
 *
 * Since: 1.60
 */
guint
gi_get_micro_version (void)
{
  return GI_MICRO_VERSION;
}
