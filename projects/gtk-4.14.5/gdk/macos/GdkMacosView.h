/* GdkMacosView.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <cairo.h>

#import "GdkMacosBaseView.h"

#include "gdkmacosbuffer-private.h"

#define GDK_IS_MACOS_VIEW(obj) ((obj) && [obj isKindOfClass:[GdkMacosView class]])

@interface GdkMacosView : GdkMacosBaseView
{
  NSRect _nextFrame;
  guint  _nextFrameDirty : 1;
}

-(void)setOpaqueRegion:(const cairo_region_t *)opaqueRegion;
-(void)swapBuffer:(GdkMacosBuffer *)buffer withDamage:(const cairo_region_t *)damage;

@end
