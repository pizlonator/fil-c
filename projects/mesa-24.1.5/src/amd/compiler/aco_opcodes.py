#
# Copyright (c) 2018 Valve Corporation
#
# SPDX-License-Identifier: MIT

# Class that represents all the information we have about the opcode
# NOTE: this must be kept in sync with aco_op_info

import sys
import itertools
import collections
from enum import Enum, IntEnum, auto
from collections import namedtuple

class InstrClass(Enum):
   Valu32 = "valu32"
   ValuConvert32 = "valu_convert32"
   Valu64 = "valu64"
   ValuQuarterRate32 = "valu_quarter_rate32"
   ValuFma = "valu_fma"
   ValuTranscendental32 = "valu_transcendental32"
   ValuDouble = "valu_double"
   ValuDoubleAdd = "valu_double_add"
   ValuDoubleConvert = "valu_double_convert"
   ValuDoubleTranscendental = "valu_double_transcendental"
   WMMA = "wmma"
   Salu = "salu"
   SMem = "smem"
   Barrier = "barrier"
   Branch = "branch"
   Sendmsg = "sendmsg"
   DS = "ds"
   Export = "exp"
   VMem = "vmem"
   Waitcnt = "waitcnt"
   Other = "other"

# Representation of the instruction's microcode encoding format
# Note: Some Vector ALU Formats can be combined, such that:
# - VOP2* | VOP3 represents a VOP2 instruction in VOP3 encoding
# - VOP2* | DPP represents a VOP2 instruction with data parallel primitive.
# - VOP2* | SDWA represents a VOP2 instruction with sub-dword addressing.
#
# (*) The same is applicable for VOP1 and VOPC instructions.
class Format(IntEnum):
   # Pseudo Instruction Formats
   PSEUDO = 0
   PSEUDO_BRANCH = auto()
   PSEUDO_BARRIER = auto()
   PSEUDO_REDUCTION = auto()
   # Scalar ALU & Control Formats
   SOP1 = auto()
   SOP2 = auto()
   SOPK = auto()
   SOPP = auto()
   SOPC = auto()
   # Scalar Memory Format
   SMEM = auto()
   # LDS/GDS Format
   DS = auto()
   LDSDIR = auto()
   # Vector Memory Buffer Formats
   MTBUF = auto()
   MUBUF = auto()
   # Vector Memory Image Format
   MIMG = auto()
   # Export Format
   EXP = auto()
   # Flat Formats
   FLAT = auto()
   GLOBAL = auto()
   SCRATCH = auto()
   # Vector Parameter Interpolation Formats
   VINTRP = auto()
   # Vector ALU Formats
   VINTERP_INREG = auto()
   VOPD = auto()
   VOP1 = 1 << 7
   VOP2 = 1 << 8
   VOPC = 1 << 9
   VOP3 = 1 << 10
   VOP3P = 1 << 11
   SDWA = 1 << 12
   DPP16 = 1 << 13
   DPP8 = 1 << 14

   def get_accessor(self):
      if self in [Format.VOP3, Format.VOP3P]:
         return "valu"
      elif self in [Format.SOPP, Format.SOPK]:
         return "salu"
      elif self in [Format.FLAT, Format.GLOBAL, Format.SCRATCH]:
         return "flatlike"
      elif self in [Format.PSEUDO_BRANCH, Format.PSEUDO_REDUCTION, Format.PSEUDO_BARRIER]:
         return self.name.split("_")[-1].lower()
      else:
         return self.name.lower()

   def get_builder_fields(self):
      if self == Format.SOPK:
         return [('uint32_t', 'imm', '0')]
      elif self == Format.SOPP:
         return [('uint32_t', 'imm', '0')]
      elif self == Format.SMEM:
         return [('memory_sync_info', 'sync', 'memory_sync_info()'),
                 ('bool', 'glc', 'false'),
                 ('bool', 'dlc', 'false'),
                 ('bool', 'nv', 'false')]
      elif self == Format.DS:
         return [('uint16_t', 'offset0', '0'),
                 ('uint8_t', 'offset1', '0'),
                 ('bool', 'gds', 'false')]
      elif self == Format.LDSDIR:
         return [('uint8_t', 'attr', 0),
                 ('uint8_t', 'attr_chan', 0),
                 ('memory_sync_info', 'sync', 'memory_sync_info()'),
                 ('uint8_t', 'wait_vdst', 15)]
      elif self == Format.MTBUF:
         return [('unsigned', 'dfmt', None),
                 ('unsigned', 'nfmt', None),
                 ('unsigned', 'offset', None),
                 ('bool', 'offen', None),
                 ('bool', 'idxen', 'false'),
                 ('bool', 'disable_wqm', 'false'),
                 ('bool', 'glc', 'false'),
                 ('bool', 'dlc', 'false'),
                 ('bool', 'slc', 'false'),
                 ('bool', 'tfe', 'false')]
      elif self == Format.MUBUF:
         return [('unsigned', 'offset', None),
                 ('bool', 'offen', None),
                 ('bool', 'swizzled', 'false'),
                 ('bool', 'idxen', 'false'),
                 ('bool', 'addr64', 'false'),
                 ('bool', 'disable_wqm', 'false'),
                 ('bool', 'glc', 'false'),
                 ('bool', 'dlc', 'false'),
                 ('bool', 'slc', 'false'),
                 ('bool', 'tfe', 'false'),
                 ('bool', 'lds', 'false')]
      elif self == Format.MIMG:
         return [('unsigned', 'dmask', '0xF'),
                 ('bool', 'da', 'false'),
                 ('bool', 'unrm', 'false'),
                 ('bool', 'disable_wqm', 'false'),
                 ('bool', 'glc', 'false'),
                 ('bool', 'dlc', 'false'),
                 ('bool', 'slc', 'false'),
                 ('bool', 'tfe', 'false'),
                 ('bool', 'lwe', 'false'),
                 ('bool', 'r128', 'false'),
                 ('bool', 'a16', 'false'),
                 ('bool', 'd16', 'false')]
         return [('unsigned', 'attribute', None),
                 ('unsigned', 'component', None)]
      elif self == Format.EXP:
         return [('unsigned', 'enabled_mask', None),
                 ('unsigned', 'dest', None),
                 ('bool', 'compr', 'false', 'compressed'),
                 ('bool', 'done', 'false'),
                 ('bool', 'vm', 'false', 'valid_mask')]
      elif self == Format.PSEUDO_BRANCH:
         return [('uint32_t', 'target0', '0', 'target[0]'),
                 ('uint32_t', 'target1', '0', 'target[1]')]
      elif self == Format.PSEUDO_REDUCTION:
         return [('ReduceOp', 'op', None, 'reduce_op'),
                 ('unsigned', 'cluster_size', '0')]
      elif self == Format.PSEUDO_BARRIER:
         return [('memory_sync_info', 'sync', None),
                 ('sync_scope', 'exec_scope', 'scope_invocation')]
      elif self == Format.VINTRP:
         return [('unsigned', 'attribute', None),
                 ('unsigned', 'component', None),
                 ('bool', 'high_16bits', 'false')]
      elif self == Format.DPP16:
         return [('uint16_t', 'dpp_ctrl', None),
                 ('uint8_t', 'row_mask', '0xF'),
                 ('uint8_t', 'bank_mask', '0xF'),
                 ('bool', 'bound_ctrl', 'true'),
                 ('bool', 'fetch_inactive', 'true')]
      elif self == Format.DPP8:
         return [('uint32_t', 'lane_sel', 0),
                 ('bool', 'fetch_inactive', 'true')]
      elif self == Format.VOP3P:
         return [('uint8_t', 'opsel_lo', None),
                 ('uint8_t', 'opsel_hi', None)]
      elif self == Format.VOPD:
         return [('aco_opcode', 'opy', None)]
      elif self == Format.VINTERP_INREG:
         return [('uint8_t', 'opsel', 0),
                 ('unsigned', 'wait_exp', 7)]
      elif self in [Format.FLAT, Format.GLOBAL, Format.SCRATCH]:
         return [('int16_t', 'offset', 0),
                 ('memory_sync_info', 'sync', 'memory_sync_info()'),
                 ('bool', 'glc', 'false'),
                 ('bool', 'slc', 'false'),
                 ('bool', 'lds', 'false'),
                 ('bool', 'nv', 'false')]
      else:
         return []

   def get_builder_field_names(self):
      return [f[1] for f in self.get_builder_fields()]

   def get_builder_field_dests(self):
      return [(f[3] if len(f) >= 4 else f[1]) for f in self.get_builder_fields()]

   def get_builder_field_decls(self):
      return [('%s %s=%s' % (f[0], f[1], f[2]) if f[2] != None else '%s %s' % (f[0], f[1])) for f in self.get_builder_fields()]

   def get_builder_initialization(self, num_operands):
      res = ''
      if self == Format.SDWA:
         for i in range(min(num_operands, 2)):
            res += 'instr->sdwa().sel[{0}] = SubdwordSel(op{0}.op.bytes(), 0, false);'.format(i)
         res += 'instr->sdwa().dst_sel = SubdwordSel(def0.bytes(), 0, false);\n'
      elif self == Format.DPP16:
         res += 'instr->dpp16().fetch_inactive &= program->gfx_level >= GFX10;\n'
      elif self == Format.DPP8:
         res += 'instr->dpp8().fetch_inactive &= program->gfx_level >= GFX10;\n'
      return res


Opcode = namedtuple('Opcode', ['gfx6', 'gfx7', 'gfx8', 'gfx9', 'gfx10', 'gfx11'])
# namedtuple 'defaults' keyword requires python 3.7+. Use an equivalent construct
# to support older versions.
Opcode.__new__.__defaults__=(-1, -1, -1, -1, -1, -1)

class Instruction(object):
   """Class that represents all the information we have about the opcode
   NOTE: this must be kept in sync with aco_op_info
   """
   def __init__(self, name, opcode, format, input_mod, output_mod, is_atomic, cls, definitions, operands):
      assert isinstance(name, str)
      assert isinstance(opcode, Opcode)
      assert isinstance(format, Format)
      assert isinstance(input_mod, bool)
      assert isinstance(output_mod, bool)
      assert isinstance(definitions, int)
      assert isinstance(operands, int)
      assert opcode.gfx6 == -1 or opcode.gfx7 == -1 or opcode.gfx6 == opcode.gfx7
      assert opcode.gfx8 == -1 or opcode.gfx9 == -1 or opcode.gfx8 == opcode.gfx9

      self.name = name
      self.op = opcode
      self.input_mod = "1" if input_mod else "0"
      self.output_mod = "1" if output_mod else "0"
      self.is_atomic = "1" if is_atomic else "0"
      self.format = format
      self.cls = cls
      self.definitions = definitions
      self.operands = operands

      parts = name.replace('_e64', '').rsplit('_', 2)
      op_dtype = parts[-1]

      op_dtype_sizes = {'{}{}'.format(prefix, size) : size for prefix in 'biuf' for size in [64, 32, 24, 16]}
      # inline constants are 32-bit for 16-bit integer/typeless instructions: https://reviews.llvm.org/D81841
      op_dtype_sizes['b16'] = 32
      op_dtype_sizes['i16'] = 32
      op_dtype_sizes['u16'] = 32

      # If we can't tell the operand size, default to 32.
      self.operand_size = op_dtype_sizes.get(op_dtype, 32)

      # exceptions for operands:
      if 'qsad_' in name:
        self.operand_size = 0
      elif 'sad_' in name:
        self.operand_size = 32
      elif name in ['v_mad_u64_u32', 'v_mad_i64_i32']:
        self.operand_size = 0
      elif self.operand_size == 24:
        self.operand_size = 32
      elif op_dtype == 'u8' or op_dtype == 'i8':
        self.operand_size = 32
      elif name in ['v_cvt_f32_ubyte0', 'v_cvt_f32_ubyte1',
                    'v_cvt_f32_ubyte2', 'v_cvt_f32_ubyte3']:
        self.operand_size = 32


# Matches PhysReg
VCC = 106
M0 = 124
EXEC_LO = 126
EXEC = 127 # Some instructins only write lo, so use exec_hi encoding here
SCC = 253

def src(op1 = 0, op2 = 0, op3 = 0, op4 = 0):
   return op1 | (op2 << 8) | (op3 << 16) | (op4 << 24)

def dst(def1 = 0, def2 = 0, def3 = 0, def4 = 0):
   return def1 | (def2 << 8) | (def3 << 16) | (def4 << 24)

def op(*args, **kwargs):
   enc = [None] * len(Opcode._fields)

   if len(args) > 0:
      assert(len(args) == 1)
      enc[0] = args[0]

   for gen, val in kwargs.items():
      idx = Opcode._fields.index(gen)
      enc[idx] = val

   for i in range(len(enc)):
      if enc[i] == None:
         enc[i] = enc[i - 1] if i > 0 else -1

   return Opcode(*enc)

# global dictionary of instructions
instructions = {}

def insn(name, opcode = Opcode(), format = Format.PSEUDO, cls = InstrClass.Other, input_mod = False, output_mod = False, is_atomic = False, definitions = 0, operands = 0):
   assert name not in instructions
   instructions[name] = Instruction(name, opcode, format, input_mod, output_mod, is_atomic, cls, definitions, operands)

def default_class(instructions, cls):
   for i in instructions:
      if isinstance(i[-1], InstrClass):
         yield i
      else:
         yield i + (cls,)

insn("exp", op(0), format = Format.EXP, cls = InstrClass.Export)
insn("p_parallelcopy")
insn("p_startpgm")
insn("p_return")
insn("p_phi")
insn("p_linear_phi")
insn("p_as_uniform")
insn("p_unit_test")

insn("p_create_vector")
insn("p_extract_vector")
insn("p_split_vector")

# start/end the parts where we can use exec based instructions
# implicitly
insn("p_logical_start")
insn("p_logical_end")

# e.g. subgroupMin() in SPIR-V
insn("p_reduce", format=Format.PSEUDO_REDUCTION)
# e.g. subgroupInclusiveMin()
insn("p_inclusive_scan", format=Format.PSEUDO_REDUCTION)
# e.g. subgroupExclusiveMin()
insn("p_exclusive_scan", format=Format.PSEUDO_REDUCTION)

insn("p_branch", format=Format.PSEUDO_BRANCH)
insn("p_cbranch", format=Format.PSEUDO_BRANCH)
insn("p_cbranch_z", format=Format.PSEUDO_BRANCH)
insn("p_cbranch_nz", format=Format.PSEUDO_BRANCH)

insn("p_barrier", format=Format.PSEUDO_BARRIER)

# Primitive Ordered Pixel Shading pseudo-instructions.

# For querying whether the current wave can enter the ordered section on GFX9-10.3, doing
# s_add_i32(pops_exiting_wave_id, op0), but in a way that it's different from a usual SALU
# instruction so that it's easier to maintain the volatility of pops_exiting_wave_id and to handle
# the polling specially in scheduling.
# Definitions:
# - Result SGPR;
# - Clobbered SCC.
# Operands:
# - s1 value to add, usually -(current_wave_ID + 1) (or ~current_wave_ID) to remap the exiting wave
#   ID from wrapping [0, 0x3FF] to monotonic [0, 0xFFFFFFFF].
insn("p_pops_gfx9_add_exiting_wave_id")

# Indicates that the wait for the completion of the ordered section in overlapped waves has been
# finished on GFX9-10.3. Not lowered to any hardware instructions.
insn("p_pops_gfx9_overlapped_wave_wait_done")

# Indicates that a POPS ordered section has ended, hints that overlapping waves can possibly
# continue execution. The overlapping waves may actually be resumed by this instruction or anywhere
# later, however, especially taking into account the fact that there can be multiple ordered
# sections in a wave (for instance, if one is chosen in divergent control flow in the source
# shader), thus multiple p_pops_gfx9_ordered_section_done instructions. At least one must be present
# in the program if POPS is used, however, otherwise the location of the end of the ordered section
# will be undefined. Only needed on GFX9-10.3 (GFX11+ ordered section is until the last export,
# can't be exited early). Not lowered to any hardware instructions.
insn("p_pops_gfx9_ordered_section_done")

insn("p_spill")
insn("p_reload")

# Start/end linear vgprs. p_start_linear_vgpr can take an operand to copy from, into the linear vgpr
insn("p_start_linear_vgpr")
insn("p_end_linear_vgpr")

insn("p_end_wqm")
insn("p_discard_if")
insn("p_demote_to_helper")
insn("p_is_helper")
insn("p_exit_early_if")

# simulates proper bpermute behavior using v_readlane_b32
# definitions: result VGPR, temp EXEC, clobbered VCC
# operands: index, input data
insn("p_bpermute_readlane")

# simulates proper wave64 bpermute behavior using shared vgprs (for GFX10/10.3)
# definitions: result VGPR, temp EXEC, clobbered SCC
# operands: index * 4, input data, same half (bool)
insn("p_bpermute_shared_vgpr")

# simulates proper wave64 bpermute behavior using v_permlane64_b32 (for GFX11+)
# definitions: result VGPR, temp EXEC, clobbered SCC
# operands: linear VGPR, index * 4, input data, same half (bool)
insn("p_bpermute_permlane")

# creates a lane mask where only the first active lane is selected
insn("p_elect")

insn("p_constaddr")
insn("p_resume_shader_address")

# These don't have to be pseudo-ops, but it makes optimization easier to only
# have to consider two instructions.
# (src0 >> (index * bits)) & ((1 << bits) - 1) with optional sign extension
insn("p_extract") # src1=index, src2=bits, src3=signext
# (src0 & ((1 << bits) - 1)) << (index * bits)
insn("p_insert") # src1=index, src2=bits

insn("p_init_scratch")

# jumps to a shader epilog
insn("p_jump_to_epilog")

# loads and interpolates a fragment shader input with a correct exec mask
#dst0=result, src0=linear_vgpr, src1=attribute, src2=component, src3=high_16bits, src4=coord1, src5=coord2, src6=m0
#dst0=result, src0=linear_vgpr, src1=attribute, src2=component, src3=dpp_ctrl, src4=m0
insn("p_interp_gfx11")

# performs dual source MRTs swizzling and emits exports on GFX11
insn("p_dual_src_export_gfx11")

# Let shader end with specific registers set to wanted value, used by multi part
# shader to pass arguments to next part.
insn("p_end_with_regs")

