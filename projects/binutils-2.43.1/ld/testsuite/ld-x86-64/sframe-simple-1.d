#as: --gsframe
#source: sframe-foo.s
#source: sframe-bar.s
#objdump: --sframe=.sframe
#ld: -shared --no-rosegment
#name: SFrame simple link

.*: +file format .*

Contents of the SFrame section .sframe:
  Header :

    Version: SFRAME_VERSION_2
    Flags: SFRAME_F_FDE_SORTED
    CFA fixed RA offset: \-8
#...

  Function Index :

#...

#...

    func idx \[2\]: pc = 0x1020, size = 53 bytes
    STARTPC +CFA +FP +RA +
    0+1020 +sp\+8 +u +f +
    0+1021 +sp\+16 +c-16 +f +
    0+1024 +fp\+16 +c-16 +f +
    0+1054 +sp\+8 +c-16 +f +

    func idx \[3\]: pc = 0x1055, size = 37 bytes
    STARTPC +CFA +FP +RA +
    0+1055 +sp\+8 +u +f +
    0+1056 +sp\+16 +c-16 +f +
    0+1059 +fp\+16 +c-16 +f +
    0+1079 +sp\+8 +c-16 +f +
