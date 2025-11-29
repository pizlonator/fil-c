/*
 * Copyright © 2023 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

/* Introduction
 * ============
 *
 * This pass optimizes varyings between 2 shaders, which means dead input/
 * output removal, constant and uniform load propagation, deduplication,
 * compaction, and inter-shader code motion. This is used during the shader
 * linking process.
 *
 *
 * Notes on behavior
 * =================
 *
 * The pass operates on scalar varyings using 32-bit and 16-bit types. Vector
 * varyings are not allowed.
 *
 * Indirectly-indexed varying slots (not vertices) are not optimized or
 * compacted, but unused slots of indirectly-indexed varyings are still filled
 * with directly-indexed varyings during compaction. Indirectly-indexed
 * varyings are still removed if they are unused by the other shader.
 *
 * Indirectly-indexed vertices don't disallow optimizations, but compromises
 * are made depending on how they are accessed. They are common in TCS, TES,
 * and GS, so there is a desire to optimize them as much as possible. More on
 * that in various sections below.
 *
 * Transform feedback doesn't prevent most optimizations such as constant
 * propagation and compaction. Shaders can be left with output stores that set
 * the no_varying flag, meaning the output is not consumed by the next shader,
 * which means that optimizations did their job and now the output is only
 * consumed by transform feedback.
 *
 * All legacy varying slots are optimized when it's allowed.
 *
 *
 * Convergence property of shader outputs
 * ======================================
 *
 * When an output stores an SSA that is convergent and all stores of that
 * output appear in unconditional blocks or conditional blocks with
 * a convergent entry condition and the shader is not GS, it implies that all
 * vertices of that output have the same value, therefore the output can be
 * promoted to flat because all interpolation modes lead to the same result
 * as flat. Such outputs are opportunistically compacted with both flat and
 * non-flat varyings based on whichever has unused slots in their vec4s. This
 * pass refers to such inputs, outputs, and varyings as "convergent" (meaning
 * all vertices are always equal).
 *
 * Flat varyings are the only ones that are never considered convergent
 * because we want the flexibility to pack convergent varyings with both flat
 * and non-flat varyings, and since flat varyings can contain integers and
 * doubles, we can never interpolate them as FP32 or FP16. Optimizations start
 * with separate interpolated, flat, and convergent groups of varyings, and
 * they choose whether they want to promote convergent to interpolated or
 * flat, or whether to leave that decision to the end when the compaction
 * happens.
 *
 * TES patch inputs are always convergent because they are uniform within
 * a primitive.
 *
 *
 * Optimization steps
 * ==================
 *
 * 1. Determine which varying slots can be optimized and how.
 *
 *    * When a varying is said to be "optimized" in the following text, it
 *      means all optimizations are performed, such as removal, constant
 *      propagation, and deduplication.
 *    * All VARn, PATCHn, and FOGC varyings are always optimized and
 *      compacted.
 *    * PRIMITIVE_ID is treated as VARn in (GS, FS).
 *    * TEXn are removed if they are dead (except TEXn inputs, which can't be
 *      removed due to being affected by the coord replace state). TEXn can’t
 *      also be optimized or compacted due to being affected by the coord
 *      replace state. TEXn not consumed by FS are treated as VARn.
 *    * COLn and BFCn only propagate constants if they are between 0 and 1
 *      because of the clamp vertex color state, and they are only
 *      deduplicated and compacted among themselves because they are affected
 *      by the flat shade, provoking vertex, two-side color selection, and
 *      clamp vertex color states. COLn and BFCn not consumed by FS are
 *      treated as VARn.
 *    * All system value outputs like POS, PSIZ, CLIP_DISTn, etc. can’t be
 *      removed, but they are demoted to sysval-only outputs by setting
 *      the "no_varying" flag (i.e. they can be removed as varyings), so
 *      drivers should look at the "no_varying" flag. If an output is not
 *      a sysval output in a specific stage, it's treated as VARn. (such as
 *      POS in TCS)
 *    * TESS_LEVEL_* inputs in TES can’t be touched if TCS is missing.
 *
 * 2. Remove unused inputs and outputs
 *
 *    * Outputs not used in the next shader are removed.
 *    * Inputs not initialized by the previous shader are replaced with undef
 *      except:
 *      * LAYER and VIEWPORT are replaced with 0 in FS.
 *      * TEXn.xy is untouched because the coord replace state can set it, and
 *        TEXn.zw is replaced by (0, 1), which is equal to the coord replace
 *        value.
 *    * Output loads that have no output stores anywhere in the shader are
 *      replaced with undef. (for TCS, though it works with any shader)
 *    * Output stores with transform feedback are preserved, but get
 *      the “no_varying” flag, meaning they are not consumed by the next
 *      shader stage. Later, transform-feedback-only varyings are compacted
 *      (relocated) such that they are always last.
 *    * TCS outputs that are read by TCS, but not used by TES get
 *      the "no_varying" flag to indicate that they are only read by TCS and
 *      not consumed by TES. Later, such TCS outputs are compacted (relocated)
 *      such that they are always last to keep all outputs consumed by TES
 *      consecutive without holes.
 *
 * 3. Constant, uniform, UBO load, and uniform expression propagation
 *
 *    * Define “uniform expressions” as ALU expressions only sourcing
 *      constants, uniforms, and UBO loads.
 *    * Constants, uniforms, UBO loads, and uniform expressions stored
 *      in outputs are moved into the next shader, and the outputs are removed.
 *    * The same propagation is done from output stores to output loads.
 *      (for TCS, though it works with any shader)
 *    * If there are multiple stores to the same output, all such stores
 *      should store the same constant, uniform, UBO load, or uniform
 *      expression for the expression to be propagated. If an output has
 *      multiple vertices, all vertices should store the same expression.
 *    * nir->options has callbacks that are used to estimate the cost of
 *      uniform expressions that drivers can set to control the complexity of
 *      uniform expressions that are propagated. This is to ensure that
 *      we don't increase the GPU overhead measurably by moving code across
 *      pipeline stages that amplify GPU work.
 *    * Special cases:
 *      * Constant COLn and BFCn are propagated only if the constants are
 *        in the [0, 1] range because of the clamp vertex color state.
 *        If both COLn and BFCn are written, they must write the same
 *        constant. If BFCn is written but not COLn, the constant is
 *        propagated from BFCn to COLn.
 *      * TEX.xy is untouched because of the coord replace state.
 *        If TEX.zw is (0, 1), only those constants are propagated because
 *        they match the coord replace values.
 *      * CLIP_DISTn, LAYER and VIEWPORT are always propagated.
 *        Eliminated output stores get the "no_varying" flag if they are also
 *        xfb stores or write sysval outputs.
 *
 * 4. Remove duplicated output components
 *
 *    * By comparing SSA defs.
 *    * If there are multiple stores to the same output, all such stores
 *      should store the same SSA as all stores of another output for
 *      the output to be considered duplicated. If an output has multiple
 *      vertices, all vertices should store the same SSA.
 *    * Deduplication can only be done between outputs of the same category.
 *      Those are: interpolated, patch, flat, interpolated color, flat color,
 *                 and conditionally interpolated color based on the flat
 *                 shade state
 *    * Everything is deduplicated except TEXn due to the coord replace state.
 *    * Eliminated output stores get the "no_varying" flag if they are also
 *      xfb stores or write sysval outputs.
 *
 * 5. Backward inter-shader code motion
 *
 *    "Backward" refers to moving code in the opposite direction that shaders
 *    are executed, i.e. moving code from the consumer to the producer.
 *
 *    Fragment shader example:
 *    ```
 *       result = input0 * uniform + input1 * constant + UBO.variable;
 *    ```
 *
 *    The computation of "result" in the above example can be moved into
 *    the previous shader and both inputs can be replaced with a new input
 *    holding the value of "result", thus making the shader smaller and
 *    possibly reducing the number of inputs, uniforms, and UBOs by 1.
 *
 *    Such code motion can be performed for any expression sourcing only
 *    inputs, constants, and uniforms except for fragment shaders, which can
 *    also do it but with the following limitations:
 *    * Only these transformations can be perfomed with interpolated inputs
 *      and any composition of these transformations (such as lerp), which can
 *      all be proven mathematically:
 *      * interp(x, i, j) + interp(y, i, j) = interp(x + y, i, j)
 *      * interp(x, i, j) + convergent_expr = interp(x + convergent_expr, i, j)
 *      * interp(x, i, j) * convergent_expr = interp(x * convergent_expr, i, j)
 *        * all of these transformations are considered "inexact" in NIR
 *        * interp interpolates an input according to the barycentric
 *          coordinates (i, j), which are different for perspective,
 *          noperspective, center, centroid, sample, at_offset, and at_sample
 *          modes.
 *        * convergent_expr is any expression sourcing only constants,
 *          uniforms, and convergent inputs. The only requirement on
 *          convergent_expr is that it doesn't vary between vertices of
 *          the same primitive, but it can vary between primitives.
 *    * If inputs are flat or convergent, there are no limitations on
 *      expressions that can be moved.
 *    * Interpolated and flat inputs can't mix in the same expression, but
 *      convergent inputs can mix with both.
 *    * The interpolation qualifier of the new input is inherited from
 *      the removed non-convergent inputs that should all have the same (i, j).
 *      If there are no non-convergent inputs, then the new input is declared
 *      as flat (for simplicity; we can't choose the barycentric coordinates
 *      at random because AMD doesn't like when there are multiple sets of
 *      barycentric coordinates in the same shader unnecessarily).
 *    * Inf values break code motion across interpolation. See the section
 *      discussing how we handle it near the end.
 *
 *    The above rules also apply to open-coded TES input interpolation, which
 *    is handled the same as FS input interpolation. The only differences are:
 *    * Open-coded TES input interpolation must match one of the allowed
 *      equations. Different interpolation equations are treated the same as
 *      different interpolation qualifiers in FS.
 *    * Patch varyings are always treated as convergent.
 *
 *    Prerequisites:
 *    * We need a post-dominator tree that is constructed from a graph where
 *      vertices are instructions and directed edges going into them are
 *      the values of their source operands. This is different from how NIR
 *      dominance works, which represents all instructions within a basic
 *      block as a linear chain of vertices in the graph.
 *      In our graph, all loads without source operands and all constants are
 *      entry nodes in the graph, and all stores and discards are exit nodes
 *      in the graph. Each shader can have multiple disjoint graphs where
 *      the Lowest Common Ancestor of 2 instructions doesn't exist.
 *    * Given the above definition, the instruction whose result is the best
 *      candidate for a new input is the farthest instruction that
 *      post-dominates one of more inputs and is movable between shaders.
 *
 *    Algorithm Idea Part 1: Search
 *    * Pick any input load that is hypothetically movable and call it
 *      the iterator.
 *    * Get the immediate post-dominator of the iterator, and if it's movable,
 *      replace the iterator with it.
 *    * Repeat the previous step until the obtained immediate post-dominator
 *      is not movable.
 *    * The iterator now contains the farthest post-dominator that is movable.
 *    * Gather all input loads that the post-dominator consumes.
 *    * For each of those input loads, all matching output stores must be
 *      in the same block (because they will be replaced by a single store).
 *
 *    Algorithm Idea Part 2: Code Motion
 *    * Clone the post-dominator in the producer except input loads, which
 *      should be replaced by stored output values. Uniform and UBO loads,
 *      if any, should be cloned too.
 *    * Remove the original output stores.
 *    * Replace the post-dominator from the consumer with a new input load.
 *    * The step above makes the post-dominated input load that we picked
 *      at the beginning dead, but other input loads used by the post-
 *      dominator might still have other uses (shown in the example below).
 *
 *    Example SSA-use graph - initial shader and the result:
 *    ```
 *          input0 input1             input0 input1
 *              \   / \                  |      \
 *    constant   alu  ...    ======>     |     ...
 *           \   /
 *            alu
 *      (post-dominator)
 *    ```
 *
 *    Description:
 *       On the right, the algorithm moved the constant and both ALU opcodes
 *       into the previous shader and input0 now contains the value of
 *       the post-dominator. input1 stays the same because it still has one
 *       use left. If input1 hadn't had the other use, it would have been
 *       removed.
 *
 *    If the algorithm moves any code, the algorithm is repeated until there
 *    is no code that it can move.
 *
 *    Which shader pairs are supported:
 *    * (VS, FS), (TES, FS): yes, fully
 *      * Limitation: If Infs must be preserved, no code is moved across
 *                    interpolation, so only flat varyings are optimized.
 *    * (VS, TCS), (VS, GS), (TES, GS): no, but possible -- TODO
 *      * Current behavior:
 *        * Per-vertex inputs are rejected.
 *      * Possible solution:
 *        * All input loads used by an accepted post-dominator must use
 *          the same vertex index. The post-dominator must use all loads with
 *          that vertex index.
 *        * If a post-dominator is found for an input load from a specific
 *          slot, all other input loads from that slot must also have
 *          an accepted post-dominator, and all such post-dominators should
 *          be identical expressions.
 *    * (TCS, TES), (VS, TES): yes, with limitations
 *      * Limitations:
 *        * Only 1 store and 1 load per slot allowed.
 *        * No output loads allowed.
 *        * All stores used by an accepted post-dominator must be in
 *          the same block.
 *        * TCS barriers don't matter because there are no output loads.
 *        * Patch varyings are handled trivially with the above constraints.
 *        * Per-vertex outputs should only be indexed by gl_InvocationID.
 *        * An interpolated TES load is any ALU instruction that computes
 *          the result of linear interpolation of per-vertex inputs from
 *          the same slot using gl_TessCoord. If such an ALU instruction is
 *          found, it must be the only one, and all per-vertex input loads
 *          from that slot must feed into it. The interpolation equation must
 *          be equal to one of the allowed equations. Then the same rules as
 *          for interpolated FS inputs are used, treating different
 *          interpolation equations just like different interpolation
 *          qualifiers.
 *        * Patch inputs are treated as convergent, which means they are
 *          allowed to be in the same movable expression as interpolated TES
 *          inputs, and the same rules as for convergent FS inputs apply.
 *    * (GS, FS), (MS, FS): no
 *      * Workaround: Add a passthrough VS between GS/MS and FS, run
 *                    the pass on the (VS, FS) pair to move code out of FS,
 *                    and inline that VS at the end of your hw-specific
 *                    GS/MS if it's possible.
 *    * (TS, MS): no
 *
 *    The disadvantage of using the post-dominator tree is that it's a tree,
 *    which means there is only 1 post-dominator of each input. This example
 *    shows a case that could be optimized by replacing 3 inputs with 2 inputs,
 *    reducing the number of inputs by 1, but the immediate post-dominator of
 *    all input loads is NULL:
 *    ```
 *        temp0 = input0 + input1 + input2;
 *        temp1 = input0 + input1 * const1 + input2 * const2;
 *    ```
 *
 *    If there is a graph algorithm that returns the best solution to
 *    the above case (which is temp0 and temp1 to replace all 3 inputs), let
 *    us know.
 *
 * 6. Forward inter-shader code motion
 *
 *    TODO: Not implemented. The text below is a draft of the description.
 *
 *    "Forward" refers to moving code in the direction that shaders are
 *    executed, i.e. moving code from the producer to the consumer.
 *
 *    Vertex shader example:
 *    ```
 *       output0 = value + 1;
 *       output1 = value * 2;
 *    ```
 *
 *    Both outputs can be replaced by 1 output storing "value", and both ALU
 *    operations can be moved into the next shader.
 *
 *    The same dominance algorithm as in the previous optimization is used,
 *    except that:
 *    * Instead of inputs, we use outputs.
 *    * Instead of a post-dominator tree, we use a dominator tree of the exact
 *      same graph.
 *
 *    The algorithm idea is: For each pair of 2 output stores, find their
 *    Lowest Common Ancestor in the dominator tree, and that's a candidate
 *    for a new output. All movable loads like load_const should be removed
 *    from the graph, otherwise the LCA wouldn't exist.
 *
 *    The limitations on instructions that can be moved between shaders across
 *    interpolated loads are exactly the same as the previous optimization.
 *
 *    nir->options has callbacks that are used to estimate the cost of
 *    expressions that drivers can set to control the complexity of
 *    expressions that can be moved to later shaders. This is to ensure that
 *    we don't increase the GPU overhead measurably by moving code across
 *    pipeline stages that amplify GPU work.
 *
 * 7. Compaction to vec4 slots (AKA packing)
 *
 *    First, varyings are divided into these groups, and each group is
 *    compacted separately with some exceptions listed below:
 *
 *    Non-FS groups (patch and non-patch are packed separately):
 *    * 32-bit flat
 *    * 16-bit flat
 *    * 32-bit no-varying (TCS outputs read by TCS but not TES)
 *    * 16-bit no-varying (TCS outputs read by TCS but not TES)
 *
 *    FS groups:
 *    * 32-bit interpolated (always FP32)
 *    * 32-bit flat
 *    * 32-bit convergent (always FP32)
 *    * 16-bit interpolated (always FP16)
 *    * 16-bit flat
 *    * 16-bit convergent (always FP16)
 *    * 32-bit transform feedback only
 *    * 16-bit transform feedback only
 *
 *    Then, all scalar varyings are relocated into new slots, starting from
 *    VAR0.x and increasing the scalar slot offset in 32-bit or 16-bit
 *    increments. Rules:
 *    * Both 32-bit and 16-bit flat varyings are packed in the same vec4.
 *    * Convergent varyings can be packed with interpolated varyings of
 *      the same type or flat. The group to pack with is chosen based on
 *      whichever has unused scalar slots because we want to reduce the total
 *      number of vec4s. After filling all unused scalar slots, the remaining
 *      convergent varyings are packed as flat.
 *    * Transform-feedback-only slots and no-varying slots are packed last,
 *      so that they are consecutive and not intermixed with varyings consumed
 *      by the next shader stage, and 32-bit and 16-bit slots are packed in
 *      the same vec4. This allows reducing memory for outputs by ignoring
 *      the trailing outputs that the next shader stage doesn't read.
 *
 *    In the end, we should end up with these groups for FS:
 *    * 32-bit interpolated (always FP32) on separate vec4s
 *    * 16-bit interpolated (always FP16) on separate vec4s
 *    * 32-bit flat and 16-bit flat, mixed in the same vec4
 *    * 32-bit and 16-bit transform feedback only, sharing vec4s with flat
 *
 *    Colors are compacted the same but separately because they can't be mixed
 *    with VARn. Colors are divided into 3 FS groups. They are:
 *    * 32-bit maybe-interpolated (affected by the flat-shade state)
 *    * 32-bit interpolated (not affected by the flat-shade state)
 *    * 32-bit flat (not affected by the flat-shade state)
 *
 *    To facilitate driver-specific output merging, color channels are
 *    assigned in a rotated order depending on which one the first unused VARn
 *    channel is. For example, if the first unused VARn channel is VAR0.z,
 *    color channels are allocated in this order:
 *       COL0.z, COL0.w, COL0.x, COL0.y, COL1.z, COL1.w, COL1.x, COL1.y
 *    The reason is that some drivers merge outputs if each output sets
 *    different components, for example 2 outputs defining VAR0.xy and COL0.z.
 *    If drivers do interpolation in the fragment shader and color
 *    interpolation can differ for each component, VAR0.xy and COL.z can be
 *    stored in the same output storage slot, and the consumer can load VAR0
 *    and COL0 from the same slot.
 *
 *    If COLn, BFCn, and TEXn are transform-feedback-only, they are moved to
 *    VARn. PRIMITIVE_ID in (GS, FS) and FOGC in (xx, FS) are always moved to
 *    VARn for better packing.
 *
 *
 * Issue: Interpolation converts Infs to NaNs
 * ==========================================
 *
 * Interpolation converts Infs to NaNs, i.e. interp(Inf, i, j) = NaN, which
 * impacts and limits backward inter-shader code motion, uniform expression
 * propagation, and compaction.
 *
 * When we decide not to interpolate a varying, we need to convert Infs to
 * NaNs manually. Infs can be converted to NaNs like this: x*0 + x
 * (suggested by Ian Romanick, the multiplication must be "exact")
 *
 * Changes to optimizations:
 * - When we propagate a uniform expression and NaNs must be preserved,
 *   convert Infs in the result to NaNs using "x*0 + x" in the consumer.
 * - When we change interpolation to flat for convergent varyings and NaNs
 *   must be preserved, apply "x*0 + x" to the stored output value
 *   in the producer.
 * - There is no solution for backward inter-shader code motion with
 *   interpolation if Infs must be preserved. As an alternative, we can allow
 *   code motion across interpolation only for specific shader hashes in
 *   can_move_alu_across_interp. We can use shader-db to automatically produce
 *   a list of shader hashes that benefit from this optimization.
 *
 *
 * Usage
 * =====
 *
 * Requirements:
 * - ALUs should be scalarized
 * - Dot products and other vector opcodes should be lowered (recommended)
 * - Input loads and output stores should be scalarized
 * - 64-bit varyings should be lowered to 32 bits
 * - nir_vertex_divergence_analysis must be called on the producer if
 *   the constumer is a fragment shader
 *
 * It's recommended to run this for all shader pairs from the first shader
 * to the last shader first (to propagate constants etc.). If the optimization
 * of (S1, S2) stages leads to changes in S1, remember the highest S1. Then
 * re-run this for all shader pairs in the descending order from S1 to VS.
 *
 * NIR optimizations should be performed after every run that changes the IR.
 *
 *
 * Analyzing the optimization potential of linking separate shaders
 * ================================================================
 *
 * We can use this pass in an analysis pass that decides whether a separate
 * shader has the potential to benefit from full draw-time linking. The way
 * it would work is that we would create a passthrough shader adjacent to
 * the separate shader, run this pass on both shaders, and check if the number
 * of varyings decreased. This way we can decide to perform the draw-time
 * linking only if we are confident that it would help performance.
 *
 * TODO: not implemented, mention the pass that implements it
 */

