#include "config.h"

#include "gskvulkandeviceprivate.h"

#include "gskgpuglobalsopprivate.h"
#include "gskgpushaderopprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkanimageprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkvulkancontextprivate.h"

struct _GskVulkanDevice
{
  GskGpuDevice parent_instance;

  GskVulkanAllocator *allocators[VK_MAX_MEMORY_TYPES];
  GskVulkanAllocator *external_allocator;
  GdkVulkanFeatures features;

  guint max_immutable_samplers;
  guint max_samplers;
  guint max_buffers;

  GHashTable *conversion_cache;
  GHashTable *render_pass_cache;
  GHashTable *pipeline_layouts;
  GskVulkanPipelineLayout *pipeline_layout_cache;

  VkCommandPool vk_command_pool;
  VkSampler vk_samplers[GSK_GPU_SAMPLER_N_SAMPLERS];
};

struct _GskVulkanDeviceClass
{
  GskGpuDeviceClass parent_class;
};

G_DEFINE_TYPE (GskVulkanDevice, gsk_vulkan_device, GSK_TYPE_GPU_DEVICE)

typedef struct _ConversionCacheEntry ConversionCacheEntry;
typedef struct _PipelineCacheKey PipelineCacheKey;
typedef struct _RenderPassCacheKey RenderPassCacheKey;
typedef struct _GskVulkanPipelineLayoutSetup GskVulkanPipelineLayoutSetup;

struct _GskVulkanPipelineLayoutSetup
{
  gsize n_buffers;
  gsize n_samplers;
  gsize n_immutable_samplers;
  VkSampler *immutable_samplers;
};

struct _GskVulkanPipelineLayout
{
  gint ref_count;

  VkDescriptorSetLayout vk_buffer_set_layout;
  VkDescriptorSetLayout vk_image_set_layout;
  VkPipelineLayout vk_pipeline_layout;
  GHashTable *pipeline_cache;

  GskVulkanPipelineLayoutSetup setup;
  VkSampler samplers[];
};

struct _ConversionCacheEntry
{
  VkFormat vk_format;

  /* actual data */
  VkSamplerYcbcrConversion vk_conversion;
  VkSampler vk_sampler;
};

struct _PipelineCacheKey
{
  const GskGpuShaderOpClass *op_class;
  guint32 variation;
  GskGpuShaderClip clip;
  GskGpuBlend blend;
  VkFormat format;
  VkPipeline pipeline;
};

struct _RenderPassCacheKey
{
  VkFormat format;
  VkImageLayout from_layout;
  VkImageLayout to_layout;
  VkRenderPass render_pass;
};

static guint
conversion_cache_entry_hash (gconstpointer data)
{
  const ConversionCacheEntry *key = data;

  return key->vk_format;
}

static gboolean
conversion_cache_entry_equal (gconstpointer a,
                              gconstpointer b)
{
  const ConversionCacheEntry *keya = a;
  const ConversionCacheEntry *keyb = b;

  return keya->vk_format == keyb->vk_format;
}

static guint
pipeline_cache_key_hash (gconstpointer data)
{
  const PipelineCacheKey *key = data;

  return GPOINTER_TO_UINT (key->op_class) ^
         key->clip ^
         (key->variation << 2) ^
         (key->blend << 6) ^
         (key->format << 8);
}

static gboolean
pipeline_cache_key_equal (gconstpointer a,
                          gconstpointer b)
{
  const PipelineCacheKey *keya = a;
  const PipelineCacheKey *keyb = b;

  return keya->op_class == keyb->op_class &&
         keya->variation == keyb->variation &&
         keya->clip == keyb->clip &&
         keya->blend == keyb->blend &&
         keya->format == keyb->format;
}

static guint
render_pass_cache_key_hash (gconstpointer data)
{
  const RenderPassCacheKey *key = data;

  return (key->from_layout << 20) ^
         (key->to_layout << 16) ^
         (key->format);
}

static gboolean
render_pass_cache_key_equal (gconstpointer a,
                             gconstpointer b)
{
  const RenderPassCacheKey *keya = a;
  const RenderPassCacheKey *keyb = b;

  return keya->from_layout == keyb->from_layout &&
         keya->to_layout == keyb->to_layout &&
         keya->format == keyb->format;
}