# SOP2 instructions: 2 scalar inputs, 1 scalar output (+optional scc)
SOP2 = {
   ("s_add_u32",            dst(1, SCC), src(1, 1), op(0x00)),
   ("s_sub_u32",            dst(1, SCC), src(1, 1), op(0x01)),
   ("s_add_i32",            dst(1, SCC), src(1, 1), op(0x02)),
   ("s_sub_i32",            dst(1, SCC), src(1, 1), op(0x03)),
   ("s_addc_u32",           dst(1, SCC), src(1, 1, SCC), op(0x04)),
   ("s_subb_u32",           dst(1, SCC), src(1, 1, SCC), op(0x05)),
   ("s_min_i32",            dst(1, SCC), src(1, 1), op(0x06, gfx11=0x12)),
   ("s_min_u32",            dst(1, SCC), src(1, 1), op(0x07, gfx11=0x13)),
   ("s_max_i32",            dst(1, SCC), src(1, 1), op(0x08, gfx11=0x14)),
   ("s_max_u32",            dst(1, SCC), src(1, 1), op(0x09, gfx11=0x15)),
   ("s_cselect_b32",        dst(1), src(1, 1, SCC), op(0x0a, gfx11=0x30)),
   ("s_cselect_b64",        dst(2), src(2, 2, SCC), op(0x0b, gfx11=0x31)),
   ("s_and_b32",            dst(1, SCC), src(1, 1), op(0x0e, gfx8=0x0c, gfx10=0x0e, gfx11=0x16)),
   ("s_and_b64",            dst(2, SCC), src(2, 2), op(0x0f, gfx8=0x0d, gfx10=0x0f, gfx11=0x17)),
   ("s_or_b32",             dst(1, SCC), src(1, 1), op(0x10, gfx8=0x0e, gfx10=0x10, gfx11=0x18)),
   ("s_or_b64",             dst(2, SCC), src(2, 2), op(0x11, gfx8=0x0f, gfx10=0x11, gfx11=0x19)),
   ("s_xor_b32",            dst(1, SCC), src(1, 1), op(0x12, gfx8=0x10, gfx10=0x12, gfx11=0x1a)),
   ("s_xor_b64",            dst(2, SCC), src(2, 2), op(0x13, gfx8=0x11, gfx10=0x13, gfx11=0x1b)),
   ("s_andn2_b32",          dst(1, SCC), src(1, 1), op(0x14, gfx8=0x12, gfx10=0x14, gfx11=0x22)), #s_and_not1_b32 in GFX11
   ("s_andn2_b64",          dst(2, SCC), src(2, 2), op(0x15, gfx8=0x13, gfx10=0x15, gfx11=0x23)), #s_and_not1_b64 in GFX11
   ("s_orn2_b32",           dst(1, SCC), src(1, 1), op(0x16, gfx8=0x14, gfx10=0x16, gfx11=0x24)), #s_or_not1_b32 in GFX11
   ("s_orn2_b64",           dst(2, SCC), src(2, 2), op(0x17, gfx8=0x15, gfx10=0x17, gfx11=0x25)), #s_or_not1_b64 in GFX11
   ("s_nand_b32",           dst(1, SCC), src(1, 1), op(0x18, gfx8=0x16, gfx10=0x18, gfx11=0x1c)),
   ("s_nand_b64",           dst(2, SCC), src(2, 2), op(0x19, gfx8=0x17, gfx10=0x19, gfx11=0x1d)),
   ("s_nor_b32",            dst(1, SCC), src(1, 1), op(0x1a, gfx8=0x18, gfx10=0x1a, gfx11=0x1e)),
   ("s_nor_b64",            dst(2, SCC), src(2, 2), op(0x1b, gfx8=0x19, gfx10=0x1b, gfx11=0x1f)),
   ("s_xnor_b32",           dst(1, SCC), src(1, 1), op(0x1c, gfx8=0x1a, gfx10=0x1c, gfx11=0x20)),
   ("s_xnor_b64",           dst(2, SCC), src(2, 2), op(0x1d, gfx8=0x1b, gfx10=0x1d, gfx11=0x21)),
   ("s_lshl_b32",           dst(1, SCC), src(1, 1), op(0x1e, gfx8=0x1c, gfx10=0x1e, gfx11=0x08)),
   ("s_lshl_b64",           dst(2, SCC), src(2, 1), op(0x1f, gfx8=0x1d, gfx10=0x1f, gfx11=0x09)),
   ("s_lshr_b32",           dst(1, SCC), src(1, 1), op(0x20, gfx8=0x1e, gfx10=0x20, gfx11=0x0a)),
   ("s_lshr_b64",           dst(2, SCC), src(2, 1), op(0x21, gfx8=0x1f, gfx10=0x21, gfx11=0x0b)),
   ("s_ashr_i32",           dst(1, SCC), src(1, 1), op(0x22, gfx8=0x20, gfx10=0x22, gfx11=0x0c)),
   ("s_ashr_i64",           dst(2, SCC), src(2, 1), op(0x23, gfx8=0x21, gfx10=0x23, gfx11=0x0d)),
   ("s_bfm_b32",            dst(1), src(1, 1), op(0x24, gfx8=0x22, gfx10=0x24, gfx11=0x2a)),
   ("s_bfm_b64",            dst(2), src(1, 1), op(0x25, gfx8=0x23, gfx10=0x25, gfx11=0x2b)),
   ("s_mul_i32",            dst(1), src(1, 1), op(0x26, gfx8=0x24, gfx10=0x26, gfx11=0x2c)),
   ("s_bfe_u32",            dst(1, SCC), src(1, 1), op(0x27, gfx8=0x25, gfx10=0x27, gfx11=0x26)),
   ("s_bfe_i32",            dst(1, SCC), src(1, 1), op(0x28, gfx8=0x26, gfx10=0x28, gfx11=0x27)),
   ("s_bfe_u64",            dst(2, SCC), src(2, 1), op(0x29, gfx8=0x27, gfx10=0x29, gfx11=0x28)),
   ("s_bfe_i64",            dst(2, SCC), src(2, 1), op(0x2a, gfx8=0x28, gfx10=0x2a, gfx11=0x29)),
   ("s_cbranch_g_fork",     dst(), src(), op(0x2b, gfx8=0x29, gfx10=-1), InstrClass.Branch),
   ("s_absdiff_i32",        dst(1, SCC), src(1, 1), op(0x2c, gfx8=0x2a, gfx10=0x2c, gfx11=0x06)),
   ("s_rfe_restore_b64",    dst(), src(), op(gfx8=0x2b, gfx10=-1), InstrClass.Branch),
   ("s_lshl1_add_u32",      dst(1, SCC), src(1, 1), op(gfx9=0x2e, gfx11=0x0e)),
   ("s_lshl2_add_u32",      dst(1, SCC), src(1, 1), op(gfx9=0x2f, gfx11=0x0f)),
   ("s_lshl3_add_u32",      dst(1, SCC), src(1, 1), op(gfx9=0x30, gfx11=0x10)),
   ("s_lshl4_add_u32",      dst(1, SCC), src(1, 1), op(gfx9=0x31, gfx11=0x11)),
   ("s_pack_ll_b32_b16",    dst(1), src(1, 1), op(gfx9=0x32)),
   ("s_pack_lh_b32_b16",    dst(1), src(1, 1), op(gfx9=0x33)),
   ("s_pack_hh_b32_b16",    dst(1), src(1, 1), op(gfx9=0x34)),
   ("s_pack_hl_b32_b16",    dst(1), src(1, 1), op(gfx11=0x35)),
   ("s_mul_hi_u32",         dst(1), src(1, 1), op(gfx9=0x2c, gfx10=0x35, gfx11=0x2d)),
   ("s_mul_hi_i32",         dst(1), src(1, 1), op(gfx9=0x2d, gfx10=0x36, gfx11=0x2e)),
   # actually a pseudo-instruction. it's lowered to SALU during assembly though, so it's useful to identify it as a SOP2.
   ("p_constaddr_addlo",    dst(1, SCC), src(1, 1, 1), op(-1)),
   ("p_resumeaddr_addlo",   dst(1, SCC), src(1, 1, 1), op(-1)),
}
for (name, defs, ops, num, cls) in default_class(SOP2, InstrClass.Salu):
    insn(name, num, Format.SOP2, cls, definitions = defs, operands = ops)


# SOPK instructions: 0 input (+ imm), 1 output + optional scc
SOPK = {
   ("s_movk_i32",             dst(1), src(), op(0x00)),
   ("s_version",              dst(), src(), op(gfx10=0x01)),
   ("s_cmovk_i32",            dst(1), src(1, SCC), op(0x02, gfx8=0x01, gfx10=0x02)),
   ("s_cmpk_eq_i32",          dst(SCC), src(1), op(0x03, gfx8=0x02, gfx10=0x03)),
   ("s_cmpk_lg_i32",          dst(SCC), src(1), op(0x04, gfx8=0x03, gfx10=0x04)),
   ("s_cmpk_gt_i32",          dst(SCC), src(1), op(0x05, gfx8=0x04, gfx10=0x05)),
   ("s_cmpk_ge_i32",          dst(SCC), src(1), op(0x06, gfx8=0x05, gfx10=0x06)),
   ("s_cmpk_lt_i32",          dst(SCC), src(1), op(0x07, gfx8=0x06, gfx10=0x07)),
   ("s_cmpk_le_i32",          dst(SCC), src(1), op(0x08, gfx8=0x07, gfx10=0x08)),
   ("s_cmpk_eq_u32",          dst(SCC), src(1), op(0x09, gfx8=0x08, gfx10=0x09)),
   ("s_cmpk_lg_u32",          dst(SCC), src(1), op(0x0a, gfx8=0x09, gfx10=0x0a)),
   ("s_cmpk_gt_u32",          dst(SCC), src(1), op(0x0b, gfx8=0x0a, gfx10=0x0b)),
   ("s_cmpk_ge_u32",          dst(SCC), src(1), op(0x0c, gfx8=0x0b, gfx10=0x0c)),
   ("s_cmpk_lt_u32",          dst(SCC), src(1), op(0x0d, gfx8=0x0c, gfx10=0x0d)),
   ("s_cmpk_le_u32",          dst(SCC), src(1), op(0x0e, gfx8=0x0d, gfx10=0x0e)),
   ("s_addk_i32",             dst(1, SCC), src(1), op(0x0f, gfx8=0x0e, gfx10=0x0f)),
   ("s_mulk_i32",             dst(1), src(1), op(0x10, gfx8=0x0f, gfx10=0x10)),
   ("s_cbranch_i_fork",       dst(), src(), op(0x11, gfx8=0x10, gfx10=-1), InstrClass.Branch),
   ("s_getreg_b32",           dst(1), src(), op(0x12, gfx8=0x11, gfx10=0x12, gfx11=0x11)),
   ("s_setreg_b32",           dst(), src(1), op(0x13, gfx8=0x12, gfx10=0x13, gfx11=0x12)),
   ("s_setreg_imm32_b32",     dst(), src(1), op(0x15, gfx8=0x14, gfx10=0x15, gfx11=0x13)), # requires 32bit literal
   ("s_call_b64",             dst(2), src(), op(gfx8=0x15, gfx10=0x16, gfx11=0x14), InstrClass.Branch),
   ("s_waitcnt_vscnt",        dst(), src(1), op(gfx10=0x17, gfx11=0x18), InstrClass.Waitcnt),
   ("s_waitcnt_vmcnt",        dst(), src(1), op(gfx10=0x18, gfx11=0x19), InstrClass.Waitcnt),
   ("s_waitcnt_expcnt",       dst(), src(1), op(gfx10=0x19, gfx11=0x1a), InstrClass.Waitcnt),
   ("s_waitcnt_lgkmcnt",      dst(), src(1), op(gfx10=0x1a, gfx11=0x1b), InstrClass.Waitcnt),
   ("s_subvector_loop_begin", dst(), src(), op(gfx10=0x1b, gfx11=0x16), InstrClass.Branch),
   ("s_subvector_loop_end",   dst(), src(), op(gfx10=0x1c, gfx11=0x17), InstrClass.Branch),
}
for (name, defs, ops, num, cls) in default_class(SOPK, InstrClass.Salu):
   insn(name, num, Format.SOPK, cls, definitions = defs, operands = ops)


# SOP1 instructions: 1 input, 1 output (+optional SCC)
SOP1 = {
   ("s_mov_b32",                dst(1), src(1), op(0x03, gfx8=0x00, gfx10=0x03, gfx11=0x00)),
   ("s_mov_b64",                dst(2), src(2), op(0x04, gfx8=0x01, gfx10=0x04, gfx11=0x01)),
   ("s_cmov_b32",               dst(1), src(1, 1, SCC), op(0x05, gfx8=0x02, gfx10=0x05, gfx11=0x02)),
   ("s_cmov_b64",               dst(2), src(2, 2, SCC), op(0x06, gfx8=0x03, gfx10=0x06, gfx11=0x03)),
   ("s_not_b32",                dst(1, SCC), src(1), op(0x07, gfx8=0x04, gfx10=0x07, gfx11=0x1e)),
   ("s_not_b64",                dst(2, SCC), src(2), op(0x08, gfx8=0x05, gfx10=0x08, gfx11=0x1f)),
   ("s_wqm_b32",                dst(1, SCC), src(1), op(0x09, gfx8=0x06, gfx10=0x09, gfx11=0x1c)),
   ("s_wqm_b64",                dst(2, SCC), src(2), op(0x0a, gfx8=0x07, gfx10=0x0a, gfx11=0x1d)),
   ("s_brev_b32",               dst(1), src(1), op(0x0b, gfx8=0x08, gfx10=0x0b, gfx11=0x04)),
   ("s_brev_b64",               dst(2), src(2), op(0x0c, gfx8=0x09, gfx10=0x0c, gfx11=0x05)),
   ("s_bcnt0_i32_b32",          dst(1, SCC), src(1), op(0x0d, gfx8=0x0a, gfx10=0x0d, gfx11=0x16)),
   ("s_bcnt0_i32_b64",          dst(1, SCC), src(2), op(0x0e, gfx8=0x0b, gfx10=0x0e, gfx11=0x17)),
   ("s_bcnt1_i32_b32",          dst(1, SCC), src(1), op(0x0f, gfx8=0x0c, gfx10=0x0f, gfx11=0x18)),
   ("s_bcnt1_i32_b64",          dst(1, SCC), src(2), op(0x10, gfx8=0x0d, gfx10=0x10, gfx11=0x19)),
   ("s_ff0_i32_b32",            dst(1), src(1), op(0x11, gfx8=0x0e, gfx10=0x11, gfx11=-1)),
   ("s_ff0_i32_b64",            dst(1), src(2), op(0x12, gfx8=0x0f, gfx10=0x12, gfx11=-1)),
   ("s_ff1_i32_b32",            dst(1), src(1), op(0x13, gfx8=0x10, gfx10=0x13, gfx11=0x08)), #s_ctz_i32_b32 in GFX11
   ("s_ff1_i32_b64",            dst(1), src(2), op(0x14, gfx8=0x11, gfx10=0x14, gfx11=0x09)), #s_ctz_i32_b64 in GFX11
   ("s_flbit_i32_b32",          dst(1), src(1), op(0x15, gfx8=0x12, gfx10=0x15, gfx11=0x0a)), #s_clz_i32_u32 in GFX11
   ("s_flbit_i32_b64",          dst(1), src(2), op(0x16, gfx8=0x13, gfx10=0x16, gfx11=0x0b)), #s_clz_i32_u64 in GFX11
   ("s_flbit_i32",              dst(1), src(1), op(0x17, gfx8=0x14, gfx10=0x17, gfx11=0x0c)), #s_cls_i32 in GFX11
   ("s_flbit_i32_i64",          dst(1), src(2), op(0x18, gfx8=0x15, gfx10=0x18, gfx11=0x0d)), #s_cls_i32_i64 in GFX11
   ("s_sext_i32_i8",            dst(1), src(1), op(0x19, gfx8=0x16, gfx10=0x19, gfx11=0x0e)),
   ("s_sext_i32_i16",           dst(1), src(1), op(0x1a, gfx8=0x17, gfx10=0x1a, gfx11=0x0f)),
   ("s_bitset0_b32",            dst(1), src(1, 1), op(0x1b, gfx8=0x18, gfx10=0x1b, gfx11=0x10)),
   ("s_bitset0_b64",            dst(2), src(1, 2), op(0x1c, gfx8=0x19, gfx10=0x1c, gfx11=0x11)),
   ("s_bitset1_b32",            dst(1), src(1, 1), op(0x1d, gfx8=0x1a, gfx10=0x1d, gfx11=0x12)),
   ("s_bitset1_b64",            dst(2), src(1, 2), op(0x1e, gfx8=0x1b, gfx10=0x1e, gfx11=0x13)),
   ("s_getpc_b64",              dst(2), src(), op(0x1f, gfx8=0x1c, gfx10=0x1f, gfx11=0x47)),
   ("s_setpc_b64",              dst(), src(2), op(0x20, gfx8=0x1d, gfx10=0x20, gfx11=0x48), InstrClass.Branch),
   ("s_swappc_b64",             dst(2), src(2), op(0x21, gfx8=0x1e, gfx10=0x21, gfx11=0x49), InstrClass.Branch),
   ("s_rfe_b64",                dst(), src(2), op(0x22, gfx8=0x1f, gfx10=0x22, gfx11=0x4a), InstrClass.Branch),
   ("s_and_saveexec_b64",       dst(2, SCC, EXEC), src(2, EXEC), op(0x24, gfx8=0x20, gfx10=0x24, gfx11=0x21)),
   ("s_or_saveexec_b64",        dst(2, SCC, EXEC), src(2, EXEC), op(0x25, gfx8=0x21, gfx10=0x25, gfx11=0x23)),
   ("s_xor_saveexec_b64",       dst(2, SCC, EXEC), src(2, EXEC), op(0x26, gfx8=0x22, gfx10=0x26, gfx11=0x25)),
   ("s_andn2_saveexec_b64",     dst(2, SCC, EXEC), src(2, EXEC), op(0x27, gfx8=0x23, gfx10=0x27, gfx11=0x31)), #s_and_not1_saveexec_b64 in GFX11
   ("s_orn2_saveexec_b64",      dst(2, SCC, EXEC), src(2, EXEC), op(0x28, gfx8=0x24, gfx10=0x28, gfx11=0x33)), #s_or_not1_saveexec_b64 in GFX11
   ("s_nand_saveexec_b64",      dst(2, SCC, EXEC), src(2, EXEC), op(0x29, gfx8=0x25, gfx10=0x29, gfx11=0x27)),
   ("s_nor_saveexec_b64",       dst(2, SCC, EXEC), src(2, EXEC), op(0x2a, gfx8=0x26, gfx10=0x2a, gfx11=0x29)),
   ("s_xnor_saveexec_b64",      dst(2, SCC, EXEC), src(2, EXEC), op(0x2b, gfx8=0x27, gfx10=0x2b)),
   ("s_quadmask_b32",           dst(1, SCC), src(1), op(0x2c, gfx8=0x28, gfx10=0x2c, gfx11=0x1a)),
   ("s_quadmask_b64",           dst(2, SCC), src(2), op(0x2d, gfx8=0x29, gfx10=0x2d, gfx11=0x1b)), # Always writes 0 to the second SGPR
   ("s_movrels_b32",            dst(1), src(1, M0), op(0x2e, gfx8=0x2a, gfx10=0x2e, gfx11=0x40)),
   ("s_movrels_b64",            dst(2), src(2, M0), op(0x2f, gfx8=0x2b, gfx10=0x2f, gfx11=0x41)),
   ("s_movreld_b32",            dst(1), src(1, M0), op(0x30, gfx8=0x2c, gfx10=0x30, gfx11=0x42)),
   ("s_movreld_b64",            dst(2), src(2, M0), op(0x31, gfx8=0x2d, gfx10=0x31, gfx11=0x43)),
   ("s_cbranch_join",           dst(), src(), op(0x32, gfx8=0x2e, gfx10=-1), InstrClass.Branch),
   ("s_abs_i32",                dst(1, SCC), src(1), op(0x34, gfx8=0x30, gfx10=0x34, gfx11=0x15)),
   ("s_mov_fed_b32",            dst(), src(), op(0x35, gfx8=-1, gfx10=0x35, gfx11=-1)),
   ("s_set_gpr_idx_idx",        dst(M0), src(1, M0), op(gfx8=0x32, gfx10=-1)),
   ("s_andn1_saveexec_b64",     dst(2, SCC, EXEC), src(2, EXEC), op(gfx9=0x33, gfx10=0x37, gfx11=0x2d)), #s_and_not0_savexec_b64 in GFX11
   ("s_orn1_saveexec_b64",      dst(2, SCC, EXEC), src(2, EXEC), op(gfx9=0x34, gfx10=0x38, gfx11=0x2f)), #s_or_not0_savexec_b64 in GFX11
   ("s_andn1_wrexec_b64",       dst(2, SCC, EXEC), src(2, EXEC), op(gfx9=0x35, gfx10=0x39, gfx11=0x35)), #s_and_not0_wrexec_b64 in GFX11
   ("s_andn2_wrexec_b64",       dst(2, SCC, EXEC), src(2, EXEC), op(gfx9=0x36, gfx10=0x3a, gfx11=0x37)), #s_and_not1_wrexec_b64 in GFX11
   ("s_bitreplicate_b64_b32",   dst(2), src(1), op(gfx9=0x37, gfx10=0x3b, gfx11=0x14)),
   ("s_and_saveexec_b32",       dst(1, SCC, EXEC_LO), src(1, EXEC_LO), op(gfx10=0x3c, gfx11=0x20)),
   ("s_or_saveexec_b32",        dst(1, SCC, EXEC_LO), src(1, EXEC_LO), op(gfx10=0x3d, gfx11=0x22)),
   ("s_xor_saveexec_b32",       dst(1, SCC, EXEC_LO), src(1, EXEC_LO), op(gfx10=0x3e, gfx11=0x24)),
   ("s_andn2_saveexec_b32",     dst(1, SCC, EXEC_LO), src(1, EXEC_LO), op(gfx10=0x3f, gfx11=0x30)), #s_and_not1_saveexec_b32 in GFX11
   ("s_orn2_saveexec_b32",      dst(1, SCC, EXEC_LO), src(1, EXEC_LO), op(gfx10=0x40, gfx11=0x32)), #s_or_not1_saveexec_b32 in GFX11
   ("s_nand_saveexec_b32",      dst(1, SCC, EXEC_LO), src(1, EXEC_LO), op(gfx10=0x41, gfx11=0x26)),
   ("s_nor_saveexec_b32",       dst(1, SCC, EXEC_LO), src(1, EXEC_LO), op(gfx10=0x42, gfx11=0x28)),
   ("s_xnor_saveexec_b32",      dst(1, SCC, EXEC_LO), src(1, EXEC_LO), op(gfx10=0x43, gfx11=0x2a)),
   ("s_andn1_saveexec_b32",     dst(1, SCC, EXEC_LO), src(1, EXEC_LO), op(gfx10=0x44, gfx11=0x2c)), #s_and_not0_savexec_b32 in GFX11
   ("s_orn1_saveexec_b32",      dst(1, SCC, EXEC_LO), src(1, EXEC_LO), op(gfx10=0x45, gfx11=0x2e)), #s_or_not0_savexec_b32 in GFX11
   ("s_andn1_wrexec_b32",       dst(1, SCC, EXEC_LO), src(1, EXEC_LO), op(gfx10=0x46, gfx11=0x34)), #s_and_not0_wrexec_b32 in GFX11
   ("s_andn2_wrexec_b32",       dst(1, SCC, EXEC_LO), src(1, EXEC_LO), op(gfx10=0x47, gfx11=0x36)), #s_and_not1_wrexec_b32 in GFX11
   ("s_movrelsd_2_b32",         dst(1), src(1, M0), op(gfx10=0x49, gfx11=0x44)),
   ("s_sendmsg_rtn_b32",        dst(1), src(1), op(gfx11=0x4c)),
   ("s_sendmsg_rtn_b64",        dst(2), src(1), op(gfx11=0x4d)),
   # actually a pseudo-instruction. it's lowered to SALU during assembly though, so it's useful to identify it as a SOP1.
   ("p_constaddr_getpc",        dst(2), src(1), op(-1)),
   ("p_resumeaddr_getpc",       dst(2), src(1), op(-1)),
   ("p_load_symbol",            dst(1), src(1), op(-1)),
}
for (name, defs, ops, num, cls) in default_class(SOP1, InstrClass.Salu):
   insn(name, num, Format.SOP1, cls, definitions = defs, operands = ops)


