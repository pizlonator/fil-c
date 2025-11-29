#pragma once

#include "gdksnapshot.h"

G_BEGIN_DECLS

#define GDK_SNAPSHOT_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_SNAPSHOT, GdkSnapshotClass))
#define GDK_IS_SNAPSHOT_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_SNAPSHOT))
#define GDK_SNAPSHOT_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_SNAPSHOT, GdkSnapshotClass))

struct _GdkSnapshot
{
  GObject parent_instance;
};

struct _GdkSnapshotClass {
  GObjectClass parent_class;
};

G_END_DECLS

