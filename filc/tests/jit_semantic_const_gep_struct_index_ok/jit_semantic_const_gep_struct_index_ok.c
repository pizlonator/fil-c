#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the struct-index walk: in-range two-index and
   nested-struct three-index getelementptrs stay legal, and the walk descends
   through the nested struct ({ i32, { f32, f32 } }) rather than rejecting it.
   The built gep loads back the value it points at. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef f32 = LLVMFloatTypeInContext(context);
    LLVMTypeRef innerFieldTys[2] = { f32, f32 };
    LLVMTypeRef innerTy = LLVMStructTypeInContext(context, innerFieldTys, 2, false);
    LLVMTypeRef fieldTys[2] = { i32, innerTy };
    LLVMTypeRef structTy = LLVMStructTypeInContext(context, fieldTys, 2, false);
    LLVMValueRef fields[2] = { LLVMConstInt(i32, 11, false),
                               LLVMConstNull(innerTy) };
    LLVMValueRef global = LLVMAddGlobal(module, structTy, "g");
    LLVMSetInitializer(global, LLVMConstStructInContext(context, fields, 2, false));

    LLVMValueRef zero = LLVMConstInt(i32, 0, false);
    LLVMValueRef one = LLVMConstInt(i32, 1, false);
    LLVMValueRef two[2] = { zero, one };
    if (!LLVMConstGEP2(structTy, global, two, 2)) {
        fprintf(stderr, "bad: two-index const gep\n");
        abort();
    }
    LLVMValueRef nested[3] = { zero, one, zero };
    LLVMValueRef nestedGep = LLVMConstGEP2(structTy, global, nested, 3);
    if (!nestedGep) {
        fprintf(stderr, "bad: nested const gep\n");
        abort();
    }

    LLVMTypeRef fnTy = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    /* gep inbounds %struct, ptr %g, i32 0, i32 1, i32 0 -> the f32 field. */
    LLVMValueRef indices[3] = { zero, one, zero };
    LLVMValueRef ptr = LLVMBuildGEP2(builder, structTy, global, indices, 3, "");
    LLVMValueRef asI32 = LLVMBuildBitCast(builder, ptr,
                                          LLVMPointerType(i32, 0), "");
    LLVMBuildRet(builder, LLVMBuildLoad2(builder, i32, asI32, ""));

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(result, "test");
    if (!test || test() != 0) {
        fprintf(stderr, "bad: wrong result\n");
        abort();
    }

    printf("const gep struct index ok\n");
    return 0;
}
