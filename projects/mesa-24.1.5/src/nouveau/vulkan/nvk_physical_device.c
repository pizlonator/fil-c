/*
 * Copyright © 2022 Collabora Ltd. and Red Hat Inc.
 * SPDX-License-Identifier: MIT
 */
#include "nvk_physical_device.h"

#include "nak.h"
#include "nvk_buffer.h"
#include "nvk_entrypoints.h"
#include "nvk_format.h"
#include "nvk_image.h"
#include "nvk_instance.h"
#include "nvk_shader.h"
#include "nvk_wsi.h"
#include "git_sha1.h"
#include "util/disk_cache.h"
#include "util/mesa-sha1.h"

#include "vk_device.h"
#include "vk_drm_syncobj.h"
#include "vk_shader_module.h"
#include "vulkan/wsi/wsi_common.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <xf86drm.h>

#include "cl90c0.h"
#include "cl91c0.h"
#include "cla097.h"
#include "cla0c0.h"
#include "cla1c0.h"
#include "clb097.h"
#include "clb0c0.h"
#include "clb197.h"
#include "clb1c0.h"
#include "clc097.h"
#include "clc0c0.h"
#include "clc1c0.h"
#include "clc397.h"
#include "clc3c0.h"
#include "clc597.h"
#include "clc5c0.h"
#include "clc997.h"

static bool
nvk_use_nak(const struct nv_device_info *info)
{
   const VkShaderStageFlags vk10_stages =
      VK_SHADER_STAGE_VERTEX_BIT |
      VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
      VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
      VK_SHADER_STAGE_GEOMETRY_BIT |
      VK_SHADER_STAGE_FRAGMENT_BIT |
      VK_SHADER_STAGE_COMPUTE_BIT;

   return !(vk10_stages & ~nvk_nak_stages(info));
}

static uint32_t
nvk_get_vk_version(const struct nv_device_info *info)
{
   /* Version override takes priority */
   const uint32_t version_override = vk_get_version_override();
   if (version_override)
      return version_override;

   /* If we're using codegen for anything, lock to version 1.0 */
   if (!nvk_use_nak(info))
      return VK_MAKE_VERSION(1, 0, VK_HEADER_VERSION);

   return VK_MAKE_VERSION(1, 3, VK_HEADER_VERSION);
}

static void
nvk_get_device_extensions(const struct nvk_instance *instance,
                          const struct nv_device_info *info,
                          bool has_tiled_bos,
                          struct vk_device_extension_table *ext)
{
   *ext = (struct vk_device_extension_table) {
      .KHR_8bit_storage = true,
      .KHR_16bit_storage = true,
      .KHR_bind_memory2 = true,
      .KHR_buffer_device_address = true,
      .KHR_calibrated_timestamps = true,
      .KHR_copy_commands2 = true,
      .KHR_create_renderpass2 = true,
      .KHR_dedicated_allocation = true,
      .KHR_depth_stencil_resolve = true,
      .KHR_descriptor_update_template = true,
      .KHR_device_group = true,
      .KHR_draw_indirect_count = info->cls_eng3d >= TURING_A,
      .KHR_driver_properties = true,
      .KHR_dynamic_rendering = true,
      .KHR_external_fence = true,
      .KHR_external_fence_fd = true,
      .KHR_external_memory = true,
      .KHR_external_memory_fd = true,
      .KHR_external_semaphore = true,
      .KHR_external_semaphore_fd = true,
      .KHR_format_feature_flags2 = true,
      .KHR_fragment_shader_barycentric = info->cls_eng3d >= TURING_A &&
         (nvk_nak_stages(info) & VK_SHADER_STAGE_FRAGMENT_BIT) != 0,
      .KHR_get_memory_requirements2 = true,
      .KHR_image_format_list = true,
      .KHR_imageless_framebuffer = true,
#ifdef NVK_USE_WSI_PLATFORM
      .KHR_incremental_present = true,
#endif
      .KHR_index_type_uint8 = true,
      .KHR_line_rasterization = true,
      .KHR_load_store_op_none = true,
      .KHR_maintenance1 = true,
      .KHR_maintenance2 = true,
      .KHR_maintenance3 = true,
      .KHR_maintenance4 = true,
      .KHR_maintenance5 = true,
      .KHR_maintenance6 = true,
      .KHR_map_memory2 = true,
      .KHR_multiview = true,
      .KHR_pipeline_executable_properties = true,
      .KHR_pipeline_library = true,
#ifdef NVK_USE_WSI_PLATFORM
      /* Hide these behind dri configs for now since we cannot implement it
       * reliably on all surfaces yet. There is no surface capability query
       * for present wait/id, but the feature is useful enough to hide behind
       * an opt-in mechanism for now.  If the instance only enables surface
       * extensions that unconditionally support present wait, we can also
       * expose the extension that way.
       */
      .KHR_present_id = driQueryOptionb(&instance->dri_options, "vk_khr_present_wait") ||
                        wsi_common_vk_instance_supports_present_wait(&instance->vk),
      .KHR_present_wait = driQueryOptionb(&instance->dri_options, "vk_khr_present_wait") ||
                          wsi_common_vk_instance_supports_present_wait(&instance->vk),
#endif
      .KHR_push_descriptor = true,
      .KHR_relaxed_block_layout = true,
      .KHR_sampler_mirror_clamp_to_edge = true,
      .KHR_sampler_ycbcr_conversion = true,
      .KHR_separate_depth_stencil_layouts = true,
      .KHR_shader_atomic_int64 = info->cls_eng3d >= MAXWELL_A &&
                                 nvk_use_nak(info),
      .KHR_shader_clock = true,
      .KHR_shader_draw_parameters = true,
      .KHR_shader_expect_assume = true,
      .KHR_shader_float_controls = true,
      .KHR_shader_float16_int8 = true,
      .KHR_shader_integer_dot_product = true,
      .KHR_shader_maximal_reconvergence = true,
      .KHR_shader_non_semantic_info = true,
      .KHR_shader_subgroup_extended_types = true,
      .KHR_shader_subgroup_rotate = nvk_use_nak(info),
      .KHR_shader_subgroup_uniform_control_flow = nvk_use_nak(info),
      .KHR_shader_terminate_invocation =
         (nvk_nak_stages(info) & VK_SHADER_STAGE_FRAGMENT_BIT) != 0,
      .KHR_spirv_1_4 = true,
      .KHR_storage_buffer_storage_class = true,
      .KHR_timeline_semaphore = true,
#ifdef NVK_USE_WSI_PLATFORM
      .KHR_swapchain = true,
      .KHR_swapchain_mutable_format = true,
#endif
      .KHR_synchronization2 = true,
      .KHR_uniform_buffer_standard_layout = true,
      .KHR_variable_pointers = true,
      .KHR_vertex_attribute_divisor = true,
      .KHR_vulkan_memory_model = nvk_use_nak(info),
      .KHR_workgroup_memory_explicit_layout = true,
      .KHR_zero_initialize_workgroup_memory = true,
      .EXT_4444_formats = true,
      .EXT_attachment_feedback_loop_layout = true,
      .EXT_border_color_swizzle = true,
      .EXT_buffer_device_address = true,
      .EXT_calibrated_timestamps = true,
      .EXT_conditional_rendering = true,
      .EXT_color_write_enable = true,
      .EXT_custom_border_color = true,
      .EXT_depth_bias_control = true,
      .EXT_depth_clip_control = true,
      .EXT_depth_clip_enable = true,
      .EXT_descriptor_indexing = true,
#ifdef VK_USE_PLATFORM_DISPLAY_KHR
      .EXT_display_control = true,
#endif
      .EXT_image_drm_format_modifier = has_tiled_bos,
      .EXT_dynamic_rendering_unused_attachments = true,
      .EXT_extended_dynamic_state = true,
      .EXT_extended_dynamic_state2 = true,
      .EXT_extended_dynamic_state3 = true,
      .EXT_external_memory_dma_buf = true,
      .EXT_graphics_pipeline_library = true,
      .EXT_host_query_reset = true,
      .EXT_image_2d_view_of_3d = true,
      .EXT_image_robustness = true,
      .EXT_image_sliced_view_of_3d = true,
      .EXT_image_view_min_lod = true,
      .EXT_index_type_uint8 = true,
      .EXT_inline_uniform_block = true,
      .EXT_line_rasterization = true,
      .EXT_load_store_op_none = true,
      .EXT_map_memory_placed = true,
      .EXT_memory_budget = true,
      .EXT_multi_draw = true,
      .EXT_mutable_descriptor_type = true,
      .EXT_nested_command_buffer = true,
      .EXT_non_seamless_cube_map = true,
      .EXT_pci_bus_info = info->type == NV_DEVICE_TYPE_DIS,
      .EXT_pipeline_creation_cache_control = true,
      .EXT_pipeline_creation_feedback = true,
      .EXT_physical_device_drm = true,
      .EXT_primitive_topology_list_restart = true,
      .EXT_private_data = true,
      .EXT_primitives_generated_query = true,
      .EXT_provoking_vertex = true,
      .EXT_queue_family_foreign = true,
      .EXT_robustness2 = true,
      .EXT_sample_locations = info->cls_eng3d >= MAXWELL_B,
      .EXT_sampler_filter_minmax = info->cls_eng3d >= MAXWELL_B,
      .EXT_scalar_block_layout = nvk_use_nak(info),
      .EXT_separate_stencil_usage = true,
      .EXT_shader_image_atomic_int64 = info->cls_eng3d >= MAXWELL_A &&
                                       nvk_use_nak(info),
      .EXT_shader_demote_to_helper_invocation = true,
      .EXT_shader_module_identifier = true,
      .EXT_shader_object = true,
      .EXT_shader_subgroup_ballot = true,
      .EXT_shader_subgroup_vote = true,
      .EXT_shader_viewport_index_layer = info->cls_eng3d >= MAXWELL_B,
      .EXT_subgroup_size_control = true,
#ifdef NVK_USE_WSI_PLATFORM
      .EXT_swapchain_maintenance1 = true,
#endif
      .EXT_texel_buffer_alignment = true,
      .EXT_tooling_info = true,
      .EXT_transform_feedback = true,
      .EXT_vertex_attribute_divisor = true,
      .EXT_vertex_input_dynamic_state = true,
      .EXT_ycbcr_2plane_444_formats = true,
      .EXT_ycbcr_image_arrays = true,
      .GOOGLE_decorate_string = true,
      .GOOGLE_hlsl_functionality1 = true,
      .GOOGLE_user_type = true,
      .NV_shader_sm_builtins = true,
      .VALVE_mutable_descriptor_type = true,
   };
}

