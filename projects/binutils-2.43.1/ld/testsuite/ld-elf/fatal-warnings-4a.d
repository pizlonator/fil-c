#PR ld/31289
#source: start.s
#ld: -z bad-option1 -z bad-option2 --fatal-warnings
#error: unsupported option: -z bad-option1
#xfail: ![uses_elf_em]
# These targets don't support -z options.
