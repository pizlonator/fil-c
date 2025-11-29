#include <gtk/gtk.h>

#include <epoxy/gl.h>

#define N 10

static GdkGLContext *gl_context = NULL;
static GskRenderer *gl_renderer = NULL;
static GskRenderer *ngl_renderer = NULL;
static GskRenderer *vulkan_renderer = NULL;

typedef struct _TextureBuilder TextureBuilder;

typedef enum {
  TEXTURE_METHOD_LOCAL,
  TEXTURE_METHOD_GL,
  TEXTURE_METHOD_GL_RELEASED,
  TEXTURE_METHOD_GL_NATIVE,
  TEXTURE_METHOD_NGL,
  TEXTURE_METHOD_VULKAN,
  TEXTURE_METHOD_PNG,
  TEXTURE_METHOD_PNG_PIXBUF,
  TEXTURE_METHOD_TIFF,
  TEXTURE_METHOD_TIFF_PIXBUF,

  N_TEXTURE_METHODS
} TextureMethod;

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
  guint n0 = 0;
  guint n1 = 0;
  guint n2 = 0;

  if (e > 112)
    n0 = (((e - 112) << 10) & 0x7C00) | m >> 13;
  if (e < 113 && e > 101)
    n1 = (((0x007FF000 + m) >> (125- e)) + 1) >> 1;
  if (e > 143)
    n2 = 0x7FFF;

  return (b & 0x80000000) >> 16 | n0 | n1 | n2; // sign : normalized : denormalized : saturate
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

