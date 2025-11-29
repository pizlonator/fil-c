%{
/*
 * Copyright © 2018 Intel Corporation
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "elk_asm.h"

#undef yyerror
#ifdef YYBYACC
struct YYLTYPE;
void yyerror (struct YYLTYPE *, char *);
#else
void yyerror (char *);
#endif

#undef ALIGN16

#define YYLTYPE YYLTYPE
typedef struct YYLTYPE
{
	int first_line;
	int first_column;
	int last_line;
	int last_column;
} YYLTYPE;

enum message_level {
	WARN,
	ERROR,
};

int yydebug = 1;

static void
message(enum message_level level, YYLTYPE *location,
	const char *fmt, ...)
{
	static const char *level_str[] = { "warning", "error" };
	va_list args;

	if (location)
		fprintf(stderr, "%s:%d:%d: %s: ", input_filename,
		        location->first_line,
		        location->first_column, level_str[level]);
	else
		fprintf(stderr, "%s:%s: ", input_filename, level_str[level]);

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

#define warn(flag, l, fmt, ...) 				 \
	do {							 \
		if (warning_flags & WARN_ ## flag)		 \
			message(WARN, l, fmt, ## __VA_ARGS__);	 \
	} while (0)

#define error(l, fmt, ...)				   	 \
	do {						   	 \
		message(ERROR, l, fmt, ## __VA_ARGS__);	   	 \
	} while (0)

static bool
isPowerofTwo(unsigned int x)
{
	return x && (!(x & (x - 1)));
}

static struct elk_reg
set_direct_src_operand(struct elk_reg *reg, int type)
{
	return elk_reg(reg->file,
		       reg->nr,
		       reg->subnr,
		       0,		// negate
		       0,		// abs
		       type,
		       0,		// vstride
		       0,		// width
		       0,		// hstride
		       ELK_SWIZZLE_NOOP,
		       WRITEMASK_XYZW);
}

static void
i965_asm_unary_instruction(int opcode, struct elk_codegen *p,
		           struct elk_reg dest, struct elk_reg src0)
{
	switch (opcode) {
	case ELK_OPCODE_BFREV:
		elk_BFREV(p, dest, src0);
		break;
	case ELK_OPCODE_CBIT:
		elk_CBIT(p, dest, src0);
		break;
	case ELK_OPCODE_F32TO16:
		elk_F32TO16(p, dest, src0);
		break;
	case ELK_OPCODE_F16TO32:
		elk_F16TO32(p, dest, src0);
		break;
	case ELK_OPCODE_MOV:
		elk_MOV(p, dest, src0);
		break;
	case ELK_OPCODE_FBL:
		elk_FBL(p, dest, src0);
		break;
	case ELK_OPCODE_FRC:
		elk_FRC(p, dest, src0);
		break;
	case ELK_OPCODE_FBH:
		elk_FBH(p, dest, src0);
		break;
	case ELK_OPCODE_NOT:
		elk_NOT(p, dest, src0);
		break;
	case ELK_OPCODE_RNDE:
		elk_RNDE(p, dest, src0);
		break;
	case ELK_OPCODE_RNDZ:
		elk_RNDZ(p, dest, src0);
		break;
	case ELK_OPCODE_RNDD:
		elk_RNDD(p, dest, src0);
		break;
	case ELK_OPCODE_LZD:
		elk_LZD(p, dest, src0);
		break;
	case ELK_OPCODE_DIM:
		elk_DIM(p, dest, src0);
		break;
	case ELK_OPCODE_RNDU:
		fprintf(stderr, "Opcode ELK_OPCODE_RNDU unhandled\n");
		break;
	default:
		fprintf(stderr, "Unsupported unary opcode\n");
	}
}

static void
i965_asm_binary_instruction(int opcode,
			    struct elk_codegen *p,
			    struct elk_reg dest,
			    struct elk_reg src0,
			    struct elk_reg src1)
{
	switch (opcode) {
	case ELK_OPCODE_ADDC:
		elk_ADDC(p, dest, src0, src1);
		break;
	case ELK_OPCODE_BFI1:
		elk_BFI1(p, dest, src0, src1);
		break;
	case ELK_OPCODE_DP2:
		elk_DP2(p, dest, src0, src1);
		break;
	case ELK_OPCODE_DP3:
		elk_DP3(p, dest, src0, src1);
		break;
	case ELK_OPCODE_DP4:
		elk_DP4(p, dest, src0, src1);
		break;
	case ELK_OPCODE_DPH:
		elk_DPH(p, dest, src0, src1);
		break;
	case ELK_OPCODE_LINE:
		elk_LINE(p, dest, src0, src1);
		break;
	case ELK_OPCODE_MAC:
		elk_MAC(p, dest, src0, src1);
		break;
	case ELK_OPCODE_MACH:
		elk_MACH(p, dest, src0, src1);
		break;
	case ELK_OPCODE_PLN:
		elk_PLN(p, dest, src0, src1);
		break;
	case ELK_OPCODE_SAD2:
		fprintf(stderr, "Opcode ELK_OPCODE_SAD2 unhandled\n");
		break;
	case ELK_OPCODE_SADA2:
		fprintf(stderr, "Opcode ELK_OPCODE_SADA2 unhandled\n");
		break;
	case ELK_OPCODE_SUBB:
		elk_SUBB(p, dest, src0, src1);
		break;
	case ELK_OPCODE_ADD:
		elk_ADD(p, dest, src0, src1);
		break;
	case ELK_OPCODE_CMP:
		/* Third parameter is conditional modifier
		 * which gets updated later
		 */
		elk_CMP(p, dest, 0, src0, src1);
		break;
	case ELK_OPCODE_AND:
		elk_AND(p, dest, src0, src1);
		break;
	case ELK_OPCODE_ASR:
		elk_ASR(p, dest, src0, src1);
		break;
	case ELK_OPCODE_AVG:
		elk_AVG(p, dest, src0, src1);
		break;
	case ELK_OPCODE_OR:
		elk_OR(p, dest, src0, src1);
		break;
	case ELK_OPCODE_SEL:
		elk_SEL(p, dest, src0, src1);
		break;
	case ELK_OPCODE_SHL:
		elk_SHL(p, dest, src0, src1);
		break;
	case ELK_OPCODE_SHR:
		elk_SHR(p, dest, src0, src1);
		break;
	case ELK_OPCODE_XOR:
		elk_XOR(p, dest, src0, src1);
		break;
	case ELK_OPCODE_MUL:
		elk_MUL(p, dest, src0, src1);
		break;
	default:
		fprintf(stderr, "Unsupported binary opcode\n");
	}
}

static void
i965_asm_ternary_instruction(int opcode,
			     struct elk_codegen *p,
			     struct elk_reg dest,
			     struct elk_reg src0,
			     struct elk_reg src1,
			     struct elk_reg src2)
{
	switch (opcode) {
	case ELK_OPCODE_MAD:
		elk_MAD(p, dest, src0, src1, src2);
		break;
	case ELK_OPCODE_CSEL:
		elk_CSEL(p, dest, src0, src1, src2);
		break;
	case ELK_OPCODE_LRP:
		elk_LRP(p, dest, src0, src1, src2);
		break;
	case ELK_OPCODE_BFE:
		elk_BFE(p, dest, src0, src1, src2);
		break;
	case ELK_OPCODE_BFI2:
		elk_BFI2(p, dest, src0, src1, src2);
		break;
	default:
		fprintf(stderr, "Unsupported ternary opcode\n");
	}
}

static void
i965_asm_set_instruction_options(struct elk_codegen *p,
				 struct options options)
{
	elk_inst_set_access_mode(p->devinfo, elk_last_inst,
			         options.access_mode);
	elk_inst_set_mask_control(p->devinfo, elk_last_inst,
				  options.mask_control);
        elk_inst_set_thread_control(p->devinfo, elk_last_inst,
                                    options.thread_control);
        elk_inst_set_no_dd_check(p->devinfo, elk_last_inst,
                                 options.no_dd_check);
        elk_inst_set_no_dd_clear(p->devinfo, elk_last_inst,
                                 options.no_dd_clear);
	elk_inst_set_debug_control(p->devinfo, elk_last_inst,
			           options.debug_control);
	if (p->devinfo->ver >= 6)
		elk_inst_set_acc_wr_control(p->devinfo, elk_last_inst,
					    options.acc_wr_control);
	elk_inst_set_cmpt_control(p->devinfo, elk_last_inst,
				  options.compaction);
}

static void
i965_asm_set_dst_nr(struct elk_codegen *p,
	            struct elk_reg *reg,
	            struct options options)
{
	if (p->devinfo->ver <= 6) {
		if (reg->file == ELK_MESSAGE_REGISTER_FILE &&
		    options.qtr_ctrl == ELK_COMPRESSION_COMPRESSED &&
		    !options.is_compr)
			reg->nr |= ELK_MRF_COMPR4;
	}
}

