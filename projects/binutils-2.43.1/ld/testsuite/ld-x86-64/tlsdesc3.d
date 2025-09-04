#name: TLS GDesc->LE transition check (LEA)
#as: --64
#ld: -melf_x86_64
#error: .*: relocation R_X86_64_GOTPC32_TLSDESC against `foo' must be used in LEA only
