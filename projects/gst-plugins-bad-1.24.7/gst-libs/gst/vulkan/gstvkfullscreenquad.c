/*
 * GStreamer Plugins Vulkan
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvkfullscreenquad.h"

/**
 * SECTION:vkfullscreenquad
 * @title: GstVulkanFullScreenQuad
 * @short_description: Vulkan full screen quad
 * @see_also: #GstVulkanDevice, #GstVulkanImageMemory
 *
 * A #GstVulkanFullScreenQuad is a helper object for rendering a single input
 * image to an output #GstBuffer
 */

#define GST_CAT_DEFAULT gst_vulkan_full_screen_quad_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

/* XXX: privatise this on moving to lib */
struct Vertex
{
  float x, y, z;
  float s, t;
};

struct _GstVulkanFullScreenQuadPrivate
{
  GstBuffer *inbuf;
  GstBuffer *outbuf;

  GstMemory *vertices;
  GstMemory *indices;
  gsize n_indices;
  GstMemory *uniforms;
  gsize uniform_size;

  GstVulkanHandle *vert;
  GstVulkanHandle *frag;

  VkBool32 blend_enable;
  VkBlendFactor src_blend_factor;
  VkBlendFactor src_alpha_blend_factor;
  VkBlendFactor dst_blend_factor;
  VkBlendFactor dst_alpha_blend_factor;
  VkBlendOp colour_blend_op;
  VkBlendOp alpha_blend_op;

  gboolean enable_clear;
};

G_DEFINE_TYPE_WITH_CODE (GstVulkanFullScreenQuad, gst_vulkan_full_screen_quad,
    GST_TYPE_OBJECT, GST_DEBUG_CATEGORY_INIT (gst_vulkan_full_screen_quad_debug,
        "vulkanfullscreenquad", 0, "vulkan fullscreen quad render");
    G_ADD_PRIVATE (GstVulkanFullScreenQuad));

#define GET_PRIV(self) gst_vulkan_full_screen_quad_get_instance_private (self)

struct Vertex vertices[] = {
  {-1.0f, -1.0f, 0.0f, 0.0f, 0.0f},
  {1.0f, -1.0f, 0.0f, 1.0f, 0.0f},
  {1.0f, 1.0f, 0.0f, 1.0f, 1.0f},
  {-1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
};

gushort indices[] = {
  0, 1, 2, 0, 2, 3,
};

static gboolean
create_sampler (GstVulkanFullScreenQuad * self, GError ** error)
{
  /* *INDENT-OFF* */
  VkSamplerCreateInfo samplerInfo = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 1,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .mipLodBias = 0.0f,
      .minLod = 0.0f,
      .maxLod = 0.0f
  };
  /* *INDENT-ON* */
  VkSampler sampler;
  VkResult err;

  err =
      vkCreateSampler (self->queue->device->device, &samplerInfo, NULL,
      &sampler);
  if (gst_vulkan_error_to_g_error (err, error, "vkCreateSampler") < 0) {
    return FALSE;
  }

  self->sampler = gst_vulkan_handle_new_wrapped (self->queue->device,
      GST_VULKAN_HANDLE_TYPE_SAMPLER, (GstVulkanHandleTypedef) sampler,
      gst_vulkan_handle_free_sampler, NULL);

  return TRUE;
}

static GstVulkanDescriptorSet *
get_and_update_descriptor_set (GstVulkanFullScreenQuad * self,
    GstVulkanImageView ** views, guint n_mems, GError ** error)
{
  GstVulkanFullScreenQuadPrivate *priv = GET_PRIV (self);
  GstVulkanDescriptorSet *set;

  if (!self->sampler)
    if (!create_sampler (self, error))
      return NULL;

  if (!(set =
          gst_vulkan_descriptor_cache_acquire (self->descriptor_cache, error)))
    return NULL;

  {
    VkWriteDescriptorSet writes[GST_VIDEO_MAX_PLANES + 1];
    VkDescriptorImageInfo image_info[GST_VIDEO_MAX_PLANES];
    VkDescriptorBufferInfo buffer_info;
    int write_n = 0;
    int i;

    /* *INDENT-OFF* */
    if (priv->uniforms) {
      buffer_info = (VkDescriptorBufferInfo) {
          .buffer = ((GstVulkanBufferMemory *) priv->uniforms)->buffer,
          .offset = 0,
          .range = priv->uniform_size
      };

      writes[write_n++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .pNext = NULL,
          .dstSet = set->set,
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
          .pBufferInfo = &buffer_info
      };
    }

    for (i = 0; i < n_mems; i++) {
      image_info[i] = (VkDescriptorImageInfo) {
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          .imageView = views[i]->view,
          .sampler = (VkSampler) self->sampler->handle
      };

      writes[write_n++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .pNext = NULL,
          .dstSet = set->set,
          .dstBinding = i + 1,
          .dstArrayElement = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .pImageInfo = &image_info[i]
      };
    }
    /* *INDENT-ON* */
    vkUpdateDescriptorSets (self->queue->device->device, write_n, writes, 0,
        NULL);
  }

  return set;
}

static gboolean
create_descriptor_set_layout (GstVulkanFullScreenQuad * self, GError ** error)
{
  GstVulkanFullScreenQuadPrivate *priv = GET_PRIV (self);
  VkDescriptorSetLayoutBinding bindings[GST_VIDEO_MAX_PLANES + 1] = { {0,} };
  VkDescriptorSetLayoutCreateInfo layout_info;
  VkDescriptorSetLayout descriptor_set_layout;
  int descriptor_n = 0;
  VkResult err;
  int i, n_mems;

  /* *INDENT-OFF* */
  bindings[descriptor_n++] = (VkDescriptorSetLayoutBinding) {
      .binding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .pImmutableSamplers = NULL,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
  };
  n_mems = gst_buffer_n_memory (priv->inbuf);
  for (i = 0; i < n_mems; i++) {
    bindings[descriptor_n++] = (VkDescriptorSetLayoutBinding) {
      .binding = i+1,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImmutableSamplers = NULL,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };
  };

  layout_info = (VkDescriptorSetLayoutCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = NULL,
      .bindingCount = descriptor_n,
      .pBindings = bindings
  };
  /* *INDENT-ON* */

  err =
      vkCreateDescriptorSetLayout (self->queue->device->device, &layout_info,
      NULL, &descriptor_set_layout);
  if (gst_vulkan_error_to_g_error (err, error,
          "vkCreateDescriptorSetLayout") < 0) {
    return FALSE;
  }

  self->descriptor_set_layout =
      gst_vulkan_handle_new_wrapped (self->queue->device,
      GST_VULKAN_HANDLE_TYPE_DESCRIPTOR_SET_LAYOUT,
      (GstVulkanHandleTypedef) descriptor_set_layout,
      gst_vulkan_handle_free_descriptor_set_layout, NULL);

  return TRUE;
}