static void
add_label(struct elk_codegen *p, const char* label_name, enum instr_label_type type)
{
	if (!label_name) {
		return;
	}

	struct instr_label *label = rzalloc(p->mem_ctx, struct instr_label);

	label->name = ralloc_strdup(p->mem_ctx, label_name);
	label->offset = p->next_insn_offset;
	label->type = type;

	list_addtail(&label->link, &instr_labels);
}

%}

%locations

%start ROOT

%union {
	char *string;
	double number;
	int integer;
	unsigned long long int llint;
	struct elk_reg reg;
	enum elk_reg_type reg_type;
	struct elk_codegen *program;
	struct predicate predicate;
	struct condition condition;
	struct options options;
	struct instoption instoption;
	struct msgdesc msgdesc;
	elk_inst *instruction;
}

%token ABS
%token COLON
%token COMMA
%token DOT
%token LANGLE RANGLE
%token LCURLY RCURLY
%token LPAREN RPAREN
%token LSQUARE RSQUARE
%token PLUS MINUS
%token SEMICOLON
%token ASSIGN

/* datatypes */
%token <integer> TYPE_B TYPE_UB
%token <integer> TYPE_W TYPE_UW
%token <integer> TYPE_D TYPE_UD
%token <integer> TYPE_Q TYPE_UQ
%token <integer> TYPE_V TYPE_UV
%token <integer> TYPE_F TYPE_HF
%token <integer> TYPE_DF TYPE_NF
%token <integer> TYPE_VF

/* label */
%token <string> JUMP_LABEL
%token <string> JUMP_LABEL_TARGET

/* opcodes */
%token <integer> ADD ADDC AND ASR AVG
%token <integer> BFE BFI1 BFI2 BFB BFREV BRC BRD BREAK
%token <integer> CALL CALLA CASE CBIT CMP CMPN CONT CSEL
%token <integer> DIM DO DP2 DP3 DP4 DPH
%token <integer> ELSE ENDIF F16TO32 F32TO16 FBH FBL FORK FRC
%token <integer> GOTO
%token <integer> HALT
%token <integer> IF IFF ILLEGAL
%token <integer> JMPI JOIN
%token <integer> LINE LRP LZD
%token <integer> MAC MACH MAD MADM MOV MOVI MUL MREST MSAVE
%token <integer> NENOP NOP NOT
%token <integer> OR
%token <integer> PLN POP PUSH
%token <integer> RET RNDD RNDE RNDU RNDZ
%token <integer> SAD2 SADA2 SEL SHL SHR SMOV SUBB
%token <integer> SEND SENDC
%token <integer> WAIT WHILE
%token <integer> XOR

/* extended math functions */
%token <integer> COS EXP FDIV INV INVM INTDIV INTDIVMOD INTMOD LOG POW RSQ
%token <integer> RSQRTM SIN SINCOS SQRT

/* shared functions for send */
%token CONST CRE DATA DP_DATA_1 GATEWAY MATH PIXEL_INTERP READ RENDER SAMPLER
%token THREAD_SPAWNER URB VME WRITE DP_SAMPLER RT_ACCEL SLM TGM UGM

/* message details for send */
%token MSGDESC_BEGIN SRC1_LEN EX_BSO MSGDESC_END
%type <msgdesc> msgdesc msgdesc_parts;

/* Conditional modifiers */
%token <integer> EQUAL GREATER GREATER_EQUAL LESS LESS_EQUAL NOT_EQUAL
%token <integer> NOT_ZERO OVERFLOW UNORDERED ZERO

/* register Access Modes */
%token ALIGN1 ALIGN16

/* accumulator write control */
%token ACCWREN

/* compaction control */
%token CMPTCTRL

/* compression control */
%token COMPR COMPR4 SECHALF

/* mask control (WeCtrl) */
%token WECTRL

/* debug control */
%token BREAKPOINT

/* dependency control */
%token NODDCLR NODDCHK

/* end of thread */
%token EOT

/* mask control */
%token MASK_DISABLE;

/* predicate control */
%token <integer> ANYV ALLV ANY2H ALL2H ANY4H ALL4H ANY8H ALL8H ANY16H ALL16H
%token <integer> ANY32H ALL32H

/* round instructions */
%token <integer> ROUND_INCREMENT

/* staturation */
%token SATURATE

/* thread control */
%token ATOMIC SWITCH

/* quater control */
%token QTR_2Q QTR_3Q QTR_4Q QTR_2H QTR_2N QTR_3N QTR_4N QTR_5N
%token QTR_6N QTR_7N QTR_8N

/* channels */
%token <integer> X Y Z W

/* reg files */
%token GENREGFILE MSGREGFILE

/* vertical stride in register region */
%token VxH

/* register type */
%token <integer> GENREG MSGREG ADDRREG ACCREG FLAGREG NOTIFYREG STATEREG
%token <integer> CONTROLREG IPREG PERFORMANCEREG THREADREG CHANNELENABLEREG
%token <integer> MASKREG

%token <integer> INTEGER
%token <llint> LONG
%token NULL_TOKEN

%nonassoc SUBREGNUM
%left PLUS MINUS
%nonassoc DOT
%nonassoc EMPTYEXECSIZE
%nonassoc LPAREN

%type <integer> execsize simple_int exp
%type <llint> exp2

/* predicate control */
%type <integer> predctrl predstate
%type <predicate> predicate

/* conditional modifier */
%type <condition> cond_mod
%type <integer> condModifiers

/* instruction options  */
%type <options> instoptions instoption_list
%type <instoption> instoption

/* writemask */
%type <integer> writemask_x writemask_y writemask_z writemask_w
%type <integer> writemask

/* dst operand */
%type <reg> dst dstoperand dstoperandex dstoperandex_typed dstreg
%type <integer> dstregion

%type <integer> saturate relativelocation rellocation
%type <reg> relativelocation2

/* src operand */
%type <reg> directsrcoperand directsrcaccoperand indirectsrcoperand srcacc
%type <reg> srcarcoperandex srcaccimm srcarcoperandex_typed srcimm
%type <reg> indirectgenreg indirectregion
%type <reg> immreg src reg32 payload directgenreg_list addrparam region
%type <reg> region_wh directgenreg directmsgreg indirectmsgreg
%type <integer> swizzle

/* registers */
%type <reg> accreg addrreg channelenablereg controlreg flagreg ipreg
%type <reg> notifyreg nullreg performancereg threadcontrolreg statereg maskreg
%type <integer> subregnum

/* register types */
%type <reg_type> reg_type imm_type

/* immediate values */
%type <llint> immval

/* instruction opcodes */
%type <integer> unaryopcodes binaryopcodes binaryaccopcodes ternaryopcodes
%type <integer> sendop
%type <instruction> sendopcode

%type <integer> negate abs chansel math_function sharedfunction

%type <string> jumplabeltarget
%type <string> jumplabel

%code {

static void
add_instruction_option(struct options *options, struct instoption opt)
{
	switch (opt.uint_value) {
	case ALIGN1:
		options->access_mode = ELK_ALIGN_1;
		break;
	case ALIGN16:
		options->access_mode = ELK_ALIGN_16;
		break;
	case SECHALF:
		options->qtr_ctrl |= ELK_COMPRESSION_2NDHALF;
		break;
	case COMPR:
		options->qtr_ctrl |= ELK_COMPRESSION_COMPRESSED;
		options->is_compr = true;
		break;
	case COMPR4:
		options->qtr_ctrl |= ELK_COMPRESSION_COMPRESSED;
		break;
	case SWITCH:
		options->thread_control |= ELK_THREAD_SWITCH;
		break;
	case ATOMIC:
		options->thread_control |= ELK_THREAD_ATOMIC;
		break;
	case NODDCHK:
		options->no_dd_check = true;
		break;
	case NODDCLR:
		options->no_dd_clear = ELK_DEPENDENCY_NOTCLEARED;
		break;
	case MASK_DISABLE:
		options->mask_control |= ELK_MASK_DISABLE;
		break;
	case BREAKPOINT:
		options->debug_control = ELK_DEBUG_BREAKPOINT;
		break;
	case WECTRL:
		options->mask_control |= ELK_WE_ALL;
		break;
	case CMPTCTRL:
		options->compaction = true;
		break;
	case ACCWREN:
		options->acc_wr_control = true;
		break;
	case EOT:
		options->end_of_thread = true;
		break;
	/* TODO : Figure out how to set instruction group and get rid of
	 * code below
	 */
	case QTR_2Q:
		options->qtr_ctrl = ELK_COMPRESSION_2NDHALF;
		break;
	case QTR_3Q:
		options->qtr_ctrl = ELK_COMPRESSION_COMPRESSED;
		break;
	case QTR_4Q:
		options->qtr_ctrl = 3;
		break;
	case QTR_2H:
		options->qtr_ctrl = ELK_COMPRESSION_COMPRESSED;
		break;
	case QTR_2N:
		options->qtr_ctrl = ELK_COMPRESSION_NONE;
		options->nib_ctrl = true;
		break;
	case QTR_3N:
		options->qtr_ctrl = ELK_COMPRESSION_2NDHALF;
		break;
	case QTR_4N:
		options->qtr_ctrl = ELK_COMPRESSION_2NDHALF;
		options->nib_ctrl = true;
		break;
	case QTR_5N:
		options->qtr_ctrl = ELK_COMPRESSION_COMPRESSED;
		break;
	case QTR_6N:
		options->qtr_ctrl = ELK_COMPRESSION_COMPRESSED;
		options->nib_ctrl = true;
		break;
	case QTR_7N:
		options->qtr_ctrl = 3;
		break;
	case QTR_8N:
		options->qtr_ctrl = 3;
		options->nib_ctrl = true;
		break;
	}
}
}
%%

