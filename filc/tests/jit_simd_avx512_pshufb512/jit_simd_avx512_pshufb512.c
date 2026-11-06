/* 512-bit byte shuffle through the LLVM JIT: llvm.x86.avx512.pshuf.b.512 on
   <64 x i8> (the zmm form of VPHUFB) called from JITed code. The JIT
   compiles host-native, so no target attributes are needed on the JITed
   side; the run-tests needsAVX512 predicate keeps this off hosts without
   AVX512 (the instruction is AVX512BW). The expected result is computed on
   the host with _mm512_shuffle_epi8 under __attribute__((target("avx512bw")))
   — note that the shuffle indexes per 128-bit lane, which the host intrinsic
   models exactly.
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

#define LANES 64

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

static __attribute__((noinline)) void (*buildPshufbFn(void))(unsigned char*,
                                                             unsigned char*,
                                                             unsigned char*)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef vecType = LLVMVectorType(i8Type, LANES);
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
        module, context, "llvm.x86.avx512.pshuf.b.512", NULL, 0, &callType);
    LLVMValueRef a = LLVMBuildLoad2(builder, vecType, LLVMGetParam(function, 0), "a");
    LLVMValueRef b = LLVMBuildLoad2(builder, vecType, LLVMGetParam(function, 1), "b");
    LLVMValueRef args[2] = { a, b };
    LLVMValueRef res = LLVMBuildCall2(builder, callType, intrinsic, args, 2, "shuffled");
    LLVMBuildStore(builder, res, LLVMGetParam(function, 2));
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    return (void (*)(unsigned char*, unsigned char*, unsigned char*))
        FilLLVMJIT_GetPointerToGlobal(compiled, "test");
}

__attribute__((target("avx512bw")))
static void hostPshufb512(const unsigned char* a, const unsigned char* b, unsigned char* out)
{
    __m512i va = _mm512_loadu_si512((const __m512i*)a);
    __m512i vb = _mm512_loadu_si512((const __m512i*)b);
    _mm512_storeu_si512((__m512i*)out, _mm512_shuffle_epi8(va, vb));
}

int main()
{
    void (*test)(unsigned char*, unsigned char*, unsigned char*) = buildPshufbFn();

    _Alignas(64) unsigned char a[LANES];
    _Alignas(64) unsigned char b[LANES];
    _Alignas(64) unsigned char c[LANES];
    _Alignas(64) unsigned char want[LANES];
    int i;
    /* Bit 7 of b's bytes controls zeroing, and the low nibble is the index —
       make both vary across lanes. */
    for (i = 0; i < LANES; ++i) {
        a[i] = (unsigned char)(i * 3 + 1);
        b[i] = (unsigned char)(i * 7 + ((i % 4) ? 0x80 : 0));
    }

    test((unsigned char*)opaque(a), (unsigned char*)opaque(b), (unsigned char*)opaque(c));

    hostPshufb512((const unsigned char*)opaque(a), (const unsigned char*)opaque(b),
                  (unsigned char*)opaque(want));

    if (memcmp(c, want, LANES)) {
        for (i = 0; i < LANES; ++i)
            fprintf(stderr, "lane %d: %02x, expected %02x\n", i, c[i], want[i]);
        return 1;
    }

    printf("jit simd avx512 pshufb512 ok\n");
    return 0;
}
