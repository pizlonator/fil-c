/*
 * Copyright (C) 2022 Collabora Ltd.
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

#pragma once

#if !defined(PAN_ARCH) || PAN_ARCH < 10
#error "cs_builder.h requires PAN_ARCH >= 10"
#endif

#include "gen_macros.h"

/*
 * cs_builder implements a builder for CSF command streams. It manages the
 * allocation and overflow behaviour of queues and provides helpers for emitting
 * commands to run on the CSF pipe.
 *
 * Users are responsible for the CS buffer allocation and must initialize the
 * command stream with an initial buffer using cs_builder_init(). The CS can
 * be extended with new buffers allocated with cs_builder_conf::alloc_buffer()
 * if the builder runs out of memory.
 */

struct cs_buffer {
   /* CPU pointer */
   uint64_t *cpu;

   /* GPU pointer */
   uint64_t gpu;

   /* Capacity in number of 64-bit instructions */
   uint32_t capacity;
};

struct cs_builder_conf {
   /* Number of 32-bit registers in the hardware register file */
   uint8_t nr_registers;

   /* Number of 32-bit registers used by the kernel at submission time */
   uint8_t nr_kernel_registers;

   /* CS buffer allocator */
   struct cs_buffer (*alloc_buffer)(void *cookie);

   /* Cookie passed back to alloc_buffer() */
   void *cookie;
};

/* The CS is formed of one or more CS chunks linked with JUMP instructions.
 * The builder keeps track of the current chunk and the position inside this
 * chunk, so it can emit new instructions, and decide when a new chunk needs
 * to be allocated.
 */
struct cs_chunk {
   /* CS buffer object backing this chunk */
   struct cs_buffer buffer;

   union {
      /* Current position in the buffer object when the chunk is active. */
      uint32_t pos;

      /* Chunk size when the chunk was wrapped. */
      uint32_t size;
   };
};

struct cs_builder {
   /* CS builder configuration */
   struct cs_builder_conf conf;

   /* Initial (root) CS chunk. */
   struct cs_chunk root_chunk;

   /* Current CS chunk. */
   struct cs_chunk cur_chunk;

   /* Move immediate instruction at the end of the last CS chunk that needs to
    * be patched with the final length of the current CS chunk in order to
    * facilitate correct overflow behaviour.
    */
   uint32_t *length_patch;

   /* Used as temporary storage when the allocator couldn't allocate a new
    * CS chunk.
    */
   uint64_t discard_instr_slot;
};

static void
cs_builder_init(struct cs_builder *b, const struct cs_builder_conf *conf,
                struct cs_buffer root_buffer)
{
   *b = (struct cs_builder){
      .conf = *conf,
      .root_chunk.buffer = root_buffer,
      .cur_chunk.buffer = root_buffer,
   };

   /* We need at least 3 registers for CS chunk linking. Assume the kernel needs
    * at least that too.
    */
   b->conf.nr_kernel_registers = MAX2(b->conf.nr_kernel_registers, 3);
}

static bool
cs_is_valid(struct cs_builder *b)
{
   return b->cur_chunk.buffer.cpu != NULL;
}

/*
 * Wrap the current queue. External users shouldn't call this function
 * directly, they should call cs_finish() when they are done building
 * the command stream, which will in turn call cs_wrap_queue().
 *
 * Internally, this is also used to finalize internal CS chunks when
 * allocating new sub-chunks. See cs_alloc_chunk() for details.
 *
 * This notably requires patching the previous chunk with the length
 * we ended up emitting for this chunk.
 */
static void
cs_wrap_chunk(struct cs_builder *b)
{
   if (!cs_is_valid(b))
      return;

   if (b->length_patch) {
      *b->length_patch = (b->cur_chunk.pos * 8);
      b->length_patch = NULL;
   }

   if (b->root_chunk.buffer.gpu == b->cur_chunk.buffer.gpu)
      b->root_chunk.size = b->cur_chunk.size;
}