ROOT:
	instrseq
	;

instrseq:
	instrseq instruction SEMICOLON
	| instrseq relocatableinstruction SEMICOLON
	| instruction SEMICOLON
	| relocatableinstruction SEMICOLON
	| instrseq jumplabeltarget
	| jumplabeltarget
	;

/* Instruction Group */
instruction:
	unaryinstruction
	| binaryinstruction
	| binaryaccinstruction
	| mathinstruction
	| nopinstruction
	| waitinstruction
	| ternaryinstruction
	| sendinstruction
	| illegalinstruction
	;

relocatableinstruction:
	jumpinstruction
	| branchinstruction
	| breakinstruction
	| loopinstruction
	;

illegalinstruction:
	ILLEGAL execsize instoptions
	{
		elk_next_insn(p, $1);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $2);
		i965_asm_set_instruction_options(p, $3);
	}
	;

/* Unary instruction */
unaryinstruction:
	predicate unaryopcodes saturate cond_mod execsize dst srcaccimm	instoptions
	{
		i965_asm_set_dst_nr(p, &$6, $8);
		elk_set_default_access_mode(p, $8.access_mode);
		i965_asm_unary_instruction($2, p, $6, $7);
		elk_pop_insn_state(p);
		i965_asm_set_instruction_options(p, $8);
		elk_inst_set_cond_modifier(p->devinfo, elk_last_inst,
					   $4.cond_modifier);

		if (p->devinfo->ver >= 7 && $2 != ELK_OPCODE_DIM &&
		    !elk_inst_flag_reg_nr(p->devinfo, elk_last_inst)) {
			elk_inst_set_flag_reg_nr(p->devinfo,
						 elk_last_inst,
						 $4.flag_reg_nr);
			elk_inst_set_flag_subreg_nr(p->devinfo,
						    elk_last_inst,
						    $4.flag_subreg_nr);
		}

		if ($7.file != ELK_IMMEDIATE_VALUE) {
			elk_inst_set_src0_vstride(p->devinfo, elk_last_inst,
						  $7.vstride);
		}
		elk_inst_set_saturate(p->devinfo, elk_last_inst, $3);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $5);
		// TODO: set instruction group instead of qtr and nib ctrl
		elk_inst_set_qtr_control(p->devinfo, elk_last_inst,
				         $8.qtr_ctrl);

		if (p->devinfo->ver >= 7)
			elk_inst_set_nib_control(p->devinfo, elk_last_inst,
					         $8.nib_ctrl);
	}
	;

unaryopcodes:
	BFREV
	| CBIT
	| DIM
	| F16TO32
	| F32TO16
	| FBH
	| FBL
	| FRC
	| LZD
	| MOV
	| NOT
	| RNDD
	| RNDE
	| RNDU
	| RNDZ
	;

/* Binary instruction */
binaryinstruction:
	predicate binaryopcodes saturate cond_mod execsize dst srcimm srcimm instoptions
	{
		i965_asm_set_dst_nr(p, &$6, $9);
		elk_set_default_access_mode(p, $9.access_mode);
		i965_asm_binary_instruction($2, p, $6, $7, $8);
		i965_asm_set_instruction_options(p, $9);
		elk_inst_set_cond_modifier(p->devinfo, elk_last_inst,
					   $4.cond_modifier);

		if (p->devinfo->ver >= 7 &&
		    !elk_inst_flag_reg_nr(p->devinfo, elk_last_inst)) {
			elk_inst_set_flag_reg_nr(p->devinfo, elk_last_inst,
					         $4.flag_reg_nr);
			elk_inst_set_flag_subreg_nr(p->devinfo, elk_last_inst,
						    $4.flag_subreg_nr);
		}

		elk_inst_set_saturate(p->devinfo, elk_last_inst, $3);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $5);
		// TODO: set instruction group instead of qtr and nib ctrl
		elk_inst_set_qtr_control(p->devinfo, elk_last_inst,
				         $9.qtr_ctrl);

		if (p->devinfo->ver >= 7)
			elk_inst_set_nib_control(p->devinfo, elk_last_inst,
					         $9.nib_ctrl);

		elk_pop_insn_state(p);
	}
	;

binaryopcodes:
	ADDC
	| BFI1
	| DP2
	| DP3
	| DP4
	| DPH
	| LINE
	| MAC
	| MACH
	| MUL
	| PLN
	| SAD2
	| SADA2
	| SUBB
	;

/* Binary acc instruction */
binaryaccinstruction:
	predicate binaryaccopcodes saturate cond_mod execsize dst srcacc srcimm instoptions
	{
		i965_asm_set_dst_nr(p, &$6, $9);
		elk_set_default_access_mode(p, $9.access_mode);
		i965_asm_binary_instruction($2, p, $6, $7, $8);
		elk_pop_insn_state(p);
		i965_asm_set_instruction_options(p, $9);
		elk_inst_set_cond_modifier(p->devinfo, elk_last_inst,
					   $4.cond_modifier);

		if (p->devinfo->ver >= 7 &&
		    !elk_inst_flag_reg_nr(p->devinfo, elk_last_inst)) {
			elk_inst_set_flag_reg_nr(p->devinfo,
						 elk_last_inst,
						 $4.flag_reg_nr);
			elk_inst_set_flag_subreg_nr(p->devinfo,
						    elk_last_inst,
						    $4.flag_subreg_nr);
		}

		elk_inst_set_saturate(p->devinfo, elk_last_inst, $3);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $5);
		// TODO: set instruction group instead of qtr and nib ctrl
		elk_inst_set_qtr_control(p->devinfo, elk_last_inst,
				         $9.qtr_ctrl);

		if (p->devinfo->ver >= 7)
			elk_inst_set_nib_control(p->devinfo, elk_last_inst,
					         $9.nib_ctrl);
	}
	;

binaryaccopcodes:
	ADD
	| AND
	| ASR
	| AVG
	| CMP
	| CMPN
	| OR
	| SEL
	| SHL
	| SHR
	| XOR
	;

/* Math instruction */
mathinstruction:
	predicate MATH saturate math_function execsize dst src srcimm instoptions
	{
		elk_set_default_access_mode(p, $9.access_mode);
		elk_gfx6_math(p, $6, $4, $7, $8);
		i965_asm_set_instruction_options(p, $9);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $5);
		elk_inst_set_saturate(p->devinfo, elk_last_inst, $3);
		// TODO: set instruction group instead
		elk_inst_set_qtr_control(p->devinfo, elk_last_inst,
				         $9.qtr_ctrl);

		if (p->devinfo->ver >= 7)
			elk_inst_set_nib_control(p->devinfo, elk_last_inst,
					         $9.nib_ctrl);

		elk_pop_insn_state(p);
	}
	;

math_function:
	COS
	| EXP
	| FDIV
	| INV
	| INVM
	| INTDIV
	| INTDIVMOD
	| INTMOD
	| LOG
	| POW
	| RSQ
	| RSQRTM
	| SIN
	| SQRT
	| SINCOS
	;

/* NOP instruction */
nopinstruction:
	NOP
	{
		elk_NOP(p);
	}
	;

/* Ternary operand instruction */
ternaryinstruction:
	predicate ternaryopcodes saturate cond_mod execsize dst srcimm src srcimm instoptions
	{
		elk_set_default_access_mode(p, $10.access_mode);
		i965_asm_ternary_instruction($2, p, $6, $7, $8, $9);
		elk_pop_insn_state(p);
		i965_asm_set_instruction_options(p, $10);
		elk_inst_set_cond_modifier(p->devinfo, elk_last_inst,
					   $4.cond_modifier);

		if (p->devinfo->ver >= 7) {
			elk_inst_set_3src_a16_flag_reg_nr(p->devinfo, elk_last_inst,
					         $4.flag_reg_nr);
			elk_inst_set_3src_a16_flag_subreg_nr(p->devinfo, elk_last_inst,
						    $4.flag_subreg_nr);
		}

		elk_inst_set_saturate(p->devinfo, elk_last_inst, $3);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $5);
		// TODO: set instruction group instead of qtr and nib ctrl
		elk_inst_set_qtr_control(p->devinfo, elk_last_inst,
				         $10.qtr_ctrl);

		if (p->devinfo->ver >= 7)
			elk_inst_set_nib_control(p->devinfo, elk_last_inst,
					         $10.nib_ctrl);
	}
	;

ternaryopcodes:
	CSEL
	| BFE
	| BFI2
	| LRP
	| MAD
	;

