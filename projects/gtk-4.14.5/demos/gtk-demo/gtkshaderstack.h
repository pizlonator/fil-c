#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_SHADER_STACK     (gtk_shader_stack_get_type ())
G_DECLARE_FINAL_TYPE (GtkShaderStack, gtk_shader_stack, GTK, SHADER_STACK, GtkWidget)

GtkWidget * gtk_shader_stack_new          (void);
void        gtk_shader_stack_set_shader   (GtkShaderStack *self,
                                           GskGLShader    *shader);
void        gtk_shader_stack_add_child    (GtkShaderStack *self,
                                           GtkWidget      *child);
void        gtk_shader_stack_transition   (GtkShaderStack *self,
                                           gboolean        forward);
void        gtk_shader_stack_set_active   (GtkShaderStack *self,
                                           int             index);

G_END_DECLS
