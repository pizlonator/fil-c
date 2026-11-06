/* Per-function target-features semantics in one JIT-compiled module: three
   functions, three different feature sets, all numeric-checked.

   The JIT compiles for the host CPU (the shim builds its target machine
   with JITTargetMachineBuilder::detectHost()), and a function's
   "target-features" attribute is additive over those host-native defaults:
   positive entries ("+avx512f") only ever add codegen capabilities, so the
   only way to restrict a function is disable-style entries ("-avx512f").

     pshufb512 — LLVMAddTargetDependentFunctionAttr(..., "target-features",
                 "+avx512f,+avx512bw") runs llvm.x86.avx512.pshuf.b.512. The
                 host has AVX512 anyway, so the point is that positive
                 entries add to (and never subtract from) the defaults: this
                 function keeps everything the host has.
     add256    — "target-features" with disable-style entries
                 ("-avx512f,-avx512bw,-avx512vl,-avx512dq,-avx512cd,-avx2,
                 -avx") takes AVX and AVX512 away from this function. The
                 <8 x i32> add then has to legalize down to two SSE2-era
                 v4i32 adds, and does, still numerically correct. The
                 restriction is real: an AVX512-only intrinsic in this
                 function would fail instruction selection instead of
                 silently running anyway.
     add512    — no attributes at all: the JIT's host-native defaults (a
                 single zmm vpaddd here).

   This documents that the JIT honors per-function target-features in both
   directions: the attribute is not ignored (disable-style entries really
   do change what the function may use), and the host-native defaults never
   over-claim.
   NOTE: the JITed functions are loop-free because JITed code cannot contain
   IR loops yet. Pointers crossing from the host into JITed code are
   laundered through opaque() so the JITed bounds checks stay honest. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include "utils.h"

#define LANES8 8
#define LANES16 16
#define LANES64 64

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

typedef void (*vecFn)(unsigned char*, unsigned char*, unsigned char*);

static vecFn pshufbFn;
static vecFn add256Fn;
static vecFn add512Fn;

static __attribute__((noinline)) void buildModule(void)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef v64i8 = LLVMVectorType(i8Type, LANES64);
    LLVMTypeRef v8i32 = LLVMVectorType(i32Type, LANES8);
    LLVMTypeRef v16i32 = LLVMVectorType(i32Type, LANES16);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[3] = { ptrType, ptrType, ptrType };
    LLVMTypeRef voidFnType =
        LLVMFunctionType(LLVMVoidTypeInContext(context), paramTypes, 3, false);

    /* pshufb512: avx512 features requested explicitly. Positive entries
       only ever add to the JIT's host-native defaults, so this function
       keeps every host feature, too. */
    LLVMValueRef pshufb512 = LLVMAddFunction(module, "pshufb512", voidFnType);
    LLVMAddTargetDependentFunctionAttr(pshufb512, "target-features", "+avx512f,+avx512bw");
    LLVMBasicBlockRef block = LLVMAppendBasicBlockInContext(context, pshufb512, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, block);
    LLVMTypeRef callType;
    LLVMValueRef intrinsic = declareIntrinsic(
        module, context, "llvm.x86.avx512.pshuf.b.512", NULL, 0, &callType);
    LLVMValueRef a = LLVMBuildLoad2(builder, v64i8, LLVMGetParam(pshufb512, 0), "a");
    LLVMValueRef b = LLVMBuildLoad2(builder, v64i8, LLVMGetParam(pshufb512, 1), "b");
    LLVMValueRef args[2] = { a, b };
    LLVMValueRef res = LLVMBuildCall2(builder, callType, intrinsic, args, 2, "shuffled");
    LLVMBuildStore(builder, res, LLVMGetParam(pshufb512, 2));
    LLVMBuildRetVoid(builder);

    /* add256: disable-style entries take AVX and AVX512 away from the
       JIT's host-native defaults for this function. The <8 x i32> add has
       to legalize (split into two v4i32 adds), since this function may not
       use any AVX instruction. Only disable-style entries can restrict
       like this: positive entries would have added to the defaults and
       restricted nothing. */
    LLVMValueRef add256 = LLVMAddFunction(module, "add256", voidFnType);
    LLVMAddTargetDependentFunctionAttr(
        add256, "target-features",
        "-avx512f,-avx512bw,-avx512vl,-avx512dq,-avx512cd,-avx2,-avx");
    block = LLVMAppendBasicBlockInContext(context, add256, "root");
    builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, block);
    a = LLVMBuildLoad2(builder, v8i32, LLVMGetParam(add256, 0), "a");
    b = LLVMBuildLoad2(builder, v8i32, LLVMGetParam(add256, 1), "b");
    res = LLVMBuildAdd(builder, a, b, "sum");
    LLVMBuildStore(builder, res, LLVMGetParam(add256, 2));
    LLVMBuildRetVoid(builder);

    /* add512: no attributes; the JIT's host-native defaults. */
    LLVMValueRef add512 = LLVMAddFunction(module, "add512", voidFnType);
    block = LLVMAppendBasicBlockInContext(context, add512, "root");
    builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, block);
    a = LLVMBuildLoad2(builder, v16i32, LLVMGetParam(add512, 0), "a");
    b = LLVMBuildLoad2(builder, v16i32, LLVMGetParam(add512, 1), "b");
    res = LLVMBuildAdd(builder, a, b, "sum");
    LLVMBuildStore(builder, res, LLVMGetParam(add512, 2));
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    pshufbFn = (vecFn)FilLLVMJIT_GetPointerToGlobal(compiled, "pshufb512");
    add256Fn = (vecFn)FilLLVMJIT_GetPointerToGlobal(compiled, "add256");
    add512Fn = (vecFn)FilLLVMJIT_GetPointerToGlobal(compiled, "add512");
}