#include "nir.h"
#include "nir_builder.h"
#include "util/u_math.h"
#include "util/u_memory.h"

/* nir_opt_varyings works at scalar 16-bit granularity across all varyings.
 *
 * Slots (i % 8 == 0,2,4,6) are 32-bit channels or low bits of 16-bit channels.
 * Slots (i % 8 == 1,3,5,7) are high bits of 16-bit channels. 32-bit channels
 * don't set these slots as used in bitmasks.
 */
#define NUM_SCALAR_SLOTS  (NUM_TOTAL_VARYING_SLOTS * 8)

/* Fragment shader input slots can be packed with indirectly-indexed vec4
 * slots if there are unused components, but only if the vec4 slot has
 * the same interpolation type. There are only 3 types: FLAT, FP32, FP16.
 */
enum fs_vec4_type {
   FS_VEC4_TYPE_NONE = 0,
   FS_VEC4_TYPE_FLAT,
   FS_VEC4_TYPE_INTERP_FP32,
   FS_VEC4_TYPE_INTERP_FP16,
   FS_VEC4_TYPE_INTERP_COLOR,
   FS_VEC4_TYPE_INTERP_EXPLICIT,
   FS_VEC4_TYPE_INTERP_EXPLICIT_STRICT,
   FS_VEC4_TYPE_PER_PRIMITIVE,
};

static unsigned
get_scalar_16bit_slot(nir_io_semantics sem, unsigned component)
{
   return sem.location * 8 + component * 2 + sem.high_16bits;
}

static unsigned
intr_get_scalar_16bit_slot(nir_intrinsic_instr *intr)
{
    return get_scalar_16bit_slot(nir_intrinsic_io_semantics(intr),
                                 nir_intrinsic_component(intr));
}

static unsigned
vec4_slot(unsigned scalar_slot)
{
   return scalar_slot / 8;
}

struct list_node {
   struct list_head head;
   nir_intrinsic_instr *instr;
};

/* Information about 1 scalar varying slot for both shader stages. */
struct scalar_slot {
   struct {
      /* Linked list of all store instructions writing into the scalar slot
       * in the producer.
       */
      struct list_head stores;

      /* Only for TCS: Linked list of all load instructions read the scalar
       * slot in the producer.
       */
      struct list_head loads;

      /* If there is only one store instruction or if all store instructions
       * store the same value in the producer, this is the instruction
       * computing the stored value. Used by constant and uniform propagation
       * to the next shader.
       */
      nir_instr *value;
   } producer;

   struct {
      /* Linked list of all load instructions loading from the scalar slot
       * in the consumer.
       */
      struct list_head loads;

      /* The result of TES input interpolation. */
      nir_alu_instr *tes_interp_load;
      unsigned tes_interp_mode;  /* FLAG_INTERP_TES_* */
      nir_def *tes_load_tess_coord;
   } consumer;

   /* The number of accessed slots if this slot has indirect indexing. */
   unsigned num_slots;
};

struct linkage_info {
   struct scalar_slot slot[NUM_SCALAR_SLOTS];

   bool spirv;
   bool can_move_uniforms;
   bool can_move_ubos;

   gl_shader_stage producer_stage;
   gl_shader_stage consumer_stage;
   nir_builder producer_builder;
   nir_builder consumer_builder;
   unsigned max_varying_expression_cost;

   /* Memory context for linear_alloc_child (fast allocation). */
   void *linear_mem_ctx;

   /* If any component of a vec4 slot is accessed indirectly, this is its
    * FS vec4 qualifier type, which is either FLAT, FP32, or FP16.
    * Components with different qualifier types can't be compacted
    * in the same vec4.
    */
   uint8_t fs_vec4_type[NUM_TOTAL_VARYING_SLOTS];

   /* Mask of all varyings that can be removed. Only a few non-VARn non-PATCHn
    * varyings can't be removed.
    */
   BITSET_DECLARE(removable_mask, NUM_SCALAR_SLOTS);

   /* Mask of all slots that have transform feedback info. */
   BITSET_DECLARE(xfb_mask, NUM_SCALAR_SLOTS);

   /* Mask of all slots that have transform feedback info, but are not used
    * by the next shader. Separate masks for 32-bit and 16-bit outputs.
    */
   BITSET_DECLARE(xfb32_only_mask, NUM_SCALAR_SLOTS);
   BITSET_DECLARE(xfb16_only_mask, NUM_SCALAR_SLOTS);

   /* Mask of all TCS->TES slots that are read by TCS, but not TES. */
   BITSET_DECLARE(no_varying32_mask, NUM_SCALAR_SLOTS);
   BITSET_DECLARE(no_varying16_mask, NUM_SCALAR_SLOTS);

   /* Mask of all slots accessed with indirect indexing. */
   BITSET_DECLARE(indirect_mask, NUM_SCALAR_SLOTS);

   /* The following masks only contain slots that can be compacted and
    * describe the groups in which they should be compacted. Non-fragment
    * shaders only use the flat bitmasks.
    *
    * Some legacy varyings are excluded when they can't be compacted due to
    * being affected by pipeline states (like coord replace). That only
    * applies to xx->FS shader pairs. Other shader pairs get all legacy
    * varyings compacted and relocated to VARn.
    *
    * Indirectly-indexed varyings are also excluded because they are not
    * compacted.
    */
   BITSET_DECLARE(interp_fp32_mask, NUM_SCALAR_SLOTS);
   BITSET_DECLARE(interp_fp16_mask, NUM_SCALAR_SLOTS);
   BITSET_DECLARE(flat32_mask, NUM_SCALAR_SLOTS);
   BITSET_DECLARE(flat16_mask, NUM_SCALAR_SLOTS);
   BITSET_DECLARE(interp_explicit32_mask, NUM_SCALAR_SLOTS);
   BITSET_DECLARE(interp_explicit16_mask, NUM_SCALAR_SLOTS);
   BITSET_DECLARE(interp_explicit_strict32_mask, NUM_SCALAR_SLOTS);
   BITSET_DECLARE(interp_explicit_strict16_mask, NUM_SCALAR_SLOTS);
   BITSET_DECLARE(per_primitive32_mask, NUM_SCALAR_SLOTS);
   BITSET_DECLARE(per_primitive16_mask, NUM_SCALAR_SLOTS);

   /* Color interpolation unqualified (follows the flat-shade state). */
   BITSET_DECLARE(color32_mask, NUM_SCALAR_SLOTS);

   /* Mask of output components that have only one store instruction, or if
    * they have multiple store instructions, all those instructions store
    * the same value. If the output has multiple vertices, all vertices store
    * the same value. This is a useful property for:
    * - constant and uniform propagation to the next shader
    * - deduplicating outputs
    */
   BITSET_DECLARE(output_equal_mask, NUM_SCALAR_SLOTS);

   /* Mask of output components that store values that are convergent,
    * i.e. all values stored into the outputs are equal within a primitive.
    *
    * This is different from output_equal_mask, which says that all stores
    * to the same slot in the same thread are equal, while this says that
    * each store to the same slot can be different, but it always stores
    * a convergent value, which means the stored value is equal among all
    * threads within a primitive.
    *
    * The advantage is that these varyings can always be promoted to flat
    * regardless of the original interpolation mode, and they can always be
    * compacted with both interpolated and flat varyings.
    */
   BITSET_DECLARE(convergent32_mask, NUM_SCALAR_SLOTS);
   BITSET_DECLARE(convergent16_mask, NUM_SCALAR_SLOTS);
};

/******************************************************************
 * HELPERS
 ******************************************************************/

/* Return whether the low or high 16-bit slot is 1. */
#define BITSET_TEST32(m, b) \
   (BITSET_TEST(m, (b) & ~0x1) || BITSET_TEST(m, ((b) & ~0x1) + 1))

static void
print_linkage(struct linkage_info *linkage)
{
   printf("Linkage: %s -> %s\n",
          _mesa_shader_stage_to_abbrev(linkage->producer_stage),
          _mesa_shader_stage_to_abbrev(linkage->consumer_stage));

   for (unsigned i = 0; i < NUM_SCALAR_SLOTS; i++) {
      struct scalar_slot *slot = &linkage->slot[i];

      if (!slot->num_slots &&
          list_is_empty(&slot->producer.stores) &&
          list_is_empty(&slot->producer.loads) &&
          list_is_empty(&slot->consumer.loads) &&
          !BITSET_TEST(linkage->removable_mask, i) &&
          !BITSET_TEST(linkage->indirect_mask, i) &&
          !BITSET_TEST(linkage->xfb32_only_mask, i) &&
          !BITSET_TEST(linkage->xfb16_only_mask, i) &&
          !BITSET_TEST(linkage->no_varying32_mask, i) &&
          !BITSET_TEST(linkage->no_varying16_mask, i) &&
          !BITSET_TEST(linkage->interp_fp32_mask, i) &&
          !BITSET_TEST(linkage->interp_fp16_mask, i) &&
          !BITSET_TEST(linkage->flat32_mask, i) &&
          !BITSET_TEST(linkage->flat16_mask, i) &&
          !BITSET_TEST(linkage->interp_explicit32_mask, i) &&
          !BITSET_TEST(linkage->interp_explicit16_mask, i) &&
          !BITSET_TEST(linkage->interp_explicit_strict32_mask, i) &&
          !BITSET_TEST(linkage->interp_explicit_strict16_mask, i) &&
          !BITSET_TEST(linkage->per_primitive32_mask, i) &&
          !BITSET_TEST(linkage->per_primitive16_mask, i) &&
          !BITSET_TEST(linkage->convergent32_mask, i) &&
          !BITSET_TEST(linkage->convergent16_mask, i) &&
          !BITSET_TEST(linkage->output_equal_mask, i))
         continue;

      printf("  %7s.%c.%s: num_slots=%2u%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
             gl_varying_slot_name_for_stage(vec4_slot(i),
                                            linkage->producer_stage) + 13,
             "xyzw"[(i / 2) % 4],
             i % 2 ? "hi" : "lo",
             slot->num_slots,
             BITSET_TEST(linkage->removable_mask, i) ? " removable" : "",
             BITSET_TEST(linkage->indirect_mask, i) ? " indirect" : "",
             BITSET_TEST(linkage->xfb32_only_mask, i) ? " xfb32_only" : "",
             BITSET_TEST(linkage->xfb16_only_mask, i) ? " xfb16_only" : "",
             BITSET_TEST(linkage->no_varying32_mask, i) ? " no_varying32" : "",
             BITSET_TEST(linkage->no_varying16_mask, i) ? " no_varying16" : "",
             BITSET_TEST(linkage->interp_fp32_mask, i) ? " interp_fp32" : "",
             BITSET_TEST(linkage->interp_fp16_mask, i) ? " interp_fp16" : "",
             BITSET_TEST(linkage->flat32_mask, i) ? " flat32" : "",
             BITSET_TEST(linkage->flat16_mask, i) ? " flat16" : "",
             BITSET_TEST(linkage->interp_explicit32_mask, i) ? " interp_explicit32" : "",
             BITSET_TEST(linkage->interp_explicit16_mask, i) ? " interp_explicit16" : "",
             BITSET_TEST(linkage->interp_explicit_strict32_mask, i) ? " interp_explicit_strict32" : "",
             BITSET_TEST(linkage->interp_explicit_strict16_mask, i) ? " interp_explicit_strict16" : "",
             BITSET_TEST(linkage->per_primitive32_mask, i) ? " per_primitive32" : "",
             BITSET_TEST(linkage->per_primitive32_mask, i) ? " per_primitive16" : "",
             BITSET_TEST(linkage->convergent32_mask, i) ? " convergent32" : "",
             BITSET_TEST(linkage->convergent16_mask, i) ? " convergent16" : "",
             BITSET_TEST(linkage->output_equal_mask, i) ? " output_equal" : "",
             !list_is_empty(&slot->producer.stores) ? " producer_stores" : "",
             !list_is_empty(&slot->producer.loads) ? " producer_loads" : "",
             !list_is_empty(&slot->consumer.loads) ? " consumer_loads" : "");
   }
}

static void
slot_disable_optimizations_and_compaction(struct linkage_info *linkage,
                                          unsigned i)
{
   BITSET_CLEAR(linkage->output_equal_mask, i);
   BITSET_CLEAR(linkage->convergent32_mask, i);
   BITSET_CLEAR(linkage->convergent16_mask, i);
   BITSET_CLEAR(linkage->interp_fp32_mask, i);
   BITSET_CLEAR(linkage->interp_fp16_mask, i);
   BITSET_CLEAR(linkage->flat32_mask, i);
   BITSET_CLEAR(linkage->flat16_mask, i);
   BITSET_CLEAR(linkage->interp_explicit32_mask, i);
   BITSET_CLEAR(linkage->interp_explicit16_mask, i);
   BITSET_CLEAR(linkage->interp_explicit_strict32_mask, i);
   BITSET_CLEAR(linkage->interp_explicit_strict16_mask, i);
   BITSET_CLEAR(linkage->per_primitive32_mask, i);
   BITSET_CLEAR(linkage->per_primitive16_mask, i);
   BITSET_CLEAR(linkage->no_varying32_mask, i);
   BITSET_CLEAR(linkage->no_varying16_mask, i);
   BITSET_CLEAR(linkage->color32_mask, i);
}

static void
clear_slot_info_after_removal(struct linkage_info *linkage, unsigned i, bool uses_xfb)
{
   slot_disable_optimizations_and_compaction(linkage, i);

   if (uses_xfb)
      return;

   linkage->slot[i].num_slots = 0;

   BITSET_CLEAR(linkage->indirect_mask, i);
   BITSET_CLEAR(linkage->removable_mask, i);

   /* Transform feedback stores can't be removed. */
   assert(!BITSET_TEST(linkage->xfb32_only_mask, i));
   assert(!BITSET_TEST(linkage->xfb16_only_mask, i));
}

static bool
has_xfb(nir_intrinsic_instr *intr)
{
   /* This means whether the instrinsic is ABLE to have xfb info. */
   if (!nir_intrinsic_has_io_xfb(intr))
      return false;

   unsigned comp = nir_intrinsic_component(intr);

   if (comp >= 2)
      return nir_intrinsic_io_xfb2(intr).out[comp - 2].num_components > 0;
   else
      return nir_intrinsic_io_xfb(intr).out[comp].num_components > 0;
}

static bool
is_interpolated_color(struct linkage_info *linkage, unsigned i)
{
   if (linkage->consumer_stage != MESA_SHADER_FRAGMENT)
      return false;

   /* BFCn stores are bunched in the COLn slots with COLn, so we should never
    * get BFCn here.
    */
   assert(vec4_slot(i) != VARYING_SLOT_BFC0 &&
          vec4_slot(i) != VARYING_SLOT_BFC1);

   return vec4_slot(i) == VARYING_SLOT_COL0 ||
          vec4_slot(i) == VARYING_SLOT_COL1;
}

static bool
is_interpolated_texcoord(struct linkage_info *linkage, unsigned i)
{
   if (linkage->consumer_stage != MESA_SHADER_FRAGMENT)
      return false;

   return vec4_slot(i) >= VARYING_SLOT_TEX0 &&
          vec4_slot(i) <= VARYING_SLOT_TEX7;
}

static bool
color_uses_shade_model(struct linkage_info *linkage, unsigned i)
{
   if (!is_interpolated_color(linkage, i))
      return false;

   list_for_each_entry(struct list_node, iter,
                       &linkage->slot[i].consumer.loads, head) {
      assert(iter->instr->intrinsic == nir_intrinsic_load_interpolated_input);

      nir_intrinsic_instr *baryc =
         nir_instr_as_intrinsic(iter->instr->src[0].ssa->parent_instr);
      if (nir_intrinsic_interp_mode(baryc) == INTERP_MODE_NONE)
         return true;
   }

   return false;
}

static bool
preserve_infs_nans(nir_shader *nir, unsigned bit_size)
{
   unsigned mode = nir->info.float_controls_execution_mode;

   return nir_is_float_control_inf_preserve(mode, bit_size) ||
          nir_is_float_control_nan_preserve(mode, bit_size);
}

static bool
preserve_nans(nir_shader *nir, unsigned bit_size)
{
   unsigned mode = nir->info.float_controls_execution_mode;

   return nir_is_float_control_nan_preserve(mode, bit_size);
}

static nir_def *
build_convert_inf_to_nan(nir_builder *b, nir_def *x)
{
   /* Do x*0 + x. The multiplication by 0 can't be optimized out. */
   nir_def *fma = nir_ffma_imm1(b, x, 0, x);
   nir_instr_as_alu(fma->parent_instr)->exact = true;
   return fma;
}

/******************************************************************
 * GATHERING INPUTS & OUTPUTS
 ******************************************************************/

static bool
is_active_sysval_output(struct linkage_info *linkage, unsigned slot,
                        nir_intrinsic_instr *intr)
{
   return nir_slot_is_sysval_output(vec4_slot(slot),
                                    linkage->consumer_stage) &&
          !nir_intrinsic_io_semantics(intr).no_sysval_output;
}

/**
 * This function acts like a filter. The pass won't touch varyings that
 * return false here, and the return value is saved in the linkage bitmasks,
 * so that all subpasses will *automatically* skip such varyings.
 */
static bool
can_remove_varying(struct linkage_info *linkage, gl_varying_slot location)
{
   if (linkage->consumer_stage == MESA_SHADER_FRAGMENT) {
      /* User-defined varyings and fog coordinates can always be removed. */
      if (location >= VARYING_SLOT_VAR0 ||
          location == VARYING_SLOT_FOGC)
         return true;

      /* Workaround for mesh shader multiview in RADV.
       * A layer output is inserted by ac_nir_lower_ngg which is called later.
       * Prevent removing the layer input from FS when producer is MS.
       */
      if (linkage->producer_stage == MESA_SHADER_MESH &&
          location == VARYING_SLOT_LAYER)
         return false;

      /* These can be removed as varyings, which means they will be demoted to
       * sysval-only outputs keeping their culling/rasterization functions
       * while not passing the values to FS. Drivers should handle
       * the "no_varying" semantic to benefit from this.
       *
       * Note: When removing unset LAYER and VIEWPORT FS inputs, they will
       *       be replaced by 0 instead of undef.
       */
      if (location == VARYING_SLOT_CLIP_DIST0 ||
          location == VARYING_SLOT_CLIP_DIST1 ||
          location == VARYING_SLOT_CULL_DIST0 ||
          location == VARYING_SLOT_CULL_DIST1 ||
          location == VARYING_SLOT_LAYER ||
          location == VARYING_SLOT_VIEWPORT)
         return true;

      /* COLn inputs can be removed only if both COLn and BFCn are not
       * written. Both COLn and BFCn outputs can be removed if COLn inputs
       * aren't read.
       *
       * TEXn inputs can never be removed in FS because of the coord replace
       * state, but TEXn outputs can be removed if they are not read by FS.
       */
      if (location == VARYING_SLOT_COL0 ||
          location == VARYING_SLOT_COL1 ||
          location == VARYING_SLOT_BFC0 ||
          location == VARYING_SLOT_BFC1 ||
          (location >= VARYING_SLOT_TEX0 && location <= VARYING_SLOT_TEX7))
         return true;

      /* "GS -> FS" can remove the primitive ID if not written or not read. */
      if ((linkage->producer_stage == MESA_SHADER_GEOMETRY ||
           linkage->producer_stage == MESA_SHADER_MESH) &&
          location == VARYING_SLOT_PRIMITIVE_ID)
         return true;

      /* No other varyings can be removed. */
      return false;
   } else if (linkage->consumer_stage == MESA_SHADER_TESS_EVAL) {
      /* Only VS->TES shouldn't remove TESS_LEVEL_* inputs because the values
       * come from glPatchParameterfv.
       *
       * For TCS->TES, TESS_LEVEL_* outputs can be removed as varyings, which
       * means they will be demoted to sysval-only outputs, so that drivers
       * know that TES doesn't read them.
       */
      if (linkage->producer_stage == MESA_SHADER_VERTEX &&
          (location == VARYING_SLOT_TESS_LEVEL_INNER ||
           location == VARYING_SLOT_TESS_LEVEL_OUTER))
         return false;

      return true;
   }

   /* All other varyings can be removed. */
   return true;
}

struct opt_options {
   bool propagate_uniform_expr:1;
   bool deduplicate:1;
   bool inter_shader_code_motion:1;
   bool compact:1;
   bool disable_all:1;
};

/**
 * Return which optimizations are allowed.
 */