# SOPC instructions: 2 inputs and 0 outputs (+SCC)
SOPC = {
   ("s_cmp_eq_i32",     dst(SCC), src(1, 1), op(0x00)),
   ("s_cmp_lg_i32",     dst(SCC), src(1, 1), op(0x01)),
   ("s_cmp_gt_i32",     dst(SCC), src(1, 1), op(0x02)),
   ("s_cmp_ge_i32",     dst(SCC), src(1, 1), op(0x03)),
   ("s_cmp_lt_i32",     dst(SCC), src(1, 1), op(0x04)),
   ("s_cmp_le_i32",     dst(SCC), src(1, 1), op(0x05)),
   ("s_cmp_eq_u32",     dst(SCC), src(1, 1), op(0x06)),
   ("s_cmp_lg_u32",     dst(SCC), src(1, 1), op(0x07)),
   ("s_cmp_gt_u32",     dst(SCC), src(1, 1), op(0x08)),
   ("s_cmp_ge_u32",     dst(SCC), src(1, 1), op(0x09)),
   ("s_cmp_lt_u32",     dst(SCC), src(1, 1), op(0x0a)),
   ("s_cmp_le_u32",     dst(SCC), src(1, 1), op(0x0b)),
   ("s_bitcmp0_b32",    dst(SCC), src(1, 1), op(0x0c)),
   ("s_bitcmp1_b32",    dst(SCC), src(1, 1), op(0x0d)),
   ("s_bitcmp0_b64",    dst(SCC), src(2, 1), op(0x0e)),
   ("s_bitcmp1_b64",    dst(SCC), src(2, 1), op(0x0f)),
   ("s_setvskip",       dst(), src(1, 1), op(0x10, gfx10=-1)),
   ("s_set_gpr_idx_on", dst(M0), src(1, 1, M0), op(gfx8=0x11, gfx10=-1)),
   ("s_cmp_eq_u64",     dst(SCC), src(2, 2), op(gfx8=0x12, gfx11=0x10)),
   ("s_cmp_lg_u64",     dst(SCC), src(2, 2), op(gfx8=0x13, gfx11=0x11)),
}
for (name, defs, ops, num) in SOPC:
   insn(name, num, Format.SOPC, InstrClass.Salu, definitions = defs, operands = ops)


# SOPP instructions: 0 inputs (+optional scc/vcc), 0 outputs
SOPP = {
   ("s_nop",                      dst(), src(), op(0x00)),
   ("s_endpgm",                   dst(), src(), op(0x01, gfx11=0x30)),
   ("s_branch",                   dst(), src(), op(0x02, gfx11=0x20), InstrClass.Branch),
   ("s_wakeup",                   dst(), src(), op(gfx8=0x03, gfx11=0x34)),
   ("s_cbranch_scc0",             dst(), src(), op(0x04, gfx11=0x21), InstrClass.Branch),
   ("s_cbranch_scc1",             dst(), src(), op(0x05, gfx11=0x22), InstrClass.Branch),
   ("s_cbranch_vccz",             dst(), src(), op(0x06, gfx11=0x23), InstrClass.Branch),
   ("s_cbranch_vccnz",            dst(), src(), op(0x07, gfx11=0x24), InstrClass.Branch),
   ("s_cbranch_execz",            dst(), src(), op(0x08, gfx11=0x25), InstrClass.Branch),
   ("s_cbranch_execnz",           dst(), src(), op(0x09, gfx11=0x26), InstrClass.Branch),
   ("s_barrier",                  dst(), src(), op(0x0a, gfx11=0x3d), InstrClass.Barrier),
   ("s_setkill",                  dst(), src(), op(gfx7=0x0b, gfx11=0x01)),
   ("s_waitcnt",                  dst(), src(), op(0x0c, gfx11=0x09), InstrClass.Waitcnt),
   ("s_sethalt",                  dst(), src(), op(0x0d, gfx11=0x02)),
   ("s_sleep",                    dst(), src(), op(0x0e, gfx11=0x03)),
   ("s_setprio",                  dst(), src(), op(0x0f, gfx11=0x35)),
   ("s_sendmsg",                  dst(), src(), op(0x10, gfx11=0x36), InstrClass.Sendmsg),
   ("s_sendmsghalt",              dst(), src(), op(0x11, gfx11=0x37), InstrClass.Sendmsg),
   ("s_trap",                     dst(), src(), op(0x12, gfx11=0x10), InstrClass.Other),
   ("s_icache_inv",               dst(), src(), op(0x13, gfx11=0x3c)),
   ("s_incperflevel",             dst(), src(), op(0x14, gfx11=0x38)),
   ("s_decperflevel",             dst(), src(), op(0x15, gfx11=0x39)),
   ("s_ttracedata",               dst(), src(M0), op(0x16, gfx11=0x3a)),
   ("s_cbranch_cdbgsys",          dst(), src(), op(gfx7=0x17, gfx11=0x27), InstrClass.Branch),
   ("s_cbranch_cdbguser",         dst(), src(), op(gfx7=0x18, gfx11=0x28), InstrClass.Branch),
   ("s_cbranch_cdbgsys_or_user",  dst(), src(), op(gfx7=0x19, gfx11=0x29), InstrClass.Branch),
   ("s_cbranch_cdbgsys_and_user", dst(), src(), op(gfx7=0x1a, gfx11=0x2a), InstrClass.Branch),
   ("s_endpgm_saved",             dst(), src(), op(gfx8=0x1b, gfx11=0x31)),
   ("s_set_gpr_idx_off",          dst(), src(), op(gfx8=0x1c, gfx10=-1)),
   ("s_set_gpr_idx_mode",         dst(M0), src(M0), op(gfx8=0x1d, gfx10=-1)),
   ("s_endpgm_ordered_ps_done",   dst(), src(), op(gfx9=0x1e, gfx11=-1)),
   ("s_code_end",                 dst(), src(), op(gfx10=0x1f)),
   ("s_inst_prefetch",            dst(), src(), op(gfx10=0x20, gfx11=0x04)), #s_set_inst_prefetch_distance in GFX11
   ("s_clause",                   dst(), src(), op(gfx10=0x21, gfx11=0x05)),
   ("s_wait_idle",                dst(), src(), op(gfx10=0x22, gfx11=0x0a), InstrClass.Waitcnt),
   ("s_waitcnt_depctr",           dst(), src(), op(gfx10=0x23, gfx11=0x08), InstrClass.Waitcnt),
   ("s_round_mode",               dst(), src(), op(gfx10=0x24, gfx11=0x11)),
   ("s_denorm_mode",              dst(), src(), op(gfx10=0x25, gfx11=0x12)),
   ("s_ttracedata_imm",           dst(), src(), op(gfx10=0x26, gfx11=0x3b)),
   ("s_delay_alu",                dst(), src(), op(gfx11=0x07), InstrClass.Waitcnt),
   ("s_wait_event",               dst(), src(), op(gfx11=0x0b)),
}
for (name, defs, ops, num, cls) in default_class(SOPP, InstrClass.Salu):
   insn(name, num, Format.SOPP, cls, definitions = defs, operands = ops)


# SMEM instructions: sbase input (2 sgpr), potentially 2 offset inputs, 1 sdata input/output
# Unlike GFX10, GFX10.3 does not have SMEM store, atomic or scratch instructions
SMEM = {
   ("s_load_dword",               op(0x00)), #s_load_b32 in GFX11
   ("s_load_dwordx2",             op(0x01)), #s_load_b64 in GFX11
   ("s_load_dwordx4",             op(0x02)), #s_load_b128 in GFX11
   ("s_load_dwordx8",             op(0x03)), #s_load_b256 in GFX11
   ("s_load_dwordx16",            op(0x04)), #s_load_b512 in GFX11
   ("s_scratch_load_dword",       op(gfx9=0x05, gfx11=-1)),
   ("s_scratch_load_dwordx2",     op(gfx9=0x06, gfx11=-1)),
   ("s_scratch_load_dwordx4",     op(gfx9=0x07, gfx11=-1)),
   ("s_buffer_load_dword",        op(0x08)), #s_buffer_load_b32 in GFX11
   ("s_buffer_load_dwordx2",      op(0x09)), #s_buffer_load_b64 in GFX11
   ("s_buffer_load_dwordx4",      op(0x0a)), #s_buffer_load_b128 in GFX11
   ("s_buffer_load_dwordx8",      op(0x0b)), #s_buffer_load_b256 in GFX11
   ("s_buffer_load_dwordx16",     op(0x0c)), #s_buffer_load_b512 in GFX11
   ("s_store_dword",              op(gfx8=0x10, gfx11=-1)),
   ("s_store_dwordx2",            op(gfx8=0x11, gfx11=-1)),
   ("s_store_dwordx4",            op(gfx8=0x12, gfx11=-1)),
   ("s_scratch_store_dword",      op(gfx9=0x15, gfx11=-1)),
   ("s_scratch_store_dwordx2",    op(gfx9=0x16, gfx11=-1)),
   ("s_scratch_store_dwordx4",    op(gfx9=0x17, gfx11=-1)),
   ("s_buffer_store_dword",       op(gfx8=0x18, gfx11=-1)),
   ("s_buffer_store_dwordx2",     op(gfx8=0x19, gfx11=-1)),
   ("s_buffer_store_dwordx4",     op(gfx8=0x1a, gfx11=-1)),
   ("s_gl1_inv",                  op(gfx8=0x1f, gfx11=0x20)),
   ("s_dcache_inv",               op(0x1f, gfx8=0x20, gfx11=0x21)),
   ("s_dcache_wb",                op(gfx8=0x21, gfx11=-1)),
   ("s_dcache_inv_vol",           op(gfx7=0x1d, gfx8=0x22, gfx10=-1)),
   ("s_dcache_wb_vol",            op(gfx8=0x23, gfx10=-1)),
   ("s_memtime",                  op(0x1e, gfx8=0x24, gfx11=-1)), #GFX6-GFX10
   ("s_memrealtime",              op(gfx8=0x25, gfx11=-1)),
   ("s_atc_probe",                op(gfx8=0x26, gfx11=0x22)),
   ("s_atc_probe_buffer",         op(gfx8=0x27, gfx11=0x23)),
   ("s_dcache_discard",           op(gfx9=0x28, gfx11=-1)),
   ("s_dcache_discard_x2",        op(gfx9=0x29, gfx11=-1)),
   ("s_get_waveid_in_workgroup",  op(gfx10=0x2a, gfx11=-1)),
   ("s_buffer_atomic_swap",       op(gfx9=0x40, gfx11=-1)),
   ("s_buffer_atomic_cmpswap",    op(gfx9=0x41, gfx11=-1)),
   ("s_buffer_atomic_add",        op(gfx9=0x42, gfx11=-1)),
   ("s_buffer_atomic_sub",        op(gfx9=0x43, gfx11=-1)),
   ("s_buffer_atomic_smin",       op(gfx9=0x44, gfx11=-1)),
   ("s_buffer_atomic_umin",       op(gfx9=0x45, gfx11=-1)),
   ("s_buffer_atomic_smax",       op(gfx9=0x46, gfx11=-1)),
   ("s_buffer_atomic_umax",       op(gfx9=0x47, gfx11=-1)),
   ("s_buffer_atomic_and",        op(gfx9=0x48, gfx11=-1)),
   ("s_buffer_atomic_or",         op(gfx9=0x49, gfx11=-1)),
   ("s_buffer_atomic_xor",        op(gfx9=0x4a, gfx11=-1)),
   ("s_buffer_atomic_inc",        op(gfx9=0x4b, gfx11=-1)),
   ("s_buffer_atomic_dec",        op(gfx9=0x4c, gfx11=-1)),
   ("s_buffer_atomic_swap_x2",    op(gfx9=0x60, gfx11=-1)),
   ("s_buffer_atomic_cmpswap_x2", op(gfx9=0x61, gfx11=-1)),
   ("s_buffer_atomic_add_x2",     op(gfx9=0x62, gfx11=-1)),
   ("s_buffer_atomic_sub_x2",     op(gfx9=0x63, gfx11=-1)),
   ("s_buffer_atomic_smin_x2",    op(gfx9=0x64, gfx11=-1)),
   ("s_buffer_atomic_umin_x2",    op(gfx9=0x65, gfx11=-1)),
   ("s_buffer_atomic_smax_x2",    op(gfx9=0x66, gfx11=-1)),
   ("s_buffer_atomic_umax_x2",    op(gfx9=0x67, gfx11=-1)),
   ("s_buffer_atomic_and_x2",     op(gfx9=0x68, gfx11=-1)),
   ("s_buffer_atomic_or_x2",      op(gfx9=0x69, gfx11=-1)),
   ("s_buffer_atomic_xor_x2",     op(gfx9=0x6a, gfx11=-1)),
   ("s_buffer_atomic_inc_x2",     op(gfx9=0x6b, gfx11=-1)),
   ("s_buffer_atomic_dec_x2",     op(gfx9=0x6c, gfx11=-1)),
   ("s_atomic_swap",              op(gfx9=0x80, gfx11=-1)),
   ("s_atomic_cmpswap",           op(gfx9=0x81, gfx11=-1)),
   ("s_atomic_add",               op(gfx9=0x82, gfx11=-1)),
   ("s_atomic_sub",               op(gfx9=0x83, gfx11=-1)),
   ("s_atomic_smin",              op(gfx9=0x84, gfx11=-1)),
   ("s_atomic_umin",              op(gfx9=0x85, gfx11=-1)),
   ("s_atomic_smax",              op(gfx9=0x86, gfx11=-1)),
   ("s_atomic_umax",              op(gfx9=0x87, gfx11=-1)),
   ("s_atomic_and",               op(gfx9=0x88, gfx11=-1)),
   ("s_atomic_or",                op(gfx9=0x89, gfx11=-1)),
   ("s_atomic_xor",               op(gfx9=0x8a, gfx11=-1)),
   ("s_atomic_inc",               op(gfx9=0x8b, gfx11=-1)),
   ("s_atomic_dec",               op(gfx9=0x8c, gfx11=-1)),
   ("s_atomic_swap_x2",           op(gfx9=0xa0, gfx11=-1)),
   ("s_atomic_cmpswap_x2",        op(gfx9=0xa1, gfx11=-1)),
   ("s_atomic_add_x2",            op(gfx9=0xa2, gfx11=-1)),
   ("s_atomic_sub_x2",            op(gfx9=0xa3, gfx11=-1)),
   ("s_atomic_smin_x2",           op(gfx9=0xa4, gfx11=-1)),
   ("s_atomic_umin_x2",           op(gfx9=0xa5, gfx11=-1)),
   ("s_atomic_smax_x2",           op(gfx9=0xa6, gfx11=-1)),
   ("s_atomic_umax_x2",           op(gfx9=0xa7, gfx11=-1)),
   ("s_atomic_and_x2",            op(gfx9=0xa8, gfx11=-1)),
   ("s_atomic_or_x2",             op(gfx9=0xa9, gfx11=-1)),
   ("s_atomic_xor_x2",            op(gfx9=0xaa, gfx11=-1)),
   ("s_atomic_inc_x2",            op(gfx9=0xab, gfx11=-1)),
   ("s_atomic_dec_x2",            op(gfx9=0xac, gfx11=-1)),
}
for (name, num) in SMEM:
   insn(name, num, Format.SMEM, InstrClass.SMem, is_atomic = "atomic" in name)


