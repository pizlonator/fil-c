#source: pr31868a.s
#source: pr31868b.s
#as: --64 -defsym __64_bit__=1
#warning_output: pr31868a.l
#ld: -r -m elf_x86_64 -z isa-level-report=all
#readelf: -n

Displaying notes found in: .note.gnu.property
[ 	]+Owner[ 	]+Data size[ 	]+Description
  GNU                  0x00000020	NT_GNU_PROPERTY_TYPE_0
      Properties: x86 ISA needed: x86-64-baseline, x86-64-v2, x86-64-v3, <unknown: 10>, <unknown: 20>
	x86 ISA used: x86-64-baseline, x86-64-v2, x86-64-v3, x86-64-v4, <unknown: 10>, <unknown: 40>
