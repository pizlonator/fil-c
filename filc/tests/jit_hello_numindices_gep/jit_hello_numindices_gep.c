#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

static void bad(const char* what, unsigned count)
{
    fprintf(stderr, "bad %s: %u\n", what, count);
    abort();
}

/* A getelementptr constant expression is legal input for LLVMGetNumIndices:
   Core.cpp dispatches on llvm::GEPOperator (llvm-c/Core.h even says "NB: This
   also works on GEP operators"), and Operator.h's ConcreteOperator classof
   matches a ConstantExpr whose opcode is LLVMGetElementPtr.  This pins that
   the LLVMGetNumIndices check does not over-reject GEP constant expressions.
   The index is nonzero because LLVM folds an all-zero-index GEP into its base
   pointer (ConstantFoldGetElementPtr), which would leave no constant
   expression to test. */
static void gepConstantExpr(void)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef int32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef arrayType = LLVMArrayType(int32Type, 4);
    LLVMValueRef global = LLVMAddGlobal(module, arrayType, "g");

    LLVMValueRef index = LLVMConstInt(int32Type, 1, 0);
    LLVMValueRef gep = LLVMConstGEP2(arrayType, global, &index, 1);
    if (gep == NULL)
        bad("GEP constant expression", 0);
    if (!LLVMIsAConstantExpr(gep))
        bad("GEP constant expression kind", 0);
    if (LLVMGetConstOpcode(gep) != LLVMGetElementPtr)
        bad("GEP constant expression opcode",
            (unsigned)LLVMGetConstOpcode(gep));
    if (LLVMGetNumIndices(gep) != 1)
        bad("GEP constant expression index count", LLVMGetNumIndices(gep));
}

/* A getelementptr instruction is legal input for LLVMGetNumIndices too. */
static void gepInstruction(void)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef int32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef functionType = LLVMFunctionType(int32Type, NULL, 0, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef slot = LLVMBuildAlloca(builder, int32Type, "slot");

    LLVMValueRef index = LLVMConstInt(int32Type, 0, 0);
    LLVMValueRef gep = LLVMBuildGEP2(builder, int32Type, slot, &index, 1, "gep");
    if (LLVMGetNumIndices(gep) != 1)
        bad("GEP instruction index count", LLVMGetNumIndices(gep));
}

/* extractvalue and insertvalue instructions are the other inputs that
   LLVMGetNumIndices accepts.  The aggregate is loaded from a global because
   the builder folds extractvalue and insertvalue on constant aggregates
   (IRBuilder::CreateExtractValue/CreateInsertValue), which would leave no
   instruction to test. */
static void extractAndInsertValueInst(void)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef int32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef functionType = LLVMFunctionType(int32Type, NULL, 0, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);

    LLVMTypeRef fieldTypes[2] = { int32Type, int32Type };
    LLVMTypeRef structType = LLVMStructTypeInContext(context, fieldTypes, 2, false);
    LLVMValueRef global = LLVMAddGlobal(module, structType, "g");
    LLVMValueRef agg = LLVMBuildLoad2(builder, structType, global, "agg");

    LLVMValueRef extract = LLVMBuildExtractValue(builder, agg, 1, "extract");
    if (LLVMGetNumIndices(extract) != 1)
        bad("extractvalue index count", LLVMGetNumIndices(extract));

    LLVMValueRef value = LLVMConstInt(int32Type, 42, 0);
    LLVMValueRef insert = LLVMBuildInsertValue(builder, agg, value, 1, "insert");
    if (LLVMGetNumIndices(insert) != 1)
        bad("insertvalue index count", LLVMGetNumIndices(insert));
}

int main()
{
    gepConstantExpr();
    gepInstruction();
    extractAndInsertValueInst();

    printf("gep constexpr numindices ok\n");
    return 0;
}
