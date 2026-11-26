#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMTargetExtTypeInContext with both type and int parameters stays legal,
   including for the one name TargetExtType::checkParams (llvm/lib/IR/
   Type.cpp) actually validates at construction: "riscv.vector.tuple" wants
   exactly one type parameter and one int parameter.  This also exercises the
   snapshot of the caller's int-param array, since the parameters are read
   back below.  TargetExtType::get for a name checkParams rejects (for
   example "aarch64.svcount" with any parameters) cantFails and needs a shim
   predicate, so it is out of scope for now. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef typeParams[1] = { i32 };
    unsigned intParams[1] = { 256 };
    LLVMTypeRef tuple = LLVMTargetExtTypeInContext(context, "riscv.vector.tuple",
                                                   typeParams, 1, intParams, 1);
    if (!tuple || LLVMGetTypeKind(tuple) != LLVMTargetExtTypeKind ||
        LLVMGetTargetExtTypeNumTypeParams(tuple) != 1 ||
        LLVMGetTargetExtTypeNumIntParams(tuple) != 1 ||
        LLVMGetTargetExtTypeIntParam(tuple, 0) != 256) {
        fprintf(stderr, "bad: tuple params did not come back\n");
        abort();
    }
    if (LLVMGetTypeKind(LLVMGetTargetExtTypeTypeParam(tuple, 0)) !=
            LLVMIntegerTypeKind ||
        LLVMGetIntTypeWidth(LLVMGetTargetExtTypeTypeParam(tuple, 0)) != 32) {
        fprintf(stderr, "bad: tuple type param did not come back\n");
        abort();
    }

    unsigned vlsInts[1] = { 128 };
    LLVMTypeRef vls = LLVMTargetExtTypeInContext(context, "arm64.sve.vls",
                                                 NULL, 0, vlsInts, 1);
    if (!vls || LLVMGetTypeKind(vls) != LLVMTargetExtTypeKind ||
        LLVMGetTargetExtTypeNumIntParams(vls) != 1 ||
        LLVMGetTargetExtTypeIntParam(vls, 0) != 128) {
        fprintf(stderr, "bad: vls int params did not come back\n");
        abort();
    }

    printf("target ext params ok\n");
    return 0;
}
