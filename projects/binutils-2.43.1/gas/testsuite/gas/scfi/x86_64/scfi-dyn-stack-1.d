#as: --scfi=experimental --gsframe -W
#as: --gsframe
#objdump: --sframe
#name: SCFI for dynamic alloc stack
#...

Contents of the SFrame section .sframe:
  Header :

    Version: SFRAME_VERSION_2
    Flags: NONE
    CFA fixed RA offset: \-8
    Num FDEs: 1
    Num FREs: 4

  Function Index :

    func idx \[0\]: pc = 0x0, size = 87 bytes
    STARTPC + CFA + FP + RA +
    0+0000 + sp\+8 + u + f +
    0+0001 + sp\+16 + c-16 + f +
    0+0004 + fp\+16 + c-16 + f +
    0+0056 + sp\+8 + u + f +

#pass
