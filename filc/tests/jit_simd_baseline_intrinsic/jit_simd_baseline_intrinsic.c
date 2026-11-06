/* Baseline x86 intrinsic through the LLVM JIT: llvm.x86.sse2.packssdw.128
   (SSE2 is baseline on x86_64, so no target attributes are needed — the JIT
   compiles host-native and every x86_64 host has SSE2).
   NOTE: the originally planned llvm.x86.sse.cvtps2pd does not exist in this
   LLVM (the SSE/SSE2 FP-conversion intrinsics became generic IR ops), so
   packssdw.128 stands in as the baseline x86 intrinsic; its availability is
   confirmed by LLVMLookupIntrinsicID at run time.
   NOTE: the JITed function is loop-free because JITed code cannot contain IR
   loops yet. Pointers crossing from the host into JITed code are laundered
   through opaque() so the JITed bounds checks stay honest. */

#include <filc_llvm_jit.h>
#include <immintrin.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include "utils.h"

#define INLANES 4
#define OUTLANES 8

/* Looks up name, fails loudly (exit 1) if it does not resolve, and declares
   it in module.  Tys are passed iff the intrinsic is overloaded.  The
   intrinsic's FunctionType (for LLVMBuildCall2) is stored through
   outFunctionType. */
static LLVMValueRef declareIntrinsic(LLVMModuleRef module, LLVMContextRef context,
                                     const char* name, LLVMTypeRef* tys, unsigned nTys,
                                     LLVMTypeRef* outFunctionType)
{
    unsigned id = LLVMLookupIntrinsicID(name, strlen(name));
    if (!id) {
        fprintf(stderr, "no intrinsic named %s\n", name);
        exit(1);
    }
    LLVMBool overloaded = LLVMIntrinsicIsOverloaded(id);
    *outFunctionType = LLVMIntrinsicGetType(
        context, id, overloaded ? tys : NULL, overloaded ? (size_t)nTys : 0);
    return LLVMGetIntrinsicDeclaration(
        module, id, overloaded ? tys : NULL, overloaded ? (size_t)nTys : 0);
}

static __attribute__((noinline)) void (*buildPackFn(void))(int*, int*, short*)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef i16Type = LLVMInt16TypeInContext(context);
    LLVMTypeRef inVecType = LLVMVectorType(i32Type, INLANES);
    LLVMTypeRef outVecType = LLVMVectorType(i16Type, OUTLANES);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[3] = { ptrType, ptrType, ptrType };
    LLVMTypeRef functionType =
        LLVMFunctionType(LLVMVoidTypeInContext(context), paramTypes, 3, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMTypeRef callType;
    LLVMValueRef intrinsic = declareIntrinsic(
        module, context, "llvm.x86.sse2.packssdw.128", NULL, 0, &callType);
    LLVMValueRef a = LLVMBuildLoad2(builder, inVecType, LLVMGetParam(function, 0), "a");
    LLVMValueRef b = LLVMBuildLoad2(builder, inVecType, LLVMGetParam(function, 1), "b");
    LLVMValueRef args[2] = { a, b };
    LLVMValueRef res = LLVMBuildCall2(builder, callType, intrinsic, args, 2, "packed");
    LLVMBuildStore(builder, res, LLVMGetParam(function, 2));
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    return (void (*)(int*, int*, short*))FilLLVMJIT_GetPointerToGlobal(compiled, "test");
}

__attribute__((target("sse2")))
static void hostPack(const int* a, const int* b, short* out)
{
    __m128i va = _mm_loadu_si128((const __m128i*)a);
    __m128i vb = _mm_loadu_si128((const __m128i*)b);
    _mm_storeu_si128((__m128i*)out, _mm_packs_epi32(va, vb));
}

int main()
{
    void (*test)(int*, int*, short*) = buildPackFn();

    _Alignas(64) int a[INLANES];
    _Alignas(64) int b[INLANES];
    _Alignas(64) short c[OUTLANES];
    _Alignas(64) short want[OUTLANES];
    int i;
    /* Values that exercise the i16 saturation on both sides. */
    for (i = 0; i < INLANES; ++i) {
        a[i] = (i == 0) ? 40000 : (i == 1) ? -40000 : (i == 2) ? 32767 : -32768;
        b[i] = (i == 0) ? 12345 : (i == 1) ? -1 : (i == 2) ? 32768 : -32769;
    }

    test((int*)opaque(a), (int*)opaque(b), (short*)opaque(c));

    hostPack((const int*)opaque(a), (const int*)opaque(b), (short*)opaque(want));

    if (memcmp(c, want, sizeof(c))) {
        for (i = 0; i < OUTLANES; ++i)
            fprintf(stderr, "lane %d: %d, expected %d\n", i, (int)c[i], (int)want[i]);
        return 1;
    }

    printf("jit simd baseline intrinsic ok\n");
    return 0;
}
