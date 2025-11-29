#include <gtk/gtk.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/dma-heap.h>
#include <gdk/gdkdmabuffourccprivate.h>

#ifdef GDK_RENDERING_VULKAN
#include <vulkan/vulkan.h>
#endif

#include "gtkclipperprivate.h"

/* For this to work, you may need to give /dev/dma_heap/system
 * lax permissions.
 */

static int dma_heap_fd = -1;
#ifdef GDK_RENDERING_VULKAN
static uint32_t vk_memory_type_index = 0;
static VkDevice vk_device = VK_NULL_HANDLE;
#endif

static gboolean
initialize_dma_heap (void)
{
  dma_heap_fd = open ("/dev/dma_heap/system", O_RDONLY | O_CLOEXEC);
  return dma_heap_fd != -1;
}

static gboolean
initialize_vulkan (void)
{
#ifdef GDK_RENDERING_VULKAN
  VkInstance vk_instance;
  VkPhysicalDevice vk_physical_device;
  VkResult res;
  uint32_t i, n_devices = 1;
  VkPhysicalDeviceMemoryProperties properties;

  if (vkCreateInstance (&(VkInstanceCreateInfo) {
                          .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                          .pApplicationInfo = &(VkApplicationInfo) {
                            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                            .pApplicationName = g_get_application_name (),
                            .applicationVersion = 0,
                            .pEngineName = "GTK testsuite",
                            .engineVersion = VK_MAKE_VERSION (GDK_MAJOR_VERSION, GDK_MINOR_VERSION, GDK_MICRO_VERSION),
                            .apiVersion = VK_API_VERSION_1_0,
                          },
                          .enabledExtensionCount = 3,
                          .ppEnabledExtensionNames = (const char * [3]) {
                            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
                            VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
                            VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
                          }
                        },
                        NULL,
                        &vk_instance) != VK_SUCCESS)
    return FALSE;

  res = vkEnumeratePhysicalDevices (vk_instance, &n_devices, &vk_physical_device);
  if (res != VK_SUCCESS && res != VK_INCOMPLETE)
    {
      vkDestroyInstance (vk_instance, NULL);
      return FALSE;
    }

  if (vkCreateDevice (vk_physical_device,
                      &(VkDeviceCreateInfo) {
                        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                        .queueCreateInfoCount = 1,
                        .pQueueCreateInfos = &(VkDeviceQueueCreateInfo) {
                          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                          .queueFamilyIndex = 0,
                          .queueCount = 1,
                          .pQueuePriorities = (float []) { 1.0f },
                        },
                        .enabledExtensionCount = 11,
                        .ppEnabledExtensionNames = (const char * [11]) {
                          VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
                          VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
                          VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
                          VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
                          VK_KHR_MAINTENANCE_1_EXTENSION_NAME,
                          VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
                          VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
                          VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
                          VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
                          VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
                          VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
                        },
                      },
                      NULL,
                      &vk_device) != VK_SUCCESS)
    {
      vkDestroyInstance (vk_instance, NULL);
      return FALSE;
    }

  vkGetPhysicalDeviceMemoryProperties (vk_physical_device, &properties);

#define REQUIRED_FLAGS (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
  for (i = 0; i < properties.memoryTypeCount; i++)
    {
      if ((properties.memoryTypes[i].propertyFlags & REQUIRED_FLAGS) == REQUIRED_FLAGS)
        break;
    }
#undef REQUIRED_FLAGS

  if (i >= properties.memoryTypeCount)
    {
      vkDestroyDevice (vk_device, NULL);
      vkDestroyInstance (vk_instance, NULL);
      vk_device = VK_NULL_HANDLE;
      return FALSE;
    }

  vk_memory_type_index = i;

  return TRUE;
#else
  return FALSE;
#endif
}

