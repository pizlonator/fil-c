/*
 * Copyright © 2023 Valve Corporation
 * SPDX-License-Identifier: MIT
 */

#include "lvp_acceleration_structure.h"
#include "lvp_entrypoints.h"

#include "util/format/format_utils.h"
#include "util/half_float.h"

static_assert(sizeof(struct lvp_bvh_triangle_node) % 8 == 0, "lvp_bvh_triangle_node is not padded");
static_assert(sizeof(struct lvp_bvh_aabb_node) % 8 == 0, "lvp_bvh_aabb_node is not padded");
static_assert(sizeof(struct lvp_bvh_instance_node) % 8 == 0, "lvp_bvh_instance_node is not padded");
static_assert(sizeof(struct lvp_bvh_box_node) % 8 == 0, "lvp_bvh_box_node is not padded");

VKAPI_ATTR void VKAPI_CALL
lvp_GetAccelerationStructureBuildSizesKHR(
   VkDevice _device, VkAccelerationStructureBuildTypeKHR buildType,
   const VkAccelerationStructureBuildGeometryInfoKHR *pBuildInfo,
   const uint32_t *pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR *pSizeInfo)
{
   pSizeInfo->buildScratchSize = 64;
   pSizeInfo->updateScratchSize = 64;

   uint32_t leaf_count = 0;
   for (uint32_t i = 0; i < pBuildInfo->geometryCount; i++)
      leaf_count += pMaxPrimitiveCounts[i];

   uint32_t internal_count = MAX2(leaf_count, 2) - 1;

   VkGeometryTypeKHR geometry_type = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
   if (pBuildInfo->geometryCount) {
      if (pBuildInfo->pGeometries)
         geometry_type = pBuildInfo->pGeometries[0].geometryType;
      else
         geometry_type = pBuildInfo->ppGeometries[0]->geometryType;
   }

   uint32_t leaf_size;
   switch (geometry_type) {
   case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
      leaf_size = sizeof(struct lvp_bvh_triangle_node);
      break;
   case VK_GEOMETRY_TYPE_AABBS_KHR:
      leaf_size = sizeof(struct lvp_bvh_aabb_node);
      break;
   case VK_GEOMETRY_TYPE_INSTANCES_KHR:
      leaf_size = sizeof(struct lvp_bvh_instance_node);
      break;
   default:
      unreachable("Unknown VkGeometryTypeKHR");
   }

   uint32_t bvh_size = sizeof(struct lvp_bvh_header);
   bvh_size += leaf_count * leaf_size;
   bvh_size += internal_count * sizeof(struct lvp_bvh_box_node);

   pSizeInfo->accelerationStructureSize = bvh_size;
}

VKAPI_ATTR VkResult VKAPI_CALL
lvp_WriteAccelerationStructuresPropertiesKHR(
   VkDevice _device, uint32_t accelerationStructureCount,
   const VkAccelerationStructureKHR *pAccelerationStructures, VkQueryType queryType,
   size_t dataSize, void *pData, size_t stride)
{
   unreachable("Unimplemented");
   return VK_ERROR_FEATURE_NOT_PRESENT;
}

VKAPI_ATTR VkResult VKAPI_CALL
lvp_BuildAccelerationStructuresKHR(
   VkDevice _device, VkDeferredOperationKHR deferredOperation, uint32_t infoCount,
   const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
   const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos)
{
   unreachable("Unimplemented");
   return VK_ERROR_FEATURE_NOT_PRESENT;
}

VKAPI_ATTR void VKAPI_CALL
lvp_GetDeviceAccelerationStructureCompatibilityKHR(
   VkDevice _device, const VkAccelerationStructureVersionInfoKHR *pVersionInfo,
   VkAccelerationStructureCompatibilityKHR *pCompatibility)
{
   uint8_t uuid[VK_UUID_SIZE];
   lvp_device_get_cache_uuid(uuid);
   bool compat = memcmp(pVersionInfo->pVersionData, uuid, VK_UUID_SIZE) == 0;
   *pCompatibility = compat ? VK_ACCELERATION_STRUCTURE_COMPATIBILITY_COMPATIBLE_KHR
                            : VK_ACCELERATION_STRUCTURE_COMPATIBILITY_INCOMPATIBLE_KHR;
}

