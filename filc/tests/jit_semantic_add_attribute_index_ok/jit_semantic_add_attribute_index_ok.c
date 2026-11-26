#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the attribute-index check: the return index (0), the
   function index (UINT_MAX == LLVMAttributeFunctionIndex), and an in-range
   parameter index (1 on a one-parameter function) all stay legal, on both the
   function and the call-site adders.  A string attribute is used for the
   return and parameter indices because enum attribute kinds have
   per-position restrictions (the verifier rejects, say, allocalign on
   returns) that have nothing to do with the index check under test here. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef paramTys[1] = { i32 };
    LLVMTypeRef fnTy = LLVMFunctionType(i32, paramTys, 1, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMAttributeRef strAttr = LLVMCreateStringAttribute(context, "filc.test", 9, "1", 1);
    unsigned nounwind = LLVMGetEnumAttributeKindForName("nounwind", 8);
    LLVMAttributeRef nounwindAttr = LLVMCreateEnumAttribute(context, nounwind, 0);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef arg = LLVMConstInt(i32, 7, false);
    LLVMValueRef call = LLVMBuildCall2(builder, fnTy, fn, &arg, 1, "");

    LLVMAddAttributeAtIndex(fn, 0, strAttr);
    LLVMAddAttributeAtIndex(fn, (LLVMAttributeIndex)-1, nounwindAttr);
    LLVMAddAttributeAtIndex(fn, 1, strAttr);
    LLVMAddCallSiteAttribute(call, 0, strAttr);
    LLVMAddCallSiteAttribute(call, (LLVMAttributeIndex)-1, nounwindAttr);
    LLVMAddCallSiteAttribute(call, 1, strAttr);

    LLVMBuildRet(builder, arg);
    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    if (!FilLLVMJIT_GetPointerToGlobal(result, "test")) {
        fprintf(stderr, "bad: no test pointer\n");
        abort();
    }

    printf("add attribute index ok\n");
    return 0;
}
