/* b16.  */
jirl	  $r4,$r5,%b16(.L1)

/* b21.  */
beqz	  $r4,%b21(.L1)

/* b26.  */
b	  %b26(.L1)

/* lu12i.w.  */
lu12i.w	  $r4,%abs_hi20(.L1)

/* ori   */
ori	  $r4,$r5,%abs_lo12(.L1)

/* lu32i.d.  */
lu32i.d	  $r4,%abs64_lo20(.L1)

/* lu52i.d.  */
lu52i.d	  $r5,$r4,%abs64_hi12(.L1)

pcalau12i $r4,%pc_hi20(.L1)
addi.d	  $r5,$r4,%pc_lo12(.L1)
lu32i.d	  $r4,%pc64_lo20(.L1)
lu52i.d	  $r5,$r4,%pc64_hi12(.L1)

pcalau12i $r4,%got_pc_hi20(.L1)
ld.d	  $r5,$r4,%got_pc_lo12(.L1)
lu32i.d	  $r4,%got64_pc_lo20(.L1)
lu52i.d	  $r5,$r4,%got64_pc_hi12(.L1)

lu12i.w	  $r4,%got_hi20(.L1)
ori	  $r4,$r4,%got_lo12(.L1)
lu32i.d	  $r4,%got64_lo20(.L1)
lu52i.d	  $r5,$r4,%got64_hi12(.L1)

/* TLS LE.  */
lu12i.w	  $r4,%le_hi20(TLSL1)
ori	  $r5,$r4,%le_lo12(TLSL1)
lu32i.d	  $r4,%le64_lo20(TLSL1)
lu52i.d	  $r5,$r4,%le64_hi12(TLSL1)

/* Part of IE relocs.  */
pcalau12i $r4,%ie_pc_hi20(TLSL1)
addi.d	  $r5,$r0,%ie_pc_lo12(TLSL1)
lu32i.d	  $r5,%ie64_pc_lo20(TLSL1)
lu52i.d	  $r5,$r5,%ie64_pc_hi12(TLSL1)

lu12i.w	  $r4,%ie_hi20(TLSL1)
ori	  $r4,$r4,%ie_lo12(TLSL1)
lu32i.d	  $r4,%ie64_lo20(TLSL1)
lu52i.d	  $r4,$r4,%ie64_hi12(TLSL1)

/* Part of LD relocs.  */
pcalau12i $r4,%ld_pc_hi20(TLSL1)
lu12i.w	  $r4,%ld_hi20(TLSL1)

/* Part of GD relocs.  */
pcalau12i $r4,%gd_pc_hi20(TLSL1)
lu12i.w	  $r4,%gd_hi20(TLSL1)

/* Test insn relocs.  */
.L1:
/* 32-bit PC relative.  */
.4byte	  .L2-.L1
.L2:
/* 64-bit PC relative.  */
.8byte	  .L3-.L2

.L3:
nop

/* R_LARCH_ALIGN.  */
.align	4

/* R_LARCH_PCREL20_S2.  */
pcaddi	  $r12,.L1

/* R_LARCH_ADD_CALL36  */
pcaddu18i $r1, %call36(a)
jirl	  $r1, $r1, 0

/* Part of DESC relocs.   */
pcalau12i $r4,%desc_pc_hi20(TLSL1)
addi.d	  $r5,$r5,%desc_pc_lo12(TLSL1)
lu32i.d	  $r5,%desc64_pc_lo20(TLSL1)
lu52i.d	  $r5,$r5,%desc64_pc_hi12(TLSL1)

lu12i.w	  $r4,%desc_hi20(TLSL1)
ori	  $r4,$r4,%desc_lo12(TLSL1)
lu32i.d	  $r4,%desc64_lo20(TLSL1)
lu52i.d	  $r4,$r4,%desc64_hi12(TLSL1)
ld.d	  $r1,$r4,%desc_ld(TLSL1)
jirl	  $r1,$r1,%desc_call(TLSL1)

/* New TLS Insn.  */
lu12i.w	  $r4,%le_hi20_r(TLSL1)
add.d	  $r5,$r5,$r4,%le_add_r(TLSL1)
st.w	  $r5,$r4,%le_lo12_r(TLSL1)

/* New TLS Insn with addend.  */
lu12i.w	  $r4,%le_hi20_r(TLSL1)
add.d	  $r5,$r5,$r4,%le_add_r(TLSL1)
st.w	  $r5,$r4,%le_lo12_r(TLSL1)

/* Part of relaxed LD/GD/DESC insn sequence.   */
pcaddi	  $a0,%ld_pcrel_20(TLSL1)
pcaddi	  $a0,%gd_pcrel_20(TLSL1)
pcaddi	  $a0,%desc_pcrel_20(TLSL1)