VKAPI_ATTR VkResult VKAPI_CALL
lvp_CopyAccelerationStructureKHR(VkDevice _device, VkDeferredOperationKHR deferredOperation,
                                 const VkCopyAccelerationStructureInfoKHR *pInfo)
{
   unreachable("Unimplemented");
   return VK_ERROR_FEATURE_NOT_PRESENT;
}

VKAPI_ATTR VkResult VKAPI_CALL
lvp_CopyMemoryToAccelerationStructureKHR(VkDevice _device, VkDeferredOperationKHR deferredOperation,
                                         const VkCopyMemoryToAccelerationStructureInfoKHR *pInfo)
{
   unreachable("Unimplemented");
   return VK_ERROR_FEATURE_NOT_PRESENT;
}

VKAPI_ATTR VkResult VKAPI_CALL
lvp_CopyAccelerationStructureToMemoryKHR(VkDevice _device, VkDeferredOperationKHR deferredOperation,
                                         const VkCopyAccelerationStructureToMemoryInfoKHR *pInfo)
{
   unreachable("Unimplemented");
   return VK_ERROR_FEATURE_NOT_PRESENT;
}

static uint32_t
lvp_pack_geometry_id_and_flags(uint32_t geometry_id, uint32_t flags)
{
   uint32_t geometry_id_and_flags = geometry_id;
   if (flags & VK_GEOMETRY_OPAQUE_BIT_KHR)
      geometry_id_and_flags |= LVP_GEOMETRY_OPAQUE;

   return geometry_id_and_flags;
}

static uint32_t
lvp_pack_sbt_offset_and_flags(uint32_t sbt_offset, VkGeometryInstanceFlagsKHR flags)
{
   uint32_t ret = sbt_offset;
   if (flags & VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR)
      ret |= LVP_INSTANCE_FORCE_OPAQUE;
   if (!(flags & VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR))
      ret |= LVP_INSTANCE_NO_FORCE_NOT_OPAQUE;
   if (flags & VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR)
      ret |= LVP_INSTANCE_TRIANGLE_FACING_CULL_DISABLE;
   if (flags & VK_GEOMETRY_INSTANCE_TRIANGLE_FLIP_FACING_BIT_KHR)
      ret |= LVP_INSTANCE_TRIANGLE_FLIP_FACING;
   return ret;
}

struct lvp_build_internal_ctx {
   uint8_t *dst;
   uint32_t dst_offset;

   void *leaf_nodes;
   uint32_t leaf_nodes_offset;
   uint32_t leaf_node_type;
   uint32_t leaf_node_size;
};