static struct opt_options
can_optimize_varying(struct linkage_info *linkage, gl_varying_slot location)
{
   struct opt_options options_var = {
      .propagate_uniform_expr = true,
      .deduplicate = true,
      .inter_shader_code_motion = true,
      .compact = true,
   };
   struct opt_options options_color = {
      .propagate_uniform_expr = true, /* only constants in [0, 1] */
      .deduplicate = true,
      .compact = true,
   };
   struct opt_options options_tex = {
      .propagate_uniform_expr = true, /* only TEX.zw if equal to (0, 1) */
   };
   struct opt_options options_sysval_output = {
      .propagate_uniform_expr = true,
      .deduplicate = true,
   };
   struct opt_options options_tess_levels = {
      .propagate_uniform_expr = true,
      .deduplicate = true,
   };
   struct opt_options options_disable_all = {
      .disable_all = true,
   };

   assert(can_remove_varying(linkage, location));

   if (linkage->consumer_stage == MESA_SHADER_FRAGMENT) {
      /* xx -> FS */
      /* User-defined varyings and fog coordinates can always be optimized. */
      if (location >= VARYING_SLOT_VAR0 ||
          location == VARYING_SLOT_FOGC)
         return options_var;

      /* The primitive ID can always be optimized in GS -> FS and MS -> FS. */
      if ((linkage->producer_stage == MESA_SHADER_GEOMETRY ||
           linkage->producer_stage == MESA_SHADER_MESH) &&
          location == VARYING_SLOT_PRIMITIVE_ID)
         return options_var;

      /* Colors can only do constant propagation if COLn and BFCn store the
       * same constant and the constant is between 0 and 1 (because clamp
       * vertex color state is unknown). Uniform propagation isn't possible
       * because of the clamping.
       *
       * Color components can only be deduplicated and compacted among
       * themselves if they have the same interpolation qualifier, and can't
       * be mixed with other varyings.
       */
      if (location == VARYING_SLOT_COL0 ||
          location == VARYING_SLOT_COL1 ||
          location == VARYING_SLOT_BFC0 ||
          location == VARYING_SLOT_BFC1)
         return options_color;

      /* TEXn.zw can only be constant-propagated if the value is (0, 1)
       * because it matches the coord replace values.
       */
      if (location >= VARYING_SLOT_TEX0 && location <= VARYING_SLOT_TEX7)
         return options_tex;

      /* LAYER, VIEWPORT, CLIP_DISTn, and CULL_DISTn can only propagate
       * uniform expressions and be compacted (moved to VARn while keeping
       * the sysval outputs where they are).
       */
      if (location == VARYING_SLOT_LAYER ||
          location == VARYING_SLOT_VIEWPORT ||
          location == VARYING_SLOT_CLIP_DIST0 ||
          location == VARYING_SLOT_CLIP_DIST1 ||
          location == VARYING_SLOT_CULL_DIST0 ||
          location == VARYING_SLOT_CULL_DIST1)
         return options_sysval_output;

      /* Everything else can't be read by the consumer, such as POS, PSIZ,
       * CLIP_VERTEX, EDGE, PRIMITIVE_SHADING_RATE, etc.
       */
      return options_disable_all;
   }

   if (linkage->producer_stage == MESA_SHADER_TESS_CTRL) {
      /* TESS_LEVEL_* can only propagate uniform expressions.
       * Compaction is disabled because AMD doesn't want the varying to be
       * moved to PATCHn while keeping the sysval output where it is.
       */
      if (location == VARYING_SLOT_TESS_LEVEL_INNER ||
          location == VARYING_SLOT_TESS_LEVEL_OUTER)
         return options_tess_levels;
   }

   /* All other shader pairs, which are (VS, TCS), (TCS, TES), (VS, TES),
    * (TES, GS), and (VS, GS) can compact and optimize all varyings.
    */
   return options_var;
}

static bool
gather_inputs(struct nir_builder *builder, nir_intrinsic_instr *intr, void *cb_data)
{
   struct linkage_info *linkage = (struct linkage_info *)cb_data;

   if (intr->intrinsic != nir_intrinsic_load_input &&
       intr->intrinsic != nir_intrinsic_load_per_vertex_input &&
       intr->intrinsic != nir_intrinsic_load_interpolated_input &&
       intr->intrinsic != nir_intrinsic_load_input_vertex)
      return false;

   /* nir_lower_io_to_scalar is required before this */
   assert(intr->def.num_components == 1);
   /* Non-zero constant offsets should have been folded by
    * nir_io_add_const_offset_to_base.
    */
   nir_src offset = *nir_get_io_offset_src(intr);
   assert(!nir_src_is_const(offset) || nir_src_as_uint(offset) == 0);

   nir_io_semantics sem = nir_intrinsic_io_semantics(intr);

   if (!can_remove_varying(linkage, sem.location))
      return false;

   /* Insert the load into the list of loads for this scalar slot. */
   unsigned slot = intr_get_scalar_16bit_slot(intr);
   struct scalar_slot *in = &linkage->slot[slot];
   struct list_node *node = linear_alloc_child(linkage->linear_mem_ctx,
                                               sizeof(struct list_node));
   node->instr = intr;
   list_addtail(&node->head, &in->consumer.loads);
   in->num_slots = MAX2(in->num_slots, sem.num_slots);

   BITSET_SET(linkage->removable_mask, slot);

   enum fs_vec4_type fs_vec4_type = FS_VEC4_TYPE_NONE;

   /* Determine the type of the input for compaction. Other inputs
    * can be compacted with indirectly-indexed vec4 slots if they
    * have unused components, but only if they are of the same type.
    */
   if (linkage->consumer_stage == MESA_SHADER_FRAGMENT) {
      switch (intr->intrinsic) {
      case nir_intrinsic_load_input:
         if (sem.per_primitive)
            fs_vec4_type = FS_VEC4_TYPE_PER_PRIMITIVE;
         else
            fs_vec4_type = FS_VEC4_TYPE_FLAT;
         break;
      case nir_intrinsic_load_input_vertex:
         if (sem.interp_explicit_strict)
            fs_vec4_type = FS_VEC4_TYPE_INTERP_EXPLICIT_STRICT;
         else
            fs_vec4_type = FS_VEC4_TYPE_INTERP_EXPLICIT;
         break;
      case nir_intrinsic_load_interpolated_input:
         if (color_uses_shade_model(linkage, slot))
            fs_vec4_type = FS_VEC4_TYPE_INTERP_COLOR;
         else if (intr->def.bit_size == 32)
            fs_vec4_type = FS_VEC4_TYPE_INTERP_FP32;
         else if (intr->def.bit_size == 16)
            fs_vec4_type = FS_VEC4_TYPE_INTERP_FP16;
         else
            unreachable("invalid load_interpolated_input type");
         break;
      default:
         unreachable("unexpected input load intrinsic");
      }

      linkage->fs_vec4_type[sem.location] = fs_vec4_type;
   }

   /* Indirect indexing. */
   if (!nir_src_is_const(offset)) {
      /* Only the indirectly-indexed component is marked as indirect. */
      for (unsigned i = 0; i < sem.num_slots; i++)
         BITSET_SET(linkage->indirect_mask, slot + i * 8);

      /* Set the same vec4 type as the first element in all slots. */
      if (linkage->consumer_stage == MESA_SHADER_FRAGMENT) {
         for (unsigned i = 1; i < sem.num_slots; i++)
            linkage->fs_vec4_type[sem.location + i] = fs_vec4_type;
      }
      return false;
   }

   if (!can_optimize_varying(linkage, sem.location).compact)
      return false;

   /* Record inputs that can be compacted. */
   if (linkage->consumer_stage == MESA_SHADER_FRAGMENT) {
      switch (intr->intrinsic) {
      case nir_intrinsic_load_input:
         if (intr->def.bit_size == 32) {
            if (sem.per_primitive)
               BITSET_SET(linkage->per_primitive32_mask, slot);
            else
               BITSET_SET(linkage->flat32_mask, slot);
         } else if (intr->def.bit_size == 16) {
            if (sem.per_primitive)
               BITSET_SET(linkage->per_primitive16_mask, slot);
            else
               BITSET_SET(linkage->flat16_mask, slot);
         } else {
            unreachable("invalid load_input type");
         }
         break;
      case nir_intrinsic_load_input_vertex:
         if (sem.interp_explicit_strict) {
            if (intr->def.bit_size == 32)
               BITSET_SET(linkage->interp_explicit_strict32_mask, slot);
            else if (intr->def.bit_size == 16)
               BITSET_SET(linkage->interp_explicit_strict16_mask, slot);
            else
               unreachable("invalid load_input_vertex type");
         } else {
            if (intr->def.bit_size == 32)
               BITSET_SET(linkage->interp_explicit32_mask, slot);
            else if (intr->def.bit_size == 16)
               BITSET_SET(linkage->interp_explicit16_mask, slot);
            else
               unreachable("invalid load_input_vertex type");
         }
         break;
      case nir_intrinsic_load_interpolated_input:
         if (color_uses_shade_model(linkage, slot))
            BITSET_SET(linkage->color32_mask, slot);
         else if (intr->def.bit_size == 32)
            BITSET_SET(linkage->interp_fp32_mask, slot);
         else if (intr->def.bit_size == 16)
            BITSET_SET(linkage->interp_fp16_mask, slot);
         else
            unreachable("invalid load_interpolated_input type");
         break;
      default:
         unreachable("unexpected input load intrinsic");
      }
   } else {
      if (intr->def.bit_size == 32)
         BITSET_SET(linkage->flat32_mask, slot);
      else if (intr->def.bit_size == 16)
         BITSET_SET(linkage->flat16_mask, slot);
      else
         unreachable("invalid load_input type");
   }
   return false;
}

static bool
gather_outputs(struct nir_builder *builder, nir_intrinsic_instr *intr, void *cb_data)
{
   struct linkage_info *linkage = (struct linkage_info *)cb_data;

   if (intr->intrinsic != nir_intrinsic_store_output &&
       intr->intrinsic != nir_intrinsic_load_output &&
       intr->intrinsic != nir_intrinsic_store_per_vertex_output &&
       intr->intrinsic != nir_intrinsic_store_per_primitive_output &&
       intr->intrinsic != nir_intrinsic_load_per_vertex_output &&
       intr->intrinsic != nir_intrinsic_load_per_primitive_output)
      return false;

   bool is_store =
      intr->intrinsic == nir_intrinsic_store_output ||
      intr->intrinsic == nir_intrinsic_store_per_vertex_output ||
      intr->intrinsic == nir_intrinsic_store_per_primitive_output;

   if (is_store) {
      /* nir_lower_io_to_scalar is required before this */
      assert(intr->src[0].ssa->num_components == 1);
      /* nit_opt_undef is required before this. */
      assert(intr->src[0].ssa->parent_instr->type !=
            nir_instr_type_undef);
   } else {
      /* nir_lower_io_to_scalar is required before this */
      assert(intr->def.num_components == 1);
      /* Outputs loads are only allowed in TCS. */
      assert(linkage->producer_stage == MESA_SHADER_TESS_CTRL);
   }

   /* Non-zero constant offsets should have been folded by
    * nir_io_add_const_offset_to_base.
    */
   nir_src offset = *nir_get_io_offset_src(intr);
   assert(!nir_src_is_const(offset) || nir_src_as_uint(offset) == 0);

   nir_io_semantics sem = nir_intrinsic_io_semantics(intr);

   if (!can_remove_varying(linkage, sem.location))
      return false;

   /* For "xx -> FS", treat BFCn stores as COLn to make dead varying
    * elimination do the right thing automatically. The rules are:
    * - COLn inputs can be removed only if both COLn and BFCn are not
    *   written.
    * - Both COLn and BFCn outputs can be removed if COLn inputs
    *   aren't read.
    */
   if (linkage->consumer_stage == MESA_SHADER_FRAGMENT) {
      if (sem.location == VARYING_SLOT_BFC0)
         sem.location = VARYING_SLOT_COL0;
      else if (sem.location == VARYING_SLOT_BFC1)
         sem.location = VARYING_SLOT_COL1;
   }

   /* Insert the instruction into the list of stores or loads for this
    * scalar slot.
    */
   unsigned slot =
      get_scalar_16bit_slot(sem, nir_intrinsic_component(intr));

   struct scalar_slot *out = &linkage->slot[slot];
   struct list_node *node = linear_alloc_child(linkage->linear_mem_ctx,
                                               sizeof(struct list_node));
   node->instr = intr;
   out->num_slots = MAX2(out->num_slots, sem.num_slots);

   if (is_store) {
      list_addtail(&node->head, &out->producer.stores);

      if (has_xfb(intr)) {
         BITSET_SET(linkage->xfb_mask, slot);

         if (sem.no_varying &&
             !is_active_sysval_output(linkage, slot, intr)) {
            if (intr->src[0].ssa->bit_size == 32)
               BITSET_SET(linkage->xfb32_only_mask, slot);
            else if (intr->src[0].ssa->bit_size == 16)
               BITSET_SET(linkage->xfb16_only_mask, slot);
            else
               unreachable("invalid load_input type");
         }
      }
   } else {
      list_addtail(&node->head, &out->producer.loads);
   }

   BITSET_SET(linkage->removable_mask, slot);

   /* Indirect indexing. */
   if (!nir_src_is_const(offset)) {
      /* Only the indirectly-indexed component is marked as indirect. */
      for (unsigned i = 0; i < sem.num_slots; i++)
         BITSET_SET(linkage->indirect_mask, slot + i * 8);

      /* Set the same vec4 type as the first element in all slots. */
      if (linkage->consumer_stage == MESA_SHADER_FRAGMENT) {
         enum fs_vec4_type fs_vec4_type =
            linkage->fs_vec4_type[sem.location];

         for (unsigned i = 1; i < sem.num_slots; i++)
            linkage->fs_vec4_type[sem.location + i] = fs_vec4_type;
      }
      return false;
   }

   if (can_optimize_varying(linkage, sem.location).disable_all)
      return false;

   if (is_store) {
      nir_def *value = intr->src[0].ssa;

      const bool constant = value->parent_instr->type == nir_instr_type_load_const;

      /* If the store instruction is executed in a divergent block, the value
       * that's stored in the output becomes divergent.
       *
       * Mesh shaders get special treatment because we can't follow their topology,
       * so we only propagate constants.
       * TODO: revisit this when workgroup divergence analysis is merged.
       */
      const bool divergent = value->divergent ||
                             intr->instr.block->divergent ||
                             (!constant && linkage->producer_stage == MESA_SHADER_MESH);

      if (!out->producer.value) {
         /* This is the first store to this output. */
         BITSET_SET(linkage->output_equal_mask, slot);
         out->producer.value = value->parent_instr;

         /* Set whether the value is convergent. Such varyings can be
          * promoted to flat regardless of their original interpolation
          * mode.
          */
         if (linkage->consumer_stage == MESA_SHADER_FRAGMENT && !divergent) {
            if (value->bit_size == 32)
               BITSET_SET(linkage->convergent32_mask, slot);
            else if (value->bit_size == 16)
               BITSET_SET(linkage->convergent16_mask, slot);
            else
               unreachable("invalid store_output type");
         }
      } else {
         /* There are multiple stores to the same output. If they store
          * different values, clear the mask.
          */
         if (out->producer.value != value->parent_instr)
            BITSET_CLEAR(linkage->output_equal_mask, slot);

         /* Update divergence information. */
         if (linkage->consumer_stage == MESA_SHADER_FRAGMENT && divergent) {
            if (value->bit_size == 32)
               BITSET_CLEAR(linkage->convergent32_mask, slot);
            else if (value->bit_size == 16)
               BITSET_CLEAR(linkage->convergent16_mask, slot);
            else
               unreachable("invalid store_output type");
         }
      }
   } else {
      /* Only TCS output loads can get here.
       *
       * We need to record output loads as flat32 or flat16, otherwise
       * compaction will think that the slot is free and will put some
       * other output in its place.
       */
      assert(linkage->producer_stage == MESA_SHADER_TESS_CTRL);

      if (!can_optimize_varying(linkage, sem.location).compact)
         return false;

      if (intr->def.bit_size == 32)
         BITSET_SET(linkage->flat32_mask, slot);
      else if (intr->def.bit_size == 16)
         BITSET_SET(linkage->flat16_mask, slot);
      else
         unreachable("invalid load_input type");
   }
   return false;
}

/******************************************************************
 * TIDYING UP INDIRECT VARYINGS (BEFORE DEAD VARYINGS REMOVAL)
 ******************************************************************/

static void
tidy_up_indirect_varyings(struct linkage_info *linkage)
{
   unsigned i;

   /* Indirectly-indexed slots can have direct access too and thus set
    * various bitmasks, so clear those bitmasks to make sure they are not
    * touched.
    */
   BITSET_FOREACH_SET(i, linkage->indirect_mask, NUM_SCALAR_SLOTS) {
      slot_disable_optimizations_and_compaction(linkage, i);
   }

   /* If some slots have both direct and indirect accesses, move instructions
    * of such slots to the slot representing the first array element, so that
    * we can remove all loads/stores of dead indirectly-indexed varyings
    * by only looking at the first element.
    */
   BITSET_FOREACH_SET(i, linkage->indirect_mask, NUM_SCALAR_SLOTS) {
      struct scalar_slot *first = &linkage->slot[i];

      /* Skip if this is not the first array element. The first element
       * always sets num_slots to at least 2.
       */
      if (first->num_slots <= 1)
         continue;

      /* Move instructions from other elements of the indirectly-accessed
       * array to the first element (by merging the linked lists).
       */
      for (unsigned elem = 1; elem < first->num_slots; elem++) {
         /* The component slots are at 16-bit granularity, so we need to
          * increment by 8 to get the same component in the next vec4 slot.
          */
         struct scalar_slot *other = &linkage->slot[i + elem * 8];

         list_splicetail(&other->producer.stores, &first->producer.stores);
         list_splicetail(&other->producer.loads, &first->producer.loads);
         list_splicetail(&other->consumer.loads, &first->consumer.loads);
         list_inithead(&other->producer.stores);
         list_inithead(&other->producer.loads);
         list_inithead(&other->consumer.loads);
      }
   }
}

/******************************************************************
 * TIDYING UP CONVERGENT VARYINGS
 ******************************************************************/

/**
 * Reorganize bitmasks for FS because they are initialized such that they can
 * intersect with the convergent bitmasks. We want them to be disjoint, so
 * that masks of interpolated, flat, and convergent varyings don't intersect.
 */
static void
tidy_up_convergent_varyings(struct linkage_info *linkage)
{
   if (linkage->consumer_stage != MESA_SHADER_FRAGMENT)
      return;

   unsigned i;
   /* Whether to promote convergent interpolated slots to flat if it
    * doesn't lead to worse compaction.
    */
   bool optimize_convergent_slots = true; /* only turn off for debugging */

   if (optimize_convergent_slots) {
      /* If a slot is flat and convergent, keep the flat bit and remove
       * the convergent bit.
       *
       * If a slot is interpolated and convergent, remove the interpolated
       * bit and keep the convergent bit, which means that it's interpolated,
       * but can be promoted to flat.
       *
       * Since the geometry shader is the only shader that can store values
       * in multiple vertices before FS, it's required that all stores are
       * equal to be considered convergent (output_equal_mask), otherwise
       * the promotion to flat would be incorrect.
       */
      BITSET_FOREACH_SET(i, linkage->convergent32_mask, NUM_SCALAR_SLOTS) {
         if (!BITSET_TEST(linkage->interp_fp32_mask, i) &&
             !BITSET_TEST(linkage->flat32_mask, i) &&
             !BITSET_TEST(linkage->color32_mask, i)) {
            /* Compaction disallowed. */
            BITSET_CLEAR(linkage->convergent32_mask, i);
         } else if (BITSET_TEST(linkage->flat32_mask, i) ||
                    (linkage->producer_stage == MESA_SHADER_GEOMETRY &&
                     !BITSET_TEST(linkage->output_equal_mask, i))) {
            /* Keep the original qualifier. */
            BITSET_CLEAR(linkage->convergent32_mask, i);
         } else {
            /* Keep it convergent. */
            BITSET_CLEAR(linkage->interp_fp32_mask, i);
            BITSET_CLEAR(linkage->color32_mask, i);
         }
      }
      BITSET_FOREACH_SET(i, linkage->convergent16_mask, NUM_SCALAR_SLOTS) {
         if (!BITSET_TEST(linkage->interp_fp16_mask, i) &&
             !BITSET_TEST(linkage->flat16_mask, i)) {
            /* Compaction disallowed. */
            BITSET_CLEAR(linkage->convergent16_mask, i);
         } else if (BITSET_TEST(linkage->flat16_mask, i) ||
                    (linkage->producer_stage == MESA_SHADER_GEOMETRY &&
                     !BITSET_TEST(linkage->output_equal_mask, i))) {
            /* Keep the original qualifier. */
            BITSET_CLEAR(linkage->convergent16_mask, i);
         } else {
            /* Keep it convergent. */
            BITSET_CLEAR(linkage->interp_fp16_mask, i);
         }
      }
   } else {
      /* Don't do anything with convergent slots. */
      BITSET_ZERO(linkage->convergent32_mask);
      BITSET_ZERO(linkage->convergent16_mask);
   }
}

/******************************************************************
 * DETERMINING UNIFORM AND UBO MOVABILITY BASED ON DRIVER LIMITS
 ******************************************************************/

static bool
is_variable_present(nir_shader *nir, nir_variable *var,
                    nir_variable_mode mode, bool spirv)
{
   nir_foreach_variable_with_modes(it, nir, mode) {
      if ((spirv && it->data.binding == var->data.binding) ||
          (!spirv && !strcmp(it->name, var->name)))
         return true;
   }
   return false;
}

/* TODO: this should be a helper in common code */
static unsigned
get_uniform_components(const struct glsl_type *type)
{
   unsigned size = glsl_get_aoa_size(type);
   size = MAX2(size, 1);
   size *= glsl_get_matrix_columns(glsl_without_array(type));

   if (glsl_type_is_dual_slot(glsl_without_array(type)))
      size *= 2;

   /* Convert from vec4 to scalar. */
   return size * 4;
}

