/*
 * Copyright © 2012 Intel Corporation
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
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#ifndef ELK_CFG_H
#define ELK_CFG_H

#include "elk_ir.h"
#ifdef __cplusplus
#include "elk_ir_analysis.h"
#endif

struct elk_bblock_t;

/**
 * CFG edge types.
 *
 * A logical edge represents a potential control flow path of the original
 * scalar program, while a physical edge represents a control flow path that
 * may not have existed in the original program but was introduced during
 * vectorization in order to implement divergent control flow of different
 * shader invocations within the same SIMD thread.
 *
 * All logical edges in the CFG are considered to be physical edges but not
 * the other way around -- I.e. the logical CFG is a subset of the physical
 * one.
 */
enum bblock_link_kind {
   bblock_link_logical = 0,
   bblock_link_physical
};

struct elk_bblock_link {
#ifdef __cplusplus
   DECLARE_RALLOC_CXX_OPERATORS(elk_bblock_link)

   elk_bblock_link(elk_bblock_t *block, enum bblock_link_kind kind)
      : block(block), kind(kind)
   {
   }
#endif

   struct exec_node link;
   struct elk_bblock_t *block;

   /* Type of this CFG edge.  Because bblock_link_logical also implies
    * bblock_link_physical, the proper way to test for membership of edge 'l'
    * in CFG kind 'k' is 'l.kind <= k'.
    */
   enum bblock_link_kind kind;
};

struct elk_backend_shader;
struct elk_cfg_t;

struct elk_bblock_t {
#ifdef __cplusplus
   DECLARE_RALLOC_CXX_OPERATORS(elk_bblock_t)

   explicit elk_bblock_t(elk_cfg_t *cfg);

   void add_successor(void *mem_ctx, elk_bblock_t *successor,
                      enum bblock_link_kind kind);
   bool is_predecessor_of(const elk_bblock_t *block,
                          enum bblock_link_kind kind) const;
   bool is_successor_of(const elk_bblock_t *block,
                        enum bblock_link_kind kind) const;
   bool can_combine_with(const elk_bblock_t *that) const;
   void combine_with(elk_bblock_t *that);
   void dump(FILE *file = stderr) const;

   elk_backend_instruction *start();
   const elk_backend_instruction *start() const;
   elk_backend_instruction *end();
   const elk_backend_instruction *end() const;

   elk_bblock_t *next();
   const elk_bblock_t *next() const;
   elk_bblock_t *prev();
   const elk_bblock_t *prev() const;

   bool starts_with_control_flow() const;
   bool ends_with_control_flow() const;

   elk_backend_instruction *first_non_control_flow_inst();
   elk_backend_instruction *last_non_control_flow_inst();

private:
   /**
    * \sa unlink_parents, unlink_children
    */
   void unlink_list(exec_list *);

public:
   void unlink_parents()
   {
      unlink_list(&parents);
   }

   void unlink_children()
   {
      unlink_list(&children);
   }
#endif

   struct exec_node link;
   struct elk_cfg_t *cfg;

   int start_ip;
   int end_ip;

   /**
    * Change in end_ip since the last time IPs of later blocks were updated.
    */
   int end_ip_delta;

   struct exec_list instructions;
   struct exec_list parents;
   struct exec_list children;
   int num;
};

static inline struct elk_backend_instruction *
bblock_start(struct elk_bblock_t *block)
{
   return (struct elk_backend_instruction *)exec_list_get_head(&block->instructions);
}

static inline const struct elk_backend_instruction *
bblock_start_const(const struct elk_bblock_t *block)
{
   return (const struct elk_backend_instruction *)exec_list_get_head_const(&block->instructions);
}

static inline struct elk_backend_instruction *
bblock_end(struct elk_bblock_t *block)
{
   return (struct elk_backend_instruction *)exec_list_get_tail(&block->instructions);
}

static inline const struct elk_backend_instruction *
bblock_end_const(const struct elk_bblock_t *block)
{
   return (const struct elk_backend_instruction *)exec_list_get_tail_const(&block->instructions);
}

static inline struct elk_bblock_t *
bblock_next(struct elk_bblock_t *block)
{
   if (exec_node_is_tail_sentinel(block->link.next))
      return NULL;

   return (struct elk_bblock_t *)block->link.next;
}

static inline const struct elk_bblock_t *
bblock_next_const(const struct elk_bblock_t *block)
{
   if (exec_node_is_tail_sentinel(block->link.next))
      return NULL;

   return (const struct elk_bblock_t *)block->link.next;
}

static inline struct elk_bblock_t *
bblock_prev(struct elk_bblock_t *block)
{
   if (exec_node_is_head_sentinel(block->link.prev))
      return NULL;

   return (struct elk_bblock_t *)block->link.prev;
}

