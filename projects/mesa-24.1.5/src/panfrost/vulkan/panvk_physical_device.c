/*
 * Copyright © 2021 Collabora Ltd.
 *
 * Derived from tu_device.c which is:
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include <sys/sysinfo.h>

#include "util/disk_cache.h"
#include "git_sha1.h"

#include "vk_device.h"
#include "vk_drm_syncobj.h"
#include "vk_format.h"
#include "vk_log.h"
#include "vk_util.h"

#include "panvk_device.h"
#include "panvk_entrypoints.h"
#include "panvk_instance.h"
#include "panvk_physical_device.h"
#include "panvk_wsi.h"

#include "pan_format.h"
#include "pan_props.h"

#include "genxml/gen_macros.h"

#define ARM_VENDOR_ID        0x13b5
#define MAX_VIEWPORTS        1
#define MAX_PUSH_DESCRIPTORS 32
/* We reserve one ubo for push constant, one for sysvals and one per-set for the
 * descriptor metadata  */
#define RESERVED_UBO_COUNT                   6
#define MAX_INLINE_UNIFORM_BLOCK_DESCRIPTORS 32 - RESERVED_UBO_COUNT
#define MAX_INLINE_UNIFORM_BLOCK_SIZE        (1 << 16)

static int
get_cache_uuid(uint16_t family, void *uuid)
{
   uint32_t mesa_timestamp;
   uint16_t f = family;

   if (!disk_cache_get_function_timestamp(get_cache_uuid, &mesa_timestamp))
      return -1;

   memset(uuid, 0, VK_UUID_SIZE);
   memcpy(uuid, &mesa_timestamp, 4);
   memcpy((char *)uuid + 4, &f, 2);
   snprintf((char *)uuid + 6, VK_UUID_SIZE - 10, "pan");
   return 0;
}

static void
get_driver_uuid(void *uuid)
{
   memset(uuid, 0, VK_UUID_SIZE);
   snprintf(uuid, VK_UUID_SIZE, "panfrost");
}

static void
get_device_uuid(void *uuid)
{
   memset(uuid, 0, VK_UUID_SIZE);
}

static void
get_device_extensions(const struct panvk_physical_device *device,
                      struct vk_device_extension_table *ext)
{
   *ext = (struct vk_device_extension_table){
      .KHR_copy_commands2 = true,
      .KHR_shader_expect_assume = true,
      .KHR_storage_buffer_storage_class = true,
      .KHR_descriptor_update_template = true,
      .KHR_driver_properties = true,
      .KHR_push_descriptor = true,
#ifdef PANVK_USE_WSI_PLATFORM
      .KHR_swapchain = true,
#endif
      .KHR_synchronization2 = true,
      .KHR_variable_pointers = true,
      .EXT_custom_border_color = true,
      .EXT_index_type_uint8 = true,
      .EXT_vertex_attribute_divisor = true,
   };
}

static void
get_features(const struct panvk_physical_device *device,
             struct vk_features *features)
{
   unsigned arch = pan_arch(device->kmod.props.gpu_prod_id);

   *features = (struct vk_features){
      /* Vulkan 1.0 */
      .robustBufferAccess = true,
      .fullDrawIndexUint32 = true,
      .independentBlend = true,
      .logicOp = true,
      .wideLines = true,
      .largePoints = true,
      .textureCompressionETC2 = true,
      .textureCompressionASTC_LDR = true,
      .shaderUniformBufferArrayDynamicIndexing = true,
      .shaderSampledImageArrayDynamicIndexing = true,
      .shaderStorageBufferArrayDynamicIndexing = true,
      .shaderStorageImageArrayDynamicIndexing = true,

      /* Vulkan 1.1 */
      .storageBuffer16BitAccess = false,
      .uniformAndStorageBuffer16BitAccess = false,
      .storagePushConstant16 = false,
      .storageInputOutput16 = false,
      .multiview = false,
      .multiviewGeometryShader = false,
      .multiviewTessellationShader = false,
      .variablePointersStorageBuffer = true,
      .variablePointers = true,
      .protectedMemory = false,
      .samplerYcbcrConversion = false,
      .shaderDrawParameters = false,

      /* Vulkan 1.2 */
      .samplerMirrorClampToEdge = false,
      .drawIndirectCount = false,
      .storageBuffer8BitAccess = false,
      .uniformAndStorageBuffer8BitAccess = false,
      .storagePushConstant8 = false,
      .shaderBufferInt64Atomics = false,
      .shaderSharedInt64Atomics = false,
      .shaderFloat16 = false,
      .shaderInt8 = false,

      .descriptorIndexing = false,
      .shaderInputAttachmentArrayDynamicIndexing = false,
      .shaderUniformTexelBufferArrayDynamicIndexing = false,
      .shaderStorageTexelBufferArrayDynamicIndexing = false,
      .shaderUniformBufferArrayNonUniformIndexing = false,
      .shaderSampledImageArrayNonUniformIndexing = false,
      .shaderStorageBufferArrayNonUniformIndexing = false,
      .shaderStorageImageArrayNonUniformIndexing = false,
      .shaderInputAttachmentArrayNonUniformIndexing = false,
      .shaderUniformTexelBufferArrayNonUniformIndexing = false,
      .shaderStorageTexelBufferArrayNonUniformIndexing = false,
      .descriptorBindingUniformBufferUpdateAfterBind = false,
      .descriptorBindingSampledImageUpdateAfterBind = false,
      .descriptorBindingStorageImageUpdateAfterBind = false,
      .descriptorBindingStorageBufferUpdateAfterBind = false,
      .descriptorBindingUniformTexelBufferUpdateAfterBind = false,
      .descriptorBindingStorageTexelBufferUpdateAfterBind = false,
      .descriptorBindingUpdateUnusedWhilePending = false,
      .descriptorBindingPartiallyBound = false,
      .descriptorBindingVariableDescriptorCount = false,
      .runtimeDescriptorArray = false,

      .samplerFilterMinmax = false,
      .scalarBlockLayout = false,
      .imagelessFramebuffer = false,
      .uniformBufferStandardLayout = false,
      .shaderSubgroupExtendedTypes = false,
      .separateDepthStencilLayouts = false,
      .hostQueryReset = false,
      .timelineSemaphore = false,
      .bufferDeviceAddress = true,
      .bufferDeviceAddressCaptureReplay = false,
      .bufferDeviceAddressMultiDevice = false,
      .vulkanMemoryModel = false,
      .vulkanMemoryModelDeviceScope = false,
      .vulkanMemoryModelAvailabilityVisibilityChains = false,
      .shaderOutputViewportIndex = false,
      .shaderOutputLayer = false,
      .subgroupBroadcastDynamicId = false,

      /* Vulkan 1.3 */
      .robustImageAccess = false,
      .inlineUniformBlock = false,
      .descriptorBindingInlineUniformBlockUpdateAfterBind = false,
      .pipelineCreationCacheControl = false,
      .privateData = true,
      .shaderDemoteToHelperInvocation = false,
      .shaderTerminateInvocation = false,
      .subgroupSizeControl = false,
      .computeFullSubgroups = false,
      .synchronization2 = true,
      .textureCompressionASTC_HDR = false,
      .shaderZeroInitializeWorkgroupMemory = false,
      .dynamicRendering = false,
      .shaderIntegerDotProduct = false,
      .maintenance4 = false,

      /* VK_EXT_index_type_uint8 */
      .indexTypeUint8 = true,

      /* VK_EXT_vertex_attribute_divisor */
      .vertexAttributeInstanceRateDivisor = true,
      .vertexAttributeInstanceRateZeroDivisor = true,

      /* VK_EXT_depth_clip_enable */
      .depthClipEnable = true,

      /* VK_EXT_4444_formats */
      .formatA4R4G4B4 = true,
      .formatA4B4G4R4 = true,

      /* VK_EXT_custom_border_color */
      .customBorderColors = true,

      /* v7 doesn't support AFBC(BGR). We need to tweak the texture swizzle to
       * make it work, which forces us to apply the same swizzle on the border
       * color, meaning we need to know the format when preparing the border
       * color.
       */
      .customBorderColorWithoutFormat = arch != 7,

      /* VK_KHR_shader_expect_assume */
      .shaderExpectAssume = true,
   };
}

