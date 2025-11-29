/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkfilechoosernativequartz.c: Quartz Native File selector dialog
 * Copyright (C) 2017, Tom Schoonjans
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

#include "gtkfilechoosernativeprivate.h"
#include "gtknativedialogprivate.h"

#include <glib/gi18n-lib.h>
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
#include "gtknative.h"
#include "gtkfilefilterprivate.h"

#include "macos/gdkmacos.h"
#include "macos/gdkmacosdisplay-private.h"
#include "macos/gdkmacossurface-private.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

@class FilterComboBox;

typedef struct {
  GtkFileChooserNative *self;

  NSSavePanel *panel;
  NSWindow *parent;
  NSWindow *key_window;
  gboolean skip_response;
  gboolean save;
  gboolean folder;
  gboolean create_folders;
  gboolean modal;
  gboolean select_multiple;
  gboolean running;

  char *accept_label;
  char *cancel_label;
  char *title;
  char *message;

  GFile *current_folder;
  GFile *current_file;
  char *current_name;

  NSMutableArray *filters;
  NSMutableArray *filter_names;
  FilterComboBox *filter_popup_button;

  GSList *files;
  int response;
} FileChooserQuartzData;

@interface FilterComboBox : NSPopUpButton
{
  FileChooserQuartzData *data;
}
- (id) initWithData:(FileChooserQuartzData *) quartz_data;
- (void) popUpButtonSelectionChanged:(id) sender;
@end

@implementation FilterComboBox

- (id) initWithData:(FileChooserQuartzData *) quartz_data
{
  [super initWithFrame:NSMakeRect(0, 0, 200, 24)];
  [self setTarget:self];
  [self setAction:@selector(popUpButtonSelectionChanged:)];
  data = quartz_data;
  return self;
}
- (void)popUpButtonSelectionChanged:(id)sender
{
  NSInteger selected_index = [data->filter_popup_button indexOfSelectedItem];
  NSArray *filter = [data->filters objectAtIndex:selected_index];
  // check for empty strings in filter -> indicates all filetypes should be allowed!
  if ([filter containsObject:@""])
    [data->panel setAllowedFileTypes:nil];
  else
    [data->panel setAllowedFileTypes:filter];

  GListModel *filters = gtk_file_chooser_get_filters (GTK_FILE_CHOOSER (data->self));
  data->self->current_filter = g_list_model_get_item (filters, selected_index);
  g_object_unref (data->self->current_filter);
  g_object_unref (filters);
  g_object_notify (G_OBJECT (data->self), "filter");
}
@end

static GFile *
ns_url_to_g_file (NSURL *url)
{
  if (url == nil)
    {
      return NULL;
    }

  return g_file_new_for_uri ([[url absoluteString] UTF8String]);
}

static GSList *
chooser_get_files (FileChooserQuartzData *data)
{

  GSList *ret = NULL;

  if (!data->save)
    {
      NSArray *urls;
      int i;

      urls = [(NSOpenPanel *)data->panel URLs];

      for (i = 0; i < [urls count]; i++)
        {
          NSURL *url;

          url = (NSURL *)[urls objectAtIndex:i];
          ret = g_slist_prepend (ret, ns_url_to_g_file (url));
        }
    }
  else
    {
      GFile *file;

      file = ns_url_to_g_file ([data->panel URL]);

      if (file != NULL)
        {
          ret = g_slist_prepend (ret, file);
        }
    }

  return g_slist_reverse (ret);
}

static void
chooser_set_current_folder (FileChooserQuartzData *data,
                            GFile                 *folder)
{

  if (folder != NULL)
    {
      char *uri;

      uri = g_file_get_uri (folder);
      [data->panel setDirectoryURL:[NSURL URLWithString:[NSString stringWithUTF8String:uri]]];
      g_free (uri);
    }
}

static void
chooser_set_current_name (FileChooserQuartzData *data,
                          const char            *name)
{

  if (name != NULL)
    [data->panel setNameFieldStringValue:[NSString stringWithUTF8String:name]];
}


static void
filechooser_quartz_data_free (FileChooserQuartzData *data)
{

  if (data->filters)
    {
      [data->filters release];
    }

  if (data->filter_names)
    {
      [data->filter_names release];
    }

  g_clear_object (&data->current_folder);
  g_clear_object (&data->current_file);
  g_free (data->current_name);

  g_slist_free_full (data->files, g_object_unref);
  if (data->self)
    g_object_unref (data->self);
  g_free (data->accept_label);
  g_free (data->cancel_label);
  g_free (data->title);
  g_free (data->message);
  g_free (data);
}

@protocol CanSetAccessoryViewDisclosed
- (void)setAccessoryViewDisclosed:(BOOL)val;
@end

