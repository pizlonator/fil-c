/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Red Hat, Inc.
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

#include "reftest-module.h"

struct _ReftestModule {
  int      refcount;
  char    *filename;
  GModule *module;
};


static GHashTable *all_modules = NULL;

static ReftestModule *
reftest_module_find_existing (const char *filename)
{
  if (all_modules == NULL)
    return NULL;

  return g_hash_table_lookup (all_modules, filename ? filename : "");
}

static ReftestModule *
reftest_module_new_take (GModule *module,
                         char    *filename)
{
  ReftestModule *result;

  g_return_val_if_fail (module != NULL, NULL);

  result = g_new0 (ReftestModule, 1);

  result->refcount = 1;
  result->filename = filename;
  result->module = module;

  if (all_modules == NULL)
    all_modules = g_hash_table_new (g_str_hash, g_str_equal);

  g_hash_table_insert (all_modules, filename ? filename : g_strdup (""), result);

  return result;
}

ReftestModule *
reftest_module_new_self (void)
{
  ReftestModule *result;
  GModule *module;

  result = reftest_module_find_existing (NULL);
  if (result)
    return reftest_module_ref (result);

  module = g_module_open (NULL, G_MODULE_BIND_LAZY);
  if (module == NULL)
    return NULL;

  return reftest_module_new_take (module, NULL);
}

ReftestModule *
reftest_module_new (const char *directory,
                    const char *module_name)
{
  ReftestModule *result;
  char *full_path;
  GModule *module;

  g_return_val_if_fail (module_name != NULL, NULL);

  full_path = g_module_build_path (directory, module_name);

  result = reftest_module_find_existing (full_path);
  if (result)
    {
      g_free (full_path);
      return reftest_module_ref (result);
    }

  module = g_module_open (full_path, G_MODULE_BIND_LOCAL | G_MODULE_BIND_LAZY);
  if (module == NULL)
    {
      g_free (full_path);
      return NULL;
    }

  return reftest_module_new_take (module, full_path);
}

ReftestModule *
reftest_module_ref (ReftestModule *module)
{
  g_return_val_if_fail (module != NULL, NULL);

  module->refcount++;

  return module;
}

void
reftest_module_unref (ReftestModule *module)
{
  g_return_if_fail (module != NULL);

  module->refcount--;
  if (module->refcount > 0)
    return;

  if (!g_module_close (module->module))
    {
      g_assert_not_reached ();
    }

  if (!g_hash_table_remove (all_modules, module->filename ? module->filename : ""))
    {
      g_assert_not_reached ();
    }

  g_free (module->filename);
  g_free (module);
}

GCallback
reftest_module_lookup (ReftestModule *module,
                       const char    *function_name)
{
  gpointer result;

  g_return_val_if_fail (module != NULL, NULL);
  g_return_val_if_fail (function_name != NULL, NULL);

  if (!g_module_symbol (module->module, function_name, &result))
    return NULL;

  return result;
}

