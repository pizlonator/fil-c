
#pragma once

#include "gtkwidget.h"
#include "gtkenums.h"

#define GTK_TYPE_PANED_HANDLE                 (gtk_paned_handle_get_type ())
#define GTK_PANED_HANDLE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PANED_HANDLE, GtkPanedHandle))
#define GTK_PANED_HANDLE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PANED_HANDLE, GtkPanedHandleClass))
#define GTK_IS_PANED_HANDLE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PANED_HANDLE))
#define GTK_IS_PANED_HANDLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PANED_HANDLE))
#define GTK_PANED_HANDLE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PANED_HANDLE, GtkPanedHandleClass))

typedef struct _GtkPanedHandle             GtkPanedHandle;
typedef struct _GtkPanedHandleClass        GtkPanedHandleClass;

struct _GtkPanedHandle
{
  GtkWidget parent_instance;
};

struct _GtkPanedHandleClass
{
  GtkWidgetClass parent_class;
};

GType      gtk_paned_handle_get_type (void) G_GNUC_CONST;

GtkWidget *gtk_paned_handle_new (void);