static void
nvk_get_device_features(const struct nv_device_info *info,
                        const struct vk_device_extension_table *supported_extensions,
                        struct vk_features *features)
{
   *features = (struct vk_features) {
      /* Vulkan 1.0 */
      .robustBufferAccess = true,
      .fullDrawIndexUint32 = true,
      .imageCubeArray = true,
      .independentBlend = true,
      .geometryShader = true,
      .tessellationShader = true,
      .sampleRateShading = true,
      .dualSrcBlend = true,
      .logicOp = true,
      .multiDrawIndirect = true,
      .drawIndirectFirstInstance = true,
      .depthClamp = true,
      .depthBiasClamp = true,
      .fillModeNonSolid = true,
      .depthBounds = true,
      .wideLines = true,
      .largePoints = true,
      .alphaToOne = true,
      .multiViewport = true,
      .samplerAnisotropy = true,
      .textureCompressionETC2 = false,
      .textureCompressionBC = true,
      .textureCompressionASTC_LDR = false,
      .occlusionQueryPrecise = true,
      .pipelineStatisticsQuery = true,
      .vertexPipelineStoresAndAtomics = true,
      .fragmentStoresAndAtomics = true,
      .shaderTessellationAndGeometryPointSize = true,
      .shaderImageGatherExtended = true,
      .shaderStorageImageExtendedFormats = true,
      .shaderStorageImageMultisample = true,
      .shaderStorageImageReadWithoutFormat = info->cls_eng3d >= MAXWELL_A,
      .shaderStorageImageWriteWithoutFormat = true,
      .shaderUniformBufferArrayDynamicIndexing = true,
      .shaderSampledImageArrayDynamicIndexing = true,
      .shaderStorageBufferArrayDynamicIndexing = true,
      .shaderStorageImageArrayDynamicIndexing = true,
      .shaderClipDistance = true,
      .shaderCullDistance = true,
      .shaderFloat64 = true,
      .shaderInt64 = true,
      .shaderInt16 = true,
      .shaderResourceResidency = info->cls_eng3d >= VOLTA_A,
      .shaderResourceMinLod = info->cls_eng3d >= VOLTA_A,
      .sparseBinding = true,
      .sparseResidency2Samples = info->cls_eng3d >= MAXWELL_B,
      .sparseResidency4Samples = info->cls_eng3d >= MAXWELL_B,
      .sparseResidency8Samples = info->cls_eng3d >= MAXWELL_B,
      .sparseResidencyAliased = info->cls_eng3d >= MAXWELL_B,
      .sparseResidencyBuffer = info->cls_eng3d >= MAXWELL_B,
      .sparseResidencyImage2D = info->cls_eng3d >= MAXWELL_B,
      .sparseResidencyImage3D = info->cls_eng3d >= MAXWELL_B,
      .variableMultisampleRate = true,
      .inheritedQueries = true,

      /* Vulkan 1.1 */
      .storageBuffer16BitAccess = true,
      .uniformAndStorageBuffer16BitAccess = true,
      .storagePushConstant16 = true,
      .multiview = true,
      .multiviewGeometryShader = true,
      .multiviewTessellationShader = true,
      .variablePointersStorageBuffer = true,
      .variablePointers = true,
      .shaderDrawParameters = true,
      .samplerYcbcrConversion = true,

      /* Vulkan 1.2 */
      .samplerMirrorClampToEdge = true,
      .drawIndirectCount = info->cls_eng3d >= TURING_A,
      .storageBuffer8BitAccess = true,
      .uniformAndStorageBuffer8BitAccess = true,
      .storagePushConstant8 = true,
      .shaderBufferInt64Atomics = info->cls_eng3d >= MAXWELL_A &&
                                  nvk_use_nak(info),
      .shaderSharedInt64Atomics = false, /* TODO */
      /* TODO: Fp16 is currently busted on Turing and Volta due to instruction
       * scheduling issues.  Re-enable it once those are sorted.
       */
      .shaderFloat16 = info->sm >= 80 && nvk_use_nak(info),
      .shaderInt8 = true,
      .descriptorIndexing = true,
      .shaderInputAttachmentArrayDynamicIndexing = true,
      .shaderUniformTexelBufferArrayDynamicIndexing = true,
      .shaderStorageTexelBufferArrayDynamicIndexing = true,
      .shaderUniformBufferArrayNonUniformIndexing = true,
      .shaderSampledImageArrayNonUniformIndexing = true,
      .shaderStorageBufferArrayNonUniformIndexing = true,
      .shaderStorageImageArrayNonUniformIndexing = true,
      .shaderInputAttachmentArrayNonUniformIndexing = true,
      .shaderUniformTexelBufferArrayNonUniformIndexing = true,
      .shaderStorageTexelBufferArrayNonUniformIndexing = true,
      .descriptorBindingUniformBufferUpdateAfterBind = true,
      .descriptorBindingSampledImageUpdateAfterBind = true,
      .descriptorBindingStorageImageUpdateAfterBind = true,
      .descriptorBindingStorageBufferUpdateAfterBind = true,
      .descriptorBindingUniformTexelBufferUpdateAfterBind = true,
      .descriptorBindingStorageTexelBufferUpdateAfterBind = true,
      .descriptorBindingUpdateUnusedWhilePending = true,
      .descriptorBindingPartiallyBound = true,
      .descriptorBindingVariableDescriptorCount = true,
      .runtimeDescriptorArray = true,
      .samplerFilterMinmax = info->cls_eng3d >= MAXWELL_B,
      .scalarBlockLayout = nvk_use_nak(info),
      .imagelessFramebuffer = true,
      .uniformBufferStandardLayout = true,
      .shaderSubgroupExtendedTypes = true,
      .separateDepthStencilLayouts = true,
      .hostQueryReset = true,
      .timelineSemaphore = true,
      .bufferDeviceAddress = true,
      .bufferDeviceAddressCaptureReplay = true,
      .bufferDeviceAddressMultiDevice = false,
      .vulkanMemoryModel = nvk_use_nak(info),
      .vulkanMemoryModelDeviceScope = nvk_use_nak(info),
      .vulkanMemoryModelAvailabilityVisibilityChains = nvk_use_nak(info),
      .shaderOutputViewportIndex = info->cls_eng3d >= MAXWELL_B,
      .shaderOutputLayer = info->cls_eng3d >= MAXWELL_B,
      .subgroupBroadcastDynamicId = nvk_use_nak(info),

      /* Vulkan 1.3 */
      .robustImageAccess = true,
      .inlineUniformBlock = true,
      .descriptorBindingInlineUniformBlockUpdateAfterBind = true,
      .pipelineCreationCacheControl = true,
      .privateData = true,
      .shaderDemoteToHelperInvocation = true,
      .shaderTerminateInvocation = true,
      .subgroupSizeControl = true,
      .computeFullSubgroups = true,
      .synchronization2 = true,
      .shaderZeroInitializeWorkgroupMemory = true,
      .dynamicRendering = true,
      .shaderIntegerDotProduct = true,
      .maintenance4 = true,

      /* VK_KHR_fragment_shader_barycentric */
      .fragmentShaderBarycentric = info->cls_eng3d >= TURING_A &&
         (nvk_nak_stages(info) & VK_SHADER_STAGE_FRAGMENT_BIT) != 0,

      /* VK_KHR_index_type_uint8 */
      .indexTypeUint8 = true,

      /* VK_KHR_line_rasterization */
      .rectangularLines = true,
      .bresenhamLines = true,
      .smoothLines = true,
      .stippledRectangularLines = true,
      .stippledBresenhamLines = true,
      .stippledSmoothLines = true,

      /* VK_KHR_maintenance5 */
      .maintenance5 = true,

      /* VK_KHR_maintenance6 */
      .maintenance6 = true,

      /* VK_KHR_pipeline_executable_properties */
      .pipelineExecutableInfo = true,

      /* VK_KHR_present_id */
      .presentId = supported_extensions->KHR_present_id,

      /* VK_KHR_present_wait */
      .presentWait = supported_extensions->KHR_present_wait,

      /* VK_KHR_shader_clock */
      .shaderSubgroupClock = true,
      .shaderDeviceClock = true,

      /* VK_KHR_shader_expect_assume */
      .shaderExpectAssume = true,

      /* VK_KHR_shader_maximal_reconvergence */
      .shaderMaximalReconvergence = true,

      /* VK_KHR_shader_subgroup_rotate */
      .shaderSubgroupRotate = nvk_use_nak(info),
      .shaderSubgroupRotateClustered = nvk_use_nak(info),

      /* VK_KHR_vertex_attribute_divisor */
      .vertexAttributeInstanceRateDivisor = true,
      .vertexAttributeInstanceRateZeroDivisor = true,

      /* VK_KHR_workgroup_memory_explicit_layout */
      .workgroupMemoryExplicitLayout = true,
      .workgroupMemoryExplicitLayoutScalarBlockLayout = true,
      .workgroupMemoryExplicitLayout8BitAccess = false,
      .workgroupMemoryExplicitLayout16BitAccess = false,

      /* VK_EXT_4444_formats */
      .formatA4R4G4B4 = true,
      .formatA4B4G4R4 = true,

      /* VK_EXT_attachment_feedback_loop_layout */
      .attachmentFeedbackLoopLayout = true,

      /* VK_EXT_border_color_swizzle */
      .borderColorSwizzle = true,
      .borderColorSwizzleFromImage = false,

      /* VK_EXT_buffer_device_address */
      .bufferDeviceAddressCaptureReplayEXT = true,

      /* VK_EXT_color_write_enable */
      .colorWriteEnable = true,

      /* VK_EXT_conditional_rendering */
      .conditionalRendering = true,
      .inheritedConditionalRendering = true,

      /* VK_EXT_custom_border_color */
      .customBorderColors = true,
      .customBorderColorWithoutFormat = true,

      /* VK_EXT_depth_bias_control */
      .depthBiasControl = true,
      .leastRepresentableValueForceUnormRepresentation = true,
      .floatRepresentation = false,
      .depthBiasExact = true,

      /* VK_EXT_depth_clip_control */
      .depthClipControl = info->cls_eng3d >= VOLTA_A,

      /* VK_EXT_depth_clip_enable */
      .depthClipEnable = true,

      /* VK_EXT_dynamic_rendering_unused_attachments */
      .dynamicRenderingUnusedAttachments = true,

      /* VK_EXT_extended_dynamic_state */
      .extendedDynamicState = true,

      /* VK_EXT_extended_dynamic_state2 */
      .extendedDynamicState2 = true,
      .extendedDynamicState2LogicOp = true,
      .extendedDynamicState2PatchControlPoints = true,

      /* VK_EXT_extended_dynamic_state3 */
      .extendedDynamicState3TessellationDomainOrigin = true,
      .extendedDynamicState3DepthClampEnable = true,
      .extendedDynamicState3PolygonMode = true,
      .extendedDynamicState3RasterizationSamples = true,
      .extendedDynamicState3SampleMask = true,
      .extendedDynamicState3AlphaToCoverageEnable = true,
      .extendedDynamicState3AlphaToOneEnable = true,
      .extendedDynamicState3LogicOpEnable = true,
      .extendedDynamicState3ColorBlendEnable = true,
      .extendedDynamicState3ColorBlendEquation = true,
      .extendedDynamicState3ColorWriteMask = true,
      .extendedDynamicState3RasterizationStream = true,
      .extendedDynamicState3ConservativeRasterizationMode = false,
      .extendedDynamicState3ExtraPrimitiveOverestimationSize = false,
      .extendedDynamicState3DepthClipEnable = true,
      .extendedDynamicState3SampleLocationsEnable = info->cls_eng3d >= MAXWELL_B,
      .extendedDynamicState3ColorBlendAdvanced = false,
      .extendedDynamicState3ProvokingVertexMode = true,
      .extendedDynamicState3LineRasterizationMode = true,
      .extendedDynamicState3LineStippleEnable = true,
      .extendedDynamicState3DepthClipNegativeOneToOne = true,
      .extendedDynamicState3ViewportWScalingEnable = false,
      .extendedDynamicState3ViewportSwizzle = false,
      .extendedDynamicState3CoverageToColorEnable = false,
      .extendedDynamicState3CoverageToColorLocation = false,
      .extendedDynamicState3CoverageModulationMode = false,
      .extendedDynamicState3CoverageModulationTableEnable = false,
      .extendedDynamicState3CoverageModulationTable = false,
      .extendedDynamicState3CoverageReductionMode = false,
      .extendedDynamicState3RepresentativeFragmentTestEnable = false,
      .extendedDynamicState3ShadingRateImageEnable = false,

      /* VK_EXT_graphics_pipeline_library */
      .graphicsPipelineLibrary = true,

      /* VK_EXT_image_2d_view_of_3d */
      .image2DViewOf3D = true,
      .sampler2DViewOf3D = true,

      /* VK_EXT_image_sliced_view_of_3d */
      .imageSlicedViewOf3D = true,

#ifdef NVK_USE_WSI_PLATFORM
      /* VK_EXT_swapchain_maintenance1 */
      .swapchainMaintenance1 = true,
#endif

      /* VK_EXT_image_view_min_lod */
      .minLod = true,

      /* VK_EXT_map_memory_placed */
      .memoryMapPlaced = true,
      .memoryMapRangePlaced = false,
      .memoryUnmapReserve = true,

      /* VK_EXT_multi_draw */
      .multiDraw = true,

      /* VK_EXT_mutable_descriptor_type */
      .mutableDescriptorType = true,

      /* VK_EXT_nested_command_buffer */
      .nestedCommandBuffer = true,
      .nestedCommandBufferRendering = true,
      .nestedCommandBufferSimultaneousUse = true,

      /* VK_EXT_non_seamless_cube_map */
      .nonSeamlessCubeMap = true,

      /* VK_EXT_primitive_topology_list_restart */
      .primitiveTopologyListRestart = true,
      .primitiveTopologyPatchListRestart = true,

      /* VK_EXT_primitives_generated_query */
      .primitivesGeneratedQuery = true,
      .primitivesGeneratedQueryWithNonZeroStreams = true,
      .primitivesGeneratedQueryWithRasterizerDiscard = true,

      /* VK_EXT_provoking_vertex */
      .provokingVertexLast = true,
      .transformFeedbackPreservesProvokingVertex = true,

      /* VK_EXT_robustness2 */
      .robustBufferAccess2 = true,
      .robustImageAccess2 = true,
      .nullDescriptor = true,

      /* VK_EXT_shader_image_atomic_int64 */
      .shaderImageInt64Atomics = info->cls_eng3d >= MAXWELL_A &&
                                 nvk_use_nak(info),
      .sparseImageInt64Atomics = info->cls_eng3d >= MAXWELL_A &&
                                 nvk_use_nak(info),

      /* VK_EXT_shader_module_identifier */
      .shaderModuleIdentifier = true,

      /* VK_EXT_shader_object */
      .shaderObject = true,

      /* VK_KHR_shader_subgroup_uniform_control_flow */
      .shaderSubgroupUniformControlFlow = nvk_use_nak(info),

      /* VK_EXT_texel_buffer_alignment */
      .texelBufferAlignment = true,

      /* VK_EXT_transform_feedback */
      .transformFeedback = true,
      .geometryStreams = true,

      /* VK_EXT_vertex_input_dynamic_state */
      .vertexInputDynamicState = true,

      /* VK_EXT_ycbcr_2plane_444_formats */
      .ycbcr2plane444Formats = true,

      /* VK_EXT_ycbcr_image_arrays */
      .ycbcrImageArrays = true,

      /* VK_NV_shader_sm_builtins */
      .shaderSMBuiltins = true,
   };
}

