	.include "sysreg-test-utils.inc"

.text
	rw_sys_reg sys_reg=PMSDSFR_EL1 xreg=x3 r=1 w=1

	mrs x0, ERXGSR_EL1

	rw_sys_reg sys_reg=SCTLR2_EL1 xreg=x3 r=1 w=1
	rw_sys_reg sys_reg=SCTLR2_EL12 xreg=x3 r=1 w=1
	rw_sys_reg sys_reg=SCTLR2_EL2 xreg=x3 r=1 w=1
	rw_sys_reg sys_reg=SCTLR2_EL3 xreg=x3 r=1 w=1

	rw_sys_reg sys_reg=HDFGRTR2_EL2 xreg=x3 r=1 w=1
	rw_sys_reg sys_reg=HDFGWTR2_EL2 xreg=x3 r=1 w=1
	rw_sys_reg sys_reg=HFGRTR2_EL2 xreg=x3 r=1 w=1
	rw_sys_reg sys_reg=HFGWTR2_EL2 xreg=x3 r=1 w=1

	rw_sys_reg sys_reg=PFAR_EL1 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=PFAR_EL2 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=PFAR_EL12 xreg=x0 r=1 w=1

	/* AT.  */
	at s1e1a, x1
	at s1e2a, x3
	at s1e3a, x5

	/* FEAT_AIE.  */
	rw_sys_reg sys_reg=amair2_el1 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=amair2_el12 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=amair2_el2 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=amair2_el3 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=mair2_el1 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=mair2_el12 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=mair2_el2 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=mair2_el3 xreg=x0 r=1 w=1

	/* FEAT_S1PIE.  */
	rw_sys_reg sys_reg=pir_el1 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=pir_el12 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=pir_el2 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=pir_el3 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=pire0_el1 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=pire0_el12 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=pire0_el2 xreg=x0 r=1 w=1

	/* FEAT_S2PIE.  */
	rw_sys_reg sys_reg=s2pir_el2 xreg=x0 r=1 w=1

	/* FEAT_S1POE.  */
	rw_sys_reg sys_reg=por_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=por_el1 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=por_el12 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=por_el2 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=por_el3 xreg=x0 r=1 w=1

	/* FEAT_S21POE.  */
	rw_sys_reg sys_reg=s2por_el1 xreg=x0 r=1 w=1

	/* FEAT_TCR2.  */
	rw_sys_reg sys_reg=tcr2_el1 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=tcr2_el12 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=tcr2_el2 xreg=x0 r=1 w=1

	/* FEAT_DEBUGv8p9 Extension.  */
	rw_sys_reg sys_reg=mdselr_el1 xreg=x0 r=1 w=1

	/* FEAT_PMUv3p9 Extension.  */
	rw_sys_reg sys_reg=pmuacr_el1 xreg=x0 r=1 w=1

	/* FEAT_PMUv3_SS Extension.  */
	mrs x0, pmccntsvr_el1
	mrs x0, pmicntsvr_el1
	rw_sys_reg sys_reg=pmsscr_el1 xreg=x0 r=1 w=1
	mrs x0, pmevcntsvr0_el1
	mrs x0, pmevcntsvr10_el1
	mrs x0, pmevcntsvr11_el1
	mrs x0, pmevcntsvr12_el1
	mrs x0, pmevcntsvr13_el1
	mrs x0, pmevcntsvr14_el1
	mrs x0, pmevcntsvr15_el1
	mrs x0, pmevcntsvr16_el1
	mrs x0, pmevcntsvr17_el1
	mrs x0, pmevcntsvr18_el1
	mrs x0, pmevcntsvr19_el1
	mrs x0, pmevcntsvr1_el1
	mrs x0, pmevcntsvr20_el1
	mrs x0, pmevcntsvr21_el1
	mrs x0, pmevcntsvr22_el1
	mrs x0, pmevcntsvr23_el1
	mrs x0, pmevcntsvr24_el1
	mrs x0, pmevcntsvr25_el1
	mrs x0, pmevcntsvr26_el1
	mrs x0, pmevcntsvr27_el1
	mrs x0, pmevcntsvr28_el1
	mrs x0, pmevcntsvr29_el1
	mrs x0, pmevcntsvr30_el1
	mrs x0, pmevcntsvr3_el1
	mrs x0, pmevcntsvr4_el1
	mrs x0, pmevcntsvr5_el1
	mrs x0, pmevcntsvr6_el1
	mrs x0, pmevcntsvr7_el1
	mrs x0, pmevcntsvr8_el1
	mrs x0, pmevcntsvr9_el1

	/* FEAT_PMUv3_ICNTR Extension.  */
	rw_sys_reg sys_reg=pmicntr_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=pmicfiltr_el0 xreg=x0 r=1 w=1
	msr pmzr_el0, x0

	/* FEAT_SPMU */
	rw_sys_reg sys_reg=spmaccessr_el1 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmaccessr_el2 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmaccessr_el3 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmcfgr_el1 xreg=x0 r=1 w=0
	rw_sys_reg sys_reg=spmcgcr0_el1 xreg=x0 r=1 w=0
	rw_sys_reg sys_reg=spmcgcr1_el1 xreg=x0 r=1 w=0
	rw_sys_reg sys_reg=spmcntenclr_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmcntenset_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmcr_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmdevaff_el1 xreg=x0 r=1 w=0
	rw_sys_reg sys_reg=spmdevarch_el1 xreg=x0 r=1 w=0
	rw_sys_reg sys_reg=spmevcntr0_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevcntr1_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevcntr2_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevcntr3_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevcntr4_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevcntr5_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevcntr6_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevcntr7_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevcntr8_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevcntr9_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevcntr10_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevcntr11_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevcntr12_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevcntr13_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevcntr14_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevcntr15_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r0_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r1_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r2_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r3_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r4_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r5_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r6_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r7_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r8_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r9_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r10_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r11_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r12_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r13_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r14_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfilt2r15_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr0_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr1_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr2_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr3_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr4_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr5_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr6_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr7_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr8_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr9_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr10_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr11_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr12_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr13_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr14_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevfiltr15_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper0_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper1_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper2_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper3_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper4_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper5_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper6_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper7_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper8_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper9_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper10_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper11_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper12_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper13_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper14_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmevtyper15_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmiidr_el1 xreg=x0 r=1 w=0
	rw_sys_reg sys_reg=spmintenclr_el1 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmintenset_el1 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmovsclr_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmovsset_el0 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmrootcr_el3 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmscr_el1 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=spmselr_el0 xreg=x0 r=1 w=1

	/* FEAT_SEBEP Extension.  */
	rw_sys_reg sys_reg=pmecr_el1 xreg=x0 r=1 w=1
	rw_sys_reg sys_reg=pmiar_el1 xreg=x0 r=1 w=1