static unsigned
get_ubo_slots(const nir_variable *var)
{
   if (glsl_type_is_interface(glsl_without_array(var->type))) {
      unsigned slots = glsl_get_aoa_size(var->type);
      return MAX2(slots, 1);
   }

   return 1;
}

/**
 * Count uniforms and see if the combined uniform component count is over
 * the limit. If it is, don't move any uniforms. It's sufficient if drivers
 * declare a very high limit.
 */
static void
determine_uniform_movability(struct linkage_info *linkage,
                             unsigned max_uniform_components)
{
   nir_shader *producer = linkage->producer_builder.shader;
   nir_shader *consumer = linkage->consumer_builder.shader;
   unsigned num_producer_uniforms = 0;
   unsigned num_consumer_uniforms = 0;
   unsigned num_shared_uniforms = 0;

   nir_foreach_variable_with_modes(var, producer, nir_var_uniform) {
      if (is_variable_present(consumer, var, nir_var_uniform, linkage->spirv))
         num_shared_uniforms += get_uniform_components(var->type);
      else
         num_producer_uniforms += get_uniform_components(var->type);
   }

   nir_foreach_variable_with_modes(var, consumer, nir_var_uniform) {
      if (!is_variable_present(producer, var, nir_var_uniform, linkage->spirv))
         num_consumer_uniforms += get_uniform_components(var->type);
   }

   linkage->can_move_uniforms =
      num_producer_uniforms + num_consumer_uniforms + num_shared_uniforms <=
      max_uniform_components;
}

/**
 * Count UBOs and see if the combined UBO count is over the limit. If it is,
 * don't move any UBOs. It's sufficient if drivers declare a very high limit.
 */
static void
determine_ubo_movability(struct linkage_info *linkage,
                         unsigned max_ubos_per_stage)
{
   nir_shader *producer = linkage->producer_builder.shader;
   nir_shader *consumer = linkage->consumer_builder.shader;
   unsigned num_producer_ubos = 0;
   unsigned num_consumer_ubos = 0;
   unsigned num_shared_ubos = 0;

   nir_foreach_variable_with_modes(var, producer, nir_var_mem_ubo) {
      if (is_variable_present(consumer, var, nir_var_mem_ubo, linkage->spirv))
         num_shared_ubos += get_ubo_slots(var);
      else
         num_producer_ubos += get_ubo_slots(var);
   }

   nir_foreach_variable_with_modes(var, consumer, nir_var_mem_ubo) {
      if (!is_variable_present(producer, var, nir_var_mem_ubo,
                               linkage->spirv))
         num_consumer_ubos += get_ubo_slots(var);
   }

   linkage->can_move_ubos =
      num_producer_ubos + num_consumer_ubos + num_shared_ubos <=
      max_ubos_per_stage;
}

/******************************************************************
 * DEAD VARYINGS REMOVAL
 ******************************************************************/

static void
remove_all_stores(struct linkage_info *linkage, unsigned i,
                  bool *uses_xfb, nir_opt_varyings_progress *progress)
{
   struct scalar_slot *slot = &linkage->slot[i];

   assert(!list_is_empty(&slot->producer.stores) &&
          list_is_empty(&slot->producer.loads) &&
          list_is_empty(&slot->consumer.loads));

   /* Remove all stores. */
   list_for_each_entry_safe(struct list_node, iter, &slot->producer.stores, head) {
      if (nir_remove_varying(iter->instr, linkage->consumer_stage)) {
         list_del(&iter->head);
         *progress |= nir_progress_producer;
      } else {
         if (has_xfb(iter->instr)) {
            *uses_xfb = true;

            if (!is_active_sysval_output(linkage, i, iter->instr)) {
               if (iter->instr->src[0].ssa->bit_size == 32)
                  BITSET_SET(linkage->xfb32_only_mask, i);
               else if (iter->instr->src[0].ssa->bit_size == 16)
                  BITSET_SET(linkage->xfb16_only_mask, i);
               else
                  unreachable("invalid load_input type");
            }
         }
      }
   }
}

static void
remove_dead_varyings(struct linkage_info *linkage,
                     nir_opt_varyings_progress *progress)
{
   unsigned i;

   /* Remove dead inputs and outputs. */
   BITSET_FOREACH_SET(i, linkage->removable_mask, NUM_SCALAR_SLOTS) {
      struct scalar_slot *slot = &linkage->slot[i];

      /* Only indirect access can have no loads and stores because we moved
       * them to the first element in tidy_up_indirect_varyings().
       */
      assert(!list_is_empty(&slot->producer.stores) ||
             !list_is_empty(&slot->producer.loads) ||
             !list_is_empty(&slot->consumer.loads) ||
             BITSET_TEST(linkage->indirect_mask, i));

      /* Nothing to do if there are no loads and stores. */
      if (list_is_empty(&slot->producer.stores) &&
          list_is_empty(&slot->producer.loads) &&
          list_is_empty(&slot->consumer.loads))
         continue;

      /* If there are producer loads (e.g. TCS) but no consumer loads
       * (e.g. TES), set the "no_varying" flag to indicate that the outputs
       * are not consumed by the next shader stage (e.g. TES).
       */
      if (!list_is_empty(&slot->producer.stores) &&
          !list_is_empty(&slot->producer.loads) &&
          list_is_empty(&slot->consumer.loads)) {
         for (unsigned list_index = 0; list_index < 2; list_index++) {
            struct list_head *list = list_index ? &slot->producer.stores :
                                                  &slot->producer.loads;

            list_for_each_entry(struct list_node, iter, list, head) {
               nir_io_semantics sem = nir_intrinsic_io_semantics(iter->instr);
               sem.no_varying = 1;
               nir_intrinsic_set_io_semantics(iter->instr, sem);
            }
         }

         /* This tells the compaction to move these varyings to the end. */
         if (BITSET_TEST(linkage->flat32_mask, i)) {
            assert(linkage->consumer_stage != MESA_SHADER_FRAGMENT);
            BITSET_CLEAR(linkage->flat32_mask, i);
            BITSET_SET(linkage->no_varying32_mask, i);
         }
         if (BITSET_TEST(linkage->flat16_mask, i)) {
            assert(linkage->consumer_stage != MESA_SHADER_FRAGMENT);
            BITSET_CLEAR(linkage->flat16_mask, i);
            BITSET_SET(linkage->no_varying16_mask, i);
         }
         continue;
      }

      /* The varyings aren't dead if both loads and stores are present. */
      if (!list_is_empty(&slot->producer.stores) &&
          (!list_is_empty(&slot->producer.loads) ||
           !list_is_empty(&slot->consumer.loads)))
         continue;

      bool uses_xfb = false;

      if (list_is_empty(&slot->producer.stores)) {
         /* There are no stores. */
         assert(!list_is_empty(&slot->producer.loads) ||
                !list_is_empty(&slot->consumer.loads));

         /* TEXn.xy loads can't be removed in FS because of the coord
          * replace state, but TEXn outputs can be removed if they are
          * not read by FS.
          *
          * TEXn.zw loads can be eliminated and replaced by (0, 1), which
          * is equal to the coord replace value.
          */
         if (is_interpolated_texcoord(linkage, i)) {
            assert(i % 2 == 0); /* high 16-bit slots disallowed */
            /* Keep TEXn.xy. */
            if (i % 8 < 4)
               continue;
         }

         /* Replace all loads with undef. Do that for both input loads
          * in the consumer stage and output loads in the producer stage
          * because we also want to eliminate TCS loads that have no
          * corresponding TCS stores.
          */
         for (unsigned list_index = 0; list_index < 2; list_index++) {
            struct list_head *list = list_index ? &slot->producer.loads :
                                                  &slot->consumer.loads;
            nir_builder *b = list_index ? &linkage->producer_builder :
                                          &linkage->consumer_builder;

            list_for_each_entry(struct list_node, iter, list, head) {
               nir_intrinsic_instr *loadi = iter->instr;
               nir_def *replacement = NULL;

               b->cursor = nir_before_instr(&loadi->instr);

               /* LAYER and VIEWPORT FS inputs should be replaced by 0
                * instead of undef.
                */
               gl_varying_slot location = (gl_varying_slot)(vec4_slot(i));

               if (linkage->consumer_stage == MESA_SHADER_FRAGMENT &&
                   (location == VARYING_SLOT_LAYER ||
                    location == VARYING_SLOT_VIEWPORT ||
                    /* TEXn.z is replaced by 0 (matching coord replace) */
                    (is_interpolated_texcoord(linkage, i) && i % 8 == 4)))
                  replacement = nir_imm_intN_t(b, 0, loadi->def.bit_size);
               else if (linkage->consumer_stage == MESA_SHADER_FRAGMENT &&
                        /* TEXn.w is replaced by 1 (matching coord replace) */
                        is_interpolated_texcoord(linkage, i) && i % 8 == 6)
                  replacement = nir_imm_floatN_t(b, 1, loadi->def.bit_size);
               else
                  replacement = nir_undef(b, 1, loadi->def.bit_size);

               nir_def_rewrite_uses(&loadi->def, replacement);
               nir_instr_remove(&loadi->instr);

               *progress |= list_index ? nir_progress_producer :
                                         nir_progress_consumer;
            }
         }

         /* Clear the lists. */
         list_inithead(&slot->producer.loads);
         list_inithead(&slot->consumer.loads);
      } else {
         /* There are no loads. */
         remove_all_stores(linkage, i, &uses_xfb, progress);
      }

      /* Clear bitmasks associated with this varying slot or array. */
      for (unsigned elem = 0; elem < slot->num_slots; elem++)
         clear_slot_info_after_removal(linkage, i + elem, uses_xfb);
   }
}

/******************************************************************
 * SSA CLONING HELPERS
 ******************************************************************/

/* Pass flags for inter-shader code motion. Also used by helpers. */
#define FLAG_ALU_IS_TES_INTERP_LOAD    BITFIELD_BIT(0)
#define FLAG_MOVABLE                   BITFIELD_BIT(1)
#define FLAG_UNMOVABLE                 BITFIELD_BIT(2)
#define FLAG_POST_DOMINATOR_PROCESSED  BITFIELD_BIT(3)
#define FLAG_GATHER_LOADS_VISITED      BITFIELD_BIT(4)

#define FLAG_INTERP_MASK               BITFIELD_RANGE(5, 3)
#define FLAG_INTERP_CONVERGENT         (0 << 5)
#define FLAG_INTERP_FLAT               (1 << 5)
/* FS-only interpolation modes. */
#define FLAG_INTERP_PERSP_PIXEL        (2 << 5)
#define FLAG_INTERP_PERSP_CENTROID     (3 << 5)
#define FLAG_INTERP_PERSP_SAMPLE       (4 << 5)
#define FLAG_INTERP_LINEAR_PIXEL       (5 << 5)
#define FLAG_INTERP_LINEAR_CENTROID    (6 << 5)
#define FLAG_INTERP_LINEAR_SAMPLE      (7 << 5)
/* TES-only interpolation modes. (these were found in shaders) */
#define FLAG_INTERP_TES_TRIANGLE_UVW   (2 << 5) /* v0*u + v1*v + v2*w */
#define FLAG_INTERP_TES_TRIANGLE_WUV   (3 << 5) /* v0*w + v1*u + v2*v */
/* TODO: Feel free to insert more TES interpolation equations here. */

static bool
can_move_deref_between_shaders(struct linkage_info *linkage, nir_instr *instr)
{
   nir_deref_instr *deref = nir_instr_as_deref(instr);
   unsigned allowed_modes =
      (linkage->can_move_uniforms ? nir_var_uniform : 0) |
      (linkage->can_move_ubos ? nir_var_mem_ubo : 0);

   if (!nir_deref_mode_is_one_of(deref, allowed_modes))
      return false;

   /* Indirectly-indexed uniforms and UBOs are not moved into later shaders
    * due to performance concerns, and they are not moved into previous shaders
    * because it's unimplemented (TODO).
    */
   if (nir_deref_instr_has_indirect(deref))
      return false;

   nir_variable *var = nir_deref_instr_get_variable(deref);

   /* Subroutine uniforms are not moved. Even though it works and subroutine
    * uniforms are moved correctly and subroutines have been inlined at this
    * point, subroutine functions aren't moved and the linker doesn't like
    * when a shader only contains a subroutine uniform but no subroutine
    * functions. This could be fixed in the linker, but for now, don't
    * move subroutine uniforms.
    */
   if (var->name && strstr(var->name, "__subu_") == var->name)
      return false;

   return true;
}

static nir_intrinsic_instr *
find_per_vertex_load_for_tes_interp(nir_instr *instr)
{
   switch (instr->type) {
   case nir_instr_type_alu: {
      nir_alu_instr *alu = nir_instr_as_alu(instr);
      unsigned num_srcs = nir_op_infos[alu->op].num_inputs;

      for (unsigned i = 0; i < num_srcs; i++) {
         nir_instr *src = alu->src[i].src.ssa->parent_instr;
         nir_intrinsic_instr *intr = find_per_vertex_load_for_tes_interp(src);

         if (intr)
            return intr;
      }
      return NULL;
   }

   case nir_instr_type_intrinsic: {
      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

      return intr->intrinsic == nir_intrinsic_load_per_vertex_input ?
               intr : NULL;
   }

   default:
      unreachable("unexpected instruction type");
   }
}

static nir_def *
get_stored_value_for_load(struct linkage_info *linkage, nir_instr *instr)
{
   nir_intrinsic_instr *intr;

   if (instr->type == nir_instr_type_intrinsic) {
      intr = nir_instr_as_intrinsic(instr);
   } else {
      assert(instr->type == nir_instr_type_alu &&
             instr->pass_flags & FLAG_ALU_IS_TES_INTERP_LOAD);
      intr = find_per_vertex_load_for_tes_interp(instr);
   }

   unsigned slot_index = intr_get_scalar_16bit_slot(intr);
   assert(list_is_singular(&linkage->slot[slot_index].producer.stores));

   nir_def *stored_value =
      list_first_entry(&linkage->slot[slot_index].producer.stores,
                       struct list_node, head)->instr->src[0].ssa;
   assert(stored_value->num_components == 1);
   return stored_value;
}

/* Clone the SSA, which can be in a different shader. */
static nir_def *
clone_ssa(struct linkage_info *linkage, nir_builder *b, nir_def *ssa)
{
   switch (ssa->parent_instr->type) {
   case nir_instr_type_load_const:
      return nir_build_imm(b, ssa->num_components, ssa->bit_size,
                           nir_instr_as_load_const(ssa->parent_instr)->value);

   case nir_instr_type_undef:
      return nir_undef(b, ssa->num_components, ssa->bit_size);

   case nir_instr_type_alu: {
      nir_alu_instr *alu = nir_instr_as_alu(ssa->parent_instr);

      if (alu->instr.pass_flags & FLAG_ALU_IS_TES_INTERP_LOAD) {
         /* We are cloning an interpolated TES load in the producer for
          * backward inter-shader code motion.
          */
         assert(&linkage->producer_builder == b);
         return get_stored_value_for_load(linkage, &alu->instr);
      }

      nir_def *src[4] = {0};
      unsigned num_srcs = nir_op_infos[alu->op].num_inputs;
      assert(num_srcs <= ARRAY_SIZE(src));

      for (unsigned i = 0; i < num_srcs; i++)
         src[i] = clone_ssa(linkage, b, alu->src[i].src.ssa);

      nir_def *clone = nir_build_alu(b, alu->op, src[0], src[1], src[2], src[3]);
      nir_alu_instr *alu_clone = nir_instr_as_alu(clone->parent_instr);

      alu_clone->exact = alu->exact;
      alu_clone->no_signed_wrap = alu->no_signed_wrap;
      alu_clone->no_unsigned_wrap = alu->no_unsigned_wrap;
      alu_clone->def.num_components = alu->def.num_components;
      alu_clone->def.bit_size = alu->def.bit_size;

      for (unsigned i = 0; i < num_srcs; i++) {
         memcpy(alu_clone->src[i].swizzle, alu->src[i].swizzle,
                NIR_MAX_VEC_COMPONENTS);
      }

      return clone;
   }

   case nir_instr_type_intrinsic: {
      /* Clone load_deref of uniform or ubo. It's the only thing that can
       * occur here.
       */
      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(ssa->parent_instr);

      switch (intr->intrinsic) {
      case nir_intrinsic_load_deref: {
         nir_deref_instr *deref = nir_src_as_deref(intr->src[0]);

         assert(deref);
         assert(nir_deref_mode_is_one_of(deref, nir_var_uniform | nir_var_mem_ubo));
         /* Indirect uniform indexing is disallowed here. */
         assert(!nir_deref_instr_has_indirect(deref));

         /* Get the uniform from the original shader. */
         nir_variable *var = nir_deref_instr_get_variable(deref);
         assert(!(var->data.mode & nir_var_mem_ubo) || linkage->can_move_ubos);

         /* Declare the uniform in the target shader. If it's the same shader
          * (in the case of replacing output loads with a uniform), this has
          * no effect.
          */
         var = nir_clone_uniform_variable(b->shader, var, linkage->spirv);

         /* Re-build the uniform deref load before the load. */
         nir_deref_instr *load_uniform_deref =
            nir_clone_deref_instr(b, var, deref);

         return nir_load_deref(b, load_uniform_deref);
      }

      case nir_intrinsic_load_input:
      case nir_intrinsic_load_interpolated_input: {
         /* We are cloning load_input in the producer for backward
          * inter-shader code motion. Replace the input load with the stored
          * output value. That way we can clone any expression using inputs
          * from the consumer in the producer.
          */
         assert(&linkage->producer_builder == b);
         return get_stored_value_for_load(linkage, &intr->instr);
      }

      default:
         unreachable("unexpected intrinsic");
      }
   }

   default:
      unreachable("unexpected instruction type");
   }
}

/******************************************************************
 * UNIFORM EXPRESSION PROPAGATION (CONSTANTS, UNIFORMS, UBO LOADS)
 ******************************************************************/

static void
remove_all_stores_and_clear_slot(struct linkage_info *linkage, unsigned slot,
                                 nir_opt_varyings_progress *progress)
{
   bool uses_xfb = false;
   remove_all_stores(linkage, slot, &uses_xfb, progress);
   clear_slot_info_after_removal(linkage, slot, uses_xfb);
}

struct is_uniform_expr_state {
   struct linkage_info *linkage;
   unsigned cost;
};

static bool
is_uniform_expression(nir_instr *instr, struct is_uniform_expr_state *state);

static bool
src_is_uniform_expression(nir_src *src, void *data)
{
   return is_uniform_expression(src->ssa->parent_instr,
                                (struct is_uniform_expr_state*)data);
}

/**
 * Return whether instr is a uniform expression that can be moved into
 * the next shader.
 */
static bool
is_uniform_expression(nir_instr *instr, struct is_uniform_expr_state *state)
{
   const nir_shader_compiler_options *options =
      state->linkage->producer_builder.shader->options;

   switch (instr->type) {
   case nir_instr_type_load_const:
   case nir_instr_type_undef:
      return true;

   case nir_instr_type_alu:
      state->cost += options->varying_estimate_instr_cost ?
                        options->varying_estimate_instr_cost(instr) : 1;
      return nir_foreach_src(instr, src_is_uniform_expression, state);

   case nir_instr_type_intrinsic:
      if (nir_instr_as_intrinsic(instr)->intrinsic ==
          nir_intrinsic_load_deref) {
         state->cost += options->varying_estimate_instr_cost ?
                           options->varying_estimate_instr_cost(instr) : 1;
         return nir_foreach_src(instr, src_is_uniform_expression, state);
      }
      return false;

   case nir_instr_type_deref:
      return can_move_deref_between_shaders(state->linkage, instr);

   default:
      return false;
   }
}

/**
 * Propagate constants, uniforms, UBO loads, and uniform expressions
 * in output components to inputs loads in the next shader and output
 * loads in the current stage, and remove the output components.
 *
 * Uniform expressions are ALU expressions only sourcing constants, uniforms,
 * and UBO loads.
 */
