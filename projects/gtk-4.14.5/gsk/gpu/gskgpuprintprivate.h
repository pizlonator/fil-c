#pragma once

#include "gskgputypesprivate.h"

#include "gskroundedrect.h"

#include <cairo.h>
#include <graphene.h>

void                    gsk_gpu_print_indent                            (GString                        *string,
                                                                         guint                           indent);
void                    gsk_gpu_print_op                                (GString                        *string,
                                                                         guint                           indent,
                                                                         const char                     *op_name);

void                    gsk_gpu_print_newline                           (GString                        *string);

void                    gsk_gpu_print_string                            (GString                        *string,
                                                                         const char                     *s);
void                    gsk_gpu_print_shader_info                       (GString                        *string,
                                                                         GskGpuShaderClip                clip);
void                    gsk_gpu_print_enum                              (GString                        *string,
                                                                         GType                           type,
                                                                         int                             value);
void                    gsk_gpu_print_rect                              (GString                        *string,
                                                                         const float                     rect[4]);
void                    gsk_gpu_print_int_rect                          (GString                        *string,
                                                                         const cairo_rectangle_int_t    *rect);
void                    gsk_gpu_print_rounded_rect                      (GString                        *string,
                                                                         const float                     rect[12]);
void                    gsk_gpu_print_rgba                              (GString                        *string,
                                                                         const float                     rgba[4]);
void                    gsk_gpu_print_image                             (GString                        *string,
                                                                         GskGpuImage                    *image);
void                    gsk_gpu_print_image_descriptor                  (GString                        *string,
                                                                         GskGpuDescriptors              *desc,
                                                                         guint32                         descriptor);
