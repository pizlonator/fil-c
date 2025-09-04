	.include "sysreg-test-utils.inc"

	.text

	rw_sys_reg	id_dfr1_el1 w=0
	rw_sys_reg	id_mmfr5_el1 w=0
	rw_sys_reg	id_isar6_el1 w=0

	rw_sys_reg	icc_pmr_el1
	rw_sys_reg	icc_iar0_el1 w=0
	rw_sys_reg	icc_eoir0_el1 r=0
	rw_sys_reg	icc_hppir0_el1 w=0
	rw_sys_reg	icc_bpr0_el1
	rw_sys_reg	icc_ap0r0_el1
	rw_sys_reg	icc_ap0r1_el1
	rw_sys_reg	icc_ap0r2_el1
	rw_sys_reg	icc_ap0r3_el1
	rw_sys_reg	icc_ap1r0_el1
	rw_sys_reg	icc_ap1r1_el1
	rw_sys_reg	icc_ap1r2_el1
	rw_sys_reg	icc_ap1r3_el1
	rw_sys_reg	icc_dir_el1 r=0
	rw_sys_reg	icc_rpr_el1 w=0
	rw_sys_reg	icc_sgi1r_el1 r=0
	rw_sys_reg	icc_asgi1r_el1 r=0
	rw_sys_reg	icc_sgi0r_el1 r=0
	rw_sys_reg	icc_iar1_el1 w=0
	rw_sys_reg	icc_eoir1_el1 r=0
	rw_sys_reg	icc_hppir1_el1 w=0
	rw_sys_reg	icc_bpr1_el1
	rw_sys_reg	icc_ctlr_el1
	rw_sys_reg	icc_igrpen0_el1
	rw_sys_reg	icc_igrpen1_el1
	rw_sys_reg	ich_ap0r0_el2
	rw_sys_reg	ich_ap0r1_el2
	rw_sys_reg	ich_ap0r2_el2
	rw_sys_reg	ich_ap0r3_el2
	rw_sys_reg	ich_ap1r0_el2
	rw_sys_reg	ich_ap1r1_el2
	rw_sys_reg	ich_ap1r2_el2
	rw_sys_reg	ich_ap1r3_el2
	rw_sys_reg	ich_hcr_el2
	rw_sys_reg	ich_misr_el2 w=0
	rw_sys_reg	ich_eisr_el2 w=0
	rw_sys_reg	ich_elrsr_el2 w=0
	rw_sys_reg	ich_vmcr_el2
	rw_sys_reg	ich_lr0_el2
	rw_sys_reg	ich_lr1_el2
	rw_sys_reg	ich_lr2_el2
	rw_sys_reg	ich_lr3_el2
	rw_sys_reg	ich_lr4_el2
	rw_sys_reg	ich_lr5_el2
	rw_sys_reg	ich_lr6_el2
	rw_sys_reg	ich_lr7_el2
	rw_sys_reg	ich_lr8_el2
	rw_sys_reg	ich_lr9_el2
	rw_sys_reg	ich_lr10_el2
	rw_sys_reg	ich_lr11_el2
	rw_sys_reg	ich_lr12_el2
	rw_sys_reg	ich_lr13_el2
	rw_sys_reg	ich_lr14_el2
	rw_sys_reg	ich_lr15_el2
	rw_sys_reg	icc_igrpen1_el3

	.arch	armv8.1-a

	rw_sys_reg	lorid_el1 w=0

	.arch	armv8.3-a

	rw_sys_reg	ccsidr2_el1 w=0

	.arch	armv8.4-a

	rw_sys_reg	trfcr_el1
	rw_sys_reg	pmmir_el1 w=0
	rw_sys_reg	trfcr_el2

	rw_sys_reg	trfcr_el12

	rw_sys_reg	amcr_el0
	rw_sys_reg	amcfgr_el0 w=0
	rw_sys_reg	amcgcr_el0 w=0
	rw_sys_reg	amuserenr_el0
	rw_sys_reg	amcntenclr0_el0
	rw_sys_reg	amcntenset0_el0
	rw_sys_reg	amcntenclr1_el0
	rw_sys_reg	amcntenset1_el0
	rw_sys_reg	amevcntr00_el0
	rw_sys_reg	amevcntr01_el0
	rw_sys_reg	amevcntr02_el0
	rw_sys_reg	amevcntr03_el0
	rw_sys_reg	amevtyper00_el0 w=0
	rw_sys_reg	amevtyper01_el0 w=0
	rw_sys_reg	amevtyper02_el0 w=0
	rw_sys_reg	amevtyper03_el0 w=0
	rw_sys_reg	amevcntr10_el0
	rw_sys_reg	amevcntr11_el0
	rw_sys_reg	amevcntr12_el0
	rw_sys_reg	amevcntr13_el0
	rw_sys_reg	amevcntr14_el0
	rw_sys_reg	amevcntr15_el0
	rw_sys_reg	amevcntr16_el0
	rw_sys_reg	amevcntr17_el0
	rw_sys_reg	amevcntr18_el0
	rw_sys_reg	amevcntr19_el0
	rw_sys_reg	amevcntr110_el0
	rw_sys_reg	amevcntr111_el0
	rw_sys_reg	amevcntr112_el0
	rw_sys_reg	amevcntr113_el0
	rw_sys_reg	amevcntr114_el0
	rw_sys_reg	amevcntr115_el0
	rw_sys_reg	amevtyper10_el0
	rw_sys_reg	amevtyper11_el0
	rw_sys_reg	amevtyper12_el0
	rw_sys_reg	amevtyper13_el0
	rw_sys_reg	amevtyper14_el0
	rw_sys_reg	amevtyper15_el0
	rw_sys_reg	amevtyper16_el0
	rw_sys_reg	amevtyper17_el0
	rw_sys_reg	amevtyper18_el0
	rw_sys_reg	amevtyper19_el0
	rw_sys_reg	amevtyper110_el0
	rw_sys_reg	amevtyper111_el0
	rw_sys_reg	amevtyper112_el0
	rw_sys_reg	amevtyper113_el0
	rw_sys_reg	amevtyper114_el0
	rw_sys_reg	amevtyper115_el0

	.arch	armv8.6-a

	rw_sys_reg	amcg1idr_el0 w=0
	rw_sys_reg	cntpctss_el0 w=0
	rw_sys_reg	cntvctss_el0 w=0
	rw_sys_reg	hfgrtr_el2
	rw_sys_reg	hfgwtr_el2
	rw_sys_reg	hfgitr_el2
	rw_sys_reg	hdfgrtr_el2
	rw_sys_reg	hdfgwtr_el2
	rw_sys_reg	hafgrtr_el2
	rw_sys_reg	amevcntvoff00_el2
	rw_sys_reg	amevcntvoff01_el2
	rw_sys_reg	amevcntvoff02_el2
	rw_sys_reg	amevcntvoff03_el2
	rw_sys_reg	amevcntvoff04_el2
	rw_sys_reg	amevcntvoff05_el2
	rw_sys_reg	amevcntvoff06_el2
	rw_sys_reg	amevcntvoff07_el2
	rw_sys_reg	amevcntvoff08_el2
	rw_sys_reg	amevcntvoff09_el2
	rw_sys_reg	amevcntvoff010_el2
	rw_sys_reg	amevcntvoff011_el2
	rw_sys_reg	amevcntvoff012_el2
	rw_sys_reg	amevcntvoff013_el2
	rw_sys_reg	amevcntvoff014_el2
	rw_sys_reg	amevcntvoff015_el2
	rw_sys_reg	amevcntvoff10_el2
	rw_sys_reg	amevcntvoff11_el2
	rw_sys_reg	amevcntvoff12_el2
	rw_sys_reg	amevcntvoff13_el2
	rw_sys_reg	amevcntvoff14_el2
	rw_sys_reg	amevcntvoff15_el2
	rw_sys_reg	amevcntvoff16_el2
	rw_sys_reg	amevcntvoff17_el2
	rw_sys_reg	amevcntvoff18_el2
	rw_sys_reg	amevcntvoff19_el2
	rw_sys_reg	amevcntvoff110_el2
	rw_sys_reg	amevcntvoff111_el2
	rw_sys_reg	amevcntvoff112_el2
	rw_sys_reg	amevcntvoff113_el2
	rw_sys_reg	amevcntvoff114_el2
	rw_sys_reg	amevcntvoff115_el2
	rw_sys_reg	cntpoff_el2

	.arch	armv8.7-a

	rw_sys_reg	pmsnevfr_el1
	rw_sys_reg	hcrx_el2

	.arch	armv8-a+the

	rw_sys_reg	rcwmask_el1
	rw_sys_reg	rcwsmask_el1
