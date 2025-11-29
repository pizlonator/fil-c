/* GdkMacosLayer.h
 *
 * Copyright © 2022 Red Hat, Inc.
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

#include <QuartzCore/QuartzCore.h>
#include <IOSurface/IOSurface.h>

#include <cairo.h>
#include <glib.h>

#include "gdkmacosbuffer-private.h"

#define GDK_IS_MACOS_LAYER(obj) ((obj) && [obj isKindOfClass:[GdkMacosLayer class]])

@interface GdkMacosLayer : CALayer
{
  cairo_region_t *_opaqueRegion;
  GArray         *_tiles;
  guint           _opaque : 1;
  guint           _layoutInvalid : 1;
  guint           _inSwapBuffer : 1;
  guint           _isFlipped : 1;
};

-(void)setOpaqueRegion:(const cairo_region_t *)opaqueRegion;
-(void)swapBuffer:(GdkMacosBuffer *)buffer withDamage:(const cairo_region_t *)damage;

@end
