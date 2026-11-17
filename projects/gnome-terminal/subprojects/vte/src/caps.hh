/*
 * Copyright (C) 2001,2002 Red Hat, Inc.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

/* The interfaces in this file are subject to change at any time. */

#pragma once

#define _VTE_CAP_ESC "\033"		/* Escape */
#define _VTE_CAP_CSI _VTE_CAP_ESC "["	/* Control Sequence Introducer */
#define _VTE_CAP_ST  _VTE_CAP_ESC "\\"	/* String Terminator */
#define _VTE_CAP_OSC _VTE_CAP_ESC "]"	/* Operating System Command */
#define _VTE_CAP_PM  _VTE_CAP_ESC "^"	/* Privacy Message */
#define _VTE_CAP_APC _VTE_CAP_ESC "_"	/* Application Program Command */
#define _VTE_CAP_SS2 _VTE_CAP_ESC "N"	/* Single-shift to G2 */
#define _VTE_CAP_SS3 _VTE_CAP_ESC "O"	/* Single-shift to G3 */