static gboolean
filechooser_quartz_launch (FileChooserQuartzData *data)
{

  if (data->save)
    {

      if (data->folder)
        {
          NSOpenPanel *panel = [[NSOpenPanel openPanel] retain];
          [panel setCanChooseDirectories:YES];
          [panel setCanChooseFiles:NO];
          [panel setCanCreateDirectories:YES];
          data->panel = panel;
        }
      else
        {
          NSSavePanel *panel = [[NSSavePanel savePanel] retain];
          if (data->create_folders)
            {
              [panel setCanCreateDirectories:YES];
            }
          else
            {
              [panel setCanCreateDirectories:NO];
            }
          data->panel = panel;
        }
    }
  else
    {
      NSOpenPanel *panel = [[NSOpenPanel openPanel] retain];

      if (data->select_multiple)
        {
          [panel setAllowsMultipleSelection:YES];
        }
      if (data->folder)
        {
          [panel setCanChooseDirectories:YES];
          [panel setCanChooseFiles:NO];
          if (data->create_folders)
            {
              [panel setCanCreateDirectories:YES];
            }
        }
      else
      {
        [panel setCanChooseDirectories:NO];
        [panel setCanChooseFiles:YES];
      }

      data->panel = panel;
  }

  [data->panel setReleasedWhenClosed:YES];

  if (data->accept_label)
    [data->panel setPrompt:[NSString stringWithUTF8String:data->accept_label]];

  if (data->title)
    [data->panel setTitle:[NSString stringWithUTF8String:data->title]];

  if (data->message)
    [data->panel setMessage:[NSString stringWithUTF8String:data->message]];

  if (data->current_file)
    {
      GFile *folder;
      char *name;

      folder = g_file_get_parent (data->current_file);
      name = g_file_get_basename (data->current_file);

      chooser_set_current_folder (data, folder);
      chooser_set_current_name (data, name);

      g_object_unref (folder);
      g_free (name);
    }

  if (data->current_folder)
    {
      chooser_set_current_folder (data, data->current_folder);
    }

  if (data->current_name)
    {
      chooser_set_current_name (data, data->current_name);
    }

  if (data->filters)
    {
      // when filters have been provided, a combobox needs to be added
      data->filter_popup_button = [[FilterComboBox alloc] initWithData:data];
      [data->filter_popup_button addItemsWithTitles:data->filter_names];

      if (data->self->current_filter)
        {
          GListModel *filters;
          guint i, n;
          guint current_filter_index = GTK_INVALID_LIST_POSITION;

          filters = gtk_file_chooser_get_filters (GTK_FILE_CHOOSER (data->self));
          n = g_list_model_get_n_items (filters);
          for (i = 0; i < n; i++)
            {
              gpointer item = g_list_model_get_item (filters, i);
              if (item == data->self->current_filter)
                {
                  g_object_unref (item);
                  current_filter_index = i;
                  break;
                }
              g_object_unref (item);
            }
          g_object_unref (filters);

          if (current_filter_index != GTK_INVALID_LIST_POSITION)
            [data->filter_popup_button selectItemAtIndex:current_filter_index];
          else
            [data->filter_popup_button selectItemAtIndex:0];
        }
      else
        {
          [data->filter_popup_button selectItemAtIndex:0];
        }

      [data->filter_popup_button popUpButtonSelectionChanged:NULL];
      [data->filter_popup_button setToolTip:[NSString stringWithUTF8String:_("Select which types of files are shown")]];
      [data->panel setAccessoryView:data->filter_popup_button];
      if ([data->panel isKindOfClass:[NSOpenPanel class]] && [data->panel respondsToSelector:@selector(setAccessoryViewDisclosed:)])
        {
          [(id<CanSetAccessoryViewDisclosed>) data->panel setAccessoryViewDisclosed:YES];
        }
    }
  data->response = GTK_RESPONSE_CANCEL;


  void (^handler)(NSInteger ret) = ^(NSInteger result) {

    if (result == NSModalResponseOK)
      {
        // get selected files and update data->files
        data->response = GTK_RESPONSE_ACCEPT;
	data->files = chooser_get_files (data);
      }

    GtkFileChooserNative *self = data->self;

    self->mode_data = NULL;

    if (data->parent)
      {
        [data->panel orderOut:nil];
        [data->parent makeKeyAndOrderFront:nil];
      }
    else
      {
        [data->key_window makeKeyAndOrderFront:nil];
      }

    /* Need to clear our cached copy of ordered windows */
    _gdk_macos_display_clear_sorting (GDK_MACOS_DISPLAY (gdk_display_get_default ()));

    if (!data->skip_response)
      {
        g_slist_free_full (self->custom_files, g_object_unref);
        self->custom_files = data->files;
        data->files = NULL;

        _gtk_native_dialog_emit_response (GTK_NATIVE_DIALOG (data->self),
                                          data->response);
      }
    // free data!
    filechooser_quartz_data_free (data);
  };

  if (data->parent != NULL && data->modal)
    {
      [data->panel beginSheetModalForWindow:data->parent completionHandler:handler];
    }
  else
    {
      [data->panel beginWithCompletionHandler:handler];
    }

  return TRUE;
}