static gboolean
create_pipeline_layout (GstVulkanFullScreenQuad * self, GError ** error)
{
  VkPipelineLayoutCreateInfo pipeline_layout_info;
  VkPipelineLayout pipeline_layout;
  VkResult err;

  if (!self->descriptor_set_layout)
    if (!create_descriptor_set_layout (self, error))
      return FALSE;

  /* *INDENT-OFF* */
  pipeline_layout_info = (VkPipelineLayoutCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = NULL,
      .setLayoutCount = 1,
      .pSetLayouts = (VkDescriptorSetLayout *) &self->descriptor_set_layout->handle,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = NULL,
  };
  /* *INDENT-ON* */

  err =
      vkCreatePipelineLayout (self->queue->device->device,
      &pipeline_layout_info, NULL, &pipeline_layout);
  if (gst_vulkan_error_to_g_error (err, error, "vkCreatePipelineLayout") < 0) {
    return FALSE;
  }

  self->pipeline_layout = gst_vulkan_handle_new_wrapped (self->queue->device,
      GST_VULKAN_HANDLE_TYPE_PIPELINE_LAYOUT,
      (GstVulkanHandleTypedef) pipeline_layout,
      gst_vulkan_handle_free_pipeline_layout, NULL);

  return TRUE;
}

static gboolean
create_render_pass (GstVulkanFullScreenQuad * self, GError ** error)
{
  GstVulkanFullScreenQuadPrivate *priv = GET_PRIV (self);
  VkAttachmentDescription color_attachments[GST_VIDEO_MAX_PLANES];
  VkAttachmentReference color_attachment_refs[GST_VIDEO_MAX_PLANES];
  VkRenderPassCreateInfo render_pass_info;
  VkSubpassDescription subpass;
  VkRenderPass render_pass;
  VkResult err;
  int i, n_mems;

  n_mems = gst_buffer_n_memory (priv->outbuf);
  for (i = 0; i < n_mems; i++) {
    /* *INDENT-OFF* */
    color_attachments[i] = (VkAttachmentDescription) {
        .format = gst_vulkan_format_from_video_info (&self->out_info, i),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = priv->enable_clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        /* FIXME: share this between elements to avoid pipeline barriers */
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    color_attachment_refs[i] = (VkAttachmentReference) {
      .attachment = i,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
    /* *INDENT-ON* */
  }

  /* *INDENT-OFF* */
  subpass = (VkSubpassDescription) {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = n_mems,
      .pColorAttachments = color_attachment_refs
  };

  render_pass_info = (VkRenderPassCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = NULL,
      .attachmentCount = n_mems,
      .pAttachments = color_attachments,
      .subpassCount = 1,
      .pSubpasses = &subpass
  };
  /* *INDENT-ON* */

  err =
      vkCreateRenderPass (self->queue->device->device, &render_pass_info, NULL,
      &render_pass);
  if (gst_vulkan_error_to_g_error (err, error, "vkCreateRenderPass") < 0) {
    return FALSE;
  }

  self->render_pass = gst_vulkan_handle_new_wrapped (self->queue->device,
      GST_VULKAN_HANDLE_TYPE_RENDER_PASS,
      (GstVulkanHandleTypedef) render_pass,
      gst_vulkan_handle_free_render_pass, NULL);

  return TRUE;
}

static gboolean
create_pipeline (GstVulkanFullScreenQuad * self, GError ** error)
{
  GstVulkanFullScreenQuadPrivate *priv = GET_PRIV (self);
  VkVertexInputBindingDescription vertex_binding;
  VkVertexInputAttributeDescription attribute_descriptions[2];
  VkPipelineShaderStageCreateInfo shader_create_info[2];
  VkPipelineVertexInputStateCreateInfo vertex_input_info;
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  VkPipelineViewportStateCreateInfo viewport_state;
  VkPipelineRasterizationStateCreateInfo rasterizer;
  VkPipelineMultisampleStateCreateInfo multisampling;
  VkPipelineColorBlendAttachmentState
      color_blend_attachments[GST_VIDEO_MAX_PLANES];
  VkPipelineColorBlendStateCreateInfo color_blending;
  VkGraphicsPipelineCreateInfo pipeline_create_info;
  VkPipeline pipeline;
  VkResult err;

  if (!priv->vert || !priv->frag) {
    g_set_error_literal (error, GST_VULKAN_ERROR,
        VK_ERROR_INITIALIZATION_FAILED, "Missing shader information");
    return FALSE;
  }

  if (!self->pipeline_layout)
    if (!create_pipeline_layout (self, error))
      return FALSE;

  if (!self->render_pass)
    if (!create_render_pass (self, error))
      return FALSE;

  /* *INDENT-OFF* */
  shader_create_info[0] = (VkPipelineShaderStageCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = NULL,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = (VkShaderModule) priv->vert->handle,
      .pName = "main"
  };

  shader_create_info[1] = (VkPipelineShaderStageCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = NULL,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = (VkShaderModule) priv->frag->handle,
      .pName = "main"
  };

  /* *INDENT-OFF* */
  vertex_binding = (VkVertexInputBindingDescription) {
      .binding = 0,
      .stride = sizeof (struct Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
  };

  attribute_descriptions[0] = (VkVertexInputAttributeDescription) {
      .binding = 0,
      .location = 0,
      .format = VK_FORMAT_R32G32B32_SFLOAT,
      .offset = G_STRUCT_OFFSET (struct Vertex, x)
  };
  attribute_descriptions[1] = (VkVertexInputAttributeDescription) {
      .binding = 0,
      .location = 1,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = G_STRUCT_OFFSET (struct Vertex, s)
  };

  vertex_input_info = (VkPipelineVertexInputStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = NULL,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vertex_binding,
      .vertexAttributeDescriptionCount = 2,
      .pVertexAttributeDescriptions = attribute_descriptions,
  };

  input_assembly = (VkPipelineInputAssemblyStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .pNext = NULL,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
      .primitiveRestartEnable = VK_FALSE
  };

  viewport_state = (VkPipelineViewportStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = NULL,
      .viewportCount = 1,
      .pViewports = &(VkViewport) {
          .x = 0.0f,
          .y = 0.0f,
          .width = (float) GST_VIDEO_INFO_WIDTH (&self->out_info),
          .height = (float) GST_VIDEO_INFO_HEIGHT (&self->out_info),
          .minDepth = 0.0f,
          .maxDepth = 1.0f
      },
      .scissorCount = 1,
      .pScissors = &(VkRect2D) {
          .offset = { 0, 0 },
          .extent = {
              GST_VIDEO_INFO_WIDTH (&self->out_info),
              GST_VIDEO_INFO_HEIGHT (&self->out_info)
          }
      }
  };

  rasterizer = (VkPipelineRasterizationStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .pNext = NULL,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .lineWidth = 1.0f,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasEnable = VK_FALSE
  };

  multisampling = (VkPipelineMultisampleStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = NULL,
      .sampleShadingEnable = VK_FALSE,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
  };

  color_blend_attachments[0] = (VkPipelineColorBlendAttachmentState) {
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .srcColorBlendFactor = priv->src_blend_factor,
      .dstColorBlendFactor = priv->dst_blend_factor,
      .colorBlendOp = priv->colour_blend_op,
      .srcAlphaBlendFactor = priv->src_alpha_blend_factor,
      .dstAlphaBlendFactor = priv->dst_alpha_blend_factor,
      .alphaBlendOp = priv->alpha_blend_op,
      .blendEnable = priv->blend_enable
  };
  color_blend_attachments[1] = (VkPipelineColorBlendAttachmentState) {
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .srcColorBlendFactor = priv->src_blend_factor,
      .dstColorBlendFactor = priv->dst_blend_factor,
      .colorBlendOp = priv->colour_blend_op,
      .srcAlphaBlendFactor = priv->src_alpha_blend_factor,
      .dstAlphaBlendFactor = priv->dst_alpha_blend_factor,
      .alphaBlendOp = priv->alpha_blend_op,
      .blendEnable = priv->blend_enable
  };
  color_blend_attachments[2] = (VkPipelineColorBlendAttachmentState) {
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .srcColorBlendFactor = priv->src_blend_factor,
      .dstColorBlendFactor = priv->dst_blend_factor,
      .colorBlendOp = priv->colour_blend_op,
      .srcAlphaBlendFactor = priv->src_alpha_blend_factor,
      .dstAlphaBlendFactor = priv->dst_alpha_blend_factor,
      .alphaBlendOp = priv->alpha_blend_op,
      .blendEnable = priv->blend_enable
  };
  color_blend_attachments[3] = (VkPipelineColorBlendAttachmentState) {
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .srcColorBlendFactor = priv->src_blend_factor,
      .dstColorBlendFactor = priv->dst_blend_factor,
      .colorBlendOp = priv->colour_blend_op,
      .srcAlphaBlendFactor = priv->src_alpha_blend_factor,
      .dstAlphaBlendFactor = priv->dst_alpha_blend_factor,
      .alphaBlendOp = priv->alpha_blend_op,
      .blendEnable = priv->blend_enable
  };

  color_blending = (VkPipelineColorBlendStateCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = NULL,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = gst_buffer_n_memory (priv->outbuf),
      .pAttachments = color_blend_attachments,
      .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }
  };

  pipeline_create_info = (VkGraphicsPipelineCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = NULL,
      .stageCount = 2,
      .pStages = shader_create_info,
      .pVertexInputState = &vertex_input_info,
      .pInputAssemblyState = &input_assembly,
      .pViewportState = &viewport_state,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pColorBlendState = &color_blending,
      .layout = (VkPipelineLayout) self->pipeline_layout->handle,
      .renderPass = (VkRenderPass) self->render_pass->handle,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE
  };
  /* *INDENT-ON* */

  err =
      vkCreateGraphicsPipelines (self->queue->device->device, VK_NULL_HANDLE, 1,
      &pipeline_create_info, NULL, &pipeline);
  if (gst_vulkan_error_to_g_error (err, error, "vkCreateGraphicsPipelines") < 0) {
    return FALSE;
  }

  self->graphics_pipeline = gst_vulkan_handle_new_wrapped (self->queue->device,
      GST_VULKAN_HANDLE_TYPE_PIPELINE, (GstVulkanHandleTypedef) pipeline,
      gst_vulkan_handle_free_pipeline, NULL);

  return TRUE;
}

static gboolean
create_descriptor_pool (GstVulkanFullScreenQuad * self, GError ** error)
{
  GstVulkanFullScreenQuadPrivate *priv = GET_PRIV (self);
  VkDescriptorPoolCreateInfo pool_info;
  gsize max_sets = 32;          /* FIXME: don't hardcode this! */
  guint n_pools = 1;
  VkDescriptorPoolSize pool_sizes[2];
  VkDescriptorPool pool;
  GstVulkanDescriptorPool *ret;
  VkResult err;

  /* *INDENT-OFF* */
  pool_sizes[0] = (VkDescriptorPoolSize) {
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = max_sets * gst_buffer_n_memory (priv->inbuf),
  };

  if (priv->uniforms) {
    pool_sizes[1] = (VkDescriptorPoolSize) {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = max_sets
    };
    n_pools++;
  }

  pool_info = (VkDescriptorPoolCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .poolSizeCount = n_pools,
      .pPoolSizes = pool_sizes,
      .maxSets = max_sets
  };
  /* *INDENT-ON* */

  err =
      vkCreateDescriptorPool (self->queue->device->device, &pool_info, NULL,
      &pool);
  if (gst_vulkan_error_to_g_error (err, error, "vkCreateDescriptorPool") < 0) {
    return FALSE;
  }

  ret =
      gst_vulkan_descriptor_pool_new_wrapped (self->queue->device, pool,
      max_sets);
  self->descriptor_cache =
      gst_vulkan_descriptor_cache_new (ret, 1, &self->descriptor_set_layout);
  gst_object_unref (ret);

  return TRUE;
}

static gboolean
create_framebuffer (GstVulkanFullScreenQuad * self, GstVulkanImageView ** views,
    guint n_mems, GError ** error)
{
  VkImageView attachments[GST_VIDEO_MAX_PLANES] = { 0, };
  VkFramebufferCreateInfo framebuffer_info;
  VkFramebuffer framebuffer;
  VkResult err;
  int i;

  for (i = 0; i < n_mems; i++) {
    attachments[i] = views[i]->view;
  }

  /* *INDENT-OFF* */
  framebuffer_info = (VkFramebufferCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .pNext = NULL,
      .renderPass = (VkRenderPass) self->render_pass->handle,
      .attachmentCount = n_mems,
      .pAttachments = attachments,
      .width = GST_VIDEO_INFO_WIDTH (&self->out_info),
      .height = GST_VIDEO_INFO_HEIGHT (&self->out_info),
      .layers = 1
  };
  /* *INDENT-ON* */

  err =
      vkCreateFramebuffer (self->queue->device->device, &framebuffer_info, NULL,
      &framebuffer);
  if (gst_vulkan_error_to_g_error (err, error, "vkCreateFramebuffer") < 0) {
    return FALSE;
  }

  self->framebuffer = gst_vulkan_handle_new_wrapped (self->queue->device,
      GST_VULKAN_HANDLE_TYPE_FRAMEBUFFER, (GstVulkanHandleTypedef) framebuffer,
      gst_vulkan_handle_free_framebuffer, NULL);

  return TRUE;
}

#define LAST_FENCE_OR_ALWAYS_SIGNALLED(self,device) \
    self->last_fence ? gst_vulkan_fence_ref (self->last_fence) : gst_vulkan_fence_new_always_signalled (device)

GstVulkanFence *
gst_vulkan_full_screen_quad_get_last_fence (GstVulkanFullScreenQuad * self)
{
  g_return_val_if_fail (GST_IS_VULKAN_FULL_SCREEN_QUAD (self), NULL);

  return LAST_FENCE_OR_ALWAYS_SIGNALLED (self, self->queue->device);
}

#define clear_field(field,type,trash_free_func) \
static void \
G_PASTE(clear_,field) (GstVulkanFullScreenQuad * self) \
{ \
  GstVulkanFence *last_fence = \
      LAST_FENCE_OR_ALWAYS_SIGNALLED (self, self->queue->device); \
 \
  if (self->field) \
    gst_vulkan_trash_list_add (self->trash_list, \
        gst_vulkan_trash_list_acquire (self->trash_list, last_fence, \
            trash_free_func, (type) self->field)); \
  self->field = NULL; \
 \
  gst_vulkan_fence_unref (last_fence); \
}

#define clear_field_mini_object(field) clear_field (field,GstMiniObject *,gst_vulkan_trash_mini_object_unref);
#define clear_field_object(field) clear_field (field,GstObject *,gst_vulkan_trash_object_unref);

clear_field_mini_object (descriptor_set);
clear_field_mini_object (framebuffer);
clear_field_mini_object (sampler);
clear_field_mini_object (pipeline_layout);
clear_field_mini_object (graphics_pipeline);
clear_field_mini_object (descriptor_set_layout);
clear_field_object (cmd_pool);
clear_field_object (descriptor_cache);

static void
clear_shaders (GstVulkanFullScreenQuad * self)
{
  GstVulkanFullScreenQuadPrivate *priv = GET_PRIV (self);
  GstVulkanFence *last_fence =
      LAST_FENCE_OR_ALWAYS_SIGNALLED (self, self->queue->device);

  if (priv->vert)
    gst_vulkan_trash_list_add (self->trash_list,
        gst_vulkan_trash_list_acquire (self->trash_list, last_fence,
            gst_vulkan_trash_mini_object_unref, (GstMiniObject *) priv->vert));
  priv->vert = NULL;

  if (priv->frag)
    gst_vulkan_trash_list_add (self->trash_list,
        gst_vulkan_trash_list_acquire (self->trash_list, last_fence,
            gst_vulkan_trash_mini_object_unref, (GstMiniObject *) priv->frag));
  priv->frag = NULL;

  gst_vulkan_fence_unref (last_fence);
}

static void
clear_uniform_data (GstVulkanFullScreenQuad * self)
{
  GstVulkanFullScreenQuadPrivate *priv = GET_PRIV (self);
  GstVulkanFence *last_fence =
      LAST_FENCE_OR_ALWAYS_SIGNALLED (self, self->queue->device);

  if (priv->uniforms)
    gst_vulkan_trash_list_add (self->trash_list,
        gst_vulkan_trash_list_acquire (self->trash_list, last_fence,
            gst_vulkan_trash_mini_object_unref,
            (GstMiniObject *) priv->uniforms));
  priv->uniforms = NULL;
  priv->uniform_size = 0;

  gst_vulkan_fence_unref (last_fence);
}

static void
clear_index_data (GstVulkanFullScreenQuad * self)
{
  GstVulkanFullScreenQuadPrivate *priv = GET_PRIV (self);
  GstVulkanFence *last_fence =
      LAST_FENCE_OR_ALWAYS_SIGNALLED (self, self->queue->device);

  if (priv->indices)
    gst_vulkan_trash_list_add (self->trash_list,
        gst_vulkan_trash_list_acquire (self->trash_list, last_fence,
            gst_vulkan_trash_mini_object_unref,
            (GstMiniObject *) priv->indices));
  priv->indices = NULL;
  priv->n_indices = 0;

  gst_vulkan_fence_unref (last_fence);
}

static void
clear_vertex_data (GstVulkanFullScreenQuad * self)
{
  GstVulkanFullScreenQuadPrivate *priv = GET_PRIV (self);
  GstVulkanFence *last_fence =
      LAST_FENCE_OR_ALWAYS_SIGNALLED (self, self->queue->device);

  if (priv->vertices)
    gst_vulkan_trash_list_add (self->trash_list,
        gst_vulkan_trash_list_acquire (self->trash_list, last_fence,
            gst_vulkan_trash_mini_object_unref,
            (GstMiniObject *) priv->vertices));
  priv->vertices = NULL;

  gst_vulkan_fence_unref (last_fence);
}

static void
clear_render_pass (GstVulkanFullScreenQuad * self)
{
  GstVulkanFence *last_fence =
      LAST_FENCE_OR_ALWAYS_SIGNALLED (self, self->queue->device);

  if (self->render_pass)
    gst_vulkan_trash_list_add (self->trash_list,
        gst_vulkan_trash_list_acquire (self->trash_list, last_fence,
            gst_vulkan_trash_mini_object_unref,
            (GstMiniObject *) self->render_pass));
  self->render_pass = NULL;

  gst_vulkan_fence_unref (last_fence);
}

static void
destroy_pipeline (GstVulkanFullScreenQuad * self)
{
  GstVulkanFence *last_fence =
      LAST_FENCE_OR_ALWAYS_SIGNALLED (self, self->queue->device);

  clear_render_pass (self);
  clear_pipeline_layout (self);
  clear_graphics_pipeline (self);
  clear_descriptor_set_layout (self);

  gst_vulkan_fence_unref (last_fence);

  gst_vulkan_trash_list_gc (self->trash_list);
}

void
gst_vulkan_full_screen_quad_init (GstVulkanFullScreenQuad * self)
{
  GstVulkanFullScreenQuadPrivate *priv = GET_PRIV (self);

  self->trash_list = gst_vulkan_trash_fence_list_new ();

  priv->src_blend_factor = VK_BLEND_FACTOR_ONE;
  priv->src_alpha_blend_factor = VK_BLEND_FACTOR_ONE;
  priv->dst_blend_factor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  priv->dst_alpha_blend_factor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  priv->colour_blend_op = VK_BLEND_OP_ADD;
  priv->alpha_blend_op = VK_BLEND_OP_ADD;
  priv->enable_clear = TRUE;
}

/**
 * gst_vulkan_full_screen_quad_new:
 * @queue: a #GstVulkanQueue
 *
 * Returns: (transfer full): a new #GstVulkanFullScreenQuad
 *
 * Since: 1.18
 */
GstVulkanFullScreenQuad *
gst_vulkan_full_screen_quad_new (GstVulkanQueue * queue)
{
  GstVulkanFullScreenQuad *self;
  GError *error = NULL;

  g_return_val_if_fail (GST_IS_VULKAN_QUEUE (queue), NULL);

  self = g_object_new (GST_TYPE_VULKAN_FULL_SCREEN_QUAD, NULL);
  self->queue = gst_object_ref (queue);
  self->cmd_pool = gst_vulkan_queue_create_command_pool (queue, &error);
  if (!self->cmd_pool)
    GST_WARNING_OBJECT (self, "Failed to create command pool: %s",
        error->message);

  gst_object_ref_sink (self);

  return self;
}

static void
gst_vulkan_full_screen_quad_finalize (GObject * object)
{
  GstVulkanFullScreenQuad *self = GST_VULKAN_FULL_SCREEN_QUAD (object);
  GstVulkanFullScreenQuadPrivate *priv = GET_PRIV (self);

  destroy_pipeline (self);
  clear_cmd_pool (self);
  clear_sampler (self);
  clear_framebuffer (self);
  clear_descriptor_set (self);
  clear_descriptor_cache (self);
  clear_shaders (self);
  clear_uniform_data (self);
  clear_index_data (self);
  clear_vertex_data (self);

  gst_vulkan_trash_list_wait (self->trash_list, -1);
  gst_vulkan_trash_list_gc (self->trash_list);
  gst_clear_object (&self->trash_list);

  gst_clear_mini_object (((GstMiniObject **) & self->last_fence));

  gst_clear_object (&self->queue);

  gst_clear_buffer (&priv->inbuf);
  gst_clear_buffer (&priv->outbuf);

  G_OBJECT_CLASS (gst_vulkan_full_screen_quad_parent_class)->finalize (object);
}

static void
gst_vulkan_full_screen_quad_class_init (GstVulkanFullScreenQuadClass * klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  obj_class->finalize = gst_vulkan_full_screen_quad_finalize;
}

/**
 * gst_vulkan_full_screen_quad_set_info:
 * @self: the #GstVulkanFullScreenQuad
 * @in_info: the input #GstVideoInfo to set
 * @out_info: the output #GstVideoInfo to set
 *
 * Returns: whether the information could be successfully set
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_full_screen_quad_set_info (GstVulkanFullScreenQuad * self,
    GstVideoInfo * in_info, GstVideoInfo * out_info)
{
  self->out_info = *out_info;
  self->in_info = *in_info;

  destroy_pipeline (self);
  clear_framebuffer (self);
  clear_descriptor_set (self);
  clear_descriptor_cache (self);
  clear_uniform_data (self);

  return TRUE;
}

/**
 * gst_vulkan_full_screen_quad_set_input_buffer:
 * @self: the #GstVulkanFullScreenQuad
 * @buffer: the input #GstBuffer to set
 * @error: #GError to fill on failure
 *
 * Returns: whether the input buffer could be changed
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_full_screen_quad_set_input_buffer (GstVulkanFullScreenQuad * self,
    GstBuffer * buffer, GError ** error)
{
  GstVulkanFullScreenQuadPrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_FULL_SCREEN_QUAD (self), FALSE);

  priv = GET_PRIV (self);

  gst_buffer_replace (&priv->inbuf, buffer);
  clear_descriptor_set (self);
  return TRUE;
}

/**
 * gst_vulkan_full_screen_quad_set_output_buffer:
 * @self: the #GstVulkanFullScreenQuad
 * @buffer: the output #GstBuffer to set
 * @error: #GError to fill on failure
 *
 * Returns: whether the input buffer could be changed
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_full_screen_quad_set_output_buffer (GstVulkanFullScreenQuad * self,
    GstBuffer * buffer, GError ** error)
{
  GstVulkanFullScreenQuadPrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_FULL_SCREEN_QUAD (self), FALSE);

  priv = GET_PRIV (self);

  gst_buffer_replace (&priv->outbuf, buffer);
  clear_framebuffer (self);
  return TRUE;
}

/**
 * gst_vulkan_full_screen_quad_set_shaders:
 * @self: the #GstVulkanFullScreenQuad
 * @vert: the vertex shader to set
 * @frag: the fragment shader to set
 *
 * Returns: whether the shaders could be set
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_full_screen_quad_set_shaders (GstVulkanFullScreenQuad * self,
    GstVulkanHandle * vert, GstVulkanHandle * frag)
{
  GstVulkanFullScreenQuadPrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_FULL_SCREEN_QUAD (self), FALSE);
  g_return_val_if_fail (vert != NULL, FALSE);
  g_return_val_if_fail (vert->type == GST_VULKAN_HANDLE_TYPE_SHADER, FALSE);
  g_return_val_if_fail (frag != NULL, FALSE);
  g_return_val_if_fail (frag->type == GST_VULKAN_HANDLE_TYPE_SHADER, FALSE);

  priv = GET_PRIV (self);

  clear_shaders (self);
  destroy_pipeline (self);

  priv->vert = gst_vulkan_handle_ref (vert);
  priv->frag = gst_vulkan_handle_ref (frag);

  return TRUE;
}

/**
 * gst_vulkan_full_screen_quad_set_uniform_buffer:
 * @self: the #GstVulkanFullScreenQuad
 * @uniforms: the uniform data to set. Must be a #GstVulkanBufferMemory
 * @error: a #GError to fill on failure
 *
 * Returns: whether the shaders could be set
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_full_screen_quad_set_uniform_buffer (GstVulkanFullScreenQuad * self,
    GstMemory * uniforms, GError ** error)
{
  GstVulkanFullScreenQuadPrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_FULL_SCREEN_QUAD (self), FALSE);
  g_return_val_if_fail (uniforms == NULL
      || gst_is_vulkan_buffer_memory (uniforms), FALSE);

  priv = GET_PRIV (self);

  clear_uniform_data (self);
  if (uniforms) {
    priv->uniforms = gst_memory_ref (uniforms);
    priv->uniform_size = gst_memory_get_sizes (uniforms, NULL, NULL);
  }

  return TRUE;
}

/**
 * gst_vulkan_full_screen_quad_set_index_buffer:
 * @self: the #GstVulkanFullScreenQuad
 * @indices: the index data.  Must be a #GstVulkanBufferMemory
 * @n_indices: number of indices in @indices
 * @error: #GError to fill on failure
 *
 * See also gst_vulkan_full_screen_quad_set_vertex_buffer()
 *
 * Returns: whether the index data could be set
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_full_screen_quad_set_index_buffer (GstVulkanFullScreenQuad * self,
    GstMemory * indices, gsize n_indices, GError ** error)
{
  GstVulkanFullScreenQuadPrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_FULL_SCREEN_QUAD (self), FALSE);
  g_return_val_if_fail (indices == NULL
      || gst_is_vulkan_buffer_memory (indices), FALSE);

  priv = GET_PRIV (self);

  clear_index_data (self);
  if (indices) {
    priv->indices = gst_memory_ref (indices);
    priv->n_indices = n_indices;
  }

  return TRUE;
}

/**
 * gst_vulkan_full_screen_quad_set_vertex_buffer:
 * @self: the #GstVulkanFullScreenQuad
 * @vertices: the vertex data. Must be a #GstVulkanBufferMemory
 * @error: #GError to fill on failure
 *
 * Returns: whether the index data could be set
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_full_screen_quad_set_vertex_buffer (GstVulkanFullScreenQuad * self,
    GstMemory * vertices, GError ** error)
{
  GstVulkanFullScreenQuadPrivate *priv;

  g_return_val_if_fail (GST_IS_VULKAN_FULL_SCREEN_QUAD (self), FALSE);
  g_return_val_if_fail (vertices == NULL
      || gst_is_vulkan_buffer_memory (vertices), FALSE);

  priv = GET_PRIV (self);

  clear_vertex_data (self);
  if (vertices) {
    priv->vertices = gst_memory_ref (vertices);
  }

  return TRUE;
}

static GstVulkanImageMemory *
peek_image_from_buffer (GstBuffer * buffer, guint i)
{
  GstMemory *mem = gst_buffer_peek_memory (buffer, i);
  g_return_val_if_fail (gst_is_vulkan_image_memory (mem), NULL);
  return (GstVulkanImageMemory *) mem;
}

static gboolean
ensure_vertex_data (GstVulkanFullScreenQuad * self, GError ** error)
{
  GstVulkanFullScreenQuadPrivate *priv = GET_PRIV (self);
  GstMapInfo map_info;

  if (!priv->vertices) {
    priv->vertices = gst_vulkan_buffer_memory_alloc (self->queue->device,
        sizeof (vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (!gst_memory_map (priv->vertices, &map_info, GST_MAP_WRITE)) {
      g_set_error_literal (error, GST_VULKAN_ERROR, VK_ERROR_MEMORY_MAP_FAILED,
          "Failed to map memory");
      goto failure;
    }

    memcpy (map_info.data, vertices, map_info.size);
    gst_memory_unmap (priv->vertices, &map_info);
  }

  if (!priv->indices) {
    priv->indices = gst_vulkan_buffer_memory_alloc (self->queue->device,
        sizeof (indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (!gst_memory_map (priv->indices, &map_info, GST_MAP_WRITE)) {
      g_set_error_literal (error, GST_VULKAN_ERROR, VK_ERROR_MEMORY_MAP_FAILED,
          "Failed to map memory");
      goto failure;
    }

    memcpy (map_info.data, indices, map_info.size);
    gst_memory_unmap (priv->indices, &map_info);

    priv->n_indices = G_N_ELEMENTS (indices);
  }

  return TRUE;

failure:
  if (priv->vertices)
    gst_memory_unref (priv->vertices);
  priv->vertices = NULL;
  if (priv->indices)
    gst_memory_unref (priv->indices);
  priv->indices = NULL;
  priv->n_indices = 0;
  return FALSE;
}

/**
 * gst_vulkan_full_screen_quad_draw:
 * @self: the #GstVulkanFullScreenQuad
 * @error: a #GError filled on error
 *
 * Helper function for creation and submission of a command buffer that draws
 * a full screen quad.  If you need to add other things to the command buffer,
 * create the command buffer manually and call
 * gst_vulkan_full_screen_quad_prepare_draw(),
 * gst_vulkan_full_screen_quad_fill_command_buffer() and
 * gst_vulkan_full_screen_quad_submit() instead.
 *
 * Returns: whether the draw was successful
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_full_screen_quad_draw (GstVulkanFullScreenQuad * self,
    GError ** error)
{
  GstVulkanCommandBuffer *cmd = NULL;
  GstVulkanFence *fence = NULL;
  VkResult err;

  g_return_val_if_fail (GST_IS_VULKAN_FULL_SCREEN_QUAD (self), FALSE);

  fence = gst_vulkan_device_create_fence (self->queue->device, error);
  if (!fence)
    goto error;

  if (!gst_vulkan_full_screen_quad_prepare_draw (self, fence, error))
    goto error;

  if (!(cmd = gst_vulkan_command_pool_create (self->cmd_pool, error)))
    goto error;

  {
    VkCommandBufferBeginInfo cmd_buf_info = { 0, };

    /* *INDENT-OFF* */
    cmd_buf_info = (VkCommandBufferBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL
    };
    /* *INDENT-ON* */

    gst_vulkan_command_buffer_lock (cmd);
    err = vkBeginCommandBuffer (cmd->cmd, &cmd_buf_info);
    if (gst_vulkan_error_to_g_error (err, error, "vkBeginCommandBuffer") < 0)
      goto unlock_error;
  }

  if (!gst_vulkan_full_screen_quad_fill_command_buffer (self, cmd, fence,
          error))
    goto unlock_error;

  err = vkEndCommandBuffer (cmd->cmd);
  gst_vulkan_command_buffer_unlock (cmd);
  if (gst_vulkan_error_to_g_error (err, error, "vkEndCommandBuffer") < 0)
    goto error;

  if (!gst_vulkan_full_screen_quad_submit (self, cmd, fence, error))
    goto error;

  gst_vulkan_fence_unref (fence);

  return TRUE;

unlock_error:
  gst_vulkan_command_buffer_unlock (cmd);

error:
  gst_clear_mini_object ((GstMiniObject **) & cmd);
  gst_clear_mini_object ((GstMiniObject **) & fence);
  return FALSE;
}

