/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* This file gets installed, so we can't assume config.h is available */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "utility.h"

G_DEFINE_TYPE (UtilityObject, utility_object, G_TYPE_OBJECT);

/**
 * UtilityBuffer:
 * @data: (type gpointer): the data
 *
 **/

static void
utility_object_class_init (UtilityObjectClass *klass)
{

}

static void
utility_object_init (UtilityObject *object)
{

}

void
utility_object_watch_dir (UtilityObject *object,
                          const char *path,
                          UtilityFileFunc func,
                          gpointer user_data,
                          GDestroyNotify destroy)
{

}

/**
 * utility_dir_foreach:
 * @path::
 * @func: (scope call):
 * @user_data::
 *
 */
void
utility_dir_foreach (const char *path, UtilityFileFunc func, gpointer user_data)
{

}