static GskVulkanPipelineLayout *
gsk_vulkan_pipeline_layout_new (GskVulkanDevice                    *self,
                                const GskVulkanPipelineLayoutSetup *setup)
{
  GskVulkanPipelineLayout *layout;
  GdkDisplay *display;
  gboolean descriptor_indexing;
  
  descriptor_indexing = gsk_vulkan_device_has_feature (self, GDK_VULKAN_FEATURE_DESCRIPTOR_INDEXING);

  layout = g_malloc (sizeof (GskVulkanPipelineLayout) + setup->n_immutable_samplers * sizeof (VkSampler));
  layout->ref_count = 1;

  layout->setup = *setup;
  if (setup->n_immutable_samplers)
    memcpy (layout->samplers, setup->immutable_samplers, setup->n_immutable_samplers * sizeof (VkSampler));
  layout->setup.immutable_samplers = layout->samplers;
  layout->pipeline_cache = g_hash_table_new (pipeline_cache_key_hash, pipeline_cache_key_equal);

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));

  GSK_VK_CHECK (vkCreateDescriptorSetLayout, display->vk_device,
                                             &(VkDescriptorSetLayoutCreateInfo) {
                                                 .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                 .bindingCount = 2,
                                                 .flags = 0,
                                                 .pBindings = (VkDescriptorSetLayoutBinding[2]) {
                                                     {
                                                         .binding = 0,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = MAX (1, layout->setup.n_immutable_samplers),
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = layout->setup.n_immutable_samplers
                                                                               ? layout->setup.immutable_samplers
                                                                               : (VkSampler[1]) {
                                                                                   gsk_vulkan_device_get_vk_sampler (self, GSK_GPU_SAMPLER_DEFAULT)
                                                                               },
                                                     },
                                                     {
                                                         .binding = 1,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = layout->setup.n_samplers,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                                                     }
                                                 },
                                                 .pNext = !descriptor_indexing ? NULL : &(VkDescriptorSetLayoutBindingFlagsCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                                                   .bindingCount = 2,
                                                   .pBindingFlags = (VkDescriptorBindingFlags[2]) {
                                                     VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
                                                     VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                                                     | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
                                                   },
                                                 }
                                             },
                                             NULL,
                                             &layout->vk_image_set_layout);

  GSK_VK_CHECK (vkCreateDescriptorSetLayout, display->vk_device,
                                             &(VkDescriptorSetLayoutCreateInfo) {
                                                 .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                 .bindingCount = 1,
                                                 .flags = 0,
                                                 .pBindings = (VkDescriptorSetLayoutBinding[1]) {
                                                     {
                                                         .binding = 0,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                         .descriptorCount = layout->setup.n_buffers,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                                                     },
                                                 },
                                                 .pNext = !descriptor_indexing ? NULL : &(VkDescriptorSetLayoutBindingFlagsCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                                                   .bindingCount = 1,
                                                   .pBindingFlags = (VkDescriptorBindingFlags[1]) {
                                                     VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                                                     | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
                                                   },
                                                 }
                                             },
                                             NULL,
                                             &layout->vk_buffer_set_layout);

  GSK_VK_CHECK (vkCreatePipelineLayout, display->vk_device,
                                        &(VkPipelineLayoutCreateInfo) {
                                            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                            .setLayoutCount = 2,
                                            .pSetLayouts = (VkDescriptorSetLayout[2]) {
                                                layout->vk_image_set_layout,
                                                layout->vk_buffer_set_layout,
                                            },
                                            .pushConstantRangeCount = 1,
                                            .pPushConstantRanges = (VkPushConstantRange[1]) {
                                                {
                                                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                                    .offset = 0,
                                                    .size = sizeof (GskGpuGlobalsInstance)
                                                }
                                            }
                                        },
                                        NULL,
                                        &layout->vk_pipeline_layout);

  g_hash_table_insert (self->pipeline_layouts, &layout->setup, layout);

  return layout;
}

static void
gsk_vulkan_pipeline_layout_unref (GskVulkanDevice         *self,
                                  GskVulkanPipelineLayout *layout)
{
  GdkDisplay *display;
  GHashTableIter iter;
  gpointer key, value;

  layout->ref_count--;
  if (layout->ref_count)
    return;

  if (!g_hash_table_remove (self->pipeline_layouts, &layout->setup))
    {
      g_assert_not_reached ();
    }

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));

  vkDestroyDescriptorSetLayout (display->vk_device,
                                layout->vk_image_set_layout,
                                NULL);
  vkDestroyDescriptorSetLayout (display->vk_device,
                                layout->vk_buffer_set_layout,
                                NULL);

  vkDestroyPipelineLayout (display->vk_device,
                           layout->vk_pipeline_layout,
                           NULL);

  g_hash_table_iter_init (&iter, layout->pipeline_cache);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      vkDestroyPipeline (display->vk_device,
                         ((PipelineCacheKey *)key)->pipeline,
                         NULL);
      g_free (key);
    }
  g_hash_table_unref (layout->pipeline_cache);

  g_free (layout);
}

static void
gsk_vulkan_pipeline_layout_ref (GskVulkanDevice         *self,
                                GskVulkanPipelineLayout *layout)
{
  layout->ref_count++;
}

static guint
gsk_vulkan_pipeline_layout_setup_hash (gconstpointer data)
{
  const GskVulkanPipelineLayoutSetup *setup = data;
  guint result;
  gsize i;

  result = (setup->n_buffers << 23) |
           (setup->n_samplers << 7) |
           setup->n_immutable_samplers;

  for (i = 0; i < setup->n_immutable_samplers; i++)
    {
      result = (result << 13) ^
               GPOINTER_TO_SIZE (setup->immutable_samplers[i]) ^
               (GPOINTER_TO_SIZE (setup->immutable_samplers[i]) >> 32);
    }

  return result;
}

