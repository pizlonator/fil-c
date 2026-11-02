#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

static void bad(const char* what, unsigned count)
{
    fprintf(stderr, "bad %s: %u\n", what, count);
    abort();
}

/* A single raw value operand is legal for LLVMMDNodeInContext: Core.cpp
   treats it as function-local metadata and returns a metadata-as-value
   wrapping it (which reports one operand).  This pins that the element
   check on LLVMMDNodeInContext does not over-reject the raw-value case. */
static void localMetadata(void)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef int32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef functionType = LLVMFunctionType(int32Type, NULL, 0, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef alloca = LLVMBuildAlloca(builder, int32Type, "slot");

    LLVMValueRef vals[1] = { alloca };
    LLVMValueRef node = LLVMMDNodeInContext(context, vals, 1);
    if (node == NULL)
        bad("local metadata node", 0);
    if (LLVMGetMDNodeNumOperands(node) != 1)
        bad("local metadata operand count",
            LLVMGetMDNodeNumOperands(node));
}

int main()
{
    localMetadata();

    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);

    /* A null element is legal for LLVMMDNodeInContext: the C API maps it to
       null metadata, and the null survives as an operand. */
    LLVMValueRef str = LLVMMDStringInContext(context, "x", 1);
    LLVMValueRef vals[2] = { str, NULL };
    LLVMValueRef node = LLVMMDNodeInContext(context, vals, 2);
    if (node == NULL)
        bad("MDNode", 0);
    if (LLVMGetMDNodeNumOperands(node) != 2)
        bad("MDNode operand count", LLVMGetMDNodeNumOperands(node));

    /* A node whose only element is null is legal too: LLVM canonicalizes an
       MDNode with a single null operand to an empty one. */
    LLVMValueRef onlyNull[1] = { NULL };
    LLVMValueRef emptyNode = LLVMMDNodeInContext(context, onlyNull, 1);
    if (emptyNode == NULL)
        bad("empty MDNode", 0);
    if (LLVMGetMDNodeNumOperands(emptyNode) != 0)
        bad("empty MDNode operand count",
            LLVMGetMDNodeNumOperands(emptyNode));

    /* Nulls are legal for LLVMMDNodeInContext2 as well: they are forwarded
       to MDNode::get, and survive as operands. */
    LLVMMetadataRef m_ds[2] = { NULL, NULL };
    LLVMMetadataRef node2 = LLVMMDNodeInContext2(context, m_ds, 2);
    if (node2 == NULL) {
        fprintf(stderr, "bad MDNode2\n");
        abort();
    }
    if (LLVMGetMDNodeNumOperands(LLVMMetadataAsValue(context, node2)) != 2)
        bad("MDNode2 operand count",
            LLVMGetMDNodeNumOperands(LLVMMetadataAsValue(context, node2)));

    /* A plain enum attribute is legal input for LLVMGetEnumAttributeValue:
       it must read as value 0, not panic. */
    unsigned kindID = LLVMGetEnumAttributeKindForName("alwaysinline", 12);
    LLVMAttributeRef attr = LLVMCreateEnumAttribute(context, kindID, 0);
    if (LLVMGetEnumAttributeValue(attr) != 0)
        bad("enum attribute value", (unsigned)LLVMGetEnumAttributeValue(attr));

    printf("mdnode null ok\n");
    (void)module;
    return 0;
}