/* Wait instruction */
waitinstruction:
	WAIT execsize dst instoptions
	{
		elk_next_insn(p, $1);
		i965_asm_set_instruction_options(p, $4);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $2);
		elk_set_default_access_mode(p, $4.access_mode);
		struct elk_reg dest = $3;
		dest.swizzle = elk_swizzle_for_mask(dest.writemask);
		if (dest.file != ARF || dest.nr != ELK_ARF_NOTIFICATION_COUNT)
			error(&@1, "WAIT must use the notification register\n");
		elk_set_dest(p, elk_last_inst, dest);
		elk_set_src0(p, elk_last_inst, dest);
		elk_set_src1(p, elk_last_inst, elk_null_reg());
		elk_inst_set_mask_control(p->devinfo, elk_last_inst, ELK_MASK_DISABLE);
	}
	;

/* Send instruction */
sendinstruction:
	predicate sendopcode execsize dst payload exp2 sharedfunction msgdesc instoptions
	{
		i965_asm_set_instruction_options(p, $9);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);
		elk_set_dest(p, elk_last_inst, $4);
		elk_set_src0(p, elk_last_inst, $5);
		elk_inst_set_bits(elk_last_inst, 127, 96, $6);
		elk_inst_set_src1_file_type(p->devinfo, elk_last_inst,
				            ELK_IMMEDIATE_VALUE,
					    ELK_REGISTER_TYPE_UD);
		elk_inst_set_sfid(p->devinfo, elk_last_inst, $7);
		elk_inst_set_eot(p->devinfo, elk_last_inst, $9.end_of_thread);
		// TODO: set instruction group instead of qtr and nib ctrl
		elk_inst_set_qtr_control(p->devinfo, elk_last_inst,
				         $9.qtr_ctrl);

		if (p->devinfo->ver >= 7)
			elk_inst_set_nib_control(p->devinfo, elk_last_inst,
					         $9.nib_ctrl);

		elk_pop_insn_state(p);
	}
	| predicate sendopcode execsize exp dst payload exp2 sharedfunction msgdesc instoptions
	{
		assert(p->devinfo->ver < 6);

		i965_asm_set_instruction_options(p, $10);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);
		elk_inst_set_base_mrf(p->devinfo, elk_last_inst, $4);
		elk_set_dest(p, elk_last_inst, $5);
		elk_set_src0(p, elk_last_inst, $6);
		elk_inst_set_bits(elk_last_inst, 127, 96, $7);
		elk_inst_set_src1_file_type(p->devinfo, elk_last_inst,
				            ELK_IMMEDIATE_VALUE,
					    ELK_REGISTER_TYPE_UD);
		elk_inst_set_sfid(p->devinfo, elk_last_inst, $8);
		elk_inst_set_eot(p->devinfo, elk_last_inst, $10.end_of_thread);
		// TODO: set instruction group instead of qtr and nib ctrl
		elk_inst_set_qtr_control(p->devinfo, elk_last_inst,
				         $10.qtr_ctrl);

		elk_pop_insn_state(p);
	}
	| predicate sendopcode execsize dst payload payload exp2 sharedfunction msgdesc instoptions
	{
		assert(p->devinfo->ver >= 6);

		i965_asm_set_instruction_options(p, $10);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);
		elk_set_dest(p, elk_last_inst, $4);
		elk_set_src0(p, elk_last_inst, $5);
		elk_inst_set_bits(elk_last_inst, 127, 96, $7);
		elk_inst_set_sfid(p->devinfo, elk_last_inst, $8);
		elk_inst_set_eot(p->devinfo, elk_last_inst, $10.end_of_thread);
		// TODO: set instruction group instead of qtr and nib ctrl
		elk_inst_set_qtr_control(p->devinfo, elk_last_inst,
				         $10.qtr_ctrl);

		if (p->devinfo->ver >= 7)
			elk_inst_set_nib_control(p->devinfo, elk_last_inst,
					         $10.nib_ctrl);

		elk_pop_insn_state(p);
	}
	;

sendop:
	SEND
	| SENDC
	;

sendopcode:
	sendop   { $$ = elk_next_insn(p, $1); }
	;

sharedfunction:
	NULL_TOKEN 	        { $$ = ELK_SFID_NULL; }
	| MATH 		        { $$ = ELK_SFID_MATH; }
	| GATEWAY 	        { $$ = ELK_SFID_MESSAGE_GATEWAY; }
	| READ 		        { $$ = ELK_SFID_DATAPORT_READ; }
	| WRITE 	        { $$ = ELK_SFID_DATAPORT_WRITE; }
	| URB 		        { $$ = ELK_SFID_URB; }
	| THREAD_SPAWNER 	{ $$ = ELK_SFID_THREAD_SPAWNER; }
	| VME 		        { $$ = ELK_SFID_VME; }
	| RENDER 	        { $$ = GFX6_SFID_DATAPORT_RENDER_CACHE; }
	| CONST 	        { $$ = GFX6_SFID_DATAPORT_CONSTANT_CACHE; }
	| DATA 		        { $$ = GFX7_SFID_DATAPORT_DATA_CACHE; }
	| PIXEL_INTERP 	        { $$ = GFX7_SFID_PIXEL_INTERPOLATOR; }
	| DP_DATA_1 	        { $$ = HSW_SFID_DATAPORT_DATA_CACHE_1; }
	| CRE 		        { $$ = HSW_SFID_CRE; }
	| SAMPLER	        { $$ = ELK_SFID_SAMPLER; }
	| DP_SAMPLER	        { $$ = GFX6_SFID_DATAPORT_SAMPLER_CACHE; }
	;

exp2:
	LONG 		{ $$ = $1; }
	| MINUS LONG 	{ $$ = -$2; }
	;

/* Jump instruction */
jumpinstruction:
	predicate JMPI execsize relativelocation2 instoptions
	{
		elk_next_insn(p, $2);
		i965_asm_set_instruction_options(p, $5);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);
		elk_set_dest(p, elk_last_inst, elk_ip_reg());
		elk_set_src0(p, elk_last_inst, elk_ip_reg());
		elk_set_src1(p, elk_last_inst, $4);
		elk_inst_set_pred_control(p->devinfo, elk_last_inst,
					  elk_inst_pred_control(p->devinfo,
								elk_last_inst));
		elk_pop_insn_state(p);
	}
	;

