/*
 * Copyright (c) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include <gtk/gtk.h>

#include <shell/cc-shell-model.h>
#include "cc-search-provider.h"

G_BEGIN_DECLS

typedef struct {
  GtkApplication parent;

  CcShellModel     *model;
  CcSearchProvider *search_provider;
} CcSearchProviderApp;

typedef struct {
  GtkApplicationClass parent_class;
} CcSearchProviderAppClass;

#define CC_TYPE_SEARCH_PROVIDER_APP cc_search_provider_app_get_type ()

#define CC_SEARCH_PROVIDER_APP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_SEARCH_PROVIDER_APP, CcSearchProviderApp))

GType cc_search_provider_app_get_type (void) G_GNUC_CONST;

CcSearchProviderApp *cc_search_provider_app_get (void);

CcShellModel *cc_search_provider_app_get_model (CcSearchProviderApp *application);

G_END_DECLS
