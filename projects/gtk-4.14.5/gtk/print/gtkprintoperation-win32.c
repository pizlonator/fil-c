/* GTK - The GIMP Toolkit
 * gtkprintoperation-win32.c: Print Operation Details for Win32
 * Copyright (C) 2006, Red Hat, Inc.
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
 */

#ifndef _MSC_VER
#ifndef _WIN32_WINNT
/* Vista or newer */
#define _WIN32_WINNT 0x0600
#endif
#ifndef WINVER
#define WINVER _WIN32_WINNT
#endif
#endif

#define COBJMACROS
#include "config.h"
#include <math.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <cairo-win32.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include "gtkwidgetprivate.h"

#include "gtkprintoperation-private.h"
#include "gtkprint-win32.h"
#include "gtkprivate.h"
#include "gdkprivate.h"
#include "win32/gdkprivate-win32.h"

#define MAX_PAGE_RANGES 20
#define STATUS_POLLING_TIME 2000

#ifndef JOB_STATUS_RESTART
#define JOB_STATUS_RESTART 0x800
#endif

#ifndef JOB_STATUS_COMPLETE
#define JOB_STATUS_COMPLETE 0x1000
#endif

/* Forward declarations */
GtkPrintOperationResult
gtk_print_operation_run_without_dialog (GtkPrintOperation *op,
					gboolean          *do_print);
GtkPrintOperationResult
gtk_print_operation_run_with_dialog (GtkPrintOperation *op,
				     GtkWindow         *parent,
				     gboolean          *do_print);
UINT_PTR CALLBACK
run_mainloop_hook (HWND hdlg,
		   UINT uiMsg,
		   WPARAM wParam,
		   LPARAM lParam);
void
win32_start_page (GtkPrintOperation *op,
		  GtkPrintContext *print_context,
		  GtkPageSetup *page_setup);

typedef struct {
  HDC hdc;
  HGLOBAL devmode;
  HGLOBAL devnames;
  HANDLE printerHandle;
  int job_id;
  guint timeout_id;

  cairo_surface_t *surface;
  GtkWidget *embed_widget;
} GtkPrintOperationWin32;

static void win32_poll_status (GtkPrintOperation *op);

static const GUID myIID_IPrintDialogCallback  = {0x5852a2c3,0x6530,0x11d1,{0xb6,0xa3,0x0,0x0,0xf8,0x75,0x7b,0xf9}};

#if !defined (_MSC_VER) && !defined (HAVE_IPRINTDIALOGCALLBACK)
#undef INTERFACE
#define INTERFACE IPrintDialogCallback
DECLARE_INTERFACE_ (IPrintDialogCallback, IUnknown)
{
    STDMETHOD (QueryInterface)(THIS_ REFIID,LPVOID*) PURE;
    STDMETHOD_ (ULONG, AddRef)(THIS) PURE;
    STDMETHOD_ (ULONG, Release)(THIS) PURE;
    STDMETHOD (InitDone)(THIS) PURE;
    STDMETHOD (SelectionChange)(THIS) PURE;
    STDMETHOD (HandleMessage)(THIS_ HWND,UINT,WPARAM,LPARAM,LRESULT*) PURE;
}; 
#endif

static UINT got_gdk_events_message;

UINT_PTR CALLBACK
run_mainloop_hook (HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
  if (uiMsg == WM_INITDIALOG)
    {
      gdk_win32_set_modal_dialog_libgtk_only (hdlg);
      while (g_main_context_pending (NULL))
	g_main_context_iteration (NULL, TRUE);
    }
  else if (uiMsg == got_gdk_events_message)
    {
      while (g_main_context_pending (NULL))
	g_main_context_iteration (NULL, TRUE);
      return 1;
    }
  return 0;
}

static GtkPageOrientation
orientation_from_win32 (short orientation)
{
  if (orientation == DMORIENT_LANDSCAPE)
    return GTK_PAGE_ORIENTATION_LANDSCAPE;
  return GTK_PAGE_ORIENTATION_PORTRAIT;
}

static short
orientation_to_win32 (GtkPageOrientation orientation)
{
  if (orientation == GTK_PAGE_ORIENTATION_LANDSCAPE ||
      orientation == GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE)
    return DMORIENT_LANDSCAPE;
  return DMORIENT_PORTRAIT;
}

static GtkPaperSize *
paper_size_from_win32 (short size)
{
  const char *name;
  
  switch (size)
    {
    case DMPAPER_LETTER_TRANSVERSE:
    case DMPAPER_LETTER:
    case DMPAPER_LETTERSMALL:
      name = "na_letter";
      break;
    case DMPAPER_TABLOID:
    case DMPAPER_LEDGER:
      name = "na_ledger";
      break;
    case DMPAPER_LEGAL:
      name = "na_legal";
      break;
    case DMPAPER_STATEMENT:
      name = "na_invoice";
      break;
    case DMPAPER_EXECUTIVE:
      name = "na_executive";
      break;
    case DMPAPER_A3:
    case DMPAPER_A3_TRANSVERSE:
      name = "iso_a3";
      break;
    case DMPAPER_A4:
    case DMPAPER_A4SMALL:
    case DMPAPER_A4_TRANSVERSE:
      name = "iso_a4";
      break;
    case DMPAPER_A5:
    case DMPAPER_A5_TRANSVERSE:
      name = "iso_a5";
      break;
    case DMPAPER_B4:
      name = "jis_b4";
      break;
    case DMPAPER_B5:
    case DMPAPER_B5_TRANSVERSE:
      name = "jis_b5";
      break;
    case DMPAPER_QUARTO:
      name = "na_quarto";
      break;
    case DMPAPER_10X14:
      name = "na_10x14";
      break;
    case DMPAPER_11X17:
      name = "na_ledger";
      break;
    case DMPAPER_NOTE:
      name = "na_letter";
      break;
    case DMPAPER_ENV_9:
      name = "na_number-9";
      break;
    case DMPAPER_ENV_10:
      name = "na_number-10";
      break;
    case DMPAPER_ENV_11:
      name = "na_number-11";
      break;
    case DMPAPER_ENV_12:
      name = "na_number-12";
      break;
    case DMPAPER_ENV_14:
      name = "na_number-14";
      break;
    case DMPAPER_CSHEET:
      name = "na_c";
      break;
    case DMPAPER_DSHEET:
      name = "na_d";
      break;
    case DMPAPER_ESHEET:
      name = "na_e";
      break;
    case DMPAPER_ENV_DL:
      name = "iso_dl";
      break;
    case DMPAPER_ENV_C5:
      name = "iso_c5";
      break;
    case DMPAPER_ENV_C3:
      name = "iso_c3";
      break;
    case DMPAPER_ENV_C4:
      name = "iso_c4";
      break;
    case DMPAPER_ENV_C6:
      name = "iso_c6";
      break;
    case DMPAPER_ENV_C65:
      name = "iso_c6c5";
      break;
    case DMPAPER_ENV_B4:
      name = "iso_b4";
      break;
    case DMPAPER_ENV_B5:
      name = "iso_b5";
      break;
    case DMPAPER_ENV_B6:
      name = "iso_b6";
      break;
    case DMPAPER_ENV_ITALY:
      name = "om_italian";
      break;
    case DMPAPER_ENV_MONARCH:
      name = "na_monarch";
      break;
    case DMPAPER_ENV_PERSONAL:
      name = "na_personal";
      break;
    case DMPAPER_FANFOLD_US:
      name = "na_fanfold-us";
      break;
    case DMPAPER_FANFOLD_STD_GERMAN:
      name = "na_fanfold-eur";
      break;
    case DMPAPER_FANFOLD_LGL_GERMAN:
      name = "na_foolscap";
      break;
    case DMPAPER_ISO_B4:
      name = "iso_b4";
      break;
    case DMPAPER_JAPANESE_POSTCARD:
      name = "jpn_hagaki";
      break;
    case DMPAPER_9X11:
      name = "na_9x11";
      break;
    case DMPAPER_10X11:
      name = "na_10x11";
      break;
    case DMPAPER_ENV_INVITE:
      name = "om_invite";
       break;
    case DMPAPER_LETTER_EXTRA:
    case DMPAPER_LETTER_EXTRA_TRANSVERSE:
      name = "na_letter-extra";
      break;
    case DMPAPER_LEGAL_EXTRA:
      name = "na_legal-extra";
      break;
    case DMPAPER_TABLOID_EXTRA:
      name = "na_arch";
      break;
    case DMPAPER_A4_EXTRA:
      name = "iso_a4-extra";
      break;
    case DMPAPER_B_PLUS:
      name = "na_b-plus";
      break;
    case DMPAPER_LETTER_PLUS:
      name = "na_letter-plus";
      break;
    case DMPAPER_A3_EXTRA:
    case DMPAPER_A3_EXTRA_TRANSVERSE:
      name = "iso_a3-extra";
      break;
    case DMPAPER_A5_EXTRA:
      name = "iso_a5-extra";
      break;
    case DMPAPER_B5_EXTRA:
      name = "iso_b5-extra";
      break;
    case DMPAPER_A2:
      name = "iso_a2";
      break;
      
    default:
      name = NULL;
      break;
    }

  if (name)
    return gtk_paper_size_new (name);
  else 
    return NULL;
}