/* Call this when you are done building a command stream and want to prepare
 * it for submission.
 */
static void
cs_finish(struct cs_builder *b)
{
   if (!cs_is_valid(b))
      return;

   cs_wrap_chunk(b);

   /* This prevents adding instructions after that point. */
   memset(&b->cur_chunk, 0, sizeof(b->cur_chunk));
}

enum cs_index_type {
   CS_INDEX_REGISTER = 0,
   CS_INDEX_UNDEF,
};

struct cs_index {
   enum cs_index_type type;

   /* Number of 32-bit words in the index, must be nonzero */
   uint8_t size;

   union {
      uint64_t imm;
      uint8_t reg;
   };
};

static inline struct cs_index
cs_undef(void)
{
   return (struct cs_index){
      .type = CS_INDEX_UNDEF,
   };
}

static inline uint8_t
cs_to_reg_tuple(struct cs_index idx, ASSERTED unsigned expected_size)
{
   assert(idx.type == CS_INDEX_REGISTER);
   assert(idx.size == expected_size);

   return idx.reg;
}

static inline uint8_t
cs_to_reg32(struct cs_index idx)
{
   return cs_to_reg_tuple(idx, 1);
}

static inline uint8_t
cs_to_reg64(struct cs_index idx)
{
   return cs_to_reg_tuple(idx, 2);
}

static inline struct cs_index
cs_reg_tuple(ASSERTED struct cs_builder *b, unsigned reg, unsigned size)
{
   assert(reg + size <= b->conf.nr_registers - b->conf.nr_kernel_registers &&
          "overflowed register file");
   assert(size <= 16 && "unsupported");

   return (struct cs_index){
      .type = CS_INDEX_REGISTER,
      .size = size,
      .reg = reg,
   };
}

static inline struct cs_index
cs_reg32(struct cs_builder *b, unsigned reg)
{
   return cs_reg_tuple(b, reg, 1);
}

static inline struct cs_index
cs_reg64(struct cs_builder *b, unsigned reg)
{
   assert((reg % 2) == 0 && "unaligned 64-bit reg");
   return cs_reg_tuple(b, reg, 2);
}

/*
 * The top of the register file is reserved for cs_builder internal use. We
 * need 3 spare registers for handling command queue overflow. These are
 * available here.
 */
static inline uint8_t
cs_overflow_address_reg(struct cs_builder *b)
{
   return b->conf.nr_registers - 2;
}

static inline uint8_t
cs_overflow_length_reg(struct cs_builder *b)
{
   return b->conf.nr_registers - 3;
}

static inline struct cs_index
cs_extract32(struct cs_builder *b, struct cs_index idx, unsigned word)
{
   assert(idx.type == CS_INDEX_REGISTER && "unsupported");
   assert(word < idx.size && "overrun");

   return cs_reg32(b, idx.reg + word);
}

#define JUMP_SEQ_INSTR_COUNT 4

