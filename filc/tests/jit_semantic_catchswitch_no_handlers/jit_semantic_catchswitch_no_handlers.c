#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildCatchSwitch with num_handlers=0.  LLVM itself tolerates a
   zero-reservation catchswitch when an unwind destination is present (the
   reservation is NumHandlers plus the parent and unwind slots), but with no
   unwind destination the hung-off-uses allocation runs off the end and the
   call only survives as an incidental Fil-C memory-safety panic ("cannot
   access pointer with null object" inside LLVM's allocHungoffUses).
   Requiring at least one handler is a deliberate Fil-C-side restriction: a
   catchswitch without handlers is only ever useful via a later AddHandler,
   and the reservation shape it depends on is fragile. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef voidTy = LLVMVoidTypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(voidTy, NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef none = LLVMConstNull(LLVMTokenTypeInContext(context));

    LLVMBuildCatchSwitch(builder, none, entry, 0, "cs");

    return 0;
}
