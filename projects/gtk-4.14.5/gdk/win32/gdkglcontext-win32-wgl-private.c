/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-win32-wgl-private.c: Win32 specific OpenGL wrappers
 *
 * Copyright © 2023 Chun-wei Fan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * These wrapper functions are used when we don't want to use the wgl*() core functions
 * that we acquire via libepoxy (such as when we are disposing the Gdk(W)GLContext from,
 * different threads, so for these calls, we are actually linking to the system's/ICD
 * opengl32.dll directly, so that we are guaranteed that the "right" versions of these
 * WGL calls are carried out.  This must be a separate source file because we can't include
 * the system's GL/gl.h with epoxy/(w)gl.h together in a single source file.  We should not
 * need to use these when we are creating/initializing a WGL context in GDK, since we should
 * be in the same thread at this point.
 */

#define DONT_INCLUDE_LIBEPOXY
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include "gdkglcontext-win32.h"

void
gdk_win32_private_wglDeleteContext (HGLRC hglrc)
{
  wglDeleteContext (hglrc);
}

HGLRC
gdk_win32_private_wglGetCurrentContext (void)
{
  return wglGetCurrentContext ();
}

BOOL
gdk_win32_private_wglMakeCurrent (HDC   hdc,
                                  HGLRC hglrc)
{
  return wglMakeCurrent (hdc, hglrc);
}
