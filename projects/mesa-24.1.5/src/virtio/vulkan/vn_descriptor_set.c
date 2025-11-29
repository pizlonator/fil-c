/*
 * Copyright 2019 Google LLC
 * SPDX-License-Identifier: MIT
 *
 * based in part on anv and radv which are:
 * Copyright © 2015 Intel Corporation
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 */

#include "vn_descriptor_set.h"

#include "venus-protocol/vn_protocol_driver_descriptor_pool.h"
#include "venus-protocol/vn_protocol_driver_descriptor_set.h"
#include "venus-protocol/vn_protocol_driver_descriptor_set_layout.h"
#include "venus-protocol/vn_protocol_driver_descriptor_update_template.h"

#include "vn_device.h"
#include "vn_pipeline.h"

void
vn_descriptor_set_layout_destroy(struct vn_device *dev,
                                 struct vn_descriptor_set_layout *layout)
{
   VkDevice dev_handle = vn_device_to_handle(dev);
   VkDescriptorSetLayout layout_handle =
      vn_descriptor_set_layout_to_handle(layout);
   const VkAllocationCallbacks *alloc = &dev->base.base.alloc;

   vn_async_vkDestroyDescriptorSetLayout(dev->primary_ring, dev_handle,
                                         layout_handle, NULL);

   vn_object_base_fini(&layout->base);
   vk_free(alloc, layout);
}

static void
vn_descriptor_set_destroy(struct vn_device *dev,
                          struct vn_descriptor_set *set,
                          const VkAllocationCallbacks *alloc)
{
   list_del(&set->head);

   vn_descriptor_set_layout_unref(dev, set->layout);

   vn_object_base_fini(&set->base);
   vk_free(alloc, set);
}

/* Map VkDescriptorType to contiguous enum vn_descriptor_type */
static enum vn_descriptor_type
vn_descriptor_type(VkDescriptorType type)
{
   switch (type) {
   case VK_DESCRIPTOR_TYPE_SAMPLER:
      return VN_DESCRIPTOR_TYPE_SAMPLER;
   case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      return VN_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      return VN_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
   case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      return VN_DESCRIPTOR_TYPE_STORAGE_IMAGE;
   case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      return VN_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
   case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      return VN_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      return VN_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      return VN_DESCRIPTOR_TYPE_STORAGE_BUFFER;
   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      return VN_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      return VN_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
   case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
      return VN_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
   case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
      return VN_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
   case VK_DESCRIPTOR_TYPE_MUTABLE_EXT:
      return VN_DESCRIPTOR_TYPE_MUTABLE_EXT;
   default:
      break;
   }

   unreachable("bad VkDescriptorType");
}

/* descriptor set layout commands */

void
vn_GetDescriptorSetLayoutSupport(
   VkDevice device,
   const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
   VkDescriptorSetLayoutSupport *pSupport)
{
   struct vn_device *dev = vn_device_from_handle(device);

   /* TODO per-device cache */
   vn_call_vkGetDescriptorSetLayoutSupport(dev->primary_ring, device,
                                           pCreateInfo, pSupport);
}

static void
vn_descriptor_set_layout_init(
   struct vn_device *dev,
   const VkDescriptorSetLayoutCreateInfo *create_info,
   uint32_t last_binding,
   struct vn_descriptor_set_layout *layout)
{
   VkDevice dev_handle = vn_device_to_handle(dev);
   VkDescriptorSetLayout layout_handle =
      vn_descriptor_set_layout_to_handle(layout);
   const VkDescriptorSetLayoutBindingFlagsCreateInfo *binding_flags =
      vk_find_struct_const(create_info->pNext,
                           DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO);

   const VkMutableDescriptorTypeCreateInfoEXT *mutable_descriptor_info =
      vk_find_struct_const(create_info->pNext,
                           MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT);

   /* 14.2.1. Descriptor Set Layout
    *
    * If bindingCount is zero or if this structure is not included in
    * the pNext chain, the VkDescriptorBindingFlags for each descriptor
    * set layout binding is considered to be zero.
    */
   if (binding_flags && !binding_flags->bindingCount)
      binding_flags = NULL;

   layout->is_push_descriptor =
      create_info->flags &
      VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;

   layout->refcount = VN_REFCOUNT_INIT(1);
   layout->last_binding = last_binding;