static short
paper_size_to_win32 (GtkPaperSize *paper_size)
{
  const char *format;

  if (gtk_paper_size_is_custom (paper_size))
    return 0;
  
  format = gtk_paper_size_get_name (paper_size);

  if (strcmp (format, "na_letter") == 0)
    return DMPAPER_LETTER;
  if (strcmp (format, "na_ledger") == 0)
    return DMPAPER_LEDGER;
  if (strcmp (format, "na_legal") == 0)
    return DMPAPER_LEGAL;
  if (strcmp (format, "na_invoice") == 0)
    return DMPAPER_STATEMENT;
  if (strcmp (format, "na_executive") == 0)
    return DMPAPER_EXECUTIVE;
  if (strcmp (format, "iso_a2") == 0)
    return DMPAPER_A2;
  if (strcmp (format, "iso_a3") == 0)
    return DMPAPER_A3;
  if (strcmp (format, "iso_a4") == 0)
    return DMPAPER_A4;
  if (strcmp (format, "iso_a5") == 0)
    return DMPAPER_A5;
  if (strcmp (format, "iso_b4") == 0)
    return DMPAPER_B4;
  if (strcmp (format, "iso_b5") == 0)
    return DMPAPER_B5;
  if (strcmp (format, "na_quarto") == 0)
    return DMPAPER_QUARTO;
  if (strcmp (format, "na_10x14") == 0)
    return DMPAPER_10X14;
  if (strcmp (format, "na_number-9") == 0)
    return DMPAPER_ENV_9;
  if (strcmp (format, "na_number-10") == 0)
    return DMPAPER_ENV_10;
  if (strcmp (format, "na_number-11") == 0)
    return DMPAPER_ENV_11;
  if (strcmp (format, "na_number-12") == 0)
    return DMPAPER_ENV_12;
  if (strcmp (format, "na_number-14") == 0)
    return DMPAPER_ENV_14;
  if (strcmp (format, "na_c") == 0)
    return DMPAPER_CSHEET;
  if (strcmp (format, "na_d") == 0)
    return DMPAPER_DSHEET;
  if (strcmp (format, "na_e") == 0)
    return DMPAPER_ESHEET;
  if (strcmp (format, "iso_dl") == 0)
    return DMPAPER_ENV_DL;
  if (strcmp (format, "iso_c3") == 0)
    return DMPAPER_ENV_C3;
  if (strcmp (format, "iso_c4") == 0)
    return DMPAPER_ENV_C4;
  if (strcmp (format, "iso_c5") == 0)
    return DMPAPER_ENV_C5;
  if (strcmp (format, "iso_c6") == 0)
    return DMPAPER_ENV_C6;
  if (strcmp (format, "iso_c5c6") == 0)
    return DMPAPER_ENV_C65;
  if (strcmp (format, "iso_b6") == 0)
    return DMPAPER_ENV_B6;
  if (strcmp (format, "om_italian") == 0)
    return DMPAPER_ENV_ITALY;
  if (strcmp (format, "na_monarch") == 0)
    return DMPAPER_ENV_MONARCH;
  if (strcmp (format, "na_personal") == 0)
    return DMPAPER_ENV_PERSONAL;
  if (strcmp (format, "na_fanfold-us") == 0)
    return DMPAPER_FANFOLD_US;
  if (strcmp (format, "na_fanfold-eur") == 0)
    return DMPAPER_FANFOLD_STD_GERMAN;
  if (strcmp (format, "na_foolscap") == 0)
    return DMPAPER_FANFOLD_LGL_GERMAN;
  if (strcmp (format, "jpn_hagaki") == 0)
    return DMPAPER_JAPANESE_POSTCARD;
  if (strcmp (format, "na_9x11") == 0)
    return DMPAPER_9X11;
  if (strcmp (format, "na_10x11") == 0)
    return DMPAPER_10X11;
  if (strcmp (format, "om_invite") == 0)
    return DMPAPER_ENV_INVITE;
  if (strcmp (format, "na_letter-extra") == 0)
    return DMPAPER_LETTER_EXTRA;
  if (strcmp (format, "na_legal-extra") == 0)
    return DMPAPER_LEGAL_EXTRA;
  if (strcmp (format, "na_arch") == 0)
    return DMPAPER_TABLOID_EXTRA;
  if (strcmp (format, "iso_a3-extra") == 0)
    return DMPAPER_A3_EXTRA;
  if (strcmp (format, "iso_a4-extra") == 0)
    return DMPAPER_A4_EXTRA;
  if (strcmp (format, "iso_a5-extra") == 0)
    return DMPAPER_A5_EXTRA;
  if (strcmp (format, "iso_b5-extra") == 0)
    return DMPAPER_B5_EXTRA;
  if (strcmp (format, "na_b-plus") == 0)
    return DMPAPER_B_PLUS;
  if (strcmp (format, "na_letter-plus") == 0)
    return DMPAPER_LETTER_PLUS;

  return 0;
}

static char *
get_default_printer (void)
{
  wchar_t *win32_printer_name = NULL;
  char *printer_name = NULL;
  DWORD needed;

  GetDefaultPrinterW (NULL, &needed);
  win32_printer_name = g_malloc ((gsize) needed * sizeof (wchar_t));
  if (!GetDefaultPrinterW (win32_printer_name, &needed))
    {
      g_free (win32_printer_name);
      return NULL;
    }
  printer_name = g_utf16_to_utf8 (win32_printer_name, -1, NULL, NULL, NULL);
  g_free (win32_printer_name);

  return printer_name;
}

static void
set_hard_margins (GtkPrintOperation *op)
{
  double top, bottom, left, right;
  GtkPrintOperationWin32 *op_win32 = op->priv->platform_data;

  top = GetDeviceCaps (op_win32->hdc, PHYSICALOFFSETY);
  bottom = GetDeviceCaps (op_win32->hdc, PHYSICALHEIGHT)
      - GetDeviceCaps (op_win32->hdc, VERTRES) - top;
  left = GetDeviceCaps (op_win32->hdc, PHYSICALOFFSETX);
  right = GetDeviceCaps (op_win32->hdc, PHYSICALWIDTH)
      - GetDeviceCaps (op_win32->hdc, HORZRES) - left;

  _gtk_print_context_set_hard_margins (op->priv->print_context, top, bottom, left, right);
}

void
win32_start_page (GtkPrintOperation *op,
		  GtkPrintContext *print_context,
		  GtkPageSetup *page_setup)
{
  GtkPrintOperationWin32 *op_win32 = op->priv->platform_data;
  LPDEVMODEW devmode;
  GtkPaperSize *paper_size;
  double x_off, y_off;

  devmode = GlobalLock (op_win32->devmode);
  
  devmode->dmFields |= DM_ORIENTATION;
  devmode->dmOrientation =
    orientation_to_win32 (gtk_page_setup_get_orientation (page_setup));
  
  paper_size = gtk_page_setup_get_paper_size (page_setup);
  devmode->dmFields |= DM_PAPERSIZE;
  devmode->dmFields &= ~(DM_PAPERWIDTH | DM_PAPERLENGTH);
  devmode->dmPaperSize = paper_size_to_win32 (paper_size);
  if (devmode->dmPaperSize == 0)
    {
      devmode->dmPaperSize = DMPAPER_USER;
      devmode->dmFields |= DM_PAPERWIDTH | DM_PAPERLENGTH;

      /* Lengths in DEVMODE are in tenths of a millimeter */
      devmode->dmPaperWidth = gtk_paper_size_get_width (paper_size, GTK_UNIT_MM) * 10.0;
      devmode->dmPaperLength = gtk_paper_size_get_height (paper_size, GTK_UNIT_MM) * 10.0;
    }
  
  ResetDCW (op_win32->hdc, devmode);
  
  GlobalUnlock (op_win32->devmode);

  set_hard_margins (op);
  x_off = GetDeviceCaps (op_win32->hdc, PHYSICALOFFSETX);
  y_off = GetDeviceCaps (op_win32->hdc, PHYSICALOFFSETY);
  cairo_surface_set_device_offset (op_win32->surface, -x_off, -y_off);
  
  StartPage (op_win32->hdc);
}

static void
win32_end_page (GtkPrintOperation *op,
		GtkPrintContext *print_context)
{
  GtkPrintOperationWin32 *op_win32 = op->priv->platform_data;

  cairo_surface_show_page (op_win32->surface);

  EndPage (op_win32->hdc);
}

static gboolean
win32_poll_status_timeout (GtkPrintOperation *op)
{
  GtkPrintOperationWin32 *op_win32 = op->priv->platform_data;
  
  op_win32->timeout_id = 0;
  /* We need to ref this, as setting the status to finished
     might unref the object */
  g_object_ref (op);
  win32_poll_status (op);

  if (!gtk_print_operation_is_finished (op)) {
    op_win32->timeout_id = g_timeout_add (STATUS_POLLING_TIME,
					  (GSourceFunc)win32_poll_status_timeout,
					  op);
    gdk_source_set_static_name_by_id (op_win32->timeout_id, "[gtk] win32_poll_status_timeout");
  }
  g_object_unref (op);
  return FALSE;
}


static void
win32_end_run (GtkPrintOperation *op,
	       gboolean           wait,
	       gboolean           cancelled)
{
  GtkPrintOperationWin32 *op_win32 = op->priv->platform_data;
  LPDEVNAMES devnames;
  HANDLE printerHandle = 0;

  cairo_surface_finish (op_win32->surface);
  
  EndDoc (op_win32->hdc);

  if (op->priv->track_print_status)
    {
      devnames = GlobalLock (op_win32->devnames);
      if (!OpenPrinterW (((gunichar2 *)devnames) + devnames->wDeviceOffset,
			 &printerHandle, NULL))
	printerHandle = 0;
      GlobalUnlock (op_win32->devnames);
    }
  
  GlobalFree (op_win32->devmode);
  GlobalFree (op_win32->devnames);

  cairo_surface_destroy (op_win32->surface);
  op_win32->surface = NULL;

  DeleteDC (op_win32->hdc);
  
  if (printerHandle != 0)
    {
      op_win32->printerHandle = printerHandle;
      win32_poll_status (op);
      op_win32->timeout_id = g_timeout_add (STATUS_POLLING_TIME,
					    (GSourceFunc)win32_poll_status_timeout,
					    op);
      gdk_source_set_static_name_by_id (op_win32->timeout_id, "[gtk] win32_poll_status_timeout");
    }
  else
    /* Dunno what happened, pretend its finished */
    _gtk_print_operation_set_status (op, GTK_PRINT_STATUS_FINISHED, NULL);
}

