#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMGetInlineAsm with a dialect that is neither ATT nor Intel.  InlineAsm
   keeps the value verbatim and LLVMGetInlineAsmDialect ends in an
   llvm_unreachable ("Unrecognized inline assembly dialect",
   llvm/lib/IR/Core.cpp) for anything else, so the wrapper's dialect check
   rejects it before LLVM sees it. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef ifnTy = LLVMFunctionType(LLVMVoidTypeInContext(context),
                                         NULL, 0, false);

    LLVMGetInlineAsm(ifnTy, "nop", 3, "", 0, false, false, 2, false);

    return 0;
}
