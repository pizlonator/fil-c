#include "config.h"

#include "gskgpurendererprivate.h"

#include "gskdebugprivate.h"
#include "gskgpudeviceprivate.h"
#include "gskgpuframeprivate.h"
#include "gskprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"
#include "gskgpuimageprivate.h"

#include "gdk/gdkdebugprivate.h"
#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkdmabuftextureprivate.h"
#include "gdk/gdkdrawcontextprivate.h"
#include "gdk/gdkprofilerprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdktexturedownloaderprivate.h"
#include "gdk/gdkdrawcontextprivate.h"

#include <graphene.h>

#define GSK_GPU_MAX_FRAMES 4

static const GdkDebugKey gsk_gpu_optimization_keys[] = {
  { "uber", GSK_GPU_OPTIMIZE_UBER, "Don't use the uber shader" },
  { "clear", GSK_GPU_OPTIMIZE_CLEAR, "Use shaders instead of vkCmdClearAttachment()/glClear()" },
  { "merge", GSK_GPU_OPTIMIZE_MERGE, "Use one vkCmdDraw()/glDrawArrays() per operation" },
  { "blit", GSK_GPU_OPTIMIZE_BLIT, "Use shaders instead of vkCmdBlit()/glBlitFramebuffer()" },
  { "gradients", GSK_GPU_OPTIMIZE_GRADIENTS, "Don't supersample gradients" },
  { "mipmap", GSK_GPU_OPTIMIZE_MIPMAP, "Avoid creating mipmaps" },
};

typedef struct _GskGpuRendererPrivate GskGpuRendererPrivate;

struct _GskGpuRendererPrivate
{
  GskGpuDevice *device;
  GdkDrawContext *context;
  GskGpuOptimizations optimizations;

  GskGpuFrame *frames[GSK_GPU_MAX_FRAMES];
};

static void     gsk_gpu_renderer_dmabuf_downloader_init         (GdkDmabufDownloaderInterface   *iface);

G_DEFINE_TYPE_EXTENDED (GskGpuRenderer, gsk_gpu_renderer, GSK_TYPE_RENDERER, 0,
                        G_ADD_PRIVATE (GskGpuRenderer)
                        G_IMPLEMENT_INTERFACE (GDK_TYPE_DMABUF_DOWNLOADER,
                                               gsk_gpu_renderer_dmabuf_downloader_init))

static void
gsk_gpu_renderer_make_current (GskGpuRenderer *self)
{
  GSK_GPU_RENDERER_GET_CLASS (self)->make_current (self);
}

static GskGpuFrame *
gsk_gpu_renderer_create_frame (GskGpuRenderer *self)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  GskGpuRendererClass *klass = GSK_GPU_RENDERER_GET_CLASS (self);
  GskGpuFrame *result;

  result = g_object_new (klass->frame_type, NULL);

  gsk_gpu_frame_setup (result, self, priv->device, priv->optimizations);

  return result;
}

static void
gsk_gpu_renderer_dmabuf_downloader_close (GdkDmabufDownloader *downloader)
{
  gsk_renderer_unrealize (GSK_RENDERER (downloader));
}

static gboolean
gsk_gpu_renderer_dmabuf_downloader_supports (GdkDmabufDownloader  *downloader,
                                             GdkDmabufTexture     *texture,
                                             GError              **error)
{
  GskGpuRenderer *self = GSK_GPU_RENDERER (downloader);
  const GdkDmabuf *dmabuf;
  GdkDmabufFormats *formats;

  dmabuf = gdk_dmabuf_texture_get_dmabuf (texture);

  formats = GSK_GPU_RENDERER_GET_CLASS (self)->get_dmabuf_formats (self);

  if (!gdk_dmabuf_formats_contains (formats, dmabuf->fourcc, dmabuf->modifier))
    {
      g_set_error (error,
                   GDK_DMABUF_ERROR, GDK_DMABUF_ERROR_UNSUPPORTED_FORMAT,
                   "Unsupported dmabuf format: %.4s:%#" G_GINT64_MODIFIER "x",
                   (char *) &dmabuf->fourcc, dmabuf->modifier);
      return FALSE;
    }

  return TRUE;
}

