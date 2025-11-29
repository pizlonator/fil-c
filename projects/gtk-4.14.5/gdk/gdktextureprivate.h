#pragma once

#include "gdktexture.h"

#include "gdkenums.h"

G_BEGIN_DECLS

typedef struct _GdkTextureChain GdkTextureChain;

struct _GdkTextureChain
{
  gatomicrefcount ref_count;
  GMutex lock;
};

#define GDK_TEXTURE_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_TEXTURE, GdkTextureClass))
#define GDK_IS_TEXTURE_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_TEXTURE))
#define GDK_TEXTURE_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_TEXTURE, GdkTextureClass))

struct _GdkTexture
{
  GObject parent_instance;

  GdkMemoryFormat format;
  int width;
  int height;

  gpointer render_key;
  gpointer render_data;
  GDestroyNotify render_notify;

  /* for diffing swapchain-like textures.
   * Textures in the same chain are connected in a double linked list which is
   * protected using the chain's shared mutex.
   */
  GdkTextureChain *chain;  /* lazy, atomic, shared by all chain links */
  GdkTexture *next_texture;  /* no reference, guarded by chain lock */
  GdkTexture *previous_texture;  /* no reference, guarded by chain lock */
  cairo_region_t *diff_to_previous;  /* guarded by chain lock */
};

struct _GdkTextureClass {
  GObjectClass parent_class;

  /* mandatory: Download in the given format into data */
  void                  (* download)                    (GdkTexture             *texture,
                                                         GdkMemoryFormat         format,
                                                         guchar                 *data,
                                                         gsize                   stride);
};

gboolean                gdk_texture_can_load            (GBytes                 *bytes);

GdkTexture *            gdk_texture_new_for_surface     (cairo_surface_t        *surface);
cairo_surface_t *       gdk_texture_download_surface    (GdkTexture             *texture);

void                    gdk_texture_do_download         (GdkTexture             *texture,
                                                         GdkMemoryFormat         format,
                                                         guchar                 *data,
                                                         gsize                   stride);
void                    gdk_texture_diff                (GdkTexture             *self,
                                                         GdkTexture             *other,
                                                         cairo_region_t         *region);

void                    gdk_texture_set_diff            (GdkTexture             *self,
                                                         GdkTexture             *previous,
                                                         cairo_region_t         *diff);

gboolean                gdk_texture_set_render_data     (GdkTexture             *self,
                                                         gpointer                key,
                                                         gpointer                data,
                                                         GDestroyNotify          notify);
void                    gdk_texture_clear_render_data   (GdkTexture             *self);
gpointer                gdk_texture_get_render_data     (GdkTexture             *self,
                                                         gpointer                key);

G_END_DECLS

