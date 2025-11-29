#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_SHADER_BIN      (gtk_shader_bin_get_type ())
G_DECLARE_FINAL_TYPE (GtkShaderBin, gtk_shader_bin, GTK, SHADER_BIN, GtkWidget)

GtkWidget  *gtk_shader_bin_new        (void);
void        gtk_shader_bin_add_shader (GtkShaderBin *self,
                                       GskGLShader  *shader,
                                       GtkStateFlags state,
                                       GtkStateFlags state_mask,
                                       float         extra_border);
void       gtk_shader_bin_set_child   (GtkShaderBin *self,
                                       GtkWidget    *child);
GtkWidget *gtk_shader_bin_get_child   (GtkShaderBin *self);

G_END_DECLS