/**
 * gst_vulkan_full_screen_quad_enable_blend:
 * @self: the #GstVulkanFullScreenQuad
 * @enable_blend: whether to enable blending
 *
 * Enables blending of the input image to the output image.
 *
 * See also: gst_vulkan_full_screen_quad_set_blend_operation() and
 * gst_vulkan_full_screen_quad_set_blend_factors().
 *
 * Since: 1.22
 */
void
gst_vulkan_full_screen_quad_enable_blend (GstVulkanFullScreenQuad * self,
    gboolean enable_blend)
{
  GstVulkanFullScreenQuadPrivate *priv;

  g_return_if_fail (GST_IS_VULKAN_FULL_SCREEN_QUAD (self));

  priv = GET_PRIV (self);

  if (priv->blend_enable == VK_TRUE && enable_blend)
    return;
  if (priv->blend_enable == VK_FALSE && !enable_blend)
    return;
  priv->blend_enable = enable_blend ? VK_TRUE : VK_FALSE;

  clear_graphics_pipeline (self);
}

/**
 * gst_vulkan_full_screen_quad_set_blend_factors:
 * @self: the #GstVulkanFullScreenQuad
 * @src_blend_factor: the `VkBlendFactor` for the source image for the colour
 *                    components (RGB)
 * @src_alpha_blend_factor: the `VkBlendFactor` for the source image for the
 *                          alpha component.
 * @dst_blend_factor: the `VkBlendFactor` for the destination image for the
 *                    colour components (RGB)
 * @dst_alpha_blend_factor: the `VkBlendFactor` for the destination image for
 *                          the alpha component.
 *
 * You need to enable blend with gst_vulkan_full_screen_quad_enable_blend().
 *
 * See also: gst_vulkan_full_screen_quad_set_blend_operation().
 *
 * Since: 1.22
 */