# VOP2 instructions: 2 inputs, 1 output (+ optional vcc)
# TODO: misses some GFX6_7 opcodes which were shifted to VOP3 in GFX8
VOP2 = {
   ("v_cndmask_b32",       True, False, dst(1), src(1, 1, VCC), op(0x00, gfx10=0x01)),
   ("v_readlane_b32",      False, False, dst(1), src(1, 1), op(0x01, gfx8=-1)),
   ("v_writelane_b32",     False, False, dst(1), src(1, 1, 1), op(0x02, gfx8=-1)),
   ("v_add_f32",           True, True, dst(1), src(1, 1), op(0x03, gfx8=0x01, gfx10=0x03)),
   ("v_sub_f32",           True, True, dst(1), src(1, 1), op(0x04, gfx8=0x02, gfx10=0x04)),
   ("v_subrev_f32",        True, True, dst(1), src(1, 1), op(0x05, gfx8=0x03, gfx10=0x05)),
   ("v_mac_legacy_f32",    True, True, dst(1), src(1, 1, 1), op(0x06, gfx8=-1, gfx10=0x06, gfx11=-1)), #GFX6,7,10
   ("v_fmac_legacy_f32",   True, True, dst(1), src(1, 1, 1), op(gfx10=0x06)), #GFX10.3+, v_fmac_dx9_zero_f32 in GFX11
   ("v_mul_legacy_f32",    True, True, dst(1), src(1, 1), op(0x07, gfx8=0x04, gfx10=0x07)), #v_mul_dx9_zero_f32 in GFX11
   ("v_mul_f32",           True, True, dst(1), src(1, 1), op(0x08, gfx8=0x05, gfx10=0x08)),
   ("v_mul_i32_i24",       False, False, dst(1), src(1, 1), op(0x09, gfx8=0x06, gfx10=0x09)),
   ("v_mul_hi_i32_i24",    False, False, dst(1), src(1, 1), op(0x0a, gfx8=0x07, gfx10=0x0a)),
   ("v_mul_u32_u24",       False, False, dst(1), src(1, 1), op(0x0b, gfx8=0x08, gfx10=0x0b)),
   ("v_mul_hi_u32_u24",    False, False, dst(1), src(1, 1), op(0x0c, gfx8=0x09, gfx10=0x0c)),
   ("v_dot4c_i32_i8",      False, False, dst(1), src(1, 1, 1), op(gfx9=0x39, gfx10=0x0d, gfx11=-1)),
   ("v_min_legacy_f32",    True, True, dst(1), src(1, 1), op(0x0d, gfx8=-1)),
   ("v_max_legacy_f32",    True, True, dst(1), src(1, 1), op(0x0e, gfx8=-1)),
   ("v_min_f32",           True, True, dst(1), src(1, 1), op(0x0f, gfx8=0x0a, gfx10=0x0f)),
   ("v_max_f32",           True, True, dst(1), src(1, 1), op(0x10, gfx8=0x0b, gfx10=0x10)),
   ("v_min_i32",           False, False, dst(1), src(1, 1), op(0x11, gfx8=0x0c, gfx10=0x11)),
   ("v_max_i32",           False, False, dst(1), src(1, 1), op(0x12, gfx8=0x0d, gfx10=0x12)),
   ("v_min_u32",           False, False, dst(1), src(1, 1), op(0x13, gfx8=0x0e, gfx10=0x13)),
   ("v_max_u32",           False, False, dst(1), src(1, 1), op(0x14, gfx8=0x0f, gfx10=0x14)),
   ("v_lshr_b32",          False, False, dst(1), src(1, 1), op(0x15, gfx8=-1)),
   ("v_lshrrev_b32",       False, False, dst(1), src(1, 1), op(0x16, gfx8=0x10, gfx10=0x16, gfx11=0x19)),
   ("v_ashr_i32",          False, False, dst(1), src(1, 1), op(0x17, gfx8=-1)),
   ("v_ashrrev_i32",       False, False, dst(1), src(1, 1), op(0x18, gfx8=0x11, gfx10=0x18, gfx11=0x1a)),
   ("v_lshl_b32",          False, False, dst(1), src(1, 1), op(0x19, gfx8=-1)),
   ("v_lshlrev_b32",       False, False, dst(1), src(1, 1), op(0x1a, gfx8=0x12, gfx10=0x1a, gfx11=0x18)),
   ("v_and_b32",           False, False, dst(1), src(1, 1), op(0x1b, gfx8=0x13, gfx10=0x1b)),
   ("v_or_b32",            False, False, dst(1), src(1, 1), op(0x1c, gfx8=0x14, gfx10=0x1c)),
   ("v_xor_b32",           False, False, dst(1), src(1, 1), op(0x1d, gfx8=0x15, gfx10=0x1d)),
   ("v_xnor_b32",          False, False, dst(1), src(1, 1), op(gfx10=0x1e)),
   ("v_mac_f32",           True, True, dst(1), src(1, 1, 1), op(0x1f, gfx8=0x16, gfx10=0x1f, gfx11=-1)),
   ("v_madmk_f32",         False, False, dst(1), src(1, 1, 1), op(0x20, gfx8=0x17, gfx10=0x20, gfx11=-1)),
   ("v_madak_f32",         False, False, dst(1), src(1, 1, 1), op(0x21, gfx8=0x18, gfx10=0x21, gfx11=-1)),
   ("v_mbcnt_hi_u32_b32",  False, False, dst(1), src(1, 1), op(0x24, gfx8=-1)),
   ("v_add_co_u32",        False, False, dst(1, VCC), src(1, 1), op(0x25, gfx8=0x19, gfx10=-1)), # VOP3B only in RDNA
   ("v_sub_co_u32",        False, False, dst(1, VCC), src(1, 1), op(0x26, gfx8=0x1a, gfx10=-1)), # VOP3B only in RDNA
   ("v_subrev_co_u32",     False, False, dst(1, VCC), src(1, 1), op(0x27, gfx8=0x1b, gfx10=-1)), # VOP3B only in RDNA
   ("v_addc_co_u32",       False, False, dst(1, VCC), src(1, 1, VCC), op(0x28, gfx8=0x1c, gfx10=0x28, gfx11=0x20)), # v_add_co_ci_u32 in RDNA
   ("v_subb_co_u32",       False, False, dst(1, VCC), src(1, 1, VCC), op(0x29, gfx8=0x1d, gfx10=0x29, gfx11=0x21)), # v_sub_co_ci_u32 in RDNA
   ("v_subbrev_co_u32",    False, False, dst(1, VCC), src(1, 1, VCC), op(0x2a, gfx8=0x1e, gfx10=0x2a, gfx11=0x22)), # v_subrev_co_ci_u32 in RDNA
   ("v_fmac_f32",          True, True, dst(1), src(1, 1, 1), op(gfx10=0x2b)),
   ("v_fmamk_f32",         False, False, dst(1), src(1, 1, 1), op(gfx10=0x2c)),
   ("v_fmaak_f32",         False, False, dst(1), src(1, 1, 1), op(gfx10=0x2d)),
   ("v_cvt_pkrtz_f16_f32", True, False, dst(1), src(1, 1), op(0x2f, gfx8=-1, gfx10=0x2f)), #v_cvt_pk_rtz_f16_f32 in GFX11
   ("v_add_f16",           True, True, dst(1), src(1, 1), op(gfx8=0x1f, gfx10=0x32)),
   ("v_sub_f16",           True, True, dst(1), src(1, 1), op(gfx8=0x20, gfx10=0x33)),
   ("v_subrev_f16",        True, True, dst(1), src(1, 1), op(gfx8=0x21, gfx10=0x34)),
   ("v_mul_f16",           True, True, dst(1), src(1, 1), op(gfx8=0x22, gfx10=0x35)),
   ("v_mac_f16",           True, True, dst(1), src(1, 1, 1), op(gfx8=0x23, gfx10=-1)),
   ("v_madmk_f16",         False, False, dst(1), src(1, 1, 1), op(gfx8=0x24, gfx10=-1)),
   ("v_madak_f16",         False, False, dst(1), src(1, 1, 1), op(gfx8=0x25, gfx10=-1)),
   ("v_add_u16",           False, False, dst(1), src(1, 1), op(gfx8=0x26, gfx10=-1)),
   ("v_sub_u16",           False, False, dst(1), src(1, 1), op(gfx8=0x27, gfx10=-1)),
   ("v_subrev_u16",        False, False, dst(1), src(1, 1), op(gfx8=0x28, gfx10=-1)),
   ("v_mul_lo_u16",        False, False, dst(1), src(1, 1), op(gfx8=0x29, gfx10=-1)),
   ("v_lshlrev_b16",       False, False, dst(1), src(1, 1), op(gfx8=0x2a, gfx10=-1)),
   ("v_lshrrev_b16",       False, False, dst(1), src(1, 1), op(gfx8=0x2b, gfx10=-1)),
   ("v_ashrrev_i16",       False, False, dst(1), src(1, 1), op(gfx8=0x2c, gfx10=-1)),
   ("v_max_f16",           True, True, dst(1), src(1, 1), op(gfx8=0x2d, gfx10=0x39)),
   ("v_min_f16",           True, True, dst(1), src(1, 1), op(gfx8=0x2e, gfx10=0x3a)),
   ("v_max_u16",           False, False, dst(1), src(1, 1), op(gfx8=0x2f, gfx10=-1)),
   ("v_max_i16",           False, False, dst(1), src(1, 1), op(gfx8=0x30, gfx10=-1)),
   ("v_min_u16",           False, False, dst(1), src(1, 1), op(gfx8=0x31, gfx10=-1)),
   ("v_min_i16",           False, False, dst(1), src(1, 1), op(gfx8=0x32, gfx10=-1)),
   ("v_ldexp_f16",         False, True, dst(1), src(1, 1), op(gfx8=0x33, gfx10=0x3b)),
   ("v_add_u32",           False, False, dst(1), src(1, 1), op(gfx9=0x34, gfx10=0x25)), # called v_add_nc_u32 in RDNA
   ("v_sub_u32",           False, False, dst(1), src(1, 1), op(gfx9=0x35, gfx10=0x26)), # called v_sub_nc_u32 in RDNA
   ("v_subrev_u32",        False, False, dst(1), src(1, 1), op(gfx9=0x36, gfx10=0x27)), # called v_subrev_nc_u32 in RDNA
   ("v_fmac_f16",          True, True, dst(1), src(1, 1, 1), op(gfx10=0x36)),
   ("v_fmamk_f16",         False, False, dst(1), src(1, 1, 1), op(gfx10=0x37)),
   ("v_fmaak_f16",         False, False, dst(1), src(1, 1, 1), op(gfx10=0x38)),
   ("v_pk_fmac_f16",       False, False, dst(1), src(1, 1, 1), op(gfx10=0x3c)),
   ("v_dot2c_f32_f16",     False, False, dst(1), src(1, 1, 1), op(gfx9=0x37, gfx10=0x02)), #v_dot2acc_f32_f16 in GFX11
}
for (name, in_mod, out_mod, defs, ops, num) in VOP2:
   insn(name, num, Format.VOP2, InstrClass.Valu32, in_mod, out_mod, definitions = defs, operands = ops)


# VOP1 instructions: instructions with 1 input and 1 output
VOP1 = {
   ("v_nop",                      False, False, dst(), src(), op(0x00)),
   ("v_mov_b32",                  False, False, dst(1), src(1), op(0x01)),
   ("v_readfirstlane_b32",        False, False, dst(1), src(1), op(0x02)),
   ("v_cvt_i32_f64",              True, False, dst(1), src(2), op(0x03), InstrClass.ValuDoubleConvert),
   ("v_cvt_f64_i32",              False, True, dst(2), src(1), op(0x04), InstrClass.ValuDoubleConvert),
   ("v_cvt_f32_i32",              False, True, dst(1), src(1), op(0x05)),
   ("v_cvt_f32_u32",              False, True, dst(1), src(1), op(0x06)),
   ("v_cvt_u32_f32",              True, False, dst(1), src(1), op(0x07)),
   ("v_cvt_i32_f32",              True, False, dst(1), src(1), op(0x08)),
   ("v_cvt_f16_f32",              True, True, dst(1), src(1), op(0x0a)),
   ("p_cvt_f16_f32_rtne",         True, True, dst(1), src(1), op(-1)),
   ("v_cvt_f32_f16",              True, True, dst(1), src(1), op(0x0b)),
   ("v_cvt_rpi_i32_f32",          True, False, dst(1), src(1), op(0x0c)), #v_cvt_nearest_i32_f32 in GFX11
   ("v_cvt_flr_i32_f32",          True, False, dst(1), src(1), op(0x0d)),#v_cvt_floor_i32_f32 in GFX11
   ("v_cvt_off_f32_i4",           False, True, dst(1), src(1), op(0x0e)),
   ("v_cvt_f32_f64",              True, True, dst(1), src(2), op(0x0f), InstrClass.ValuDoubleConvert),
   ("v_cvt_f64_f32",              True, True, dst(2), src(1), op(0x10), InstrClass.ValuDoubleConvert),
   ("v_cvt_f32_ubyte0",           False, True, dst(1), src(1), op(0x11)),
   ("v_cvt_f32_ubyte1",           False, True, dst(1), src(1), op(0x12)),
   ("v_cvt_f32_ubyte2",           False, True, dst(1), src(1), op(0x13)),
   ("v_cvt_f32_ubyte3",           False, True, dst(1), src(1), op(0x14)),
   ("v_cvt_u32_f64",              True, False, dst(1), src(2), op(0x15), InstrClass.ValuDoubleConvert),
   ("v_cvt_f64_u32",              False, True, dst(2), src(1), op(0x16), InstrClass.ValuDoubleConvert),
   ("v_trunc_f64",                True, True, dst(2), src(2), op(gfx7=0x17), InstrClass.ValuDouble),
   ("v_ceil_f64",                 True, True, dst(2), src(2), op(gfx7=0x18), InstrClass.ValuDouble),
   ("v_rndne_f64",                True, True, dst(2), src(2), op(gfx7=0x19), InstrClass.ValuDouble),
   ("v_floor_f64",                True, True, dst(2), src(2), op(gfx7=0x1a), InstrClass.ValuDouble),
   ("v_pipeflush",                False, False, dst(), src(), op(gfx10=0x1b)),
   ("v_fract_f32",                True, True, dst(1), src(1), op(0x20, gfx8=0x1b, gfx10=0x20)),
   ("v_trunc_f32",                True, True, dst(1), src(1), op(0x21, gfx8=0x1c, gfx10=0x21)),
   ("v_ceil_f32",                 True, True, dst(1), src(1), op(0x22, gfx8=0x1d, gfx10=0x22)),
   ("v_rndne_f32",                True, True, dst(1), src(1), op(0x23, gfx8=0x1e, gfx10=0x23)),
   ("v_floor_f32",                True, True, dst(1), src(1), op(0x24, gfx8=0x1f, gfx10=0x24)),
   ("v_exp_f32",                  True, True, dst(1), src(1), op(0x25, gfx8=0x20, gfx10=0x25), InstrClass.ValuTranscendental32),
   ("v_log_clamp_f32",            True, True, dst(1), src(1), op(0x26, gfx8=-1), InstrClass.ValuTranscendental32),
   ("v_log_f32",                  True, True, dst(1), src(1), op(0x27, gfx8=0x21, gfx10=0x27), InstrClass.ValuTranscendental32),
   ("v_rcp_clamp_f32",            True, True, dst(1), src(1), op(0x28, gfx8=-1), InstrClass.ValuTranscendental32),
   ("v_rcp_legacy_f32",           True, True, dst(1), src(1), op(0x29, gfx8=-1), InstrClass.ValuTranscendental32),
   ("v_rcp_f32",                  True, True, dst(1), src(1), op(0x2a, gfx8=0x22, gfx10=0x2a), InstrClass.ValuTranscendental32),
   ("v_rcp_iflag_f32",            True, True, dst(1), src(1), op(0x2b, gfx8=0x23, gfx10=0x2b), InstrClass.ValuTranscendental32),
   ("v_rsq_clamp_f32",            True, True, dst(1), src(1), op(0x2c, gfx8=-1), InstrClass.ValuTranscendental32),
   ("v_rsq_legacy_f32",           True, True, dst(1), src(1), op(0x2d, gfx8=-1), InstrClass.ValuTranscendental32),
   ("v_rsq_f32",                  True, True, dst(1), src(1), op(0x2e, gfx8=0x24, gfx10=0x2e), InstrClass.ValuTranscendental32),
   ("v_rcp_f64",                  True, True, dst(2), src(2), op(0x2f, gfx8=0x25, gfx10=0x2f), InstrClass.ValuDoubleTranscendental),
   ("v_rcp_clamp_f64",            True, True, dst(2), src(2), op(0x30, gfx8=-1), InstrClass.ValuDoubleTranscendental),
   ("v_rsq_f64",                  True, True, dst(2), src(2), op(0x31, gfx8=0x26, gfx10=0x31), InstrClass.ValuDoubleTranscendental),
   ("v_rsq_clamp_f64",            True, True, dst(2), src(2), op(0x32, gfx8=-1), InstrClass.ValuDoubleTranscendental),
   ("v_sqrt_f32",                 True, True, dst(1), src(1), op(0x33, gfx8=0x27, gfx10=0x33), InstrClass.ValuTranscendental32),
   ("v_sqrt_f64",                 True, True, dst(2), src(2), op(0x34, gfx8=0x28, gfx10=0x34), InstrClass.ValuDoubleTranscendental),
   ("v_sin_f32",                  True, True, dst(1), src(1), op(0x35, gfx8=0x29, gfx10=0x35), InstrClass.ValuTranscendental32),
   ("v_cos_f32",                  True, True, dst(1), src(1), op(0x36, gfx8=0x2a, gfx10=0x36), InstrClass.ValuTranscendental32),
   ("v_not_b32",                  False, False, dst(1), src(1), op(0x37, gfx8=0x2b, gfx10=0x37)),
   ("v_bfrev_b32",                False, False, dst(1), src(1), op(0x38, gfx8=0x2c, gfx10=0x38)),
   ("v_ffbh_u32",                 False, False, dst(1), src(1), op(0x39, gfx8=0x2d, gfx10=0x39)), #v_clz_i32_u32 in GFX11
   ("v_ffbl_b32",                 False, False, dst(1), src(1), op(0x3a, gfx8=0x2e, gfx10=0x3a)), #v_ctz_i32_b32 in GFX11
   ("v_ffbh_i32",                 False, False, dst(1), src(1), op(0x3b, gfx8=0x2f, gfx10=0x3b)), #v_cls_i32 in GFX11
   ("v_frexp_exp_i32_f64",        True, False, dst(1), src(2), op(0x3c, gfx8=0x30, gfx10=0x3c), InstrClass.ValuDouble),
   ("v_frexp_mant_f64",           True, False, dst(2), src(2), op(0x3d, gfx8=0x31, gfx10=0x3d), InstrClass.ValuDouble),
   ("v_fract_f64",                True, True, dst(2), src(2), op(0x3e, gfx8=0x32, gfx10=0x3e), InstrClass.ValuDouble),
   ("v_frexp_exp_i32_f32",        True, False, dst(1), src(1), op(0x3f, gfx8=0x33, gfx10=0x3f)),
   ("v_frexp_mant_f32",           True, False, dst(1), src(1), op(0x40, gfx8=0x34, gfx10=0x40)),
   ("v_clrexcp",                  False, False, dst(), src(), op(0x41, gfx8=0x35, gfx10=0x41, gfx11=-1)),
   ("v_movreld_b32",              False, False, dst(1), src(1, M0), op(0x42, gfx8=0x36, gfx9=-1, gfx10=0x42)),
   ("v_movrels_b32",              False, False, dst(1), src(1, M0), op(0x43, gfx8=0x37, gfx9=-1, gfx10=0x43)),
   ("v_movrelsd_b32",             False, False, dst(1), src(1, M0), op(0x44, gfx8=0x38, gfx9=-1, gfx10=0x44)),
   ("v_movrelsd_2_b32",           False, False, dst(1), src(1, M0), op(gfx10=0x48)),
   ("v_screen_partition_4se_b32", False, False, dst(1), src(1), op(gfx9=0x37, gfx10=-1)),
   ("v_cvt_f16_u16",              False, True, dst(1), src(1), op(gfx8=0x39, gfx10=0x50)),
   ("v_cvt_f16_i16",              False, True, dst(1), src(1), op(gfx8=0x3a, gfx10=0x51)),
   ("v_cvt_u16_f16",              True, False, dst(1), src(1), op(gfx8=0x3b, gfx10=0x52)),
   ("v_cvt_i16_f16",              True, False, dst(1), src(1), op(gfx8=0x3c, gfx10=0x53)),
   ("v_rcp_f16",                  True, True, dst(1), src(1), op(gfx8=0x3d, gfx10=0x54), InstrClass.ValuTranscendental32),
   ("v_sqrt_f16",                 True, True, dst(1), src(1), op(gfx8=0x3e, gfx10=0x55), InstrClass.ValuTranscendental32),
   ("v_rsq_f16",                  True, True, dst(1), src(1), op(gfx8=0x3f, gfx10=0x56), InstrClass.ValuTranscendental32),
   ("v_log_f16",                  True, True, dst(1), src(1), op(gfx8=0x40, gfx10=0x57), InstrClass.ValuTranscendental32),
   ("v_exp_f16",                  True, True, dst(1), src(1), op(gfx8=0x41, gfx10=0x58), InstrClass.ValuTranscendental32),
   ("v_frexp_mant_f16",           True, False, dst(1), src(1), op(gfx8=0x42, gfx10=0x59)),
   ("v_frexp_exp_i16_f16",        True, False, dst(1), src(1), op(gfx8=0x43, gfx10=0x5a)),
   ("v_floor_f16",                True, True, dst(1), src(1), op(gfx8=0x44, gfx10=0x5b)),
   ("v_ceil_f16",                 True, True, dst(1), src(1), op(gfx8=0x45, gfx10=0x5c)),
   ("v_trunc_f16",                True, True, dst(1), src(1), op(gfx8=0x46, gfx10=0x5d)),
   ("v_rndne_f16",                True, True, dst(1), src(1), op(gfx8=0x47, gfx10=0x5e)),
   ("v_fract_f16",                True, True, dst(1), src(1), op(gfx8=0x48, gfx10=0x5f)),
   ("v_sin_f16",                  True, True, dst(1), src(1), op(gfx8=0x49, gfx10=0x60), InstrClass.ValuTranscendental32),
   ("v_cos_f16",                  True, True, dst(1), src(1), op(gfx8=0x4a, gfx10=0x61), InstrClass.ValuTranscendental32),
   ("v_exp_legacy_f32",           True, True, dst(1), src(1), op(gfx7=0x46, gfx8=0x4b, gfx10=-1), InstrClass.ValuTranscendental32),
   ("v_log_legacy_f32",           True, True, dst(1), src(1), op(gfx7=0x45, gfx8=0x4c, gfx10=-1), InstrClass.ValuTranscendental32),
   ("v_sat_pk_u8_i16",            False, False, dst(1), src(1), op(gfx9=0x4f, gfx10=0x62)),
   ("v_cvt_norm_i16_f16",         True, False, dst(1), src(1), op(gfx9=0x4d, gfx10=0x63)),
   ("v_cvt_norm_u16_f16",         True, False, dst(1), src(1), op(gfx9=0x4e, gfx10=0x64)),
   ("v_swap_b32",                 False, False, dst(1, 1), src(1, 1), op(gfx9=0x51, gfx10=0x65)),
   ("v_swaprel_b32",              False, False, dst(1, 1), src(1, 1, M0), op(gfx10=0x68)),
   ("v_permlane64_b32",           False, False, dst(1), src(1), op(gfx11=0x67)), #cannot use VOP3
   ("v_not_b16",                  False, False, dst(1), src(1), op(gfx11=0x69)),
   ("v_cvt_i32_i16",              False, False, dst(1), src(1), op(gfx11=0x6a)),
   ("v_cvt_u32_u16",              False, False, dst(1), src(1), op(gfx11=0x6b)),
   ("v_mov_b16",                  True, False, dst(1), src(1), op(gfx11=0x1c)),
}
for (name, in_mod, out_mod, defs, ops, num, cls) in default_class(VOP1, InstrClass.Valu32):
   insn(name, num, Format.VOP1, cls, in_mod, out_mod, definitions = defs, operands = ops)


# VOPC instructions:

VOPC_CLASS = {
   ("v_cmp_class_f32",  dst(VCC), src(1, 1), op(0x88, gfx8=0x10, gfx10=0x88, gfx11=0x7e)),
   ("v_cmp_class_f16",  dst(VCC), src(1, 1), op(gfx8=0x14, gfx10=0x8f, gfx11=0x7d)),
   ("v_cmpx_class_f32", dst(EXEC), src(1, 1), op(0x98, gfx8=0x11, gfx10=0x98, gfx11=0xfe)),
   ("v_cmpx_class_f16", dst(EXEC), src(1, 1), op(gfx8=0x15, gfx10=0x9f, gfx11=0xfd)),
   ("v_cmp_class_f64",  dst(VCC), src(2, 1), op(0xa8, gfx8=0x12, gfx10=0xa8, gfx11=0x7f), InstrClass.ValuDouble),
   ("v_cmpx_class_f64", dst(EXEC), src(2, 1), op(0xb8, gfx8=0x13, gfx10=0xb8, gfx11=0xff), InstrClass.ValuDouble),
}
for (name, defs, ops, num, cls) in default_class(VOPC_CLASS, InstrClass.Valu32):
    insn(name, num, Format.VOPC, cls, True, False, definitions = defs, operands = ops)

VopcDataType = collections.namedtuple('VopcDataTypeInfo',
                                      ['kind', 'size', 'gfx6', 'gfx8', 'gfx10', 'gfx11'])