static void
win32_poll_status (GtkPrintOperation *op)
{
  GtkPrintOperationWin32 *op_win32 = op->priv->platform_data;
  guchar *data;
  DWORD needed;
  JOB_INFO_1W *job_info;
  GtkPrintStatus status;
  char *status_str;
  BOOL ret;

  GetJobW (op_win32->printerHandle, op_win32->job_id,
	   1,(LPBYTE)NULL, 0, &needed);
  data = g_malloc (needed);
  ret = GetJobW (op_win32->printerHandle, op_win32->job_id,
		 1, (LPBYTE)data, needed, &needed);

  status_str = NULL;
  if (ret)
    {
      DWORD win32_status;
      job_info = (JOB_INFO_1W *)data;
      win32_status = job_info->Status;

      if (job_info->pStatus)
	status_str = g_utf16_to_utf8 (job_info->pStatus, 
				      -1, NULL, NULL, NULL);
     
      if (win32_status &
	  (JOB_STATUS_COMPLETE | JOB_STATUS_PRINTED))
	status = GTK_PRINT_STATUS_FINISHED;
      else if (win32_status &
	       (JOB_STATUS_OFFLINE |
		JOB_STATUS_PAPEROUT |
		JOB_STATUS_PAUSED |
		JOB_STATUS_USER_INTERVENTION))
	{
	  status = GTK_PRINT_STATUS_PENDING_ISSUE;
	  if (status_str == NULL)
	    {
	      if (win32_status & JOB_STATUS_OFFLINE)
		status_str = g_strdup (_("Printer offline"));
	      else if (win32_status & JOB_STATUS_PAPEROUT)
		status_str = g_strdup (_("Out of paper"));
	      else if (win32_status & JOB_STATUS_PAUSED)
		status_str = g_strdup (_("Paused"));
	      else if (win32_status & JOB_STATUS_USER_INTERVENTION)
		status_str = g_strdup (_("Need user intervention"));
	    }
	}
      else if (win32_status &
	       (JOB_STATUS_BLOCKED_DEVQ |
		JOB_STATUS_DELETED |
		JOB_STATUS_ERROR))
	status = GTK_PRINT_STATUS_FINISHED_ABORTED;
      else if (win32_status &
	       (JOB_STATUS_SPOOLING |
		JOB_STATUS_DELETING))
	status = GTK_PRINT_STATUS_PENDING;
      else if (win32_status & JOB_STATUS_PRINTING)
	status = GTK_PRINT_STATUS_PRINTING;
      else
	status = GTK_PRINT_STATUS_FINISHED;
    }
  else
    status = GTK_PRINT_STATUS_FINISHED;

  g_free (data);

  _gtk_print_operation_set_status (op, status, status_str);
 
  g_free (status_str);
}

static void
op_win32_free (GtkPrintOperationWin32 *op_win32)
{
  if (op_win32->printerHandle)
    ClosePrinter (op_win32->printerHandle);
  if (op_win32->timeout_id != 0)
    g_source_remove (op_win32->timeout_id);
  g_free (op_win32);
}

static HWND
get_parent_hwnd (GtkWidget *widget)
{
  GtkNative *native;

  native = gtk_widget_get_native (widget);
  gtk_widget_realize (GTK_WIDGET (native));
  return gdk_win32_surface_get_handle (gtk_native_get_surface (native));
}

static void
devnames_to_settings (GtkPrintSettings *settings,
		      HANDLE hDevNames)
{
  GtkPrintWin32Devnames *devnames = gtk_print_win32_devnames_from_win32 (hDevNames);
  gtk_print_settings_set_printer (settings, devnames->device);
  gtk_print_win32_devnames_free (devnames);
}

static void
devmode_to_settings (GtkPrintSettings *settings,
		     HANDLE hDevMode)
{
  LPDEVMODEW devmode;

  devmode = GlobalLock (hDevMode);
  
  gtk_print_settings_set_int (settings, GTK_PRINT_SETTINGS_WIN32_DRIVER_VERSION,
			      devmode->dmDriverVersion);
  if (devmode->dmDriverExtra != 0)
    {
      char *extra = g_base64_encode (((const guchar *)devmode) + sizeof (DEVMODEW),
				     devmode->dmDriverExtra);
      gtk_print_settings_set (settings,
			      GTK_PRINT_SETTINGS_WIN32_DRIVER_EXTRA,
			      extra);
      g_free (extra);
    }
  
  char *devmode_name = g_utf16_to_utf8 (devmode->dmDeviceName, -1, NULL, NULL, NULL);
  gtk_print_settings_set (settings, "win32-devmode-name", devmode_name);
  g_free (devmode_name);

  if (devmode->dmFields & DM_ORIENTATION)
    gtk_print_settings_set_orientation (settings,
					orientation_from_win32 (devmode->dmOrientation));
  
  
  if (devmode->dmFields & DM_PAPERSIZE &&
      devmode->dmPaperSize != 0)
    {
      GtkPaperSize *paper_size = paper_size_from_win32 (devmode->dmPaperSize);
      if (paper_size)
	{
	  gtk_print_settings_set_paper_size (settings, paper_size);
	  gtk_paper_size_free (paper_size);
	}
      gtk_print_settings_set_int (settings, "win32-paper-size", (int)devmode->dmPaperSize);
    }
  else if ((devmode->dmFields & DM_PAPERSIZE &&
	    devmode->dmPaperSize == 0) ||
	   ((devmode->dmFields & DM_PAPERWIDTH) &&
	    (devmode->dmFields & DM_PAPERLENGTH)))
    {
      GtkPaperSize *paper_size;
      char *form_name = NULL;
      if (devmode->dmFields & DM_FORMNAME)
	form_name = g_utf16_to_utf8 (devmode->dmFormName, 
				     -1, NULL, NULL, NULL);
      if (form_name == NULL || form_name[0] == 0)
	form_name = g_strdup (_("Custom size"));

      /* Lengths in DEVMODE are in tenths of a millimeter */
      paper_size = gtk_paper_size_new_custom (form_name,
					      form_name,
					      devmode->dmPaperWidth / 10.0,
					      devmode->dmPaperLength / 10.0,
					      GTK_UNIT_MM);
      gtk_print_settings_set_paper_size (settings, paper_size);
      gtk_paper_size_free (paper_size);
    }
  
  if (devmode->dmFields & DM_SCALE)
    gtk_print_settings_set_scale (settings, devmode->dmScale);
  
  if (devmode->dmFields & DM_COPIES)
    gtk_print_settings_set_n_copies (settings,
				     devmode->dmCopies);
  
  if (devmode->dmFields & DM_DEFAULTSOURCE)
    {
      const char *source;
      switch (devmode->dmDefaultSource)
	{
	default:
	case DMBIN_AUTO:
	  source = "auto";
	  break;
	case DMBIN_CASSETTE:
	  source = "cassette";
	  break;
	case DMBIN_ENVELOPE:
	  source = "envelope";
	  break;
	case DMBIN_ENVMANUAL:
	  source = "envelope-manual";
	  break;
	case DMBIN_LOWER:
	  source = "lower";
	  break;
	case DMBIN_MANUAL:
	  source = "manual";
	  break;
	case DMBIN_MIDDLE:
	  source = "middle";
	  break;
	case DMBIN_ONLYONE:
	  source = "only-one";
	  break;
	case DMBIN_FORMSOURCE:
	  source = "form-source";
	  break;
	case DMBIN_LARGECAPACITY:
	  source = "large-capacity";
	  break;
	case DMBIN_LARGEFMT:
	  source = "large-format";
	  break;
	case DMBIN_TRACTOR:
	  source = "tractor";
	  break;
	case DMBIN_SMALLFMT:
	  source = "small-format";
	  break;
	}
      gtk_print_settings_set_default_source (settings, source);
      gtk_print_settings_set_int (settings, "win32-default-source", devmode->dmDefaultSource);
    }
  
  if (devmode->dmFields & DM_PRINTQUALITY)
    {
      GtkPrintQuality quality;
      switch (devmode->dmPrintQuality)
	{
	case DMRES_LOW:
	  quality = GTK_PRINT_QUALITY_LOW;
	  break;
	case DMRES_MEDIUM:
	  quality = GTK_PRINT_QUALITY_NORMAL;
	  break;
	default:
	case DMRES_HIGH:
	  quality = GTK_PRINT_QUALITY_HIGH;
	  break;
	case DMRES_DRAFT:
	  quality = GTK_PRINT_QUALITY_DRAFT;
	  break;
	}
      gtk_print_settings_set_quality (settings, quality);
      gtk_print_settings_set_int (settings, "win32-print-quality", devmode->dmPrintQuality);
    }
  
  if (devmode->dmFields & DM_COLOR)
    gtk_print_settings_set_use_color (settings, devmode->dmColor == DMCOLOR_COLOR);
  
  if (devmode->dmFields & DM_DUPLEX)
    {
      GtkPrintDuplex duplex;
      switch (devmode->dmDuplex)
	{
	default:
	case DMDUP_SIMPLEX:
	  duplex = GTK_PRINT_DUPLEX_SIMPLEX;
	  break;
	case DMDUP_HORIZONTAL:
	  duplex = GTK_PRINT_DUPLEX_HORIZONTAL;
	  break;
	case DMDUP_VERTICAL:
	  duplex = GTK_PRINT_DUPLEX_VERTICAL;
	  break;
	}
      
      gtk_print_settings_set_duplex (settings, duplex);
    }
  
  if (devmode->dmFields & DM_COLLATE)
    gtk_print_settings_set_collate (settings,
				    devmode->dmCollate == DMCOLLATE_TRUE);
  
  if (devmode->dmFields & DM_MEDIATYPE)
    {
      const char *media_type;
      switch (devmode->dmMediaType)
	{
	default:
	case DMMEDIA_STANDARD:
	  media_type = "stationery";
	  break;
	case DMMEDIA_TRANSPARENCY:
	  media_type = "transparency";
	  break;
	case DMMEDIA_GLOSSY:
	  media_type = "photographic-glossy";
	  break;
	}
      gtk_print_settings_set_media_type (settings, media_type);
      gtk_print_settings_set_int (settings, "win32-media-type", devmode->dmMediaType);
    }
  
  if (devmode->dmFields & DM_DITHERTYPE)
    {
      const char *dither;
      switch (devmode->dmDitherType)
	{
	default:
	case DMDITHER_FINE:
	  dither = "fine";
	  break;
	case DMDITHER_NONE:
	  dither = "none";
	  break;
	case DMDITHER_COARSE:
	  dither = "coarse";
	  break;
	case DMDITHER_LINEART:
	  dither = "lineart";
	  break;
	case DMDITHER_GRAYSCALE:
	  dither = "grayscale";
	  break;
	case DMDITHER_ERRORDIFFUSION:
	  dither = "error-diffusion";
	  break;
	}
      gtk_print_settings_set_dither (settings, dither);
      gtk_print_settings_set_int (settings, "win32-dither-type", devmode->dmDitherType);
    }
  
  GlobalUnlock (hDevMode);
}

