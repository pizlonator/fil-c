/*
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 *
 */

/* From http://en.wikipedia.org/wiki/Date_notation_by_country */
typedef enum {
  DATE_ENDIANESS_BIG,    /* Big-endian (year, month, day), e.g. 03-04-05 */
  DATE_ENDIANESS_LITTLE, /* Little-endian (day, month, year), e.g. 05/04/03 */
  DATE_ENDIANESS_MIDDLE, /* Middle-endian (month, day, year), e.g. 04/05/03 */
  DATE_ENDIANESS_YDM     /* YDM-endian (year, day, month), e.g. 03/05/04 */
} DateEndianess;

DateEndianess date_endian_get_default  (gboolean verbose);
DateEndianess date_endian_get_for_lang (const char *lang,
					gboolean    verbose);
const char  * date_endian_to_string    (DateEndianess endianess);
