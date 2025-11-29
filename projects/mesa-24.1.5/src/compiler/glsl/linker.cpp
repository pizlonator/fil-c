/*
 * Copyright © 2010 Intel Corporation
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
 * \file linker.cpp
 * GLSL linker implementation
 *
 * Given a set of shaders that are to be linked to generate a final program,
 * there are three distinct stages.
 *
 * In the first stage shaders are partitioned into groups based on the shader
 * type.  All shaders of a particular type (e.g., vertex shaders) are linked
 * together.
 *
 *   - Undefined references in each shader are resolve to definitions in
 *     another shader.
 *   - Types and qualifiers of uniforms, outputs, and global variables defined
 *     in multiple shaders with the same name are verified to be the same.
 *   - Initializers for uniforms and global variables defined
 *     in multiple shaders with the same name are verified to be the same.
 *
 * The result, in the terminology of the GLSL spec, is a set of shader
 * executables for each processing unit.
 *
 * After the first stage is complete, a series of semantic checks are performed
 * on each of the shader executables.
 *
 *   - Each shader executable must define a \c main function.
 *   - Each vertex shader executable must write to \c gl_Position.
 *   - Each fragment shader executable must write to either \c gl_FragData or
 *     \c gl_FragColor.
 *
 * In the final stage individual shader executables are linked to create a
 * complete exectuable.
 *
 *   - Types of uniforms defined in multiple shader stages with the same name
 *     are verified to be the same.
 *   - Initializers for uniforms defined in multiple shader stages with the
 *     same name are verified to be the same.
 *   - Types and qualifiers of outputs defined in one stage are verified to
 *     be the same as the types and qualifiers of inputs defined with the same
 *     name in a later stage.
 *
 * \author Ian Romanick <ian.d.romanick@intel.com>
 */

#include <ctype.h>
#include "util/strndup.h"
#include "glsl_symbol_table.h"
#include "glsl_parser_extras.h"
#include "ir.h"
#include "nir.h"
#include "program.h"
#include "program/prog_instruction.h"
#include "program/program.h"
#include "util/mesa-sha1.h"
#include "util/set.h"
#include "string_to_uint_map.h"
#include "linker.h"
#include "linker_util.h"
#include "ir_optimization.h"
#include "ir_rvalue_visitor.h"
#include "ir_uniform.h"
#include "builtin_functions.h"
#include "shader_cache.h"
#include "util/u_string.h"
#include "util/u_math.h"


#include "main/shaderobj.h"
#include "main/enums.h"
#include "main/mtypes.h"
#include "main/context.h"


namespace {

/**
 * A visitor helper that provides methods for updating the types of
 * ir_dereferences.  Classes that update variable types (say, updating
 * array sizes) will want to use this so that dereference types stay in sync.
 */
class deref_type_updater : public ir_hierarchical_visitor {
public:
   virtual ir_visitor_status visit(ir_dereference_variable *ir)
   {
      ir->type = ir->var->type;
      return visit_continue;
   }

   virtual ir_visitor_status visit_leave(ir_dereference_array *ir)
   {
      const glsl_type *const vt = ir->array->type;
      if (glsl_type_is_array(vt))
         ir->type = vt->fields.array;
      return visit_continue;
   }

   virtual ir_visitor_status visit_leave(ir_dereference_record *ir)
   {
      ir->type = ir->record->type->fields.structure[ir->field_idx].type;
      return visit_continue;
   }
};


class array_resize_visitor : public deref_type_updater {
public:
   using deref_type_updater::visit;

   unsigned num_vertices;
   gl_shader_program *prog;
   gl_shader_stage stage;

   array_resize_visitor(unsigned num_vertices,
                        gl_shader_program *prog,
                        gl_shader_stage stage)
   {
      this->num_vertices = num_vertices;
      this->prog = prog;
      this->stage = stage;
   }

   virtual ~array_resize_visitor()
   {
      /* empty */
   }

   virtual ir_visitor_status visit(ir_variable *var)
   {
      if (!glsl_type_is_array(var->type) || var->data.mode != ir_var_shader_in ||
          var->data.patch)
         return visit_continue;

      unsigned size = var->type->length;

      if (stage == MESA_SHADER_GEOMETRY) {
         /* Generate a link error if the shader has declared this array with
          * an incorrect size.
          */
         if (!var->data.implicit_sized_array &&
             size && size != this->num_vertices) {
            linker_error(this->prog, "size of array %s declared as %u, "
                         "but number of input vertices is %u\n",
                         var->name, size, this->num_vertices);
            return visit_continue;
         }

         /* Generate a link error if the shader attempts to access an input
          * array using an index too large for its actual size assigned at
          * link time.
          */
         if (var->data.max_array_access >= (int)this->num_vertices) {
            linker_error(this->prog, "%s shader accesses element %i of "
                         "%s, but only %i input vertices\n",
                         _mesa_shader_stage_to_string(this->stage),
                         var->data.max_array_access, var->name, this->num_vertices);
            return visit_continue;
         }
      }

      var->type = glsl_array_type(var->type->fields.array,
                                  this->num_vertices, 0);
      var->data.max_array_access = this->num_vertices - 1;

      return visit_continue;
   }
};

class array_length_to_const_visitor : public ir_rvalue_visitor {
public:
   array_length_to_const_visitor()
   {
      this->progress = false;
   }

   virtual ~array_length_to_const_visitor()
   {
      /* empty */
   }

   bool progress;

   virtual void handle_rvalue(ir_rvalue **rvalue)
   {
      if (*rvalue == NULL || (*rvalue)->ir_type != ir_type_expression)
         return;

      ir_expression *expr = (*rvalue)->as_expression();
      if (expr) {
         if (expr->operation == ir_unop_implicitly_sized_array_length) {
            assert(!glsl_type_is_unsized_array(expr->operands[0]->type));
            ir_constant *constant = new(expr)
               ir_constant(glsl_array_size(expr->operands[0]->type));
            if (constant) {
               *rvalue = constant;
            }
         }
      }
   }
};

} /* anonymous namespace */

void
linker_error(gl_shader_program *prog, const char *fmt, ...)
{
   va_list ap;

   ralloc_strcat(&prog->data->InfoLog, "error: ");
   va_start(ap, fmt);
   ralloc_vasprintf_append(&prog->data->InfoLog, fmt, ap);
   va_end(ap);

   prog->data->LinkStatus = LINKING_FAILURE;
}


void
linker_warning(gl_shader_program *prog, const char *fmt, ...)
{
   va_list ap;

   ralloc_strcat(&prog->data->InfoLog, "warning: ");
   va_start(ap, fmt);
   ralloc_vasprintf_append(&prog->data->InfoLog, fmt, ap);
   va_end(ap);

}

bool
validate_intrastage_arrays(struct gl_shader_program *prog,
                           ir_variable *const var,
                           ir_variable *const existing,
                           bool match_precision)
{
   /* Consider the types to be "the same" if both types are arrays
    * of the same type and one of the arrays is implicitly sized.
    * In addition, set the type of the linked variable to the
    * explicitly sized array.
    */
   if (glsl_type_is_array(var->type) && glsl_type_is_array(existing->type)) {
      const glsl_type *no_array_var = var->type->fields.array;
      const glsl_type *no_array_existing = existing->type->fields.array;
      bool type_matches;

      type_matches = (match_precision ?
                      no_array_var == no_array_existing :
                      glsl_type_compare_no_precision(no_array_var, no_array_existing));

      if (type_matches &&
          ((var->type->length == 0)|| (existing->type->length == 0))) {
         if (var->type->length != 0) {
            if ((int)var->type->length <= existing->data.max_array_access) {
               linker_error(prog, "%s `%s' declared as type "
                           "`%s' but outermost dimension has an index"
                           " of `%i'\n",
                           mode_string(var),
                           var->name, glsl_get_type_name(var->type),
                           existing->data.max_array_access);
            }
            existing->type = var->type;
            return true;
         } else if (existing->type->length != 0) {
            if((int)existing->type->length <= var->data.max_array_access &&
               !existing->data.from_ssbo_unsized_array) {
               linker_error(prog, "%s `%s' declared as type "
                           "`%s' but outermost dimension has an index"
                           " of `%i'\n",
                           mode_string(var),
                           var->name, glsl_get_type_name(existing->type),
                           var->data.max_array_access);
            }
            return true;
         }
      }
   }
   return false;
}


/**
 * Perform validation of global variables used across multiple shaders
 */
