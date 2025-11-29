#include <gtk/gtk.h>

static const char *format_name[] = {
  "BGRAp", "ARGBp", "RGBAp",
  "BGRA", "ARGB", "RGBA", "ABGR",
  "RGB", "BGR", NULL
};

static const char *
format_to_string (GdkMemoryFormat format)
{
  if (format < g_strv_length ((gchar **) format_name))
    return format_name[format];
  else
    return "ERROR";
}

/* Copied from gdkmemorytexture.c */

static void
convert_memcpy (guchar       *dest_data,
                gsize         dest_stride,
                const guchar *src_data,
                gsize         src_stride,
                gsize         width,
                gsize         height)
{
  gsize y;

  for (y = 0; y < height; y++)
    memcpy (dest_data + y * dest_stride, src_data + y * src_stride, 4 * width);
}

static void
convert_memcpy3 (guchar       *dest_data,
                 gsize         dest_stride,
                 const guchar *src_data,
                 gsize         src_stride,
                 gsize         width,
                 gsize         height)
{
  gsize y;

  for (y = 0; y < height; y++)
    memcpy (dest_data + y * dest_stride, src_data + y * src_stride, 3 * width);
}

#define SWIZZLE3(R,G,B) \
static void \
convert_swizzle ## R ## G ## B (guchar       *dest_data, \
                                gsize         dest_stride, \
                                const guchar *src_data, \
                                gsize         src_stride, \
                                gsize         width, \
                                gsize         height) \
{ \
  gsize x, y; \
\
  for (y = 0; y < height; y++) \
    { \
      for (x = 0; x < width; x++) \
        { \
          dest_data[3 * x + R] = src_data[3 * x + 0]; \
          dest_data[3 * x + G] = src_data[3 * x + 1]; \
          dest_data[3 * x + B] = src_data[3 * x + 2]; \
        } \
\
      dest_data += dest_stride; \
      src_data += src_stride; \
    } \
}

SWIZZLE3(2,1,0)

#define SWIZZLE(A,R,G,B) \
static void \
convert_swizzle ## A ## R ## G ## B (guchar       *dest_data, \
                                     gsize         dest_stride, \
                                     const guchar *src_data, \
                                     gsize         src_stride, \
                                     gsize         width, \
                                     gsize         height) \
{ \
  gsize x, y; \
\
  for (y = 0; y < height; y++) \
    { \
      for (x = 0; x < width; x++) \
        { \
          dest_data[4 * x + A] = src_data[4 * x + 0]; \
          dest_data[4 * x + R] = src_data[4 * x + 1]; \
          dest_data[4 * x + G] = src_data[4 * x + 2]; \
          dest_data[4 * x + B] = src_data[4 * x + 3]; \
        } \
\
      dest_data += dest_stride; \
      src_data += src_stride; \
    } \
}

SWIZZLE(3,2,1,0)
SWIZZLE(2,1,0,3)
SWIZZLE(3,0,1,2)
SWIZZLE(1,2,3,0)

#define SWIZZLE_OPAQUE(A,R,G,B) \
static void \
convert_swizzle_opaque_## A ## R ## G ## B (guchar       *dest_data, \
                                            gsize         dest_stride, \
                                            const guchar *src_data, \
                                            gsize         src_stride, \
                                            gsize         width, \
                                            gsize         height) \
{ \
  gsize x, y; \
\
  for (y = 0; y < height; y++) \
    { \
      for (x = 0; x < width; x++) \
        { \
          dest_data[4 * x + A] = 0xFF; \
          dest_data[4 * x + R] = src_data[3 * x + 0]; \
          dest_data[4 * x + G] = src_data[3 * x + 1]; \
          dest_data[4 * x + B] = src_data[3 * x + 2]; \
        } \
\
      dest_data += dest_stride; \
      src_data += src_stride; \
    } \
}

SWIZZLE_OPAQUE(3,2,1,0)
SWIZZLE_OPAQUE(3,0,1,2)
SWIZZLE_OPAQUE(0,1,2,3)
SWIZZLE_OPAQUE(0,3,2,1)

#define PREMULTIPLY(d,c,a) G_STMT_START { guint t = c * a + 0x80; d = ((t >> 8) + t) >> 8; } G_STMT_END
#define SWIZZLE_PREMULTIPLY(A,R,G,B, A2,R2,G2,B2) \
static void \
convert_swizzle_premultiply_ ## A ## R ## G ## B ## _ ## A2 ## R2 ## G2 ## B2 \
                                    (guchar       *dest_data, \
                                     gsize         dest_stride, \
                                     const guchar *src_data, \
                                     gsize         src_stride, \
                                     gsize         width, \
                                     gsize         height) \
{ \
  gsize x, y; \
\
  for (y = 0; y < height; y++) \
    { \
      for (x = 0; x < width; x++) \
        { \
          dest_data[4 * x + A] = src_data[4 * x + A2]; \
          PREMULTIPLY(dest_data[4 * x + R], src_data[4 * x + R2], src_data[4 * x + A2]); \
          PREMULTIPLY(dest_data[4 * x + G], src_data[4 * x + G2], src_data[4 * x + A2]); \
          PREMULTIPLY(dest_data[4 * x + B], src_data[4 * x + B2], src_data[4 * x + A2]); \
        } \
\
      dest_data += dest_stride; \
      src_data += src_stride; \
    } \
}

