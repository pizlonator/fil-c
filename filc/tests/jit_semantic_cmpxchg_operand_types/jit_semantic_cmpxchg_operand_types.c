#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* LLVMBuildAtomicCmpXchg with an i32 comparand and an i64 new value.
   AtomicCmpXchgInst::Init asserts "Cmp type and NewVal type must be same!"
   (llvm/lib/IR/Instructions.cpp), so the wrapper's same-type check rejects
   it before LLVM sees it. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMValueRef fn = LLVMAddFunction(module, "test",
        LLVMFunctionType(LLVMVoidTypeInContext(context), NULL, 0, false));
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef slot = LLVMBuildAlloca(builder, i32, "slot");
    LLVMBuildAtomicCmpXchg(builder, slot, LLVMConstInt(i32, 0, false),
                           LLVMConstInt(i64, 0, false),
                           LLVMAtomicOrderingMonotonic,
                           LLVMAtomicOrderingMonotonic, false);

    return 0;
}
