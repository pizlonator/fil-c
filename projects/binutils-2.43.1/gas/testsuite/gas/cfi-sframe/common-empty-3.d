#as: --gsframe
#warning: skipping SFrame FDE; non-default RA register 0
#objdump: --sframe=.sframe
#name: SFrame supports only default return column
#...
Contents of the SFrame section .sframe:

  Header :

    Version: SFRAME_VERSION_2
    Flags: NONE
#?    CFA fixed FP offset: \-?\d+
#?    CFA fixed RA offset: \-?\d+
    Num FDEs: 0
    Num FREs: 0

#pass
