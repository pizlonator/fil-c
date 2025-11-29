#pragma once

#include "gtkbuildable.h"

G_BEGIN_DECLS

void      gtk_buildable_set_buildable_id       (GtkBuildable        *buildable,
                                                const char          *id);
void      gtk_buildable_add_child              (GtkBuildable        *buildable,
                                                GtkBuilder          *builder,
                                                GObject             *child,
                                                const char          *type);
GObject * gtk_buildable_construct_child        (GtkBuildable        *buildable,
                                                GtkBuilder          *builder,
                                                const char          *name);
gboolean  gtk_buildable_custom_tag_start       (GtkBuildable        *buildable,
                                                GtkBuilder          *builder,
                                                GObject             *child,
                                                const char          *tagname,
                                                GtkBuildableParser  *parser,
                                                gpointer            *data);
void      gtk_buildable_custom_tag_end         (GtkBuildable        *buildable,
                                                GtkBuilder          *builder,
                                                GObject             *child,
                                                const char          *tagname,
                                                gpointer             data);
void      gtk_buildable_custom_finished        (GtkBuildable        *buildable,
                                                GtkBuilder          *builder,
                                                GObject             *child,
                                                const char          *tagname,
                                                gpointer             data);
void      gtk_buildable_parser_finished        (GtkBuildable        *buildable,
                                                GtkBuilder          *builder);
GObject * gtk_buildable_get_internal_child     (GtkBuildable        *buildable,
                                                GtkBuilder          *builder,
                                                const char          *childname);

G_END_DECLS