uint32_t
nvk_min_cbuf_alignment(const struct nv_device_info *info)
{
   return info->cls_eng3d >= TURING_A ? 64 : 256;
}

static void
nvk_get_device_properties(const struct nvk_instance *instance,
                          const struct nv_device_info *info,
                          bool conformant,
                          struct vk_properties *properties)
{
   const VkSampleCountFlagBits sample_counts = VK_SAMPLE_COUNT_1_BIT |
                                               VK_SAMPLE_COUNT_2_BIT |
                                               VK_SAMPLE_COUNT_4_BIT |
                                               VK_SAMPLE_COUNT_8_BIT;

   uint64_t os_page_size = 4096;
   os_get_page_size(&os_page_size);

   *properties = (struct vk_properties) {
      .apiVersion = nvk_get_vk_version(info),
      .driverVersion = vk_get_driver_version(),
      .vendorID = instance->force_vk_vendor != 0 ?
                  instance->force_vk_vendor : NVIDIA_VENDOR_ID,
      .deviceID = info->device_id,
      .deviceType = info->type == NV_DEVICE_TYPE_DIS ?
                    VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU :
                    VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,

      /* Vulkan 1.0 limits */
      .maxImageDimension1D = nvk_image_max_dimension(info, VK_IMAGE_TYPE_1D),
      .maxImageDimension2D = nvk_image_max_dimension(info, VK_IMAGE_TYPE_2D),
      .maxImageDimension3D = nvk_image_max_dimension(info, VK_IMAGE_TYPE_3D),
      .maxImageDimensionCube = 0x8000,
      .maxImageArrayLayers = 2048,
      .maxTexelBufferElements = 128 * 1024 * 1024,
      .maxUniformBufferRange = 65536,
      .maxStorageBufferRange = UINT32_MAX,
      .maxPushConstantsSize = NVK_MAX_PUSH_SIZE,
      .maxMemoryAllocationCount = 4096,
      .maxSamplerAllocationCount = 4000,
      .bufferImageGranularity = info->cls_eng3d >= MAXWELL_B ? 0x400 : 0x10000,
      .sparseAddressSpaceSize = NVK_SPARSE_ADDR_SPACE_SIZE,
      .maxBoundDescriptorSets = NVK_MAX_SETS,
      .maxPerStageDescriptorSamplers = NVK_MAX_DESCRIPTORS,
      .maxPerStageDescriptorUniformBuffers = NVK_MAX_DESCRIPTORS,
      .maxPerStageDescriptorStorageBuffers = NVK_MAX_DESCRIPTORS,
      .maxPerStageDescriptorSampledImages = NVK_MAX_DESCRIPTORS,
      .maxPerStageDescriptorStorageImages = NVK_MAX_DESCRIPTORS,
      .maxPerStageDescriptorInputAttachments = NVK_MAX_DESCRIPTORS,
      .maxPerStageResources = UINT32_MAX,
      .maxDescriptorSetSamplers = NVK_MAX_DESCRIPTORS,
      .maxDescriptorSetUniformBuffers = NVK_MAX_DESCRIPTORS,
      .maxDescriptorSetUniformBuffersDynamic = NVK_MAX_DYNAMIC_BUFFERS / 2,
      .maxDescriptorSetStorageBuffers = NVK_MAX_DESCRIPTORS,
      .maxDescriptorSetStorageBuffersDynamic = NVK_MAX_DYNAMIC_BUFFERS / 2,
      .maxDescriptorSetSampledImages = NVK_MAX_DESCRIPTORS,
      .maxDescriptorSetStorageImages = NVK_MAX_DESCRIPTORS,
      .maxDescriptorSetInputAttachments = NVK_MAX_DESCRIPTORS,
      .maxVertexInputAttributes = 32,
      .maxVertexInputBindings = 32,
      .maxVertexInputAttributeOffset = 2047,
      .maxVertexInputBindingStride = 2048,
      .maxVertexOutputComponents = 128,
      .maxTessellationGenerationLevel = 64,
      .maxTessellationPatchSize = 32,
      .maxTessellationControlPerVertexInputComponents = 128,
      .maxTessellationControlPerVertexOutputComponents = 128,
      .maxTessellationControlPerPatchOutputComponents = 120,
      .maxTessellationControlTotalOutputComponents = 4216,
      .maxTessellationEvaluationInputComponents = 128,
      .maxTessellationEvaluationOutputComponents = 128,
      .maxGeometryShaderInvocations = 32,
      .maxGeometryInputComponents = 128,
      .maxGeometryOutputComponents = 128,
      .maxGeometryOutputVertices = 1024,
      .maxGeometryTotalOutputComponents = 1024,
      .maxFragmentInputComponents = 128,
      .maxFragmentOutputAttachments = NVK_MAX_RTS,
      .maxFragmentDualSrcAttachments = 1,
      .maxFragmentCombinedOutputResources = 16,
      .maxComputeSharedMemorySize = NVK_MAX_SHARED_SIZE,
      .maxComputeWorkGroupCount = {0x7fffffff, 65535, 65535},
      .maxComputeWorkGroupInvocations = 1024,
      .maxComputeWorkGroupSize = {1024, 1024, 64},
      .subPixelPrecisionBits = 8,
      .subTexelPrecisionBits = 8,
      .mipmapPrecisionBits = 8,
      .maxDrawIndexedIndexValue = UINT32_MAX,
      .maxDrawIndirectCount = UINT32_MAX,
      .maxSamplerLodBias = 15,
      .maxSamplerAnisotropy = 16,
      .maxViewports = NVK_MAX_VIEWPORTS,
      .maxViewportDimensions = { 32768, 32768 },
      .viewportBoundsRange = { -65536, 65536 },
      .viewportSubPixelBits = 8,
      .minMemoryMapAlignment = os_page_size,
      .minTexelBufferOffsetAlignment = NVK_MIN_TEXEL_BUFFER_ALIGNMENT,
      .minUniformBufferOffsetAlignment = nvk_min_cbuf_alignment(info),
      .minStorageBufferOffsetAlignment = NVK_MIN_SSBO_ALIGNMENT,
      .minTexelOffset = -8,
      .maxTexelOffset = 7,
      .minTexelGatherOffset = -32,
      .maxTexelGatherOffset = 31,
      .minInterpolationOffset = -0.5,
      .maxInterpolationOffset = 0.4375,
      .subPixelInterpolationOffsetBits = 4,
      .maxFramebufferHeight = info->cls_eng3d >= PASCAL_A ? 0x8000 : 0x4000,
      .maxFramebufferWidth = info->cls_eng3d >= PASCAL_A ? 0x8000 : 0x4000,
      .maxFramebufferLayers = 2048,
      .framebufferColorSampleCounts = sample_counts,
      .framebufferDepthSampleCounts = sample_counts,
      .framebufferNoAttachmentsSampleCounts = sample_counts,
      .framebufferStencilSampleCounts = sample_counts,
      .maxColorAttachments = NVK_MAX_RTS,
      .sampledImageColorSampleCounts = sample_counts,
      .sampledImageIntegerSampleCounts = sample_counts,
      .sampledImageDepthSampleCounts = sample_counts,
      .sampledImageStencilSampleCounts = sample_counts,
      .storageImageSampleCounts = sample_counts,
      .maxSampleMaskWords = 1,
      .timestampComputeAndGraphics = true,
      .timestampPeriod = 1,
      .maxClipDistances = 8,
      .maxCullDistances = 8,
      .maxCombinedClipAndCullDistances = 8,
      .discreteQueuePriorities = 2,
      .pointSizeRange = { 1.0, 2047.94 },
      .lineWidthRange = { 1, 64 },
      .pointSizeGranularity = 0.0625,
      .lineWidthGranularity = 0.0625,
      .strictLines = true,
      .standardSampleLocations = true,
      .optimalBufferCopyOffsetAlignment = 1,
      .optimalBufferCopyRowPitchAlignment = 1,
      .nonCoherentAtomSize = 64,

      /* Vulkan 1.0 sparse properties */
      .sparseResidencyNonResidentStrict = true,
      .sparseResidencyAlignedMipSize = info->cls_eng3d < MAXWELL_B, /* DXVK/vkd3d-proton requires this to be advertised as VK_FALSE for FL12 */
      .sparseResidencyStandard2DBlockShape = true,
      .sparseResidencyStandard2DMultisampleBlockShape = true,
      .sparseResidencyStandard3DBlockShape = true,

      /* Vulkan 1.1 properties */
      .subgroupSize = 32,
      .subgroupSupportedStages = nvk_nak_stages(info),
      .subgroupSupportedOperations = VK_SUBGROUP_FEATURE_ARITHMETIC_BIT |
                                     VK_SUBGROUP_FEATURE_BALLOT_BIT |
                                     VK_SUBGROUP_FEATURE_BASIC_BIT |
                                     VK_SUBGROUP_FEATURE_CLUSTERED_BIT |
                                     VK_SUBGROUP_FEATURE_QUAD_BIT |
                                     VK_SUBGROUP_FEATURE_ROTATE_BIT_KHR |
                                     VK_SUBGROUP_FEATURE_ROTATE_CLUSTERED_BIT_KHR |
                                     VK_SUBGROUP_FEATURE_SHUFFLE_BIT |
                                     VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT |
                                     VK_SUBGROUP_FEATURE_VOTE_BIT,
      .subgroupQuadOperationsInAllStages = false,
      .pointClippingBehavior = VK_POINT_CLIPPING_BEHAVIOR_USER_CLIP_PLANES_ONLY,
      .maxMultiviewViewCount = NVK_MAX_MULTIVIEW_VIEW_COUNT,
      .maxMultiviewInstanceIndex = UINT32_MAX,
      .maxPerSetDescriptors = UINT32_MAX,
      .maxMemoryAllocationSize = (1u << 31),

      /* Vulkan 1.2 properties */
      .supportedDepthResolveModes = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT |
                                    VK_RESOLVE_MODE_AVERAGE_BIT |
                                    VK_RESOLVE_MODE_MIN_BIT |
                                    VK_RESOLVE_MODE_MAX_BIT,
      .supportedStencilResolveModes = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT |
                                      VK_RESOLVE_MODE_MIN_BIT |
                                      VK_RESOLVE_MODE_MAX_BIT,
      .independentResolveNone = true,
      .independentResolve = true,
      .driverID = VK_DRIVER_ID_MESA_NVK,
      .conformanceVersion =
         conformant ? (VkConformanceVersion) { 1, 3, 7, 3 }
                    : (VkConformanceVersion) { 0, 0, 0, 0 },
      .denormBehaviorIndependence = VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL,
      .roundingModeIndependence = VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL,
      .shaderSignedZeroInfNanPreserveFloat16 = true,
      .shaderSignedZeroInfNanPreserveFloat32 = true,
      .shaderSignedZeroInfNanPreserveFloat64 = true,
      .shaderDenormPreserveFloat16 = true,
      .shaderDenormPreserveFloat32 = true,
      .shaderDenormPreserveFloat64 = true,
      .shaderDenormFlushToZeroFloat16 = false,
      .shaderDenormFlushToZeroFloat32 = true,
      .shaderDenormFlushToZeroFloat64 = false,
      .shaderRoundingModeRTEFloat16 = true,
      .shaderRoundingModeRTEFloat32 = true,
      .shaderRoundingModeRTEFloat64 = true,
      .shaderRoundingModeRTZFloat16 = false,
      .shaderRoundingModeRTZFloat32 = true,
      .shaderRoundingModeRTZFloat64 = true,
      .maxUpdateAfterBindDescriptorsInAllPools = UINT32_MAX,
      .shaderUniformBufferArrayNonUniformIndexingNative = false,
      .shaderSampledImageArrayNonUniformIndexingNative = info->cls_eng3d >= TURING_A,
      .shaderStorageBufferArrayNonUniformIndexingNative = true,
      .shaderStorageImageArrayNonUniformIndexingNative = info->cls_eng3d >= TURING_A,
      .shaderInputAttachmentArrayNonUniformIndexingNative = false,
      .robustBufferAccessUpdateAfterBind = true,
      .quadDivergentImplicitLod = info->cls_eng3d >= TURING_A,
      .maxPerStageDescriptorUpdateAfterBindSamplers = NVK_MAX_DESCRIPTORS,
      .maxPerStageDescriptorUpdateAfterBindUniformBuffers = NVK_MAX_DESCRIPTORS,
      .maxPerStageDescriptorUpdateAfterBindStorageBuffers = NVK_MAX_DESCRIPTORS,
      .maxPerStageDescriptorUpdateAfterBindSampledImages = NVK_MAX_DESCRIPTORS,
      .maxPerStageDescriptorUpdateAfterBindStorageImages = NVK_MAX_DESCRIPTORS,
      .maxPerStageDescriptorUpdateAfterBindInputAttachments = NVK_MAX_DESCRIPTORS,
      .maxPerStageUpdateAfterBindResources = UINT32_MAX,
      .maxDescriptorSetUpdateAfterBindSamplers = NVK_MAX_DESCRIPTORS,
      .maxDescriptorSetUpdateAfterBindUniformBuffers = NVK_MAX_DESCRIPTORS,
      .maxDescriptorSetUpdateAfterBindUniformBuffersDynamic = NVK_MAX_DYNAMIC_BUFFERS / 2,
      .maxDescriptorSetUpdateAfterBindStorageBuffers = NVK_MAX_DESCRIPTORS,
      .maxDescriptorSetUpdateAfterBindStorageBuffersDynamic = NVK_MAX_DYNAMIC_BUFFERS / 2,
      .maxDescriptorSetUpdateAfterBindSampledImages = NVK_MAX_DESCRIPTORS,
      .maxDescriptorSetUpdateAfterBindStorageImages = NVK_MAX_DESCRIPTORS,
      .maxDescriptorSetUpdateAfterBindInputAttachments = NVK_MAX_DESCRIPTORS,
      .filterMinmaxSingleComponentFormats = true,
      .filterMinmaxImageComponentMapping = true,
      .maxTimelineSemaphoreValueDifference = UINT64_MAX,
      .framebufferIntegerColorSampleCounts = sample_counts,

      /* Vulkan 1.3 properties */
      .minSubgroupSize = 32,
      .maxSubgroupSize = 32,
      .maxComputeWorkgroupSubgroups = 1024 / 32,
      .requiredSubgroupSizeStages = 0,
      .maxInlineUniformBlockSize = 1 << 16,
      .maxPerStageDescriptorInlineUniformBlocks = 32,
      .maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks = 32,
      .maxDescriptorSetInlineUniformBlocks = 6 * 32,
      .maxDescriptorSetUpdateAfterBindInlineUniformBlocks = 6 * 32,
      .maxInlineUniformTotalSize = 1 << 16,
      .integerDotProduct4x8BitPackedUnsignedAccelerated
         = info->cls_eng3d >= VOLTA_A,
      .integerDotProduct4x8BitPackedSignedAccelerated
         = info->cls_eng3d >= VOLTA_A,
      .integerDotProduct4x8BitPackedMixedSignednessAccelerated
         = info->cls_eng3d >= VOLTA_A,
      .storageTexelBufferOffsetAlignmentBytes = NVK_MIN_TEXEL_BUFFER_ALIGNMENT,
      .storageTexelBufferOffsetSingleTexelAlignment = true,
      .uniformTexelBufferOffsetAlignmentBytes = NVK_MIN_TEXEL_BUFFER_ALIGNMENT,
      .uniformTexelBufferOffsetSingleTexelAlignment = true,
      .maxBufferSize = NVK_MAX_BUFFER_SIZE,

      /* VK_KHR_push_descriptor */
      .maxPushDescriptors = NVK_MAX_PUSH_DESCRIPTORS,

      /* VK_EXT_custom_border_color */
      .maxCustomBorderColorSamplers = 4000,

      /* VK_EXT_extended_dynamic_state3 */
      .dynamicPrimitiveTopologyUnrestricted = true,

      /* VK_EXT_graphics_pipeline_library */
      .graphicsPipelineLibraryFastLinking = true,
      .graphicsPipelineLibraryIndependentInterpolationDecoration = true,

      /* VK_KHR_line_rasterization */
      .lineSubPixelPrecisionBits = 8,

      /* VK_KHR_maintenance5 */
      .earlyFragmentMultisampleCoverageAfterSampleCounting = true,
      .earlyFragmentSampleMaskTestBeforeSampleCounting = true,
      .depthStencilSwizzleOneSupport = true,
      .polygonModePointSize = true,
      .nonStrictSinglePixelWideLinesUseParallelogram = false,
      .nonStrictWideLinesUseParallelogram = false,

      /* VK_KHR_maintenance6 */
      .blockTexelViewCompatibleMultipleLayers = true,
      .maxCombinedImageSamplerDescriptorCount = 3,
      .fragmentShadingRateClampCombinerInputs = false, /* TODO */

      /* VK_EXT_map_memory_placed */
      .minPlacedMemoryMapAlignment = os_page_size,

      /* VK_EXT_multi_draw */
      .maxMultiDrawCount = UINT32_MAX,

      /* VK_EXT_nested_command_buffer */
      .maxCommandBufferNestingLevel = UINT32_MAX,

      /* VK_EXT_pci_bus_info */
      .pciDomain   = info->pci.domain,
      .pciBus      = info->pci.bus,
      .pciDevice   = info->pci.dev,
      .pciFunction = info->pci.func,

      /* VK_EXT_physical_device_drm gets populated later */

      /* VK_EXT_provoking_vertex */
      .provokingVertexModePerPipeline = true,
      .transformFeedbackPreservesTriangleFanProvokingVertex = true,

      /* VK_EXT_robustness2 */
      .robustStorageBufferAccessSizeAlignment = NVK_SSBO_BOUNDS_CHECK_ALIGNMENT,
      .robustUniformBufferAccessSizeAlignment = nvk_min_cbuf_alignment(info),

      /* VK_EXT_sample_locations */
      .sampleLocationSampleCounts = sample_counts,
      .maxSampleLocationGridSize = (VkExtent2D){ 1, 1 },
      .sampleLocationCoordinateRange[0] = 0.0f,
      .sampleLocationCoordinateRange[1] = 0.9375f,
      .sampleLocationSubPixelBits = 4,
      .variableSampleLocations = true,

      /* VK_EXT_shader_object */
      .shaderBinaryVersion = 0,

      /* VK_EXT_transform_feedback */
      .maxTransformFeedbackStreams = 4,
      .maxTransformFeedbackBuffers = 4,
      .maxTransformFeedbackBufferSize = UINT32_MAX,
      .maxTransformFeedbackStreamDataSize = 2048,
      .maxTransformFeedbackBufferDataSize = 512,
      .maxTransformFeedbackBufferDataStride = 2048,
      .transformFeedbackQueries = true,
      .transformFeedbackStreamsLinesTriangles = false,
      .transformFeedbackRasterizationStreamSelect = true,
      .transformFeedbackDraw = true,

      /* VK_KHR_vertex_attribute_divisor */
      .maxVertexAttribDivisor = UINT32_MAX,
      .supportsNonZeroFirstInstance = true,

      /* VK_KHR_fragment_shader_barycentric */
      .triStripVertexOrderIndependentOfProvokingVertex = false,

      /* VK_NV_shader_sm_builtins */
      .shaderSMCount = (uint32_t)info->tpc_count * info->mp_per_tpc,
      .shaderWarpsPerSM = info->max_warps_per_mp,
   };