static gboolean
gsk_vulkan_pipeline_layout_setup_equal (gconstpointer a,
                                        gconstpointer b)
{
  const GskVulkanPipelineLayoutSetup *setupa = a;
  const GskVulkanPipelineLayoutSetup *setupb = b;
  gsize i;

  if (setupa->n_buffers != setupb->n_buffers ||
      setupa->n_samplers != setupb->n_samplers ||
      setupa->n_immutable_samplers != setupb->n_immutable_samplers)
    return FALSE;

  for (i = 0; i < setupa->n_immutable_samplers; i++)
    {
      if (setupa->immutable_samplers[i] != setupb->immutable_samplers[i])
        return FALSE;
    }

  return TRUE;
}

static GskGpuImage *
gsk_vulkan_device_create_offscreen_image (GskGpuDevice   *device,
                                          gboolean        with_mipmap,
                                          GdkMemoryDepth  depth,
                                          gsize           width,
                                          gsize           height)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (device);

  return gsk_vulkan_image_new_for_offscreen (self,
                                             with_mipmap,
                                             gdk_memory_depth_get_format (depth),
                                             width,
                                             height);
}

static GskGpuImage *
gsk_vulkan_device_create_atlas_image (GskGpuDevice *device,
                                      gsize         width,
                                      gsize         height)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (device);

  return gsk_vulkan_image_new_for_atlas (self,
                                         width,
                                         height);
}

static GskGpuImage *
gsk_vulkan_device_create_upload_image (GskGpuDevice    *device,
                                       gboolean         with_mipmap,
                                       GdkMemoryFormat  format,
                                       gsize            width,
                                       gsize            height)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (device);

  return gsk_vulkan_image_new_for_upload (self,
                                          with_mipmap,
                                          format,
                                          width,
                                          height);
}

static GskGpuImage *
gsk_vulkan_device_create_download_image (GskGpuDevice   *device,
                                         GdkMemoryDepth  depth,
                                         gsize           width,
                                         gsize           height)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (device);
  GskGpuImage *image;

#ifdef HAVE_DMABUF
  image = gsk_vulkan_image_new_dmabuf (self,
                                       gdk_memory_depth_get_format (depth),
                                       width,
                                       height);
  if (image != NULL)
    return image;
#endif

  image = gsk_vulkan_image_new_for_offscreen (self,
                                              FALSE,
                                              gdk_memory_depth_get_format (depth),
                                              width,
                                              height);

  return image;
}

static void
gsk_vulkan_device_make_current (GskGpuDevice *device)
{
}

static void
gsk_vulkan_device_finalize (GObject *object)
{
  GskVulkanDevice *self = GSK_VULKAN_DEVICE (object);
  GskGpuDevice *device = GSK_GPU_DEVICE (self);
  GdkDisplay *display;
  GHashTableIter iter;
  gpointer key, value;
  gsize i;

  g_object_steal_data (G_OBJECT (gsk_gpu_device_get_display (device)), "-gsk-vulkan-device");

  display = gsk_gpu_device_get_display (device);

  if (self->pipeline_layout_cache)
    gsk_vulkan_pipeline_layout_unref (self, self->pipeline_layout_cache);
  g_assert (g_hash_table_size (self->pipeline_layouts) == 0);
  g_hash_table_unref (self->pipeline_layouts);

  g_hash_table_iter_init (&iter, self->conversion_cache);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      ConversionCacheEntry *entry = key;
      vkDestroySamplerYcbcrConversion (display->vk_device, entry->vk_conversion, NULL);
      vkDestroySampler (display->vk_device, entry->vk_sampler, NULL);
      g_free (key);
    }
  g_hash_table_unref (self->conversion_cache);

  g_hash_table_iter_init (&iter, self->render_pass_cache);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      vkDestroyRenderPass (display->vk_device,
                           ((RenderPassCacheKey *)key)->render_pass,
                           NULL);
      g_free (key);
    }
  g_hash_table_unref (self->render_pass_cache);

  for (i = 0; i < G_N_ELEMENTS (self->vk_samplers); i++)
    {
      if (self->vk_samplers[i] != VK_NULL_HANDLE)
        vkDestroySampler (display->vk_device,
                          self->vk_samplers[i],
                          NULL);
    }

  vkDestroyCommandPool (display->vk_device,
                        self->vk_command_pool,
                        NULL);

  for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
    g_clear_pointer (&self->allocators[i], gsk_vulkan_allocator_unref);
  g_clear_pointer (&self->external_allocator, gsk_vulkan_allocator_unref);

  gdk_display_unref_vulkan (display);

  G_OBJECT_CLASS (gsk_vulkan_device_parent_class)->finalize (object);
}

