/*
 * Copyright © 2017 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Christian J. Kellner <christian@kellner.me>
 */

#pragma once

#include "bolt-names.h"
#include "bolt-enum-types.h"


gboolean          bolt_enum_validate (GType    enum_type,
                                      gint     value,
                                      GError **error);

gboolean          bolt_enum_class_validate (GEnumClass *enum_class,
                                            gint        value,
                                            GError    **error);

const char *      bolt_enum_to_string (GType    enum_type,
                                       gint     value,
                                       GError **error);

gint              bolt_enum_from_string (GType       enum_type,
                                         const char *string,
                                         GError    **error);


char *            bolt_flags_class_to_string (GFlagsClass *flags_class,
                                              guint        value,
                                              GError     **error);

gboolean          bolt_flags_class_from_string (GFlagsClass *flags_class,
                                                const char  *string,
                                                guint       *flags_out,
                                                GError     **error);

char *            bolt_flags_to_string (GType    flags_type,
                                        guint    value,
                                        GError **error);

gboolean          bolt_flags_from_string (GType       flags_type,
                                          const char *string,
                                          guint      *flags_out,
                                          GError    **error);

gboolean          bolt_flags_update (guint  from,
                                     guint *to,
                                     guint  mask);

#define bolt_flag_isset(flags_, flag_)  (!!(flags_ & flag_))
#define bolt_flag_isclear(flags_, flag_) (!(flags_ & flag_))

/**
 * BoltStatus:
 * @BOLT_STATUS_UNKNOWN: Device is in an unknown state (should normally not happen).
 * @BOLT_STATUS_DISCONNECTED: Device is not connected.
 * @BOLT_STATUS_CONNECTING: Device is currently being connected.
 * @BOLT_STATUS_CONNECTED: Device is connected, but not authorized.
 * @BOLT_STATUS_AUTHORIZING: Device is currently authorizing.
 * @BOLT_STATUS_AUTH_ERROR: Failed to authorize a device via a key.
 * @BOLT_STATUS_AUTHORIZED: Device connected and authorized.
 * @BOLT_STATUS_AUTHORIZED_SECURE: Device connected and securely authorized via a key (deprecated).
 * @BOLT_STATUS_AUTHORIZED_NEWKEY: Device connected and authorized via a new key (deprecated).
 * @BOLT_STATUS_AUTHORIZED_DPONLY: Device authorized but with thunderbolt disabled (deprecated).
 *
 * The current status of the device.
 */
typedef enum {

  BOLT_STATUS_UNKNOWN = -1,
  BOLT_STATUS_DISCONNECTED = 0,
  BOLT_STATUS_CONNECTING,
  BOLT_STATUS_CONNECTED,
  BOLT_STATUS_AUTHORIZING,
  BOLT_STATUS_AUTH_ERROR,
  BOLT_STATUS_AUTHORIZED,

  /* deprecated, do not use */
  BOLT_STATUS_AUTHORIZED_SECURE,
  BOLT_STATUS_AUTHORIZED_NEWKEY,
  BOLT_STATUS_AUTHORIZED_DPONLY

} BoltStatus;

const char *     bolt_status_to_string (BoltStatus status);
gboolean         bolt_status_is_authorized (BoltStatus status);
gboolean         bolt_status_is_connected (BoltStatus status);
gboolean         bolt_status_is_pending (BoltStatus status);
gboolean         bolt_status_validate (BoltStatus status);

/**
 * BoltAuthFlags:
 * @BOLT_AUTH_NONE: No specific authorization.
 * @BOLT_AUTH_NOPCIE: PCIe tunnels are *not* authorized.
 * @BOLT_AUTH_SECURE: Device is securely authorized.
 * @BOLT_AUTH_NOKEY: Device does *not* support key verification.
 * @BOLT_AUTH_BOOT: Device was already authorized during pre-boot.
 *
 * More specific information about device authorization.
 */
typedef enum { /*< flags >*/

  BOLT_AUTH_NONE   = 0,
  BOLT_AUTH_NOPCIE = 1 << 0,
  BOLT_AUTH_SECURE = 1 << 1,
  BOLT_AUTH_NOKEY  = 1 << 2,
  BOLT_AUTH_BOOT   = 1 << 3,

} BoltAuthFlags;

