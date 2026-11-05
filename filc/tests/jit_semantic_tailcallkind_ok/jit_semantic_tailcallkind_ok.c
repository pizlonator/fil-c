#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMSetTailCallKind tail-call-kind check: all
   four llvm::LLVMTailCallKind values (none, tail, musttail, notail) stay
   legal, and the last one is observable through LLVMGetTailCallKind.
 */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnty = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef callee = LLVMAddFunction(module, "callee", fnty);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(i32, NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef call = LLVMBuildCall2(builder, fnty, callee, NULL, 0, "");
    LLVMSetTailCallKind(call, LLVMTailCallKindNone);
    LLVMSetTailCallKind(call, LLVMTailCallKindTail);
    LLVMSetTailCallKind(call, LLVMTailCallKindMustTail);
    LLVMSetTailCallKind(call, LLVMTailCallKindNoTail);
    if (LLVMGetTailCallKind(call) != LLVMTailCallKindNoTail) {
        fprintf(stderr, "bad: wrong tail call kind\n");
        abort();
    }
    LLVMBuildRet(builder, call);

    printf("tailcallkind ok\n");
    return 0;
}
