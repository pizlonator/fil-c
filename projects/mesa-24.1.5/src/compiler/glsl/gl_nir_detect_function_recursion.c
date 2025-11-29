/*
 * Copyright © 2011 Intel Corporation
 * Copyright © 2024 Valve Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * Determine whether a shader contains static recursion.
 *
 * Consider the (possibly disjoint) graph of function calls in a shader.  If a
 * program contains recursion, this graph will contain a cycle.  If a function
 * is part of a cycle, it will have a caller and it will have a callee (it
 * calls another function).
 *
 * To detect recursion, the function call graph is constructed.  The graph is
 * repeatedly reduced by removing any function that either has no callees
 * (leaf functions) or has no caller.  Eventually the only functions that
 * remain will be the functions in the cycles.
 *
 * The GLSL spec is a bit wishy-washy about recursion.
 *
 * From page 39 (page 45 of the PDF) of the GLSL 1.10 spec:
 *
 *     "Behavior is undefined if recursion is used. Recursion means having any
 *     function appearing more than once at any one time in the run-time stack
 *     of function calls. That is, a function may not call itself either
 *     directly or indirectly. Compilers may give diagnostic messages when
 *     this is detectable at compile time, but not all such cases can be
 *     detected at compile time."
 *
 * From page 79 (page 85 of the PDF):
 *
 *     "22) Should recursion be supported?
 *
 *      DISCUSSION: Probably not necessary, but another example of limiting
 *      the language based on how it would directly map to hardware. One
 *      thought is that recursion would benefit ray tracing shaders. On the
 *      other hand, many recursion operations can also be implemented with the
 *      user managing the recursion through arrays. RenderMan doesn't support
 *      recursion. This could be added at a later date, if it proved to be
 *      necessary.
 *
 *      RESOLVED on September 10, 2002: Implementations are not required to
 *      support recursion.
 *
 *      CLOSED on September 10, 2002."
 *
 * From page 79 (page 85 of the PDF):
 *
 *     "56) Is it an error for an implementation to support recursion if the
 *     specification says recursion is not supported?
 *
 *     ADDED on September 10, 2002.
 *
 *     DISCUSSION: This issues is related to Issue (22). If we say that
 *     recursion (or some other piece of functionality) is not supported, is
 *     it an error for an implementation to support it? Perhaps the
 *     specification should remain silent on these kind of things so that they
 *     could be gracefully added later as an extension or as part of the
 *     standard.
 *
 *     RESOLUTION: Languages, in general, have programs that are not
 *     well-formed in ways a compiler cannot detect. Portability is only
 *     ensured for well-formed programs. Detecting recursion is an example of
 *     this. The language will say a well-formed program may not recurse, but
 *     compilers are not forced to detect that recursion may happen.
 *
 *     CLOSED: November 29, 2002."
 *
 * In GLSL 1.10 the behavior of recursion is undefined.  Compilers don't have
 * to reject shaders (at compile-time or link-time) that contain recursion.
 * Instead they could work, or crash.
 *
 * From page 44 (page 50 of the PDF) of the GLSL 1.20 spec:
 *
 *     "Recursion is not allowed, not even statically. Static recursion is
 *     present if the static function call graph of the program contains
 *     cycles."
 *
 * This langauge clears things up a bit, but it still leaves a lot of
 * questions unanswered.
 *
 *     - Is the error generated at compile-time or link-time?
 *
 *     - Is it an error to have a recursive function that is never statically
 *       called by main or any function called directly or indirectly by main?
 *       Technically speaking, such a function is not in the "static function
 *       call graph of the program" at all.
 *
 * \bug
 * If a shader has multiple cycles, this algorithm may erroneously complain
 * about functions that aren't in any cycle, but are in the part of the call
 * tree that connects them.  For example, if the call graph consists of a
 * cycle between A and B, and a cycle between D and E, and B also calls C
 * which calls D, then this algorithm will report C as a function which "has
 * static recursion" even though it is not part of any cycle.
 *
 * A better algorithm for cycle detection that doesn't have this drawback can
 * be found here:
 *
 * http://en.wikipedia.org/wiki/Tarjan%E2%80%99s_strongly_connected_components_algorithm
 *
 */

#include "gl_nir_linker.h"
#include "linker_util.h"
#include "nir.h"
#include "util/hash_table.h"

struct function_state {
   nir_function *sig;

   /** List of functions called by this function. */
   struct list_head callees;

   /** List of functions that call this function. */
   struct list_head callers;
};

struct has_recursion_state {
   struct hash_table *function_hash;
   bool progress;
};

struct call_node {
   struct list_head call_link;

   struct function_state *func;
};