/* branch instruction */
branchinstruction:
	predicate ENDIF execsize JUMP_LABEL instoptions
	{
		add_label(p, $4, INSTR_LABEL_JIP);

		elk_next_insn(p, $2);
		i965_asm_set_instruction_options(p, $5);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);

		if (p->devinfo->ver == 6) {
			elk_set_dest(p, elk_last_inst, elk_imm_w(0x0));
			elk_set_src0(p, elk_last_inst, retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D));
			elk_set_src1(p, elk_last_inst, retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D));
		} else if (p->devinfo->ver == 7) {
			elk_set_dest(p, elk_last_inst, retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D));
			elk_set_src0(p, elk_last_inst, retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D));
			elk_set_src1(p, elk_last_inst, elk_imm_w(0x0));
		} else {
			elk_set_src0(p, elk_last_inst, elk_imm_d(0x0));
		}

		elk_pop_insn_state(p);
	}
	| predicate ENDIF execsize relativelocation instoptions
	{
		elk_next_insn(p, $2);
		i965_asm_set_instruction_options(p, $5);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);

		elk_set_dest(p, elk_last_inst, retype(elk_null_reg(),
					ELK_REGISTER_TYPE_D));
		elk_set_src0(p, elk_last_inst, retype(elk_null_reg(),
					ELK_REGISTER_TYPE_D));
		elk_set_src1(p, elk_last_inst, elk_imm_d(0x0));
		elk_inst_set_gfx4_pop_count(p->devinfo, elk_last_inst, $4);

		elk_inst_set_thread_control(p->devinfo, elk_last_inst,
						ELK_THREAD_SWITCH);

		elk_pop_insn_state(p);
	}
	| ELSE execsize JUMP_LABEL jumplabel instoptions
	{
		add_label(p, $3, INSTR_LABEL_JIP);
		add_label(p, $4, INSTR_LABEL_UIP);

		elk_next_insn(p, $1);
		i965_asm_set_instruction_options(p, $5);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $2);

		if (p->devinfo->ver == 6) {
			elk_set_dest(p, elk_last_inst, elk_imm_w(0x0));
			elk_set_src0(p, elk_last_inst, retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D));
			elk_set_src1(p, elk_last_inst, retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D));
		} else if (p->devinfo->ver == 7) {
			elk_set_dest(p, elk_last_inst, retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D));
			elk_set_src0(p, elk_last_inst, retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D));
			elk_set_src1(p, elk_last_inst, elk_imm_w(0));
		} else {
			elk_set_dest(p, elk_last_inst, retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D));
                        elk_set_src0(p, elk_last_inst, elk_imm_d(0));
		}
	}
	| ELSE execsize relativelocation rellocation instoptions
	{
		elk_next_insn(p, $1);
		i965_asm_set_instruction_options(p, $5);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $2);

		elk_set_dest(p, elk_last_inst, elk_ip_reg());
		elk_set_src0(p, elk_last_inst, elk_ip_reg());
		elk_set_src1(p, elk_last_inst, elk_imm_d(0x0));
		elk_inst_set_gfx4_jump_count(p->devinfo, elk_last_inst, $3);
		elk_inst_set_gfx4_pop_count(p->devinfo, elk_last_inst, $4);

		if (!p->single_program_flow)
			elk_inst_set_thread_control(p->devinfo, elk_last_inst,
						    ELK_THREAD_SWITCH);
	}
	| predicate IF execsize JUMP_LABEL jumplabel instoptions
	{
		add_label(p, $4, INSTR_LABEL_JIP);
		add_label(p, $5, INSTR_LABEL_UIP);

		elk_next_insn(p, $2);
		i965_asm_set_instruction_options(p, $6);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);

		if (p->devinfo->ver == 6) {
			elk_set_dest(p, elk_last_inst, elk_imm_w(0x0));
			elk_set_src0(p, elk_last_inst,
				     vec1(retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D)));
			elk_set_src1(p, elk_last_inst,
				     vec1(retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D)));
		} else if (p->devinfo->ver == 7) {
			elk_set_dest(p, elk_last_inst,
				     vec1(retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D)));
			elk_set_src0(p, elk_last_inst,
				     vec1(retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D)));
			elk_set_src1(p, elk_last_inst, elk_imm_w(0x0));
		} else {
			elk_set_dest(p, elk_last_inst,
				     vec1(retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D)));
			elk_set_src0(p, elk_last_inst, elk_imm_d(0x0));
		}

		elk_pop_insn_state(p);
	}
	| predicate IF execsize relativelocation rellocation instoptions
	{
		elk_next_insn(p, $2);
		i965_asm_set_instruction_options(p, $6);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);

		elk_set_dest(p, elk_last_inst, elk_ip_reg());
		elk_set_src0(p, elk_last_inst, elk_ip_reg());
		elk_set_src1(p, elk_last_inst, elk_imm_d(0x0));
		elk_inst_set_gfx4_jump_count(p->devinfo, elk_last_inst, $4);
		elk_inst_set_gfx4_pop_count(p->devinfo, elk_last_inst, $5);

		if (!p->single_program_flow)
			elk_inst_set_thread_control(p->devinfo, elk_last_inst,
						    ELK_THREAD_SWITCH);

		elk_pop_insn_state(p);
	}
	| predicate IFF execsize JUMP_LABEL instoptions
	{
		add_label(p, $4, INSTR_LABEL_JIP);

		elk_next_insn(p, $2);
		i965_asm_set_instruction_options(p, $5);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);

		if (p->devinfo->ver == 6) {
			elk_set_src0(p, elk_last_inst,
				     vec1(retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D)));
			elk_set_src1(p, elk_last_inst,
				     vec1(retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D)));
		} else if (p->devinfo->ver == 7) {
			elk_set_dest(p, elk_last_inst,
				     vec1(retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D)));
			elk_set_src0(p, elk_last_inst,
				     vec1(retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D)));
			elk_set_src1(p, elk_last_inst, elk_imm_w(0x0));
		} else {
			elk_set_dest(p, elk_last_inst,
				     vec1(retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D)));
			elk_set_src0(p, elk_last_inst, elk_imm_d(0x0));
		}

		elk_pop_insn_state(p);
	}
	| predicate IFF execsize relativelocation instoptions
	{
		elk_next_insn(p, $2);
		i965_asm_set_instruction_options(p, $5);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);

		elk_set_dest(p, elk_last_inst, elk_ip_reg());
		elk_set_src0(p, elk_last_inst, elk_ip_reg());
		elk_inst_set_gfx4_jump_count(p->devinfo, elk_last_inst, $4);
		elk_set_src1(p, elk_last_inst, elk_imm_d($4));

		if (!p->single_program_flow)
			elk_inst_set_thread_control(p->devinfo, elk_last_inst,
						    ELK_THREAD_SWITCH);

		elk_pop_insn_state(p);
	}
	;

/* break instruction */
breakinstruction:
	predicate BREAK execsize JUMP_LABEL JUMP_LABEL instoptions
	{
		add_label(p, $4, INSTR_LABEL_JIP);
		add_label(p, $5, INSTR_LABEL_UIP);

		elk_next_insn(p, $2);
		i965_asm_set_instruction_options(p, $6);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);

		if (p->devinfo->ver >= 8) {
			elk_set_dest(p, elk_last_inst, retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D));
			elk_set_src0(p, elk_last_inst, elk_imm_d(0x0));
		} else {
			elk_set_dest(p, elk_last_inst, retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D));
			elk_set_src0(p, elk_last_inst, retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D));
			elk_set_src1(p, elk_last_inst, elk_imm_d(0x0));
		}

		elk_pop_insn_state(p);
	}
	| predicate BREAK execsize relativelocation relativelocation instoptions
	{
		elk_next_insn(p, $2);
		i965_asm_set_instruction_options(p, $6);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);

		elk_set_dest(p, elk_last_inst, elk_ip_reg());
		elk_set_src0(p, elk_last_inst, elk_ip_reg());
		elk_set_src1(p, elk_last_inst, elk_imm_d(0x0));
		elk_inst_set_gfx4_jump_count(p->devinfo, elk_last_inst, $4);
		elk_inst_set_gfx4_pop_count(p->devinfo, elk_last_inst, $5);

		elk_pop_insn_state(p);
	}
	| predicate HALT execsize JUMP_LABEL JUMP_LABEL instoptions
	{
		add_label(p, $4, INSTR_LABEL_JIP);
		add_label(p, $5, INSTR_LABEL_UIP);

		elk_next_insn(p, $2);
		i965_asm_set_instruction_options(p, $6);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);

		elk_set_dest(p, elk_last_inst, retype(elk_null_reg(),
			     ELK_REGISTER_TYPE_D));

		if (p->devinfo->ver < 8) {
			elk_set_src0(p, elk_last_inst, retype(elk_null_reg(),
				     ELK_REGISTER_TYPE_D));
			elk_set_src1(p, elk_last_inst, elk_imm_d(0x0));
		} else {
			elk_set_src0(p, elk_last_inst, elk_imm_d(0x0));
		}

		elk_pop_insn_state(p);
	}
	| predicate CONT execsize JUMP_LABEL JUMP_LABEL instoptions
	{
		add_label(p, $4, INSTR_LABEL_JIP);
		add_label(p, $5, INSTR_LABEL_UIP);

		elk_next_insn(p, $2);
		i965_asm_set_instruction_options(p, $6);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);
		elk_set_dest(p, elk_last_inst, elk_ip_reg());

		if (p->devinfo->ver >= 8) {
			elk_set_src0(p, elk_last_inst, elk_imm_d(0x0));
		} else {
			elk_set_src0(p, elk_last_inst, elk_ip_reg());
			elk_set_src1(p, elk_last_inst, elk_imm_d(0x0));
		}

		elk_pop_insn_state(p);
	}
	| predicate CONT execsize relativelocation relativelocation instoptions
	{
		elk_next_insn(p, $2);
		i965_asm_set_instruction_options(p, $6);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);
		elk_set_dest(p, elk_last_inst, elk_ip_reg());

		elk_set_src0(p, elk_last_inst, elk_ip_reg());
		elk_set_src1(p, elk_last_inst, elk_imm_d(0x0));

		elk_inst_set_gfx4_jump_count(p->devinfo, elk_last_inst, $4);
		elk_inst_set_gfx4_pop_count(p->devinfo, elk_last_inst, $5);

		elk_pop_insn_state(p);
	}
	;

/* loop instruction */
loopinstruction:
	predicate WHILE execsize JUMP_LABEL instoptions
	{
		add_label(p, $4, INSTR_LABEL_JIP);

		elk_next_insn(p, $2);
		i965_asm_set_instruction_options(p, $5);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);

		if (p->devinfo->ver >= 8) {
			elk_set_dest(p, elk_last_inst,
						retype(elk_null_reg(),
						ELK_REGISTER_TYPE_D));
			elk_set_src0(p, elk_last_inst, elk_imm_d(0x0));
		} else if (p->devinfo->ver == 7) {
			elk_set_dest(p, elk_last_inst,
						retype(elk_null_reg(),
						ELK_REGISTER_TYPE_D));
			elk_set_src0(p, elk_last_inst,
						retype(elk_null_reg(),
						ELK_REGISTER_TYPE_D));
			elk_set_src1(p, elk_last_inst,
						elk_imm_w(0x0));
		} else {
			elk_set_dest(p, elk_last_inst, elk_imm_w(0x0));
			elk_set_src0(p, elk_last_inst,
						retype(elk_null_reg(),
						ELK_REGISTER_TYPE_D));
			elk_set_src1(p, elk_last_inst,
						retype(elk_null_reg(),
						ELK_REGISTER_TYPE_D));
		}

		elk_pop_insn_state(p);
	}
	| predicate WHILE execsize relativelocation instoptions
	{
		elk_next_insn(p, $2);
		i965_asm_set_instruction_options(p, $5);
		elk_inst_set_exec_size(p->devinfo, elk_last_inst, $3);

		elk_set_dest(p, elk_last_inst, elk_ip_reg());
		elk_set_src0(p, elk_last_inst, elk_ip_reg());
		elk_set_src1(p, elk_last_inst, elk_imm_d(0x0));
		elk_inst_set_gfx4_jump_count(p->devinfo, elk_last_inst, $4);
		elk_inst_set_gfx4_pop_count(p->devinfo, elk_last_inst, 0);

		elk_pop_insn_state(p);
	}
	| DO execsize instoptions
	{
		elk_next_insn(p, $1);
		if (p->devinfo->ver < 6) {
			elk_inst_set_exec_size(p->devinfo, elk_last_inst, $2);
			i965_asm_set_instruction_options(p, $3);
			elk_set_dest(p, elk_last_inst, elk_null_reg());
			elk_set_src0(p, elk_last_inst, elk_null_reg());
			elk_set_src1(p, elk_last_inst, elk_null_reg());

			elk_inst_set_qtr_control(p->devinfo, elk_last_inst, ELK_COMPRESSION_NONE);
		}
	}
	;