static void
cross_validate_globals(const struct gl_constants *consts,
                       struct gl_shader_program *prog,
                       struct exec_list *ir, glsl_symbol_table *variables,
                       bool uniforms_only)
{
   foreach_in_list(ir_instruction, node, ir) {
      ir_variable *const var = node->as_variable();

      if (var == NULL)
         continue;

      if (uniforms_only && (var->data.mode != ir_var_uniform && var->data.mode != ir_var_shader_storage))
         continue;

      /* don't cross validate subroutine uniforms */
      if (glsl_contains_subroutine(var->type))
         continue;

      /* Don't cross validate interface instances. These are only relevant
       * inside a shader. The cross validation is done at the Interface Block
       * name level.
       */
      if (var->is_interface_instance())
         continue;

      /* Don't cross validate temporaries that are at global scope.  These
       * will eventually get pulled into the shaders 'main'.
       */
      if (var->data.mode == ir_var_temporary)
         continue;

      /* If a global with this name has already been seen, verify that the
       * new instance has the same type.  In addition, if the globals have
       * initializers, the values of the initializers must be the same.
       */
      ir_variable *const existing = variables->get_variable(var->name);
      if (existing != NULL) {
         /* Check if types match. */
         if (var->type != existing->type) {
            if (!validate_intrastage_arrays(prog, var, existing)) {
               /* If it is an unsized array in a Shader Storage Block,
                * two different shaders can access to different elements.
                * Because of that, they might be converted to different
                * sized arrays, then check that they are compatible but
                * ignore the array size.
                */
               if (!(var->data.mode == ir_var_shader_storage &&
                     var->data.from_ssbo_unsized_array &&
                     existing->data.mode == ir_var_shader_storage &&
                     existing->data.from_ssbo_unsized_array &&
                     var->type->gl_type == existing->type->gl_type)) {
                  linker_error(prog, "%s `%s' declared as type "
                                 "`%s' and type `%s'\n",
                                 mode_string(var),
                                 var->name, glsl_get_type_name(var->type),
                                 glsl_get_type_name(existing->type));
                  return;
               }
            }
         }

         if (var->data.explicit_location) {
            if (existing->data.explicit_location
                && (var->data.location != existing->data.location)) {
               linker_error(prog, "explicit locations for %s "
                            "`%s' have differing values\n",
                            mode_string(var), var->name);
               return;
            }

            if (var->data.location_frac != existing->data.location_frac) {
               linker_error(prog, "explicit components for %s `%s' have "
                            "differing values\n", mode_string(var), var->name);
               return;
            }

            existing->data.location = var->data.location;
            existing->data.explicit_location = true;
         } else {
            /* Check if uniform with implicit location was marked explicit
             * by earlier shader stage. If so, mark it explicit in this stage
             * too to make sure later processing does not treat it as
             * implicit one.
             */
            if (existing->data.explicit_location) {
               var->data.location = existing->data.location;
               var->data.explicit_location = true;
            }
         }

         /* From the GLSL 4.20 specification:
          * "A link error will result if two compilation units in a program
          *  specify different integer-constant bindings for the same
          *  opaque-uniform name.  However, it is not an error to specify a
          *  binding on some but not all declarations for the same name"
          */
         if (var->data.explicit_binding) {
            if (existing->data.explicit_binding &&
                var->data.binding != existing->data.binding) {
               linker_error(prog, "explicit bindings for %s "
                            "`%s' have differing values\n",
                            mode_string(var), var->name);
               return;
            }

            existing->data.binding = var->data.binding;
            existing->data.explicit_binding = true;
         }

         if (glsl_contains_atomic(var->type) &&
             var->data.offset != existing->data.offset) {
            linker_error(prog, "offset specifications for %s "
                         "`%s' have differing values\n",
                         mode_string(var), var->name);
            return;
         }

         /* Validate layout qualifiers for gl_FragDepth.
          *
          * From the AMD/ARB_conservative_depth specs:
          *
          *    "If gl_FragDepth is redeclared in any fragment shader in a
          *    program, it must be redeclared in all fragment shaders in
          *    that program that have static assignments to
          *    gl_FragDepth. All redeclarations of gl_FragDepth in all
          *    fragment shaders in a single program must have the same set
          *    of qualifiers."
          */
         if (strcmp(var->name, "gl_FragDepth") == 0) {
            bool layout_declared = var->data.depth_layout != ir_depth_layout_none;
            bool layout_differs =
               var->data.depth_layout != existing->data.depth_layout;

            if (layout_declared && layout_differs) {
               linker_error(prog,
                            "All redeclarations of gl_FragDepth in all "
                            "fragment shaders in a single program must have "
                            "the same set of qualifiers.\n");
            }

            if (var->data.used && layout_differs) {
               linker_error(prog,
                            "If gl_FragDepth is redeclared with a layout "
                            "qualifier in any fragment shader, it must be "
                            "redeclared with the same layout qualifier in "
                            "all fragment shaders that have assignments to "
                            "gl_FragDepth\n");
            }
         }

         /* Page 35 (page 41 of the PDF) of the GLSL 4.20 spec says:
          *
          *     "If a shared global has multiple initializers, the
          *     initializers must all be constant expressions, and they
          *     must all have the same value. Otherwise, a link error will
          *     result. (A shared global having only one initializer does
          *     not require that initializer to be a constant expression.)"
          *
          * Previous to 4.20 the GLSL spec simply said that initializers
          * must have the same value.  In this case of non-constant
          * initializers, this was impossible to determine.  As a result,
          * no vendor actually implemented that behavior.  The 4.20
          * behavior matches the implemented behavior of at least one other
          * vendor, so we'll implement that for all GLSL versions.
          * If (at least) one of these constant expressions is implicit,
          * because it was added by glsl_zero_init, we skip the verification.
          */
         if (var->constant_initializer != NULL) {
            if (existing->constant_initializer != NULL &&
                !existing->data.is_implicit_initializer &&
                !var->data.is_implicit_initializer) {
               if (!var->constant_initializer->has_value(existing->constant_initializer)) {
                  linker_error(prog, "initializers for %s "
                               "`%s' have differing values\n",
                               mode_string(var), var->name);
                  return;
               }
            } else {
               /* If the first-seen instance of a particular uniform did
                * not have an initializer but a later instance does,
                * replace the former with the later.
                */
               if (!var->data.is_implicit_initializer)
                  variables->replace_variable(existing->name, var);
            }
         }

         if (var->data.has_initializer) {
            if (existing->data.has_initializer
                && (var->constant_initializer == NULL
                    || existing->constant_initializer == NULL)) {
               linker_error(prog,
                            "shared global variable `%s' has multiple "
                            "non-constant initializers.\n",
                            var->name);
               return;
            }
         }

         if (existing->data.explicit_invariant != var->data.explicit_invariant) {
            linker_error(prog, "declarations for %s `%s' have "
                         "mismatching invariant qualifiers\n",
                         mode_string(var), var->name);
            return;
         }
         if (existing->data.centroid != var->data.centroid) {
            linker_error(prog, "declarations for %s `%s' have "
                         "mismatching centroid qualifiers\n",
                         mode_string(var), var->name);
            return;
         }
         if (existing->data.sample != var->data.sample) {
            linker_error(prog, "declarations for %s `%s` have "
                         "mismatching sample qualifiers\n",
                         mode_string(var), var->name);
            return;
         }
         if (existing->data.image_format != var->data.image_format) {
            linker_error(prog, "declarations for %s `%s` have "
                         "mismatching image format qualifiers\n",
                         mode_string(var), var->name);
            return;
         }

         /* Check the precision qualifier matches for uniform variables on
          * GLSL ES.
          */
         if (!consts->AllowGLSLRelaxedES &&
             prog->IsES && !var->get_interface_type() &&
             existing->data.precision != var->data.precision) {
            if ((existing->data.used && var->data.used) ||
                prog->GLSL_Version >= 300) {
               linker_error(prog, "declarations for %s `%s` have "
                            "mismatching precision qualifiers\n",
                            mode_string(var), var->name);
               return;
            } else {
               linker_warning(prog, "declarations for %s `%s` have "
                              "mismatching precision qualifiers\n",
                              mode_string(var), var->name);
            }
         }

         /* In OpenGL GLSL 3.20 spec, section 4.3.9:
          *
          *   "It is a link-time error if any particular shader interface
          *    contains:
          *
          *    - two different blocks, each having no instance name, and each
          *      having a member of the same name, or
          *
          *    - a variable outside a block, and a block with no instance name,
          *      where the variable has the same name as a member in the block."
          */
         const glsl_type *var_itype = var->get_interface_type();
         const glsl_type *existing_itype = existing->get_interface_type();
         if (var_itype != existing_itype) {
            if (!var_itype || !existing_itype) {
               linker_error(prog, "declarations for %s `%s` are inside block "
                            "`%s` and outside a block",
                            mode_string(var), var->name,
                            glsl_get_type_name(var_itype ? var_itype : existing_itype));
               return;
            } else if (strcmp(glsl_get_type_name(var_itype), glsl_get_type_name(existing_itype)) != 0) {
               linker_error(prog, "declarations for %s `%s` are inside blocks "
                            "`%s` and `%s`",
                            mode_string(var), var->name,
                            glsl_get_type_name(existing_itype),
                            glsl_get_type_name(var_itype));
               return;
            }
         }
      } else
         variables->add_variable(var);
   }
}