#                  kind, size, gfx6, gfx8, gfx10,gfx11
F16 = VopcDataType('f',  16,      0, 0x20, 0xc8, 0x00)
F32 = VopcDataType('f',  32,   0x00, 0x40, 0x00, 0x10)
F64 = VopcDataType('f',  64,   0x20, 0x60, 0x20, 0x20)
I16 = VopcDataType('i',  16,      0, 0xa0, 0x88, 0x30)
I32 = VopcDataType('i',  32,   0x80, 0xc0, 0x80, 0x40)
I64 = VopcDataType('i',  64,   0xa0, 0xe0, 0xa0, 0x50)
U16 = VopcDataType('u',  16,      0, 0xa8, 0xa8, 0x38)
U32 = VopcDataType('u',  32,   0xc0, 0xc8, 0xc0, 0x48)
U64 = VopcDataType('u',  64,   0xe0, 0xe8, 0xe0, 0x58)
dtypes = [F16, F32, F64, I16, I32, I64, U16, U32, U64]

COMPF = ["f", "lt", "eq", "le", "gt", "lg", "ge", "o", "u", "nge", "nlg", "ngt", "nle", "neq", "nlt", "tru"]
COMPI = ["f", "lt", "eq", "le", "gt", "lg", "ge", "tru"]
for comp, dtype, cmps, cmpx in itertools.product(range(16), dtypes, range(1), range(2)):
   if (comp >= 8 or cmps) and dtype.kind != 'f':
      continue

   name = COMPF[comp] if dtype.kind == 'f' else COMPI[comp]
   name = 'v_cmp{}{}_{}_{}{}'.format('s' if cmps else '', 'x' if cmpx else '', name, dtype.kind, dtype.size)

   gfx6 = comp | (cmpx<<4) | (cmps<<6) | dtype.gfx6
   gfx8 = comp | (cmpx<<4) | dtype.gfx8
   if dtype == F16:
      gfx10 = (comp & 0x7) | ((comp & 0x8) << 2) | (cmpx<<4) | dtype.gfx10
   else:
      gfx10 = comp | (cmpx<<4) | dtype.gfx10
   gfx11 = comp | (cmpx<<7) | dtype.gfx11

   if cmps:
      gfx8 = -1
      gfx10 = -1
      gfx11 = -1

   if dtype.size == 16:
      gfx6 = -1

   if dtype in [I16, U16] and comp in [0, 7]:
      gfx10 = -1
      gfx11 = -1

   cls = InstrClass.Valu32
   if dtype == F64:
      cls = InstrClass.ValuDouble
   elif dtype in [I64, U64]:
      cls = InstrClass.Valu64

   enc = Opcode(gfx6, gfx6, gfx8, gfx8, gfx10, gfx11)
   insn(name, enc, Format.VOPC, cls, dtype.kind == 'f', False,
        definitions = dst(EXEC if cmpx else VCC),
        operands = src(2, 2) if dtype.size == 64 else src(1, 1))


# VOPP instructions: packed 16bit instructions - 2 or 3 inputs and 1 output
VOPP = {
   ("v_pk_mad_i16",     False, dst(1), src(1, 1, 1), op(gfx9=0x00)),
   ("v_pk_mul_lo_u16",  False, dst(1), src(1, 1), op(gfx9=0x01)),
   ("v_pk_add_i16",     False, dst(1), src(1, 1), op(gfx9=0x02)),
   ("v_pk_sub_i16",     False, dst(1), src(1, 1), op(gfx9=0x03)),
   ("v_pk_lshlrev_b16", False, dst(1), src(1, 1), op(gfx9=0x04)),
   ("v_pk_lshrrev_b16", False, dst(1), src(1, 1), op(gfx9=0x05)),
   ("v_pk_ashrrev_i16", False, dst(1), src(1, 1), op(gfx9=0x06)),
   ("v_pk_max_i16",     False, dst(1), src(1, 1), op(gfx9=0x07)),
   ("v_pk_min_i16",     False, dst(1), src(1, 1), op(gfx9=0x08)),
   ("v_pk_mad_u16",     False, dst(1), src(1, 1, 1), op(gfx9=0x09)),
   ("v_pk_add_u16",     False, dst(1), src(1, 1), op(gfx9=0x0a)),
   ("v_pk_sub_u16",     False, dst(1), src(1, 1), op(gfx9=0x0b)),
   ("v_pk_max_u16",     False, dst(1), src(1, 1), op(gfx9=0x0c)),
   ("v_pk_min_u16",     False, dst(1), src(1, 1), op(gfx9=0x0d)),
   ("v_pk_fma_f16",     True, dst(1), src(1, 1, 1), op(gfx9=0x0e)),
   ("v_pk_add_f16",     True, dst(1), src(1, 1), op(gfx9=0x0f)),
   ("v_pk_mul_f16",     True, dst(1), src(1, 1), op(gfx9=0x10)),
   ("v_pk_min_f16",     True, dst(1), src(1, 1), op(gfx9=0x11)),
   ("v_pk_max_f16",     True, dst(1), src(1, 1), op(gfx9=0x12)),
   ("v_fma_mix_f32",    True, dst(1), src(1, 1, 1), op(gfx9=0x20)), # v_mad_mix_f32 in VEGA ISA, v_fma_mix_f32 in RDNA ISA
   ("v_fma_mixlo_f16",  True, dst(1), src(1, 1, 1), op(gfx9=0x21)), # v_mad_mixlo_f16 in VEGA ISA, v_fma_mixlo_f16 in RDNA ISA
   ("v_fma_mixhi_f16",  True, dst(1), src(1, 1, 1), op(gfx9=0x22)), # v_mad_mixhi_f16 in VEGA ISA, v_fma_mixhi_f16 in RDNA ISA
   ("v_dot2_i32_i16",      False, dst(1), src(1, 1, 1), op(gfx9=0x26, gfx10=0x14, gfx11=-1)),
   ("v_dot2_u32_u16",      False, dst(1), src(1, 1, 1), op(gfx9=0x27, gfx10=0x15, gfx11=-1)),
   ("v_dot4_i32_iu8",      False, dst(1), src(1, 1, 1), op(gfx11=0x16)),
   ("v_dot4_i32_i8",       False, dst(1), src(1, 1, 1), op(gfx9=0x28, gfx10=0x16, gfx11=-1)),
   ("v_dot4_u32_u8",       False, dst(1), src(1, 1, 1), op(gfx9=0x29, gfx10=0x17)),
   ("v_dot8_i32_iu4",      False, dst(1), src(1, 1, 1), op(gfx11=0x18)),
   ("v_dot8_u32_u4",       False, dst(1), src(1, 1, 1), op(gfx9=0x2b, gfx10=0x19)),
   ("v_dot2_f32_f16",      False, dst(1), src(1, 1, 1), op(gfx9=0x23, gfx10=0x13)),
   ("v_dot2_f32_bf16",     False, dst(1), src(1, 1, 1), op(gfx11=0x1a)),
   ("v_wmma_f32_16x16x16_f16",       False, dst(), src(), op(gfx11=0x40), InstrClass.WMMA),
   ("v_wmma_f32_16x16x16_bf16",      False, dst(), src(), op(gfx11=0x41), InstrClass.WMMA),
   ("v_wmma_f16_16x16x16_f16",       False, dst(), src(), op(gfx11=0x42), InstrClass.WMMA),
   ("v_wmma_bf16_16x16x16_bf16",     False, dst(), src(), op(gfx11=0x43), InstrClass.WMMA),
   ("v_wmma_i32_16x16x16_iu8",       False, dst(), src(), op(gfx11=0x44), InstrClass.WMMA),
   ("v_wmma_i32_16x16x16_iu4",       False, dst(), src(), op(gfx11=0x45), InstrClass.WMMA),
}
for (name, modifiers, defs, ops, num, cls) in default_class(VOPP, InstrClass.Valu32):
   insn(name, num, Format.VOP3P, cls, modifiers, modifiers, definitions = defs, operands = ops)


# VINTRP (GFX6 - GFX10.3) instructions:
VINTRP = {
   ("v_interp_p1_f32",  dst(1), src(1, M0), op(0x00, gfx11=-1)),
   ("v_interp_p2_f32",  dst(1), src(1, M0, 1), op(0x01, gfx11=-1)),
   ("v_interp_mov_f32", dst(1), src(1, M0), op(0x02, gfx11=-1)),
}
for (name, defs, ops, num) in VINTRP:
   insn(name, num, Format.VINTRP, InstrClass.Valu32, definitions = defs, operands = ops)


# VINTERP (GFX11+) instructions:
VINTERP = {
   ("v_interp_p10_f32_inreg",         op(gfx11=0x00)),
   ("v_interp_p2_f32_inreg",          op(gfx11=0x01)),
   ("v_interp_p10_f16_f32_inreg",     op(gfx11=0x02)),
   ("v_interp_p2_f16_f32_inreg",      op(gfx11=0x03)),
   ("v_interp_p10_rtz_f16_f32_inreg", op(gfx11=0x04)),
   ("v_interp_p2_rtz_f16_f32_inreg",  op(gfx11=0x05)),
}
for (name, num) in VINTERP:
   insn(name, num, Format.VINTERP_INREG, InstrClass.Valu32, False, True, definitions = dst(1), operands = src(1, 1, 1))


# VOP3 instructions: 3 inputs, 1 output
# VOP3b instructions: have a unique scalar output, e.g. VOP2 with vcc out
VOP3 = {
   ("v_mad_legacy_f32",        True, True, dst(1), src(1, 1, 1), op(0x140, gfx8=0x1c0, gfx10=0x140, gfx11=-1)), # GFX6-GFX10
   ("v_mad_f32",               True, True, dst(1), src(1, 1, 1), op(0x141, gfx8=0x1c1, gfx10=0x141, gfx11=-1)),
   ("v_mad_i32_i24",           False, False, dst(1), src(1, 1, 1), op(0x142, gfx8=0x1c2, gfx10=0x142, gfx11=0x20a)),
   ("v_mad_u32_u24",           False, False, dst(1), src(1, 1, 1), op(0x143, gfx8=0x1c3, gfx10=0x143, gfx11=0x20b)),
   ("v_cubeid_f32",            True, True, dst(1), src(1, 1, 1), op(0x144, gfx8=0x1c4, gfx10=0x144, gfx11=0x20c)),
   ("v_cubesc_f32",            True, True, dst(1), src(1, 1, 1), op(0x145, gfx8=0x1c5, gfx10=0x145, gfx11=0x20d)),
   ("v_cubetc_f32",            True, True, dst(1), src(1, 1, 1), op(0x146, gfx8=0x1c6, gfx10=0x146, gfx11=0x20e)),
   ("v_cubema_f32",            True, True, dst(1), src(1, 1, 1), op(0x147, gfx8=0x1c7, gfx10=0x147, gfx11=0x20f)),
   ("v_bfe_u32",               False, False, dst(1), src(1, 1, 1), op(0x148, gfx8=0x1c8, gfx10=0x148, gfx11=0x210)),
   ("v_bfe_i32",               False, False, dst(1), src(1, 1, 1), op(0x149, gfx8=0x1c9, gfx10=0x149, gfx11=0x211)),
   ("v_bfi_b32",               False, False, dst(1), src(1, 1, 1), op(0x14a, gfx8=0x1ca, gfx10=0x14a, gfx11=0x212)),
   ("v_fma_f32",               True, True, dst(1), src(1, 1, 1), op(0x14b, gfx8=0x1cb, gfx10=0x14b, gfx11=0x213), InstrClass.ValuFma),
   ("v_fma_f64",               True, True, dst(2), src(2, 2, 2), op(0x14c, gfx8=0x1cc, gfx10=0x14c, gfx11=0x214), InstrClass.ValuDouble),
   ("v_lerp_u8",               False, False, dst(1), src(1, 1, 1), op(0x14d, gfx8=0x1cd, gfx10=0x14d, gfx11=0x215)),
   ("v_alignbit_b32",          False, False, dst(1), src(1, 1, 1), op(0x14e, gfx8=0x1ce, gfx10=0x14e, gfx11=0x216)),
   ("v_alignbyte_b32",         False, False, dst(1), src(1, 1, 1), op(0x14f, gfx8=0x1cf, gfx10=0x14f, gfx11=0x217)),
   ("v_mullit_f32",            True, True, dst(1), src(1, 1, 1), op(0x150, gfx8=-1, gfx10=0x150, gfx11=0x218)),
   ("v_min3_f32",              True, True, dst(1), src(1, 1, 1), op(0x151, gfx8=0x1d0, gfx10=0x151, gfx11=0x219)),
   ("v_min3_i32",              False, False, dst(1), src(1, 1, 1), op(0x152, gfx8=0x1d1, gfx10=0x152, gfx11=0x21a)),
   ("v_min3_u32",              False, False, dst(1), src(1, 1, 1), op(0x153, gfx8=0x1d2, gfx10=0x153, gfx11=0x21b)),
   ("v_max3_f32",              True, True, dst(1), src(1, 1, 1), op(0x154, gfx8=0x1d3, gfx10=0x154, gfx11=0x21c)),
   ("v_max3_i32",              False, False, dst(1), src(1, 1, 1), op(0x155, gfx8=0x1d4, gfx10=0x155, gfx11=0x21d)),
   ("v_max3_u32",              False, False, dst(1), src(1, 1, 1), op(0x156, gfx8=0x1d5, gfx10=0x156, gfx11=0x21e)),
   ("v_med3_f32",              True, True, dst(1), src(1, 1, 1), op(0x157, gfx8=0x1d6, gfx10=0x157, gfx11=0x21f)),
   ("v_med3_i32",              False, False, dst(1), src(1, 1, 1), op(0x158, gfx8=0x1d7, gfx10=0x158, gfx11=0x220)),
   ("v_med3_u32",              False, False, dst(1), src(1, 1, 1), op(0x159, gfx8=0x1d8, gfx10=0x159, gfx11=0x221)),
   ("v_sad_u8",                False, False, dst(1), src(1, 1, 1), op(0x15a, gfx8=0x1d9, gfx10=0x15a, gfx11=0x222)),
   ("v_sad_hi_u8",             False, False, dst(1), src(1, 1, 1), op(0x15b, gfx8=0x1da, gfx10=0x15b, gfx11=0x223)),
   ("v_sad_u16",               False, False, dst(1), src(1, 1, 1), op(0x15c, gfx8=0x1db, gfx10=0x15c, gfx11=0x224)),
   ("v_sad_u32",               False, False, dst(1), src(1, 1, 1), op(0x15d, gfx8=0x1dc, gfx10=0x15d, gfx11=0x225)),
   ("v_cvt_pk_u8_f32",         True, False, dst(1), src(1, 1, 1), op(0x15e, gfx8=0x1dd, gfx10=0x15e, gfx11=0x226)),
   ("v_div_fixup_f32",         True, True, dst(1), src(1, 1, 1), op(0x15f, gfx8=0x1de, gfx10=0x15f, gfx11=0x227)),
   ("v_div_fixup_f64",         True, True, dst(2), src(2, 2, 2), op(0x160, gfx8=0x1df, gfx10=0x160, gfx11=0x228)),
   ("v_lshl_b64",              False, False, dst(2), src(2, 1), op(0x161, gfx8=-1), InstrClass.Valu64),
   ("v_lshr_b64",              False, False, dst(2), src(2, 1), op(0x162, gfx8=-1), InstrClass.Valu64),
   ("v_ashr_i64",              False, False, dst(2), src(2, 1), op(0x163, gfx8=-1), InstrClass.Valu64),
   ("v_add_f64",               True, True, dst(2), src(2, 2), op(0x164, gfx8=0x280, gfx10=0x164, gfx11=0x327), InstrClass.ValuDoubleAdd),
   ("v_mul_f64",               True, True, dst(2), src(2, 2), op(0x165, gfx8=0x281, gfx10=0x165, gfx11=0x328), InstrClass.ValuDouble),
   ("v_min_f64",               True, True, dst(2), src(2, 2), op(0x166, gfx8=0x282, gfx10=0x166, gfx11=0x329), InstrClass.ValuDouble),
   ("v_max_f64",               True, True, dst(2), src(2, 2), op(0x167, gfx8=0x283, gfx10=0x167, gfx11=0x32a), InstrClass.ValuDouble),
   ("v_ldexp_f64",             False, True, dst(2), src(2, 1), op(0x168, gfx8=0x284, gfx10=0x168, gfx11=0x32b), InstrClass.ValuDouble), # src1 can take input modifiers
   ("v_mul_lo_u32",            False, False, dst(1), src(1, 1), op(0x169, gfx8=0x285, gfx10=0x169, gfx11=0x32c), InstrClass.ValuQuarterRate32),
   ("v_mul_hi_u32",            False, False, dst(1), src(1, 1), op(0x16a, gfx8=0x286, gfx10=0x16a, gfx11=0x32d), InstrClass.ValuQuarterRate32),
   ("v_mul_lo_i32",            False, False, dst(1), src(1, 1), op(0x16b, gfx8=0x285, gfx10=0x16b, gfx11=0x32c), InstrClass.ValuQuarterRate32), # identical to v_mul_lo_u32
   ("v_mul_hi_i32",            False, False, dst(1), src(1, 1), op(0x16c, gfx8=0x287, gfx10=0x16c, gfx11=0x32e), InstrClass.ValuQuarterRate32),
   ("v_div_scale_f32",         True, True, dst(1, VCC), src(1, 1, 1), op(0x16d, gfx8=0x1e0, gfx10=0x16d, gfx11=0x2fc)),
   ("v_div_scale_f64",         True, True, dst(2, VCC), src(2, 2, 2), op(0x16e, gfx8=0x1e1, gfx10=0x16e, gfx11=0x2fd), InstrClass.ValuDouble),
   ("v_div_fmas_f32",          True, True, dst(1), src(1, 1, 1, VCC), op(0x16f, gfx8=0x1e2, gfx10=0x16f, gfx11=0x237)),
   ("v_div_fmas_f64",          True, True, dst(2), src(2, 2, 2, VCC), op(0x170, gfx8=0x1e3, gfx10=0x170, gfx11=0x238), InstrClass.ValuDouble),
   ("v_msad_u8",               False, False, dst(1), src(1, 1, 1), op(0x171, gfx8=0x1e4, gfx10=0x171, gfx11=0x239)),
   ("v_qsad_pk_u16_u8",        False, False, dst(2), src(2, 1, 2), op(0x172, gfx8=0x1e5, gfx10=0x172, gfx11=0x23a)),
   ("v_mqsad_pk_u16_u8",       False, False, dst(2), src(2, 1, 2), op(0x173, gfx8=0x1e6, gfx10=0x173, gfx11=0x23b)),
   ("v_trig_preop_f64",        False, False, dst(2), src(2, 2), op(0x174, gfx8=0x292, gfx10=0x174, gfx11=0x32f), InstrClass.ValuDouble),
   ("v_mqsad_u32_u8",          False, False, dst(4), src(2, 1, 4), op(gfx7=0x175, gfx8=0x1e7, gfx10=0x175, gfx11=0x23d), InstrClass.ValuQuarterRate32),
   ("v_mad_u64_u32",           False, False, dst(2, VCC), src(1, 1, 2), op(gfx7=0x176, gfx8=0x1e8, gfx10=0x176, gfx11=0x2fe), InstrClass.Valu64),
   ("v_mad_i64_i32",           False, False, dst(2, VCC), src(1, 1, 2), op(gfx7=0x177, gfx8=0x1e9, gfx10=0x177, gfx11=0x2ff), InstrClass.Valu64),
   ("v_mad_legacy_f16",        True, True, dst(1), src(1, 1, 1), op(gfx8=0x1ea, gfx10=-1)),
   ("v_mad_legacy_u16",        False, False, dst(1), src(1, 1, 1), op(gfx8=0x1eb, gfx10=-1)),
   ("v_mad_legacy_i16",        False, False, dst(1), src(1, 1, 1), op(gfx8=0x1ec, gfx10=-1)),
   ("v_perm_b32",              False, False, dst(1), src(1, 1, 1), op(gfx8=0x1ed, gfx10=0x344, gfx11=0x244)),
   ("v_fma_legacy_f16",        True, True, dst(1), src(1, 1, 1), op(gfx8=0x1ee, gfx10=-1), InstrClass.ValuFma),
   ("v_div_fixup_legacy_f16",  True, True, dst(1), src(1, 1, 1), op(gfx8=0x1ef, gfx10=-1)),
   ("v_cvt_pkaccum_u8_f32",    True, False, dst(1), src(1, 1, 1), op(0x12c, gfx8=0x1f0, gfx10=-1)),
   ("v_mad_u32_u16",           False, False, dst(1), src(1, 1, 1), op(gfx9=0x1f1, gfx10=0x373, gfx11=0x259)),
   ("v_mad_i32_i16",           False, False, dst(1), src(1, 1, 1), op(gfx9=0x1f2, gfx10=0x375, gfx11=0x25a)),
   ("v_xad_u32",               False, False, dst(1), src(1, 1, 1), op(gfx9=0x1f3, gfx10=0x345, gfx11=0x245)),
   ("v_min3_f16",              True, True, dst(1), src(1, 1, 1), op(gfx9=0x1f4, gfx10=0x351, gfx11=0x249)),
   ("v_min3_i16",              False, False, dst(1), src(1, 1, 1), op(gfx9=0x1f5, gfx10=0x352, gfx11=0x24a)),
   ("v_min3_u16",              False, False, dst(1), src(1, 1, 1), op(gfx9=0x1f6, gfx10=0x353, gfx11=0x24b)),
   ("v_max3_f16",              True, True, dst(1), src(1, 1, 1), op(gfx9=0x1f7, gfx10=0x354, gfx11=0x24c)),
   ("v_max3_i16",              False, False, dst(1), src(1, 1, 1), op(gfx9=0x1f8, gfx10=0x355, gfx11=0x24d)),
   ("v_max3_u16",              False, False, dst(1), src(1, 1, 1), op(gfx9=0x1f9, gfx10=0x356, gfx11=0x24e)),
   ("v_med3_f16",              True, True, dst(1), src(1, 1, 1), op(gfx9=0x1fa, gfx10=0x357, gfx11=0x24f)),
   ("v_med3_i16",              False, False, dst(1), src(1, 1, 1), op(gfx9=0x1fb, gfx10=0x358, gfx11=0x250)),
   ("v_med3_u16",              False, False, dst(1), src(1, 1, 1), op(gfx9=0x1fc, gfx10=0x359, gfx11=0x251)),
   ("v_lshl_add_u32",          False, False, dst(1), src(1, 1, 1), op(gfx9=0x1fd, gfx10=0x346, gfx11=0x246)),
   ("v_add_lshl_u32",          False, False, dst(1), src(1, 1, 1), op(gfx9=0x1fe, gfx10=0x347, gfx11=0x247)),
   ("v_add3_u32",              False, False, dst(1), src(1, 1, 1), op(gfx9=0x1ff, gfx10=0x36d, gfx11=0x255)),
   ("v_lshl_or_b32",           False, False, dst(1), src(1, 1, 1), op(gfx9=0x200, gfx10=0x36f, gfx11=0x256)),
   ("v_and_or_b32",            False, False, dst(1), src(1, 1, 1), op(gfx9=0x201, gfx10=0x371, gfx11=0x257)),
   ("v_or3_b32",               False, False, dst(1), src(1, 1, 1), op(gfx9=0x202, gfx10=0x372, gfx11=0x258)),
   ("v_mad_f16",               True, True, dst(1), src(1, 1, 1), op(gfx9=0x203, gfx10=-1)),
   ("v_mad_u16",               False, False, dst(1), src(1, 1, 1), op(gfx9=0x204, gfx10=0x340, gfx11=0x241)),
   ("v_mad_i16",               False, False, dst(1), src(1, 1, 1), op(gfx9=0x205, gfx10=0x35e, gfx11=0x253)),
   ("v_fma_f16",               True, True, dst(1), src(1, 1, 1), op(gfx9=0x206, gfx10=0x34b, gfx11=0x248)),
   ("v_div_fixup_f16",         True, True, dst(1), src(1, 1, 1), op(gfx9=0x207, gfx10=0x35f, gfx11=0x254)),
   ("v_interp_p1ll_f16",       True, True, dst(1), src(1, M0), op(gfx8=0x274, gfx10=0x342, gfx11=-1)),
   ("v_interp_p1lv_f16",       True, True, dst(1), src(1, M0, 1), op(gfx8=0x275, gfx10=0x343, gfx11=-1)),
   ("v_interp_p2_legacy_f16",  True, True, dst(1), src(1, M0, 1), op(gfx8=0x276, gfx10=-1)),
   ("v_interp_p2_f16",         True, True, dst(1), src(1, M0, 1), op(gfx9=0x277, gfx10=0x35a, gfx11=-1)),
   ("v_interp_p2_hi_f16",      True, True, dst(1), src(1, M0, 1), op(gfx9=0x277, gfx10=0x35a, gfx11=-1)),
   ("v_ldexp_f32",             False, True, dst(1), src(1, 1), op(0x12b, gfx8=0x288, gfx10=0x362, gfx11=0x31c)),
   ("v_readlane_b32_e64",      False, False, dst(1), src(1, 1), op(gfx8=0x289, gfx10=0x360)),
   ("v_writelane_b32_e64",     False, False, dst(1), src(1, 1, 1), op(gfx8=0x28a, gfx10=0x361)),
   ("v_bcnt_u32_b32",          False, False, dst(1), src(1, 1), op(0x122, gfx8=0x28b, gfx10=0x364, gfx11=0x31e)),
   ("v_mbcnt_lo_u32_b32",      False, False, dst(1), src(1, 1), op(0x123, gfx8=0x28c, gfx10=0x365, gfx11=0x31f)),
   ("v_mbcnt_hi_u32_b32_e64",  False, False, dst(1), src(1, 1), op(gfx8=0x28d, gfx10=0x366, gfx11=0x320)),
   ("v_lshlrev_b64",           False, False, dst(2), src(1, 2), op(gfx8=0x28f, gfx10=0x2ff, gfx11=0x33c), InstrClass.Valu64),
   ("v_lshrrev_b64",           False, False, dst(2), src(1, 2), op(gfx8=0x290, gfx10=0x300, gfx11=0x33d), InstrClass.Valu64),
   ("v_ashrrev_i64",           False, False, dst(2), src(1, 2), op(gfx8=0x291, gfx10=0x301, gfx11=0x33e), InstrClass.Valu64),
   ("v_bfm_b32",               False, False, dst(1), src(1, 1), op(0x11e, gfx8=0x293, gfx10=0x363, gfx11=0x31d)),
   ("v_cvt_pknorm_i16_f32",    True, False, dst(1), src(1, 1), op(0x12d, gfx8=0x294, gfx10=0x368, gfx11=0x321)),
   ("v_cvt_pknorm_u16_f32",    True, False, dst(1), src(1, 1), op(0x12e, gfx8=0x295, gfx10=0x369, gfx11=0x322)),
   ("v_cvt_pkrtz_f16_f32_e64", True, False, dst(1), src(1, 1), op(gfx8=0x296, gfx10=-1)),
   ("v_cvt_pk_u16_u32",        False, False, dst(1), src(1, 1), op(0x130, gfx8=0x297, gfx10=0x36a, gfx11=0x323)),
   ("v_cvt_pk_i16_i32",        False, False, dst(1), src(1, 1), op(0x131, gfx8=0x298, gfx10=0x36b, gfx11=0x324)),
   ("v_cvt_pknorm_i16_f16",    True, False, dst(1), src(1, 1), op(gfx9=0x299, gfx10=0x312)), #v_cvt_pk_norm_i16_f32 in GFX11
   ("v_cvt_pknorm_u16_f16",    True, False, dst(1), src(1, 1), op(gfx9=0x29a, gfx10=0x313)), #v_cvt_pk_norm_u16_f32 in GFX11
   ("v_add_i32",               False, False, dst(1), src(1, 1), op(gfx9=0x29c, gfx10=0x37f, gfx11=0x326)),
   ("v_sub_i32",               False, False, dst(1), src(1, 1), op(gfx9=0x29d, gfx10=0x376, gfx11=0x325)),
   ("v_add_i16",               False, False, dst(1), src(1, 1), op(gfx9=0x29e, gfx10=0x30d)),
   ("v_sub_i16",               False, False, dst(1), src(1, 1), op(gfx9=0x29f, gfx10=0x30e)),
   ("v_pack_b32_f16",          True, False, dst(1), src(1, 1), op(gfx9=0x2a0, gfx10=0x311)),
   ("v_xor3_b32",              False, False, dst(1), src(1, 1, 1), op(gfx10=0x178, gfx11=0x240)),
   ("v_permlane16_b32",        False, False, dst(1), src(1, 1, 1), op(gfx10=0x377, gfx11=0x25b)),
   ("v_permlanex16_b32",       False, False, dst(1), src(1, 1, 1), op(gfx10=0x378, gfx11=0x25c)),
   ("v_add_co_u32_e64",        False, False, dst(1, VCC), src(1, 1), op(gfx10=0x30f, gfx11=0x300)),
   ("v_sub_co_u32_e64",        False, False, dst(1, VCC), src(1, 1), op(gfx10=0x310, gfx11=0x301)),
   ("v_subrev_co_u32_e64",     False, False, dst(1, VCC), src(1, 1), op(gfx10=0x319, gfx11=0x302)),
   ("v_add_u16_e64",           False, False, dst(1), src(1, 1), op(gfx10=0x303)),
   ("v_sub_u16_e64",           False, False, dst(1), src(1, 1), op(gfx10=0x304)),
   ("v_mul_lo_u16_e64",        False, False, dst(1), src(1, 1), op(gfx10=0x305)),
   ("v_max_u16_e64",           False, False, dst(1), src(1, 1), op(gfx10=0x309)),
   ("v_max_i16_e64",           False, False, dst(1), src(1, 1), op(gfx10=0x30a)),
   ("v_min_u16_e64",           False, False, dst(1), src(1, 1), op(gfx10=0x30b)),
   ("v_min_i16_e64",           False, False, dst(1), src(1, 1), op(gfx10=0x30c)),
   ("v_lshrrev_b16_e64",       False, False, dst(1), src(1, 1), op(gfx10=0x307, gfx11=0x339)),
   ("v_ashrrev_i16_e64",       False, False, dst(1), src(1, 1), op(gfx10=0x308, gfx11=0x33a)),
   ("v_lshlrev_b16_e64",       False, False, dst(1), src(1, 1), op(gfx10=0x314, gfx11=0x338)),
   ("v_fma_legacy_f32",        True, True, dst(1), src(1, 1, 1), op(gfx10=0x140, gfx11=0x209), InstrClass.ValuFma), #GFX10.3+, v_fma_dx9_zero_f32 in GFX11
   ("v_maxmin_f32",            True, True, dst(1), src(1, 1, 1), op(gfx11=0x25e)),
   ("v_minmax_f32",            True, True, dst(1), src(1, 1, 1), op(gfx11=0x25f)),
   ("v_maxmin_f16",            True, True, dst(1), src(1, 1, 1), op(gfx11=0x260)),
   ("v_minmax_f16",            True, True, dst(1), src(1, 1, 1), op(gfx11=0x261)),
   ("v_maxmin_u32",            False, False, dst(1), src(1, 1, 1), op(gfx11=0x262)),
   ("v_minmax_u32",            False, False, dst(1), src(1, 1, 1), op(gfx11=0x263)),
   ("v_maxmin_i32",            False, False, dst(1), src(1, 1, 1), op(gfx11=0x264)),
   ("v_minmax_i32",            False, False, dst(1), src(1, 1, 1), op(gfx11=0x265)),
   ("v_dot2_f16_f16",          False, False, dst(1), src(1, 1, 1), op(gfx11=0x266)),
   ("v_dot2_bf16_bf16",        False, False, dst(1), src(1, 1, 1), op(gfx11=0x267)),
   ("v_cvt_pk_i16_f32",        True, False, dst(1), src(1, 1), op(gfx11=0x306)),
   ("v_cvt_pk_u16_f32",        True, False, dst(1), src(1, 1), op(gfx11=0x307)),
   ("v_and_b16",               False, False, dst(1), src(1, 1), op(gfx11=0x362)),
   ("v_or_b16",                False, False, dst(1), src(1, 1), op(gfx11=0x363)),
   ("v_xor_b16",               False, False, dst(1), src(1, 1), op(gfx11=0x364)),
   ("v_cndmask_b16",           True, False, dst(1), src(1, 1, VCC), op(gfx11=0x25d)),
}
for (name, in_mod, out_mod, defs, ops, num, cls) in default_class(VOP3, InstrClass.Valu32):
   insn(name, num, Format.VOP3, cls, in_mod, out_mod, definitions = defs, operands = ops)


