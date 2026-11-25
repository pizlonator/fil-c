// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.

// Test: an instruction zegarmistrz does not implement must exit with the
// unimplemented-instruction exit code, NOT the poison exit code. The guest
// executes VGETMANTSS (EVEX.NDS.LIG.F3.0F.W0 27 /r imm8; bytes below), a
// real AVX512F instruction the emulator does not implement, so the
// interpreter throws the "unimplemented vgetmantss (EVEX (phase 2:
// unsupported EVEX form))" guest error. djb: poison violations exit 139
// (SIGSEGV) while unknown instructions must exit 132 (SIGILL). Most hosts
// SIGILL on this instruction too (they lack it), so run-tests only runs
// this guest under the emulator. The raw bytes keep the build independent
// of assembler AVX512 support.

#include <unistd.h>

int main(void) {

    // vgetmantss $0, %xmm1, %xmm2, %xmm3 (not implemented by zegarmistrz).
    __asm__ __volatile__(".byte 0x62, 0xf3, 0x6d, 0x08, 0x27, 0xd9, 0x00\n\t"

                         ::: "xmm1", "xmm2", "xmm3");

    (void)!write(1, "NOTREACHED\n", 11);

    return 0;

}
