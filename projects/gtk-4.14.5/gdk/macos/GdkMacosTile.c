/* GdkMacosTile.c
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

#include "config.h"

#include <AppKit/AppKit.h>

#import "GdkMacosTile.h"

@implementation GdkMacosTile

-(id)init
{
  self = [super init];

  [self setContentsScale:1.0];
  [self setEdgeAntialiasingMask:0];
  [self setDrawsAsynchronously:YES];

  return self;
}

-(void)swapBuffer:(IOSurfaceRef)buffer withRect:(CGRect)rect
{
  if G_LIKELY ([self contents] == (id)buffer)
    [(id<CanSetContentsChanged>)self setContentsChanged];
  else
    [self setContents:(id)buffer];

  if G_UNLIKELY (!CGRectEqualToRect ([self contentsRect], rect))
    self.contentsRect = rect;
}

@end
