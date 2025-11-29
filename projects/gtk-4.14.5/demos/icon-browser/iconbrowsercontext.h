#pragma once

#include <gtk.h>

#define IB_TYPE_CONTEXT (ib_context_get_type ())
G_DECLARE_FINAL_TYPE (IbContext, ib_context, IB, CONTEXT, GObject)

IbContext   *ib_context_new             (const char     *id,
                                         const char     *name,
                                         const char     *description);

const char *ib_context_get_id           (IbContext      *context);
const char *ib_context_get_name         (IbContext      *context);
const char *ib_context_get_description  (IbContext      *context);
