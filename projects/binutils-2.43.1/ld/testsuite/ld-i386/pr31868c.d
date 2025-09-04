#source: ../ld-x86-64/pr31868a.s
#source: ../ld-x86-64/pr31868b.s
#as: --32
#ld: -r -melf_i386 -z isa-level-report=used
#warning_output:../ld-x86-64/pr31868c.l
#readelf: -n

Displaying notes found in: .note.gnu.property
[ 	]+Owner[ 	]+Data size[ 	]+Description
  GNU                  0x00000018	NT_GNU_PROPERTY_TYPE_0
      Properties: x86 ISA needed: x86-64-baseline, x86-64-v2, x86-64-v3, <unknown: 10>, <unknown: 20>
	x86 ISA used: x86-64-baseline, x86-64-v2, x86-64-v3, x86-64-v4, <unknown: 10>, <unknown: 40>