static void
get_device_properties(const struct panvk_physical_device *device,
                      struct vk_properties *properties)
{
   /* HW supports MSAA 4, 8 and 16, but we limit ourselves to MSAA 4 for now. */
   VkSampleCountFlags sample_counts =
      VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT;

   uint64_t os_page_size = 4096;
   os_get_page_size(&os_page_size);

   *properties = (struct vk_properties){
      .apiVersion = panvk_get_vk_version(),
      .driverVersion = vk_get_driver_version(),
      .vendorID = ARM_VENDOR_ID,

      /* Collect arch_major, arch_minor, arch_rev and product_major,
       * as done by the Arm driver.
       */
      .deviceID = device->kmod.props.gpu_prod_id << 16,
      .deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,

      /* Vulkan 1.0 limits */
      /* Maximum texture dimension is 2^16. */
      .maxImageDimension1D = (1 << 16),
      .maxImageDimension2D = (1 << 16),
      .maxImageDimension3D = (1 << 16),
      .maxImageDimensionCube = (1 << 16),
      .maxImageArrayLayers = (1 << 16),
      /* Currently limited by the 1D texture size, which is 2^16.
       * TODO: If we expose buffer views as 2D textures, we can increase the
       * limit.
       */
      .maxTexelBufferElements = (1 << 16),
      /* Each uniform entry is 16-byte and the number of entries is encoded in a
       * 12-bit field, with the minus(1) modifier, which gives 2^20.
       */
      .maxUniformBufferRange = 1 << 20,
      /* Storage buffer access is lowered to globals, so there's no limit here,
       * except for the SW-descriptor we use to encode storage buffer
       * descriptors, where the size is a 32-bit field.
       */
      .maxStorageBufferRange = UINT32_MAX,
      /* 128 bytes of push constants, so we're aligned with the minimum Vulkan
       * requirements.
       */
      .maxPushConstantsSize = 128,
      /* There's no HW limit here. Should we advertize something smaller? */
      .maxMemoryAllocationCount = UINT32_MAX,
      /* Again, no hardware limit, but most drivers seem to advertive 64k. */
      .maxSamplerAllocationCount = 64 * 1024,
      /* A cache line. */
      .bufferImageGranularity = 64,
      /* Sparse binding not supported yet. */
      .sparseAddressSpaceSize = 0,
      /* Software limit. Pick the minimum required by Vulkan, because Bifrost
       * GPUs don't have unified descriptor tables, which forces us to
       * agregatte all descriptors from all sets and dispatch them to per-type
       * descriptor tables emitted at draw/dispatch time.
       * The more sets we support the more copies we are likely to have to do
       * at draw time.
       */
      .maxBoundDescriptorSets = 4,
      /* MALI_RENDERER_STATE::sampler_count is 16-bit. */
      .maxPerStageDescriptorSamplers = UINT16_MAX,
      .maxDescriptorSetSamplers = UINT16_MAX,
      /* MALI_RENDERER_STATE::uniform_buffer_count is 8-bit. We reserve 32 slots
       * for our internal UBOs.
       */
      .maxPerStageDescriptorUniformBuffers = UINT8_MAX - 32,
      .maxDescriptorSetUniformBuffers = UINT8_MAX - 32,
      /* SSBOs are limited by the size of a uniform buffer which contains our
       * panvk_ssbo_desc objects.
       * panvk_ssbo_desc is 16-byte, and each uniform entry in the Mali UBO is
       * 16-byte too. The number of entries is encoded in a 12-bit field, with
       * a minus(1) modifier, which gives a maximum of 2^12 SSBO
       * descriptors.
       */
      .maxPerStageDescriptorStorageBuffers = 1 << 12,
      .maxDescriptorSetStorageBuffers = 1 << 12,
      /* MALI_RENDERER_STATE::sampler_count is 16-bit. */
      .maxPerStageDescriptorSampledImages = UINT16_MAX,
      .maxDescriptorSetSampledImages = UINT16_MAX,
      /* MALI_ATTRIBUTE::buffer_index is 9-bit, and each image takes two
       * MALI_ATTRIBUTE_BUFFER slots, which gives a maximum of (1 << 8) images.
       */
      .maxPerStageDescriptorStorageImages = 1 << 8,
      .maxDescriptorSetStorageImages = 1 << 8,
      /* A maximum of 8 color render targets, and one depth-stencil render
       * target.
       */
      .maxPerStageDescriptorInputAttachments = 9,
      .maxDescriptorSetInputAttachments = 9,
      /* Could be the sum of all maxPerStageXxx values, but we limit ourselves
       * to 2^16 to make things simpler.
       */
      .maxPerStageResources = 1 << 16,
      /* Software limits to keep VkCommandBuffer tracking sane. */
      .maxDescriptorSetUniformBuffersDynamic = 16,
      .maxDescriptorSetStorageBuffersDynamic = 8,
      /* Software limit to keep VkCommandBuffer tracking sane. The HW supports
       * up to 2^9 vertex attributes.
       */
      .maxVertexInputAttributes = 16,
      .maxVertexInputBindings = 16,
      /* MALI_ATTRIBUTE::offset is 32-bit. */
      .maxVertexInputAttributeOffset = UINT32_MAX,
      /* MALI_ATTRIBUTE_BUFFER::stride is 32-bit. */
      .maxVertexInputBindingStride = UINT32_MAX,
      /* 32 vec4 varyings. */
      .maxVertexOutputComponents = 128,
      /* Tesselation shaders not supported. */
      .maxTessellationGenerationLevel = 0,
      .maxTessellationPatchSize = 0,
      .maxTessellationControlPerVertexInputComponents = 0,
      .maxTessellationControlPerVertexOutputComponents = 0,
      .maxTessellationControlPerPatchOutputComponents = 0,
      .maxTessellationControlTotalOutputComponents = 0,
      .maxTessellationEvaluationInputComponents = 0,
      .maxTessellationEvaluationOutputComponents = 0,
      /* Geometry shaders not supported. */
      .maxGeometryShaderInvocations = 0,
      .maxGeometryInputComponents = 0,
      .maxGeometryOutputComponents = 0,
      .maxGeometryOutputVertices = 0,
      .maxGeometryTotalOutputComponents = 0,
      /* 32 vec4 varyings. */
      .maxFragmentInputComponents = 128,
      /* 8 render targets. */
      .maxFragmentOutputAttachments = 8,
      /* We don't support dual source blending yet. */
      .maxFragmentDualSrcAttachments = 0,
      /* 8 render targets, 2^12 storage buffers and 2^8 storage images (see
       * above).
       */
      .maxFragmentCombinedOutputResources = 8 + (1 << 12) + (1 << 8),
      /* MALI_LOCAL_STORAGE::wls_size_{base,scale} allows us to have up to
       * (7 << 30) bytes of shared memory, but we cap it to 32K as it doesn't
       * really make sense to expose this amount of memory, especially since
       * it's backed by global memory anyway.
       */
      .maxComputeSharedMemorySize = 32768,
      /* Software limit to meet Vulkan 1.0 requirements. We split the
       * dispatch in several jobs if it's too big.
       */
      .maxComputeWorkGroupCount = {65535, 65535, 65535},
      /* We have 10 bits to encode the local-size, and there's a minus(1)
       * modifier, so, a size of 1 takes no bit.
       */
      .maxComputeWorkGroupInvocations = 1 << 10,
      .maxComputeWorkGroupSize = {1 << 10, 1 << 10, 1 << 10},
      /* 8-bit subpixel precision. */
      .subPixelPrecisionBits = 8,
      .subTexelPrecisionBits = 8,
      .mipmapPrecisionBits = 8,
      /* Software limit. */
      .maxDrawIndexedIndexValue = UINT32_MAX,
      /* Make it one for now. */
      .maxDrawIndirectCount = 1,
      .maxSamplerLodBias = 255,
      .maxSamplerAnisotropy = 16,
      .maxViewports = 1,
      /* Same as the framebuffer limit. */
      .maxViewportDimensions = {(1 << 14), (1 << 14)},
      /* Encoded in a 16-bit signed integer. */
      .viewportBoundsRange = {INT16_MIN, INT16_MAX},
      .viewportSubPixelBits = 0,
      /* Align on a page. */
      .minMemoryMapAlignment = os_page_size,
      /* Some compressed texture formats require 128-byte alignment. */
      .minTexelBufferOffsetAlignment = 64,
      /* Always aligned on a uniform slot (vec4). */
      .minUniformBufferOffsetAlignment = 16,
      /* Lowered to global accesses, which happen at the 32-bit granularity. */
      .minStorageBufferOffsetAlignment = 4,
      /* Signed 4-bit value. */
      .minTexelOffset = -8,
      .maxTexelOffset = 7,
      .minTexelGatherOffset = -8,
      .maxTexelGatherOffset = 7,
      .minInterpolationOffset = -0.5,
      .maxInterpolationOffset = 0.5,
      .subPixelInterpolationOffsetBits = 8,
      .maxFramebufferWidth = (1 << 14),
      .maxFramebufferHeight = (1 << 14),
      .maxFramebufferLayers = 256,
      .framebufferColorSampleCounts = sample_counts,
      .framebufferDepthSampleCounts = sample_counts,
      .framebufferStencilSampleCounts = sample_counts,
      .framebufferNoAttachmentsSampleCounts = sample_counts,
      .maxColorAttachments = 8,
      .sampledImageColorSampleCounts = sample_counts,
      .sampledImageIntegerSampleCounts = VK_SAMPLE_COUNT_1_BIT,
      .sampledImageDepthSampleCounts = sample_counts,
      .sampledImageStencilSampleCounts = sample_counts,
      .storageImageSampleCounts = VK_SAMPLE_COUNT_1_BIT,
      .maxSampleMaskWords = 1,
      .timestampComputeAndGraphics = false,
      .timestampPeriod = 0,
      .maxClipDistances = 0,
      .maxCullDistances = 0,
      .maxCombinedClipAndCullDistances = 0,
      .discreteQueuePriorities = 2,
      .pointSizeRange = {0.125, 4095.9375},
      .lineWidthRange = {0.0, 7.9921875},
      .pointSizeGranularity = (1.0 / 16.0),
      .lineWidthGranularity = (1.0 / 128.0),
      .strictLines = false,
      .standardSampleLocations = true,
      .optimalBufferCopyOffsetAlignment = 64,
      .optimalBufferCopyRowPitchAlignment = 64,
      .nonCoherentAtomSize = 64,

      /* Vulkan 1.0 sparse properties */
      .sparseResidencyNonResidentStrict = false,
      .sparseResidencyAlignedMipSize = false,
      .sparseResidencyStandard2DBlockShape = false,
      .sparseResidencyStandard2DMultisampleBlockShape = false,
      .sparseResidencyStandard3DBlockShape = false,

      /* Vulkan 1.1 properties */
      /* XXX: 1.1 support */
      .subgroupSize = 8,
      .subgroupSupportedStages = VK_SHADER_STAGE_ALL,
      .subgroupSupportedOperations =
         VK_SUBGROUP_FEATURE_ARITHMETIC_BIT | VK_SUBGROUP_FEATURE_BALLOT_BIT |
         VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_CLUSTERED_BIT |
         VK_SUBGROUP_FEATURE_QUAD_BIT | VK_SUBGROUP_FEATURE_SHUFFLE_BIT |
         VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT |
         VK_SUBGROUP_FEATURE_VOTE_BIT,
      .subgroupQuadOperationsInAllStages = false,
      .pointClippingBehavior = VK_POINT_CLIPPING_BEHAVIOR_ALL_CLIP_PLANES,
      .maxMultiviewViewCount = 0,
      .maxMultiviewInstanceIndex = 0,
      .protectedNoFault = false,
      /* Make sure everything is addressable by a signed 32-bit int, and
       * our largest descriptors are 96 bytes. */
      .maxPerSetDescriptors = (1ull << 31) / 96,
      /* Our buffer size fields allow only this much */
      .maxMemoryAllocationSize = UINT32_MAX,

      /* Vulkan 1.2 properties */
      /* XXX: 1.2 support */
      /* XXX: VK_KHR_depth_stencil_resolve */
      .supportedDepthResolveModes = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
      .supportedStencilResolveModes = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,
      .independentResolveNone = true,
      .independentResolve = true,
      /* VK_KHR_driver_properties */
      .driverID = VK_DRIVER_ID_MESA_PANVK,
      .conformanceVersion = (VkConformanceVersion){0, 0, 0, 0},
      /* XXX: VK_KHR_shader_float_controls */
      .denormBehaviorIndependence = VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL,
      .roundingModeIndependence = VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL,
      .shaderSignedZeroInfNanPreserveFloat16 = true,
      .shaderSignedZeroInfNanPreserveFloat32 = true,
      .shaderSignedZeroInfNanPreserveFloat64 = false,
      .shaderDenormPreserveFloat16 = true,
      .shaderDenormPreserveFloat32 = true,
      .shaderDenormPreserveFloat64 = false,
      .shaderDenormFlushToZeroFloat16 = true,
      .shaderDenormFlushToZeroFloat32 = true,
      .shaderDenormFlushToZeroFloat64 = false,
      .shaderRoundingModeRTEFloat16 = true,
      .shaderRoundingModeRTEFloat32 = true,
      .shaderRoundingModeRTEFloat64 = false,
      .shaderRoundingModeRTZFloat16 = true,
      .shaderRoundingModeRTZFloat32 = true,
      .shaderRoundingModeRTZFloat64 = false,
      /* XXX: VK_EXT_descriptor_indexing */
      .maxUpdateAfterBindDescriptorsInAllPools = 0,
      .shaderUniformBufferArrayNonUniformIndexingNative = false,
      .shaderSampledImageArrayNonUniformIndexingNative = false,
      .shaderStorageBufferArrayNonUniformIndexingNative = false,
      .shaderStorageImageArrayNonUniformIndexingNative = false,
      .shaderInputAttachmentArrayNonUniformIndexingNative = false,
      .robustBufferAccessUpdateAfterBind = false,
      .quadDivergentImplicitLod = false,
      .maxPerStageDescriptorUpdateAfterBindSamplers = 0,
      .maxPerStageDescriptorUpdateAfterBindUniformBuffers = 0,
      .maxPerStageDescriptorUpdateAfterBindStorageBuffers = 0,
      .maxPerStageDescriptorUpdateAfterBindSampledImages = 0,
      .maxPerStageDescriptorUpdateAfterBindStorageImages = 0,
      .maxPerStageDescriptorUpdateAfterBindInputAttachments = 0,
      .maxPerStageDescriptorUpdateAfterBindInputAttachments = 0,
      .maxPerStageUpdateAfterBindResources = 0,
      .maxDescriptorSetUpdateAfterBindSamplers = 0,
      .maxDescriptorSetUpdateAfterBindUniformBuffers = 0,
      .maxDescriptorSetUpdateAfterBindUniformBuffersDynamic = 0,
      .maxDescriptorSetUpdateAfterBindStorageBuffers = 0,
      .maxDescriptorSetUpdateAfterBindStorageBuffersDynamic = 0,
      .maxDescriptorSetUpdateAfterBindSampledImages = 0,
      .maxDescriptorSetUpdateAfterBindStorageImages = 0,
      .maxDescriptorSetUpdateAfterBindInputAttachments = 0,
      /* XXX: VK_EXT_sampler_filter_minmax */
      .filterMinmaxSingleComponentFormats = false,
      .filterMinmaxImageComponentMapping = false,
      /* XXX: VK_KHR_timeline_semaphore */
      .maxTimelineSemaphoreValueDifference = INT64_MAX,
      .framebufferIntegerColorSampleCounts = sample_counts,

      /* Vulkan 1.3 properties */
      /* XXX: 1.3 support */
      /* XXX: VK_EXT_subgroup_size_control */
      .minSubgroupSize = 8,
      .maxSubgroupSize = 8,
      .maxComputeWorkgroupSubgroups = 48,
      .requiredSubgroupSizeStages = VK_SHADER_STAGE_ALL,
      /* XXX: VK_EXT_inline_uniform_block */
      .maxInlineUniformBlockSize = MAX_INLINE_UNIFORM_BLOCK_SIZE,
      .maxPerStageDescriptorInlineUniformBlocks =
         MAX_INLINE_UNIFORM_BLOCK_DESCRIPTORS,
      .maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks =
         MAX_INLINE_UNIFORM_BLOCK_DESCRIPTORS,
      .maxDescriptorSetInlineUniformBlocks =
         MAX_INLINE_UNIFORM_BLOCK_DESCRIPTORS,
      .maxDescriptorSetUpdateAfterBindInlineUniformBlocks =
         MAX_INLINE_UNIFORM_BLOCK_DESCRIPTORS,
      .maxInlineUniformTotalSize =
         MAX_INLINE_UNIFORM_BLOCK_DESCRIPTORS * MAX_INLINE_UNIFORM_BLOCK_SIZE,
      /* XXX: VK_KHR_shader_integer_dot_product */
      .integerDotProduct8BitUnsignedAccelerated = true,
      .integerDotProduct8BitSignedAccelerated = true,
      .integerDotProduct4x8BitPackedUnsignedAccelerated = true,
      .integerDotProduct4x8BitPackedSignedAccelerated = true,
      /* XXX: VK_EXT_texel_buffer_alignment */
      .storageTexelBufferOffsetAlignmentBytes = 64,
      .storageTexelBufferOffsetSingleTexelAlignment = false,
      .uniformTexelBufferOffsetAlignmentBytes = 4,
      .uniformTexelBufferOffsetSingleTexelAlignment = true,
      /* XXX: VK_KHR_maintenance4 */
      .maxBufferSize = 1 << 30,

      /* VK_EXT_custom_border_color */
      .maxCustomBorderColorSamplers = 32768,

      /* VK_KHR_vertex_attribute_divisor */
      /* We will have to restrict this a bit for multiview */
      .maxVertexAttribDivisor = UINT32_MAX,
      .supportsNonZeroFirstInstance = false,

      /* VK_KHR_push_descriptor */
      .maxPushDescriptors = MAX_PUSH_DESCRIPTORS,
   };

