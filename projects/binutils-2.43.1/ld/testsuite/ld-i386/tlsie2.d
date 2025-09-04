#name: TLS IE->LE transition check (R_386_TLS_GOTIE with %eax)
#as: --32
#ld: -melf_i386
#error: .*: relocation R_386_TLS_GOTIE against `foo' must be used in ADD, SUB or MOV only