static inline const struct elk_bblock_t *
bblock_prev_const(const struct elk_bblock_t *block)
{
   if (exec_node_is_head_sentinel(block->link.prev))
      return NULL;

   return (const struct elk_bblock_t *)block->link.prev;
}

static inline bool
bblock_starts_with_control_flow(const struct elk_bblock_t *block)
{
   enum elk_opcode op = bblock_start_const(block)->opcode;
   return op == ELK_OPCODE_DO || op == ELK_OPCODE_ENDIF;
}

static inline bool
bblock_ends_with_control_flow(const struct elk_bblock_t *block)
{
   enum elk_opcode op = bblock_end_const(block)->opcode;
   return op == ELK_OPCODE_IF ||
          op == ELK_OPCODE_ELSE ||
          op == ELK_OPCODE_WHILE ||
          op == ELK_OPCODE_BREAK ||
          op == ELK_OPCODE_CONTINUE;
}

static inline struct elk_backend_instruction *
bblock_first_non_control_flow_inst(struct elk_bblock_t *block)
{
   struct elk_backend_instruction *inst = bblock_start(block);
   if (bblock_starts_with_control_flow(block))
#ifdef __cplusplus
      inst = (struct elk_backend_instruction *)inst->next;
#else
      inst = (struct elk_backend_instruction *)inst->link.next;
#endif
   return inst;
}

static inline struct elk_backend_instruction *
bblock_last_non_control_flow_inst(struct elk_bblock_t *block)
{
   struct elk_backend_instruction *inst = bblock_end(block);
   if (bblock_ends_with_control_flow(block))
#ifdef __cplusplus
      inst = (struct elk_backend_instruction *)inst->prev;
#else
      inst = (struct elk_backend_instruction *)inst->link.prev;
#endif
   return inst;
}

#ifdef __cplusplus
inline elk_backend_instruction *
elk_bblock_t::start()
{
   return bblock_start(this);
}

inline const elk_backend_instruction *
elk_bblock_t::start() const
{
   return bblock_start_const(this);
}

inline elk_backend_instruction *
elk_bblock_t::end()
{
   return bblock_end(this);
}

inline const elk_backend_instruction *
elk_bblock_t::end() const
{
   return bblock_end_const(this);
}

inline elk_bblock_t *
elk_bblock_t::next()
{
   return bblock_next(this);
}

inline const elk_bblock_t *
elk_bblock_t::next() const
{
   return bblock_next_const(this);
}

inline elk_bblock_t *
elk_bblock_t::prev()
{
   return bblock_prev(this);
}

inline const elk_bblock_t *
elk_bblock_t::prev() const
{
   return bblock_prev_const(this);
}

inline bool
elk_bblock_t::starts_with_control_flow() const
{
   return bblock_starts_with_control_flow(this);
}

inline bool
elk_bblock_t::ends_with_control_flow() const
{
   return bblock_ends_with_control_flow(this);
}

inline elk_backend_instruction *
elk_bblock_t::first_non_control_flow_inst()
{
   return bblock_first_non_control_flow_inst(this);
}

inline elk_backend_instruction *
elk_bblock_t::last_non_control_flow_inst()
{
   return bblock_last_non_control_flow_inst(this);
}
#endif

struct elk_cfg_t {
#ifdef __cplusplus
   DECLARE_RALLOC_CXX_OPERATORS(elk_cfg_t)

   elk_cfg_t(const elk_backend_shader *s, exec_list *instructions);
   ~elk_cfg_t();

   void remove_block(elk_bblock_t *block);

   elk_bblock_t *first_block();
   const elk_bblock_t *first_block() const;
   elk_bblock_t *last_block();
   const elk_bblock_t *last_block() const;

   elk_bblock_t *new_block();
   void set_next_block(elk_bblock_t **cur, elk_bblock_t *block, int ip);
   void make_block_array();

   void dump(FILE *file = stderr);
   void dump_cfg();

#ifdef NDEBUG
   void validate(UNUSED const char *stage_abbrev) { }
#else
   void validate(const char *stage_abbrev);
#endif

   /**
    * Propagate elk_bblock_t::end_ip_delta data through the CFG.
    */
   inline void adjust_block_ips();

#endif
   const struct elk_backend_shader *s;
   void *mem_ctx;

   /** Ordered list (by ip) of basic blocks */
   struct exec_list block_list;
   struct elk_bblock_t **blocks;
   int num_blocks;
};

static inline struct elk_bblock_t *
cfg_first_block(struct elk_cfg_t *cfg)
{
   return (struct elk_bblock_t *)exec_list_get_head(&cfg->block_list);
}

static inline const struct elk_bblock_t *
cfg_first_block_const(const struct elk_cfg_t *cfg)
{
   return (const struct elk_bblock_t *)exec_list_get_head_const(&cfg->block_list);
}

