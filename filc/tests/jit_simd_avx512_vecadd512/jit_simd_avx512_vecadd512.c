/* 512-bit SIMD through the LLVM JIT: <16 x i32> add (a single zmm vpaddd).
   No target attributes are needed: the JIT compiles host-native
   (its shim does the host feature detection for us), so on an AVX512 host
   this is one zmm instruction. The run-tests needsAVX512 predicate keeps
   this off hosts without AVX512.
   NOTE: the JITed function is loop-free because JITed code cannot contain IR
   loops yet. Pointers crossing from the host into JITed code are laundered
   through opaque() so the JITed bounds checks stay honest. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

#define LANES 16

static __attribute__((noinline)) void (*buildAddVecFn(void))(int*, int*, int*)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef vecType = LLVMVectorType(i32Type, LANES);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[3] = { ptrType, ptrType, ptrType };
    LLVMTypeRef functionType =
        LLVMFunctionType(LLVMVoidTypeInContext(context), paramTypes, 3, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef a = LLVMBuildLoad2(builder, vecType, LLVMGetParam(function, 0), "a");
    LLVMValueRef b = LLVMBuildLoad2(builder, vecType, LLVMGetParam(function, 1), "b");
    LLVMValueRef sum = LLVMBuildAdd(builder, a, b, "sum");
    LLVMBuildStore(builder, sum, LLVMGetParam(function, 2));
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    return (void (*)(int*, int*, int*))FilLLVMJIT_GetPointerToGlobal(compiled, "test");
}

int main()
{
    void (*test)(int*, int*, int*) = buildAddVecFn();

    _Alignas(64) int a[LANES];
    _Alignas(64) int b[LANES];
    _Alignas(64) int c[LANES];
    int i;
    for (i = 0; i < LANES; ++i) {
        a[i] = i * 1234567 + 42;
        b[i] = -(i * 7654321) - 1;
    }

    test((int*)opaque(a), (int*)opaque(b), (int*)opaque(c));

    for (i = 0; i < LANES; ++i) {
        if (c[i] != a[i] + b[i]) {
            fprintf(stderr, "lane %d: %d, expected %d\n", i, c[i], a[i] + b[i]);
            return 1;
        }
    }

    printf("jit simd avx512 vecadd512 ok\n");
    return 0;
}