static void
gsk_gpu_renderer_dmabuf_downloader_download (GdkDmabufDownloader *downloader,
                                             GdkDmabufTexture    *texture,
                                             GdkMemoryFormat      format,
                                             guchar              *data,
                                             gsize                stride)
{
  GskGpuRenderer *self = GSK_GPU_RENDERER (downloader);
  GskGpuFrame *frame;

  gsk_gpu_renderer_make_current (self);

  frame = gsk_gpu_renderer_create_frame (self);

  gsk_gpu_frame_download_texture (frame,
                                  g_get_monotonic_time (),
                                  GDK_TEXTURE (texture),
                                  format,
                                  data,
                                  stride);

  g_object_unref (frame);
}

static void
gsk_gpu_renderer_dmabuf_downloader_init (GdkDmabufDownloaderInterface *iface)
{
  iface->close = gsk_gpu_renderer_dmabuf_downloader_close;
  iface->supports = gsk_gpu_renderer_dmabuf_downloader_supports;
  iface->download = gsk_gpu_renderer_dmabuf_downloader_download;
}

static cairo_region_t *
get_render_region (GskGpuRenderer *self)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  const cairo_region_t *damage;
  cairo_region_t *scaled_damage;
  double scale;

  scale = gsk_gpu_renderer_get_scale (self);

  damage = gdk_draw_context_get_frame_region (priv->context);
  scaled_damage = cairo_region_create ();
  for (int i = 0; i < cairo_region_num_rectangles (damage); i++)
    {
      cairo_rectangle_int_t rect;
      cairo_region_get_rectangle (damage, i, &rect);
      cairo_region_union_rectangle (scaled_damage, &(cairo_rectangle_int_t) {
                                      .x = (int) floor (rect.x * scale),
                                      .y = (int) floor (rect.y * scale),
                                      .width = (int) ceil ((rect.x + rect.width) * scale) - floor (rect.x * scale),
                                      .height = (int) ceil ((rect.y + rect.height) * scale) - floor (rect.y * scale),
                                    });
    }

  return scaled_damage;
}

static GskGpuFrame *
gsk_gpu_renderer_get_frame (GskGpuRenderer *self)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  GskGpuFrame *earliest_frame = NULL;
  gint64 earliest_time = G_MAXINT64;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (priv->frames); i++)
    {
      gint64 timestamp;

      if (priv->frames[i] == NULL)
        {
          priv->frames[i] = gsk_gpu_renderer_create_frame (self);
          return priv->frames[i];
        }

      if (!gsk_gpu_frame_is_busy (priv->frames[i]))
        return priv->frames[i];

      timestamp = gsk_gpu_frame_get_timestamp (priv->frames[i]);
      if (timestamp < earliest_time)
        {
          earliest_time = timestamp;
          earliest_frame = priv->frames[i];
        }
    }

  g_assert (earliest_frame);

  gsk_gpu_frame_wait (earliest_frame);

  return earliest_frame;
}

static gboolean
gsk_gpu_renderer_realize (GskRenderer  *renderer,
                          GdkDisplay   *display,
                          GdkSurface   *surface,
                          GError      **error)
{
  GskGpuRenderer *self = GSK_GPU_RENDERER (renderer);
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  GskGpuOptimizations context_optimizations;

  priv->device = GSK_GPU_RENDERER_GET_CLASS (self)->get_device (display, error);
  if (priv->device == NULL)
    return FALSE;

  priv->context = GSK_GPU_RENDERER_GET_CLASS (self)->create_context (self, display, surface, &context_optimizations, error);
  if (priv->context == NULL)
    {
      g_clear_object (&priv->device);
      return FALSE;
    }

  priv->optimizations &= context_optimizations;

  return TRUE;
}

