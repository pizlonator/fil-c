#name: TLS IE->LE transition check
#as: --64
#ld: -melf_x86_64
#error: .*: relocation R_X86_64_GOTTPOFF against `foo' must be used in ADD or MOV only
