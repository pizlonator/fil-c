#objdump: -dr --prefix-addresses --show-raw-insn
#skip: *-*-pe *-*-wince *-*-vxworks
#name: Group relocation tests (ldc)

.*: +file format .*arm.*

Disassembly of section .text:
0[0-9a-f]+ <[^>]+> ed900085 	ldc	0, cr0, \[r0, #532\].*
			0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed900085 	ldc	0, cr0, \[r0, #532\].*
			4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed900085 	ldc	0, cr0, \[r0, #532\].*
			8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed900085 	ldc	0, cr0, \[r0, #532\].*
			c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed900085 	ldc	0, cr0, \[r0, #532\].*
			10: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed900085 	ldc	0, cr0, \[r0, #532\].*
			14: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed800085 	stc	0, cr0, \[r0, #532\].*
			18: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed800085 	stc	0, cr0, \[r0, #532\].*
			1c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed800085 	stc	0, cr0, \[r0, #532\].*
			20: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed800085 	stc	0, cr0, \[r0, #532\].*
			24: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed800085 	stc	0, cr0, \[r0, #532\].*
			28: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed800085 	stc	0, cr0, \[r0, #532\].*
			2c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed100085 	ldc	0, cr0, \[r0, #-532\].*
			30: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed100085 	ldc	0, cr0, \[r0, #-532\].*
			34: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed100085 	ldc	0, cr0, \[r0, #-532\].*
			38: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed100085 	ldc	0, cr0, \[r0, #-532\].*
			3c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed100085 	ldc	0, cr0, \[r0, #-532\].*
			40: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed100085 	ldc	0, cr0, \[r0, #-532\].*
			44: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed000085 	stc	0, cr0, \[r0, #-532\].*
			48: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed000085 	stc	0, cr0, \[r0, #-532\].*
			4c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed000085 	stc	0, cr0, \[r0, #-532\].*
			50: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed000085 	stc	0, cr0, \[r0, #-532\].*
			54: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed000085 	stc	0, cr0, \[r0, #-532\].*
			58: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed000085 	stc	0, cr0, \[r0, #-532\].*
			5c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> edd00085 	ldcl	0, cr0, \[r0, #532\].*
			60: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> edd00085 	ldcl	0, cr0, \[r0, #532\].*
			64: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> edd00085 	ldcl	0, cr0, \[r0, #532\].*
			68: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> edd00085 	ldcl	0, cr0, \[r0, #532\].*
			6c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> edd00085 	ldcl	0, cr0, \[r0, #532\].*
			70: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> edd00085 	ldcl	0, cr0, \[r0, #532\].*
			74: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> edc00085 	stcl	0, cr0, \[r0, #532\].*
			78: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> edc00085 	stcl	0, cr0, \[r0, #532\].*
			7c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> edc00085 	stcl	0, cr0, \[r0, #532\].*
			80: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> edc00085 	stcl	0, cr0, \[r0, #532\].*
			84: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> edc00085 	stcl	0, cr0, \[r0, #532\].*
			88: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> edc00085 	stcl	0, cr0, \[r0, #532\].*
			8c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed500085 	ldcl	0, cr0, \[r0, #-532\].*
			90: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed500085 	ldcl	0, cr0, \[r0, #-532\].*
			94: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed500085 	ldcl	0, cr0, \[r0, #-532\].*
			98: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed500085 	ldcl	0, cr0, \[r0, #-532\].*
			9c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed500085 	ldcl	0, cr0, \[r0, #-532\].*
			a0: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed500085 	ldcl	0, cr0, \[r0, #-532\].*
			a4: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed400085 	stcl	0, cr0, \[r0, #-532\].*
			a8: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed400085 	stcl	0, cr0, \[r0, #-532\].*
			ac: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed400085 	stcl	0, cr0, \[r0, #-532\].*
			b0: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed400085 	stcl	0, cr0, \[r0, #-532\].*
			b4: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed400085 	stcl	0, cr0, \[r0, #-532\].*
			b8: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed400085 	stcl	0, cr0, \[r0, #-532\].*
			bc: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fd900085 	ldc2	0, cr0, \[r0, #532\].*
			c0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fd900085 	ldc2	0, cr0, \[r0, #532\].*
			c4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fd900085 	ldc2	0, cr0, \[r0, #532\].*
			c8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fd900085 	ldc2	0, cr0, \[r0, #532\].*
			cc: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fd900085 	ldc2	0, cr0, \[r0, #532\].*
			d0: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fd900085 	ldc2	0, cr0, \[r0, #532\].*
			d4: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fd800085 	stc2	0, cr0, \[r0, #532\].*
			d8: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fd800085 	stc2	0, cr0, \[r0, #532\].*
			dc: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fd800085 	stc2	0, cr0, \[r0, #532\].*
			e0: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fd800085 	stc2	0, cr0, \[r0, #532\].*
			e4: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fd800085 	stc2	0, cr0, \[r0, #532\].*
			e8: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fd800085 	stc2	0, cr0, \[r0, #532\].*
			ec: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fd100085 	ldc2	0, cr0, \[r0, #-532\].*
			f0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fd100085 	ldc2	0, cr0, \[r0, #-532\].*
			f4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fd100085 	ldc2	0, cr0, \[r0, #-532\].*
			f8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fd100085 	ldc2	0, cr0, \[r0, #-532\].*
			fc: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fd100085 	ldc2	0, cr0, \[r0, #-532\].*
			100: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fd100085 	ldc2	0, cr0, \[r0, #-532\].*
			104: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fd000085 	stc2	0, cr0, \[r0, #-532\].*
			108: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fd000085 	stc2	0, cr0, \[r0, #-532\].*
			10c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fd000085 	stc2	0, cr0, \[r0, #-532\].*
			110: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fd000085 	stc2	0, cr0, \[r0, #-532\].*
			114: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fd000085 	stc2	0, cr0, \[r0, #-532\].*
			118: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fd000085 	stc2	0, cr0, \[r0, #-532\].*
			11c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fdd00085 	ldc2l	0, cr0, \[r0, #532\].*
			120: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fdd00085 	ldc2l	0, cr0, \[r0, #532\].*
			124: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fdd00085 	ldc2l	0, cr0, \[r0, #532\].*
			128: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fdd00085 	ldc2l	0, cr0, \[r0, #532\].*
			12c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fdd00085 	ldc2l	0, cr0, \[r0, #532\].*
			130: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fdd00085 	ldc2l	0, cr0, \[r0, #532\].*
			134: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fdc00085 	stc2l	0, cr0, \[r0, #532\].*
			138: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fdc00085 	stc2l	0, cr0, \[r0, #532\].*
			13c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fdc00085 	stc2l	0, cr0, \[r0, #532\].*
			140: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fdc00085 	stc2l	0, cr0, \[r0, #532\].*
			144: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fdc00085 	stc2l	0, cr0, \[r0, #532\].*
			148: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fdc00085 	stc2l	0, cr0, \[r0, #532\].*
			14c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fd500085 	ldc2l	0, cr0, \[r0, #-532\].*
			150: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fd500085 	ldc2l	0, cr0, \[r0, #-532\].*
			154: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fd500085 	ldc2l	0, cr0, \[r0, #-532\].*
			158: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fd500085 	ldc2l	0, cr0, \[r0, #-532\].*
			15c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fd500085 	ldc2l	0, cr0, \[r0, #-532\].*
			160: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fd500085 	ldc2l	0, cr0, \[r0, #-532\].*
			164: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> fd400085 	stc2l	0, cr0, \[r0, #-532\].*
			168: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> fd400085 	stc2l	0, cr0, \[r0, #-532\].*
			16c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> fd400085 	stc2l	0, cr0, \[r0, #-532\].*
			170: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> fd400085 	stc2l	0, cr0, \[r0, #-532\].*
			174: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> fd400085 	stc2l	0, cr0, \[r0, #-532\].*
			178: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> fd400085 	stc2l	0, cr0, \[r0, #-532\].*
			17c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed900a85 	(vldr|flds)	s0, \[r0, #532\].*
			180: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed900a85 	(vldr|flds)	s0, \[r0, #532\].*
			184: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed900a85 	(vldr|flds)	s0, \[r0, #532\].*
			188: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed900a85 	(vldr|flds)	s0, \[r0, #532\].*
			18c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed900a85 	(vldr|flds)	s0, \[r0, #532\].*
			190: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed900a85 	(vldr|flds)	s0, \[r0, #532\].*
			194: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed800a85 	(vstr|fsts)	s0, \[r0, #532\].*
			198: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed800a85 	(vstr|fsts)	s0, \[r0, #532\].*
			19c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed800a85 	(vstr|fsts)	s0, \[r0, #532\].*
			1a0: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed800a85 	(vstr|fsts)	s0, \[r0, #532\].*
			1a4: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed800a85 	(vstr|fsts)	s0, \[r0, #532\].*
			1a8: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed800a85 	(vstr|fsts)	s0, \[r0, #532\].*
			1ac: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed100a85 	(vldr|flds)	s0, \[r0, #-532\].*
			1b0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed100a85 	(vldr|flds)	s0, \[r0, #-532\].*
			1b4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed100a85 	(vldr|flds)	s0, \[r0, #-532\].*
			1b8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed100a85 	(vldr|flds)	s0, \[r0, #-532\].*
			1bc: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed100a85 	(vldr|flds)	s0, \[r0, #-532\].*
			1c0: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed100a85 	(vldr|flds)	s0, \[r0, #-532\].*
			1c4: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed000a85 	(vstr|fsts)	s0, \[r0, #-532\].*
			1c8: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed000a85 	(vstr|fsts)	s0, \[r0, #-532\].*
			1cc: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed000a85 	(vstr|fsts)	s0, \[r0, #-532\].*
			1d0: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed000a85 	(vstr|fsts)	s0, \[r0, #-532\].*
			1d4: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed000a85 	(vstr|fsts)	s0, \[r0, #-532\].*
			1d8: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed000a85 	(vstr|fsts)	s0, \[r0, #-532\].*
			1dc: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\].*
			1e0: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\].*
			1e4: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\].*
			1e8: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\].*
			1ec: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\].*
			1f0: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\].*
			1f4: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\].*
			1f8: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\].*
			1fc: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\].*
			200: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\].*
			204: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\].*
			208: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\].*
			20c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\].*
			210: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\].*
			214: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\].*
			218: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\].*
			21c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\].*
			220: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\].*
			224: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\].*
			228: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\].*
			22c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\].*
			230: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\].*
			234: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\].*
			238: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\].*
			23c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\].*
			240: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\].*
			244: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\].*
			248: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\].*
			24c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\].*
			250: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed900b85 	vldr	d0, \[r0, #532\].*
			254: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\].*
			258: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\].*
			25c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\].*
			260: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\].*
			264: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\].*
			268: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed800b85 	vstr	d0, \[r0, #532\].*
			26c: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\].*
			270: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\].*
			274: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\].*
			278: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\].*
			27c: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\].*
			280: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed100b85 	vldr	d0, \[r0, #-532\].*
			284: R_ARM_LDC_SB_G2	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\].*
			288: R_ARM_LDC_PC_G0	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\].*
			28c: R_ARM_LDC_PC_G1	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\].*
			290: R_ARM_LDC_PC_G2	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\].*
			294: R_ARM_LDC_SB_G0	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\].*
			298: R_ARM_LDC_SB_G1	f
0[0-9a-f]+ <[^>]+> ed000b85 	vstr	d0, \[r0, #-532\].*
			29c: R_ARM_LDC_SB_G2	f
