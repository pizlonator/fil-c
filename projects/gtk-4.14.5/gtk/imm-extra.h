/*
 * Copyright (C) 2003 Takuro Ashie
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
 * $Id$ 
 */

#include <windows.h>
#include <imm.h>


/* these aren't defined in Cygwin's imm.h */

#ifndef WM_IME_REQUEST
#   define WM_IME_REQUEST                  0x0288
#endif  /* WM_IME_REQUEST */

#ifndef IMR_COMPOSITIONWINDOW
#   define IMR_COMPOSITIONWINDOW           0x0001
#endif /* IMR_COMPOSITIONWINDOW */

#ifndef IMR_CANDIDATEWINDOW
#   define IMR_CANDIDATEWINDOW             0x0002
#endif /* IMR_CANDIDATEWINDOW */

#ifndef IMR_COMPOSITIONFONT
#   define IMR_COMPOSITIONFONT             0x0003
#endif /* IMR_COMPOSITIONFONT */

#ifndef IMR_RECONVERTSTRING
#   define IMR_RECONVERTSTRING             0x0004
#endif /* IMR_RECONVERTSTRING */

#ifndef IMR_CONFIRMRECONVERTSTRING
#   define IMR_CONFIRMRECONVERTSTRING      0x0005
#endif /* IMR_CONFIRMRECONVERTSTRING */

#ifndef IMR_QUERYCHARPOSITION
#   define IMR_QUERYCHARPOSITION           0x0006
typedef struct tagIMECHARPOSITION {
  DWORD  dwSize;
  DWORD  dwCharPos;
  POINT  pt;
  UINT   cLineHeight;
  RECT   rcDocument;
} IMECHARPOSITION, *PIMECHARPOSITION;
#endif /* IMR_QUERYCHARPOSITION */

#ifndef IMR_DOCUMENTFEED
#   define IMR_DOCUMENTFEED                0x0007
#endif /* IMR_DOCUMENTFEED */
