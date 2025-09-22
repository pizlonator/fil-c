/*
 * plymouth-ctrl.h	Header file for communications with plymouthd
 *
 * Copyright (c) 2016 SUSE Linux GmbH, All rights reserved.
 * Copyright (c) 2016 Werner Fink <werner@suse.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Author: Werner Fink <werner@suse.de>
 */

/*
 * Taken from plymouth 0.9.0 src/ply-boot-protocol.h
 */

#ifndef UTIL_LINUX_PLYMOUTH_CTRL_H
#define UTIL_LINUX_PLYMOUTH_CTRL_H

#define PLYMOUTH_SOCKET_PATH	"\0/org/freedesktop/plymouthd"
#define ANSWER_TYP		'\x2'
#define ANSWER_ENQ		'\x5'
#define ANSWER_ACK		'\x6'
#define ANSWER_MLT		'\t'
#define ANSWER_NCK		'\x15'

#define MAGIC_PRG_STOP		'A'
#define MAGIC_PRG_CONT		'a'
#define MAGIC_UPDATE		'U'
#define MAGIC_SYS_UPDATE	'u'
#define MAGIC_SYS_INIT		'S'
#define MAGIC_DEACTIVATE	'D'
#define MAGIC_REACTIVATE	'r'
#define MAGIC_SHOW_SPLASH	'$'
#define MAGIC_HIDE_SPLASH	'H'
#define MAGIC_CHMOD		'C'
#define MAGIC_CHROOT		'R'
#define MAGIC_ACTIVE_VT		'V'
#define MAGIC_QUESTION		'W'
#define MAGIC_SHOW_MSG		'M'
#define MAGIC_HIDE_MSG		'm'
#define MAGIC_KEYSTROKE		'K'
#define MAGIC_KEYSTROKE_RM	'L'
#define MAGIC_PING		'P'
#define MAGIC_QUIT		'Q'
#define MAGIC_CACHED_PWD	'c'
#define MAGIC_ASK_PWD		'*'
#define MAGIC_DETAILS		'!'

#define PLYMOUTH_TERMIOS_FLAGS_DELAY	30
extern int plymouth_command(int cmd, ...);

#endif /* UTIL_LINUX_PLYMOUTH_CTRL_H */
