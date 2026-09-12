/* Extern global for the misaligned-access test (separate module, so the
   asm side materializes it through `#! global ptr`). */
unsigned long extcap[4] = {
    0x1111111111111111UL, 0x2222222222222222UL,
    0x3333333333333333UL, 0x4444444444444444UL
};