/**
 * Populates a shaders symbol table with all global declarations
 */
static void
populate_symbol_table(gl_linked_shader *sh, glsl_symbol_table *symbols)
{
   sh->symbols = new(sh) glsl_symbol_table;

   _mesa_glsl_copy_symbols_from_table(sh->ir, symbols, sh->symbols);
}


/**
 * Remap variables referenced in an instruction tree
 *
 * This is used when instruction trees are cloned from one shader and placed in
 * another.  These trees will contain references to \c ir_variable nodes that
 * do not exist in the target shader.  This function finds these \c ir_variable
 * references and replaces the references with matching variables in the target
 * shader.
 *
 * If there is no matching variable in the target shader, a clone of the
 * \c ir_variable is made and added to the target shader.  The new variable is
 * added to \b both the instruction stream and the symbol table.
 *
 * \param inst         IR tree that is to be processed.
 * \param symbols      Symbol table containing global scope symbols in the
 *                     linked shader.
 * \param instructions Instruction stream where new variable declarations
 *                     should be added.
 */
static void
remap_variables(ir_instruction *inst, struct gl_linked_shader *target,
                hash_table *temps)
{
   class remap_visitor : public ir_hierarchical_visitor {
   public:
         remap_visitor(struct gl_linked_shader *target, hash_table *temps)
      {
         this->target = target;
         this->symbols = target->symbols;
         this->instructions = target->ir;
         this->temps = temps;
      }

      virtual ir_visitor_status visit(ir_dereference_variable *ir)
      {
         if (ir->var->data.mode == ir_var_temporary) {
            hash_entry *entry = _mesa_hash_table_search(temps, ir->var);
            ir_variable *var = entry ? (ir_variable *) entry->data : NULL;

            assert(var != NULL);
            ir->var = var;
            return visit_continue;
         }

         ir_variable *const existing =
            this->symbols->get_variable(ir->var->name);
         if (existing != NULL)
            ir->var = existing;
         else {
            ir_variable *copy = ir->var->clone(this->target, NULL);

            this->symbols->add_variable(copy);
            this->instructions->push_head(copy);
            ir->var = copy;
         }

         return visit_continue;
      }

   private:
      struct gl_linked_shader *target;
      glsl_symbol_table *symbols;
      exec_list *instructions;
      hash_table *temps;
   };

   remap_visitor v(target, temps);

   inst->accept(&v);
}


/**
 * Move non-declarations from one instruction stream to another
 *
 * The intended usage pattern of this function is to pass the pointer to the
 * head sentinel of a list (i.e., a pointer to the list cast to an \c exec_node
 * pointer) for \c last and \c false for \c make_copies on the first
 * call.  Successive calls pass the return value of the previous call for
 * \c last and \c true for \c make_copies.
 *
 * \param instructions Source instruction stream
 * \param last         Instruction after which new instructions should be
 *                     inserted in the target instruction stream
 * \param make_copies  Flag selecting whether instructions in \c instructions
 *                     should be copied (via \c ir_instruction::clone) into the
 *                     target list or moved.
 *
 * \return
 * The new "last" instruction in the target instruction stream.  This pointer
 * is suitable for use as the \c last parameter of a later call to this
 * function.
 */
static exec_node *
move_non_declarations(exec_list *instructions, exec_node *last,
                      bool make_copies, gl_linked_shader *target)
{
   hash_table *temps = NULL;

   if (make_copies)
      temps = _mesa_pointer_hash_table_create(NULL);

   foreach_in_list_safe(ir_instruction, inst, instructions) {
      if (inst->as_function())
         continue;

      ir_variable *var = inst->as_variable();
      if ((var != NULL) && (var->data.mode != ir_var_temporary))
         continue;

      assert(inst->as_assignment()
             || inst->as_call()
             || inst->as_if() /* for initializers with the ?: operator */
             || ((var != NULL) && (var->data.mode == ir_var_temporary)));

      if (make_copies) {
         inst = inst->clone(target, NULL);

         if (var != NULL)
            _mesa_hash_table_insert(temps, var, inst);
         else
            remap_variables(inst, target, temps);
      } else {
         inst->remove();
      }

      last->insert_after(inst);
      last = inst;
   }

   if (make_copies)
      _mesa_hash_table_destroy(temps, NULL);

   return last;
}


/**
 * This class is only used in link_intrastage_shaders() below but declaring
 * it inside that function leads to compiler warnings with some versions of
 * gcc.
 */
class array_sizing_visitor : public deref_type_updater {
public:
   using deref_type_updater::visit;

   array_sizing_visitor()
      : mem_ctx(ralloc_context(NULL)),
        unnamed_interfaces(_mesa_pointer_hash_table_create(NULL))
   {
   }

   ~array_sizing_visitor()
   {
      _mesa_hash_table_destroy(this->unnamed_interfaces, NULL);
      ralloc_free(this->mem_ctx);
   }

   virtual ir_visitor_status visit(ir_variable *var)
   {
      const glsl_type *type_without_array;
      bool implicit_sized_array = var->data.implicit_sized_array;
      fixup_type(&var->type, var->data.max_array_access,
                 var->data.from_ssbo_unsized_array,
                 &implicit_sized_array);
      var->data.implicit_sized_array = implicit_sized_array;
      type_without_array = glsl_without_array(var->type);
      if (glsl_type_is_interface(var->type)) {
         if (interface_contains_unsized_arrays(var->type)) {
            const glsl_type *new_type =
               resize_interface_members(var->type,
                                        var->get_max_ifc_array_access(),
                                        var->is_in_shader_storage_block());
            var->type = new_type;
            var->change_interface_type(new_type);
         }
      } else if (glsl_type_is_interface(type_without_array)) {
         if (interface_contains_unsized_arrays(type_without_array)) {
            const glsl_type *new_type =
               resize_interface_members(type_without_array,
                                        var->get_max_ifc_array_access(),
                                        var->is_in_shader_storage_block());
            var->change_interface_type(new_type);
            var->type = update_interface_members_array(var->type, new_type);
         }
      } else if (const glsl_type *ifc_type = var->get_interface_type()) {
         /* Store a pointer to the variable in the unnamed_interfaces
          * hashtable.
          */
         hash_entry *entry =
               _mesa_hash_table_search(this->unnamed_interfaces,
                                       ifc_type);

         ir_variable **interface_vars = entry ? (ir_variable **) entry->data : NULL;

         if (interface_vars == NULL) {
            interface_vars = rzalloc_array(mem_ctx, ir_variable *,
                                           ifc_type->length);
            _mesa_hash_table_insert(this->unnamed_interfaces, ifc_type,
                                    interface_vars);
         }
         unsigned index = glsl_get_field_index(ifc_type, var->name);
         assert(index < ifc_type->length);
         assert(interface_vars[index] == NULL);
         interface_vars[index] = var;
      }
      return visit_continue;
   }

   /**
    * For each unnamed interface block that was discovered while running the
    * visitor, adjust the interface type to reflect the newly assigned array
    * sizes, and fix up the ir_variable nodes to point to the new interface
    * type.
    */
   void fixup_unnamed_interface_types()
   {
      hash_table_call_foreach(this->unnamed_interfaces,
                              fixup_unnamed_interface_type, NULL);
   }

private:
   /**
    * If the type pointed to by \c type represents an unsized array, replace
    * it with a sized array whose size is determined by max_array_access.
    */
   static void fixup_type(const glsl_type **type, unsigned max_array_access,
                          bool from_ssbo_unsized_array, bool *implicit_sized)
   {
      if (!from_ssbo_unsized_array && glsl_type_is_unsized_array(*type)) {
         *type = glsl_array_type((*type)->fields.array,
                                 max_array_access + 1, 0);
         *implicit_sized = true;
         assert(*type != NULL);
      }
   }