   snprintf(properties->deviceName, sizeof(properties->deviceName), "%s",
            device->name);

   memcpy(properties->pipelineCacheUUID, device->cache_uuid, VK_UUID_SIZE);

   memcpy(properties->driverUUID, device->driver_uuid, VK_UUID_SIZE);
   memcpy(properties->deviceUUID, device->device_uuid, VK_UUID_SIZE);

   snprintf(properties->driverName, VK_MAX_DRIVER_NAME_SIZE, "panvk");
   snprintf(properties->driverInfo, VK_MAX_DRIVER_INFO_SIZE,
            "Mesa " PACKAGE_VERSION MESA_GIT_SHA1);
}

void
panvk_physical_device_finish(struct panvk_physical_device *device)
{
   panvk_wsi_finish(device);

   pan_kmod_dev_destroy(device->kmod.dev);
   if (device->master_fd != -1)
      close(device->master_fd);

   vk_physical_device_finish(&device->vk);
}

VkResult
panvk_physical_device_init(struct panvk_physical_device *device,
                           struct panvk_instance *instance,
                           drmDevicePtr drm_device)
{
   const char *path = drm_device->nodes[DRM_NODE_RENDER];
   VkResult result = VK_SUCCESS;
   drmVersionPtr version;
   int fd;
   int master_fd = -1;

   if (!getenv("PAN_I_WANT_A_BROKEN_VULKAN_DRIVER")) {
      return vk_errorf(
         instance, VK_ERROR_INCOMPATIBLE_DRIVER,
         "WARNING: panvk is not a conformant vulkan implementation, "
         "pass PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1 if you know what you're doing.");
   }

   fd = open(path, O_RDWR | O_CLOEXEC);
   if (fd < 0) {
      return vk_errorf(instance, VK_ERROR_INCOMPATIBLE_DRIVER,
                       "failed to open device %s", path);
   }

   version = drmGetVersion(fd);
   if (!version) {
      close(fd);
      return vk_errorf(instance, VK_ERROR_INCOMPATIBLE_DRIVER,
                       "failed to query kernel driver version for device %s",
                       path);
   }

   if (strcmp(version->name, "panfrost")) {
      drmFreeVersion(version);
      close(fd);
      return vk_errorf(instance, VK_ERROR_INCOMPATIBLE_DRIVER,
                       "device %s does not use the panfrost kernel driver",
                       path);
   }

   drmFreeVersion(version);

   if (instance->debug_flags & PANVK_DEBUG_STARTUP)
      vk_logi(VK_LOG_NO_OBJS(instance), "Found compatible device '%s'.", path);

   device->kmod.dev = pan_kmod_dev_create(fd, PAN_KMOD_DEV_FLAG_OWNS_FD,
                                          &instance->kmod.allocator);
   pan_kmod_dev_query_props(device->kmod.dev, &device->kmod.props);

   unsigned arch = pan_arch(device->kmod.props.gpu_prod_id);

   if (arch <= 5 || arch >= 8) {
      result = vk_errorf(instance, VK_ERROR_INCOMPATIBLE_DRIVER,
                         "%s not supported", device->model->name);
      goto fail;
   }

   if (instance->vk.enabled_extensions.KHR_display) {
      master_fd = open(drm_device->nodes[DRM_NODE_PRIMARY], O_RDWR | O_CLOEXEC);
      if (master_fd >= 0) {
         /* TODO: free master_fd is accel is not working? */
      }
   }

   device->master_fd = master_fd;

   device->model = panfrost_get_model(device->kmod.props.gpu_prod_id,
                                      device->kmod.props.gpu_variant);
   device->formats.all = panfrost_format_table(arch);
   device->formats.blendable = panfrost_blendable_format_table(arch);

   memset(device->name, 0, sizeof(device->name));
   sprintf(device->name, "%s", device->model->name);

   if (get_cache_uuid(device->kmod.props.gpu_prod_id, device->cache_uuid)) {
      result = vk_errorf(instance, VK_ERROR_INITIALIZATION_FAILED,
                         "cannot generate UUID");
      goto fail;
   }

   vk_warn_non_conformant_implementation("panvk");

   get_driver_uuid(&device->driver_uuid);
   get_device_uuid(&device->device_uuid);

   device->drm_syncobj_type = vk_drm_syncobj_get_type(device->kmod.dev->fd);
   /* We don't support timelines in the uAPI yet and we don't want it getting
    * suddenly turned on by vk_drm_syncobj_get_type() without us adding panvk
    * code for it first.
    */
   device->drm_syncobj_type.features &= ~VK_SYNC_FEATURE_TIMELINE;

   struct vk_device_extension_table supported_extensions;
   get_device_extensions(device, &supported_extensions);

   struct vk_features supported_features;
   get_features(device, &supported_features);

   struct vk_properties properties;
   get_device_properties(device, &properties);

   struct vk_physical_device_dispatch_table dispatch_table;
   vk_physical_device_dispatch_table_from_entrypoints(
      &dispatch_table, &panvk_physical_device_entrypoints, true);
   vk_physical_device_dispatch_table_from_entrypoints(
      &dispatch_table, &wsi_physical_device_entrypoints, false);

   result = vk_physical_device_init(&device->vk, &instance->vk,
                                    &supported_extensions, &supported_features,
                                    &properties, &dispatch_table);

   if (result != VK_SUCCESS) {
      vk_error(instance, result);
      goto fail;
   }

   device->sync_types[0] = &device->drm_syncobj_type;
   device->sync_types[1] = NULL;
   device->vk.supported_sync_types = device->sync_types;

   result = panvk_wsi_init(device);
   if (result != VK_SUCCESS) {
      vk_error(instance, result);
      goto fail;
   }

   return VK_SUCCESS;

fail:
   if (device->vk.instance)
      vk_physical_device_finish(&device->vk);

   if (device->kmod.dev)
      pan_kmod_dev_destroy(device->kmod.dev);

   if (fd != -1)
      close(fd);
   if (master_fd != -1)
      close(master_fd);
   return result;
}