static ChannelType
gdk_memory_format_get_channel_type (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
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
    case GDK_MEMORY_G8:
    case GDK_MEMORY_G8A8:
    case GDK_MEMORY_G8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8:
      return CHANNEL_UINT_8;

    case GDK_MEMORY_R16G16B16:
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16:
    case GDK_MEMORY_G16:
    case GDK_MEMORY_G16A16:
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
    case GDK_MEMORY_A16:
      return CHANNEL_UINT_16;

    case GDK_MEMORY_R16G16B16_FLOAT:
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R16G16B16A16_FLOAT:
    case GDK_MEMORY_A16_FLOAT:
      return CHANNEL_FLOAT_16;

    case GDK_MEMORY_R32G32B32_FLOAT:
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
    case GDK_MEMORY_R32G32B32A32_FLOAT:
    case GDK_MEMORY_A32_FLOAT:
      return CHANNEL_FLOAT_32;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return CHANNEL_UINT_8;
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
gdk_memory_format_is_deep (GdkMemoryFormat format)
{
  return gdk_memory_format_get_channel_type (format) != CHANNEL_UINT_8;
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
encode (GdkMemoryFormat format,
        TextureMethod   method)
{
  return GSIZE_TO_POINTER (method * GDK_MEMORY_N_FORMATS + format);
}

static void
decode (gconstpointer     data,
        GdkMemoryFormat *format,
        TextureMethod   *method)
{
  gsize value = GPOINTER_TO_SIZE (data);

  *format = value % GDK_MEMORY_N_FORMATS;
  value /= GDK_MEMORY_N_FORMATS;

  *method = value;
}

static gpointer
encode_two_formats (GdkMemoryFormat format1,
                    GdkMemoryFormat format2)
{
  return GSIZE_TO_POINTER (format1 * GDK_MEMORY_N_FORMATS + format2);
}

static void
decode_two_formats (gconstpointer    data,
                    GdkMemoryFormat *format1,
                    GdkMemoryFormat *format2)
{
  gsize value = GPOINTER_TO_SIZE (data);

  *format2 = value % GDK_MEMORY_N_FORMATS;
  value /= GDK_MEMORY_N_FORMATS;

  *format1 = value;
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

static GdkTexture *
upload_to_renderer (GdkTexture  *texture,
                    GskRenderer *renderer)
{
  GskRenderNode *node;
  GdkTexture *result;

  if (renderer == NULL)
    {
      g_test_skip ("renderer not supported");
      return texture;
    }

  node = gsk_texture_node_new (texture,
                               &GRAPHENE_RECT_INIT(
                                 0, 0, 
                                 gdk_texture_get_width (texture),
                                 gdk_texture_get_height (texture)
                               ));
  result = gsk_renderer_render_texture (renderer, node, NULL);
  gsk_render_node_unref (node);
  g_object_unref (texture);

  return result;
}

static gboolean
gl_native_should_skip_format (GdkMemoryFormat format)
{
  int major, minor;

  if (gl_context == NULL)
    {
      g_test_skip ("OpenGL is not supported");
      return TRUE;
    }

  if (!gdk_gl_context_get_use_es (gl_context))
    return FALSE;

  gdk_gl_context_get_version (gl_context, &major, &minor);

  if (major < 3)
    {
      g_test_skip ("GLES < 3.0 is not supported");
      return TRUE;
    }

  if (gdk_memory_format_is_deep (format) &&
      (major < 3 || (major == 3 && minor < 1)))
    {
      g_test_skip ("GLES < 3.1 can't handle 16bit non-RGBA formats");
      return TRUE;
    }

  return FALSE;
}

static void
release_texture (gpointer data)
{
  unsigned int id = GPOINTER_TO_UINT (data);

  gdk_gl_context_make_current (gl_context);

  glDeleteTextures (1, &id);
}

static GdkTexture *
upload_to_gl_native (GdkTexture *texture)
{
  struct {
    GdkMemoryFormat format;
    unsigned int bpp;
    int gl_internalformat;
    int gl_format;
    int gl_type;
    int swizzle[4];
  } formats[] = {
    { GDK_MEMORY_R8G8B8, 3, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, { GL_RED, GL_GREEN, GL_BLUE, GL_ONE } },
    { GDK_MEMORY_R16G16B16A16_PREMULTIPLIED, 8, GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT, { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA } },
    { GDK_MEMORY_G8, 1, GL_R8, GL_RED, GL_UNSIGNED_BYTE, { GL_RED, GL_RED, GL_RED, GL_ONE } },
    { GDK_MEMORY_G16, 2, GL_R16, GL_RED, GL_UNSIGNED_SHORT, { GL_RED, GL_RED, GL_RED, GL_ONE } },
    { GDK_MEMORY_G8A8_PREMULTIPLIED, 2, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, { GL_RED, GL_RED, GL_RED, GL_GREEN } },
    { GDK_MEMORY_G16A16_PREMULTIPLIED, 4, GL_RG16, GL_RG, GL_UNSIGNED_SHORT, { GL_RED, GL_RED, GL_RED, GL_GREEN } },
    { GDK_MEMORY_G8A8, 2, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, { GL_RED, GL_RED, GL_RED, GL_GREEN } },
    { GDK_MEMORY_G16A16, 4, GL_RG16, GL_RG, GL_UNSIGNED_SHORT, { GL_RED, GL_RED, GL_RED, GL_GREEN } },
    { GDK_MEMORY_A8, 1, GL_R8, GL_RED, GL_UNSIGNED_BYTE, { GL_ONE, GL_ONE, GL_ONE, GL_RED } },
    { GDK_MEMORY_A16, 2, GL_R16, GL_RED, GL_UNSIGNED_SHORT, { GL_ONE, GL_ONE, GL_ONE, GL_RED } },
  };

  if (gl_native_should_skip_format (gdk_texture_get_format (texture)))
    return texture;

  gdk_gl_context_make_current (gl_context);

  for (unsigned int i = 0; i < G_N_ELEMENTS (formats); i++)
    {
      unsigned int id;
      guchar *data;
      int width, height;
      gsize stride;
      GdkTextureDownloader *d;
      GdkGLTextureBuilder *b;
      GdkTexture *result;

      if (formats[i].format != gdk_texture_get_format (texture))
        continue;

      width = gdk_texture_get_width (texture);
      height = gdk_texture_get_height (texture);
      data = g_malloc (width * height * formats[i].bpp);
      stride = width * formats[i].bpp;

      d = gdk_texture_downloader_new (texture);
      gdk_texture_downloader_set_format (d, formats[i].format);
      gdk_texture_downloader_download_into (d, data, stride);
      gdk_texture_downloader_free (d);

      glGenTextures (1, &id);
      glActiveTexture (GL_TEXTURE0);
      glBindTexture (GL_TEXTURE_2D, id);
      glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
      glTexImage2D (GL_TEXTURE_2D, 0, formats[i].gl_internalformat, width, height, 0, formats[i].gl_format, formats[i].gl_type, data);
      glTexParameteriv (GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, formats[i].swizzle);

      g_free (data);

      b = gdk_gl_texture_builder_new  ();
      gdk_gl_texture_builder_set_context (b, gl_context);
      gdk_gl_texture_builder_set_id (b, id);
      gdk_gl_texture_builder_set_width (b, width);
      gdk_gl_texture_builder_set_height (b, height);
      gdk_gl_texture_builder_set_format (b, formats[i].format);
      result = gdk_gl_texture_builder_build (b, release_texture, GUINT_TO_POINTER (id));
      g_object_unref (b);

      g_object_unref (texture);

      return result;
    }

  return upload_to_renderer (texture, gl_renderer);
}

static GdkTexture *
create_texture (GdkMemoryFormat  format,
                TextureMethod    method,
                int              width,
                int              height,
                const GdkRGBA   *color)
{
  TextureBuilder builder;
  GdkTexture *texture;

  texture_builder_init (&builder, format, width, height);
  texture_builder_fill (&builder, color);

  texture = texture_builder_finish (&builder);

  switch (method)
  {
    case TEXTURE_METHOD_LOCAL:
      break;

    case TEXTURE_METHOD_GL:
      texture = upload_to_renderer (texture, gl_renderer);
      break;

    case TEXTURE_METHOD_GL_RELEASED:
      texture = upload_to_renderer (texture, gl_renderer);
      if (GDK_IS_GL_TEXTURE (texture))
        gdk_gl_texture_release (GDK_GL_TEXTURE (texture));
      break;

    case TEXTURE_METHOD_GL_NATIVE:
      texture = upload_to_gl_native (texture);
      break;

    case TEXTURE_METHOD_NGL:
      texture = upload_to_renderer (texture, ngl_renderer);
      break;

    case TEXTURE_METHOD_VULKAN:
      texture = upload_to_renderer (texture, vulkan_renderer);
      break;

    case TEXTURE_METHOD_PNG:
      {
        GBytes *bytes = gdk_texture_save_to_png_bytes (texture);
        g_assert (bytes);
        g_object_unref (texture);
        texture = gdk_texture_new_from_bytes (bytes, NULL);
        g_assert (texture);
        g_bytes_unref (bytes);
      }
      break;

    case TEXTURE_METHOD_PNG_PIXBUF:
      {
        GInputStream *stream;
        GdkPixbuf *pixbuf;
        GBytes *bytes;

        bytes = gdk_texture_save_to_png_bytes (texture);
        g_assert (bytes);
        g_object_unref (texture);
        stream = g_memory_input_stream_new_from_bytes (bytes);
        pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
        g_object_unref (stream);
        g_assert (pixbuf);
        texture = gdk_texture_new_for_pixbuf (pixbuf);
        g_assert (texture);
        g_object_unref (pixbuf);
        g_bytes_unref (bytes);
      }
      break;

    case TEXTURE_METHOD_TIFF:
      {
        GBytes *bytes = gdk_texture_save_to_tiff_bytes (texture);
        g_assert (bytes);
        g_object_unref (texture);
        texture = gdk_texture_new_from_bytes (bytes, NULL);
        g_assert (texture);
        g_bytes_unref (bytes);
      }
      break;

    case TEXTURE_METHOD_TIFF_PIXBUF:
      {
        GInputStream *stream;
        GdkPixbuf *pixbuf;
        GBytes *bytes;

        bytes = gdk_texture_save_to_tiff_bytes (texture);
        g_assert (bytes);
        g_object_unref (texture);
        stream = g_memory_input_stream_new_from_bytes (bytes);
        pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
        g_object_unref (stream);
        g_assert (pixbuf);
        texture = gdk_texture_new_for_pixbuf (pixbuf);
        g_assert (texture);
        g_object_unref (pixbuf);
        g_bytes_unref (bytes);
      }
      break;

    case N_TEXTURE_METHODS:
    default:
      g_assert_not_reached ();
      break;
  }

  return texture;
}

static gboolean
texture_method_is_accurate (TextureMethod method)
{
  switch (method)
  {
    case TEXTURE_METHOD_LOCAL:
    case TEXTURE_METHOD_TIFF:
      return TRUE;

    case TEXTURE_METHOD_GL:
    case TEXTURE_METHOD_GL_RELEASED:
    case TEXTURE_METHOD_GL_NATIVE:
    case TEXTURE_METHOD_NGL:
    case TEXTURE_METHOD_VULKAN:
    case TEXTURE_METHOD_PNG:
    case TEXTURE_METHOD_PNG_PIXBUF:
    case TEXTURE_METHOD_TIFF_PIXBUF:
      return FALSE;

    case N_TEXTURE_METHODS:
    default:
      g_assert_not_reached ();
      return FALSE;
  }
}

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

static gboolean
should_skip_download_test (GdkMemoryFormat format,
                           TextureMethod   method)
{
  switch (method)
  {
    case TEXTURE_METHOD_LOCAL:
    case TEXTURE_METHOD_PNG:
    case TEXTURE_METHOD_PNG_PIXBUF:
    case TEXTURE_METHOD_TIFF:
      return FALSE;

    case TEXTURE_METHOD_NGL:
      if (ngl_renderer == NULL)
        {
          g_test_skip ("NGL renderer is not supported");
          return TRUE;
        }
      return FALSE;

    case TEXTURE_METHOD_GL:
    case TEXTURE_METHOD_GL_RELEASED:
      if (gl_renderer == NULL)
        {
          g_test_skip ("OpenGL renderer is not supported");
          return TRUE;
        }
      return FALSE;

    case TEXTURE_METHOD_GL_NATIVE:
      return gl_native_should_skip_format (format);

    case TEXTURE_METHOD_VULKAN:
      if (vulkan_renderer == NULL)
        {
          g_test_skip ("Vulkan is not supported");
          return TRUE;
        }
      return FALSE;

    case TEXTURE_METHOD_TIFF_PIXBUF:
      g_test_skip ("the pixbuf tiff loader is broken (gdk-pixbuf#100)");
      return TRUE;

    case N_TEXTURE_METHODS:
    default:
      g_assert_not_reached ();
      return TRUE;
  }
}

static void
test_download (gconstpointer data,
               unsigned int  width,
               unsigned int  height,
               gsize         n_runs)
{
  GdkMemoryFormat format;
  TextureMethod method;
  GdkTexture *expected, *test;
  gsize i;

  decode (data, &format, &method);

  if (should_skip_download_test (format, method))
    return;

  for (i = 0; i < n_runs; i++)
    {
      GdkRGBA color;

      create_random_color (&color);

      /* these methods may premultiply during operation */
      if (color.alpha == 0.f &&
          !gdk_memory_format_is_premultiplied (format) &&
          gdk_memory_format_has_alpha (format) &&
          (method == TEXTURE_METHOD_GL || method == TEXTURE_METHOD_GL_RELEASED ||
           method == TEXTURE_METHOD_GL_NATIVE || method == TEXTURE_METHOD_VULKAN ||
           method == TEXTURE_METHOD_NGL))
        color = (GdkRGBA) { 0, 0, 0, 0 };

      expected = create_texture (format, TEXTURE_METHOD_LOCAL, width, height, &color);
      test = create_texture (format, method, width, height, &color);
      test = ensure_texture_format (test, format);
      
      compare_textures (expected, test, texture_method_is_accurate (method));

      g_object_unref (expected);
      g_object_unref (test);
    }
}

static void
test_download_1x1 (gconstpointer data)
{
  test_download (data, 1, 1, N);
}

static void
test_download_random (gconstpointer data)
{
  int width, height;

  /* Make sure the maximum is:
   * - larger than what GSK puts into the icon cache
   * - larger than the small max-texture-size we test against
   */
  do
    {
      width = g_test_rand_int_range (1, 40) * g_test_rand_int_range (1, 40);
      height = g_test_rand_int_range (1, 40) * g_test_rand_int_range (1, 40);
    }
  while (width * height >= 32 * 1024);

  test_download (data, width, height, 1);
}

static void
test_conversion (gconstpointer data,
                 int           size)
{
  GdkMemoryFormat format1, format2;
  GdkTexture *test1, *test2;
  GdkRGBA color1, color2;
  gboolean accurate;
  gsize i;

  decode_two_formats (data, &format1, &format2);

  if (gdk_memory_format_get_channel_type (format1) == CHANNEL_FLOAT_16)
    accurate = FALSE;
  else
    accurate = TRUE;

  for (i = 0; i < N; i++)
    {
      /* non-premultiplied can represet GdkRGBA (1, 1, 1, 0)
       * but premultiplied cannot.
       * Premultiplied will always represent this as (0, 0, 0, 0)
       */
      do
        {
          create_random_color (&color1);
        }
      while (color1.alpha == 0 &&
             gdk_memory_format_is_premultiplied (format1) !=
             gdk_memory_format_is_premultiplied (format2));

      /* If the source can't handle alpha, make sure
       * the target uses with the opaque version of the color.
       */
      color2 = color1;
      if (!gdk_memory_format_has_alpha (format1) &&
          gdk_memory_format_has_alpha (format2))
        color_make_opaque (&color2, &color2);

      /* If the source has fewer color channels than the
       * target, make sure the colors get adjusted.
       */
      if (gdk_memory_format_n_colors (format1) <
          gdk_memory_format_n_colors (format2))
        {
          if (gdk_memory_format_n_colors (format1) == 1)
            color_make_gray (&color2, &color2);
          else
            color_make_white (&color2, &color2);
        }

      test1 = create_texture (format1, TEXTURE_METHOD_LOCAL, 1, 1, &color1);
      test2 = create_texture (format2, TEXTURE_METHOD_LOCAL, 1, 1, &color2);

      /* Convert the first one to the format of the 2nd */
      test1 = ensure_texture_format (test1, format2);
      
      compare_textures (test1, test2, accurate);

      g_object_unref (test2);
      g_object_unref (test1);
    }
}

static void
test_conversion_1x1 (gconstpointer data)
{
  test_conversion (data, 1);
}

static void
test_conversion_random (gconstpointer data)
{
  test_conversion (data, g_test_rand_int_range (2, 18));
}

static void
add_test (const char    *name,
          GTestDataFunc  func)
{
  GdkMemoryFormat format;
  TextureMethod method;
  GEnumClass *enum_class;

  enum_class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);

  for (format = 0; format < GDK_MEMORY_N_FORMATS; format++)
    {
      for (method = 0; method < N_TEXTURE_METHODS; method++)
        {
          const char *method_names[N_TEXTURE_METHODS] = {
            [TEXTURE_METHOD_LOCAL] = "local",
            [TEXTURE_METHOD_GL] = "gl",
            [TEXTURE_METHOD_GL_RELEASED] = "gl-released",
            [TEXTURE_METHOD_GL_NATIVE] = "gl-native",
            [TEXTURE_METHOD_NGL] = "ngl",
            [TEXTURE_METHOD_VULKAN] = "vulkan",
            [TEXTURE_METHOD_PNG] = "png",
            [TEXTURE_METHOD_PNG_PIXBUF] = "png-pixbuf",
            [TEXTURE_METHOD_TIFF] = "tiff",
            [TEXTURE_METHOD_TIFF_PIXBUF] = "tiff-pixbuf"
          };
          char *test_name = g_strdup_printf ("%s/%s/%s",
                                             name,
                                             g_enum_get_value (enum_class, format)->value_nick,
                                             method_names[method]);
          g_test_add_data_func_full (test_name, encode (format, method), func, NULL);
          g_free (test_name);
        }
    }
}

static void
add_conversion_test (const char    *name,
                     GTestDataFunc  func)
{
  GdkMemoryFormat format1, format2;
  GEnumClass *enum_class;

  enum_class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);

  for (format1 = 0; format1 < GDK_MEMORY_N_FORMATS; format1++)
    {
      for (format2 = 0; format2 < GDK_MEMORY_N_FORMATS; format2++)
        {
          char *test_name = g_strdup_printf ("%s/%s/%s",
                                             name,
                                             g_enum_get_value (enum_class, format1)->value_nick,
                                             g_enum_get_value (enum_class, format2)->value_nick);
          g_test_add_data_func_full (test_name, encode_two_formats (format1, format2), func, NULL);
          g_free (test_name);
        }
    }
}

int
main (int argc, char *argv[])
{
  GdkDisplay *display;
  int result;

  gtk_test_init (&argc, &argv, NULL);

  add_test ("/memorytexture/download_1x1", test_download_1x1);
  add_test ("/memorytexture/download_random", test_download_random);
  add_conversion_test ("/memorytexture/conversion_1x1", test_conversion_1x1);
  add_conversion_test ("/memorytexture/conversion_random", test_conversion_random);

  display = gdk_display_get_default ();

  gl_context = gdk_display_create_gl_context (display, NULL);
  if (gl_context == NULL || !gdk_gl_context_realize (gl_context, NULL))
    {
      g_clear_object (&gl_context);
    }

  gl_renderer = gsk_gl_renderer_new ();
  if (!gsk_renderer_realize_for_display (gl_renderer, display, NULL))
    {
      g_clear_object (&gl_renderer);
    }

  ngl_renderer = gsk_ngl_renderer_new ();
  if (!gsk_renderer_realize_for_display (ngl_renderer, display, NULL))
    {
      g_clear_object (&ngl_renderer);
    }

  vulkan_renderer = gsk_vulkan_renderer_new ();
  if (!gsk_renderer_realize_for_display (vulkan_renderer, display, NULL))
    {
      g_clear_object (&vulkan_renderer);
    }

  result = g_test_run ();

#ifdef GDK_RENDERING_VULKAN
  if (vulkan_renderer)
    {
      gsk_renderer_unrealize (vulkan_renderer);
      g_clear_object (&vulkan_renderer);
    }
#endif
  if (ngl_renderer)
    {
      gsk_renderer_unrealize (ngl_renderer);
      g_clear_object (&ngl_renderer);
    }
  if (gl_renderer)
    {
      gsk_renderer_unrealize (gl_renderer);
      g_clear_object (&gl_renderer);
    }
  gdk_gl_context_clear_current ();

  g_clear_object (&gl_context);

  return result;
}
