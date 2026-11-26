#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* A getelementptr with zero indices is legal LLVM IR: getIndexedTypeInternal
   (llvm/lib/IR/Instructions.cpp) returns the source element type for an empty
   index list, so nothing aborts at creation, and the zero-index gep
   verifies and compiles (probed: a zero-index constant gep and a zero-index
   gep instruction both survive a full JIT compile).  This pins that the GEP
   index walk does not over-reject zero indices. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(context);
    LLVMTypeRef fieldTys[3] = { i32, i64, i32 };
    LLVMTypeRef structTy = LLVMStructTypeInContext(context, fieldTys, 3, false);
    LLVMValueRef global = LLVMAddGlobal(module, structTy, "g");
    LLVMSetInitializer(global, LLVMConstNull(structTy));
    LLVMValueRef zeroIndices = LLVMConstGEP2(structTy, global, NULL, 0);
    if (!zeroIndices ||
        LLVMGetTypeKind(LLVMTypeOf(zeroIndices)) != LLVMPointerTypeKind) {
        fprintf(stderr, "bad: zero-index const gep\n");
        abort();
    }

    LLVMTypeRef fnTy = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", fnTy);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef slot = LLVMBuildAlloca(builder, structTy, "");
    LLVMValueRef gep = LLVMBuildGEP2(builder, structTy, slot, NULL, 0, "");
    LLVMValueRef first = LLVMBuildStructGEP2(builder, structTy, gep, 0, "");
    LLVMBuildRet(builder, LLVMBuildLoad2(builder, i32, first, ""));

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(void) = FilLLVMJIT_GetPointerToGlobal(result, "test");
    if (!test || test() != 0) {
        fprintf(stderr, "bad: wrong result\n");
        abort();
    }

    printf("const gep zero indices ok\n");
    return 0;
}