static void
gsk_vulkan_device_class_init (GskVulkanDeviceClass *klass)
{
  GskGpuDeviceClass *gpu_device_class = GSK_GPU_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gpu_device_class->create_offscreen_image = gsk_vulkan_device_create_offscreen_image;
  gpu_device_class->create_atlas_image = gsk_vulkan_device_create_atlas_image;
  gpu_device_class->create_upload_image = gsk_vulkan_device_create_upload_image;
  gpu_device_class->create_download_image = gsk_vulkan_device_create_download_image;
  gpu_device_class->make_current = gsk_vulkan_device_make_current;

  object_class->finalize = gsk_vulkan_device_finalize;
}

static void
gsk_vulkan_device_init (GskVulkanDevice *self)
{
  self->conversion_cache = g_hash_table_new (conversion_cache_entry_hash, conversion_cache_entry_equal);
  self->render_pass_cache = g_hash_table_new (render_pass_cache_key_hash, render_pass_cache_key_equal);
  self->pipeline_layouts = g_hash_table_new (gsk_vulkan_pipeline_layout_setup_hash, gsk_vulkan_pipeline_layout_setup_equal);
}

static void
gsk_vulkan_device_create_vk_objects (GskVulkanDevice *self)
{
  GdkDisplay *display;

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));

  GSK_VK_CHECK (vkCreateCommandPool, display->vk_device,
                                     &(const VkCommandPoolCreateInfo) {
                                         .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                         .queueFamilyIndex = display->vk_queue_family_index,
                                         .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                     },
                                     NULL,
                                     &self->vk_command_pool);
}

static void
gsk_vulkan_device_setup (GskVulkanDevice *self,
                         GdkDisplay      *display)
{
  VkPhysicalDeviceVulkan12Properties vk12_props = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES,
  };
  VkPhysicalDeviceProperties2 vk_props = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    .pNext = &vk12_props
  };

  vkGetPhysicalDeviceProperties2 (display->vk_physical_device, &vk_props);
  if (gsk_vulkan_device_has_feature (self, GDK_VULKAN_FEATURE_DESCRIPTOR_INDEXING))
    {
      self->max_buffers = vk12_props.maxPerStageDescriptorUpdateAfterBindUniformBuffers;
      self->max_samplers = vk12_props.maxPerStageDescriptorUpdateAfterBindSampledImages;
    }
  else
    {
      self->max_buffers = vk_props.properties.limits.maxPerStageDescriptorUniformBuffers;
      self->max_samplers = vk_props.properties.limits.maxPerStageDescriptorSampledImages;
    }
  if (!gsk_vulkan_device_has_feature (self, GDK_VULKAN_FEATURE_DYNAMIC_INDEXING) ||
      !gsk_vulkan_device_has_feature (self, GDK_VULKAN_FEATURE_NONUNIFORM_INDEXING))
    {
      /* These numbers can be improved in the shader sources by adding more
       * entries to the big if() ladders */
      self->max_buffers = MIN (self->max_buffers, 32);
      self->max_samplers = MIN (self->max_samplers, 32);
    }
  self->max_immutable_samplers = MIN (self->max_samplers / 3, 32);
  gsk_gpu_device_setup (GSK_GPU_DEVICE (self),
                        display,
                        vk_props.properties.limits.maxImageDimension2D);
}

GskGpuDevice *
gsk_vulkan_device_get_for_display (GdkDisplay  *display,
                                   GError     **error)
{
  GskVulkanDevice *self;

  self = g_object_get_data (G_OBJECT (display), "-gsk-vulkan-device");
  if (self)
    return GSK_GPU_DEVICE (g_object_ref (self));

  if (!gdk_display_init_vulkan (display, error))
    return NULL;

  self = g_object_new (GSK_TYPE_VULKAN_DEVICE, NULL);
  self->features = display->vulkan_features;

  gsk_vulkan_device_setup (self, display);

  gsk_vulkan_device_create_vk_objects (self);

  g_object_set_data (G_OBJECT (display), "-gsk-vulkan-device", self);

  return GSK_GPU_DEVICE (self);
}

gsize
gsk_vulkan_device_get_max_immutable_samplers (GskVulkanDevice *self)
{
  return self->max_immutable_samplers;
}

gsize
gsk_vulkan_device_get_max_samplers (GskVulkanDevice *self)
{
  return self->max_samplers;
}

gsize
gsk_vulkan_device_get_max_buffers (GskVulkanDevice *self)
{
  return self->max_buffers;
}

gboolean
gsk_vulkan_device_has_feature (GskVulkanDevice   *self,
                               GdkVulkanFeatures  feature)
{
  return (self->features & feature) == feature;
}

VkDevice
gsk_vulkan_device_get_vk_device (GskVulkanDevice *self)
{
  return gsk_gpu_device_get_display (GSK_GPU_DEVICE (self))->vk_device;
}

VkPhysicalDevice
gsk_vulkan_device_get_vk_physical_device (GskVulkanDevice *self)
{
  return gsk_gpu_device_get_display (GSK_GPU_DEVICE (self))->vk_physical_device;
}