/**
 * BoltKeyState:
 * @BOLT_KEY_UNKNOWN: unknown key state
 * @BOLT_KEY_MISSING: no key
 * @BOLT_KEY_HAVE: key exists
 * @BOLT_KEY_NEW: key is new
 *
 * The state of the key.
 */

typedef enum {

  BOLT_KEY_UNKNOWN = -1,
  BOLT_KEY_MISSING = 0,
  BOLT_KEY_HAVE = 1,
  BOLT_KEY_NEW = 2

} BoltKeyState;

/**
 * BoltSecurity:
 * @BOLT_SECURITY_UNKNOWN : Unknown security.
 * @BOLT_SECURITY_NONE    : No security, all devices are automatically connected.
 * @BOLT_SECURITY_DPONLY  : Display Port only devices only.
 * @BOLT_SECURITY_USER    : User needs to authorize devices.
 * @BOLT_SECURITY_SECURE  : User needs to authorize devices. Authorization can
 *     be done via key exchange to verify the device identity.
 * @BOLT_SECURITY_USBONLY : Only create a PCIe tunnel to the USB controller in a
 *     connected thunderbolt dock, allowing no downstream PCIe tunnels.
 *
 * The security level of the thunderbolt domain.
 */
typedef enum {

  BOLT_SECURITY_UNKNOWN = -1,
  BOLT_SECURITY_NONE = 0,
  BOLT_SECURITY_DPONLY = 1,
  BOLT_SECURITY_USER = '1',
  BOLT_SECURITY_SECURE = '2',
  BOLT_SECURITY_USBONLY = 4,

} BoltSecurity;


BoltSecurity     bolt_security_from_string (const char *str);
const char *     bolt_security_to_string (BoltSecurity security);
gboolean         bolt_security_validate (BoltSecurity security);
gboolean         bolt_security_allows_pcie (BoltSecurity security);

/**
 * BoltPolicy:
 * @BOLT_POLICY_UNKNOWN: Unknown policy.
 * @BOLT_POLICY_DEFAULT: Default policy.
 * @BOLT_POLICY_MANUAL: Manual authorization of the device.
 * @BOLT_POLICY_AUTO: Connect the device automatically,
 *   with the best possible security level supported
 *   by the domain controller.
 *
 * What do to for connected devices.
 */
typedef enum {

  BOLT_POLICY_UNKNOWN = -1,
  BOLT_POLICY_DEFAULT = 0,
  BOLT_POLICY_MANUAL = 1,
  BOLT_POLICY_AUTO = 2,

} BoltPolicy;


BoltPolicy       bolt_policy_from_string (const char *str);
const char *     bolt_policy_to_string (BoltPolicy policy);
gboolean         bolt_policy_validate (BoltPolicy policy);

/**
 * BoltAuthCtrl:
 * @BOLT_AUTHCTRL_NONE: No authorization flags.
 *
 * Control authorization.
 */
typedef enum { /*< flags >*/

  BOLT_AUTHCTRL_NONE = 0

} BoltAuthCtrl;

/**
 * BoltDeviceType:
 * @BOLT_DEVICE_UNKNOWN_TYPE: Unknown device type
 * @BOLT_DEVICE_HOST: The device representing the host
 * @BOLT_DEVICE_PERIPHERAL: A generic thunderbolt peripheral
 *
 * The type of the device.
 */
typedef enum {

  BOLT_DEVICE_UNKNOWN_TYPE = -1,
  BOLT_DEVICE_HOST = 0,
  BOLT_DEVICE_PERIPHERAL

} BoltDeviceType;

BoltDeviceType   bolt_device_type_from_string (const char *str);
const char *     bolt_device_type_to_string (BoltDeviceType type);
gboolean         bolt_device_type_validate (BoltDeviceType type);
gboolean         bolt_device_type_is_host (BoltDeviceType type);

/**
 * BoltAuthMode:
 * @BOLT_AUTH_DISABLED: Authorization is disabled
 * @BOLT_AUTH_ENABLED: Authorization is enabled.
 *
 * Control authorization.
 */
typedef enum { /*< flags >*/

  BOLT_AUTH_DISABLED = 0,
  BOLT_AUTH_ENABLED  = 1

} BoltAuthMode;

#define bolt_auth_mode_is_enabled(auth) ((auth & BOLT_AUTH_ENABLED) != 0)
#define bolt_auth_mode_is_disabled(auth) (!bolt_auth_mode_is_enabled (auth))
