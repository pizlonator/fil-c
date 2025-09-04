#name: TLS GDesc->LE transition check (indirect CALL)
#as: --64
#ld: -melf_x86_64
#error: .*: relocation R_X86_64_TLSDESC_CALL against `foo' must be used in indirect CALL only