   static const glsl_type *
   update_interface_members_array(const glsl_type *type,
                                  const glsl_type *new_interface_type)
   {
      const glsl_type *element_type = type->fields.array;
      if (glsl_type_is_array(element_type)) {
         const glsl_type *new_array_type =
            update_interface_members_array(element_type, new_interface_type);
         return glsl_array_type(new_array_type, type->length, 0);
      } else {
         return glsl_array_type(new_interface_type, type->length, 0);
      }
   }

   /**
    * Determine whether the given interface type contains unsized arrays (if
    * it doesn't, array_sizing_visitor doesn't need to process it).
    */
   static bool interface_contains_unsized_arrays(const glsl_type *type)
   {
      for (unsigned i = 0; i < type->length; i++) {
         const glsl_type *elem_type = type->fields.structure[i].type;
         if (glsl_type_is_unsized_array(elem_type))
            return true;
      }
      return false;
   }

   /**
    * Create a new interface type based on the given type, with unsized arrays
    * replaced by sized arrays whose size is determined by
    * max_ifc_array_access.
    */
   static const glsl_type *
   resize_interface_members(const glsl_type *type,
                            const int *max_ifc_array_access,
                            bool is_ssbo)
   {
      unsigned num_fields = type->length;
      glsl_struct_field *fields = new glsl_struct_field[num_fields];
      memcpy(fields, type->fields.structure,
             num_fields * sizeof(*fields));
      for (unsigned i = 0; i < num_fields; i++) {
         bool implicit_sized_array = fields[i].implicit_sized_array;
         /* If SSBO last member is unsized array, we don't replace it by a sized
          * array.
          */
         if (is_ssbo && i == (num_fields - 1))
            fixup_type(&fields[i].type, max_ifc_array_access[i],
                       true, &implicit_sized_array);
         else
            fixup_type(&fields[i].type, max_ifc_array_access[i],
                       false, &implicit_sized_array);
         fields[i].implicit_sized_array = implicit_sized_array;
      }
      glsl_interface_packing packing =
         (glsl_interface_packing) type->interface_packing;
      bool row_major = (bool) type->interface_row_major;
      const glsl_type *new_ifc_type =
         glsl_interface_type(fields, num_fields,
                             packing, row_major, glsl_get_type_name(type));
      delete [] fields;
      return new_ifc_type;
   }

   static void fixup_unnamed_interface_type(const void *key, void *data,
                                            void *)
   {
      const glsl_type *ifc_type = (const glsl_type *) key;
      ir_variable **interface_vars = (ir_variable **) data;
      unsigned num_fields = ifc_type->length;
      glsl_struct_field *fields = new glsl_struct_field[num_fields];
      memcpy(fields, ifc_type->fields.structure,
             num_fields * sizeof(*fields));
      bool interface_type_changed = false;
      for (unsigned i = 0; i < num_fields; i++) {
         if (interface_vars[i] != NULL &&
             fields[i].type != interface_vars[i]->type) {
            fields[i].type = interface_vars[i]->type;
            interface_type_changed = true;
         }
      }
      if (!interface_type_changed) {
         delete [] fields;
         return;
      }
      glsl_interface_packing packing =
         (glsl_interface_packing) ifc_type->interface_packing;
      bool row_major = (bool) ifc_type->interface_row_major;
      const glsl_type *new_ifc_type =
         glsl_interface_type(fields, num_fields, packing,
                             row_major, glsl_get_type_name(ifc_type));
      delete [] fields;
      for (unsigned i = 0; i < num_fields; i++) {
         if (interface_vars[i] != NULL)
            interface_vars[i]->change_interface_type(new_ifc_type);
      }
   }

   /**
    * Memory context used to allocate the data in \c unnamed_interfaces.
    */
   void *mem_ctx;

   /**
    * Hash table from const glsl_type * to an array of ir_variable *'s
    * pointing to the ir_variables constituting each unnamed interface block.
    */
   hash_table *unnamed_interfaces;
};

static bool
validate_xfb_buffer_stride(const struct gl_constants *consts, unsigned idx,
                           struct gl_shader_program *prog)
{
   /* We will validate doubles at a later stage */
   if (prog->TransformFeedback.BufferStride[idx] % 4) {
      linker_error(prog, "invalid qualifier xfb_stride=%d must be a "
                   "multiple of 4 or if its applied to a type that is "
                   "or contains a double a multiple of 8.",
                   prog->TransformFeedback.BufferStride[idx]);
      return false;
   }

   if (prog->TransformFeedback.BufferStride[idx] / 4 >
       consts->MaxTransformFeedbackInterleavedComponents) {
      linker_error(prog, "The MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS "
                   "limit has been exceeded.");
      return false;
   }

   return true;
}

/**
 * Check for conflicting xfb_stride default qualifiers and store buffer stride
 * for later use.
 */
static void
link_xfb_stride_layout_qualifiers(const struct gl_constants *consts,
                                  struct gl_shader_program *prog,
                                  struct gl_shader **shader_list,
                                  unsigned num_shaders)
{
   for (unsigned i = 0; i < MAX_FEEDBACK_BUFFERS; i++) {
      prog->TransformFeedback.BufferStride[i] = 0;
   }

   for (unsigned i = 0; i < num_shaders; i++) {
      struct gl_shader *shader = shader_list[i];

      for (unsigned j = 0; j < MAX_FEEDBACK_BUFFERS; j++) {
         if (shader->TransformFeedbackBufferStride[j]) {
            if (prog->TransformFeedback.BufferStride[j] == 0) {
               prog->TransformFeedback.BufferStride[j] =
                  shader->TransformFeedbackBufferStride[j];
               if (!validate_xfb_buffer_stride(consts, j, prog))
                  return;
            } else if (prog->TransformFeedback.BufferStride[j] !=
                       shader->TransformFeedbackBufferStride[j]){
               linker_error(prog,
                            "intrastage shaders defined with conflicting "
                            "xfb_stride for buffer %d (%d and %d)\n", j,
                            prog->TransformFeedback.BufferStride[j],
                            shader->TransformFeedbackBufferStride[j]);
               return;
            }
         }
      }
   }
}

/**
 * Check for conflicting bindless/bound sampler/image layout qualifiers at
 * global scope.
 */
static void
link_bindless_layout_qualifiers(struct gl_shader_program *prog,
                                struct gl_shader **shader_list,
                                unsigned num_shaders)
{
   bool bindless_sampler, bindless_image;
   bool bound_sampler, bound_image;

   bindless_sampler = bindless_image = false;
   bound_sampler = bound_image = false;

   for (unsigned i = 0; i < num_shaders; i++) {
      struct gl_shader *shader = shader_list[i];

      if (shader->bindless_sampler)
         bindless_sampler = true;
      if (shader->bindless_image)
         bindless_image = true;
      if (shader->bound_sampler)
         bound_sampler = true;
      if (shader->bound_image)
         bound_image = true;

      if ((bindless_sampler && bound_sampler) ||
          (bindless_image && bound_image)) {
         /* From section 4.4.6 of the ARB_bindless_texture spec:
          *
          *     "If both bindless_sampler and bound_sampler, or bindless_image
          *      and bound_image, are declared at global scope in any
          *      compilation unit, a link- time error will be generated."
          */
         linker_error(prog, "both bindless_sampler and bound_sampler, or "
                      "bindless_image and bound_image, can't be declared at "
                      "global scope");
      }
   }
}

/**
 * Check for conflicting viewport_relative settings across shaders, and sets
 * the value for the linked shader.
 */
static void
link_layer_viewport_relative_qualifier(struct gl_shader_program *prog,
                                       struct gl_program *gl_prog,
                                       struct gl_shader **shader_list,
                                       unsigned num_shaders)
{
   unsigned i;

   /* Find first shader with explicit layer declaration */
   for (i = 0; i < num_shaders; i++) {
      if (shader_list[i]->redeclares_gl_layer) {
         gl_prog->info.layer_viewport_relative =
            shader_list[i]->layer_viewport_relative;
         break;
      }
   }

   /* Now make sure that each subsequent shader's explicit layer declaration
    * matches the first one's.
    */
   for (; i < num_shaders; i++) {
      if (shader_list[i]->redeclares_gl_layer &&
          shader_list[i]->layer_viewport_relative !=
          gl_prog->info.layer_viewport_relative) {
         linker_error(prog, "all gl_Layer redeclarations must have identical "
                      "viewport_relative settings");
      }
   }
}

/**
 * Performs the cross-validation of tessellation control shader vertices and
 * layout qualifiers for the attached tessellation control shaders,
 * and propagates them to the linked TCS and linked shader program.
 */