static void
propagate_uniform_expressions(struct linkage_info *linkage,
                              nir_opt_varyings_progress *progress)
{
   unsigned i;

   /* Clear pass_flags, which is used by clone_ssa. */
   nir_shader_clear_pass_flags(linkage->consumer_builder.shader);

   /* Find uniform expressions. If there are multiple stores, they should all
    * store the same value. That's guaranteed by output_equal_mask.
    */
   BITSET_FOREACH_SET(i, linkage->output_equal_mask, NUM_SCALAR_SLOTS) {
      if (!can_optimize_varying(linkage, vec4_slot(i)).propagate_uniform_expr)
         continue;

      struct scalar_slot *slot = &linkage->slot[i];
      assert(!list_is_empty(&slot->producer.loads) ||
             !list_is_empty(&slot->consumer.loads));

      struct is_uniform_expr_state state = {
         .linkage = linkage,
         .cost = 0,
      };

      if (!is_uniform_expression(slot->producer.value, &state))
         continue;

      if (state.cost > linkage->max_varying_expression_cost)
         continue;

      /* Colors can be propagated only if they are constant between [0, 1]
       * because that's the only case when the clamp vertex color state has
       * no effect.
       */
      if (is_interpolated_color(linkage, i) &&
          (slot->producer.value->type != nir_instr_type_load_const ||
           nir_instr_as_load_const(slot->producer.value)->value[0].f32 < 0 ||
           nir_instr_as_load_const(slot->producer.value)->value[0].f32 > 1))
         continue;

      /* TEXn.zw can be propagated only if it's equal to (0, 1) because it's
       * the coord replace value.
       */
      if (is_interpolated_texcoord(linkage, i)) {
         assert(i % 2 == 0); /* high 16-bit slots disallowed */

         if (i % 8 == 0 || /* TEXn.x */
             i % 8 == 2 || /* TEXn.y */
             slot->producer.value->type != nir_instr_type_load_const)
            continue;

         float value =
            nir_instr_as_load_const(slot->producer.value)->value[0].f32;

         /* This ignores signed zeros, but those are destroyed by
          * interpolation, so it doesn't matter.
          */
         if ((i % 8 == 4 && value != 0) ||
             (i % 8 == 6 && value != 1))
            continue;
      }

      /* Replace all loads. Do that for both input and output loads. */
      for (unsigned list_index = 0; list_index < 2; list_index++) {
         struct list_head *load = list_index ? &slot->producer.loads :
                                               &slot->consumer.loads;
         nir_builder *b = list_index ? &linkage->producer_builder :
                                       &linkage->consumer_builder;

         list_for_each_entry(struct list_node, node, load, head) {
            nir_intrinsic_instr *loadi = node->instr;
            b->cursor = nir_before_instr(&loadi->instr);

            /* Copy the uniform expression before the load. */
            nir_def *clone = clone_ssa(linkage, b,
                                       nir_instr_def(slot->producer.value));

            /* Interpolation converts Infs to NaNs. If we skip it, we need to
             * convert Infs to NaNs manually.
             */
            if (loadi->intrinsic == nir_intrinsic_load_interpolated_input &&
                preserve_nans(b->shader, clone->bit_size))
               clone = build_convert_inf_to_nan(b, clone);

            /* Replace the original load. */
            nir_def_rewrite_uses(&loadi->def, clone);
            nir_instr_remove(&loadi->instr);
            *progress |= list_index ? nir_progress_producer :
                                      nir_progress_consumer;
         }
      }

      /* Clear the lists. */
      list_inithead(&slot->producer.loads);
      list_inithead(&slot->consumer.loads);

      /* Remove all stores now that loads have been replaced. */
      remove_all_stores_and_clear_slot(linkage, i, progress);
   }
}

/******************************************************************
 * OUTPUT DEDUPLICATION
 ******************************************************************/

/* We can only deduplicate outputs that have the same qualifier, and color
 * components must be deduplicated separately because they are affected by GL
 * states.
 *
 * QUAL_*_INTERP_ANY means that the interpolation qualifier doesn't matter for
 * deduplication as long as it's not flat.
 *
 * QUAL_COLOR_SHADEMODEL_ANY is the same, but can be switched to flat
 * by the flatshade state, so it can't be deduplicated with
 * QUAL_COLOR_INTERP_ANY, which is never flat.
 */
enum var_qualifier {
   QUAL_PATCH,
   QUAL_VAR_FLAT,
   QUAL_COLOR_FLAT,
   QUAL_EXPLICIT,
   QUAL_EXPLICIT_STRICT,
   QUAL_PER_PRIMITIVE,
   /* When nir_io_has_flexible_input_interpolation_except_flat is set: */
   QUAL_VAR_INTERP_ANY,
   QUAL_COLOR_INTERP_ANY,
   QUAL_COLOR_SHADEMODEL_ANY,
   /* When nir_io_has_flexible_input_interpolation_except_flat is unset: */
   QUAL_VAR_PERSP_PIXEL,
   QUAL_VAR_PERSP_CENTROID,
   QUAL_VAR_PERSP_SAMPLE,
   QUAL_VAR_LINEAR_PIXEL,
   QUAL_VAR_LINEAR_CENTROID,
   QUAL_VAR_LINEAR_SAMPLE,
   QUAL_COLOR_PERSP_PIXEL,
   QUAL_COLOR_PERSP_CENTROID,
   QUAL_COLOR_PERSP_SAMPLE,
   QUAL_COLOR_LINEAR_PIXEL,
   QUAL_COLOR_LINEAR_CENTROID,
   QUAL_COLOR_LINEAR_SAMPLE,
   QUAL_COLOR_SHADEMODEL_PIXEL,
   QUAL_COLOR_SHADEMODEL_CENTROID,
   QUAL_COLOR_SHADEMODEL_SAMPLE,
   NUM_DEDUP_QUALIFIERS,

   QUAL_SKIP,
   QUAL_UNKNOWN,
};

/* Return the input qualifier if all loads use the same one, else skip.
 * This is only used by output deduplication to determine input compatibility.
 */
static enum var_qualifier
get_input_qualifier(struct linkage_info *linkage, unsigned i)
{
   assert(linkage->consumer_stage == MESA_SHADER_FRAGMENT);
   struct scalar_slot *slot = &linkage->slot[i];
   bool is_color = is_interpolated_color(linkage, i);
   nir_intrinsic_instr *load =
      list_first_entry(&slot->consumer.loads, struct list_node, head)->instr;

   if (load->intrinsic == nir_intrinsic_load_input) {
      if (nir_intrinsic_io_semantics(load).per_primitive)
         return QUAL_PER_PRIMITIVE;
      return is_color ? QUAL_COLOR_FLAT : QUAL_VAR_FLAT;
   }

   if (load->intrinsic == nir_intrinsic_load_input_vertex) {
      return nir_intrinsic_io_semantics(load).interp_explicit_strict ?
               QUAL_EXPLICIT_STRICT : QUAL_EXPLICIT;
   }

   assert(load->intrinsic == nir_intrinsic_load_interpolated_input);
   nir_intrinsic_instr *baryc =
      nir_instr_as_intrinsic(load->src[0].ssa->parent_instr);

   if (linkage->consumer_builder.shader->options->io_options &
       nir_io_has_flexible_input_interpolation_except_flat) {
      if (is_color) {
         return nir_intrinsic_interp_mode(baryc) == INTERP_MODE_NONE ?
                   QUAL_COLOR_SHADEMODEL_ANY : QUAL_COLOR_INTERP_ANY;
      } else {
         return QUAL_VAR_INTERP_ANY;
      }
   }

   /* Get the exact interpolation qualifier. */
   unsigned pixel_location;
   enum var_qualifier qual;

   switch (baryc->intrinsic) {
   case nir_intrinsic_load_barycentric_pixel:
      pixel_location = 0;
      break;
   case nir_intrinsic_load_barycentric_centroid:
      pixel_location = 1;
      break;
   case nir_intrinsic_load_barycentric_sample:
      pixel_location = 2;
      break;
   case nir_intrinsic_load_barycentric_at_offset:
   case nir_intrinsic_load_barycentric_at_sample:
      /* Don't deduplicate outputs that are interpolated at offset/sample. */
      return QUAL_SKIP;
   default:
      unreachable("unexpected barycentric src");
   }

   switch (nir_intrinsic_interp_mode(baryc)) {
   case INTERP_MODE_NONE:
      qual = is_color ? QUAL_COLOR_SHADEMODEL_PIXEL :
                        QUAL_VAR_PERSP_PIXEL;
      break;
   case INTERP_MODE_SMOOTH:
      qual = is_color ? QUAL_COLOR_PERSP_PIXEL : QUAL_VAR_PERSP_PIXEL;
      break;
   case INTERP_MODE_NOPERSPECTIVE:
      qual = is_color ? QUAL_COLOR_LINEAR_PIXEL : QUAL_VAR_LINEAR_PIXEL;
      break;
   default:
      unreachable("unexpected interp mode");
   }

   /* The ordering of the "qual" enum was carefully chosen to make this
    * addition correct.
    */
   STATIC_ASSERT(QUAL_VAR_PERSP_PIXEL + 1 == QUAL_VAR_PERSP_CENTROID);
   STATIC_ASSERT(QUAL_VAR_PERSP_PIXEL + 2 == QUAL_VAR_PERSP_SAMPLE);
   STATIC_ASSERT(QUAL_VAR_LINEAR_PIXEL + 1 == QUAL_VAR_LINEAR_CENTROID);
   STATIC_ASSERT(QUAL_VAR_LINEAR_PIXEL + 2 == QUAL_VAR_LINEAR_SAMPLE);
   STATIC_ASSERT(QUAL_COLOR_PERSP_PIXEL + 1 == QUAL_COLOR_PERSP_CENTROID);
   STATIC_ASSERT(QUAL_COLOR_PERSP_PIXEL + 2 == QUAL_COLOR_PERSP_SAMPLE);
   STATIC_ASSERT(QUAL_COLOR_LINEAR_PIXEL + 1 == QUAL_COLOR_LINEAR_CENTROID);
   STATIC_ASSERT(QUAL_COLOR_LINEAR_PIXEL + 2 == QUAL_COLOR_LINEAR_SAMPLE);
   STATIC_ASSERT(QUAL_COLOR_SHADEMODEL_PIXEL + 1 ==
                 QUAL_COLOR_SHADEMODEL_CENTROID);
   STATIC_ASSERT(QUAL_COLOR_SHADEMODEL_PIXEL + 2 ==
                 QUAL_COLOR_SHADEMODEL_SAMPLE);
   return qual + pixel_location;
}

static void
deduplicate_outputs(struct linkage_info *linkage,
                    nir_opt_varyings_progress *progress)
{
   struct hash_table *tables[NUM_DEDUP_QUALIFIERS] = {NULL};
   unsigned i;

   /* Find duplicated outputs. If there are multiple stores, they should all
    * store the same value as all stores of some other output. That's
    * guaranteed by output_equal_mask.
    */
   BITSET_FOREACH_SET(i, linkage->output_equal_mask, NUM_SCALAR_SLOTS) {
      if (!can_optimize_varying(linkage, vec4_slot(i)).deduplicate)
         continue;

      struct scalar_slot *slot = &linkage->slot[i];
      enum var_qualifier qualifier;
      gl_varying_slot var_slot = vec4_slot(i);

      /* Determine which qualifier this slot has. */
      if ((var_slot >= VARYING_SLOT_PATCH0 &&
           var_slot <= VARYING_SLOT_PATCH31) ||
          var_slot == VARYING_SLOT_TESS_LEVEL_INNER ||
          var_slot == VARYING_SLOT_TESS_LEVEL_OUTER)
         qualifier = QUAL_PATCH;
      else if (linkage->consumer_stage != MESA_SHADER_FRAGMENT)
         qualifier = QUAL_VAR_FLAT;
      else
         qualifier = get_input_qualifier(linkage, i);

      if (qualifier == QUAL_SKIP)
         continue;

      struct hash_table **table = &tables[qualifier];
      if (!*table)
         *table = _mesa_pointer_hash_table_create(NULL);

      nir_instr *value = slot->producer.value;

      struct hash_entry *entry = _mesa_hash_table_search(*table, value);
      if (!entry) {
         _mesa_hash_table_insert(*table, value, (void*)(uintptr_t)i);
         continue;
      }

      /* We've found a duplicate. Redirect loads and remove stores. */
      struct scalar_slot *found_slot = &linkage->slot[(uintptr_t)entry->data];
      nir_intrinsic_instr *store =
         list_first_entry(&found_slot->producer.stores,
                          struct list_node, head)->instr;
      nir_io_semantics sem = nir_intrinsic_io_semantics(store);
      unsigned component = nir_intrinsic_component(store);

      /* Redirect loads. */
      for (unsigned list_index = 0; list_index < 2; list_index++) {
         struct list_head *src_loads = list_index ? &slot->producer.loads :
                                                    &slot->consumer.loads;
         struct list_head *dst_loads = list_index ? &found_slot->producer.loads :
                                                    &found_slot->consumer.loads;
         bool has_progress = !list_is_empty(src_loads);

         list_for_each_entry(struct list_node, iter, src_loads, head) {
            nir_intrinsic_instr *loadi = iter->instr;

            nir_intrinsic_set_io_semantics(loadi, sem);
            nir_intrinsic_set_component(loadi, component);

            /* We also need to set the base to match the duplicate load, so
             * that CSE can eliminate it.
             */
            if (!list_is_empty(dst_loads)) {
               struct list_node *first =
                  list_first_entry(dst_loads, struct list_node, head);
               nir_intrinsic_set_base(loadi, nir_intrinsic_base(first->instr));
            } else {
               /* Use the base of the found store if there are no loads (it can
                * only happen with TCS).
                */
               assert(list_index == 0);
               nir_intrinsic_set_base(loadi, nir_intrinsic_base(store));
            }
         }

         if (has_progress) {
            /* Move the redirected loads to the found slot, so that compaction
             * can find them.
             */
            list_splicetail(src_loads, dst_loads);
            list_inithead(src_loads);

            *progress |= list_index ? nir_progress_producer :
                                      nir_progress_consumer;
         }
      }

      /* Remove all duplicated stores now that loads have been redirected. */
      remove_all_stores_and_clear_slot(linkage, i, progress);
   }

   for (unsigned i = 0; i < ARRAY_SIZE(tables); i++)
      _mesa_hash_table_destroy(tables[i], NULL);
}

/******************************************************************
 * FIND OPEN-CODED TES INPUT INTERPOLATION
 ******************************************************************/

static bool
is_sysval(nir_instr *instr, gl_system_value sysval)
{
   if (instr->type == nir_instr_type_intrinsic) {
      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

      if (intr->intrinsic == nir_intrinsic_from_system_value(sysval))
         return true;

      if (intr->intrinsic == nir_intrinsic_load_deref) {
          nir_deref_instr *deref =
            nir_instr_as_deref(intr->src[0].ssa->parent_instr);

          return nir_deref_mode_is_one_of(deref, nir_var_system_value) &&
                 deref->var->data.location == sysval;
      }
   }

   return false;
}

static nir_alu_instr *
get_single_use_as_alu(nir_def *def)
{
   /* Only 1 use allowed. */
   if (!list_is_singular(&def->uses))
      return NULL;

   nir_instr *instr =
      nir_src_parent_instr(list_first_entry(&def->uses, nir_src, use_link));
   if (instr->type != nir_instr_type_alu)
      return NULL;

   return nir_instr_as_alu(instr);
}

static nir_alu_instr *
check_tes_input_load_get_single_use_alu(nir_intrinsic_instr *load,
                                        unsigned *vertex_index,
                                        unsigned *vertices_used,
                                        unsigned max_vertices)
{
   if (load->intrinsic != nir_intrinsic_load_per_vertex_input)
      return NULL;

   /* Check the vertex index. Each vertex can be loaded only once. */
   if (!nir_src_is_const(load->src[0]))
      return false;

   *vertex_index = nir_src_as_uint(load->src[0]);
   if (*vertex_index >= max_vertices ||
       *vertices_used & BITFIELD_BIT(*vertex_index))
      return false;

   *vertices_used |= BITFIELD_BIT(*vertex_index);

   return get_single_use_as_alu(&load->def);
}

static bool
gather_fmul_tess_coord(nir_intrinsic_instr *load, nir_alu_instr *fmul,
                       unsigned vertex_index, unsigned *tess_coord_swizzle,
                       unsigned *tess_coord_used, nir_def **load_tess_coord)
{
   unsigned other_src = fmul->src[0].src.ssa == &load->def;
   nir_instr *other_instr = fmul->src[other_src].src.ssa->parent_instr;

   assert(fmul->src[!other_src].swizzle[0] == 0);

   if (!is_sysval(other_instr, SYSTEM_VALUE_TESS_COORD))
      return false;

   unsigned tess_coord_component = fmul->src[other_src].swizzle[0];
   /* Each tesscoord component can be used only once. */
   if (*tess_coord_used & BITFIELD_BIT(tess_coord_component))
      return false;

   *tess_coord_swizzle |= tess_coord_component << (4 * vertex_index);
   *tess_coord_used |= BITFIELD_BIT(tess_coord_component);
   *load_tess_coord = &nir_instr_as_intrinsic(other_instr)->def;
   return true;
}

/**
 * Find interpolation of the form:
 *    input[0].slot * TessCoord.a +
 *    input[1].slot * TessCoord.b +
 *    input[2].slot * TessCoord.c;
 *
 * a,b,c can be any of x,y,z, but each can occur only once.
 */
static bool
find_tes_triangle_interp_3fmul_2fadd(struct linkage_info *linkage, unsigned i)
{
   struct scalar_slot *slot = &linkage->slot[i];
   unsigned vertices_used = 0;
   unsigned tess_coord_used = 0;
   unsigned tess_coord_swizzle = 0;
   unsigned num_fmuls = 0, num_fadds = 0;
   nir_alu_instr *fadds[2];
   nir_def *load_tess_coord = NULL;

   /* Find 3 multiplications by TessCoord and their uses, which must be
    * fadds.
    */
   list_for_each_entry(struct list_node, iter, &slot->consumer.loads, head) {
      unsigned vertex_index;
      nir_alu_instr *fmul =
         check_tes_input_load_get_single_use_alu(iter->instr, &vertex_index,
                                                 &vertices_used, 3);
      /* Only maximum of 3 loads expected. Also reject exact ops because we
       * are going to do an inexact transformation with it.
       */
      if (!fmul || fmul->op != nir_op_fmul || fmul->exact || num_fmuls == 3 ||
          !gather_fmul_tess_coord(iter->instr, fmul, vertex_index,
                                  &tess_coord_swizzle, &tess_coord_used,
                                  &load_tess_coord))
         return false;

      num_fmuls++;

      /* The multiplication must only be used by fadd. Also reject exact ops.
       */
      nir_alu_instr *fadd = get_single_use_as_alu(&fmul->def);
      if (!fadd || fadd->op != nir_op_fadd || fadd->exact)
         return false;

      /* The 3 fmuls must only be used by 2 fadds. */
      unsigned i;
      for (i = 0; i < num_fadds; i++) {
         if (fadds[i] == fadd)
            break;
      }
      if (i == num_fadds) {
         if (num_fadds == 2)
            return false;

         fadds[num_fadds++] = fadd;
      }
   }

   if (num_fmuls != 3 || num_fadds != 2)
      return false;

   assert(tess_coord_used == 0x7);

   /* We have found that the only uses of the 3 fmuls are 2 fadds, which
    * implies that at least 2 fmuls are used by the same fadd.
    *
    * Check that 1 fadd is used by the other fadd, which can only be
    * the result of the TessCoord interpolation.
    */
   for (unsigned i = 0; i < 2; i++) {
      if (get_single_use_as_alu(&fadds[i]->def) == fadds[!i]) {
         switch (tess_coord_swizzle) {
         case 0x210:
            slot->consumer.tes_interp_load = fadds[!i];
            slot->consumer.tes_interp_mode = FLAG_INTERP_TES_TRIANGLE_UVW;
            slot->consumer.tes_load_tess_coord = load_tess_coord;
            return true;

         case 0x102:
            slot->consumer.tes_interp_load = fadds[!i];
            slot->consumer.tes_interp_mode = FLAG_INTERP_TES_TRIANGLE_WUV;
            slot->consumer.tes_load_tess_coord = load_tess_coord;
            return true;

         default:
            return false;
         }
      }
   }

   return false;
}

/**
 * Find interpolation of the form:
 *    fma(input[0].slot, TessCoord.a,
 *        fma(input[1].slot, TessCoord.b,
 *            input[2].slot * TessCoord.c))
 *
 * a,b,c can be any of x,y,z, but each can occur only once.
 */
static bool
find_tes_triangle_interp_1fmul_2ffma(struct linkage_info *linkage, unsigned i)
{
   struct scalar_slot *slot = &linkage->slot[i];
   unsigned vertices_used = 0;
   unsigned tess_coord_used = 0;
   unsigned tess_coord_swizzle = 0;
   unsigned num_fmuls = 0, num_ffmas = 0;
   nir_alu_instr *ffmas[2], *fmul = NULL;
   nir_def *load_tess_coord = NULL;

   list_for_each_entry(struct list_node, iter, &slot->consumer.loads, head) {
      unsigned vertex_index;
      nir_alu_instr *alu =
         check_tes_input_load_get_single_use_alu(iter->instr, &vertex_index,
                                                 &vertices_used, 3);

      /* Reject exact ops because we are going to do an inexact transformation
       * with it.
       */
      if (!alu || (alu->op != nir_op_fmul && alu->op != nir_op_ffma) ||
          alu->exact ||
          !gather_fmul_tess_coord(iter->instr, alu, vertex_index,
                                  &tess_coord_swizzle, &tess_coord_used,
                                  &load_tess_coord))
         return false;

      /* The multiplication must only be used by ffma. */
      if (alu->op == nir_op_fmul) {
         nir_alu_instr *ffma = get_single_use_as_alu(&alu->def);
         if (!ffma || ffma->op != nir_op_ffma)
            return false;

         if (num_fmuls == 1)
            return false;

         fmul = alu;
         num_fmuls++;
      } else {
         if (num_ffmas == 2)
            return false;

         ffmas[num_ffmas++] = alu;
      }
   }

   if (num_fmuls != 1 || num_ffmas != 2)
      return false;

   assert(tess_coord_used == 0x7);

   /* We have found that fmul has only 1 use and it's ffma, and there are 2
    * ffmas. Fail if neither ffma is using fmul.
    */
   if (ffmas[0]->src[2].src.ssa != &fmul->def &&
       ffmas[1]->src[2].src.ssa != &fmul->def)
      return false;

   /* If one ffma is using the other ffma, it's guaranteed to be src[2]. */
   for (unsigned i = 0; i < 2; i++) {
      if (get_single_use_as_alu(&ffmas[i]->def) == ffmas[!i]) {
         switch (tess_coord_swizzle) {
         case 0x210:
            slot->consumer.tes_interp_load = ffmas[!i];
            slot->consumer.tes_interp_mode = FLAG_INTERP_TES_TRIANGLE_UVW;
            slot->consumer.tes_load_tess_coord = load_tess_coord;
            return true;

         case 0x102:
            slot->consumer.tes_interp_load = ffmas[!i];
            slot->consumer.tes_interp_mode = FLAG_INTERP_TES_TRIANGLE_WUV;
            slot->consumer.tes_load_tess_coord = load_tess_coord;
            return true;

         default:
            return false;
         }
      }
   }

   return false;
}