VkQueue
gsk_vulkan_device_get_vk_queue (GskVulkanDevice *self)
{
  return gsk_gpu_device_get_display (GSK_GPU_DEVICE (self))->vk_queue;
}

uint32_t
gsk_vulkan_device_get_vk_queue_family_index (GskVulkanDevice *self)
{
  return gsk_gpu_device_get_display (GSK_GPU_DEVICE (self))->vk_queue_family_index;
}

VkDescriptorSetLayout
gsk_vulkan_device_get_vk_image_set_layout (GskVulkanDevice         *self,
                                           GskVulkanPipelineLayout *layout)
{
  return layout->vk_image_set_layout;
}

VkDescriptorSetLayout
gsk_vulkan_device_get_vk_buffer_set_layout (GskVulkanDevice         *self,
                                            GskVulkanPipelineLayout *layout)
{
  return layout->vk_buffer_set_layout;
}

VkPipelineLayout
gsk_vulkan_device_get_vk_pipeline_layout (GskVulkanDevice         *self,
                                          GskVulkanPipelineLayout *layout)
{
  return layout->vk_pipeline_layout;
}

VkCommandPool
gsk_vulkan_device_get_vk_command_pool (GskVulkanDevice *self)
{
  return self->vk_command_pool;
}

static VkSampler
gsk_vulkan_device_create_sampler (GskVulkanDevice          *self,
                                  VkSamplerYcbcrConversion  vk_conversion,
                                  VkFilter                  vk_filter,
                                  VkSamplerAddressMode      vk_address_mode,
                                  VkSamplerMipmapMode       vk_mipmap_mode,
                                  float                     max_lod)
{
  VkSampler result;

  GSK_VK_CHECK (vkCreateSampler, gsk_vulkan_device_get_vk_device (self),
                                 &(VkSamplerCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = vk_filter,
                                     .minFilter = vk_filter,
                                     .mipmapMode = vk_mipmap_mode,
                                     .addressModeU = vk_address_mode,
                                     .addressModeV = vk_address_mode,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                     .unnormalizedCoordinates = VK_FALSE,
                                     .maxAnisotropy = 1.0,
                                     .minLod = 0.0,
                                     .maxLod = max_lod,
                                     .pNext = vk_conversion == VK_NULL_HANDLE ? NULL : &(VkSamplerYcbcrConversionInfo) {
                                         .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
                                         .conversion = vk_conversion
                                     }
                                 },
                                 NULL,
                                 &result);

  return result;
}

VkSampler
gsk_vulkan_device_get_vk_sampler (GskVulkanDevice     *self,
                                  GskGpuSampler        sampler)
{
  const struct {
    VkFilter filter;
    VkSamplerAddressMode address_mode;
    VkSamplerMipmapMode mipmap_mode;
    float max_lod;
  } filter_attrs[GSK_GPU_SAMPLER_N_SAMPLERS] = {
      [GSK_GPU_SAMPLER_DEFAULT] = {
          .filter = VK_FILTER_LINEAR,
          .address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
          .max_lod = 0.0f,
      },
      [GSK_GPU_SAMPLER_TRANSPARENT] = {
          .filter = VK_FILTER_LINEAR,
          .address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
          .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
          .max_lod = 0.0f,
      },
      [GSK_GPU_SAMPLER_REPEAT] = {
          .filter = VK_FILTER_LINEAR,
          .address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
          .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
          .max_lod = 0.0f,
      },
      [GSK_GPU_SAMPLER_NEAREST] = {
          .filter = VK_FILTER_NEAREST,
          .address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
          .max_lod = 0.0f,
      },
      [GSK_GPU_SAMPLER_MIPMAP_DEFAULT] = {
          .filter = VK_FILTER_LINEAR,
          .address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
          .max_lod = VK_LOD_CLAMP_NONE,
      },
  };

  if (self->vk_samplers[sampler] == VK_NULL_HANDLE)
    {
      self->vk_samplers[sampler] = gsk_vulkan_device_create_sampler (self,
                                                                     VK_NULL_HANDLE,
                                                                     filter_attrs[sampler].filter,
                                                                     filter_attrs[sampler].address_mode,
                                                                     filter_attrs[sampler].mipmap_mode,
                                                                     filter_attrs[sampler].max_lod);
    }

  return self->vk_samplers[sampler];
}