void
gst_vulkan_full_screen_quad_set_blend_factors (GstVulkanFullScreenQuad * self,
    VkBlendFactor src_blend_factor, VkBlendFactor dst_blend_factor,
    VkBlendFactor src_alpha_blend_factor, VkBlendFactor dst_alpha_blend_factor)
{
  GstVulkanFullScreenQuadPrivate *priv;

  g_return_if_fail (GST_IS_VULKAN_FULL_SCREEN_QUAD (self));

  priv = GET_PRIV (self);

  if (priv->src_blend_factor == src_blend_factor
      && priv->src_alpha_blend_factor == src_alpha_blend_factor
      && priv->dst_blend_factor == dst_blend_factor
      && priv->dst_alpha_blend_factor == dst_alpha_blend_factor)
    return;

  priv->src_blend_factor = src_blend_factor;
  priv->src_alpha_blend_factor = src_alpha_blend_factor;
  priv->dst_blend_factor = dst_blend_factor;
  priv->dst_alpha_blend_factor = dst_alpha_blend_factor;

  clear_graphics_pipeline (self);
}

/**
 * gst_vulkan_full_screen_quad_set_blend_operation:
 * @self: the #GstVulkanFullScreenQuad
 * @colour_blend_op: the `VkBlendOp` to use for blending colour (RGB) values
 * @alpha_blend_op: the `VkBlendOp` to use for blending alpha values
 *
 * You need to enable blend with gst_vulkan_full_screen_quad_enable_blend().
 *
 * See also: gst_vulkan_full_screen_quad_set_blend_factors().
 *
 * Since: 1.22
 */