static uint32_t
lvp_build_internal_node(struct lvp_build_internal_ctx *ctx, uint32_t first_leaf, uint32_t last_leaf)
{
   uint32_t dst_offset = ctx->dst_offset;
   ctx->dst_offset += sizeof(struct lvp_bvh_box_node);

   uint32_t node_id = dst_offset | lvp_bvh_node_internal;

   struct lvp_bvh_box_node *node = (void *)(ctx->dst + dst_offset);

   uint32_t split = (first_leaf + last_leaf) / 2;

   if (first_leaf < split)
      node->children[0] = lvp_build_internal_node(ctx, first_leaf, split);
   else
      node->children[0] =
         (ctx->leaf_nodes_offset + (first_leaf * ctx->leaf_node_size)) | ctx->leaf_node_type;

   if (first_leaf < last_leaf) {
      if (split + 1 < last_leaf)
         node->children[1] = lvp_build_internal_node(ctx, split + 1, last_leaf);
      else
         node->children[1] =
            (ctx->leaf_nodes_offset + (last_leaf * ctx->leaf_node_size)) | ctx->leaf_node_type;
   } else {
      node->children[1] = LVP_BVH_INVALID_NODE;
   }

   for (uint32_t i = 0; i < 2; i++) {
      struct lvp_aabb *aabb = &node->bounds[i];

      if (node->children[i] == LVP_BVH_INVALID_NODE) {
         aabb->min.x = INFINITY;
         aabb->min.y = INFINITY;
         aabb->min.z = INFINITY;
         aabb->max.x = -INFINITY;
         aabb->max.y = -INFINITY;
         aabb->max.z = -INFINITY;
         continue;
      }

      uint32_t child_offset = node->children[i] & (~3u);
      uint32_t child_type = node->children[i] & 3u;
      void *child_node = (void *)(ctx->dst + child_offset);

      switch (child_type) {
      case lvp_bvh_node_triangle: {
         struct lvp_bvh_triangle_node *triangle = child_node;

         aabb->min.x = MIN3(triangle->coords[0][0], triangle->coords[1][0], triangle->coords[2][0]);
         aabb->min.y = MIN3(triangle->coords[0][1], triangle->coords[1][1], triangle->coords[2][1]);
         aabb->min.z = MIN3(triangle->coords[0][2], triangle->coords[1][2], triangle->coords[2][2]);

         aabb->max.x = MAX3(triangle->coords[0][0], triangle->coords[1][0], triangle->coords[2][0]);
         aabb->max.y = MAX3(triangle->coords[0][1], triangle->coords[1][1], triangle->coords[2][1]);
         aabb->max.z = MAX3(triangle->coords[0][2], triangle->coords[1][2], triangle->coords[2][2]);

         break;
      }
      case lvp_bvh_node_internal: {
         struct lvp_bvh_box_node *box = child_node;

         aabb->min.x = MIN2(box->bounds[0].min.x, box->bounds[1].min.x);
         aabb->min.y = MIN2(box->bounds[0].min.y, box->bounds[1].min.y);
         aabb->min.z = MIN2(box->bounds[0].min.z, box->bounds[1].min.z);

         aabb->max.x = MAX2(box->bounds[0].max.x, box->bounds[1].max.x);
         aabb->max.y = MAX2(box->bounds[0].max.y, box->bounds[1].max.y);
         aabb->max.z = MAX2(box->bounds[0].max.z, box->bounds[1].max.z);

         break;
      }
      case lvp_bvh_node_instance: {
         struct lvp_bvh_instance_node *instance = child_node;
         struct lvp_bvh_header *instance_header = (void *)(uintptr_t)instance->bvh_ptr;

         float bounds[2][3];

         float header_bounds[2][3];
         memcpy(header_bounds, &instance_header->bounds, sizeof(struct lvp_aabb));

         for (unsigned j = 0; j < 3; ++j) {
            bounds[0][j] = instance->otw_matrix.values[j][3];
            bounds[1][j] = instance->otw_matrix.values[j][3];
            for (unsigned k = 0; k < 3; ++k) {
               bounds[0][j] += MIN2(instance->otw_matrix.values[j][k] * header_bounds[0][k],
                                    instance->otw_matrix.values[j][k] * header_bounds[1][k]);
               bounds[1][j] += MAX2(instance->otw_matrix.values[j][k] * header_bounds[0][k],
                                    instance->otw_matrix.values[j][k] * header_bounds[1][k]);
            }
         }

         memcpy(aabb, bounds, sizeof(struct lvp_aabb));

         break;
      }
      case lvp_bvh_node_aabb: {
         struct lvp_bvh_aabb_node *aabb_node = child_node;

         memcpy(aabb, &aabb_node->bounds, sizeof(struct lvp_aabb));

         break;
      }
      default:
         unreachable("Invalid node type");
      }
   }

   return node_id;
}