   /* Add the driver to the device name (like other Mesa drivers do) */
   if (!strcmp(info->device_name, info->chipset_name)) {
      snprintf(properties->deviceName, sizeof(properties->deviceName),
               "NVK %s", info->device_name);
   } else {
      snprintf(properties->deviceName, sizeof(properties->deviceName),
               "%s (NVK %s)", info->device_name, info->chipset_name);
   }

   /* VK_EXT_shader_module_identifier */
   STATIC_ASSERT(sizeof(vk_shaderModuleIdentifierAlgorithmUUID) ==
      sizeof(properties->shaderModuleIdentifierAlgorithmUUID));
   memcpy(properties->shaderModuleIdentifierAlgorithmUUID,
            vk_shaderModuleIdentifierAlgorithmUUID,
            sizeof(properties->shaderModuleIdentifierAlgorithmUUID));

   const struct {
      uint16_t vendor_id;
      uint16_t device_id;
      uint8_t pad[12];
   } dev_uuid = {
      .vendor_id = NVIDIA_VENDOR_ID,
      .device_id = info->device_id,
   };
   STATIC_ASSERT(sizeof(dev_uuid) == VK_UUID_SIZE);
   memcpy(properties->deviceUUID, &dev_uuid, VK_UUID_SIZE);
   STATIC_ASSERT(sizeof(instance->driver_build_sha) >= VK_UUID_SIZE);
   memcpy(properties->driverUUID, instance->driver_build_sha, VK_UUID_SIZE);

