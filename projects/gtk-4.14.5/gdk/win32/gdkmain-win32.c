/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
 * Copyright (C) 2023 the GTK team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <io.h>

#include "gdk.h"
#include "gdkdebugprivate.h"
#include "gdkkeysyms.h"
#include <glib/gi18n-lib.h>
#include "gdkprivate-win32.h"
#include "gdkinput-dmanipulation.h"
#include "gdkwin32.h"

#include <objbase.h>

#include <windows.h>
#include <wintab.h>
#include <imm.h>

/* Whether GDK initialized COM */
static gboolean co_initialized = FALSE;

/* Whether GDK initialized OLE */
static gboolean ole_initialized = FALSE;

void
_gdk_win32_surfaceing_init (void)
{
  _gdk_win32_clipdrop_init ();

  gdk_dmanipulation_initialize ();
}

gboolean
gdk_win32_ensure_com (void)
{
  if (!co_initialized)
    {
      /* UI thread should only use STA model. See
       * -> https://devblogs.microsoft.com/oldnewthing/20080424-00/?p=22603
       * -> https://devblogs.microsoft.com/oldnewthing/20071018-00/?p=24743
       */
      const DWORD flags = COINIT_APARTMENTTHREADED |
                          COINIT_DISABLE_OLE1DDE;
      HRESULT hr;

      hr = CoInitializeEx (NULL, flags);
      if (SUCCEEDED (hr))
        co_initialized = TRUE;
      else switch (hr)
        {
        case RPC_E_CHANGED_MODE:
          g_warning ("COM runtime already initialized on the main "
                     "thread with an incompatible apartment model");
        break;
        default:
          HR_LOG (hr);
        break;
        }
    }

  return co_initialized;
}

gboolean
gdk_win32_ensure_ole (void)
{
  if (!ole_initialized)
    {
      HRESULT hr = OleInitialize (NULL);
      if (SUCCEEDED (hr))
        ole_initialized = TRUE;
      else switch (hr)
        {
        case RPC_E_CHANGED_MODE:
          g_warning ("Failed to initialize the OLE2 runtime because "
                     "the thread has an incompatible apartment model");
        break;
        default:
          HR_LOG (hr);
        break;
        }
    }

  return ole_initialized;
}

void
_gdk_win32_api_failed (const char *where,
                       const char *api)
{
  DWORD error_code = GetLastError ();
  char *msg = g_win32_error_message (error_code);
  g_warning ("%s: %s failed with code %lu: %s", where, api, error_code, msg);
  g_free (msg);
}

void
_gdk_other_api_failed (const char *where,
		      const char *api)
{
  g_warning ("%s: %s failed", where, api);
}


/*
 * Like g_strdup_printf, but to a static buffer. Return value does not
 * have to be g_free()d. The buffer is of bounded size and reused
 * cyclically. Thus the return value is valid only until that part of
 * the buffer happens to get reused. This doesn’t matter as this
 * function’s return value is used in debugging output right after the call,
 * and the return value isn’t used after that.
 */
static char *
static_printf (const char *format,
	       ...)
{
  static char buf[10000];
  char *msg;
  static char *bufp = buf;
  char *retval;
  va_list args;

  va_start (args, format);
  msg = g_strdup_vprintf (format, args);
  va_end (args);

  g_assert (strlen (msg) < sizeof (buf));

  if (bufp + strlen (msg) + 1 > buf + sizeof (buf))
    bufp = buf;
  retval = bufp;

  strcpy (bufp, msg);
  bufp += strlen (msg) + 1;
  g_free (msg);

  return retval;
}

