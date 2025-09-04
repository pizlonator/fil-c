#name: TLS IE->LE transition check (R_386_TLS_IE)
#as: --32
#ld: -melf_i386
#error: .*: relocation R_386_TLS_IE against `foo' must be used in ADD or MOV only