static inline void *
cs_alloc_ins(struct cs_builder *b)
{
   /* If an allocation failure happened before, we just discard all following
    * instructions.
    */
   if (unlikely(!b->cur_chunk.buffer.cpu))
      return &b->discard_instr_slot;

   /* If the current chunk runs out of space, allocate a new one and jump to it.
    * We actually do this a few instructions before running out, because the
    * sequence to jump to a new queue takes multiple instructions.
    */
   if (unlikely((b->cur_chunk.size + JUMP_SEQ_INSTR_COUNT) >
                b->cur_chunk.buffer.capacity)) {
      /* Now, allocate a new chunk */
      struct cs_buffer newbuf = b->conf.alloc_buffer(b->conf.cookie);

      /* Allocation failure, from now on, all new instructions will be
       * discarded.
       */
      if (unlikely(!b->cur_chunk.buffer.cpu))
         return &b->discard_instr_slot;

      uint64_t *ptr = b->cur_chunk.buffer.cpu + (b->cur_chunk.pos++);

      pan_pack(ptr, CS_MOVE, I) {
         I.destination = cs_overflow_address_reg(b);
         I.immediate = newbuf.gpu;
      }

      ptr = b->cur_chunk.buffer.cpu + (b->cur_chunk.pos++);

      pan_pack(ptr, CS_MOVE32, I) {
         I.destination = cs_overflow_length_reg(b);
      }

      /* The length will be patched in later */
      uint32_t *length_patch = (uint32_t *)ptr;

      ptr = b->cur_chunk.buffer.cpu + (b->cur_chunk.pos++);

      pan_pack(ptr, CS_JUMP, I) {
         I.length = cs_overflow_length_reg(b);
         I.address = cs_overflow_address_reg(b);
      }

      /* Now that we've emitted everything, finish up the previous queue */
      cs_wrap_chunk(b);

      /* And make this one current */
      b->length_patch = length_patch;
      b->cur_chunk.buffer = newbuf;
      b->cur_chunk.pos = 0;
   }

   assert(b->cur_chunk.size < b->cur_chunk.buffer.capacity);
   return b->cur_chunk.buffer.cpu + (b->cur_chunk.pos++);
}

/*
 * Helper to emit a new instruction into the command queue. The allocation needs
 * to be separated out being pan_pack can evaluate its argument multiple times,
 * yet cs_alloc has side effects.
 */
#define cs_emit(b, T, cfg) pan_pack(cs_alloc_ins(b), CS_##T, cfg)

/* Asynchronous operations take a mask of scoreboard slots to wait on
 * before executing the instruction, and signal a scoreboard slot when
 * the operation is complete.
 * A wait_mask of zero means the operation is synchronous, and signal_slot
 * is ignored in that case.
 */
struct cs_async_op {
   uint16_t wait_mask;
   uint8_t signal_slot;
};

static inline struct cs_async_op
cs_defer(unsigned wait_mask, unsigned signal_slot)
{
   /* The scoreboard slot to signal is incremented before the wait operation,
    * waiting on it would cause an infinite wait.
    */
   assert(!(wait_mask & BITFIELD_BIT(signal_slot)));

   return (struct cs_async_op){
      .wait_mask = wait_mask,
      .signal_slot = signal_slot,
   };
}

static inline struct cs_async_op
cs_now(void)
{
   return (struct cs_async_op){
      .wait_mask = 0,
      .signal_slot = 0,
   };
}

#define cs_apply_async(I, async)                                               \
   do {                                                                        \
      I.wait_mask = async.wait_mask;                                           \
      I.signal_slot = async.signal_slot;                                       \
   } while (0)

static inline void
cs_move32_to(struct cs_builder *b, struct cs_index dest, unsigned imm)
{
   cs_emit(b, MOVE32, I) {
      I.destination = cs_to_reg32(dest);
      I.immediate = imm;
   }
}

static inline void
cs_move48_to(struct cs_builder *b, struct cs_index dest, uint64_t imm)
{
   cs_emit(b, MOVE, I) {
      I.destination = cs_to_reg64(dest);
      I.immediate = imm;
   }
}

static inline void
cs_move64_to(struct cs_builder *b, struct cs_index dest, uint64_t imm)
{
   if (imm < (1ull << 48)) {
      /* Zero extends */
      cs_move48_to(b, dest, imm);
   } else {
      cs_move32_to(b, cs_extract32(b, dest, 0), imm);
      cs_move32_to(b, cs_extract32(b, dest, 1), imm >> 32);
   }
}

static inline void
cs_wait_slots(struct cs_builder *b, unsigned wait_mask, bool progress_inc)
{
   cs_emit(b, WAIT, I) {
      I.wait_mask = wait_mask;
      I.progress_increment = progress_inc;
   }
}

