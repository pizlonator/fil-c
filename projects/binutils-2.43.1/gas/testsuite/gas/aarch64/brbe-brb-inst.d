#as: -march=armv9.1-a+brbe
#objdump: -dr
# This test is only valid on ELF based ports.
#notarget: *-*-*coff *-*-pe *-*-wince *-*-*aout* *-*-netbsd

.*:     file format .*

Disassembly of section \.text:

0+ <.*>:
.*:	d509729f 	brb	iall
.*:	d509729e 	brb	iall	// unpredictable encoding \(Rt!=31\): #30
.*:	d50972bf 	brb	inj
.*:	d50972be 	brb	inj	// unpredictable encoding \(Rt!=31\): #30
