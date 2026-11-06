/* Baseline SIMD through the LLVM JIT: plain vector adds of <2 x i32>,
   <4 x i32>, and a <2 x double> FAdd, with no intrinsics and no target
   attributes at all. The JIT compiles host-native (its shim does the host
   feature detection for us), so even baseline SSE vector math Just Works
   on any x86_64 host.
   NOTE: the JITed functions are loop-free (unrolled lanes) because JITed
   code cannot contain IR loops yet — loop back-edges need a
   filc_pollcheck_slow safepoint that the JIT does not currently provide.
   Pointers crossing from the host into JITed code are laundered through
   opaque() so the JITed bounds checks stay honest. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdfil.h>
#include "utils.h"

#define LANES4 4
#define LANES2 2

static __attribute__((noinline)) void (*buildAddVec4Fn(void))(int*, int*, int*)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef vecType = LLVMVectorType(i32Type, LANES4);
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

static __attribute__((noinline)) void (*buildAddVec2Fn(void))(int*, int*, int*)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef vecType = LLVMVectorType(i32Type, LANES2);
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

static __attribute__((noinline)) void (*buildFAddVec2Fn(void))(double*, double*, double*)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef f64Type = LLVMDoubleTypeInContext(context);
    LLVMTypeRef vecType = LLVMVectorType(f64Type, LANES2);
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
    return (void (*)(double*, double*, double*))FilLLVMJIT_GetPointerToGlobal(compiled, "test");
}

int main()
{
    void (*addVec4)(int*, int*, int*) = buildAddVec4Fn();
    void (*addVec2)(int*, int*, int*) = buildAddVec2Fn();
    void (*faddVec2)(double*, double*, double*) = buildFAddVec2Fn();

    _Alignas(64) int a4[LANES4];
    _Alignas(64) int b4[LANES4];
    _Alignas(64) int c4[LANES4];
    _Alignas(64) int a2[LANES2];
    _Alignas(64) int b2[LANES2];
    _Alignas(64) int c2[LANES2];
    _Alignas(64) double da2[LANES2];
    _Alignas(64) double db2[LANES2];
    _Alignas(64) double dc2[LANES2];
    int i;
    for (i = 0; i < LANES4; ++i) {
        a4[i] = i * 3 + 7;
        b4[i] = -(i * 5 + 11);
    }
    for (i = 0; i < LANES2; ++i) {
        a2[i] = 1000 + i;
        b2[i] = 333 * (i + 1);
        da2[i] = 1.5 + i;
        db2[i] = -0.25 - i * 0.125;
    }

    addVec4((int*)opaque(a4), (int*)opaque(b4), (int*)opaque(c4));
    for (i = 0; i < LANES4; ++i) {
        if (c4[i] != a4[i] + b4[i]) {
            fprintf(stderr, "vec4 lane %d: %d, expected %d\n", i, c4[i], a4[i] + b4[i]);
            return 1;
        }
    }

    addVec2((int*)opaque(a2), (int*)opaque(b2), (int*)opaque(c2));
    for (i = 0; i < LANES2; ++i) {
        if (c2[i] != a2[i] + b2[i]) {
            fprintf(stderr, "vec2 lane %d: %d, expected %d\n", i, c2[i], a2[i] + b2[i]);
            return 1;
        }
    }

    faddVec2((double*)opaque(da2), (double*)opaque(db2), (double*)opaque(dc2));
    for (i = 0; i < LANES2; ++i) {
        if (dc2[i] != da2[i] + db2[i]) {
            fprintf(stderr, "fvec2 lane %d: %f, expected %f\n", i, dc2[i], da2[i] + db2[i]);
            return 1;
        }
    }

    printf("jit simd baseline ok\n");
    return 0;
}
