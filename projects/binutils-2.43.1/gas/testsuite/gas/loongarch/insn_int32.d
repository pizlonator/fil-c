#as-new:
#objdump: -d -M no-aliases
#skip: loongarch64-*-*

.*:     file format .*


Disassembly of section .text:

0+ <.*>:
   0:	020000a4 	slti        	\$a0, \$a1, 0
   4:	021ffca4 	slti        	\$a0, \$a1, 2047
   8:	022004a4 	slti        	\$a0, \$a1, -2047
   c:	024000a4 	sltui       	\$a0, \$a1, 0
  10:	025ffca4 	sltui       	\$a0, \$a1, 2047
  14:	026004a4 	sltui       	\$a0, \$a1, -2047
  18:	028000a4 	addi.w      	\$a0, \$a1, 0
  1c:	029ffca4 	addi.w      	\$a0, \$a1, 2047
  20:	02a004a4 	addi.w      	\$a0, \$a1, -2047
  24:	034000a4 	andi        	\$a0, \$a1, 0x0
  28:	035ffca4 	andi        	\$a0, \$a1, 0x7ff
  2c:	038000a4 	ori         	\$a0, \$a1, 0x0
  30:	039ffca4 	ori         	\$a0, \$a1, 0x7ff
  34:	03c000a4 	xori        	\$a0, \$a1, 0x0
  38:	03dffca4 	xori        	\$a0, \$a1, 0x7ff
  3c:	14000004 	lu12i.w     	\$a0, 0
  40:	14ffffe4 	lu12i.w     	\$a0, 524287
  44:	1c000004 	pcaddu12i   	\$a0, 0
  48:	1cffffe4 	pcaddu12i   	\$a0, 524287
  4c:	1d000024 	pcaddu12i   	\$a0, -524287
  50:	0004b58b 	alsl.w      	\$a7, \$t0, \$t1, 0x2
  54:	0006b58b 	alsl.wu     	\$a7, \$t0, \$t1, 0x2
  58:	002a0002 	break       	0x2
  5c:	002a8002 	dbcl        	0x2
  60:	002b0002 	syscall     	0x2
  64:	0040898b 	slli.w      	\$a7, \$t0, 0x2
  68:	0044898b 	srli.w      	\$a7, \$t0, 0x2
  6c:	004889ac 	srai.w      	\$t0, \$t1, 0x2
  70:	02048dac 	slti        	\$t0, \$t1, 291
  74:	02448dac 	sltui       	\$t0, \$t1, 291
  78:	02848dac 	addi.w      	\$t0, \$t1, 291
  7c:	034009ac 	andi        	\$t0, \$t1, 0x2
  80:	038009ac 	ori         	\$t0, \$t1, 0x2
  84:	03c009ac 	xori        	\$t0, \$t1, 0x2
  88:	1400246c 	lu12i.w     	\$t0, 291
  8c:	1c00246c 	pcaddu12i   	\$t0, 291
  90:	04048c0c 	csrrd       	\$t0, 0x123
  94:	04048c2c 	csrwr       	\$t0, 0x123
  98:	040009ac 	csrxchg     	\$t0, \$t1, 0x2
  9c:	060009a2 	cacop       	0x2, \$t1, 2
  a0:	064009ac 	lddir       	\$t0, \$t1, 0x2
  a4:	06440980 	ldpte       	\$t0, 0x2
  a8:	0649b9a2 	invtlb      	0x2, \$t1, \$t2
  ac:	000060a4 	rdtimel.w   	\$a0, \$a1
  b0:	000064a4 	rdtimeh.w   	\$a0, \$a1
  b4:	000418a4 	alsl.w      	\$a0, \$a1, \$a2, 0x1
  b8:	000598a4 	alsl.w      	\$a0, \$a1, \$a2, 0x4
  bc:	000618a4 	alsl.wu     	\$a0, \$a1, \$a2, 0x1
  c0:	000798a4 	alsl.wu     	\$a0, \$a1, \$a2, 0x4
  c4:	001018a4 	add.w       	\$a0, \$a1, \$a2
  c8:	001118a4 	sub.w       	\$a0, \$a1, \$a2
  cc:	001218a4 	slt         	\$a0, \$a1, \$a2
  d0:	001298a4 	sltu        	\$a0, \$a1, \$a2
  d4:	001418a4 	nor         	\$a0, \$a1, \$a2
  d8:	001498a4 	and         	\$a0, \$a1, \$a2
  dc:	001518a4 	or          	\$a0, \$a1, \$a2
  e0:	001598a4 	xor         	\$a0, \$a1, \$a2
  e4:	001718a4 	sll.w       	\$a0, \$a1, \$a2
  e8:	001798a4 	srl.w       	\$a0, \$a1, \$a2
  ec:	001818a4 	sra.w       	\$a0, \$a1, \$a2
  f0:	001c18a4 	mul.w       	\$a0, \$a1, \$a2
  f4:	001c98a4 	mulh.w      	\$a0, \$a1, \$a2
  f8:	001d18a4 	mulh.wu     	\$a0, \$a1, \$a2
  fc:	002018a4 	div.w       	\$a0, \$a1, \$a2
 100:	002098a4 	mod.w       	\$a0, \$a1, \$a2
 104:	002118a4 	div.wu      	\$a0, \$a1, \$a2
 108:	002198a4 	mod.wu      	\$a0, \$a1, \$a2
 10c:	002a0000 	break       	0x0
 110:	002a7fff 	break       	0x7fff
 114:	002a8000 	dbcl        	0x0
 118:	002affff 	dbcl        	0x7fff
 11c:	004080a4 	slli.w      	\$a0, \$a1, 0x0
 120:	004084a4 	slli.w      	\$a0, \$a1, 0x1
 124:	0040fca4 	slli.w      	\$a0, \$a1, 0x1f
 128:	004480a4 	srli.w      	\$a0, \$a1, 0x0
 12c:	004484a4 	srli.w      	\$a0, \$a1, 0x1
 130:	0044fca4 	srli.w      	\$a0, \$a1, 0x1f
 134:	004880a4 	srai.w      	\$a0, \$a1, 0x0
 138:	004884a4 	srai.w      	\$a0, \$a1, 0x1
 13c:	0048fca4 	srai.w      	\$a0, \$a1, 0x1f
 140:	200000a4 	ll.w        	\$a0, \$a1, 0
 144:	203ffca4 	ll.w        	\$a0, \$a1, 16380
 148:	210000a4 	sc.w        	\$a0, \$a1, 0
 14c:	213ffca4 	sc.w        	\$a0, \$a1, 16380
 150:	280000a4 	ld.b        	\$a0, \$a1, 0
 154:	281ffca4 	ld.b        	\$a0, \$a1, 2047
 158:	282004a4 	ld.b        	\$a0, \$a1, -2047
 15c:	284000a4 	ld.h        	\$a0, \$a1, 0
 160:	285ffca4 	ld.h        	\$a0, \$a1, 2047
 164:	286004a4 	ld.h        	\$a0, \$a1, -2047
 168:	288000a4 	ld.w        	\$a0, \$a1, 0
 16c:	289ffca4 	ld.w        	\$a0, \$a1, 2047
 170:	28a004a4 	ld.w        	\$a0, \$a1, -2047
 174:	290000a4 	st.b        	\$a0, \$a1, 0
 178:	291ffca4 	st.b        	\$a0, \$a1, 2047
 17c:	292004a4 	st.b        	\$a0, \$a1, -2047
 180:	294000a4 	st.h        	\$a0, \$a1, 0
 184:	295ffca4 	st.h        	\$a0, \$a1, 2047
 188:	296004a4 	st.h        	\$a0, \$a1, -2047
 18c:	298000a4 	st.w        	\$a0, \$a1, 0
 190:	299ffca4 	st.w        	\$a0, \$a1, 2047
 194:	29a004a4 	st.w        	\$a0, \$a1, -2047
 198:	2a0000a4 	ld.bu       	\$a0, \$a1, 0
 19c:	2a1ffca4 	ld.bu       	\$a0, \$a1, 2047
 1a0:	2a2004a4 	ld.bu       	\$a0, \$a1, -2047
 1a4:	2a4000a4 	ld.hu       	\$a0, \$a1, 0
 1a8:	2a5ffca4 	ld.hu       	\$a0, \$a1, 2047
 1ac:	2a6004a4 	ld.hu       	\$a0, \$a1, -2047
 1b0:	2ac000a0 	preld       	0x0, \$a1, 0
 1b4:	2adffcbf 	preld       	0x1f, \$a1, 2047
 1b8:	2ae004bf 	preld       	0x1f, \$a1, -2047
 1bc:	385714c4 	sc.q        	\$a0, \$a1, \$a2
 1c0:	385714c4 	sc.q        	\$a0, \$a1, \$a2
 1c4:	385780a4 	llacq.w     	\$a0, \$a1
 1c8:	385780a4 	llacq.w     	\$a0, \$a1
 1cc:	385784a4 	screl.w     	\$a0, \$a1
 1d0:	385784a4 	screl.w     	\$a0, \$a1
 1d4:	38720000 	dbar        	0x0
 1d8:	38727fff 	dbar        	0x7fff
 1dc:	38728000 	ibar        	0x0
 1e0:	3872ffff 	ibar        	0x7fff

