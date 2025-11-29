/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RADV_PIPELINE_CACHE_H
#define RADV_PIPELINE_CACHE_H

#include "util/mesa-blake3.h"

#include "nir.h"

#include "vk_pipeline_cache.h"

struct radv_device;
struct radv_graphics_state_key;
struct radv_pipeline;
struct radv_pipeline_layout;
struct radv_ray_tracing_group;
struct radv_ray_tracing_pipeline;
struct radv_graphics_pipeline;
struct radv_compute_pipeline;
struct radv_ray_tracing_stage;
struct radv_shader_binary;
struct radv_shader_stage;
struct radv_spirv_to_nir_options;

void radv_hash_shaders(const struct radv_device *device, unsigned char *hash, const struct radv_shader_stage *stages,
                       uint32_t stage_count, const struct radv_pipeline_layout *layout,
                       const struct radv_graphics_state_key *gfx_state);

void radv_hash_graphics_spirv_to_nir(blake3_hash hash, const struct radv_shader_stage *stage,
                                     const struct radv_spirv_to_nir_options *options);

void radv_hash_rt_shaders(const struct radv_device *device, unsigned char *hash,
                          const struct radv_ray_tracing_stage *stages,
                          const VkRayTracingPipelineCreateInfoKHR *pCreateInfo,
                          const struct radv_ray_tracing_group *groups);

struct radv_shader *radv_shader_create(struct radv_device *device, struct vk_pipeline_cache *cache,
                                       const struct radv_shader_binary *binary, bool skip_cache);

bool radv_graphics_pipeline_cache_search(struct radv_device *device, struct vk_pipeline_cache *cache,
                                         struct radv_graphics_pipeline *pipeline, bool *found_in_application_cache);

bool radv_compute_pipeline_cache_search(struct radv_device *device, struct vk_pipeline_cache *cache,
                                        struct radv_compute_pipeline *pipeline, bool *found_in_application_cache);

void radv_pipeline_cache_insert(struct radv_device *device, struct vk_pipeline_cache *cache,
                                struct radv_pipeline *pipeline);

bool radv_ray_tracing_pipeline_cache_search(struct radv_device *device, struct vk_pipeline_cache *cache,
                                            struct radv_ray_tracing_pipeline *pipeline,
                                            bool *found_in_application_cache);

void radv_ray_tracing_pipeline_cache_insert(struct radv_device *device, struct vk_pipeline_cache *cache,
                                            struct radv_ray_tracing_pipeline *pipeline, unsigned num_stages);

nir_shader *radv_pipeline_cache_lookup_nir(struct radv_device *device, struct vk_pipeline_cache *cache,
                                           gl_shader_stage stage, const blake3_hash key);

void radv_pipeline_cache_insert_nir(struct radv_device *device, struct vk_pipeline_cache *cache, const blake3_hash key,
                                    const nir_shader *nir);

struct vk_pipeline_cache_object *radv_pipeline_cache_lookup_nir_handle(struct radv_device *device,
                                                                       struct vk_pipeline_cache *cache,
                                                                       const unsigned char *sha1);

struct nir_shader *radv_pipeline_cache_handle_to_nir(struct radv_device *device,
                                                     struct vk_pipeline_cache_object *object);

struct vk_pipeline_cache_object *radv_pipeline_cache_nir_to_handle(struct radv_device *device,
                                                                   struct vk_pipeline_cache *cache,
                                                                   struct nir_shader *nir, const unsigned char *sha1,
                                                                   bool cached);

#endif /* RADV_PIPELINE_CACHE_H */