static void
link_tcs_out_layout_qualifiers(struct gl_shader_program *prog,
                               struct gl_program *gl_prog,
                               struct gl_shader **shader_list,
                               unsigned num_shaders)
{
   if (gl_prog->info.stage != MESA_SHADER_TESS_CTRL)
      return;

   gl_prog->info.tess.tcs_vertices_out = 0;

   /* From the GLSL 4.0 spec (chapter 4.3.8.2):
    *
    *     "All tessellation control shader layout declarations in a program
    *      must specify the same output patch vertex count.  There must be at
    *      least one layout qualifier specifying an output patch vertex count
    *      in any program containing tessellation control shaders; however,
    *      such a declaration is not required in all tessellation control
    *      shaders."
    */

   for (unsigned i = 0; i < num_shaders; i++) {
      struct gl_shader *shader = shader_list[i];

      if (shader->info.TessCtrl.VerticesOut != 0) {
         if (gl_prog->info.tess.tcs_vertices_out != 0 &&
             gl_prog->info.tess.tcs_vertices_out !=
             (unsigned) shader->info.TessCtrl.VerticesOut) {
            linker_error(prog, "tessellation control shader defined with "
                         "conflicting output vertex count (%d and %d)\n",
                         gl_prog->info.tess.tcs_vertices_out,
                         shader->info.TessCtrl.VerticesOut);
            return;
         }
         gl_prog->info.tess.tcs_vertices_out =
            shader->info.TessCtrl.VerticesOut;
      }
   }

   /* Just do the intrastage -> interstage propagation right now,
    * since we already know we're in the right type of shader program
    * for doing it.
    */
   if (gl_prog->info.tess.tcs_vertices_out == 0) {
      linker_error(prog, "tessellation control shader didn't declare "
                   "vertices out layout qualifier\n");
      return;
   }
}


/**
 * Performs the cross-validation of tessellation evaluation shader
 * primitive type, vertex spacing, ordering and point_mode layout qualifiers
 * for the attached tessellation evaluation shaders, and propagates them
 * to the linked TES and linked shader program.
 */
static void
link_tes_in_layout_qualifiers(struct gl_shader_program *prog,
                              struct gl_program *gl_prog,
                              struct gl_shader **shader_list,
                              unsigned num_shaders)
{
   if (gl_prog->info.stage != MESA_SHADER_TESS_EVAL)
      return;

   int point_mode = -1;
   unsigned vertex_order = 0;

   gl_prog->info.tess._primitive_mode = TESS_PRIMITIVE_UNSPECIFIED;
   gl_prog->info.tess.spacing = TESS_SPACING_UNSPECIFIED;

   /* From the GLSL 4.0 spec (chapter 4.3.8.1):
    *
    *     "At least one tessellation evaluation shader (compilation unit) in
    *      a program must declare a primitive mode in its input layout.
    *      Declaration vertex spacing, ordering, and point mode identifiers is
    *      optional.  It is not required that all tessellation evaluation
    *      shaders in a program declare a primitive mode.  If spacing or
    *      vertex ordering declarations are omitted, the tessellation
    *      primitive generator will use equal spacing or counter-clockwise
    *      vertex ordering, respectively.  If a point mode declaration is
    *      omitted, the tessellation primitive generator will produce lines or
    *      triangles according to the primitive mode."
    */

   for (unsigned i = 0; i < num_shaders; i++) {
      struct gl_shader *shader = shader_list[i];

      if (shader->info.TessEval._PrimitiveMode != TESS_PRIMITIVE_UNSPECIFIED) {
         if (gl_prog->info.tess._primitive_mode != TESS_PRIMITIVE_UNSPECIFIED &&
             gl_prog->info.tess._primitive_mode !=
             shader->info.TessEval._PrimitiveMode) {
            linker_error(prog, "tessellation evaluation shader defined with "
                         "conflicting input primitive modes.\n");
            return;
         }
         gl_prog->info.tess._primitive_mode =
            shader->info.TessEval._PrimitiveMode;
      }

      if (shader->info.TessEval.Spacing != 0) {
         if (gl_prog->info.tess.spacing != 0 && gl_prog->info.tess.spacing !=
             shader->info.TessEval.Spacing) {
            linker_error(prog, "tessellation evaluation shader defined with "
                         "conflicting vertex spacing.\n");
            return;
         }
         gl_prog->info.tess.spacing = shader->info.TessEval.Spacing;
      }

      if (shader->info.TessEval.VertexOrder != 0) {
         if (vertex_order != 0 &&
             vertex_order != shader->info.TessEval.VertexOrder) {
            linker_error(prog, "tessellation evaluation shader defined with "
                         "conflicting ordering.\n");
            return;
         }
         vertex_order = shader->info.TessEval.VertexOrder;
      }

      if (shader->info.TessEval.PointMode != -1) {
         if (point_mode != -1 &&
             point_mode != shader->info.TessEval.PointMode) {
            linker_error(prog, "tessellation evaluation shader defined with "
                         "conflicting point modes.\n");
            return;
         }
         point_mode = shader->info.TessEval.PointMode;
      }

   }

   /* Just do the intrastage -> interstage propagation right now,
    * since we already know we're in the right type of shader program
    * for doing it.
    */
   if (gl_prog->info.tess._primitive_mode == TESS_PRIMITIVE_UNSPECIFIED) {
      linker_error(prog,
                   "tessellation evaluation shader didn't declare input "
                   "primitive modes.\n");
      return;
   }

   if (gl_prog->info.tess.spacing == TESS_SPACING_UNSPECIFIED)
      gl_prog->info.tess.spacing = TESS_SPACING_EQUAL;

   if (vertex_order == 0 || vertex_order == GL_CCW)
      gl_prog->info.tess.ccw = true;
   else
      gl_prog->info.tess.ccw = false;


   if (point_mode == -1 || point_mode == GL_FALSE)
      gl_prog->info.tess.point_mode = false;
   else
      gl_prog->info.tess.point_mode = true;
}


/**
 * Performs the cross-validation of layout qualifiers specified in
 * redeclaration of gl_FragCoord for the attached fragment shaders,
 * and propagates them to the linked FS and linked shader program.
 */
static void
link_fs_inout_layout_qualifiers(struct gl_shader_program *prog,
                                struct gl_linked_shader *linked_shader,
                                struct gl_shader **shader_list,
                                unsigned num_shaders,
                                bool arb_fragment_coord_conventions_enable)
{
   bool redeclares_gl_fragcoord = false;
   bool uses_gl_fragcoord = false;
   bool origin_upper_left = false;
   bool pixel_center_integer = false;

   if (linked_shader->Stage != MESA_SHADER_FRAGMENT ||
       (prog->GLSL_Version < 150 && !arb_fragment_coord_conventions_enable))
      return;

   for (unsigned i = 0; i < num_shaders; i++) {
      struct gl_shader *shader = shader_list[i];
      /* From the GLSL 1.50 spec, page 39:
       *
       *   "If gl_FragCoord is redeclared in any fragment shader in a program,
       *    it must be redeclared in all the fragment shaders in that program
       *    that have a static use gl_FragCoord."
       */
      if ((redeclares_gl_fragcoord && !shader->redeclares_gl_fragcoord &&
           shader->uses_gl_fragcoord)
          || (shader->redeclares_gl_fragcoord && !redeclares_gl_fragcoord &&
              uses_gl_fragcoord)) {
             linker_error(prog, "fragment shader defined with conflicting "
                         "layout qualifiers for gl_FragCoord\n");
      }

      /* From the GLSL 1.50 spec, page 39:
       *
       *   "All redeclarations of gl_FragCoord in all fragment shaders in a
       *    single program must have the same set of qualifiers."
       */
      if (redeclares_gl_fragcoord && shader->redeclares_gl_fragcoord &&
          (shader->origin_upper_left != origin_upper_left ||
           shader->pixel_center_integer != pixel_center_integer)) {
         linker_error(prog, "fragment shader defined with conflicting "
                      "layout qualifiers for gl_FragCoord\n");
      }

      /* Update the linked shader state.  Note that uses_gl_fragcoord should
       * accumulate the results.  The other values should replace.  If there
       * are multiple redeclarations, all the fields except uses_gl_fragcoord
       * are already known to be the same.
       */
      if (shader->redeclares_gl_fragcoord || shader->uses_gl_fragcoord) {
         redeclares_gl_fragcoord = shader->redeclares_gl_fragcoord;
         uses_gl_fragcoord |= shader->uses_gl_fragcoord;
         origin_upper_left = shader->origin_upper_left;
         pixel_center_integer = shader->pixel_center_integer;
      }

      linked_shader->Program->info.fs.early_fragment_tests |=
         shader->EarlyFragmentTests || shader->PostDepthCoverage;
      linked_shader->Program->info.fs.inner_coverage |= shader->InnerCoverage;
      linked_shader->Program->info.fs.post_depth_coverage |=
         shader->PostDepthCoverage;
      linked_shader->Program->info.fs.pixel_interlock_ordered |=
         shader->PixelInterlockOrdered;
      linked_shader->Program->info.fs.pixel_interlock_unordered |=
         shader->PixelInterlockUnordered;
      linked_shader->Program->info.fs.sample_interlock_ordered |=
         shader->SampleInterlockOrdered;
      linked_shader->Program->info.fs.sample_interlock_unordered |=
         shader->SampleInterlockUnordered;
      linked_shader->Program->info.fs.advanced_blend_modes |= shader->BlendSupport;
   }

   linked_shader->Program->info.fs.pixel_center_integer = pixel_center_integer;
   linked_shader->Program->info.fs.origin_upper_left = origin_upper_left;
}