void
gst_vulkan_full_screen_quad_set_blend_operation (GstVulkanFullScreenQuad * self,
    VkBlendOp colour_blend_op, VkBlendOp alpha_blend_op)
{
  GstVulkanFullScreenQuadPrivate *priv;

  g_return_if_fail (GST_IS_VULKAN_FULL_SCREEN_QUAD (self));

  priv = GET_PRIV (self);

  priv->colour_blend_op = colour_blend_op;
  priv->alpha_blend_op = alpha_blend_op;

  clear_graphics_pipeline (self);
}

/**
 * gst_vulkan_full_screen_quad_enable_clear:
 * @self: the #GstVulkanFullScreenQuad
 * @enable_clear: whether to clear the framebuffer on load
 *
 * Since: 1.22
 */
void
gst_vulkan_full_screen_quad_enable_clear (GstVulkanFullScreenQuad * self,
    gboolean enable_clear)
{
  GstVulkanFullScreenQuadPrivate *priv;

  g_return_if_fail (GST_IS_VULKAN_FULL_SCREEN_QUAD (self));

  priv = GET_PRIV (self);

  if (priv->enable_clear == enable_clear)
    return;

  priv->enable_clear = enable_clear;

  clear_graphics_pipeline (self);
  clear_render_pass (self);
}

