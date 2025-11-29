#pragma once

#include <gtk/gtk.h>

#define RESOURCE_TYPE_HOLDER (resource_holder_get_type ())

G_DECLARE_FINAL_TYPE (ResourceHolder, resource_holder, RESOURCE, HOLDER, GObject)

ResourceHolder * resource_holder_new         (const char *name,
                                              const char *path,
                                              int         count,
                                              gsize       size,
                                              GListModel *children);

const char *resource_holder_get_name     (ResourceHolder *holder);
const char *resource_holder_get_path     (ResourceHolder *holder);
int         resource_holder_get_count    (ResourceHolder *holder);
gsize       resource_holder_get_size     (ResourceHolder *holder);
GListModel *resource_holder_get_children (ResourceHolder *holder);
ResourceHolder *resource_holder_get_parent   (ResourceHolder *holder);