VOPD = {
   ("v_dual_fmac_f32",         op(gfx11=0x00)),
   ("v_dual_fmaak_f32",        op(gfx11=0x01)),
   ("v_dual_fmamk_f32",        op(gfx11=0x02)),
   ("v_dual_mul_f32",          op(gfx11=0x03)),
   ("v_dual_add_f32",          op(gfx11=0x04)),
   ("v_dual_sub_f32",          op(gfx11=0x05)),
   ("v_dual_subrev_f32",       op(gfx11=0x06)),
   ("v_dual_mul_dx9_zero_f32", op(gfx11=0x07)),
   ("v_dual_mov_b32",          op(gfx11=0x08)),
   ("v_dual_cndmask_b32",      op(gfx11=0x09)),
   ("v_dual_max_f32",          op(gfx11=0x0a)),
   ("v_dual_min_f32",          op(gfx11=0x0b)),
   ("v_dual_dot2acc_f32_f16",  op(gfx11=0x0c)),
   ("v_dual_dot2acc_f32_bf16", op(gfx11=0x0d)),
   ("v_dual_add_nc_u32",       op(gfx11=0x10)),
   ("v_dual_lshlrev_b32",      op(gfx11=0x11)),
   ("v_dual_and_b32",          op(gfx11=0x12)),
}
for (name, num) in VOPD:
   insn(name, num, format = Format.VOPD, cls = InstrClass.Valu32)


# DS instructions: 3 inputs (1 addr, 2 data), 1 output
DS = {
   ("ds_add_u32",              op(0x00)),
   ("ds_sub_u32",              op(0x01)),
   ("ds_rsub_u32",             op(0x02)),
   ("ds_inc_u32",              op(0x03)),
   ("ds_dec_u32",              op(0x04)),
   ("ds_min_i32",              op(0x05)),
   ("ds_max_i32",              op(0x06)),
   ("ds_min_u32",              op(0x07)),
   ("ds_max_u32",              op(0x08)),
   ("ds_and_b32",              op(0x09)),
   ("ds_or_b32",               op(0x0a)),
   ("ds_xor_b32",              op(0x0b)),
   ("ds_mskor_b32",            op(0x0c)),
   ("ds_write_b32",            op(0x0d)), #ds_store_b32 in GFX11
   ("ds_write2_b32",           op(0x0e)), #ds_store_2addr_b32 in GFX11
   ("ds_write2st64_b32",       op(0x0f)), #ds_store_2addr_stride64_b32 in GFX11
   ("ds_cmpst_b32",            op(0x10)), #ds_cmpstore_b32 in GFX11
   ("ds_cmpst_f32",            op(0x11)), #ds_cmpstore_f32 in GFX11
   ("ds_min_f32",              op(0x12)),
   ("ds_max_f32",              op(0x13)),
   ("ds_nop",                  op(gfx7=0x14)),
   ("ds_add_f32",              op(gfx8=0x15)),
   ("ds_write_addtid_b32",     op(gfx8=0x1d, gfx10=0xb0)), #ds_store_addtid_b32 in GFX11
   ("ds_write_b8",             op(0x1e)), #ds_store_b8 in GFX11
   ("ds_write_b16",            op(0x1f)), #ds_store_b16 in GFX11
   ("ds_add_rtn_u32",          op(0x20)),
   ("ds_sub_rtn_u32",          op(0x21)),
   ("ds_rsub_rtn_u32",         op(0x22)),
   ("ds_inc_rtn_u32",          op(0x23)),
   ("ds_dec_rtn_u32",          op(0x24)),
   ("ds_min_rtn_i32",          op(0x25)),
   ("ds_max_rtn_i32",          op(0x26)),
   ("ds_min_rtn_u32",          op(0x27)),
   ("ds_max_rtn_u32",          op(0x28)),
   ("ds_and_rtn_b32",          op(0x29)),
   ("ds_or_rtn_b32",           op(0x2a)),
   ("ds_xor_rtn_b32",          op(0x2b)),
   ("ds_mskor_rtn_b32",        op(0x2c)),
   ("ds_wrxchg_rtn_b32",       op(0x2d)), #ds_storexchg_rtn_b32 in GFX11
   ("ds_wrxchg2_rtn_b32",      op(0x2e)), #ds_storexchg_2addr_rtn_b32 in GFX11
   ("ds_wrxchg2st64_rtn_b32",  op(0x2f)), #ds_storexchg_2addr_stride64_rtn_b32 in GFX11
   ("ds_cmpst_rtn_b32",        op(0x30)), #ds_cmpstore_rtn_b32 in GFX11
   ("ds_cmpst_rtn_f32",        op(0x31)), #ds_cmpstore_rtn_f32 in GFX11
   ("ds_min_rtn_f32",          op(0x32)),
   ("ds_max_rtn_f32",          op(0x33)),
   ("ds_wrap_rtn_b32",         op(gfx7=0x34)),
   ("ds_add_rtn_f32",          op(gfx8=0x35, gfx10=0x55, gfx11=0x79)),
   ("ds_read_b32",             op(0x36)), #ds_load_b32 in GFX11
   ("ds_read2_b32",            op(0x37)), #ds_load_2addr_b32 in GFX11
   ("ds_read2st64_b32",        op(0x38)), #ds_load_2addr_stride64_b32 in GFX11
   ("ds_read_i8",              op(0x39)), #ds_load_i8 in GFX11
   ("ds_read_u8",              op(0x3a)), #ds_load_u8 in GFX11
   ("ds_read_i16",             op(0x3b)), #ds_load_i16 in GFX11
   ("ds_read_u16",             op(0x3c)), #ds_load_u16 in GFX11
   ("ds_swizzle_b32",          op(0x35, gfx8=0x3d, gfx10=0x35)), #data1 & offset, no addr/data2
   ("ds_permute_b32",          op(gfx8=0x3e, gfx10=0xb2)),
   ("ds_bpermute_b32",         op(gfx8=0x3f, gfx10=0xb3)),
   ("ds_add_u64",              op(0x40)),
   ("ds_sub_u64",              op(0x41)),
   ("ds_rsub_u64",             op(0x42)),
   ("ds_inc_u64",              op(0x43)),
   ("ds_dec_u64",              op(0x44)),
   ("ds_min_i64",              op(0x45)),
   ("ds_max_i64",              op(0x46)),
   ("ds_min_u64",              op(0x47)),
   ("ds_max_u64",              op(0x48)),
   ("ds_and_b64",              op(0x49)),
   ("ds_or_b64",               op(0x4a)),
   ("ds_xor_b64",              op(0x4b)),
   ("ds_mskor_b64",            op(0x4c)),
   ("ds_write_b64",            op(0x4d)), #ds_store_b64 in GFX11
   ("ds_write2_b64",           op(0x4e)), #ds_store_2addr_b64 in GFX11
   ("ds_write2st64_b64",       op(0x4f)), #ds_store_2addr_stride64_b64 in GFX11
   ("ds_cmpst_b64",            op(0x50)), #ds_cmpstore_b64 in GFX11
   ("ds_cmpst_f64",            op(0x51)), #ds_cmpstore_f64 in GFX11
   ("ds_min_f64",              op(0x52)),
   ("ds_max_f64",              op(0x53)),
   ("ds_write_b8_d16_hi",      op(gfx9=0x54, gfx10=0xa0)), #ds_store_b8_d16_hi in GFX11
   ("ds_write_b16_d16_hi",     op(gfx9=0x55, gfx10=0xa1)), #ds_store_b16_d16_hi in GFX11
   ("ds_read_u8_d16",          op(gfx9=0x56, gfx10=0xa2)), #ds_load_u8_d16 in GFX11
   ("ds_read_u8_d16_hi",       op(gfx9=0x57, gfx10=0xa3)), #ds_load_u8_d16_hi in GFX11
   ("ds_read_i8_d16",          op(gfx9=0x58, gfx10=0xa4)), #ds_load_i8_d16 in GFX11
   ("ds_read_i8_d16_hi",       op(gfx9=0x59, gfx10=0xa5)), #ds_load_i8_d16_hi in GFX11
   ("ds_read_u16_d16",         op(gfx9=0x5a, gfx10=0xa6)), #ds_load_u16_d16 in GFX11
   ("ds_read_u16_d16_hi",      op(gfx9=0x5b, gfx10=0xa7)), #ds_load_u16_d16_hi in GFX11
   ("ds_add_rtn_u64",          op(0x60)),
   ("ds_sub_rtn_u64",          op(0x61)),
   ("ds_rsub_rtn_u64",         op(0x62)),
   ("ds_inc_rtn_u64",          op(0x63)),
   ("ds_dec_rtn_u64",          op(0x64)),
   ("ds_min_rtn_i64",          op(0x65)),
   ("ds_max_rtn_i64",          op(0x66)),
   ("ds_min_rtn_u64",          op(0x67)),
   ("ds_max_rtn_u64",          op(0x68)),
   ("ds_and_rtn_b64",          op(0x69)),
   ("ds_or_rtn_b64",           op(0x6a)),
   ("ds_xor_rtn_b64",          op(0x6b)),
   ("ds_mskor_rtn_b64",        op(0x6c)),
   ("ds_wrxchg_rtn_b64",       op(0x6d)), #ds_storexchg_rtn_b64 in GFX11
   ("ds_wrxchg2_rtn_b64",      op(0x6e)), #ds_storexchg_2addr_rtn_b64 in GFX11
   ("ds_wrxchg2st64_rtn_b64",  op(0x6f)), #ds_storexchg_2addr_stride64_rtn_b64 in GFX11
   ("ds_cmpst_rtn_b64",        op(0x70)), #ds_cmpstore_rtn_b64 in GFX11
   ("ds_cmpst_rtn_f64",        op(0x71)), #ds_cmpstore_rtn_f64 in GFX11
   ("ds_min_rtn_f64",          op(0x72)),
   ("ds_max_rtn_f64",          op(0x73)),
   ("ds_read_b64",             op(0x76)), #ds_load_b64 in GFX11
   ("ds_read2_b64",            op(0x77)), #ds_load_2addr_b64 in GFX11
   ("ds_read2st64_b64",        op(0x78)), #ds_load_2addr_stride64_b64 in GFX11
   ("ds_condxchg32_rtn_b64",   op(gfx7=0x7e)),
   ("ds_add_src2_u32",         op(0x80, gfx11=-1)),
   ("ds_sub_src2_u32",         op(0x81, gfx11=-1)),
   ("ds_rsub_src2_u32",        op(0x82, gfx11=-1)),
   ("ds_inc_src2_u32",         op(0x83, gfx11=-1)),
   ("ds_dec_src2_u32",         op(0x84, gfx11=-1)),
   ("ds_min_src2_i32",         op(0x85, gfx11=-1)),
   ("ds_max_src2_i32",         op(0x86, gfx11=-1)),
   ("ds_min_src2_u32",         op(0x87, gfx11=-1)),
   ("ds_max_src2_u32",         op(0x88, gfx11=-1)),
   ("ds_and_src2_b32",         op(0x89, gfx11=-1)),
   ("ds_or_src2_b32",          op(0x8a, gfx11=-1)),
   ("ds_xor_src2_b32",         op(0x8b, gfx11=-1)),
   ("ds_write_src2_b32",       op(0x8d, gfx11=-1)),
   ("ds_min_src2_f32",         op(0x92, gfx11=-1)),
   ("ds_max_src2_f32",         op(0x93, gfx11=-1)),
   ("ds_add_src2_f32",         op(gfx8=0x95, gfx11=-1)),
   ("ds_gws_sema_release_all", op(gfx7=0x18, gfx8=0x98, gfx10=0x18)),
   ("ds_gws_init",             op(0x19, gfx8=0x99, gfx10=0x19)),
   ("ds_gws_sema_v",           op(0x1a, gfx8=0x9a, gfx10=0x1a)),
   ("ds_gws_sema_br",          op(0x1b, gfx8=0x9b, gfx10=0x1b)),
   ("ds_gws_sema_p",           op(0x1c, gfx8=0x9c, gfx10=0x1c)),
   ("ds_gws_barrier",          op(0x1d, gfx8=0x9d, gfx10=0x1d)),
   ("ds_read_addtid_b32",      op(gfx8=0xb6, gfx10=0xb1)), #ds_load_addtid_b32 in GFX11
   ("ds_consume",              op(0x3d, gfx8=0xbd, gfx10=0x3d)),
   ("ds_append",               op(0x3e, gfx8=0xbe, gfx10=0x3e)),
   ("ds_ordered_count",        op(0x3f, gfx8=0xbf, gfx10=0x3f)),
   ("ds_add_src2_u64",         op(0xc0, gfx11=-1)),
   ("ds_sub_src2_u64",         op(0xc1, gfx11=-1)),
   ("ds_rsub_src2_u64",        op(0xc2, gfx11=-1)),
   ("ds_inc_src2_u64",         op(0xc3, gfx11=-1)),
   ("ds_dec_src2_u64",         op(0xc4, gfx11=-1)),
   ("ds_min_src2_i64",         op(0xc5, gfx11=-1)),
   ("ds_max_src2_i64",         op(0xc6, gfx11=-1)),
   ("ds_min_src2_u64",         op(0xc7, gfx11=-1)),
   ("ds_max_src2_u64",         op(0xc8, gfx11=-1)),
   ("ds_and_src2_b64",         op(0xc9, gfx11=-1)),
   ("ds_or_src2_b64",          op(0xca, gfx11=-1)),
   ("ds_xor_src2_b64",         op(0xcb, gfx11=-1)),
   ("ds_write_src2_b64",       op(0xcd, gfx11=-1)),
   ("ds_min_src2_f64",         op(0xd2, gfx11=-1)),
   ("ds_max_src2_f64",         op(0xd3, gfx11=-1)),
   ("ds_write_b96",            op(gfx7=0xde)), #ds_store_b96 in GFX11
   ("ds_write_b128",           op(gfx7=0xdf)), #ds_store_b128 in GFX11
   ("ds_condxchg32_rtn_b128",  op(gfx7=0xfd, gfx9=-1)),
   ("ds_read_b96",             op(gfx7=0xfe)), #ds_load_b96 in GFX11
   ("ds_read_b128",            op(gfx7=0xff)), #ds_load_b128 in GFX11
   ("ds_add_gs_reg_rtn",       op(gfx11=0x7a)),
   ("ds_sub_gs_reg_rtn",       op(gfx11=0x7b)),
}
for (name, num) in DS:
    insn(name, num, Format.DS, InstrClass.DS)