/* Relative location */
relativelocation2:
	immreg
	| reg32
	;

simple_int:
	INTEGER 	        { $$ = $1; }
	| MINUS INTEGER 	{ $$ = -$2; }
	| LONG 		        { $$ = $1; }
	| MINUS LONG 	        { $$ = -$2; }
	;

rellocation:
	relativelocation
	| /* empty */ { $$ = 0; }
	;

relativelocation:
	simple_int
	{
		$$ = $1;
	}
	;

jumplabel:
	JUMP_LABEL	{ $$ = $1; }
	| /* empty */	{ $$ = NULL; }
	;

jumplabeltarget:
	JUMP_LABEL_TARGET
	{
		struct target_label *label = rzalloc(p->mem_ctx, struct target_label);

		label->name = ralloc_strdup(p->mem_ctx, $1);
		label->offset = p->next_insn_offset;

		list_addtail(&label->link, &target_labels);
	}
	;

/* Destination register */
dst:
	dstoperand
	| dstoperandex
	;

dstoperand:
	dstreg dstregion writemask reg_type
	{
		$$ = $1;
		$$.vstride = ELK_VERTICAL_STRIDE_1;
		$$.width = ELK_WIDTH_1;
		$$.hstride = $2;
		$$.type = $4;
		$$.writemask = $3;
		$$.swizzle = ELK_SWIZZLE_NOOP;
		$$.subnr = $$.subnr * elk_reg_type_to_size($4);
	}
	;

dstoperandex:
	dstoperandex_typed dstregion writemask reg_type
	{
		$$ = $1;
		$$.hstride = $2;
		$$.type = $4;
		$$.writemask = $3;
		$$.subnr = $$.subnr * elk_reg_type_to_size($4);
	}
	/* BSpec says "When the conditional modifier is present, updates
	 * to the selected flag register also occur. In this case, the
	 * register region fields of the ‘null’ operand are valid."
	 */
	| nullreg dstregion writemask reg_type
	{
		$$ = $1;
		$$.vstride = ELK_VERTICAL_STRIDE_1;
		$$.width = ELK_WIDTH_1;
		$$.hstride = $2;
		$$.writemask = $3;
		$$.type = $4;
	}
	| threadcontrolreg
	{
		$$ = $1;
		$$.hstride = 1;
		$$.type = ELK_REGISTER_TYPE_UW;
	}
	;

dstoperandex_typed:
	accreg
	| addrreg
	| channelenablereg
	| controlreg
	| flagreg
	| ipreg
	| maskreg
	| notifyreg
	| performancereg
	| statereg
	;

dstreg:
	directgenreg
	{
		$$ = $1;
		$$.address_mode = ELK_ADDRESS_DIRECT;
	}
	| indirectgenreg
	{
		$$ = $1;
		$$.address_mode = ELK_ADDRESS_REGISTER_INDIRECT_REGISTER;
	}
	| directmsgreg
	{
		$$ = $1;
		$$.address_mode = ELK_ADDRESS_DIRECT;
	}
	| indirectmsgreg
	{
		$$ = $1;
		$$.address_mode = ELK_ADDRESS_REGISTER_INDIRECT_REGISTER;
	}
	;

/* Source register */
srcaccimm:
	srcacc
	| immreg
	;

immreg:
	immval imm_type
	{
		switch ($2) {
		case ELK_REGISTER_TYPE_UD:
			$$ = elk_imm_ud($1);
			break;
		case ELK_REGISTER_TYPE_D:
			$$ = elk_imm_d($1);
			break;
		case ELK_REGISTER_TYPE_UW:
			$$ = elk_imm_uw($1 | ($1 << 16));
			break;
		case ELK_REGISTER_TYPE_W:
			$$ = elk_imm_w($1);
			break;
		case ELK_REGISTER_TYPE_F:
			$$ = elk_imm_reg(ELK_REGISTER_TYPE_F);
			/* Set u64 instead of ud since DIM uses a 64-bit F-typed imm */
			$$.u64 = $1;
			break;
		case ELK_REGISTER_TYPE_V:
			$$ = elk_imm_v($1);
			break;
		case ELK_REGISTER_TYPE_UV:
			$$ = elk_imm_uv($1);
			break;
		case ELK_REGISTER_TYPE_VF:
			$$ = elk_imm_vf($1);
			break;
		case ELK_REGISTER_TYPE_Q:
			$$ = elk_imm_q($1);
			break;
		case ELK_REGISTER_TYPE_UQ:
			$$ = elk_imm_uq($1);
			break;
		case ELK_REGISTER_TYPE_DF:
			$$ = elk_imm_reg(ELK_REGISTER_TYPE_DF);
			$$.d64 = $1;
			break;
		case ELK_REGISTER_TYPE_HF:
			$$ = elk_imm_reg(ELK_REGISTER_TYPE_HF);
			$$.ud = $1 | ($1 << 16);
			break;
		default:
			error(&@2, "Unknown immediate type %s\n",
			      elk_reg_type_to_letters($2));
		}
	}
	;

reg32:
	directgenreg region reg_type
	{
		$$ = set_direct_src_operand(&$1, $3);
		$$ = stride($$, $2.vstride, $2.width, $2.hstride);
	}
	;

payload:
	directsrcoperand
	;

src:
	directsrcoperand
	| indirectsrcoperand
	;

srcacc:
	directsrcaccoperand
	| indirectsrcoperand
	;

srcimm:
	directsrcoperand
	| indirectsrcoperand
	| immreg
	;

directsrcaccoperand:
	directsrcoperand
	| negate abs accreg region reg_type
	{
		$$ = set_direct_src_operand(&$3, $5);
		$$.negate = $1;
		$$.abs = $2;
		$$.vstride = $4.vstride;
		$$.width = $4.width;
		$$.hstride = $4.hstride;
	}
	;

srcarcoperandex:
	srcarcoperandex_typed region reg_type
	{
		$$ = elk_reg($1.file,
			     $1.nr,
			     $1.subnr,
			     0,
			     0,
			     $3,
			     $2.vstride,
			     $2.width,
			     $2.hstride,
			     ELK_SWIZZLE_NOOP,
			     WRITEMASK_XYZW);
	}
	| nullreg region reg_type
	{
		$$ = set_direct_src_operand(&$1, $3);
		$$.vstride = $2.vstride;
		$$.width = $2.width;
		$$.hstride = $2.hstride;
	}
	| threadcontrolreg
	{
		$$ = set_direct_src_operand(&$1, ELK_REGISTER_TYPE_UW);
	}
	;

srcarcoperandex_typed:
	channelenablereg
	| controlreg
	| flagreg
	| ipreg
	| maskreg
	| statereg
	;

indirectsrcoperand:
	negate abs indirectgenreg indirectregion swizzle reg_type
	{
		$$ = elk_reg($3.file,
			     0,
			     $3.subnr,
			     $1,  // negate
			     $2,  // abs
			     $6,
			     $4.vstride,
			     $4.width,
			     $4.hstride,
			     $5,
			     WRITEMASK_X);

		$$.address_mode = ELK_ADDRESS_REGISTER_INDIRECT_REGISTER;
		// elk_reg set indirect_offset to 0 so set it to valid value
		$$.indirect_offset = $3.indirect_offset;
	}
	;

directgenreg_list:
	directgenreg
	| directmsgreg
	| notifyreg
	| addrreg
	| performancereg
	;

directsrcoperand:
	negate abs directgenreg_list region swizzle reg_type
	{
		$$ = elk_reg($3.file,
			     $3.nr,
			     $3.subnr,
			     $1,
			     $2,
			     $6,
			     $4.vstride,
			     $4.width,
			     $4.hstride,
			     $5,
			     WRITEMASK_X);
	}
	| srcarcoperandex
	;

/* Address register */
addrparam:
	addrreg exp
	{
		memset(&$$, '\0', sizeof($$));
		$$.subnr = $1.subnr;
		$$.indirect_offset = $2;
	}
	| addrreg
	;

/* Register files and register numbers */
exp:
	INTEGER 	{ $$ = $1; }
	| LONG 	        { $$ = $1; }
	;