void
lvp_build_acceleration_structure(VkAccelerationStructureBuildGeometryInfoKHR *info,
                                 const VkAccelerationStructureBuildRangeInfoKHR *ranges)
{
   VK_FROM_HANDLE(vk_acceleration_structure, accel_struct, info->dstAccelerationStructure);
   void *dst = (void *)(uintptr_t)vk_acceleration_structure_get_va(accel_struct);

   memset(dst, 0, accel_struct->size);

   struct lvp_bvh_header *header = dst;
   header->instance_count = 0;

   struct lvp_bvh_box_node *root = (void *)((uint8_t *)dst + sizeof(struct lvp_bvh_header));

   uint32_t leaf_count = 0;
   for (unsigned i = 0; i < info->geometryCount; i++)
      leaf_count += ranges[i].primitiveCount;

   if (!leaf_count) {
      for (uint32_t i = 0; i < 2; i++) {
         root->bounds[i].min.x = INFINITY;
         root->bounds[i].min.y = INFINITY;
         root->bounds[i].min.z = INFINITY;
         root->bounds[i].max.x = -INFINITY;
         root->bounds[i].max.y = -INFINITY;
         root->bounds[i].max.z = -INFINITY;
      }
      return;
   }

   uint32_t internal_count = MAX2(leaf_count, 2) - 1;

   uint32_t primitive_index = 0;

   header->leaf_nodes_offset =
      sizeof(struct lvp_bvh_header) + sizeof(struct lvp_bvh_box_node) * internal_count;
   void *leaf_nodes = (void *)((uint8_t *)dst + header->leaf_nodes_offset);

   for (unsigned i = 0; i < info->geometryCount; i++) {
      const VkAccelerationStructureGeometryKHR *geom =
         info->pGeometries ? &info->pGeometries[i] : info->ppGeometries[i];

      const VkAccelerationStructureBuildRangeInfoKHR *range = &ranges[i];

      uint32_t geometry_id_and_flags = lvp_pack_geometry_id_and_flags(i, geom->flags);

      switch (geom->geometryType) {
      case VK_GEOMETRY_TYPE_TRIANGLES_KHR: {
         assert(info->type == VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);

         const uint8_t *vertex_data_base = geom->geometry.triangles.vertexData.hostAddress;
         vertex_data_base += range->firstVertex * geom->geometry.triangles.vertexStride;

         const uint8_t *index_data = geom->geometry.triangles.indexData.hostAddress;

         if (geom->geometry.triangles.indexType == VK_INDEX_TYPE_NONE_KHR)
            vertex_data_base += range->primitiveOffset;
         else
            index_data += range->primitiveOffset;

         VkTransformMatrixKHR transform_matrix = {
            .matrix =
               {
                  {1.0, 0.0, 0.0, 0.0},
                  {0.0, 1.0, 0.0, 0.0},
                  {0.0, 0.0, 1.0, 0.0},
               },
         };

         const uint8_t *transform = geom->geometry.triangles.transformData.hostAddress;
         if (transform) {
            transform += range->transformOffset;
            transform_matrix = *(VkTransformMatrixKHR *)transform;
         }

         VkDeviceSize stride = geom->geometry.triangles.vertexStride;
         VkFormat vertex_format = geom->geometry.triangles.vertexFormat;
         VkIndexType index_type = geom->geometry.triangles.indexType;

         for (uint32_t j = 0; j < range->primitiveCount; j++) {
            struct lvp_bvh_triangle_node *node = leaf_nodes;
            node += primitive_index;

            node->primitive_id = j;
            node->geometry_id_and_flags = geometry_id_and_flags;

            for (uint32_t v = 0; v < 3; v++) {
               uint32_t index = range->firstVertex;
               switch (index_type) {
               case VK_INDEX_TYPE_NONE_KHR:
                  index += j * 3 + v;
                  break;
               case VK_INDEX_TYPE_UINT8_EXT:
                  index += *(const uint8_t *)index_data;
                  index_data += 1;
                  break;
               case VK_INDEX_TYPE_UINT16:
                  index += *(const uint16_t *)index_data;
                  index_data += 2;
                  break;
               case VK_INDEX_TYPE_UINT32:
                  index += *(const uint32_t *)index_data;
                  index_data += 4;
                  break;
               case VK_INDEX_TYPE_MAX_ENUM:
                  unreachable("Unhandled VK_INDEX_TYPE_MAX_ENUM");
                  break;
               }

               const uint8_t *vertex_data = vertex_data_base + index * stride;
               float coords[4];
               switch (vertex_format) {
               case VK_FORMAT_R32G32_SFLOAT:
                  coords[0] = *(const float *)(vertex_data + 0);
                  coords[1] = *(const float *)(vertex_data + 4);
                  coords[2] = 0.0f;
                  coords[3] = 1.0f;
                  break;
               case VK_FORMAT_R32G32B32_SFLOAT:
                  coords[0] = *(const float *)(vertex_data + 0);
                  coords[1] = *(const float *)(vertex_data + 4);
                  coords[2] = *(const float *)(vertex_data + 8);
                  coords[3] = 1.0f;
                  break;
               case VK_FORMAT_R32G32B32A32_SFLOAT:
                  coords[0] = *(const float *)(vertex_data + 0);
                  coords[1] = *(const float *)(vertex_data + 4);
                  coords[2] = *(const float *)(vertex_data + 8);
                  coords[3] = *(const float *)(vertex_data + 12);
                  break;
               case VK_FORMAT_R16G16_SFLOAT:
                  coords[0] = _mesa_half_to_float(*(const uint16_t *)(vertex_data + 0));
                  coords[1] = _mesa_half_to_float(*(const uint16_t *)(vertex_data + 2));
                  coords[2] = 0.0f;
                  coords[3] = 1.0f;
                  break;
               case VK_FORMAT_R16G16B16_SFLOAT:
                  coords[0] = _mesa_half_to_float(*(const uint16_t *)(vertex_data + 0));
                  coords[1] = _mesa_half_to_float(*(const uint16_t *)(vertex_data + 2));
                  coords[2] = _mesa_half_to_float(*(const uint16_t *)(vertex_data + 4));
                  coords[3] = 1.0f;
                  break;
               case VK_FORMAT_R16G16B16A16_SFLOAT:
                  coords[0] = _mesa_half_to_float(*(const uint16_t *)(vertex_data + 0));
                  coords[1] = _mesa_half_to_float(*(const uint16_t *)(vertex_data + 2));
                  coords[2] = _mesa_half_to_float(*(const uint16_t *)(vertex_data + 4));
                  coords[3] = _mesa_half_to_float(*(const uint16_t *)(vertex_data + 6));
                  break;
               case VK_FORMAT_R16G16_SNORM:
                  coords[0] = _mesa_snorm_to_float(*(const int16_t *)(vertex_data + 0), 16);
                  coords[1] = _mesa_snorm_to_float(*(const int16_t *)(vertex_data + 2), 16);
                  coords[2] = 0.0f;
                  coords[3] = 1.0f;
                  break;
               case VK_FORMAT_R16G16_UNORM:
                  coords[0] = _mesa_unorm_to_float(*(const uint16_t *)(vertex_data + 0), 16);
                  coords[1] = _mesa_unorm_to_float(*(const uint16_t *)(vertex_data + 2), 16);
                  coords[2] = 0.0f;
                  coords[3] = 1.0f;
                  break;
               case VK_FORMAT_R16G16B16A16_SNORM:
                  coords[0] = _mesa_snorm_to_float(*(const int16_t *)(vertex_data + 0), 16);
                  coords[1] = _mesa_snorm_to_float(*(const int16_t *)(vertex_data + 2), 16);
                  coords[2] = _mesa_snorm_to_float(*(const int16_t *)(vertex_data + 4), 16);
                  coords[3] = _mesa_snorm_to_float(*(const int16_t *)(vertex_data + 6), 16);
                  break;
               case VK_FORMAT_R16G16B16A16_UNORM:
                  coords[0] = _mesa_unorm_to_float(*(const uint16_t *)(vertex_data + 0), 16);
                  coords[1] = _mesa_unorm_to_float(*(const uint16_t *)(vertex_data + 2), 16);
                  coords[2] = _mesa_unorm_to_float(*(const uint16_t *)(vertex_data + 4), 16);
                  coords[3] = _mesa_unorm_to_float(*(const uint16_t *)(vertex_data + 6), 16);
                  break;
               case VK_FORMAT_R8G8_SNORM:
                  coords[0] = _mesa_snorm_to_float(*(const int8_t *)(vertex_data + 0), 8);
                  coords[1] = _mesa_snorm_to_float(*(const int8_t *)(vertex_data + 1), 8);
                  coords[2] = 0.0f;
                  coords[3] = 1.0f;
                  break;
               case VK_FORMAT_R8G8_UNORM:
                  coords[0] = _mesa_unorm_to_float(*(const uint8_t *)(vertex_data + 0), 8);
                  coords[1] = _mesa_unorm_to_float(*(const uint8_t *)(vertex_data + 1), 8);
                  coords[2] = 0.0f;
                  coords[3] = 1.0f;
                  break;
               case VK_FORMAT_R8G8B8A8_SNORM:
                  coords[0] = _mesa_snorm_to_float(*(const int8_t *)(vertex_data + 0), 8);
                  coords[1] = _mesa_snorm_to_float(*(const int8_t *)(vertex_data + 1), 8);
                  coords[2] = _mesa_snorm_to_float(*(const int8_t *)(vertex_data + 2), 8);
                  coords[3] = _mesa_snorm_to_float(*(const int8_t *)(vertex_data + 3), 8);
                  break;
               case VK_FORMAT_R8G8B8A8_UNORM:
                  coords[0] = _mesa_unorm_to_float(*(const uint8_t *)(vertex_data + 0), 8);
                  coords[1] = _mesa_unorm_to_float(*(const uint8_t *)(vertex_data + 1), 8);
                  coords[2] = _mesa_unorm_to_float(*(const uint8_t *)(vertex_data + 2), 8);
                  coords[3] = _mesa_unorm_to_float(*(const uint8_t *)(vertex_data + 3), 8);
                  break;
               case VK_FORMAT_A2B10G10R10_UNORM_PACK32: {
                  uint32_t val = *(const uint32_t *)vertex_data;
                  coords[0] = _mesa_unorm_to_float((val >> 0) & 0x3FF, 10);
                  coords[1] = _mesa_unorm_to_float((val >> 10) & 0x3FF, 10);
                  coords[2] = _mesa_unorm_to_float((val >> 20) & 0x3FF, 10);
                  coords[3] = _mesa_unorm_to_float((val >> 30) & 0x3, 2);
               } break;
               default:
                  unreachable("Unhandled vertex format in BVH build");
               }

               for (unsigned comp = 0; comp < 3; comp++) {
                  float r = 0;
                  for (unsigned col = 0; col < 4; col++)
                     r += transform_matrix.matrix[comp][col] * coords[col];

                  node->coords[v][comp] = r;
               }
            }

            primitive_index++;
         }

         break;
      }
      case VK_GEOMETRY_TYPE_AABBS_KHR: {
         assert(info->type == VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);

         const uint8_t *data = geom->geometry.aabbs.data.hostAddress;
         data += range->primitiveOffset;

         VkDeviceSize stride = geom->geometry.aabbs.stride;

         for (uint32_t j = 0; j < range->primitiveCount; j++) {
            struct lvp_bvh_aabb_node *node = leaf_nodes;
            node += primitive_index;

            node->primitive_id = j;
            node->geometry_id_and_flags = geometry_id_and_flags;

            const VkAabbPositionsKHR *aabb = (const VkAabbPositionsKHR *)(data + j * stride);
            node->bounds.min.x = aabb->minX;
            node->bounds.min.y = aabb->minY;
            node->bounds.min.z = aabb->minZ;
            node->bounds.max.x = aabb->maxX;
            node->bounds.max.y = aabb->maxY;
            node->bounds.max.z = aabb->maxZ;

            primitive_index++;
         }

         break;
      }
      case VK_GEOMETRY_TYPE_INSTANCES_KHR: {
         assert(info->type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR);

         const uint8_t *data = geom->geometry.instances.data.hostAddress;
         data += range->primitiveOffset;

         for (uint32_t j = 0; j < range->primitiveCount; j++) {
            struct lvp_bvh_instance_node *node = leaf_nodes;
            node += primitive_index;

            const VkAccelerationStructureInstanceKHR *instance =
               geom->geometry.instances.arrayOfPointers
                  ? (((const VkAccelerationStructureInstanceKHR *const *)data)[j])
                  : &((const VkAccelerationStructureInstanceKHR *)data)[j];
            if (!instance->accelerationStructureReference)
               continue;

            node->bvh_ptr = instance->accelerationStructureReference;

            float transform[16], inv_transform[16];
            memcpy(transform, &instance->transform.matrix, sizeof(instance->transform.matrix));
            transform[12] = transform[13] = transform[14] = 0.0f;
            transform[15] = 1.0f;

            util_invert_mat4x4(inv_transform, transform);
            memcpy(node->wto_matrix.values, inv_transform, sizeof(node->wto_matrix.values));

            node->custom_instance_and_mask = instance->instanceCustomIndex | (instance->mask << 24);
            node->sbt_offset_and_flags = lvp_pack_sbt_offset_and_flags(
               instance->instanceShaderBindingTableRecordOffset, instance->flags);
            node->instance_id = j;

            memcpy(node->otw_matrix.values, instance->transform.matrix,
                   sizeof(node->otw_matrix.values));

            primitive_index++;
            header->instance_count++;
         }

         break;
      }
      default:
         unreachable("Unknown geometryType");
      }
   }

   leaf_count = primitive_index;

   struct lvp_build_internal_ctx internal_ctx = {
      .dst = dst,
      .dst_offset = sizeof(struct lvp_bvh_header),

      .leaf_nodes = leaf_nodes,
      .leaf_nodes_offset = header->leaf_nodes_offset,
   };

   VkGeometryTypeKHR geometry_type = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
   if (info->geometryCount) {
      if (info->pGeometries)
         geometry_type = info->pGeometries[0].geometryType;
      else
         geometry_type = info->ppGeometries[0]->geometryType;
   }

   switch (geometry_type) {
   case VK_GEOMETRY_TYPE_TRIANGLES_KHR:
      internal_ctx.leaf_node_type = lvp_bvh_node_triangle;
      internal_ctx.leaf_node_size = sizeof(struct lvp_bvh_triangle_node);
      break;
   case VK_GEOMETRY_TYPE_AABBS_KHR:
      internal_ctx.leaf_node_type = lvp_bvh_node_aabb;
      internal_ctx.leaf_node_size = sizeof(struct lvp_bvh_aabb_node);
      break;
   case VK_GEOMETRY_TYPE_INSTANCES_KHR:
      internal_ctx.leaf_node_type = lvp_bvh_node_instance;
      internal_ctx.leaf_node_size = sizeof(struct lvp_bvh_instance_node);
      break;
   default:
      unreachable("Unknown VkGeometryTypeKHR");
   }

   if (leaf_count) {
      lvp_build_internal_node(&internal_ctx, 0, leaf_count - 1);
   } else {
      root->children[0] = LVP_BVH_INVALID_NODE;
      root->children[1] = LVP_BVH_INVALID_NODE;
   }

   header->bounds.min.x = MIN2(root->bounds[0].min.x, root->bounds[1].min.x);
   header->bounds.min.y = MIN2(root->bounds[0].min.y, root->bounds[1].min.y);
   header->bounds.min.z = MIN2(root->bounds[0].min.z, root->bounds[1].min.z);

   header->bounds.max.x = MAX2(root->bounds[0].max.x, root->bounds[1].max.x);
   header->bounds.max.y = MAX2(root->bounds[0].max.y, root->bounds[1].max.y);
   header->bounds.max.z = MAX2(root->bounds[0].max.z, root->bounds[1].max.z);

   header->serialization_size = sizeof(struct lvp_accel_struct_serialization_header) +
                                sizeof(uint64_t) * header->instance_count + accel_struct->size;
}
