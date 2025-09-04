#source: pr31868a.s
#source: pr31868b.s
#as: --x32
#warning_output: pr31868c.l
#ld: -r -m elf32_x86_64 -z isa-level-report=used
#readelf: -n

Displaying notes found in: .note.gnu.property
[ 	]+Owner[ 	]+Data size[ 	]+Description
  GNU                  0x00000018	NT_GNU_PROPERTY_TYPE_0
      Properties: x86 ISA needed: x86-64-baseline, x86-64-v2, x86-64-v3, <unknown: 10>, <unknown: 20>
	x86 ISA used: x86-64-baseline, x86-64-v2, x86-64-v3, x86-64-v4, <unknown: 10>, <unknown: 40>