   snprintf(properties->driverName, VK_MAX_DRIVER_NAME_SIZE, "NVK");
   snprintf(properties->driverInfo, VK_MAX_DRIVER_INFO_SIZE,
            "Mesa " PACKAGE_VERSION MESA_GIT_SHA1);
}

static void
nvk_physical_device_init_pipeline_cache(struct nvk_physical_device *pdev)
{
   struct nvk_instance *instance = nvk_physical_device_instance(pdev);

   struct mesa_sha1 sha_ctx;
   _mesa_sha1_init(&sha_ctx);

   _mesa_sha1_update(&sha_ctx, instance->driver_build_sha,
                     sizeof(instance->driver_build_sha));

   const uint64_t compiler_flags = nvk_physical_device_compiler_flags(pdev);
   _mesa_sha1_update(&sha_ctx, &compiler_flags, sizeof(compiler_flags));

   unsigned char sha[SHA1_DIGEST_LENGTH];
   _mesa_sha1_final(&sha_ctx, sha);

   STATIC_ASSERT(SHA1_DIGEST_LENGTH >= VK_UUID_SIZE);
   memcpy(pdev->vk.properties.pipelineCacheUUID, sha, VK_UUID_SIZE);
   memcpy(pdev->vk.properties.shaderBinaryUUID, sha, VK_UUID_SIZE);

#ifdef ENABLE_SHADER_CACHE
   char renderer[10];
   ASSERTED int len = snprintf(renderer, sizeof(renderer), "nvk_%04x",
                               pdev->info.chipset);
   assert(len == sizeof(renderer) - 2);

   char timestamp[41];
   _mesa_sha1_format(timestamp, instance->driver_build_sha);

   const uint64_t driver_flags = nvk_physical_device_compiler_flags(pdev);
   pdev->vk.disk_cache = disk_cache_create(renderer, timestamp, driver_flags);
#endif
}