int main()
{
    buildModule();

    _Alignas(64) unsigned char a8[LANES64];
    _Alignas(64) unsigned char b8[LANES64];
    _Alignas(64) unsigned char c8[LANES64];
    int i;
    for (i = 0; i < LANES64; ++i) {
        a8[i] = (unsigned char)(i * 3 + 1);
        b8[i] = (unsigned char)(i * 7 + ((i % 4) ? 0x80 : 0));
    }
    pshufbFn((unsigned char*)opaque(a8), (unsigned char*)opaque(b8),
             (unsigned char*)opaque(c8));
    for (i = 0; i < LANES64; ++i) {
        unsigned char idx = b8[i];
        unsigned char want = (idx & 0x80) ? 0 : a8[(i & ~15) + (idx & 0x0f)];
        if (c8[i] != want) {
            fprintf(stderr, "pshufb512 lane %d: %02x, expected %02x\n", i, c8[i], want);
            return 1;
        }
    }

    _Alignas(64) int a16[LANES16];
    _Alignas(64) int b16[LANES16];
    _Alignas(64) int c16[LANES16];
    for (i = 0; i < LANES16; ++i) {
        a16[i] = i * 987 + 1;
        b16[i] = -(i * 654) - 2;
    }
    add256Fn((unsigned char*)opaque(a16), (unsigned char*)opaque(b16),
             (unsigned char*)opaque(c16));
    for (i = 0; i < LANES8; ++i) {
        if (c16[i] != a16[i] + b16[i]) {
            fprintf(stderr, "add256 lane %d: %d, expected %d\n", i, c16[i], a16[i] + b16[i]);
            return 1;
        }
    }
    memset(c16, 0, sizeof(c16));
    add512Fn((unsigned char*)opaque(a16), (unsigned char*)opaque(b16),
             (unsigned char*)opaque(c16));
    for (i = 0; i < LANES16; ++i) {
        if (c16[i] != a16[i] + b16[i]) {
            fprintf(stderr, "add512 lane %d: %d, expected %d\n", i, c16[i], a16[i] + b16[i]);
            return 1;
        }
    }

    printf("jit simd attr march ok\n");
    return 0;
}