VkSamplerYcbcrConversion
gsk_vulkan_device_get_vk_conversion (GskVulkanDevice *self,
                                     VkFormat         vk_format,
                                     VkSampler       *out_sampler)
{
  ConversionCacheEntry lookup;
  ConversionCacheEntry *entry;
  GdkDisplay *display;

  lookup = (ConversionCacheEntry) {
    .vk_format = vk_format,
  };
  entry = g_hash_table_lookup (self->conversion_cache, &lookup);
  if (entry)
    {
      if (out_sampler)
        *out_sampler = entry->vk_sampler;

      return entry->vk_conversion;
    }

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));

  entry = g_memdup (&lookup, sizeof (ConversionCacheEntry));

  GSK_VK_CHECK (vkCreateSamplerYcbcrConversion, display->vk_device,
                                                &(VkSamplerYcbcrConversionCreateInfo) {
                                                    .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
                                                    .format = vk_format,
                                                    .ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601,
                                                    .ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_NARROW,
                                                    .components = (VkComponentMapping) {
                                                        VK_COMPONENT_SWIZZLE_IDENTITY,
                                                        VK_COMPONENT_SWIZZLE_IDENTITY,
                                                        VK_COMPONENT_SWIZZLE_IDENTITY,
                                                        VK_COMPONENT_SWIZZLE_IDENTITY
                                                    },
                                                    .xChromaOffset = VK_CHROMA_LOCATION_COSITED_EVEN,
                                                    .yChromaOffset = VK_CHROMA_LOCATION_COSITED_EVEN,
                                                    .chromaFilter = VK_FILTER_LINEAR,
                                                    .forceExplicitReconstruction = VK_FALSE
                                                },
                                                NULL,
                                                &entry->vk_conversion);

  entry->vk_sampler = gsk_vulkan_device_create_sampler (self,
                                                        entry->vk_conversion,
                                                        VK_FILTER_LINEAR,
                                                        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                        VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                                        0.0f);

  g_hash_table_insert (self->conversion_cache, entry, entry);

  if (out_sampler)
    *out_sampler = entry->vk_sampler;

  return entry->vk_conversion;
}

VkRenderPass
gsk_vulkan_device_get_vk_render_pass (GskVulkanDevice *self,
                                      VkFormat         format,
                                      VkImageLayout    from_layout,
                                      VkImageLayout    to_layout)
{
  RenderPassCacheKey cache_key;
  RenderPassCacheKey *cached_result;
  VkRenderPass render_pass;
  GdkDisplay *display;

  cache_key = (RenderPassCacheKey) {
    .format = format,
    .from_layout = from_layout,
    .to_layout = to_layout,
  };
  cached_result = g_hash_table_lookup (self->render_pass_cache, &cache_key);
  if (cached_result)
    return cached_result->render_pass;

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));

  GSK_VK_CHECK (vkCreateRenderPass, display->vk_device,
                                    &(VkRenderPassCreateInfo) {
                                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                        .attachmentCount = 1,
                                        .pAttachments = (VkAttachmentDescription[]) {
                                           {
                                              .format = format,
                                              .samples = VK_SAMPLE_COUNT_1_BIT,
                                              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                              .initialLayout = from_layout,
                                              .finalLayout = to_layout
                                           }
                                        },
                                        .subpassCount = 1,
                                        .pSubpasses = (VkSubpassDescription []) {
                                           {
                                              .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                                              .inputAttachmentCount = 0,
                                              .colorAttachmentCount = 1,
                                              .pColorAttachments = (VkAttachmentReference []) {
                                                 {
                                                    .attachment = 0,
                                                     .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                  }
                                               },
                                               .pResolveAttachments = (VkAttachmentReference []) {
                                                  {
                                                     .attachment = VK_ATTACHMENT_UNUSED,
                                                     .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                  }
                                               },
                                               .pDepthStencilAttachment = NULL,
                                            }
                                         },
                                         .dependencyCount = 0,
                                      },
                                      NULL,
                                      &render_pass);

  cached_result = g_memdup (&cache_key, sizeof (RenderPassCacheKey));
  cached_result->render_pass = render_pass;

  g_hash_table_insert (self->render_pass_cache, cached_result, cached_result);

  return render_pass;
}

typedef struct _GskVulkanShaderSpecialization GskVulkanShaderSpecialization;
struct _GskVulkanShaderSpecialization
{
  guint32 clip;
  guint32 n_immutable_samplers;
  guint32 n_samplers;
  guint32 n_buffers;
  guint32 variation;
};

static VkPipelineColorBlendAttachmentState blend_attachment_states[3] = {
  [GSK_GPU_BLEND_OVER] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_ADD] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_CLEAR] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
};

