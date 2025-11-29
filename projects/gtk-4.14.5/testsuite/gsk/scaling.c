#include <gtk/gtk.h>

#define N 10

struct {
  const char *name;
  GskRenderer * (*create_func) (void);
  GskRenderer *renderer;
} renderers[] = {
#if 0
  /* The GL renderer is broken, no idea why. It's suppsoed to work. */
  {
    "gl",
    gsk_gl_renderer_new,
  },
#endif
  {
    "cairo",
    gsk_cairo_renderer_new,
  },
  {
    "vulkan",
    gsk_vulkan_renderer_new,
  },
  {
    "ngl",
    gsk_ngl_renderer_new,
  },
};

typedef struct _TextureBuilder TextureBuilder;

typedef enum {
  CHANNEL_UINT_8,
  CHANNEL_UINT_16,
  CHANNEL_FLOAT_16,
  CHANNEL_FLOAT_32,
} ChannelType;

struct _TextureBuilder
{
  GdkMemoryFormat format;
  int width;
  int height;

  guchar *pixels;
  gsize stride;
  gsize offset;
};

static inline guint
as_uint (const float x)
{
  return *(guint*)&x;
}

static inline float
as_float (const guint x)
{
  return *(float*)&x;
}

// IEEE-754 16-bit floating-point format (without infinity): 1-5-10
//
static inline float
half_to_float (const guint16 x)
{
  const guint e = (x&0x7C00)>>10; // exponent
  const guint m = (x&0x03FF)<<13; // mantissa
  const guint v = as_uint((float)m)>>23;
  return as_float((x&0x8000)<<16 | (e!=0)*((e+112)<<23|m) | ((e==0)&(m!=0))*((v-37)<<23|((m<<(150-v))&0x007FE000)));
}

static inline guint16
float_to_half (const float x)
{
  const guint b = *(guint*)&x+0x00001000; // round-to-nearest-even
  const guint e = (b&0x7F800000)>>23; // exponent
  const guint m = b&0x007FFFFF; // mantissa
  return (b&0x80000000)>>16 | (e>112)*((((e-112)<<10)&0x7C00)|m>>13) | ((e<113)&(e>101))*((((0x007FF000+m)>>(125-e))+1)>>1) | (e>143)*0x7FFF; // sign : normalized : denormalized : saturate
}

static gsize
gdk_memory_format_bytes_per_pixel (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_G8:
    case GDK_MEMORY_A8:
      return 1;

    case GDK_MEMORY_G8A8_PREMULTIPLIED:
    case GDK_MEMORY_G8A8:
    case GDK_MEMORY_G16:
    case GDK_MEMORY_A16:
    case GDK_MEMORY_A16_FLOAT:
      return 2;

    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
      return 3;

    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_B8G8R8X8:
    case GDK_MEMORY_X8R8G8B8:
    case GDK_MEMORY_R8G8B8X8:
    case GDK_MEMORY_X8B8G8R8:
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
    case GDK_MEMORY_G16A16:
    case GDK_MEMORY_A32_FLOAT:
      return 4;

    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16_FLOAT:
      return 6;

    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
      return 8;

    case GDK_MEMORY_R32G32B32_FLOAT:
      return 12;

    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
      return 16;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return 4;
    }
}

/* return the number of color channels, ignoring alpha */
static guint
gdk_memory_format_n_colors (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R32G32B32_FLOAT:
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_B8G8R8X8:
    case GDK_MEMORY_X8R8G8B8:
    case GDK_MEMORY_R8G8B8X8:
    case GDK_MEMORY_X8B8G8R8:
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
      return 3;

    case GDK_MEMORY_G8:
    case GDK_MEMORY_G16:
    case GDK_MEMORY_G8A8_PREMULTIPLIED:
    case GDK_MEMORY_G8A8:
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
    case GDK_MEMORY_G16A16:
      return 1;

    case GDK_MEMORY_A8:
    case GDK_MEMORY_A16:
    case GDK_MEMORY_A16_FLOAT:
    case GDK_MEMORY_A32_FLOAT:
      return 0;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return TRUE;
    }
}