# LDSDIR instructions:
LDSDIR = {
   ("lds_param_load",  op(gfx11=0x00)),
   ("lds_direct_load", op(gfx11=0x01)),
}
for (name, num) in LDSDIR:
    insn(name, num, Format.LDSDIR, InstrClass.DS)

# MUBUF instructions:
MUBUF = {
   ("buffer_load_format_x",         op(0x00)),
   ("buffer_load_format_xy",        op(0x01)),
   ("buffer_load_format_xyz",       op(0x02)),
   ("buffer_load_format_xyzw",      op(0x03)),
   ("buffer_store_format_x",        op(0x04)),
   ("buffer_store_format_xy",       op(0x05)),
   ("buffer_store_format_xyz",      op(0x06)),
   ("buffer_store_format_xyzw",     op(0x07)),
   ("buffer_load_format_d16_x",     op(gfx8=0x08, gfx10=0x80, gfx11=0x08)),
   ("buffer_load_format_d16_xy",    op(gfx8=0x09, gfx10=0x81, gfx11=0x09)),
   ("buffer_load_format_d16_xyz",   op(gfx8=0x0a, gfx10=0x82, gfx11=0x0a)),
   ("buffer_load_format_d16_xyzw",  op(gfx8=0x0b, gfx10=0x83, gfx11=0x0b)),
   ("buffer_store_format_d16_x",    op(gfx8=0x0c, gfx10=0x84, gfx11=0x0c)),
   ("buffer_store_format_d16_xy",   op(gfx8=0x0d, gfx10=0x85, gfx11=0x0d)),
   ("buffer_store_format_d16_xyz",  op(gfx8=0x0e, gfx10=0x86, gfx11=0x0e)),
   ("buffer_store_format_d16_xyzw", op(gfx8=0x0f, gfx10=0x87, gfx11=0x0f)),
   ("buffer_load_ubyte",            op(0x08, gfx8=0x10, gfx10=0x08, gfx11=0x10)),
   ("buffer_load_sbyte",            op(0x09, gfx8=0x11, gfx10=0x09, gfx11=0x11)),
   ("buffer_load_ushort",           op(0x0a, gfx8=0x12, gfx10=0x0a, gfx11=0x12)),
   ("buffer_load_sshort",           op(0x0b, gfx8=0x13, gfx10=0x0b, gfx11=0x13)),
   ("buffer_load_dword",            op(0x0c, gfx8=0x14, gfx10=0x0c, gfx11=0x14)),
   ("buffer_load_dwordx2",          op(0x0d, gfx8=0x15, gfx10=0x0d, gfx11=0x15)),
   ("buffer_load_dwordx3",          op(gfx7=0x0f, gfx8=0x16, gfx10=0x0f, gfx11=0x16)),
   ("buffer_load_dwordx4",          op(0x0e, gfx8=0x17, gfx10=0x0e, gfx11=0x17)),
   ("buffer_store_byte",            op(0x18)),
   ("buffer_store_byte_d16_hi",     op(gfx9=0x19, gfx11=0x24)),
   ("buffer_store_short",           op(0x1a, gfx11=0x19)),
   ("buffer_store_short_d16_hi",    op(gfx9=0x1b, gfx11=0x25)),
   ("buffer_store_dword",           op(0x1c, gfx11=0x1a)),
   ("buffer_store_dwordx2",         op(0x1d, gfx11=0x1b)),
   ("buffer_store_dwordx3",         op(gfx7=0x1f, gfx8=0x1e, gfx10=0x1f, gfx11=0x1c)),
   ("buffer_store_dwordx4",         op(0x1e, gfx8=0x1f, gfx10=0x1e, gfx11=0x1d)),
   ("buffer_load_ubyte_d16",        op(gfx9=0x20, gfx11=0x1e)),
   ("buffer_load_ubyte_d16_hi",     op(gfx9=0x21)),
   ("buffer_load_sbyte_d16",        op(gfx9=0x22, gfx11=0x1f)),
   ("buffer_load_sbyte_d16_hi",     op(gfx9=0x23, gfx11=0x22)),
   ("buffer_load_short_d16",        op(gfx9=0x24, gfx11=0x20)),
   ("buffer_load_short_d16_hi",     op(gfx9=0x25, gfx11=0x23)),
   ("buffer_load_format_d16_hi_x",  op(gfx9=0x26)),
   ("buffer_store_format_d16_hi_x", op(gfx9=0x27)),
   ("buffer_store_lds_dword",       op(gfx8=0x3d, gfx10=-1)),
   ("buffer_wbinvl1",               op(0x71, gfx8=0x3e, gfx10=-1)),
   ("buffer_wbinvl1_vol",           op(0x70, gfx8=0x3f, gfx10=-1)),
   ("buffer_atomic_swap",           op(0x30, gfx8=0x40, gfx10=0x30, gfx11=0x33)),
   ("buffer_atomic_cmpswap",        op(0x31, gfx8=0x41, gfx10=0x31, gfx11=0x34)),
   ("buffer_atomic_add",            op(0x32, gfx8=0x42, gfx10=0x32, gfx11=0x35)),
   ("buffer_atomic_sub",            op(0x33, gfx8=0x43, gfx10=0x33, gfx11=0x36)),
   ("buffer_atomic_rsub",           op(0x34, gfx7=-1)),
   ("buffer_atomic_smin",           op(0x35, gfx8=0x44, gfx10=0x35, gfx11=0x38)),
   ("buffer_atomic_umin",           op(0x36, gfx8=0x45, gfx10=0x36, gfx11=0x39)),
   ("buffer_atomic_smax",           op(0x37, gfx8=0x46, gfx10=0x37, gfx11=0x3a)),
   ("buffer_atomic_umax",           op(0x38, gfx8=0x47, gfx10=0x38, gfx11=0x3b)),
   ("buffer_atomic_and",            op(0x39, gfx8=0x48, gfx10=0x39, gfx11=0x3c)),
   ("buffer_atomic_or",             op(0x3a, gfx8=0x49, gfx10=0x3a, gfx11=0x3d)),
   ("buffer_atomic_xor",            op(0x3b, gfx8=0x4a, gfx10=0x3b, gfx11=0x3e)),
   ("buffer_atomic_inc",            op(0x3c, gfx8=0x4b, gfx10=0x3c, gfx11=0x3f)),
   ("buffer_atomic_dec",            op(0x3d, gfx8=0x4c, gfx10=0x3d, gfx11=0x40)),
   ("buffer_atomic_fcmpswap",       op(0x3e, gfx8=-1, gfx10=0x3e, gfx11=0x50)),
   ("buffer_atomic_fmin",           op(0x3f, gfx8=-1, gfx10=0x3f, gfx11=0x51)),
   ("buffer_atomic_fmax",           op(0x40, gfx8=-1, gfx10=0x40, gfx11=0x52)),
   ("buffer_atomic_swap_x2",        op(0x50, gfx8=0x60, gfx10=0x50, gfx11=0x41)),
   ("buffer_atomic_cmpswap_x2",     op(0x51, gfx8=0x61, gfx10=0x51, gfx11=0x42)),
   ("buffer_atomic_add_x2",         op(0x52, gfx8=0x62, gfx10=0x52, gfx11=0x43)),
   ("buffer_atomic_sub_x2",         op(0x53, gfx8=0x63, gfx10=0x53, gfx11=0x44)),
   ("buffer_atomic_rsub_x2",        op(0x54, gfx7=-1)),
   ("buffer_atomic_smin_x2",        op(0x55, gfx8=0x64, gfx10=0x55, gfx11=0x45)),
   ("buffer_atomic_umin_x2",        op(0x56, gfx8=0x65, gfx10=0x56, gfx11=0x46)),
   ("buffer_atomic_smax_x2",        op(0x57, gfx8=0x66, gfx10=0x57, gfx11=0x47)),
   ("buffer_atomic_umax_x2",        op(0x58, gfx8=0x67, gfx10=0x58, gfx11=0x48)),
   ("buffer_atomic_and_x2",         op(0x59, gfx8=0x68, gfx10=0x59, gfx11=0x49)),
   ("buffer_atomic_or_x2",          op(0x5a, gfx8=0x69, gfx10=0x5a, gfx11=0x4a)),
   ("buffer_atomic_xor_x2",         op(0x5b, gfx8=0x6a, gfx10=0x5b, gfx11=0x4b)),
   ("buffer_atomic_inc_x2",         op(0x5c, gfx8=0x6b, gfx10=0x5c, gfx11=0x4c)),
   ("buffer_atomic_dec_x2",         op(0x5d, gfx8=0x6c, gfx10=0x5d, gfx11=0x4d)),
   ("buffer_atomic_fcmpswap_x2",    op(0x5e, gfx8=-1, gfx10=0x5e, gfx11=-1)),
   ("buffer_atomic_fmin_x2",        op(0x5f, gfx8=-1, gfx10=0x5f, gfx11=-1)),
   ("buffer_atomic_fmax_x2",        op(0x60, gfx8=-1, gfx10=0x60, gfx11=-1)),
   ("buffer_gl0_inv",               op(gfx10=0x71, gfx11=0x2b)),
   ("buffer_gl1_inv",               op(gfx10=0x72, gfx11=0x2c)),
   ("buffer_atomic_csub",           op(gfx10=0x34, gfx11=0x37)), #GFX10.3+. seems glc must be set. buffer_atomic_csub_u32 in GFX11
   ("buffer_load_lds_b32",          op(gfx11=0x31)),
   ("buffer_load_lds_format_x",     op(gfx11=0x32)),
   ("buffer_load_lds_i8",           op(gfx11=0x2e)),
   ("buffer_load_lds_i16",          op(gfx11=0x30)),
   ("buffer_load_lds_u8",           op(gfx11=0x2d)),
   ("buffer_load_lds_u16",          op(gfx11=0x2f)),
   ("buffer_atomic_add_f32",        op(gfx11=0x56)),
}
for (name, num) in MUBUF:
    insn(name, num, Format.MUBUF, InstrClass.VMem, is_atomic = "atomic" in name)

MTBUF = {
   ("tbuffer_load_format_x",         op(0x00)),
   ("tbuffer_load_format_xy",        op(0x01)),
   ("tbuffer_load_format_xyz",       op(0x02)),
   ("tbuffer_load_format_xyzw",      op(0x03)),
   ("tbuffer_store_format_x",        op(0x04)),
   ("tbuffer_store_format_xy",       op(0x05)),
   ("tbuffer_store_format_xyz",      op(0x06)),
   ("tbuffer_store_format_xyzw",     op(0x07)),
   ("tbuffer_load_format_d16_x",     op(gfx8=0x08)),
   ("tbuffer_load_format_d16_xy",    op(gfx8=0x09)),
   ("tbuffer_load_format_d16_xyz",   op(gfx8=0x0a)),
   ("tbuffer_load_format_d16_xyzw",  op(gfx8=0x0b)),
   ("tbuffer_store_format_d16_x",    op(gfx8=0x0c)),
   ("tbuffer_store_format_d16_xy",   op(gfx8=0x0d)),
   ("tbuffer_store_format_d16_xyz",  op(gfx8=0x0e)),
   ("tbuffer_store_format_d16_xyzw", op(gfx8=0x0f)),
}
for (name, num) in MTBUF:
    insn(name, num, Format.MTBUF, InstrClass.VMem)


MIMG = {
   ("image_load",                op(0x00)),
   ("image_load_mip",            op(0x01)),
   ("image_load_pck",            op(0x02)),
   ("image_load_pck_sgn",        op(0x03)),
   ("image_load_mip_pck",        op(0x04)),
   ("image_load_mip_pck_sgn",    op(0x05)),
   ("image_store",               op(0x08, gfx11=0x06)),
   ("image_store_mip",           op(0x09, gfx11=0x07)),
   ("image_store_pck",           op(0x0a, gfx11=0x08)),
   ("image_store_mip_pck",       op(0x0b, gfx11=0x09)),
   ("image_get_resinfo",         op(0x0e, gfx11=0x17)),
   ("image_get_lod",             op(0x60, gfx11=0x38)),
   ("image_msaa_load",           op(gfx10=0x80, gfx11=0x18)), #GFX10.3+
   ("image_atomic_swap",         op(0x0f, gfx8=0x10, gfx10=0x0f, gfx11=0x0a)),
   ("image_atomic_cmpswap",      op(0x10, gfx8=0x11, gfx10=0x10, gfx11=0x0b)),
   ("image_atomic_add",          op(0x11, gfx8=0x12, gfx10=0x11, gfx11=0x0c)),
   ("image_atomic_sub",          op(0x12, gfx8=0x13, gfx10=0x12, gfx11=0x0d)),
   ("image_atomic_rsub",         op(0x13, gfx7=-1)),
   ("image_atomic_smin",         op(0x14, gfx11=0x0e)),
   ("image_atomic_umin",         op(0x15, gfx11=0x0f)),
   ("image_atomic_smax",         op(0x16, gfx11=0x10)),
   ("image_atomic_umax",         op(0x17, gfx11=0x11)),
   ("image_atomic_and",          op(0x18, gfx11=0x12)),
   ("image_atomic_or",           op(0x19, gfx11=0x13)),
   ("image_atomic_xor",          op(0x1a, gfx11=0x14)),
   ("image_atomic_inc",          op(0x1b, gfx11=0x15)),
   ("image_atomic_dec",          op(0x1c, gfx11=0x16)),
   ("image_atomic_fcmpswap",     op(0x1d, gfx8=-1, gfx10=0x1d, gfx11=-1)),
   ("image_atomic_fmin",         op(0x1e, gfx8=-1, gfx10=0x1e, gfx11=-1)),
   ("image_atomic_fmax",         op(0x1f, gfx8=-1, gfx10=0x1f, gfx11=-1)),
   ("image_sample",              op(0x20, gfx11=0x1b)),
   ("image_sample_cl",           op(0x21, gfx11=0x40)),
   ("image_sample_d",            op(0x22, gfx11=0x1c)),
   ("image_sample_d_cl",         op(0x23, gfx11=0x41)),
   ("image_sample_l",            op(0x24, gfx11=0x1d)),
   ("image_sample_b",            op(0x25, gfx11=0x1e)),
   ("image_sample_b_cl",         op(0x26, gfx11=0x42)),
   ("image_sample_lz",           op(0x27, gfx11=0x1f)),
   ("image_sample_c",            op(0x28, gfx11=0x20)),
   ("image_sample_c_cl",         op(0x29, gfx11=0x43)),
   ("image_sample_c_d",          op(0x2a, gfx11=0x21)),
   ("image_sample_c_d_cl",       op(0x2b, gfx11=0x44)),
   ("image_sample_c_l",          op(0x2c, gfx11=0x22)),
   ("image_sample_c_b",          op(0x2d, gfx11=0x23)),
   ("image_sample_c_b_cl",       op(0x2e, gfx11=0x45)),
   ("image_sample_c_lz",         op(0x2f, gfx11=0x24)),
   ("image_sample_o",            op(0x30, gfx11=0x25)),
   ("image_sample_cl_o",         op(0x31, gfx11=0x46)),
   ("image_sample_d_o",          op(0x32, gfx11=0x26)),
   ("image_sample_d_cl_o",       op(0x33, gfx11=0x47)),
   ("image_sample_l_o",          op(0x34, gfx11=0x27)),
   ("image_sample_b_o",          op(0x35, gfx11=0x28)),
   ("image_sample_b_cl_o",       op(0x36, gfx11=0x48)),
   ("image_sample_lz_o",         op(0x37, gfx11=0x29)),
   ("image_sample_c_o",          op(0x38, gfx11=0x2a)),
   ("image_sample_c_cl_o",       op(0x39, gfx11=0x49)),
   ("image_sample_c_d_o",        op(0x3a, gfx11=0x2b)),
   ("image_sample_c_d_cl_o",     op(0x3b, gfx11=0x4a)),
   ("image_sample_c_l_o",        op(0x3c, gfx11=0x2c)),
   ("image_sample_c_b_o",        op(0x3d, gfx11=0x2d)),
   ("image_sample_c_b_cl_o",     op(0x3e, gfx11=0x4b)),
   ("image_sample_c_lz_o",       op(0x3f, gfx11=0x2e)),
   ("image_sample_cd",           op(0x68, gfx11=-1)),
   ("image_sample_cd_cl",        op(0x69, gfx11=-1)),
   ("image_sample_c_cd",         op(0x6a, gfx11=-1)),
   ("image_sample_c_cd_cl",      op(0x6b, gfx11=-1)),
   ("image_sample_cd_o",         op(0x6c, gfx11=-1)),
   ("image_sample_cd_cl_o",      op(0x6d, gfx11=-1)),
   ("image_sample_c_cd_o",       op(0x6e, gfx11=-1)),
   ("image_sample_c_cd_cl_o",    op(0x6f, gfx11=-1)),
   ("image_sample_d_g16",        op(gfx10=0xa2, gfx11=0x39)),
   ("image_sample_d_cl_g16",     op(gfx10=0xa3, gfx11=0x5f)),
   ("image_sample_c_d_g16",      op(gfx10=0xaa, gfx11=0x3a)),
   ("image_sample_c_d_cl_g16",   op(gfx10=0xab, gfx11=0x54)),
   ("image_sample_d_o_g16",      op(gfx10=0xb2, gfx11=0x3b)),
   ("image_sample_d_cl_o_g16",   op(gfx10=0xb3, gfx11=0x55)),
   ("image_sample_c_d_o_g16",    op(gfx10=0xba, gfx11=0x3c)),
   ("image_sample_c_d_cl_o_g16", op(gfx10=0xbb, gfx11=0x56)),
   #("image_gather4h",            op(gfx9=0x42, gfx10=0x61, gfx11=0x90)), VEGA only?
   #("image_gather4h_pck",        op(gfx9=0x4a, gfx10=-1)), VEGA only?
   #("image_gather8h_pck",        op(gfx9=0x4b, gfx10=-1)), VEGA only?
   ("image_gather4",             op(0x40, gfx11=0x2f)),
   ("image_gather4_cl",          op(0x41, gfx11=0x60)),
   ("image_gather4_l",           op(0x44, gfx11=0x30)), # following instructions have different opcodes according to ISA sheet.
   ("image_gather4_b",           op(0x45, gfx11=0x31)),
   ("image_gather4_b_cl",        op(0x46, gfx11=0x61)),
   ("image_gather4_lz",          op(0x47, gfx11=0x32)),
   ("image_gather4_c",           op(0x48, gfx11=0x33)),
   ("image_gather4_c_cl",        op(0x49, gfx11=0x62)), # previous instructions have different opcodes according to ISA sheet.
   ("image_gather4_c_l",         op(0x4c, gfx11=0x63)),
   ("image_gather4_c_b",         op(0x4d, gfx11=0x64)),
   ("image_gather4_c_b_cl",      op(0x4e, gfx11=0x65)),
   ("image_gather4_c_lz",        op(0x4f, gfx11=0x34)),
   ("image_gather4_o",           op(0x50, gfx11=0x35)),
   ("image_gather4_cl_o",        op(0x51, gfx11=-1)),
   ("image_gather4_l_o",         op(0x54, gfx11=-1)),
   ("image_gather4_b_o",         op(0x55, gfx11=-1)),
   ("image_gather4_b_cl_o",      op(0x56, gfx11=-1)),
   ("image_gather4_lz_o",        op(0x57, gfx11=0x36)),
   ("image_gather4_c_o",         op(0x58, gfx11=-1)),
   ("image_gather4_c_cl_o",      op(0x59, gfx11=-1)),
   ("image_gather4_c_l_o",       op(0x5c, gfx11=-1)),
   ("image_gather4_c_b_o",       op(0x5d, gfx11=-1)),
   ("image_gather4_c_b_cl_o",    op(0x5e, gfx11=-1)),
   ("image_gather4_c_lz_o",      op(0x5f, gfx11=0x37)),
   ("image_bvh_intersect_ray",   op(gfx10=0xe6, gfx11=0x19)),
   ("image_bvh64_intersect_ray", op(gfx10=0xe7, gfx11=0x1a)),
}
for (name, num) in MIMG:
   insn(name, num, Format.MIMG, InstrClass.VMem, is_atomic = "atomic" in name)

