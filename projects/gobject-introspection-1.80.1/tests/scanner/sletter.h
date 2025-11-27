#ifndef __SLETTER_H__
#define __SLETTER_H__

#include <gio/gio.h>

#include "gitestmacros.h"

typedef struct {
  double x;
  double y;
} SPoint;

_GI_TEST_EXTERN
void s_hello (void);

/* Like GSpawnError; not registered with GType */
typedef enum
{
  S_SPAWN_ERROR_CODE1 = 1,
  S_SPAWN_ERROR_CODE2 = 2,
  S_SPAWN_ERROR_CODE3 = 3
} SSpawnError;

_GI_TEST_EXTERN
GQuark s_spawn_error_quark (void);

/* Like GDBusError but not registered with GType */
typedef enum
{
  S_DBUS_ERROR_CODE1 = 1,
  S_DBUS_ERROR_CODE2 = 2,
  S_DBUS_ERROR_CODE3 = 3
} SDBusError;

_GI_TEST_EXTERN
GQuark s_dbus_error_quark (void);

#endif