static gboolean
gdk_memory_format_has_alpha (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R32G32B32_FLOAT:
    case GDK_MEMORY_G8:
    case GDK_MEMORY_G16:
    case GDK_MEMORY_B8G8R8X8:
    case GDK_MEMORY_X8R8G8B8:
    case GDK_MEMORY_R8G8B8X8:
    case GDK_MEMORY_X8B8G8R8:
      return FALSE;

    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
    case GDK_MEMORY_G8A8_PREMULTIPLIED:
    case GDK_MEMORY_G8A8:
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
    case GDK_MEMORY_G16A16:
    case GDK_MEMORY_A8:
    case GDK_MEMORY_A16:
    case GDK_MEMORY_A16_FLOAT:
    case GDK_MEMORY_A32_FLOAT:
      return TRUE;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return TRUE;
    }
}

static gboolean
gdk_memory_format_is_premultiplied (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_G8A8_PREMULTIPLIED:
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
    case GDK_MEMORY_A8:
    case GDK_MEMORY_A16:
    case GDK_MEMORY_A16_FLOAT:
    case GDK_MEMORY_A32_FLOAT:
      return TRUE;

    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R32G32B32_FLOAT:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_B8G8R8X8:
    case GDK_MEMORY_X8R8G8B8:
    case GDK_MEMORY_R8G8B8X8:
    case GDK_MEMORY_X8B8G8R8:
    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
    case GDK_MEMORY_G8:
    case GDK_MEMORY_G8A8:
    case GDK_MEMORY_G16:
    case GDK_MEMORY_G16A16:
      return FALSE;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static gboolean
gdk_memory_format_pixel_equal (GdkMemoryFormat  format,
                               gboolean         accurate,
                               const guchar    *pixel1,
                               const guchar    *pixel2)
{
  switch (format)
    {
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
    case GDK_MEMORY_A8:
    case GDK_MEMORY_G8:
    case GDK_MEMORY_G8A8:
    case GDK_MEMORY_G8A8_PREMULTIPLIED:
      return memcmp (pixel1, pixel2, gdk_memory_format_bytes_per_pixel (format)) == 0;

    case GDK_MEMORY_B8G8R8X8:
    case GDK_MEMORY_R8G8B8X8:
      return memcmp (pixel1, pixel2, 3) == 0;

    case GDK_MEMORY_X8R8G8B8:
    case GDK_MEMORY_X8B8G8R8:
      return memcmp (pixel1 + 1, pixel2 + 1, 3) == 0;

    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_G16:
    case GDK_MEMORY_G16A16:
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
    case GDK_MEMORY_A16:
      {
        const guint16 *u1 = (const guint16 *) pixel1;
        const guint16 *u2 = (const guint16 *) pixel2;
        guint i;
        for (i = 0; i < gdk_memory_format_bytes_per_pixel (format) / sizeof (guint16); i++)
          {
            if (!G_APPROX_VALUE (u1[i], u2[i], accurate ? 1 : 256))
              return FALSE;
          }
      }
      return TRUE;

    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_A16_FLOAT:
      {
        guint i;
        for (i = 0; i < gdk_memory_format_bytes_per_pixel (format) / sizeof (guint16); i++)
          {
            float f1 = half_to_float (((guint16 *) pixel1)[i]);
            float f2 = half_to_float (((guint16 *) pixel2)[i]);
            if (!G_APPROX_VALUE (f1, f2, accurate ? 1./65535 : 1./255))
              return FALSE;
          }
      }
      return TRUE;

    case GDK_MEMORY_R32G32B32_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_A32_FLOAT:
      {
        const float *f1 = (const float *) pixel1;
        const float *f2 = (const float *) pixel2;
        guint i;
        for (i = 0; i < gdk_memory_format_bytes_per_pixel (format) / sizeof (float); i++)
          {
            if (!G_APPROX_VALUE (f1[i], f2[i], accurate ? 1./65535 : 1./255))
              return FALSE;
          }
      }
      return TRUE;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static gpointer
encode_renderer_format (guint           renderer,
                        GdkMemoryFormat format)
{
  return GSIZE_TO_POINTER (format * G_N_ELEMENTS (renderers) + renderer);
}

static void
decode_renderer_format (gconstpointer     data,
                        GskRenderer     **renderer,
                        GdkMemoryFormat  *format)
{
  gsize value = GPOINTER_TO_SIZE (data);

  *renderer = renderers[value % G_N_ELEMENTS (renderers)].renderer;
  value /= G_N_ELEMENTS (renderers);

  *format = value;
}

static void
texture_builder_init (TextureBuilder  *builder,
                      GdkMemoryFormat  format,
                      int              width,
                      int              height)
{
  gsize extra_stride;

  builder->format = format;
  builder->width = width;
  builder->height = height;

  extra_stride = g_test_rand_bit() ? g_test_rand_int_range (0, 16) : 0;
  builder->offset = g_test_rand_bit() ? g_test_rand_int_range (0, 128) : 0;
  builder->stride = width * gdk_memory_format_bytes_per_pixel (format) + extra_stride;
  builder->pixels = g_malloc0 (builder->offset + builder->stride * height);
}

static GdkTexture *
texture_builder_finish (TextureBuilder *builder)
{
  GBytes *bytes;
  GdkTexture *texture;

  bytes = g_bytes_new_with_free_func (builder->pixels + builder->offset,
                                      builder->height * builder->stride,
                                      g_free,
                                      builder->pixels);
  texture = gdk_memory_texture_new (builder->width,
                                    builder->height,
                                    builder->format,
                                    bytes,
                                    builder->stride);
  g_bytes_unref (bytes);

  return texture;
}

static inline void
set_pixel_u8 (guchar          *data,
              int              r,
              int              g,
              int              b,
              int              a,
              gboolean         premultiply,
              const GdkRGBA   *color)
{
  if (a >= 0)
    data[a] = CLAMP (color->alpha * 255.f + 0.5f, 0.f, 255.f);
  if (premultiply)
    {
      data[r] = CLAMP (color->red * color->alpha * 255.f + 0.5f, 0.f, 255.f);
      data[g] = CLAMP (color->green * color->alpha * 255.f + 0.5f, 0.f, 255.f);
      data[b] = CLAMP (color->blue * color->alpha * 255.f + 0.5f, 0.f, 255.f);
    }
  else
    {
      data[r] = CLAMP (color->red * 255.f + 0.5f, 0.f, 255.f);
      data[g] = CLAMP (color->green * 255.f + 0.5f, 0.f, 255.f);
      data[b] = CLAMP (color->blue * 255.f + 0.5f, 0.f, 255.f);
    }
}

static float
color_gray (const GdkRGBA *color)
{
  return 1/3.f * (color->red + color->green + color->blue);
}

static void
texture_builder_set_pixel (TextureBuilder  *builder,
                           int              x,
                           int              y,
                           const GdkRGBA   *color)
{
  guchar *data;

  g_assert_cmpint (x, >=, 0);
  g_assert_cmpint (x, <, builder->width);
  g_assert_cmpint (y, >=, 0);
  g_assert_cmpint (y, <, builder->height);

  data = builder->pixels
         + builder->offset
         + y * builder->stride
         + x * gdk_memory_format_bytes_per_pixel (builder->format);

  switch (builder->format)
  {
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
      set_pixel_u8 (data, 2, 1, 0, 3, TRUE, color);
      break;
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
      set_pixel_u8 (data, 1, 2, 3, 0, TRUE, color);
      break;
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
      set_pixel_u8 (data, 0, 1, 2, 3, TRUE, color);
      break;
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
      set_pixel_u8 (data, 3, 2, 1, 0, TRUE, color);
      break;
    case GDK_MEMORY_B8G8R8A8:
      set_pixel_u8 (data, 2, 1, 0, 3, FALSE, color);
      break;
    case GDK_MEMORY_A8R8G8B8:
      set_pixel_u8 (data, 1, 2, 3, 0, FALSE, color);
      break;
    case GDK_MEMORY_R8G8B8A8:
      set_pixel_u8 (data, 0, 1, 2, 3, FALSE, color);
      break;
    case GDK_MEMORY_A8B8G8R8:
      set_pixel_u8 (data, 3, 2, 1, 0, FALSE, color);
      break;
    case GDK_MEMORY_B8G8R8X8:
      set_pixel_u8 (data, 2, 1, 0, -1, TRUE, color);
      break;
    case GDK_MEMORY_X8R8G8B8:
      set_pixel_u8 (data, 1, 2, 3, -1, TRUE, color);
      break;
    case GDK_MEMORY_R8G8B8X8:
      set_pixel_u8 (data, 0, 1, 2, -1, TRUE, color);
      break;
    case GDK_MEMORY_X8B8G8R8:
      set_pixel_u8 (data, 3, 2, 1, -1, TRUE, color);
      break;
    case GDK_MEMORY_R8G8B8:
      set_pixel_u8 (data, 0, 1, 2, -1, TRUE, color);
      break;
    case GDK_MEMORY_B8G8R8:
      set_pixel_u8 (data, 2, 1, 0, -1, TRUE, color);
      break;
    case GDK_MEMORY_R16G16B16:
      {
        guint16 pixels[3] = {
          CLAMP (color->red * color->alpha * 65535.f + 0.5f, 0, 65535.f),
          CLAMP (color->green * color->alpha * 65535.f + 0.5f, 0, 65535.f),
          CLAMP (color->blue * color->alpha * 65535.f + 0.5f, 0, 65535.f),
        };
        memcpy (data, pixels, 3 * sizeof (guint16));
      }
      break;
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
      {
        guint16 pixels[4] = {
          CLAMP (color->red * color->alpha * 65535.f + 0.5f, 0, 65535.f),
          CLAMP (color->green * color->alpha * 65535.f + 0.5f, 0, 65535.f),
          CLAMP (color->blue * color->alpha * 65535.f + 0.5f, 0, 65535.f),
          CLAMP (color->alpha * 65535.f + 0.5f, 0, 65535.f),
        };
        memcpy (data, pixels, 4 * sizeof (guint16));
      }
      break;
    case GDK_MEMORY_R16G16B16A16:
      {
        guint16 pixels[4] = {
          CLAMP (color->red * 65535.f + 0.5f, 0, 65535.f),
          CLAMP (color->green * 65535.f + 0.5f, 0, 65535.f),
          CLAMP (color->blue * 65535.f + 0.5f, 0, 65535.f),
          CLAMP (color->alpha * 65535.f + 0.5f, 0, 65535.f),
        };
        memcpy (data, pixels, 4 * sizeof (guint16));
      }
      break;
    case GDK_MEMORY_R16G16B16_FLOAT:
      {
        guint16 pixels[3] = {
          float_to_half (color->red * color->alpha),
          float_to_half (color->green * color->alpha),
          float_to_half (color->blue * color->alpha)
        };
        memcpy (data, pixels, 3 * sizeof (guint16));
      }
      break;
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
      {
        guint16 pixels[4] = {
          float_to_half (color->red * color->alpha),
          float_to_half (color->green * color->alpha),
          float_to_half (color->blue * color->alpha),
          float_to_half (color->alpha)
        };
        memcpy (data, pixels, 4 * sizeof (guint16));
      }
      break;
    case GDK_MEMORY_R16G16B16A16_FLOAT:
      {
        guint16 pixels[4] = {
          float_to_half (color->red),
          float_to_half (color->green),
          float_to_half (color->blue),
          float_to_half (color->alpha)
        };
        memcpy (data, pixels, 4 * sizeof (guint16));
      }
      break;
    case GDK_MEMORY_R32G32B32_FLOAT:
      {
        float pixels[3] = {
          color->red * color->alpha,
          color->green * color->alpha,
          color->blue * color->alpha
        };
        memcpy (data, pixels, 3 * sizeof (float));
      }
      break;
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
      {
        float pixels[4] = {
          color->red * color->alpha,
          color->green * color->alpha,
          color->blue * color->alpha,
          color->alpha
        };
        memcpy (data, pixels, 4 * sizeof (float));
      }
      break;
    case GDK_MEMORY_R32G32B32A32_FLOAT:
      {
        float pixels[4] = {
          color->red,
          color->green,
          color->blue,
          color->alpha
        };
        memcpy (data, pixels, 4 * sizeof (float));
      }
      break;
    case GDK_MEMORY_G8A8_PREMULTIPLIED:
      {
        data[0] = CLAMP (color_gray (color) * color->alpha * 255.f + 0.5f, 0.f, 255.f);
        data[1] = CLAMP (color->alpha * 255.f + 0.5f, 0.f, 255.f);
      }
      break;
    case GDK_MEMORY_G8A8:
      {
        data[0] = CLAMP (color_gray (color) * 255.f + 0.5f, 0.f, 255.f);
        data[1] = CLAMP (color->alpha * 255.f + 0.5f, 0.f, 255.f);
      }
      break;
    case GDK_MEMORY_G8:
      {
        *data = CLAMP (color_gray (color) * color->alpha * 255.f + 0.5f, 0.f, 255.f);
      }
      break;
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
      {
        guint16 pixels[2] = {
          CLAMP (color_gray (color) * color->alpha * 65535.f + 0.5f, 0.f, 65535.f),
          CLAMP (color->alpha * 65535.f + 0.5f, 0.f, 65535.f),
        };
        memcpy (data, pixels, 2 * sizeof (guint16));
      }
      break;
    case GDK_MEMORY_G16A16:
      {
        guint16 pixels[2] = {
          CLAMP (color_gray (color) * 65535.f + 0.5f, 0.f, 65535.f),
          CLAMP (color->alpha * 65535.f + 0.5f, 0.f, 65535.f),
        };
        memcpy (data, pixels, 2 * sizeof (guint16));
      }
      break;
    case GDK_MEMORY_G16:
      {
        guint16 pixel = CLAMP (color_gray (color) * color->alpha * 65535.f + 0.5f, 0.f, 65535.f);
        memcpy (data, &pixel,  sizeof (guint16));
      }
      break;
    case GDK_MEMORY_A8:
      {
        *data = CLAMP (color->alpha * 255.f + 0.5f, 0.f, 255.f);
      }
      break;
    case GDK_MEMORY_A16:
      {
        guint16 pixel = CLAMP (color->alpha * 65535.f, 0.f, 65535.f);
        memcpy (data, &pixel,  sizeof (guint16));
      }
      break;
    case GDK_MEMORY_A16_FLOAT:
      {
        guint16 pixel = float_to_half (color->alpha);
        memcpy (data, &pixel, sizeof (guint16));
      }
      break;
    case GDK_MEMORY_A32_FLOAT:
      {
        memcpy (data, &color->alpha, sizeof (float));
      }
      break;
    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
texture_builder_fill (TextureBuilder  *builder,
                      const GdkRGBA   *color)
{
  int x, y;
  for (y = 0; y < builder->height; y++)
    for (x = 0; x < builder->width; x++)
      texture_builder_set_pixel (builder, x, y, color);
}

static void
compare_textures (GdkTexture *texture1,
                  GdkTexture *texture2,
                  gboolean    accurate_compare)
{
  GdkTextureDownloader *downloader1, *downloader2;
  GBytes *bytes1, *bytes2;
  gsize stride1, stride2, bpp;
  const guchar *data1, *data2;
  int width, height, x, y;
  GdkMemoryFormat format;

  g_assert_cmpint (gdk_texture_get_width (texture1), ==, gdk_texture_get_width (texture2));
  g_assert_cmpint (gdk_texture_get_height (texture1), ==, gdk_texture_get_height (texture2));
  g_assert_cmpint (gdk_texture_get_format (texture1), ==, gdk_texture_get_format (texture2));

  format = gdk_texture_get_format (texture1);
  bpp = gdk_memory_format_bytes_per_pixel (format);
  width = gdk_texture_get_width (texture1);
  height = gdk_texture_get_height (texture1);

  downloader1 = gdk_texture_downloader_new (texture1);
  gdk_texture_downloader_set_format (downloader1, format);
  bytes1 = gdk_texture_downloader_download_bytes (downloader1, &stride1);
  g_assert_cmpint (stride1, >=, bpp * width);
  g_assert_nonnull (bytes1);
  gdk_texture_downloader_free (downloader1);

  downloader2 = gdk_texture_downloader_new (texture2);
  gdk_texture_downloader_set_format (downloader2, format);
  bytes2 = gdk_texture_downloader_download_bytes (downloader2, &stride2);
  g_assert_cmpint (stride2, >=, bpp * width);
  g_assert_nonnull (bytes2);
  gdk_texture_downloader_free (downloader2);

  data1 = g_bytes_get_data (bytes1, NULL);
  data2 = g_bytes_get_data (bytes2, NULL);
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          g_assert_true (gdk_memory_format_pixel_equal (format, accurate_compare, data1 + bpp * x, data2 + bpp * x));
        }
      data1 += stride1;
      data2 += stride2;
    }

  g_bytes_unref (bytes2);
  g_bytes_unref (bytes1);
}

#if 0
static GdkTexture *
ensure_texture_format (GdkTexture      *texture,
                       GdkMemoryFormat  format)
{
  GdkTextureDownloader *downloader;
  GdkTexture *result;
  GBytes *bytes;
  gsize stride;

  if (gdk_texture_get_format (texture) == format)
    return texture;

  downloader = gdk_texture_downloader_new (texture);
  gdk_texture_downloader_set_format (downloader, format);
  bytes = gdk_texture_downloader_download_bytes (downloader, &stride);
  gdk_texture_downloader_free (downloader);

  result = gdk_memory_texture_new (gdk_texture_get_width (texture),
                                   gdk_texture_get_height (texture),
                                   format,
                                   bytes,
                                   stride);
  g_bytes_unref (bytes);
  g_object_unref (texture);

  return result;
}
#endif

static void
color_make_opaque (GdkRGBA       *result,
                   const GdkRGBA *color)
{
  result->red *= color->alpha;
  result->green *= color->alpha;
  result->blue *= color->alpha;
  result->alpha = 1.0f;
}

static void
color_make_gray (GdkRGBA       *result,
                 const GdkRGBA *color)
{
  result->red = (color->red + color->green + color->blue) / 3.0f;
  result->green = result->red;
  result->blue = result->red;
  result->alpha = color->alpha;
}

static void
color_make_white (GdkRGBA       *result,
                  const GdkRGBA *color)
{
  result->red = 1.0f;
  result->green = 1.0f;
  result->blue = 1.0f;
  result->alpha = color->alpha;
}

/* Generate colors so that premultiplying will result in values in steps of 1/15th.
 * Also make sure that an averaged gray value fits in that range. */
static void
create_random_color (GdkRGBA *color)
{
  int r, g, b;
  do
    {
      r = g_test_rand_int_range (0, 6);
      g = g_test_rand_int_range (0, 6);
      b = g_test_rand_int_range (0, 6);
    }
  while ((r + g + b) % 3 != 0);
  color->red = r / 5.f;
  color->green = g / 5.f;
  color->blue = b / 5.f;
  color->alpha = g_test_rand_int_range (0, 4) / 3.f;
}

static void
create_random_color_for_format (GdkRGBA *color,
                                GdkMemoryFormat format)
{
  /* non-premultiplied can represet GdkRGBA (1, 1, 1, 0)
   * but premultiplied cannot.
   * Premultiplied will always represent this as (0, 0, 0, 0)
   */
  do
    {
      create_random_color (color);
    }
  while (color->alpha == 0 &&
         gdk_memory_format_is_premultiplied (format));

  /* If the format can't handle alpha, make things opaque
   */
  if (!gdk_memory_format_has_alpha (format))
    color_make_opaque (color, color);

  /* If the format has fewer color channels than the
   * target, make sure the colors get adjusted.
   */
  if (gdk_memory_format_n_colors (format) == 1)
    color_make_gray (color, color);
  else if (gdk_memory_format_n_colors (format) == 0)
    color_make_white (color, color);
}

static GdkTexture *
create_solid_color_texture (GdkMemoryFormat  format,
                            int              width,
                            int              height,
                            const GdkRGBA   *color)
{
  TextureBuilder builder;

  texture_builder_init (&builder, format, width, height);
  texture_builder_fill (&builder, color);

  return texture_builder_finish (&builder);
}

/* randomly creates 4 colors with values that are multiples
 * of 16, so that averaging the colors works without rounding
 * errors, and then creates a stipple pattern like this:
 *
 * 1 2 1 2 1 2 ...
 * 3 4 3 4 3 4
 * 1 2 1 2 1 2
 * 3 4 3 4 3 4
 * 1 2 1 2 1 2
 * 3 4 3 4 3 4
 * ⋮
 */
static GdkTexture *
create_stipple_texture (GdkMemoryFormat  format,
                        gsize            width,
                        gsize            height,
                        GdkRGBA         *average)
{
  TextureBuilder builder;
  GdkTexture *texture;
  int x, y;
  GdkRGBA colors[2][2];

  *average = (GdkRGBA) { 0, 0, 0, 0 };

  for (y = 0; y < 2; y++)
    {
      for (x = 0; x < 2; x++)
        {
          create_random_color_for_format (&colors[x][y], format);
          if (gdk_memory_format_has_alpha (format))
            colors[x][y].alpha *= 16.f/17.f;
          else
            {
              colors[x][y].red *= 16.f/17.f;
              colors[x][y].green *= 16.f/17.f;
              colors[x][y].blue *= 16.f/17.f;
            }
          average->red += colors[x][y].red * colors[x][y].alpha;
          average->green += colors[x][y].green * colors[x][y].alpha;
          average->blue += colors[x][y].blue * colors[x][y].alpha;
          average->alpha += colors[x][y].alpha;
        }
    }

  average->red /= average->alpha;
  average->green /= average->alpha;
  average->blue /= average->alpha;
  average->alpha /= 4.0f;

  texture_builder_init (&builder, format, width, height);
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          texture_builder_set_pixel (&builder, x, y, &colors[x % 2][y % 2]);
        }
    }
  texture = texture_builder_finish (&builder);

  return texture;
}

static void
test_linear_filtering (gconstpointer data,
                       gsize         width,
                       gsize         height)
{
  GdkMemoryFormat format;
  GskRenderer *renderer;
  GdkTexture *input, *output, *expected;
  GskRenderNode *node;
  GdkRGBA average_color;

  decode_renderer_format (data, &renderer, &format);

  input = create_stipple_texture (format, width, height, &average_color);
  node = gsk_texture_scale_node_new (input, &GRAPHENE_RECT_INIT (0, 0, width / 2, height / 2), GSK_SCALING_FILTER_LINEAR);
  output = gsk_renderer_render_texture (renderer, node, NULL);
  expected = create_solid_color_texture (gdk_texture_get_format (output), width / 2, height / 2, &average_color);

  compare_textures (expected, output, FALSE);

  g_object_unref (expected);
  g_object_unref (output);
  gsk_render_node_unref (node);
  g_object_unref (input);
}

static void
test_mipmaps (gconstpointer data)
{
  GdkMemoryFormat format;
  GskRenderer *renderer;
  GdkTexture *input, *output, *expected;
  GskRenderNode *node;
  GdkRGBA average_color;

  decode_renderer_format (data, &renderer, &format);

  input = create_stipple_texture (format, 2, 2, &average_color);
  node = gsk_texture_scale_node_new (input, &GRAPHENE_RECT_INIT (0, 0, 1, 1), GSK_SCALING_FILTER_TRILINEAR);
  output = gsk_renderer_render_texture (renderer, node, NULL);
  expected = create_solid_color_texture (gdk_texture_get_format (output), 1, 1, &average_color);

  compare_textures (expected, output, FALSE);

  g_object_unref (expected);
  g_object_unref (output);
  gsk_render_node_unref (node);
  g_object_unref (input);
}

static void
test_linear_filtering_2x2 (gconstpointer data)
{
  test_linear_filtering (data, 2, 2);
}

static void
test_linear_filtering_512x512 (gconstpointer data)
{
  test_linear_filtering (data, 512, 512);
}

static void
add_format_test (const char    *name,
                 GTestDataFunc  func)
{
  GdkMemoryFormat format;
  gsize renderer;
  GEnumClass *enum_class;

  enum_class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);

  for (renderer = 0; renderer < G_N_ELEMENTS (renderers); renderer++)
    {
      if (renderers[renderer].renderer == NULL)
        continue;

      for (format = 0; format < GDK_MEMORY_N_FORMATS; format++)
        {
          char *test_name = g_strdup_printf ("%s/%s/%s",
                                             name,
                                             renderers[renderer].name,
                                             g_enum_get_value (enum_class, format)->value_nick);
          g_test_add_data_func_full (test_name, encode_renderer_format (renderer, format), func, NULL);
          g_free (test_name);
        }
    }
}

static void
create_renderers (void)
{
  GError *error = NULL;
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (renderers); i++)
    {
      renderers[i].renderer = renderers[i].create_func ();
      if (!gsk_renderer_realize_for_display (renderers[i].renderer, gdk_display_get_default (), &error))
        {
          g_test_message ("Could not realize %s renderer: %s", renderers[i].name, error->message);
          g_clear_error (&error);
          g_clear_object (&renderers[i].renderer);
        }
    }
}

static void
destroy_renderers (void)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (renderers); i++)
    {
      if (renderers[i].renderer == NULL)
        continue;

      gsk_renderer_unrealize (renderers[i].renderer);
      g_clear_object (&renderers[i].renderer);
    }
}

int
main (int argc, char *argv[])
{
  int result;

  gtk_test_init (&argc, &argv, NULL);
  create_renderers ();

  add_format_test ("/scaling/linear-filtering", test_linear_filtering_2x2);
  add_format_test ("/scaling/linear-filtering-large", test_linear_filtering_512x512);
  add_format_test ("/scaling/mipmap", test_mipmaps);

  result = g_test_run ();

  /* So the context gets actually destroyed */
  gdk_gl_context_clear_current ();

  destroy_renderers ();

  return result;
}
