#name: TLS GDesc->LE transition check (LEA)
#as: --32
#ld: -melf_i386
#error: .*: relocation R_386_TLS_GOTDESC against `foo' must be used in LEA only
