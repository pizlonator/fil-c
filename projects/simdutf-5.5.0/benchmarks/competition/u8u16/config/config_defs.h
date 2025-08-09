/* Target architecture choices. */
#define LONG_LONG_TARGET 1
#define ALTIVEC_TARGET 2
#define SPU_TARGET 3
#define SSE_TARGET 4
#define MMX_TARGET 5
#define VIS_TARGET 6


/* Transposition algorithm choices. */
#define S2P_IDEAL 1
#define S2P_BYTEPACK 2
#define P2S_IDEAL 1
#define P2S_BYTEMERGE 2

/* Deletion algorithm choices. */
/* A.  Bit deletion */
#define NO_BIT_DELETION 1
#define ROTATION_TO_LEFT8 2
#define ROTATION_TO_LEFT4 3
#define SHIFT_TO_RIGHT4 4
#define SHIFT_TO_RIGHT8 5
#define ADDITIVE_SHIFT_TO_LEFT4 6
#define ADDITIVE_SHIFT_TO_LEFT8 7
#define PERMUTE_INDEX_TO_RIGHT8 8

/* B. Deletion in byte space. */
#define NO_BYTE_DELETION 1
#define BYTE_DEL_BY_PERMUTE_TO_LEFT8 2
#define BYTE_DEL_BY_PERMUTE_TO_RIGHT8 3

/* C. Deletion in doublebyte space. */
#define FROM_LEFT8 1
#define FROM_LEFT4 2
#define ALTIVEC_FROM_LEFT8 3


#define LITTLE_ENDIAN  4321
#define BIG_ENDIAN     1234

#ifdef NO_OPTIMIZATION
#define NO_ASCII_OPTIMIZATION
#endif
