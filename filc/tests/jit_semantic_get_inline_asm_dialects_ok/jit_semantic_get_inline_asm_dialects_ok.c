#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMGetInlineAsm dialect check: both dialects,
   ATT and Intel, stay legal and round-trip through LLVMGetInlineAsmDialect. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef ifnTy = LLVMFunctionType(LLVMVoidTypeInContext(context),
                                         NULL, 0, false);
    LLVMValueRef att = LLVMGetInlineAsm(ifnTy, "nop", 3, "", 0, false, false,
                                        LLVMInlineAsmDialectATT, false);
    LLVMValueRef intel = LLVMGetInlineAsm(ifnTy, "nop", 3, "", 0, false, false,
                                          LLVMInlineAsmDialectIntel, false);
    if (!att || !intel || !LLVMIsAInlineAsm(att) || !LLVMIsAInlineAsm(intel)) {
        fprintf(stderr, "bad: no inline asm values\n");
        abort();
    }
    if (LLVMGetInlineAsmDialect(att) != LLVMInlineAsmDialectATT ||
        LLVMGetInlineAsmDialect(intel) != LLVMInlineAsmDialectIntel) {
        fprintf(stderr, "bad: wrong dialects\n");
        abort();
    }

    printf("inline asm dialects ok\n");
    return 0;
}
