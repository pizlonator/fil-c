/* BFloat16 conversion through the LLVM JIT: llvm.x86.avx512bf16.cvtneps2bf16.512
   (VCVTNEPS2BF16) converts a <16 x float> to a <16 x bfloat> with round-to-
   nearest-even. The JIT compiles host-native, so no target attributes are
   needed on the JITed side; the run-tests needsAVX512BF16 predicate keeps
   this off hosts without AVX512-BF16 (note that the CPUID bit lives in leaf
   7 subleaf 1 EAX, which has_avx512bf16.c knows about). The expected result
   is computed on the host with _mm512_cvtneps_pbh under
   __attribute__((target("avx512bf16"))), which the recon verified bit-exact.
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

#define LANES 16

/* Looks up name, fails loudly (exit 1) if it does not resolve, and declares
   it in module. Tys are passed iff the intrinsic is overloaded. The
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

static __attribute__((noinline)) void (*buildCvtFn(void))(float*, unsigned short*)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef f32Type = LLVMFloatTypeInContext(context);
    LLVMTypeRef bf16Type = LLVMBFloatTypeInContext(context);
    LLVMTypeRef inVecType = LLVMVectorType(f32Type, LANES);
    LLVMTypeRef outVecType = LLVMVectorType(bf16Type, LANES);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[2] = { ptrType, ptrType };
    LLVMTypeRef functionType =
        LLVMFunctionType(LLVMVoidTypeInContext(context), paramTypes, 2, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMTypeRef callType;
    LLVMValueRef intrinsic = declareIntrinsic(
        module, context, "llvm.x86.avx512bf16.cvtneps2bf16.512", NULL, 0, &callType);
    LLVMValueRef a = LLVMBuildLoad2(builder, inVecType, LLVMGetParam(function, 0), "a");
    LLVMValueRef args[1] = { a };
    LLVMValueRef res = LLVMBuildCall2(builder, callType, intrinsic, args, 1, "bf16");
    LLVMBuildStore(builder, res, LLVMGetParam(function, 1));
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    return (void (*)(float*, unsigned short*))FilLLVMJIT_GetPointerToGlobal(compiled, "test");
}

__attribute__((target("avx512bf16")))
static void hostCvt(const float* a, unsigned short* out)
{
    __m512 va = _mm512_loadu_ps(a);
    __m256 vb = _mm512_cvtneps_pbh(va);
    _mm256_storeu_si256((__m256i*)out, vb);
}

int main()
{
    void (*test)(float*, unsigned short*) = buildCvtFn();

    _Alignas(64) float a[LANES];
    _Alignas(64) unsigned short c[LANES];
    _Alignas(64) unsigned short want[LANES];
    int i;
    /* Values with a low mantissa bit that forces RNE to round both up and
       down depending on the bit above it. */
    for (i = 0; i < LANES; ++i)
        a[i] = 1.0f + i * 0.25f + (i % 2) * 0.001f;

    test((float*)opaque(a), (unsigned short*)opaque(c));

    hostCvt((const float*)opaque(a), (unsigned short*)opaque(want));

    if (memcmp(c, want, LANES * 2)) {
        for (i = 0; i < LANES; ++i)
            fprintf(stderr, "lane %d: %04x, expected %04x\n", i, c[i], want[i]);
        return 1;
    }

    printf("jit simd avx512bf16 ok\n");
    return 0;
}
