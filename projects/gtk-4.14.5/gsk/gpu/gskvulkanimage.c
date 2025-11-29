#include "config.h"

#include "gskvulkanimageprivate.h"

#include "gskvulkanbufferprivate.h"
#include "gskvulkanframeprivate.h"
#include "gskvulkanmemoryprivate.h"

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkdmabuftextureprivate.h"
#include "gdk/gdkvulkancontextprivate.h"
#include "gdk/gdkmemoryformatprivate.h"
#include "gdk/gdkvulkancontextprivate.h"

#include <fcntl.h>
#include <string.h>
#ifdef HAVE_DMABUF
#include <linux/dma-buf.h>
#endif

struct _GskVulkanImage
{
  GskGpuImage parent_instance;

  GdkDisplay *display;

  VkFormat vk_format;
  VkImageTiling vk_tiling;
  VkImageUsageFlags vk_usage;
  VkImage vk_image;
  VkImageView vk_image_view;
  VkFramebuffer vk_framebuffer;
  VkImageView vk_framebuffer_image_view;
  VkSampler vk_sampler;
  VkSemaphore vk_semaphore;

  VkPipelineStageFlags vk_pipeline_stage;
  VkImageLayout vk_image_layout;
  VkAccessFlags vk_access;

  GskVulkanAllocator *allocator;
  GskVulkanAllocation allocation;
};

G_DEFINE_TYPE (GskVulkanImage, gsk_vulkan_image, GSK_TYPE_GPU_IMAGE)

static gboolean
gsk_component_mapping_is_framebuffer_compatible (const VkComponentMapping *components)
{
  if (components->r != VK_COMPONENT_SWIZZLE_R ||
      components->g != VK_COMPONENT_SWIZZLE_G ||
      components->b != VK_COMPONENT_SWIZZLE_B ||
      components->a != VK_COMPONENT_SWIZZLE_A)
    return FALSE;

  return TRUE;
}