static void
gsk_gpu_renderer_unrealize (GskRenderer *renderer)
{
  GskGpuRenderer *self = GSK_GPU_RENDERER (renderer);
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  gsize i;

  gsk_gpu_renderer_make_current (self);

  for (i = 0; i < G_N_ELEMENTS (priv->frames); i++)
    {
      if (priv->frames[i] == NULL)
        break;
      if (gsk_gpu_frame_is_busy (priv->frames[i]))
        gsk_gpu_frame_wait (priv->frames[i]);
      g_clear_object (&priv->frames[i]);
    }

  g_clear_object (&priv->context);
  g_clear_object (&priv->device);
}

static GdkTexture *
gsk_gpu_renderer_fallback_render_texture (GskGpuRenderer        *self,
                                          GskRenderNode         *root,
                                          const graphene_rect_t *rounded_viewport)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  GskGpuImage *image;
  gsize width, height, max_size, image_width, image_height;
  gsize x, y, size, bpp, stride;
  GdkMemoryFormat format;
  GdkMemoryDepth depth;
  GBytes *bytes;
  guchar *data;
  GdkTexture *texture;
  GdkTextureDownloader downloader;
  GskGpuFrame *frame;

  max_size = gsk_gpu_device_get_max_image_size (priv->device);
  depth = gsk_render_node_get_preferred_depth (root);
  do
    {
      image = gsk_gpu_device_create_download_image (priv->device,
                                                    gsk_render_node_get_preferred_depth (root),
                                                    MIN (max_size, rounded_viewport->size.width),
                                                    MIN (max_size, rounded_viewport->size.height));
      max_size /= 2;
    }
  while (image == NULL);

  format = gsk_gpu_image_get_format (image);
  bpp = gdk_memory_format_bytes_per_pixel (format);
  image_width = gsk_gpu_image_get_width (image);
  image_height = gsk_gpu_image_get_height (image);
  width = rounded_viewport->size.width;
  height = rounded_viewport->size.height;
  stride = width * bpp;
  size = stride * height;
  data = g_malloc_n (stride, height);

  for (y = 0; y < height; y += image_height)
    {
      for (x = 0; x < width; x += image_width)
        {
          texture = NULL;
          if (image == NULL)
            image = gsk_gpu_device_create_download_image (priv->device,
                                                          depth,
                                                          MIN (image_width, width - x),
                                                          MIN (image_height, height - y));

          frame = gsk_gpu_renderer_create_frame (self);
          gsk_gpu_frame_render (frame,
                                g_get_monotonic_time (),
                                image,
                                NULL,
                                root,
                                &GRAPHENE_RECT_INIT (rounded_viewport->origin.x + x,
                                                     rounded_viewport->origin.y + y,
                                                     image_width,
                                                     image_height),
                                &texture);
          g_object_unref (frame);

          g_assert (texture);
          gdk_texture_downloader_init (&downloader, texture);
          gdk_texture_downloader_set_format (&downloader, format);
          gdk_texture_downloader_download_into (&downloader,
                                                data + stride * y + x * bpp,
                                                stride);

          gdk_texture_downloader_finish (&downloader);
          g_object_unref (texture);
          g_clear_object (&image);
        }
    }

  bytes = g_bytes_new_take (data, size);
  texture = gdk_memory_texture_new (width, height, GDK_MEMORY_DEFAULT, bytes, stride);
  g_bytes_unref (bytes);
  return texture;
}