static void
dialog_to_print_settings (GtkPrintOperation *op,
			  LPPRINTDLGEXW printdlgex)
{
  guint i;
  GtkPrintSettings *settings;

  settings = gtk_print_settings_new ();

  gtk_print_settings_set_print_pages (settings,
				      GTK_PRINT_PAGES_ALL);
  if (printdlgex->Flags & PD_CURRENTPAGE)
    gtk_print_settings_set_print_pages (settings,
					GTK_PRINT_PAGES_CURRENT);
  else if (printdlgex->Flags & PD_PAGENUMS)
    gtk_print_settings_set_print_pages (settings,
					GTK_PRINT_PAGES_RANGES);

  if (printdlgex->nPageRanges > 0)
    {
      GtkPageRange *ranges;
      ranges = g_new (GtkPageRange, printdlgex->nPageRanges);

      for (i = 0; i < printdlgex->nPageRanges; i++)
	{
	  ranges[i].start = printdlgex->lpPageRanges[i].nFromPage - 1;
	  ranges[i].end = printdlgex->lpPageRanges[i].nToPage - 1;
	}

      gtk_print_settings_set_page_ranges (settings, ranges,
					  printdlgex->nPageRanges);
      g_free (ranges);
    }
  
  if (printdlgex->hDevNames != NULL)
    devnames_to_settings (settings, printdlgex->hDevNames);

  if (printdlgex->hDevMode != NULL)
    devmode_to_settings (settings, printdlgex->hDevMode);
  
  gtk_print_operation_set_print_settings (op, settings);
}

static HANDLE
devmode_from_settings (GtkPrintSettings *settings,
		       GtkPageSetup *page_setup,
		       HANDLE hDevModeParam)
{
  HANDLE hDevMode = hDevModeParam;
  LPDEVMODEW devmode;
  guchar *extras;
  GtkPaperSize *paper_size;
  const char *extras_base64;
  gsize extras_len;
  const char *val;

  /* If we already provided a valid hDevMode, don't initialize a new one; just lock the one we have */
  if (hDevMode)
    {
      devmode = GlobalLock (hDevMode);
    }
  else
    {
      extras = NULL;
      extras_len = 0;
      extras_base64 = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_WIN32_DRIVER_EXTRA);
      if (extras_base64)
        extras = g_base64_decode (extras_base64, &extras_len);
  
      hDevMode = GlobalAlloc (GMEM_MOVEABLE, 
			      sizeof (DEVMODEW) + extras_len);

      devmode = GlobalLock (hDevMode);

      memset (devmode, 0, sizeof (DEVMODEW));
  
      devmode->dmSpecVersion = DM_SPECVERSION;
      devmode->dmSize = sizeof (DEVMODEW);
  
      gunichar2 *device_name = g_utf8_to_utf16 (gtk_print_settings_get (settings, "win32-devmode-name"), -1, NULL, NULL, NULL);
      memcpy (devmode->dmDeviceName, device_name, CCHDEVICENAME);
      g_free (device_name);


      devmode->dmDriverExtra = 0;
      if (extras && extras_len > 0)
        {
          devmode->dmDriverExtra = extras_len;
          memcpy (((char *)devmode) + sizeof (DEVMODEW), extras, extras_len);
        }
      g_free (extras);
  
      if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_WIN32_DRIVER_VERSION))
        {
          devmode->dmDriverVersion = gtk_print_settings_get_int (settings, GTK_PRINT_SETTINGS_WIN32_DRIVER_VERSION);
        }
    }
  
  if (page_setup ||
      gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_ORIENTATION))
    {
      GtkPageOrientation orientation = gtk_print_settings_get_orientation (settings);
      if (page_setup)
	orientation = gtk_page_setup_get_orientation (page_setup);
      devmode->dmFields |= DM_ORIENTATION;
      devmode->dmOrientation = orientation_to_win32 (orientation);
    }

  if (page_setup)
    paper_size = gtk_paper_size_copy (gtk_page_setup_get_paper_size (page_setup));
  else
    {
      int size;
      if (gtk_print_settings_has_key (settings, "win32-paper-size") &&
	  (size = gtk_print_settings_get_int (settings, "win32-paper-size")) != 0)
	{
	  devmode->dmFields |= DM_PAPERSIZE;
	  devmode->dmPaperSize = size;
	  paper_size = NULL;
	}
      else
	paper_size = gtk_print_settings_get_paper_size (settings);
    }
  if (paper_size)
    {
      devmode->dmFields |= DM_PAPERSIZE;
      devmode->dmPaperSize = paper_size_to_win32 (paper_size);
      if (devmode->dmPaperSize == 0)
	{
	  devmode->dmPaperSize = DMPAPER_USER;
	  devmode->dmFields |= DM_PAPERWIDTH | DM_PAPERLENGTH;

          /* Lengths in DEVMODE are in tenths of a millimeter */
	  devmode->dmPaperWidth = gtk_paper_size_get_width (paper_size, GTK_UNIT_MM) * 10.0;
	  devmode->dmPaperLength = gtk_paper_size_get_height (paper_size, GTK_UNIT_MM) * 10.0;
	}
      gtk_paper_size_free (paper_size);
    }

  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_SCALE))
    {
      devmode->dmFields |= DM_SCALE;
      devmode->dmScale = gtk_print_settings_get_scale (settings);
    }
  
  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_N_COPIES))
    {
      devmode->dmFields |= DM_COPIES;
      devmode->dmCopies = gtk_print_settings_get_n_copies (settings);
    }

  if (gtk_print_settings_has_key (settings, "win32-default-source"))
    {
      devmode->dmFields |= DM_DEFAULTSOURCE;
      devmode->dmDefaultSource = gtk_print_settings_get_int (settings, "win32-default-source");
    }
  else if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_DEFAULT_SOURCE))
    {
      devmode->dmFields |= DM_DEFAULTSOURCE;
      devmode->dmDefaultSource = DMBIN_AUTO;

      val = gtk_print_settings_get_default_source (settings);
      if (strcmp (val, "auto") == 0)
	devmode->dmDefaultSource = DMBIN_AUTO;
      if (strcmp (val, "cassette") == 0)
	devmode->dmDefaultSource = DMBIN_CASSETTE;
      if (strcmp (val, "envelope") == 0)
	devmode->dmDefaultSource = DMBIN_ENVELOPE;
      if (strcmp (val, "envelope-manual") == 0)
	devmode->dmDefaultSource = DMBIN_ENVMANUAL;
      if (strcmp (val, "lower") == 0)
	devmode->dmDefaultSource = DMBIN_LOWER;
      if (strcmp (val, "manual") == 0)
	devmode->dmDefaultSource = DMBIN_MANUAL;
      if (strcmp (val, "middle") == 0)
	devmode->dmDefaultSource = DMBIN_MIDDLE;
      if (strcmp (val, "only-one") == 0)
	devmode->dmDefaultSource = DMBIN_ONLYONE;
      if (strcmp (val, "form-source") == 0)
	devmode->dmDefaultSource = DMBIN_FORMSOURCE;
      if (strcmp (val, "large-capacity") == 0)
	devmode->dmDefaultSource = DMBIN_LARGECAPACITY;
      if (strcmp (val, "large-format") == 0)
	devmode->dmDefaultSource = DMBIN_LARGEFMT;
      if (strcmp (val, "tractor") == 0)
	devmode->dmDefaultSource = DMBIN_TRACTOR;
      if (strcmp (val, "small-format") == 0)
	devmode->dmDefaultSource = DMBIN_SMALLFMT;
    }

  if (gtk_print_settings_has_key (settings, "win32-print-quality"))
    {
      devmode->dmFields |= DM_PRINTQUALITY;
      devmode->dmPrintQuality = gtk_print_settings_get_int (settings, "win32-print-quality");
    }
  else if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_RESOLUTION))
    {
      devmode->dmFields |= DM_PRINTQUALITY;
      devmode->dmPrintQuality = gtk_print_settings_get_resolution (settings);
    } 
  else if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_QUALITY))
    {
      devmode->dmFields |= DM_PRINTQUALITY;
      switch (gtk_print_settings_get_quality (settings))
	{
	case GTK_PRINT_QUALITY_LOW:
	  devmode->dmPrintQuality = DMRES_LOW;
	  break;
	case GTK_PRINT_QUALITY_DRAFT:
	  devmode->dmPrintQuality = DMRES_DRAFT;
	  break;
	default:
	case GTK_PRINT_QUALITY_NORMAL:
	  devmode->dmPrintQuality = DMRES_MEDIUM;
	  break;
	case GTK_PRINT_QUALITY_HIGH:
	  devmode->dmPrintQuality = DMRES_HIGH;
	  break;
	}
    }

  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_USE_COLOR))
    {
      devmode->dmFields |= DM_COLOR;
      if (gtk_print_settings_get_use_color (settings))
	devmode->dmColor = DMCOLOR_COLOR;
      else
	devmode->dmColor = DMCOLOR_MONOCHROME;
    }

  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_DUPLEX))
    {
      devmode->dmFields |= DM_DUPLEX;
      switch (gtk_print_settings_get_duplex (settings))
	{
	default:
	case GTK_PRINT_DUPLEX_SIMPLEX:
	  devmode->dmDuplex = DMDUP_SIMPLEX;
	  break;
	case GTK_PRINT_DUPLEX_HORIZONTAL:
	  devmode->dmDuplex = DMDUP_HORIZONTAL;
	  break;
	case GTK_PRINT_DUPLEX_VERTICAL:
	  devmode->dmDuplex = DMDUP_VERTICAL;
	  break;
	}
    }

  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_COLLATE))
    {
      devmode->dmFields |= DM_COLLATE;
      if (gtk_print_settings_get_collate (settings))
	devmode->dmCollate = DMCOLLATE_TRUE;
      else
	devmode->dmCollate = DMCOLLATE_FALSE;
    }

  if (gtk_print_settings_has_key (settings, "win32-media-type"))
    {
      devmode->dmFields |= DM_MEDIATYPE;
      devmode->dmMediaType = gtk_print_settings_get_int (settings, "win32-media-type");
    }
  else if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_MEDIA_TYPE))
    {
      devmode->dmFields |= DM_MEDIATYPE;
      devmode->dmMediaType = DMMEDIA_STANDARD;
      
      val = gtk_print_settings_get_media_type (settings);
      if (strcmp (val, "transparency") == 0)
	devmode->dmMediaType = DMMEDIA_TRANSPARENCY;
      if (strcmp (val, "photographic-glossy") == 0)
	devmode->dmMediaType = DMMEDIA_GLOSSY;
    }
 
  if (gtk_print_settings_has_key (settings, "win32-dither-type"))
    {
      devmode->dmFields |= DM_DITHERTYPE;
      devmode->dmDitherType = gtk_print_settings_get_int (settings, "win32-dither-type");
    }
  else if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_DITHER))
    {
      devmode->dmFields |= DM_DITHERTYPE;
      devmode->dmDitherType = DMDITHER_FINE;
      
      val = gtk_print_settings_get_dither (settings);
      if (strcmp (val, "none") == 0)
	devmode->dmDitherType = DMDITHER_NONE;
      if (strcmp (val, "coarse") == 0)
	devmode->dmDitherType = DMDITHER_COARSE;
      if (strcmp (val, "fine") == 0)
	devmode->dmDitherType = DMDITHER_FINE;
      if (strcmp (val, "lineart") == 0)
	devmode->dmDitherType = DMDITHER_LINEART;
      if (strcmp (val, "grayscale") == 0)
	devmode->dmDitherType = DMDITHER_GRAYSCALE;
      if (strcmp (val, "error-diffusion") == 0)
	devmode->dmDitherType = DMDITHER_ERRORDIFFUSION;
    }
  
  GlobalUnlock (hDevMode);

  return hDevMode;
}

