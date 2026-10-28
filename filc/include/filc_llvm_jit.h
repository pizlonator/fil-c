/*
   Copyright (c) 2026 Filip Pizlo. All Rights Reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
   OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdfil.h>

/* Fil-C LLVM JIT API.

   This header exposes a curated subset of the LLVM C API (IR building plus
   most of Core) together with the FilLLVMJIT* JIT-compile API.  All LLVM*Ref
   pointer types are opaque.  This header is a standalone substitute for the
   llvm-c headers: include this header INSTEAD of <llvm-c/Core.h> et al,
   never both.  It is self-contained (only <stddef.h> and <stdint.h>) and
   deliberately defines its contents unconditionally: it never includes
   <llvm-c/...> and never tests for LLVM_C_*_H.

   Deliberately NOT exposed here:
    - destructive operations bottoming out in eraseFromParent,
      removeFromParent, or delete,
    - LLVMDispose* of any kind,
    - LLVMShutdown and LLVMGetGlobalContext,
    - global-context creators without an LLVMContextRef parameter (use the
      InContext sibling: LLVMModuleCreateWithNameInContext,
      LLVMCreateBuilderInContext, LLVMAppendBasicBlockInContext, and the
      other ...InContext forms),
    - diagnostic-handler and yield-callback operations,
    - APIs returning owned strings that would need LLVMDisposeMessage.

   Object lifetimes are managed by the Fil-C runtime (filc_llvm_jit.c); user
   code never disposes of LLVM objects directly.
 */

#ifndef FILC_LLVM_JIT_H
#define FILC_LLVM_JIT_H

#ifdef __cplusplus
extern "C" {
#endif

#if 0
} /* tell emacs what is up */
#endif

#ifndef __FILC__
#error "This header should only be included by Fil-C code."
#endif

typedef int LLVMBool;
typedef struct LLVMOpaqueMemoryBuffer *LLVMMemoryBufferRef;
typedef struct LLVMOpaqueContext *LLVMContextRef;
typedef struct LLVMOpaqueModule *LLVMModuleRef;
typedef struct LLVMOpaqueType *LLVMTypeRef;
typedef struct LLVMOpaqueValue *LLVMValueRef;
typedef struct LLVMOpaqueBasicBlock *LLVMBasicBlockRef;
typedef struct LLVMOpaqueMetadata *LLVMMetadataRef;
typedef struct LLVMOpaqueNamedMDNode *LLVMNamedMDNodeRef;
typedef struct LLVMOpaqueValueMetadataEntry LLVMValueMetadataEntry;
typedef struct LLVMOpaqueBuilder *LLVMBuilderRef;
typedef struct LLVMOpaqueDIBuilder *LLVMDIBuilderRef;
typedef struct LLVMOpaqueModuleProvider *LLVMModuleProviderRef;
typedef struct LLVMOpaquePassManager *LLVMPassManagerRef;
typedef struct LLVMOpaqueUse *LLVMUseRef;
typedef struct LLVMOpaqueOperandBundle *LLVMOperandBundleRef;
typedef struct LLVMOpaqueAttributeRef *LLVMAttributeRef;
typedef struct LLVMOpaqueDiagnosticInfo *LLVMDiagnosticInfoRef;
typedef struct LLVMComdat *LLVMComdatRef;
typedef struct LLVMOpaqueModuleFlagEntry LLVMModuleFlagEntry;
typedef struct LLVMOpaqueJITEventListener *LLVMJITEventListenerRef;
typedef struct LLVMOpaqueBinary *LLVMBinaryRef;
typedef struct LLVMOpaqueDbgRecord *LLVMDbgRecordRef;

/* Enumerations from llvm-c/Core.h (minus diagnostics). */
typedef enum { LLVMRet = 1, LLVMBr = 2, LLVMSwitch = 3, LLVMIndirectBr = 4, LLVMInvoke = 5, LLVMUnreachable = 7, LLVMCallBr = 67, LLVMFNeg = 66, LLVMAdd = 8, LLVMFAdd = 9, LLVMSub = 10, LLVMFSub = 11, LLVMMul = 12, LLVMFMul = 13, LLVMUDiv = 14, LLVMSDiv = 15, LLVMFDiv = 16, LLVMURem = 17, LLVMSRem = 18, LLVMFRem = 19, LLVMShl = 20, LLVMLShr = 21, LLVMAShr = 22, LLVMAnd = 23, LLVMOr = 24, LLVMXor = 25, LLVMAlloca = 26, LLVMLoad = 27, LLVMStore = 28, LLVMGetElementPtr = 29, LLVMTrunc = 30, LLVMZExt = 31, LLVMSExt = 32, LLVMFPToUI = 33, LLVMFPToSI = 34, LLVMUIToFP = 35, LLVMSIToFP = 36, LLVMFPTrunc = 37, LLVMFPExt = 38, LLVMPtrToInt = 39, LLVMIntToPtr = 40, LLVMBitCast = 41, LLVMAddrSpaceCast = 60, LLVMICmp = 42, LLVMFCmp = 43, LLVMPHI = 44, LLVMCall = 45, LLVMSelect = 46, LLVMUserOp1 = 47, LLVMUserOp2 = 48, LLVMVAArg = 49, LLVMExtractElement = 50, LLVMInsertElement = 51, LLVMShuffleVector = 52, LLVMExtractValue = 53, LLVMInsertValue = 54, LLVMFreeze = 68, LLVMFence = 55, LLVMAtomicCmpXchg = 56, LLVMAtomicRMW = 57, LLVMResume = 58, LLVMLandingPad = 59, LLVMCleanupRet = 61, LLVMCatchRet = 62, LLVMCatchPad = 63, LLVMCleanupPad = 64, LLVMCatchSwitch = 65 } LLVMOpcode;
typedef enum { LLVMVoidTypeKind = 0, LLVMHalfTypeKind = 1, LLVMFloatTypeKind = 2, LLVMDoubleTypeKind = 3, LLVMX86_FP80TypeKind = 4, LLVMFP128TypeKind = 5, LLVMPPC_FP128TypeKind = 6, LLVMLabelTypeKind = 7, LLVMIntegerTypeKind = 8, LLVMFunctionTypeKind = 9, LLVMStructTypeKind = 10, LLVMArrayTypeKind = 11, LLVMPointerTypeKind = 12, LLVMVectorTypeKind = 13, LLVMMetadataTypeKind = 14, LLVMTokenTypeKind = 16, LLVMScalableVectorTypeKind = 17, LLVMBFloatTypeKind = 18, LLVMX86_AMXTypeKind = 19, LLVMTargetExtTypeKind = 20, } LLVMTypeKind;
typedef enum { LLVMExternalLinkage, LLVMAvailableExternallyLinkage, LLVMLinkOnceAnyLinkage, LLVMLinkOnceODRLinkage, LLVMLinkOnceODRAutoHideLinkage, LLVMWeakAnyLinkage, LLVMWeakODRLinkage, LLVMAppendingLinkage, LLVMInternalLinkage, LLVMPrivateLinkage, LLVMDLLImportLinkage, LLVMDLLExportLinkage, LLVMExternalWeakLinkage, LLVMGhostLinkage, LLVMCommonLinkage, LLVMLinkerPrivateLinkage, LLVMLinkerPrivateWeakLinkage } LLVMLinkage;
typedef enum { LLVMDefaultVisibility, LLVMHiddenVisibility, LLVMProtectedVisibility } LLVMVisibility;
typedef enum { LLVMNoUnnamedAddr, LLVMLocalUnnamedAddr, LLVMGlobalUnnamedAddr } LLVMUnnamedAddr;
typedef enum { LLVMDefaultStorageClass = 0, LLVMDLLImportStorageClass = 1, LLVMDLLExportStorageClass = 2 } LLVMDLLStorageClass;
typedef enum { LLVMCCallConv = 0, LLVMFastCallConv = 8, LLVMColdCallConv = 9, LLVMGHCCallConv = 10, LLVMHiPECallConv = 11, LLVMAnyRegCallConv = 13, LLVMPreserveMostCallConv = 14, LLVMPreserveAllCallConv = 15, LLVMSwiftCallConv = 16, LLVMCXXFASTTLSCallConv = 17, LLVMX86StdcallCallConv = 64, LLVMX86FastcallCallConv = 65, LLVMARMAPCSCallConv = 66, LLVMARMAAPCSCallConv = 67, LLVMARMAAPCSVFPCallConv = 68, LLVMMSP430INTRCallConv = 69, LLVMX86ThisCallCallConv = 70, LLVMPTXKernelCallConv = 71, LLVMPTXDeviceCallConv = 72, LLVMSPIRFUNCCallConv = 75, LLVMSPIRKERNELCallConv = 76, LLVMIntelOCLBICallConv = 77, LLVMX8664SysVCallConv = 78, LLVMWin64CallConv = 79, LLVMX86VectorCallCallConv = 80, LLVMHHVMCallConv = 81, LLVMHHVMCCallConv = 82, LLVMX86INTRCallConv = 83, LLVMAVRINTRCallConv = 84, LLVMAVRSIGNALCallConv = 85, LLVMAVRBUILTINCallConv = 86, LLVMAMDGPUVSCallConv = 87, LLVMAMDGPUGSCallConv = 88, LLVMAMDGPUPSCallConv = 89, LLVMAMDGPUCSCallConv = 90, LLVMAMDGPUKERNELCallConv = 91, LLVMX86RegCallCallConv = 92, LLVMAMDGPUHSCallConv = 93, LLVMMSP430BUILTINCallConv = 94, LLVMAMDGPULSCallConv = 95, LLVMAMDGPUESCallConv = 96 } LLVMCallConv;
typedef enum { LLVMArgumentValueKind, LLVMBasicBlockValueKind, LLVMMemoryUseValueKind, LLVMMemoryDefValueKind, LLVMMemoryPhiValueKind, LLVMFunctionValueKind, LLVMGlobalAliasValueKind, LLVMGlobalIFuncValueKind, LLVMGlobalVariableValueKind, LLVMBlockAddressValueKind, LLVMConstantExprValueKind, LLVMConstantArrayValueKind, LLVMConstantStructValueKind, LLVMConstantVectorValueKind, LLVMUndefValueValueKind, LLVMConstantAggregateZeroValueKind, LLVMConstantDataArrayValueKind, LLVMConstantDataVectorValueKind, LLVMConstantIntValueKind, LLVMConstantFPValueKind, LLVMConstantPointerNullValueKind, LLVMConstantTokenNoneValueKind, LLVMMetadataAsValueValueKind, LLVMInlineAsmValueKind, LLVMInstructionValueKind, LLVMPoisonValueValueKind, LLVMConstantTargetNoneValueKind, LLVMConstantPtrAuthValueKind, } LLVMValueKind;
typedef enum { LLVMIntEQ = 32, LLVMIntNE, LLVMIntUGT, LLVMIntUGE, LLVMIntULT, LLVMIntULE, LLVMIntSGT, LLVMIntSGE, LLVMIntSLT, LLVMIntSLE } LLVMIntPredicate;
typedef enum { LLVMRealPredicateFalse, LLVMRealOEQ, LLVMRealOGT, LLVMRealOGE, LLVMRealOLT, LLVMRealOLE, LLVMRealONE, LLVMRealORD, LLVMRealUNO, LLVMRealUEQ, LLVMRealUGT, LLVMRealUGE, LLVMRealULT, LLVMRealULE, LLVMRealUNE, LLVMRealPredicateTrue } LLVMRealPredicate;
typedef enum { LLVMLandingPadCatch, LLVMLandingPadFilter } LLVMLandingPadClauseTy;
typedef enum { LLVMNotThreadLocal = 0, LLVMGeneralDynamicTLSModel, LLVMLocalDynamicTLSModel, LLVMInitialExecTLSModel, LLVMLocalExecTLSModel } LLVMThreadLocalMode;
typedef enum { LLVMAtomicOrderingNotAtomic = 0, LLVMAtomicOrderingUnordered = 1, LLVMAtomicOrderingMonotonic = 2, LLVMAtomicOrderingAcquire = 4, LLVMAtomicOrderingRelease = 5, LLVMAtomicOrderingAcquireRelease = 6, LLVMAtomicOrderingSequentiallyConsistent = 7 } LLVMAtomicOrdering;
typedef enum { LLVMAtomicRMWBinOpXchg, LLVMAtomicRMWBinOpAdd, LLVMAtomicRMWBinOpSub, LLVMAtomicRMWBinOpAnd, LLVMAtomicRMWBinOpNand, LLVMAtomicRMWBinOpOr, LLVMAtomicRMWBinOpXor, LLVMAtomicRMWBinOpMax, LLVMAtomicRMWBinOpMin, LLVMAtomicRMWBinOpUMax, LLVMAtomicRMWBinOpUMin, LLVMAtomicRMWBinOpFAdd, LLVMAtomicRMWBinOpFSub, LLVMAtomicRMWBinOpFMax, LLVMAtomicRMWBinOpFMin, LLVMAtomicRMWBinOpUIncWrap, LLVMAtomicRMWBinOpUDecWrap, LLVMAtomicRMWBinOpUSubCond, LLVMAtomicRMWBinOpUSubSat, } LLVMAtomicRMWBinOp;
typedef enum { LLVMInlineAsmDialectATT, LLVMInlineAsmDialectIntel } LLVMInlineAsmDialect;
typedef enum { LLVMModuleFlagBehaviorError, LLVMModuleFlagBehaviorWarning, LLVMModuleFlagBehaviorRequire, LLVMModuleFlagBehaviorOverride, LLVMModuleFlagBehaviorAppend, LLVMModuleFlagBehaviorAppendUnique, } LLVMModuleFlagBehavior;
typedef enum { LLVMTailCallKindNone = 0, LLVMTailCallKindTail = 1, LLVMTailCallKindMustTail = 2, LLVMTailCallKindNoTail = 3, } LLVMTailCallKind;
typedef unsigned LLVMAttributeIndex;
typedef unsigned LLVMFastMathFlags;
typedef unsigned LLVMGEPNoWrapFlags;