char *
_gdk_win32_surface_state_to_string (GdkToplevelState state)
{
  char buf[100];
  char *bufp = buf;
  char *s = "";

  buf[0] = '\0';

#define BIT(x)						\
  if (state & GDK_TOPLEVEL_STATE_ ## x)			\
    (bufp += sprintf (bufp, "%s" #x, s), s = "|")

  BIT (MINIMIZED);
  BIT (MAXIMIZED);
  BIT (STICKY);
#undef BIT

  return static_printf ("%s", buf);
}

char *
_gdk_win32_surface_style_to_string (LONG style)
{
  char buf[1000];
  char *bufp = buf;
  char *s = "";

  buf[0] = '\0';

#define BIT(x)						\
  if (style & WS_ ## x)					\
    (bufp += sprintf (bufp, "%s" #x, s), s = "|")

  /* Note that many of the WS_* macros are in face several bits.
   * Handle just the individual bits here. Sort as in w32api's
   * winuser.h.
   */
  BIT (BORDER);
  BIT (CHILD);
  BIT (CLIPCHILDREN);
  BIT (CLIPSIBLINGS);
  BIT (DISABLED);
  BIT (DLGFRAME);
  BIT (GROUP);
  BIT (HSCROLL);
  BIT (ICONIC);
  BIT (MAXIMIZE);
  BIT (MAXIMIZEBOX);
  BIT (MINIMIZE);
  BIT (MINIMIZEBOX);
  BIT (POPUP);
  BIT (SIZEBOX);
  BIT (SYSMENU);
  BIT (TABSTOP);
  BIT (THICKFRAME);
  BIT (VISIBLE);
  BIT (VSCROLL);
#undef BIT

  return static_printf ("%s", buf);
}

char *
_gdk_win32_surface_exstyle_to_string (LONG style)
{
  char buf[1000];
  char *bufp = buf;
  char *s = "";

  buf[0] = '\0';

#define BIT(x)						\
  if (style & WS_EX_ ## x)				\
    (bufp += sprintf (bufp, "%s" #x, s), s = "|")

  /* Note that many of the WS_EX_* macros are in face several bits.
   * Handle just the individual bits here. Sort as in w32api's
   * winuser.h.
   */
  BIT (ACCEPTFILES);
  BIT (APPWINDOW);
  BIT (CLIENTEDGE);
#ifndef WS_EX_COMPOSITED
#  define WS_EX_COMPOSITED 0x02000000L
#endif
  BIT (COMPOSITED);
  BIT (CONTEXTHELP);
  BIT (CONTROLPARENT);
  BIT (DLGMODALFRAME);
  BIT (LAYOUTRTL);
  BIT (LEFTSCROLLBAR);
  BIT (MDICHILD);
  BIT (NOACTIVATE);
  BIT (NOINHERITLAYOUT);
  BIT (NOPARENTNOTIFY);
  BIT (RIGHT);
  BIT (RTLREADING);
  BIT (STATICEDGE);
  BIT (TOOLWINDOW);
  BIT (TOPMOST);
  BIT (TRANSPARENT);
  BIT (WINDOWEDGE);
#undef BIT

  return static_printf ("%s", buf);
}

char *
_gdk_win32_surface_pos_bits_to_string (UINT flags)
{
  char buf[1000];
  char *bufp = buf;
  char *s = "";

  buf[0] = '\0';

#define BIT(x)						\
  if (flags & SWP_ ## x)				\
    (bufp += sprintf (bufp, "%s" #x, s), s = "|")

  BIT (DRAWFRAME);
  BIT (FRAMECHANGED);
  BIT (HIDEWINDOW);
  BIT (NOACTIVATE);
  BIT (NOCOPYBITS);
  BIT (NOMOVE);
  BIT (NOSIZE);
  BIT (NOREDRAW);
  BIT (NOZORDER);
  BIT (SHOWWINDOW);
  BIT (NOOWNERZORDER);
  BIT (NOSENDCHANGING);
  BIT (DEFERERASE);
  BIT (ASYNCWINDOWPOS);
#undef BIT

  return static_printf ("%s", buf);
}

char *
_gdk_win32_drag_action_to_string (GdkDragAction actions)
{
  char buf[100];
  char *bufp = buf;
  char *s = "";

  buf[0] = '\0';

#define BIT(x)						\
  if (actions & GDK_ACTION_ ## x)				\
    (bufp += sprintf (bufp, "%s" #x, s), s = "|")

  BIT (COPY);
  BIT (MOVE);
  BIT (LINK);
  BIT (ASK);
#undef BIT

  return static_printf ("%s", buf);
}

static char *
_gdk_win32_rop2_to_string (int rop2)
{
  switch (rop2)
    {
#define CASE(x) case R2_##x: return #x
      CASE (BLACK);
      CASE (COPYPEN);
      CASE (MASKNOTPEN);
      CASE (MASKPEN);
      CASE (MASKPENNOT);
      CASE (MERGENOTPEN);
      CASE (MERGEPEN);
      CASE (MERGEPENNOT);
      CASE (NOP);
      CASE (NOT);
      CASE (NOTCOPYPEN);
      CASE (NOTMASKPEN);
      CASE (NOTMERGEPEN);
      CASE (NOTXORPEN);
      CASE (WHITE);
      CASE (XORPEN);
#undef CASE
    default: return static_printf ("illegal_%x", rop2);
    }
  /* NOTREACHED */
  return NULL;
}

static char *
_gdk_win32_lbstyle_to_string (UINT brush_style)
{
  switch (brush_style)
    {
#define CASE(x) case BS_##x: return #x
      CASE (DIBPATTERN);
      CASE (DIBPATTERNPT);
      CASE (HATCHED);
      CASE (HOLLOW);
      CASE (PATTERN);
      CASE (SOLID);
#undef CASE
    default: return static_printf ("illegal_%d", brush_style);
    }
  /* NOTREACHED */
  return NULL;
}

static char *
_gdk_win32_pstype_to_string (DWORD pen_style)
{
  switch (pen_style & PS_TYPE_MASK)
    {
    case PS_GEOMETRIC: return "GEOMETRIC";
    case PS_COSMETIC: return "COSMETIC";
    default: return static_printf ("illegal_%d", pen_style & PS_TYPE_MASK);
    }
  /* NOTREACHED */
  return NULL;
}

static char *
_gdk_win32_psstyle_to_string (DWORD pen_style)
{
  switch (pen_style & PS_STYLE_MASK)
    {
#define CASE(x) case PS_##x: return #x
      CASE (ALTERNATE);
      CASE (SOLID);
      CASE (DASH);
      CASE (DOT);
      CASE (DASHDOT);
      CASE (DASHDOTDOT);
      CASE (NULL);
      CASE (USERSTYLE);
      CASE (INSIDEFRAME);
#undef CASE
    default: return static_printf ("illegal_%d", pen_style & PS_STYLE_MASK);
    }
  /* NOTREACHED */
  return NULL;
}

static char *
_gdk_win32_psendcap_to_string (DWORD pen_style)
{
  switch (pen_style & PS_ENDCAP_MASK)
    {
#define CASE(x) case PS_ENDCAP_##x: return #x
      CASE (ROUND);
      CASE (SQUARE);
      CASE (FLAT);
#undef CASE
    default: return static_printf ("illegal_%d", pen_style & PS_ENDCAP_MASK);
    }
  /* NOTREACHED */
  return NULL;
}

static char *
_gdk_win32_psjoin_to_string (DWORD pen_style)
{
  switch (pen_style & PS_JOIN_MASK)
    {
#define CASE(x) case PS_JOIN_##x: return #x
      CASE (ROUND);
      CASE (BEVEL);
      CASE (MITER);
#undef CASE
    default: return static_printf ("illegal_%d", pen_style & PS_JOIN_MASK);
    }
  /* NOTREACHED */
  return NULL;
}

void
_gdk_win32_print_dc (HDC hdc)
{
  HGDIOBJ obj;
  LOGBRUSH logbrush;
  EXTLOGPEN extlogpen;
  HRGN hrgn;
  RECT rect;
  int flag;

  g_print ("%p:\n", hdc);

  obj = GetCurrentObject (hdc, OBJ_BRUSH);
  GetObject (obj, sizeof (LOGBRUSH), &logbrush);

  g_print ("brush: %s color=%06lx hatch=%p\n",
           _gdk_win32_lbstyle_to_string (logbrush.lbStyle),
           logbrush.lbColor, (gpointer) logbrush.lbHatch);

  obj = GetCurrentObject (hdc, OBJ_PEN);
  GetObject (obj, sizeof (EXTLOGPEN), &extlogpen);

  g_print ("pen: %s %s %s %s w=%d %s\n",
           _gdk_win32_pstype_to_string (extlogpen.elpPenStyle),
           _gdk_win32_psstyle_to_string (extlogpen.elpPenStyle),
           _gdk_win32_psendcap_to_string (extlogpen.elpPenStyle),
           _gdk_win32_psjoin_to_string (extlogpen.elpPenStyle),
           (int) extlogpen.elpWidth,
           _gdk_win32_lbstyle_to_string (extlogpen.elpBrushStyle));

  g_print ("rop2: %s textcolor=%06lx\n",
           _gdk_win32_rop2_to_string (GetROP2 (hdc)),
           GetTextColor (hdc));

  hrgn = CreateRectRgn (0, 0, 0, 0);

  if ((flag = GetClipRgn (hdc, hrgn)) == -1)
    WIN32_API_FAILED ("GetClipRgn");
  else if (flag == 0)
    g_print ("no clip region\n");
  else if (flag == 1)
    {
      GetRgnBox (hrgn, &rect);
      g_print ("clip region: %p bbox: %s\n",
               hrgn, _gdk_win32_rect_to_string (&rect));
    }

  DeleteObject (hrgn);
}

char *
_gdk_win32_message_to_string (UINT msg)
{
  switch (msg)
    {
#define CASE(x) case x: return #x
      CASE (WM_NULL);
      CASE (WM_CREATE);
      CASE (WM_DESTROY);
      CASE (WM_MOVE);
      CASE (WM_SIZE);
      CASE (WM_ACTIVATE);
      CASE (WM_SETFOCUS);
      CASE (WM_KILLFOCUS);
      CASE (WM_ENABLE);
      CASE (WM_SETREDRAW);
      CASE (WM_SETTEXT);
      CASE (WM_GETTEXT);
      CASE (WM_GETTEXTLENGTH);
      CASE (WM_PAINT);
      CASE (WM_CLOSE);
      CASE (WM_QUERYENDSESSION);
      CASE (WM_QUERYOPEN);
      CASE (WM_ENDSESSION);
      CASE (WM_QUIT);
      CASE (WM_ERASEBKGND);
      CASE (WM_SYSCOLORCHANGE);
      CASE (WM_SHOWWINDOW);
      CASE (WM_WININICHANGE);
      CASE (WM_DEVMODECHANGE);
      CASE (WM_ACTIVATEAPP);
      CASE (WM_FONTCHANGE);
      CASE (WM_TIMECHANGE);
      CASE (WM_CANCELMODE);
      CASE (WM_SETCURSOR);
      CASE (WM_MOUSEACTIVATE);
      CASE (WM_CHILDACTIVATE);
      CASE (WM_QUEUESYNC);
      CASE (WM_GETMINMAXINFO);
      CASE (WM_PAINTICON);
      CASE (WM_ICONERASEBKGND);
      CASE (WM_NEXTDLGCTL);
      CASE (WM_SPOOLERSTATUS);
      CASE (WM_DRAWITEM);
      CASE (WM_MEASUREITEM);
      CASE (WM_DELETEITEM);
      CASE (WM_VKEYTOITEM);
      CASE (WM_CHARTOITEM);
      CASE (WM_SETFONT);
      CASE (WM_GETFONT);
      CASE (WM_SETHOTKEY);
      CASE (WM_GETHOTKEY);
      CASE (WM_QUERYDRAGICON);
      CASE (WM_COMPAREITEM);
      CASE (WM_GETOBJECT);
      CASE (WM_COMPACTING);
      CASE (WM_WINDOWPOSCHANGING);
      CASE (WM_WINDOWPOSCHANGED);
      CASE (WM_POWER);
      CASE (WM_COPYDATA);
      CASE (WM_CANCELJOURNAL);
      CASE (WM_NOTIFY);
      CASE (WM_INPUTLANGCHANGEREQUEST);
      CASE (WM_INPUTLANGCHANGE);
      CASE (WM_TCARD);
      CASE (WM_HELP);
      CASE (WM_USERCHANGED);
      CASE (WM_NOTIFYFORMAT);
      CASE (WM_CONTEXTMENU);
      CASE (WM_STYLECHANGING);
      CASE (WM_STYLECHANGED);
      CASE (WM_DISPLAYCHANGE);
      CASE (WM_GETICON);
      CASE (WM_SETICON);
      CASE (WM_NCCREATE);
      CASE (WM_NCDESTROY);
      CASE (WM_NCCALCSIZE);
      CASE (WM_NCHITTEST);
      CASE (WM_NCPAINT);
      CASE (WM_NCACTIVATE);
      CASE (WM_GETDLGCODE);
      CASE (WM_SYNCPAINT);
      CASE (WM_NCMOUSEMOVE);
      CASE (WM_NCLBUTTONDOWN);
      CASE (WM_NCLBUTTONUP);
      CASE (WM_NCLBUTTONDBLCLK);
      CASE (WM_NCRBUTTONDOWN);
      CASE (WM_NCRBUTTONUP);
      CASE (WM_NCRBUTTONDBLCLK);
      CASE (WM_NCMBUTTONDOWN);
      CASE (WM_NCMBUTTONUP);
      CASE (WM_NCMBUTTONDBLCLK);
      CASE (WM_NCXBUTTONDOWN);
      CASE (WM_NCXBUTTONUP);
      CASE (WM_NCXBUTTONDBLCLK);
      CASE (WM_KEYDOWN);
      CASE (WM_KEYUP);
      CASE (WM_CHAR);
      CASE (WM_DEADCHAR);
      CASE (WM_SYSKEYDOWN);
      CASE (WM_SYSKEYUP);
      CASE (WM_SYSCHAR);
      CASE (WM_SYSDEADCHAR);
      CASE (WM_KEYLAST);
      CASE (WM_IME_STARTCOMPOSITION);
      CASE (WM_IME_ENDCOMPOSITION);
      CASE (WM_IME_COMPOSITION);
      CASE (WM_INITDIALOG);
      CASE (WM_COMMAND);
      CASE (WM_SYSCOMMAND);
      CASE (WM_TIMER);
      CASE (WM_HSCROLL);
      CASE (WM_VSCROLL);
      CASE (WM_INITMENU);
      CASE (WM_INITMENUPOPUP);
      CASE (WM_MENUSELECT);
      CASE (WM_MENUCHAR);
      CASE (WM_ENTERIDLE);
      CASE (WM_MENURBUTTONUP);
      CASE (WM_MENUDRAG);
      CASE (WM_MENUGETOBJECT);
      CASE (WM_UNINITMENUPOPUP);
      CASE (WM_MENUCOMMAND);
      CASE (WM_CHANGEUISTATE);
      CASE (WM_UPDATEUISTATE);
      CASE (WM_QUERYUISTATE);
      CASE (WM_CTLCOLORMSGBOX);
      CASE (WM_CTLCOLOREDIT);
      CASE (WM_CTLCOLORLISTBOX);
      CASE (WM_CTLCOLORBTN);
      CASE (WM_CTLCOLORDLG);
      CASE (WM_CTLCOLORSCROLLBAR);
      CASE (WM_CTLCOLORSTATIC);
      CASE (WM_MOUSEMOVE);
      CASE (WM_LBUTTONDOWN);
      CASE (WM_LBUTTONUP);
      CASE (WM_LBUTTONDBLCLK);
      CASE (WM_RBUTTONDOWN);
      CASE (WM_RBUTTONUP);
      CASE (WM_RBUTTONDBLCLK);
      CASE (WM_MBUTTONDOWN);
      CASE (WM_MBUTTONUP);
      CASE (WM_MBUTTONDBLCLK);
      CASE (WM_MOUSEWHEEL);
      CASE (WM_MOUSEHWHEEL);
      CASE (WM_XBUTTONDOWN);
      CASE (WM_XBUTTONUP);
      CASE (WM_XBUTTONDBLCLK);
      CASE (WM_PARENTNOTIFY);
      CASE (WM_ENTERMENULOOP);
      CASE (WM_EXITMENULOOP);
      CASE (WM_NEXTMENU);
      CASE (WM_SIZING);
      CASE (WM_CAPTURECHANGED);
      CASE (WM_MOVING);
      CASE (WM_POWERBROADCAST);
      CASE (WM_DEVICECHANGE);
      CASE (WM_MDICREATE);
      CASE (WM_MDIDESTROY);
      CASE (WM_MDIACTIVATE);
      CASE (WM_MDIRESTORE);
      CASE (WM_MDINEXT);
      CASE (WM_MDIMAXIMIZE);
      CASE (WM_MDITILE);
      CASE (WM_MDICASCADE);
      CASE (WM_MDIICONARRANGE);
      CASE (WM_MDIGETACTIVE);
      CASE (WM_MDISETMENU);
      CASE (WM_ENTERSIZEMOVE);
      CASE (WM_EXITSIZEMOVE);
      CASE (WM_DROPFILES);
      CASE (WM_MDIREFRESHMENU);
      CASE (WM_IME_SETCONTEXT);
      CASE (WM_IME_NOTIFY);
      CASE (WM_IME_CONTROL);
      CASE (WM_IME_COMPOSITIONFULL);
      CASE (WM_IME_SELECT);
      CASE (WM_IME_CHAR);
      CASE (WM_IME_REQUEST);
      CASE (WM_IME_KEYDOWN);
      CASE (WM_IME_KEYUP);
      CASE (WM_MOUSEHOVER);
      CASE (WM_MOUSELEAVE);
      CASE (WM_NCMOUSEHOVER);
      CASE (WM_NCMOUSELEAVE);
      CASE (WM_CUT);
      CASE (WM_COPY);
      CASE (WM_PASTE);
      CASE (WM_CLEAR);
      CASE (WM_UNDO);
      CASE (WM_RENDERFORMAT);
      CASE (WM_RENDERALLFORMATS);
      CASE (WM_DESTROYCLIPBOARD);
      CASE (WM_DRAWCLIPBOARD);
      CASE (WM_PAINTCLIPBOARD);
      CASE (WM_VSCROLLCLIPBOARD);
      CASE (WM_SIZECLIPBOARD);
      CASE (WM_ASKCBFORMATNAME);
      CASE (WM_CHANGECBCHAIN);
      CASE (WM_HSCROLLCLIPBOARD);
      CASE (WM_QUERYNEWPALETTE);
      CASE (WM_PALETTEISCHANGING);
      CASE (WM_PALETTECHANGED);
      CASE (WM_HOTKEY);
      CASE (WM_PRINT);
      CASE (WM_PRINTCLIENT);
      CASE (WM_APPCOMMAND);
      CASE (WM_HANDHELDFIRST);
      CASE (WM_HANDHELDLAST);
      CASE (WM_AFXFIRST);
      CASE (WM_AFXLAST);
      CASE (WM_PENWINFIRST);
      CASE (WM_PENWINLAST);
      CASE (WM_APP);
      CASE (WT_PACKET);
      CASE (WT_CSRCHANGE);
      CASE (WT_PROXIMITY);
      CASE (WM_DPICHANGED);
#undef CASE
    default:
      if (msg >= WM_HANDHELDFIRST && msg <= WM_HANDHELDLAST)
	return static_printf ("WM_HANDHELDFIRST+%d", msg - WM_HANDHELDFIRST);
      else if (msg >= WM_AFXFIRST && msg <= WM_AFXLAST)
	return static_printf ("WM_AFXFIRST+%d", msg - WM_AFXFIRST);
      else if (msg >= WM_PENWINFIRST && msg <= WM_PENWINLAST)
	return static_printf ("WM_PENWINFIRST+%d", msg - WM_PENWINFIRST);
      else if (msg >= WM_USER && msg <= 0x7FFF)
	return static_printf ("WM_USER+%d", msg - WM_USER);
      else if (msg >= 0xC000 && msg <= 0xFFFF)
	return static_printf ("reg-%#x", msg);
      else
	return static_printf ("unk-%#x", msg);
    }
  /* NOTREACHED */
  return NULL;
}

char *
_gdk_win32_key_to_string (LONG lParam)
{
  char buf[100];
  char *keyname_utf8;

  if (GetKeyNameText (lParam, buf, sizeof (buf)) &&
      (keyname_utf8 = g_locale_to_utf8 (buf, -1, NULL, NULL, NULL)) != NULL)
    {
      char *retval = static_printf ("%s", keyname_utf8);

      g_free (keyname_utf8);

      return retval;
    }

  return static_printf ("unk-%#lx", lParam);
}

char *
_gdk_win32_cf_to_string (UINT format)
{
  char buf[100];

  switch (format)
    {
#define CASE(x) case CF_##x: return "CF_" #x
      CASE (BITMAP);
      CASE (DIB);
      CASE (DIBV5);
      CASE (DIF);
      CASE (DSPBITMAP);
      CASE (DSPENHMETAFILE);
      CASE (DSPMETAFILEPICT);
      CASE (DSPTEXT);
      CASE (ENHMETAFILE);
      CASE (HDROP);
      CASE (LOCALE);
      CASE (METAFILEPICT);
      CASE (OEMTEXT);
      CASE (OWNERDISPLAY);
      CASE (PALETTE);
      CASE (PENDATA);
      CASE (RIFF);
      CASE (SYLK);
      CASE (TEXT);
      CASE (WAVE);
      CASE (TIFF);
      CASE (UNICODETEXT);
    default:
      if (format >= CF_GDIOBJFIRST &&
	  format <= CF_GDIOBJLAST)
	return static_printf ("CF_GDIOBJ%d", format - CF_GDIOBJFIRST);
      if (format >= CF_PRIVATEFIRST &&
	  format <= CF_PRIVATELAST)
	return static_printf ("CF_PRIVATE%d", format - CF_PRIVATEFIRST);
      if (GetClipboardFormatName (format, buf, sizeof (buf)))
	return static_printf ("'%s'", buf);
      else
	return static_printf ("unk-%#lx", format);
    }
}

char *
_gdk_win32_rect_to_string (const RECT *rect)
{
  return static_printf ("%ldx%ld@%+ld%+ld",
			(rect->right - rect->left), (rect->bottom - rect->top),
			rect->left, rect->top);
}