static void
dialog_from_print_settings (GtkPrintOperation *op,
			    LPPRINTDLGEXW printdlgex)
{
  GtkPrintSettings *settings = op->priv->print_settings;
  const char *printer;

  if (settings == NULL)
    return;

  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_PRINT_PAGES))
    {
      GtkPrintPages print_pages = gtk_print_settings_get_print_pages (settings);

      switch (print_pages)
	{
	default:
	case GTK_PRINT_PAGES_SELECTION:
	case GTK_PRINT_PAGES_ALL:
	  printdlgex->Flags |= PD_ALLPAGES;
	  break;
	case GTK_PRINT_PAGES_CURRENT:
	  printdlgex->Flags |= PD_CURRENTPAGE;
	  break;
	case GTK_PRINT_PAGES_RANGES:
	  printdlgex->Flags |= PD_PAGENUMS;
	  break;
	}
    }
  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_PAGE_RANGES))
    {
      GtkPageRange *ranges;
      int num_ranges, i;

      ranges = gtk_print_settings_get_page_ranges (settings, &num_ranges);

      if (num_ranges > MAX_PAGE_RANGES)
	num_ranges = MAX_PAGE_RANGES;

      printdlgex->nPageRanges = num_ranges;
      for (i = 0; i < num_ranges; i++)
	{
	  printdlgex->lpPageRanges[i].nFromPage = ranges[i].start + 1;
	  printdlgex->lpPageRanges[i].nToPage = ranges[i].end + 1;
	}
    }
  
  /* If we have a printer saved, restore our settings */
  printer = gtk_print_settings_get_printer (settings);
  if (printer)
    {
      printdlgex->hDevNames = gtk_print_win32_devnames_to_win32_from_printer_name (printer);
  
      printdlgex->hDevMode = devmode_from_settings (settings,
						  op->priv->default_page_setup, NULL);
    }
  else
    {
      /* Otherwise, use the default settings */
      DWORD FlagsCopy = printdlgex->Flags;
      printdlgex->Flags |= PD_RETURNDEFAULT;
      PrintDlgExW (printdlgex);
      printdlgex->Flags = FlagsCopy;

      devmode_from_settings (settings, op->priv->default_page_setup, printdlgex->hDevMode);
    }
}

typedef struct {
  IPrintDialogCallback iPrintDialogCallback;
  gboolean set_hwnd;
  int ref_count;
} PrintDialogCallback;


static ULONG STDMETHODCALLTYPE
iprintdialogcallback_addref (IPrintDialogCallback *This)
{
  PrintDialogCallback *callback = (PrintDialogCallback *)This;
  return ++callback->ref_count;
}

static ULONG STDMETHODCALLTYPE
iprintdialogcallback_release (IPrintDialogCallback *This)
{
  PrintDialogCallback *callback = (PrintDialogCallback *)This;
  int ref_count = --callback->ref_count;

  if (ref_count == 0)
    g_free (This);

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
iprintdialogcallback_queryinterface (IPrintDialogCallback *This,
				     REFIID       riid,
				     LPVOID      *ppvObject)
{
   if (IsEqualIID (riid, &IID_IUnknown) ||
       IsEqualIID (riid, &myIID_IPrintDialogCallback))
     {
       *ppvObject = This;
       IUnknown_AddRef ((IUnknown *)This);
       return NOERROR;
     }
   else
     {
       *ppvObject = NULL;
       return E_NOINTERFACE;
     }
}

static HRESULT STDMETHODCALLTYPE
iprintdialogcallback_initdone (IPrintDialogCallback *This)
{
  return S_FALSE;
}

static HRESULT STDMETHODCALLTYPE
iprintdialogcallback_selectionchange (IPrintDialogCallback *This)
{
  return S_FALSE;
}

static HRESULT STDMETHODCALLTYPE
iprintdialogcallback_handlemessage (IPrintDialogCallback *This,
				    HWND hDlg,
				    UINT uMsg,
				    WPARAM wParam,
				    LPARAM lParam,
				    LRESULT *pResult)
{
  PrintDialogCallback *callback = (PrintDialogCallback *)This;

  if (!callback->set_hwnd)
    {
      gdk_win32_set_modal_dialog_libgtk_only (hDlg);
      callback->set_hwnd = TRUE;
      while (g_main_context_pending (NULL))
        g_main_context_iteration (NULL, TRUE);
    }
  else if (uMsg == got_gdk_events_message)
    {
      while (g_main_context_pending (NULL))
        g_main_context_iteration (NULL, TRUE);
      *pResult = TRUE;
      return S_OK;
    }
  
  *pResult = 0;
  return S_FALSE;
}

static IPrintDialogCallbackVtbl ipdc_vtbl = {
  iprintdialogcallback_queryinterface,
  iprintdialogcallback_addref,
  iprintdialogcallback_release,
  iprintdialogcallback_initdone,
  iprintdialogcallback_selectionchange,
  iprintdialogcallback_handlemessage
};

static IPrintDialogCallback *
print_callback_new  (void)
{
  PrintDialogCallback *callback;

  callback = g_new0 (PrintDialogCallback, 1);
  callback->iPrintDialogCallback.lpVtbl = &ipdc_vtbl;
  callback->ref_count = 1;
  callback->set_hwnd = FALSE;

  return &callback->iPrintDialogCallback;
}

static INT_PTR CALLBACK
pageDlgProc (HWND wnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  GtkPrintOperation *op;
  GtkPrintOperationWin32 *op_win32;
  
  if (message == WM_INITDIALOG)
    {
      PROPSHEETPAGEW *page = (PROPSHEETPAGEW *)lparam;
      GtkWidget *plug;

      op = GTK_PRINT_OPERATION ((gpointer)page->lParam);
      op_win32 = op->priv->platform_data;
      plug = g_object_new(GTK_TYPE_WIDGET, NULL);

      SetWindowLongPtrW (wnd, GWLP_USERDATA, (LONG_PTR)op);

      gtk_window_set_modal (GTK_WINDOW (plug), TRUE);
      op_win32->embed_widget = plug;
      gtk_box_append (GTK_BOX (plug), op->priv->custom_widget);
      gtk_widget_set_visible (op->priv->custom_widget, TRUE);
      gtk_widget_set_visible (plug, TRUE);

      /* This dialog is modal, so we grab the embed widget */
      gtk_grab_add (plug);
      return FALSE;
    }
  else if (message == WM_DESTROY)
    {
      op = GTK_PRINT_OPERATION (GetWindowLongPtrW (wnd, GWLP_USERDATA));
      op_win32 = op->priv->platform_data;
      
      g_signal_emit_by_name (op, "custom-widget-apply", op->priv->custom_widget);
      g_object_unref (g_object_ref_sink (op_win32->embed_widget));
      op_win32->embed_widget = NULL;
      op->priv->custom_widget = NULL;
    }
  else 
    {
      op = GTK_PRINT_OPERATION (GetWindowLongPtrW (wnd, GWLP_USERDATA));
      op_win32 = op->priv->platform_data;

      /* TODO: We don't have GtkWin32EmbedWidgets anymore, but it is not currently clear
       *       at this point what will be the proper replacement for this.  For now,
       *       do the message handling that was in _gtk_win32_embed_widget_dialog_procedure ()
       *       here and fill in the rest when things become clearer.
       */
      if (message == WM_SIZE)
        {
          gtk_widget_queue_resize (op_win32->embed_widget);
    }

      return FALSE;
    }
  
  return FALSE;
}

static HPROPSHEETPAGE
create_application_page (GtkPrintOperation *op)
{
  HPROPSHEETPAGE hpage;
  PROPSHEETPAGEW page;
  DLGTEMPLATE *template;
  HGLOBAL htemplate;
  LONG base_units;
  WORD baseunitX, baseunitY;
  WORD *array;
  GtkRequisition requisition;
  const char *tab_label;

  /* Make the template the size of the custom widget size request */
  gtk_widget_get_preferred_size (op->priv->custom_widget,
                                 &requisition, NULL);

  base_units = GetDialogBaseUnits ();
  baseunitX = LOWORD (base_units);
  baseunitY = HIWORD (base_units);
  
  htemplate = GlobalAlloc (GMEM_MOVEABLE, 
			   sizeof (DLGTEMPLATE) + sizeof (WORD) * 3);
  template = GlobalLock (htemplate);
  template->style = WS_CHILDWINDOW | DS_CONTROL;
  template->dwExtendedStyle = WS_EX_CONTROLPARENT;
  template->cdit = 0;
  template->x = MulDiv (0, 4, baseunitX);
  template->y = MulDiv (0, 8, baseunitY);
  template->cx = MulDiv (requisition.width, 4, baseunitX);
  template->cy = MulDiv (requisition.height, 8, baseunitY);
  
  array = (WORD *) (template+1);
  *array++ = 0; /* menu */
  *array++ = 0; /* class */
  *array++ = 0; /* title */
  
  memset (&page, 0, sizeof (page));
  page.dwSize = sizeof (page);
  page.dwFlags = PSP_DLGINDIRECT | PSP_USETITLE | PSP_PREMATURE;
  page.hInstance = NULL;
  page.pResource = template;
  
  tab_label = op->priv->custom_tab_label;
  if (tab_label == NULL)
    tab_label = g_get_application_name ();
  if (tab_label == NULL)
    tab_label = _("Application");
  page.pszTitle = g_utf8_to_utf16 (tab_label, 
				   -1, NULL, NULL, NULL);
  page.pfnDlgProc = pageDlgProc;
  page.pfnCallback = NULL;
  page.lParam = (LPARAM) op;
  hpage = CreatePropertySheetPageW (&page);
  
  GlobalUnlock (htemplate);
  
  /* TODO: We're leaking htemplate here... */
  
  return hpage;
}

static GtkPageSetup *
create_page_setup (GtkPrintOperation *op)
{
  GtkPrintOperationPrivate *priv = op->priv;
  GtkPageSetup *page_setup;
  GtkPrintSettings *settings;
  
  if (priv->default_page_setup)
    page_setup = gtk_page_setup_copy (priv->default_page_setup);
  else
    page_setup = gtk_page_setup_new ();

  settings = priv->print_settings;
  if (settings)
    {
      GtkPaperSize *paper_size;
      
      if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_ORIENTATION))
	gtk_page_setup_set_orientation (page_setup,
					gtk_print_settings_get_orientation (settings));


      paper_size = gtk_print_settings_get_paper_size (settings);
      if (paper_size)
	{
	  gtk_page_setup_set_paper_size (page_setup, paper_size);
	  gtk_paper_size_free (paper_size);
	}

      /* TODO: Margins? */
    }
  
  return page_setup;
}