static char *
strip_mnemonic (const char *s)
{
  char *escaped;
  char *ret = NULL;

  if (s == NULL)
    return NULL;

  escaped = g_markup_escape_text (s, -1);
  pango_parse_markup (escaped, -1, '_', NULL, &ret, NULL, NULL);

  if (ret != NULL)
    {
      return ret;
    }
  else
    {
      return g_strdup (s);
    }
}

static gboolean
file_filter_to_quartz (GtkFileFilter *file_filter,
                       NSMutableArray *filters,
		       NSMutableArray *filter_names)
{
  const char *name;
  NSArray *pattern_nsstrings;

  pattern_nsstrings = _gtk_file_filter_get_as_pattern_nsstrings (file_filter);
  if (pattern_nsstrings == NULL)
    return FALSE;

  name = gtk_file_filter_get_name (file_filter);
  NSString *name_nsstring;
  if (name == NULL)
    {
      name_nsstring = [pattern_nsstrings componentsJoinedByString:@","];;
    }
  else
    {
      name_nsstring = [NSString stringWithUTF8String:name];
      [name_nsstring retain];
    }

  [filter_names addObject:name_nsstring];
  [filters addObject:pattern_nsstrings];

  return TRUE;
}

gboolean
gtk_file_chooser_native_quartz_show (GtkFileChooserNative *self)
{
  FileChooserQuartzData *data;
  GtkWindow *transient_for;
  GtkFileChooserAction action;

  GListModel *filters;
  guint n_filters, i;
  char *message = NULL;

  data = g_new0 (FileChooserQuartzData, 1);

  // examine filters!
  filters = gtk_file_chooser_get_filters (GTK_FILE_CHOOSER (self));
  n_filters = g_list_model_get_n_items (filters);
  if (n_filters > 0)
    {
      data->filters = [NSMutableArray arrayWithCapacity:n_filters];
      [data->filters retain];
      data->filter_names = [NSMutableArray arrayWithCapacity:n_filters];
      [data->filter_names retain];

      for (i = 0; i < n_filters; i++)
        {
          GtkFileFilter *filter = g_list_model_get_item (filters, i);
          if (!file_filter_to_quartz (filter, data->filters, data->filter_names))
            {
              filechooser_quartz_data_free (data);
              g_object_unref (filter);
              g_object_unref (filters);
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

  data->create_folders = gtk_file_chooser_get_create_folders (GTK_FILE_CHOOSER (self));

  // shortcut_folder_uris support seems difficult if not impossible

  // mnemonics are not supported on macOS, so remove the underscores
  data->accept_label = strip_mnemonic (self->accept_label);
  // cancel button is not present in macOS filechooser dialogs!
  // data->cancel_label = strip_mnemonic (self->cancel_label);

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
      GtkNative *native = GTK_NATIVE (transient_for);
      GdkSurface *surface = gtk_native_get_surface (native);
      NSWindow *window = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (surface));

      gtk_widget_realize (GTK_WIDGET (transient_for));
      data->parent = window;

      if (gtk_native_dialog_get_modal (GTK_NATIVE_DIALOG (self)))
        data->modal = TRUE;
    }

  data->title =
    g_strdup (gtk_native_dialog_get_title (GTK_NATIVE_DIALOG (self)));

  data->message = message;

  if (self->current_file)
    data->current_file = g_object_ref (self->current_file);
  else
    {
      if (self->current_folder)
        data->current_folder = g_object_ref (self->current_folder);

      if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
        data->current_name = g_strdup (self->current_name);
    }

  data->key_window = [NSApp keyWindow];

  return filechooser_quartz_launch(data);
}

void
gtk_file_chooser_native_quartz_hide (GtkFileChooserNative *self)
{
  FileChooserQuartzData *data = self->mode_data;

  /* This is always set while dialog visible */
  g_assert (data != NULL);

  data->skip_response = TRUE;
  if (data->panel == NULL)
    return;

  [data->panel orderBack:nil];
  [data->panel close];

  if (data->parent)
    {
      [data->parent makeKeyAndOrderFront:nil];
    }
  else
    {
      [data->key_window makeKeyAndOrderFront:nil];
    }
  data->panel = NULL;
}