/**
 * Performs the cross-validation of geometry shader max_vertices and
 * primitive type layout qualifiers for the attached geometry shaders,
 * and propagates them to the linked GS and linked shader program.
 */
static void
link_gs_inout_layout_qualifiers(struct gl_shader_program *prog,
                                struct gl_program *gl_prog,
                                struct gl_shader **shader_list,
                                unsigned num_shaders)
{
   /* No in/out qualifiers defined for anything but GLSL 1.50+
    * geometry shaders so far.
    */
   if (gl_prog->info.stage != MESA_SHADER_GEOMETRY || prog->GLSL_Version < 150)
      return;

   int vertices_out = -1;

   gl_prog->info.gs.invocations = 0;
   gl_prog->info.gs.input_primitive = MESA_PRIM_UNKNOWN;
   gl_prog->info.gs.output_primitive = MESA_PRIM_UNKNOWN;

   /* From the GLSL 1.50 spec, page 46:
    *
    *     "All geometry shader output layout declarations in a program
    *      must declare the same layout and same value for
    *      max_vertices. There must be at least one geometry output
    *      layout declaration somewhere in a program, but not all
    *      geometry shaders (compilation units) are required to
    *      declare it."
    */

   for (unsigned i = 0; i < num_shaders; i++) {
      struct gl_shader *shader = shader_list[i];

      if (shader->info.Geom.InputType != MESA_PRIM_UNKNOWN) {
         if (gl_prog->info.gs.input_primitive != MESA_PRIM_UNKNOWN &&
             gl_prog->info.gs.input_primitive !=
             shader->info.Geom.InputType) {
            linker_error(prog, "geometry shader defined with conflicting "
                         "input types\n");
            return;
         }
         gl_prog->info.gs.input_primitive = (enum mesa_prim)shader->info.Geom.InputType;
      }

      if (shader->info.Geom.OutputType != MESA_PRIM_UNKNOWN) {
         if (gl_prog->info.gs.output_primitive != MESA_PRIM_UNKNOWN &&
             gl_prog->info.gs.output_primitive !=
             shader->info.Geom.OutputType) {
            linker_error(prog, "geometry shader defined with conflicting "
                         "output types\n");
            return;
         }
         gl_prog->info.gs.output_primitive = (enum mesa_prim)shader->info.Geom.OutputType;
      }

      if (shader->info.Geom.VerticesOut != -1) {
         if (vertices_out != -1 &&
             vertices_out != shader->info.Geom.VerticesOut) {
            linker_error(prog, "geometry shader defined with conflicting "
                         "output vertex count (%d and %d)\n",
                         vertices_out, shader->info.Geom.VerticesOut);
            return;
         }
         vertices_out = shader->info.Geom.VerticesOut;
      }

      if (shader->info.Geom.Invocations != 0) {
         if (gl_prog->info.gs.invocations != 0 &&
             gl_prog->info.gs.invocations !=
             (unsigned) shader->info.Geom.Invocations) {
            linker_error(prog, "geometry shader defined with conflicting "
                         "invocation count (%d and %d)\n",
                         gl_prog->info.gs.invocations,
                         shader->info.Geom.Invocations);
            return;
         }
         gl_prog->info.gs.invocations = shader->info.Geom.Invocations;
      }
   }

   /* Just do the intrastage -> interstage propagation right now,
    * since we already know we're in the right type of shader program
    * for doing it.
    */
   if (gl_prog->info.gs.input_primitive == MESA_PRIM_UNKNOWN) {
      linker_error(prog,
                   "geometry shader didn't declare primitive input type\n");
      return;
   }

   if (gl_prog->info.gs.output_primitive == MESA_PRIM_UNKNOWN) {
      linker_error(prog,
                   "geometry shader didn't declare primitive output type\n");
      return;
   }

   if (vertices_out == -1) {
      linker_error(prog,
                   "geometry shader didn't declare max_vertices\n");
      return;
   } else {
      gl_prog->info.gs.vertices_out = vertices_out;
   }

   if (gl_prog->info.gs.invocations == 0)
      gl_prog->info.gs.invocations = 1;
}


/**
 * Perform cross-validation of compute shader local_size_{x,y,z} layout and
 * derivative arrangement qualifiers for the attached compute shaders, and
 * propagate them to the linked CS and linked shader program.
 */
static void
link_cs_input_layout_qualifiers(struct gl_shader_program *prog,
                                struct gl_program *gl_prog,
                                struct gl_shader **shader_list,
                                unsigned num_shaders)
{
   /* This function is called for all shader stages, but it only has an effect
    * for compute shaders.
    */
   if (gl_prog->info.stage != MESA_SHADER_COMPUTE)
      return;

   for (int i = 0; i < 3; i++)
      gl_prog->info.workgroup_size[i] = 0;

   gl_prog->info.workgroup_size_variable = false;

   gl_prog->info.cs.derivative_group = DERIVATIVE_GROUP_NONE;

   /* From the ARB_compute_shader spec, in the section describing local size
    * declarations:
    *
    *     If multiple compute shaders attached to a single program object
    *     declare local work-group size, the declarations must be identical;
    *     otherwise a link-time error results. Furthermore, if a program
    *     object contains any compute shaders, at least one must contain an
    *     input layout qualifier specifying the local work sizes of the
    *     program, or a link-time error will occur.
    */
   for (unsigned sh = 0; sh < num_shaders; sh++) {
      struct gl_shader *shader = shader_list[sh];

      if (shader->info.Comp.LocalSize[0] != 0) {
         if (gl_prog->info.workgroup_size[0] != 0) {
            for (int i = 0; i < 3; i++) {
               if (gl_prog->info.workgroup_size[i] !=
                   shader->info.Comp.LocalSize[i]) {
                  linker_error(prog, "compute shader defined with conflicting "
                               "local sizes\n");
                  return;
               }
            }
         }
         for (int i = 0; i < 3; i++) {
            gl_prog->info.workgroup_size[i] =
               shader->info.Comp.LocalSize[i];
         }
      } else if (shader->info.Comp.LocalSizeVariable) {
         if (gl_prog->info.workgroup_size[0] != 0) {
            /* The ARB_compute_variable_group_size spec says:
             *
             *     If one compute shader attached to a program declares a
             *     variable local group size and a second compute shader
             *     attached to the same program declares a fixed local group
             *     size, a link-time error results.
             */
            linker_error(prog, "compute shader defined with both fixed and "
                         "variable local group size\n");
            return;
         }
         gl_prog->info.workgroup_size_variable = true;
      }

      enum gl_derivative_group group = shader->info.Comp.DerivativeGroup;
      if (group != DERIVATIVE_GROUP_NONE) {
         if (gl_prog->info.cs.derivative_group != DERIVATIVE_GROUP_NONE &&
             gl_prog->info.cs.derivative_group != group) {
            linker_error(prog, "compute shader defined with conflicting "
                         "derivative groups\n");
            return;
         }
         gl_prog->info.cs.derivative_group = group;
      }
   }

   /* Just do the intrastage -> interstage propagation right now,
    * since we already know we're in the right type of shader program
    * for doing it.
    */
   if (gl_prog->info.workgroup_size[0] == 0 &&
       !gl_prog->info.workgroup_size_variable) {
      linker_error(prog, "compute shader must contain a fixed or a variable "
                         "local group size\n");
      return;
   }

   if (gl_prog->info.cs.derivative_group == DERIVATIVE_GROUP_QUADS) {
      if (gl_prog->info.workgroup_size[0] % 2 != 0) {
         linker_error(prog, "derivative_group_quadsNV must be used with a "
                      "local group size whose first dimension "
                      "is a multiple of 2\n");
         return;
      }
      if (gl_prog->info.workgroup_size[1] % 2 != 0) {
         linker_error(prog, "derivative_group_quadsNV must be used with a local"
                      "group size whose second dimension "
                      "is a multiple of 2\n");
         return;
      }
   } else if (gl_prog->info.cs.derivative_group == DERIVATIVE_GROUP_LINEAR) {
      if ((gl_prog->info.workgroup_size[0] *
           gl_prog->info.workgroup_size[1] *
           gl_prog->info.workgroup_size[2]) % 4 != 0) {
         linker_error(prog, "derivative_group_linearNV must be used with a "
                      "local group size whose total number of invocations "
                      "is a multiple of 4\n");
         return;
      }
   }
}

