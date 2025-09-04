#as: -march=rv64i -mrelax
#source: fixup-local.s
#objdump: -dr -Mno-aliases

.*:[ 	]+file format .*


Disassembly of section .text:

0+0000 <foo>:
[ 	]+0:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+0:[ 	]+R_RISCV_PCREL_HI20[ 	]+.LL0.*
[ 	]+0:[ 	]+R_RISCV_RELAX.*
[ 	]+4:[ 	]+01850513[ 	]+addi[ 	]+a0,a0,24 # 18 <.LL0>
[ 	]+4:[ 	]+R_RISCV_PCREL_LO12_I[ 	]+.L0.*
[ 	]+4:[ 	]+R_RISCV_RELAX.*
[ 	]+8:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+8:[ 	]+R_RISCV_PCREL_HI20[ 	]+.LL0.*
[ 	]+8:[ 	]+R_RISCV_RELAX.*
[ 	]+c:[ 	]+01051503[ 	]+lh[ 	]+a0,16\(a0\) # 18 <.LL0>
[ 	]+c:[ 	]+R_RISCV_PCREL_LO12_I[ 	]+.L0.*
[ 	]+c:[ 	]+R_RISCV_RELAX.*
[ 	]+10:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+10:[ 	]+R_RISCV_PCREL_HI20[ 	]+.LL0.*
[ 	]+10:[ 	]+R_RISCV_RELAX.*
[ 	]+14:[ 	]+00a51423[ 	]+sh[ 	]+a0,8\(a0\) # 18 <.LL0>
[ 	]+14:[ 	]+R_RISCV_PCREL_LO12_S[ 	]+.L0.*
[ 	]+14:[ 	]+R_RISCV_RELAX.*

0+0018 <.LL0>:
[ 	]+18:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+18:[ 	]+R_RISCV_PCREL_HI20[ 	]+bar.*
[ 	]+18:[ 	]+R_RISCV_RELAX.*
[ 	]+1c:[ 	]+00050513[ 	]+addi[ 	]+a0,a0,0 # 18 <.LL0>
[ 	]+1c:[ 	]+R_RISCV_PCREL_LO12_I[ 	]+.L0.*
[ 	]+1c:[ 	]+R_RISCV_RELAX.*
[ 	]+20:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+20:[ 	]+R_RISCV_PCREL_HI20[ 	]+bar.*
[ 	]+20:[ 	]+R_RISCV_RELAX.*
[ 	]+24:[ 	]+00051503[ 	]+lh[ 	]+a0,0\(a0\) # 20 <.LL0\+0x8>
[ 	]+24:[ 	]+R_RISCV_PCREL_LO12_I[ 	]+.L0.*
[ 	]+24:[ 	]+R_RISCV_RELAX.*
[ 	]+28:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+28:[ 	]+R_RISCV_PCREL_HI20[ 	]+bar.*
[ 	]+28:[ 	]+R_RISCV_RELAX.*
[ 	]+2c:[ 	]+00a51023[ 	]+sh[ 	]+a0,0\(a0\) # 28 <.LL0\+0x10>
[ 	]+2c:[ 	]+R_RISCV_PCREL_LO12_S[ 	]+.L0.*
[ 	]+2c:[ 	]+R_RISCV_RELAX.*
[ 	]+30:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+30:[ 	]+R_RISCV_PCREL_HI20[ 	]+foo.*
[ 	]+30:[ 	]+R_RISCV_RELAX.*
[ 	]+34:[ 	]+00050513[ 	]+addi[ 	]+a0,a0,0 # 30 <.LL0\+0x18>
[ 	]+34:[ 	]+R_RISCV_PCREL_LO12_I[ 	]+.L0.*
[ 	]+34:[ 	]+R_RISCV_RELAX.*
[ 	]+38:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+38:[ 	]+R_RISCV_PCREL_HI20[ 	]+foo.*
[ 	]+38:[ 	]+R_RISCV_RELAX.*
[ 	]+3c:[ 	]+00051503[ 	]+lh[ 	]+a0,0\(a0\) # 38 <.LL0\+0x20>
[ 	]+3c:[ 	]+R_RISCV_PCREL_LO12_I[ 	]+.L0.*
[ 	]+3c:[ 	]+R_RISCV_RELAX.*
[ 	]+40:[ 	]+00000517[ 	]+auipc[ 	]+a0,0x0
[ 	]+40:[ 	]+R_RISCV_PCREL_HI20[ 	]+foo.*
[ 	]+40:[ 	]+R_RISCV_RELAX.*
[ 	]+44:[ 	]+00a51023[ 	]+sh[ 	]+a0,0\(a0\) # 40 <.LL0\+0x28>
[ 	]+44:[ 	]+R_RISCV_PCREL_LO12_S[ 	]+.L0.*
[ 	]+44:[ 	]+R_RISCV_RELAX.*

0+0048 <.LL1>:
[ 	]+48:[ 	]+00000517[ 	]+auipc[ 	]a0,0x0
[ 	]+48:[ 	]+R_RISCV_PCREL_HI20[ 	]+.LL3.*
[ 	]+48:[ 	]+R_RISCV_RELAX.*
[ 	]+4c:[ 	]+01052503[ 	]+lw[ 	]+a0,16\(a0\) # 58 <.LL3>
[ 	]+4c:[ 	]+R_RISCV_PCREL_LO12_I[ 	]+.LL1.*
[ 	]+4c:[ 	]+R_RISCV_RELAX.*
[ 	]+50:[ 	]+00000517[ 	]+auipc[ 	]a0,0x0
[ 	]+50:[ 	]+R_RISCV_PCREL_HI20[ 	]+.LL3.*
[ 	]+50:[ 	]+R_RISCV_RELAX.*
[ 	]+54:[ 	]+00a52823[ 	]+sw[ 	]+a0,16\(a0\) # 60 <.LL3\+0x8>
[ 	]+54:[ 	]+R_RISCV_PCREL_LO12_S[ 	]+.LL1.*
[ 	]+54:[ 	]+R_RISCV_RELAX.*

0+0058 <.LL3>:
[ 	]+58:[ 	]+00008067[ 	]+jalr[ 	]+zero,0\(ra\)