/* Curated LLVM Core C API subset.  No destruction, disposal,
   shutdown, global-context, diagnostic, or yield operations. */
LLVMValueRef LLVMAddAlias2(LLVMModuleRef M, LLVMTypeRef ValueTy, unsigned AddrSpace, LLVMValueRef Aliasee, const char *Name);
void LLVMAddAttributeAtIndex(LLVMValueRef F, LLVMAttributeIndex Idx, LLVMAttributeRef A);
void LLVMAddCallSiteAttribute(LLVMValueRef C, LLVMAttributeIndex Idx, LLVMAttributeRef A);
void LLVMAddCase(LLVMValueRef Switch, LLVMValueRef OnVal, LLVMBasicBlockRef Dest);
void LLVMAddClause(LLVMValueRef LandingPad, LLVMValueRef ClauseVal);
void LLVMAddDestination(LLVMValueRef IndirectBr, LLVMBasicBlockRef Dest);
LLVMValueRef LLVMAddFunction(LLVMModuleRef M, const char *Name, LLVMTypeRef FunctionTy);
LLVMValueRef LLVMAddGlobal(LLVMModuleRef M, LLVMTypeRef Ty, const char *Name);
LLVMValueRef LLVMAddGlobalIFunc(LLVMModuleRef M, const char *Name, __SIZE_TYPE__ NameLen, LLVMTypeRef Ty, unsigned AddrSpace, LLVMValueRef Resolver);
LLVMValueRef LLVMAddGlobalInAddressSpace(LLVMModuleRef M, LLVMTypeRef Ty, const char *Name, unsigned AddressSpace);
void LLVMAddHandler(LLVMValueRef CatchSwitch, LLVMBasicBlockRef Dest);
void LLVMAddIncoming(LLVMValueRef PhiNode, LLVMValueRef *IncomingValues, LLVMBasicBlockRef *IncomingBlocks, unsigned Count);
void LLVMAddMetadataToInst(LLVMBuilderRef Builder, LLVMValueRef Inst);
void LLVMAddModuleFlag(LLVMModuleRef M, LLVMModuleFlagBehavior Behavior, const char *Key, __SIZE_TYPE__ KeyLen, LLVMMetadataRef Val);
void LLVMAddNamedMetadataOperand(LLVMModuleRef M, const char *Name, LLVMValueRef Val);
void LLVMAddTargetDependentFunctionAttr(LLVMValueRef Fn, const char *A, const char *V);
LLVMValueRef LLVMAliasGetAliasee(LLVMValueRef Alias);
void LLVMAliasSetAliasee(LLVMValueRef Alias, LLVMValueRef Aliasee);
LLVMValueRef LLVMAlignOf(LLVMTypeRef Ty);
LLVMBasicBlockRef LLVMAppendBasicBlockInContext(LLVMContextRef C, LLVMValueRef Fn, const char *Name);
void LLVMAppendExistingBasicBlock(LLVMValueRef Fn, LLVMBasicBlockRef BB);
void LLVMAppendModuleInlineAsm(LLVMModuleRef M, const char *Asm, __SIZE_TYPE__ Len);
LLVMTypeRef LLVMArrayType(LLVMTypeRef ElementType, unsigned ElementCount);
LLVMTypeRef LLVMArrayType2(LLVMTypeRef ElementType, unsigned long long ElementCount);
LLVMTypeRef LLVMBFloatTypeInContext(LLVMContextRef C);
LLVMValueRef LLVMBasicBlockAsValue(LLVMBasicBlockRef BB);
LLVMValueRef LLVMBlockAddress(LLVMValueRef F, LLVMBasicBlockRef BB);
LLVMValueRef LLVMBuildAShr(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildAdd(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildAddrSpaceCast(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildAggregateRet(LLVMBuilderRef, LLVMValueRef *RetVals, unsigned N);
LLVMValueRef LLVMBuildAlloca(LLVMBuilderRef, LLVMTypeRef Ty, const char *Name);
LLVMValueRef LLVMBuildAnd(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildArrayAlloca(LLVMBuilderRef, LLVMTypeRef Ty, LLVMValueRef Val, const char *Name);
LLVMValueRef LLVMBuildArrayMalloc(LLVMBuilderRef, LLVMTypeRef Ty, LLVMValueRef Val, const char *Name);
LLVMValueRef LLVMBuildAtomicCmpXchg(LLVMBuilderRef B, LLVMValueRef Ptr, LLVMValueRef Cmp, LLVMValueRef New, LLVMAtomicOrdering SuccessOrdering, LLVMAtomicOrdering FailureOrdering, LLVMBool SingleThread);
LLVMValueRef LLVMBuildAtomicCmpXchgSyncScope(LLVMBuilderRef B, LLVMValueRef Ptr, LLVMValueRef Cmp, LLVMValueRef New, LLVMAtomicOrdering SuccessOrdering, LLVMAtomicOrdering FailureOrdering, unsigned SSID);
LLVMValueRef LLVMBuildAtomicRMW(LLVMBuilderRef B, LLVMAtomicRMWBinOp op, LLVMValueRef PTR, LLVMValueRef Val, LLVMAtomicOrdering ordering, LLVMBool singleThread);
LLVMValueRef LLVMBuildAtomicRMWSyncScope(LLVMBuilderRef B, LLVMAtomicRMWBinOp op, LLVMValueRef PTR, LLVMValueRef Val, LLVMAtomicOrdering ordering, unsigned SSID);
LLVMValueRef LLVMBuildBinOp(LLVMBuilderRef B, LLVMOpcode Op, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildBitCast(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildBr(LLVMBuilderRef, LLVMBasicBlockRef Dest);
LLVMValueRef LLVMBuildCall2(LLVMBuilderRef, LLVMTypeRef, LLVMValueRef Fn, LLVMValueRef *Args, unsigned NumArgs, const char *Name);
LLVMValueRef LLVMBuildCallBr(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef Fn, LLVMBasicBlockRef DefaultDest, LLVMBasicBlockRef *IndirectDests, unsigned NumIndirectDests, LLVMValueRef *Args, unsigned NumArgs, LLVMOperandBundleRef *Bundles, unsigned NumBundles, const char *Name);
LLVMValueRef LLVMBuildCast(LLVMBuilderRef B, LLVMOpcode Op, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildCatchPad(LLVMBuilderRef B, LLVMValueRef ParentPad, LLVMValueRef *Args, unsigned NumArgs, const char *Name);
LLVMValueRef LLVMBuildCatchRet(LLVMBuilderRef B, LLVMValueRef CatchPad, LLVMBasicBlockRef BB);
LLVMValueRef LLVMBuildCatchSwitch(LLVMBuilderRef B, LLVMValueRef ParentPad, LLVMBasicBlockRef UnwindBB, unsigned NumHandlers, const char *Name);
LLVMValueRef LLVMBuildCleanupPad(LLVMBuilderRef B, LLVMValueRef ParentPad, LLVMValueRef *Args, unsigned NumArgs, const char *Name);
LLVMValueRef LLVMBuildCleanupRet(LLVMBuilderRef B, LLVMValueRef CatchPad, LLVMBasicBlockRef BB);
LLVMValueRef LLVMBuildCondBr(LLVMBuilderRef, LLVMValueRef If, LLVMBasicBlockRef Then, LLVMBasicBlockRef Else);
LLVMValueRef LLVMBuildExactSDiv(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildExactUDiv(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildExtractElement(LLVMBuilderRef, LLVMValueRef VecVal, LLVMValueRef Index, const char *Name);
LLVMValueRef LLVMBuildExtractValue(LLVMBuilderRef, LLVMValueRef AggVal, unsigned Index, const char *Name);
LLVMValueRef LLVMBuildFAdd(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildFCmp(LLVMBuilderRef, LLVMRealPredicate Op, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildFDiv(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildFMul(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildFNeg(LLVMBuilderRef, LLVMValueRef V, const char *Name);
LLVMValueRef LLVMBuildFPCast(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildFPExt(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildFPToSI(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildFPToUI(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildFPTrunc(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildFRem(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildFSub(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildFence(LLVMBuilderRef B, LLVMAtomicOrdering ordering, LLVMBool singleThread, const char *Name);
LLVMValueRef LLVMBuildFenceSyncScope(LLVMBuilderRef B, LLVMAtomicOrdering ordering, unsigned SSID, const char *Name);
LLVMValueRef LLVMBuildFree(LLVMBuilderRef, LLVMValueRef PointerVal);
LLVMValueRef LLVMBuildFreeze(LLVMBuilderRef, LLVMValueRef Val, const char *Name);
LLVMValueRef LLVMBuildGEP2(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef Pointer, LLVMValueRef *Indices, unsigned NumIndices, const char *Name);
LLVMValueRef LLVMBuildGEPWithNoWrapFlags(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef Pointer, LLVMValueRef *Indices, unsigned NumIndices, const char *Name, LLVMGEPNoWrapFlags NoWrapFlags);
LLVMValueRef LLVMBuildGlobalString(LLVMBuilderRef B, const char *Str, const char *Name);
LLVMValueRef LLVMBuildGlobalStringPtr(LLVMBuilderRef B, const char *Str, const char *Name);
LLVMValueRef LLVMBuildICmp(LLVMBuilderRef, LLVMIntPredicate Op, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildInBoundsGEP2(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef Pointer, LLVMValueRef *Indices, unsigned NumIndices, const char *Name);
LLVMValueRef LLVMBuildIndirectBr(LLVMBuilderRef B, LLVMValueRef Addr, unsigned NumDests);
LLVMValueRef LLVMBuildInsertElement(LLVMBuilderRef, LLVMValueRef VecVal, LLVMValueRef EltVal, LLVMValueRef Index, const char *Name);
LLVMValueRef LLVMBuildInsertValue(LLVMBuilderRef, LLVMValueRef AggVal, LLVMValueRef EltVal, unsigned Index, const char *Name);
LLVMValueRef LLVMBuildIntCast(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildIntCast2(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, LLVMBool IsSigned, const char *Name);
LLVMValueRef LLVMBuildIntToPtr(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildInvoke2(LLVMBuilderRef, LLVMTypeRef Ty, LLVMValueRef Fn, LLVMValueRef *Args, unsigned NumArgs, LLVMBasicBlockRef Then, LLVMBasicBlockRef Catch, const char *Name);
LLVMValueRef LLVMBuildIsNotNull(LLVMBuilderRef, LLVMValueRef Val, const char *Name);
LLVMValueRef LLVMBuildIsNull(LLVMBuilderRef, LLVMValueRef Val, const char *Name);
LLVMValueRef LLVMBuildLShr(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildLandingPad(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef PersFn, unsigned NumClauses, const char *Name);
LLVMValueRef LLVMBuildLoad2(LLVMBuilderRef, LLVMTypeRef Ty, LLVMValueRef PointerVal, const char *Name);
LLVMValueRef LLVMBuildMalloc(LLVMBuilderRef, LLVMTypeRef Ty, const char *Name);
LLVMValueRef LLVMBuildMemCpy(LLVMBuilderRef B, LLVMValueRef Dst, unsigned DstAlign, LLVMValueRef Src, unsigned SrcAlign, LLVMValueRef Size);
LLVMValueRef LLVMBuildMemMove(LLVMBuilderRef B, LLVMValueRef Dst, unsigned DstAlign, LLVMValueRef Src, unsigned SrcAlign, LLVMValueRef Size);
LLVMValueRef LLVMBuildMemSet(LLVMBuilderRef B, LLVMValueRef Ptr, LLVMValueRef Val, LLVMValueRef Len, unsigned Align);
LLVMValueRef LLVMBuildMul(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildNSWAdd(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildNSWMul(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildNSWNeg(LLVMBuilderRef B, LLVMValueRef V, const char *Name);
LLVMValueRef LLVMBuildNSWSub(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildNUWAdd(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildNUWMul(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildNUWSub(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildNeg(LLVMBuilderRef, LLVMValueRef V, const char *Name);
LLVMValueRef LLVMBuildNot(LLVMBuilderRef, LLVMValueRef V, const char *Name);
LLVMValueRef LLVMBuildOr(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildPhi(LLVMBuilderRef, LLVMTypeRef Ty, const char *Name);
LLVMValueRef LLVMBuildPointerCast(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildPtrDiff2(LLVMBuilderRef, LLVMTypeRef ElemTy, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildPtrToInt(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildResume(LLVMBuilderRef B, LLVMValueRef Exn);
LLVMValueRef LLVMBuildRet(LLVMBuilderRef, LLVMValueRef V);
LLVMValueRef LLVMBuildRetVoid(LLVMBuilderRef);
LLVMValueRef LLVMBuildSDiv(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildSExt(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildSExtOrBitCast(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildSIToFP(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildSRem(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildSelect(LLVMBuilderRef, LLVMValueRef If, LLVMValueRef Then, LLVMValueRef Else, const char *Name);
LLVMValueRef LLVMBuildShl(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildShuffleVector(LLVMBuilderRef, LLVMValueRef V1, LLVMValueRef V2, LLVMValueRef Mask, const char *Name);
LLVMValueRef LLVMBuildStore(LLVMBuilderRef, LLVMValueRef Val, LLVMValueRef Ptr);
LLVMValueRef LLVMBuildStructGEP2(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef Pointer, unsigned Idx, const char *Name);
LLVMValueRef LLVMBuildSub(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildSwitch(LLVMBuilderRef, LLVMValueRef V, LLVMBasicBlockRef Else, unsigned NumCases);
LLVMValueRef LLVMBuildTrunc(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildTruncOrBitCast(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildUDiv(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildUIToFP(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildURem(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildUnreachable(LLVMBuilderRef);
LLVMValueRef LLVMBuildVAArg(LLVMBuilderRef, LLVMValueRef List, LLVMTypeRef Ty, const char *Name);
LLVMValueRef LLVMBuildXor(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name);
LLVMValueRef LLVMBuildZExt(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMValueRef LLVMBuildZExtOrBitCast(LLVMBuilderRef, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name);
LLVMMetadataRef LLVMBuilderGetDefaultFPMathTag(LLVMBuilderRef Builder);
void LLVMBuilderSetDefaultFPMathTag(LLVMBuilderRef Builder, LLVMMetadataRef FPMathTag);
LLVMBool LLVMCanValueUseFastMathFlags(LLVMValueRef Inst);
void LLVMClearInsertionPosition(LLVMBuilderRef Builder);
LLVMModuleRef LLVMCloneModule(LLVMModuleRef M);
LLVMValueRef LLVMConstAdd(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstAddrSpaceCast(LLVMValueRef ConstantVal, LLVMTypeRef ToType);
LLVMValueRef LLVMConstAllOnes(LLVMTypeRef Ty);
LLVMValueRef LLVMConstArray(LLVMTypeRef ElementTy, LLVMValueRef *ConstantVals, unsigned Length);
LLVMValueRef LLVMConstArray2(LLVMTypeRef ElementTy, LLVMValueRef *ConstantVals, unsigned long long Length);
LLVMValueRef LLVMConstBitCast(LLVMValueRef ConstantVal, LLVMTypeRef ToType);
LLVMValueRef LLVMConstExtractElement(LLVMValueRef VectorConstant, LLVMValueRef IndexConstant);
LLVMValueRef LLVMConstGEP2(LLVMTypeRef Ty, LLVMValueRef ConstantVal, LLVMValueRef *ConstantIndices, unsigned NumIndices);
LLVMValueRef LLVMConstGEPWithNoWrapFlags(LLVMTypeRef Ty, LLVMValueRef ConstantVal, LLVMValueRef *ConstantIndices, unsigned NumIndices, LLVMGEPNoWrapFlags NoWrapFlags);
LLVMValueRef LLVMConstInBoundsGEP2(LLVMTypeRef Ty, LLVMValueRef ConstantVal, LLVMValueRef *ConstantIndices, unsigned NumIndices);
LLVMValueRef LLVMConstInlineAsm(LLVMTypeRef Ty, const char *AsmString, const char *Constraints, LLVMBool HasSideEffects, LLVMBool IsAlignStack);
LLVMValueRef LLVMConstInsertElement(LLVMValueRef VectorConstant, LLVMValueRef ElementValueConstant, LLVMValueRef IndexConstant);
LLVMValueRef LLVMConstInt(LLVMTypeRef IntTy, unsigned long long N, LLVMBool SignExtend);
long long LLVMConstIntGetSExtValue(LLVMValueRef ConstantVal);
unsigned long long LLVMConstIntGetZExtValue(LLVMValueRef ConstantVal);
LLVMValueRef LLVMConstIntOfArbitraryPrecision(LLVMTypeRef IntTy, unsigned NumWords, const unsigned long long Words[]);
LLVMValueRef LLVMConstIntOfString(LLVMTypeRef IntTy, const char *Text, unsigned Radix);
LLVMValueRef LLVMConstIntOfStringAndSize(LLVMTypeRef IntTy, const char *Text, unsigned SLen, unsigned Radix);
LLVMValueRef LLVMConstIntToPtr(LLVMValueRef ConstantVal, LLVMTypeRef ToType);
LLVMValueRef LLVMConstMul(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstNSWAdd(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstNSWMul(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstNSWNeg(LLVMValueRef ConstantVal);
LLVMValueRef LLVMConstNSWSub(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstNUWAdd(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstNUWMul(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstNUWSub(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstNamedStruct(LLVMTypeRef StructTy, LLVMValueRef *ConstantVals, unsigned Count);
LLVMValueRef LLVMConstNeg(LLVMValueRef ConstantVal);
LLVMValueRef LLVMConstNot(LLVMValueRef ConstantVal);
LLVMValueRef LLVMConstNull(LLVMTypeRef Ty);
LLVMValueRef LLVMConstPointerCast(LLVMValueRef ConstantVal, LLVMTypeRef ToType);
LLVMValueRef LLVMConstPointerNull(LLVMTypeRef Ty);
LLVMValueRef LLVMConstPtrToInt(LLVMValueRef ConstantVal, LLVMTypeRef ToType);
LLVMValueRef LLVMConstReal(LLVMTypeRef RealTy, double N);
double LLVMConstRealGetDouble(LLVMValueRef ConstantVal, LLVMBool *losesInfo);
LLVMValueRef LLVMConstRealOfString(LLVMTypeRef RealTy, const char *Text);
LLVMValueRef LLVMConstRealOfStringAndSize(LLVMTypeRef RealTy, const char *Text, unsigned SLen);
LLVMValueRef LLVMConstShuffleVector(LLVMValueRef VectorAConstant, LLVMValueRef VectorBConstant, LLVMValueRef MaskConstant);
LLVMValueRef LLVMConstStringInContext(LLVMContextRef C, const char *Str, unsigned Length, LLVMBool DontNullTerminate);
LLVMValueRef LLVMConstStringInContext2(LLVMContextRef C, const char *Str, __SIZE_TYPE__ Length, LLVMBool DontNullTerminate);
LLVMValueRef LLVMConstStructInContext(LLVMContextRef C, LLVMValueRef *ConstantVals, unsigned Count, LLVMBool Packed);
LLVMValueRef LLVMConstSub(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstTrunc(LLVMValueRef ConstantVal, LLVMTypeRef ToType);
LLVMValueRef LLVMConstTruncOrBitCast(LLVMValueRef ConstantVal, LLVMTypeRef ToType);
LLVMValueRef LLVMConstVector(LLVMValueRef *ScalarConstantVals, unsigned Size);
LLVMValueRef LLVMConstXor(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
LLVMValueRef LLVMConstantPtrAuth(LLVMValueRef Ptr, LLVMValueRef Key, LLVMValueRef Disc, LLVMValueRef AddrDisc);
LLVMContextRef LLVMContextCreate(void);
void LLVMContextSetDiscardValueNames(LLVMContextRef C, LLVMBool Discard);
LLVMBool LLVMContextShouldDiscardValueNames(LLVMContextRef C);
unsigned LLVMCountBasicBlocks(LLVMValueRef Fn);
unsigned LLVMCountIncoming(LLVMValueRef PhiNode);
unsigned LLVMCountParamTypes(LLVMTypeRef FunctionTy);
unsigned LLVMCountParams(LLVMValueRef Fn);
unsigned LLVMCountStructElementTypes(LLVMTypeRef StructTy);
LLVMBasicBlockRef LLVMCreateBasicBlockInContext(LLVMContextRef C, const char *Name);
LLVMBuilderRef LLVMCreateBuilderInContext(LLVMContextRef C);
LLVMAttributeRef LLVMCreateConstantRangeAttribute(LLVMContextRef C, unsigned KindID, unsigned NumBits, const unsigned long long LowerWords[], const unsigned long long UpperWords[]);
LLVMAttributeRef LLVMCreateEnumAttribute(LLVMContextRef C, unsigned KindID, unsigned long long Val);
LLVMAttributeRef LLVMCreateStringAttribute(LLVMContextRef C, const char *K, unsigned KLength, const char *V, unsigned VLength);
LLVMAttributeRef LLVMCreateTypeAttribute(LLVMContextRef C, unsigned KindID, LLVMTypeRef type_ref);
LLVMTypeRef LLVMDoubleTypeInContext(LLVMContextRef C);
void LLVMDumpModule(LLVMModuleRef M);
void LLVMDumpType(LLVMTypeRef Val);
void LLVMDumpValue(LLVMValueRef Val);
LLVMTypeRef LLVMFP128TypeInContext(LLVMContextRef C);
LLVMTypeRef LLVMFloatTypeInContext(LLVMContextRef C);
LLVMTypeRef LLVMFunctionType(LLVMTypeRef ReturnType, LLVMTypeRef *ParamTypes, unsigned ParamCount, LLVMBool IsVarArg);
LLVMGEPNoWrapFlags LLVMGEPGetNoWrapFlags(LLVMValueRef GEP);
void LLVMGEPSetNoWrapFlags(LLVMValueRef GEP, LLVMGEPNoWrapFlags NoWrapFlags);
LLVMValueRef LLVMGetAggregateElement(LLVMValueRef C, unsigned Idx);
unsigned LLVMGetAlignment(LLVMValueRef V);
LLVMTypeRef LLVMGetAllocatedType(LLVMValueRef Alloca);
LLVMValueRef LLVMGetArgOperand(LLVMValueRef Funclet, unsigned i);
unsigned LLVMGetArrayLength(LLVMTypeRef ArrayTy);
unsigned long long LLVMGetArrayLength2(LLVMTypeRef ArrayTy);
LLVMAtomicRMWBinOp LLVMGetAtomicRMWBinOp(LLVMValueRef AtomicRMWInst);
unsigned LLVMGetAtomicSyncScopeID(LLVMValueRef AtomicInst);
unsigned LLVMGetAttributeCountAtIndex(LLVMValueRef F, LLVMAttributeIndex Idx);
void LLVMGetAttributesAtIndex(LLVMValueRef F, LLVMAttributeIndex Idx, LLVMAttributeRef *Attrs);
const char *LLVMGetBasicBlockName(LLVMBasicBlockRef BB);
LLVMValueRef LLVMGetBasicBlockParent(LLVMBasicBlockRef BB);
LLVMValueRef LLVMGetBasicBlockTerminator(LLVMBasicBlockRef BB);
void LLVMGetBasicBlocks(LLVMValueRef Fn, LLVMBasicBlockRef *BasicBlocks);
LLVMBasicBlockRef LLVMGetBlockAddressBasicBlock(LLVMValueRef BlockAddr);
LLVMValueRef LLVMGetBlockAddressFunction(LLVMValueRef BlockAddr);
LLVMContextRef LLVMGetBuilderContext(LLVMBuilderRef Builder);
LLVMBasicBlockRef LLVMGetCallBrDefaultDest(LLVMValueRef CallBr);
LLVMBasicBlockRef LLVMGetCallBrIndirectDest(LLVMValueRef CallBr, unsigned Idx);
unsigned LLVMGetCallBrNumIndirectDests(LLVMValueRef CallBr);
unsigned LLVMGetCallSiteAttributeCount(LLVMValueRef C, LLVMAttributeIndex Idx);
void LLVMGetCallSiteAttributes(LLVMValueRef C, LLVMAttributeIndex Idx, LLVMAttributeRef *Attrs);
LLVMAttributeRef LLVMGetCallSiteEnumAttribute(LLVMValueRef C, LLVMAttributeIndex Idx, unsigned KindID);
LLVMAttributeRef LLVMGetCallSiteStringAttribute(LLVMValueRef C, LLVMAttributeIndex Idx, const char *K, unsigned KLen);
LLVMTypeRef LLVMGetCalledFunctionType(LLVMValueRef C);
LLVMValueRef LLVMGetCalledValue(LLVMValueRef Instr);
LLVMOpcode LLVMGetCastOpcode(LLVMValueRef Src, LLVMBool SrcIsSigned, LLVMTypeRef DestTy, LLVMBool DestIsSigned);
LLVMValueRef LLVMGetClause(LLVMValueRef LandingPad, unsigned Idx);
LLVMAtomicOrdering LLVMGetCmpXchgFailureOrdering(LLVMValueRef CmpXchgInst);
LLVMAtomicOrdering LLVMGetCmpXchgSuccessOrdering(LLVMValueRef CmpXchgInst);
LLVMValueRef LLVMGetCondition(LLVMValueRef Branch);
LLVMOpcode LLVMGetConstOpcode(LLVMValueRef ConstantVal);
LLVMValueRef LLVMGetConstantPtrAuthAddrDiscriminator(LLVMValueRef PtrAuth);
LLVMValueRef LLVMGetConstantPtrAuthDiscriminator(LLVMValueRef PtrAuth);
LLVMValueRef LLVMGetConstantPtrAuthKey(LLVMValueRef PtrAuth);
LLVMValueRef LLVMGetConstantPtrAuthPointer(LLVMValueRef PtrAuth);
LLVMValueRef LLVMGetCurrentDebugLocation(LLVMBuilderRef Builder);
LLVMMetadataRef LLVMGetCurrentDebugLocation2(LLVMBuilderRef Builder);
LLVMDLLStorageClass LLVMGetDLLStorageClass(LLVMValueRef Global);
const char *LLVMGetDataLayout(LLVMModuleRef M);
const char *LLVMGetDataLayoutStr(LLVMModuleRef M);
unsigned LLVMGetDebugLocColumn(LLVMValueRef Val);
const char *LLVMGetDebugLocDirectory(LLVMValueRef Val, unsigned *Length);
const char *LLVMGetDebugLocFilename(LLVMValueRef Val, unsigned *Length);
unsigned LLVMGetDebugLocLine(LLVMValueRef Val);
LLVMTypeRef LLVMGetElementType(LLVMTypeRef Ty);
LLVMBasicBlockRef LLVMGetEntryBasicBlock(LLVMValueRef Fn);
LLVMAttributeRef LLVMGetEnumAttributeAtIndex(LLVMValueRef F, LLVMAttributeIndex Idx, unsigned KindID);
unsigned LLVMGetEnumAttributeKind(LLVMAttributeRef A);
unsigned LLVMGetEnumAttributeKindForName(const char *Name, __SIZE_TYPE__ SLen);
unsigned long long LLVMGetEnumAttributeValue(LLVMAttributeRef A);
LLVMBool LLVMGetExact(LLVMValueRef DivOrShrInst);
LLVMRealPredicate LLVMGetFCmpPredicate(LLVMValueRef Inst);
LLVMFastMathFlags LLVMGetFastMathFlags(LLVMValueRef FPMathInst);
LLVMBasicBlockRef LLVMGetFirstBasicBlock(LLVMValueRef Fn);
LLVMDbgRecordRef LLVMGetFirstDbgRecord(LLVMValueRef Inst);
LLVMValueRef LLVMGetFirstFunction(LLVMModuleRef M);
LLVMValueRef LLVMGetFirstGlobal(LLVMModuleRef M);
LLVMValueRef LLVMGetFirstGlobalAlias(LLVMModuleRef M);
LLVMValueRef LLVMGetFirstGlobalIFunc(LLVMModuleRef M);
LLVMValueRef LLVMGetFirstInstruction(LLVMBasicBlockRef BB);
LLVMNamedMDNodeRef LLVMGetFirstNamedMetadata(LLVMModuleRef M);
LLVMValueRef LLVMGetFirstParam(LLVMValueRef Fn);
LLVMUseRef LLVMGetFirstUse(LLVMValueRef Val);
unsigned LLVMGetFunctionCallConv(LLVMValueRef Fn);
const char *LLVMGetGC(LLVMValueRef Fn);
LLVMTypeRef LLVMGetGEPSourceElementType(LLVMValueRef GEP);
LLVMValueRef LLVMGetGlobalIFuncResolver(LLVMValueRef IFunc);
LLVMModuleRef LLVMGetGlobalParent(LLVMValueRef Global);
void LLVMGetHandlers(LLVMValueRef CatchSwitch, LLVMBasicBlockRef *Handlers);
LLVMIntPredicate LLVMGetICmpPredicate(LLVMValueRef Inst);
LLVMBasicBlockRef LLVMGetIncomingBlock(LLVMValueRef PhiNode, unsigned Index);
LLVMValueRef LLVMGetIncomingValue(LLVMValueRef PhiNode, unsigned Index);
const unsigned *LLVMGetIndices(LLVMValueRef Inst);
LLVMValueRef LLVMGetInitializer(LLVMValueRef GlobalVar);
LLVMValueRef LLVMGetInlineAsm(LLVMTypeRef Ty, const char *AsmString, __SIZE_TYPE__ AsmStringSize, const char *Constraints, __SIZE_TYPE__ ConstraintsSize, LLVMBool HasSideEffects, LLVMBool IsAlignStack, LLVMInlineAsmDialect Dialect, LLVMBool CanThrow);
const char *LLVMGetInlineAsmAsmString(LLVMValueRef InlineAsmVal, __SIZE_TYPE__ *Len);
LLVMBool LLVMGetInlineAsmCanUnwind(LLVMValueRef InlineAsmVal);
const char *LLVMGetInlineAsmConstraintString(LLVMValueRef InlineAsmVal, __SIZE_TYPE__ *Len);
LLVMInlineAsmDialect LLVMGetInlineAsmDialect(LLVMValueRef InlineAsmVal);
LLVMTypeRef LLVMGetInlineAsmFunctionType(LLVMValueRef InlineAsmVal);
LLVMBool LLVMGetInlineAsmHasSideEffects(LLVMValueRef InlineAsmVal);
LLVMBool LLVMGetInlineAsmNeedsAlignedStack(LLVMValueRef InlineAsmVal);
LLVMBasicBlockRef LLVMGetInsertBlock(LLVMBuilderRef Builder);
unsigned LLVMGetInstructionCallConv(LLVMValueRef Instr);
LLVMOpcode LLVMGetInstructionOpcode(LLVMValueRef Inst);
LLVMBasicBlockRef LLVMGetInstructionParent(LLVMValueRef Inst);
unsigned LLVMGetIntTypeWidth(LLVMTypeRef IntegerTy);
LLVMValueRef LLVMGetIntrinsicDeclaration(LLVMModuleRef Mod, unsigned ID, LLVMTypeRef *ParamTypes, __SIZE_TYPE__ ParamCount);
unsigned LLVMGetIntrinsicID(LLVMValueRef Fn);
LLVMBool LLVMGetIsDisjoint(LLVMValueRef Inst);
LLVMBasicBlockRef LLVMGetLastBasicBlock(LLVMValueRef Fn);
LLVMDbgRecordRef LLVMGetLastDbgRecord(LLVMValueRef Inst);
unsigned LLVMGetLastEnumAttributeKind(void);
LLVMValueRef LLVMGetLastFunction(LLVMModuleRef M);
LLVMValueRef LLVMGetLastGlobal(LLVMModuleRef M);
LLVMValueRef LLVMGetLastGlobalAlias(LLVMModuleRef M);
LLVMValueRef LLVMGetLastGlobalIFunc(LLVMModuleRef M);
LLVMValueRef LLVMGetLastInstruction(LLVMBasicBlockRef BB);
LLVMNamedMDNodeRef LLVMGetLastNamedMetadata(LLVMModuleRef M);
LLVMValueRef LLVMGetLastParam(LLVMValueRef Fn);
LLVMLinkage LLVMGetLinkage(LLVMValueRef Global);
unsigned LLVMGetMDKindIDInContext(LLVMContextRef C, const char *Name, unsigned SLen);
unsigned LLVMGetMDNodeNumOperands(LLVMValueRef V);
void LLVMGetMDNodeOperands(LLVMValueRef V, LLVMValueRef *Dest);
const char *LLVMGetMDString(LLVMValueRef V, unsigned *Length);
int LLVMGetMaskValue(LLVMValueRef ShuffleVectorInst, unsigned Elt);
LLVMValueRef LLVMGetMetadata(LLVMValueRef Val, unsigned KindID);
LLVMContextRef LLVMGetModuleContext(LLVMModuleRef M);
LLVMMetadataRef LLVMGetModuleFlag(LLVMModuleRef M, const char *Key, __SIZE_TYPE__ KeyLen);
const char *LLVMGetModuleIdentifier(LLVMModuleRef M, __SIZE_TYPE__ *Len);
const char *LLVMGetModuleInlineAsm(LLVMModuleRef M, __SIZE_TYPE__ *Len);
LLVMBool LLVMGetNNeg(LLVMValueRef NonNegInst);
LLVMBool LLVMGetNSW(LLVMValueRef ArithInst);
LLVMBool LLVMGetNUW(LLVMValueRef ArithInst);
LLVMValueRef LLVMGetNamedFunction(LLVMModuleRef M, const char *Name);
LLVMValueRef LLVMGetNamedFunctionWithLength(LLVMModuleRef M, const char *Name, __SIZE_TYPE__ Length);
LLVMValueRef LLVMGetNamedGlobal(LLVMModuleRef M, const char *Name);
LLVMValueRef LLVMGetNamedGlobalAlias(LLVMModuleRef M, const char *Name, __SIZE_TYPE__ NameLen);
LLVMValueRef LLVMGetNamedGlobalIFunc(LLVMModuleRef M, const char *Name, __SIZE_TYPE__ NameLen);
LLVMValueRef LLVMGetNamedGlobalWithLength(LLVMModuleRef M, const char *Name, __SIZE_TYPE__ Length);
LLVMNamedMDNodeRef LLVMGetNamedMetadata(LLVMModuleRef M, const char *Name, __SIZE_TYPE__ NameLen);
const char *LLVMGetNamedMetadataName(LLVMNamedMDNodeRef NamedMD, __SIZE_TYPE__ *NameLen);
unsigned LLVMGetNamedMetadataNumOperands(LLVMModuleRef M, const char *Name);
void LLVMGetNamedMetadataOperands(LLVMModuleRef M, const char *Name, LLVMValueRef *Dest);
LLVMBasicBlockRef LLVMGetNextBasicBlock(LLVMBasicBlockRef BB);
LLVMDbgRecordRef LLVMGetNextDbgRecord(LLVMDbgRecordRef DbgRecord);
LLVMValueRef LLVMGetNextFunction(LLVMValueRef Fn);
LLVMValueRef LLVMGetNextGlobal(LLVMValueRef GlobalVar);
LLVMValueRef LLVMGetNextGlobalAlias(LLVMValueRef GA);
LLVMValueRef LLVMGetNextGlobalIFunc(LLVMValueRef IFunc);
LLVMValueRef LLVMGetNextInstruction(LLVMValueRef Inst);
LLVMNamedMDNodeRef LLVMGetNextNamedMetadata(LLVMNamedMDNodeRef NamedMDNode);
LLVMValueRef LLVMGetNextParam(LLVMValueRef Arg);
LLVMUseRef LLVMGetNextUse(LLVMUseRef U);
LLVMBasicBlockRef LLVMGetNormalDest(LLVMValueRef InvokeInst);
unsigned LLVMGetNumArgOperands(LLVMValueRef Instr);
unsigned LLVMGetNumClauses(LLVMValueRef LandingPad);
unsigned LLVMGetNumContainedTypes(LLVMTypeRef Tp);
unsigned LLVMGetNumHandlers(LLVMValueRef CatchSwitch);
unsigned LLVMGetNumIndices(LLVMValueRef Inst);
unsigned LLVMGetNumMaskElements(LLVMValueRef ShuffleVectorInst);
int LLVMGetNumOperands(LLVMValueRef Val);
unsigned LLVMGetNumSuccessors(LLVMValueRef Term);
LLVMValueRef LLVMGetOperand(LLVMValueRef Val, unsigned Index);
LLVMUseRef LLVMGetOperandUse(LLVMValueRef Val, unsigned Index);
LLVMNamedMDNodeRef LLVMGetOrInsertNamedMetadata(LLVMModuleRef M, const char *Name, __SIZE_TYPE__ NameLen);
LLVMAtomicOrdering LLVMGetOrdering(LLVMValueRef MemoryAccessInst);
LLVMValueRef LLVMGetParam(LLVMValueRef Fn, unsigned Index);
LLVMValueRef LLVMGetParamParent(LLVMValueRef Inst);
void LLVMGetParamTypes(LLVMTypeRef FunctionTy, LLVMTypeRef *Dest);
void LLVMGetParams(LLVMValueRef Fn, LLVMValueRef *Params);
LLVMValueRef LLVMGetParentCatchSwitch(LLVMValueRef CatchPad);
LLVMValueRef LLVMGetPersonalityFn(LLVMValueRef Fn);
unsigned LLVMGetPointerAddressSpace(LLVMTypeRef PointerTy);
LLVMValueRef LLVMGetPoison(LLVMTypeRef Ty);
LLVMValueRef LLVMGetPrefixData(LLVMValueRef Fn);
LLVMBasicBlockRef LLVMGetPreviousBasicBlock(LLVMBasicBlockRef BB);
LLVMDbgRecordRef LLVMGetPreviousDbgRecord(LLVMDbgRecordRef DbgRecord);
LLVMValueRef LLVMGetPreviousFunction(LLVMValueRef Fn);
LLVMValueRef LLVMGetPreviousGlobal(LLVMValueRef GlobalVar);
LLVMValueRef LLVMGetPreviousGlobalAlias(LLVMValueRef GA);
LLVMValueRef LLVMGetPreviousGlobalIFunc(LLVMValueRef IFunc);
LLVMValueRef LLVMGetPreviousInstruction(LLVMValueRef Inst);
LLVMNamedMDNodeRef LLVMGetPreviousNamedMetadata(LLVMNamedMDNodeRef NamedMDNode);
LLVMValueRef LLVMGetPreviousParam(LLVMValueRef Arg);
LLVMValueRef LLVMGetPrologueData(LLVMValueRef Fn);
LLVMTypeRef LLVMGetReturnType(LLVMTypeRef FunctionTy);
const char *LLVMGetSection(LLVMValueRef Global);
const char *LLVMGetSourceFileName(LLVMModuleRef M, __SIZE_TYPE__ *Len);
LLVMAttributeRef LLVMGetStringAttributeAtIndex(LLVMValueRef F, LLVMAttributeIndex Idx, const char *K, unsigned KLen);
const char *LLVMGetStringAttributeKind(LLVMAttributeRef A, unsigned *Length);
const char *LLVMGetStringAttributeValue(LLVMAttributeRef A, unsigned *Length);
void LLVMGetStructElementTypes(LLVMTypeRef StructTy, LLVMTypeRef *Dest);
const char *LLVMGetStructName(LLVMTypeRef Ty);
void LLVMGetSubtypes(LLVMTypeRef Tp, LLVMTypeRef *Arr);
LLVMBasicBlockRef LLVMGetSuccessor(LLVMValueRef Term, unsigned i);
LLVMBasicBlockRef LLVMGetSwitchDefaultDest(LLVMValueRef SwitchInstr);
unsigned LLVMGetSyncScopeID(LLVMContextRef C, const char *Name, __SIZE_TYPE__ SLen);
LLVMTailCallKind LLVMGetTailCallKind(LLVMValueRef CallInst);
const char *LLVMGetTarget(LLVMModuleRef M);
unsigned LLVMGetTargetExtTypeIntParam(LLVMTypeRef TargetExtTy, unsigned Idx);
const char *LLVMGetTargetExtTypeName(LLVMTypeRef TargetExtTy);
unsigned LLVMGetTargetExtTypeNumIntParams(LLVMTypeRef TargetExtTy);
unsigned LLVMGetTargetExtTypeNumTypeParams(LLVMTypeRef TargetExtTy);
LLVMTypeRef LLVMGetTargetExtTypeTypeParam(LLVMTypeRef TargetExtTy, unsigned Idx);
LLVMThreadLocalMode LLVMGetThreadLocalMode(LLVMValueRef GlobalVar);
LLVMTypeRef LLVMGetTypeAttributeValue(LLVMAttributeRef A);
LLVMTypeRef LLVMGetTypeByName(LLVMModuleRef M, const char *Name);
LLVMTypeRef LLVMGetTypeByName2(LLVMContextRef C, const char *Name);
LLVMContextRef LLVMGetTypeContext(LLVMTypeRef Ty);
LLVMTypeKind LLVMGetTypeKind(LLVMTypeRef Ty);
LLVMValueRef LLVMGetUndef(LLVMTypeRef Ty);
int LLVMGetUndefMaskElem(void);
LLVMUnnamedAddr LLVMGetUnnamedAddress(LLVMValueRef Global);
LLVMBasicBlockRef LLVMGetUnwindDest(LLVMValueRef InvokeInst);
LLVMValueRef LLVMGetUsedValue(LLVMUseRef U);
LLVMValueRef LLVMGetUser(LLVMUseRef U);
LLVMContextRef LLVMGetValueContext(LLVMValueRef Val);
LLVMValueKind LLVMGetValueKind(LLVMValueRef Val);
const char *LLVMGetValueName(LLVMValueRef Val);
const char *LLVMGetValueName2(LLVMValueRef Val, __SIZE_TYPE__ *Length);
unsigned LLVMGetVectorSize(LLVMTypeRef VectorTy);
void LLVMGetVersion(unsigned *Major, unsigned *Minor, unsigned *Patch);
LLVMVisibility LLVMGetVisibility(LLVMValueRef Global);
LLVMBool LLVMGetVolatile(LLVMValueRef MemoryAccessInst);
LLVMBool LLVMGetWeak(LLVMValueRef CmpXchgInst);
LLVMValueMetadataEntry *LLVMGlobalCopyAllMetadata(LLVMValueRef Value, __SIZE_TYPE__ *NumEntries);
LLVMTypeRef LLVMGlobalGetValueType(LLVMValueRef Global);
void LLVMGlobalSetMetadata(LLVMValueRef Global, unsigned Kind, LLVMMetadataRef MD);
LLVMTypeRef LLVMHalfTypeInContext(LLVMContextRef C);
int LLVMHasMetadata(LLVMValueRef Val);
LLVMBool LLVMHasPersonalityFn(LLVMValueRef Fn);
LLVMBool LLVMHasPrefixData(LLVMValueRef Fn);
LLVMBool LLVMHasPrologueData(LLVMValueRef Fn);
LLVMBool LLVMHasUnnamedAddr(LLVMValueRef Global);
LLVMBasicBlockRef LLVMInsertBasicBlockInContext(LLVMContextRef C, LLVMBasicBlockRef BB, const char *Name);
void LLVMInsertExistingBasicBlockAfterInsertBlock(LLVMBuilderRef Builder, LLVMBasicBlockRef BB);
void LLVMInsertIntoBuilder(LLVMBuilderRef Builder, LLVMValueRef Instr);
void LLVMInsertIntoBuilderWithName(LLVMBuilderRef Builder, LLVMValueRef Instr, const char *Name);
LLVMValueRef LLVMInstructionClone(LLVMValueRef Inst);
LLVMValueMetadataEntry * LLVMInstructionGetAllMetadataOtherThanDebugLoc(LLVMValueRef Instr, __SIZE_TYPE__ *NumEntries);
LLVMTypeRef LLVMInt128TypeInContext(LLVMContextRef C);
LLVMTypeRef LLVMInt16TypeInContext(LLVMContextRef C);
LLVMTypeRef LLVMInt1TypeInContext(LLVMContextRef C);
LLVMTypeRef LLVMInt32TypeInContext(LLVMContextRef C);
LLVMTypeRef LLVMInt64TypeInContext(LLVMContextRef C);
LLVMTypeRef LLVMInt8TypeInContext(LLVMContextRef C);
LLVMTypeRef LLVMIntTypeInContext(LLVMContextRef C, unsigned NumBits);
char *LLVMIntrinsicCopyOverloadedName(unsigned ID, LLVMTypeRef *ParamTypes, __SIZE_TYPE__ ParamCount, __SIZE_TYPE__ *NameLength);
char *LLVMIntrinsicCopyOverloadedName2(LLVMModuleRef Mod, unsigned ID, LLVMTypeRef *ParamTypes, __SIZE_TYPE__ ParamCount, __SIZE_TYPE__ *NameLength);
const char *LLVMIntrinsicGetName(unsigned ID, __SIZE_TYPE__ *NameLength);
LLVMTypeRef LLVMIntrinsicGetType(LLVMContextRef Ctx, unsigned ID, LLVMTypeRef *ParamTypes, __SIZE_TYPE__ ParamCount);
LLVMBool LLVMIntrinsicIsOverloaded(unsigned ID);
LLVMValueRef LLVMIsAAddrSpaceCastInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAAllocaInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAArgument(LLVMValueRef Val);
LLVMValueRef LLVMIsAAtomicCmpXchgInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAAtomicRMWInst(LLVMValueRef Val);
LLVMValueRef LLVMIsABasicBlock(LLVMValueRef Val);
LLVMValueRef LLVMIsABinaryOperator(LLVMValueRef Val);
LLVMValueRef LLVMIsABitCastInst(LLVMValueRef Val);
LLVMValueRef LLVMIsABlockAddress(LLVMValueRef Val);
LLVMValueRef LLVMIsABranchInst(LLVMValueRef Val);
LLVMValueRef LLVMIsACallBrInst(LLVMValueRef Val);
LLVMValueRef LLVMIsACallInst(LLVMValueRef Val);
LLVMValueRef LLVMIsACastInst(LLVMValueRef Val);
LLVMValueRef LLVMIsACatchPadInst(LLVMValueRef Val);
LLVMValueRef LLVMIsACatchReturnInst(LLVMValueRef Val);
LLVMValueRef LLVMIsACatchSwitchInst(LLVMValueRef Val);
LLVMValueRef LLVMIsACleanupPadInst(LLVMValueRef Val);
LLVMValueRef LLVMIsACleanupReturnInst(LLVMValueRef Val);
LLVMValueRef LLVMIsACmpInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAConstant(LLVMValueRef Val);
LLVMValueRef LLVMIsAConstantAggregateZero(LLVMValueRef Val);
LLVMValueRef LLVMIsAConstantArray(LLVMValueRef Val);
LLVMValueRef LLVMIsAConstantDataArray(LLVMValueRef Val);
LLVMValueRef LLVMIsAConstantDataSequential(LLVMValueRef Val);
LLVMValueRef LLVMIsAConstantDataVector(LLVMValueRef Val);
LLVMValueRef LLVMIsAConstantExpr(LLVMValueRef Val);
LLVMValueRef LLVMIsAConstantFP(LLVMValueRef Val);
LLVMValueRef LLVMIsAConstantInt(LLVMValueRef Val);
LLVMValueRef LLVMIsAConstantPointerNull(LLVMValueRef Val);
LLVMValueRef LLVMIsAConstantPtrAuth(LLVMValueRef Val);
LLVMValueRef LLVMIsAConstantStruct(LLVMValueRef Val);
LLVMValueRef LLVMIsAConstantTokenNone(LLVMValueRef Val);
LLVMValueRef LLVMIsAConstantVector(LLVMValueRef Val);
LLVMValueRef LLVMIsADbgDeclareInst(LLVMValueRef Val);
LLVMValueRef LLVMIsADbgInfoIntrinsic(LLVMValueRef Val);
LLVMValueRef LLVMIsADbgLabelInst(LLVMValueRef Val);
LLVMValueRef LLVMIsADbgVariableIntrinsic(LLVMValueRef Val);
LLVMValueRef LLVMIsAExtractElementInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAExtractValueInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAFCmpInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAFPExtInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAFPToSIInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAFPToUIInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAFPTruncInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAFenceInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAFreezeInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAFuncletPadInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAFunction(LLVMValueRef Val);
LLVMValueRef LLVMIsAGetElementPtrInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAGlobalAlias(LLVMValueRef Val);
LLVMValueRef LLVMIsAGlobalIFunc(LLVMValueRef Val);
LLVMValueRef LLVMIsAGlobalObject(LLVMValueRef Val);
LLVMValueRef LLVMIsAGlobalValue(LLVMValueRef Val);
LLVMValueRef LLVMIsAGlobalVariable(LLVMValueRef Val);
LLVMValueRef LLVMIsAICmpInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAIndirectBrInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAInlineAsm(LLVMValueRef Val);
LLVMValueRef LLVMIsAInsertElementInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAInsertValueInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAInstruction(LLVMValueRef Val);
LLVMValueRef LLVMIsAIntToPtrInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAIntrinsicInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAInvokeInst(LLVMValueRef Val);
LLVMValueRef LLVMIsALandingPadInst(LLVMValueRef Val);
LLVMValueRef LLVMIsALoadInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAMemCpyInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAMemIntrinsic(LLVMValueRef Val);
LLVMValueRef LLVMIsAMemMoveInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAMemSetInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAPHINode(LLVMValueRef Val);
LLVMValueRef LLVMIsAPoisonValue(LLVMValueRef Val);
LLVMValueRef LLVMIsAPtrToIntInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAResumeInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAReturnInst(LLVMValueRef Val);
LLVMValueRef LLVMIsASExtInst(LLVMValueRef Val);
LLVMValueRef LLVMIsASIToFPInst(LLVMValueRef Val);
LLVMValueRef LLVMIsASelectInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAShuffleVectorInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAStoreInst(LLVMValueRef Val);
LLVMValueRef LLVMIsASwitchInst(LLVMValueRef Val);
LLVMValueRef LLVMIsATruncInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAUIToFPInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAUnaryInstruction(LLVMValueRef Val);
LLVMValueRef LLVMIsAUnaryOperator(LLVMValueRef Val);
LLVMValueRef LLVMIsAUndefValue(LLVMValueRef Val);
LLVMValueRef LLVMIsAUnreachableInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAUser(LLVMValueRef Val);
LLVMValueRef LLVMIsAVAArgInst(LLVMValueRef Val);
LLVMValueRef LLVMIsAZExtInst(LLVMValueRef Val);
LLVMBool LLVMIsCleanup(LLVMValueRef LandingPad);
LLVMBool LLVMIsConditional(LLVMValueRef Branch);
LLVMBool LLVMIsConstant(LLVMValueRef Val);
LLVMBool LLVMIsConstantString(LLVMValueRef c);
LLVMBool LLVMIsDeclaration(LLVMValueRef Global);
LLVMBool LLVMIsEnumAttribute(LLVMAttributeRef A);
LLVMBool LLVMIsExternallyInitialized(LLVMValueRef GlobalVar);
LLVMBool LLVMIsFunctionVarArg(LLVMTypeRef FunctionTy);
LLVMBool LLVMIsGlobalConstant(LLVMValueRef GlobalVar);
LLVMBool LLVMIsInBounds(LLVMValueRef GEP);
LLVMBool LLVMIsLiteralStruct(LLVMTypeRef StructTy);
LLVMBool LLVMIsMultithreaded(void);
LLVMBool LLVMIsNewDbgInfoFormat(LLVMModuleRef M);
LLVMBool LLVMIsNull(LLVMValueRef Val);
LLVMBool LLVMIsOpaqueStruct(LLVMTypeRef StructTy);
LLVMBool LLVMIsPackedStruct(LLVMTypeRef StructTy);
LLVMBool LLVMIsPoison(LLVMValueRef Val);
LLVMBool LLVMIsStringAttribute(LLVMAttributeRef A);
LLVMBool LLVMIsTailCall(LLVMValueRef CallInst);
LLVMBool LLVMIsThreadLocal(LLVMValueRef GlobalVar);
LLVMBool LLVMIsTypeAttribute(LLVMAttributeRef A);
LLVMBool LLVMIsUndef(LLVMValueRef Val);
LLVMTypeRef LLVMLabelTypeInContext(LLVMContextRef C);
unsigned LLVMLookupIntrinsicID(const char *Name, __SIZE_TYPE__ NameLen);
LLVMValueRef LLVMMDNodeInContext(LLVMContextRef C, LLVMValueRef *Vals, unsigned Count);
LLVMMetadataRef LLVMMDNodeInContext2(LLVMContextRef C, LLVMMetadataRef *MDs, __SIZE_TYPE__ Count);
LLVMValueRef LLVMMDStringInContext(LLVMContextRef C, const char *Str, unsigned SLen);
LLVMMetadataRef LLVMMDStringInContext2(LLVMContextRef C, const char *Str, __SIZE_TYPE__ SLen);
LLVMValueRef LLVMMetadataAsValue(LLVMContextRef C, LLVMMetadataRef MD);
LLVMTypeRef LLVMMetadataTypeInContext(LLVMContextRef C);
LLVMModuleRef LLVMModuleCreateWithNameInContext(const char *ModuleID, LLVMContextRef C);
LLVMModuleFlagBehavior LLVMModuleFlagEntriesGetFlagBehavior(LLVMModuleFlagEntry *Entries, unsigned Index);
const char *LLVMModuleFlagEntriesGetKey(LLVMModuleFlagEntry *Entries, unsigned Index, __SIZE_TYPE__ *Len);
LLVMMetadataRef LLVMModuleFlagEntriesGetMetadata(LLVMModuleFlagEntry *Entries, unsigned Index);
void LLVMMoveBasicBlockAfter(LLVMBasicBlockRef BB, LLVMBasicBlockRef MovePos);
void LLVMMoveBasicBlockBefore(LLVMBasicBlockRef BB, LLVMBasicBlockRef MovePos);
LLVMTypeRef LLVMPPCFP128TypeInContext(LLVMContextRef C);
LLVMTypeRef LLVMPointerType(LLVMTypeRef ElementType, unsigned AddressSpace);
LLVMTypeRef LLVMPointerTypeInContext(LLVMContextRef C, unsigned AddressSpace);
LLVMBool LLVMPointerTypeIsOpaque(LLVMTypeRef Ty);
void LLVMPositionBuilder(LLVMBuilderRef Builder, LLVMBasicBlockRef Block, LLVMValueRef Instr);
void LLVMPositionBuilderAtEnd(LLVMBuilderRef Builder, LLVMBasicBlockRef Block);
void LLVMPositionBuilderBefore(LLVMBuilderRef Builder, LLVMValueRef Instr);
void LLVMPositionBuilderBeforeDbgRecords(LLVMBuilderRef Builder, LLVMBasicBlockRef Block, LLVMValueRef Inst);
void LLVMPositionBuilderBeforeInstrAndDbgRecords(LLVMBuilderRef Builder, LLVMValueRef Instr);
void LLVMRemoveCallSiteEnumAttribute(LLVMValueRef C, LLVMAttributeIndex Idx, unsigned KindID);
void LLVMRemoveCallSiteStringAttribute(LLVMValueRef C, LLVMAttributeIndex Idx, const char *K, unsigned KLen);
void LLVMRemoveEnumAttributeAtIndex(LLVMValueRef F, LLVMAttributeIndex Idx, unsigned KindID);
void LLVMRemoveStringAttributeAtIndex(LLVMValueRef F, LLVMAttributeIndex Idx, const char *K, unsigned KLen);
void LLVMReplaceAllUsesWith(LLVMValueRef OldVal, LLVMValueRef NewVal);
void LLVMReplaceMDNodeOperandWith(LLVMValueRef V, unsigned Index, LLVMMetadataRef Replacement);
LLVMTypeRef LLVMScalableVectorType(LLVMTypeRef ElementType, unsigned ElementCount);
void LLVMSetAlignment(LLVMValueRef V, unsigned Bytes);
void LLVMSetArgOperand(LLVMValueRef Funclet, unsigned i, LLVMValueRef value);
void LLVMSetAtomicRMWBinOp(LLVMValueRef AtomicRMWInst, LLVMAtomicRMWBinOp BinOp);
void LLVMSetAtomicSingleThread(LLVMValueRef AtomicInst, LLVMBool SingleThread);
void LLVMSetAtomicSyncScopeID(LLVMValueRef AtomicInst, unsigned SSID);
void LLVMSetCleanup(LLVMValueRef LandingPad, LLVMBool Val);
void LLVMSetCmpXchgFailureOrdering(LLVMValueRef CmpXchgInst, LLVMAtomicOrdering Ordering);
void LLVMSetCmpXchgSuccessOrdering(LLVMValueRef CmpXchgInst, LLVMAtomicOrdering Ordering);
void LLVMSetCondition(LLVMValueRef Branch, LLVMValueRef Cond);
void LLVMSetCurrentDebugLocation(LLVMBuilderRef Builder, LLVMValueRef L);
void LLVMSetCurrentDebugLocation2(LLVMBuilderRef Builder, LLVMMetadataRef Loc);
void LLVMSetDLLStorageClass(LLVMValueRef Global, LLVMDLLStorageClass Class);
void LLVMSetDataLayout(LLVMModuleRef M, const char *DataLayoutStr);
void LLVMSetExact(LLVMValueRef DivOrShrInst, LLVMBool IsExact);
void LLVMSetExternallyInitialized(LLVMValueRef GlobalVar, LLVMBool IsExtInit);
void LLVMSetFastMathFlags(LLVMValueRef FPMathInst, LLVMFastMathFlags FMF);
void LLVMSetFunctionCallConv(LLVMValueRef Fn, unsigned CC);
void LLVMSetGC(LLVMValueRef Fn, const char *Name);
void LLVMSetGlobalConstant(LLVMValueRef GlobalVar, LLVMBool IsConstant);
void LLVMSetGlobalIFuncResolver(LLVMValueRef IFunc, LLVMValueRef Resolver);
void LLVMSetInitializer(LLVMValueRef GlobalVar, LLVMValueRef ConstantVal);
void LLVMSetInstDebugLocation(LLVMBuilderRef Builder, LLVMValueRef Inst);
void LLVMSetInstrParamAlignment(LLVMValueRef Instr, LLVMAttributeIndex Idx, unsigned Align);
void LLVMSetInstructionCallConv(LLVMValueRef Instr, unsigned CC);
void LLVMSetIsDisjoint(LLVMValueRef Inst, LLVMBool IsDisjoint);
void LLVMSetIsInBounds(LLVMValueRef GEP, LLVMBool InBounds);
void LLVMSetIsNewDbgInfoFormat(LLVMModuleRef M, LLVMBool UseNewFormat);
void LLVMSetLinkage(LLVMValueRef Global, LLVMLinkage Linkage);
void LLVMSetMetadata(LLVMValueRef Val, unsigned KindID, LLVMValueRef Node);
void LLVMSetModuleIdentifier(LLVMModuleRef M, const char *Ident, __SIZE_TYPE__ Len);
void LLVMSetModuleInlineAsm(LLVMModuleRef M, const char *Asm);
void LLVMSetModuleInlineAsm2(LLVMModuleRef M, const char *Asm, __SIZE_TYPE__ Len);
void LLVMSetNNeg(LLVMValueRef NonNegInst, LLVMBool IsNonNeg);
void LLVMSetNSW(LLVMValueRef ArithInst, LLVMBool HasNSW);
void LLVMSetNUW(LLVMValueRef ArithInst, LLVMBool HasNUW);
void LLVMSetNormalDest(LLVMValueRef InvokeInst, LLVMBasicBlockRef B);
void LLVMSetOperand(LLVMValueRef User, unsigned Index, LLVMValueRef Val);
void LLVMSetOrdering(LLVMValueRef MemoryAccessInst, LLVMAtomicOrdering Ordering);
void LLVMSetParamAlignment(LLVMValueRef Arg, unsigned Align);
void LLVMSetParentCatchSwitch(LLVMValueRef CatchPad, LLVMValueRef CatchSwitch);
void LLVMSetPersonalityFn(LLVMValueRef Fn, LLVMValueRef PersonalityFn);
void LLVMSetPrefixData(LLVMValueRef Fn, LLVMValueRef prefixData);
void LLVMSetPrologueData(LLVMValueRef Fn, LLVMValueRef prologueData);
void LLVMSetSection(LLVMValueRef Global, const char *Section);
void LLVMSetSourceFileName(LLVMModuleRef M, const char *Name, __SIZE_TYPE__ Len);
void LLVMSetSuccessor(LLVMValueRef Term, unsigned i, LLVMBasicBlockRef block);
void LLVMSetTailCall(LLVMValueRef CallInst, LLVMBool IsTailCall);
void LLVMSetTailCallKind(LLVMValueRef CallInst, LLVMTailCallKind kind);
void LLVMSetTarget(LLVMModuleRef M, const char *Triple);
void LLVMSetThreadLocal(LLVMValueRef GlobalVar, LLVMBool IsThreadLocal);
void LLVMSetThreadLocalMode(LLVMValueRef GlobalVar, LLVMThreadLocalMode Mode);
void LLVMSetUnnamedAddr(LLVMValueRef Global, LLVMBool HasUnnamedAddr);
void LLVMSetUnnamedAddress(LLVMValueRef Global, LLVMUnnamedAddr UnnamedAddr);
void LLVMSetUnwindDest(LLVMValueRef InvokeInst, LLVMBasicBlockRef B);
void LLVMSetValueName(LLVMValueRef Val, const char *Name);
void LLVMSetValueName2(LLVMValueRef Val, const char *Name, __SIZE_TYPE__ NameLen);
void LLVMSetVisibility(LLVMValueRef Global, LLVMVisibility Viz);
void LLVMSetVolatile(LLVMValueRef MemoryAccessInst, LLVMBool IsVolatile);
void LLVMSetWeak(LLVMValueRef CmpXchgInst, LLVMBool IsWeak);
LLVMValueRef LLVMSizeOf(LLVMTypeRef Ty);
LLVMTypeRef LLVMStructCreateNamed(LLVMContextRef C, const char *Name);
LLVMTypeRef LLVMStructGetTypeAtIndex(LLVMTypeRef StructTy, unsigned i);
void LLVMStructSetBody(LLVMTypeRef StructTy, LLVMTypeRef *ElementTypes, unsigned ElementCount, LLVMBool Packed);
LLVMTypeRef LLVMStructTypeInContext(LLVMContextRef C, LLVMTypeRef *ElementTypes, unsigned ElementCount, LLVMBool Packed);
LLVMTypeRef LLVMTargetExtTypeInContext(LLVMContextRef C, const char *Name, LLVMTypeRef *TypeParams, unsigned TypeParamCount, unsigned *IntParams, unsigned IntParamCount);
LLVMTypeRef LLVMTokenTypeInContext(LLVMContextRef C);
LLVMBool LLVMTypeIsSized(LLVMTypeRef Ty);
LLVMTypeRef LLVMTypeOf(LLVMValueRef Val);
LLVMBasicBlockRef LLVMValueAsBasicBlock(LLVMValueRef Val);
LLVMMetadataRef LLVMValueAsMetadata(LLVMValueRef Val);
LLVMBool LLVMValueIsBasicBlock(LLVMValueRef Val);
unsigned LLVMValueMetadataEntriesGetKind(LLVMValueMetadataEntry *Entries, unsigned Index);
LLVMMetadataRef LLVMValueMetadataEntriesGetMetadata(LLVMValueMetadataEntry *Entries, unsigned Index);
LLVMTypeRef LLVMVectorType(LLVMTypeRef ElementType, unsigned ElementCount);
LLVMTypeRef LLVMVoidTypeInContext(LLVMContextRef C);
LLVMTypeRef LLVMX86AMXTypeInContext(LLVMContextRef C);
LLVMTypeRef LLVMX86FP80TypeInContext(LLVMContextRef C);

/* FilLLVMJIT* JIT-compile API.

   Typical flow (no explicit initialization: the first call that needs the
   shim lazily dlopens it and boots LLVM's native target plus the JIT heap
   automatically):
     FilLLVMJITRef jit = FilLLVMJIT_Create();
     FilLLVMJIT_SetOption(jit, "opt-level", "2");      // optional tuning
     FilLLVMJIT_SetZExactPointerTable(jit, table);
     FilLLVMJITModuleRef modref =
         FilLLVMJIT_CompileModule(jit, module, "mymod");
     void *fn = FilLLVMJIT_GetFunctionAddress(modref, "myfunc");

   CompileModule consumes the module (not the session): after it
   returns, the module handle may not be used again.  The returned module
   owns everything the JIT needs (compiled code plus the resolver and
   table snapshots taken at compile time), so the session may be forgotten
   once compilation finishes.  On failure CompileModule returns NULL; see
   FilLLVMJIT_GetLastError for details.

   Addresses come back as raw pointers.  Cast the result of
   GetFunctionAddress, GetGlobalValueAddress, or GetPointerToGlobal to the
   matching function type and call it from any thread; no executor helper
   is needed or provided.

   Compile options (set with FilLLVMJIT_SetOption prior to compilation):
     "opt-level"                  "0".."3"   (default "2")
     "verify"                     "0"/"1"    (default "1")
     "filc-optimize"              "0"/"1"    (default "1")
     "filc-inline"                "0"/"1"    (default "1")
     "filc-inline-sroa"           "0"/"1"    (default "1")
     "filc-inline-early-cse"      "0"/"1"    (default "1")
     "filc-inline-jump-threading" "0"/"1"    (default "1")
     "filc-inline-cvp"            "0"/"1"    (default "1")
     "filc-inline-simplify-cfg"   "0"/"1"    (default "1")
     "filc-inline-inst-combine-early" "0"/"1" (default "1")
     "filc-inline-inst-combine-late"  "0"/"1" (default "1")
     "filc-inline-gvn"            "0"/"1"    (default "1")
     "filc-inline-sccp"           "0"/"1"    (default "1")
     "filc-inline-adce"           "0"/"1"    (default "1")
     "filc-inline-dse"            "0"/"1"    (default "1")
     "filc-dse"                   "0"/"1"    (default "0")

   KillUB and the FilPizlonator pass are NOT optional: they always run,
   regardless of options.

   Returns: FilLLVMJIT_* functions returning int yield 0 on success and
   nonzero on failure; CompileModule and the address lookups return NULL on
   failure; see FilLLVMJIT_GetLastError for details.
 */

typedef struct FilLLVMOpaqueJIT *FilLLVMJITRef;
typedef struct FilLLVMOpaqueJITModule *FilLLVMJITModuleRef;

/* Create a JIT.
 
   This object's job is to hold configuration parameters consumed by
   CompileModule. It has no other role or state.

   Hence, it's valid to create one of these, set some parameters, and then
   use it whenever you want to compile something with those parameters.

   On caveat: */
FilLLVMJITRef FilLLVMJIT_Create(void);

/* Set a key/value compile option (see list above).  Unknown keys fail. */
int FilLLVMJIT_SetOption(FilLLVMJITRef jit, const char *key, const char *value);

/* Set a mapping to use for resolving extern symbols. */
void FilLLVMJIT_SetGlobalMapping(FilLLVMJITRef jit,
                                 const char* key,
                                 const void* destination);

/* Provide a blessed pointer that LLVM IR can refer to using:
   
   void* zblessedptr(uintptr_t)

   This Fil-C intrinsic is only available in JIT mode. The compiler will crash
   unless the pointer passed to it is a compile-time constant integer that was
   added via this call. What's returned is the version of that pointer that has
   the full capability that you provided to BlessPointer. */
void FilLLVMJIT_BlessPointer(FilLLVMJITRef jit, void* ptr);

/* Run the Fil-C pass pipeline over mod, add it to the JIT under the given
   name, and return the compiled module.  This consumes the module (not the
   session): the caller must not use the module handle after the call,
   whether it succeeds or fails, but the session stays usable for further
   compiles and may be forgotten once compilation finishes.  The module is
   taken over as is (no bitcode copy is made); the caller must have built
   it in a dedicated context that nothing else shares, and that context
   moves to the JIT along with the module.  Extern declarations matching
   AddGlobalMapping entries are bound to their mapped addresses during
   compilation: the blessed lazy-resolution declarations
   (filc_llvm_jit_resolve, filc_llvm_jit_resolve_raw_pointer) keep the raw
   ABI and route through module-private wrappers carrying that compile's
   resolve callbacks, while other function names keep their declarations
   so FilPizlonator authors lazy-extern workers for them, whose bodies are
   then stubbed to return the mapped address wrapped as a flight {addr,addr}
   (a raw ptr constant would be a type mismatch against the flight-pointer
   ABI the call sites lower to). */
FilLLVMJITModuleRef FilLLVMJIT_CompileModule(FilLLVMJITRef jit,
                                             LLVMModuleRef mod,
                                             const char *name);

/* Address lookup. */
void *FilLLVMJIT_GetPointerToGlobal(FilLLVMJITModuleRef mod, const char *name);

#if 0
{ /* tell emacs what is up */
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* FILC_LLVM_JIT_H */