#ifdef GDK_RENDERING_VULKAN
static int
allocate_vulkan (gsize size)
{
  VkDeviceMemory vk_memory;
  PFN_vkGetMemoryFdKHR func_vkGetMemoryFdKHR;
  int fd;

  func_vkGetMemoryFdKHR = (PFN_vkGetMemoryFdKHR) vkGetDeviceProcAddr (vk_device, "vkGetMemoryFdKHR");

  if (vkAllocateMemory (vk_device,
                        &(VkMemoryAllocateInfo) {
                          .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                          .allocationSize = size,
                          .memoryTypeIndex = vk_memory_type_index,
                          .pNext = &(VkExportMemoryAllocateInfo) {
                            .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
                            .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT
                          },
                        },
                        NULL,
                        &vk_memory) != VK_SUCCESS)
    return -1;

  if (func_vkGetMemoryFdKHR (vk_device,
                             &(VkMemoryGetFdInfoKHR) {
                               .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
                               .memory = vk_memory,
                               .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                             },
                             &fd) != VK_SUCCESS)
    return -1;

  return fd;
}
#endif

static int
allocate_dma_buf (gsize size)
{
  struct dma_heap_allocation_data heap_data;
  int ret;

  heap_data.len = size;
  heap_data.fd = 0;
  heap_data.fd_flags = O_RDWR | O_CLOEXEC;
  heap_data.heap_flags = 0;

  ret = ioctl (dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &heap_data);
  if (ret)
    g_error ("dma-buf allocation failed");

  return heap_data.fd;
}

static int
allocate_memfd (gsize size)
{
  int fd;

  fd = memfd_create ("buffer", MFD_CLOEXEC);
  if (fd == -1)
    g_error ("memfd allocation failed");

  ftruncate (fd, size);

  return fd;
}

static int
allocate_buffer (gsize size)
{
#ifdef GDK_RENDERING_VULKAN
  if (vk_device)
    return allocate_vulkan (size);
#endif
  if (dma_heap_fd != -1)
    return allocate_dma_buf (size);
  else
    return allocate_memfd (size);
}

