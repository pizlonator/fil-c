/*
 * Copyright © 2022 Collabora, LTD
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef VK_LIMITS_H
#define VK_LIMITS_H

/* Maximun number of shader stages in a single graphics pipeline */
#define MESA_VK_MAX_GRAPHICS_PIPELINE_STAGES 5

#define MESA_VK_MAX_DESCRIPTOR_SETS 32

/* From the Vulkan 1.3.274 spec:
 *
 *    VUID-VkPipelineLayoutCreateInfo-pPushConstantRanges-00292
 *
 *    "Any two elements of pPushConstantRanges must not include the same
 *    stage in stageFlags"
 *
 * and
 *
 *    VUID-VkPushConstantRange-stageFlags-requiredbitmask
 *
 *    "stageFlags must not be 0"
 *
 * This means that the number of push constant ranges is effectively bounded
 * by the number of possible shader stages.  Not the number of stages that can
 * be compiled together (a pipeline layout can be used in multiple pipelnes
 * wth different sets of shaders) but the total number of stage bits supported
 * by the implementation.  Currently, those are
 *
 *    - VK_SHADER_STAGE_VERTEX_BIT
 *    - VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
 *    - VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
 *    - VK_SHADER_STAGE_GEOMETRY_BIT
 *    - VK_SHADER_STAGE_FRAGMENT_BIT
 *    - VK_SHADER_STAGE_COMPUTE_BIT
 *    - VK_SHADER_STAGE_RAYGEN_BIT_KHR
 *    - VK_SHADER_STAGE_ANY_HIT_BIT_KHR
 *    - VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
 *    - VK_SHADER_STAGE_MISS_BIT_KHR
 *    - VK_SHADER_STAGE_INTERSECTION_BIT_KHR
 *    - VK_SHADER_STAGE_CALLABLE_BIT_KHR
 *    - VK_SHADER_STAGE_TASK_BIT_EXT
 *    - VK_SHADER_STAGE_MESH_BIT_EXT
 */
#define MESA_VK_MAX_PUSH_CONSTANT_RANGES 14

#define MESA_VK_MAX_VERTEX_BINDINGS 32
#define MESA_VK_MAX_VERTEX_ATTRIBUTES 32

/* As of June 29, 2022, according to vulkan.gpuinfo.org, 99% of all reports
 * listed a max vertex stride that fits in 16 bits.
 */
#define MESA_VK_MAX_VERTEX_BINDING_STRIDE UINT16_MAX

#define MESA_VK_MAX_VIEWPORTS 16
#define MESA_VK_MAX_SCISSORS 16
#define MESA_VK_MAX_DISCARD_RECTANGLES 4

/* As of June 29, 2022, according to vulkan.gpuinfo.org, no reports list more
 * than 16 samples for framebufferColorSampleCounts except one layer running
 * on top of WARP on Windows.
 */
#define MESA_VK_MAX_SAMPLES 16

/* As of June 29, 2022, according to vulkan.gpuinfo.org, the only GPUs
 * claiming support for maxSampleLocationGridSize greater than 1x1 is AMD
 * which supports 2x2 but only up to 8 samples.
 */
#define MESA_VK_MAX_SAMPLE_LOCATIONS 32

#define MESA_VK_MAX_COLOR_ATTACHMENTS 8

/* Since VkSubpassDescription2::viewMask is a 32-bit integer, there are a
 * maximum of 32 possible views.
 */
#define MESA_VK_MAX_MULTIVIEW_VIEW_COUNT 32

#endif /* VK_LIMITS_H */
