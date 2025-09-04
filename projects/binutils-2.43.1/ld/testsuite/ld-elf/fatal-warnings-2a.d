#PR ld/31289
#source: start.s
#ld: -z bad-option1 -z bad-option2
#warning: -z bad-option1 ignored
#xfail: ![uses_elf_em]
# These targets don't support -z options.
