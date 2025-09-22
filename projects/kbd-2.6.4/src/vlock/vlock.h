
/*

  Main header for vlock, the VT locking program for linux.

  Copyright (C) 1994-1998 Michael K. Johnson <johnsonm@redhat.com>
  Copyright (C) 2002, 2005 Dmitry V. Levin <ldv@altlinux.org>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

int init_vt(const char *tty);
void restore_vt(void);

void init_screen(void);
void restore_screen(void);

const char *get_username(void);

void parse(int ac, char *const av[]);

const char *locked_name(void);

/*
 * This determines whether the default behavior is to lock only the
 * current VT or all of them.
 * 0 means current, 1 means all.
 */
extern int o_lock_all;

/* Copy of the VT mode when the program was started. */
extern int is_vt;
