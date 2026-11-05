#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMCreateEnumAttribute is legal for a pure enum kind with value zero and
   for an int kind with a nonzero value (Attribute::get, Attributes.cpp).
   The enum attribute is used on the test function to prove it works. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef fn_ty = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fn_ty);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildRet(builder, LLVMConstInt(i32, 42, false));

    unsigned nounwind = LLVMGetEnumAttributeKindForName("nounwind", 8);
    LLVMAttributeRef enum_attr = LLVMCreateEnumAttribute(context, nounwind, 0);
    unsigned deref = LLVMGetEnumAttributeKindForName("dereferenceable", 15);
    LLVMCreateEnumAttribute(context, deref, 8);
    /* LLVMAttributeFunctionIndex is -1 (llvm-c/Core.h). */
    LLVMAddAttributeAtIndex(fn, (LLVMAttributeIndex)-1, enum_attr);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("enumattr ok\n");
    return 0;
}
