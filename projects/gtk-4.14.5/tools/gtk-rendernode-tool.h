
#pragma once

void do_benchmark   (int *argc, const char ***argv);
void do_compare     (int *argc, const char ***argv);
void do_info        (int *argc, const char ***argv);
void do_show        (int *argc, const char ***argv);
void do_render      (int *argc, const char ***argv);

GskRenderNode *load_node_file (const char *filename);
GskRenderer   *create_renderer (const char *name, GError **error);
