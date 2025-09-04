#name: Verify Armv9.5-A implies Armv9.4-A features by checking bfloat16 is enabled
#source: bfloat16.s
#as: -mno-warn-deprecated -march=armv9.5-a+simd -I$srcdir/$subdir
#objdump: -dr --show-raw-insn
#...