static void
populate_buffer (int           fd,
                 const guchar *data,
                 gsize         size)
{
  guchar *buf;

  buf = mmap (NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
  memcpy (buf, data, size);
  munmap (buf, size);
}


/* The YUV conversion code is adapted from weston/tests/yuv-buffer-test.c */

/*
 * Based on Rec. ITU-R BT.601-7
 *
 * This is intended to be obvious and accurate, not fast.
 */
static void
x8r8g8b8_to_ycbcr8_bt601 (guint32 xrgb,
                          guchar *y_out,
                          guchar *cb_out,
                          guchar *cr_out)
{
  double y, cb, cr;
  double r = (xrgb >> 16) & 0xff;
  double g = (xrgb >> 8) & 0xff;
  double b = (xrgb >> 0) & 0xff;

  /* normalize to [0.0, 1.0] */
  r /= 255.0;
  g /= 255.0;
  b /= 255.0;

  /* Y normalized to [0.0, 1.0], Cb and Cr [-0.5, 0.5] */
  y = 0.299 * r + 0.587 * g + 0.114 * b;
  cr = (r - y) / 1.402;
  cb = (b - y) / 1.772;

  /* limited range quantization to 8 bit */
  *y_out = round(219.0 * y + 16.0);
  if (cr_out)
    *cr_out = round(224.0 * cr + 128.0);
  if (cb_out)
    *cb_out = round(224.0 * cb + 128.0);
}

/*
 * 3 plane YCbCr
 * plane 0: Y plane, [7:0] Y
 * plane 1: Cb plane, [7:0] Cb
 * plane 2: Cr plane, [7:0] Cr
 * YUV420: 2x2 subsampled Cb (1) and Cr (2) planes
 * YUV444: no subsampling
 */
static guchar *
y_u_v_create_buffer (uint32_t  drm_format,
                     guchar   *rgb_data,
                     int       rgb_width,
                     int       rgb_height,
                     int      *size,
                     int      *u_offset,
                     int      *v_offset)
{
  gsize bytes;
  int x, y;
  guint32 *rgb_row;
  guchar *y_base;
  guchar *u_base;
  guchar *v_base;
  guchar *y_row;
  guchar *u_row;
  guchar *v_row;
  guint32 argb;
  int sub = (drm_format == DRM_FORMAT_YUV420) ? 2 : 1;
  guchar *buf;

  g_assert (drm_format == DRM_FORMAT_YUV420 ||
            drm_format == DRM_FORMAT_YUV444);

  /* Full size Y plus quarter U and V */
  bytes = rgb_width * rgb_height +
          (rgb_width / sub) * (rgb_height / sub) * 2;

  buf = g_new (guchar, bytes);

  *size = bytes;
  *u_offset = rgb_width * rgb_height;
  *v_offset = *u_offset + (rgb_width / sub) * (rgb_height / sub);

  y_base = buf;
  u_base = y_base + rgb_width * rgb_height;
  v_base = u_base + (rgb_width / sub) * (rgb_height / sub);

  for (y = 0; y < rgb_height; y++)
    {
      rgb_row = (guint32 *) (rgb_data + y * 4 * rgb_width);
      y_row = y_base + y * rgb_width;
      u_row = u_base + (y / sub) * (rgb_width / sub);
      v_row = v_base + (y / sub) * (rgb_width / sub);

      for (x = 0; x < rgb_width; x++)
        {
          /*
           * Sub-sample the source image instead, so that U and V
           * sub-sampling does not require proper
           * filtering/averaging/siting.
           */
          argb = rgb_row[x];

          /*
           * A stupid way of "sub-sampling" chroma. This does not
           * do the necessary filtering/averaging/siting or
           * alternate Cb/Cr rows.
           */
          if ((y & (sub - 1)) == 0 && (x & (sub - 1)) == 0)
            x8r8g8b8_to_ycbcr8_bt601 (argb, y_row + x, u_row + x / sub, v_row + x / sub);
          else
            x8r8g8b8_to_ycbcr8_bt601 (argb, y_row + x, NULL, NULL);
        }
    }

  return buf;
}

/*
 * 2 plane YCbCr
 * plane 0 = Y plane, [7:0] Y
 * plane 1 = Cr:Cb plane, [15:0] Cr:Cb little endian
 * 2x2 subsampled Cr:Cb plane
 */
static guchar *
nv12_create_buffer (uint32_t drm_format,
                    guchar   *rgb_data,
                    int       rgb_width,
                    int       rgb_height,
                    int      *size,
                    int      *uv_offset)
{
  size_t bytes;
  int x, y;
  uint32_t *rgb_row;
  uint8_t *y_base;
  uint8_t *uv_base;
  uint8_t *y_row;
  uint16_t *uv_row;
  uint32_t argb;
  uint8_t cr;
  uint8_t cb;
  guchar *buf;

  g_assert (drm_format == DRM_FORMAT_NV12);

  /* Full size Y plane, half size UV plane */
  bytes = rgb_width * rgb_height +
          rgb_width * (rgb_height / 2);
  *size = bytes;

  buf = g_new0 (guchar, bytes);

  *uv_offset = rgb_width * rgb_height;
  y_base = buf;
  uv_base = y_base + rgb_width * rgb_height;

  for (y = 0; y < rgb_height; y++)
    {
      rgb_row = (uint32_t *) (rgb_data + y * 4 * rgb_width);
      y_row = y_base + y * rgb_width;
      uv_row = (uint16_t *) (uv_base + (y / 2) * rgb_width);

      for (x = 0; x < rgb_width; x++)
        {
          /*
           * Sub-sample the source image instead, so that U and V
           * sub-sampling does not require proper
           * filtering/averaging/siting.
           */
          argb = rgb_row[x];

          /*
           * A stupid way of "sub-sampling" chroma. This does not
           * do the necessary filtering/averaging/siting.
           */
          if ((y & 1) == 0 && (x & 1) == 0)
            {
              x8r8g8b8_to_ycbcr8_bt601(argb, y_row + x, &cb, &cr);
              *(uv_row + x / 2) = ((uint16_t)cr << 8) | cb;
            }
          else
            {
              x8r8g8b8_to_ycbcr8_bt601(argb, y_row + x, NULL, NULL);
            }
        }
    }

  return buf;
}

static void
texture_builder_set_planes (GdkDmabufTextureBuilder *builder,
                            gboolean                 disjoint,
                            const guchar            *buf,
                            unsigned                 size,
                            unsigned                 n_planes,
                            unsigned                 strides[4],
                            unsigned                 sizes[4])
{
  gdk_dmabuf_texture_builder_set_n_planes (builder, n_planes);

  if (disjoint)
    {
      unsigned offset = 0;
      unsigned i;

      for (i = 0; i < n_planes; i++)
        {
          int fd = allocate_buffer (sizes[i]);
          populate_buffer (fd, buf + offset, sizes[i]);

          gdk_dmabuf_texture_builder_set_fd (builder, i, fd);
          gdk_dmabuf_texture_builder_set_stride (builder, i, strides[i]);
          gdk_dmabuf_texture_builder_set_offset (builder, i, 0);
          offset += sizes[i];
        }
    }
  else
    {
      unsigned offset = 0;
      unsigned i;
      int fd = allocate_buffer (size);
      populate_buffer (fd, buf, size);

      for (i = 0; i < n_planes; i++)
        {
          gdk_dmabuf_texture_builder_set_fd (builder, i, fd);
          gdk_dmabuf_texture_builder_set_stride (builder, i, strides[i]);
          gdk_dmabuf_texture_builder_set_offset (builder, i, offset);
          offset += sizes[i];
        }
    }
}

static GdkTexture *
make_dmabuf_texture (GdkTexture *texture,
                     guint32     format,
                     gboolean    disjoint,
                     gboolean    premultiplied,
                     gboolean    flip)
{
  GdkTextureDownloader *downloader;
  int width, height;
  gsize rgb_stride, rgb_size;
  guchar *rgb_data;
  int fd;
  GdkDmabufTextureBuilder *builder;
  GError *error = NULL;

  if (initialize_vulkan ())
    g_print ("Using Vulkan\n");
  else if (initialize_dma_heap ())
    g_print ("Using dma_heap\n");
  else
    g_print ("Using memfd\n");

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);
  rgb_stride = 4 * width;
  rgb_size = rgb_stride * height;

  rgb_data = g_new0 (guchar, rgb_size);

  downloader = gdk_texture_downloader_new (texture);

  if (premultiplied)
    gdk_texture_downloader_set_format (downloader, GDK_MEMORY_B8G8R8A8_PREMULTIPLIED);
  else
    gdk_texture_downloader_set_format (downloader, GDK_MEMORY_B8G8R8A8);

  gdk_texture_downloader_download_into (downloader, rgb_data, rgb_stride);
  gdk_texture_downloader_free (downloader);

  if (flip)
    {
      for (int y = 0; y < height; y++)
        {
          guint32 *row = (guint32 *) (rgb_data + y * rgb_stride);
          for (int x = 0; x < width / 2; x++)
            {
              guint32 p = row[x];
              row[x] = row[width - 1 - x];
              row[width - 1 - x] = p;
            }
        }
    }

  builder = gdk_dmabuf_texture_builder_new ();

  gdk_dmabuf_texture_builder_set_display (builder, gdk_display_get_default ());
  gdk_dmabuf_texture_builder_set_width (builder, width);
  gdk_dmabuf_texture_builder_set_height (builder, height);
  gdk_dmabuf_texture_builder_set_fourcc (builder, format);
  gdk_dmabuf_texture_builder_set_modifier (builder, DRM_FORMAT_MOD_LINEAR);
  gdk_dmabuf_texture_builder_set_premultiplied (builder, premultiplied);

  if (format == DRM_FORMAT_XRGB8888 ||
      format == DRM_FORMAT_ARGB8888)
    {
      gdk_dmabuf_texture_builder_set_n_planes (builder, 1);

      fd = allocate_buffer (rgb_size);
      populate_buffer (fd, rgb_data, rgb_size);

      gdk_dmabuf_texture_builder_set_fd (builder, 0, fd);
      gdk_dmabuf_texture_builder_set_stride (builder, 0, rgb_stride);
    }
  else if (format == DRM_FORMAT_XRGB8888_A8)
    {
      guchar *alpha_data;
      gsize alpha_stride;
      gsize alpha_size;

      gdk_dmabuf_texture_builder_set_n_planes (builder, 2);

      fd = allocate_buffer (rgb_size);
      populate_buffer (fd, rgb_data, rgb_size);

      gdk_dmabuf_texture_builder_set_fd (builder, 0, fd);
      gdk_dmabuf_texture_builder_set_stride (builder, 0, rgb_stride);

      alpha_stride = width;
      alpha_size = alpha_stride * height;
      alpha_data = g_new0 (guchar, alpha_size);

      for (gsize i = 0; i <  height; i++)
        for (gsize j = 0; j < width; j++)
          alpha_data[i * alpha_stride + j] = rgb_data[i * rgb_stride + j * 4 + 3];

      fd = allocate_buffer (alpha_size);
      populate_buffer (fd, alpha_data, alpha_size);

      gdk_dmabuf_texture_builder_set_fd (builder, 1, fd);
      gdk_dmabuf_texture_builder_set_stride (builder, 1, alpha_stride);

      g_free (alpha_data);
    }
  else if (format == DRM_FORMAT_YUV420)
    {
      guchar *buf;
      int size, u_offset, v_offset;

      buf = y_u_v_create_buffer (format, rgb_data, width, height, &size, &u_offset, &v_offset);

      texture_builder_set_planes (builder,
                                  disjoint,
                                  buf,
                                  size,
                                  3,
                                  (unsigned[4]) { width, width / 2, width / 2 }, 
                                  (unsigned[4]) { width * height, width * height / 4, width * height / 4 });

      g_free (buf);
    }
  else if (format == DRM_FORMAT_NV12)
    {
      guchar *buf;
      int size, uv_offset;

      buf = nv12_create_buffer (format, rgb_data, width, height, &size, &uv_offset);

      texture_builder_set_planes (builder,
                                  disjoint,
                                  buf,
                                  size,
                                  2,
                                  (unsigned[4]) { width, width, }, 
                                  (unsigned[4]) { width * height, width * height / 2 });

      g_free (buf);
    }

  g_free (rgb_data);

  texture = gdk_dmabuf_texture_builder_build (builder, NULL, NULL, &error);
  if (!texture)
    g_error ("Failed to create dmabuf texture: %s", error->message);

  g_object_unref (builder);

  return texture;
}

