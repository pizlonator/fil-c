/* SHA-NI through the LLVM JIT: llvm.x86.sha256msg1 (SHA256MSG1) called from
   JITed code. The JIT compiles host-native, so no target attributes are
   needed on the JITed side; the run-tests needsSHANI predicate keeps this
   off hosts without the Intel SHA extensions. The expected result is
   computed on the host with _mm_sha256msg1_epu32 under
   __attribute__((target("sha"))) — the recon verified this bit-exact (and
   note that SHA256MSG1 takes sigma0 of the FIRST operand's next dword for
   lanes 0-2, and sigma0 of the SECOND operand's dword 0 for lane 3; the host
   intrinsic models the hardware exactly, which is why it is used for the
   expectation rather than a hand-derived formula).
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

#define LANES 4

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

static __attribute__((noinline)) void (*buildShaFn(void))(unsigned*, unsigned*, unsigned*)
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
    LLVMTypeRef callType;
    LLVMValueRef intrinsic = declareIntrinsic(
        module, context, "llvm.x86.sha256msg1", NULL, 0, &callType);
    LLVMValueRef a = LLVMBuildLoad2(builder, vecType, LLVMGetParam(function, 0), "a");
    LLVMValueRef b = LLVMBuildLoad2(builder, vecType, LLVMGetParam(function, 1), "b");
    LLVMValueRef args[2] = { a, b };
    LLVMValueRef res = LLVMBuildCall2(builder, callType, intrinsic, args, 2, "msg1");
    LLVMBuildStore(builder, res, LLVMGetParam(function, 2));
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    return (void (*)(unsigned*, unsigned*, unsigned*))
        FilLLVMJIT_GetPointerToGlobal(compiled, "test");
}

__attribute__((target("sha")))
static void hostSha256msg1(const unsigned* a, const unsigned* b, unsigned* out)
{
    __m128i va = _mm_loadu_si128((const __m128i*)a);
    __m128i vb = _mm_loadu_si128((const __m128i*)b);
    _mm_storeu_si128((__m128i*)out, _mm_sha256msg1_epu32(va, vb));
}

int main()
{
    void (*test)(unsigned*, unsigned*, unsigned*) = buildShaFn();

    _Alignas(64) unsigned a[LANES];
    _Alignas(64) unsigned b[LANES];
    _Alignas(64) unsigned c[LANES];
    _Alignas(64) unsigned want[LANES];
    int i;
    for (i = 0; i < LANES; ++i) {
        a[i] = 0x01020304u + (unsigned)i;
        b[i] = 0xdead0000u + (unsigned)i;
    }

    test((unsigned*)opaque(a), (unsigned*)opaque(b), (unsigned*)opaque(c));

    hostSha256msg1((const unsigned*)opaque(a), (const unsigned*)opaque(b),
                   (unsigned*)opaque(want));

    if (memcmp(c, want, LANES * 4)) {
        for (i = 0; i < LANES; ++i)
            fprintf(stderr, "lane %d: %08x, expected %08x\n", i, c[i], want[i]);
        return 1;
    }

    printf("jit simd shani ok\n");
    return 0;
}