/**
 * gst_vulkan_full_screen_quad_prepare_draw:
 * @self: the #GstVulkanFullScreenQuad
 * @fence: a #GstVulkanFence that will be signalled after submission
 * @error: a #GError filled on error
 *
 * Returns: whether the necessary information could be generated for drawing a
 * frame.
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_full_screen_quad_prepare_draw (GstVulkanFullScreenQuad * self,
    GstVulkanFence * fence, GError ** error)
{
  GstVulkanFullScreenQuadPrivate *priv;
  GstVulkanImageView *in_views[GST_VIDEO_MAX_PLANES] = { NULL, };
  GstVulkanImageView *out_views[GST_VIDEO_MAX_PLANES] = { NULL, };
  int i, n_mems;

  g_return_val_if_fail (GST_IS_VULKAN_FULL_SCREEN_QUAD (self), FALSE);
  g_return_val_if_fail (fence != NULL, FALSE);

  priv = GET_PRIV (self);

  if (!self->graphics_pipeline)
    if (!create_pipeline (self, error))
      return FALSE;

  if (!ensure_vertex_data (self, error))
    goto error;

  if (!self->descriptor_cache)
    if (!create_descriptor_pool (self, error))
      goto error;

  if (!self->descriptor_set) {
    n_mems = gst_buffer_n_memory (priv->inbuf);
    for (i = 0; i < n_mems; i++) {
      GstVulkanImageMemory *img_mem = peek_image_from_buffer (priv->inbuf, i);
      if (!gst_is_vulkan_image_memory ((GstMemory *) img_mem)) {
        g_set_error_literal (error, GST_VULKAN_ERROR, GST_VULKAN_FAILED,
            "Input memory must be a GstVulkanImageMemory");
        goto error;
      }
      in_views[i] = gst_vulkan_get_or_create_image_view (img_mem);
      gst_vulkan_trash_list_add (self->trash_list,
          gst_vulkan_trash_list_acquire (self->trash_list, fence,
              gst_vulkan_trash_mini_object_unref,
              (GstMiniObject *) in_views[i]));
    }
    if (!(self->descriptor_set =
            get_and_update_descriptor_set (self, in_views, n_mems, error)))
      goto error;
  }

  if (!self->framebuffer) {
    n_mems = gst_buffer_n_memory (priv->outbuf);
    for (i = 0; i < n_mems; i++) {
      GstVulkanImageMemory *img_mem = peek_image_from_buffer (priv->outbuf, i);
      if (!gst_is_vulkan_image_memory ((GstMemory *) img_mem)) {
        g_set_error_literal (error, GST_VULKAN_ERROR, GST_VULKAN_FAILED,
            "Output memory must be a GstVulkanImageMemory");
        goto error;
      }
      out_views[i] = gst_vulkan_get_or_create_image_view (img_mem);
      gst_vulkan_trash_list_add (self->trash_list,
          gst_vulkan_trash_list_acquire (self->trash_list, fence,
              gst_vulkan_trash_mini_object_unref,
              (GstMiniObject *) out_views[i]));
    }
    if (!create_framebuffer (self, out_views, n_mems, error))
      goto error;
  }

  if (!self->cmd_pool)
    if (!(self->cmd_pool =
            gst_vulkan_queue_create_command_pool (self->queue, error)))
      goto error;

  return TRUE;

error:
  for (i = 0; i < GST_VIDEO_MAX_PLANES; i++)
    gst_clear_mini_object ((GstMiniObject **) & in_views[i]);
  for (i = 0; i < GST_VIDEO_MAX_PLANES; i++)
    gst_clear_mini_object ((GstMiniObject **) & out_views[i]);
  return FALSE;
}

/**
 * gst_vulkan_full_screen_quad_fill_command_buffer:
 * @self: a #GstVulkanFullScreenQuad
 * @cmd: the #GstVulkanCommandBuffer to fill with commands
 * @error: a #GError to fill on error
 *
 * Returns: whether @cmd could be filled with the necessary commands
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_full_screen_quad_fill_command_buffer (GstVulkanFullScreenQuad * self,
    GstVulkanCommandBuffer * cmd, GstVulkanFence * fence, GError ** error)
{
  GstVulkanFullScreenQuadPrivate *priv;
  GstVulkanImageView *in_views[GST_VIDEO_MAX_PLANES] = { NULL, };
  GstVulkanImageView *out_views[GST_VIDEO_MAX_PLANES] = { NULL, };
  int i;
  guint n_in_mems, n_out_mems;

  g_return_val_if_fail (GST_IS_VULKAN_FULL_SCREEN_QUAD (self), FALSE);
  g_return_val_if_fail (cmd != NULL, FALSE);
  g_return_val_if_fail (fence != NULL, FALSE);

  priv = GET_PRIV (self);

  n_in_mems = gst_buffer_n_memory (priv->inbuf);
  for (i = 0; i < n_in_mems; i++) {
    GstVulkanImageMemory *img_mem = peek_image_from_buffer (priv->inbuf, i);
    if (!gst_is_vulkan_image_memory ((GstMemory *) img_mem)) {
      g_set_error_literal (error, GST_VULKAN_ERROR, GST_VULKAN_FAILED,
          "Input memory must be a GstVulkanImageMemory");
      goto error;
    }
    in_views[i] = gst_vulkan_get_or_create_image_view (img_mem);
    gst_vulkan_trash_list_add (self->trash_list,
        gst_vulkan_trash_list_acquire (self->trash_list, fence,
            gst_vulkan_trash_mini_object_unref, (GstMiniObject *) in_views[i]));
  }
  n_out_mems = gst_buffer_n_memory (priv->outbuf);
  for (i = 0; i < n_out_mems; i++) {
    GstVulkanImageMemory *img_mem = peek_image_from_buffer (priv->outbuf, i);
    if (!gst_is_vulkan_image_memory ((GstMemory *) img_mem)) {
      g_set_error_literal (error, GST_VULKAN_ERROR, GST_VULKAN_FAILED,
          "Output memory must be a GstVulkanImageMemory");
      goto error;
    }
    out_views[i] = gst_vulkan_get_or_create_image_view (img_mem);
    gst_vulkan_trash_list_add (self->trash_list,
        gst_vulkan_trash_list_acquire (self->trash_list, fence,
            gst_vulkan_trash_mini_object_unref,
            (GstMiniObject *) out_views[i]));
  }

  for (i = 0; i < n_in_mems; i++) {
    /* *INDENT-OFF* */
    VkImageMemoryBarrier in_image_memory_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = in_views[i]->image->barrier.parent.access_flags,
        .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
        .oldLayout = in_views[i]->image->barrier.image_layout,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        /* FIXME: implement exclusive transfers */
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = in_views[i]->image->image,
        .subresourceRange = in_views[i]->image->barrier.subresource_range
    };
    /* *INDENT-ON* */

    vkCmdPipelineBarrier (cmd->cmd,
        in_views[i]->image->barrier.parent.pipeline_stages,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1,
        &in_image_memory_barrier);

    in_views[i]->image->barrier.parent.pipeline_stages =
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    in_views[i]->image->barrier.parent.access_flags =
        in_image_memory_barrier.dstAccessMask;
    in_views[i]->image->barrier.image_layout =
        in_image_memory_barrier.newLayout;
  }

  for (i = 0; i < n_out_mems; i++) {
    /* *INDENT-OFF* */
    VkImageMemoryBarrier out_image_memory_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = out_views[i]->image->barrier.parent.access_flags,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = out_views[i]->image->barrier.image_layout,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        /* FIXME: implement exclusive transfers */
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = out_views[i]->image->image,
        .subresourceRange = out_views[i]->image->barrier.subresource_range
    };
    /* *INDENT-ON* */

    vkCmdPipelineBarrier (cmd->cmd,
        out_views[i]->image->barrier.parent.pipeline_stages,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1,
        &out_image_memory_barrier);

    out_views[i]->image->barrier.parent.pipeline_stages =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    out_views[i]->image->barrier.parent.access_flags =
        out_image_memory_barrier.dstAccessMask;
    out_views[i]->image->barrier.image_layout =
        out_image_memory_barrier.newLayout;
  }

  {
    /* *INDENT-OFF* */
    VkClearValue clearColor = {{{ 0.0f, 0.0f, 0.0f, 1.0f }}};
    VkClearValue clearColors[GST_VIDEO_MAX_PLANES] = {
      clearColor, clearColor, clearColor, clearColor,
    };
    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = (VkRenderPass) self->render_pass->handle,
        .framebuffer = (VkFramebuffer) self->framebuffer->handle,
        .renderArea.offset = { 0, 0 },
        .renderArea.extent = {
            GST_VIDEO_INFO_WIDTH (&self->out_info),
            GST_VIDEO_INFO_HEIGHT (&self->out_info)
        },
        .clearValueCount = n_out_mems,
        .pClearValues = clearColors,
    };
    /* *INDENT-ON* */
    VkDeviceSize offsets[] = { 0 };

    vkCmdBindDescriptorSets (cmd->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        (VkPipelineLayout) self->pipeline_layout->handle, 0, 1,
        &self->descriptor_set->set, 0, NULL);

    vkCmdBeginRenderPass (cmd->cmd, &render_pass_info,
        VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline (cmd->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        (VkPipeline) self->graphics_pipeline->handle);
    vkCmdBindVertexBuffers (cmd->cmd, 0, 1,
        &((GstVulkanBufferMemory *) priv->vertices)->buffer, offsets);
    vkCmdBindIndexBuffer (cmd->cmd,
        ((GstVulkanBufferMemory *) priv->indices)->buffer, 0,
        VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed (cmd->cmd, priv->n_indices, 1, 0, 0, 0);
    vkCmdEndRenderPass (cmd->cmd);
  }

  return TRUE;

error:
  return FALSE;
}