static inline void
cs_wait_slot(struct cs_builder *b, unsigned slot, bool progress_inc)
{
   assert(slot < 8 && "invalid slot");

   cs_wait_slots(b, BITFIELD_BIT(slot), progress_inc);
}

struct cs_shader_res_sel {
   uint8_t srt, fau, spd, tsd;
};

static inline struct cs_shader_res_sel
cs_shader_res_sel(unsigned srt, unsigned fau, unsigned spd, unsigned tsd)
{
   return (struct cs_shader_res_sel){
      .srt = srt,
      .fau = fau,
      .spd = spd,
      .tsd = tsd,
   };
}

static inline void
cs_run_compute(struct cs_builder *b, unsigned task_increment,
               enum mali_task_axis task_axis, bool progress_inc,
               struct cs_shader_res_sel res_sel)
{
   cs_emit(b, RUN_COMPUTE, I) {
      I.task_increment = task_increment;
      I.task_axis = task_axis;
      I.progress_increment = progress_inc;
      I.srt_select = res_sel.srt;
      I.spd_select = res_sel.spd;
      I.tsd_select = res_sel.tsd;
      I.fau_select = res_sel.fau;
   }
}

static inline void
cs_run_tiling(struct cs_builder *b, uint32_t flags_override, bool progress_inc,
              struct cs_shader_res_sel res_sel)
{
   cs_emit(b, RUN_TILING, I) {
      I.flags_override = flags_override;
      I.progress_increment = progress_inc;
      I.srt_select = res_sel.srt;
      I.spd_select = res_sel.spd;
      I.tsd_select = res_sel.tsd;
      I.fau_select = res_sel.fau;
   }
}

static inline void
cs_run_idvs(struct cs_builder *b, uint32_t flags_override, bool progress_inc,
            bool malloc_enable, struct cs_shader_res_sel varying_sel,
            struct cs_shader_res_sel frag_sel, struct cs_index draw_id)
{
   cs_emit(b, RUN_IDVS, I) {
      I.flags_override = flags_override;
      I.progress_increment = progress_inc;
      I.malloc_enable = malloc_enable;

      if (draw_id.type == CS_INDEX_UNDEF) {
         I.draw_id_register_enable = false;
      } else {
         I.draw_id_register_enable = true;
         I.draw_id = cs_to_reg32(draw_id);
      }

      assert(varying_sel.spd == 1);
      assert(varying_sel.fau == 0 || varying_sel.fau == 1);
      assert(varying_sel.srt == 0 || varying_sel.srt == 1);
      assert(varying_sel.tsd == 0 || varying_sel.tsd == 1);
      I.varying_fau_select = varying_sel.fau == 1;
      I.varying_srt_select = varying_sel.srt == 1;
      I.varying_tsd_select = varying_sel.tsd == 1;

      assert(frag_sel.spd == 2);
      assert(frag_sel.fau == 2);
      assert(frag_sel.srt == 2 || frag_sel.srt == 0);
      assert(frag_sel.tsd == 2 || frag_sel.tsd == 0);
      I.fragment_srt_select = frag_sel.srt == 2;
      I.fragment_tsd_select = frag_sel.tsd == 2;
   }
}

static inline void
cs_run_fragment(struct cs_builder *b, bool enable_tem,
                enum mali_tile_render_order tile_order, bool progress_inc)
{
   cs_emit(b, RUN_FRAGMENT, I) {
      I.enable_tem = enable_tem;
      I.tile_order = tile_order;
      I.progress_increment = progress_inc;
   }
}

static inline void
cs_run_fullscreen(struct cs_builder *b, uint32_t flags_override,
                  bool progress_inc, struct cs_index dcd)
{
   cs_emit(b, RUN_FULLSCREEN, I) {
      I.flags_override = flags_override;
      I.progress_increment = progress_inc;
      I.dcd = cs_to_reg64(dcd);
   }
}