static GdkTexture *
gsk_gpu_renderer_render_texture (GskRenderer           *renderer,
                                 GskRenderNode         *root,
                                 const graphene_rect_t *viewport)
{
  GskGpuRenderer *self = GSK_GPU_RENDERER (renderer);
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  GskGpuFrame *frame;
  GskGpuImage *image;
  GdkTexture *texture;
  graphene_rect_t rounded_viewport;

  gsk_gpu_device_maybe_gc (priv->device);

  gsk_gpu_renderer_make_current (self);

  rounded_viewport = GRAPHENE_RECT_INIT (viewport->origin.x,
                                         viewport->origin.y,
                                         ceil (viewport->size.width),
                                         ceil (viewport->size.height));
  image = gsk_gpu_device_create_download_image (priv->device,
                                                gsk_render_node_get_preferred_depth (root),
                                                rounded_viewport.size.width,
                                                rounded_viewport.size.height);

  if (image == NULL)
    return gsk_gpu_renderer_fallback_render_texture (self, root, &rounded_viewport);

  frame = gsk_gpu_renderer_create_frame (self);

  texture = NULL;
  gsk_gpu_frame_render (frame,
                        g_get_monotonic_time (),
                        image,
                        NULL,
                        root,
                        &rounded_viewport,
                        &texture);

  g_object_unref (frame);
  g_object_unref (image);

  gsk_gpu_device_queue_gc (priv->device);

  /* check that callback setting texture was actually called, as its technically async */
  g_assert (texture);

  return texture;
}

static void
gsk_gpu_renderer_render (GskRenderer          *renderer,
                         GskRenderNode        *root,
                         const cairo_region_t *region)
{
  GskGpuRenderer *self = GSK_GPU_RENDERER (renderer);
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  GskGpuFrame *frame;
  GskGpuImage *backbuffer;
  cairo_region_t *render_region;
  double scale;

  if (cairo_region_is_empty (region))
    {
      gdk_draw_context_empty_frame (priv->context);
      return;
    }

  gdk_draw_context_begin_frame_full (priv->context,
                                     gsk_render_node_get_preferred_depth (root),
                                     region);

  gsk_gpu_device_maybe_gc (priv->device);

  gsk_gpu_renderer_make_current (self);

  backbuffer = GSK_GPU_RENDERER_GET_CLASS (self)->get_backbuffer (self);

  frame = gsk_gpu_renderer_get_frame (self);
  render_region = get_render_region (self);
  scale = gsk_gpu_renderer_get_scale (self);

  gsk_gpu_frame_render (frame,
                        g_get_monotonic_time (),
                        backbuffer,
                        render_region,
                        root,
                        &GRAPHENE_RECT_INIT (
                          0, 0,
                          gsk_gpu_image_get_width (backbuffer) / scale,
                          gsk_gpu_image_get_height (backbuffer) / scale
                        ),
                        NULL);

  gsk_gpu_device_queue_gc (priv->device);

  gdk_draw_context_end_frame (priv->context);

  g_clear_pointer (&render_region, cairo_region_destroy);
}

static double
gsk_gpu_renderer_real_get_scale (GskGpuRenderer *self)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);
  GdkSurface *surface;

  surface = gdk_draw_context_get_surface (priv->context);

  return gdk_surface_get_scale (surface);
}

static void
gsk_gpu_renderer_class_init (GskGpuRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->supports_offload = TRUE;

  renderer_class->realize = gsk_gpu_renderer_realize;
  renderer_class->unrealize = gsk_gpu_renderer_unrealize;
  renderer_class->render = gsk_gpu_renderer_render;
  renderer_class->render_texture = gsk_gpu_renderer_render_texture;

  gsk_ensure_resources ();

  klass->optimizations = -1;
  klass->optimizations &= ~gdk_parse_debug_var ("GSK_GPU_DISABLE",
                                                gsk_gpu_optimization_keys,
                                                G_N_ELEMENTS (gsk_gpu_optimization_keys));
  klass->get_scale = gsk_gpu_renderer_real_get_scale;
}

static void
gsk_gpu_renderer_init (GskGpuRenderer *self)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);

  priv->optimizations = GSK_GPU_RENDERER_GET_CLASS (self)->optimizations;
}

GdkDrawContext *
gsk_gpu_renderer_get_context (GskGpuRenderer *self)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);

  return priv->context;
}

GskGpuDevice *
gsk_gpu_renderer_get_device (GskGpuRenderer *self)
{
  GskGpuRendererPrivate *priv = gsk_gpu_renderer_get_instance_private (self);

  return priv->device;
}

double
gsk_gpu_renderer_get_scale (GskGpuRenderer *self)
{
  return GSK_GPU_RENDERER_GET_CLASS (self)->get_scale (self);
}