static const VkQueueFamilyProperties panvk_queue_family_properties = {
   .queueFlags =
      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT,
   .queueCount = 1,
   .timestampValidBits = 0,
   .minImageTransferGranularity = {1, 1, 1},
};

VKAPI_ATTR void VKAPI_CALL
panvk_GetPhysicalDeviceQueueFamilyProperties2(
   VkPhysicalDevice physicalDevice, uint32_t *pQueueFamilyPropertyCount,
   VkQueueFamilyProperties2 *pQueueFamilyProperties)
{
   VK_OUTARRAY_MAKE_TYPED(VkQueueFamilyProperties2, out, pQueueFamilyProperties,
                          pQueueFamilyPropertyCount);

   vk_outarray_append_typed(VkQueueFamilyProperties2, &out, p)
   {
      p->queueFamilyProperties = panvk_queue_family_properties;
   }
}

static uint64_t
get_system_heap_size()
{
   struct sysinfo info;
   sysinfo(&info);

   uint64_t total_ram = (uint64_t)info.totalram * info.mem_unit;

   /* We don't want to burn too much ram with the GPU.  If the user has 4GiB
    * or less, we use at most half.  If they have more than 4GiB, we use 3/4.
    */
   uint64_t available_ram;
   if (total_ram <= 4ull * 1024 * 1024 * 1024)
      available_ram = total_ram / 2;
   else
      available_ram = total_ram * 3 / 4;

   return available_ram;
}

