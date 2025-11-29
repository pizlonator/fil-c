/* GdkMacosWindow.h
 *
 * Copyright © 2020 Red Hat, Inc.
 * Copyright © 2005-2007 Imendio AB
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

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <IOSurface/IOSurface.h>

#include <gdk/gdk.h>

#include "gdkmacosbuffer-private.h"
#include "gdkmacosdisplay.h"
#include "gdkmacossurface.h"
#include "edgesnapping.h"

#define GDK_IS_MACOS_WINDOW(obj) ([obj isKindOfClass:[GdkMacosWindow class]])

@interface GdkMacosWindow : NSWindow <NSDraggingSource, NSDraggingDestination> {
  GdkMacosSurface *gdk_surface;

  BOOL             inMove;
  BOOL             inShowOrHide;
  BOOL             initialPositionKnown;

  /* Manually triggered move/resize (not by the window manager) */
  BOOL             inManualMove;
  BOOL             inManualResize;
  BOOL             inTrackManualResize;
  NSPoint          initialResizeLocation;
  NSRect           initialResizeFrame;
  GdkSurfaceEdge   resizeEdge;

  EdgeSnapping     snapping;

  BOOL             inFullscreenTransition;
}

-(void)beginManualMove;
-(void)beginManualResize:(GdkSurfaceEdge)edge;
-(void)hide;
-(BOOL)isInManualResizeOrMove;
-(BOOL)isInMove;
-(GdkMacosDisplay *)gdkDisplay;
-(GdkMacosSurface *)gdkSurface;
-(void)setGdkSurface:(GdkMacosSurface *)surface;
-(void)setStyleMask:(NSWindowStyleMask)styleMask;
-(void)showAndMakeKey:(BOOL)makeKey;
-(BOOL)trackManualMove;
-(BOOL)trackManualResize;
-(void)setDecorated:(BOOL)decorated;
-(void)swapBuffer:(GdkMacosBuffer *)buffer withDamage:(const cairo_region_t *)damage;
-(BOOL)needsMouseDownQuirk;
-(BOOL)inFullscreenTransition;

@end
