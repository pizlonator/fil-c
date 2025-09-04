#as: -march=rv64i -mno-relax
#source: fixup-local.s
#objdump: -dr -Mno-aliases

.*:[ 	]+file format .*


Disassembly of section .text:

0+0000 <foo>:
[ 	]+0:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+4:[ 	]+01850513[ 	]+addi[ 	]+a0,a0,24 # 18 <foo\+0x18>
[ 	]+8:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+c:[ 	]+01051503[ 	]+lh[ 	]+a0,16\(a0\) # 18 <foo\+0x18>
[ 	]+10:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+14:[ 	]+00a51423[ 	]+sh[ 	]+a0,8\(a0\) # 18 <foo\+0x18>
[ 	]+18:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+18:[ 	]+R_RISCV_PCREL_HI20[ 	]+bar.*
[ 	]+1c:[ 	]+00050513[ 	]+addi[ 	]+a0,a0,0 # 18 <foo\+0x18>
[ 	]+1c:[ 	]+R_RISCV_PCREL_LO12_I[ 	]+.L0.*
[ 	]+20:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+20:[ 	]+R_RISCV_PCREL_HI20[ 	]+bar.*
[ 	]+24:[ 	]+00051503[ 	]+lh[ 	]+a0,0\(a0\) # 20 <foo\+0x20>
[ 	]+24:[ 	]+R_RISCV_PCREL_LO12_I[ 	]+.L0.*
[ 	]+28:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+28:[ 	]+R_RISCV_PCREL_HI20[ 	]+bar.*
[ 	]+2c:[ 	]+00a51023[ 	]+sh[ 	]+a0,0\(a0\) # 28 <foo\+0x28>
[ 	]+2c:[ 	]+R_RISCV_PCREL_LO12_S[ 	]+.L0.*
[ 	]+30:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+30:[ 	]+R_RISCV_PCREL_HI20[ 	]+foo.*
[ 	]+34:[ 	]+00050513[ 	]+addi[ 	]+a0,a0,0 # 30 <foo\+0x30>
[ 	]+34:[ 	]+R_RISCV_PCREL_LO12_I[ 	]+.L0.*
[ 	]+38:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+38:[ 	]+R_RISCV_PCREL_HI20[ 	]+foo.*
[ 	]+3c:[ 	]+00051503[ 	]+lh[ 	]+a0,0\(a0\) # 38 <foo\+0x38>
[ 	]+3c:[ 	]+R_RISCV_PCREL_LO12_I[ 	]+.L0.*
[ 	]+40:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+40:[ 	]+R_RISCV_PCREL_HI20[ 	]+foo.*
[ 	]+44:[ 	]+00a51023[ 	]+sh[ 	]+a0,0\(a0\) # 40 <foo\+0x40>
[ 	]+44:[ 	]+R_RISCV_PCREL_LO12_S[ 	]+.L0.*
[ 	]+48:[ 	]+00000517[ 	]+auipc[ 	]a0,0x0
[ 	]+4c:[ 	]+01052503[ 	]+lw[ 	]+a0,16\(a0\) # 58 <foo\+0x58>
[ 	]+50:[ 	]+00000517[ 	]+auipc[ 	]a0,0x0
[ 	]+54:[ 	]+00a52823[ 	]+sw[ 	]+a0,16\(a0\) # 60 <foo\+0x60>
[ 	]+58:[ 	]+00008067[ 	]+jalr[ 	]+zero,0\(ra\)