GtkPrintOperationResult
gtk_print_operation_run_without_dialog (GtkPrintOperation *op,
					gboolean          *do_print)
{
  GtkPrintOperationResult result;
  GtkPrintOperationWin32 *op_win32;
  GtkPrintOperationPrivate *priv;
  GtkPrintSettings *settings;
  GtkPageSetup *page_setup;
  DOCINFOW docinfo;
  HGLOBAL hDevMode = NULL;
  HGLOBAL hDevNames = NULL;
  HDC hDC = NULL;
  const char *printer = NULL;
  double dpi_x, dpi_y;
  int job_id;
  cairo_t *cr;
  DEVNAMES *pdn;
  DEVMODEW *pdm;

  *do_print = FALSE;

  priv = op->priv;
  settings = priv->print_settings;
  
  op_win32 = g_new0 (GtkPrintOperationWin32, 1);
  priv->platform_data = op_win32;
  priv->free_platform_data = (GDestroyNotify) op_win32_free;
  printer = gtk_print_settings_get_printer (settings);

  if (!printer)
    {
      /* No printer selected. Get the system default printer and store
       * it in settings.
       */
      char *tmp_printer = get_default_printer ();
      if (!tmp_printer)
	{
	  result = GTK_PRINT_OPERATION_RESULT_ERROR;
	  g_set_error_literal (&priv->error,
			       GTK_PRINT_ERROR,
			       GTK_PRINT_ERROR_INTERNAL_ERROR,
			       _("No printer found"));
	  goto out;
	}
      gtk_print_settings_set_printer (settings, tmp_printer);
      printer = gtk_print_settings_get_printer (settings);
      g_free (tmp_printer);
    }

  hDevNames = gtk_print_win32_devnames_to_win32_from_printer_name (printer);
  hDevMode = devmode_from_settings (settings, op->priv->default_page_setup, NULL);

  /* Create a printer DC for the print settings and page setup provided. */
  pdn = GlobalLock (hDevNames);
  pdm = GlobalLock (hDevMode);
  hDC = CreateDCW ((wchar_t*)pdn + pdn->wDriverOffset,
		   (wchar_t*)pdn + pdn->wDeviceOffset,
		   (wchar_t*)pdn + pdn->wOutputOffset,
		   pdm );
  GlobalUnlock (hDevNames);
  GlobalUnlock (hDevMode);

  if (!hDC)
    {
      result = GTK_PRINT_OPERATION_RESULT_ERROR;
      g_set_error_literal (&priv->error,
			   GTK_PRINT_ERROR,
			   GTK_PRINT_ERROR_INTERNAL_ERROR,
			   _("Invalid argument to CreateDC"));
      goto out;
    }
  
  priv->print_context = _gtk_print_context_new (op);
  page_setup = create_page_setup (op);
  _gtk_print_context_set_page_setup (priv->print_context, page_setup);
  g_object_unref (page_setup);

  *do_print = TRUE;

  op_win32->surface = cairo_win32_printing_surface_create (hDC);
  dpi_x = (double) GetDeviceCaps (hDC, LOGPIXELSX);
  dpi_y = (double) GetDeviceCaps (hDC, LOGPIXELSY);

  cr = cairo_create (op_win32->surface);
  gtk_print_context_set_cairo_context (priv->print_context, cr, dpi_x, dpi_y);
  cairo_destroy (cr);

  set_hard_margins (op);

  memset (&docinfo, 0, sizeof (DOCINFOW));
  docinfo.cbSize = sizeof (DOCINFOW); 
  docinfo.lpszDocName = g_utf8_to_utf16 (op->priv->job_name, -1, NULL, NULL, NULL); 
  docinfo.lpszOutput = NULL; 
  docinfo.lpszDatatype = NULL; 
  docinfo.fwType = 0; 

  job_id = StartDocW (hDC, &docinfo); 
  g_free ((void *)docinfo.lpszDocName);
  if (job_id <= 0)
    { 
      result = GTK_PRINT_OPERATION_RESULT_ERROR;
      g_set_error_literal (&priv->error,
			   GTK_PRINT_ERROR,
			   GTK_PRINT_ERROR_GENERAL,
			   _("Error from StartDoc"));
      *do_print = FALSE;
      cairo_surface_destroy (op_win32->surface);
      op_win32->surface = NULL;
      goto out; 
    }

  result = GTK_PRINT_OPERATION_RESULT_APPLY;
  op_win32->hdc = hDC;
  op_win32->devmode = hDevMode;
  op_win32->devnames = hDevNames;
  op_win32->job_id = job_id;
  op->priv->print_pages = gtk_print_settings_get_print_pages (op->priv->print_settings);
  op->priv->num_page_ranges = 0;
  if (op->priv->print_pages == GTK_PRINT_PAGES_RANGES)
    op->priv->page_ranges = gtk_print_settings_get_page_ranges (op->priv->print_settings,
								&op->priv->num_page_ranges);
  op->priv->manual_num_copies = 1;
  op->priv->manual_collation = FALSE;
  op->priv->manual_reverse = FALSE;
  op->priv->manual_orientation = FALSE;
  op->priv->manual_scale = 1.0;
  op->priv->manual_page_set = GTK_PAGE_SET_ALL;
  op->priv->manual_number_up = 1;
  op->priv->manual_number_up_layout = GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM;

  op->priv->start_page = win32_start_page;
  op->priv->end_page = win32_end_page;
  op->priv->end_run = win32_end_run;
  
 out:
  if (!*do_print && hDC != NULL)
    DeleteDC (hDC);

  if (!*do_print && hDevMode != NULL)
    GlobalFree (hDevMode);

  if (!*do_print && hDevNames != NULL)
    GlobalFree (hDevNames);

  return result;
}

