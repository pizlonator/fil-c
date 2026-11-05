#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* LLVMGetCallBrIndirectDest with an index past the indirect-destination
   count.  CallBrInst::getIndirectDest indexes the operand array raw
   (*(&Op<-1>() - getNumIndirectDests() + i)), so an out-of-range index
   reads wild memory; the wrapper must panic instead.  Deliberate tripwire:
   the manifest's output-excludes greps for LLVM's assert text, so if the
   check ever fails to fire the test fails loudly. */

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
    LLVMGetCallBrIndirectDest(call_br, 7);
    return 0;
}
