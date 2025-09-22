/*
 * SPDX-FileCopyrightText: 1989 - 1991, Julianne Frances Haugh
 * SPDX-FileCopyrightText: 1996 - 1997, Marek Michałkiewicz
 * SPDX-FileCopyrightText: 2005       , Tomasz Kłoczko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * port.h - structure of /etc/porttime
 *
 *	$Id$
 *
 *	Each entry in /etc/porttime consists of a TTY device
 *	name or "*" to indicate all TTY devices, followed by
 *	a list of 1 or more user IDs or "*" to indicate all
 *	user names, followed by a list of zero or more valid
 *	login times.  Login time entries consist of zero or
 *	more day names (Su, Mo, Tu, We, Th, Fr, Sa, Wk, Al)
 *	followed by a pair of time values in HHMM format
 *	separated by a "-".
 */

/*
 * PORTS - Name of system port access time file.
 * PORT_IDS - Allowable number of IDs per entry.
 * PORT_TTY - Allowable number of TTYs per entry.
 * PORT_TIMES - Allowable number of time entries per entry.
 * PORT_DAY - Day of the week to a bit value (0 = Sunday).
 */

#define	PORTS	"/etc/porttime"
#define	PORT_IDS	64
#define	PORT_TTY	64
#define	PORT_TIMES	24
#define	PORT_DAY(day)	(1<<(day))

/*
 *	pt_names - pointer to array of device names in /dev/
 *	pt_users - pointer to array of applicable user IDs.
 *	pt_times - pointer to list of allowable time periods.
 */

struct port {
	char **pt_names;
	char **pt_users;
	struct pt_time *pt_times;
};

/*
 *	t_days - bit array for each day of the week (0 = Sunday)
 *	t_start - starting time for this entry
 *	t_end - ending time for this entry
 */

struct pt_time {
	short t_days;
	short t_start;
	short t_end;
};