   for (uint32_t i = 0; i < create_info->bindingCount; i++) {
      const VkDescriptorSetLayoutBinding *binding_info =
         &create_info->pBindings[i];
      const enum vn_descriptor_type type =
         vn_descriptor_type(binding_info->descriptorType);
      struct vn_descriptor_set_layout_binding *binding =
         &layout->bindings[binding_info->binding];

      if (binding_info->binding == last_binding) {
         /* 14.2.1. Descriptor Set Layout
          *
          * VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT must only be
          * used for the last binding in the descriptor set layout (i.e. the
          * binding with the largest value of binding).
          *
          * 41. Features
          *
          * descriptorBindingVariableDescriptorCount indicates whether the
          * implementation supports descriptor sets with a variable-sized last
          * binding. If this feature is not enabled,
          * VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT must not be
          * used.
          */
         layout->has_variable_descriptor_count =
            binding_flags &&
            (binding_flags->pBindingFlags[i] &
             VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT);
      }

      binding->type = type;
      binding->count = binding_info->descriptorCount;

      switch (type) {
      case VN_DESCRIPTOR_TYPE_SAMPLER:
      case VN_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
         binding->has_immutable_samplers = binding_info->pImmutableSamplers;
         break;
      case VN_DESCRIPTOR_TYPE_MUTABLE_EXT:
         assert(mutable_descriptor_info->mutableDescriptorTypeListCount &&
                mutable_descriptor_info->pMutableDescriptorTypeLists[i]
                   .descriptorTypeCount);
         const VkMutableDescriptorTypeListEXT *list =
            &mutable_descriptor_info->pMutableDescriptorTypeLists[i];
         for (uint32_t j = 0; j < list->descriptorTypeCount; j++) {
            BITSET_SET(binding->mutable_descriptor_types,
                       vn_descriptor_type(list->pDescriptorTypes[j]));
         }
         break;
      default:
         break;
      }
   }

   vn_async_vkCreateDescriptorSetLayout(dev->primary_ring, dev_handle,
                                        create_info, NULL, &layout_handle);
}

