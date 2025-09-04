#as: -32 -trap
#objdump: -dr --prefix-addresses --show-raw-insn
#name: MIPS div with traps
#source: div.s

# Test the div macro.

.*: +file format .*mips.*

Disassembly of section .text:
[0-9a-f]+ <[^>]*> 0085001a 	div	zero,a0,a1
[0-9a-f]+ <[^>]*> 00a001f4 	teq	a1,zero,0x7
[0-9a-f]+ <[^>]*> 0085001a 	div	zero,a0,a1
[0-9a-f]+ <[^>]*> 2401ffff 	li	at,-1
[0-9a-f]+ <[^>]*> 14a10002 	bne	a1,at,[0-9a-f]+ <[^>]*>
[0-9a-f]+ <[^>]*> 3c018000 	lui	at,0x8000
[0-9a-f]+ <[^>]*> 008101b4 	teq	a0,at,0x6
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 00c001f4 	teq	a2,zero,0x7
[0-9a-f]+ <[^>]*> 00a6001a 	div	zero,a1,a2
[0-9a-f]+ <[^>]*> 2401ffff 	li	at,-1
[0-9a-f]+ <[^>]*> 14c10002 	bne	a2,at,[0-9a-f]+ <[^>]*>
[0-9a-f]+ <[^>]*> 3c018000 	lui	at,0x8000
[0-9a-f]+ <[^>]*> 00a101b4 	teq	a1,at,0x6
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 00802025 	move	a0,a0
[0-9a-f]+ <[^>]*> 00a02025 	move	a0,a1
[0-9a-f]+ <[^>]*> 00042022 	neg	a0,a0
[0-9a-f]+ <[^>]*> 00052022 	neg	a0,a1
[0-9a-f]+ <[^>]*> 24010002 	li	at,2
[0-9a-f]+ <[^>]*> 0081001a 	div	zero,a0,at
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 24010002 	li	at,2
[0-9a-f]+ <[^>]*> 00a1001a 	div	zero,a1,at
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 34018000 	li	at,0x8000
[0-9a-f]+ <[^>]*> 0081001a 	div	zero,a0,at
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 34018000 	li	at,0x8000
[0-9a-f]+ <[^>]*> 00a1001a 	div	zero,a1,at
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 24018000 	li	at,-32768
[0-9a-f]+ <[^>]*> 0081001a 	div	zero,a0,at
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 24018000 	li	at,-32768
[0-9a-f]+ <[^>]*> 00a1001a 	div	zero,a1,at
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 3c010001 	lui	at,0x1
[0-9a-f]+ <[^>]*> 0081001a 	div	zero,a0,at
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 3c010001 	lui	at,0x1
[0-9a-f]+ <[^>]*> 00a1001a 	div	zero,a1,at
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 3c010001 	lui	at,0x1
[0-9a-f]+ <[^>]*> 3421a5a5 	ori	at,at,0xa5a5
[0-9a-f]+ <[^>]*> 0081001a 	div	zero,a0,at
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 3c010001 	lui	at,0x1
[0-9a-f]+ <[^>]*> 3421a5a5 	ori	at,at,0xa5a5
[0-9a-f]+ <[^>]*> 00a1001a 	div	zero,a1,at
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 0085001b 	divu	zero,a0,a1
[0-9a-f]+ <[^>]*> 00a001f4 	teq	a1,zero,0x7
[0-9a-f]+ <[^>]*> 0085001b 	divu	zero,a0,a1
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 00c001f4 	teq	a2,zero,0x7
[0-9a-f]+ <[^>]*> 00a6001b 	divu	zero,a1,a2
[0-9a-f]+ <[^>]*> 00002012 	mflo	a0
[0-9a-f]+ <[^>]*> 00802025 	move	a0,a0
[0-9a-f]+ <[^>]*> 00c001f4 	teq	a2,zero,0x7
[0-9a-f]+ <[^>]*> 00a6001a 	div	zero,a1,a2
[0-9a-f]+ <[^>]*> 2401ffff 	li	at,-1
[0-9a-f]+ <[^>]*> 14c10002 	bne	a2,at,[0-9a-f]+ <[^>]*>
[0-9a-f]+ <[^>]*> 3c018000 	lui	at,0x8000
[0-9a-f]+ <[^>]*> 00a101b4 	teq	a1,at,0x6
[0-9a-f]+ <[^>]*> 00002010 	mfhi	a0
[0-9a-f]+ <[^>]*> 24010002 	li	at,2
[0-9a-f]+ <[^>]*> 00a1001b 	divu	zero,a1,at
[0-9a-f]+ <[^>]*> 00002010 	mfhi	a0
	\.\.\.