static void
find_open_coded_tes_input_interpolation(struct linkage_info *linkage)
{
   if (linkage->consumer_stage != MESA_SHADER_TESS_EVAL)
      return;

   unsigned i;
   BITSET_FOREACH_SET(i, linkage->flat32_mask, NUM_SCALAR_SLOTS) {
      if (vec4_slot(i) >= VARYING_SLOT_PATCH0 &&
          vec4_slot(i) <= VARYING_SLOT_PATCH31)
         continue;
      if (find_tes_triangle_interp_3fmul_2fadd(linkage, i))
         continue;
      if (find_tes_triangle_interp_1fmul_2ffma(linkage, i))
         continue;
   }

   BITSET_FOREACH_SET(i, linkage->flat16_mask, NUM_SCALAR_SLOTS) {
      if (vec4_slot(i) >= VARYING_SLOT_PATCH0 &&
          vec4_slot(i) <= VARYING_SLOT_PATCH31)
         continue;
      if (find_tes_triangle_interp_3fmul_2fadd(linkage, i))
         continue;
      if (find_tes_triangle_interp_1fmul_2ffma(linkage, i))
         continue;
   }
}

/******************************************************************
 * BACKWARD INTER-SHADER CODE MOTION
 ******************************************************************/

#define NEED_UPDATE_MOVABLE_FLAGS(instr) \
   (!((instr)->pass_flags & (FLAG_MOVABLE | FLAG_UNMOVABLE)))

#define GET_SRC_INTERP(alu, i) \
   ((alu)->src[i].src.ssa->parent_instr->pass_flags & FLAG_INTERP_MASK)

static bool
can_move_alu_across_interp(struct linkage_info *linkage, nir_alu_instr *alu)
{
   /* Exact ALUs can't be moved across interpolation. */
   if (alu->exact)
      return false;

   /* Interpolation converts Infs to NaNs. If we turn a result of an ALU
    * instruction into a new interpolated input, it converts Infs to NaNs for
    * that instruction, while removing the Infs to NaNs conversion for sourced
    * interpolated values. We can't do that if Infs and NaNs must be preserved.
    */
   if (preserve_infs_nans(linkage->consumer_builder.shader, alu->def.bit_size))
      return false;

   switch (alu->op) {
   /* Always legal if the sources are interpolated identically because:
    *    interp(x, i, j) + interp(y, i, j) = interp(x + y, i, j)
    *    interp(x, i, j) + convergent_expr = interp(x + convergent_expr, i, j)
    */
   case nir_op_fadd:
   case nir_op_fsub:
   /* This is the same as multiplying by -1, which is always legal, see fmul.
    */
   case nir_op_fneg:
   case nir_op_mov:
      return true;

   /* At least one side of the multiplication must be convergent because this
    * is the only equation with multiplication that is true:
    *    interp(x, i, j) * convergent_expr = interp(x * convergent_expr, i, j)
    */
   case nir_op_fmul:
   case nir_op_fmulz:
   case nir_op_ffma:
   case nir_op_ffmaz:
      return GET_SRC_INTERP(alu, 0) == FLAG_INTERP_CONVERGENT ||
             GET_SRC_INTERP(alu, 1) == FLAG_INTERP_CONVERGENT;

   case nir_op_fdiv:
      /* The right side must be convergent, which then follows the fmul rule.
       */
      return GET_SRC_INTERP(alu, 1) == FLAG_INTERP_CONVERGENT;

   case nir_op_flrp:
      /* Using the same rule as fmul. */
      return (GET_SRC_INTERP(alu, 0) == FLAG_INTERP_CONVERGENT &&
              GET_SRC_INTERP(alu, 1) == FLAG_INTERP_CONVERGENT) ||
             GET_SRC_INTERP(alu, 2) == FLAG_INTERP_CONVERGENT;

   default:
      /* Moving other ALU instructions across interpolation is illegal. */
      return false;
   }
}

/* Determine whether an instruction is movable from the consumer to
 * the producer. Also determine which interpolation modes each ALU instruction
 * should use if its value was promoted to a new input.
 */
static void
update_movable_flags(struct linkage_info *linkage, nir_instr *instr)
{
   /* This function shouldn't be called more than once for each instruction
    * to minimize recursive calling.
    */
   assert(NEED_UPDATE_MOVABLE_FLAGS(instr));

   switch (instr->type) {
   case nir_instr_type_undef:
   case nir_instr_type_load_const:
      /* Treat constants as convergent, which means compatible with both flat
       * and non-flat inputs.
       */
      instr->pass_flags |= FLAG_MOVABLE | FLAG_INTERP_CONVERGENT;
      return;

   case nir_instr_type_alu: {
      nir_alu_instr *alu = nir_instr_as_alu(instr);
      unsigned num_srcs = nir_op_infos[alu->op].num_inputs;
      unsigned alu_interp;

      /* These are shader-dependent and thus unmovable. */
      if (nir_op_is_derivative(alu->op)) {
         instr->pass_flags |= FLAG_UNMOVABLE;
         return;
      }

      /* Make vector ops unmovable. They are technically movable but more
       * complicated, and NIR should be scalarized for this pass anyway.
       * The only remaining vector ops should be vecN for intrinsic sources.
       */
      if (alu->def.num_components > 1) {
         instr->pass_flags |= FLAG_UNMOVABLE;
         return;
      }

      alu_interp = FLAG_INTERP_CONVERGENT;

      for (unsigned i = 0; i < num_srcs; i++) {
         nir_instr *src_instr = alu->src[i].src.ssa->parent_instr;

         if (NEED_UPDATE_MOVABLE_FLAGS(src_instr))
            update_movable_flags(linkage, src_instr);

         if (src_instr->pass_flags & FLAG_UNMOVABLE) {
            instr->pass_flags |= FLAG_UNMOVABLE;
            return;
         }

         /* Determine which interpolation mode this ALU instruction should
          * use if it was promoted to a new input.
          */
         unsigned src_interp = src_instr->pass_flags & FLAG_INTERP_MASK;

         if (alu_interp == src_interp ||
             src_interp == FLAG_INTERP_CONVERGENT) {
            /* Nothing to do. */
         } else if (alu_interp == FLAG_INTERP_CONVERGENT) {
            alu_interp = src_interp;
         } else {
            assert(alu_interp != FLAG_INTERP_CONVERGENT &&
                   src_interp != FLAG_INTERP_CONVERGENT &&
                   alu_interp != src_interp);
            /* The ALU instruction sources conflicting interpolation flags.
             * It can never become a new input.
             */
            instr->pass_flags |= FLAG_UNMOVABLE;
            return;
         }
      }

      /* Check if we can move the ALU instruction across an interpolated
       * load into the previous shader.
       */
      if (alu_interp > FLAG_INTERP_FLAT &&
          !can_move_alu_across_interp(linkage, alu)) {
         instr->pass_flags |= FLAG_UNMOVABLE;
         return;
      }

      instr->pass_flags |= FLAG_MOVABLE | alu_interp;
      return;
   }

   case nir_instr_type_intrinsic: {
      /* Movable input loads already have FLAG_MOVABLE on them.
       * Unmovable input loads skipped by initialization get UNMOVABLE here.
       * (e.g. colors, texcoords)
       *
       * The only other movable intrinsic is load_deref for uniforms and UBOs.
       * Other intrinsics are not movable.
       */
      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

      if (intr->intrinsic == nir_intrinsic_load_deref) {
         nir_instr *deref = intr->src[0].ssa->parent_instr;

         if (NEED_UPDATE_MOVABLE_FLAGS(deref))
            update_movable_flags(linkage, deref);

         if (deref->pass_flags & FLAG_MOVABLE) {
            /* Treat uniforms as convergent, which means compatible with both
             * flat and non-flat inputs.
             */
            instr->pass_flags |= FLAG_MOVABLE | FLAG_INTERP_CONVERGENT;
            return;
         }
      }

      instr->pass_flags |= FLAG_UNMOVABLE;
      return;
   }

   case nir_instr_type_deref:
      if (can_move_deref_between_shaders(linkage, instr))
         instr->pass_flags |= FLAG_MOVABLE;
      else
         instr->pass_flags |= FLAG_UNMOVABLE;
      return;

   default:
      instr->pass_flags |= FLAG_UNMOVABLE;
      return;
   }
}

/* Gather the input loads used by the post-dominator using DFS. */
static void
gather_used_input_loads(nir_instr *instr,
                        nir_intrinsic_instr *loads[NUM_SCALAR_SLOTS],
                        unsigned *num_loads)
{
   switch (instr->type) {
   case nir_instr_type_undef:
   case nir_instr_type_load_const:
      return;

   case nir_instr_type_alu: {
      nir_alu_instr *alu = nir_instr_as_alu(instr);
      unsigned num_srcs = nir_op_infos[alu->op].num_inputs;

      for (unsigned i = 0; i < num_srcs; i++) {
         gather_used_input_loads(alu->src[i].src.ssa->parent_instr,
                                 loads, num_loads);
      }
      return;
   }

   case nir_instr_type_intrinsic: {
      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

      switch (intr->intrinsic) {
      case nir_intrinsic_load_deref:
      case nir_intrinsic_load_tess_coord:
         return;

      case nir_intrinsic_load_input:
      case nir_intrinsic_load_per_vertex_input:
      case nir_intrinsic_load_interpolated_input:
         if (!(intr->instr.pass_flags & FLAG_GATHER_LOADS_VISITED)) {
            assert(*num_loads < NUM_SCALAR_SLOTS*8);
            loads[(*num_loads)++] = intr;
            intr->instr.pass_flags |= FLAG_GATHER_LOADS_VISITED;
         }
         return;

      default:
         printf("%u\n", intr->intrinsic);
         unreachable("unexpected intrinsic");
      }
   }

   default:
      unreachable("unexpected instr type");
   }
}

/* Move a post-dominator, which is an ALU opcode, into the previous shader,
 * and replace the post-dominator with a new input load.
 */
static bool
try_move_postdominator(struct linkage_info *linkage,
                       struct nir_use_dominance_state *postdom_state,
                       nir_alu_instr *postdom,
                       nir_def *load_def,
                       nir_intrinsic_instr *first_load,
                       nir_opt_varyings_progress *progress)
{
#define PRINT 0
#if PRINT
   printf("Trying to move post-dom: ");
   nir_print_instr(&postdom->instr, stdout);
   puts("");
#endif

   /* Gather the input loads used by the post-dominator using DFS. */
   nir_intrinsic_instr *loads[NUM_SCALAR_SLOTS*8];
   unsigned num_loads = 0;
   gather_used_input_loads(&postdom->instr, loads, &num_loads);

   /* Clear the flag set by gather_used_input_loads. */
   for (unsigned i = 0; i < num_loads; i++)
      loads[i]->instr.pass_flags &= ~FLAG_GATHER_LOADS_VISITED;

   /* For all the loads, the previous shader must have the corresponding
    * output stores in the same basic block because we are going to replace
    * them with 1 store. Only TCS and GS can have stores of different outputs
    * in different blocks.
    */
   nir_block *block = NULL;

   for (unsigned i = 0; i < num_loads; i++) {
      unsigned slot_index = intr_get_scalar_16bit_slot(loads[i]);
      struct scalar_slot *slot = &linkage->slot[slot_index];

      assert(list_is_singular(&slot->producer.stores));
      nir_intrinsic_instr *store =
         list_first_entry(&slot->producer.stores, struct list_node,
                          head)->instr;

      if (!block) {
         block = store->instr.block;
         continue;
      }
      if (block != store->instr.block)
         return false;
   }

   assert(block);

#if PRINT
   printf("Post-dom accepted: ");
   nir_print_instr(&postdom->instr, stdout);
   puts("\n");
#endif

   /* Determine the scalar slot index of the new varying. It will reuse
    * the slot of the load we started from because the load will be
    * removed.
    */
   unsigned final_slot = intr_get_scalar_16bit_slot(first_load);

   /* Replace the post-dominator in the consumer with a new input load.
    * Since we are reusing the same slot as the first load and it has
    * the right interpolation qualifiers, use it as the new load by using
    * it in place of the post-dominator.
    *
    * Boolean post-dominators are upcast in the producer and then downcast
    * in the consumer.
    */
   unsigned slot_index = final_slot;
   struct scalar_slot *slot = &linkage->slot[slot_index];
   nir_builder *b = &linkage->consumer_builder;
   b->cursor = nir_after_instr(load_def->parent_instr);
   unsigned alu_interp = postdom->instr.pass_flags & FLAG_INTERP_MASK;
   nir_def *new_input, *new_tes_loads[3];
   BITSET_WORD *mask;

   /* NIR can't do 1-bit inputs. Convert them to a bigger size. */
   assert(postdom->def.bit_size & (1 | 16 | 32));
   unsigned new_bit_size = postdom->def.bit_size;

   if (new_bit_size == 1) {
      assert(alu_interp == FLAG_INTERP_CONVERGENT ||
             alu_interp == FLAG_INTERP_FLAT);
      /* TODO: We could use 16 bits instead, but that currently fails on AMD.
       */
      new_bit_size = 32;
   }

   /* Create the new input load. This creates a new load (or a series of
    * loads in case of open-coded TES interpolation) that's identical to
    * the original load(s).
    */
   if (linkage->consumer_stage == MESA_SHADER_FRAGMENT &&
       alu_interp > FLAG_INTERP_FLAT) {
      nir_def *baryc = NULL;

      /* Determine the barycentric coordinates. */
      switch (alu_interp) {
      case FLAG_INTERP_PERSP_PIXEL:
      case FLAG_INTERP_LINEAR_PIXEL:
         baryc = nir_load_barycentric_pixel(b, 32);
         break;
      case FLAG_INTERP_PERSP_CENTROID:
      case FLAG_INTERP_LINEAR_CENTROID:
         baryc = nir_load_barycentric_centroid(b, 32);
         break;
      case FLAG_INTERP_PERSP_SAMPLE:
      case FLAG_INTERP_LINEAR_SAMPLE:
         baryc = nir_load_barycentric_sample(b, 32);
         break;
      }

      nir_intrinsic_instr *baryc_i =
         nir_instr_as_intrinsic(baryc->parent_instr);

      if (alu_interp == FLAG_INTERP_LINEAR_PIXEL ||
          alu_interp == FLAG_INTERP_LINEAR_CENTROID ||
          alu_interp == FLAG_INTERP_LINEAR_SAMPLE)
         nir_intrinsic_set_interp_mode(baryc_i, INTERP_MODE_NOPERSPECTIVE);
      else
         nir_intrinsic_set_interp_mode(baryc_i, INTERP_MODE_SMOOTH);

      new_input = nir_load_interpolated_input(
                     b, 1, new_bit_size, baryc, nir_imm_int(b, 0),
                     .base = nir_intrinsic_base(first_load),
                     .component = nir_intrinsic_component(first_load),
                     .dest_type = nir_alu_type_get_base_type(nir_intrinsic_dest_type(first_load)) |
                                  new_bit_size,
                     .io_semantics = nir_intrinsic_io_semantics(first_load));

      mask = new_bit_size == 16 ? linkage->interp_fp16_mask
                                : linkage->interp_fp32_mask;
   } else if (linkage->consumer_stage == MESA_SHADER_TESS_EVAL &&
              alu_interp > FLAG_INTERP_FLAT) {
      nir_def *zero = nir_imm_int(b, 0);

      for (unsigned i = 0; i < 3; i++) {
         new_tes_loads[i] =
            nir_load_per_vertex_input(b, 1, new_bit_size,
                  i ? nir_imm_int(b, i) : zero, zero,
                  .base = nir_intrinsic_base(first_load),
                  .component = nir_intrinsic_component(first_load),
                     .dest_type = nir_alu_type_get_base_type(nir_intrinsic_dest_type(first_load)) |
                                  new_bit_size,
                  .io_semantics = nir_intrinsic_io_semantics(first_load));
      }

      int remap_uvw[3] = {0, 1, 2};
      int remap_wuv[3] = {2, 0, 1};
      int *remap;

      switch (alu_interp) {
      case FLAG_INTERP_TES_TRIANGLE_UVW:
         remap = remap_uvw;
         break;
      case FLAG_INTERP_TES_TRIANGLE_WUV:
         remap = remap_wuv;
         break;
      default:
         unreachable("invalid TES interpolation mode");
      }

      nir_def *tesscoord = slot->consumer.tes_load_tess_coord;
      nir_def *defs[3];

      for (unsigned i = 0; i < 3; i++) {
         if (i == 0) {
            defs[i] = nir_fmul(b, new_tes_loads[i],
                               nir_channel(b, tesscoord, remap[i]));
         } else {
            defs[i] = nir_ffma(b, new_tes_loads[i],
                               nir_channel(b, tesscoord, remap[i]),
                               defs[i - 1]);
         }
      }
      new_input = defs[2];

      mask = new_bit_size == 16 ? linkage->flat16_mask
                                : linkage->flat32_mask;
   } else {
      new_input =
         nir_load_input(b, 1, new_bit_size, nir_imm_int(b, 0),
                        .base = nir_intrinsic_base(first_load),
                        .component = nir_intrinsic_component(first_load),
                        .dest_type = nir_alu_type_get_base_type(nir_intrinsic_dest_type(first_load)) |
                                    new_bit_size,
                        .io_semantics = nir_intrinsic_io_semantics(first_load));

      if (linkage->consumer_stage == MESA_SHADER_FRAGMENT &&
          alu_interp == FLAG_INTERP_CONVERGENT) {
         mask = new_bit_size == 16 ? linkage->convergent16_mask
                                   : linkage->convergent32_mask;
      } else {
         mask = new_bit_size == 16 ? linkage->flat16_mask
                                   : linkage->flat32_mask;
      }
   }

   assert(!BITSET_TEST(linkage->no_varying32_mask, slot_index));
   assert(!BITSET_TEST(linkage->no_varying16_mask, slot_index));

   /* Re-set the category of the new scalar input. This will cause
    * the compaction to treat it as a different type, so that it will move it
    * into the vec4 that has compatible interpolation qualifiers.
    *
    * This shouldn't be done if any of the interp masks are not set, which
    * indicates that compaction is disallowed.
    */
   if (BITSET_TEST(linkage->interp_fp32_mask, slot_index) ||
       BITSET_TEST(linkage->interp_fp16_mask, slot_index) ||
       BITSET_TEST(linkage->flat32_mask, slot_index) ||
       BITSET_TEST(linkage->flat16_mask, slot_index) ||
       BITSET_TEST(linkage->convergent32_mask, slot_index) ||
       BITSET_TEST(linkage->convergent16_mask, slot_index)) {
      BITSET_CLEAR(linkage->interp_fp32_mask, slot_index);
      BITSET_CLEAR(linkage->interp_fp16_mask, slot_index);
      BITSET_CLEAR(linkage->flat16_mask, slot_index);
      BITSET_CLEAR(linkage->flat32_mask, slot_index);
      BITSET_CLEAR(linkage->convergent16_mask, slot_index);
      BITSET_CLEAR(linkage->convergent32_mask, slot_index);
      BITSET_SET(mask, slot_index);
   }

   /* Replace the existing load with the new load in the slot. */
   if (linkage->consumer_stage == MESA_SHADER_TESS_EVAL &&
       alu_interp >= FLAG_INTERP_TES_TRIANGLE_UVW) {
      /* For TES, replace all 3 loads. */
      unsigned i = 0;
      list_for_each_entry(struct list_node, iter, &slot->consumer.loads,
                          head) {
         assert(i < 3);
         iter->instr = nir_instr_as_intrinsic(new_tes_loads[i]->parent_instr);
         i++;
      }

      assert(i == 3);
      assert(postdom->def.bit_size != 1);

      slot->consumer.tes_interp_load =
         nir_instr_as_alu(new_input->parent_instr);
   } else {
      assert(list_is_singular(&slot->consumer.loads));
      list_first_entry(&slot->consumer.loads, struct list_node, head)->instr =
         nir_instr_as_intrinsic(new_input->parent_instr);

      /* The input is a bigger type even if the post-dominator is boolean. */
      if (postdom->def.bit_size == 1)
         new_input = nir_ine_imm(b, new_input, 0);
   }

   nir_def_rewrite_uses(&postdom->def, new_input);

   /* Clone the post-dominator at the end of the block in the producer
    * where the output stores are.
    */
   b = &linkage->producer_builder;
   b->cursor = nir_after_block_before_jump(block);
   nir_def *producer_clone = clone_ssa(linkage, b, &postdom->def);

   /* Boolean post-dominators are upcast in the producer because we can't
    * use 1-bit outputs.
    */
   if (producer_clone->bit_size == 1)
      producer_clone = nir_b2bN(b, producer_clone, new_bit_size);

   /* Move the existing store to the end of the block and rewrite it to use
    * the post-dominator result.
    */
   nir_intrinsic_instr *store =
      list_first_entry(&linkage->slot[final_slot].producer.stores,
                       struct list_node, head)->instr;
   nir_instr_move(b->cursor, &store->instr);
   if (nir_src_bit_size(store->src[0]) != producer_clone->bit_size)
      nir_intrinsic_set_src_type(store, nir_alu_type_get_base_type(nir_intrinsic_src_type(store)) |
                                        producer_clone->bit_size);
   nir_src_rewrite(&store->src[0], producer_clone);

   /* Remove all loads and stores that we are replacing from the producer
    * and consumer.
    */
   for (unsigned i = 0; i < num_loads; i++) {
      unsigned slot_index = intr_get_scalar_16bit_slot(loads[i]);

      if (slot_index == final_slot) {
         /* Keep the load and store that we reused. */
         continue;
      }

      /* Remove loads and stores that are dead after the code motion. Only
       * those loads that are post-dominated by the post-dominator are dead.
       */
      struct scalar_slot *slot = &linkage->slot[slot_index];
      nir_instr *load;

      if (slot->consumer.tes_interp_load) {
         load = &slot->consumer.tes_interp_load->instr;

         /* With interpolated TES loads, we get here 3 times, once for each
          * per-vertex load. Skip this if we've been here before.
          */
         if (list_is_empty(&slot->producer.stores)) {
            assert(list_is_empty(&slot->consumer.loads));
            continue;
         }
      } else {
         assert(list_is_singular(&slot->consumer.loads));
         load = &list_first_entry(&slot->consumer.loads,
                                  struct list_node, head)->instr->instr;
      }

      if (nir_instr_dominates_use(postdom_state, &postdom->instr, load)) {
         list_inithead(&slot->consumer.loads);

         /* Remove stores. (transform feedback is allowed here, just not
          * in final_slot)
          */
         remove_all_stores_and_clear_slot(linkage, slot_index, progress);
      }
   }

   *progress |= nir_progress_producer | nir_progress_consumer;
   return true;
}