/**
 * gst_vulkan_full_screen_quad_submit:
 * @self: a #GstVulkanFullScreenQuad
 * @cmd: (transfer full): a #GstVulkanCommandBuffer to submit
 * @fence: a #GstVulkanFence to signal on completion
 * @error: a #GError to fill on error
 *
 * Returns: whether @cmd could be submitted to the queue
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_full_screen_quad_submit (GstVulkanFullScreenQuad * self,
    GstVulkanCommandBuffer * cmd, GstVulkanFence * fence, GError ** error)
{
  VkResult err;

  g_return_val_if_fail (GST_IS_VULKAN_FULL_SCREEN_QUAD (self), FALSE);
  g_return_val_if_fail (cmd != NULL, FALSE);
  g_return_val_if_fail (fence != NULL, FALSE);

  {
    /* *INDENT-OFF* */
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd->cmd,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
    };
    /* *INDENT-ON* */

    gst_vulkan_queue_submit_lock (self->queue);
    err =
        vkQueueSubmit (self->queue->queue, 1, &submit_info,
        GST_VULKAN_FENCE_FENCE (fence));
    gst_vulkan_queue_submit_unlock (self->queue);
    if (gst_vulkan_error_to_g_error (err, error, "vkQueueSubmit") < 0)
      goto error;
  }

  gst_vulkan_trash_list_add (self->trash_list,
      gst_vulkan_trash_list_acquire (self->trash_list, fence,
          gst_vulkan_trash_mini_object_unref, GST_MINI_OBJECT_CAST (cmd)));

  gst_vulkan_trash_list_gc (self->trash_list);

  if (self->last_fence)
    gst_vulkan_fence_unref (self->last_fence);
  self->last_fence = gst_vulkan_fence_ref (fence);

  return TRUE;

error:
  return FALSE;
}