SWIZZLE_PREMULTIPLY (3,2,1,0, 3,2,1,0)
SWIZZLE_PREMULTIPLY (0,1,2,3, 3,2,1,0)
SWIZZLE_PREMULTIPLY (3,2,1,0, 0,1,2,3)
SWIZZLE_PREMULTIPLY (0,1,2,3, 0,1,2,3)
SWIZZLE_PREMULTIPLY (3,2,1,0, 3,0,1,2)
SWIZZLE_PREMULTIPLY (0,1,2,3, 3,0,1,2)
SWIZZLE_PREMULTIPLY (3,2,1,0, 0,3,2,1)
SWIZZLE_PREMULTIPLY (0,1,2,3, 0,3,2,1)
SWIZZLE_PREMULTIPLY (3,0,1,2, 3,2,1,0)
SWIZZLE_PREMULTIPLY (3,0,1,2, 0,1,2,3)
SWIZZLE_PREMULTIPLY (3,0,1,2, 3,0,1,2)
SWIZZLE_PREMULTIPLY (3,0,1,2, 0,3,2,1)

typedef void (* ConversionFunc) (guchar       *dest_data,
                                 gsize         dest_stride,
                                 const guchar *src_data,
                                 gsize         src_stride,
                                 gsize         width,
                                 gsize         height);

static ConversionFunc converters[GDK_MEMORY_N_FORMATS][GDK_MEMORY_N_FORMATS] =
{
  { convert_memcpy, convert_swizzle3210, convert_swizzle2103, NULL, NULL, NULL, NULL, NULL, NULL },
  { convert_swizzle3210, convert_memcpy, convert_swizzle3012, NULL, NULL, NULL, NULL, NULL, NULL },
  { convert_swizzle2103, convert_swizzle1230, convert_memcpy, NULL, NULL, NULL, NULL, NULL, NULL },
  { convert_swizzle_premultiply_3210_3210, convert_swizzle_premultiply_0123_3210, convert_swizzle_premultiply_3012_3210, convert_memcpy, NULL, NULL, NULL, NULL, NULL },
  { convert_swizzle_premultiply_3210_0123, convert_swizzle_premultiply_0123_0123, convert_swizzle_premultiply_3012_0123, NULL, convert_memcpy, NULL, NULL, NULL, NULL },
  { convert_swizzle_premultiply_3210_3012, convert_swizzle_premultiply_0123_3012, convert_swizzle_premultiply_3012_3012, convert_swizzle2103, convert_swizzle1230, convert_memcpy, convert_swizzle3210, NULL, NULL },
  { convert_swizzle_premultiply_3210_0321, convert_swizzle_premultiply_0123_0321, convert_swizzle_premultiply_3012_0321, NULL, NULL, NULL, convert_memcpy, NULL, NULL },
  { convert_swizzle_opaque_3210, convert_swizzle_opaque_0123, convert_swizzle_opaque_3012, NULL, NULL, NULL, NULL, convert_memcpy3, convert_swizzle210 },
  { convert_swizzle_opaque_3012, convert_swizzle_opaque_0321, convert_swizzle_opaque_3210, NULL, NULL, NULL, NULL, convert_swizzle210, convert_memcpy3 },
};

static void
gdk_memory_convert (guchar          *dest_data,
                    gsize            dest_stride,
                    GdkMemoryFormat  dest_format,
                    const guchar    *src_data,
                    gsize            src_stride,
                    GdkMemoryFormat  src_format,
                    gsize            width,
                    gsize            height)
{
  g_assert (dest_format < GDK_MEMORY_N_FORMATS);
  g_assert (src_format < GDK_MEMORY_N_FORMATS);

  if (converters[src_format][dest_format] == NULL)
    g_error ("Conversion from %s to %s not supported", format_to_string (src_format), format_to_string (dest_format));

  converters[src_format][dest_format] (dest_data, dest_stride, src_data, src_stride, width, height);
}

/* End of copied code */

