#name: bad .bss / .struct data allocation directives
#source: bss.s
#error_output: bad-bss.err
#notarget: arm*-*-elf arm*-*-nto*
#target: i?86-*-* x86_64-*-* ia64-*-* arm-*-* aarch64-*-* kvx-*-*
