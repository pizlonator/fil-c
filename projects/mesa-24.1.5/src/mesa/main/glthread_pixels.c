/*
 * Copyright 2024 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include "main/glthread_marshal.h"
#include "main/dispatch.h"
#include "main/image.h"

#define MAX_BITMAP_BYTE_SIZE     4096
#define MAX_DRAWPIX_BYTE_SIZE    4096

struct marshal_cmd_Bitmap
{
   struct marshal_cmd_base cmd_base;
   uint16_t num_slots;
   GLsizei width;
   GLsizei height;
   GLfloat xorig;
   GLfloat yorig;
   GLfloat xmove;
   GLfloat ymove;
   GLubyte *bitmap;
};

uint32_t
_mesa_unmarshal_Bitmap(struct gl_context *ctx,
                       const struct marshal_cmd_Bitmap *restrict cmd)
{
   CALL_Bitmap(ctx->Dispatch.Current,
               (cmd->width, cmd->height, cmd->xorig, cmd->yorig, cmd->xmove,
                cmd->ymove, cmd->bitmap));
   return cmd->num_slots;
}

void GLAPIENTRY
_mesa_marshal_Bitmap(GLsizei width, GLsizei height, GLfloat xorig,
                     GLfloat yorig, GLfloat xmove, GLfloat ymove,
                     const GLubyte *bitmap)
{
   GET_CURRENT_CONTEXT(ctx);
   int cmd_size = sizeof(struct marshal_cmd_Bitmap);

   /* If not building a display list... */
   if (!ctx->GLThread.ListMode) {
      /* PBO path or bitmap == NULL (which means xmove/ymove only move the raster
       * pos.
       */
      if (!bitmap || _mesa_glthread_has_unpack_buffer(ctx)) {
         struct marshal_cmd_Bitmap *cmd =
            _mesa_glthread_allocate_command(ctx, DISPATCH_CMD_Bitmap,
                                            cmd_size);
         cmd->num_slots = align(cmd_size, 8) / 8;
         cmd->width = width;
         cmd->height = height;
         cmd->xorig = xorig;
         cmd->yorig = yorig;
         cmd->xmove = xmove;
         cmd->ymove = ymove;
         cmd->bitmap = (GLubyte *)bitmap;
         return;
      }

      size_t bitmap_size =
         (size_t)_mesa_image_row_stride(&ctx->GLThread.Unpack, width,
                                        GL_COLOR_INDEX, GL_BITMAP) * height;

      /* If the bitmap is small enough, copy it into the batch. */
      if (bitmap_size <= MAX_BITMAP_BYTE_SIZE) {
         struct marshal_cmd_Bitmap *cmd =
            _mesa_glthread_allocate_command(ctx, DISPATCH_CMD_Bitmap,
                                            cmd_size + bitmap_size);
         cmd->num_slots = align(cmd_size + bitmap_size, 8) / 8;
         cmd->width = width;
         cmd->height = height;
         cmd->xorig = xorig;
         cmd->yorig = yorig;
         cmd->xmove = xmove;
         cmd->ymove = ymove;
         cmd->bitmap = (GLubyte *)(cmd + 1);
         memcpy(cmd->bitmap, bitmap, bitmap_size);
         return;
      }
   }

   _mesa_glthread_finish_before(ctx, "Bitmap");
   CALL_Bitmap(ctx->Dispatch.Current,
               (width, height, xorig, yorig, xmove, ymove, bitmap));
}

struct marshal_cmd_DrawPixels
{
   struct marshal_cmd_base cmd_base;
   uint16_t num_slots;
   GLenum16 format;
   GLenum16 type;
   GLsizei width;
   GLsizei height;
   GLvoid *pixels;
};

uint32_t
_mesa_unmarshal_DrawPixels(struct gl_context *ctx,
                           const struct marshal_cmd_DrawPixels *restrict cmd)
{
   CALL_DrawPixels(ctx->Dispatch.Current,
                   (cmd->width, cmd->height, cmd->format, cmd->type,
                    cmd->pixels));
   return cmd->num_slots;
}

void GLAPIENTRY
_mesa_marshal_DrawPixels(GLsizei width, GLsizei height, GLenum format,
                         GLenum type, const GLvoid *pixels)
{
   GET_CURRENT_CONTEXT(ctx);
   int cmd_size = sizeof(struct marshal_cmd_DrawPixels);

   /* If not building a display list... */
   if (!ctx->GLThread.ListMode) {
      /* PBO */
      if (_mesa_glthread_has_unpack_buffer(ctx)) {
         struct marshal_cmd_DrawPixels *cmd =
            _mesa_glthread_allocate_command(ctx, DISPATCH_CMD_DrawPixels,
                                            cmd_size);
         cmd->num_slots = align(cmd_size, 8) / 8;
         cmd->format = MIN2(format, 0xffff); /* clamped to 0xffff (invalid enum) */
         cmd->type = MIN2(type, 0xffff); /* clamped to 0xffff (invalid enum) */
         cmd->width = width;
         cmd->height = height;
         cmd->pixels = (void *)pixels;
         return;
      }

      /* A negative stride is unimplemented (it inverts the offset). */
      if (!ctx->Unpack.Invert) {
         size_t image_size =
            (size_t)_mesa_image_row_stride(&ctx->GLThread.Unpack,
                                           width, format, type) * height;

         /* If the image is small enough, copy it into the batch. */
         if (image_size <= MAX_DRAWPIX_BYTE_SIZE) {
            struct marshal_cmd_DrawPixels *cmd =
               _mesa_glthread_allocate_command(ctx, DISPATCH_CMD_DrawPixels,
                                               cmd_size + image_size);
            cmd->num_slots = align(cmd_size + image_size, 8) / 8;
            cmd->format = MIN2(format, 0xffff); /* clamped to 0xffff (invalid enum) */
            cmd->type = MIN2(type, 0xffff); /* clamped to 0xffff (invalid enum) */
            cmd->width = width;
            cmd->height = height;
            cmd->pixels = cmd + 1;
            memcpy(cmd->pixels, pixels, image_size);
            return;
         }
      }
   }

   _mesa_glthread_finish_before(ctx, "DrawPixels");
   CALL_DrawPixels(ctx->Dispatch.Current,
                   (width, height, format, type, pixels));
}
