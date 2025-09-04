#as:
#objdump: -dr
#skip: loongarch32-*-*

.*:[    ]+file format .*


Disassembly of section .text:

[ 	]*0000000000000000 <.L1-0x8c>:
[ 	]+0:[ 	]+4c008ca4[ 	]+jirl[ 	]+\$a0, \$a1, 140
[ 	]+0: R_LARCH_B16[ 	]+.L1
[ 	]+4:[ 	]+40008880[ 	]+beqz[ 	]+\$a0, 136[ 	]+# 8c <.L1>
[ 	]+4: R_LARCH_B21[ 	]+.L1
[ 	]+8:[ 	]+50008400[ 	]+b[ 	]+132[ 	]+# 8c <.L1>
[ 	]+8: R_LARCH_B26[ 	]+.L1
[ 	]+c:[ 	]+14000004[ 	]+lu12i.w[ 	]+\$a0, 0
[ 	]+c: R_LARCH_ABS_HI20[ 	]+.L1
[ 	]+10:[ 	]+038000a4[ 	]+ori[ 	]+\$a0, \$a1, 0x0
[ 	]+10: R_LARCH_ABS_LO12[ 	]+.L1
[ 	]+14:[ 	]+16000004[ 	]+lu32i.d[ 	]+\$a0, 0
[ 	]+14: R_LARCH_ABS64_LO20[ 	]+.L1
[ 	]+18:[ 	]+03000085[ 	]+lu52i.d[ 	]+\$a1, \$a0, 0
[ 	]+18: R_LARCH_ABS64_HI12[ 	]+.L1
[ 	]+1c:[ 	]+1a000004[ 	]+pcalau12i[ 	]+\$a0, 0
[ 	]+1c: R_LARCH_PCALA_HI20[ 	]+.L1
[ 	]+20:[ 	]+02c00085[ 	]+addi.d[ 	]+\$a1, \$a0, 0
[ 	]+20: R_LARCH_PCALA_LO12[ 	]+.L1
[ 	]+24:[ 	]+16000004[ 	]+lu32i.d[ 	]+\$a0, 0
[ 	]+24: R_LARCH_PCALA64_LO20[ 	]+.L1
[ 	]+28:[ 	]+03000085[ 	]+lu52i.d[ 	]+\$a1, \$a0, 0
[ 	]+28: R_LARCH_PCALA64_HI12[ 	]+.L1
[ 	]+2c:[ 	]+1a000004[ 	]+pcalau12i[ 	]+\$a0, 0
[ 	]+2c: R_LARCH_GOT_PC_HI20[ 	]+.L1
[ 	]+30:[ 	]+28c00085[ 	]+ld.d[ 	]+\$a1, \$a0, 0
[ 	]+30: R_LARCH_GOT_PC_LO12[ 	]+.L1
[ 	]+34:[ 	]+16000004[ 	]+lu32i.d[ 	]+\$a0, 0
[ 	]+34: R_LARCH_GOT64_PC_LO20[ 	]+.L1
[ 	]+38:[ 	]+03000085[ 	]+lu52i.d[ 	]+\$a1, \$a0, 0
[ 	]+38: R_LARCH_GOT64_PC_HI12[ 	]+.L1
[ 	]+3c:[ 	]+14000004[ 	]+lu12i.w[ 	]+\$a0, 0
[ 	]+3c: R_LARCH_GOT_HI20[ 	]+.L1
[ 	]+40:[ 	]+03800084[ 	]+ori[ 	]+\$a0, \$a0, 0x0
[ 	]+40: R_LARCH_GOT_LO12[ 	]+.L1
[ 	]+44:[ 	]+16000004[ 	]+lu32i.d[ 	]+\$a0, 0
[ 	]+44: R_LARCH_GOT64_LO20[ 	]+.L1
[ 	]+48:[ 	]+03000085[ 	]+lu52i.d[ 	]+\$a1, \$a0, 0
[ 	]+48: R_LARCH_GOT64_HI12[ 	]+.L1
[ 	]+4c:[ 	]+14000004[ 	]+lu12i.w[ 	]+\$a0, 0
[ 	]+4c: R_LARCH_TLS_LE_HI20[ 	]+TLSL1
[ 	]+4c: R_LARCH_RELAX[ 	]+\*ABS\*
[ 	]+50:[ 	]+03800085[ 	]+ori[ 	]+\$a1, \$a0, 0x0
[ 	]+50: R_LARCH_TLS_LE_LO12[ 	]+TLSL1
[ 	]+50: R_LARCH_RELAX[ 	]+\*ABS\*
[ 	]+54:[ 	]+16000004[ 	]+lu32i.d[ 	]+\$a0, 0
[ 	]+54: R_LARCH_TLS_LE64_LO20[ 	]+TLSL1
[ 	]+54: R_LARCH_RELAX[ 	]+\*ABS\*
[ 	]+58:[ 	]+03000085[ 	]+lu52i.d[ 	]+\$a1, \$a0, 0
[ 	]+58: R_LARCH_TLS_LE64_HI12[ 	]+TLSL1
[ 	]+58: R_LARCH_RELAX[ 	]+\*ABS\*
[ 	]+5c:[ 	]+1a000004[ 	]+pcalau12i[ 	]+\$a0, 0
[ 	]+5c: R_LARCH_TLS_IE_PC_HI20[ 	]+TLSL1
[ 	]+60:[ 	]+02c00005[ 	]+li.d[ 	]+\$a1, 0
[ 	]+60: R_LARCH_TLS_IE_PC_LO12[ 	]+TLSL1
[ 	]+64:[ 	]+16000005[ 	]+lu32i.d[ 	]+\$a1, 0
[ 	]+64: R_LARCH_TLS_IE64_PC_LO20[ 	]+TLSL1
[ 	]+68:[ 	]+030000a5[ 	]+lu52i.d[ 	]+\$a1, \$a1, 0
[ 	]+68: R_LARCH_TLS_IE64_PC_HI12[ 	]+TLSL1
[ 	]+6c:[ 	]+14000004[ 	]+lu12i.w[ 	]+\$a0, 0
[ 	]+6c: R_LARCH_TLS_IE_HI20[ 	]+TLSL1
[ 	]+70:[ 	]+03800084[ 	]+ori[ 	]+\$a0, \$a0, 0x0
[ 	]+70: R_LARCH_TLS_IE_LO12[ 	]+TLSL1
[ 	]+74:[ 	]+16000004[ 	]+lu32i.d[ 	]+\$a0, 0
[ 	]+74: R_LARCH_TLS_IE64_LO20[ 	]+TLSL1
[ 	]+78:[ 	]+03000084[ 	]+lu52i.d[ 	]+\$a0, \$a0, 0
[ 	]+78: R_LARCH_TLS_IE64_HI12[ 	]+TLSL1
[ 	]+7c:[ 	]+1a000004[ 	]+pcalau12i[ 	]+\$a0, 0
[ 	]+7c: R_LARCH_TLS_LD_PC_HI20[ 	]+TLSL1
[ 	]+80:[ 	]+14000004[ 	]+lu12i.w[ 	]+\$a0, 0
[ 	]+80: R_LARCH_TLS_LD_HI20[ 	]+TLSL1
[ 	]+84:[ 	]+1a000004[ 	]+pcalau12i[ 	]+\$a0, 0
[ 	]+84: R_LARCH_TLS_GD_PC_HI20[ 	]+TLSL1
[ 	]+88:[ 	]+14000004[ 	]+lu12i.w[ 	]+\$a0, 0
[ 	]+88: R_LARCH_TLS_GD_HI20[ 	]+TLSL1
000000000000008c <.L1>:
[ 	]+8c:[ 	]+00000000[ 	]+.word[ 	]+[ 	]+0x00000000
[ 	]+8c: R_LARCH_ADD32[ 	]+.L2
[ 	]+8c: R_LARCH_SUB32[ 	]+.L1
0000000000000090 <.L2>:
[ 	]+...
[ 	]+90: R_LARCH_ADD64[ 	]+.L3
[ 	]+90: R_LARCH_SUB64[ 	]+.L2
0000000000000098 <.L3>:
[ 	]+98:[ 	]+03400000[ 	]+nop
[ 	]+9c:[ 	]+03400000[ 	]+nop
[ 	]+9c: R_LARCH_ALIGN[ 	]+\*ABS\*\+0xc
[ 	]+a0:[ 	]+03400000[ 	]+nop
[ 	]+a4:[ 	]+03400000[ 	]+nop
[ 	]+a8:[ 	]+1800000c[ 	]+pcaddi[ 	]+\$t0, 0
[ 	]+a8: R_LARCH_PCREL20_S2[ 	]+.L1
[ 	]+ac:[ 	]+1e000001[ 	]+pcaddu18i[ 	]+\$ra, 0
[ 	]+ac: R_LARCH_CALL36[ 	]+a
[ 	]+ac: R_LARCH_RELAX[ 	]+\*ABS\*
[ 	]+b0:[ 	]+4c000021[ 	]+jirl[ 	]+\$ra, \$ra, 0
[ 	]+b4:[ 	]+1a000004[ 	]+pcalau12i[ 	]+\$a0, 0
[ 	]+b4: R_LARCH_TLS_DESC_PC_HI20[ 	]+TLSL1
[ 	]+b8:[ 	]+02c000a5[ 	]+addi.d[ 	]+\$a1, \$a1, 0
[ 	]+b8: R_LARCH_TLS_DESC_PC_LO12[ 	]+TLSL1
[ 	]+bc:[ 	]+16000005[ 	]+lu32i.d[ 	]+\$a1, 0
[ 	]+bc: R_LARCH_TLS_DESC64_PC_LO20[ 	]+TLSL1
[ 	]+c0:[ 	]+030000a5[ 	]+lu52i.d[ 	]+\$a1, \$a1, 0
[ 	]+c0: R_LARCH_TLS_DESC64_PC_HI12[ 	]+TLSL1
[ 	]+c4:[ 	]+14000004[ 	]+lu12i.w[ 	]+\$a0, 0
[ 	]+c4: R_LARCH_TLS_DESC_HI20[ 	]+TLSL1
[ 	]+c8:[ 	]+03800084[ 	]+ori[ 	]+\$a0, \$a0, 0x0
[ 	]+c8: R_LARCH_TLS_DESC_LO12[ 	]+TLSL1
[ 	]+cc:[ 	]+16000004[ 	]+lu32i.d[ 	]+\$a0, 0
[ 	]+cc: R_LARCH_TLS_DESC64_LO20[ 	]+TLSL1
[ 	]+d0:[ 	]+03000084[ 	]+lu52i.d[ 	]+\$a0, \$a0, 0
[ 	]+d0: R_LARCH_TLS_DESC64_HI12[ 	]+TLSL1
[ 	]+d4:[ 	]+28c00081[ 	]+ld.d[ 	]+\$ra, \$a0, 0
[ 	]+d4: R_LARCH_TLS_DESC_LD[ 	]+TLSL1
[ 	]+d8:[ 	]+4c000021[ 	]+jirl[ 	]+\$ra, \$ra, 0
[ 	]+d8: R_LARCH_TLS_DESC_CALL[ 	]+TLSL1
[ 	]+dc:[ 	]+14000004[ 	]+lu12i.w[ 	]+\$a0, 0
[ 	]+dc: R_LARCH_TLS_LE_HI20_R[ 	]+TLSL1
[ 	]+dc: R_LARCH_RELAX[ 	]+\*ABS\*
[ 	]+e0:[ 	]+001090a5[ 	]+add.d[ 	]+\$a1, \$a1, \$a0
[ 	]+e0: R_LARCH_TLS_LE_ADD_R[ 	]+TLSL1
[ 	]+e0: R_LARCH_RELAX[ 	]+\*ABS\*
[ 	]+e4:[ 	]+29800085[ 	]+st.w[ 	]+\$a1, \$a0, 0
[ 	]+e4: R_LARCH_TLS_LE_LO12_R[ 	]+TLSL1
[ 	]+e4: R_LARCH_RELAX[ 	]+\*ABS\*
[ 	]+e8:[ 	]+14000004[ 	]+lu12i.w[ 	]+\$a0, 0
[ 	]+e8: R_LARCH_TLS_LE_HI20_R[ 	]+TLSL1
[ 	]+e8: R_LARCH_RELAX[ 	]+\*ABS\*
[ 	]+ec:[ 	]+001090a5[ 	]+add.d[ 	]+\$a1, \$a1, \$a0
[ 	]+ec: R_LARCH_TLS_LE_ADD_R[ 	]+TLSL1
[ 	]+ec: R_LARCH_RELAX[ 	]+\*ABS\*
[ 	]+f0:[ 	]+29800085[ 	]+st.w[ 	]+\$a1, \$a0, 0
[ 	]+f0: R_LARCH_TLS_LE_LO12_R[ 	]+TLSL1
[ 	]+f0: R_LARCH_RELAX[ 	]+\*ABS\*
[ 	]+f4:[ 	]+18000004[ 	]+pcaddi[ 	]+\$a0, 0
[ 	]+f4: R_LARCH_TLS_LD_PCREL20_S2[ 	]+TLSL1
[ 	]+f8:[ 	]+18000004[ 	]+pcaddi[ 	]+\$a0, 0
[ 	]+f8: R_LARCH_TLS_GD_PCREL20_S2[ 	]+TLSL1
[ 	]+fc:[ 	]+18000004[ 	]+pcaddi[ 	]+\$a0, 0
[ 	]+fc: R_LARCH_TLS_DESC_PCREL20_S2[ 	]+TLSL1
