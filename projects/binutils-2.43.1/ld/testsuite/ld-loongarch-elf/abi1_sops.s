.macro sign_check reloc_sym reloc_name reloc_bits
  .ifdef \reloc_sym
    .ifdef overflow
      .reloc 0, R_LARCH_SOP_PUSH_ABSOLUTE, 1 << (\reloc_bits - 1)
      .reloc 0, \reloc_name
    .else
      .reloc 0, R_LARCH_SOP_PUSH_ABSOLUTE, -(1 << (\reloc_bits - 1)) - 1
      .reloc 0, \reloc_name
    .endif
    .word 0
  .endif
.endm

.macro sign_check_s2 reloc_sym reloc_name reloc_bits
  .ifdef \reloc_sym
    .ifdef overflow
      .reloc 0, R_LARCH_SOP_PUSH_ABSOLUTE, (1 << (\reloc_bits - 1)) << 2
      .reloc 0, \reloc_name
    .else
      .reloc 0, R_LARCH_SOP_PUSH_ABSOLUTE, (-(1 << (\reloc_bits - 1)) - 1) << 2
      .reloc 0, \reloc_name
    .endif
    .word 0
  .endif
.endm

.macro unsign_check reloc_sym reloc_name reloc_bits
  .ifdef \reloc_sym
    .reloc 0, R_LARCH_SOP_PUSH_ABSOLUTE, 1 << \reloc_bits
    .reloc 0, \reloc_name
    .word 0
  .endif
.endm

sign_check	s_10_5		R_LARCH_SOP_POP_32_S_10_5 5
sign_check	s_10_12		R_LARCH_SOP_POP_32_S_10_12 12
sign_check	s_10_16		R_LARCH_SOP_POP_32_S_10_16 16
sign_check_s2	s_10_16_s2	R_LARCH_SOP_POP_32_S_10_16_S2 16
sign_check_s2	s_0_5_10_16_s2	R_LARCH_SOP_POP_32_S_0_5_10_16_S2 21
sign_check_s2	s_0_10_10_16_s2	R_LARCH_SOP_POP_32_S_0_10_10_16_S2 26
sign_check	s_5_20		R_LARCH_SOP_POP_32_S_5_20 20
unsign_check	u_10_12		R_LARCH_SOP_POP_32_U_10_12 12
unsign_check	u		R_LARCH_SOP_POP_32_U 32