static void
nvk_physical_device_free_disk_cache(struct nvk_physical_device *pdev)
{
#ifdef ENABLE_SHADER_CACHE
   if (pdev->vk.disk_cache) {
      disk_cache_destroy(pdev->vk.disk_cache);
      pdev->vk.disk_cache = NULL;
   }
#else
   assert(pdev->vk.disk_cache == NULL);
#endif
}

static uint64_t
nvk_get_sysmem_heap_size(void)
{
   uint64_t sysmem_size_B = 0;
   if (!os_get_total_physical_memory(&sysmem_size_B))
      return 0;

   /* Use 3/4 of total size to avoid swapping */
   return ROUND_DOWN_TO(sysmem_size_B * 3 / 4, 1 << 20);
}

static uint64_t
nvk_get_sysmem_heap_available(struct nvk_physical_device *pdev)
{
   uint64_t sysmem_size_B = 0;
   if (!os_get_available_system_memory(&sysmem_size_B)) {
      vk_loge(VK_LOG_OBJS(pdev), "Failed to query available system memory");
      return 0;
   }

   /* Use 3/4 of available to avoid swapping */
   return ROUND_DOWN_TO(sysmem_size_B * 3 / 4, 1 << 20);
}

static uint64_t
nvk_get_vram_heap_available(struct nvk_physical_device *pdev)
{
   const uint64_t used = nouveau_ws_device_vram_used(pdev->ws_dev);
   if (used > pdev->info.vram_size_B)
      return 0;

   return pdev->info.vram_size_B - used;
}

static bool
drm_device_is_nouveau(const char *path)
{
   int fd = open(path, O_RDWR | O_CLOEXEC);
   if (fd < 0)
      return false;

   drmVersionPtr ver = drmGetVersion(fd);
   if (!ver) {
      close(fd);
      return false;
   }

   const bool is_nouveau = !strncmp("nouveau", ver->name, ver->name_len);

   drmFreeVersion(ver);
   close(fd);

   return is_nouveau;
}

