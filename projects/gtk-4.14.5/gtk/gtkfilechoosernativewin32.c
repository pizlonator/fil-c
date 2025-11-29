/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkfilechoosernativewin32.c: Win32 Native File selector dialog
 * Copyright (C) 2015, Red Hat, Inc.
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

#include "config.h"

/* Vista or newer */
#define _WIN32_WINNT 0x0600
#include <sdkddkver.h>

#define COBJMACROS

#include "gtkfilechoosernativeprivate.h"
#include "gtknativedialogprivate.h"

#include "gtkprivate.h"
#include "deprecated/gtkfilechooserdialog.h"
#include "gtkfilechooserprivate.h"
#include "gtkfilechooserwidgetprivate.h"
#include "gtkfilechooserutils.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtksettings.h"
#include "gtktogglebutton.h"
#include "gtkheaderbar.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkfilefilterprivate.h"
#include "gtknative.h"

#include "win32/gdkwin32.h"
#include <shlobj.h>
#include <windows.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

typedef struct {
  GtkFileChooserNative *self;

  GtkWidget *grab_widget;

  IFileDialogEvents *events;

  HWND parent;
  gboolean skip_response;
  gboolean save;
  gboolean folder;
  gboolean modal;
  gboolean select_multiple;

  char *accept_label;
  char *cancel_label;
  char *title;

  GListModel *shortcut_files;
  GArray *choices_selections;

  GFile *current_folder;
  GFile *current_file;
  char *current_name;

  COMDLG_FILTERSPEC *filters;

  GSList *files;
  int response;
} FilechooserWin32ThreadData;

static void
g_warning_hr (const char *msg, HRESULT hr)
{
  char *errmsg;
  errmsg = g_win32_error_message (hr);
  g_warning ("%s: %s", msg, errmsg);
  g_free (errmsg);
}

/* {3CAFD12E-82AE-4184-8309-848C0104B4DC} */
static const GUID myIID_IFileDialogEvents =
{ 0x3cafd12e, 0x82ae, 0x4184, { 0x83, 0x9, 0x84, 0x8c, 0x1, 0x4, 0xb4, 0xdc } };

/* Protects access to dialog_hwnd, do_close and ref_count */
G_LOCK_DEFINE_STATIC(FileDialogEvents);

typedef struct {
  IFileDialogEvents iFileDialogEvents;
  int ref_count;
  gboolean enable_owner;
  gboolean got_hwnd;
  HWND dialog_hwnd;
  gboolean do_close; /* Set if hide was called before dialog_hwnd was set */
  FilechooserWin32ThreadData *data;
} FileDialogEvents;


static ULONG STDMETHODCALLTYPE
ifiledialogevents_AddRef (IFileDialogEvents *self)
{
  FileDialogEvents *events = (FileDialogEvents *)self;
  ULONG ref_count;

  G_LOCK (FileDialogEvents);
  ref_count = ++events->ref_count;
  G_UNLOCK (FileDialogEvents);

  return ref_count;
}

static ULONG STDMETHODCALLTYPE
ifiledialogevents_Release (IFileDialogEvents *self)
{
  FileDialogEvents *events = (FileDialogEvents *)self;
  int ref_count;

  G_LOCK (FileDialogEvents);
  ref_count = --events->ref_count;
  G_UNLOCK (FileDialogEvents);

  if (ref_count == 0)
    g_free (self);

  return ref_count;
}

static HRESULT STDMETHODCALLTYPE
ifiledialogevents_QueryInterface (IFileDialogEvents *self,
                                  REFIID       riid,
                                  LPVOID      *ppvObject)
{
  if (IsEqualIID (riid, &IID_IUnknown) ||
      IsEqualIID (riid, &myIID_IFileDialogEvents))
    {
      *ppvObject = self;
      IUnknown_AddRef ((IUnknown *)self);
      return NOERROR;
    }
  else
    {
      *ppvObject = NULL;
      return E_NOINTERFACE;
    }
}