static struct function_state *
get_function(void *mem_ctx, nir_function *function_sig,
             struct hash_table *function_hash)
{
      struct function_state *f;
      struct hash_entry *entry =
         _mesa_hash_table_search(function_hash, function_sig);
      if (entry == NULL) {
         f = ralloc(mem_ctx, struct function_state);
         f->sig = function_sig;
         list_inithead(&f->callers);
         list_inithead(&f->callees);
         _mesa_hash_table_insert(function_hash, function_sig, f);
      } else {
         f = (struct function_state *) entry->data;
      }

      return f;
}

static void
find_recursion(void *mem_ctx, nir_shader *shader,
               struct hash_table *function_hash)
{
   nir_foreach_function_impl(impl, shader) {
      struct function_state *current =
          get_function(mem_ctx, impl->function, function_hash);

      nir_foreach_block(block, impl) {
         nir_foreach_instr(instr, block) {
            if (instr->type == nir_instr_type_call) {
               nir_call_instr *call = nir_instr_as_call(instr);
               struct function_state *target =
                  get_function(mem_ctx, call->callee, function_hash);

               /* Create a link from the caller to the callee. */
               struct call_node *node = ralloc(mem_ctx, struct call_node);
               node->func = target;
               list_addtail(&node->call_link, &current->callees);

               /* Create a link from the callee to the caller. */
               node = ralloc(mem_ctx, struct call_node);
               node->func = current;
               list_addtail(&node->call_link, &target->callers);
            }
         }
      }
   }
}

/**
 * Generate a ralloced string representing the prototype of the function.
 */
static char *
prototype_string(nir_function *sig)
{
   char *str = NULL;

   unsigned i = 0;
   if (sig->params && sig->params[0].is_return) {
      str = ralloc_asprintf(NULL, "%s ",
                            glsl_get_type_name(sig->params[i].type));
      i = 1;
   }

   ralloc_asprintf_append(&str, "%s(", sig->name);

   const char *comma = "";
   for ( ; i < sig->num_params; i++) {
      ralloc_asprintf_append(&str, "%s%s", comma,
                             glsl_get_type_name(sig->params[i].type));
      comma = ", ";
   }

   ralloc_strcat(&str, ")");
   return str;
}

static void
destroy_links(struct list_head *list, struct function_state *f)
{
   list_for_each_entry_safe(struct call_node, n, list, call_link) {
      /* If this is the right function, remove it.  Note that the loop cannot
       * terminate now.  There can be multiple links to a function if it is
       * either called multiple times or calls multiple times.
       */
      if (n->func == f)
         list_del(&n->call_link);
   }
}

/**
 * Remove a function if it has either no in or no out links
 */
static void
remove_unlinked_functions(const void *key, void *data, void *closure)
{
   struct has_recursion_state *state = (struct has_recursion_state *) closure;
   struct function_state *f = (struct function_state *) data;

   if (list_is_empty(&f->callers) || list_is_empty(&f->callees)) {
      list_for_each_entry_safe(struct call_node, n, &f->callers, call_link) {
         list_del(&n->call_link);
         ralloc_free(n);
      }

      list_for_each_entry_safe(struct call_node, n, &f->callees, call_link) {
         destroy_links(&n->func->callers, f);
      }

      struct hash_entry *entry =
         _mesa_hash_table_search(state->function_hash, key);
      _mesa_hash_table_remove(state->function_hash, entry);
      state->progress = true;
   }
}

static void
emit_errors_linked(const void *key, void *data, void *closure)
{
   struct gl_shader_program *prog =
      (struct gl_shader_program *) closure;
   struct function_state *f = (struct function_state *) data;

   (void) key;

   char *proto = prototype_string(f->sig);

   linker_error(prog, "function `%s' has static recursion.\n", proto);
   ralloc_free(proto);
}

void
gl_nir_detect_recursion_linked(struct gl_shader_program *prog,
                               nir_shader *shader)
{
   void *mem_ctx = ralloc_context(NULL);
   struct hash_table *function_hash =
      _mesa_pointer_hash_table_create(mem_ctx);

   /* Collect all of the information about which functions call which other
    * functions.
    */
   find_recursion(mem_ctx, shader, function_hash);

   /* Remove from the set all of the functions that either have no caller or
    * call no other functions.  Repeat until no functions are removed.
    */
   struct has_recursion_state state;
   state.function_hash = function_hash;
   do {
      state.progress = false;
      hash_table_call_foreach(state.function_hash, remove_unlinked_functions,
                              &state);
   } while (state.progress);


   /* At this point any functions still in the hash must be part of a cycle.
    */
   hash_table_call_foreach(state.function_hash, emit_errors_linked, prog);

   ralloc_free(mem_ctx);
}