static bool
backward_inter_shader_code_motion(struct linkage_info *linkage,
                                  nir_opt_varyings_progress *progress)
{
   /* These producers are not supported. The description at the beginning
    * suggests a possible workaround.
    */
   if (linkage->producer_stage == MESA_SHADER_GEOMETRY ||
       linkage->producer_stage == MESA_SHADER_MESH ||
       linkage->producer_stage == MESA_SHADER_TASK)
      return false;

   /* Clear pass_flags. */
   nir_shader_clear_pass_flags(linkage->consumer_builder.shader);

   /* Gather inputs that can be moved into the previous shader. These are only
    * checked for the basic constraints for movability.
    */
   struct {
      nir_def *def;
      nir_intrinsic_instr *first_load;
   } movable_loads[NUM_SCALAR_SLOTS];
   unsigned num_movable_loads = 0;
   unsigned i;

   BITSET_FOREACH_SET(i, linkage->output_equal_mask, NUM_SCALAR_SLOTS) {
      if (!can_optimize_varying(linkage,
                                vec4_slot(i)).inter_shader_code_motion)
         continue;

      struct scalar_slot *slot = &linkage->slot[i];

      assert(!list_is_empty(&slot->producer.stores));
      assert(!is_interpolated_texcoord(linkage, i));
      assert(!is_interpolated_color(linkage, i));

      /* Disallow producer loads. */
      if (!list_is_empty(&slot->producer.loads))
         continue;

      /* There should be only 1 store per output. */
      if (!list_is_singular(&slot->producer.stores))
         continue;

      nir_def *load_def = NULL;
      nir_intrinsic_instr *load =
         list_first_entry(&slot->consumer.loads, struct list_node,
                          head)->instr;

      nir_intrinsic_instr *store =
        list_first_entry(&slot->producer.stores, struct list_node,
                         head)->instr;

      /* Set interpolation flags.
       * Handle interpolated TES loads first because they are special.
       */
      if (linkage->consumer_stage == MESA_SHADER_TESS_EVAL &&
          slot->consumer.tes_interp_load) {
         if (linkage->producer_stage == MESA_SHADER_VERTEX) {
            /* VS -> TES has no constraints on VS stores. */
            load_def = &slot->consumer.tes_interp_load->def;
            load_def->parent_instr->pass_flags |= FLAG_ALU_IS_TES_INTERP_LOAD |
                                                  slot->consumer.tes_interp_mode;
         } else {
            assert(linkage->producer_stage == MESA_SHADER_TESS_CTRL);
            assert(store->intrinsic == nir_intrinsic_store_per_vertex_output);

            /* The vertex index of the store must InvocationID. */
            if (is_sysval(store->src[1].ssa->parent_instr,
                          SYSTEM_VALUE_INVOCATION_ID)) {
               load_def = &slot->consumer.tes_interp_load->def;
               load_def->parent_instr->pass_flags |= FLAG_ALU_IS_TES_INTERP_LOAD |
                                                     slot->consumer.tes_interp_mode;
            } else {
               continue;
            }
         }
      } else {
         /* Allow only 1 load per input. CSE should be run before this. */
         if (!list_is_singular(&slot->consumer.loads))
            continue;

         /* This can only be TCS -> TES, which is handled above and rejected
          * otherwise.
          */
         if (store->intrinsic == nir_intrinsic_store_per_vertex_output) {
            assert(linkage->producer_stage == MESA_SHADER_TESS_CTRL);
            continue;
         }

         /* TODO: handle load_per_vertex_input for TCS and GS.
          * TES can also occur here if tes_interp_load is NULL.
          */
         if (load->intrinsic == nir_intrinsic_load_per_vertex_input)
            continue;

         load_def = &load->def;

         switch (load->intrinsic) {
         case nir_intrinsic_load_interpolated_input: {
            assert(linkage->consumer_stage == MESA_SHADER_FRAGMENT);
            nir_intrinsic_instr *baryc =
               nir_instr_as_intrinsic(load->src[0].ssa->parent_instr);
            nir_intrinsic_op op = baryc->intrinsic;
            enum glsl_interp_mode interp = nir_intrinsic_interp_mode(baryc);
            bool linear = interp == INTERP_MODE_NOPERSPECTIVE;
            bool convergent = BITSET_TEST(linkage->convergent32_mask, i) ||
                              BITSET_TEST(linkage->convergent16_mask, i);

            assert(interp == INTERP_MODE_NONE ||
                   interp == INTERP_MODE_SMOOTH ||
                   interp == INTERP_MODE_NOPERSPECTIVE);

            if (convergent) {
               load->instr.pass_flags |= FLAG_INTERP_CONVERGENT;
            } else if (op == nir_intrinsic_load_barycentric_pixel) {
               load->instr.pass_flags |= linear ? FLAG_INTERP_LINEAR_PIXEL
                                                : FLAG_INTERP_PERSP_PIXEL;
            } else if (op == nir_intrinsic_load_barycentric_centroid) {
               load->instr.pass_flags |= linear ? FLAG_INTERP_LINEAR_CENTROID
                                                : FLAG_INTERP_PERSP_CENTROID;
            } else if (op == nir_intrinsic_load_barycentric_sample) {
               load->instr.pass_flags |= linear ? FLAG_INTERP_LINEAR_SAMPLE
                                                : FLAG_INTERP_PERSP_SAMPLE;
            } else {
               /* Optimizing at_offset and at_sample would be possible but
                * maybe not worth it if they are not convergent. Convergent
                * inputs can trivially switch the barycentric coordinates
                * to different ones or flat.
                */
               continue;
            }
            break;
         }
         case nir_intrinsic_load_input:
            if (linkage->consumer_stage == MESA_SHADER_FRAGMENT) {
               if (BITSET_TEST(linkage->convergent32_mask, i) ||
                   BITSET_TEST(linkage->convergent16_mask, i))
                  load->instr.pass_flags |= FLAG_INTERP_CONVERGENT;
               else
                  load->instr.pass_flags |= FLAG_INTERP_FLAT;
            } else if (linkage->consumer_stage == MESA_SHADER_TESS_EVAL) {
               assert(vec4_slot(i) >= VARYING_SLOT_PATCH0 &&
                      vec4_slot(i) <= VARYING_SLOT_PATCH31);
               /* Patch inputs are always convergent. */
               load->instr.pass_flags |= FLAG_INTERP_CONVERGENT;
            } else {
               /* It's not a fragment shader. We still need to set this. */
               load->instr.pass_flags |= FLAG_INTERP_FLAT;
            }
            break;
         case nir_intrinsic_load_input_vertex:
            /* Inter-shader code motion is unimplemented for explicit
             * interpolation.
             */
            continue;
         default:
            unreachable("unexpected load intrinsic");
         }
      }

      load_def->parent_instr->pass_flags |= FLAG_MOVABLE;

      /* Disallow transform feedback. The load is "movable" for the purpose of
       * finding a movable post-dominator, we just can't rewrite the store
       * because we need to keep it for xfb, so the post-dominator search
       * will have to start from a different load (only that varying will have
       * its value rewritten).
       */
      if (BITSET_TEST(linkage->xfb_mask, i))
         continue;

      assert(num_movable_loads < ARRAY_SIZE(movable_loads));
      movable_loads[num_movable_loads].def = load_def;
      movable_loads[num_movable_loads].first_load = load;
      num_movable_loads++;
   }

   if (!num_movable_loads)
      return false;

   /* Inter-shader code motion turns ALU results into outputs, but not all
    * bit sizes are supported by outputs.
    *
    * The 1-bit type is allowed because the pass always promotes 1-bit
    * outputs to 16 or 32 bits, whichever is supported.
    *
    * TODO: We could support replacing 2 32-bit inputs with one 64-bit
    * post-dominator by supporting 64 bits here, but the likelihood of that
    * occuring seems low.
    */
   unsigned supported_io_types = 32 | 1;

   if (linkage->producer_builder.shader->options->io_options &
       linkage->consumer_builder.shader->options->io_options &
       nir_io_16bit_input_output_support)
      supported_io_types |= 16;

   struct nir_use_dominance_state *postdom_state =
      nir_calc_use_dominance_impl(linkage->consumer_builder.impl, true);

   for (unsigned i = 0; i < num_movable_loads; i++) {
      nir_def *load_def = movable_loads[i].def;
      nir_instr *iter = load_def->parent_instr;
      nir_instr *movable_postdom = NULL;

      /* Find the farthest post-dominator that is movable. */
      while (iter) {
         iter = nir_get_immediate_use_dominator(postdom_state, iter);
         if (iter) {
            if (NEED_UPDATE_MOVABLE_FLAGS(iter))
               update_movable_flags(linkage, iter);

            if (iter->pass_flags & FLAG_UNMOVABLE)
               break;

            /* This can only be an ALU instruction. */
            nir_alu_instr *alu = nir_instr_as_alu(iter);

            /* Skip unsupported bit sizes and keep searching. */
            if (!(alu->def.bit_size & supported_io_types))
               continue;

            /* Skip comparison opcodes that directly source the first load
             * and a constant because any 1-bit values would have to be
             * converted to 32 bits in the producer and then converted back
             * to 1 bit using nir_op_ine in the consumer, achieving nothing.
             */
            if (alu->def.bit_size == 1 &&
                ((nir_op_infos[alu->op].num_inputs == 1 &&
                  alu->src[0].src.ssa == load_def) ||
                 (nir_op_infos[alu->op].num_inputs == 2 &&
                  ((alu->src[0].src.ssa == load_def &&
                    alu->src[1].src.ssa->parent_instr->type ==
                    nir_instr_type_load_const) ||
                   (alu->src[0].src.ssa->parent_instr->type ==
                    nir_instr_type_load_const &&
                    alu->src[1].src.ssa == load_def)))))
               continue;

            movable_postdom = iter;
         }
      }

      /* Add the post-dominator to the list unless it's been added already. */
      if (movable_postdom &&
          !(movable_postdom->pass_flags & FLAG_POST_DOMINATOR_PROCESSED)) {
         if (try_move_postdominator(linkage, postdom_state,
                                    nir_instr_as_alu(movable_postdom),
                                    load_def, movable_loads[i].first_load,
                                    progress)) {
            /* Moving only one postdominator can change the IR enough that
             * we should start from scratch.
             */
            ralloc_free(postdom_state);
            return true;
         }

         movable_postdom->pass_flags |= FLAG_POST_DOMINATOR_PROCESSED;
      }
   }

   ralloc_free(postdom_state);
   return false;
}

/******************************************************************
 * COMPACTION
 ******************************************************************/

/* Relocate a slot to a new index. Used by compaction. new_index is
 * the component index at 16-bit granularity, so the size of vec4 is 8
 * in that representation.
 */
static void
relocate_slot(struct linkage_info *linkage, struct scalar_slot *slot,
              unsigned i, unsigned new_index, enum fs_vec4_type fs_vec4_type,
              nir_opt_varyings_progress *progress)
{
   assert(!list_is_empty(&slot->producer.stores));

   list_for_each_entry(struct list_node, iter, &slot->producer.stores, head) {
      assert(!nir_intrinsic_io_semantics(iter->instr).no_varying ||
             has_xfb(iter->instr) ||
             linkage->producer_stage == MESA_SHADER_TESS_CTRL);
      assert(!is_active_sysval_output(linkage, i, iter->instr));
   }

   /* Relocate the slot in all loads and stores. */
   struct list_head *instruction_lists[3] = {
      &slot->producer.stores,
      &slot->producer.loads,
      &slot->consumer.loads,
   };

   for (unsigned i = 0; i < ARRAY_SIZE(instruction_lists); i++) {
      list_for_each_entry(struct list_node, iter, instruction_lists[i], head) {
         nir_intrinsic_instr *intr = iter->instr;

         gl_varying_slot new_semantic = vec4_slot(new_index);
         unsigned new_component = (new_index % 8) / 2;
         bool new_high_16bits = new_index % 2;

         /* We also need to relocate xfb info because it's always relative
          * to component 0. This just moves it into the correct xfb slot.
          */
         if (has_xfb(intr)) {
            unsigned old_component = nir_intrinsic_component(intr);
            static const nir_io_xfb clear_xfb;
            nir_io_xfb xfb;
            bool new_is_odd = new_component % 2 == 1;

            memset(&xfb, 0, sizeof(xfb));

            if (old_component >= 2) {
               xfb.out[new_is_odd] = nir_intrinsic_io_xfb2(intr).out[old_component - 2];
               nir_intrinsic_set_io_xfb2(intr, clear_xfb);
            } else {
               xfb.out[new_is_odd] = nir_intrinsic_io_xfb(intr).out[old_component];
               nir_intrinsic_set_io_xfb(intr, clear_xfb);
            }

            if (new_component >= 2)
               nir_intrinsic_set_io_xfb2(intr, xfb);
            else
               nir_intrinsic_set_io_xfb(intr, xfb);
         }

         nir_io_semantics sem = nir_intrinsic_io_semantics(intr);

         /* When relocating a back color store, don't change it to a front
          * color as that would be incorrect. Keep it as back color and only
          * relocate it between BFC0 and BFC1.
          */
         if (linkage->consumer_stage == MESA_SHADER_FRAGMENT &&
             (sem.location == VARYING_SLOT_BFC0 ||
              sem.location == VARYING_SLOT_BFC1)) {
            assert(new_semantic == VARYING_SLOT_COL0 ||
                   new_semantic == VARYING_SLOT_COL1);
            new_semantic = VARYING_SLOT_BFC0 +
                           (new_semantic - VARYING_SLOT_COL0);
         }

#if PRINT_RELOCATE_SLOT
         unsigned bit_size =
            (intr->intrinsic == nir_intrinsic_load_input ||
             intr->intrinsic == nir_intrinsic_load_input_vertex ||
             intr->intrinsic == nir_intrinsic_load_interpolated_input)
            ? intr->def.bit_size : intr->src[0].ssa->bit_size;

         assert(bit_size == 16 || bit_size == 32);

         fprintf(stderr, "--- relocating: %s.%c%s%s -> %s.%c%s%s\n",
                 gl_varying_slot_name_for_stage(sem.location, linkage->producer_stage) + 13,
                 "xyzw"[nir_intrinsic_component(intr) % 4],
                 (bit_size == 16 && !sem.high_16bits) ? ".lo" : "",
                 (bit_size == 16 && sem.high_16bits) ? ".hi" : "",
                 gl_varying_slot_name_for_stage(new_semantic, linkage->producer_stage) + 13,
                 "xyzw"[new_component % 4],
                 (bit_size == 16 && !new_high_16bits) ? ".lo" : "",
                 (bit_size == 16 && new_high_16bits) ? ".hi" : "");
#endif /* PRINT_RELOCATE_SLOT */

         sem.location = new_semantic;
         sem.high_16bits = new_high_16bits;

         /* This is never indirectly indexed. Simplify num_slots. */
         sem.num_slots = 1;

         nir_intrinsic_set_io_semantics(intr, sem);
         nir_intrinsic_set_component(intr, new_component);

         if (fs_vec4_type == FS_VEC4_TYPE_PER_PRIMITIVE) {
            assert(intr->intrinsic == nir_intrinsic_store_per_primitive_output ||
                   intr->intrinsic == nir_intrinsic_load_per_primitive_output ||
                   intr->intrinsic == nir_intrinsic_load_input);
            assert(intr->intrinsic != nir_intrinsic_load_input || sem.per_primitive);
         } else {
            assert(!sem.per_primitive);
            assert(intr->intrinsic != nir_intrinsic_store_per_primitive_output &&
                   intr->intrinsic != nir_intrinsic_load_per_primitive_output);
         }

         /* This path is used when promoting convergent interpolated
          * inputs to flat. Replace load_interpolated_input with load_input.
          */
         if (fs_vec4_type == FS_VEC4_TYPE_FLAT &&
             intr->intrinsic == nir_intrinsic_load_interpolated_input) {
            assert(instruction_lists[i] == &slot->consumer.loads);
            nir_builder *b = &linkage->consumer_builder;

            b->cursor = nir_before_instr(&intr->instr);
            nir_def *load =
               nir_load_input(b, 1, intr->def.bit_size,
                              nir_get_io_offset_src(intr)->ssa,
                              .io_semantics = sem,
                              .component = new_component,
                              .dest_type = nir_intrinsic_dest_type(intr));

            nir_def_rewrite_uses(&intr->def, load);
            iter->instr = nir_instr_as_intrinsic(load->parent_instr);
            nir_instr_remove(&intr->instr);
            *progress |= nir_progress_consumer;

            /* Interpolation converts Infs to NaNs. If we change it to flat,
             * we need to convert Infs to NaNs manually in the producer to
             * preserve that.
             */
            if (preserve_nans(linkage->consumer_builder.shader,
                              load->bit_size)) {
               list_for_each_entry(struct list_node, iter,
                                   &slot->producer.stores, head) {
                  nir_intrinsic_instr *store = iter->instr;

                  nir_builder *b = &linkage->producer_builder;
                  b->cursor = nir_before_instr(&store->instr);
                  nir_def *repl =
                     build_convert_inf_to_nan(b, store->src[0].ssa);
                  nir_src_rewrite(&store->src[0], repl);
               }
            }
         }
      }
   }
}

/**
 * A helper function for compact_varyings(). Assign new slot indices for
 * existing slots of a certain vec4 type (FLAT, FP16, or FP32). Skip already-
 * assigned scalar slots (determined by assigned_mask) and don't assign to
 * vec4 slots that have an incompatible vec4 type (determined by
 * assigned_fs_vec4_type). This works with both 32-bit and 16-bit types.
 * slot_size is the component size in the units of 16 bits (2 means 32 bits).
 *
 * The number of slots to assign can optionally be limited by
 * max_assigned_slots.
 *
 * Return how many 16-bit slots are left unused in the last vec4 (up to 8
 * slots).
 */