VKAPI_ATTR void VKAPI_CALL
panvk_GetPhysicalDeviceMemoryProperties2(
   VkPhysicalDevice physicalDevice,
   VkPhysicalDeviceMemoryProperties2 *pMemoryProperties)
{
   pMemoryProperties->memoryProperties = (VkPhysicalDeviceMemoryProperties){
      .memoryHeapCount = 1,
      .memoryHeaps[0].size = get_system_heap_size(),
      .memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT,
      .memoryTypeCount = 1,
      .memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      .memoryTypes[0].heapIndex = 0,
   };
}

VKAPI_ATTR void VKAPI_CALL
panvk_GetPhysicalDeviceExternalSemaphoreProperties(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceExternalSemaphoreInfo *pExternalSemaphoreInfo,
   VkExternalSemaphoreProperties *pExternalSemaphoreProperties)
{
   if ((pExternalSemaphoreInfo->handleType ==
           VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT ||
        pExternalSemaphoreInfo->handleType ==
           VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT)) {
      pExternalSemaphoreProperties->exportFromImportedHandleTypes =
         VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT |
         VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
      pExternalSemaphoreProperties->compatibleHandleTypes =
         VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT |
         VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
      pExternalSemaphoreProperties->externalSemaphoreFeatures =
         VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT |
         VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT;
   } else {
      pExternalSemaphoreProperties->exportFromImportedHandleTypes = 0;
      pExternalSemaphoreProperties->compatibleHandleTypes = 0;
      pExternalSemaphoreProperties->externalSemaphoreFeatures = 0;
   }
}