VkPipeline
gsk_vulkan_device_get_vk_pipeline (GskVulkanDevice           *self,
                                   GskVulkanPipelineLayout   *layout,
                                   const GskGpuShaderOpClass *op_class,
                                   guint32                    variation,
                                   GskGpuShaderClip           clip,
                                   GskGpuBlend                blend,
                                   VkFormat                   format,
                                   VkRenderPass               render_pass)
{
  PipelineCacheKey cache_key;
  PipelineCacheKey *cached_result;
  VkPipeline pipeline;
  GdkDisplay *display;
  const char *version_string;
  char *vertex_shader_name, *fragment_shader_name;

  cache_key = (PipelineCacheKey) {
    .op_class = op_class,
    .variation = variation,
    .clip = clip,
    .blend = blend,
    .format = format,
  };
  cached_result = g_hash_table_lookup (layout->pipeline_cache, &cache_key);
  if (cached_result)
    return cached_result->pipeline;

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (self));
  if (gsk_vulkan_device_has_feature (self, GDK_VULKAN_FEATURE_DYNAMIC_INDEXING) &&
      gsk_vulkan_device_has_feature (self, GDK_VULKAN_FEATURE_NONUNIFORM_INDEXING))
    version_string = ".1.2";
  else
    version_string = ".1.0";
  vertex_shader_name = g_strconcat ("/org/gtk/libgsk/shaders/vulkan/",
                                    op_class->shader_name,
                                    version_string,
                                    ".vert.spv",
                                    NULL);
  fragment_shader_name = g_strconcat ("/org/gtk/libgsk/shaders/vulkan/",
                                      op_class->shader_name,
                                      version_string,
                                      ".frag.spv",
                                      NULL);

  GSK_VK_CHECK (vkCreateGraphicsPipelines, display->vk_device,
                                           display->vk_pipeline_cache,
                                           1,
                                           &(VkGraphicsPipelineCreateInfo) {
                                               .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                               .stageCount = 2,
                                               .pStages = (VkPipelineShaderStageCreateInfo[2]) {
                                                   {
                                                       .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                       .stage = VK_SHADER_STAGE_VERTEX_BIT,
                                                       .module = gdk_display_get_vk_shader_module (display, vertex_shader_name),
                                                       .pName = "main",
                                                       .pSpecializationInfo = &(VkSpecializationInfo) {
                                                           .mapEntryCount = 5,
                                                           .pMapEntries = (VkSpecializationMapEntry[5]) {
                                                               {
                                                                   .constantID = 0,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, clip),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 1,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, n_immutable_samplers),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 2,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, n_samplers),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 3,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, n_buffers),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 4,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, variation),
                                                                   .size = sizeof (guint32),
                                                               },
                                                           },
                                                           .dataSize = sizeof (GskVulkanShaderSpecialization),
                                                           .pData = &(GskVulkanShaderSpecialization) {
                                                               .clip = clip,
                                                               .n_immutable_samplers = MAX (1, layout->setup.n_immutable_samplers),
                                                               .n_samplers = layout->setup.n_samplers - MAX (3 * layout->setup.n_immutable_samplers, 1),
                                                               .n_buffers = layout->setup.n_buffers,
                                                               .variation = variation,
                                                           },
                                                       },
                                                   },
                                                   {
                                                       .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                       .module = gdk_display_get_vk_shader_module (display, fragment_shader_name),
                                                       .pName = "main",
                                                       .pSpecializationInfo = &(VkSpecializationInfo) {
                                                           .mapEntryCount = 5,
                                                           .pMapEntries = (VkSpecializationMapEntry[5]) {
                                                               {
                                                                   .constantID = 0,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, clip),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 1,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, n_immutable_samplers),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 2,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, n_samplers),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 3,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, n_buffers),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 4,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, variation),
                                                                   .size = sizeof (guint32),
                                                               },
                                                           },
                                                           .dataSize = sizeof (GskVulkanShaderSpecialization),
                                                           .pData = &(GskVulkanShaderSpecialization) {
                                                               .clip = clip,
                                                               .n_immutable_samplers = MAX (1, layout->setup.n_immutable_samplers),
                                                               .n_samplers = layout->setup.n_samplers - MAX (3 * layout->setup.n_immutable_samplers, 1),
                                                               .n_buffers = layout->setup.n_buffers,
                                                               .variation = variation,
                                                           },
                                                       },
                                                   },
                                               },

                                               .pVertexInputState = op_class->vertex_input_state,
                                               .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                   .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                   .primitiveRestartEnable = VK_FALSE,
                                               },
                                               .pTessellationState = NULL,
                                               .pViewportState = &(VkPipelineViewportStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                   .viewportCount = 1,
                                                   .scissorCount = 1
                                               },
                                               .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                   .depthClampEnable = VK_FALSE,
                                                   .rasterizerDiscardEnable = VK_FALSE,
                                                   .polygonMode = VK_POLYGON_MODE_FILL,
                                                   .cullMode = VK_CULL_MODE_NONE,
                                                   .frontFace = VK_FRONT_FACE_CLOCKWISE,
                                                   .lineWidth = 1.0f,
                                               },
                                               .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                   .rasterizationSamples = 1,
                                               },
                                               .pDepthStencilState = &(VkPipelineDepthStencilStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
                                               },
                                               .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                   .attachmentCount = 1,
                                                   .pAttachments = &blend_attachment_states[blend],
                                               },
                                               .pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                   .dynamicStateCount = 2,
                                                   .pDynamicStates = (VkDynamicState[2]) {
                                                       VK_DYNAMIC_STATE_VIEWPORT,
                                                       VK_DYNAMIC_STATE_SCISSOR
                                                   },
                                               },
                                               .layout = layout->vk_pipeline_layout,
                                               .renderPass = render_pass,
                                               .subpass = 0,
                                               .basePipelineHandle = VK_NULL_HANDLE,
                                               .basePipelineIndex = -1,
                                           },
                                           NULL,
                                           &pipeline);

  g_free (fragment_shader_name);
  g_free (vertex_shader_name);

  cached_result = g_memdup (&cache_key, sizeof (PipelineCacheKey));
  cached_result->pipeline = pipeline;
  g_hash_table_insert (layout->pipeline_cache, cached_result, cached_result);
  gdk_display_vulkan_pipeline_cache_updated (display);

  return pipeline;
}

