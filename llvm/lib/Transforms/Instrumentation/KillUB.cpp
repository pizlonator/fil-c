//===- KillUB.cpp - remove all UB flags -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is a part of Fil-C. https://fil-c.org/
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Instrumentation/KillUB.h"

#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/AttributeMask.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/Instructions.h>
#include <vector>
#include <llvm/Support/Compiler.h>
#include <llvm/Support/ErrorHandling.h>
#ifdef FILC_PASS_ASSERTIONS
#define FILC_ASSERT_STR_IMPL(x) #x
#define FILC_ASSERT_STR(x) FILC_ASSERT_STR_IMPL(x)
#define FILC_ASSERT(expr)                                                      \
  do {                                                                         \
    if (LLVM_UNLIKELY(!(expr)))                                                \
      ::llvm::report_fatal_error(                                              \
          "Fil-C pass assertion failed: " #expr " (" __FILE__ ":"              \
          FILC_ASSERT_STR(__LINE__) ")");                                      \
  } while (0)
#undef assert
#define assert(expr) FILC_ASSERT(expr)
#endif

using namespace llvm;

PreservedAnalyses KillUBPass::run(Module &M, ModuleAnalysisManager&) {
  // This is just the beginning! There are probably more UB killings that have to happen.
  
  AttributeMask AM;
  AM.addAttribute(Attribute::AllocSize);
  AM.addAttribute(Attribute::NoUndef);
  AM.addAttribute(Attribute::Convergent);
  AM.addAttribute(Attribute::Dereferenceable);
  AM.addAttribute(Attribute::DereferenceableOrNull);
  AM.addAttribute(Attribute::Memory);
  AM.addAttribute(Attribute::NoAlias);
  AM.addAttribute(Attribute::NoCallback);
  AM.addAttribute(Attribute::NoCapture);
  AM.addAttribute(Attribute::Captures);
  AM.addAttribute(Attribute::NoDivergenceSource);
  AM.addAttribute(Attribute::NoExt);
  AM.addAttribute(Attribute::NoFree);
  AM.addAttribute(Attribute::DeadOnUnwind);
  AM.addAttribute(Attribute::NonNull);
  AM.addAttribute(Attribute::NoRecurse);
  AM.addAttribute(Attribute::NoRedZone);
  AM.addAttribute(Attribute::NoReturn);
  AM.addAttribute(Attribute::NoSync);
  AM.addAttribute(Attribute::Range);
  AM.addAttribute(Attribute::ReadNone);
  AM.addAttribute(Attribute::ReadOnly);
  AM.addAttribute(Attribute::Returned);
  AM.addAttribute(Attribute::Returned);
  AM.addAttribute(Attribute::WillReturn);
  AM.addAttribute(Attribute::Writable);
  AM.addAttribute(Attribute::WriteOnly);
  AM.addAttribute(Attribute::MustProgress);

  for (Function& F : M) {
    if (F.isIntrinsic()) {
      assert(F.isDeclaration());
      continue;
    }
    F.removeFnAttrs(AM);
    F.removeRetAttrs(AM);
    for (size_t Idx = F.arg_size(); Idx--;)
      F.removeParamAttrs(Idx, AM);
    std::vector<Instruction*> ToErase;
    std::vector<Instruction*> Insts;
    for (BasicBlock& BB : F) {
      for (Instruction& I : BB)
        Insts.push_back(&I);
    }
    for (Instruction* I : Insts) {
      I->dropUnknownNonDebugMetadata();
      I->dropPoisonGeneratingAnnotations();
      I->dropUBImplyingAttrsAndUnknownMetadata();
      bool MaybePoison = false;
      if (I->isShift()) {
        if (isPowerOf2_32(I->getType()->getScalarSizeInBits())) {
          // The amount modulo the width: what x86-64 and aarch64 do for scalar 32- and 64-bit shifts,
          // where the mask folds away.
          Instruction* Amount = BinaryOperator::Create(
            Instruction::And, I->getOperand(1),
            ConstantInt::get(I->getType(), I->getType()->getScalarSizeInBits() - 1),
            "filc_shift_amount", I);
          Amount->setDebugLoc(I->getDebugLoc());
          I->setOperand(1, Amount);
          continue;
        }
        MaybePoison = true;
      }
      if (isa<ExtractElementInst>(I) || isa<InsertElementInst>(I))   {
        if (FixedVectorType* VT = dyn_cast<FixedVectorType>(I->getOperand(0)->getType())) {
          // Bring the lane index into range, as the code generator does itself when it lowers the
          // access: wrapped for a power-of-two length, otherwise clamped to the last lane.
          unsigned IndexOperand = isa<InsertElementInst>(I) ? 2 : 1;
          Value* Index = I->getOperand(IndexOperand);
          uint64_t LastLane = VT->getNumElements() - 1;
          if (!isUIntN(Index->getType()->getScalarSizeInBits(), LastLane))
            continue; // The index type cannot name a lane past the end.
          Constant* LastLaneConstant = ConstantInt::get(Index->getType(), LastLane);
          Instruction* Clamped;
          if (isPowerOf2_32(VT->getNumElements())) {
            Clamped = BinaryOperator::Create(
              Instruction::And, Index, LastLaneConstant, "filc_lane_index", I);
          } else {
            Clamped = CallInst::Create(
              Intrinsic::getOrInsertDeclaration(&M, Intrinsic::umin, { Index->getType() }),
              { Index, LastLaneConstant }, "filc_lane_index", I);
          }
          Clamped->setDebugLoc(I->getDebugLoc());
          I->setOperand(IndexOperand, Clamped);
          continue;
        }
        MaybePoison = true;
      }
      if (BinaryOperator* BO = dyn_cast<BinaryOperator>(I)) {
        if (BO->isIntDivRem()) {
          // FIXME: If we're on arm64, we could just convert this to the arm64 intrinsic that does
          // this.
          Value* Dividend = BO->getOperand(0);
          Value* Divisor = BO->getOperand(1);
          Type* T = BO->getType();
          Instruction* Bad = new ICmpInst(
            BO, ICmpInst::ICMP_EQ, Divisor, Constant::getNullValue(T), "filc_divisor_is_zero");
          Bad->setDebugLoc(BO->getDebugLoc());
          Instruction* GoodDividend = SelectInst::Create(Bad, Constant::getNullValue(T), Dividend,
                                                         "filc_good_dividend", I);
          GoodDividend->setDebugLoc(BO->getDebugLoc());
          I->setOperand(0, GoodDividend);
          if (BO->getOpcode() == Instruction::SDiv || BO->getOpcode() == Instruction::SRem) {
            Instruction* IsMin = new ICmpInst(
              BO, ICmpInst::ICMP_EQ, Dividend,
              ConstantInt::get(T, APInt::getSignedMinValue(T->getScalarSizeInBits())),
              "filc_dividend_is_min");
            Instruction* IsMinusOne = new ICmpInst(
              BO, ICmpInst::ICMP_EQ, Divisor, Constant::getAllOnesValue(T), "filc_divisor_is_minus_one");
            Instruction* Overflows = BinaryOperator::Create(
              Instruction::And, IsMin, IsMinusOne, "filc_division_overflows", BO);
            Bad = BinaryOperator::Create(Instruction::Or, Bad, Overflows, "filc_bad_division", BO);
            for (Instruction* New : { IsMin, IsMinusOne, Overflows, Bad })
              New->setDebugLoc(BO->getDebugLoc());
          }
          Instruction* GoodDivisor = SelectInst::Create(
            Bad, ConstantInt::get(T, 1), Divisor, "filc_good_divisor", I);
          GoodDivisor->setDebugLoc(BO->getDebugLoc());
          I->setOperand(1, GoodDivisor);
          continue;
        }
      }
      if (CallBase* CI = dyn_cast<CallBase>(I)) {
        Function* Callee = dyn_cast<Function>(CI->getCalledOperand());
        if (!Callee || !Callee->isIntrinsic())
          continue;
        bool ShouldErase = false;
        switch (Callee->getIntrinsicID()) {
        case Intrinsic::assume:
        case Intrinsic::experimental_noalias_scope_decl:
        case Intrinsic::invariant_start:
        case Intrinsic::invariant_end:
          ShouldErase = true;
          break;
        case Intrinsic::launder_invariant_group:
        case Intrinsic::strip_invariant_group:
          CI->replaceAllUsesWith(CI->getArgOperand(0));
          ShouldErase = true;
          break;
        case Intrinsic::ctlz:
        case Intrinsic::cttz:
        case Intrinsic::abs:
          // Second argument false: ctlz and cttz of 0 are the width, abs of the minimum is the
          // minimum.
          CI->setArgOperand(1, ConstantInt::getFalse(M.getContext()));
          break;
        case Intrinsic::sshl_sat:
        case Intrinsic::ushl_sat:
        case Intrinsic::experimental_constrained_fptosi:
        case Intrinsic::experimental_constrained_fptoui:
        case Intrinsic::experimental_constrained_lrint:
        case Intrinsic::experimental_constrained_llrint:
        case Intrinsic::experimental_constrained_lround:
        case Intrinsic::experimental_constrained_llround:
          MaybePoison = true;
          break;
        default:
          break;
        }
        if (ShouldErase) {
          assert(!MaybePoison);
          if (InvokeInst* II = dyn_cast<InvokeInst>(CI))
            BranchInst::Create(II->getNormalDest(), CI)->setDebugLoc(CI->getDebugLoc());
          ToErase.push_back(CI);
        }
      }
      if (isa<FPToSIInst>(I) || isa<FPToUIInst>(I) || isa<ShuffleVectorInst>(I))
        MaybePoison = true;
      if (MaybePoison && canCreatePoison(cast<Operator>(I), /*ConsiderFlagsAndMetadata=*/false)) {
        Instruction* Frozen = new FreezeInst(I, "filc_defined", I->getNextNode());
        Frozen->setDebugLoc(I->getDebugLoc());
        I->replaceUsesWithIf(Frozen, [&] (Use& U) { return U.getUser() != Frozen; });
      }
    }
    for (Instruction* I : ToErase)
      I->eraseFromParent();
  }
  
  return PreservedAnalyses::none();
}