static HRESULT STDMETHODCALLTYPE
ifiledialogevents_OnFileOk (IFileDialogEvents *self,
                            IFileDialog *pfd)
{
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ifiledialogevents_OnFolderChanging (IFileDialogEvents *self,
                                    IFileDialog *pfd,
                                    IShellItem *psiFolder)
{
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ifiledialogevents_OnFolderChange (IFileDialogEvents *self,
                                  IFileDialog *pfd)
{
  FileDialogEvents *events = (FileDialogEvents *)self;
  IOleWindow *olew = NULL;
  HWND dialog_hwnd;
  HRESULT hr;

  if (!events->got_hwnd)
    {
      events->got_hwnd = TRUE;

      hr = IFileDialog_QueryInterface (pfd, &IID_IOleWindow, (LPVOID *) &olew);
      if (SUCCEEDED (hr))
        {
          hr = IOleWindow_GetWindow (olew, &dialog_hwnd);
          if (SUCCEEDED (hr))
            {
              G_LOCK (FileDialogEvents);
              events->dialog_hwnd = dialog_hwnd;
              if (events->do_close)
                SendMessage (events->dialog_hwnd, WM_CLOSE, 0, 0);
              G_UNLOCK (FileDialogEvents);
            }
          else
            g_warning_hr ("Can't get HWND", hr);

          hr = IOleWindow_Release (olew);
          if (FAILED (hr))
            g_warning_hr ("Can't unref IOleWindow", hr);
        }
      else
        g_warning_hr ("Can't get IOleWindow", hr);

      if (events->enable_owner && events->dialog_hwnd)
        {
          HWND owner = GetWindow (events->dialog_hwnd, GW_OWNER);
          if (owner)
            EnableWindow (owner, TRUE);
        }
    }

  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ifiledialogevents_OnSelectionChange (IFileDialogEvents * self,
                                     IFileDialog *pfd)
{
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ifiledialogevents_OnShareViolation (IFileDialogEvents * self,
                                    IFileDialog *pfd,
                                    IShellItem *psi,
                                    FDE_SHAREVIOLATION_RESPONSE *pResponse)
{
  return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE
ifiledialogevents_OnTypeChange (IFileDialogEvents * self,
                                IFileDialog *pfd)
{
  FileDialogEvents *events = (FileDialogEvents *) self;
  UINT fileType;
  HRESULT hr = IFileDialog_GetFileTypeIndex (pfd, &fileType);
  if (FAILED (hr))
    {
      g_warning_hr ("Can't get current file type", hr);
      return S_OK;
    }
  fileType--; // fileTypeIndex starts at 1 
  GListModel *filters = gtk_file_chooser_get_filters (GTK_FILE_CHOOSER (events->data->self));
  GtkFileFilter *filter = g_list_model_get_item (filters, fileType);
  events->data->self->current_filter = filter;
  g_object_unref (filter);
  g_object_unref (filters);
  g_object_notify (G_OBJECT (events->data->self), "filter");
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE
ifiledialogevents_OnOverwrite (IFileDialogEvents * self,
                               IFileDialog *pfd,
                               IShellItem *psi,
                               FDE_OVERWRITE_RESPONSE *pResponse)
{
  return E_NOTIMPL;
}

static IFileDialogEventsVtbl ifde_vtbl = {
  ifiledialogevents_QueryInterface,
  ifiledialogevents_AddRef,
  ifiledialogevents_Release,
  ifiledialogevents_OnFileOk,
  ifiledialogevents_OnFolderChanging,
  ifiledialogevents_OnFolderChange,
  ifiledialogevents_OnSelectionChange,
  ifiledialogevents_OnShareViolation,
  ifiledialogevents_OnTypeChange,
  ifiledialogevents_OnOverwrite
};

static void
file_dialog_events_send_close (IFileDialogEvents *self)
{
  FileDialogEvents *events = (FileDialogEvents *)self;

  G_LOCK (FileDialogEvents);

  if (events->dialog_hwnd)
    SendMessage (events->dialog_hwnd, WM_CLOSE, 0, 0);
  else
    events->do_close = TRUE;

  G_UNLOCK (FileDialogEvents);
}

static IFileDialogEvents *
file_dialog_events_new (gboolean enable_owner, FilechooserWin32ThreadData *data)
{
  FileDialogEvents *events;

  events = g_new0 (FileDialogEvents, 1);
  events->iFileDialogEvents.lpVtbl = &ifde_vtbl;
  events->ref_count = 1;
  events->enable_owner = enable_owner;
  events->data = data;

  return &events->iFileDialogEvents;
}

static void
filechooser_win32_thread_data_free (FilechooserWin32ThreadData *data)
{
  int i;
  if (data->filters)
    {
      for (i = 0; data->filters[i].pszName != NULL; i++)
        {
          g_free ((char *)data->filters[i].pszName);
          g_free ((char *)data->filters[i].pszSpec);
        }
      g_free (data->filters);
    }

  if (data->events)
    IFileDialogEvents_Release (data->events);

  if (data->grab_widget)
    {
      gtk_grab_remove (data->grab_widget);
      g_object_unref (data->grab_widget);
    }

  g_clear_object (&data->current_folder);
  g_clear_object (&data->current_file);
  g_free (data->current_name);

  if (data->choices_selections)
    {
      g_array_free (data->choices_selections, TRUE);
      data->choices_selections = NULL;
    }
  g_object_unref (data->shortcut_files);
  g_slist_free_full (data->files, g_object_unref);
  if (data->self)
    g_object_unref (data->self);
  g_free (data->accept_label);
  g_free (data->cancel_label);
  g_free (data->title);
  g_free (data);
}

static gboolean
filechooser_win32_thread_done (gpointer _data)
{
  FilechooserWin32ThreadData *data = _data;
  GtkFileChooserNative *self = data->self;
  GSList *l;

  self->mode_data = NULL;

  for (l = self->choices; l; l = l->next)
    {
      GtkFileChooserNativeChoice *choice = (GtkFileChooserNativeChoice*) l->data;
      int sel = g_array_index (data->choices_selections, int,
                                g_slist_position (self->choices, l));

      if (sel >= 0)
        {
          if (choice->options)
            {
              g_free (choice->selected);
              choice->selected = g_strdup (choice->options[sel]);
            }
          else
            {
              g_free (choice->selected);
              choice->selected = sel ? g_strdup ("true") : g_strdup ("false");
            }
        }
    }

  if (!data->skip_response)
    {
      g_slist_free_full (self->custom_files, g_object_unref);
      self->custom_files = g_slist_reverse(data->files);
      data->files = NULL;

      _gtk_native_dialog_emit_response (GTK_NATIVE_DIALOG (data->self),
                                        data->response);
    }

  filechooser_win32_thread_data_free (data);

  return FALSE;
}

static GFile *
get_file_for_shell_item (IShellItem *item)
{
  HRESULT hr;
  PWSTR pathw = NULL;
  char *path;
  GFile *file;

  hr = IShellItem_GetDisplayName (item, SIGDN_FILESYSPATH, &pathw);
  if (SUCCEEDED (hr))
    {
      path = g_utf16_to_utf8 (pathw, -1, NULL, NULL, NULL);
      CoTaskMemFree (pathw);
      if (path != NULL)
        {
          file = g_file_new_for_path (path);
          g_free (path);
          return file;
       }
    }

  /* TODO: also support URLs through SIGDN_URL, but Windows URLS are not
   * RFC 3986 compliant and we'd need to convert them first.
   */

  return NULL;
}

static void
data_add_shell_item (FilechooserWin32ThreadData *data,
                     IShellItem *item)
{
  GFile *file;

  file = get_file_for_shell_item (item);
  if (file != NULL)
    {
      data->files = g_slist_prepend (data->files, file);
      data->response = GTK_RESPONSE_ACCEPT;
    }
}

static IShellItem *
get_shell_item_for_uri (const char *uri)
{
  IShellItem *item;
  HRESULT hr;
  gunichar2 *uri_w = g_utf8_to_utf16 (uri, -1, NULL, NULL, NULL);

  hr = SHCreateItemFromParsingName(uri_w, 0, &IID_IShellItem, (LPVOID *) &item);
  if (SUCCEEDED (hr))
    return item;
  else
    g_warning_hr ("Can't create shell item from shortcut", hr);

  return NULL;
}

static IShellItem *
get_shell_item_for_file (GFile *file)
{
  char *uri;
  IShellItem *item;

  uri = g_file_get_uri (file);
  item = get_shell_item_for_uri (uri);
  g_free (uri);

  return item;
}

static gpointer
filechooser_win32_thread (gpointer _data)
{
  FilechooserWin32ThreadData *data = _data;
  HRESULT hr;
  IFileDialog *pfd = NULL;
  IFileDialog2 *pfd2 = NULL;
  DWORD flags = 0;
  DWORD cookie;
  guint j, n_items;

  CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);

  if (data->save && !data->folder)
    hr = CoCreateInstance (&CLSID_FileSaveDialog,
                           NULL, CLSCTX_INPROC_SERVER,
                           &IID_IFileSaveDialog, (LPVOID *) &pfd);
  else
    hr = CoCreateInstance (&CLSID_FileOpenDialog,
                           NULL, CLSCTX_INPROC_SERVER,
                           &IID_IFileOpenDialog, (LPVOID *) &pfd);

  if (FAILED (hr))
    g_error ("Can't create FileOpenDialog: %s", g_win32_error_message (hr));

  hr = IFileDialog_GetOptions (pfd, &flags);
  if (FAILED (hr))
    g_error ("Can't get FileDialog options: %s", g_win32_error_message (hr));

  flags |= FOS_FORCEFILESYSTEM |
           FOS_OVERWRITEPROMPT |
           FOS_NOTESTFILECREATE |
           FOS_NOCHANGEDIR;

  if (data->folder)
    flags |= FOS_PICKFOLDERS;

  if (data->folder && data->save)
    flags &= ~(FOS_FILEMUSTEXIST);

  if (data->select_multiple)
    flags |= FOS_ALLOWMULTISELECT;

  hr = IFileDialog_SetOptions (pfd, flags);
  if (FAILED (hr))
    g_error ("Can't set FileDialog options: %s", g_win32_error_message (hr));

  if (data->title)
    {
      gunichar2 *label = g_utf8_to_utf16 (data->title, -1,
                                        NULL, NULL, NULL);
      IFileDialog_SetTitle (pfd, label);
      g_free (label);
    }

  if (data->accept_label)
    {
      gunichar2 *label = g_utf8_to_utf16 (data->accept_label, -1,
                                        NULL, NULL, NULL);
      IFileDialog_SetOkButtonLabel (pfd, label);
      g_free (label);
    }

  if (data->cancel_label)
    {
      gunichar2 *label = g_utf8_to_utf16 (data->cancel_label, -1,
                                        NULL, NULL, NULL);
      hr = IFileDialog_QueryInterface (pfd, &IID_IFileDialog2, (LPVOID *) &pfd2);
      if (SUCCEEDED (hr))
        {
          IFileDialog2_SetCancelButtonLabel (pfd2, label);
          IFileDialog2_Release (pfd2);
        }
      g_free (label);
    }

  n_items = g_list_model_get_n_items (data->shortcut_files);
  for (j = 0; j < n_items; j++)
    {
      GFile *file = g_list_model_get_item (data->shortcut_files, j);
      IShellItem *item = get_shell_item_for_file (file);
      if (item)
        {
          hr = IFileDialog_AddPlace (pfd, item, FDAP_BOTTOM);
          if (FAILED (hr))
            g_warning_hr ("Can't add dialog shortcut", hr);
          IShellItem_Release (item);
        }
      g_object_unref (file);
    }

  if (data->current_file)
    {
      IFileSaveDialog *pfsd;
      hr = IFileDialog_QueryInterface (pfd, &IID_IFileSaveDialog, (LPVOID *) &pfsd);
      if (SUCCEEDED (hr))
        {
          IShellItem *item = get_shell_item_for_file (data->current_file);
          if (item)
            {
              hr = IFileSaveDialog_SetSaveAsItem (pfsd, item);
              if (FAILED (hr))
                g_warning_hr ("Can't set save as item", hr);
              IShellItem_Release (item);
            }
          IFileSaveDialog_Release (pfsd);
        }
    }

  if (data->current_folder)
    {
      IShellItem *item = get_shell_item_for_file (data->current_folder);
      if (item)
        {
          hr = IFileDialog_SetFolder (pfd, item);
          if (FAILED (hr))
            g_warning_hr ("Can't set folder", hr);
          IShellItem_Release (item);
        }
    }

  if (data->current_name)
    {
      gunichar2 *name = g_utf8_to_utf16 (data->current_name, -1, NULL, NULL, NULL);
      hr = IFileDialog_SetFileName (pfd, name);
      if (FAILED (hr))
        g_warning_hr ("Can't set file name", hr);
      g_free (name);
    }

  if (data->filters)
    {
      int n;
      for (n = 0; data->filters[n].pszName != NULL; n++)
        {}
      hr = IFileDialog_SetFileTypes (pfd, n, data->filters);
      if (FAILED (hr))
        g_warning_hr ("Can't set file types", hr);

      hr = IFileDialog_SetDefaultExtension (pfd, L"");
      if (FAILED (hr))
        g_warning_hr ("Can't set default extension", hr);

      if (data->self->current_filter)
        {
          GListModel *filters;
          guint current_filter_index = GTK_INVALID_LIST_POSITION;

          filters = gtk_file_chooser_get_filters (GTK_FILE_CHOOSER (data->self));
          n_items = g_list_model_get_n_items (filters);
          for (j = 0; j < n_items; j++)
            {
              gpointer item = g_list_model_get_item (filters, j);
              if (item == data->self->current_filter)
                {
                  current_filter_index = j;
                  g_object_unref (item);
                  break;
                }
              g_object_unref (item);
            }
	  g_object_unref (filters);

	  if (current_filter_index >= 0)
	    hr = IFileDialog_SetFileTypeIndex (pfd, current_filter_index + 1);
	  else
	    hr = IFileDialog_SetFileTypeIndex (pfd, 1);
        }
      else
        {
	  hr = IFileDialog_SetFileTypeIndex (pfd, 1);
        }
      if (FAILED (hr))
        g_warning_hr ("Can't set current file type", hr);
    }

  if (data->self->choices)
    {
      IFileDialogCustomize *pfdc;
      DWORD dialog_control_id = 0;
      DWORD dialog_auxiliary_id = (DWORD) g_slist_length (data->self->choices);

      hr = IFileDialog_QueryInterface (pfd, &IID_IFileDialogCustomize, (LPVOID *) &pfdc);
      if (SUCCEEDED (hr))
        {
          GSList *l;

          for (l = data->self->choices; l; l = l->next, dialog_control_id++)
            {
              GtkFileChooserNativeChoice *choice = (GtkFileChooserNativeChoice*) l->data;

              if (choice->options)
                {
                  gunichar2 *label = g_utf8_to_utf16 (choice->label, -1, NULL, NULL, NULL);
                  DWORD sub_id = 0;
                  char **option;

                  IFileDialogCustomize_StartVisualGroup (pfdc, dialog_auxiliary_id++, label);
                  hr = IFileDialogCustomize_AddComboBox (pfdc, dialog_control_id);
                  if (FAILED (hr))
                    g_warning_hr ("Can't add choice", hr);
                  IFileDialogCustomize_EndVisualGroup (pfdc);

                  for (option = choice->options; *option != NULL; option++, sub_id++)
                    {
                      gunichar2 *option_label = g_utf8_to_utf16 (choice->option_labels[sub_id],
                                                                 -1, NULL, NULL, NULL);

                      hr = IFileDialogCustomize_AddControlItem (pfdc,
                                                                dialog_control_id,
                                                                sub_id,
                                                                (LPCWSTR) option_label);
                      if (FAILED (hr))
                        g_warning_hr ("Can't add choice option", hr);

                      if (choice->selected && g_str_equal (*option, choice->selected))
                        IFileDialogCustomize_SetSelectedControlItem (pfdc,
                                                                     dialog_control_id,
                                                                     sub_id);

                      g_free (option_label);
                    }

                  g_free (label);
                }
              else
                {
                  gunichar2 *label = g_utf8_to_utf16 (choice->label,
                                                      -1, NULL, NULL, NULL);

                  hr = IFileDialogCustomize_AddCheckButton (pfdc,
                                                            dialog_control_id,
                                                            label,
                                                            FALSE);
                  if (FAILED (hr))
                    g_warning_hr ("Can't add choice", hr);

                  if (choice->selected)
                    IFileDialogCustomize_SetCheckButtonState (pfdc,
                                                              dialog_control_id,
                                                              g_str_equal (choice->selected, "true"));

                  g_free (label);
                }
            }

          IFileDialogCustomize_Release (pfdc);
        }
    }

  data->response = GTK_RESPONSE_CANCEL;

  hr = IFileDialog_Advise (pfd, data->events, &cookie);
  if (FAILED (hr))
    g_error ("Can't Advise FileDialog: %s", g_win32_error_message (hr));

  hr = IFileDialog_Show (pfd, data->parent);
  if (SUCCEEDED (hr))
    {
      IFileOpenDialog *pfod = NULL;
      hr = IFileDialog_QueryInterface (pfd,&IID_IFileOpenDialog, (LPVOID *) &pfod);

      if (SUCCEEDED (hr))
        {
          IShellItemArray *res;
          DWORD i, count;

          hr = IFileOpenDialog_GetResults (pfod, &res);
          if (FAILED (hr))
            g_error ("Can't get FileOpenDialog results: %s", g_win32_error_message (hr));

          hr = IShellItemArray_GetCount (res, &count);
          if (FAILED (hr))
            g_error ("Can't get FileOpenDialog count: %s", g_win32_error_message (hr));

          for (i = 0; i < count; i++)
            {
              IShellItem *item;
              hr = IShellItemArray_GetItemAt (res, i, &item);
              if (FAILED (hr))
                g_error ("Can't get item at %lu: %s", i, g_win32_error_message (hr));
              data_add_shell_item (data, item);
              IShellItem_Release (item);
            }
          IShellItemArray_Release (res);

          IFileOpenDialog_Release (pfod);
        }
      else
        {
          IShellItem *item;
          hr = IFileDialog_GetResult (pfd, &item);
          if (FAILED (hr))
            g_error ("Can't get FileDialog result: %s", g_win32_error_message (hr));

          data_add_shell_item (data, item);
          IShellItem_Release (item);
        }
    }

  if (data->self->choices)
    {
      IFileDialogCustomize *pfdc = NULL;

      if (data->choices_selections)
        g_array_free (data->choices_selections, TRUE);
      data->choices_selections = g_array_sized_new (FALSE, FALSE, sizeof(int),
                                                    g_slist_length (data->self->choices));

      hr = IFileDialog_QueryInterface (pfd, &IID_IFileDialogCustomize, (LPVOID *) &pfdc);
      if (SUCCEEDED (hr))
        {
          GSList *l;

          for (l = data->self->choices; l; l = l->next)
            {
              GtkFileChooserNativeChoice *choice = (GtkFileChooserNativeChoice*) l->data;
              DWORD dialog_item_id = (DWORD) g_slist_position (data->self->choices, l);
              int val = -1;

              if (choice->options)
                {
                  DWORD dialog_sub_item_id = 0;

                  hr = IFileDialogCustomize_GetSelectedControlItem (pfdc,
                                                                    dialog_item_id,
                                                                    &dialog_sub_item_id);
                  if (SUCCEEDED (hr))
                    val = (int) dialog_sub_item_id;
                }
              else
                {
                  BOOL dialog_check_box_checked = FALSE;

                  hr = IFileDialogCustomize_GetCheckButtonState (pfdc,
                                                                 dialog_item_id,
                                                                 &dialog_check_box_checked);
                  if (SUCCEEDED (hr))
                    val = dialog_check_box_checked ? 1 : 0;
                }

              g_array_append_val (data->choices_selections, val);
            }

          IFileDialogCustomize_Release (pfdc);
        }
    }


  hr = IFileDialog_Unadvise (pfd, cookie);
  if (FAILED (hr))
    g_error ("Can't Unadvise FileDialog: %s", g_win32_error_message (hr));

  IFileDialog_Release ((IUnknown *)pfd);

  CoUninitialize();

  g_main_context_invoke (NULL,
                         filechooser_win32_thread_done,
                         data);

  return NULL;
}

static gboolean
file_filter_to_win32 (GtkFileFilter *filter,
                      COMDLG_FILTERSPEC *spec)
{
  const char *name;
  char **patterns;
  char *pattern_list;

  patterns = _gtk_file_filter_get_as_patterns (filter);
  if (patterns == NULL)
    return FALSE;

  pattern_list = g_strjoinv (";", patterns);
  g_strfreev (patterns);

  name = gtk_file_filter_get_name (filter);
  if (name == NULL)
    name = pattern_list;
  spec->pszName = g_utf8_to_utf16 (name, -1, NULL, NULL, NULL);
  spec->pszSpec = g_utf8_to_utf16 (pattern_list, -1, NULL, NULL, NULL);

  g_free (pattern_list);

  return TRUE;
}

static char *
translate_mnemonics (const char *src)
{
  GString *s;
  const char *p;
  char c;

  if (src == NULL)
    return NULL;

  s = g_string_sized_new (strlen (src));

  for (p = src; *p; p++)
    {
      c = *p;
      switch (c)
        {
        case '_':
          /* __ is _ escaped */
          if (*(p+1) == '_')
            {
              g_string_append_c (s, '_');
              p++;
            }
          else
            g_string_append_c (s, '&');
          break;
        case '&':
          /* Win32 needs ampersands escaped */
          g_string_append (s, "&&");
          break;
        default:
          g_string_append_c (s, c);
        }
    }

  return g_string_free (s, FALSE);
}

gboolean
gtk_file_chooser_native_win32_show (GtkFileChooserNative *self)
{
  GThread *thread;
  FilechooserWin32ThreadData *data;
  GtkWindow *transient_for;
  GtkFileChooserAction action;
  GListModel *filters;
  guint n_filters, i;

  data = g_new0 (FilechooserWin32ThreadData, 1);

  filters = gtk_file_chooser_get_filters (GTK_FILE_CHOOSER (self));
  n_filters = g_list_model_get_n_items (filters);
  if (n_filters > 0)
    {
      data->filters = g_new0 (COMDLG_FILTERSPEC, n_filters + 1);

      for (i = 0; i < n_filters; i++)
        {
          GtkFileFilter *filter = g_list_model_get_item (filters, i);
          if (!file_filter_to_win32 (filter, &data->filters[i]))
            {
              g_object_unref (filter);
              g_object_unref (filters);
              filechooser_win32_thread_data_free (data);
              return FALSE;
            }
          g_object_unref (filter);
        }
      self->current_filter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (self));
    }
  else
    {
      self->current_filter = NULL;
    }
  g_object_unref (filters);

  self->mode_data = data;
  data->self = g_object_ref (self);

  data->shortcut_files =
    gtk_file_chooser_get_shortcut_folders (GTK_FILE_CHOOSER (self->dialog));

  data->accept_label = translate_mnemonics (self->accept_label);
  data->cancel_label = translate_mnemonics (self->cancel_label);

  action = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (self->dialog));
  if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
    data->save = TRUE;

  if (action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    data->folder = TRUE;

  if ((action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
       action == GTK_FILE_CHOOSER_ACTION_OPEN) &&
      gtk_file_chooser_get_select_multiple (GTK_FILE_CHOOSER (self->dialog)))
    data->select_multiple = TRUE;

  transient_for = gtk_native_dialog_get_transient_for (GTK_NATIVE_DIALOG (self));
  if (transient_for)
    {
      gtk_widget_realize (GTK_WIDGET (transient_for));
      data->parent = gdk_win32_surface_get_handle (gtk_native_get_surface (GTK_NATIVE (transient_for)));

      if (gtk_native_dialog_get_modal (GTK_NATIVE_DIALOG (self)))
        data->modal = TRUE;
    }

  data->title =
    g_strdup (gtk_native_dialog_get_title (GTK_NATIVE_DIALOG (self)));

  if (self->current_file)
    data->current_file = g_object_ref (self->current_file);
  else
    {
      if (self->current_folder)
        data->current_folder = g_object_ref (self->current_folder);

      if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
        data->current_name = g_strdup (self->current_name);
    }

  data->events = file_dialog_events_new (!data->modal, data);

  thread = g_thread_new ("win32 filechooser", filechooser_win32_thread, data);
  if (thread == NULL)
    {
      filechooser_win32_thread_data_free (data);
      return FALSE;
    }

  if (data->modal)
    {
      data->grab_widget = g_object_ref_sink (gtk_label_new (""));
      gtk_grab_add (GTK_WIDGET (data->grab_widget));
    }

  return TRUE;
}

void
gtk_file_chooser_native_win32_hide (GtkFileChooserNative *self)
{
  FilechooserWin32ThreadData *data = self->mode_data;

  /* This is always set while dialog visible */
  g_assert (data != NULL);

  data->skip_response = TRUE;
  file_dialog_events_send_close (data->events);
}
