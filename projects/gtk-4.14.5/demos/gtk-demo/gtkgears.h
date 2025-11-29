#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

enum {
  GTK_GEARS_X_AXIS,
  GTK_GEARS_Y_AXIS,
  GTK_GEARS_Z_AXIS,

  GTK_GEARS_N_AXIS
};

#define GTK_TYPE_GEARS      (gtk_gears_get_type ())
#define GTK_GEARS(inst)     (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                             GTK_TYPE_GEARS,             \
                             GtkGears))
#define GTK_IS_GEARS(inst)  (G_TYPE_CHECK_INSTANCE_TYPE ((inst), \
                             GTK_TYPE_GEARS))

typedef struct _GtkGears GtkGears;
typedef struct _GtkGearsClass GtkGearsClass;

struct _GtkGears {
  GtkGLArea parent;
};

struct _GtkGearsClass {
  GtkGLAreaClass parent_class;
};

GType      gtk_gears_get_type      (void) G_GNUC_CONST;

GtkWidget *gtk_gears_new           (void);
void       gtk_gears_set_axis      (GtkGears *gears,
                                    int       axis,
                                    double    value);
double     gtk_gears_get_axis      (GtkGears *gears,
                                    int       axis);
void       gtk_gears_set_fps_label (GtkGears *gears,
                                    GtkLabel *label);


G_END_DECLS
