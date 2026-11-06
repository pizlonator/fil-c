/* AVX512 down-conversion through the LLVM JIT: llvm.x86.avx512.mask.pmov.db.512
   (VPMOVDB) truncates a <16 x i32> to a <16 x i8>, with a per-byte write
   mask. Here the mask is all-ones and the passthrough operand is all-zeros,
   so every lane is written with the truncated value. The JIT compiles
   host-native, so no target attributes are needed; the run-tests needsAVX512
   predicate keeps this off hosts without AVX512.
   NOTE: the JITed function is loop-free because JITed code cannot contain IR
   loops yet. Pointers crossing from the host into JITed code are laundered
   through opaque() so the JITed bounds checks stay honest. */

#include <filc_llvm_jit.h>
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

static __attribute__((noinline)) void (*buildPmovdbFn(void))(int*, unsigned char*)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef i16Type = LLVMInt16TypeInContext(context);
    LLVMTypeRef inVecType = LLVMVectorType(LLVMInt32TypeInContext(context), LANES);
    LLVMTypeRef outVecType = LLVMVectorType(i8Type, LANES);
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
        module, context, "llvm.x86.avx512.mask.pmov.db.512", NULL, 0, &callType);
    LLVMValueRef a = LLVMBuildLoad2(builder, inVecType, LLVMGetParam(function, 0), "a");
    LLVMValueRef args[3] = {
        a,
        LLVMConstNull(outVecType),   /* passthrough: nothing bleeds through */
        LLVMConstAllOnes(i16Type)    /* 16-bit write mask, every lane enabled */
    };
    LLVMValueRef res = LLVMBuildCall2(builder, callType, intrinsic, args, 3, "packed");
    LLVMBuildStore(builder, res, LLVMGetParam(function, 1));
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    return (void (*)(int*, unsigned char*))FilLLVMJIT_GetPointerToGlobal(compiled, "test");
}

int main()
{
    void (*test)(int*, unsigned char*) = buildPmovdbFn();

    _Alignas(64) int a[LANES];
    _Alignas(64) unsigned char c[LANES];
    int i;
    /* Values that cross the byte-truncation boundary in both directions. */
    for (i = 0; i < LANES; ++i)
        a[i] = (i - LANES / 2) * 1000 + i;

    test((int*)opaque(a), (unsigned char*)opaque(c));

    for (i = 0; i < LANES; ++i) {
        unsigned char want = (unsigned char)(a[i] & 0xff);
        if (c[i] != want) {
            fprintf(stderr, "lane %d: %d, expected %d\n", i, (int)c[i], (int)want);
            return 1;
        }
    }

    printf("jit simd avx512 pmovdb ok\n");
    return 0;
}