VKAPI_ATTR void VKAPI_CALL
panvk_GetPhysicalDeviceExternalFenceProperties(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceExternalFenceInfo *pExternalFenceInfo,
   VkExternalFenceProperties *pExternalFenceProperties)
{
   pExternalFenceProperties->exportFromImportedHandleTypes = 0;
   pExternalFenceProperties->compatibleHandleTypes = 0;
   pExternalFenceProperties->externalFenceFeatures = 0;
}

#define DEVICE_PER_ARCH_FUNCS(_ver)                                            \
   VkResult panvk_v##_ver##_create_device(                                     \
      struct panvk_physical_device *physical_device,                           \
      const VkDeviceCreateInfo *pCreateInfo,                                   \
      const VkAllocationCallbacks *pAllocator, VkDevice *pDevice);             \
                                                                               \
   void panvk_v##_ver##_destroy_device(                                        \
      struct panvk_device *device, const VkAllocationCallbacks *pAllocator)

DEVICE_PER_ARCH_FUNCS(6);
DEVICE_PER_ARCH_FUNCS(7);

VKAPI_ATTR VkResult VKAPI_CALL
panvk_CreateDevice(VkPhysicalDevice physicalDevice,
                   const VkDeviceCreateInfo *pCreateInfo,
                   const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
   VK_FROM_HANDLE(panvk_physical_device, physical_device, physicalDevice);
   unsigned arch = pan_arch(physical_device->kmod.props.gpu_prod_id);
   VkResult result = VK_ERROR_INITIALIZATION_FAILED;

   panvk_arch_dispatch_ret(arch, create_device, result, physical_device,
                           pCreateInfo, pAllocator, pDevice);

   return result;
}

VKAPI_ATTR void VKAPI_CALL
panvk_DestroyDevice(VkDevice _device, const VkAllocationCallbacks *pAllocator)
{
   VK_FROM_HANDLE(panvk_device, device, _device);
   struct panvk_physical_device *physical_device =
      to_panvk_physical_device(device->vk.physical);
   unsigned arch = pan_arch(physical_device->kmod.props.gpu_prod_id);

   panvk_arch_dispatch(arch, destroy_device, device, pAllocator);
}

static void
get_format_properties(struct panvk_physical_device *physical_device,
                      VkFormat format, VkFormatProperties *out_properties)
{
   VkFormatFeatureFlags tex = 0, buffer = 0;
   enum pipe_format pfmt = vk_format_to_pipe_format(format);
   const struct panfrost_format fmt = physical_device->formats.all[pfmt];

   if (!pfmt || !fmt.hw)
      goto end;

   /* 3byte formats are not supported by the buffer <-> image copy helpers. */
   if (util_format_get_blocksize(pfmt) == 3)
      goto end;

   /* We don't support compressed formats yet: this is causing trouble when
    * doing a vkCmdCopyImage() between a compressed and a non-compressed format
    * on a tiled/AFBC resource.
    */
   if (util_format_is_compressed(pfmt))
      goto end;

   buffer |=
      VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;

   /* Reject sRGB formats (see
    * https://github.com/KhronosGroup/Vulkan-Docs/issues/2214).
    */
   if ((fmt.bind & PAN_BIND_VERTEX_BUFFER) && !util_format_is_srgb(pfmt))
      buffer |= VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT;

   if (fmt.bind & PAN_BIND_SAMPLER_VIEW) {
      tex |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
             VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
             VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
             VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT |
             VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT;

      /* Integer formats only support nearest filtering */
      if (!util_format_is_scaled(pfmt) && !util_format_is_pure_integer(pfmt))
         tex |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

      buffer |= VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT;

      tex |= VK_FORMAT_FEATURE_BLIT_SRC_BIT;
   }

   /* SNORM rendering isn't working yet, disable */
   if (fmt.bind & PAN_BIND_RENDER_TARGET && !util_format_is_snorm(pfmt)) {
      tex |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
             VK_FORMAT_FEATURE_BLIT_DST_BIT;

      tex |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
      buffer |= VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT;

      /* Can always blend via blend shaders */
      tex |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
   }

   if (fmt.bind & PAN_BIND_DEPTH_STENCIL)
      tex |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

end:
   out_properties->linearTilingFeatures = tex;
   out_properties->optimalTilingFeatures = tex;
   out_properties->bufferFeatures = buffer;
}