GtkPrintOperationResult
gtk_print_operation_run_with_dialog (GtkPrintOperation *op,
				     GtkWindow         *parent,
				     gboolean          *do_print)
{
  HRESULT hResult;
  LPPRINTDLGEXW printdlgex = NULL;
  LPPRINTPAGERANGE page_ranges = NULL;
  HWND parentHWnd;
  GtkWidget *invisible = NULL;
  GtkPrintOperationResult result;
  GtkPrintOperationWin32 *op_win32;
  GtkPrintOperationPrivate *priv;
  IPrintDialogCallback *callback;
  HPROPSHEETPAGE prop_page;
  static gsize common_controls_initialized = 0;

  if (g_once_init_enter (&common_controls_initialized))
    {
      BOOL initialized;
      INITCOMMONCONTROLSEX icc;

      memset (&icc, 0, sizeof (icc));
      icc.dwSize = sizeof (icc);
      icc.dwICC = ICC_WIN95_CLASSES;

      initialized = InitCommonControlsEx (&icc);
      if (!initialized)
        g_warning ("Failed to InitCommonControlsEx: %lu", GetLastError ());

      _gtk_load_dll_with_libgtk3_manifest ("comdlg32.dll");

      g_once_init_leave (&common_controls_initialized, initialized ? 1 : 0);
    }
  
  *do_print = FALSE;

  priv = op->priv;
  
  op_win32 = g_new0 (GtkPrintOperationWin32, 1);
  priv->platform_data = op_win32;
  priv->free_platform_data = (GDestroyNotify) op_win32_free;
  
  if (parent == NULL)
    {
      invisible = gtk_window_new ();
      parentHWnd = get_parent_hwnd (invisible);
    }
  else
    parentHWnd = get_parent_hwnd (GTK_WIDGET (parent));

  printdlgex = (LPPRINTDLGEXW)GlobalAlloc (GPTR, sizeof (PRINTDLGEXW));
  if (!printdlgex)
    {
      result = GTK_PRINT_OPERATION_RESULT_ERROR;
      g_set_error_literal (&priv->error,
                           GTK_PRINT_ERROR,
                           GTK_PRINT_ERROR_NOMEM,
                           _("Not enough free memory"));
      goto out;
    }      

  printdlgex->lStructSize = sizeof (PRINTDLGEXW);
  printdlgex->hwndOwner = parentHWnd;
  printdlgex->hDevMode = NULL;
  printdlgex->hDevNames = NULL;
  printdlgex->hDC = NULL;
  printdlgex->Flags = PD_RETURNDC | PD_NOSELECTION;
  if (op->priv->current_page == -1)
    printdlgex->Flags |= PD_NOCURRENTPAGE;
  printdlgex->Flags2 = 0;
  printdlgex->ExclusionFlags = 0;

  page_ranges = (LPPRINTPAGERANGE) GlobalAlloc (GPTR, 
						MAX_PAGE_RANGES * sizeof (PRINTPAGERANGE));
  if (!page_ranges) 
    {
      result = GTK_PRINT_OPERATION_RESULT_ERROR;
      g_set_error_literal (&priv->error,
                           GTK_PRINT_ERROR,
                           GTK_PRINT_ERROR_NOMEM,
                           _("Not enough free memory"));
      goto out;
    }

  printdlgex->nPageRanges = 0;
  printdlgex->nMaxPageRanges = MAX_PAGE_RANGES;
  printdlgex->lpPageRanges = page_ranges;
  printdlgex->nMinPage = 1;
  if (op->priv->nr_of_pages != -1)
    printdlgex->nMaxPage = op->priv->nr_of_pages;
  else
    printdlgex->nMaxPage = 10000;
  printdlgex->nCopies = 1;
  printdlgex->hInstance = 0;
  printdlgex->lpPrintTemplateName = NULL;
  printdlgex->lpCallback = NULL;

  g_signal_emit_by_name (op, "create-custom-widget",
			 &op->priv->custom_widget);
  if (op->priv->custom_widget) {
    prop_page = create_application_page (op);
    printdlgex->nPropertyPages = 1;
    printdlgex->lphPropertyPages = &prop_page;
  } else {
    printdlgex->nPropertyPages = 0;
    printdlgex->lphPropertyPages = NULL;
  }
  
  printdlgex->nStartPage = START_PAGE_GENERAL;
  printdlgex->dwResultAction = 0;

  dialog_from_print_settings (op, printdlgex);

  callback = print_callback_new ();
  printdlgex->lpCallback = (IUnknown *)callback;
  got_gdk_events_message = RegisterWindowMessage ("GDK_WIN32_GOT_EVENTS");

  hResult = PrintDlgExW (printdlgex);
  IUnknown_Release ((IUnknown *)callback);
  gdk_win32_set_modal_dialog_libgtk_only (NULL);

  if (hResult != S_OK) 
    {
      result = GTK_PRINT_OPERATION_RESULT_ERROR;
      if (hResult == E_OUTOFMEMORY)
	g_set_error_literal (&priv->error,
                             GTK_PRINT_ERROR,
                             GTK_PRINT_ERROR_NOMEM,
                             _("Not enough free memory"));
      else if (hResult == E_INVALIDARG)
	g_set_error_literal (&priv->error,
                             GTK_PRINT_ERROR,
                             GTK_PRINT_ERROR_INTERNAL_ERROR,
                             _("Invalid argument to PrintDlgEx"));
      else if (hResult == E_POINTER)
	g_set_error_literal (&priv->error,
                             GTK_PRINT_ERROR,
                             GTK_PRINT_ERROR_INTERNAL_ERROR,
                             _("Invalid pointer to PrintDlgEx"));
      else if (hResult == E_HANDLE)
	g_set_error_literal (&priv->error,
                             GTK_PRINT_ERROR,
                             GTK_PRINT_ERROR_INTERNAL_ERROR,
                             _("Invalid handle to PrintDlgEx"));
      else /* E_FAIL */
	g_set_error_literal (&priv->error,
                             GTK_PRINT_ERROR,
                             GTK_PRINT_ERROR_GENERAL,
                             _("Unspecified error"));
      goto out;
    }

  if (printdlgex->dwResultAction == PD_RESULT_PRINT ||
      printdlgex->dwResultAction == PD_RESULT_APPLY)
    {
      result = GTK_PRINT_OPERATION_RESULT_APPLY;
      dialog_to_print_settings (op, printdlgex);
    }
  else
    result = GTK_PRINT_OPERATION_RESULT_CANCEL;
  
  if (printdlgex->dwResultAction == PD_RESULT_PRINT)
    {
      DOCINFOW docinfo;
      int job_id;
      double dpi_x, dpi_y;
      cairo_t *cr;
      GtkPageSetup *page_setup;

      priv->print_context = _gtk_print_context_new (op);
      page_setup = create_page_setup (op);
      _gtk_print_context_set_page_setup (priv->print_context, page_setup);
      g_object_unref (page_setup);
      
      *do_print = TRUE;

      op_win32->surface = cairo_win32_printing_surface_create (printdlgex->hDC);

      dpi_x = (double)GetDeviceCaps (printdlgex->hDC, LOGPIXELSX);
      dpi_y = (double)GetDeviceCaps (printdlgex->hDC, LOGPIXELSY);

      cr = cairo_create (op_win32->surface);
      gtk_print_context_set_cairo_context (priv->print_context, cr, dpi_x, dpi_y);
      cairo_destroy (cr);

      set_hard_margins (op);

      memset ( &docinfo, 0, sizeof (DOCINFOW));
      docinfo.cbSize = sizeof (DOCINFOW); 
      docinfo.lpszDocName = g_utf8_to_utf16 (op->priv->job_name, -1, NULL, NULL, NULL); 
      docinfo.lpszOutput = (LPCWSTR) NULL; 
      docinfo.lpszDatatype = (LPCWSTR) NULL; 
      docinfo.fwType = 0; 

      job_id = StartDocW (printdlgex->hDC, &docinfo); 
      g_free ((void *)docinfo.lpszDocName);
      if (job_id <= 0) 
	{
	  result = GTK_PRINT_OPERATION_RESULT_ERROR;
	  g_set_error_literal (&priv->error,
                               GTK_PRINT_ERROR,
                               GTK_PRINT_ERROR_GENERAL,
                               _("Error from StartDoc"));
	  *do_print = FALSE;
	  cairo_surface_destroy (op_win32->surface);
	  op_win32->surface = NULL;
	  goto out; 
	} 
      
      op_win32->hdc = printdlgex->hDC;
      op_win32->devmode = printdlgex->hDevMode;
      op_win32->devnames = printdlgex->hDevNames;
      op_win32->job_id = job_id;
      
      op->priv->print_pages = gtk_print_settings_get_print_pages (op->priv->print_settings);
      op->priv->num_page_ranges = 0;
      if (op->priv->print_pages == GTK_PRINT_PAGES_RANGES)
	op->priv->page_ranges = gtk_print_settings_get_page_ranges (op->priv->print_settings,
								    &op->priv->num_page_ranges);
      op->priv->manual_num_copies = printdlgex->nCopies;
      op->priv->manual_collation = (printdlgex->Flags & PD_COLLATE) != 0;
      op->priv->manual_reverse = FALSE;
      op->priv->manual_orientation = FALSE;
      op->priv->manual_scale = 1.0;
      op->priv->manual_page_set = GTK_PAGE_SET_ALL;
      op->priv->manual_number_up = 1;
      op->priv->manual_number_up_layout = GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM;
    }

  op->priv->start_page = win32_start_page;
  op->priv->end_page = win32_end_page;
  op->priv->end_run = win32_end_run;
  
  out:
  if (!*do_print && printdlgex && printdlgex->hDC != NULL)
    DeleteDC (printdlgex->hDC);

  if (!*do_print && printdlgex && printdlgex->hDevMode != NULL) 
    GlobalFree (printdlgex->hDevMode); 

  if (!*do_print && printdlgex && printdlgex->hDevNames != NULL) 
    GlobalFree (printdlgex->hDevNames); 

  if (page_ranges)
    GlobalFree (page_ranges);

  if (printdlgex)
    GlobalFree (printdlgex);

  if (invisible)
    gtk_window_destroy (GTK_WINDOW (invisible));

  return result;
}

GtkPrintOperationResult
_gtk_print_operation_platform_backend_run_dialog (GtkPrintOperation *op,
						  gboolean           show_dialog,
						  GtkWindow         *parent,
						  gboolean          *do_print)
{
  if (show_dialog)
    return gtk_print_operation_run_with_dialog (op, parent, do_print);
  else
    return gtk_print_operation_run_without_dialog (op, do_print);
}

