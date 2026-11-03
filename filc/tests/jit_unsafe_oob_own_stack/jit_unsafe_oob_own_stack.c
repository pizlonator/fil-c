/* Out-of-bounds access to the JITed function's OWN stack alloca: the JITed
   function allocas an int[2] and loads through a gep whose index is a
   parameter. A runtime index cannot hide in allocation slack (that caveat
   only applies to constant-index overruns), so index 7 must panic with
   "cannot read pointer with ptr >= upper." and exit 42. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

static __attribute__((noinline)) int (*buildFn(void))(int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef paramTypes[1] = { i32Type };
    LLVMTypeRef functionType = LLVMFunctionType(i32Type, paramTypes, 1, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMTypeRef arrayType = LLVMArrayType(i32Type, 2);
    LLVMValueRef buf = LLVMBuildAlloca(builder, arrayType, "buf");
    LLVMValueRef i = LLVMGetParam(function, 0);
    LLVMValueRef zero = LLVMConstInt(i32Type, 0, false);
    LLVMValueRef indices[2] = { zero, i };
    LLVMValueRef elemPtr = LLVMBuildGEP2(
        builder, arrayType, buf, indices, 2, "p");
    LLVMValueRef loaded = LLVMBuildLoad2(builder, i32Type, elemPtr, "v");
    LLVMBuildRet(builder, loaded);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    int (*test)(int) = buildFn();
    int i = (int)(intptr_t)opaque((void*)(intptr_t)7);
    /* buf[i] with i = 7 is 28 bytes past the start of the 8-byte alloca. */
    test(i);
    fprintf(stderr, "should have panicked\n");
    return 1;
}