VKAPI_ATTR void VKAPI_CALL
panvk_GetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice,
                                        VkFormat format,
                                        VkFormatProperties *pFormatProperties)
{
   VK_FROM_HANDLE(panvk_physical_device, physical_device, physicalDevice);

   get_format_properties(physical_device, format, pFormatProperties);
}

VKAPI_ATTR void VKAPI_CALL
panvk_GetPhysicalDeviceFormatProperties2(VkPhysicalDevice physicalDevice,
                                         VkFormat format,
                                         VkFormatProperties2 *pFormatProperties)
{
   VK_FROM_HANDLE(panvk_physical_device, physical_device, physicalDevice);

   get_format_properties(physical_device, format,
                         &pFormatProperties->formatProperties);

   VkDrmFormatModifierPropertiesListEXT *list = vk_find_struct(
      pFormatProperties->pNext, DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT);
   if (list) {
      VK_OUTARRAY_MAKE_TYPED(VkDrmFormatModifierPropertiesEXT, out,
                             list->pDrmFormatModifierProperties,
                             &list->drmFormatModifierCount);

      vk_outarray_append_typed(VkDrmFormatModifierProperties2EXT, &out,
                               mod_props)
      {
         mod_props->drmFormatModifier = DRM_FORMAT_MOD_LINEAR;
         mod_props->drmFormatModifierPlaneCount = 1;
      }
   }
}