GskVulkanPipelineLayout *
gsk_vulkan_device_acquire_pipeline_layout (GskVulkanDevice *self,
                                           VkSampler       *immutable_samplers,
                                           gsize            n_immutable_samplers,
                                           gsize            n_samplers,
                                           gsize            n_buffers)
{
  GskVulkanPipelineLayoutSetup setup;
  GskVulkanPipelineLayout *layout;

  /* round the number of samplers/buffer up a bit, so we don't (re)create
   * excessive amounts of layouts */
  n_samplers = MAX (n_samplers, 8);
  g_assert (n_samplers <= self->max_samplers);
  n_buffers = MAX (n_buffers, 8);
  g_assert (n_buffers <= self->max_buffers);
  setup.n_samplers = MIN (2 << g_bit_nth_msf (n_samplers - 1, -1), self->max_samplers);
  setup.n_buffers = MIN (2 << g_bit_nth_msf (n_buffers - 1, -1), self->max_buffers);
  setup.n_immutable_samplers = n_immutable_samplers;
  setup.immutable_samplers = immutable_samplers;

  layout = g_hash_table_lookup (self->pipeline_layouts, &setup);
  if (layout)
    {
      gsk_vulkan_pipeline_layout_ref (self, layout);
      return layout;
    }

  return gsk_vulkan_pipeline_layout_new (self, &setup);
}

void
gsk_vulkan_device_release_pipeline_layout (GskVulkanDevice         *self,
                                           GskVulkanPipelineLayout *layout)
{
  if (self->pipeline_layout_cache)
    gsk_vulkan_pipeline_layout_unref (self, self->pipeline_layout_cache);

  self->pipeline_layout_cache = layout;
}

void
gsk_vulkan_device_get_pipeline_sizes (GskVulkanDevice        *self,
                                      GskVulkanPipelineLayout*layout,
                                      gsize                  *n_immutable_samplers,
                                      gsize                  *n_samplers,
                                      gsize                  *n_buffers)
{
  *n_immutable_samplers = layout->setup.n_immutable_samplers;
  *n_samplers = layout->setup.n_samplers;
  *n_buffers = layout->setup.n_buffers;
}

static GskVulkanAllocator *
gsk_vulkan_device_get_allocator (GskVulkanDevice    *self,
                                 gsize               index,
                                 const VkMemoryType *type)
{
  if (self->allocators[index] == NULL)
    {
      self->allocators[index] = gsk_vulkan_direct_allocator_new (gsk_vulkan_device_get_vk_device (self),
                                                                 index,
                                                                 type);
      self->allocators[index] = gsk_vulkan_buddy_allocator_new (self->allocators[index],
                                                                1024 * 1024);
      //allocators[index] = gsk_vulkan_stats_allocator_new (allocators[index]);
    }

  return self->allocators[index];
}

/* following code found in
 * https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceMemoryProperties.html */
GskVulkanAllocator *
gsk_vulkan_device_find_allocator (GskVulkanDevice       *self,
                                  uint32_t               allowed_types,
                                  VkMemoryPropertyFlags  required_flags,
                                  VkMemoryPropertyFlags  desired_flags)
{
  VkPhysicalDeviceMemoryProperties properties;
  uint32_t i, found;

  vkGetPhysicalDeviceMemoryProperties (gsk_vulkan_device_get_vk_physical_device (self),
                                       &properties);

  found = properties.memoryTypeCount;
  for (i = 0; i < properties.memoryTypeCount; i++)
    {
      if (!(allowed_types & (1 << i)))
        continue;

      if ((properties.memoryTypes[i].propertyFlags & required_flags) != required_flags)
        continue;

      found = MIN (i, found);

      if ((properties.memoryTypes[i].propertyFlags & desired_flags) == desired_flags)
        break;
  }

  g_assert (found < properties.memoryTypeCount);

  return gsk_vulkan_allocator_ref (gsk_vulkan_device_get_allocator (self, found, &properties.memoryTypes[found]));
}

GskVulkanAllocator *
gsk_vulkan_device_get_external_allocator (GskVulkanDevice *self)
{
  if (self->external_allocator == NULL)
    {
      self->external_allocator = gsk_vulkan_external_allocator_new (gsk_vulkan_device_get_vk_device (self));
      //self->external_allocator = gsk_vulkan_stats_allocator_new (self->external_allocator);
    }

  return self->external_allocator;
}