0+1e4 <.L1>:
 1e4:	03400000 	andi        	\$zero, \$zero, 0x0
 1e8:	53ffffff 	b           	-4	# 1e4 <.L1>
 1ec:	57fffbff 	bl          	-8	# 1e4 <.L1>
 1f0:	5bfff485 	beq         	\$a0, \$a1, -12	# 1e4 <.L1>
 1f4:	5ffff085 	bne         	\$a0, \$a1, -16	# 1e4 <.L1>
 1f8:	63ffec85 	blt         	\$a0, \$a1, -20	# 1e4 <.L1>
 1fc:	63ffe8a4 	blt         	\$a1, \$a0, -24	# 1e4 <.L1>
 200:	67ffe485 	bge         	\$a0, \$a1, -28	# 1e4 <.L1>
 204:	67ffe0a4 	bge         	\$a1, \$a0, -32	# 1e4 <.L1>
 208:	6bffdc85 	bltu        	\$a0, \$a1, -36	# 1e4 <.L1>
 20c:	6bffd8a4 	bltu        	\$a1, \$a0, -40	# 1e4 <.L1>
 210:	6fffd485 	bgeu        	\$a0, \$a1, -44	# 1e4 <.L1>
 214:	6fffd0a4 	bgeu        	\$a1, \$a0, -48	# 1e4 <.L1>
 218:	4c000080 	jirl        	\$zero, \$a0, 0