VkResult
vn_CreateDescriptorSetLayout(
   VkDevice device,
   const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
   const VkAllocationCallbacks *pAllocator,
   VkDescriptorSetLayout *pSetLayout)
{
   struct vn_device *dev = vn_device_from_handle(device);
   /* ignore pAllocator as the layout is reference-counted */
   const VkAllocationCallbacks *alloc = &dev->base.base.alloc;

   STACK_ARRAY(VkDescriptorSetLayoutBinding, bindings,
               pCreateInfo->bindingCount);

   uint32_t last_binding = 0;
   VkDescriptorSetLayoutCreateInfo local_create_info;
   if (pCreateInfo->bindingCount) {
      typed_memcpy(bindings, pCreateInfo->pBindings,
                   pCreateInfo->bindingCount);

      for (uint32_t i = 0; i < pCreateInfo->bindingCount; i++) {
         VkDescriptorSetLayoutBinding *binding = &bindings[i];

         if (last_binding < binding->binding)
            last_binding = binding->binding;

         switch (binding->descriptorType) {
         case VK_DESCRIPTOR_TYPE_SAMPLER:
         case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            break;
         default:
            binding->pImmutableSamplers = NULL;
            break;
         }
      }

      local_create_info = *pCreateInfo;
      local_create_info.pBindings = bindings;
      pCreateInfo = &local_create_info;
   }

   const size_t layout_size =
      offsetof(struct vn_descriptor_set_layout, bindings[last_binding + 1]);
   /* allocated with the device scope */
   struct vn_descriptor_set_layout *layout =
      vk_zalloc(alloc, layout_size, VN_DEFAULT_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
   if (!layout) {
      STACK_ARRAY_FINISH(bindings);
      return vn_error(dev->instance, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   vn_object_base_init(&layout->base, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                       &dev->base);

   vn_descriptor_set_layout_init(dev, pCreateInfo, last_binding, layout);

   STACK_ARRAY_FINISH(bindings);

   *pSetLayout = vn_descriptor_set_layout_to_handle(layout);

   return VK_SUCCESS;
}

void
vn_DestroyDescriptorSetLayout(VkDevice device,
                              VkDescriptorSetLayout descriptorSetLayout,
                              const VkAllocationCallbacks *pAllocator)
{
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_descriptor_set_layout *layout =
      vn_descriptor_set_layout_from_handle(descriptorSetLayout);

   if (!layout)
      return;

   vn_descriptor_set_layout_unref(dev, layout);
}

/* descriptor pool commands */

VkResult
vn_CreateDescriptorPool(VkDevice device,
                        const VkDescriptorPoolCreateInfo *pCreateInfo,
                        const VkAllocationCallbacks *pAllocator,
                        VkDescriptorPool *pDescriptorPool)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.base.alloc;

   const VkDescriptorPoolInlineUniformBlockCreateInfo *iub_info =
      vk_find_struct_const(pCreateInfo->pNext,
                           DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO);

   uint32_t mutable_states_count = 0;
   for (uint32_t i = 0; i < pCreateInfo->poolSizeCount; i++) {
      const VkDescriptorPoolSize *pool_size = &pCreateInfo->pPoolSizes[i];
      if (pool_size->type == VK_DESCRIPTOR_TYPE_MUTABLE_EXT)
         mutable_states_count++;
   }
   struct vn_descriptor_pool *pool;
   struct vn_descriptor_pool_state_mutable *mutable_states;

   VK_MULTIALLOC(ma);
   vk_multialloc_add(&ma, &pool, __typeof__(*pool), 1);
   vk_multialloc_add(&ma, &mutable_states, __typeof__(*mutable_states),
                     mutable_states_count);

   if (!vk_multialloc_zalloc(&ma, alloc, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT))
      return vn_error(dev->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   vn_object_base_init(&pool->base, VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                       &dev->base);

   pool->allocator = *alloc;
   pool->mutable_states = mutable_states;

   const VkMutableDescriptorTypeCreateInfoEXT *mutable_descriptor_info =
      vk_find_struct_const(pCreateInfo->pNext,
                           MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT);

   /* Without VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, the set
    * allocation must not fail due to a fragmented pool per spec. In this
    * case, set allocation can be asynchronous with pool resource tracking.
    */
   pool->async_set_allocation =
      !VN_PERF(NO_ASYNC_SET_ALLOC) &&
      !(pCreateInfo->flags &
        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

   pool->max.set_count = pCreateInfo->maxSets;

   if (iub_info)
      pool->max.iub_binding_count = iub_info->maxInlineUniformBlockBindings;

   uint32_t next_mutable_state = 0;
   for (uint32_t i = 0; i < pCreateInfo->poolSizeCount; i++) {
      const VkDescriptorPoolSize *pool_size = &pCreateInfo->pPoolSizes[i];
      const enum vn_descriptor_type type =
         vn_descriptor_type(pool_size->type);

      if (type != VN_DESCRIPTOR_TYPE_MUTABLE_EXT) {
         pool->max.descriptor_counts[type] += pool_size->descriptorCount;
         continue;
      }

      struct vn_descriptor_pool_state_mutable *mutable_state = NULL;
      BITSET_DECLARE(mutable_types, VN_NUM_DESCRIPTOR_TYPES);
      if (!mutable_descriptor_info ||
          i >= mutable_descriptor_info->mutableDescriptorTypeListCount) {
         BITSET_ONES(mutable_types);
      } else {
         BITSET_ZERO(mutable_types);
         const VkMutableDescriptorTypeListEXT *list =
            &mutable_descriptor_info->pMutableDescriptorTypeLists[i];

         for (uint32_t j = 0; j < list->descriptorTypeCount; j++) {
            BITSET_SET(mutable_types,
                       vn_descriptor_type(list->pDescriptorTypes[j]));
         }
      }
      for (uint32_t j = 0; j < next_mutable_state; j++) {
         if (BITSET_EQUAL(mutable_types, pool->mutable_states[j].types)) {
            mutable_state = &pool->mutable_states[j];
            break;
         }
      }

      if (!mutable_state) {
         /* The application must ensure that partial overlap does not exist in
          * pPoolSizes. so this entry must have a disjoint set of types.
          */
         mutable_state = &pool->mutable_states[next_mutable_state++];
         BITSET_COPY(mutable_state->types, mutable_types);
      }

      mutable_state->max += pool_size->descriptorCount;
   }

   pool->mutable_states_count = next_mutable_state;
   list_inithead(&pool->descriptor_sets);

   VkDescriptorPool pool_handle = vn_descriptor_pool_to_handle(pool);
   vn_async_vkCreateDescriptorPool(dev->primary_ring, device, pCreateInfo,
                                   NULL, &pool_handle);

   vn_tls_set_async_pipeline_create();

   *pDescriptorPool = pool_handle;

   return VK_SUCCESS;
}

void
vn_DestroyDescriptorPool(VkDevice device,
                         VkDescriptorPool descriptorPool,
                         const VkAllocationCallbacks *pAllocator)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_descriptor_pool *pool =
      vn_descriptor_pool_from_handle(descriptorPool);
   const VkAllocationCallbacks *alloc;

   if (!pool)
      return;

   alloc = pAllocator ? pAllocator : &pool->allocator;

   vn_async_vkDestroyDescriptorPool(dev->primary_ring, device, descriptorPool,
                                    NULL);

   list_for_each_entry_safe(struct vn_descriptor_set, set,
                            &pool->descriptor_sets, head)
      vn_descriptor_set_destroy(dev, set, alloc);

   vn_object_base_fini(&pool->base);
   vk_free(alloc, pool);
}

static struct vn_descriptor_pool_state_mutable *
vn_get_mutable_state(const struct vn_descriptor_pool *pool,
                     const struct vn_descriptor_set_layout_binding *binding)
{
   for (uint32_t i = 0; i < pool->mutable_states_count; i++) {
      struct vn_descriptor_pool_state_mutable *mutable_state =
         &pool->mutable_states[i];
      BITSET_DECLARE(shared_types, VN_NUM_DESCRIPTOR_TYPES);
      BITSET_AND(shared_types, mutable_state->types,
                 binding->mutable_descriptor_types);

      /* The application must ensure that partial overlap does not exist in
       * pPoolSizes, so there only exists one matching entry.
       */
      if (BITSET_EQUAL(shared_types, binding->mutable_descriptor_types))
         return mutable_state;
   }
   unreachable("bad mutable descriptor binding");
}

static inline void
vn_pool_restore_mutable_states(struct vn_descriptor_pool *pool,
                               const struct vn_descriptor_set_layout *layout,
                               uint32_t binding_index,
                               uint32_t descriptor_count)
{
   assert(layout->bindings[binding_index].type ==
          VN_DESCRIPTOR_TYPE_MUTABLE_EXT);
   assert(descriptor_count);
   struct vn_descriptor_pool_state_mutable *mutable_state =
      vn_get_mutable_state(pool, &layout->bindings[binding_index]);
   assert(mutable_state && mutable_state->used >= descriptor_count);
   mutable_state->used -= descriptor_count;
}

static bool
vn_descriptor_pool_alloc_descriptors(
   struct vn_descriptor_pool *pool,
   const struct vn_descriptor_set_layout *layout,
   uint32_t last_binding_descriptor_count)
{
   assert(pool->async_set_allocation);

   if (pool->used.set_count == pool->max.set_count)
      return false;

   /* backup current pool state to recovery */
   struct vn_descriptor_pool_state recovery = pool->used;
   pool->used.set_count++;

   uint32_t i = 0;
   for (; i <= layout->last_binding; i++) {
      const struct vn_descriptor_set_layout_binding *binding =
         &layout->bindings[i];
      const enum vn_descriptor_type type = binding->type;
      const uint32_t count = i == layout->last_binding
                                ? last_binding_descriptor_count
                                : binding->count;

      /* Skip resource accounting for either of below:
       * - reserved binding entry that has a valid type with a zero count
       * - invalid binding entry from sparse binding indices
       */
      if (!count)
         continue;

      if (type == VN_DESCRIPTOR_TYPE_MUTABLE_EXT) {
         /* A mutable descriptor can be allocated if below are satisfied:
          * - vn_descriptor_pool_state_mutable::types is a superset
          * - vn_descriptor_pool_state_mutable::{max - used} is enough
          */
         struct vn_descriptor_pool_state_mutable *mutable_state =
            vn_get_mutable_state(pool, binding);
         assert(mutable_state);
         if (mutable_state->used + count > mutable_state->max)
            goto restore;

         mutable_state->used += count;
      } else {
         if (type == VN_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK &&
             ++pool->used.iub_binding_count > pool->max.iub_binding_count)
            goto restore;

         pool->used.descriptor_counts[type] += count;
         if (pool->used.descriptor_counts[type] >
             pool->max.descriptor_counts[type])
            goto restore;
      }
   }

   return true;

restore:
   /* restore pool state before this allocation */
   pool->used = recovery;
   for (uint32_t j = 0; j < i; j++) {
      /* mutable state at binding i is not changed */
      const uint32_t count = layout->bindings[j].count;
      if (count && layout->bindings[j].type == VN_DESCRIPTOR_TYPE_MUTABLE_EXT)
         vn_pool_restore_mutable_states(pool, layout, j, count);
   }
   return false;
}

static void
vn_descriptor_pool_free_descriptors(
   struct vn_descriptor_pool *pool,
   const struct vn_descriptor_set_layout *layout,
   uint32_t last_binding_descriptor_count)
{
   assert(pool->async_set_allocation);

   for (uint32_t i = 0; i <= layout->last_binding; i++) {
      const uint32_t count = i == layout->last_binding
                                ? last_binding_descriptor_count
                                : layout->bindings[i].count;
      if (!count)
         continue;

      const enum vn_descriptor_type type = layout->bindings[i].type;
      if (type == VN_DESCRIPTOR_TYPE_MUTABLE_EXT) {
         vn_pool_restore_mutable_states(pool, layout, i, count);
      } else {
         pool->used.descriptor_counts[type] -= count;

         if (type == VN_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK)
            pool->used.iub_binding_count--;
      }
   }

   pool->used.set_count--;
}

static inline void
vn_descriptor_pool_reset_descriptors(struct vn_descriptor_pool *pool)
{
   assert(pool->async_set_allocation);

   memset(&pool->used, 0, sizeof(pool->used));

   for (uint32_t i = 0; i < pool->mutable_states_count; i++)
      pool->mutable_states[i].used = 0;
}

VkResult
vn_ResetDescriptorPool(VkDevice device,
                       VkDescriptorPool descriptorPool,
                       VkDescriptorPoolResetFlags flags)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_descriptor_pool *pool =
      vn_descriptor_pool_from_handle(descriptorPool);
   const VkAllocationCallbacks *alloc = &pool->allocator;

   vn_async_vkResetDescriptorPool(dev->primary_ring, device, descriptorPool,
                                  flags);

   list_for_each_entry_safe(struct vn_descriptor_set, set,
                            &pool->descriptor_sets, head)
      vn_descriptor_set_destroy(dev, set, alloc);

   if (pool->async_set_allocation)
      vn_descriptor_pool_reset_descriptors(pool);

   return VK_SUCCESS;
}

/* descriptor set commands */

VkResult
vn_AllocateDescriptorSets(VkDevice device,
                          const VkDescriptorSetAllocateInfo *pAllocateInfo,
                          VkDescriptorSet *pDescriptorSets)
{
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_descriptor_pool *pool =
      vn_descriptor_pool_from_handle(pAllocateInfo->descriptorPool);
   const VkAllocationCallbacks *alloc = &pool->allocator;
   VkResult result;

   /* 14.2.3. Allocation of Descriptor Sets
    *
    * If descriptorSetCount is zero or this structure is not included in
    * the pNext chain, then the variable lengths are considered to be zero.
    */
   const VkDescriptorSetVariableDescriptorCountAllocateInfo *variable_info =
      vk_find_struct_const(
         pAllocateInfo->pNext,
         DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO);
   if (variable_info && !variable_info->descriptorSetCount)
      variable_info = NULL;

   uint32_t i = 0;
   for (; i < pAllocateInfo->descriptorSetCount; i++) {
      struct vn_descriptor_set_layout *layout =
         vn_descriptor_set_layout_from_handle(pAllocateInfo->pSetLayouts[i]);

      /* 14.2.3. Allocation of Descriptor Sets
       *
       * If VkDescriptorSetAllocateInfo::pSetLayouts[i] does not include a
       * variable count descriptor binding, then pDescriptorCounts[i] is
       * ignored.
       */
      uint32_t last_binding_descriptor_count = 0;
      if (!layout->has_variable_descriptor_count) {
         last_binding_descriptor_count =
            layout->bindings[layout->last_binding].count;
      } else if (variable_info) {
         last_binding_descriptor_count = variable_info->pDescriptorCounts[i];
      }

      if (pool->async_set_allocation &&
          !vn_descriptor_pool_alloc_descriptors(
             pool, layout, last_binding_descriptor_count)) {
         result = VK_ERROR_OUT_OF_POOL_MEMORY;
         goto fail;
      }

      struct vn_descriptor_set *set =
         vk_zalloc(alloc, sizeof(*set), VN_DEFAULT_ALIGN,
                   VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
      if (!set) {
         if (pool->async_set_allocation) {
            vn_descriptor_pool_free_descriptors(
               pool, layout, last_binding_descriptor_count);
         }
         result = VK_ERROR_OUT_OF_HOST_MEMORY;
         goto fail;
      }

      vn_object_base_init(&set->base, VK_OBJECT_TYPE_DESCRIPTOR_SET,
                          &dev->base);

      /* We might reorder vkCmdBindDescriptorSets after
       * vkDestroyDescriptorSetLayout due to batching.  The spec says
       *
       *   VkDescriptorSetLayout objects may be accessed by commands that
       *   operate on descriptor sets allocated using that layout, and those
       *   descriptor sets must not be updated with vkUpdateDescriptorSets
       *   after the descriptor set layout has been destroyed. Otherwise, a
       *   VkDescriptorSetLayout object passed as a parameter to create
       *   another object is not further accessed by that object after the
       *   duration of the command it is passed into.
       *
       * It is ambiguous but the reordering is likely invalid.  Let's keep the
       * layout alive with the set to defer vkDestroyDescriptorSetLayout.
       */
      set->layout = vn_descriptor_set_layout_ref(dev, layout);
      set->last_binding_descriptor_count = last_binding_descriptor_count;
      list_addtail(&set->head, &pool->descriptor_sets);

      pDescriptorSets[i] = vn_descriptor_set_to_handle(set);
   }

   if (pool->async_set_allocation) {
      vn_async_vkAllocateDescriptorSets(dev->primary_ring, device,
                                        pAllocateInfo, pDescriptorSets);
   } else {
      result = vn_call_vkAllocateDescriptorSets(
         dev->primary_ring, device, pAllocateInfo, pDescriptorSets);
      if (result != VK_SUCCESS)
         goto fail;
   }

   return VK_SUCCESS;

fail:
   for (uint32_t j = 0; j < i; j++) {
      struct vn_descriptor_set *set =
         vn_descriptor_set_from_handle(pDescriptorSets[j]);

      if (pool->async_set_allocation) {
         vn_descriptor_pool_free_descriptors(
            pool, set->layout, set->last_binding_descriptor_count);
      }

      vn_descriptor_set_destroy(dev, set, alloc);
   }

   memset(pDescriptorSets, 0,
          sizeof(*pDescriptorSets) * pAllocateInfo->descriptorSetCount);

   return vn_error(dev->instance, result);
}

VkResult
vn_FreeDescriptorSets(VkDevice device,
                      VkDescriptorPool descriptorPool,
                      uint32_t descriptorSetCount,
                      const VkDescriptorSet *pDescriptorSets)
{
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_descriptor_pool *pool =
      vn_descriptor_pool_from_handle(descriptorPool);
   const VkAllocationCallbacks *alloc = &pool->allocator;

   assert(!pool->async_set_allocation);

   vn_async_vkFreeDescriptorSets(dev->primary_ring, device, descriptorPool,
                                 descriptorSetCount, pDescriptorSets);

   for (uint32_t i = 0; i < descriptorSetCount; i++) {
      struct vn_descriptor_set *set =
         vn_descriptor_set_from_handle(pDescriptorSets[i]);

      if (!set)
         continue;

      vn_descriptor_set_destroy(dev, set, alloc);
   }

   return VK_SUCCESS;
}

uint32_t
vn_descriptor_set_count_write_images(uint32_t write_count,
                                     const VkWriteDescriptorSet *writes)
{
   uint32_t img_info_count = 0;
   for (uint32_t i = 0; i < write_count; i++) {
      const VkWriteDescriptorSet *write = &writes[i];
      switch (write->descriptorType) {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
         img_info_count += write->descriptorCount;
         break;
      default:
         break;
      }
   }
   return img_info_count;
}

const VkWriteDescriptorSet *
vn_descriptor_set_get_writes(uint32_t write_count,
                             const VkWriteDescriptorSet *writes,
                             VkPipelineLayout pipeline_layout_handle,
                             struct vn_descriptor_set_writes *local)
{
   const struct vn_pipeline_layout *pipeline_layout =
      vn_pipeline_layout_from_handle(pipeline_layout_handle);

   typed_memcpy(local->writes, writes, write_count);

   uint32_t img_info_count = 0;
   for (uint32_t i = 0; i < write_count; i++) {
      const struct vn_descriptor_set_layout *set_layout =
         pipeline_layout
            ? pipeline_layout->push_descriptor_set_layout
            : vn_descriptor_set_from_handle(writes[i].dstSet)->layout;
      VkWriteDescriptorSet *write = &local->writes[i];
      VkDescriptorImageInfo *img_infos = &local->img_infos[img_info_count];
      bool ignore_sampler = true;
      bool ignore_iview = false;
      switch (write->descriptorType) {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
         ignore_iview = true;
         FALLTHROUGH;
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
         ignore_sampler =
            set_layout->bindings[write->dstBinding].has_immutable_samplers;
         FALLTHROUGH;
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
         typed_memcpy(img_infos, write->pImageInfo, write->descriptorCount);
         for (uint32_t j = 0; j < write->descriptorCount; j++) {
            if (ignore_sampler)
               img_infos[j].sampler = VK_NULL_HANDLE;
            if (ignore_iview)
               img_infos[j].imageView = VK_NULL_HANDLE;
         }
         write->pImageInfo = img_infos;
         write->pBufferInfo = NULL;
         write->pTexelBufferView = NULL;
         img_info_count += write->descriptorCount;
         break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
         write->pImageInfo = NULL;
         write->pBufferInfo = NULL;
         break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         write->pImageInfo = NULL;
         write->pTexelBufferView = NULL;
         break;
      case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
      case VK_DESCRIPTOR_TYPE_MUTABLE_EXT:
      default:
         write->pImageInfo = NULL;
         write->pBufferInfo = NULL;
         write->pTexelBufferView = NULL;
         break;
      }
   }
   return local->writes;
}

void
vn_UpdateDescriptorSets(VkDevice device,
                        uint32_t descriptorWriteCount,
                        const VkWriteDescriptorSet *pDescriptorWrites,
                        uint32_t descriptorCopyCount,
                        const VkCopyDescriptorSet *pDescriptorCopies)
{
   struct vn_device *dev = vn_device_from_handle(device);
   const uint32_t img_info_count = vn_descriptor_set_count_write_images(
      descriptorWriteCount, pDescriptorWrites);

   STACK_ARRAY(VkWriteDescriptorSet, writes, descriptorWriteCount);
   STACK_ARRAY(VkDescriptorImageInfo, img_infos, img_info_count);
   struct vn_descriptor_set_writes local = {
      .writes = writes,
      .img_infos = img_infos,
   };
   pDescriptorWrites = vn_descriptor_set_get_writes(
      descriptorWriteCount, pDescriptorWrites, VK_NULL_HANDLE, &local);

   vn_async_vkUpdateDescriptorSets(dev->primary_ring, device,
                                   descriptorWriteCount, pDescriptorWrites,
                                   descriptorCopyCount, pDescriptorCopies);

   STACK_ARRAY_FINISH(writes);
   STACK_ARRAY_FINISH(img_infos);
}

/* descriptor update template commands */

static void
vn_descriptor_update_template_init(
   struct vn_descriptor_update_template *templ,
   const VkDescriptorUpdateTemplateCreateInfo *create_info)
{
   templ->entry_count = create_info->descriptorUpdateEntryCount;
   for (uint32_t i = 0; i < create_info->descriptorUpdateEntryCount; i++) {
      const VkDescriptorUpdateTemplateEntry *entry =
         &create_info->pDescriptorUpdateEntries[i];
      templ->entries[i] = *entry;
      switch (entry->descriptorType) {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
         templ->img_info_count += entry->descriptorCount;
         break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
         templ->bview_count += entry->descriptorCount;
         break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         templ->buf_info_count += entry->descriptorCount;
         break;
      case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
         templ->iub_count += 1;
         break;
      case VK_DESCRIPTOR_TYPE_MUTABLE_EXT:
         break;
      default:
         unreachable("unhandled descriptor type");
         break;
      }
   }
}

VkResult
vn_CreateDescriptorUpdateTemplate(
   VkDevice device,
   const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
   const VkAllocationCallbacks *pAllocator,
   VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.base.alloc;

   const size_t templ_size =
      offsetof(struct vn_descriptor_update_template,
               entries[pCreateInfo->descriptorUpdateEntryCount]);
   struct vn_descriptor_update_template *templ = vk_zalloc(
      alloc, templ_size, VN_DEFAULT_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!templ)
      return vn_error(dev->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   vn_object_base_init(&templ->base,
                       VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE, &dev->base);

   if (pCreateInfo->templateType ==
       VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR) {
      struct vn_pipeline_layout *pipeline_layout =
         vn_pipeline_layout_from_handle(pCreateInfo->pipelineLayout);
      templ->push.pipeline_bind_point = pCreateInfo->pipelineBindPoint;
      templ->push.set_layout = pipeline_layout->push_descriptor_set_layout;
   }

   vn_descriptor_update_template_init(templ, pCreateInfo);

   /* no host object */
   *pDescriptorUpdateTemplate =
      vn_descriptor_update_template_to_handle(templ);

   return VK_SUCCESS;
}

void
vn_DestroyDescriptorUpdateTemplate(
   VkDevice device,
   VkDescriptorUpdateTemplate descriptorUpdateTemplate,
   const VkAllocationCallbacks *pAllocator)
{
   VN_TRACE_FUNC();
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_descriptor_update_template *templ =
      vn_descriptor_update_template_from_handle(descriptorUpdateTemplate);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.base.alloc;

   if (!templ)
      return;

   /* no host object */
   vn_object_base_fini(&templ->base);
   vk_free(alloc, templ);
}

void
vn_descriptor_set_fill_update_with_template(
   struct vn_descriptor_update_template *templ,
   VkDescriptorSet set_handle,
   const uint8_t *data,
   struct vn_descriptor_set_update *update)
{
   struct vn_descriptor_set *set = vn_descriptor_set_from_handle(set_handle);
   const struct vn_descriptor_set_layout *set_layout =
      templ->push.set_layout ? templ->push.set_layout : set->layout;

   update->write_count = templ->entry_count;

   uint32_t img_info_offset = 0;
   uint32_t buf_info_offset = 0;
   uint32_t bview_offset = 0;
   uint32_t iub_offset = 0;
   for (uint32_t i = 0; i < templ->entry_count; i++) {
      const VkDescriptorUpdateTemplateEntry *entry = &templ->entries[i];
      const uint8_t *ptr = data + entry->offset;
      bool ignore_sampler = true;
      bool ignore_iview = false;
      VkDescriptorImageInfo *img_infos = NULL;
      VkDescriptorBufferInfo *buf_infos = NULL;
      VkBufferView *bview_handles = NULL;
      VkWriteDescriptorSetInlineUniformBlock *iub = NULL;
      switch (entry->descriptorType) {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
         ignore_iview = true;
         FALLTHROUGH;
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
         ignore_sampler =
            set_layout->bindings[entry->dstBinding].has_immutable_samplers;
         FALLTHROUGH;
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
         img_infos = &update->img_infos[img_info_offset];
         for (uint32_t j = 0; j < entry->descriptorCount; j++) {
            const VkDescriptorImageInfo *src = (const void *)ptr;
            img_infos[j] = (VkDescriptorImageInfo){
               .sampler = ignore_sampler ? VK_NULL_HANDLE : src->sampler,
               .imageView = ignore_iview ? VK_NULL_HANDLE : src->imageView,
               .imageLayout = src->imageLayout,
            };
            ptr += entry->stride;
         }
         img_info_offset += entry->descriptorCount;
         break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
         bview_handles = &update->bview_handles[bview_offset];
         for (uint32_t j = 0; j < entry->descriptorCount; j++) {
            bview_handles[j] = *(const VkBufferView *)ptr;
            ptr += entry->stride;
         }
         bview_offset += entry->descriptorCount;
         break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         buf_infos = &update->buf_infos[buf_info_offset];
         for (uint32_t j = 0; j < entry->descriptorCount; j++) {
            buf_infos[j] = *(const VkDescriptorBufferInfo *)ptr;
            ptr += entry->stride;
         }
         buf_info_offset += entry->descriptorCount;
         break;
      case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK:
         iub = &update->iubs[iub_offset];
         *iub = (VkWriteDescriptorSetInlineUniformBlock){
            .sType =
               VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK,
            .dataSize = entry->descriptorCount,
            .pData = (const void *)ptr,
         };
         iub_offset++;
         break;
      case VK_DESCRIPTOR_TYPE_MUTABLE_EXT:
         break;
      default:
         unreachable("unhandled descriptor type");
         break;
      }
      update->writes[i] = (VkWriteDescriptorSet){
         .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .pNext = iub,
         .dstSet = set_handle,
         .dstBinding = entry->dstBinding,
         .dstArrayElement = entry->dstArrayElement,
         .descriptorCount = entry->descriptorCount,
         .descriptorType = entry->descriptorType,
         .pImageInfo = img_infos,
         .pBufferInfo = buf_infos,
         .pTexelBufferView = bview_handles,
      };
   }
}

void
vn_UpdateDescriptorSetWithTemplate(
   VkDevice device,
   VkDescriptorSet descriptorSet,
   VkDescriptorUpdateTemplate descriptorUpdateTemplate,
   const void *pData)
{
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_descriptor_update_template *templ =
      vn_descriptor_update_template_from_handle(descriptorUpdateTemplate);

   STACK_ARRAY(VkWriteDescriptorSet, writes, templ->entry_count);
   STACK_ARRAY(VkDescriptorImageInfo, img_infos, templ->img_info_count);
   STACK_ARRAY(VkDescriptorBufferInfo, buf_infos, templ->buf_info_count);
   STACK_ARRAY(VkBufferView, bview_handles, templ->bview_count);
   STACK_ARRAY(VkWriteDescriptorSetInlineUniformBlock, iubs,
               templ->iub_count);
   struct vn_descriptor_set_update update = {
      .writes = writes,
      .img_infos = img_infos,
      .buf_infos = buf_infos,
      .bview_handles = bview_handles,
      .iubs = iubs,
   };
   vn_descriptor_set_fill_update_with_template(templ, descriptorSet, pData,
                                               &update);

   vn_async_vkUpdateDescriptorSets(
      dev->primary_ring, device, update.write_count, update.writes, 0, NULL);

   STACK_ARRAY_FINISH(writes);
   STACK_ARRAY_FINISH(img_infos);
   STACK_ARRAY_FINISH(buf_infos);
   STACK_ARRAY_FINISH(bview_handles);
   STACK_ARRAY_FINISH(iubs);
}
