/*
 * Copyright © 2022 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <AppKit/AppKit.h>

#include "gdkmacosdisplay-private.h"
#include "gdkmacossurface-private.h"

static void
gdk_macos_display_user_defaults_changed_cb (CFNotificationCenterRef  center,
                                            void                    *observer,
                                            CFStringRef              name,
                                            const void              *object,
                                            CFDictionaryRef          userInfo)
{
  GdkMacosDisplay *self = observer;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  _gdk_macos_display_reload_settings (self);
}

static void
gdk_macos_display_monitors_changed_cb (CFNotificationCenterRef  center,
                                       void                    *observer,
                                       CFStringRef              name,
                                       const void              *object,
                                       CFDictionaryRef          userInfo)
{
  GdkMacosDisplay *self = observer;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  _gdk_macos_display_reload_monitors (self);

  /* Now we need to update all our surface positions since they
   * probably just changed origins.
   */
  for (const GList *iter = _gdk_macos_display_get_surfaces (self);
       iter != NULL;
       iter = iter->next)
    {
      GdkMacosSurface *surface = iter->data;

      g_assert (GDK_IS_MACOS_SURFACE (surface));

      _gdk_macos_surface_monitor_changed (surface);
    }
}


void
_gdk_macos_display_feedback_init (GdkMacosDisplay *self)
{
  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));

  CFNotificationCenterAddObserver (CFNotificationCenterGetLocalCenter (),
                                   self,
                                   gdk_macos_display_monitors_changed_cb,
                                   CFSTR ("NSApplicationDidChangeScreenParametersNotification"),
                                   NULL,
                                   CFNotificationSuspensionBehaviorDeliverImmediately);

  CFNotificationCenterAddObserver (CFNotificationCenterGetDistributedCenter (),
                                   self,
                                   gdk_macos_display_user_defaults_changed_cb,
                                   CFSTR ("NSUserDefaultsDidChangeNotification"),
                                   NULL,
                                   CFNotificationSuspensionBehaviorDeliverImmediately);
}

void
_gdk_macos_display_feedback_destroy (GdkMacosDisplay *self)
{
  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));

  CFNotificationCenterRemoveObserver (CFNotificationCenterGetDistributedCenter (),
                                      self,
                                      CFSTR ("NSApplicationDidChangeScreenParametersNotification"),
                                      NULL);

  CFNotificationCenterRemoveObserver (CFNotificationCenterGetDistributedCenter (),
                                      self,
                                      CFSTR ("NSUserDefaultsDidChangeNotification"),
                                      NULL);

}