static inline struct elk_bblock_t *
cfg_last_block(struct elk_cfg_t *cfg)
{
   return (struct elk_bblock_t *)exec_list_get_tail(&cfg->block_list);
}

static inline const struct elk_bblock_t *
cfg_last_block_const(const struct elk_cfg_t *cfg)
{
   return (const struct elk_bblock_t *)exec_list_get_tail_const(&cfg->block_list);
}

#ifdef __cplusplus
inline elk_bblock_t *
elk_cfg_t::first_block()
{
   return cfg_first_block(this);
}

const inline elk_bblock_t *
elk_cfg_t::first_block() const
{
   return cfg_first_block_const(this);
}

inline elk_bblock_t *
elk_cfg_t::last_block()
{
   return cfg_last_block(this);
}

const inline elk_bblock_t *
elk_cfg_t::last_block() const
{
   return cfg_last_block_const(this);
}
#endif

/* Note that this is implemented with a double for loop -- break will
 * break from the inner loop only!
 */
#define foreach_block_and_inst(__block, __type, __inst, __cfg) \
   foreach_block (__block, __cfg)                              \
      foreach_inst_in_block (__type, __inst, __block)

/* Note that this is implemented with a double for loop -- break will
 * break from the inner loop only!
 */
#define foreach_block_and_inst_safe(__block, __type, __inst, __cfg) \
   foreach_block_safe (__block, __cfg)                              \
      foreach_inst_in_block_safe (__type, __inst, __block)

#define foreach_block(__block, __cfg)                          \
   foreach_list_typed (elk_bblock_t, __block, link, &(__cfg)->block_list)

#define foreach_block_reverse(__block, __cfg)                  \
   foreach_list_typed_reverse (elk_bblock_t, __block, link, &(__cfg)->block_list)

#define foreach_block_safe(__block, __cfg)                     \
   foreach_list_typed_safe (elk_bblock_t, __block, link, &(__cfg)->block_list)

#define foreach_block_reverse_safe(__block, __cfg)             \
   foreach_list_typed_reverse_safe (elk_bblock_t, __block, link, &(__cfg)->block_list)

#define foreach_inst_in_block(__type, __inst, __block)         \
   foreach_in_list(__type, __inst, &(__block)->instructions)

#define foreach_inst_in_block_safe(__type, __inst, __block)    \
   for (__type *__inst = (__type *)__block->instructions.head_sentinel.next, \
               *__next = (__type *)__inst->next;               \
        __next != NULL;                                        \
        __inst = __next,                                       \
        __next = (__type *)__next->next)

#define foreach_inst_in_block_reverse(__type, __inst, __block) \
   foreach_in_list_reverse(__type, __inst, &(__block)->instructions)

#define foreach_inst_in_block_reverse_safe(__type, __inst, __block) \
   foreach_in_list_reverse_safe(__type, __inst, &(__block)->instructions)

#define foreach_inst_in_block_starting_from(__type, __scan_inst, __inst) \
   for (__type *__scan_inst = (__type *)__inst->next;          \
        !__scan_inst->is_tail_sentinel();                      \
        __scan_inst = (__type *)__scan_inst->next)

#define foreach_inst_in_block_reverse_starting_from(__type, __scan_inst, __inst) \
   for (__type *__scan_inst = (__type *)__inst->prev;          \
        !__scan_inst->is_head_sentinel();                      \
        __scan_inst = (__type *)__scan_inst->prev)

#ifdef __cplusplus
inline void
elk_cfg_t::adjust_block_ips()
{
   int delta = 0;

   foreach_block(block, this) {
      block->start_ip += delta;
      block->end_ip += delta;

      delta += block->end_ip_delta;

      block->end_ip_delta = 0;
   }
}

namespace elk {
   /**
    * Immediate dominator tree analysis of a shader.
    */
   struct idom_tree {
      idom_tree(const elk_backend_shader *s);
      ~idom_tree();

      bool
      validate(const elk_backend_shader *) const
      {
         /* FINISHME */
         return true;
      }

      analysis_dependency_class
      dependency_class() const
      {
         return DEPENDENCY_BLOCKS;
      }

      const elk_bblock_t *
      parent(const elk_bblock_t *b) const
      {
         assert(unsigned(b->num) < num_parents);
         return parents[b->num];
      }

      elk_bblock_t *
      parent(elk_bblock_t *b) const
      {
         assert(unsigned(b->num) < num_parents);
         return parents[b->num];
      }

      elk_bblock_t *
      intersect(elk_bblock_t *b1, elk_bblock_t *b2) const;

      void
      dump() const;

   private:
      unsigned num_parents;
      elk_bblock_t **parents;
   };
}
#endif

#endif /* ELK_CFG_H */
