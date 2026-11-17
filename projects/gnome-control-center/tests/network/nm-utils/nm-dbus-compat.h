/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright 2015 Red Hat, Inc.
 */

#ifndef __NM_DBUS_COMPAT_H__
#define __NM_DBUS_COMPAT_H__

/* Copied from <dbus/dbus-shared.h> */

/* Bus names */

/** The bus name used to talk to the bus itself. */
#define DBUS_SERVICE_DBUS      "org.freedesktop.DBus"

/* Paths */
/** The object path used to talk to the bus itself. */
#define DBUS_PATH_DBUS  "/org/freedesktop/DBus"
/** The object path used in local/in-process-generated messages. */
#define DBUS_PATH_LOCAL "/org/freedesktop/DBus/Local"

/* Interfaces, these #define don't do much other than
 * catch typos at compile time
 */
/** The interface exported by the object with #DBUS_SERVICE_DBUS and #DBUS_PATH_DBUS */
#define DBUS_INTERFACE_DBUS           "org.freedesktop.DBus"
/** The interface supported by introspectable objects */
#define DBUS_INTERFACE_INTROSPECTABLE "org.freedesktop.DBus.Introspectable"
/** The interface supported by objects with properties */
#define DBUS_INTERFACE_PROPERTIES     "org.freedesktop.DBus.Properties"
/** The interface supported by most dbus peers */
#define DBUS_INTERFACE_PEER           "org.freedesktop.DBus.Peer"

/** This is a special interface whose methods can only be invoked
 * by the local implementation (messages from remote apps aren't
 * allowed to specify this interface).
 */
#define DBUS_INTERFACE_LOCAL "org.freedesktop.DBus.Local"

/* Owner flags */
#define DBUS_NAME_FLAG_ALLOW_REPLACEMENT 0x1 /**< Allow another service to become the primary owner if requested */
#define DBUS_NAME_FLAG_REPLACE_EXISTING  0x2 /**< Request to replace the current primary owner */
#define DBUS_NAME_FLAG_DO_NOT_QUEUE      0x4 /**< If we can not become the primary owner do not place us in the queue */

/* Replies to request for a name */
#define DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER  1 /**< Service has become the primary owner of the requested name */
#define DBUS_REQUEST_NAME_REPLY_IN_QUEUE       2 /**< Service could not become the primary owner and has been placed in the queue */
#define DBUS_REQUEST_NAME_REPLY_EXISTS         3 /**< Service is already in the queue */
#define DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER  4 /**< Service is already the primary owner */

/* Replies to releasing a name */
#define DBUS_RELEASE_NAME_REPLY_RELEASED        1 /**< Service was released from the given name */
#define DBUS_RELEASE_NAME_REPLY_NON_EXISTENT    2 /**< The given name does not exist on the bus */
#define DBUS_RELEASE_NAME_REPLY_NOT_OWNER       3 /**< Service is not an owner of the given name */

/* Replies to service starts */
#define DBUS_START_REPLY_SUCCESS         1 /**< Service was auto started */
#define DBUS_START_REPLY_ALREADY_RUNNING 2 /**< Service was already running */

#endif  /* __NM_DBUS_COMPAT_H__ */
