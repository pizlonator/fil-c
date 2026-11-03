/* Type confusion across the JIT boundary: the JITed function receives a
   pointer to a host int (a 4-byte object, whose Fil-C capability is 16 bytes
   wide on both the stack and the heap) and writes 64 bytes of i64 stores
   through it after a bitcast (a no-op on opaque pointers, but the intent is
   explicit). Small confusions hide inside the 16-byte object granularity, so
   this stores well past it; the capability layer catches the oversized write
   with "cannot write 64 bytes when upper - ptr = 16." and exit 42. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

#define STORES 8

static __attribute__((noinline)) void (*buildFn(void))(int*)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i64Type = LLVMInt64TypeInContext(context);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[1] = { ptrType };
    LLVMTypeRef functionType = LLVMFunctionType(
        LLVMVoidTypeInContext(context), paramTypes, 1, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef p = LLVMGetParam(function, 0);
    int k;
    for (k = 0; k < STORES; ++k) {
        LLVMValueRef idx = LLVMConstInt(i64Type, (unsigned long long)k, false);
        LLVMValueRef slot = LLVMBuildGEP2(builder, i64Type, p, &idx, 1, "s");
        LLVMBuildStore(
            builder, LLVMConstInt(i64Type, 0x4142434445464748ull, false), slot);
    }
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    void (*test)(int*) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    void (*test)(int*) = buildFn();
    int* x = (int*)zgc_alloc(sizeof(int));
    *x = 4242;
    test((int*)opaque(x));
    fprintf(stderr, "should have panicked\n");
    return 1;
}