static inline void
cs_finish_tiling(struct cs_builder *b, bool progress_inc)
{
   cs_emit(b, FINISH_TILING, I)
      I.progress_increment = progress_inc;
}

static inline void
cs_finish_fragment(struct cs_builder *b, bool increment_frag_completed,
                   struct cs_index first_free_heap_chunk,
                   struct cs_index last_free_heap_chunk,
                   struct cs_async_op async)
{
   cs_emit(b, FINISH_FRAGMENT, I) {
      I.increment_fragment_completed = increment_frag_completed;
      cs_apply_async(I, async);
      I.first_heap_chunk = cs_to_reg64(first_free_heap_chunk);
      I.last_heap_chunk = cs_to_reg64(last_free_heap_chunk);
   }
}

static inline void
cs_add32(struct cs_builder *b, struct cs_index dest, struct cs_index src,
         unsigned imm)
{
   cs_emit(b, ADD_IMMEDIATE32, I) {
      I.destination = cs_to_reg32(dest);
      I.source = cs_to_reg32(src);
      I.immediate = imm;
   }
}

static inline void
cs_add64(struct cs_builder *b, struct cs_index dest, struct cs_index src,
         unsigned imm)
{
   cs_emit(b, ADD_IMMEDIATE64, I) {
      I.destination = cs_to_reg64(dest);
      I.source = cs_to_reg64(src);
      I.immediate = imm;
   }
}

static inline void
cs_umin32(struct cs_builder *b, struct cs_index dest, struct cs_index src1,
          struct cs_index src2)
{
   cs_emit(b, UMIN32, I) {
      I.destination = cs_to_reg32(dest);
      I.source_1 = cs_to_reg32(src1);
      I.source_2 = cs_to_reg32(src2);
   }
}

static inline void
cs_load_to(struct cs_builder *b, struct cs_index dest, struct cs_index address,
           unsigned mask, int offset)
{
   cs_emit(b, LOAD_MULTIPLE, I) {
      I.base_register = cs_to_reg_tuple(dest, util_bitcount(mask));
      I.address = cs_to_reg64(address);
      I.mask = mask;
      I.offset = offset;
   }
}

static inline void
cs_load32_to(struct cs_builder *b, struct cs_index dest,
             struct cs_index address, int offset)
{
   cs_load_to(b, dest, address, BITFIELD_MASK(1), offset);
}

static inline void
cs_load64_to(struct cs_builder *b, struct cs_index dest,
             struct cs_index address, int offset)
{
   cs_load_to(b, dest, address, BITFIELD_MASK(2), offset);
}

static inline void
cs_store(struct cs_builder *b, struct cs_index data, struct cs_index address,
         unsigned mask, int offset)
{
   cs_emit(b, STORE_MULTIPLE, I) {
      I.base_register = cs_to_reg_tuple(data, util_bitcount(mask));
      I.address = cs_to_reg64(address);
      I.mask = mask;
      I.offset = offset;
   }
}

static inline void
cs_store32(struct cs_builder *b, struct cs_index data, struct cs_index address,
           int offset)
{
   cs_store(b, data, address, BITFIELD_MASK(1), offset);
}

static inline void
cs_store64(struct cs_builder *b, struct cs_index data, struct cs_index address,
           int offset)
{
   cs_store(b, data, address, BITFIELD_MASK(2), offset);
}

static inline void
cs_branch(struct cs_builder *b, int offset, enum mali_cs_condition cond,
          struct cs_index val)
{
   cs_emit(b, BRANCH, I) {
      I.offset = offset;
      I.condition = cond;
      I.value = cs_to_reg32(val);
   }
}

/*
 * Select which scoreboard entry will track endpoint tasks and other tasks
 * respectively. Pass to cs_wait to wait later.
 */
