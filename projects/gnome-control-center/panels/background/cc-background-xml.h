/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003-2012 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define CC_TYPE_BACKGROUND_XML (cc_background_xml_get_type ())
G_DECLARE_FINAL_TYPE (CcBackgroundXml, cc_background_xml, CC, BACKGROUND_XML, GObject)

CcBackgroundXml *cc_background_xml_new (void);

void cc_background_xml_save                          (CcBackgroundItem   *item,
						      const char         *filename);

CcBackgroundItem *cc_background_xml_get_item         (const char         *filename);
gboolean cc_background_xml_load_xml                  (CcBackgroundXml    *data,
						      const char         *filename);
void cc_background_xml_load_list_async               (CcBackgroundXml    *xml,
						      GCancellable       *cancellable,
						      GAsyncReadyCallback callback,
						      gpointer            user_data);
gboolean cc_background_xml_load_list_finish          (CcBackgroundXml    *xml,
						      GAsyncResult       *result,
						      GError            **error);

G_END_DECLS