void
_gtk_print_operation_platform_backend_launch_preview (GtkPrintOperation *op,
						      cairo_surface_t   *surface,
						      GtkWindow         *parent,
						      const char        *filename)
{
  HDC dc;
  HENHMETAFILE metafile;
  
  dc = cairo_win32_surface_get_dc (surface);
  cairo_surface_destroy (surface);
  metafile = CloseEnhMetaFile (dc);
  DeleteEnhMetaFile (metafile);
  
  ShellExecuteW (NULL, L"open", (gunichar2 *)filename, NULL, NULL, SW_SHOW);
}

void
_gtk_print_operation_platform_backend_preview_start_page (GtkPrintOperation *op,
							  cairo_surface_t *surface,
							  cairo_t *cr)
{
  HDC dc = cairo_win32_surface_get_dc (surface);
  StartPage (dc);
}

void
_gtk_print_operation_platform_backend_preview_end_page (GtkPrintOperation *op,
							cairo_surface_t *surface,
							cairo_t *cr)
{
  HDC dc;

  cairo_surface_show_page (surface);

  /* TODO: Enhanced metafiles don't support multiple pages.
   */
  dc = cairo_win32_surface_get_dc (surface);
  EndPage (dc);
}

cairo_surface_t *
_gtk_print_operation_platform_backend_create_preview_surface (GtkPrintOperation *op,
							      GtkPageSetup      *page_setup,
							      double            *dpi_x,
							      double            *dpi_y,
							      char             **target)
{
  GtkPaperSize *paper_size;
  HDC metafile_dc;
  RECT rect;
  char *template;
  char *filename;
  gunichar2 *filename_utf16;
  int fd;

  template = g_build_filename (g_get_tmp_dir (), "prXXXXXX", NULL);
  fd = g_mkstemp (template);
  close (fd);

  filename = g_strconcat (template, ".emf", NULL);
  g_free (template);
  
  filename_utf16 = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  g_free (filename);

  paper_size = gtk_page_setup_get_paper_size (page_setup);

  /* The rectangle dimensions are given in hundredths of a millimeter */
  rect.left = 0;
  rect.right = 100.0 * gtk_paper_size_get_width (paper_size, GTK_UNIT_MM);
  rect.top = 0;
  rect.bottom = 100.0 * gtk_paper_size_get_height (paper_size, GTK_UNIT_MM);
  
  metafile_dc = CreateEnhMetaFileW (NULL, filename_utf16,
				    &rect, L"Gtk+\0Print Preview\0\0");
  if (metafile_dc == NULL)
    {
      g_warning ("Can't create metafile");
      return NULL;
    }

  *target = (char *)filename_utf16;
  
  *dpi_x = (double)GetDeviceCaps (metafile_dc, LOGPIXELSX);
  *dpi_y = (double)GetDeviceCaps (metafile_dc, LOGPIXELSY);

  return cairo_win32_printing_surface_create (metafile_dc);
}

void
_gtk_print_operation_platform_backend_resize_preview_surface (GtkPrintOperation *op,
							      GtkPageSetup      *page_setup,
							      cairo_surface_t   *surface)
{
  /* TODO: Implement */
}

/**
 * gtk_print_run_page_setup_dialog:
 * @parent: (nullable): transient parent
 * @page_setup: (nullable): an existing `GtkPageSetup`
 * @settings: a `GtkPrintSettings`
 *
 * Runs a page setup dialog, letting the user modify the values from
 * @page_setup. If the user cancels the dialog, the returned `GtkPageSetup`
 * is identical to the passed in @page_setup, otherwise it contains the
 * modifications done in the dialog.
 *
 * Note that this function may use a recursive mainloop to show the page
 * setup dialog. See gtk_print_run_page_setup_dialog_async() if this is
 * a problem.
 *
 * Returns: (transfer full): a new `GtkPageSetup`
 */
GtkPageSetup *
gtk_print_run_page_setup_dialog (GtkWindow        *parent,
				 GtkPageSetup     *page_setup,
				 GtkPrintSettings *settings)
{
  LPPAGESETUPDLGW pagesetupdlg = NULL;
  BOOL res;
  gboolean free_settings;
  const char *printer;
  GtkPaperSize *paper_size;
  DWORD measure_system;
  GtkUnit unit;
  double scale;

  pagesetupdlg = (LPPAGESETUPDLGW)GlobalAlloc (GPTR, sizeof (PAGESETUPDLGW));
  if (!pagesetupdlg)
    return NULL;

  free_settings = FALSE;
  if (settings == NULL)
    {
      settings = gtk_print_settings_new ();
      free_settings = TRUE;
    }
  
  memset (pagesetupdlg, 0, sizeof (PAGESETUPDLGW));

  pagesetupdlg->lStructSize = sizeof (PAGESETUPDLGW);

  if (parent != NULL)
    pagesetupdlg->hwndOwner = get_parent_hwnd (GTK_WIDGET (parent));
  else
    pagesetupdlg->hwndOwner = NULL;

  pagesetupdlg->Flags = PSD_DEFAULTMINMARGINS;
  pagesetupdlg->hDevMode = devmode_from_settings (settings, page_setup, NULL);
  pagesetupdlg->hDevNames = NULL;
  printer = gtk_print_settings_get_printer (settings);
  if (printer)
    pagesetupdlg->hDevNames = gtk_print_win32_devnames_to_win32_from_printer_name (printer);

  GetLocaleInfoW (LOCALE_USER_DEFAULT, LOCALE_IMEASURE|LOCALE_RETURN_NUMBER,
		  (LPWSTR)&measure_system, sizeof (DWORD));

  if (measure_system == 0)
    {
      pagesetupdlg->Flags |= PSD_INHUNDREDTHSOFMILLIMETERS;
      unit = GTK_UNIT_MM;
      scale = 100;
    }
  else
    {
      pagesetupdlg->Flags |= PSD_INTHOUSANDTHSOFINCHES;
      unit = GTK_UNIT_INCH;
      scale = 1000;
    }

  /* This is the object we return, we allocate it here so that
   * we can use the default page margins */
  if (page_setup)
    page_setup = gtk_page_setup_copy (page_setup);
  else
    page_setup = gtk_page_setup_new ();
  
  pagesetupdlg->Flags |= PSD_MARGINS;
  pagesetupdlg->rtMargin.left =
    floor (gtk_page_setup_get_left_margin (page_setup, unit) * scale + 0.5);
  pagesetupdlg->rtMargin.right =
    floor (gtk_page_setup_get_right_margin (page_setup, unit) * scale + 0.5);
  pagesetupdlg->rtMargin.top = 
    floor (gtk_page_setup_get_top_margin (page_setup, unit) * scale + 0.5);
  pagesetupdlg->rtMargin.bottom =
    floor (gtk_page_setup_get_bottom_margin (page_setup, unit) * scale + 0.5);

  pagesetupdlg->Flags |= PSD_ENABLEPAGESETUPHOOK;
  pagesetupdlg->lpfnPageSetupHook = run_mainloop_hook;
  got_gdk_events_message = RegisterWindowMessage ("GDK_WIN32_GOT_EVENTS");
  
  res = PageSetupDlgW (pagesetupdlg);
  gdk_win32_set_modal_dialog_libgtk_only (NULL);

  if (res)
    {  
      if (pagesetupdlg->hDevNames != NULL)
	devnames_to_settings (settings, pagesetupdlg->hDevNames);

      if (pagesetupdlg->hDevMode != NULL)
	devmode_to_settings (settings, pagesetupdlg->hDevMode);
    }
  
  if (res)
    {
      gtk_page_setup_set_orientation (page_setup, 
				      gtk_print_settings_get_orientation (settings));
      paper_size = gtk_print_settings_get_paper_size (settings);
      if (paper_size)
	{
	  gtk_page_setup_set_paper_size (page_setup, paper_size);
	  gtk_paper_size_free (paper_size);
	}

      if (pagesetupdlg->Flags & PSD_INHUNDREDTHSOFMILLIMETERS)
	{
	  unit = GTK_UNIT_MM;
	  scale = 100;
	}
      else
	{
	  unit = GTK_UNIT_INCH;
	  scale = 1000;
	}

      gtk_page_setup_set_left_margin (page_setup,
				      pagesetupdlg->rtMargin.left / scale,
				      unit);
      gtk_page_setup_set_right_margin (page_setup,
				       pagesetupdlg->rtMargin.right / scale,
				       unit);
      gtk_page_setup_set_top_margin (page_setup,
				     pagesetupdlg->rtMargin.top / scale,
				     unit);
      gtk_page_setup_set_bottom_margin (page_setup,
					pagesetupdlg->rtMargin.bottom / scale,
					unit);
    }
  
  if (free_settings)
    g_object_unref (settings);

  return page_setup;
}

/**
 * gtk_print_run_page_setup_dialog_async:
 * @parent: (nullable): transient parent
 * @page_setup: (nullable): an existing `GtkPageSetup`
 * @settings: a `GtkPrintSettings`
 * @done_cb: (scope async): a function to call when the user saves
 *    the modified page setup
 * @data: user data to pass to @done_cb
 *
 * Runs a page setup dialog, letting the user modify the values from @page_setup.
 *
 * In contrast to gtk_print_run_page_setup_dialog(), this function  returns after
 * showing the page setup dialog on platforms that support this, and calls @done_cb
 * from a signal handler for the ::response signal of the dialog.
 */
void
gtk_print_run_page_setup_dialog_async (GtkWindow            *parent,
				       GtkPageSetup         *page_setup,
				       GtkPrintSettings     *settings,
				       GtkPageSetupDoneFunc  done_cb,
				       gpointer              data)
{
  GtkPageSetup *new_page_setup;

  new_page_setup = gtk_print_run_page_setup_dialog (parent, page_setup, settings);
  done_cb (new_page_setup, data);
  g_object_unref (new_page_setup);
}