/**
 * Link all out variables on a single stage which are not
 * directly used in a shader with the main function.
 */
static void
link_output_variables(struct gl_linked_shader *linked_shader,
                      struct gl_shader **shader_list,
                      unsigned num_shaders)
{
   struct glsl_symbol_table *symbols = linked_shader->symbols;

   for (unsigned i = 0; i < num_shaders; i++) {

      /* Skip shader object with main function */
      if (shader_list[i]->symbols->get_function("main"))
         continue;

      foreach_in_list(ir_instruction, ir, shader_list[i]->ir) {
         if (ir->ir_type != ir_type_variable)
            continue;

         ir_variable *var = (ir_variable *) ir;

         if (var->data.mode == ir_var_shader_out &&
               !symbols->get_variable(var->name)) {
            var = var->clone(linked_shader, NULL);
            symbols->add_variable(var);
            linked_shader->ir->push_head(var);
         }
      }
   }

   return;
}


/**
 * Combine a group of shaders for a single stage to generate a linked shader
 *
 * \note
 * If this function is supplied a single shader, it is cloned, and the new
 * shader is returned.
 */
struct gl_linked_shader *
link_intrastage_shaders(void *mem_ctx,
                        struct gl_context *ctx,
                        struct gl_shader_program *prog,
                        struct gl_shader **shader_list,
                        unsigned num_shaders,
                        bool allow_missing_main)
{
   bool arb_fragment_coord_conventions_enable = false;

   /* Check that global variables defined in multiple shaders are consistent.
    */
   glsl_symbol_table variables;
   for (unsigned i = 0; i < num_shaders; i++) {
      if (shader_list[i] == NULL)
         continue;
      cross_validate_globals(&ctx->Const, prog, shader_list[i]->ir, &variables,
                             false);
      if (shader_list[i]->ARB_fragment_coord_conventions_enable)
         arb_fragment_coord_conventions_enable = true;
   }

   if (!prog->data->LinkStatus)
      return NULL;

   /* Check that interface blocks defined in multiple shaders are consistent.
    */
   validate_intrastage_interface_blocks(prog, (const gl_shader **)shader_list,
                                        num_shaders);
   if (!prog->data->LinkStatus)
      return NULL;

   /* Check that there is only a single definition of each function signature
    * across all shaders.
    */
   for (unsigned i = 0; i < (num_shaders - 1); i++) {
      foreach_in_list(ir_instruction, node, shader_list[i]->ir) {
         ir_function *const f = node->as_function();

         if (f == NULL)
            continue;

         for (unsigned j = i + 1; j < num_shaders; j++) {
            ir_function *const other =
               shader_list[j]->symbols->get_function(f->name);

            /* If the other shader has no function (and therefore no function
             * signatures) with the same name, skip to the next shader.
             */
            if (other == NULL)
               continue;

            foreach_in_list(ir_function_signature, sig, &f->signatures) {
               if (!sig->is_defined)
                  continue;

               ir_function_signature *other_sig =
                  other->exact_matching_signature(NULL, &sig->parameters);

               if (other_sig != NULL && other_sig->is_defined) {
                  linker_error(prog, "function `%s' is multiply defined\n",
                               f->name);
                  return NULL;
               }
            }
         }
      }
   }

   /* Find the shader that defines main, and make a clone of it.
    *
    * Starting with the clone, search for undefined references.  If one is
    * found, find the shader that defines it.  Clone the reference and add
    * it to the shader.  Repeat until there are no undefined references or
    * until a reference cannot be resolved.
    */
   gl_shader *main = NULL;
   for (unsigned i = 0; i < num_shaders; i++) {
      if (_mesa_get_main_function_signature(shader_list[i]->symbols)) {
         main = shader_list[i];
         break;
      }
   }

   if (main == NULL && allow_missing_main)
      main = shader_list[0];

   if (main == NULL) {
      linker_error(prog, "%s shader lacks `main'\n",
                   _mesa_shader_stage_to_string(shader_list[0]->Stage));
      return NULL;
   }

   gl_linked_shader *linked = rzalloc(NULL, struct gl_linked_shader);
   linked->Stage = shader_list[0]->Stage;

   /* Create program and attach it to the linked shader */
   struct gl_program *gl_prog =
      ctx->Driver.NewProgram(ctx, shader_list[0]->Stage, prog->Name, false);
   if (!gl_prog) {
      prog->data->LinkStatus = LINKING_FAILURE;
      _mesa_delete_linked_shader(ctx, linked);
      return NULL;
   }

   _mesa_reference_shader_program_data(&gl_prog->sh.data, prog->data);

   /* Don't use _mesa_reference_program() just take ownership */
   linked->Program = gl_prog;

   linked->ir = new(linked) exec_list;
   clone_ir_list(mem_ctx, linked->ir, main->ir);

   link_fs_inout_layout_qualifiers(prog, linked, shader_list, num_shaders,
                                   arb_fragment_coord_conventions_enable);
   link_tcs_out_layout_qualifiers(prog, gl_prog, shader_list, num_shaders);
   link_tes_in_layout_qualifiers(prog, gl_prog, shader_list, num_shaders);
   link_gs_inout_layout_qualifiers(prog, gl_prog, shader_list, num_shaders);
   link_cs_input_layout_qualifiers(prog, gl_prog, shader_list, num_shaders);

   if (linked->Stage != MESA_SHADER_FRAGMENT)
      link_xfb_stride_layout_qualifiers(&ctx->Const, prog, shader_list, num_shaders);

   link_bindless_layout_qualifiers(prog, shader_list, num_shaders);

   link_layer_viewport_relative_qualifier(prog, gl_prog, shader_list, num_shaders);

   populate_symbol_table(linked, shader_list[0]->symbols);

   /* The pointer to the main function in the final linked shader (i.e., the
    * copy of the original shader that contained the main function).
    */
   ir_function_signature *const main_sig =
      _mesa_get_main_function_signature(linked->symbols);

   /* Move any instructions other than variable declarations or function
    * declarations into main.
    */
   if (main_sig != NULL) {
      exec_node *insertion_point =
         move_non_declarations(linked->ir, &main_sig->body.head_sentinel, false,
                               linked);

      for (unsigned i = 0; i < num_shaders; i++) {
         if (shader_list[i] == main)
            continue;

         insertion_point = move_non_declarations(shader_list[i]->ir,
                                                 insertion_point, true, linked);
      }
   }

   if (!link_function_calls(prog, linked, shader_list, num_shaders)) {
      _mesa_delete_linked_shader(ctx, linked);
      return NULL;
   }

   if (linked->Stage != MESA_SHADER_FRAGMENT)
      link_output_variables(linked, shader_list, num_shaders);

   /* Make a pass over all variable declarations to ensure that arrays with
    * unspecified sizes have a size specified.  The size is inferred from the
    * max_array_access field.
    */
   array_sizing_visitor v;
   v.run(linked->ir);
   v.fixup_unnamed_interface_types();

   /* Now that we know the sizes of all the arrays, we can replace .length()
    * calls with a constant expression.
    */
   array_length_to_const_visitor len_v;
   len_v.run(linked->ir);

   if (!prog->data->LinkStatus) {
      _mesa_delete_linked_shader(ctx, linked);
      return NULL;
   }

   /* At this point linked should contain all of the linked IR, so
    * validate it to make sure nothing went wrong.
    */
   validate_ir_tree(linked->ir);

   /* Set the size of geometry shader input arrays */
   if (linked->Stage == MESA_SHADER_GEOMETRY) {
      unsigned num_vertices =
         mesa_vertices_per_prim(gl_prog->info.gs.input_primitive);
      array_resize_visitor input_resize_visitor(num_vertices, prog,
                                                MESA_SHADER_GEOMETRY);
      foreach_in_list(ir_instruction, ir, linked->ir) {
         ir->accept(&input_resize_visitor);
      }
   }

   /* Set the linked source SHA1. */
   if (num_shaders == 1) {
      memcpy(linked->linked_source_sha1, shader_list[0]->compiled_source_sha1,
             SHA1_DIGEST_LENGTH);
   } else {
      struct mesa_sha1 sha1_ctx;
      _mesa_sha1_init(&sha1_ctx);

      for (unsigned i = 0; i < num_shaders; i++) {
         if (shader_list[i] == NULL)
            continue;

         _mesa_sha1_update(&sha1_ctx, shader_list[i]->compiled_source_sha1,
                           SHA1_DIGEST_LENGTH);
      }
      _mesa_sha1_final(&sha1_ctx, linked->linked_source_sha1);
   }

   return linked;
}