subregnum:
	DOT exp 		        { $$ = $2; }
	| /* empty */ %prec SUBREGNUM 	{ $$ = 0; }
	;

directgenreg:
	GENREG subregnum
	{
		memset(&$$, '\0', sizeof($$));
		$$.file = ELK_GENERAL_REGISTER_FILE;
		$$.nr = $1;
		$$.subnr = $2;
	}
	;

indirectgenreg:
	GENREGFILE LSQUARE addrparam RSQUARE
	{
		memset(&$$, '\0', sizeof($$));
		$$.file = ELK_GENERAL_REGISTER_FILE;
		$$.subnr = $3.subnr;
		$$.indirect_offset = $3.indirect_offset;
	}
	;

directmsgreg:
	MSGREG subregnum
	{
		$$.file = ELK_MESSAGE_REGISTER_FILE;
		$$.nr = $1;
		$$.subnr = $2;
	}
	;

indirectmsgreg:
	MSGREGFILE LSQUARE addrparam RSQUARE
	{
		memset(&$$, '\0', sizeof($$));
		$$.file = ELK_MESSAGE_REGISTER_FILE;
		$$.subnr = $3.subnr;
		$$.indirect_offset = $3.indirect_offset;
	}
	;

addrreg:
	ADDRREG subregnum
	{
		int subnr = (p->devinfo->ver >= 8) ? 16 : 8;

		if ($2 > subnr)
			error(&@2, "Address sub register number %d"
				   "out of range\n", $2);

		$$.file = ELK_ARCHITECTURE_REGISTER_FILE;
		$$.nr = ELK_ARF_ADDRESS;
		$$.subnr = $2;
	}
	;

accreg:
	ACCREG subregnum
	{
		int nr_reg;
		if (p->devinfo->ver < 8)
			nr_reg = 2;
		else
			nr_reg = 10;

		if ($1 > nr_reg)
			error(&@1, "Accumulator register number %d"
				   " out of range\n", $1);

		memset(&$$, '\0', sizeof($$));
		$$.file = ELK_ARCHITECTURE_REGISTER_FILE;
		$$.nr = ELK_ARF_ACCUMULATOR;
		$$.subnr = $2;
	}
	;

flagreg:
	FLAGREG subregnum
	{
		// SNB = 1 flag reg and IVB+ = 2 flag reg
		int nr_reg = (p->devinfo->ver >= 7) ? 2 : 1;
		int subnr = nr_reg;

		if ($1 > nr_reg)
			error(&@1, "Flag register number %d"
				   " out of range \n", $1);
		if ($2 > subnr)
			error(&@2, "Flag subregister number %d"
				   " out of range\n", $2);

		$$.file = ELK_ARCHITECTURE_REGISTER_FILE;
		$$.nr = ELK_ARF_FLAG | $1;
		$$.subnr = $2;
	}
	;

maskreg:
	MASKREG subregnum
	{
		if ($1 > 0)
			error(&@1, "Mask register number %d"
				   " out of range\n", $1);

		$$.file = ELK_ARCHITECTURE_REGISTER_FILE;
		$$.nr = ELK_ARF_MASK;
		$$.subnr = $2;
	}
	;

notifyreg:
	NOTIFYREG subregnum
	{
		int subnr = 3;
		if ($2 > subnr)
			error(&@2, "Notification sub register number %d"
				   " out of range\n", $2);

		$$.file = ELK_ARCHITECTURE_REGISTER_FILE;
		$$.nr = ELK_ARF_NOTIFICATION_COUNT;
		$$.subnr = $2;
	}
	;

statereg:
	STATEREG subregnum
	{
		if ($1 > 2)
			error(&@1, "State register number %d"
				   " out of range\n", $1);

		if ($2 > 4)
			error(&@2, "State sub register number %d"
				   " out of range\n", $2);

		$$.file = ELK_ARCHITECTURE_REGISTER_FILE;
		$$.nr = ELK_ARF_STATE;
		$$.subnr = $2;
	}
	;

controlreg:
	CONTROLREG subregnum
	{
		if ($2 > 3)
			error(&@2, "control sub register number %d"
				   " out of range\n", $2);

		$$.file = ELK_ARCHITECTURE_REGISTER_FILE;
		$$.nr = ELK_ARF_CONTROL;
		$$.subnr = $2;
	}
	;

ipreg:
	IPREG		{ $$ = elk_ip_reg(); }
	;

nullreg:
	NULL_TOKEN 	{ $$ = elk_null_reg(); }
	;

threadcontrolreg:
	THREADREG subregnum
	{
		if ($2 > 7)
			error(&@2, "Thread control sub register number %d"
				   " out of range\n", $2);

		$$.file = ELK_ARCHITECTURE_REGISTER_FILE;
		$$.nr = ELK_ARF_TDR;
		$$.subnr = $2;
	}
	;

performancereg:
	PERFORMANCEREG subregnum
	{
		int subnr;
		if (p->devinfo->ver <= 8)
			subnr = 3;
		else
			subnr = 4;

		if ($2 > subnr)
			error(&@2, "Performance sub register number %d"
				   " out of range\n", $2);

		$$.file = ELK_ARCHITECTURE_REGISTER_FILE;
		$$.nr = ELK_ARF_TIMESTAMP;
		$$.subnr = $2;
	}
	;

channelenablereg:
	CHANNELENABLEREG subregnum
	{
		if ($1 > 0)
			error(&@1, "Channel enable register number %d"
				   " out of range\n", $1);

		$$.file = ELK_ARCHITECTURE_REGISTER_FILE;
		$$.nr = ELK_ARF_MASK;
		$$.subnr = $2;
	}
	;

/* Immediate values */
immval:
	exp2
	{
		$$ = $1;
	}
	| LSQUARE exp2 COMMA exp2 COMMA exp2 COMMA exp2 RSQUARE
	{
		$$ = ($2 << 0) | ($4 << 8) | ($6 << 16) | ($8 << 24);
	}
	;

/* Regions */
dstregion:
	/* empty */
	{
		$$ = ELK_HORIZONTAL_STRIDE_1;
	}
	| LANGLE exp RANGLE
	{
		if ($2 != 0 && ($2 > 4 || !isPowerofTwo($2)))
			error(&@2, "Invalid Horizontal stride %d\n", $2);

		$$ = ffs($2);
	}
	;

indirectregion:
	region
	| region_wh
	;

region:
	/* empty */
	{
		$$ = stride($$, 0, 1, 0);
	}
	| LANGLE exp RANGLE
	{
		if ($2 != 0 && ($2 > 32 || !isPowerofTwo($2)))
			error(&@2, "Invalid VertStride %d\n", $2);

		$$ = stride($$, $2, 1, 0);
	}
	| LANGLE exp COMMA exp COMMA exp RANGLE
	{

		if ($2 != 0 && ($2 > 32 || !isPowerofTwo($2)))
			error(&@2, "Invalid VertStride %d\n", $2);

		if ($4 > 16 || !isPowerofTwo($4))
			error(&@4, "Invalid width %d\n", $4);

		if ($6 != 0 && ($6 > 4 || !isPowerofTwo($6)))
			error(&@6, "Invalid Horizontal stride in"
				   "  region_wh %d\n", $6);

		$$ = stride($$, $2, $4, $6);
	}
	| LANGLE exp SEMICOLON exp COMMA exp RANGLE
	{
		if ($2 != 0 && ($2 > 32 || !isPowerofTwo($2)))
			error(&@2, "Invalid VertStride %d\n", $2);

		if ($4 > 16 || !isPowerofTwo($4))
			error(&@4, "Invalid width %d\n", $4);

		if ($6 != 0 && ($6 > 4 || !isPowerofTwo($6)))
			error(&@6, "Invalid Horizontal stride in"
				   " region_wh %d\n", $6);

		$$ = stride($$, $2, $4, $6);
	}
	| LANGLE VxH COMMA exp COMMA exp RANGLE
	{
		if ($4 > 16 || !isPowerofTwo($4))
			error(&@4, "Invalid width %d\n", $4);

		if ($6 != 0 && ($6 > 4 || !isPowerofTwo($6)))
			error(&@6, "Invalid Horizontal stride in"
				   " region_wh %d\n", $6);

		$$ = elk_VxH_indirect(0, 0);
	}
	;

region_wh:
	LANGLE exp COMMA exp RANGLE
	{
		if ($2 > 16 || !isPowerofTwo($2))
			error(&@2, "Invalid width %d\n", $2);

		if ($4 != 0 && ($4 > 4 || !isPowerofTwo($4)))
			error(&@4, "Invalid Horizontal stride in"
				   " region_wh %d\n", $4);

		$$ = stride($$, 0, $2, $4);
		$$.vstride = ELK_VERTICAL_STRIDE_ONE_DIMENSIONAL;
	}
	;

