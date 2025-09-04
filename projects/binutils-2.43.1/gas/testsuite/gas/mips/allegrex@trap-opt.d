#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS divide and multiply macros with --trap
#as: -32 --trap
#warning_output: brtr-opt.l
#source: brtr-opt.s
#dump: mips1@trap-opt.d