void
link_shaders(struct gl_context *ctx, struct gl_shader_program *prog)
{
   const struct gl_constants *consts = &ctx->Const;
   prog->data->LinkStatus = LINKING_SUCCESS; /* All error paths will set this to false */
   prog->data->Validated = false;

   /* Section 7.3 (Program Objects) of the OpenGL 4.5 Core Profile spec says:
    *
    *     "Linking can fail for a variety of reasons as specified in the
    *     OpenGL Shading Language Specification, as well as any of the
    *     following reasons:
    *
    *     - No shader objects are attached to program."
    *
    * The Compatibility Profile specification does not list the error.  In
    * Compatibility Profile missing shader stages are replaced by
    * fixed-function.  This applies to the case where all stages are
    * missing.
    */
   if (prog->NumShaders == 0) {
      if (ctx->API != API_OPENGL_COMPAT)
         linker_error(prog, "no shaders attached to the program\n");
      return;
   }

#ifdef ENABLE_SHADER_CACHE
   if (shader_cache_read_program_metadata(ctx, prog))
      return;
#endif

   void *mem_ctx = ralloc_context(NULL); // temporary linker context

   /* Separate the shaders into groups based on their type.
    */
   struct gl_shader **shader_list[MESA_SHADER_STAGES];
   unsigned num_shaders[MESA_SHADER_STAGES];

   for (int i = 0; i < MESA_SHADER_STAGES; i++) {
      shader_list[i] = (struct gl_shader **)
         calloc(prog->NumShaders, sizeof(struct gl_shader *));
      num_shaders[i] = 0;
   }

   unsigned min_version = UINT_MAX;
   unsigned max_version = 0;
   for (unsigned i = 0; i < prog->NumShaders; i++) {
      min_version = MIN2(min_version, prog->Shaders[i]->Version);
      max_version = MAX2(max_version, prog->Shaders[i]->Version);

      if (!consts->AllowGLSLRelaxedES &&
          prog->Shaders[i]->IsES != prog->Shaders[0]->IsES) {
         linker_error(prog, "all shaders must use same shading "
                      "language version\n");
         goto done;
      }

      gl_shader_stage shader_type = prog->Shaders[i]->Stage;
      shader_list[shader_type][num_shaders[shader_type]] = prog->Shaders[i];
      num_shaders[shader_type]++;
   }

   /* In desktop GLSL, different shader versions may be linked together.  In
    * GLSL ES, all shader versions must be the same.
    */
   if (!consts->AllowGLSLRelaxedES && prog->Shaders[0]->IsES &&
       min_version != max_version) {
      linker_error(prog, "all shaders must use same shading "
                   "language version\n");
      goto done;
   }

   prog->GLSL_Version = max_version;
   prog->IsES = prog->Shaders[0]->IsES;

   /* Some shaders have to be linked with some other shaders present.
    */
   if (!prog->SeparateShader) {
      if (num_shaders[MESA_SHADER_GEOMETRY] > 0 &&
          num_shaders[MESA_SHADER_VERTEX] == 0) {
         linker_error(prog, "Geometry shader must be linked with "
                      "vertex shader\n");
         goto done;
      }
      if (num_shaders[MESA_SHADER_TESS_EVAL] > 0 &&
          num_shaders[MESA_SHADER_VERTEX] == 0) {
         linker_error(prog, "Tessellation evaluation shader must be linked "
                      "with vertex shader\n");
         goto done;
      }
      if (num_shaders[MESA_SHADER_TESS_CTRL] > 0 &&
          num_shaders[MESA_SHADER_VERTEX] == 0) {
         linker_error(prog, "Tessellation control shader must be linked with "
                      "vertex shader\n");
         goto done;
      }

      /* Section 7.3 of the OpenGL ES 3.2 specification says:
       *
       *    "Linking can fail for [...] any of the following reasons:
       *
       *     * program contains an object to form a tessellation control
       *       shader [...] and [...] the program is not separable and
       *       contains no object to form a tessellation evaluation shader"
       *
       * The OpenGL spec is contradictory. It allows linking without a tess
       * eval shader, but that can only be used with transform feedback and
       * rasterization disabled. However, transform feedback isn't allowed
       * with GL_PATCHES, so it can't be used.
       *
       * More investigation showed that the idea of transform feedback after
       * a tess control shader was dropped, because some hw vendors couldn't
       * support tessellation without a tess eval shader, but the linker
       * section wasn't updated to reflect that.
       *
       * All specifications (ARB_tessellation_shader, GL 4.0-4.5) have this
       * spec bug.
       *
       * Do what's reasonable and always require a tess eval shader if a tess
       * control shader is present.
       */
      if (num_shaders[MESA_SHADER_TESS_CTRL] > 0 &&
          num_shaders[MESA_SHADER_TESS_EVAL] == 0) {
         linker_error(prog, "Tessellation control shader must be linked with "
                      "tessellation evaluation shader\n");
         goto done;
      }

      if (prog->IsES) {
         if (num_shaders[MESA_SHADER_TESS_EVAL] > 0 &&
             num_shaders[MESA_SHADER_TESS_CTRL] == 0) {
            linker_error(prog, "GLSL ES requires non-separable programs "
                         "containing a tessellation evaluation shader to also "
                         "be linked with a tessellation control shader\n");
            goto done;
         }
      }
   }

   /* Compute shaders have additional restrictions. */
   if (num_shaders[MESA_SHADER_COMPUTE] > 0 &&
       num_shaders[MESA_SHADER_COMPUTE] != prog->NumShaders) {
      linker_error(prog, "Compute shaders may not be linked with any other "
                   "type of shader\n");
   }

   /* Link all shaders for a particular stage and validate the result.
    */
   for (int stage = 0; stage < MESA_SHADER_STAGES; stage++) {
      if (num_shaders[stage] > 0) {
         gl_linked_shader *const sh =
            link_intrastage_shaders(mem_ctx, ctx, prog, shader_list[stage],
                                    num_shaders[stage], false);

         if (!prog->data->LinkStatus) {
            if (sh)
               _mesa_delete_linked_shader(ctx, sh);
            goto done;
         }

         prog->_LinkedShaders[stage] = sh;
         prog->data->linked_stages |= 1 << stage;
      }
   }

done:
   for (unsigned i = 0; i < MESA_SHADER_STAGES; i++) {
      free(shader_list[i]);
      if (prog->_LinkedShaders[i] == NULL)
         continue;

      /* Do a final validation step to make sure that the IR wasn't
       * invalidated by any modifications performed after intrastage linking.
       */
      validate_ir_tree(prog->_LinkedShaders[i]->ir);

      /* Retain any live IR, but trash the rest. */
      reparent_ir(prog->_LinkedShaders[i]->ir, prog->_LinkedShaders[i]->ir);

      /* The symbol table in the linked shaders may contain references to
       * variables that were removed (e.g., unused uniforms).  Since it may
       * contain junk, there is no possible valid use.  Delete it and set the
       * pointer to NULL.
       */
      delete prog->_LinkedShaders[i]->symbols;
      prog->_LinkedShaders[i]->symbols = NULL;
   }

   ralloc_free(mem_ctx);
}

void
resource_name_updated(struct gl_resource_name *name)
{
   if (name->string) {
      name->length = strlen(name->string);

      const char *last_square_bracket = strrchr(name->string, '[');
      if (last_square_bracket) {
         name->last_square_bracket = last_square_bracket - name->string;
         name->suffix_is_zero_square_bracketed =
            strcmp(last_square_bracket, "[0]") == 0;
      } else {
         name->last_square_bracket = -1;
         name->suffix_is_zero_square_bracketed = false;
      }
   } else {
      name->length = 0;
      name->last_square_bracket = -1;
      name->suffix_is_zero_square_bracketed = false;
   }
}
