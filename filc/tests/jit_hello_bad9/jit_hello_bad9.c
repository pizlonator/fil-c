#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef int32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef functionType = LLVMFunctionType(int32Type, NULL, 0, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef alloca = LLVMBuildAlloca(builder, int32Type, "slot");

    /* LLVMValueAsMetadata of an instruction (or argument) is function-local
       metadata, and LLVMMetadataAsValue wraps it in a metadata-as-value.
       LLVMMDNodeInContext rejects that even when it is the only element
       (Core.cpp asserts "Unexpected function-local metadata outside of
       direct argument to call" independently of the element count).  A raw
       value must be passed directly instead.  This must panic in the
       wrapper instead of tripping an LLVM assertion. */
    LLVMMetadataRef md = LLVMValueAsMetadata(alloca);
    LLVMValueRef mav = LLVMMetadataAsValue(context, md);
    LLVMValueRef vals[1] = { mav };
    LLVMMDNodeInContext(context, vals, 1);

    return 0;
}