static guint32 supported_formats[] = {
  DRM_FORMAT_ARGB8888,
  DRM_FORMAT_XRGB8888,
  DRM_FORMAT_YUV420,
  DRM_FORMAT_NV12,
  DRM_FORMAT_XRGB8888_A8,
};

static gboolean
format_is_supported (guint32 fmt)
{
  for (int i = 0; i < G_N_ELEMENTS (supported_formats); i++)
    {
      if (supported_formats[i] == fmt)
        return TRUE;
    }

  return FALSE;
}

static char *
supported_formats_to_string (void)
{
  GString *s;

  s = g_string_new ("");
  for (int i = 0; i < G_N_ELEMENTS (supported_formats); i++)
    {
      if (s->len)
        g_string_append (s, ", ");
      g_string_append_printf (s, "%.4s", (char *)&supported_formats[i]);
    }

  return g_string_free (s, FALSE);
}

G_GNUC_NORETURN
static void
usage (void)
{
  char *formats = supported_formats_to_string ();
  g_print ("Usage: testdmabuf [--undecorated][--disjoint][--download-to FILE][--padding PADDING] FORMAT FILE\n"
           "Supported formats: %s\n", formats);
  g_free (formats);
  exit (1);
}

static guint32
parse_format (const char *a)
{
  if (strlen (a) == 4)
    {
      guint32 format = fourcc_code (a[0], a[1], a[2], a[3]);
      if (format_is_supported (format))
        return format;
    }

  usage ();
  return 0;
}