static VkResult
get_image_format_properties(struct panvk_physical_device *physical_device,
                            const VkPhysicalDeviceImageFormatInfo2 *info,
                            VkImageFormatProperties *pImageFormatProperties,
                            VkFormatFeatureFlags *p_feature_flags)
{
   VkFormatProperties format_props;
   VkFormatFeatureFlags format_feature_flags;
   VkExtent3D maxExtent;
   uint32_t maxMipLevels;
   uint32_t maxArraySize;
   VkSampleCountFlags sampleCounts = VK_SAMPLE_COUNT_1_BIT;
   enum pipe_format format = vk_format_to_pipe_format(info->format);

   get_format_properties(physical_device, info->format, &format_props);

   switch (info->tiling) {
   case VK_IMAGE_TILING_LINEAR:
      format_feature_flags = format_props.linearTilingFeatures;
      break;

   case VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT:
      /* The only difference between optimal and linear is currently whether
       * depth/stencil attachments are allowed on depth/stencil formats.
       * There's no reason to allow importing depth/stencil textures, so just
       * disallow it and then this annoying edge case goes away.
       *
       * TODO: If anyone cares, we could enable this by looking at the
       * modifier and checking if it's LINEAR or not.
       */
      if (util_format_is_depth_or_stencil(format))
         goto unsupported;

      assert(format_props.optimalTilingFeatures ==
             format_props.linearTilingFeatures);
      FALLTHROUGH;
   case VK_IMAGE_TILING_OPTIMAL:
      format_feature_flags = format_props.optimalTilingFeatures;
      break;
   default:
      unreachable("bad VkPhysicalDeviceImageFormatInfo2");
   }

   if (format_feature_flags == 0)
      goto unsupported;

   if (info->type != VK_IMAGE_TYPE_2D &&
       util_format_is_depth_or_stencil(format))
      goto unsupported;

   switch (info->type) {
   default:
      unreachable("bad vkimage type");
   case VK_IMAGE_TYPE_1D:
      maxExtent.width = 16384;
      maxExtent.height = 1;
      maxExtent.depth = 1;
      maxMipLevels = 15; /* log2(maxWidth) + 1 */
      maxArraySize = 2048;
      break;
   case VK_IMAGE_TYPE_2D:
      maxExtent.width = 16384;
      maxExtent.height = 16384;
      maxExtent.depth = 1;
      maxMipLevels = 15; /* log2(maxWidth) + 1 */
      maxArraySize = 2048;
      break;
   case VK_IMAGE_TYPE_3D:
      maxExtent.width = 2048;
      maxExtent.height = 2048;
      maxExtent.depth = 2048;
      maxMipLevels = 12; /* log2(maxWidth) + 1 */
      maxArraySize = 1;
      break;
   }

   if (info->tiling == VK_IMAGE_TILING_OPTIMAL &&
       info->type == VK_IMAGE_TYPE_2D &&
       (format_feature_flags &
        (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
         VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) &&
       !(info->flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) &&
       !(info->usage & VK_IMAGE_USAGE_STORAGE_BIT)) {
      sampleCounts |= VK_SAMPLE_COUNT_4_BIT;
   }

   if (info->usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_STORAGE_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      if (!(format_feature_flags &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
         goto unsupported;
      }
   }

   *pImageFormatProperties = (VkImageFormatProperties){
      .maxExtent = maxExtent,
      .maxMipLevels = maxMipLevels,
      .maxArrayLayers = maxArraySize,
      .sampleCounts = sampleCounts,

      /* FINISHME: Accurately calculate
       * VkImageFormatProperties::maxResourceSize.
       */
      .maxResourceSize = UINT32_MAX,
   };

   if (p_feature_flags)
      *p_feature_flags = format_feature_flags;

   return VK_SUCCESS;
unsupported:
   *pImageFormatProperties = (VkImageFormatProperties){
      .maxExtent = {0, 0, 0},
      .maxMipLevels = 0,
      .maxArrayLayers = 0,
      .sampleCounts = 0,
      .maxResourceSize = 0,
   };

   return VK_ERROR_FORMAT_NOT_SUPPORTED;
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_GetPhysicalDeviceImageFormatProperties(
   VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type,
   VkImageTiling tiling, VkImageUsageFlags usage,
   VkImageCreateFlags createFlags,
   VkImageFormatProperties *pImageFormatProperties)
{
   VK_FROM_HANDLE(panvk_physical_device, physical_device, physicalDevice);

   const VkPhysicalDeviceImageFormatInfo2 info = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
      .pNext = NULL,
      .format = format,
      .type = type,
      .tiling = tiling,
      .usage = usage,
      .flags = createFlags,
   };

   return get_image_format_properties(physical_device, &info,
                                      pImageFormatProperties, NULL);
}

static VkResult
panvk_get_external_image_format_properties(
   const struct panvk_physical_device *physical_device,
   const VkPhysicalDeviceImageFormatInfo2 *pImageFormatInfo,
   VkExternalMemoryHandleTypeFlagBits handleType,
   VkExternalMemoryProperties *external_properties)
{
   VkExternalMemoryFeatureFlagBits flags = 0;
   VkExternalMemoryHandleTypeFlags export_flags = 0;
   VkExternalMemoryHandleTypeFlags compat_flags = 0;

   /* From the Vulkan 1.1.98 spec:
    *
    *    If handleType is not compatible with the format, type, tiling,
    *    usage, and flags specified in VkPhysicalDeviceImageFormatInfo2,
    *    then vkGetPhysicalDeviceImageFormatProperties2 returns
    *    VK_ERROR_FORMAT_NOT_SUPPORTED.
    */
   switch (handleType) {
   case VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT:
   case VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT:
      switch (pImageFormatInfo->type) {
      case VK_IMAGE_TYPE_2D:
         flags = VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT |
                 VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT |
                 VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
         compat_flags = export_flags =
            VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT |
            VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
         break;
      default:
         return vk_errorf(
            physical_device, VK_ERROR_FORMAT_NOT_SUPPORTED,
            "VkExternalMemoryTypeFlagBits(0x%x) unsupported for VkImageType(%d)",
            handleType, pImageFormatInfo->type);
      }
      break;
   case VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT:
      flags = VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
      compat_flags = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
      break;
   default:
      return vk_errorf(physical_device, VK_ERROR_FORMAT_NOT_SUPPORTED,
                       "VkExternalMemoryTypeFlagBits(0x%x) unsupported",
                       handleType);
   }

   *external_properties = (VkExternalMemoryProperties){
      .externalMemoryFeatures = flags,
      .exportFromImportedHandleTypes = export_flags,
      .compatibleHandleTypes = compat_flags,
   };

   return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
panvk_GetPhysicalDeviceImageFormatProperties2(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceImageFormatInfo2 *base_info,
   VkImageFormatProperties2 *base_props)
{
   VK_FROM_HANDLE(panvk_physical_device, physical_device, physicalDevice);
   const VkPhysicalDeviceExternalImageFormatInfo *external_info = NULL;
   const VkPhysicalDeviceImageViewImageFormatInfoEXT *image_view_info = NULL;
   VkExternalImageFormatProperties *external_props = NULL;
   VkFilterCubicImageViewImageFormatPropertiesEXT *cubic_props = NULL;
   VkFormatFeatureFlags format_feature_flags;
   VkSamplerYcbcrConversionImageFormatProperties *ycbcr_props = NULL;
   VkResult result;

   result = get_image_format_properties(physical_device, base_info,
                                        &base_props->imageFormatProperties,
                                        &format_feature_flags);
   if (result != VK_SUCCESS)
      return result;

   /* Extract input structs */
   vk_foreach_struct_const(s, base_info->pNext) {
      switch (s->sType) {
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO:
         external_info = (const void *)s;
         break;
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_IMAGE_FORMAT_INFO_EXT:
         image_view_info = (const void *)s;
         break;
      default:
         break;
      }
   }

   /* Extract output structs */
   vk_foreach_struct(s, base_props->pNext) {
      switch (s->sType) {
      case VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES:
         external_props = (void *)s;
         break;
      case VK_STRUCTURE_TYPE_FILTER_CUBIC_IMAGE_VIEW_IMAGE_FORMAT_PROPERTIES_EXT:
         cubic_props = (void *)s;
         break;
      case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES:
         ycbcr_props = (void *)s;
         break;
      default:
         break;
      }
   }

   /* From the Vulkan 1.0.42 spec:
    *
    *    If handleType is 0, vkGetPhysicalDeviceImageFormatProperties2 will
    *    behave as if VkPhysicalDeviceExternalImageFormatInfo was not
    *    present and VkExternalImageFormatProperties will be ignored.
    */
   if (external_info && external_info->handleType != 0) {
      VkExternalImageFormatProperties fallback_external_props;

      if (!external_props) {
         memset(&fallback_external_props, 0, sizeof(fallback_external_props));
         external_props = &fallback_external_props;
      }

      result = panvk_get_external_image_format_properties(
         physical_device, base_info, external_info->handleType,
         &external_props->externalMemoryProperties);
      if (result != VK_SUCCESS)
         goto fail;
   }

   if (cubic_props) {
      /* note: blob only allows cubic filtering for 2D and 2D array views
       * its likely we can enable it for 1D and CUBE, needs testing however
       */
      if ((image_view_info->imageViewType == VK_IMAGE_VIEW_TYPE_2D ||
           image_view_info->imageViewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY) &&
          (format_feature_flags &
           VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT)) {
         cubic_props->filterCubic = true;
         cubic_props->filterCubicMinmax = true;
      } else {
         cubic_props->filterCubic = false;
         cubic_props->filterCubicMinmax = false;
      }
   }

   if (ycbcr_props)
      ycbcr_props->combinedImageSamplerDescriptorCount = 1;

   return VK_SUCCESS;

fail:
   if (result == VK_ERROR_FORMAT_NOT_SUPPORTED) {
      /* From the Vulkan 1.0.42 spec:
       *
       *    If the combination of parameters to
       *    vkGetPhysicalDeviceImageFormatProperties2 is not supported by
       *    the implementation for use in vkCreateImage, then all members of
       *    imageFormatProperties will be filled with zero.
       */
      base_props->imageFormatProperties = (VkImageFormatProperties){};
   }

   return result;
}

VKAPI_ATTR void VKAPI_CALL
panvk_GetPhysicalDeviceSparseImageFormatProperties(
   VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type,
   VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling,
   uint32_t *pNumProperties, VkSparseImageFormatProperties *pProperties)
{
   /* Sparse images are not yet supported. */
   *pNumProperties = 0;
}

VKAPI_ATTR void VKAPI_CALL
panvk_GetPhysicalDeviceSparseImageFormatProperties2(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceSparseImageFormatInfo2 *pFormatInfo,
   uint32_t *pPropertyCount, VkSparseImageFormatProperties2 *pProperties)
{
   /* Sparse images are not yet supported. */
   *pPropertyCount = 0;
}

VKAPI_ATTR void VKAPI_CALL
panvk_GetPhysicalDeviceExternalBufferProperties(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceExternalBufferInfo *pExternalBufferInfo,
   VkExternalBufferProperties *pExternalBufferProperties)
{
   panvk_stub();
}