VkResult
nvk_create_drm_physical_device(struct vk_instance *_instance,
                               drmDevicePtr drm_device,
                               struct vk_physical_device **pdev_out)
{
   struct nvk_instance *instance = (struct nvk_instance *)_instance;
   VkResult result;
   int master_fd = -1;

   if (!(drm_device->available_nodes & (1 << DRM_NODE_RENDER)))
      return VK_ERROR_INCOMPATIBLE_DRIVER;

   switch (drm_device->bustype) {
   case DRM_BUS_PCI:
      if (drm_device->deviceinfo.pci->vendor_id != NVIDIA_VENDOR_ID)
         return VK_ERROR_INCOMPATIBLE_DRIVER;
      break;

   case DRM_BUS_PLATFORM: {
      const char *compat_prefix = "nvidia,";
      bool found = false;
      for (int i = 0; drm_device->deviceinfo.platform->compatible[i] != NULL; i++) {
         if (strncmp(drm_device->deviceinfo.platform->compatible[0], compat_prefix, strlen(compat_prefix)) == 0) {
            found = true;
            break;
         }
      }
      if (!found)
         return VK_ERROR_INCOMPATIBLE_DRIVER;
      break;
   }

   default:
      return VK_ERROR_INCOMPATIBLE_DRIVER;
   }

   if (!drm_device_is_nouveau(drm_device->nodes[DRM_NODE_RENDER]))
      return VK_ERROR_INCOMPATIBLE_DRIVER;

   struct nouveau_ws_device *ws_dev = nouveau_ws_device_new(drm_device);
   if (!ws_dev)
      return vk_error(instance, VK_ERROR_INCOMPATIBLE_DRIVER);

   const struct nv_device_info info = ws_dev->info;
   const struct vk_sync_type syncobj_sync_type =
      vk_drm_syncobj_get_type(ws_dev->fd);

   /* We don't support anything pre-Kepler */
   if (info.cls_eng3d < KEPLER_A) {
      result = VK_ERROR_INCOMPATIBLE_DRIVER;
      goto fail_ws_dev;
   }

   bool conformant =
      info.type == NV_DEVICE_TYPE_DIS &&
      info.cls_eng3d >= TURING_A && info.cls_eng3d <= ADA_A;

   if (!conformant &&
       !debug_get_bool_option("NVK_I_WANT_A_BROKEN_VULKAN_DRIVER", false)) {
#ifdef NDEBUG
      result = VK_ERROR_INCOMPATIBLE_DRIVER;
#else
      result = vk_errorf(instance, VK_ERROR_INCOMPATIBLE_DRIVER,
                         "WARNING: NVK is not well-tested on %s, pass "
                         "NVK_I_WANT_A_BROKEN_VULKAN_DRIVER=1 "
                         "if you know what you're doing.",
                         info.device_name);
#endif
      goto fail_ws_dev;
   }

   if (!ws_dev->has_vm_bind) {
      result = vk_errorf(instance, VK_ERROR_INCOMPATIBLE_DRIVER,
                         "NVK Requires a Linux kernel version 6.6 or later");
      goto fail_ws_dev;
   }

   struct stat st;
   if (stat(drm_device->nodes[DRM_NODE_RENDER], &st)) {
      result = vk_errorf(instance, VK_ERROR_INITIALIZATION_FAILED,
                         "fstat() failed on %s: %m",
                         drm_device->nodes[DRM_NODE_RENDER]);
      goto fail_ws_dev;
   }
   const dev_t render_dev = st.st_rdev;

   if (!conformant)
      vk_warn_non_conformant_implementation("NVK");

