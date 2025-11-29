/*
 * Copyright © 2020 Intel Corporation
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

#include "elk_eu.h"
#include "elk_fs.h"
#include "elk_vec4.h"
#include "elk_cfg.h"

using namespace elk;

namespace {
   /**
    * Enumeration representing the various asynchronous units that can run
    * computations in parallel on behalf of a shader thread.
    */
   enum intel_eu_unit {
      /** EU front-end. */
      EU_UNIT_FE,
      /** EU FPU0 (Note that co-issue to FPU1 is currently not modeled here). */
      EU_UNIT_FPU,
      /** Extended Math unit (AKA FPU1 on Gfx8-11, part of the EU on Gfx6+). */
      EU_UNIT_EM,
      /** Sampler shared function. */
      EU_UNIT_SAMPLER,
      /** Pixel Interpolator shared function. */
      EU_UNIT_PI,
      /** Unified Return Buffer shared function. */
      EU_UNIT_URB,
      /** Data Port Data Cache shared function. */
      EU_UNIT_DP_DC,
      /** Data Port Render Cache shared function. */
      EU_UNIT_DP_RC,
      /** Data Port Constant Cache shared function. */
      EU_UNIT_DP_CC,
      /** Message Gateway shared function. */
      EU_UNIT_GATEWAY,
      /** Thread Spawner shared function. */
      EU_UNIT_SPAWNER,
      /* EU_UNIT_VME, */
      /* EU_UNIT_CRE, */
      /** Number of asynchronous units currently tracked. */
      EU_NUM_UNITS,
      /** Dummy unit for instructions that don't consume runtime from the above. */
      EU_UNIT_NULL = EU_NUM_UNITS
   };

   /**
    * Enumeration representing a computation result another computation can
    * potentially depend on.
    */
   enum intel_eu_dependency_id {
      /* Register part of the GRF. */
      EU_DEPENDENCY_ID_GRF0 = 0,
      /* Register part of the MRF.  Only used on Gfx4-6. */
      EU_DEPENDENCY_ID_MRF0 = EU_DEPENDENCY_ID_GRF0 + XE2_MAX_GRF,
      /* Address register part of the ARF. */
      EU_DEPENDENCY_ID_ADDR0 = EU_DEPENDENCY_ID_MRF0 + 24,
      /* Accumulator register part of the ARF. */
      EU_DEPENDENCY_ID_ACCUM0 = EU_DEPENDENCY_ID_ADDR0 + 1,
      /* Flag register part of the ARF. */
      EU_DEPENDENCY_ID_FLAG0 = EU_DEPENDENCY_ID_ACCUM0 + 12,
      /* SBID token write completion.  Only used on Gfx12+. */
      EU_DEPENDENCY_ID_SBID_WR0 = EU_DEPENDENCY_ID_FLAG0 + 8,
      /* SBID token read completion.  Only used on Gfx12+. */
      EU_DEPENDENCY_ID_SBID_RD0 = EU_DEPENDENCY_ID_SBID_WR0 + 32,
      /* Number of computation dependencies currently tracked. */
      EU_NUM_DEPENDENCY_IDS = EU_DEPENDENCY_ID_SBID_RD0 + 32
   };

   /**
    * State of our modeling of the program execution.
    */
   struct state {
      state() : unit_ready(), dep_ready(), unit_busy(), weight(1.0) {}
      /**
       * Time at which a given unit will be ready to execute the next
       * computation, in clock units.
       */
      unsigned unit_ready[EU_NUM_UNITS];
      /**
       * Time at which an instruction dependent on a given dependency ID will
       * be ready to execute, in clock units.
       */
      unsigned dep_ready[EU_NUM_DEPENDENCY_IDS];
      /**
       * Aggregated utilization of a given unit excluding idle cycles,
       * in clock units.
       */
      float unit_busy[EU_NUM_UNITS];
      /**
       * Factor of the overhead of a computation accounted for in the
       * aggregated utilization calculation.
       */
      float weight;
   };

   /**
    * Information derived from an IR instruction used to compute performance
    * estimates.  Allows the timing calculation to work on both FS and VEC4
    * instructions.
    */
   struct instruction_info {
      instruction_info(const struct elk_isa_info *isa, const elk_fs_inst *inst) :
         isa(isa), devinfo(isa->devinfo), op(inst->opcode),
         td(inst->dst.type), sd(DIV_ROUND_UP(inst->size_written, REG_SIZE)),
         tx(get_exec_type(inst)), sx(0), ss(0),
         sc(elk_has_bank_conflict(isa, inst) ? sd : 0),
         desc(inst->desc), sfid(inst->sfid)
      {
         for (unsigned i = 0; i < inst->sources; i++)
            ss = MAX2(ss, DIV_ROUND_UP(inst->size_read(i), REG_SIZE));

         /* Convert the execution size to GRF units. */
         sx = DIV_ROUND_UP(inst->exec_size * type_sz(tx), REG_SIZE);

         /* 32x32 integer multiplication has half the usual ALU throughput.
          * Treat it as double-precision.
          */
         if ((inst->opcode == ELK_OPCODE_MUL || inst->opcode == ELK_OPCODE_MAD) &&
             !elk_reg_type_is_floating_point(tx) && type_sz(tx) == 4 &&
             type_sz(inst->src[0].type) == type_sz(inst->src[1].type))
            tx = elk_int_type(8, tx == ELK_REGISTER_TYPE_D);
      }

      instruction_info(const struct elk_isa_info *isa,
                       const vec4_instruction *inst) :
         isa(isa), devinfo(isa->devinfo), op(inst->opcode),
         td(inst->dst.type), sd(DIV_ROUND_UP(inst->size_written, REG_SIZE)),
         tx(get_exec_type(inst)), sx(0), ss(0), sc(0),
         desc(inst->desc), sfid(inst->sfid)
      {
         /* Compute the maximum source size. */
         for (unsigned i = 0; i < ARRAY_SIZE(inst->src); i++)
            ss = MAX2(ss, DIV_ROUND_UP(inst->size_read(i), REG_SIZE));

         /* Convert the execution size to GRF units. */
         sx = DIV_ROUND_UP(inst->exec_size * type_sz(tx), REG_SIZE);

         /* 32x32 integer multiplication has half the usual ALU throughput.
          * Treat it as double-precision.
          */
         if ((inst->opcode == ELK_OPCODE_MUL || inst->opcode == ELK_OPCODE_MAD) &&
             !elk_reg_type_is_floating_point(tx) && type_sz(tx) == 4 &&
             type_sz(inst->src[0].type) == type_sz(inst->src[1].type))
            tx = elk_int_type(8, tx == ELK_REGISTER_TYPE_D);
      }

      /** ISA encoding information */
      const struct elk_isa_info *isa;
      /** Device information. */
      const struct intel_device_info *devinfo;
      /** Instruction opcode. */
      elk_opcode op;
      /** Destination type. */
      elk_reg_type td;
      /** Destination size in GRF units. */
      unsigned sd;
      /** Execution type. */
      elk_reg_type tx;
      /** Execution size in GRF units. */
      unsigned sx;
      /** Source size. */
      unsigned ss;
      /** Bank conflict penalty size in GRF units (equal to sd if non-zero). */
      unsigned sc;
      /** Send message descriptor. */
      uint32_t desc;
      /** Send message shared function ID. */
      uint8_t sfid;
   };

   /**
    * Timing information of an instruction used to estimate the performance of
    * the program.
    */
   struct perf_desc {
      perf_desc(enum intel_eu_unit u, int df, int db,
                int ls, int ld, int la, int lf) :
         u(u), df(df), db(db), ls(ls), ld(ld), la(la), lf(lf) {}

      /**
       * Back-end unit its runtime shall be accounted to, in addition to the
       * EU front-end which is always assumed to be involved.
       */
      enum intel_eu_unit u;
      /**
       * Overhead cycles from the time that the EU front-end starts executing
       * the instruction until it's ready to execute the next instruction.
       */
      int df;
      /**
       * Overhead cycles from the time that the back-end starts executing the
       * instruction until it's ready to execute the next instruction.
       */
      int db;
      /**
       * Latency cycles from the time that the back-end starts executing the
       * instruction until its sources have been read from the register file.
       */
      int ls;
      /**
       * Latency cycles from the time that the back-end starts executing the
       * instruction until its regular destination has been written to the
       * register file.
       */
      int ld;
      /**
       * Latency cycles from the time that the back-end starts executing the
       * instruction until its accumulator destination has been written to the
       * ARF file.
       *
       * Note that this is an approximation of the real behavior of
       * accumulating instructions in the hardware: Instead of modeling a pair
       * of back-to-back accumulating instructions as a first computation with
       * latency equal to ld followed by another computation with a
       * mid-pipeline stall (e.g. after the "M" part of a MAC instruction), we
       * model the stall as if it occurred at the top of the pipeline, with
       * the latency of the accumulator computation offset accordingly.
       */
      int la;
      /**
       * Latency cycles from the time that the back-end starts executing the
       * instruction until its flag destination has been written to the ARF
       * file.
       */
      int lf;
   };

   /**
    * Compute the timing information of an instruction based on any relevant
    * information from the IR and a number of parameters specifying a linear
    * approximation: Parameter X_Y specifies the derivative of timing X
    * relative to info field Y, while X_1 specifies the independent term of
    * the approximation of timing X.
    */
   perf_desc
   calculate_desc(const instruction_info &info, enum intel_eu_unit u,
                  int df_1, int df_sd, int df_sc,
                  int db_1, int db_sx,
                  int ls_1, int ld_1, int la_1, int lf_1,
                  int l_ss, int l_sd)
   {
      return perf_desc(u, df_1 + df_sd * int(info.sd) + df_sc * int(info.sc),
                          db_1 + db_sx * int(info.sx),
                          ls_1 + l_ss * int(info.ss),
                          ld_1 + l_ss * int(info.ss) + l_sd * int(info.sd),
                          la_1, lf_1);
   }

   /**
    * Compute the timing information of an instruction based on any relevant
    * information from the IR and a number of linear approximation parameters
    * hard-coded for each IR instruction.
    *
    * Most timing parameters are obtained from the multivariate linear
    * regression of a sample of empirical timings measured using the tm0
    * register (as can be done today by using the shader_time debugging
    * option).  The Gfx4-5 math timings are obtained from BSpec Volume 5c.3
    * "Shared Functions - Extended Math", Section 3.2 "Performance".
    * Parameters marked XXX shall be considered low-quality, they're possibly
    * high variance or completely guessed in cases where experimental data was
    * unavailable.
    */
   const perf_desc
   instruction_desc(const instruction_info &info)
   {
      const struct intel_device_info *devinfo = info.devinfo;

      switch (info.op) {
      case ELK_OPCODE_SEL:
      case ELK_OPCODE_NOT:
      case ELK_OPCODE_AND:
      case ELK_OPCODE_OR:
      case ELK_OPCODE_XOR:
      case ELK_OPCODE_SHR:
      case ELK_OPCODE_SHL:
      case ELK_OPCODE_DIM:
      case ELK_OPCODE_ASR:
      case ELK_OPCODE_CMPN:
      case ELK_OPCODE_F16TO32:
      case ELK_OPCODE_BFREV:
      case ELK_OPCODE_BFI1:
      case ELK_OPCODE_AVG:
      case ELK_OPCODE_FRC:
      case ELK_OPCODE_RNDU:
      case ELK_OPCODE_RNDD:
      case ELK_OPCODE_RNDE:
      case ELK_OPCODE_RNDZ:
      case ELK_OPCODE_MAC:
      case ELK_OPCODE_MACH:
      case ELK_OPCODE_LZD:
      case ELK_OPCODE_FBH:
      case ELK_OPCODE_FBL:
      case ELK_OPCODE_CBIT:
      case ELK_OPCODE_ADDC:
      case ELK_OPCODE_SUBB:
      case ELK_OPCODE_SAD2:
      case ELK_OPCODE_SADA2:
      case ELK_OPCODE_LINE:
      case ELK_OPCODE_NOP:
      case ELK_SHADER_OPCODE_CLUSTER_BROADCAST:
      case ELK_SHADER_OPCODE_SCRATCH_HEADER:
      case ELK_FS_OPCODE_DDX_COARSE:
      case ELK_FS_OPCODE_DDX_FINE:
      case ELK_FS_OPCODE_DDY_COARSE:
      case ELK_FS_OPCODE_PIXEL_X:
      case ELK_FS_OPCODE_PIXEL_Y:
      case ELK_FS_OPCODE_SET_SAMPLE_ID:
      case ELK_VEC4_OPCODE_MOV_BYTES:
      case ELK_VEC4_OPCODE_UNPACK_UNIFORM:
      case ELK_VEC4_OPCODE_DOUBLE_TO_F32:
      case ELK_VEC4_OPCODE_DOUBLE_TO_D32:
      case ELK_VEC4_OPCODE_DOUBLE_TO_U32:
      case ELK_VEC4_OPCODE_TO_DOUBLE:
      case ELK_VEC4_OPCODE_PICK_LOW_32BIT:
      case ELK_VEC4_OPCODE_PICK_HIGH_32BIT:
      case ELK_VEC4_OPCODE_SET_LOW_32BIT:
      case ELK_VEC4_OPCODE_SET_HIGH_32BIT:
      case ELK_VEC4_OPCODE_ZERO_OOB_PUSH_REGS:
      case ELK_GS_OPCODE_SET_DWORD_2:
      case ELK_GS_OPCODE_SET_WRITE_OFFSET:
      case ELK_GS_OPCODE_SET_VERTEX_COUNT:
      case ELK_GS_OPCODE_PREPARE_CHANNEL_MASKS:
      case ELK_GS_OPCODE_SET_CHANNEL_MASKS:
      case ELK_GS_OPCODE_GET_INSTANCE_ID:
      case ELK_GS_OPCODE_SET_PRIMITIVE_ID:
      case ELK_GS_OPCODE_SVB_SET_DST_INDEX:
      case ELK_TCS_OPCODE_SRC0_010_IS_ZERO:
      case ELK_TCS_OPCODE_GET_PRIMITIVE_ID:
      case ELK_TES_OPCODE_GET_PRIMITIVE_ID:
      case ELK_SHADER_OPCODE_READ_SR_REG:
         if (devinfo->ver >= 8) {
            if (type_sz(info.tx) > 4)
               return calculate_desc(info, EU_UNIT_FPU, 0, 4, 0, 0, 4,
                                     0, 12, 8 /* XXX */, 16 /* XXX */, 0, 0);
            else
               return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                     0, 8, 4, 12, 0, 0);
         } else if (devinfo->verx10 >= 75) {
            return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                  0, 10, 6 /* XXX */, 16, 0, 0);
         } else {
            return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                  0, 12, 8 /* XXX */, 18, 0, 0);
         }

      case ELK_OPCODE_MOV:
      case ELK_OPCODE_CMP:
      case ELK_OPCODE_ADD:
      case ELK_OPCODE_MUL:
      case ELK_SHADER_OPCODE_MOV_RELOC_IMM:
      case ELK_VEC4_OPCODE_MOV_FOR_SCRATCH:
         if (devinfo->ver >= 8) {
            if (type_sz(info.tx) > 4)
               return calculate_desc(info, EU_UNIT_FPU, 0, 4, 0, 0, 4,
                                     0, 12, 8 /* XXX */, 16 /* XXX */, 0, 0);
            else
               return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                     0, 8, 4, 12, 0, 0);
         } else if (devinfo->verx10 >= 75) {
            if (info.tx == ELK_REGISTER_TYPE_F)
               return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                     0, 12, 8 /* XXX */, 18, 0, 0);
            else
               return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                     0, 10, 6 /* XXX */, 16, 0, 0);
         } else if (devinfo->ver >= 7) {
            if (info.tx == ELK_REGISTER_TYPE_F)
               return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                     0, 14, 10 /* XXX */, 20, 0, 0);
            else
               return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                     0, 12, 8 /* XXX */, 18, 0, 0);
         } else {
            return calculate_desc(info, EU_UNIT_FPU, 0, 2 /* XXX */, 0,
                                  0, 2 /* XXX */,
                                  0, 12 /* XXX */, 8 /* XXX */, 18 /* XXX */,
                                  0, 0);
         }

      case ELK_OPCODE_BFE:
      case ELK_OPCODE_BFI2:
      case ELK_OPCODE_CSEL:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 0, 2, 1, 0, 2,
                                  0, 8, 4 /* XXX */, 12 /* XXX */, 0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 0, 2, 1, 0, 2,
                                  0, 10, 6 /* XXX */, 16 /* XXX */, 0, 0);
         else if (devinfo->ver >= 7)
            return calculate_desc(info, EU_UNIT_FPU, 0, 2, 1, 0, 2,
                                  0, 12, 8 /* XXX */, 18 /* XXX */, 0, 0);
         else
            abort();

      case ELK_OPCODE_MAD:
         if (devinfo->ver >= 8) {
            if (type_sz(info.tx) > 4)
               return calculate_desc(info, EU_UNIT_FPU, 0, 4, 1, 0, 4,
                                     0, 12, 8 /* XXX */, 16 /* XXX */, 0, 0);
            else
               return calculate_desc(info, EU_UNIT_FPU, 0, 2, 1, 0, 2,
                                     0, 8, 4 /* XXX */, 12 /* XXX */, 0, 0);
         } else if (devinfo->verx10 >= 75) {
            if (info.tx == ELK_REGISTER_TYPE_F)
               return calculate_desc(info, EU_UNIT_FPU, 0, 2, 1, 0, 2,
                                     0, 12, 8 /* XXX */, 18, 0, 0);
            else
               return calculate_desc(info, EU_UNIT_FPU, 0, 2, 1, 0, 2,
                                     0, 10, 6 /* XXX */, 16, 0, 0);
         } else if (devinfo->ver >= 7) {
            if (info.tx == ELK_REGISTER_TYPE_F)
               return calculate_desc(info, EU_UNIT_FPU, 0, 2, 1, 0, 2,
                                     0, 14, 10 /* XXX */, 20, 0, 0);
            else
               return calculate_desc(info, EU_UNIT_FPU, 0, 2, 1, 0, 2,
                                     0, 12, 8 /* XXX */, 18, 0, 0);
         } else if (devinfo->ver >= 6) {
            return calculate_desc(info, EU_UNIT_FPU, 0, 2 /* XXX */, 1 /* XXX */,
                                  0, 2 /* XXX */,
                                  0, 12 /* XXX */, 8 /* XXX */, 18 /* XXX */,
                                  0, 0);
         } else {
            abort();
         }

      case ELK_OPCODE_F32TO16:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 0, 4, 0, 0, 4,
                                  0, 8, 4 /* XXX */, 12 /* XXX */, 0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 0, 4, 0, 0, 4,
                                  0, 10, 6 /* XXX */, 16 /* XXX */, 0, 0);
         else if (devinfo->ver >= 7)
            return calculate_desc(info, EU_UNIT_FPU, 0, 4, 0, 0, 4,
                                  0, 12, 8 /* XXX */, 18 /* XXX */, 0, 0);
         else
            abort();

      case ELK_OPCODE_DP4:
      case ELK_OPCODE_DPH:
      case ELK_OPCODE_DP3:
      case ELK_OPCODE_DP2:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                  0, 12, 8 /* XXX */, 16 /* XXX */, 0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                  0, 10, 6 /* XXX */, 16 /* XXX */, 0, 0);
         else
            return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                  0, 12, 8 /* XXX */, 18 /* XXX */, 0, 0);

      case ELK_SHADER_OPCODE_RCP:
      case ELK_SHADER_OPCODE_RSQ:
      case ELK_SHADER_OPCODE_SQRT:
      case ELK_SHADER_OPCODE_EXP2:
      case ELK_SHADER_OPCODE_LOG2:
      case ELK_SHADER_OPCODE_SIN:
      case ELK_SHADER_OPCODE_COS:
      case ELK_SHADER_OPCODE_POW:
      case ELK_SHADER_OPCODE_INT_QUOTIENT:
      case ELK_SHADER_OPCODE_INT_REMAINDER:
         if (devinfo->ver >= 6) {
            switch (info.op) {
            case ELK_SHADER_OPCODE_RCP:
            case ELK_SHADER_OPCODE_RSQ:
            case ELK_SHADER_OPCODE_SQRT:
            case ELK_SHADER_OPCODE_EXP2:
            case ELK_SHADER_OPCODE_LOG2:
            case ELK_SHADER_OPCODE_SIN:
            case ELK_SHADER_OPCODE_COS:
               if (devinfo->ver >= 8)
                  return calculate_desc(info, EU_UNIT_EM, -2, 4, 0, 0, 4,
                                        0, 16, 0, 0, 0, 0);
               else if (devinfo->verx10 >= 75)
                  return calculate_desc(info, EU_UNIT_EM, 0, 2, 0, 0, 2,
                                        0, 12, 0, 0, 0, 0);
               else
                  return calculate_desc(info, EU_UNIT_EM, 0, 2, 0, 0, 2,
                                        0, 14, 0, 0, 0, 0);

            case ELK_SHADER_OPCODE_POW:
               if (devinfo->ver >= 8)
                  return calculate_desc(info, EU_UNIT_EM, -2, 4, 0, 0, 8,
                                        0, 24, 0, 0, 0, 0);
               else if (devinfo->verx10 >= 75)
                  return calculate_desc(info, EU_UNIT_EM, 0, 2, 0, 0, 4,
                                        0, 20, 0, 0, 0, 0);
               else
                  return calculate_desc(info, EU_UNIT_EM, 0, 2, 0, 0, 4,
                                        0, 22, 0, 0, 0, 0);

            case ELK_SHADER_OPCODE_INT_QUOTIENT:
            case ELK_SHADER_OPCODE_INT_REMAINDER:
               return calculate_desc(info, EU_UNIT_EM, 2, 0, 0, 26, 0,
                                     0, 28 /* XXX */, 0, 0, 0, 0);

            default:
               abort();
            }
         } else {
            switch (info.op) {
            case ELK_SHADER_OPCODE_RCP:
               return calculate_desc(info, EU_UNIT_EM, 2, 0, 0, 0, 8,
                                     0, 22, 0, 0, 0, 8);

            case ELK_SHADER_OPCODE_RSQ:
               return calculate_desc(info, EU_UNIT_EM, 2, 0, 0, 0, 16,
                                     0, 44, 0, 0, 0, 8);

            case ELK_SHADER_OPCODE_INT_QUOTIENT:
            case ELK_SHADER_OPCODE_SQRT:
            case ELK_SHADER_OPCODE_LOG2:
               return calculate_desc(info, EU_UNIT_EM, 2, 0, 0, 0, 24,
                                     0, 66, 0, 0, 0, 8);

            case ELK_SHADER_OPCODE_INT_REMAINDER:
            case ELK_SHADER_OPCODE_EXP2:
               return calculate_desc(info, EU_UNIT_EM, 2, 0, 0, 0, 32,
                                     0, 88, 0, 0, 0, 8);

            case ELK_SHADER_OPCODE_SIN:
            case ELK_SHADER_OPCODE_COS:
               return calculate_desc(info, EU_UNIT_EM, 2, 0, 0, 0, 48,
                                     0, 132, 0, 0, 0, 8);

            case ELK_SHADER_OPCODE_POW:
               return calculate_desc(info, EU_UNIT_EM, 2, 0, 0, 0, 64,
                                     0, 176, 0, 0, 0, 8);

            default:
               abort();
            }
         }

      case ELK_OPCODE_DO:
         if (devinfo->ver >= 6)
            return calculate_desc(info, EU_UNIT_NULL, 0, 0, 0, 0, 0,
                                  0, 0, 0, 0, 0, 0);
         else
            return calculate_desc(info, EU_UNIT_NULL, 2 /* XXX */, 0, 0, 0, 0,
                                  0, 0, 0, 0, 0, 0);

      case ELK_OPCODE_IF:
      case ELK_OPCODE_ELSE:
      case ELK_OPCODE_ENDIF:
      case ELK_OPCODE_WHILE:
      case ELK_OPCODE_BREAK:
      case ELK_OPCODE_CONTINUE:
      case ELK_OPCODE_HALT:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_NULL, 8, 0, 0, 0, 0,
                                  0, 0, 0, 0, 0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_NULL, 6, 0, 0, 0, 0,
                                  0, 0, 0, 0, 0, 0);
         else
            return calculate_desc(info, EU_UNIT_NULL, 2, 0, 0, 0, 0,
                                  0, 0, 0, 0, 0, 0);

      case ELK_FS_OPCODE_LINTERP:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 0, 4, 0, 0, 4,
                                  0, 12, 8 /* XXX */, 16 /* XXX */, 0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                  0, 10, 6 /* XXX */, 16 /* XXX */, 0, 0);
         else
            return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                  0, 12, 8 /* XXX */, 18 /* XXX */, 0, 0);

      case ELK_OPCODE_LRP:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 0, 4, 1, 0, 4,
                                  0, 12, 8 /* XXX */, 16 /* XXX */, 0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 0, 2, 1, 0, 2,
                                  0, 10, 6 /* XXX */, 16 /* XXX */, 0, 0);
         else if (devinfo->ver >= 6)
            return calculate_desc(info, EU_UNIT_FPU, 0, 2, 1, 0, 2,
                                  0, 12, 8 /* XXX */, 18 /* XXX */, 0, 0);
         else
            abort();

      case ELK_FS_OPCODE_PACK_HALF_2x16_SPLIT:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 16, 6, 0, 0, 6,
                                  0, 8 /* XXX */, 4 /* XXX */,
                                  12 /* XXX */, 0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 20, 6, 0, 0, 6,
                                  0, 10 /* XXX */, 6 /* XXX */,
                                  16 /* XXX */, 0, 0);
         else if (devinfo->ver >= 7)
            return calculate_desc(info, EU_UNIT_FPU, 24, 6, 0, 0, 6,
                                  0, 12 /* XXX */, 8 /* XXX */,
                                  18 /* XXX */, 0, 0);
         else
            abort();

      case ELK_SHADER_OPCODE_MOV_INDIRECT:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 34, 0, 0, 34, 0,
                                  0, 8 /* XXX */, 4 /* XXX */,
                                  12 /* XXX */, 0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 34, 0, 0, 34, 0,
                                  0, 10 /* XXX */, 6 /* XXX */,
                                  16 /* XXX */, 0, 0);
         else
            return calculate_desc(info, EU_UNIT_FPU, 34, 0, 0, 34, 0,
                                  0, 12 /* XXX */, 8 /* XXX */,
                                  18 /* XXX */, 0, 0);

      case ELK_SHADER_OPCODE_BROADCAST:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 18, 0, 0, 4, 0,
                                  0, 8, 4 /* XXX */, 12 /* XXX */, 0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 18, 0, 0, 4, 0,
                                  0, 10, 6 /* XXX */, 16 /* XXX */, 0, 0);
         else if (devinfo->ver >= 7)
            return calculate_desc(info, EU_UNIT_FPU, 20, 0, 0, 4, 0,
                                  0, 12, 8 /* XXX */, 18 /* XXX */, 0, 0);
         else
            abort();

      case ELK_SHADER_OPCODE_FIND_LIVE_CHANNEL:
      case ELK_SHADER_OPCODE_FIND_LAST_LIVE_CHANNEL:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 2, 0, 0, 2, 0,
                                  0, 8, 4 /* XXX */, 12 /* XXX */, 0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 36, 0, 0, 6, 0,
                                  0, 10, 6 /* XXX */, 16 /* XXX */, 0, 0);
         else if (devinfo->ver >= 7)
            return calculate_desc(info, EU_UNIT_FPU, 40, 0, 0, 6, 0,
                                  0, 12, 8 /* XXX */, 18 /* XXX */, 0, 0);
         else
            abort();

      case ELK_SHADER_OPCODE_RND_MODE:
      case ELK_SHADER_OPCODE_FLOAT_CONTROL_MODE:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 20 /* XXX */, 0, 0,
                                  4 /* XXX */, 0,
                                  0, 0, 0, 0, 0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 24 /* XXX */, 0, 0,
                                  4 /* XXX */, 0,
                                  0, 0, 0, 0, 0, 0);
         else if (devinfo->ver >= 6)
            return calculate_desc(info, EU_UNIT_FPU, 28 /* XXX */, 0, 0,
                                  4 /* XXX */, 0,
                                  0, 0, 0, 0, 0, 0);
         else
            abort();

      case ELK_SHADER_OPCODE_SHUFFLE:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 42 /* XXX */, 0, 0,
                                  42 /* XXX */, 0,
                                  0, 8 /* XXX */, 4 /* XXX */,
                                  12 /* XXX */, 0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 0, 44 /* XXX */, 0,
                                  0, 44 /* XXX */,
                                  0, 10 /* XXX */, 6 /* XXX */,
                                  16 /* XXX */, 0, 0);
         else if (devinfo->ver >= 6)
            return calculate_desc(info, EU_UNIT_FPU, 0, 46 /* XXX */, 0,
                                  0, 46 /* XXX */,
                                  0, 12 /* XXX */, 8 /* XXX */,
                                  18 /* XXX */, 0, 0);
         else
            abort();

      case ELK_SHADER_OPCODE_SEL_EXEC:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 8 /* XXX */, 4 /* XXX */, 0,
                                  0, 4 /* XXX */,
                                  0, 8 /* XXX */, 4 /* XXX */,
                                  12 /* XXX */, 0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 10 /* XXX */, 4 /* XXX */, 0,
                                  0, 4 /* XXX */,
                                  0, 10 /* XXX */, 6 /* XXX */,
                                  16 /* XXX */, 0, 0);
         else
            return calculate_desc(info, EU_UNIT_FPU, 12 /* XXX */, 4 /* XXX */, 0,
                                  0, 4 /* XXX */,
                                  0, 12 /* XXX */, 8 /* XXX */,
                                  18 /* XXX */, 0, 0);

      case ELK_SHADER_OPCODE_QUAD_SWIZZLE:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 0 /* XXX */, 8 /* XXX */, 0,
                                  0, 8 /* XXX */,
                                  0, 8 /* XXX */, 4 /* XXX */,
                                  12 /* XXX */, 0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 0 /* XXX */, 8 /* XXX */, 0,
                                  0, 8 /* XXX */,
                                  0, 10 /* XXX */, 6 /* XXX */,
                                  16 /* XXX */, 0, 0);
         else
            return calculate_desc(info, EU_UNIT_FPU, 0 /* XXX */, 8 /* XXX */, 0,
                                  0, 8 /* XXX */,
                                  0, 12 /* XXX */, 8 /* XXX */,
                                  18 /* XXX */, 0, 0);

      case ELK_FS_OPCODE_DDY_FINE:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                  0, 8, 4 /* XXX */, 12 /* XXX */, 0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                  0, 12, 8 /* XXX */, 18 /* XXX */, 0, 0);
         else
            return calculate_desc(info, EU_UNIT_FPU, 0, 2, 0, 0, 2,
                                  0, 14, 10 /* XXX */, 20 /* XXX */, 0, 0);

      case ELK_FS_OPCODE_LOAD_LIVE_CHANNELS:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 0, 2 /* XXX */, 0,
                                  0, 2 /* XXX */,
                                  0, 0, 0, 8 /* XXX */, 0, 0);
         else
            abort();

      case ELK_VEC4_OPCODE_PACK_BYTES:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 4 /* XXX */, 0, 0,
                                  4 /* XXX */, 0,
                                  0, 8 /* XXX */, 4 /* XXX */, 12 /* XXX */,
                                  0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 4 /* XXX */, 0, 0,
                                  4 /* XXX */, 0,
                                  0, 10 /* XXX */, 6 /* XXX */, 16 /* XXX */,
                                  0, 0);
         else
            return calculate_desc(info, EU_UNIT_FPU, 4 /* XXX */, 0, 0,
                                  4 /* XXX */, 0,
                                  0, 12 /* XXX */, 8 /* XXX */, 18 /* XXX */,
                                  0, 0);

      case ELK_VS_OPCODE_UNPACK_FLAGS_SIMD4X2:
      case ELK_TCS_OPCODE_GET_INSTANCE_ID:
      case ELK_VEC4_TCS_OPCODE_SET_INPUT_URB_OFFSETS:
      case ELK_VEC4_TCS_OPCODE_SET_OUTPUT_URB_OFFSETS:
      case ELK_TES_OPCODE_CREATE_INPUT_READ_HEADER:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 22 /* XXX */, 0, 0,
                                  6 /* XXX */, 0,
                                  0, 8 /* XXX */, 4 /* XXX */, 12 /* XXX */,
                                  0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 26 /* XXX */, 0, 0,
                                  6 /* XXX */, 0,
                                  0, 10 /* XXX */, 6 /* XXX */, 16 /* XXX */,
                                  0, 0);
         else
            return calculate_desc(info, EU_UNIT_FPU, 30 /* XXX */, 0, 0,
                                  6 /* XXX */, 0,
                                  0, 12 /* XXX */, 8 /* XXX */, 18 /* XXX */,
                                  0, 0);

      case ELK_GS_OPCODE_FF_SYNC_SET_PRIMITIVES:
      case ELK_TCS_OPCODE_CREATE_BARRIER_HEADER:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 32 /* XXX */, 0, 0,
                                  8 /* XXX */, 0,
                                  0, 8 /* XXX */, 4 /* XXX */, 12 /* XXX */,
                                  0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 38 /* XXX */, 0, 0,
                                  8 /* XXX */, 0,
                                  0, 10 /* XXX */, 6 /* XXX */, 16 /* XXX */,
                                  0, 0);
         else if (devinfo->ver >= 6)
            return calculate_desc(info, EU_UNIT_FPU, 44 /* XXX */, 0, 0,
                                  8 /* XXX */, 0,
                                  0, 12 /* XXX */, 8 /* XXX */, 18 /* XXX */,
                                  0, 0);
         else
            abort();

      case ELK_TES_OPCODE_ADD_INDIRECT_URB_OFFSET:
         if (devinfo->ver >= 8)
            return calculate_desc(info, EU_UNIT_FPU, 12 /* XXX */, 0, 0,
                                  4 /* XXX */, 0,
                                  0, 8 /* XXX */, 4 /* XXX */, 12 /* XXX */,
                                  0, 0);
         else if (devinfo->verx10 >= 75)
            return calculate_desc(info, EU_UNIT_FPU, 14 /* XXX */, 0, 0,
                                  4 /* XXX */, 0,
                                  0, 10 /* XXX */, 6 /* XXX */, 16 /* XXX */,
                                  0, 0);
         else if (devinfo->ver >= 7)
            return calculate_desc(info, EU_UNIT_FPU, 16 /* XXX */, 0, 0,
                                  4 /* XXX */, 0,
                                  0, 12 /* XXX */, 8 /* XXX */, 18 /* XXX */,
                                  0, 0);
         else
            abort();

      case ELK_SHADER_OPCODE_TEX:
      case ELK_FS_OPCODE_TXB:
      case ELK_SHADER_OPCODE_TXD:
      case ELK_SHADER_OPCODE_TXF:
      case ELK_SHADER_OPCODE_TXF_LZ:
      case ELK_SHADER_OPCODE_TXL:
      case ELK_SHADER_OPCODE_TXL_LZ:
      case ELK_SHADER_OPCODE_TXF_CMS:
      case ELK_SHADER_OPCODE_TXF_CMS_W:
      case ELK_SHADER_OPCODE_TXF_UMS:
      case ELK_SHADER_OPCODE_TXF_MCS:
      case ELK_SHADER_OPCODE_TXS:
      case ELK_SHADER_OPCODE_LOD:
      case ELK_SHADER_OPCODE_GET_BUFFER_SIZE:
      case ELK_SHADER_OPCODE_TG4:
      case ELK_SHADER_OPCODE_TG4_OFFSET:
      case ELK_SHADER_OPCODE_SAMPLEINFO:
      case ELK_FS_OPCODE_VARYING_PULL_CONSTANT_LOAD_GFX4:
         return calculate_desc(info, EU_UNIT_SAMPLER, 2, 0, 0, 0, 16 /* XXX */,
                               8 /* XXX */, 750 /* XXX */, 0, 0,
                               2 /* XXX */, 0);

      case ELK_VEC4_OPCODE_URB_READ:
      case ELK_VEC4_VS_OPCODE_URB_WRITE:
      case ELK_VEC4_GS_OPCODE_URB_WRITE:
      case ELK_VEC4_GS_OPCODE_URB_WRITE_ALLOCATE:
      case ELK_GS_OPCODE_THREAD_END:
      case ELK_GS_OPCODE_FF_SYNC:
      case ELK_VEC4_TCS_OPCODE_URB_WRITE:
      case ELK_TCS_OPCODE_RELEASE_INPUT:
      case ELK_TCS_OPCODE_THREAD_END:
         return calculate_desc(info, EU_UNIT_URB, 2, 0, 0, 0, 6 /* XXX */,
                               32 /* XXX */, 200 /* XXX */, 0, 0, 0, 0);

      case ELK_SHADER_OPCODE_MEMORY_FENCE:
      case ELK_SHADER_OPCODE_INTERLOCK:
         switch (info.sfid) {
         case GFX6_SFID_DATAPORT_RENDER_CACHE:
            if (devinfo->ver >= 7)
               return calculate_desc(info, EU_UNIT_DP_RC, 2, 0, 0, 30 /* XXX */, 0,
                                     10 /* XXX */, 300 /* XXX */, 0, 0, 0, 0);
            else
               abort();

         case ELK_SFID_URB:
         case GFX7_SFID_DATAPORT_DATA_CACHE:
         case HSW_SFID_DATAPORT_DATA_CACHE_1:
            if (devinfo->ver >= 7)
               return calculate_desc(info, EU_UNIT_DP_DC, 2, 0, 0, 30 /* XXX */, 0,
                                     10 /* XXX */, 100 /* XXX */, 0, 0, 0, 0);
            else
               abort();

         default:
            abort();
         }

      case ELK_SHADER_OPCODE_GFX4_SCRATCH_READ:
      case ELK_SHADER_OPCODE_GFX4_SCRATCH_WRITE:
      case ELK_SHADER_OPCODE_GFX7_SCRATCH_READ:
         return calculate_desc(info, EU_UNIT_DP_DC, 2, 0, 0, 0, 8 /* XXX */,
                               10 /* XXX */, 100 /* XXX */, 0, 0, 0, 0);

      case ELK_VEC4_OPCODE_UNTYPED_ATOMIC:
         if (devinfo->ver >= 7)
            return calculate_desc(info, EU_UNIT_DP_DC, 2, 0, 0,
                                  30 /* XXX */, 400 /* XXX */,
                                  10 /* XXX */, 100 /* XXX */, 0, 0,
                                  0, 400 /* XXX */);
         else
            abort();

      case ELK_VEC4_OPCODE_UNTYPED_SURFACE_READ:
      case ELK_VEC4_OPCODE_UNTYPED_SURFACE_WRITE:
         if (devinfo->ver >= 7)
            return calculate_desc(info, EU_UNIT_DP_DC, 2, 0, 0,
                                  0, 20 /* XXX */,
                                  10 /* XXX */, 100 /* XXX */, 0, 0,
                                  0, 0);
         else
            abort();

      case ELK_FS_OPCODE_FB_WRITE:
      case ELK_FS_OPCODE_REP_FB_WRITE:
         return calculate_desc(info, EU_UNIT_DP_RC, 2, 0, 0, 0, 450 /* XXX */,
                               10 /* XXX */, 300 /* XXX */, 0, 0, 0, 0);

      case ELK_GS_OPCODE_SVB_WRITE:
         if (devinfo->ver >= 6)
            return calculate_desc(info, EU_UNIT_DP_RC, 2 /* XXX */, 0, 0,
                                  0, 450 /* XXX */,
                                  10 /* XXX */, 300 /* XXX */, 0, 0,
                                  0, 0);
         else
            abort();

      case ELK_FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD:
         return calculate_desc(info, EU_UNIT_DP_CC, 2, 0, 0, 0, 16 /* XXX */,
                               10 /* XXX */, 100 /* XXX */, 0, 0, 0, 0);

      case ELK_VS_OPCODE_PULL_CONSTANT_LOAD:
      case ELK_VS_OPCODE_PULL_CONSTANT_LOAD_GFX7:
         return calculate_desc(info, EU_UNIT_SAMPLER, 2, 0, 0, 0, 16,
                               8, 750, 0, 0, 2, 0);

      case ELK_FS_OPCODE_INTERPOLATE_AT_SAMPLE:
      case ELK_FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET:
      case ELK_FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET:
         if (devinfo->ver >= 7)
            return calculate_desc(info, EU_UNIT_PI, 2, 0, 0, 14 /* XXX */, 0,
                                  0, 90 /* XXX */, 0, 0, 0, 0);
         else
            abort();

      case ELK_SHADER_OPCODE_BARRIER:
         if (devinfo->ver >= 7)
            return calculate_desc(info, EU_UNIT_GATEWAY, 90 /* XXX */, 0, 0,
                                  0 /* XXX */, 0,
                                  0, 0, 0, 0, 0, 0);
         else
            abort();

      case ELK_CS_OPCODE_CS_TERMINATE:
         if (devinfo->ver >= 7)
            return calculate_desc(info, EU_UNIT_SPAWNER, 2, 0, 0, 0 /* XXX */, 0,
                                  10 /* XXX */, 0, 0, 0, 0, 0);
         else
            abort();

      case ELK_SHADER_OPCODE_SEND:
         switch (info.sfid) {
         case GFX6_SFID_DATAPORT_CONSTANT_CACHE:
            if (devinfo->ver >= 7) {
               /* See ELK_FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD */
               return calculate_desc(info, EU_UNIT_DP_CC, 2, 0, 0, 0, 16 /* XXX */,
                                     10 /* XXX */, 100 /* XXX */, 0, 0, 0, 0);
            } else {
               abort();
            }
         case GFX6_SFID_DATAPORT_RENDER_CACHE:
            if (devinfo->ver >= 7) {
               switch (elk_dp_desc_msg_type(devinfo, info.desc)) {
               case GFX7_DATAPORT_RC_TYPED_ATOMIC_OP:
                  return calculate_desc(info, EU_UNIT_DP_RC, 2, 0, 0,
                                        30 /* XXX */, 450 /* XXX */,
                                        10 /* XXX */, 100 /* XXX */,
                                        0, 0, 0, 400 /* XXX */);
               default:
                  return calculate_desc(info, EU_UNIT_DP_RC, 2, 0, 0,
                                        0, 450 /* XXX */,
                                        10 /* XXX */, 300 /* XXX */, 0, 0,
                                        0, 0);
               }
            } else if (devinfo->ver >= 6)  {
               return calculate_desc(info, EU_UNIT_DP_RC, 2 /* XXX */, 0, 0,
                                     0, 450 /* XXX */,
                                     10 /* XXX */, 300 /* XXX */, 0, 0, 0, 0);
            } else {
               abort();
            }
         case ELK_SFID_SAMPLER: {
            if (devinfo->ver >= 6)
               return calculate_desc(info, EU_UNIT_SAMPLER, 2, 0, 0, 0, 16,
                                     8, 750, 0, 0, 2, 0);
            else
               abort();
         }
         case GFX7_SFID_DATAPORT_DATA_CACHE:
         case HSW_SFID_DATAPORT_DATA_CACHE_1:
            if (devinfo->verx10 >= 75) {
               switch (elk_dp_desc_msg_type(devinfo, info.desc)) {
               case HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP:
               case HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP_SIMD4X2:
               case HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP_SIMD4X2:
               case HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP:
                  return calculate_desc(info, EU_UNIT_DP_DC, 2, 0, 0,
                                        30 /* XXX */, 400 /* XXX */,
                                        10 /* XXX */, 100 /* XXX */, 0, 0,
                                        0, 400 /* XXX */);

               default:
                  return calculate_desc(info, EU_UNIT_DP_DC, 2, 0, 0,
                                        0, 20 /* XXX */,
                                        10 /* XXX */, 100 /* XXX */, 0, 0,
                                        0, 0);
               }
            } else if (devinfo->ver >= 7) {
               switch (elk_dp_desc_msg_type(devinfo, info.desc)) {
               case GFX7_DATAPORT_DC_UNTYPED_ATOMIC_OP:
                  return calculate_desc(info, EU_UNIT_DP_DC, 2, 0, 0,
                                        30 /* XXX */, 400 /* XXX */,
                                        10 /* XXX */, 100 /* XXX */,
                                        0, 0, 0, 400 /* XXX */);
               default:
                  return calculate_desc(info, EU_UNIT_DP_DC, 2, 0, 0,
                                        0, 20 /* XXX */,
                                        10 /* XXX */, 100 /* XXX */, 0, 0,
                                        0, 0);
               }
            } else {
               abort();
            }

         case GFX7_SFID_PIXEL_INTERPOLATOR:
            if (devinfo->ver >= 7)
               return calculate_desc(info, EU_UNIT_PI, 2, 0, 0, 14 /* XXX */, 0,
                                     0, 90 /* XXX */, 0, 0, 0, 0);
            else
               abort();

         case ELK_SFID_URB:
            return calculate_desc(info, EU_UNIT_URB, 2, 0, 0, 0, 6 /* XXX */,
                                  32 /* XXX */, 200 /* XXX */, 0, 0, 0, 0);

         default:
            abort();
         }

      case ELK_SHADER_OPCODE_UNDEF:
      case ELK_SHADER_OPCODE_HALT_TARGET:
      case ELK_FS_OPCODE_SCHEDULING_FENCE:
         return calculate_desc(info, EU_UNIT_NULL, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0, 0);

      default:
         abort();
      }
   }

   /**
    * Model the performance behavior of a stall on the specified dependency
    * ID.
    */
   void
   stall_on_dependency(state &st, enum intel_eu_dependency_id id)
   {
      if (id < ARRAY_SIZE(st.dep_ready))
         st.unit_ready[EU_UNIT_FE] = MAX2(st.unit_ready[EU_UNIT_FE],
                                       st.dep_ready[id]);
   }

   /**
    * Model the performance behavior of the front-end and back-end while
    * executing an instruction with the specified timing information, assuming
    * all dependencies are already clear.
    */
   void
   execute_instruction(state &st, const perf_desc &perf)
   {
      /* Compute the time at which the front-end will be ready to execute the
       * next instruction.
       */
      st.unit_ready[EU_UNIT_FE] += perf.df;

      if (perf.u < EU_NUM_UNITS) {
         /* Wait for the back-end to be ready to execute this instruction. */
         st.unit_ready[EU_UNIT_FE] = MAX2(st.unit_ready[EU_UNIT_FE],
                                       st.unit_ready[perf.u]);

         /* Compute the time at which the back-end will be ready to execute
          * the next instruction, and update the back-end utilization.
          */
         st.unit_ready[perf.u] = st.unit_ready[EU_UNIT_FE] + perf.db;
         st.unit_busy[perf.u] += perf.db * st.weight;
      }
   }

   /**
    * Model the performance behavior of a read dependency provided by an
    * instruction.
    */
   void
   mark_read_dependency(state &st, const perf_desc &perf,
                        enum intel_eu_dependency_id id)
   {
      if (id < ARRAY_SIZE(st.dep_ready))
         st.dep_ready[id] = st.unit_ready[EU_UNIT_FE] + perf.ls;
   }

   /**
    * Model the performance behavior of a write dependency provided by an
    * instruction.
    */
   void
   mark_write_dependency(state &st, const perf_desc &perf,
                         enum intel_eu_dependency_id id)
   {
      if (id >= EU_DEPENDENCY_ID_ACCUM0 && id < EU_DEPENDENCY_ID_FLAG0)
         st.dep_ready[id] = st.unit_ready[EU_UNIT_FE] + perf.la;
      else if (id >= EU_DEPENDENCY_ID_FLAG0 && id < EU_DEPENDENCY_ID_SBID_WR0)
         st.dep_ready[id] = st.unit_ready[EU_UNIT_FE] + perf.lf;
      else if (id < ARRAY_SIZE(st.dep_ready))
         st.dep_ready[id] = st.unit_ready[EU_UNIT_FE] + perf.ld;
   }

   /**
    * Return the dependency ID of a elk_backend_reg, offset by \p delta GRFs.
    */
   enum intel_eu_dependency_id
   reg_dependency_id(const intel_device_info *devinfo, const elk_backend_reg &r,
                     const int delta)
   {
      if (r.file == VGRF) {
         const unsigned i = r.nr + r.offset / REG_SIZE + delta;
         assert(i < EU_DEPENDENCY_ID_MRF0 - EU_DEPENDENCY_ID_GRF0);
         return intel_eu_dependency_id(EU_DEPENDENCY_ID_GRF0 + i);

      } else if (r.file == FIXED_GRF) {
         const unsigned i = r.nr + delta;
         assert(i < EU_DEPENDENCY_ID_MRF0 - EU_DEPENDENCY_ID_GRF0);
         return intel_eu_dependency_id(EU_DEPENDENCY_ID_GRF0 + i);

      } else if (r.file == MRF && devinfo->ver >= 7) {
         const unsigned i = GFX7_MRF_HACK_START +
                            r.nr + r.offset / REG_SIZE + delta;
         assert(i < EU_DEPENDENCY_ID_MRF0 - EU_DEPENDENCY_ID_GRF0);
         return intel_eu_dependency_id(EU_DEPENDENCY_ID_GRF0 + i);

      } else if (r.file == MRF && devinfo->ver < 7) {
         const unsigned i = (r.nr & ~ELK_MRF_COMPR4) +
                            r.offset / REG_SIZE + delta;
         assert(i < EU_DEPENDENCY_ID_ADDR0 - EU_DEPENDENCY_ID_MRF0);
         return intel_eu_dependency_id(EU_DEPENDENCY_ID_MRF0 + i);

      } else if (r.file == ARF && r.nr >= ELK_ARF_ADDRESS &&
                 r.nr < ELK_ARF_ACCUMULATOR) {
         assert(delta == 0);
         return EU_DEPENDENCY_ID_ADDR0;

      } else if (r.file == ARF && r.nr >= ELK_ARF_ACCUMULATOR &&
                 r.nr < ELK_ARF_FLAG) {
         const unsigned i = r.nr - ELK_ARF_ACCUMULATOR + delta;
         assert(i < EU_DEPENDENCY_ID_FLAG0 - EU_DEPENDENCY_ID_ACCUM0);
         return intel_eu_dependency_id(EU_DEPENDENCY_ID_ACCUM0 + i);

      } else {
         return EU_NUM_DEPENDENCY_IDS;
      }
   }

   /**
    * Return the dependency ID of flag register starting at offset \p i.
    */
   enum intel_eu_dependency_id
   flag_dependency_id(unsigned i)
   {
      assert(i < EU_DEPENDENCY_ID_SBID_WR0 - EU_DEPENDENCY_ID_FLAG0);
      return intel_eu_dependency_id(EU_DEPENDENCY_ID_FLAG0 + i);
   }

   /**
    * Return the implicit accumulator register accessed by channel \p i of the
    * instruction.
    */
   unsigned
   accum_reg_of_channel(const intel_device_info *devinfo,
                        const elk_backend_instruction *inst,
                        elk_reg_type tx, unsigned i)
   {
      assert(inst->reads_accumulator_implicitly() ||
             inst->writes_accumulator_implicitly(devinfo));
      const unsigned offset = (inst->group + i) * type_sz(tx) *
         (devinfo->ver < 7 || elk_reg_type_is_floating_point(tx) ? 1 : 2);
      return offset / (reg_unit(devinfo) * REG_SIZE) % 2;
   }

   /**
    * Model the performance behavior of an FS back-end instruction.
    */
   void
   issue_fs_inst(state &st, const struct elk_isa_info *isa,
                 const elk_backend_instruction *be_inst)
   {
      const struct intel_device_info *devinfo = isa->devinfo;
      const elk_fs_inst *inst = static_cast<const elk_fs_inst *>(be_inst);
      const instruction_info info(isa, inst);
      const perf_desc perf = instruction_desc(info);

      /* Stall on any source dependencies. */
      for (unsigned i = 0; i < inst->sources; i++) {
         for (unsigned j = 0; j < regs_read(inst, i); j++)
            stall_on_dependency(
               st, reg_dependency_id(devinfo, inst->src[i], j));
      }

      if (inst->reads_accumulator_implicitly()) {
         for (unsigned j = accum_reg_of_channel(devinfo, inst, info.tx, 0);
              j <= accum_reg_of_channel(devinfo, inst, info.tx,
                                        inst->exec_size - 1); j++)
            stall_on_dependency(
               st, reg_dependency_id(devinfo, elk_acc_reg(8), j));
      }

      if (is_send(inst) && inst->base_mrf != -1) {
         for (unsigned j = 0; j < inst->mlen; j++)
            stall_on_dependency(
               st, reg_dependency_id(
                  devinfo, elk_uvec_mrf(8, inst->base_mrf, 0), j));
      }

      if (const unsigned mask = inst->flags_read(devinfo)) {
         for (unsigned i = 0; i < sizeof(mask) * CHAR_BIT; i++) {
            if (mask & (1 << i))
               stall_on_dependency(st, flag_dependency_id(i));
         }
      }

      /* Stall on any write dependencies. */
      if (!inst->no_dd_check) {
         if (inst->dst.file != BAD_FILE && !inst->dst.is_null()) {
            for (unsigned j = 0; j < regs_written(inst); j++)
               stall_on_dependency(
                  st, reg_dependency_id(devinfo, inst->dst, j));
         }

         if (inst->writes_accumulator_implicitly(devinfo)) {
            for (unsigned j = accum_reg_of_channel(devinfo, inst, info.tx, 0);
                 j <= accum_reg_of_channel(devinfo, inst, info.tx,
                                           inst->exec_size - 1); j++)
               stall_on_dependency(
                  st, reg_dependency_id(devinfo, elk_acc_reg(8), j));
         }

         if (const unsigned mask = inst->flags_written(devinfo)) {
            for (unsigned i = 0; i < sizeof(mask) * CHAR_BIT; i++) {
               if (mask & (1 << i))
                  stall_on_dependency(st, flag_dependency_id(i));
            }
         }
      }

      /* Execute the instruction. */
      execute_instruction(st, perf);

      /* Mark any source dependencies. */
      if (inst->is_send_from_grf()) {
         for (unsigned i = 0; i < inst->sources; i++) {
            if (inst->is_payload(i)) {
               for (unsigned j = 0; j < regs_read(inst, i); j++)
                  mark_read_dependency(
                     st, perf, reg_dependency_id(devinfo, inst->src[i], j));
            }
         }
      }

      if (is_send(inst) && inst->base_mrf != -1) {
         for (unsigned j = 0; j < inst->mlen; j++)
            mark_read_dependency(st, perf,
               reg_dependency_id(devinfo, elk_uvec_mrf(8, inst->base_mrf, 0), j));
      }

      /* Mark any destination dependencies. */
      if (inst->dst.file != BAD_FILE && !inst->dst.is_null()) {
         for (unsigned j = 0; j < regs_written(inst); j++) {
            mark_write_dependency(st, perf,
                                  reg_dependency_id(devinfo, inst->dst, j));
         }
      }

      if (inst->writes_accumulator_implicitly(devinfo)) {
         for (unsigned j = accum_reg_of_channel(devinfo, inst, info.tx, 0);
              j <= accum_reg_of_channel(devinfo, inst, info.tx,
                                        inst->exec_size - 1); j++)
            mark_write_dependency(st, perf,
                                  reg_dependency_id(devinfo, elk_acc_reg(8), j));
      }

      if (const unsigned mask = inst->flags_written(devinfo)) {
         for (unsigned i = 0; i < sizeof(mask) * CHAR_BIT; i++) {
            if (mask & (1 << i))
               mark_write_dependency(st, perf, flag_dependency_id(i));
         }
      }
   }

   /**
    * Model the performance behavior of a VEC4 back-end instruction.
    */
   void
   issue_vec4_instruction(state &st, const struct elk_isa_info *isa,
                          const elk_backend_instruction *be_inst)
   {
      const struct intel_device_info *devinfo = isa->devinfo;
      const vec4_instruction *inst =
         static_cast<const vec4_instruction *>(be_inst);
      const instruction_info info(isa, inst);
      const perf_desc perf = instruction_desc(info);

      /* Stall on any source dependencies. */
      for (unsigned i = 0; i < ARRAY_SIZE(inst->src); i++) {
         for (unsigned j = 0; j < regs_read(inst, i); j++)
            stall_on_dependency(
               st, reg_dependency_id(devinfo, inst->src[i], j));
      }

      if (inst->reads_accumulator_implicitly()) {
         for (unsigned j = accum_reg_of_channel(devinfo, inst, info.tx, 0);
              j <= accum_reg_of_channel(devinfo, inst, info.tx,
                                        inst->exec_size - 1); j++)
            stall_on_dependency(
               st, reg_dependency_id(devinfo, elk_acc_reg(8), j));
      }

      if (inst->base_mrf != -1) {
         for (unsigned j = 0; j < inst->mlen; j++)
            stall_on_dependency(
               st, reg_dependency_id(
                  devinfo, elk_uvec_mrf(8, inst->base_mrf, 0), j));
      }

      if (inst->reads_flag())
         stall_on_dependency(st, EU_DEPENDENCY_ID_FLAG0);

      /* Stall on any write dependencies. */
      if (!inst->no_dd_check) {
         if (inst->dst.file != BAD_FILE && !inst->dst.is_null()) {
            for (unsigned j = 0; j < regs_written(inst); j++)
               stall_on_dependency(
                  st, reg_dependency_id(devinfo, inst->dst, j));
         }

         if (inst->writes_accumulator_implicitly(devinfo)) {
            for (unsigned j = accum_reg_of_channel(devinfo, inst, info.tx, 0);
                 j <= accum_reg_of_channel(devinfo, inst, info.tx,
                                           inst->exec_size - 1); j++)
               stall_on_dependency(
                  st, reg_dependency_id(devinfo, elk_acc_reg(8), j));
         }

         if (inst->writes_flag(devinfo))
            stall_on_dependency(st, EU_DEPENDENCY_ID_FLAG0);
      }

      /* Execute the instruction. */
      execute_instruction(st, perf);

      /* Mark any source dependencies. */
      if (inst->is_send_from_grf()) {
         for (unsigned i = 0; i < ARRAY_SIZE(inst->src); i++) {
            for (unsigned j = 0; j < regs_read(inst, i); j++)
               mark_read_dependency(
                  st, perf, reg_dependency_id(devinfo, inst->src[i], j));
         }
      }

      if (inst->base_mrf != -1) {
         for (unsigned j = 0; j < inst->mlen; j++)
            mark_read_dependency(st, perf,
               reg_dependency_id(devinfo, elk_uvec_mrf(8, inst->base_mrf, 0), j));
      }

      /* Mark any destination dependencies. */
      if (inst->dst.file != BAD_FILE && !inst->dst.is_null()) {
         for (unsigned j = 0; j < regs_written(inst); j++) {
            mark_write_dependency(st, perf,
                                  reg_dependency_id(devinfo, inst->dst, j));
         }
      }

      if (inst->writes_accumulator_implicitly(devinfo)) {
         for (unsigned j = accum_reg_of_channel(devinfo, inst, info.tx, 0);
              j <= accum_reg_of_channel(devinfo, inst, info.tx,
                                        inst->exec_size - 1); j++)
            mark_write_dependency(st, perf,
                                  reg_dependency_id(devinfo, elk_acc_reg(8), j));
      }

      if (inst->writes_flag(devinfo))
         mark_write_dependency(st, perf, EU_DEPENDENCY_ID_FLAG0);
   }

   /**
    * Calculate the maximum possible throughput of the program compatible with
    * the cycle-count utilization estimated for each asynchronous unit, in
    * threads-per-cycle units.
    */
   float
   calculate_thread_throughput(const state &st, float busy)
   {
      for (unsigned i = 0; i < EU_NUM_UNITS; i++)
         busy = MAX2(busy, st.unit_busy[i]);

      return 1.0 / busy;
   }

   /**
    * Estimate the performance of the specified shader.
    */
   void
   calculate_performance(performance &p, const elk_backend_shader *s,
                         void (*issue_instruction)(
                            state &, const struct elk_isa_info *,
                            const elk_backend_instruction *),
                         unsigned dispatch_width)
   {
      /* XXX - Note that the previous version of this code used worst-case
       *       scenario estimation of branching divergence for SIMD32 shaders,
       *       but this heuristic was removed to improve performance in common
       *       scenarios. Wider shader variants are less optimal when divergence
       *       is high, e.g. when application renders complex scene on a small
       *       surface. It is assumed that such renders are short, so their
       *       time doesn't matter and when it comes to the overall performance,
       *       they are dominated by more optimal larger renders.
       *
       *       It's possible that we could do better with divergence analysis
       *       by isolating branches which are 100% uniform.
       *
       *       Plumbing the trip counts from NIR loop analysis would allow us
       *       to do a better job regarding the loop weights.
       *
       *       In the meantime use values that roughly match the control flow
       *       weights used elsewhere in the compiler back-end.
       */
      const float discard_weight = 1.0;
      const float loop_weight = 10;
      unsigned halt_count = 0;
      unsigned elapsed = 0;
      state st;

      foreach_block(block, s->cfg) {
         const unsigned elapsed0 = elapsed;

         foreach_inst_in_block(elk_backend_instruction, inst, block) {
            const unsigned clock0 = st.unit_ready[EU_UNIT_FE];

            issue_instruction(st, &s->compiler->isa, inst);

            if (inst->opcode == ELK_SHADER_OPCODE_HALT_TARGET && halt_count)
               st.weight /= discard_weight;

            elapsed += (st.unit_ready[EU_UNIT_FE] - clock0) * st.weight;

            if (inst->opcode == ELK_OPCODE_DO)
               st.weight *= loop_weight;
            else if (inst->opcode == ELK_OPCODE_WHILE)
               st.weight /= loop_weight;
            else if (inst->opcode == ELK_OPCODE_HALT && !halt_count++)
               st.weight *= discard_weight;
         }

         p.block_latency[block->num] = elapsed - elapsed0;
      }

      p.latency = elapsed;
      p.throughput = dispatch_width * calculate_thread_throughput(st, elapsed);
   }
}

elk::performance::performance(const elk_fs_visitor *v) :
   block_latency(new unsigned[v->cfg->num_blocks])
{
   calculate_performance(*this, v, issue_fs_inst, v->dispatch_width);
}

elk::performance::performance(const vec4_visitor *v) :
   block_latency(new unsigned[v->cfg->num_blocks])
{
   calculate_performance(*this, v, issue_vec4_instruction, 8);
}

elk::performance::~performance()
{
   delete[] block_latency;
}