FLAT = {
   ("flat_load_ubyte",          op(0x08, gfx8=0x10, gfx10=0x08, gfx11=0x10)),
   ("flat_load_sbyte",          op(0x09, gfx8=0x11, gfx10=0x09, gfx11=0x11)),
   ("flat_load_ushort",         op(0x0a, gfx8=0x12, gfx10=0x0a, gfx11=0x12)),
   ("flat_load_sshort",         op(0x0b, gfx8=0x13, gfx10=0x0b, gfx11=0x13)),
   ("flat_load_dword",          op(0x0c, gfx8=0x14, gfx10=0x0c, gfx11=0x14)),
   ("flat_load_dwordx2",        op(0x0d, gfx8=0x15, gfx10=0x0d, gfx11=0x15)),
   ("flat_load_dwordx3",        op(0x0f, gfx8=0x16, gfx10=0x0f, gfx11=0x16)),
   ("flat_load_dwordx4",        op(0x0e, gfx8=0x17, gfx10=0x0e, gfx11=0x17)),
   ("flat_store_byte",          op(0x18)),
   ("flat_store_byte_d16_hi",   op(gfx8=0x19, gfx11=0x24)),
   ("flat_store_short",         op(0x1a, gfx11=0x19)),
   ("flat_store_short_d16_hi",  op(gfx8=0x1b, gfx11=0x25)),
   ("flat_store_dword",         op(0x1c, gfx11=0x1a)),
   ("flat_store_dwordx2",       op(0x1d, gfx11=0x1b)),
   ("flat_store_dwordx3",       op(0x1f, gfx8=0x1e, gfx10=0x1f, gfx11=0x1c)),
   ("flat_store_dwordx4",       op(0x1e, gfx8=0x1f, gfx10=0x1e, gfx11=0x1d)),
   ("flat_load_ubyte_d16",      op(gfx8=0x20, gfx11=0x1e)),
   ("flat_load_ubyte_d16_hi",   op(gfx8=0x21)),
   ("flat_load_sbyte_d16",      op(gfx8=0x22, gfx11=0x1f)),
   ("flat_load_sbyte_d16_hi",   op(gfx8=0x23, gfx11=0x22)),
   ("flat_load_short_d16",      op(gfx8=0x24, gfx11=0x20)),
   ("flat_load_short_d16_hi",   op(gfx8=0x25, gfx11=0x23)),
   ("flat_atomic_swap",         op(0x30, gfx8=0x40, gfx10=0x30, gfx11=0x33)),
   ("flat_atomic_cmpswap",      op(0x31, gfx8=0x41, gfx10=0x31, gfx11=0x34)),
   ("flat_atomic_add",          op(0x32, gfx8=0x42, gfx10=0x32, gfx11=0x35)),
   ("flat_atomic_sub",          op(0x33, gfx8=0x43, gfx10=0x33, gfx11=0x36)),
   ("flat_atomic_smin",         op(0x35, gfx8=0x44, gfx10=0x35, gfx11=0x38)),
   ("flat_atomic_umin",         op(0x36, gfx8=0x45, gfx10=0x36, gfx11=0x39)),
   ("flat_atomic_smax",         op(0x37, gfx8=0x46, gfx10=0x37, gfx11=0x3a)),
   ("flat_atomic_umax",         op(0x38, gfx8=0x47, gfx10=0x38, gfx11=0x3b)),
   ("flat_atomic_and",          op(0x39, gfx8=0x48, gfx10=0x39, gfx11=0x3c)),
   ("flat_atomic_or",           op(0x3a, gfx8=0x49, gfx10=0x3a, gfx11=0x3d)),
   ("flat_atomic_xor",          op(0x3b, gfx8=0x4a, gfx10=0x3b, gfx11=0x3e)),
   ("flat_atomic_inc",          op(0x3c, gfx8=0x4b, gfx10=0x3c, gfx11=0x3f)),
   ("flat_atomic_dec",          op(0x3d, gfx8=0x4c, gfx10=0x3d, gfx11=0x40)),
   ("flat_atomic_fcmpswap",     op(0x3e, gfx8=-1, gfx10=0x3e, gfx11=0x50)),
   ("flat_atomic_fmin",         op(0x3f, gfx8=-1, gfx10=0x3f, gfx11=0x51)),
   ("flat_atomic_fmax",         op(0x40, gfx8=-1, gfx10=0x40, gfx11=0x52)),
   ("flat_atomic_swap_x2",      op(0x50, gfx8=0x60, gfx10=0x50, gfx11=0x41)),
   ("flat_atomic_cmpswap_x2",   op(0x51, gfx8=0x61, gfx10=0x51, gfx11=0x42)),
   ("flat_atomic_add_x2",       op(0x52, gfx8=0x62, gfx10=0x52, gfx11=0x43)),
   ("flat_atomic_sub_x2",       op(0x53, gfx8=0x63, gfx10=0x53, gfx11=0x44)),
   ("flat_atomic_smin_x2",      op(0x55, gfx8=0x64, gfx10=0x55, gfx11=0x45)),
   ("flat_atomic_umin_x2",      op(0x56, gfx8=0x65, gfx10=0x56, gfx11=0x46)),
   ("flat_atomic_smax_x2",      op(0x57, gfx8=0x66, gfx10=0x57, gfx11=0x47)),
   ("flat_atomic_umax_x2",      op(0x58, gfx8=0x67, gfx10=0x58, gfx11=0x48)),
   ("flat_atomic_and_x2",       op(0x59, gfx8=0x68, gfx10=0x59, gfx11=0x49)),
   ("flat_atomic_or_x2",        op(0x5a, gfx8=0x69, gfx10=0x5a, gfx11=0x4a)),
   ("flat_atomic_xor_x2",       op(0x5b, gfx8=0x6a, gfx10=0x5b, gfx11=0x4b)),
   ("flat_atomic_inc_x2",       op(0x5c, gfx8=0x6b, gfx10=0x5c, gfx11=0x4c)),
   ("flat_atomic_dec_x2",       op(0x5d, gfx8=0x6c, gfx10=0x5d, gfx11=0x4d)),
   ("flat_atomic_fcmpswap_x2",  op(0x5e, gfx8=-1, gfx10=0x5e, gfx11=-1)),
   ("flat_atomic_fmin_x2",      op(0x5f, gfx8=-1, gfx10=0x5f, gfx11=-1)),
   ("flat_atomic_fmax_x2",      op(0x60, gfx8=-1, gfx10=0x60, gfx11=-1)),
   ("flat_atomic_add_f32",      op(gfx11=0x56)),
}
for (name, num) in FLAT:
    insn(name, num, Format.FLAT, InstrClass.VMem, is_atomic = "atomic" in name) #TODO: also LDS?

GLOBAL = {
   ("global_load_ubyte",             op(gfx8=0x10, gfx10=0x08, gfx11=0x10)),
   ("global_load_sbyte",             op(gfx8=0x11, gfx10=0x09, gfx11=0x11)),
   ("global_load_ushort",            op(gfx8=0x12, gfx10=0x0a, gfx11=0x12)),
   ("global_load_sshort",            op(gfx8=0x13, gfx10=0x0b, gfx11=0x13)),
   ("global_load_dword",             op(gfx8=0x14, gfx10=0x0c, gfx11=0x14)),
   ("global_load_dwordx2",           op(gfx8=0x15, gfx10=0x0d, gfx11=0x15)),
   ("global_load_dwordx3",           op(gfx8=0x16, gfx10=0x0f, gfx11=0x16)),
   ("global_load_dwordx4",           op(gfx8=0x17, gfx10=0x0e, gfx11=0x17)),
   ("global_store_byte",             op(gfx8=0x18)),
   ("global_store_byte_d16_hi",      op(gfx8=0x19, gfx11=0x24)),
   ("global_store_short",            op(gfx8=0x1a, gfx11=0x19)),
   ("global_store_short_d16_hi",     op(gfx8=0x1b, gfx11=0x25)),
   ("global_store_dword",            op(gfx8=0x1c, gfx11=0x1a)),
   ("global_store_dwordx2",          op(gfx8=0x1d, gfx11=0x1b)),
   ("global_store_dwordx3",          op(gfx8=0x1e, gfx10=0x1f, gfx11=0x1c)),
   ("global_store_dwordx4",          op(gfx8=0x1f, gfx10=0x1e, gfx11=0x1d)),
   ("global_load_ubyte_d16",         op(gfx8=0x20, gfx11=0x1e)),
   ("global_load_ubyte_d16_hi",      op(gfx8=0x21)),
   ("global_load_sbyte_d16",         op(gfx8=0x22, gfx11=0x1f)),
   ("global_load_sbyte_d16_hi",      op(gfx8=0x23, gfx11=0x22)),
   ("global_load_short_d16",         op(gfx8=0x24, gfx11=0x20)),
   ("global_load_short_d16_hi",      op(gfx8=0x25, gfx11=0x23)),
   ("global_atomic_swap",            op(gfx8=0x40, gfx10=0x30, gfx11=0x33)),
   ("global_atomic_cmpswap",         op(gfx8=0x41, gfx10=0x31, gfx11=0x34)),
   ("global_atomic_add",             op(gfx8=0x42, gfx10=0x32, gfx11=0x35)),
   ("global_atomic_sub",             op(gfx8=0x43, gfx10=0x33, gfx11=0x36)),
   ("global_atomic_smin",            op(gfx8=0x44, gfx10=0x35, gfx11=0x38)),
   ("global_atomic_umin",            op(gfx8=0x45, gfx10=0x36, gfx11=0x39)),
   ("global_atomic_smax",            op(gfx8=0x46, gfx10=0x37, gfx11=0x3a)),
   ("global_atomic_umax",            op(gfx8=0x47, gfx10=0x38, gfx11=0x3b)),
   ("global_atomic_and",             op(gfx8=0x48, gfx10=0x39, gfx11=0x3c)),
   ("global_atomic_or",              op(gfx8=0x49, gfx10=0x3a, gfx11=0x3d)),
   ("global_atomic_xor",             op(gfx8=0x4a, gfx10=0x3b, gfx11=0x3e)),
   ("global_atomic_inc",             op(gfx8=0x4b, gfx10=0x3c, gfx11=0x3f)),
   ("global_atomic_dec",             op(gfx8=0x4c, gfx10=0x3d, gfx11=0x40)),
   ("global_atomic_fcmpswap",        op(gfx10=0x3e, gfx11=0x50)),
   ("global_atomic_fmin",            op(gfx10=0x3f, gfx11=0x51)),
   ("global_atomic_fmax",            op(gfx10=0x40, gfx11=0x52)),
   ("global_atomic_swap_x2",         op(gfx8=0x60, gfx10=0x50, gfx11=0x41)),
   ("global_atomic_cmpswap_x2",      op(gfx8=0x61, gfx10=0x51, gfx11=0x42)),
   ("global_atomic_add_x2",          op(gfx8=0x62, gfx10=0x52, gfx11=0x43)),
   ("global_atomic_sub_x2",          op(gfx8=0x63, gfx10=0x53, gfx11=0x44)),
   ("global_atomic_smin_x2",         op(gfx8=0x64, gfx10=0x55, gfx11=0x45)),
   ("global_atomic_umin_x2",         op(gfx8=0x65, gfx10=0x56, gfx11=0x46)),
   ("global_atomic_smax_x2",         op(gfx8=0x66, gfx10=0x57, gfx11=0x47)),
   ("global_atomic_umax_x2",         op(gfx8=0x67, gfx10=0x58, gfx11=0x48)),
   ("global_atomic_and_x2",          op(gfx8=0x68, gfx10=0x59, gfx11=0x49)),
   ("global_atomic_or_x2",           op(gfx8=0x69, gfx10=0x5a, gfx11=0x4a)),
   ("global_atomic_xor_x2",          op(gfx8=0x6a, gfx10=0x5b, gfx11=0x4b)),
   ("global_atomic_inc_x2",          op(gfx8=0x6b, gfx10=0x5c, gfx11=0x4c)),
   ("global_atomic_dec_x2",          op(gfx8=0x6c, gfx10=0x5d, gfx11=0x4d)),
   ("global_atomic_fcmpswap_x2",     op(gfx10=0x5e, gfx11=-1)),
   ("global_atomic_fmin_x2",         op(gfx10=0x5f, gfx11=-1)),
   ("global_atomic_fmax_x2",         op(gfx10=0x60, gfx11=-1)),
   ("global_load_dword_addtid",      op(gfx10=0x16, gfx11=0x28)), #GFX10.3+
   ("global_store_dword_addtid",     op(gfx10=0x17, gfx11=0x29)), #GFX10.3+
   ("global_atomic_csub",            op(gfx10=0x34, gfx11=0x37)), #GFX10.3+. seems glc must be set
   ("global_atomic_add_f32",         op(gfx11=0x56)),
}
for (name, num) in GLOBAL:
    insn(name, num, Format.GLOBAL, InstrClass.VMem, is_atomic = "atomic" in name)

SCRATCH = {
   #GFX89,GFX10,GFX11
   ("scratch_load_ubyte",         op(gfx8=0x10, gfx10=0x08, gfx11=0x10)),
   ("scratch_load_sbyte",         op(gfx8=0x11, gfx10=0x09, gfx11=0x11)),
   ("scratch_load_ushort",        op(gfx8=0x12, gfx10=0x0a, gfx11=0x12)),
   ("scratch_load_sshort",        op(gfx8=0x13, gfx10=0x0b, gfx11=0x13)),
   ("scratch_load_dword",         op(gfx8=0x14, gfx10=0x0c, gfx11=0x14)),
   ("scratch_load_dwordx2",       op(gfx8=0x15, gfx10=0x0d, gfx11=0x15)),
   ("scratch_load_dwordx3",       op(gfx8=0x16, gfx10=0x0f, gfx11=0x16)),
   ("scratch_load_dwordx4",       op(gfx8=0x17, gfx10=0x0e, gfx11=0x17)),
   ("scratch_store_byte",         op(gfx8=0x18)),
   ("scratch_store_byte_d16_hi",  op(gfx8=0x19, gfx11=0x24)),
   ("scratch_store_short",        op(gfx8=0x1a, gfx11=0x19)),
   ("scratch_store_short_d16_hi", op(gfx8=0x1b, gfx11=0x25)),
   ("scratch_store_dword",        op(gfx8=0x1c, gfx11=0x1a)),
   ("scratch_store_dwordx2",      op(gfx8=0x1d, gfx11=0x1b)),
   ("scratch_store_dwordx3",      op(gfx8=0x1e, gfx10=0x1f, gfx11=0x1c)),
   ("scratch_store_dwordx4",      op(gfx8=0x1f, gfx10=0x1e, gfx11=0x1d)),
   ("scratch_load_ubyte_d16",     op(gfx8=0x20, gfx11=0x1e)),
   ("scratch_load_ubyte_d16_hi",  op(gfx8=0x21)),
   ("scratch_load_sbyte_d16",     op(gfx8=0x22, gfx11=0x1f)),
   ("scratch_load_sbyte_d16_hi",  op(gfx8=0x23, gfx11=0x22)),
   ("scratch_load_short_d16",     op(gfx8=0x24, gfx11=0x20)),
   ("scratch_load_short_d16_hi",  op(gfx8=0x25, gfx11=0x23)),
}
for (name, num) in SCRATCH:
    insn(name, num, Format.SCRATCH, InstrClass.VMem)

# check for duplicate opcode numbers
for ver in Opcode._fields:
    op_to_name = {}
    for inst in instructions.values():
        if inst.format in [Format.PSEUDO, Format.PSEUDO_BRANCH, Format.PSEUDO_BARRIER, Format.PSEUDO_REDUCTION]:
            continue

        opcode = getattr(inst.op, ver)
        if opcode == -1:
            continue

        key = (inst.format, opcode)

        if key in op_to_name:
            # exceptions
            names = set([op_to_name[key], inst.name])
            if ver in ['gfx8', 'gfx9', 'gfx11'] and names == set(['v_mul_lo_i32', 'v_mul_lo_u32']):
                continue
            # v_mad_legacy_f32 is replaced with v_fma_legacy_f32 on GFX10.3
            if ver == 'gfx10' and names == set(['v_mad_legacy_f32', 'v_fma_legacy_f32']):
                continue
            # v_mac_legacy_f32 is replaced with v_fmac_legacy_f32 on GFX10.3
            if ver == 'gfx10' and names == set(['v_mac_legacy_f32', 'v_fmac_legacy_f32']):
                continue
            # These are the same opcodes, but hi uses opsel
            if names == set(['v_interp_p2_f16', 'v_interp_p2_hi_f16']):
                continue

            print('%s and %s share the same opcode number (%s)' % (op_to_name[key], inst.name, ver))
            sys.exit(1)
        else:
            op_to_name[key] = inst.name