static unsigned
fs_assign_slots(struct linkage_info *linkage,
                BITSET_WORD *assigned_mask,
                uint8_t assigned_fs_vec4_type[NUM_TOTAL_VARYING_SLOTS],
                BITSET_WORD *input_mask,
                enum fs_vec4_type fs_vec4_type,
                unsigned slot_size,
                unsigned max_assigned_slots,
                bool assign_colors,
                unsigned color_channel_rotate,
                nir_opt_varyings_progress *progress)
{
   unsigned i, slot_index, max_slot;
   unsigned num_assigned_slots = 0;

   if (assign_colors) {
      slot_index = VARYING_SLOT_COL0 * 8; /* starting slot */
      max_slot = VARYING_SLOT_COL1 * 8 + 8;
   } else {
      slot_index = VARYING_SLOT_VAR0 * 8; /* starting slot */
      max_slot = VARYING_SLOT_MAX;
   }

   /* Assign new slot indices for scalar slots. */
   BITSET_FOREACH_SET(i, input_mask, NUM_SCALAR_SLOTS) {
      if (is_interpolated_color(linkage, i) != assign_colors)
         continue;

      /* Skip indirectly-indexed scalar slots and slots incompatible
       * with the FS vec4 type.
       */
      while ((fs_vec4_type != FS_VEC4_TYPE_NONE &&
              assigned_fs_vec4_type[vec4_slot(slot_index)] !=
              FS_VEC4_TYPE_NONE &&
              assigned_fs_vec4_type[vec4_slot(slot_index)] !=
              fs_vec4_type) ||
             BITSET_TEST32(linkage->indirect_mask, slot_index) ||
             BITSET_TEST(assigned_mask, slot_index)) {
         /* If the FS vec4 type is incompatible. Move to the next vec4. */
         if (fs_vec4_type != FS_VEC4_TYPE_NONE &&
             assigned_fs_vec4_type[vec4_slot(slot_index)] !=
             FS_VEC4_TYPE_NONE &&
             assigned_fs_vec4_type[vec4_slot(slot_index)] != fs_vec4_type) {
            slot_index = align(slot_index + slot_size, 8); /* move to next vec4 */
            continue;
         }

         /* Copy the FS vec4 type if indexed indirectly, and move to
          * the next slot.
          */
         if (BITSET_TEST32(linkage->indirect_mask, slot_index)) {
            if (assigned_fs_vec4_type) {
               assigned_fs_vec4_type[vec4_slot(slot_index)] =
                  linkage->fs_vec4_type[vec4_slot(slot_index)];
            }
            assert(slot_index % 2 == 0);
            slot_index += 2; /* increment by 32 bits */
            continue;
         }

         /* This slot is already assigned (assigned_mask is set). Move to
          * the next one.
          */
         slot_index += slot_size;
      }

      /* Assign color channels in this order, starting
       * at the color_channel_rotate component first. Cases:
       *    color_channel_rotate = 0: xyzw
       *    color_channel_rotate = 1: yzwx
       *    color_channel_rotate = 2: zwxy
       *    color_channel_rotate = 3: wxyz
       *
       * This has no effect on behavior per se, but some drivers merge VARn
       * and COLn into one output if each defines different components.
       * For example, if we store VAR0.xy and COL0.z, a driver can merge them
       * by mapping the same output to 2 different inputs (VAR0 and COL0) if
       * color-specific behavior is per component, but it can't merge VAR0.xy
       * and COL0.x because they both define x.
       */
      unsigned new_slot_index = slot_index;
      if (assign_colors && color_channel_rotate) {
         new_slot_index = (vec4_slot(new_slot_index)) * 8 +
                          (new_slot_index + color_channel_rotate * 2) % 8;
      }

      /* Relocate the slot. */
      assert(slot_index < max_slot * 8);
      relocate_slot(linkage, &linkage->slot[i], i, new_slot_index,
                    fs_vec4_type, progress);

      for (unsigned i = 0; i < slot_size; ++i)
         BITSET_SET(assigned_mask, slot_index + i);

      if (assigned_fs_vec4_type)
         assigned_fs_vec4_type[vec4_slot(slot_index)] = fs_vec4_type;
      slot_index += slot_size; /* move to the next slot */
      num_assigned_slots += slot_size;

      /* Remove the slot from the input (unassigned) mask. */
      BITSET_CLEAR(input_mask, i);

      /* The number of slots to assign can optionally be limited. */
      assert(num_assigned_slots <= max_assigned_slots);
      if (num_assigned_slots == max_assigned_slots)
         break;
   }

   assert(slot_index <= max_slot * 8);
   /* Return how many 16-bit slots are left unused in the last vec4. */
   return (NUM_SCALAR_SLOTS - slot_index) % 8;
}

/**
 * This is called once for 32-bit inputs and once for 16-bit inputs.
 * It assigns new slot indices to all scalar slots specified in the masks.
 *
 * \param linkage             Linkage info
 * \param assigned_mask       Which scalar (16-bit) slots are already taken.
 * \param assigned_fs_vec4_type Which vec4 slots have an assigned qualifier
 *                              and can only be filled with compatible slots.
 * \param interp_mask         The list of interp slots to assign locations for.
 * \param flat_mask           The list of flat slots to assign locations for.
 * \param convergent_mask     The list of slots that have convergent output
 *                            stores.
 * \param sized_interp_type   One of FS_VEC4_TYPE_INTERP_{FP32, FP16, COLOR}.
 * \param slot_size           1 for 16 bits, 2 for 32 bits
 * \param color_channel_rotate Assign color channels starting with this index,
 *                            e.g. 2 assigns channels in the zwxy order.
 * \param assign_colors       Whether to assign only color varyings or only
 *                            non-color varyings.
 */
static void
fs_assign_slot_groups(struct linkage_info *linkage,
                      BITSET_WORD *assigned_mask,
                      uint8_t assigned_fs_vec4_type[NUM_TOTAL_VARYING_SLOTS],
                      BITSET_WORD *interp_mask,
                      BITSET_WORD *flat_mask,
                      BITSET_WORD *convergent_mask,
                      BITSET_WORD *color_interp_mask,
                      enum fs_vec4_type sized_interp_type,
                      unsigned slot_size,
                      bool assign_colors,
                      unsigned color_channel_rotate,
                      nir_opt_varyings_progress *progress)
{
   /* Put interpolated slots first. */
   unsigned unused_interp_slots =
      fs_assign_slots(linkage, assigned_mask, assigned_fs_vec4_type,
                      interp_mask, sized_interp_type,
                      slot_size, NUM_SCALAR_SLOTS, assign_colors,
                      color_channel_rotate, progress);

   unsigned unused_color_interp_slots = 0;
   if (color_interp_mask) {
      unused_color_interp_slots =
         fs_assign_slots(linkage, assigned_mask, assigned_fs_vec4_type,
                         color_interp_mask, FS_VEC4_TYPE_INTERP_COLOR,
                         slot_size, NUM_SCALAR_SLOTS, assign_colors,
                         color_channel_rotate, progress);
   }

   /* Put flat slots next.
    * Note that only flat vec4 slots can have both 32-bit and 16-bit types
    * packed in the same vec4. 32-bit flat inputs are packed first, followed
    * by 16-bit flat inputs.
    */
   unsigned unused_flat_slots =
      fs_assign_slots(linkage, assigned_mask, assigned_fs_vec4_type,
                      flat_mask, FS_VEC4_TYPE_FLAT,
                      slot_size, NUM_SCALAR_SLOTS, assign_colors,
                      color_channel_rotate, progress);

   /* Take the inputs with convergent values and assign them as follows.
    * Since they can be assigned as both interpolated and flat, we can
    * choose. We prefer them to be flat, but if interpolated vec4s have
    * unused components, try to fill those before starting a new flat vec4.
    *
    * First, fill the unused components of flat (if any), then fill
    * the unused components of interpolated (if any), and then make
    * the remaining convergent inputs flat.
    */
   if (unused_flat_slots) {
      fs_assign_slots(linkage, assigned_mask, assigned_fs_vec4_type,
                      convergent_mask, FS_VEC4_TYPE_FLAT,
                      slot_size, unused_flat_slots, assign_colors,
                      color_channel_rotate, progress);
   }
   if (unused_interp_slots) {
      fs_assign_slots(linkage, assigned_mask, assigned_fs_vec4_type,
                      convergent_mask, sized_interp_type,
                      slot_size, unused_interp_slots, assign_colors,
                      color_channel_rotate, progress);
   }
   if (unused_color_interp_slots) {
      fs_assign_slots(linkage, assigned_mask, assigned_fs_vec4_type,
                      convergent_mask, FS_VEC4_TYPE_INTERP_COLOR,
                      slot_size, unused_color_interp_slots, assign_colors,
                      color_channel_rotate, progress);
   }
   fs_assign_slots(linkage, assigned_mask, assigned_fs_vec4_type,
                   convergent_mask, FS_VEC4_TYPE_FLAT,
                   slot_size, NUM_SCALAR_SLOTS, assign_colors,
                   color_channel_rotate, progress);
}

static void
vs_tcs_tes_gs_assign_slots(struct linkage_info *linkage,
                           BITSET_WORD *input_mask,
                           unsigned *slot_index,
                           unsigned *patch_slot_index,
                           unsigned slot_size,
                           nir_opt_varyings_progress *progress)
{
   unsigned i;

   BITSET_FOREACH_SET(i, input_mask, NUM_SCALAR_SLOTS) {
      if (i >= VARYING_SLOT_PATCH0 * 8 && i < VARYING_SLOT_TESS_MAX * 8) {
         /* Skip indirectly-indexed scalar slots at 32-bit granularity.
          * We have to do it at this granularity because the low 16-bit
          * slot is set to 1 for 32-bit inputs but not the high 16-bit slot.
          */
         while (BITSET_TEST32(linkage->indirect_mask, *patch_slot_index))
            *patch_slot_index = align(*patch_slot_index + 1, 2);

         assert(*patch_slot_index < VARYING_SLOT_TESS_MAX * 8);
         relocate_slot(linkage, &linkage->slot[i], i, *patch_slot_index,
                       FS_VEC4_TYPE_NONE, progress);
         *patch_slot_index += slot_size; /* increment by 16 or 32 bits */
      } else {
         /* If the driver wants to use POS and we've already used it, move
          * to VARn.
          */
         if (*slot_index < VARYING_SLOT_VAR0 &&
             *slot_index >= VARYING_SLOT_POS + 8)
            *slot_index = VARYING_SLOT_VAR0 * 8;

         /* Skip indirectly-indexed scalar slots at 32-bit granularity. */
         while (BITSET_TEST32(linkage->indirect_mask, *slot_index))
            *slot_index = align(*slot_index + 1, 2);

         assert(*slot_index < VARYING_SLOT_MAX * 8);
         relocate_slot(linkage, &linkage->slot[i], i, *slot_index,
                       FS_VEC4_TYPE_NONE, progress);
         *slot_index += slot_size; /* increment by 16 or 32 bits */
      }
   }
}

/**
 * Compaction means scalarizing and then packing scalar components into full
 * vec4s, so that we minimize the number of unused components in vec4 slots.
 *
 * Compaction is as simple as moving a scalar input from one scalar slot
 * to another. Indirectly-indexed slots are not touched, so the compaction
 * has to compact around them. Unused 32-bit components of indirectly-indexed
 * slots are still filled, so no space is wasted there, but if indirectly-
 * indexed 16-bit components have the other 16-bit half unused, that half is
 * wasted.
 */
static void
compact_varyings(struct linkage_info *linkage,
                 nir_opt_varyings_progress *progress)
{
   if (linkage->consumer_stage == MESA_SHADER_FRAGMENT) {
      /* These arrays are used to track which scalar slots we've already
       * assigned. We can fill unused components of indirectly-indexed slots,
       * but only if the vec4 slot type (FLAT, FP16, or FP32) is the same.
       * Assign vec4 slot type separately, skipping over already assigned
       * scalar slots.
       */
      uint8_t assigned_fs_vec4_type[NUM_TOTAL_VARYING_SLOTS] = {0};
      BITSET_DECLARE(assigned_mask, NUM_SCALAR_SLOTS);
      BITSET_ZERO(assigned_mask);

      fs_assign_slot_groups(linkage, assigned_mask, assigned_fs_vec4_type,
                            linkage->interp_fp32_mask, linkage->flat32_mask,
                            linkage->convergent32_mask, NULL,
                            FS_VEC4_TYPE_INTERP_FP32, 2, false, 0, progress);

      /* Now do the same thing, but for 16-bit inputs. */
      fs_assign_slot_groups(linkage, assigned_mask, assigned_fs_vec4_type,
                            linkage->interp_fp16_mask, linkage->flat16_mask,
                            linkage->convergent16_mask, NULL,
                            FS_VEC4_TYPE_INTERP_FP16, 1, false, 0, progress);

      /* Assign INTERP_MODE_EXPLICIT. Both FP32 and FP16 can occupy the same
       * slot because the vertex data is passed to FS as-is.
       */
      fs_assign_slots(linkage, assigned_mask, assigned_fs_vec4_type,
                      linkage->interp_explicit32_mask, FS_VEC4_TYPE_INTERP_EXPLICIT,
                      2, NUM_SCALAR_SLOTS, false, 0, progress);

      fs_assign_slots(linkage, assigned_mask, assigned_fs_vec4_type,
                      linkage->interp_explicit16_mask, FS_VEC4_TYPE_INTERP_EXPLICIT,
                      1, NUM_SCALAR_SLOTS, false, 0, progress);

      /* Same for strict vertex ordering. */
      fs_assign_slots(linkage, assigned_mask, assigned_fs_vec4_type,
                      linkage->interp_explicit_strict32_mask, FS_VEC4_TYPE_INTERP_EXPLICIT_STRICT,
                      2, NUM_SCALAR_SLOTS, false, 0, progress);

      fs_assign_slots(linkage, assigned_mask, assigned_fs_vec4_type,
                      linkage->interp_explicit_strict16_mask, FS_VEC4_TYPE_INTERP_EXPLICIT_STRICT,
                      1, NUM_SCALAR_SLOTS, false, 0, progress);

      /* Same for per-primitive. */
      fs_assign_slots(linkage, assigned_mask, assigned_fs_vec4_type,
                      linkage->per_primitive32_mask, FS_VEC4_TYPE_PER_PRIMITIVE,
                      2, NUM_SCALAR_SLOTS, false, 0, progress);

      fs_assign_slots(linkage, assigned_mask, assigned_fs_vec4_type,
                      linkage->per_primitive16_mask, FS_VEC4_TYPE_PER_PRIMITIVE,
                      1, NUM_SCALAR_SLOTS, false, 0, progress);

      /* Put transform-feedback-only outputs last. */
      fs_assign_slots(linkage, assigned_mask, NULL,
                      linkage->xfb32_only_mask, FS_VEC4_TYPE_NONE, 2,
                      NUM_SCALAR_SLOTS, false, 0, progress);

      fs_assign_slots(linkage, assigned_mask, NULL,
                      linkage->xfb16_only_mask, FS_VEC4_TYPE_NONE, 1,
                      NUM_SCALAR_SLOTS, false, 0, progress);

      /* Color varyings are only compacted among themselves. */
      /* Set whether the shader contains any color varyings. */
      unsigned col0 = VARYING_SLOT_COL0 * 8;
      bool has_colors =
         !BITSET_TEST_RANGE_INSIDE_WORD(linkage->interp_fp32_mask, col0, 16,
                                        0) ||
         !BITSET_TEST_RANGE_INSIDE_WORD(linkage->convergent32_mask, col0, 16,
                                        0) ||
         !BITSET_TEST_RANGE_INSIDE_WORD(linkage->color32_mask, col0, 16, 0) ||
         !BITSET_TEST_RANGE_INSIDE_WORD(linkage->flat32_mask, col0, 16, 0) ||
         !BITSET_TEST_RANGE_INSIDE_WORD(linkage->xfb32_only_mask, col0, 16, 0);

      if (has_colors) {
         unsigned color_channel_rotate =
            DIV_ROUND_UP(BITSET_LAST_BIT(assigned_mask), 2) % 4;

         fs_assign_slot_groups(linkage, assigned_mask, assigned_fs_vec4_type,
                               linkage->interp_fp32_mask, linkage->flat32_mask,
                               linkage->convergent32_mask, linkage->color32_mask,
                               FS_VEC4_TYPE_INTERP_FP32, 2, true,
                               color_channel_rotate, progress);

         /* Put transform-feedback-only outputs last. */
         fs_assign_slots(linkage, assigned_mask, NULL,
                         linkage->xfb32_only_mask, FS_VEC4_TYPE_NONE, 2,
                         NUM_SCALAR_SLOTS, true, color_channel_rotate,
                         progress);
      }
   } else {
      /* The consumer is a TCS, TES, or GS.
       *
       * "use_pos" says whether the driver prefers that compaction with non-FS
       * consumers puts varyings into POS first before using any VARn.
       */
      bool use_pos = !(linkage->producer_builder.shader->options->io_options &
                       nir_io_dont_use_pos_for_non_fs_varyings);
      unsigned slot_index = (use_pos ? VARYING_SLOT_POS
                                     : VARYING_SLOT_VAR0) * 8;
      unsigned patch_slot_index = VARYING_SLOT_PATCH0 * 8;

      /* Compact 32-bit inputs. */
      vs_tcs_tes_gs_assign_slots(linkage, linkage->flat32_mask, &slot_index,
                                 &patch_slot_index, 2, progress);

      /* Compact 16-bit inputs, allowing them to share vec4 slots with 32-bit
       * inputs.
       */
      vs_tcs_tes_gs_assign_slots(linkage, linkage->flat16_mask, &slot_index,
                                 &patch_slot_index, 1, progress);

      /* Put no-varying slots last. These are TCS outputs read by TCS but not
       * TES.
       */
      vs_tcs_tes_gs_assign_slots(linkage, linkage->no_varying32_mask, &slot_index,
                                 &patch_slot_index, 2, progress);
      vs_tcs_tes_gs_assign_slots(linkage, linkage->no_varying16_mask, &slot_index,
                                 &patch_slot_index, 1, progress);

      assert(slot_index <= VARYING_SLOT_MAX * 8);
      assert(patch_slot_index <= VARYING_SLOT_TESS_MAX * 8);
   }
}

/******************************************************************
 * PUTTING IT ALL TOGETHER
 ******************************************************************/

static void
init_linkage(nir_shader *producer, nir_shader *consumer, bool spirv,
             unsigned max_uniform_components, unsigned max_ubos_per_stage,
             struct linkage_info *linkage)
{
   *linkage = (struct linkage_info){
      .spirv = spirv,
      .producer_stage = producer->info.stage,
      .consumer_stage = consumer->info.stage,
      .producer_builder =
         nir_builder_create(nir_shader_get_entrypoint(producer)),
      .consumer_builder =
         nir_builder_create(nir_shader_get_entrypoint(consumer)),

      .max_varying_expression_cost =
         producer->options->varying_expression_max_cost ?
         producer->options->varying_expression_max_cost(producer, consumer) : 0,

      .linear_mem_ctx = linear_context(ralloc_context(NULL)),
   };

   for (unsigned i = 0; i < ARRAY_SIZE(linkage->slot); i++) {
      list_inithead(&linkage->slot[i].producer.loads);
      list_inithead(&linkage->slot[i].producer.stores);
      list_inithead(&linkage->slot[i].consumer.loads);
   }

   /* Preparation. */
   nir_shader_intrinsics_pass(consumer, gather_inputs, 0, linkage);
   nir_shader_intrinsics_pass(producer, gather_outputs, 0, linkage);
   tidy_up_indirect_varyings(linkage);
   determine_uniform_movability(linkage, max_uniform_components);
   determine_ubo_movability(linkage, max_ubos_per_stage);
}

static void
free_linkage(struct linkage_info *linkage)
{
   ralloc_free(ralloc_parent_of_linear_context(linkage->linear_mem_ctx));
}

static void
print_shader_linkage(nir_shader *producer, nir_shader *consumer)
{
   struct linkage_info *linkage = MALLOC_STRUCT(linkage_info);

   init_linkage(producer, consumer, false, 0, 0, linkage);
   print_linkage(linkage);
   free_linkage(linkage);
   FREE(linkage);
}

/**
 * Run lots of optimizations on varyings. See the description at the beginning
 * of this file.
 */
nir_opt_varyings_progress
nir_opt_varyings(nir_shader *producer, nir_shader *consumer, bool spirv,
                 unsigned max_uniform_components, unsigned max_ubos_per_stage)
{
   /* Task -> Mesh I/O uses payload variables and not varying slots,
    * so this pass can't do anything about it.
    */
   if (producer->info.stage == MESA_SHADER_TASK)
      return 0;

   nir_opt_varyings_progress progress = 0;
   struct linkage_info *linkage = MALLOC_STRUCT(linkage_info);
   if (linkage == NULL)
      return 0;

   /* Producers before a fragment shader must have up-to-date vertex
    * divergence information.
    */
   if (consumer->info.stage == MESA_SHADER_FRAGMENT) {
      /* Required by the divergence analysis. */
      NIR_PASS(_, producer, nir_convert_to_lcssa, true, true);
      nir_vertex_divergence_analysis(producer);
   }

   init_linkage(producer, consumer, spirv, max_uniform_components,
                max_ubos_per_stage, linkage);

   /* Part 1: Run optimizations that only remove varyings. (they can move
    * instructions between shaders)
    */
   remove_dead_varyings(linkage, &progress);
   propagate_uniform_expressions(linkage, &progress);

   /* Part 2: Deduplicate outputs. */
   deduplicate_outputs(linkage, &progress);

   /* Run CSE on the consumer after output deduplication because duplicated
    * loads can prevent finding the post-dominator for inter-shader code
    * motion.
    */
   NIR_PASS(_, consumer, nir_opt_cse);

   /* Re-gather linkage info after CSE. */
   free_linkage(linkage);
   init_linkage(producer, consumer, spirv, max_uniform_components,
                max_ubos_per_stage, linkage);
   /* This must be done again to clean up bitmasks in linkage. */
   remove_dead_varyings(linkage, &progress);

   /* This must be done after deduplication and before inter-shader code
    * motion.
    */
   tidy_up_convergent_varyings(linkage);
   find_open_coded_tes_input_interpolation(linkage);

   /* Part 3: Run optimizations that completely change varyings. */
#if PRINT
   int i = 0;
   puts("Before:");
   nir_print_shader(linkage.producer_builder.shader, stdout);
   nir_print_shader(linkage.consumer_builder.shader, stdout);
   print_linkage(&linkage);
   puts("");
#endif

   while (backward_inter_shader_code_motion(linkage, &progress)) {
#if PRINT
      i++;
      printf("Finished: %i\n", i);
      nir_print_shader(linkage->producer_builder.shader, stdout);
      nir_print_shader(linkage->consumer_builder.shader, stdout);
      print_linkage(linkage);
      puts("");
#endif
   }

   /* Part 4: Do compaction. */
   compact_varyings(linkage, &progress);

   nir_metadata_preserve(linkage->producer_builder.impl,
                         progress & nir_progress_producer ?
                            (nir_metadata_block_index |
                             nir_metadata_dominance) :
                            nir_metadata_all);
   nir_metadata_preserve(linkage->consumer_builder.impl,
                         progress & nir_progress_consumer ?
                            (nir_metadata_block_index |
                             nir_metadata_dominance) :
                            nir_metadata_all);
   free_linkage(linkage);
   FREE(linkage);

   if (progress & nir_progress_producer)
      nir_validate_shader(producer, "nir_opt_varyings");
   if (progress & nir_progress_consumer)
      nir_validate_shader(consumer, "nir_opt_varyings");

   return progress;
}
