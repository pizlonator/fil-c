#pragma once

void do_info      (int *argc, const char ***argv);
void do_decompose (int *argc, const char ***argv);
void do_restrict  (int *argc, const char ***argv);
void do_reverse   (int *argc, const char ***argv);
void do_render    (int *argc, const char ***argv);
void do_show      (int *argc, const char ***argv);

GskPath *get_path       (const char *arg);
int      get_enum_value (GType       type,
                         const char *type_nick,
                         const char *str);
void     get_color      (GdkRGBA    *rgba,
                         const char *str);

void     _gsk_stroke_set_dashes (GskStroke  *stroke,
                                 const char *dashes);
