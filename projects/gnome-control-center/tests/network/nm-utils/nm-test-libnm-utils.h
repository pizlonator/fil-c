/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright 2014 - 2015 Red Hat, Inc.
 */

#include "NetworkManager.h"

#include "nm-test-utils.h"

#if (NETWORKMANAGER_COMPILATION) & NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_GLIB
#include "nm-dbus-glib-types.h"
#endif

/*****************************************************************************/

typedef struct {
	GDBusConnection *bus;
	GDBusProxy *proxy;
	GPid pid;
	int keepalive_fd;
#if (NETWORKMANAGER_COMPILATION) & NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_GLIB
	struct {
		DBusGConnection *bus;
	} libdbus;
#endif
} NMTstcServiceInfo;

NMTstcServiceInfo *nmtstc_service_init (void);
void nmtstc_service_cleanup (NMTstcServiceInfo *info);

static inline void _nmtstc_auto_service_cleanup (NMTstcServiceInfo **info)
{
	if (info && *info) {
		nmtstc_service_cleanup (*info);
		*info = NULL;
	}
}

#define NMTSTC_SERVICE_INFO_SETUP(sinfo) \
	NM_PRAGMA_WARNING_DISABLE ("-Wunused-variable") \
	__attribute__ ((cleanup(_nmtstc_auto_service_cleanup))) NMTstcServiceInfo *sinfo = nmtstc_service_init (); \
	NM_PRAGMA_WARNING_REENABLE

/*****************************************************************************/

#if (NETWORKMANAGER_COMPILATION) & NM_NETWORKMANAGER_COMPILATION_WITH_LIBNM_GLIB

#include "nm-client.h"
#include "nm-remote-settings.h"

NMClient *nmtstc_nm_client_new (void);
NMRemoteSettings *nmtstc_nm_remote_settings_new (void);

#else

NMDevice *nmtstc_service_add_device (NMTstcServiceInfo *info,
                                     NMClient *client,
                                     const char *method,
                                     const char *ifname);

NMDevice * nmtstc_service_add_wired_device (NMTstcServiceInfo *sinfo,
                                            NMClient *client,
                                            const char *ifname,
                                            const char *hwaddr,
                                            const char **subchannels);

#endif

/*****************************************************************************/

void nmtstc_service_add_connection (NMTstcServiceInfo *sinfo,
                                    NMConnection *connection,
                                    gboolean verify_connection,
                                    char **out_path);

void nmtstc_service_add_connection_variant (NMTstcServiceInfo *sinfo,
                                            GVariant *connection,
                                            gboolean verify_connection,
                                            char **out_path);

void nmtstc_service_update_connection (NMTstcServiceInfo *sinfo,
                                       const char *path,
                                       NMConnection *connection,
                                       gboolean verify_connection);

void nmtstc_service_update_connection_variant (NMTstcServiceInfo *sinfo,
                                               const char *path,
                                               GVariant *connection,
                                               gboolean verify_connection);

