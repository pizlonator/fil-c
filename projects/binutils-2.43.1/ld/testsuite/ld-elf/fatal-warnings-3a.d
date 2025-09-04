#PR ld/31289
#source: start.s
#ld: --fatal-warnings -z bad-option1 -z bad-option2
#error: unsupported option: -z bad-option1
#xfail: ![uses_elf_em]
# These targets don't support -z options.
