#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Success control for the LLVMGetCallBrIndirectDest index check: an
   in-range index returns the indirect destination the callbr was built
   with. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBasicBlockRef otherEntry = LLVMAppendBasicBlockInContext(
        context, fn, "other");
    LLVMBasicBlockRef indirect = LLVMAppendBasicBlockInContext(context, fn,
                                                               "indirect");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMTypeRef callee_ty = LLVMFunctionType(
        LLVMVoidTypeInContext(context), NULL, 0, false);
    LLVMValueRef callee = LLVMAddFunction(module, "callee", callee_ty);
    LLVMBasicBlockRef indirect_dests[1] = { indirect };
    LLVMValueRef call_br = LLVMBuildCallBr(builder, callee_ty, callee,
                                           otherEntry, indirect_dests, 1,
                                           NULL, 0, NULL, 0, "");
    if (LLVMGetCallBrNumIndirectDests(call_br) != 1) {
        fprintf(stderr, "bad: expected one indirect destination\n");
        abort();
    }
    LLVMBasicBlockRef got = LLVMGetCallBrIndirectDest(call_br, 0);
    /* Wrapper calls hand back fresh objects, so the block is identified by
       its name. */
    if (!got || !LLVMGetBasicBlockName(got) ||
        strcmp(LLVMGetBasicBlockName(got), "indirect")) {
        fprintf(stderr, "bad: indirect destination did not come back\n");
        abort();
    }
    printf("callbr indirect dest ok\n");
    return 0;
}