static inline void
cs_set_scoreboard_entry(struct cs_builder *b, unsigned ep, unsigned other)
{
   assert(ep < 8 && "invalid slot");
   assert(other < 8 && "invalid slot");

   cs_emit(b, SET_SB_ENTRY, I) {
      I.endpoint_entry = ep;
      I.other_entry = other;
   }
}

static inline void
cs_progress_wait(struct cs_builder *b, unsigned queue, struct cs_index ref)
{
   cs_emit(b, PROGRESS_WAIT, I) {
      I.source = cs_to_reg64(ref);
      I.queue = queue;
   }
}

static inline void
cs_set_exception_handler(struct cs_builder *b,
                         enum mali_cs_exception_type exception_type,
                         struct cs_index address, struct cs_index length)
{
   cs_emit(b, SET_EXCEPTION_HANDLER, I) {
      I.exception_type = exception_type;
      I.address = cs_to_reg64(address);
      I.length = cs_to_reg32(length);
   }
}

static inline void
cs_call(struct cs_builder *b, struct cs_index address, struct cs_index length)
{
   cs_emit(b, CALL, I) {
      I.address = cs_to_reg64(address);
      I.length = cs_to_reg32(length);
   }
}

static inline void
cs_jump(struct cs_builder *b, struct cs_index address, struct cs_index length)
{
   cs_emit(b, JUMP, I) {
      I.address = cs_to_reg64(address);
      I.length = cs_to_reg32(length);
   }
}

enum cs_res_id {
   CS_COMPUTE_RES = BITFIELD_BIT(0),
   CS_FRAG_RES = BITFIELD_BIT(1),
   CS_TILER_RES = BITFIELD_BIT(2),
   CS_IDVS_RES = BITFIELD_BIT(3),
};

static inline void
cs_req_res(struct cs_builder *b, u32 res_mask)
{
   cs_emit(b, REQ_RESOURCE, I) {
      I.compute = res_mask & CS_COMPUTE_RES;
      I.tiler = res_mask & CS_TILER_RES;
      I.idvs = res_mask & CS_IDVS_RES;
      I.fragment = res_mask & CS_FRAG_RES;
   }
}

static inline void
cs_flush_caches(struct cs_builder *b, enum mali_cs_flush_mode l2,
                enum mali_cs_flush_mode lsc, bool other_inv,
                struct cs_index flush_id, struct cs_async_op async)
{
   cs_emit(b, FLUSH_CACHE2, I) {
      I.l2_flush_mode = l2;
      I.lsc_flush_mode = lsc;
      I.other_invalidate = other_inv;
      I.latest_flush_id = cs_to_reg32(flush_id);
      cs_apply_async(I, async);
   }
}

#define CS_SYNC_OPS(__cnt_width)                                               \
   static inline void cs_sync##__cnt_width##_set(                              \
      struct cs_builder *b, bool propagate_error,                              \
      enum mali_cs_sync_scope scope, struct cs_index val,                      \
      struct cs_index addr, struct cs_async_op async)                          \
   {                                                                           \
      cs_emit(b, SYNC_SET##__cnt_width, I) {                                   \
         I.error_propagate = propagate_error;                                  \
         I.scope = scope;                                                      \
         I.data = cs_to_reg##__cnt_width(val);                                 \
         I.address = cs_to_reg64(addr);                                        \
         cs_apply_async(I, async);                                             \
      }                                                                        \
   }                                                                           \
                                                                               \
   static inline void cs_sync##__cnt_width##_add(                              \
      struct cs_builder *b, bool propagate_error,                              \
      enum mali_cs_sync_scope scope, struct cs_index val,                      \
      struct cs_index addr, struct cs_async_op async)                          \
   {                                                                           \
      cs_emit(b, SYNC_ADD##__cnt_width, I) {                                   \
         I.error_propagate = propagate_error;                                  \
         I.scope = scope;                                                      \
         I.data = cs_to_reg##__cnt_width(val);                                 \
         I.address = cs_to_reg64(addr);                                        \
         cs_apply_async(I, async);                                             \
      }                                                                        \
   }                                                                           \
                                                                               \
   static inline void cs_sync##__cnt_width##_wait(                             \
      struct cs_builder *b, bool reject_error, enum mali_cs_condition cond,    \
      struct cs_index ref, struct cs_index addr)                               \
   {                                                                           \
      assert(cond == MALI_CS_CONDITION_LEQUAL ||                               \
             cond == MALI_CS_CONDITION_GREATER);                               \
      cs_emit(b, SYNC_WAIT##__cnt_width, I) {                                  \
         I.error_reject = reject_error;                                        \
         I.condition = cond;                                                   \
         I.data = cs_to_reg##__cnt_width(ref);                                 \
         I.address = cs_to_reg64(addr);                                        \
      }                                                                        \
   }

