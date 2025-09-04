#source: tls.s
#ld: --shared tmpdir/tlslib.so
#readelf: -Wr

Relocation section '.rela.dyn' at offset 0x[0-9a-f]+ contains 7 entries:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
[0-9a-f]+ +[0-9a-f]+ R_RISCV_TLS_DTPMOD64 +0
[0-9a-f]+ +[0-9a-f]+ R_RISCV_TLS_TPREL64 +4
[0-9a-f]+ +[0-9a-f]+ R_RISCV_TLSDESC +4
[0-9a-f]+ +[0-9a-f]+ R_RISCV_TLS_DTPMOD64 +0+ sg1 \+ 0
[0-9a-f]+ +[0-9a-f]+ R_RISCV_TLS_DTPREL64 +0+ sg1 \+ 0
[0-9a-f]+ +[0-9a-f]+ R_RISCV_TLS_TPREL64 +0+ sg1 \+ 0
[0-9a-f]+ +[0-9a-f]+ R_RISCV_TLSDESC +0+ sg1 \+ 0

Relocation section '.rela.plt' at offset 0x[0-9a-f]+ contains 1 entry:
 +Offset +Info +Type +Symbol's Value +Symbol's Name \+ Addend
[0-9a-f]+ +[0-9a-f]+ R_RISCV_JUMP_SLOT +0+ __tls_get_addr \+ 0
