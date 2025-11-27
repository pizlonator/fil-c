/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef __WARNLIB_H__
#define __WARNLIB_H__

#include <gio/gio.h>

#include "gitestmacros.h"

#define WARNLIB_UNPAIRED_ERROR (warnlib_unpaired_error_quark ())
_GI_TEST_EXTERN
GQuark warnlib_unpaired_error_quark (void);

_GI_TEST_EXTERN
gboolean warnlib_throw_unpaired (GError **error);

/* interface */
#define WARNLIB_TYPE_WHATEVER              (warnlib_whatever_get_type ())
#define WARNLIB_WHATEVER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), WARNLIB_TYPE_WHATEVER, WarnLibWhatever))
#define WARNLIB_IS_WHATEVER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), WARNLIB_TYPE_WHATEVER))
#define WARNLIB_WHATEVER_GET_IFACE(obj)    (G_TYPE_INSTANCE_GET_INTERFACE ((obj), WARNLIB_TYPE_WHATEVER, WarnLibWhateverIface))

typedef struct _WarnLibWhateverIface WarnLibWhateverIface;
typedef struct _WarnLibWhatever WarnLibWhatever;

struct _WarnLibWhateverIface
{
  GTypeInterface parent_iface;

  /* virtual table */

  /* explicitly test un-named parameters */
  void (*do_moo) (WarnLibWhatever *self, int, gpointer);

  void (*do_boo) (WarnLibWhatever *self, int x, gpointer y);
};

_GI_TEST_EXTERN
void warnlib_whatever_do_moo (WarnLibWhatever *self, int, gpointer);
_GI_TEST_EXTERN
void warnlib_whatever_do_boo (WarnLibWhatever *self, int, gpointer);

_GI_TEST_EXTERN
GType warnlib_whatever_get_type (void) G_GNUC_CONST;

#endif