static gboolean
toggle_fullscreen (GtkWidget *widget,
                   GVariant  *args,
                   gpointer   data)
{
  GtkWindow *window = GTK_WINDOW (widget);

  if (gtk_window_is_fullscreen (window))
    gtk_window_unfullscreen (window);
  else
    gtk_window_fullscreen (window);

  return TRUE;
}

static gboolean
toggle_overlay (GtkWidget *widget,
                GVariant  *args,
                gpointer   data)
{
  static GtkWidget *child = NULL;
  GtkOverlay *overlay = data;

  if (child)
    {
      gtk_overlay_remove_overlay (overlay, child);
      child = NULL;
    }
  else
    {
      GtkWidget *spinner;
      spinner = gtk_spinner_new ();
      gtk_spinner_start (GTK_SPINNER (spinner));
      child = gtk_box_new (0, FALSE);
      gtk_box_append (GTK_BOX (child), spinner);
      gtk_box_append (GTK_BOX (child), gtk_image_new_from_icon_name ("media-playback-start-symbolic"));
      gtk_widget_set_halign (child, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (child, GTK_ALIGN_CENTER);
      gtk_overlay_add_overlay (overlay, child);
    }

  return TRUE;
}

static GdkTexture *texture;
static GdkTexture *texture_flipped;

static gboolean
toggle_flip (GtkWidget *widget,
             GVariant  *args,
             gpointer   data)
{
  GtkPicture *picture = data;

  if (!texture_flipped)
    return FALSE;

  if (gtk_picture_get_paintable (picture) == GDK_PAINTABLE (texture))
    gtk_picture_set_paintable (picture, GDK_PAINTABLE (texture_flipped));
  else
    gtk_picture_set_paintable (picture, GDK_PAINTABLE (texture));

  return TRUE;
}

static gboolean
toggle_start (GtkWidget *widget,
              GVariant  *args,
              gpointer   data)
{
  GtkWidget *offload = data;

  if (gtk_widget_get_halign (offload) == GTK_ALIGN_CENTER)
    gtk_widget_set_halign (offload, GTK_ALIGN_START);
  else
    gtk_widget_set_halign (offload, GTK_ALIGN_CENTER);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *offload, *picture, *overlay;
  char *filename;
  guint32 format;
  gboolean disjoint = FALSE;
  gboolean premultiplied = TRUE;
  gboolean decorated = TRUE;
  gboolean fullscreen = FALSE;
  unsigned int i;
  const char *save_filename = NULL;
  GtkEventController *controller;
  GtkShortcutTrigger *trigger;
  GtkShortcutAction *action;
  GtkShortcut *shortcut;
  GdkPaintable *paintable;
  GdkTexture *orig;
  int padding[4] = { 0, }; /* left, right, top, bottom */
  int padding_set = 0;

  for (i = 1; i < argc; i++)
    {
      if (g_str_equal (argv[i], "--disjoint"))
        disjoint = TRUE;
      else if (g_str_equal (argv[i], "--undecorated"))
        decorated = FALSE;
      else if (g_str_equal (argv[i], "--fullscreen"))
        fullscreen = TRUE;
      else if (g_str_equal (argv[i], "--unpremultiplied"))
        premultiplied = FALSE;
      else if (g_str_equal (argv[i], "--download-to"))
        {
          i++;
          if (i == argc)
            usage ();

          save_filename = argv[i];
        }
      else if (g_str_equal (argv[i], "--padding"))
        {
          if (padding_set < 4)
            {
              char **strv;

              i++;
              if (i == argc)
                usage ();

              strv = g_strsplit (argv[i], ",", 0);
              if (g_strv_length (strv) > 4)
                g_error ("Too much padding");

              for (padding_set = 0; padding_set < 4; padding_set++)
                {
                  guint64 num;
                  GError *error = NULL;

                  if (!strv[padding_set])
                    break;

                  if (!g_ascii_string_to_unsigned (strv[padding_set], 10, 0, 100, &num, &error))
                    g_error ("%s", error->message);

                  padding[padding_set] = (int) num;
               }
            }
          else
            g_error ("Too much padding");
        }
      else
        break;
    }

  if (argc - i != 2)
    {
      usage ();
      return 1;
    }

  format = parse_format (argv[argc - 2]);
  filename = argv[argc - 1];

  gtk_init ();

  /* Get the list of supported formats with GDK_DEBUG=opengl */
  gdk_display_get_dmabuf_formats (gdk_display_get_default ());

  orig = gdk_texture_new_from_filename (filename, NULL);
  texture = make_dmabuf_texture (orig, format, disjoint, premultiplied, FALSE);
  texture_flipped = make_dmabuf_texture (orig, format, disjoint, premultiplied, TRUE);
  g_object_unref (orig);

  if (padding_set > 0)
    {
      paintable = gtk_clipper_new (GDK_PAINTABLE (texture),
                                   &GRAPHENE_RECT_INIT (padding[0],
                                                        padding[2],
                                                        gdk_texture_get_width (texture) - padding[0] - padding[1],
                                                        gdk_texture_get_height (texture) - padding[2] - padding[3]));
    }
  else
    paintable = GDK_PAINTABLE (texture);

  if (save_filename)
    gdk_texture_save_to_png (texture, save_filename);

  window = gtk_window_new ();
  gtk_window_set_decorated (GTK_WINDOW (window), decorated);
  if (fullscreen)
    gtk_window_fullscreen (GTK_WINDOW (window));

  picture = gtk_picture_new_for_paintable (paintable);
  offload = gtk_graphics_offload_new (picture);
  gtk_widget_set_halign (offload, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (offload, GTK_ALIGN_CENTER);
  overlay = gtk_overlay_new ();

  gtk_overlay_set_child (GTK_OVERLAY (overlay), offload);
  gtk_window_set_child (GTK_WINDOW (window), overlay);

  controller = gtk_shortcut_controller_new ();

  trigger = gtk_keyval_trigger_new (GDK_KEY_F11, GDK_NO_MODIFIER_MASK);
  action = gtk_callback_action_new (toggle_fullscreen, NULL, NULL);
  shortcut = gtk_shortcut_new (trigger, action);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  trigger = gtk_keyval_trigger_new (GDK_KEY_O, GDK_CONTROL_MASK);
  action = gtk_callback_action_new (toggle_overlay, overlay, NULL);
  shortcut = gtk_shortcut_new (trigger, action);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  trigger = gtk_keyval_trigger_new (GDK_KEY_F, GDK_CONTROL_MASK);
  action = gtk_callback_action_new (toggle_flip, picture, NULL);
  shortcut = gtk_shortcut_new (trigger, action);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  trigger = gtk_keyval_trigger_new (GDK_KEY_S, GDK_CONTROL_MASK);
  action = gtk_callback_action_new (toggle_start, offload, NULL);
  shortcut = gtk_shortcut_new (trigger, action);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  gtk_widget_add_controller (window, controller);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
