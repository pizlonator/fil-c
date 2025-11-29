/* Copyright © 2023 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

void
genX(libanv_query_copy)(global void *destination_base,
                        uint32_t destination_stride,
                        global void *query_data,
                        uint32_t first_query,
                        uint32_t num_queries,
                        uint32_t query_data_offset,
                        uint32_t query_stride,
                        uint32_t num_query_items,
                        uint32_t copy_flags,
                        uint32_t copy_item_idx)
{
   if (copy_item_idx >= num_queries)
      return;

   bool is_result64     = (copy_flags & ANV_COPY_QUERY_FLAG_RESULT64) != 0;
   bool write_available = (copy_flags & ANV_COPY_QUERY_FLAG_AVAILABLE) != 0;
   bool compute_delta   = (copy_flags & ANV_COPY_QUERY_FLAG_DELTA) != 0;
   bool partial_result  = (copy_flags & ANV_COPY_QUERY_FLAG_PARTIAL) != 0;


   uint query_byte = (first_query + copy_item_idx) * query_stride;
   uint query_data_byte = query_byte + query_data_offset;

   global uint64_t *query = query_data + (first_query + copy_item_idx) * query_stride;
   global uint64_t *dest64 = destination_base + copy_item_idx * destination_stride;
   global uint32_t *dest32 = destination_base + copy_item_idx * destination_stride;

   uint64_t availability = *(global uint32_t *)(query_data + query_byte);

   if (write_available) {
      if (is_result64)
         dest64[num_query_items] = availability;
      else
         dest32[num_query_items] = availability;
   }

   for (uint32_t i = 0; i < num_query_items; i++) {
      uint32_t qw_offset = 1 + i * 2;
      uint64_t v;
      if (compute_delta) {
         struct delta64 {
            uint64_t v0;
            uint64_t v1;
         } data = *((global struct delta64 *)&query[qw_offset]);
         v = data.v1 - data.v0;
      } else {
         v = query[qw_offset + 0];
      }

      /* vkCmdCopyQueryPoolResults:
       *
       *    "If VK_QUERY_RESULT_PARTIAL_BIT is set, then for any query that is
       *     unavailable, an intermediate result between zero and the final
       *     result value is written for that query."
       *
       * We write 0 as the values not being written yet, we can't really make
       * provide any sensible value.
       */
      if (partial_result && availability == 0)
         v = 0;

      if (is_result64)
         dest64[i] = v;
      else
         dest32[i] = v;
   }
}
