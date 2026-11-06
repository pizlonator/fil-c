/* 256-bit SIMD through the LLVM JIT: <8 x i32> add and <8 x float> FAdd.
   No target attributes are needed: the JIT compiles host-native (its shim
   does the host feature detection for us), so on an AVX2 host these become
   single vpaddd/vaddps ymm instructions. The run-tests needsAVX2 predicate
   keeps this off hosts without AVX2.
   NOTE: the JITed functions are loop-free (unrolled via the builder on the
   host side) because JITed code cannot contain IR loops yet. Pointers
   crossing from the host into JITed code are laundered through opaque() so
   the JITed bounds checks stay honest. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

#define LANES 8

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

static __attribute__((noinline)) void (*buildFAddVecFn(void))(float*, float*, float*)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef f32Type = LLVMFloatTypeInContext(context);
    LLVMTypeRef vecType = LLVMVectorType(f32Type, LANES);
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
    LLVMValueRef sum = LLVMBuildFAdd(builder, a, b, "sum");
    LLVMBuildStore(builder, sum, LLVMGetParam(function, 2));
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    return (void (*)(float*, float*, float*))FilLLVMJIT_GetPointerToGlobal(compiled, "test");
}

int main()
{
    void (*addVec)(int*, int*, int*) = buildAddVecFn();
    void (*faddVec)(float*, float*, float*) = buildFAddVecFn();

    _Alignas(64) int ai[LANES];
    _Alignas(64) int bi[LANES];
    _Alignas(64) int ci[LANES];
    _Alignas(64) float af[LANES];
    _Alignas(64) float bf[LANES];
    _Alignas(64) float cf[LANES];
    int i;
    for (i = 0; i < LANES; ++i) {
        ai[i] = i * 7777 + 3;
        bi[i] = -(i * 5555) - 9;
        /* Chosen so the float sums are exactly representable. */
        af[i] = (float)i * 2.0f + 0.5f;
        bf[i] = (float)(LANES - i) * 4.0f;
    }

    addVec((int*)opaque(ai), (int*)opaque(bi), (int*)opaque(ci));
    for (i = 0; i < LANES; ++i) {
        if (ci[i] != ai[i] + bi[i]) {
            fprintf(stderr, "i lane %d: %d, expected %d\n", i, ci[i], ai[i] + bi[i]);
            return 1;
        }
    }

    faddVec((float*)opaque(af), (float*)opaque(bf), (float*)opaque(cf));
    for (i = 0; i < LANES; ++i) {
        if (cf[i] != af[i] + bf[i]) {
            fprintf(stderr, "f lane %d: %f, expected %f\n", i, (double)cf[i],
                    (double)(af[i] + bf[i]));
            return 1;
        }
    }

    printf("jit simd avx2 vecadd256 ok\n");
    return 0;
}