reg_type:
	  TYPE_F 	{ $$ = ELK_REGISTER_TYPE_F;  }
	| TYPE_UD 	{ $$ = ELK_REGISTER_TYPE_UD; }
	| TYPE_D 	{ $$ = ELK_REGISTER_TYPE_D;  }
	| TYPE_UW 	{ $$ = ELK_REGISTER_TYPE_UW; }
	| TYPE_W 	{ $$ = ELK_REGISTER_TYPE_W;  }
	| TYPE_UB 	{ $$ = ELK_REGISTER_TYPE_UB; }
	| TYPE_B 	{ $$ = ELK_REGISTER_TYPE_B;  }
	| TYPE_DF 	{ $$ = ELK_REGISTER_TYPE_DF; }
	| TYPE_UQ 	{ $$ = ELK_REGISTER_TYPE_UQ; }
	| TYPE_Q 	{ $$ = ELK_REGISTER_TYPE_Q;  }
	| TYPE_HF 	{ $$ = ELK_REGISTER_TYPE_HF; }
	| TYPE_NF 	{ $$ = ELK_REGISTER_TYPE_NF; }
	;

imm_type:
	reg_type 	{ $$ = $1; }
	| TYPE_V 	{ $$ = ELK_REGISTER_TYPE_V;  }
	| TYPE_VF 	{ $$ = ELK_REGISTER_TYPE_VF; }
	| TYPE_UV 	{ $$ = ELK_REGISTER_TYPE_UV; }
	;

writemask:
	/* empty */
	{
		$$ = WRITEMASK_XYZW;
	}
	| DOT writemask_x writemask_y writemask_z writemask_w
	{
		$$ = $2 | $3 | $4 | $5;
	}
	;

writemask_x:
	/* empty */ 	{ $$ = 0; }
	| X 	{ $$ = 1 << ELK_CHANNEL_X; }
	;

writemask_y:
	/* empty */ 	{ $$ = 0; }
	| Y 	{ $$ = 1 << ELK_CHANNEL_Y; }
	;

writemask_z:
	/* empty */ 	{ $$ = 0; }
	| Z 	{ $$ = 1 << ELK_CHANNEL_Z; }
	;

writemask_w:
	/* empty */ 	{ $$ = 0; }
	| W 	{ $$ = 1 << ELK_CHANNEL_W; }
	;

swizzle:
	/* empty */
	{
		$$ = ELK_SWIZZLE_NOOP;
	}
	| DOT chansel
	{
		$$ = ELK_SWIZZLE4($2, $2, $2, $2);
	}
	| DOT chansel chansel chansel chansel
	{
		$$ = ELK_SWIZZLE4($2, $3, $4, $5);
	}
	;

chansel:
	X
	| Y
	| Z
	| W
	;

/* Instruction prediction and modifiers */
predicate:
	/* empty */
	{
		elk_push_insn_state(p);
		elk_set_default_predicate_control(p, ELK_PREDICATE_NONE);
		elk_set_default_flag_reg(p, 0, 0);
		elk_set_default_predicate_inverse(p, false);
	}
	| LPAREN predstate flagreg predctrl RPAREN
	{
		elk_push_insn_state(p);
		elk_set_default_predicate_inverse(p, $2);
		elk_set_default_flag_reg(p, $3.nr, $3.subnr);
		elk_set_default_predicate_control(p, $4);
	}
	;

predstate:
	/* empty */     { $$ = 0; }
	| PLUS 	        { $$ = 0; }
	| MINUS 	{ $$ = 1; }
	;

predctrl:
	/* empty */ 	{ $$ = ELK_PREDICATE_NORMAL; }
	| DOT X 	{ $$ = ELK_PREDICATE_ALIGN16_REPLICATE_X; }
	| DOT Y 	{ $$ = ELK_PREDICATE_ALIGN16_REPLICATE_Y; }
	| DOT Z 	{ $$ = ELK_PREDICATE_ALIGN16_REPLICATE_Z; }
	| DOT W 	{ $$ = ELK_PREDICATE_ALIGN16_REPLICATE_W; }
	| ANYV
	| ALLV
	| ANY2H
	| ALL2H
	| ANY4H
	| ALL4H
	| ANY8H
	| ALL8H
	| ANY16H
	| ALL16H
	| ANY32H
	| ALL32H
	;

/* Source Modification */
negate:
	/* empty */	{ $$ = 0; }
	| MINUS 	{ $$ = 1; }
	;

abs:
	/* empty */ 	{ $$ = 0; }
	| ABS 	{ $$ = 1; }
	;

/* Flag (Conditional) Modifier */
cond_mod:
	condModifiers
	{
		$$.cond_modifier = $1;
		$$.flag_reg_nr = 0;
		$$.flag_subreg_nr = 0;
	}
	| condModifiers DOT flagreg
	{
		$$.cond_modifier = $1;
		$$.flag_reg_nr = $3.nr;
		$$.flag_subreg_nr = $3.subnr;
	}
	;

condModifiers:
	/* empty */ 	{ $$ = ELK_CONDITIONAL_NONE; }
	| ZERO
	| EQUAL
	| NOT_ZERO
	| NOT_EQUAL
	| GREATER
	| GREATER_EQUAL
	| LESS
	| LESS_EQUAL
	| OVERFLOW
	| ROUND_INCREMENT
	| UNORDERED
	;

/* message details for send */
msgdesc:
	MSGDESC_BEGIN msgdesc_parts MSGDESC_END { $$ = $2; }
	;

msgdesc_parts:
	SRC1_LEN ASSIGN INTEGER msgdesc_parts
	{
		$$ = $4;
		$$.src1_len = $3;
	}
	| EX_BSO msgdesc_parts
	{
		$$ = $2;
		$$.ex_bso = 1;
	}
	| INTEGER msgdesc_parts { $$ = $2; }
	| ASSIGN msgdesc_parts { $$ = $2; }
	| /* empty */
	{
		memset(&$$, 0, sizeof($$));
	}
	;

saturate:
	/* empty */ 	{ $$ = ELK_INSTRUCTION_NORMAL; }
	| SATURATE 	{ $$ = ELK_INSTRUCTION_SATURATE; }
	;

/* Execution size */
execsize:
	/* empty */ %prec EMPTYEXECSIZE
	{
		$$ = 0;
	}
	| LPAREN exp2 RPAREN
	{
		if ($2 > 32 || !isPowerofTwo($2))
			error(&@2, "Invalid execution size %llu\n", $2);

		$$ = cvt($2) - 1;
	}
	;

/* Instruction options */
instoptions:
	/* empty */
	{
		memset(&$$, 0, sizeof($$));
	}
	| LCURLY instoption_list RCURLY
	{
		memset(&$$, 0, sizeof($$));
		$$ = $2;
	}
	;

instoption_list:
	instoption_list COMMA instoption
	{
		memset(&$$, 0, sizeof($$));
		$$ = $1;
		add_instruction_option(&$$, $3);
	}
	| instoption_list instoption
	{
		memset(&$$, 0, sizeof($$));
		$$ = $1;
		add_instruction_option(&$$, $2);
	}
	| /* empty */
	{
		memset(&$$, 0, sizeof($$));
	}
	;

instoption:
	ALIGN1 	        { $$.uint_value = ALIGN1;}
	| ALIGN16 	{ $$.uint_value = ALIGN16; }
	| ACCWREN 	{ $$.uint_value = ACCWREN; }
	| SECHALF 	{ $$.uint_value = SECHALF; }
	| COMPR 	{ $$.uint_value = COMPR; }
	| COMPR4 	{ $$.uint_value = COMPR4; }
	| BREAKPOINT 	{ $$.uint_value = BREAKPOINT; }
	| NODDCLR 	{ $$.uint_value = NODDCLR; }
	| NODDCHK 	{ $$.uint_value = NODDCHK; }
	| MASK_DISABLE 	{ $$.uint_value = MASK_DISABLE; }
	| EOT 	        { $$.uint_value = EOT; }
	| SWITCH 	{ $$.uint_value = SWITCH; }
	| ATOMIC 	{ $$.uint_value = ATOMIC; }
	| CMPTCTRL 	{ $$.uint_value = CMPTCTRL; }
	| WECTRL 	{ $$.uint_value = WECTRL; }
	| QTR_2Q 	{ $$.uint_value = QTR_2Q; }
	| QTR_3Q 	{ $$.uint_value = QTR_3Q; }
	| QTR_4Q 	{ $$.uint_value = QTR_4Q; }
	| QTR_2H 	{ $$.uint_value = QTR_2H; }
	| QTR_2N 	{ $$.uint_value = QTR_2N; }
	| QTR_3N 	{ $$.uint_value = QTR_3N; }
	| QTR_4N 	{ $$.uint_value = QTR_4N; }
	| QTR_5N 	{ $$.uint_value = QTR_5N; }
	| QTR_6N 	{ $$.uint_value = QTR_6N; }
	| QTR_7N 	{ $$.uint_value = QTR_7N; }
	| QTR_8N 	{ $$.uint_value = QTR_8N; }
	;

%%

extern int yylineno;

#ifdef YYBYACC
void
yyerror(YYLTYPE *ltype, char *msg)
#else
void
yyerror(char *msg)
#endif
{
	fprintf(stderr, "%s: %d: %s at \"%s\"\n",
	        input_filename, yylineno, msg, lex_text());
	++errors;
}