   struct nvk_physical_device *pdev =
      vk_zalloc(&instance->vk.alloc, sizeof(*pdev),
                8, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

   if (pdev == NULL) {
      result = vk_error(instance, VK_ERROR_OUT_OF_HOST_MEMORY);
      goto fail_ws_dev;
   }

   struct vk_physical_device_dispatch_table dispatch_table;
   vk_physical_device_dispatch_table_from_entrypoints(
      &dispatch_table, &nvk_physical_device_entrypoints, true);
   vk_physical_device_dispatch_table_from_entrypoints(
      &dispatch_table, &wsi_physical_device_entrypoints, false);

   const bool has_tiled_bos = nouveau_ws_device_has_tiled_bo(ws_dev);
   struct vk_device_extension_table supported_extensions;
   nvk_get_device_extensions(instance, &info, has_tiled_bos,
                             &supported_extensions);

   struct vk_features supported_features;
   nvk_get_device_features(&info, &supported_extensions, &supported_features);

   struct vk_properties properties;
   nvk_get_device_properties(instance, &info, conformant, &properties);

   properties.drmHasRender = true;
   properties.drmRenderMajor = major(render_dev);
   properties.drmRenderMinor = minor(render_dev);

   /* DRM primary is optional */
   if ((drm_device->available_nodes & (1 << DRM_NODE_PRIMARY)) &&
       !stat(drm_device->nodes[DRM_NODE_PRIMARY], &st)) {
      assert(st.st_rdev != 0);
      properties.drmHasPrimary = true;
      properties.drmPrimaryMajor = major(st.st_rdev);
      properties.drmPrimaryMinor = minor(st.st_rdev);

      /* TODO: Test if the FD is usable? */
      if (instance->vk.enabled_extensions.KHR_display)
         master_fd = open(drm_device->nodes[DRM_NODE_PRIMARY], O_RDWR | O_CLOEXEC);
   }

   result = vk_physical_device_init(&pdev->vk, &instance->vk,
                                    &supported_extensions,
                                    &supported_features,
                                    &properties,
                                    &dispatch_table);
   if (result != VK_SUCCESS)
      goto fail_master_fd;

   pdev->info = info;
   pdev->debug_flags = ws_dev->debug_flags;
   pdev->render_dev = render_dev;
   pdev->master_fd = master_fd;
   pdev->ws_dev = ws_dev;

   pdev->nak = nak_compiler_create(&pdev->info);
   if (pdev->nak == NULL) {
      result = vk_error(instance, VK_ERROR_OUT_OF_HOST_MEMORY);
      goto fail_init;
   }

   nvk_physical_device_init_pipeline_cache(pdev);

   uint64_t sysmem_size_B = nvk_get_sysmem_heap_size();
   if (sysmem_size_B == 0) {
      result = vk_errorf(instance, VK_ERROR_INITIALIZATION_FAILED,
                         "Failed to query total system memory");
      goto fail_disk_cache;
   }

   if (pdev->info.vram_size_B > 0) {
      uint32_t vram_heap_idx = pdev->mem_heap_count++;
      uint32_t bar_heap_idx = vram_heap_idx;
      pdev->mem_heaps[vram_heap_idx] = (struct nvk_memory_heap) {
         .size = pdev->info.vram_size_B,
         .flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT,
      };

      if (pdev->info.bar_size_B > 0 &&
          pdev->info.bar_size_B < pdev->info.vram_size_B) {
         bar_heap_idx = pdev->mem_heap_count++;
         pdev->mem_heaps[bar_heap_idx] = (struct nvk_memory_heap) {
            .size = pdev->info.bar_size_B,
            .flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT,
         };
      }

      /* Only set available if we have the ioctl. */
      if (nouveau_ws_device_vram_used(ws_dev) > 0)
         pdev->mem_heaps[vram_heap_idx].available = nvk_get_vram_heap_available;

      pdev->mem_types[pdev->mem_type_count++] = (VkMemoryType) {
         .propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
         .heapIndex = vram_heap_idx,
      };

      if (pdev->info.cls_eng3d >= MAXWELL_A) {
         pdev->mem_types[pdev->mem_type_count++] = (VkMemoryType) {
            .propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            .heapIndex = bar_heap_idx,
         };
      }
   }

   uint32_t sysmem_heap_idx = pdev->mem_heap_count++;
   pdev->mem_heaps[sysmem_heap_idx] = (struct nvk_memory_heap) {
      .size = sysmem_size_B,
      /* If we don't have any VRAM (iGPU), claim sysmem as DEVICE_LOCAL */
      .flags = pdev->info.vram_size_B == 0
               ? VK_MEMORY_HEAP_DEVICE_LOCAL_BIT
               : 0,
      .available = nvk_get_sysmem_heap_available,
   };

   pdev->mem_types[pdev->mem_type_count++] = (VkMemoryType) {
      /* TODO: What's the right thing to do here on Tegra? */
      .propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                       VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
      .heapIndex = sysmem_heap_idx,
   };

   assert(pdev->mem_heap_count <= ARRAY_SIZE(pdev->mem_heaps));
   assert(pdev->mem_type_count <= ARRAY_SIZE(pdev->mem_types));

   pdev->queue_families[pdev->queue_family_count++] = (struct nvk_queue_family) {
      .queue_flags = VK_QUEUE_GRAPHICS_BIT |
                     VK_QUEUE_COMPUTE_BIT |
                     VK_QUEUE_TRANSFER_BIT |
                     VK_QUEUE_SPARSE_BINDING_BIT,
      .queue_count = 1,
   };
   assert(pdev->queue_family_count <= ARRAY_SIZE(pdev->queue_families));

   unsigned st_idx = 0;
   pdev->syncobj_sync_type = syncobj_sync_type;
   pdev->sync_types[st_idx++] = &pdev->syncobj_sync_type;
   pdev->sync_types[st_idx++] = NULL;
   assert(st_idx <= ARRAY_SIZE(pdev->sync_types));
   pdev->vk.supported_sync_types = pdev->sync_types;

   result = nvk_init_wsi(pdev);
   if (result != VK_SUCCESS)
      goto fail_disk_cache;

   *pdev_out = &pdev->vk;

   return VK_SUCCESS;

fail_disk_cache:
   nvk_physical_device_free_disk_cache(pdev);
   nak_compiler_destroy(pdev->nak);
fail_init:
   vk_physical_device_finish(&pdev->vk);
fail_master_fd:
   if (master_fd >= 0)
      close(master_fd);
   vk_free(&instance->vk.alloc, pdev);
fail_ws_dev:
   nouveau_ws_device_destroy(ws_dev);
   return result;
}

void
nvk_physical_device_destroy(struct vk_physical_device *vk_pdev)
{
   struct nvk_physical_device *pdev =
      container_of(vk_pdev, struct nvk_physical_device, vk);

   nvk_finish_wsi(pdev);
   nvk_physical_device_free_disk_cache(pdev);
   nak_compiler_destroy(pdev->nak);
   if (pdev->master_fd >= 0)
      close(pdev->master_fd);
   nouveau_ws_device_destroy(pdev->ws_dev);
   vk_physical_device_finish(&pdev->vk);
   vk_free(&pdev->vk.instance->alloc, pdev);
}

VKAPI_ATTR void VKAPI_CALL
nvk_GetPhysicalDeviceMemoryProperties2(
   VkPhysicalDevice physicalDevice,
   VkPhysicalDeviceMemoryProperties2 *pMemoryProperties)
{
   VK_FROM_HANDLE(nvk_physical_device, pdev, physicalDevice);

   pMemoryProperties->memoryProperties.memoryHeapCount = pdev->mem_heap_count;
   for (int i = 0; i < pdev->mem_heap_count; i++) {
      pMemoryProperties->memoryProperties.memoryHeaps[i] = (VkMemoryHeap) {
         .size = pdev->mem_heaps[i].size,
         .flags = pdev->mem_heaps[i].flags,
      };
   }

   pMemoryProperties->memoryProperties.memoryTypeCount = pdev->mem_type_count;
   for (int i = 0; i < pdev->mem_type_count; i++) {
      pMemoryProperties->memoryProperties.memoryTypes[i] = pdev->mem_types[i];
   }

   vk_foreach_struct(ext, pMemoryProperties->pNext)
   {
      switch (ext->sType) {
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT: {
         VkPhysicalDeviceMemoryBudgetPropertiesEXT *p = (void *)ext;

         for (unsigned i = 0; i < pdev->mem_heap_count; i++) {
            const struct nvk_memory_heap *heap = &pdev->mem_heaps[i];
            uint64_t used = p_atomic_read(&heap->used);

            /* From the Vulkan 1.3.278 spec:
             *
             *    "heapUsage is an array of VK_MAX_MEMORY_HEAPS VkDeviceSize
             *    values in which memory usages are returned, with one element
             *    for each memory heap. A heap’s usage is an estimate of how
             *    much memory the process is currently using in that heap."
             *
             * TODO: Include internal allocations?
             */
            p->heapUsage[i] = used;

            uint64_t available = heap->size;
            if (heap->available)
               available = heap->available(pdev);

            /* From the Vulkan 1.3.278 spec:
             *
             *    "heapBudget is an array of VK_MAX_MEMORY_HEAPS VkDeviceSize
             *    values in which memory budgets are returned, with one
             *    element for each memory heap. A heap’s budget is a rough
             *    estimate of how much memory the process can allocate from
             *    that heap before allocations may fail or cause performance
             *    degradation. The budget includes any currently allocated
             *    device memory."
             *
             * and
             *
             *    "The heapBudget value must be less than or equal to
             *    VkMemoryHeap::size for each heap."
             *
             * available (queried above) is the total amount free memory
             * system-wide and does not include our allocations so we need
             * to add that in.
             */
            uint64_t budget = MIN2(available + used, heap->size);

            /* Set the budget at 90% of available to avoid thrashing */
            p->heapBudget[i] = ROUND_DOWN_TO(budget * 9 / 10, 1 << 20);
         }

         /* From the Vulkan 1.3.278 spec:
          *
          *    "The heapBudget and heapUsage values must be zero for array
          *    elements greater than or equal to
          *    VkPhysicalDeviceMemoryProperties::memoryHeapCount. The
          *    heapBudget value must be non-zero for array elements less than
          *    VkPhysicalDeviceMemoryProperties::memoryHeapCount."
          */
         for (unsigned i = pdev->mem_heap_count; i < VK_MAX_MEMORY_HEAPS; i++) {
            p->heapBudget[i] = 0u;
            p->heapUsage[i] = 0u;
         }
         break;
      }
      default:
         nvk_debug_ignored_stype(ext->sType);
         break;
      }
   }
}

VKAPI_ATTR void VKAPI_CALL
nvk_GetPhysicalDeviceQueueFamilyProperties2(
   VkPhysicalDevice physicalDevice,
   uint32_t *pQueueFamilyPropertyCount,
   VkQueueFamilyProperties2 *pQueueFamilyProperties)
{
   VK_FROM_HANDLE(nvk_physical_device, pdev, physicalDevice);
   VK_OUTARRAY_MAKE_TYPED(VkQueueFamilyProperties2, out, pQueueFamilyProperties,
                          pQueueFamilyPropertyCount);

   for (uint8_t i = 0; i < pdev->queue_family_count; i++) {
      const struct nvk_queue_family *queue_family = &pdev->queue_families[i];

      vk_outarray_append_typed(VkQueueFamilyProperties2, &out, p) {
         p->queueFamilyProperties.queueFlags = queue_family->queue_flags;
         p->queueFamilyProperties.queueCount = queue_family->queue_count;
         p->queueFamilyProperties.timestampValidBits = 64;
         p->queueFamilyProperties.minImageTransferGranularity =
            (VkExtent3D){1, 1, 1};
      }
   }
}

static const VkTimeDomainKHR nvk_time_domains[] = {
   VK_TIME_DOMAIN_DEVICE_KHR,
   VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR,
#ifdef CLOCK_MONOTONIC_RAW
   VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR,
#endif
};

VKAPI_ATTR VkResult VKAPI_CALL
nvk_GetPhysicalDeviceCalibrateableTimeDomainsKHR(
   VkPhysicalDevice physicalDevice,
   uint32_t *pTimeDomainCount,
   VkTimeDomainKHR *pTimeDomains)
{
   VK_OUTARRAY_MAKE_TYPED(VkTimeDomainKHR, out, pTimeDomains, pTimeDomainCount);

   for (int d = 0; d < ARRAY_SIZE(nvk_time_domains); d++) {
      vk_outarray_append_typed(VkTimeDomainKHR, &out, i) {
         *i = nvk_time_domains[d];
      }
   }

   return vk_outarray_status(&out);
}


VKAPI_ATTR void VKAPI_CALL
nvk_GetPhysicalDeviceMultisamplePropertiesEXT(
   VkPhysicalDevice physicalDevice,
   VkSampleCountFlagBits samples,
   VkMultisamplePropertiesEXT *pMultisampleProperties)
{
   VK_FROM_HANDLE(nvk_physical_device, pdev, physicalDevice);

   if (samples & pdev->vk.properties.sampleLocationSampleCounts) {
      pMultisampleProperties->maxSampleLocationGridSize = (VkExtent2D){1, 1};
   } else {
      pMultisampleProperties->maxSampleLocationGridSize = (VkExtent2D){0, 0};
   }
}