static gboolean
gsk_vulkan_device_supports_format (GskVulkanDevice   *device,
                                   VkFormat           format,
                                   uint64_t           modifier,
                                   guint              n_planes,
                                   VkImageTiling      tiling,
                                   VkImageUsageFlags  usage,
                                   gsize              width,
                                   gsize              height,
                                   GskGpuImageFlags  *out_flags)
{
  VkDrmFormatModifierPropertiesEXT drm_mod_properties[100];
  VkDrmFormatModifierPropertiesListEXT drm_properties;
  VkPhysicalDevice vk_phys_device;
  VkFormatProperties2 properties;
  VkImageFormatProperties2 image_properties;
  VkFormatFeatureFlags features;
  VkResult res;
  gsize i;

  vk_phys_device = gsk_vulkan_device_get_vk_physical_device (device);

  drm_properties = (VkDrmFormatModifierPropertiesListEXT) {
    .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    .drmFormatModifierCount = G_N_ELEMENTS (drm_mod_properties),
    .pDrmFormatModifierProperties = drm_mod_properties,
  };
  properties = (VkFormatProperties2) {
    .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
    .pNext = (tiling != VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT) ? NULL : &drm_properties
  };
  vkGetPhysicalDeviceFormatProperties2 (vk_phys_device,
                                        format,
                                        &properties);

  switch ((int) tiling)
    {
      case VK_IMAGE_TILING_OPTIMAL:
        features = properties.formatProperties.optimalTilingFeatures;
        break;
      case VK_IMAGE_TILING_LINEAR:
        features = properties.formatProperties.linearTilingFeatures;
        break;
      case VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT:
        features = 0;
        for (i = 0; i < drm_properties.drmFormatModifierCount; i++)
          {
            if (drm_mod_properties[i].drmFormatModifier == modifier &&
                drm_mod_properties[i].drmFormatModifierPlaneCount == n_planes)
              {
                features = drm_mod_properties[i].drmFormatModifierTilingFeatures;
                break;
              }
          }
        if (features == 0)
          return FALSE;
        break;
      default:
        return FALSE;
    }

  if (!(features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
    return FALSE;

  image_properties = (VkImageFormatProperties2) {
    .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
  };
  res = vkGetPhysicalDeviceImageFormatProperties2 (vk_phys_device,
                                                   &(VkPhysicalDeviceImageFormatInfo2) {
                                                     .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
                                                     .format = format,
                                                     .type = VK_IMAGE_TYPE_2D,
                                                     .tiling = tiling,
                                                     .usage = usage,
                                                     .flags = 0,
                                                     .pNext = (tiling != VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT) ? NULL : &(VkPhysicalDeviceImageDrmFormatModifierInfoEXT) {
                                                         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
                                                         .drmFormatModifier = modifier,
                                                         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                         .queueFamilyIndexCount = 1,
                                                         .pQueueFamilyIndices = (uint32_t[1]) { gsk_vulkan_device_get_vk_queue_family_index (device) },
                                                      }
                                                   },
                                                   &image_properties);
  if (res != VK_SUCCESS)
    return FALSE;

  if (image_properties.imageFormatProperties.maxExtent.width < width ||
      image_properties.imageFormatProperties.maxExtent.height < height)
    return FALSE;

  *out_flags = 0;
  if ((features & VK_FORMAT_FEATURE_BLIT_SRC_BIT) == 0)
    *out_flags |= GSK_GPU_IMAGE_NO_BLIT;
  if (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)
    *out_flags |= GSK_GPU_IMAGE_FILTERABLE;
  if (features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT)
    *out_flags |= GSK_GPU_IMAGE_RENDERABLE;

  return TRUE;
}

static void
gsk_vulkan_image_create_view (GskVulkanImage            *self,
                              VkFormat                   vk_format,
                              const VkComponentMapping  *vk_components,
                              VkSamplerYcbcrConversion   vk_conversion)
{
  GSK_VK_CHECK (vkCreateImageView, self->display->vk_device,
                                 &(VkImageViewCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                     .image = self->vk_image,
                                     .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                     .format = vk_format,
                                     .components = *vk_components,
                                     .subresourceRange = {
                                         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                         .baseMipLevel = 0,
                                         .levelCount = VK_REMAINING_MIP_LEVELS,
                                         .baseArrayLayer = 0,
                                         .layerCount = 1,
                                     },
                                     .pNext = vk_conversion == VK_NULL_HANDLE ? NULL : &(VkSamplerYcbcrConversionInfo) {
                                         .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
                                         .conversion = vk_conversion
                                     }
                                 },
                                 NULL,
                                 &self->vk_image_view);
}

static gboolean
gsk_vulkan_device_check_format (GskVulkanDevice          *device,
                                VkFormat                  vk_format,
                                const VkComponentMapping *vk_components,
                                GskGpuImageFlags          required_flags,
                                VkImageTiling             vk_tiling,
                                VkImageUsageFlags         vk_usage,
                                gsize                     width,
                                gsize                     height,
                                VkImageTiling            *out_tiling,
                                GskGpuImageFlags         *out_flags)
{
#define CHECK_FLAGS (GSK_GPU_IMAGE_NO_BLIT | GSK_GPU_IMAGE_FILTERABLE | GSK_GPU_IMAGE_RENDERABLE)
  GskGpuImageFlags flags;

  if (vk_usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT &&
      !gsk_component_mapping_is_framebuffer_compatible (vk_components))
    return FALSE;

  if (gsk_vulkan_device_supports_format (device,
                                         vk_format,
                                         0, 1,
                                         vk_tiling, vk_usage,
                                         width, height,
                                         &flags) &&
      ((flags & required_flags & CHECK_FLAGS) == (required_flags & CHECK_FLAGS)))
    {
      *out_tiling = vk_tiling;
      *out_flags = flags;
      return TRUE;
    }

  if (vk_tiling == VK_IMAGE_TILING_LINEAR &&
      gsk_vulkan_device_supports_format (device,
                                         vk_format,
                                         0, 1,
                                         VK_IMAGE_TILING_OPTIMAL, vk_usage,
                                         width, height,
                                         &flags) &&
      ((flags & required_flags & CHECK_FLAGS) == (required_flags & CHECK_FLAGS)))
    {
      *out_tiling = VK_IMAGE_TILING_OPTIMAL;
      *out_flags = flags;
      return TRUE;
    }

  return FALSE;
#undef CHECK_FLAGS
}

static GskVulkanImage *
gsk_vulkan_image_new (GskVulkanDevice           *device,
                      GdkMemoryFormat            format,
                      GskGpuImageFlags           required_flags,
                      gsize                      width,
                      gsize                      height,
                      VkImageTiling              tiling,
                      VkImageUsageFlags          usage,
                      VkPipelineStageFlags       stage,
                      VkImageLayout              layout,
                      VkAccessFlags              access,
                      VkMemoryPropertyFlags      memory)
{
  VkMemoryRequirements requirements;
  GskVulkanImage *self;
  VkDevice vk_device;
  GskGpuImageFlags flags;
  VkFormat vk_format;
  VkComponentMapping vk_components;

  g_assert (width > 0 && height > 0);

  /* First, try the actual format */
  vk_format = gdk_memory_format_vk_format (format, &vk_components);
  if (vk_format == VK_FORMAT_UNDEFINED ||
      !gsk_vulkan_device_check_format (device, vk_format, &vk_components, required_flags,
                                       tiling, usage, width, height,
                                       &tiling, &flags))
    {
      /* Second, try the potential RGBA format */
      vk_format = gdk_memory_format_vk_rgba_format (format, NULL, &vk_components);
      if (vk_format == VK_FORMAT_UNDEFINED ||
          !gsk_vulkan_device_check_format (device, vk_format, &vk_components, required_flags,
                                           tiling, usage, width, height,
                                           &tiling, &flags))
        {
          const GdkMemoryFormat *fallbacks;
          gsize i;

          /* Next, try the fallbacks */
          fallbacks = gdk_memory_format_get_fallbacks (format);
          for (i = 0; fallbacks[i] != -1; i++)
            {
              vk_format = gdk_memory_format_vk_format (fallbacks[i], &vk_components);
              if (vk_format != VK_FORMAT_UNDEFINED &&
                  gsk_vulkan_device_check_format (device, vk_format, &vk_components, required_flags,
                                                  tiling, usage, width, height,
                                                  &tiling, &flags))
                {
                  format = fallbacks[i];
                  break;
                }
            }

          /* No format found. Likely, the width/height are too big */
          if (fallbacks[i] == -1)
            return NULL;
        }
    }

  if (gdk_memory_format_alpha (format) == GDK_MEMORY_ALPHA_STRAIGHT)
    flags |= GSK_GPU_IMAGE_STRAIGHT_ALPHA;

  if (((flags & (GSK_GPU_IMAGE_FILTERABLE | GSK_GPU_IMAGE_RENDERABLE | GSK_GPU_IMAGE_NO_BLIT | GSK_GPU_IMAGE_STRAIGHT_ALPHA)) ==
       (GSK_GPU_IMAGE_FILTERABLE | GSK_GPU_IMAGE_RENDERABLE)) &&
      (required_flags & GSK_GPU_IMAGE_CAN_MIPMAP))
    flags |= GSK_GPU_IMAGE_CAN_MIPMAP;

  vk_device = gsk_vulkan_device_get_vk_device (device);

  self = g_object_new (GSK_TYPE_VULKAN_IMAGE, NULL);

  self->display = g_object_ref (gsk_gpu_device_get_display (GSK_GPU_DEVICE (device)));
  gdk_display_ref_vulkan (self->display);
  self->vk_format = vk_format;
  self->vk_tiling = tiling;
  self->vk_usage = usage;
  self->vk_pipeline_stage = stage;
  self->vk_image_layout = layout;
  self->vk_access = access;

  gsk_gpu_image_setup (GSK_GPU_IMAGE (self), flags, format, width, height);

  GSK_VK_CHECK (vkCreateImage, vk_device,
                                &(VkImageCreateInfo) {
                                    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                    .flags = 0,
                                    .imageType = VK_IMAGE_TYPE_2D,
                                    .format = vk_format,
                                    .extent = { width, height, 1 },
                                    .mipLevels = (flags & GSK_GPU_IMAGE_CAN_MIPMAP) ? gsk_vulkan_mipmap_levels (width, height) : 1,
                                    .arrayLayers = 1,
                                    .samples = VK_SAMPLE_COUNT_1_BIT,
                                    .tiling = tiling,
                                    .usage = usage |
                                             (flags & GSK_GPU_IMAGE_NO_BLIT ? 0 : VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
                                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                    .initialLayout = self->vk_image_layout,
                                },
                                NULL,
                                &self->vk_image);

  vkGetImageMemoryRequirements (vk_device,
                                self->vk_image,
                                &requirements);

  self->allocator = gsk_vulkan_device_find_allocator (device,
                                                      requirements.memoryTypeBits,
                                                      0,
                                                      tiling == VK_IMAGE_TILING_LINEAR ? GSK_VULKAN_MEMORY_MAPPABLE : 0);
  gsk_vulkan_alloc (self->allocator,
                    requirements.size,
                    requirements.alignment,
                    &self->allocation);

  GSK_VK_CHECK (vkBindImageMemory, vk_device,
                                   self->vk_image,
                                   self->allocation.vk_memory,
                                   self->allocation.offset);

  gsk_vulkan_image_create_view (self, vk_format, &vk_components, VK_NULL_HANDLE);

  return self;
}

GskGpuImage *
gsk_vulkan_image_new_for_upload (GskVulkanDevice *device,
                                 gboolean         with_mipmap,
                                 GdkMemoryFormat  format,
                                 gsize            width,
                                 gsize            height)
{
  GskVulkanImage *self;

  self = gsk_vulkan_image_new (device,
                               format,
                               with_mipmap ? (GSK_GPU_IMAGE_CAN_MIPMAP | GSK_GPU_IMAGE_RENDERABLE | GSK_GPU_IMAGE_FILTERABLE) : 0,
                               width,
                               height,
                               VK_IMAGE_TILING_LINEAR,
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_IMAGE_LAYOUT_PREINITIALIZED,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  return GSK_GPU_IMAGE (self);
}

static gboolean
gsk_vulkan_image_can_map (GskVulkanImage *self)
{
  if (GSK_DEBUG_CHECK (STAGING))
    return FALSE;

  if (self->vk_tiling != VK_IMAGE_TILING_LINEAR)
    return FALSE;

  if (self->vk_image_layout != VK_IMAGE_LAYOUT_PREINITIALIZED &&
      self->vk_image_layout != VK_IMAGE_LAYOUT_GENERAL)
    return FALSE;

  if ((self->allocation.memory_flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) == 0)
    return FALSE;

  return self->allocation.map != NULL;
}

guchar *
gsk_vulkan_image_get_data (GskVulkanImage *self,
                           gsize          *out_stride)
{
  VkImageSubresource image_res;
  VkSubresourceLayout image_layout;

  if (!gsk_vulkan_image_can_map (self))
    return NULL;

  image_res.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_res.mipLevel = 0;
  image_res.arrayLayer = 0;

  vkGetImageSubresourceLayout (self->display->vk_device,
                               self->vk_image, &image_res, &image_layout);

  *out_stride = image_layout.rowPitch;

  return self->allocation.map + image_layout.offset;
}

GskGpuImage *
gsk_vulkan_image_new_for_swapchain (GskVulkanDevice  *device,
                                    VkImage           image,
                                    VkFormat          format,
                                    gsize             width,
                                    gsize             height)
{
  GskVulkanImage *self;

  self = g_object_new (GSK_TYPE_VULKAN_IMAGE, NULL);

  self->display = g_object_ref (gsk_gpu_device_get_display (GSK_GPU_DEVICE (device)));
  gdk_display_ref_vulkan (self->display);
  self->vk_tiling = VK_IMAGE_TILING_OPTIMAL;
  self->vk_image = image;
  self->vk_format = format;
  self->vk_pipeline_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  self->vk_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  self->vk_access = 0;

  /* FIXME: The memory format and flags here are very suboptimal */
  gsk_gpu_image_setup (GSK_GPU_IMAGE (self), 0, GDK_MEMORY_DEFAULT, width, height);

  gsk_vulkan_image_create_view (self,
                                format,
                                &(VkComponentMapping) {
                                  VK_COMPONENT_SWIZZLE_R,
                                  VK_COMPONENT_SWIZZLE_G,
                                  VK_COMPONENT_SWIZZLE_B,
                                  VK_COMPONENT_SWIZZLE_A
                                },
                                VK_NULL_HANDLE);

  return GSK_GPU_IMAGE (self);
}

GskGpuImage *
gsk_vulkan_image_new_for_atlas (GskVulkanDevice *device,
                                gsize            width,
                                gsize            height)
{
  GskVulkanImage *self;

  self = gsk_vulkan_image_new (device,
                               GDK_MEMORY_DEFAULT,
                               GSK_GPU_IMAGE_FILTERABLE | GSK_GPU_IMAGE_RENDERABLE,
                               width,
                               height,
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED,
                               0,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  return GSK_GPU_IMAGE (self);
}

GskGpuImage *
gsk_vulkan_image_new_for_offscreen (GskVulkanDevice *device,
                                    gboolean         with_mipmap,
                                    GdkMemoryFormat  preferred_format,
                                    gsize            width,
                                    gsize            height)
{
  GskVulkanImage *self;

  self = gsk_vulkan_image_new (device,
                               preferred_format,
                               GSK_GPU_IMAGE_RENDERABLE |
                               (with_mipmap ? GSK_GPU_IMAGE_CAN_MIPMAP | GSK_GPU_IMAGE_FILTERABLE : 0),
                               width,
                               height,
                               VK_IMAGE_TILING_OPTIMAL,
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_SAMPLED_BIT |
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  return GSK_GPU_IMAGE (self);
}

#ifdef HAVE_DMABUF
static gboolean
gsk_vulkan_device_check_dmabuf_format (GskVulkanDevice          *device,
                                       VkFormat                  vk_format,
                                       const VkComponentMapping *vk_components,
                                       gsize                     width,
                                       gsize                     height,
                                       uint64_t                  modifiers[100],
                                       GskGpuImageFlags         *out_flags,
                                       gsize                    *out_n_modifiers)

{
  const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
  VkDrmFormatModifierPropertiesEXT drm_mod_properties[100];
  VkDrmFormatModifierPropertiesListEXT drm_properties;
  VkPhysicalDevice vk_phys_device;
  VkFormatProperties2 properties;
  VkImageFormatProperties2 image_properties;
  GskGpuImageFlags flags;
  gsize i, n_modifiers;
  VkResult res;

  drm_properties = (VkDrmFormatModifierPropertiesListEXT) {
    .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    .drmFormatModifierCount = G_N_ELEMENTS (drm_mod_properties),
    .pDrmFormatModifierProperties = drm_mod_properties,
  };
  properties = (VkFormatProperties2) {
    .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
    .pNext = &drm_properties
  };

  if (!gsk_component_mapping_is_framebuffer_compatible (vk_components))
    return FALSE;

  vk_phys_device = gsk_vulkan_device_get_vk_physical_device (device);

  vkGetPhysicalDeviceFormatProperties2 (vk_phys_device,
                                        vk_format,
                                        &properties);

  flags = GSK_GPU_IMAGE_FILTERABLE | GSK_GPU_IMAGE_RENDERABLE;
  n_modifiers = 0;
  for (i = 0; i < drm_properties.drmFormatModifierCount; i++)
    {
      if ((drm_mod_properties[i].drmFormatModifierTilingFeatures & required) != required)
        continue;

      image_properties = (VkImageFormatProperties2) {
        .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
      };
      res = vkGetPhysicalDeviceImageFormatProperties2 (vk_phys_device,
                                                       &(VkPhysicalDeviceImageFormatInfo2) {
                                                         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
                                                         .format = vk_format,
                                                         .type = VK_IMAGE_TYPE_2D,
                                                         .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
                                                         .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                         .flags = 0,
                                                         .pNext = &(VkPhysicalDeviceImageDrmFormatModifierInfoEXT) {
                                                             .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
                                                             .drmFormatModifier = drm_mod_properties[i].drmFormatModifier,
                                                             .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                             .queueFamilyIndexCount = 1,
                                                             .pQueueFamilyIndices = (uint32_t[1]) { gsk_vulkan_device_get_vk_queue_family_index (device) },
                                                          }
                                                       },
                                                       &image_properties);
      if (res != VK_SUCCESS)
        continue;

      if (image_properties.imageFormatProperties.maxExtent.width < width ||
          image_properties.imageFormatProperties.maxExtent.height < height)
        continue;

      /* we could check the real used format after creation, but for now: */
      if ((drm_mod_properties[i].drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) == 0)
        flags |= GSK_GPU_IMAGE_NO_BLIT;
      if ((drm_mod_properties[i].drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) == 0)
        flags &= ~GSK_GPU_IMAGE_FILTERABLE;
      if ((drm_mod_properties[i].drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) == 0)
        flags &= ~GSK_GPU_IMAGE_RENDERABLE;

      modifiers[n_modifiers++] = drm_mod_properties[i].drmFormatModifier;
    }

  if (n_modifiers == 0)
    return FALSE;

  *out_flags = flags;
  *out_n_modifiers = n_modifiers;
  return TRUE;
}

GskGpuImage *
gsk_vulkan_image_new_dmabuf (GskVulkanDevice *device,
                             GdkMemoryFormat  format,
                             gsize            width,
                             gsize            height)
{
  uint64_t modifiers[100];
  VkDevice vk_device;
  VkFormat vk_format;
  VkComponentMapping vk_components;
  VkMemoryRequirements requirements;
  GskVulkanImage *self;
  VkResult res;
  gsize n_modifiers;
  GskGpuImageFlags flags;

  if (!gsk_vulkan_device_has_feature (device, GDK_VULKAN_FEATURE_DMABUF))
    return NULL;

  vk_device = gsk_vulkan_device_get_vk_device (device);

  /* First, try the actual format */
  vk_format = gdk_memory_format_vk_format (format, &vk_components);
  if (vk_format == VK_FORMAT_UNDEFINED ||
      !gsk_vulkan_device_check_dmabuf_format (device, vk_format, &vk_components, width, height,
                                              modifiers, &flags, &n_modifiers))
    {
      /* Second, try the potential RGBA format, but as a fallback */
      GdkMemoryFormat rgba_format;
      vk_format = gdk_memory_format_vk_rgba_format (format, &rgba_format, NULL);
      if (vk_format != VK_FORMAT_UNDEFINED)
        vk_format = gdk_memory_format_vk_format (rgba_format, &vk_components);
      if (vk_format != VK_FORMAT_UNDEFINED &&
          gsk_vulkan_device_check_dmabuf_format (device, vk_format, &vk_components, width, height,
                                                 modifiers, &flags, &n_modifiers))
        {
          format = rgba_format;
        }
      else
        {
          const GdkMemoryFormat *fallbacks;
          gsize i;

          /* Next, try the fallbacks */
          fallbacks = gdk_memory_format_get_fallbacks (format);
          for (i = 0; fallbacks[i] != -1; i++)
            {
              vk_format = gdk_memory_format_vk_format (fallbacks[i], &vk_components);
              if (vk_format != VK_FORMAT_UNDEFINED &&
                  gsk_vulkan_device_check_dmabuf_format (device, vk_format, &vk_components, width, height,
                                                         modifiers, &flags, &n_modifiers))
                {
                  format = fallbacks[i];
                  break;
                }
            }

          /* No format found. Likely, the width/height are too big */
          if (fallbacks[i] == -1)
            return NULL;
        }
    }

  self = g_object_new (GSK_TYPE_VULKAN_IMAGE, NULL);

  self->display = g_object_ref (gsk_gpu_device_get_display (GSK_GPU_DEVICE (device)));
  gdk_display_ref_vulkan (self->display);
  self->vk_tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
  self->vk_format = vk_format;
  self->vk_pipeline_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  self->vk_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  self->vk_access = 0;

  gsk_gpu_image_setup (GSK_GPU_IMAGE (self),
                       flags | GSK_GPU_IMAGE_EXTERNAL |
                       (gdk_memory_format_alpha (format) == GDK_MEMORY_ALPHA_STRAIGHT ? GSK_GPU_IMAGE_STRAIGHT_ALPHA : 0),
                       format,
                       width, height);

  res = vkCreateImage (vk_device,
                       &(VkImageCreateInfo) {
                           .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                           .flags = 0,
                           .imageType = VK_IMAGE_TYPE_2D,
                           .format = vk_format,
                           .extent = { width, height, 1 },
                           .mipLevels = 1,
                           .arrayLayers = 1,
                           .samples = VK_SAMPLE_COUNT_1_BIT,
                           .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
                           .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                    ((flags & GSK_GPU_IMAGE_NO_BLIT) ? 0 : VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
                           .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                           .initialLayout = self->vk_image_layout,
                           .pNext = &(VkExternalMemoryImageCreateInfo) {
                               .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
                               .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                               .pNext = &(VkImageDrmFormatModifierListCreateInfoEXT) {
                                   .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT,
                                   .drmFormatModifierCount = n_modifiers,
                                   .pDrmFormatModifiers = modifiers
                               }
                           },
                       },
                       NULL,
                       &self->vk_image);
  if (res != VK_SUCCESS)
    {
      gsk_vulkan_handle_result (res, "vkCreateImage");
      return NULL;
    }

  vkGetImageMemoryRequirements (vk_device,
                                self->vk_image,
                                &requirements);

  self->allocator = gsk_vulkan_device_get_external_allocator (device);
  gsk_vulkan_allocator_ref (self->allocator);

  gsk_vulkan_alloc (self->allocator,
                    requirements.size,
                    requirements.alignment,
                    &self->allocation);

  GSK_VK_CHECK (vkAllocateMemory, vk_device,
                                  &(VkMemoryAllocateInfo) {
                                      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                      .allocationSize = requirements.size,
                                      .memoryTypeIndex = g_bit_nth_lsf (requirements.memoryTypeBits, -1),
                                      .pNext = &(VkExportMemoryAllocateInfo) {
                                          .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
                                          .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                      }
                                  },
                                  NULL,
                                  &self->allocation.vk_memory);

  GSK_VK_CHECK (vkBindImageMemory, vk_device,
                                   self->vk_image,
                                   self->allocation.vk_memory,
                                   self->allocation.offset);

  gsk_vulkan_image_create_view (self, vk_format, &vk_components, VK_NULL_HANDLE);

  return GSK_GPU_IMAGE (self);
}

GskGpuImage *
gsk_vulkan_image_new_for_dmabuf (GskVulkanDevice *device,
                                 GdkTexture      *texture)
{
  GskVulkanImage *self;
  VkDevice vk_device;
  VkFormat vk_format;
  VkComponentMapping vk_components;
  VkSamplerYcbcrConversion vk_conversion;
  PFN_vkGetMemoryFdPropertiesKHR func_vkGetMemoryFdPropertiesKHR;
  gsize i;
  int fd;
  gsize width, height;
  const GdkDmabuf *dmabuf;
  VkResult res;
  GskGpuImageFlags flags;
  gboolean is_yuv;

  if (!gsk_vulkan_device_has_feature (device, GDK_VULKAN_FEATURE_DMABUF))
    {
      GDK_DEBUG (DMABUF, "Vulkan does not support dmabufs");
      return NULL;
    }

  width = gdk_texture_get_width (texture);
  height = gdk_texture_get_height (texture);
  dmabuf = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture));
  vk_device = gsk_vulkan_device_get_vk_device (device);
  func_vkGetMemoryFdPropertiesKHR = (PFN_vkGetMemoryFdPropertiesKHR) vkGetDeviceProcAddr (vk_device, "vkGetMemoryFdPropertiesKHR");

  vk_format = gdk_dmabuf_get_vk_format (dmabuf->fourcc, &vk_components);
  if (vk_format == VK_FORMAT_UNDEFINED)
    {
      GDK_DEBUG (DMABUF, "GTK's Vulkan doesn't support fourcc %.4s", (char *) &dmabuf->fourcc);
      return NULL;
    }
  if (!gdk_dmabuf_fourcc_is_yuv (dmabuf->fourcc, &is_yuv))
    {
      g_assert_not_reached ();
    }

  /* FIXME: Add support for disjoint images */
  if (gdk_dmabuf_is_disjoint (dmabuf))
    {
      GDK_DEBUG (DMABUF, "FIXME: Add support for disjoint dmabufs to Vulkan");
      return NULL;
    }

  if (!gsk_vulkan_device_supports_format (device,
                                          vk_format,
                                          dmabuf->modifier,
                                          dmabuf->n_planes,
                                          VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
                                          VK_IMAGE_USAGE_SAMPLED_BIT,
                                          width, height,
                                          &flags))
    {
      GDK_DEBUG (DMABUF, "Vulkan driver does not support format %.4s::%016llx with %u planes",
                 (char *) &dmabuf->fourcc, (unsigned long long) dmabuf->modifier, dmabuf->n_planes);
      return NULL;
    }

  self = g_object_new (GSK_TYPE_VULKAN_IMAGE, NULL);

  self->display = g_object_ref (gsk_gpu_device_get_display (GSK_GPU_DEVICE (device)));
  gdk_display_ref_vulkan (self->display);
  self->vk_tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
  self->vk_format = vk_format;
  self->vk_pipeline_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  self->vk_image_layout = VK_IMAGE_LAYOUT_GENERAL;
  self->vk_access = 0;

  res  = vkCreateImage (vk_device,
                        &(VkImageCreateInfo) {
                            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                            .flags = 0, //disjoint ? VK_IMAGE_CREATE_DISJOINT_BIT : 0,
                            .imageType = VK_IMAGE_TYPE_2D,
                            .format = vk_format,
                            .extent = { width, height, 1 },
                            .mipLevels = 1,
                            .arrayLayers = 1,
                            .samples = VK_SAMPLE_COUNT_1_BIT,
                            .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
                            .usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                                     (flags & GSK_GPU_IMAGE_NO_BLIT ? 0 : VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                            .initialLayout = self->vk_image_layout,
                            .pNext = &(VkExternalMemoryImageCreateInfo) {
                                .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
                                .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                .pNext = &(VkImageDrmFormatModifierExplicitCreateInfoEXT) {
                                    .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
                                    .drmFormatModifier = dmabuf->modifier,
                                    .drmFormatModifierPlaneCount = dmabuf->n_planes,
                                    .pPlaneLayouts = (VkSubresourceLayout[4]) {
                                        {
                                            .offset = dmabuf->planes[0].offset,
                                            .rowPitch = dmabuf->planes[0].stride,
                                        },
                                        {
                                            .offset = dmabuf->planes[1].offset,
                                            .rowPitch = dmabuf->planes[1].stride,
                                        },
                                        {
                                            .offset = dmabuf->planes[2].offset,
                                            .rowPitch = dmabuf->planes[2].stride,
                                        },
                                        {
                                            .offset = dmabuf->planes[3].offset,
                                            .rowPitch = dmabuf->planes[3].stride,
                                        },
                                    },
                                }
                            },
                        },
                        NULL,
                        &self->vk_image);
  if (res != VK_SUCCESS)
    {
      gsk_vulkan_handle_result (res, "vkCreateImage");
      GDK_DEBUG (DMABUF, "vkCreateImage() failed: %s", gdk_vulkan_strerror (res));
      return NULL;
    }

  gsk_gpu_image_setup (GSK_GPU_IMAGE (self),
                       flags |
                       (gdk_memory_format_alpha (gdk_texture_get_format (texture)) == GDK_MEMORY_ALPHA_STRAIGHT ? GSK_GPU_IMAGE_STRAIGHT_ALPHA : 0) |
                       (is_yuv ? (GSK_GPU_IMAGE_EXTERNAL | GSK_GPU_IMAGE_NO_BLIT) : 0),
                       gdk_texture_get_format (texture),
                       width, height);
  gsk_gpu_image_toggle_ref_texture (GSK_GPU_IMAGE (self), texture);

  self->allocator = gsk_vulkan_device_get_external_allocator (device);
  gsk_vulkan_allocator_ref (self->allocator);

  fd = fcntl (dmabuf->planes[0].fd, F_DUPFD_CLOEXEC, (int) 3);
  if (fd < 0)
    {
      GDK_DEBUG (DMABUF, "Vulkan failed to dup() fd: %s", g_strerror (errno));
      vkDestroyImage (vk_device, self->vk_image, NULL);
      return NULL;
    }

  for (i = 0; i < 1 /* disjoint ? dmabuf->n_planes : 1 */; i++)
    {
      VkMemoryFdPropertiesKHR fd_props = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR,
      };
      VkMemoryRequirements2 requirements = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
      };

      GSK_VK_CHECK (func_vkGetMemoryFdPropertiesKHR, vk_device,
                                                     VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                                     fd,
                                                     &fd_props);

      vkGetImageMemoryRequirements2 (vk_device,
                                     &(VkImageMemoryRequirementsInfo2) {
                                         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
                                         .image = self->vk_image,
                                         //.pNext = !disjoint ? NULL : &(VkImagePlaneMemoryRequirementsInfo) {
                                         //    .sType = VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO,
                                         //    .planeAspect = aspect_flags[i]
                                         //},
                                     },
                                     &requirements);

      if (gsk_vulkan_device_has_feature (device, GDK_VULKAN_FEATURE_SEMAPHORE_IMPORT))
        {
          int sync_file_fd = gdk_dmabuf_export_sync_file (fd, DMA_BUF_SYNC_READ);
          if (sync_file_fd >= 0)
            {
              PFN_vkImportSemaphoreFdKHR func_vkImportSemaphoreFdKHR;
              func_vkImportSemaphoreFdKHR = (PFN_vkImportSemaphoreFdKHR) vkGetDeviceProcAddr (vk_device, "vkImportSemaphoreFdKHR");

              GSK_VK_CHECK (vkCreateSemaphore, vk_device,
                                               &(VkSemaphoreCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                               },
                                               NULL,
                                               &self->vk_semaphore);

              GSK_VK_CHECK (func_vkImportSemaphoreFdKHR, vk_device,
                                                         &(VkImportSemaphoreFdInfoKHR) {
                                                             .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR,
                                                             .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
                                                             .flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT,
                                                             .semaphore = self->vk_semaphore,
                                                             .fd = sync_file_fd,
                                                         });
            }
        }

      gsk_vulkan_alloc (self->allocator,
                        requirements.memoryRequirements.size,
                        requirements.memoryRequirements.alignment,
                        &self->allocation);
      GSK_VK_CHECK (vkAllocateMemory, vk_device,
                                      &(VkMemoryAllocateInfo) {
                                          .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                          .allocationSize = requirements.memoryRequirements.size,
                                          .memoryTypeIndex = g_bit_nth_lsf (fd_props.memoryTypeBits, -1),
                                          .pNext = &(VkImportMemoryFdInfoKHR) {
                                              .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
                                              .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                              .fd = fd,
                                              .pNext = &(VkMemoryDedicatedAllocateInfo) {
                                                  .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
                                                  .image = self->vk_image,
                                              }
                                          }
                                      },
                                      NULL,
                                      &self->allocation.vk_memory);
    }

#if 1
  GSK_VK_CHECK (vkBindImageMemory2, self->display->vk_device,
                                    1,
                                    &(VkBindImageMemoryInfo) {
                                        .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                                        .image = self->vk_image,
                                        .memory = self->allocation.vk_memory,
                                        .memoryOffset = self->allocation.offset,
                                    });
#else
  GSK_VK_CHECK (vkBindImageMemory2, self->display->vk_device,
                                    dmabuf->n_planes,
                                    (VkBindImageMemoryInfo[4]) {
                                        {
                                            .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                                            .image = self->vk_image,
                                            .memory = self->allocation.vk_memory,
                                            .memoryOffset = dmabuf->planes[0].offset,
                                            .pNext = &(VkBindImagePlaneMemoryInfo) {
                                                .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
                                                .planeAspect = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT,
                                            },
                                        },
                                        {
                                            .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                                            .image = self->vk_image,
                                            .memory = self->allocation.vk_memory,
                                            .memoryOffset = dmabuf->planes[1].offset,
                                            .pNext = &(VkBindImagePlaneMemoryInfo) {
                                                .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
                                                .planeAspect = VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT,
                                            },

                                        },
                                        {
                                            .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                                            .image = self->vk_image,
                                            .memory = self->allocation.vk_memory,
                                            .memoryOffset = dmabuf->planes[2].offset,
                                            .pNext = &(VkBindImagePlaneMemoryInfo) {
                                                .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
                                                .planeAspect = VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT,
                                            },
                                        },
                                        {
                                            .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                                            .image = self->vk_image,
                                            .memory = self->allocation.vk_memory,
                                            .memoryOffset = dmabuf->planes[3].offset,
                                            .pNext = &(VkBindImagePlaneMemoryInfo) {
                                                .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO,
                                                .planeAspect = VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT,
                                            },
                                        }
                                    });
#endif

  if (is_yuv)
    vk_conversion = gsk_vulkan_device_get_vk_conversion (device, vk_format, &self->vk_sampler);
  else
    vk_conversion = VK_NULL_HANDLE;

  gsk_vulkan_image_create_view (self,
                                vk_format,
                                &vk_components,
                                vk_conversion);

  GDK_DEBUG (DMABUF, "Vulkan uploaded %zux%zu %.4s::%016llx %sdmabuf",
             width, height,
             (char *) &dmabuf->fourcc, (unsigned long long) dmabuf->modifier,
             is_yuv ? "YUV " : "");

  return GSK_GPU_IMAGE (self);
}

static void
close_the_fd (gpointer the_fd)
{
  close (GPOINTER_TO_INT (the_fd));
}

static guint
gsk_vulkan_image_get_n_planes (GskVulkanImage *self,
                               guint64         modifier)
{
  VkDrmFormatModifierPropertiesEXT drm_mod_properties[100];
  VkDrmFormatModifierPropertiesListEXT drm_properties = {
    .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    .drmFormatModifierCount = G_N_ELEMENTS (drm_mod_properties),
    .pDrmFormatModifierProperties = drm_mod_properties,
  };
  VkFormatProperties2 properties = {
    .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
    .pNext = &drm_properties
  };
  gsize i;

  vkGetPhysicalDeviceFormatProperties2 (self->display->vk_physical_device,
                                        self->vk_format,
                                        &properties);
  
  for (i = 0; i < drm_properties.drmFormatModifierCount; i++)
    {
      if (drm_mod_properties[i].drmFormatModifier == modifier)
        return drm_mod_properties[i].drmFormatModifierPlaneCount;
    }

  g_return_val_if_reached (0);
}

GdkTexture *
gsk_vulkan_image_to_dmabuf_texture (GskVulkanImage *self)
{
  GskGpuImage *image = GSK_GPU_IMAGE (self);
  GdkDmabufTextureBuilder *builder;
  GError *error = NULL;
  PFN_vkGetImageDrmFormatModifierPropertiesEXT func_vkGetImageDrmFormatModifierPropertiesEXT;
  PFN_vkGetMemoryFdKHR func_vkGetMemoryFdKHR;
  VkImageDrmFormatModifierPropertiesEXT properties = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT,
  };
  VkSubresourceLayout layout;
  GdkTexture *texture;
  VkResult res;
  guint32 fourcc;
  int fd;
  guint plane, n_planes;

  if (!(gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_EXTERNAL))
    return FALSE;
 
  fourcc = gdk_memory_format_get_dmabuf_fourcc (gsk_gpu_image_get_format (image));
  if (fourcc == 0)
    return FALSE;

  func_vkGetImageDrmFormatModifierPropertiesEXT = (PFN_vkGetImageDrmFormatModifierPropertiesEXT)
    vkGetDeviceProcAddr (self->display->vk_device, "vkGetImageDrmFormatModifierPropertiesEXT");
  func_vkGetMemoryFdKHR = (PFN_vkGetMemoryFdKHR) vkGetDeviceProcAddr (self->display->vk_device, "vkGetMemoryFdKHR");
  res = GSK_VK_CHECK (func_vkGetImageDrmFormatModifierPropertiesEXT, self->display->vk_device, self->vk_image, &properties);
  if (res != VK_SUCCESS)
    return FALSE;
  n_planes = gsk_vulkan_image_get_n_planes (self, properties.drmFormatModifier);
  if (n_planes == 0 || n_planes > GDK_DMABUF_MAX_PLANES)
    return FALSE;
  res = GSK_VK_CHECK (func_vkGetMemoryFdKHR, self->display->vk_device,
                                             &(VkMemoryGetFdInfoKHR) {
                                                 .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
                                                 .memory = self->allocation.vk_memory,
                                                 .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
                                             },
                                             &fd);
  if (res != VK_SUCCESS)
    return FALSE;

  builder = gdk_dmabuf_texture_builder_new ();
  gdk_dmabuf_texture_builder_set_display (builder, self->display);
  gdk_dmabuf_texture_builder_set_width (builder, gsk_gpu_image_get_width (image));
  gdk_dmabuf_texture_builder_set_height (builder, gsk_gpu_image_get_height (image));
  gdk_dmabuf_texture_builder_set_fourcc (builder, fourcc);
  gdk_dmabuf_texture_builder_set_modifier (builder, properties.drmFormatModifier);
  gdk_dmabuf_texture_builder_set_premultiplied (builder, !(gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_STRAIGHT_ALPHA));
  gdk_dmabuf_texture_builder_set_n_planes (builder, n_planes);
  
  for (plane = 0; plane < n_planes; plane++)
    {
      static const VkImageAspectFlagBits aspect[GDK_DMABUF_MAX_PLANES] = {
        VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT,
        VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT,
        VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT,
        VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT
      };
      vkGetImageSubresourceLayout (self->display->vk_device,
                                   self->vk_image,
                                   &(VkImageSubresource) {
                                       .aspectMask = aspect[plane],
                                       .mipLevel = 0,
                                       .arrayLayer = 0
                                   },
                                   &layout);
      gdk_dmabuf_texture_builder_set_fd (builder, plane, fd);
      gdk_dmabuf_texture_builder_set_stride (builder, plane, layout.rowPitch);
      gdk_dmabuf_texture_builder_set_offset (builder, plane, layout.offset);
    }

  texture = gdk_dmabuf_texture_builder_build (builder, close_the_fd, GINT_TO_POINTER (fd), &error);
  g_object_unref (builder);
  if (texture == NULL)
    {
      GDK_DEBUG (VULKAN, "Failed to create dmabuf texture: %s", error->message);
      g_clear_error (&error);
      close (fd);
      return NULL;
    }

  gsk_gpu_image_toggle_ref_texture (GSK_GPU_IMAGE (self), texture);

  return texture;
}
#endif

static void
gsk_vulkan_image_get_projection_matrix (GskGpuImage       *image,
                                        graphene_matrix_t *out_projection)
{
  graphene_matrix_t scale_z;

  GSK_GPU_IMAGE_CLASS (gsk_vulkan_image_parent_class)->get_projection_matrix (image, out_projection);

  graphene_matrix_init_from_float (&scale_z,
                                   (float[16]) {
                                       1,   0,   0,   0,
                                       0,   1,   0,   0,
                                       0,   0, 0.5,   0,
                                       0,   0, 0.5,   1
                                   });

  graphene_matrix_multiply (out_projection, &scale_z, out_projection);
}

static void
gsk_vulkan_image_finalize (GObject *object)
{
  GskVulkanImage *self = GSK_VULKAN_IMAGE (object);
  VkDevice device;

  device = self->display->vk_device;

  if (self->vk_framebuffer != VK_NULL_HANDLE)
    vkDestroyFramebuffer (device, self->vk_framebuffer, NULL);

  if (self->vk_framebuffer_image_view != VK_NULL_HANDLE &&
      self->vk_framebuffer_image_view != self->vk_image_view)
    vkDestroyImageView (device, self->vk_framebuffer_image_view, NULL);

  if (self->vk_image_view != VK_NULL_HANDLE)
    vkDestroyImageView (device, self->vk_image_view, NULL);

  if (self->vk_semaphore != VK_NULL_HANDLE)
    vkDestroySemaphore (device, self->vk_semaphore, NULL);

  /* memory is NULL for for_swapchain() images, where we don't own
   * the VkImage */
  if (self->allocator)
    {
      vkDestroyImage (device, self->vk_image, NULL);
      gsk_vulkan_free (self->allocator, &self->allocation);
      gsk_vulkan_allocator_unref (self->allocator);
    }

  gdk_display_unref_vulkan (self->display);
  g_object_unref (self->display);

  G_OBJECT_CLASS (gsk_vulkan_image_parent_class)->finalize (object);
}

static void
gsk_vulkan_image_class_init (GskVulkanImageClass *klass)
{
  GskGpuImageClass *image_class = GSK_GPU_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_projection_matrix = gsk_vulkan_image_get_projection_matrix;

  object_class->finalize = gsk_vulkan_image_finalize;
}

static void
gsk_vulkan_image_init (GskVulkanImage *self)
{
}

VkFramebuffer
gsk_vulkan_image_get_vk_framebuffer (GskVulkanImage *self,
                                     VkRenderPass    render_pass)
{
  if (self->vk_framebuffer)
    return self->vk_framebuffer;

  if (gsk_gpu_image_get_flags (GSK_GPU_IMAGE (self)) & GSK_GPU_IMAGE_CAN_MIPMAP)
    {
      GSK_VK_CHECK (vkCreateImageView, self->display->vk_device,
                                     &(VkImageViewCreateInfo) {
                                         .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                         .image = self->vk_image,
                                         .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                         .format = self->vk_format,
                                         .subresourceRange = {
                                             .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                             .baseMipLevel = 0,
                                             .levelCount = 1,
                                             .baseArrayLayer = 0,
                                             .layerCount = 1,
                                         }
                                     },
                                 NULL,
                                 &self->vk_framebuffer_image_view);
    }
  else
    {
      self->vk_framebuffer_image_view = self->vk_image_view;
    }

  GSK_VK_CHECK (vkCreateFramebuffer, self->display->vk_device,
                                     &(VkFramebufferCreateInfo) {
                                         .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                         .renderPass = render_pass,
                                         .attachmentCount = 1,
                                         .pAttachments = (VkImageView[1]) {
                                             self->vk_framebuffer_image_view,
                                         },
                                         .width = gsk_gpu_image_get_width (GSK_GPU_IMAGE (self)),
                                         .height = gsk_gpu_image_get_height (GSK_GPU_IMAGE (self)),
                                         .layers = 1
                                     },
                                     NULL,
                                     &self->vk_framebuffer);

  return self->vk_framebuffer;
}

VkSampler
gsk_vulkan_image_get_vk_sampler (GskVulkanImage *self)
{
  return self->vk_sampler;
}

VkImage
gsk_vulkan_image_get_vk_image (GskVulkanImage *self)
{
  return self->vk_image;
}

VkImageView
gsk_vulkan_image_get_vk_image_view (GskVulkanImage *self)
{
  return self->vk_image_view;
}

VkPipelineStageFlags
gsk_vulkan_image_get_vk_pipeline_stage (GskVulkanImage *self)
{
  return self->vk_pipeline_stage;
}

VkImageLayout
gsk_vulkan_image_get_vk_image_layout (GskVulkanImage *self)
{
  return self->vk_image_layout;
}

VkAccessFlags
gsk_vulkan_image_get_vk_access (GskVulkanImage *self)
{
  return self->vk_access;
}

void
gsk_vulkan_image_set_vk_image_layout (GskVulkanImage       *self,
                                      VkPipelineStageFlags  stage,
                                      VkImageLayout         image_layout,
                                      VkAccessFlags         access)
{
  self->vk_pipeline_stage = stage;
  self->vk_image_layout = image_layout;
  self->vk_access = access;
}

void
gsk_vulkan_image_transition (GskVulkanImage       *self,
                             GskVulkanSemaphores  *semaphores,
                             VkCommandBuffer       command_buffer,
                             VkPipelineStageFlags  stage,
                             VkImageLayout         image_layout,
                             VkAccessFlags         access)
{
  if (self->vk_pipeline_stage == stage &&
      self->vk_image_layout == image_layout &&
      self->vk_access == access)
    return;

  if (self->vk_pipeline_stage == VK_IMAGE_LAYOUT_UNDEFINED &&
      self->vk_semaphore)
    {
      gsk_vulkan_semaphores_add_wait (semaphores, self->vk_semaphore, stage);
    }

  vkCmdPipelineBarrier (command_buffer,
                        self->vk_pipeline_stage,
                        stage,
                        0,
                        0, NULL,
                        0, NULL,
                        1, &(VkImageMemoryBarrier) {
                            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                            .srcAccessMask = self->vk_access,
                            .dstAccessMask = access,
                            .oldLayout = self->vk_image_layout,
                            .newLayout = image_layout,
                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .image = self->vk_image,
                            .subresourceRange = {
                              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                              .baseMipLevel = 0,
                              .levelCount = VK_REMAINING_MIP_LEVELS,
                              .baseArrayLayer = 0,
                              .layerCount = 1
                            },
                        });

  gsk_vulkan_image_set_vk_image_layout (self, stage, image_layout, access);
}

VkFormat
gsk_vulkan_image_get_vk_format (GskVulkanImage *self)
{
  return self->vk_format;
}