static GdkTexture *
make_texture (GdkMemoryFormat  format,
              int              padding,
              int             *out_stride,
              int             *out_bpp)
{
  GdkPixbuf *source;
  GdkMemoryFormat source_format;
  int width, height, stride, bpp;
  guchar *data;
  GBytes *bytes;
  GdkTexture *texture;
  GError *error = NULL;

  width = height = 200;

  source = gdk_pixbuf_new_from_file_at_scale ("tests/portland-rose.jpg",
                                              width, height, TRUE,
                                              &error);
  if (!source)
    g_error ("%s", error->message);

  source_format = GDK_MEMORY_R8G8B8;
  bpp = 3;

  if (format != GDK_MEMORY_R8G8B8 && format != GDK_MEMORY_B8G8R8)
    {
      bpp = 4;

      /* add an alpha channel with 50% alpha */
      GdkPixbuf *pb = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
      gdk_pixbuf_composite (source, pb, 0, 0, width, height, 0, 0, 1, 1, GDK_INTERP_BILINEAR, 128);
      g_object_unref (source);
      source = pb;

      source_format = GDK_MEMORY_R8G8B8A8;
    }

  stride = bpp * width + padding;
  data = g_new0 (guchar, stride * height);

  gdk_memory_convert (data, stride, format,
                      gdk_pixbuf_get_pixels (source),
                      gdk_pixbuf_get_rowstride (source),
                      source_format,
                      width, height);

  g_object_unref (source);

  bytes = g_bytes_new_take (data, stride * height);
  texture = gdk_memory_texture_new (width, height, format, bytes, stride);
  g_bytes_unref (bytes);

  *out_stride = stride;
  *out_bpp = bpp;

  return texture;
}

static void
update_picture (GtkWidget *picture)
{
  GdkMemoryFormat format;
  int padding;
  GdkTexture *texture;
  GtkLabel *label;
  char *text;
  int stride;
  int bpp;

  format = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (picture), "format"));
  padding = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (picture), "padding"));

  texture = make_texture (format, padding, &stride, &bpp);
  gtk_picture_set_paintable (GTK_PICTURE (picture), GDK_PAINTABLE (texture));

  label = GTK_LABEL (g_object_get_data (G_OBJECT (picture), "size_label"));
  text = g_strdup_printf ("%d x %d @ %d",
                          gdk_texture_get_width (texture),
                          gdk_texture_get_height (texture),
                          bpp);
  gtk_label_set_label (label, text);
  g_free (text);

  label = GTK_LABEL (g_object_get_data (G_OBJECT (picture), "stride_label"));
  text = g_strdup_printf ("%d", stride);
  gtk_label_set_label (label, text);
  g_free (text);

  g_object_unref (texture);
}

static void
update_format (GtkDropDown *dropdown,
               GParamSpec  *pspec,
               GtkWidget   *picture)
{
  g_object_set_data (G_OBJECT (picture), "format", GINT_TO_POINTER (gtk_drop_down_get_selected (dropdown)));
  update_picture (picture);
}

static void
update_padding (GtkSpinButton *spinbutton,
                GParamSpec    *pspec,
                GtkWidget     *picture)
{
  g_object_set_data (G_OBJECT (picture), "padding", GINT_TO_POINTER (gtk_spin_button_get_value_as_int (spinbutton)));
  update_picture (picture);
}

static void
add_to_grid (GtkWidget      *grid,
             int             left,
             int             top,
             GdkMemoryFormat format,
             int             padding)
{
  GtkWidget *dropdown, *spin, *picture, *label;

  picture = gtk_picture_new ();
  gtk_grid_attach (GTK_GRID (grid), picture, left + 2, top + 0, 1, 4);

  g_object_set_data (G_OBJECT (picture), "format", GINT_TO_POINTER (format));
  g_object_set_data (G_OBJECT (picture), "padding", GINT_TO_POINTER (padding));

  dropdown = gtk_drop_down_new_from_strings (format_name);
  gtk_widget_set_valign (dropdown, GTK_ALIGN_CENTER);
  gtk_drop_down_set_selected (GTK_DROP_DOWN (dropdown), format);
  g_signal_connect (dropdown, "notify::selected", G_CALLBACK (update_format), picture);

  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Format"), left, top, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), dropdown, left + 1, top, 1, 1);

  spin = gtk_spin_button_new_with_range (0, 10, 1);
  gtk_widget_set_valign (spin, GTK_ALIGN_CENTER);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), padding);
  g_signal_connect (spin, "notify::value", G_CALLBACK (update_padding), picture);

  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Padding"), left, top + 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), spin, left + 1, top + 1, 1, 1);

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  g_object_set_data (G_OBJECT (picture), "size_label", label);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Size"), left, top + 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), label, left + 1, top + 2, 1, 1);
  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  g_object_set_data (G_OBJECT (picture), "stride_label", label);
  gtk_grid_attach (GTK_GRID (grid), gtk_label_new ("Stride"), left, top + 3, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), label, left + 1, top + 3, 1, 1);

  update_picture (picture);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *grid;

  gtk_init ();

  window = gtk_window_new ();
  grid = gtk_grid_new ();
  gtk_widget_set_margin_top (grid, 10);
  gtk_widget_set_margin_bottom (grid, 10);
  gtk_widget_set_margin_start (grid, 10);
  gtk_widget_set_margin_end (grid, 10);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_window_set_child (GTK_WINDOW (window), grid);

  add_to_grid (grid, 0, 0, GDK_MEMORY_R8G8B8, 0);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
