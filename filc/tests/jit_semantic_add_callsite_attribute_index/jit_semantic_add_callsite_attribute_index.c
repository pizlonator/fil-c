#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMAddCallSiteAttribute(call, 0x7FFFFFFF, attr): the index flows into the
   same AttributeList::setAttributesAtIndex path as LLVMAddAttributeAtIndex,
   and 0x7FFFFFFF asks SmallVector to resize to 16GB before subscripting
   (probed: "LLVM ERROR: SmallVector unable to grow"). */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fnTy = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef call = LLVMBuildCall2(builder, fnTy, fn, NULL, 0, "");
    LLVMAddCallSiteAttribute(call, 0x7FFFFFFFu,
                             LLVMCreateEnumAttribute(context, 1, 0));
    return 0;
}