CS_SYNC_OPS(32)
CS_SYNC_OPS(64)

static inline void
cs_store_state(struct cs_builder *b, struct cs_index address, int offset,
               enum mali_cs_state state, struct cs_async_op async)
{
   cs_emit(b, STORE_STATE, I) {
      I.offset = offset;
      I.state = state;
      I.address = cs_to_reg64(address);
      cs_apply_async(I, async);
   }
}

static inline void
cs_prot_region(struct cs_builder *b, unsigned size)
{
   cs_emit(b, PROT_REGION, I) {
      I.size = size;
   }
}

static inline void
cs_progress_store(struct cs_builder *b, struct cs_index src)
{
   cs_emit(b, PROGRESS_STORE, I)
      I.source = cs_to_reg64(src);
}

static inline void
cs_progress_load(struct cs_builder *b, struct cs_index dst)
{
   cs_emit(b, PROGRESS_LOAD, I)
      I.destination = cs_to_reg64(dst);
}

static inline void
cs_run_compute_indirect(struct cs_builder *b, unsigned wg_per_task,
                        bool progress_inc, struct cs_shader_res_sel res_sel)
{
   cs_emit(b, RUN_COMPUTE_INDIRECT, I) {
      I.workgroups_per_task = wg_per_task;
      I.progress_increment = progress_inc;
      I.srt_select = res_sel.srt;
      I.spd_select = res_sel.spd;
      I.tsd_select = res_sel.tsd;
      I.fau_select = res_sel.fau;
   }
}

static inline void
cs_error_barrier(struct cs_builder *b)
{
   cs_emit(b, ERROR_BARRIER, _)
      ;
}

static inline void
cs_heap_set(struct cs_builder *b, struct cs_index address)
{
   cs_emit(b, HEAP_SET, I) {
      I.address = cs_to_reg64(address);
   }
}

static inline void
cs_heap_operation(struct cs_builder *b, enum mali_cs_heap_operation operation,
                  struct cs_async_op async)
{
   cs_emit(b, HEAP_OPERATION, I) {
      I.operation = operation;
      cs_apply_async(I, async);
   }
}

static inline void
cs_vt_start(struct cs_builder *b, struct cs_async_op async)
{
   cs_heap_operation(b, MALI_CS_HEAP_OPERATION_VERTEX_TILER_STARTED, async);
}

static inline void
cs_vt_end(struct cs_builder *b, struct cs_async_op async)
{
   cs_heap_operation(b, MALI_CS_HEAP_OPERATION_VERTEX_TILER_COMPLETED, async);
}

static inline void
cs_frag_end(struct cs_builder *b, struct cs_async_op async)
{
   cs_heap_operation(b, MALI_CS_HEAP_OPERATION_FRAGMENT_COMPLETED, async);
}

static inline void
cs_trace_point(struct cs_builder *b, struct cs_index regs,
               struct cs_async_op async)
{
   cs_emit(b, TRACE_POINT, I) {
      I.base_register = cs_to_reg_tuple(regs, regs.size);
      I.register_count = regs.size;
      cs_apply_async(I, async);
   }
}
