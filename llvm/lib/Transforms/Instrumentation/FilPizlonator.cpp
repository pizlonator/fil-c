//===- FilPizlonator.cpp - apply GIMSO semantics to LLVM IR ---------------===//
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

#include "llvm/Transforms/Instrumentation/FilPizlonator.h"

#include <llvm/Analysis/CFG.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/IR/Comdat.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/TypedPointerType.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/IntrinsicsX86.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <llvm/Transforms/Utils/PromoteMemToReg.h>
#include <llvm/TargetParser/Triple.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <cctype>

using namespace llvm;

namespace {

static cl::opt<bool> lightVerbose(
  "filc-light-verbose", cl::desc("Make FilC slightly verbose"),
  cl::Hidden, cl::init(false));
static cl::opt<bool> verbose(
  "filc-verbose", cl::desc("Make FilC verbose"),
  cl::Hidden, cl::init(false));
static cl::opt<bool> ultraVerbose(
  "filc-ultra-verbose", cl::desc("Make FilC ultra verbose"),
  cl::Hidden, cl::init(false));
static cl::opt<bool> logAllocations(
  "filc-log-allocations", cl::desc("Make FilC emit code to log every allocation"),
  cl::Hidden, cl::init(false));
static cl::opt<bool> optimizeChecks(
  "filc-optimize-checks", cl::desc("Pick an optimal schedule for checks"),
  cl::Hidden, cl::init(true));
static cl::opt<bool> propagateChecksBackward(
  "filc-propagate-checks-backward", cl::desc("Perform backward propagation of checks"),
  cl::Hidden, cl::init(true));

// This has to match the FilC runtime.

static constexpr size_t GCMinAlign = 16;
static constexpr size_t FlightPtrAlign = 16;
static constexpr size_t WordSize = 8;
static constexpr size_t WordSizeShift = 3;

static constexpr uintptr_t ObjectAuxPtrMask = 0xfffffffffffflu;
static constexpr uintptr_t ObjectAuxFlagsShift = 48;

static constexpr size_t ObjectSize = 16;

static constexpr uint16_t SpecialTypeFunction = 1;
static constexpr uint16_t SpecialTypeMask = 15;

static constexpr uint16_t ObjectFlagGlobal = 1;
static constexpr uint16_t ObjectFlagReadonly = 2;
static constexpr uint16_t ObjectFlagFree = 4;
static constexpr uint16_t ObjectFlagGlobalAux = 16;
static constexpr uint16_t ObjectFlagsSpecialShift = 7;
static constexpr uint16_t ObjectFlagsAlignShift = 11;

static constexpr uintptr_t AtomicBoxBit = 1;

static constexpr unsigned NumUnwindRegisters = 2;

static constexpr size_t CCAlignment = 64;
static constexpr size_t CCInlineSize = 256;

static constexpr uint8_t ThreadStateCheckRequested = 2;
static constexpr uint8_t ThreadStateStopRequested = 4;
static constexpr uint8_t ThreadStateDeferredSignal = 8;

static constexpr size_t ThreadAllocatorOffset = 3072;
static constexpr size_t ThreadAllocatorSize = 208;
static constexpr size_t ThreadMaxInlineSizeClass = 416;
static constexpr size_t ThreadNumAllocators = (ThreadMaxInlineSizeClass / GCMinAlign) + 1;

static constexpr size_t MaxBytesBetweenPollchecks = 10000;

static constexpr uint64_t GenericSignature = 0;
static constexpr uint64_t NumFastTypes = 11;

enum class AccessKind {
  Read,
  Write
};

static inline const char* accessKindString(AccessKind AK) {
  switch (AK) {
  case AccessKind::Read:
    return "Read";
  case AccessKind::Write:
    return "Write";
  }
  llvm_unreachable("Bad access kind");
  return nullptr;
}

enum class ConstantKind {
  Global,
  Expr
};

enum class JmpBufKind {
  setjmp,
  _setjmp,
  sigsetjmp
};

struct ConstantTarget {
  ConstantTarget() { }

  ConstantTarget(ConstantKind Kind, GlobalValue* Target)
    : Kind(Kind)
    , Target(Target) {
  }

  explicit operator bool() const { return !!Target; }
  
  ConstantKind Kind { ConstantKind::Global };
  GlobalValue* Target { nullptr }; // Getter for globals (including functions), constexpr node for constexprs
};

struct ConstantRelocation {
  ConstantRelocation() { }

  ConstantRelocation(size_t Offset, ConstantKind Kind, GlobalValue* Target)
    : Offset(Offset)
    , Kind(Kind)
    , Target(Target) {
  }
  
  size_t Offset { 0 };
  ConstantKind Kind { ConstantKind::Global };
  GlobalValue* Target { nullptr }; // Getter for globals (including functions), constexpr node for constexprs
};

enum class ConstexprOpcode {
  AddPtrImmediate
};

enum class MemoryKind {
  Invalid,
  CC,
  GlobalInit,
  ThreadLocalInit,
  Heap,
  LocalExplicit,
  LocalNaked
};

struct ValuePtr {
  ValuePtr() { }

  ValuePtr(Value* V, size_t PtrIndex)
    : V(V), PtrIndex(PtrIndex) {
  }

  Value* V { nullptr };
  size_t PtrIndex { 0 };

  bool operator==(const ValuePtr& Other) const {
    return V == Other.V && PtrIndex == Other.PtrIndex;
  }

  size_t hash() const {
    return std::hash<Value*>()(V) + PtrIndex;
  }
};

} // anonymous namespace

template<> struct std::hash<ValuePtr> {
  size_t operator()(const ValuePtr& Key) const {
    return Key.hash();
  }
};

namespace {

struct FunctionOriginKey {
  FunctionOriginKey() = default;

  FunctionOriginKey(Function* OldF, DIScope* Scope, bool CanCatch)
    : OldF(OldF), Scope(Scope), CanCatch(CanCatch) {
  }

  Function* OldF { nullptr };
  DIScope* Scope { nullptr };
  bool CanCatch { false };

  bool operator==(const FunctionOriginKey& Other) const {
    return OldF == Other.OldF && Scope == Other.Scope && CanCatch == Other.CanCatch;
  }

  size_t hash() const {
    return std::hash<Function*>()(OldF) + std::hash<DIScope*>()(Scope)
      + static_cast<size_t>(CanCatch);
  }
};

struct EHDataKey {
  EHDataKey() = default;
  EHDataKey(LandingPadInst* LPI) : LPI(LPI) { }

  LandingPadInst* LPI { nullptr };

  bool operator==(const EHDataKey& Other) const {
    if (!LPI)
      return !Other.LPI;

    if (!Other.LPI)
      return false;

    if (LPI->isCleanup() != Other.LPI->isCleanup())
      return false;

    if (LPI->getNumClauses() != Other.LPI->getNumClauses())
      return false;

    for (unsigned Idx = LPI->getNumClauses(); Idx--;) {
      if (LPI->getClause(Idx) != Other.LPI->getClause(Idx))
        return false;
    }

    return true;
  }

  size_t hash() const {
    if (!LPI)
      return 0;

    size_t Result = 0;
    Result += LPI->isCleanup();
    Result += LPI->getNumClauses();
    for (unsigned Idx = LPI->getNumClauses(); Idx--;)
      Result += std::hash<Constant*>()(LPI->getClause(Idx));
    return Result;
  }
};

struct OriginKey {
  OriginKey() = default;

  OriginKey(Function* OldF, DILocation* DI, bool CanCatch, LandingPadInst* LPI)
    : OldF(OldF), DI(DI), CanCatch(CanCatch), LPI(LPI) {
    if (LPI)
      assert(CanCatch);
  }

  Function* OldF { nullptr };
  DILocation* DI { nullptr };
  bool CanCatch { false };
  LandingPadInst* LPI { nullptr };

  bool operator==(const OriginKey& Other) const {
    return OldF == Other.OldF && DI == Other.DI && CanCatch == Other.CanCatch
      && EHDataKey(LPI) == EHDataKey(Other.LPI);
  }

  size_t hash() const {
    return std::hash<Function*>()(OldF) + std::hash<DILocation*>()(DI) + static_cast<size_t>(CanCatch)
      + EHDataKey(LPI).hash();
  }
};

struct InlineFrameKey {
  InlineFrameKey() = default;

  InlineFrameKey(Function* OldF, DILocalScope* Scope, DILocation* InlinedAt, bool CanCatch)
    : OldF(OldF), Scope(Scope), InlinedAt(InlinedAt), CanCatch(CanCatch) {
  }

  Function* OldF { nullptr };
  DILocalScope* Scope { nullptr };
  DILocation* InlinedAt { nullptr };
  bool CanCatch { false };

  bool operator==(const InlineFrameKey& Other) const {
    return OldF == Other.OldF && Scope == Other.Scope && InlinedAt == Other.InlinedAt
      && CanCatch == Other.CanCatch;
  }

  size_t hash() const {
    return std::hash<Function*>()(OldF) + std::hash<DILocalScope*>()(Scope)
      + std::hash<DILocation*>()(InlinedAt) + static_cast<size_t>(CanCatch);
  }
};

} // anonymous namespace

template<> struct std::hash<FunctionOriginKey> {
  size_t operator()(const FunctionOriginKey& Key) const {
    return Key.hash();
  }
};

template<> struct std::hash<EHDataKey> {
  size_t operator()(const EHDataKey& Key) const {
    return Key.hash();
  }
};

template<> struct std::hash<OriginKey> {
  size_t operator()(const OriginKey& Key) const {
    return Key.hash();
  }
};

template<> struct std::hash<InlineFrameKey> {
  size_t operator()(const InlineFrameKey& Key) const {
    return Key.hash();
  }
};

namespace {

static constexpr size_t NumSpecialFrameObjects = 0;

enum class MATokenKind {
  None,
  Error,
  Directive,
  Identifier,
  Integer,
  Comma,
  Plus,
  EndLine
};

inline const char* MATokenKindString(MATokenKind Kind) {
  switch (Kind) {
  case MATokenKind::None:
    return "None";
  case MATokenKind::Error:
    return "Error";
  case MATokenKind::Directive:
    return "Directive";
  case MATokenKind::Identifier:
    return "Identifier";
  case MATokenKind::Integer:
    return "Integer";
  case MATokenKind::Comma:
    return "Comma";
  case MATokenKind::Plus:
    return "Plus";
  case MATokenKind::EndLine:
    return "EndLine";
  }
  llvm_unreachable("Bad MATokenKind");
  return nullptr;
}

struct MAToken {
  MAToken() = default;

  MAToken(MATokenKind Kind, const std::string& Str): Kind(Kind), Str(Str) {}

  MATokenKind Kind { MATokenKind::None };
  std::string Str;
};

class MATokenizer {
  Module& M;
  std::string MA;
  size_t Idx { 0 };

  void skipWhitespace() {
    while (Idx < MA.size() && MA[Idx] != '\n' && isspace(MA[Idx]))
      Idx++;
  }

  void skipID() {
    while (Idx < MA.size() && (isalnum(MA[Idx]) ||
                               MA[Idx] == '_' ||
                               MA[Idx] == '$' ||
                               MA[Idx] == '@' ||
                               MA[Idx] == '.'))
      Idx++;
  }

  void skipInteger() {
    while (Idx < MA.size() && isdigit(MA[Idx]))
      Idx++;
  }
  
public:
  MATokenizer(Module& M, const std::string& MA): M(M), MA(MA) {}

  bool isAtEnd() const { return Idx >= MA.size(); }

  void error() {
    errs() << "Error parsing module asm:\n" << MA << "\n";
    errs() << "The whole module:\n" << M << "\n";
    llvm_unreachable("Error parsing module asm");
  }

  MAToken getNext() {
    skipWhitespace();
    if (isAtEnd())
      return MAToken();
    if (MA[Idx] == '\n') {
      Idx++;
      return MAToken(MATokenKind::EndLine, "\n");
    }
    if (MA[Idx] == ',') {
      Idx++;
      return MAToken(MATokenKind::Comma, ",");
    }
    if (MA[Idx] == '+') {
      Idx++;
      return MAToken(MATokenKind::Plus, "+");
    }
    if (MA[Idx] == '.') {
      size_t Start = Idx;
      skipID();
      return MAToken(MATokenKind::Directive, MA.substr(Start, Idx - Start));
    }
    if (isalpha(MA[Idx]) || MA[Idx] == '_') {
      size_t Start = Idx;
      skipID();
      return MAToken(MATokenKind::Identifier, MA.substr(Start, Idx - Start));
    }
    if (isdigit(MA[Idx])) {
      size_t Start = Idx;
      skipInteger();
      return MAToken(MATokenKind::Integer, MA.substr(Start, Idx - Start));
    }
    return MAToken(MATokenKind::Error, MA.substr(Idx, MA.size() - Idx));
  }

  MAToken getNextSpecific(MATokenKind Kind) {
    MAToken Tok = getNext();
    if (Tok.Kind != Kind) {
      errs() << "Expected " << MATokenKindString(Kind) << " but got: " << Tok.Str << "\n";
      error();
    }
    return Tok;
  }
};

enum class AIState {
  Unknown,
  Uninitialized,
  Initialized,
  MaybeInitialized
};

AIState mergeAIState(AIState A, AIState B) {
  if (A == B)
    return A;
  if (A == AIState::Unknown)
    return B;
  if (B == AIState::Unknown)
    return A;
  return AIState::MaybeInitialized;
}

__attribute__((used)) const char* aiStateString(AIState State) {
  switch (State) {
  case AIState::Unknown:
    return "Unknown";
  case AIState::Uninitialized:
    return "Uninitialized";
  case AIState::Initialized:
    return "Initialized";
  case AIState::MaybeInitialized:
    return "MaybeInitialized";
  }
  llvm_unreachable("bad AIState");
  return nullptr;
}

enum class CheckKind {
  // The order of CheckKinds is important for sorting AccessChecks!

  // Check if the object is valid (not NULL). This check has to happen before any of the other
  // checks.
  ValidObject,

  // Indicates that we already know that CanonicalPtr + Offset is aligned to Size, and there's no
  // need to check, but we can rely on it. This kind should only appear in the final ChecksForInst
  // list but never during backward or forward propagation. Not all CheckKinds have a Known variant.
  // We need a Known variant for Alignment because it influences how the TypeIsInt and UpperBound
  // check works. For CheckKinds that have no Known variant, the CheckKind will simply be excluded
  // from ChecksForInst if it's known already.
  KnownAlignment,

  // Check if the CanonicalPtr + Offset is aligned to Size.
  Alignment,

  // Check if the object is not readonly.
  CanWrite,

  // Indicates that we already know that CanoalinicalPtr + Offset is greaterequal object->lower, and
  // there's no need to check, but we can rely on it. This kind should only appear in the final
  // ChecksForInst list but never during backward or forward propagation.
  KnownLowerBound,

  // Check that CanonicalPtr + Offset is greaterequal object->lower.
  LowerBound,

  // Check that CanonicalPtr + Offset is lessequal object->upper.
  UpperBound,

  // Get the aux ptr. If it's null, just return the null.
  //
  // FIXME: It would be great if stuff like this was tracked on something broader than the canonical
  // ptr. We should identify the "root ptr", i.e. the common one ignoring all arithmetic. The trick is
  // how to then canonicalize those.
  //
  // And the same thing can be said for ValidObject, EnsureAuxPtr, and maybe even Alignment (though
  // that one is harder).
  //
  // FIXME #2: If we've previously ensured aux ptr, then loads don't have to null check the aux.
  GetAuxPtr,

  // Ensure the aux ptr. If it's null, create one. This doesn't load the aux ptr at all; it assumes
  // that already happened with a GetAuxPtr. It just checks if that ptr is null, and if it is, it does
  // the ensure. However, doing the ensure does reload it first to make sure it didn't become nonnull.
  EnsureAuxPtr,

  // Check that the object isn't free. Checking any TypeIsInt or TypeIsPtr automatically checks that
  // the object isn't free.
  NotFree
};

bool IsKnownCheckKind(CheckKind CK) {
  switch (CK) {
  case CheckKind::KnownAlignment:
  case CheckKind::KnownLowerBound:
    return true;
  case CheckKind::ValidObject:
  case CheckKind::CanWrite:
  case CheckKind::Alignment:
  case CheckKind::UpperBound:
  case CheckKind::LowerBound:
  case CheckKind::NotFree:
  case CheckKind::GetAuxPtr:
  case CheckKind::EnsureAuxPtr:
    return false;
  }
  llvm_unreachable("Bad CheckKind.");
  return false;
}

CheckKind FundamentalCheckKind(CheckKind CK) {
  switch (CK) {
  case CheckKind::KnownAlignment:
    return CheckKind::Alignment;
  case CheckKind::KnownLowerBound:
    return CheckKind::LowerBound;
  case CheckKind::ValidObject:
  case CheckKind::CanWrite:
  case CheckKind::Alignment:
  case CheckKind::UpperBound:
  case CheckKind::LowerBound:
  case CheckKind::NotFree:
  case CheckKind::GetAuxPtr:
  case CheckKind::EnsureAuxPtr:
    return CK;
  }
  llvm_unreachable("Bad CheckKind.");
  return CheckKind::ValidObject;
}

raw_ostream& operator<<(raw_ostream& OS, CheckKind CK) {
  switch (CK) {
  case CheckKind::ValidObject:
    OS << "ValidObject";
    return OS;
  case CheckKind::KnownAlignment:
    OS << "KnownAlignment";
    return OS;
  case CheckKind::Alignment:
    OS << "Alignment";
    return OS;
  case CheckKind::CanWrite:
    OS << "CanWrite";
    return OS;
  case CheckKind::UpperBound:
    OS << "UpperBound";
    return OS;
  case CheckKind::KnownLowerBound:
    OS << "KnownLowerBound";
    return OS;
  case CheckKind::LowerBound:
    OS << "LowerBound";
    return OS;
  case CheckKind::GetAuxPtr:
    OS << "GetAuxPtr";
    return OS;
  case CheckKind::EnsureAuxPtr:
    OS << "EnsureAuxPtr";
    return OS;
  case CheckKind::NotFree:
    OS << "NotFree";
    return OS;
  }
  llvm_unreachable("Bad CheckKind");
  return OS;
}

enum class AIDirection {
  Forward,
  Backward
};

struct CombinedDI {
  std::unordered_set<DILocation*> Locations; // This set may contain null.

  CombinedDI() = default;
  
  CombinedDI(DILocation* Location) {
    Locations.insert(Location);
  }

  bool operator==(const CombinedDI& Other) const {
    return Locations == Other.Locations;
  }

  size_t hash() const {
    size_t Result = Locations.size();
    for (DILocation* Location : Locations)
      Result += std::hash<DILocation*>()(Location);
    return Result;
  }
};

} // anonymous namespace

template<> struct std::hash<CombinedDI> {
  size_t operator()(const CombinedDI& Key) const {
    return Key.hash();
  }
};

template<> struct std::hash<std::pair<const CombinedDI*, const CombinedDI*>> {
  size_t operator()(const std::pair<const CombinedDI*, const CombinedDI*>& Key) const {
    return std::hash<const CombinedDI*>()(Key.first) + 3 * std::hash<const CombinedDI*>()(Key.second);
  }
};

namespace {

int64_t PositiveModulo(int64_t Offset, int64_t Alignment) {
  int64_t Result = Offset % Alignment;
  if (Result < 0)
    Result += Alignment;
  assert(Result >= 0);
  assert(Result < Alignment);
  return Result;
}

struct AccessCheck {
  Value* CanonicalPtr { nullptr };
  int64_t Offset { 0 };
  int64_t Size { 0 };
  CheckKind CK { CheckKind::ValidObject };

  AccessCheck() = default;
  
  AccessCheck(Value* CanonicalPtr, int64_t Offset, int64_t Size, CheckKind CK):
    CanonicalPtr(CanonicalPtr), Offset(Offset), Size(Size), CK(CK) {
    assert(CanonicalPtr);
    assert((int32_t)Offset == Offset);
    assert(Size >= 0);
    switch (CK) {
    case CheckKind::ValidObject:
    case CheckKind::CanWrite:
    case CheckKind::NotFree:
    case CheckKind::GetAuxPtr:
    case CheckKind::EnsureAuxPtr:
      assert(!Size);
      assert(!Offset);
      return;
    case CheckKind::Alignment:
    case CheckKind::KnownAlignment:
      assert(Size >= 1);
      assert(Size <= static_cast<int64_t>(WordSize));
      assert(!((Size - 1) & Size));
      assert(Offset >= 0);
      assert(Offset < Size);
      return;
    case CheckKind::UpperBound:
    case CheckKind::LowerBound:
    case CheckKind::KnownLowerBound:
      assert(!Size);
      return;
    }
    llvm_unreachable("Bad CheckKind.");
  }

  explicit operator bool() const {
    return !!CanonicalPtr;
  }

  // The ordering is such that:
  //
  // - Checks for canonical ptrs are clustered together and CheckKinds are ordered in a canonical way.
  //   This allows us to easily craft algorithms that understand the set of checks for a canonical
  //   ptr. For example, knowing that the alignment-related checks come before the bounds checks, and
  //   that lower bounds comes before upper bounds, makes it easier to extract the information we need
  //   for any canonical ptr.
  //
  // - Stronger checks - ones that subsume others - come first. For example, for lower bounds, the
  //   offsets are sorted in ascending order, so that a check that says P + 1 > Lower comes before
  //   (and so subsumes) a check that says P + 2 > Lower.
  bool operator<(const AccessCheck& Other) const {
    if (CanonicalPtr != Other.CanonicalPtr)
      return CanonicalPtr < Other.CanonicalPtr;

    // This is important: we require that the CheckKinds fit into the list in ascending order,
    // according to the order they are declared in CheckKind, ignoring whether they are known/merged
    // or not.
    if (FundamentalCheckKind(CK) != FundamentalCheckKind(Other.CK))
      return FundamentalCheckKind(CK) < FundamentalCheckKind(Other.CK);

    if (FundamentalCheckKind(CK) == CheckKind::Alignment && Size != Other.Size)
      return Size > Other.Size;

    if (Offset != Other.Offset) {
      if (CK == CheckKind::UpperBound)
        return Offset > Other.Offset;
      else
        return Offset < Other.Offset;
    }

    if (Size != Other.Size)
      return Size > Other.Size;

    // If everything else is equal, then the order is such that known comes before unknown.
    return CK < Other.CK;
  }

  // Note: we say that two AccessChecks are equal even if their DI's are different.
  bool operator==(const AccessCheck& Other) const {
    return Size == Other.Size && CanonicalPtr == Other.CanonicalPtr && Offset == Other.Offset
      && CK == Other.CK;
  }

  bool operator!=(const AccessCheck& Other) const {
    return !(*this == Other);
  }

  void print(raw_ostream& OS) const {
    OS << CK << "(" << CanonicalPtr << ":" << CanonicalPtr->getName();
    switch (CK) {
    case CheckKind::ValidObject:
    case CheckKind::CanWrite:
    case CheckKind::NotFree:
    case CheckKind::GetAuxPtr:
    case CheckKind::EnsureAuxPtr:
      break;
    case CheckKind::Alignment:
    case CheckKind::KnownAlignment:
      OS << ", Size = " << Size << ", Offset = " << Offset;
      break;
    case CheckKind::LowerBound:
    case CheckKind::KnownLowerBound:
    case CheckKind::UpperBound:
      OS << ", Offset = " << Offset;
      break;
    }
    OS << ")";
  }
};

raw_ostream& operator<<(raw_ostream& OS, const AccessCheck& AC) {
  AC.print(OS);
  return OS;
}

template<typename T>
raw_ostream& PrintVector(raw_ostream& OS, const std::vector<T>& V) {
  for (size_t Index = 0; Index < V.size(); ++Index) {
    if (Index)
      OS << ", ";
    OS << V[Index];
  }
  return OS;
}

raw_ostream& operator<<(raw_ostream& OS, const std::vector<AccessCheck>& Checks) {
  return PrintVector(OS, Checks);
}

struct AccessCheckWithDI : AccessCheck {
  const CombinedDI* DI { nullptr };

  AccessCheckWithDI() = default;

  AccessCheckWithDI(Value* CanonicalPtr, int64_t Offset, int64_t Size, CheckKind CK,
                    const CombinedDI* DI):
    AccessCheck(CanonicalPtr, Offset, Size, CK), DI(DI) {
  }

  AccessCheckWithDI(const AccessCheck& AC):
    AccessCheck(AC) {
  }

  AccessCheckWithDI& operator=(const AccessCheck& AC) {
    *(AccessCheck*)this = AC;
    DI = nullptr;
    return *this;
  }
};

raw_ostream& operator<<(raw_ostream& OS, const std::vector<AccessCheckWithDI>& Checks) {
  return PrintVector(OS, Checks);
}

struct ChecksOrBottom {
  bool Bottom;
  std::vector<AccessCheck> Checks;

  ChecksOrBottom(): Bottom(true) {}
};

struct ChecksWithDIOrBottom {
  bool Bottom;
  std::vector<AccessCheckWithDI> Checks;

  ChecksWithDIOrBottom(): Bottom(true) {}
};

// Used for cases where we require a known offset, and so if we can't get one, we don't return the
// underlying pointer (i.e. HighP might be a GEP).
struct PtrAndOffset {
  Value* HighP { nullptr };
  int64_t Offset { 0 };

  PtrAndOffset() = default;

  PtrAndOffset(Value* HighP, int64_t Offset): HighP(HighP), Offset(Offset) {}
};

enum class PtrRandomness {
  /* Not random accessed (we can deduce the offset statically). */
  NotRandom,

  /* Random accessed (we do not know the offset statically). */
  Random
};

// Used for cases where we require the underlying pointer, and so we might not be able to produce an
// offset.
struct PtrAndRandom {
  Value* P { nullptr };
  PtrRandomness R { PtrRandomness::NotRandom };
  int64_t Offset;

  PtrAndRandom() = default;

  PtrAndRandom(Value* P, PtrRandomness R, int64_t Offset): P(P), R(R), Offset(Offset) { }

  PtrAndRandom plus(int64_t Offset) const {
    PtrAndRandom Result = *this;
    Result.Offset += Offset;
    if ((int32_t)Result.Offset != Result.Offset) {
      Result.R = PtrRandomness::Random;
      Result.Offset = 0;
    }
    return Result;
  }
};

struct GEPKey {
  Type* SourceTy { nullptr };
  Value* Pointer { nullptr };
  std::vector<Value*> Indices;

  GEPKey() = default;

  GEPKey(GetElementPtrInst* GEP):
    SourceTy(GEP->getSourceElementType()), Pointer(GEP->getPointerOperand()) {
    for (Value* Index : GEP->indices())
      Indices.push_back(Index);
  }

  bool operator==(const GEPKey& Other) const {
    return SourceTy == Other.SourceTy && Pointer == Other.Pointer && Indices == Other.Indices;
  }

  size_t hash() const {
    size_t Result = std::hash<Type*>()(SourceTy) + std::hash<Value*>()(Pointer) + Indices.size();
    for (Value* Index : Indices)
      Result += std::hash<Value*>()(Index);
    return Result;
  }
};

struct OptimizedAccessCheckOriginKey {
  uint32_t Size { 0 };
  uint8_t Alignment { 0 };
  uint8_t AlignmentOffset { 0 };
  bool NeedsWrite { false };
  DILocation* ScheduledDI { nullptr };
  const CombinedDI* SemanticDI { nullptr };

  OptimizedAccessCheckOriginKey() = default;

  OptimizedAccessCheckOriginKey(
    uint32_t Size, uint8_t Alignment, uint8_t AlignmentOffset, bool NeedsWrite,
    DILocation* ScheduledDI, const CombinedDI* SemanticDI):
    Size(Size), Alignment(Alignment), AlignmentOffset(AlignmentOffset),
    NeedsWrite(NeedsWrite), ScheduledDI(ScheduledDI), SemanticDI(SemanticDI) {
  }

  bool operator==(const OptimizedAccessCheckOriginKey& Other) const {
    return Size == Other.Size && Alignment == Other.Alignment
      && AlignmentOffset == Other.AlignmentOffset
      && NeedsWrite == Other.NeedsWrite && ScheduledDI == Other.ScheduledDI
      && SemanticDI == Other.SemanticDI;
  }

  size_t hash() const {
    return static_cast<size_t>(Size) + static_cast<size_t>(Alignment)
      + static_cast<size_t>(AlignmentOffset)
      + static_cast<size_t>(NeedsWrite) + std::hash<DILocation*>()(ScheduledDI)
      + std::hash<const CombinedDI*>()(SemanticDI);
  }
};

struct AlignmentAndOffset {
  uint8_t Alignment { 0 };
  uint8_t AlignmentOffset { 0 };

  AlignmentAndOffset() = default;

  AlignmentAndOffset(uint8_t Alignment, uint8_t AlignmentOffset):
    Alignment(Alignment), AlignmentOffset(AlignmentOffset) {
  }

  bool operator==(const AlignmentAndOffset& Other) const {
    return Alignment == Other.Alignment && AlignmentOffset == Other.AlignmentOffset;
  }

  size_t hash() const {
    return std::hash<uint8_t>()(Alignment) + AlignmentOffset;
  }
};

struct OptimizedAlignmentContradictionOriginKey {
  std::vector<AlignmentAndOffset> Alignments;
  DILocation* ScheduledDI { nullptr };
  const CombinedDI* SemanticDI { nullptr };

  OptimizedAlignmentContradictionOriginKey() = default;

  OptimizedAlignmentContradictionOriginKey(const std::vector<AlignmentAndOffset>& Alignments,
                                           DILocation* ScheduledDI, const CombinedDI* SemanticDI):
    Alignments(Alignments), ScheduledDI(ScheduledDI), SemanticDI(SemanticDI) {
  }

  bool operator==(const OptimizedAlignmentContradictionOriginKey& Other) const {
    return Alignments == Other.Alignments && ScheduledDI == Other.ScheduledDI
      && SemanticDI == Other.SemanticDI;
  }

  size_t hash() const {
    size_t Result = Alignments.size();
    for (AlignmentAndOffset Alignment : Alignments)
      Result += Alignment.hash();
    Result += std::hash<DILocation*>()(ScheduledDI);
    Result += std::hash<const CombinedDI*>()(SemanticDI);
    return Result;
  }
};

} // anonymous namespace

template<> struct std::hash<GEPKey> {
  size_t operator()(const GEPKey& Key) const {
    return Key.hash();
  }
};

template<> struct std::hash<OptimizedAccessCheckOriginKey> {
  size_t operator()(const OptimizedAccessCheckOriginKey& Key) const {
    return Key.hash();
  }
};

template<> struct std::hash<OptimizedAlignmentContradictionOriginKey> {
  size_t operator()(const OptimizedAlignmentContradictionOriginKey& Key) const {
    return Key.hash();
  }
};

namespace {

template<typename VectorT, typename FuncT>
void EraseIf(VectorT& V, const FuncT& F) {
  size_t SrcIndex = 0;
  size_t DstIndex = 0;
  while (SrcIndex < V.size()) {
    auto Value = std::move(V[SrcIndex++]);
    if (!F(Value))
      V[DstIndex++] = std::move(Value);
  }
  V.resize(DstIndex);
}

enum class CapabilityInferenceState {
  Bottom,
  Definite,
  Top
};

struct InferredCapability {
  CapabilityInferenceState State { CapabilityInferenceState::Bottom };
  Value* Capability;

  InferredCapability() = default;

  InferredCapability(CapabilityInferenceState State, Value* Capability):
    State(State), Capability(Capability) {
    if (State != CapabilityInferenceState::Definite)
      assert(!Capability);
    else
      assert(Capability);
  }

  bool operator==(const InferredCapability& Other) const {
    return State == Other.State && Capability == Other.Capability;
  }

  bool merge(const InferredCapability& Other) {
    if (*this == Other)
      return false;
    
    switch (State) {
    case CapabilityInferenceState::Bottom:
      *this = Other;
      return true;

    case CapabilityInferenceState::Definite:
      if (Other.State == CapabilityInferenceState::Bottom)
        return false;
      State = CapabilityInferenceState::Top;
      Capability = nullptr;
      return true;

    case CapabilityInferenceState::Top:
      return false;
    }

    llvm_unreachable("Bad inference state");
    return false;
  }
};

struct IntrinsicAccessDetails {
  AccessKind AK { AccessKind::Read };
  Type* T { nullptr };
  Value* Ptr { nullptr };
  Value* Mask { nullptr };
  int64_t Alignment { 0 };

  IntrinsicAccessDetails() = default;

  explicit operator bool() const { return !!T; }
};

struct AsmIO {
  Type* T { nullptr };
  int Matching { -1 };
  int Index { -1 };

  AsmIO() = default;

  AsmIO(Type* T, int Matching, int Index):
    T(T), Matching(Matching), Index(Index) {
    assert(Matching >= -1);
    assert(Index >= -1);
  }
};

enum class AsmConstraintKind {
  Input,
  Output,
  Other
};

struct AsmConstraint {
  AsmConstraintKind Kind { AsmConstraintKind::Other };
  unsigned Index { 0 };
  unsigned NewIndex { UINT_MAX };
  int MatchingIndex { -1 };
  std::string String;
  bool IsIndirect { false };

  AsmConstraint() = default;

  AsmConstraint(AsmConstraintKind Kind, unsigned Index, std::string String)
    : Kind(Kind), Index(Index), String(String) {
  }
};

enum class FastArgType : unsigned {
  Int64,
  Float,
  Double,
  LongDouble,
  Vec128,
  Vec256,
  Vec512,
  Ptr,
  Reserved1,
  Reserved2,
  Reserved3,
  Invalid
};

uint64_t SafeAdd64(uint64_t A, uint64_t B) {
  uint64_t Result;
  bool DidOverflow = __builtin_add_overflow(A, B, &Result);
  if (DidOverflow)
    llvm_unreachable("Unexpected overflow on add");
  return Result;
}

uint64_t SafeMul64(uint64_t A, uint64_t B) {
  uint64_t Result;
  bool DidOverflow = __builtin_mul_overflow(A, B, &Result);
  if (DidOverflow)
    llvm_unreachable("Unexpected overflow on mul");
  return Result;
}

class FastTypeAccumulator {
public:
  FastTypeAccumulator() { }

  void addType(FastArgType T) {
    if (T == FastArgType::Invalid || Num == 16) {
      Valid = false;
      return;
    }
    uint64_t TV = static_cast<uint64_t>(T);
    assert(TV < NumFastTypes);
    Result = SafeAdd64(Result, Step);
    Result = SafeAdd64(Result, SafeMul64(TV, Step));
    Step = SafeMul64(Step, NumFastTypes);
    Num = SafeAdd64(Num, 1);
  }

  uint64_t getResult() const { return Result; }
  uint64_t getNum() const { return Num; }
  bool isValid() const { return Valid; }

private:
  uint64_t Result { 0 };
  uint64_t Num { 0 };
  uint64_t Step { 1 };
  bool Valid { true };
};

enum class ArgKind {
  Direct,
  ByVal
};

struct ArgInfo {
  Type* T { nullptr };
  ArgKind AK { ArgKind::Direct };
  Align A;

  ArgInfo() = default;

  ArgInfo(Type* T, ArgKind AK, Align A): T(T), AK(AK), A(A) {
  }
};

enum class PointerKind {
  // It's just a regular heap pointer.
  Escaping,

  // It's a pointer to something that doesn't escape, but that gets random accessed, so we need to
  // have an explicit filc_stack_aux for it. This means that the filc_stack_aux itself will escape
  // from LLVM IR's perspective, but at least it's a stack allocation, not a heap allocation.
  LocalExplicit,

  // It's a pointer to something that doesn't escape, and does not get random accessed. So, we do have
  // a stack aux for it, but it's "naked" (has no header, isn't tracked). This means that all stores to
  // this alloca need to replicate to the lowers array.
  LocalNaked
};

raw_ostream& operator<<(raw_ostream& OS, PointerKind PK) {
  switch (PK) {
  case PointerKind::Escaping:
    OS << "Escaping";
    return OS;
  case PointerKind::LocalExplicit:
    OS << "LocalExplicit";
    return OS;
  case PointerKind::LocalNaked:
    OS << "LocalNaked";
    return OS;
  }
  llvm_unreachable("Bad PointerKind");
  return OS;
}

PointerKind mergePointerKinds(PointerKind A, PointerKind B) {
  switch (A) {
  case PointerKind::Escaping:
    return PointerKind::Escaping;
  case PointerKind::LocalExplicit:
    if (B == PointerKind::Escaping)
      return PointerKind::Escaping;
    return PointerKind::LocalExplicit;
  case PointerKind::LocalNaked:
    return B;
  }
  llvm_unreachable("Bad PointerKind");
  return PointerKind::Escaping;
}

enum LifetimeMarkerKind {
  Start,
  End
};

struct LifetimeMarker {
  AllocaInst* AI { nullptr };
  LifetimeMarkerKind LMK { LifetimeMarkerKind::Start };

  LifetimeMarker() = default;

  LifetimeMarker(AllocaInst* AI, LifetimeMarkerKind LMK): AI(AI), LMK(LMK) { }

  explicit operator bool() const {
    return !!AI;
  }
};

enum class LifetimeState {
  // We haven't decided what the state of this thing is yet.
  Undetermined,
  
  // We know it's live.
  Live,
  
  // It could be dead, or maybe it could be live. We don't want to assume it's live. Better kill it.
  Zombie
};

raw_ostream& operator<<(raw_ostream& OS, LifetimeState LS) {
  switch (LS) {
  case LifetimeState::Undetermined:
    OS << "Undetermined";
    return OS;
  case LifetimeState::Live:
    OS << "Live";
    return OS;
  case LifetimeState::Zombie:
    OS << "Zombie";
    return OS;
  }
  llvm_unreachable("Bad LifetimeState");
  return OS;
}

LifetimeState mergeLifetimeState(LifetimeState A, LifetimeState B) {
  switch (A) {
  case LifetimeState::Undetermined:
    return B;
  case LifetimeState::Live:
    if (B != LifetimeState::Undetermined)
      return B;
    return LifetimeState::Live;
  case LifetimeState::Zombie:
    return LifetimeState::Zombie;
  }
  llvm_unreachable("Bad LifetimeState");
  return LifetimeState::Zombie;
}

enum class FrameEntryKind {
  Invalid,
  Ignored,
  Lower,
  LowerFromStackAux,
  ExplicitStackAux
};

struct FrameEntry {
  FrameEntryKind FEK { FrameEntryKind::Invalid };
  size_t Index { SIZE_MAX };

  FrameEntry() = default;

  FrameEntry(FrameEntryKind FEK, size_t Index): FEK(FEK), Index(Index) { }
};

struct LocalAllocaData {
  bool Explicit;
  AllocaInst* OrigAI { nullptr };
  AllocaInst* Payload { nullptr };
  AllocaInst* AuxAlloca { nullptr };
  Instruction* Aux { nullptr };
  uint64_t Size { UINT64_MAX };
};

struct PtrOperandData {
  PointerKind PK { PointerKind::Escaping };
  AllocaInst* AuxBaseVar { nullptr };
  LocalAllocaData LAD;
  PtrAndRandom PAR;

  PtrOperandData() = default;
};

struct MemoryAccessData {
  Value* Lower { nullptr };
  Value* P { nullptr };
  Value* AuxP { nullptr };
  Value* AuxBaseP { nullptr };
  MemoryKind MK { MemoryKind::Invalid };
  AllocaInst* OrigAI { nullptr };
  int64_t LocalOffset { INT64_MIN };
  uint64_t Size { UINT64_MAX };

  MemoryAccessData() { }

  // Lower is only needed if we're doing an atomic access.
  MemoryAccessData(Value* Lower, Value* P, Value* AuxP, Value* AuxBaseP, MemoryKind MK):
    Lower(Lower), P(P), AuxP(AuxP), AuxBaseP(AuxBaseP), MK(MK) {
    validate();
  }

  void validate() const {
    assert(P);
    assert(AuxP);
    assert(AuxBaseP);
    assert(MK == MemoryKind::Heap || MK == MemoryKind::LocalExplicit || MK == MemoryKind::LocalNaked
           || MK == MemoryKind::CC || MK == MemoryKind::GlobalInit
           || MK == MemoryKind::ThreadLocalInit);
    if (MK == MemoryKind::LocalNaked) {
      assert(OrigAI);
      assert(LocalOffset != INT64_MIN);
      assert(Size != UINT64_MAX);
    } else {
      assert(!OrigAI);
      assert(LocalOffset == INT64_MIN);
      assert(Size == UINT64_MAX);
    }
  }

  MemoryAccessData plus(Value* P, Value* AuxP, int64_t Offset) {
    MemoryAccessData Result = *this;
    Result.P = P;
    Result.AuxP = AuxP;
    if (MK == MemoryKind::LocalNaked) {
      assert(Result.LocalOffset != INT64_MIN);
      Result.LocalOffset += Offset;
      assert((int32_t)Result.LocalOffset == Result.LocalOffset);
    } else
      assert(Result.LocalOffset == INT64_MIN);
    return Result;
  }
};

struct FullMemoryAccessData {
  PtrOperandData POD;
  MemoryAccessData MAD;
};

struct UnsafeExport {
  std::string Name;
  Constant* Aliasee { nullptr };

  UnsafeExport() = default;

  UnsafeExport(const std::string& Name, Constant* Aliasee):
    Name(Name), Aliasee(Aliasee) { }
};

struct NameAndSignature {
  NameAndSignature() = default;

  NameAndSignature(const std::string Name, uint64_t Signature):
    Name(Name), Signature(Signature) {
  }

  explicit operator bool() const { return !Name.empty(); }

  bool operator==(const NameAndSignature& Other) const {
    return Name == Other.Name && Signature == Other.Signature;
  }

  size_t hash() const {
    return std::hash<std::string>()(Name) + std::hash<uint64_t>()(Signature);
  }

  std::string Name;
  uint64_t Signature { 0 };
};

} // anonymous namespace

template<> struct std::hash<NameAndSignature> {
  size_t operator()(const NameAndSignature& Key) const {
    return Key.hash();
  };
};

namespace {

class Pizlonator {
  static constexpr unsigned TargetAS = 0;
  
  LLVMContext& C;
  Module &M;
  const DataLayout DLBefore;
  const DataLayout DL;

  unsigned PtrBits;
  Type* VoidTy;
  IntegerType* Int1Ty;
  IntegerType* Int8Ty;
  IntegerType* Int16Ty;
  IntegerType* Int32Ty;
  IntegerType* Int64Ty;
  IntegerType* IntPtrTy;
  IntegerType* Int128Ty;
  Type* FloatTy;
  Type* DoubleTy;
  PointerType* RawPtrTy;
  StructType* FlightPtrTy;
  StructType* OriginNodeTy;
  StructType* FunctionOriginTy;
  StructType* OriginTy;
  StructType* InlineFrameTy;
  StructType* OriginWithEHTy;
  StructType* ObjectTy;
  StructType* FrameTy;
  StructType* ThreadTy;
  StructType* ConstantRelocationTy;
  StructType* ConstexprNodeTy;
  StructType* AlignmentAndOffsetTy;
  StructType* PizlonatedReturnValueTy;
  StructType* PtrPairTy;
  StructType* FunctionPayloadTy;
  StructType* FunctionObjectTy;
  StructType* ClosureTy;
  FunctionType* PizlonatedFuncTy;
  FunctionType* PizlonatedGetterTy;
  FunctionType* ThreadLocalEnsureTy;
  FunctionType* CtorDtorTy;
  FunctionType* SetjmpTy;
  FunctionType* SigsetjmpTy;
  Constant* RawNull;
  Constant* FlightNull;
  BitCastInst* Dummy;
  FunctionType* UnsafeFuncTy;

  // Low-level functions used by codegen.
  FunctionCallee Pollcheck;
  FunctionCallee Enter;
  FunctionCallee Exit;
  FunctionCallee StoreBarrierForLowerSlow;
  FunctionCallee StorePtrAtomicOutline;
  FunctionCallee LoadPtrAtomicOutline;
  FunctionCallee ObjectEnsureAuxPtrOutline;
  FunctionCallee ThreadEnsureCCOutlineBufferSlow;
  FunctionCallee StrongCasPtr;
  FunctionCallee XchgPtr;
  FunctionCallee GetNextPtrBytesForVAArg;
  FunctionCallee Allocate;
  FunctionCallee AllocateWithAlignment;
  FunctionCallee LocalAllocatorAllocate;
  FunctionCallee FreeWithChecks;
  FunctionCallee LogAllocate;
  FunctionCallee OptimizedAlignmentContradiction;
  FunctionCallee OptimizedAccessCheckFail;
  FunctionCallee OptimizedStackAlignmentContradiction;
  FunctionCallee OptimizedStackAccessCheckFail;
  FunctionCallee MaskedAccessCheckFail;
  FunctionCallee CheckFunctionCallFail;
  FunctionCallee CheckClosureFail;
  FunctionCallee ComdatLinkFail;
  FunctionCallee Memset;
  FunctionCallee Memmove;
  FunctionCallee MemmoveAlreadyChecked;
  FunctionCallee MemmoveAlreadyCheckedSmall;
  FunctionCallee FinishMemmoveSmall1;
  FunctionCallee FinishMemmoveSmall2;
  FunctionCallee FinishMemmoveSmall3;
  FunctionCallee FinishMemmoveSmall4;
  FunctionCallee FinishMemmoveSmall5;
  FunctionCallee MemmoveAlreadyCheckedStackToHeap;
  FunctionCallee MemmoveStackToHeap;
  FunctionCallee MemmoveAlreadyCheckedHeapToStack;
  FunctionCallee MemmoveHeapToStack;
  FunctionCallee MemmoveAlreadyCheckedStack;
  FunctionCallee MemmoveStack;
  FunctionCallee GlobalInitializationStart;
  FunctionCallee GlobalInitializationEnd;
  FunctionCallee CallIfunc;
  FunctionCallee ExecuteConstantRelocations;
  FunctionCallee DeferOrRunGlobalCtor;
  FunctionCallee RunGlobalDtor;
  FunctionCallee Error;
  FunctionCallee RealMemset;
  FunctionCallee RealMemcpy;
  FunctionCallee RealMemmove;
  FunctionCallee LandingPad;
  FunctionCallee ResumeUnwind;
  FunctionCallee JmpBufCreate;
  FunctionCallee PromoteAlreadyCheckedStackToHeapWithoutExiting;
  FunctionCallee PromoteAlreadyCheckedStackToHeapWithAlignmentWithoutExiting;
  FunctionCallee DemoteWordAlignedAlreadyCheckedHeapToStackWithoutExiting;
  FunctionCallee DemoteAlreadyCheckedHeapToStackWithoutExiting;
  FunctionCallee PromoteArgsToHeap;
  FunctionCallee PrepareToReturnWithData;
  FunctionCallee CCArgsCheckFailure;
  FunctionCallee CCRetsCheckFailure;
  FunctionCallee AllocateThreadLocal;
  FunctionCallee AllocateThreadLocalWithPtrs;
  FunctionCallee WeakMapCreate;
  FunctionCallee WeakMapSet;
  FunctionCallee _Setjmp;
  FunctionCallee ExpectI1;
  FunctionCallee LifetimeStart;
  FunctionCallee LifetimeEnd;
  FunctionCallee StackCheckAsm;
  FunctionCallee ThreadlocalAddress;
  FunctionCallee DoNothing;

  Constant* CurrentMarkingState;

  std::unordered_map<AllocaInst*, PointerKind> AllocaKinds;
  std::unordered_set<AllocaInst*> AlwaysLive;

  std::unordered_set<CombinedDI> CombinedDIs;
  std::unordered_map<std::pair<const CombinedDI*, const CombinedDI*>,
                     const CombinedDI*> CombinedDIMaker;
  std::unordered_map<DILocation*, const CombinedDI*> BasicDIs;
  
  std::unordered_map<std::string, GlobalVariable*> Strings;
  std::unordered_map<FunctionOriginKey, GlobalVariable*> FunctionOrigins;
  std::unordered_map<OriginKey, GlobalVariable*> Origins;
  std::unordered_map<InlineFrameKey, GlobalVariable*> InlineFrames;
  std::unordered_map<EHDataKey, GlobalVariable*> EHDatas; /* the value is a high-level GV, need to
                                                             lookup the getter. */
  std::unordered_map<Constant*, int> EHTypeIDs;
  bool HasSetjmps;
  size_t SetjmpSetFrameIndex;
  std::unordered_map<Type*, Constant*> CCTypes;
  std::unordered_map<Instruction*, std::vector<AccessCheckWithDI>> ChecksForInst;
  std::unordered_map<OptimizedAccessCheckOriginKey, GlobalVariable*> OptimizedAccessCheckOrigins;
  std::unordered_map<OptimizedAlignmentContradictionOriginKey,
                     GlobalVariable*> OptimizedAlignmentContradictionOrigins;
  std::unordered_map<Value*, AllocaInst*> CanonicalPtrAuxBaseVars;
  std::unordered_map<AllocaInst*, LocalAllocaData> LocalAllocaDatas;
  std::unordered_map<Instruction*, std::vector<PtrOperandData>> PtrOperandDatas;
  bool AuxBaseVarCreationAllowed { false };

  std::vector<GlobalVariable*> Globals;
  std::vector<Function*> Functions;
  std::vector<GlobalAlias*> Aliases;
  std::vector<GlobalIFunc*> IFuncs;

  std::unordered_map<Type*, Type*> FlightedTypes;

  std::unordered_map<GlobalValue*, Function*> GlobalToGetter;
  std::unordered_map<GlobalValue*, GlobalVariable*> GlobalToGlobal;
  std::unordered_set<Value*> Getters;
  std::unordered_map<Function*, Function*> FunctionToHiddenFunction;
  std::unordered_map<Function*, uint64_t> FunctionToSignature;
  std::unordered_map<Function*, Constant*> FunctionToLower;

  std::unordered_map<GlobalValue*, Comdat*> GlobalToComdat;
  std::unordered_map<Comdat*, Comdat*> ComdatMap;

  std::unordered_map<std::string, BitCastInst*> UnsafeFuncs;
  std::unordered_set<GlobalVariable*> UnsafeExportGVs;
  std::vector<UnsafeExport> UnsafeExports;

  std::string FunctionName;
  Function* OldF { nullptr };
  Function* NewF { nullptr };

  std::unordered_map<Instruction*, Type*> InstTypes;
  std::unordered_map<Instruction*, std::vector<Type*>> InstTypeVectors;
  std::unordered_map<InvokeInst*, LandingPadInst*> LPIs;

  std::unordered_map<uint64_t, Function*> CallerEntrypointThunks;
  std::unordered_map<uint64_t, Function*> CalleeEntrypointThunks;
  std::unordered_map<NameAndSignature, Function*> KnownTargetCallsiteThunks;
  
  BasicBlock* FirstRealBlock;

  BasicBlock* ReturnB;
  PHINode* ReturnPhi;
  BasicBlock* ResumeB;
  PHINode* RetSizePhi;
  BasicBlock* ReallyReturnB;

  bool UsesVastartOrZargs;
  bool UsesVariadicCC;
  size_t SnapshottedArgsFrameIndex;
  Value* SnapshottedArgsPtrForVastart;
  Value* SnapshottedArgsPtrForZargs;
  std::vector<Value*> Args;

  Value* MyThread { nullptr };

  // SIZE_MAX entries mean: do not record
  std::unordered_map<ValuePtr, FrameEntry> FrameIndexMap;
  size_t FrameSize { SIZE_MAX };
  size_t NumStackAuxes { SIZE_MAX };
  Value* Frame;

  std::vector<Instruction*> ToErase;

  BitCastInst* makeDummy(Type* T) {
    return new BitCastInst(UndefValue::get(T), T, "dummy");
  }

  GlobalVariable* getString(StringRef Str) {
    auto iter = Strings.find(Str.str());
    if (iter != Strings.end())
      return iter->second;

    Constant* C = ConstantDataArray::getString(this->C, Str);
    GlobalVariable* Result = new GlobalVariable(
      M, C->getType(), true, GlobalVariable::PrivateLinkage, C, "filc_string");
    Result->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    Strings[Str.str()] = Result;
    return Result;
  }

  std::string demangle(StringRef Name) {
    std::string Result;
    if (char* DemangledName = itaniumDemangle(Name)) {
      Result = DemangledName;
      free(DemangledName);
      return Result;
    } else
      Result = Name;
    return Result;
  }

  std::string getFunctionName(Function *F) {
    return demangle(F->getName());
  }

  // What does "CanCatch" mean in this context? CanCatch=true means we're at an origin that is either:
  // - a CallInst in a function that is !doesNotThrow, or
  // - an InvokeInst.
  //
  // Lots of origins don't meet this definition!
  Constant* getFunctionOrigin(DIScope* Scope, bool CanCatch) {
    assert(OldF);
    
    FunctionOriginKey FOK(OldF, Scope, CanCatch);
    auto iter = FunctionOrigins.find(FOK);
    if (iter != FunctionOrigins.end())
      return iter->second;

    Constant* Personality = RawNull;
    if (CanCatch && OldF->hasPersonalityFn()) {
      assert(GlobalToGetter.count(cast<Function>(OldF->getPersonalityFn())));
      Personality = GlobalToGetter[cast<Function>(OldF->getPersonalityFn())];
    }
    
    bool CanThrow = !OldF->doesNotThrow();

    assert(FrameSize < UINT_MAX);
    assert(NumStackAuxes < UINT_MAX);

    std::string Filename;
    if (Scope)
      Filename = Scope->getFilename();
    if (Filename.empty() && OldF->getSubprogram())
      Filename = OldF->getSubprogram()->getFilename();
    
    Constant* C = ConstantStruct::get(
      FunctionOriginTy,
      { ConstantStruct::get(
          OriginNodeTy,
          { getString(getFunctionName(OldF)),
            Filename.size() ? getString(Filename) : RawNull,
            ConstantInt::get(Int32Ty, FrameSize) }),
        Personality, ConstantInt::get(Int8Ty, CanThrow), ConstantInt::get(Int8Ty, CanCatch),
        ConstantInt::get(Int8Ty, HasSetjmps), ConstantInt::get(Int32Ty, NumStackAuxes) });
    GlobalVariable* Result = new GlobalVariable(
      M, FunctionOriginTy, true, GlobalVariable::PrivateLinkage, C, "filc_function_origin");
    Result->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    FunctionOrigins[FOK] = Result;
    return Result;
  }

  Constant* getEHData(LandingPadInst* LPI) {
    if (!LPI)
      return RawNull;
    assert(EHDatas.count(LPI));
    assert(GlobalToGetter.count(EHDatas[LPI]));
    return GlobalToGetter[EHDatas[LPI]];
  }

  Constant* getInlineFrame(DILocalScope* Scope, DILocation* InlinedAt, bool CanCatch) {
    assert(OldF);
    assert(InlinedAt);

    InlineFrameKey IFK(OldF, Scope, InlinedAt, CanCatch);
    auto iter = InlineFrames.find(IFK);
    if (iter != InlineFrames.end())
      return iter->second;

    unsigned Line = InlinedAt->getLine();
    unsigned Col = InlinedAt->getColumn();

    std::string FunctionName;
    if (Scope->getSubprogram()) {
      FunctionName = Scope->getSubprogram()->getLinkageName();
      if (FunctionName.size())
        FunctionName = demangle(FunctionName);
    }
    if (FunctionName.empty())
      FunctionName = Scope->getName();

    Constant* C = ConstantStruct::get(
      InlineFrameTy,
      { ConstantStruct::get(
          OriginNodeTy,
          { getString(FunctionName), getString(Scope->getFilename()),
            ConstantInt::get(Int32Ty, UINT_MAX) }),
        ConstantStruct::get(
          OriginTy,
          { getOriginNode(InlinedAt->getScope(), InlinedAt->getInlinedAt(), CanCatch),
            ConstantInt::get(Int32Ty, Line), ConstantInt::get(Int32Ty, Col) }) });
    GlobalVariable* Result = new GlobalVariable(
      M, InlineFrameTy, true, GlobalVariable::PrivateLinkage, C, "filc_inline_frame");
    Result->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    InlineFrames[IFK] = Result;
    return Result;
  }

  Constant* getOriginNode(DILocalScope* Scope, DILocation* InlinedAt, bool CanCatch) {
    assert(Scope);
    if (InlinedAt)
      return getInlineFrame(Scope, InlinedAt, CanCatch);
    return getFunctionOrigin(Scope, CanCatch);
  }

  // See the definition of CanCatch, above.
  Constant* getOrigin(DebugLoc Loc, bool CanCatch = false, LandingPadInst* LPI = nullptr) {
    if (!OldF) {
      // This happens if we try to get an origin while compiling something other than a user function.
      // In that case, we vend the NULL origin, which is OK for the one use case where this happens
      // (calling a getter recursively from a getter; the first thing any getter does is starts global
      // initialization, which sets the origin in the top frame, so it's fine to pass NULL after
      // that).
      assert(!CanCatch);
      assert(!LPI);
      return RawNull;
    }
    if (LPI)
      assert(CanCatch);
    
    DILocation* Impl = Loc.get();
    OriginKey OK(OldF, Impl, CanCatch, LPI);
    auto iter = Origins.find(OK);
    if (iter != Origins.end())
      return iter->second;

    unsigned Line = 0;
    unsigned Col = 0;
    Constant* OriginNode;
    if (Loc) {
      Line = Loc.getLine();
      Col = Loc.getCol();
      OriginNode = getOriginNode(Loc->getScope(), Loc->getInlinedAt(), CanCatch);
    } else
      OriginNode = getFunctionOrigin(nullptr, CanCatch);

    GlobalVariable* Result;
    if (CanCatch && OldF->hasPersonalityFn()) {
      Constant* C = ConstantStruct::get(
        OriginWithEHTy,
        { OriginNode, ConstantInt::get(Int32Ty, Line), ConstantInt::get(Int32Ty, Col),
          getEHData(LPI) });
      Result = new GlobalVariable(
        M, OriginWithEHTy, true, GlobalVariable::PrivateLinkage, C, "filc_origin_with_eh");
    } else {
      Constant* C = ConstantStruct::get(
        OriginTy,
        { OriginNode, ConstantInt::get(Int32Ty, Line), ConstantInt::get(Int32Ty, Col) });
      Result = new GlobalVariable(
        M, OriginTy, true, GlobalVariable::PrivateLinkage, C, "filc_origin");
    }
    Result->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    Origins[OK] = Result;
    return Result;
  }

  // See definition of CanCatch=true, above.
  Value* getCatchOrigin(DebugLoc Loc, LandingPadInst* LPI = nullptr) {
    return getOrigin(Loc, true, LPI);
  }

  Type* toFlightTypeImpl(Type* T) {
    assert(T != FlightPtrTy);
    
    if (isa<FunctionType>(T))
      return PizlonatedFuncTy;

    if (isa<TypedPointerType>(T)) {
      llvm_unreachable("Shouldn't ever see typed pointers");
      return nullptr;
    }

    if (isa<PointerType>(T)) {
      if (T->getPointerAddressSpace() == TargetAS) {
        assert(T == RawPtrTy);
        return FlightPtrTy;
      }
      return T;
    }

    if (StructType* ST = dyn_cast<StructType>(T)) {
      if (ST->isOpaque())
        return ST;
      std::vector<Type*> Elements;
      for (Type* InnerT : ST->elements())
        Elements.push_back(toFlightType(InnerT));
      if (ST->isLiteral())
        return StructType::get(C, Elements, ST->isPacked());
      std::string NewName = ("pizlonated_" + ST->getName()).str();
      return StructType::create(C, Elements, NewName, ST->isPacked());
    }
      
    if (ArrayType* AT = dyn_cast<ArrayType>(T))
      return ArrayType::get(toFlightType(AT->getElementType()), AT->getNumElements());
      
    if (FixedVectorType* VT = dyn_cast<FixedVectorType>(T)) {
      assert(!hasPtrs(VT->getElementType())); // We don't support pointer vectors yet.
      return FixedVectorType::get(
        toFlightType(VT->getElementType()), VT->getElementCount().getFixedValue());
    }

    if (isa<ScalableVectorType>(T)) {
      llvm_unreachable("Shouldn't see scalable vector types");
      return nullptr;
    }
    
    return T;
  }

  Type* toFlightType(Type* T) {
    auto iter = FlightedTypes.find(T);
    if (iter != FlightedTypes.end())
      return iter->second;

    Type* FlightT = toFlightTypeImpl(T);
    assert(T->isSized() == FlightT->isSized());
    FlightedTypes[T] = FlightT;
    return FlightT;
  }

  Value* expectBool(Value* Predicate, bool Expected, Instruction* InsertBefore) {
    CallInst* Result = CallInst::Create(
      ExpectI1, { Predicate, ConstantInt::getBool(Int1Ty, Expected) },
      Expected ? "filc_expect_true" : "filc_expect_false",
      InsertBefore);
    Result->setDebugLoc(InsertBefore->getDebugLoc());
    return Result;
  }

  Value* expectTrue(Value* Predicate, Instruction* InsertBefore) {
    return expectBool(Predicate, true, InsertBefore);
  }
  
  Value* expectFalse(Value* Predicate, Instruction* InsertBefore) {
    return expectBool(Predicate, false, InsertBefore);
  }

  Value* threadFieldPtr(Value* Thread, unsigned FieldIndex, const char* Name,
                        Instruction* InsertBefore) {
    GetElementPtrInst* GEP = GetElementPtrInst::Create(
      ThreadTy, Thread, { ConstantInt::get(IntPtrTy, 0), ConstantInt::get(Int32Ty, FieldIndex) },
      Name, InsertBefore);
    GEP->setDebugLoc(InsertBefore->getDebugLoc());
    return GEP;
  }

  Value* threadStackLimitPtr(Value* Thread, Instruction* InsertBefore) {
    return threadFieldPtr(Thread, 0, "filc_thread_stack_limit_ptr", InsertBefore);
  }

  Value* threadStatePtr(Value* Thread, Instruction* InsertBefore) {
    return threadFieldPtr(Thread, 1, "filc_thread_state_ptr", InsertBefore);
  }

  Value* threadTIDPtr(Value* Thread, Instruction* InsertBefore) {
    return threadFieldPtr(Thread, 2, "filc_thread_tid_ptr", InsertBefore);
  }

  Value* threadTopFramePtr(Value* Thread, Instruction* InsertBefore) {
    return threadFieldPtr(Thread, 3, "filc_thread_top_frame_ptr", InsertBefore);
  }

  Value* threadUnwindRegistersPtr(Value* Thread, Instruction* InsertBefore) {
    return threadFieldPtr(Thread, 5, "filc_thread_unwind_registers_ptr", InsertBefore);
  }

  Value* threadUnwindRegisterPtr(Value* Thread, unsigned RegisterIndex, Instruction* InsertBefore) {
    assert(RegisterIndex < NumUnwindRegisters);
    GetElementPtrInst* GEP = GetElementPtrInst::Create(
      FlightPtrTy, threadUnwindRegistersPtr(Thread, InsertBefore),
      { ConstantInt::get(IntPtrTy, RegisterIndex) }, "filc_unwind_register_ptr", InsertBefore);
    GEP->setDebugLoc(InsertBefore->getDebugLoc());
    return GEP;
  }

  Value* threadCookiePtrPtr(Value* Thread, Instruction* InsertBefore) {
    return threadFieldPtr(Thread, 6, "filc_thread_cookie_ptr_ptr", InsertBefore);
  }

  Value* threadCCInlineBufferPtr(Value* Thread, Instruction* InsertBefore) {
    return threadFieldPtr(Thread, 8, "filc_thread_cc_inline_buffer_ptr", InsertBefore);
  }

  Value* threadCCInlineAuxBufferPtr(Value* Thread, Instruction* InsertBefore) {
    return threadFieldPtr(Thread, 9, "filc_thread_cc_inline_aux_buffer_ptr", InsertBefore);
  }

  Value* threadCCOutlineBufferPtr(Value* Thread, Instruction* InsertBefore) {
    return threadFieldPtr(Thread, 10, "filc_thread_cc_outline_buffer_ptr", InsertBefore);
  }

  Value* threadCCOutlineAuxBufferPtr(Value* Thread, Instruction* InsertBefore) {
    return threadFieldPtr(Thread, 11, "filc_thread_cc_outline_aux_buffer_ptr", InsertBefore);
  }

  Value* threadCCOutlineSizePtr(Value* Thread, Instruction* InsertBefore) {
    return threadFieldPtr(Thread, 12, "filc_thread_cc_outline_size_ptr", InsertBefore);
  }

  Value* flightPtrPtr(Value* P, Instruction* InsertBefore) {
    if (isa<ConstantAggregateZero>(P))
      return RawNull;
    Instruction* Result = ExtractValueInst::Create(RawPtrTy, P, { 0 }, "filc_ptr_ptr", InsertBefore);
    Result->setDebugLoc(InsertBefore->getDebugLoc());
    return Result;
  }

  Value* flightPtrLower(Value* P, Instruction* InsertBefore) {
    if (isa<ConstantAggregateZero>(P))
      return RawNull;
    Instruction* Result = ExtractValueInst::Create(RawPtrTy, P, { 1 }, "filc_ptr_lower", InsertBefore);
    Result->setDebugLoc(InsertBefore->getDebugLoc());
    return Result;
  }

  Value* flightPtrPtrAsInt(Value* P, Instruction* InsertBefore) {
    Instruction* PtrAsInt = new PtrToIntInst(
      flightPtrPtr(P, InsertBefore), IntPtrTy, "filc_ptr_ptr_as_int", InsertBefore);
    PtrAsInt->setDebugLoc(InsertBefore->getDebugLoc());
    return PtrAsInt;
  }

  Value* flightPtrOffset(Value* P, Instruction* InsertBefore) {
    Instruction* LowerAsInt = new PtrToIntInst(
      flightPtrLower(P, InsertBefore), IntPtrTy, "filc_ptr_lower_as_int", InsertBefore);
    LowerAsInt->setDebugLoc(InsertBefore->getDebugLoc());
    Instruction* Result = BinaryOperator::Create(
      Instruction::Sub, flightPtrPtrAsInt(P, InsertBefore), LowerAsInt, "filc_ptr_offset",
      InsertBefore);
    Result->setDebugLoc(InsertBefore->getDebugLoc());
    return Result;
  }

  Value* createFlightPtr(Value* Lower, Value* Ptr, Instruction* InsertBefore) {
    Instruction* Result = InsertValueInst::Create(
      UndefValue::get(FlightPtrTy), Ptr, { 0 }, "filc_insert_ptr", InsertBefore);
    Result->setDebugLoc(InsertBefore->getDebugLoc());
    Result = InsertValueInst::Create(Result, Lower, { 1 }, "filc_insert_lower", InsertBefore);
    Result->setDebugLoc(InsertBefore->getDebugLoc());
    return Result;
  }

  Value* flightPtrForPayload(Value* Payload, Instruction* InsertBefore) {
    return createFlightPtr(Payload, Payload, InsertBefore);
  }

  Value* lowerForObject(Value* Object, Instruction* InsertBefore) {
    Instruction* GEP = GetElementPtrInst::Create(
      ObjectTy, Object, { ConstantInt::get(IntPtrTy, 1) }, "filc_lower_for_object", InsertBefore);
    GEP->setDebugLoc(InsertBefore->getDebugLoc());
    return GEP;
  }

  Value* objectForLower(Value* Lower, Instruction* InsertBefore) {
    Instruction* GEP = GetElementPtrInst::Create(
      ObjectTy, Lower, { ConstantInt::get(IntPtrTy, -(intptr_t)1) }, "filc_object_for_lower",
      InsertBefore);
    GEP->setDebugLoc(InsertBefore->getDebugLoc());
    return GEP;
  }

  Value* flightPtrForObject(Value* Object, Instruction* InsertBefore) {
    return flightPtrForPayload(lowerForObject(Object, InsertBefore), InsertBefore);
  }

  Value* flightPtrWithPtr(Value* FlightPtr, Value* NewRawPtr, Instruction* InsertBefore) {
    return createFlightPtr(flightPtrLower(FlightPtr, InsertBefore), NewRawPtr, InsertBefore);
  }

  Value* flightPtrWithOffset(Value* FlightPtr, Value* Offset, Instruction* InsertBefore) {
    Instruction* GEP = GetElementPtrInst::Create(
      Int8Ty, flightPtrPtr(FlightPtr, InsertBefore), { Offset }, "filc_ptr_with_offset",
      InsertBefore);
    GEP->setDebugLoc(InsertBefore->getDebugLoc());
    return flightPtrWithPtr(FlightPtr, GEP, InsertBefore);
  }

  Value* ptrToUpperForLower(Value* Lower, Instruction* InsertBefore) {
    Instruction* UpperPtr = GetElementPtrInst::Create(
      ObjectTy, Lower,
      { ConstantInt::get(IntPtrTy, -(intptr_t)1), ConstantInt::get(Int32Ty, 0) },
      "filc_object_upper_ptr", InsertBefore); 
    UpperPtr->setDebugLoc(InsertBefore->getDebugLoc());
    return UpperPtr;
  }

  Value* ptrToAuxForLower(Value* Lower, Instruction* InsertBefore)
  {
    Instruction* AuxPtr = GetElementPtrInst::Create(
      ObjectTy, Lower,
      { ConstantInt::get(IntPtrTy, -(intptr_t)1), ConstantInt::get(Int32Ty, 1) },
      "filc_object_aux_ptr", InsertBefore);
    AuxPtr->setDebugLoc(InsertBefore->getDebugLoc());
    return AuxPtr;
  }

  Value* upperForLower(Value* Lower, Instruction* InsertBefore) {
    Instruction* Upper = new LoadInst(
      RawPtrTy, ptrToUpperForLower(Lower, InsertBefore), "filc_object_upper_load", InsertBefore);
    Upper->setDebugLoc(InsertBefore->getDebugLoc());
    return Upper;
  }

  Value* auxForLower(Value* Lower, Instruction* InsertBefore) {
    Instruction* Aux = new LoadInst(
      IntPtrTy, ptrToAuxForLower(Lower, InsertBefore), "filc_object_aux_load", InsertBefore);
    Aux->setDebugLoc(InsertBefore->getDebugLoc());
    return Aux;
  }

  Value* flagsForLower(Value* Lower, Instruction* InsertBefore) {
    Value* Aux = auxForLower(Lower, InsertBefore);
    Instruction* Flags = BinaryOperator::Create(
      Instruction::LShr, Aux, ConstantInt::get(IntPtrTy, ObjectAuxFlagsShift), "filc_object_flags",
      InsertBefore);
    Flags->setDebugLoc(InsertBefore->getDebugLoc());
    return Flags;
  }

  Value* auxPtrForLower(Value* Lower, Instruction* InsertBefore) {
    Value* Aux = auxForLower(Lower, InsertBefore);
    Instruction* AuxPtrAsInt = BinaryOperator::Create(
      Instruction::And, Aux, ConstantInt::get(IntPtrTy, ObjectAuxPtrMask), "filc_aux_ptr_as_int",
      InsertBefore);
    AuxPtrAsInt->setDebugLoc(InsertBefore->getDebugLoc());
    Instruction* AuxPtr = new IntToPtrInst(AuxPtrAsInt, RawPtrTy, "filc_aux_ptr", InsertBefore);
    AuxPtr->setDebugLoc(InsertBefore->getDebugLoc());
    return AuxPtr;
  }

  Value* badFlightPtr(Value* P, Instruction* InsertBefore) {
    return createFlightPtr(RawNull, P, InsertBefore);
  }

  Value* flightPtrPtrPtr(Value* P, Instruction* InsertBefore) {
    Instruction* GEP = GetElementPtrInst::Create(
      FlightPtrTy, P, { ConstantInt::get(IntPtrTy, 0), ConstantInt::get(Int32Ty, 0) },
      "filc_flight_ptr_ptr", InsertBefore);
    GEP->setDebugLoc(InsertBefore->getDebugLoc());
    return GEP;
  }

  Value* flightPtrLowerPtr(Value* P, Instruction* InsertBefore) {
    Instruction* GEP = GetElementPtrInst::Create(
      FlightPtrTy, P, { ConstantInt::get(IntPtrTy, 0), ConstantInt::get(Int32Ty, 1) },
      "filc_flight_ptr_ptr", InsertBefore);
    GEP->setDebugLoc(InsertBefore->getDebugLoc());
    return GEP;
  }

  Value* flightPtrForWord(Value* Word, Instruction* InsertBefore) {
    if (isa<ConstantAggregateZero>(Word))
      return FlightNull;
    Instruction* IntRawPtr = new TruncInst(
      Word, IntPtrTy, "filc_flight_word_raw_ptr_int", InsertBefore);
    IntRawPtr->setDebugLoc(InsertBefore->getDebugLoc());
    Instruction* RawPtr = new IntToPtrInst(IntRawPtr, RawPtrTy, "filc_flight_ptr_ptr", InsertBefore);
    RawPtr->setDebugLoc(InsertBefore->getDebugLoc());
    Instruction* Shifted = BinaryOperator::Create(
      Instruction::LShr, Word, ConstantInt::get(Int128Ty, 64), "filc_flight_word_lower_shift",
      InsertBefore);
    Shifted->setDebugLoc(InsertBefore->getDebugLoc());
    Instruction* IntLower = new TruncInst(Shifted, IntPtrTy, "filc_flight_word_lower_int",
                                          InsertBefore);
    IntLower->setDebugLoc(InsertBefore->getDebugLoc());
    Instruction* Lower = new IntToPtrInst(IntLower, RawPtrTy, "filc_flight_ptr_lower", InsertBefore);
    Lower->setDebugLoc(InsertBefore->getDebugLoc());
    return createFlightPtr(Lower, RawPtr, InsertBefore);
  }

  Value* wordForFlightPtr(Value* P, Instruction* InsertBefore) {
    Instruction* PtrAsInt = new PtrToIntInst(
      flightPtrPtr(P, InsertBefore), IntPtrTy, "filc_ptr_as_int", InsertBefore);
    PtrAsInt->setDebugLoc(InsertBefore->getDebugLoc());
    Instruction* LowerAsInt = new PtrToIntInst(
      flightPtrLower(P, InsertBefore), IntPtrTy, "filc_object_as_int", InsertBefore);
    LowerAsInt->setDebugLoc(InsertBefore->getDebugLoc());
    Instruction* CastedPtr = new ZExtInst(PtrAsInt, Int128Ty, "filc_ptr_zext", InsertBefore);
    CastedPtr->setDebugLoc(InsertBefore->getDebugLoc());
    Instruction* CastedLower = new ZExtInst(LowerAsInt, Int128Ty, "filc_lower_zext", InsertBefore);
    CastedLower->setDebugLoc(InsertBefore->getDebugLoc());
    Instruction* ShiftedLower = BinaryOperator::Create(
      Instruction::Shl, CastedLower, ConstantInt::get(Int128Ty, 64), "filc_lower_shifted",
      InsertBefore);
    ShiftedLower->setDebugLoc(InsertBefore->getDebugLoc());
    Instruction* Word = BinaryOperator::Create(
      Instruction::Or, CastedPtr, ShiftedLower, "filc_ptr_word", InsertBefore);
    Word->setDebugLoc(InsertBefore->getDebugLoc());
    return Word;
  }

  Value* loadFlightPtr(Value* P, Instruction* InsertBefore) {
    Instruction* Lower = new LoadInst(
      RawPtrTy, flightPtrLowerPtr(P, InsertBefore), "filc_flight_load_lower", false, Align(8),
      AtomicOrdering::Monotonic, SyncScope::System, InsertBefore);
    Lower->setDebugLoc(InsertBefore->getDebugLoc());
    Instruction* RawPtr = new LoadInst(
      RawPtrTy, flightPtrPtrPtr(P, InsertBefore), "filc_flight_load_raw_ptr", false, Align(8),
      AtomicOrdering::Monotonic, SyncScope::System, InsertBefore);
    RawPtr->setDebugLoc(InsertBefore->getDebugLoc());
    return createFlightPtr(Lower, RawPtr, InsertBefore);
  }

  Value* loadPtr(
    MemoryAccessData MAD, bool isVolatile, Align A, AtomicOrdering AO, Instruction* InsertBefore) {
    assert(MAD.MK == MemoryKind::CC ||
           MAD.MK == MemoryKind::Heap ||
           MAD.MK == MemoryKind::LocalExplicit ||
           MAD.MK == MemoryKind::LocalNaked);
    if (MAD.MK == MemoryKind::LocalExplicit || MAD.MK == MemoryKind::LocalNaked) {
      // If we've proved that the memory doesn't escape then we can drop all of the atomic flags. Might
      // as well do that now.
      //
      // Note we cannot do that for volatile. Instead, we take volatile to mean that the memory
      // escapes.
      AO = AtomicOrdering::NotAtomic;
    }
    if (MAD.MK != MemoryKind::Heap) {
      assert(!isVolatile);
      assert(AO == AtomicOrdering::NotAtomic);
    }

    if (AO != AtomicOrdering::NotAtomic || isVolatile) {
      assert(MAD.MK == MemoryKind::Heap);
      assert(MAD.Lower);
      assert(MAD.P);
      storeOrigin(getOrigin(InsertBefore->getDebugLoc()), InsertBefore);
      Instruction* Result = CallInst::Create(
        LoadPtrAtomicOutline, { createFlightPtr(MAD.Lower, MAD.P, InsertBefore) }, "filc_atomic_load",
        InsertBefore);
      Result->setDebugLoc(InsertBefore->getDebugLoc());
      return Result;
    }
    
    Value* BaseAuxPIsNull;
    BasicBlock* OriginalB = InsertBefore->getParent();
    if (MAD.MK == MemoryKind::Heap) {
      Instruction* BaseAuxPIsNullInst = new ICmpInst(
        InsertBefore, ICmpInst::ICMP_EQ, MAD.AuxBaseP, RawNull, "filc_base_aux_ptr_is_null");
      BaseAuxPIsNullInst->setDebugLoc(InsertBefore->getDebugLoc());
      BaseAuxPIsNull = BaseAuxPIsNullInst;
    } else
      BaseAuxPIsNull = ConstantInt::getBool(Int1Ty, false);
    Instruction* NotNullCase = SplitBlockAndInsertIfElse(BaseAuxPIsNull, InsertBefore, false);
    Instruction* LowerAsIntLoad = new LoadInst(
      IntPtrTy, MAD.AuxP, "filc_load_lower", isVolatile, Align(WordSize),
      MAD.MK == MemoryKind::Heap ? getMergedAtomicOrdering(AtomicOrdering::Monotonic, AO) : AO,
      SyncScope::System, NotNullCase);
    LowerAsIntLoad->setDebugLoc(InsertBefore->getDebugLoc());
    PHINode* LowerAsInt = PHINode::Create(IntPtrTy, 2, "filc_lower_as_int_phi", InsertBefore);
    LowerAsInt->setDebugLoc(InsertBefore->getDebugLoc());
    LowerAsInt->addIncoming(ConstantInt::get(IntPtrTy, 0), OriginalB);
    LowerAsInt->addIncoming(LowerAsIntLoad, LowerAsIntLoad->getParent());

    auto FinishCreatingFlight = [&] (Instruction* Where) {
      Instruction* LowerToPtr = new IntToPtrInst(
        LowerAsInt, RawPtrTy, "filc_lower_as_ptr", Where);
      LowerToPtr->setDebugLoc(InsertBefore->getDebugLoc());
      Instruction* RawPtrLoad = new LoadInst(
        RawPtrTy, MAD.P, "filc_atomic_case_load_raw_ptr", isVolatile, std::max(A, Align(WordSize)), AO,
        SyncScope::System, Where);
      RawPtrLoad->setDebugLoc(InsertBefore->getDebugLoc());
      return createFlightPtr(LowerToPtr, RawPtrLoad, Where);
    };
    
    if (MAD.MK == MemoryKind::Heap) {
      assert(!isVolatile);
      assert(AO == AtomicOrdering::NotAtomic);
      Instruction* Masked = BinaryOperator::Create(
        Instruction::And, LowerAsInt, ConstantInt::get(IntPtrTy, AtomicBoxBit),
        "filc_lower_atomic_box_bit", InsertBefore);
      Masked->setDebugLoc(InsertBefore->getDebugLoc());
      Instruction* IsNotBox = new ICmpInst(
        InsertBefore, ICmpInst::ICMP_EQ, Masked, ConstantInt::get(IntPtrTy, 0),
        "filc_lower_is_not_box");
      IsNotBox->setDebugLoc(InsertBefore->getDebugLoc());
      Instruction* ThenTerm;
      Instruction* ElseTerm;
      SplitBlockAndInsertIfThenElse(expectTrue(IsNotBox, InsertBefore), InsertBefore,
                                    &ThenTerm, &ElseTerm);
      Instruction* BoxAsInt = BinaryOperator::Create(
        Instruction::And, LowerAsInt, ConstantInt::get(IntPtrTy, ~AtomicBoxBit),
        "filc_box_as_int", ElseTerm);
      BoxAsInt->setDebugLoc(InsertBefore->getDebugLoc());
      Instruction* Box = new IntToPtrInst(BoxAsInt, RawPtrTy, "filc_box", ElseTerm);
      Box->setDebugLoc(InsertBefore->getDebugLoc());
      Instruction* LowerFromBoxLoadInt = new LoadInst(
        IntPtrTy, flightPtrLowerPtr(Box, ElseTerm), "filc_lower_from_box", isVolatile,
        Align(WordSize), getMergedAtomicOrdering(AtomicOrdering::Monotonic, AO),
        SyncScope::System, ElseTerm);
      LowerFromBoxLoadInt->setDebugLoc(InsertBefore->getDebugLoc());
      PHINode* NewLowerAsInt = PHINode::Create(IntPtrTy, 2, "filc_lower_as_int_phi2", InsertBefore);
      NewLowerAsInt->setDebugLoc(InsertBefore->getDebugLoc());
      NewLowerAsInt->addIncoming(LowerAsInt, ThenTerm->getParent());
      NewLowerAsInt->addIncoming(LowerFromBoxLoadInt, ElseTerm->getParent());
      LowerAsInt = NewLowerAsInt;
    }
    return FinishCreatingFlight(InsertBefore);
  }

  Value* loadPtr(MemoryAccessData MAD, Instruction* InsertBefore) {
    return loadPtr(MAD, false, Align(WordSize), AtomicOrdering::NotAtomic, InsertBefore);
  }

  void storeBarrierForLower(Value* Lower, Instruction* InsertBefore) {
    assert(MyThread);
    DebugLoc DL = InsertBefore->getDebugLoc();
    ICmpInst* NullObject = new ICmpInst(
      InsertBefore, ICmpInst::ICMP_EQ, Lower, RawNull, "filc_barrier_null_object");
    NullObject->setDebugLoc(DL);
    Instruction* NotNullTerm = SplitBlockAndInsertIfElse(NullObject, InsertBefore, false);
    LoadInst* MarkingState = new LoadInst(Int32Ty, CurrentMarkingState, "filc_marking_state",
                                          NotNullTerm);
    MarkingState->setDebugLoc(DL);
    ICmpInst* IsNotMarking = new ICmpInst(
      NotNullTerm, ICmpInst::ICMP_EQ, MarkingState, ConstantInt::get(Int32Ty, 0),
      "filc_is_not_marking");
    IsNotMarking->setDebugLoc(DL);
    Instruction* IsMarkingTerm = SplitBlockAndInsertIfElse(
      expectTrue(IsNotMarking, NotNullTerm), NotNullTerm, false);
    CallInst::Create(StoreBarrierForLowerSlow, { MyThread, Lower }, "", IsMarkingTerm)
      ->setDebugLoc(DL);
  }
  
  void storeBarrierForValue(Value* V, Instruction* InsertBefore) {
    storeBarrierForLower(flightPtrLower(V, InsertBefore), InsertBefore);
  }
  
  void storePtr(
    Value* V, MemoryAccessData MAD, bool isVolatile, Align A, AtomicOrdering AO,
    Instruction* InsertBefore) {
    assert(MAD.MK == MemoryKind::CC ||
           MAD.MK == MemoryKind::GlobalInit ||
           MAD.MK == MemoryKind::ThreadLocalInit ||
           MAD.MK == MemoryKind::Heap ||
           MAD.MK == MemoryKind::LocalExplicit ||
           MAD.MK == MemoryKind::LocalNaked);

    if (MAD.MK == MemoryKind::LocalExplicit || MAD.MK == MemoryKind::LocalNaked)
      AO = AtomicOrdering::NotAtomic;

    if (MAD.MK != MemoryKind::Heap) {
      assert(!isVolatile);
      assert(AO == AtomicOrdering::NotAtomic);
    }

    if (AO != AtomicOrdering::NotAtomic || isVolatile) {
      assert(MAD.MK == MemoryKind::Heap);
      assert(MAD.Lower);
      assert(MAD.P);
      storeOrigin(getOrigin(InsertBefore->getDebugLoc()), InsertBefore);
      CallInst::Create(
        StorePtrAtomicOutline, { MyThread, createFlightPtr(MAD.Lower, MAD.P, InsertBefore), V }, "",
        InsertBefore)->setDebugLoc(InsertBefore->getDebugLoc());
      return;
    }
    
    if (MAD.MK == MemoryKind::Heap || MAD.MK == MemoryKind::ThreadLocalInit)
      storeBarrierForValue(V, InsertBefore);

    (new StoreInst(
      flightPtrLower(V, InsertBefore), MAD.AuxP, isVolatile, std::max(A, Align(WordSize)),
      MAD.MK == MemoryKind::Heap ? getMergedAtomicOrdering(AtomicOrdering::Monotonic, AO) : AO,
      SyncScope::System, InsertBefore))->setDebugLoc(InsertBefore->getDebugLoc());
    (new StoreInst(
      flightPtrPtr(V, InsertBefore), MAD.P, isVolatile, std::max(A, Align(WordSize)), AO,
      SyncScope::System, InsertBefore))->setDebugLoc(InsertBefore->getDebugLoc());

    // Note that we have to defend ourselves against crashing the compiler in case we are emitting a
    // totally invalid store (a store that is known to be out of bounds, or isn't aligned). But we
    // don't have to emit correct code in that case, since this path is unreachable in that case.
    if (MAD.MK == MemoryKind::LocalNaked && MAD.LocalOffset >= 0
        && static_cast<uint64_t>(MAD.LocalOffset) < MAD.Size) {
      FrameEntry FE = FrameIndexMap[ValuePtr(MAD.OrigAI, MAD.LocalOffset / WordSize)];
      assert(FE.FEK == FrameEntryKind::LowerFromStackAux);
      recordLowerAtIndex(flightPtrLower(V, InsertBefore), FE.Index, InsertBefore);
    }
  }

  void storePtr(Value* V, MemoryAccessData MAD, Instruction* InsertBefore) {
    storePtr(V, MAD, false, Align(WordSize), AtomicOrdering::NotAtomic, InsertBefore);
  }

  // This happens to work just as well for raw types and flight types, and that's important.
  bool hasPtrs(Type *T) {
    if (isa<FunctionType>(T)) {
      llvm_unreachable("shouldn't see function types in hasPtrs");
      return false;
    }

    if (isa<TypedPointerType>(T)) {
      llvm_unreachable("Shouldn't ever see typed pointers");
      return false;
    }

    if (isa<PointerType>(T)) {
      assert (!T->getPointerAddressSpace());
      return true;
    }

    if (T == FlightPtrTy)
      return true;

    if (StructType* ST = dyn_cast<StructType>(T)) {
      for (unsigned Index = ST->getNumElements(); Index--;) {
        Type* InnerT = ST->getElementType(Index);
        if (hasPtrs(InnerT))
          return true;
      }
      return false;
    }
      
    if (ArrayType* AT = dyn_cast<ArrayType>(T))
      return hasPtrs(AT->getElementType());

    if (FixedVectorType* VT = dyn_cast<FixedVectorType>(T))
      return hasPtrs(VT->getElementType());

    if (isa<ScalableVectorType>(T)) {
      llvm_unreachable("Shouldn't ever see scalable vectors in hasPtrs");
      return false;
    }
    
    return false;
  }

  bool isSomePtr(Type* T) {
    return T == RawPtrTy || T == FlightPtrTy;
  }

  Align lowAlign(Type* T, Align A, AtomicOrdering AO) {
    if (needToCheckAlignment(T) || AO != AtomicOrdering::NotAtomic)
      return A;
    return Align(1);
  }

  Value* loadValueRecurseAfterCheck(
    Type* T, MemoryAccessData MAD, bool isVolatile, Align A, AtomicOrdering AO, SyncScope::ID SS,
    Instruction* InsertBefore) {
    A = std::min(DL.getABITypeAlign(T), A);
    
    if (!hasPtrs(T)) {
      return new LoadInst(
        toFlightType(T), MAD.P, "filc_load", isVolatile, lowAlign(T, A, AO), AO, SS, InsertBefore);
    }
    
    if (isa<FunctionType>(T)) {
      llvm_unreachable("shouldn't see function types in loadValueRecurseAfterCheck");
      return nullptr;
    }

    if (isa<TypedPointerType>(T)) {
      llvm_unreachable("Shouldn't ever see typed pointers");
      return nullptr;
    }

    assert(T != FlightPtrTy);

    if (T == RawPtrTy)
      return loadPtr(MAD, isVolatile, A, AO, InsertBefore);

    assert(!isa<PointerType>(T));

    if (StructType* ST = dyn_cast<StructType>(T)) {
      Value* Result = UndefValue::get(toFlightType(T));
      const StructLayout* SL = DL.getStructLayout(ST);
      for (unsigned Index = ST->getNumElements(); Index--;) {
        Type* InnerT = ST->getElementType(Index);
        Value *InnerP = GetElementPtrInst::Create(
          ST, MAD.P, { ConstantInt::get(Int32Ty, 0), ConstantInt::get(Int32Ty, Index) },
          "filc_InnerP_struct", InsertBefore);
        Value* InnerAuxP = GetElementPtrInst::Create(
          ST, MAD.AuxP, { ConstantInt::get(Int32Ty, 0), ConstantInt::get(Int32Ty, Index) },
          "filc_InnerAuxP_struct", InsertBefore);
        Value* V = loadValueRecurseAfterCheck(
          InnerT, MAD.plus(InnerP, InnerAuxP, SL->getElementOffset(Index).getFixedValue()), isVolatile,
          A, AO, SS, InsertBefore);
        Result = InsertValueInst::Create(Result, V, Index, "filc_insert_struct", InsertBefore);
      }
      return Result;
    }
      
    if (ArrayType* AT = dyn_cast<ArrayType>(T)) {
      Value* Result = UndefValue::get(toFlightType(AT));
      assert(static_cast<unsigned>(AT->getNumElements()) == AT->getNumElements());
      Type* ET = AT->getElementType();
      size_t ESize = DL.getTypeAllocSize(ET);
      for (unsigned Index = static_cast<unsigned>(AT->getNumElements()); Index--;) {
        Value *InnerP = GetElementPtrInst::Create(
          AT, MAD.P, { ConstantInt::get(IntPtrTy, 0), ConstantInt::get(IntPtrTy, Index) },
          "filc_InnerP_array", InsertBefore);
        Value *InnerAuxP = GetElementPtrInst::Create(
          AT, MAD.AuxP, { ConstantInt::get(IntPtrTy, 0), ConstantInt::get(IntPtrTy, Index) },
          "filc_InnerAuxP_array", InsertBefore);
        Value* V = loadValueRecurseAfterCheck(
          ET, MAD.plus(InnerP, InnerAuxP, Index * ESize), isVolatile, A, AO, SS, InsertBefore);
        Result = InsertValueInst::Create(Result, V, Index, "filc_insert_array", InsertBefore);
      }
      return Result;
    }
      
    if (FixedVectorType* VT = dyn_cast<FixedVectorType>(T)) {
      Value* Result = UndefValue::get(toFlightType(VT));
      Type* ET = VT->getElementType();
      size_t ESize = DL.getTypeAllocSize(ET);
      for (unsigned Index = VT->getElementCount().getFixedValue(); Index--;) {
        Value *InnerP = GetElementPtrInst::Create(
          VT, MAD.P, { ConstantInt::get(IntPtrTy, 0), ConstantInt::get(IntPtrTy, Index) },
          "filc_InnerP_vector", InsertBefore);
        Value *InnerAuxP = GetElementPtrInst::Create(
          VT, MAD.AuxP, { ConstantInt::get(IntPtrTy, 0), ConstantInt::get(IntPtrTy, Index) },
          "filc_InnerAuxP_vector", InsertBefore);
        Value* V = loadValueRecurseAfterCheck(
          ET, MAD.plus(InnerP, InnerAuxP, Index * ESize), isVolatile, A, AO, SS, InsertBefore);
        Result = InsertElementInst::Create(
          Result, V, ConstantInt::get(IntPtrTy, Index), "filc_insert_vector", InsertBefore);
      }
      return Result;
    }

    if (isa<ScalableVectorType>(T)) {
      llvm_unreachable("Shouldn't see scalable vector types in loadValueRecurseAfterCheck");
      return nullptr;
    }

    llvm_unreachable("Should not get here.");
    return nullptr;
  }

  void storeValueRecurseAfterCheck(
    Type* T, Value* V, MemoryAccessData MAD, bool isVolatile, Align A, AtomicOrdering AO,
    SyncScope::ID SS, Instruction* InsertBefore) {
    A = std::min(DL.getABITypeAlign(T), A);
    
    if (!hasPtrs(T)) {
      new StoreInst(V, MAD.P, isVolatile, lowAlign(T, A, AO), AO, SS, InsertBefore);
      return;
    }
    
    if (isa<FunctionType>(T)) {
      llvm_unreachable("shouldn't see function types in storeValueRecurseAfterCheck");
      return;
    }

    if (isa<TypedPointerType>(T)) {
      llvm_unreachable("Shouldn't ever see typed pointers");
      return;
    }

    assert(T != FlightPtrTy);

    if (T == RawPtrTy) {
      storePtr(V, MAD, isVolatile, A, AO, InsertBefore);
      return;
    }

    assert(!isa<PointerType>(T));

    if (StructType* ST = dyn_cast<StructType>(T)) {
      const StructLayout* SL = DL.getStructLayout(ST);
      for (unsigned Index = ST->getNumElements(); Index--;) {
        Type* InnerT = ST->getElementType(Index);
        Instruction *InnerP = GetElementPtrInst::Create(
          ST, MAD.P, { ConstantInt::get(Int32Ty, 0), ConstantInt::get(Int32Ty, Index) },
          "filc_InnerP_struct", InsertBefore);
        InnerP->setDebugLoc(InsertBefore->getDebugLoc());
        Instruction *InnerAuxP = GetElementPtrInst::Create(
          ST, MAD.AuxP, { ConstantInt::get(Int32Ty, 0), ConstantInt::get(Int32Ty, Index) },
          "filc_InnerAuxP_struct", InsertBefore);
        InnerAuxP->setDebugLoc(InsertBefore->getDebugLoc());
        Instruction* InnerV = ExtractValueInst::Create(
          toFlightType(InnerT), V, { Index }, "filc_extract_struct", InsertBefore);
        InnerV->setDebugLoc(InsertBefore->getDebugLoc());
        storeValueRecurseAfterCheck(
          InnerT, InnerV, MAD.plus(InnerP, InnerAuxP, SL->getElementOffset(Index).getFixedValue()),
          isVolatile, A, AO, SS, InsertBefore);
      }
      return;
    }
      
    if (ArrayType* AT = dyn_cast<ArrayType>(T)) {
      assert(static_cast<unsigned>(AT->getNumElements()) == AT->getNumElements());
      Type* ET = AT->getElementType();
      size_t ESize = DL.getTypeAllocSize(ET);
      for (unsigned Index = static_cast<unsigned>(AT->getNumElements()); Index--;) {
        Value *InnerP = GetElementPtrInst::Create(
          AT, MAD.P, { ConstantInt::get(IntPtrTy, 0), ConstantInt::get(IntPtrTy, Index) },
          "filc_InnerP_array", InsertBefore);
        Value *InnerAuxP = GetElementPtrInst::Create(
          AT, MAD.AuxP, { ConstantInt::get(IntPtrTy, 0), ConstantInt::get(IntPtrTy, Index) },
          "filc_InnerAuxP_array", InsertBefore);
        Value* InnerV = ExtractValueInst::Create(
          toFlightType(AT->getElementType()), V, { Index }, "filc_extract_array", InsertBefore);
        storeValueRecurseAfterCheck(
          AT->getElementType(), InnerV, MAD.plus(InnerP, InnerAuxP, Index * ESize), isVolatile, A, AO,
          SS, InsertBefore);
      }
      return;
    }
      
    if (FixedVectorType* VT = dyn_cast<FixedVectorType>(T)) {
      Type* ET = VT->getElementType();
      size_t ESize = DL.getTypeAllocSize(ET);
      for (unsigned Index = VT->getElementCount().getFixedValue(); Index--;) {
        Value *InnerP = GetElementPtrInst::Create(
          VT, MAD.P, { ConstantInt::get(IntPtrTy, 0), ConstantInt::get(IntPtrTy, Index) },
          "filc_InnerP_vector", InsertBefore);
        Value *InnerAuxP = GetElementPtrInst::Create(
          VT, MAD.AuxP, { ConstantInt::get(IntPtrTy, 0), ConstantInt::get(IntPtrTy, Index) },
          "filc_InnerAuxP_vector", InsertBefore);
        Value* InnerV = ExtractElementInst::Create(
          V, ConstantInt::get(IntPtrTy, Index), "filc_extract_vector", InsertBefore);
        storeValueRecurseAfterCheck(
          VT->getElementType(), InnerV, MAD.plus(InnerP, InnerAuxP, Index * ESize), isVolatile, A, AO,
          SS, InsertBefore);
      }
      return;
    }

    if (isa<ScalableVectorType>(T)) {
      llvm_unreachable("Shouldn't see scalable vector types in storeValueRecurseAfterCheck");
      return;
    }

    llvm_unreachable("Should not get here.");
  }

  Value* allocateObjectInline(size_t Size, Instruction* InsertBefore) {
    // This sanity check allows us to avoid overflow checks. Note that if this check causes us to
    // bail then, in the non-overflowing cases, we would have bailed anyway due to the NUM_ALLOCATORS
    // limit.
    if (Size >= 65536)
      return nullptr;
    Size = (Size + GCMinAlign - 1) & -GCMinAlign;
    size_t TotalSize = ObjectSize + Size;
    assert(!(TotalSize & (GCMinAlign - 1)));
    size_t AllocatorIndex = TotalSize / GCMinAlign;
    if (AllocatorIndex >= ThreadNumAllocators)
      return nullptr;
    GetElementPtrInst* Allocator = GetElementPtrInst::Create(
      Int8Ty, MyThread,
      { ConstantInt::get(IntPtrTy, ThreadAllocatorOffset + AllocatorIndex * ThreadAllocatorSize) },
      "filc_thread_allocator", InsertBefore);
    Allocator->setDebugLoc(InsertBefore->getDebugLoc());
    CallInst* Allocate = CallInst::Create(
      LocalAllocatorAllocate, { Allocator }, "filc_allocate", InsertBefore);
    Allocate->setDebugLoc(InsertBefore->getDebugLoc());
    GetElementPtrInst* Upper = GetElementPtrInst::Create(
      Int8Ty, Allocate, { ConstantInt::get(IntPtrTy, TotalSize) },
      "filc_allocate_upper", InsertBefore);
    Upper->setDebugLoc(InsertBefore->getDebugLoc());
    GetElementPtrInst* UpperPtr = GetElementPtrInst::Create(
      ObjectTy, Allocate, { ConstantInt::get(IntPtrTy, 0), ConstantInt::get(Int32Ty, 0) },
      "filc_allocate_upper_ptr", InsertBefore);
    UpperPtr->setDebugLoc(InsertBefore->getDebugLoc());
    (new StoreInst(Upper, UpperPtr, InsertBefore))->setDebugLoc(InsertBefore->getDebugLoc());
    GetElementPtrInst* AuxPtr = GetElementPtrInst::Create(
      ObjectTy, Allocate, { ConstantInt::get(IntPtrTy, 0), ConstantInt::get(Int32Ty, 1) },
      "filc_allocate_aux_ptr", InsertBefore);
    AuxPtr->setDebugLoc(InsertBefore->getDebugLoc());
    (new StoreInst(RawNull, AuxPtr, InsertBefore))->setDebugLoc(InsertBefore->getDebugLoc());
    GetElementPtrInst* Payload = GetElementPtrInst::Create(
      ObjectTy, Allocate, { ConstantInt::get(IntPtrTy, 1) }, "filc_allocate_payload", InsertBefore);
    Payload->setDebugLoc(InsertBefore->getDebugLoc());
    CallInst* Memset = CallInst::Create(
      RealMemset,
      { Payload, ConstantInt::get(Int8Ty, 0), ConstantInt::get(IntPtrTy, Size),
        ConstantInt::getBool(Int1Ty, false) },
      "", InsertBefore);
    Memset->addParamAttr(0, Attribute::getWithAlignment(C, Align(GCMinAlign)));
    Memset->setDebugLoc(InsertBefore->getDebugLoc());
    return Allocate;
  }

  Value* allocateObject(Value* Size, Value* Alignment, Instruction* InsertBefore) {
    if (logAllocations) {
      CallInst::Create(
        LogAllocate, { MyThread, getOrigin(InsertBefore->getDebugLoc()), Size, Alignment },
        "", InsertBefore);
    }
    Instruction* Result;
    ConstantInt* AlignmentInt = dyn_cast<ConstantInt>(Alignment);
    if (AlignmentInt && AlignmentInt->getZExtValue() <= GCMinAlign) {
      if (ConstantInt* SizeConst = dyn_cast<ConstantInt>(Size)) {
        Value* ResultValue = allocateObjectInline(SizeConst->getZExtValue(), InsertBefore);
        if (ResultValue)
          return ResultValue;
      }
      Result = CallInst::Create(
        Allocate, { MyThread, Size }, "filc_allocate", InsertBefore);
    } else {
      Result = CallInst::Create(
        AllocateWithAlignment, { MyThread, Size, Alignment }, "filc_allocate", InsertBefore);
    }
    Result->setDebugLoc(InsertBefore->getDebugLoc());
    return Result;
  }

  Value* allocate(Value* Size, Value* Alignment, Instruction* InsertBefore) {
    return flightPtrForObject(allocateObject(Size, Alignment, InsertBefore), InsertBefore);
  }

  size_t countPtrs(Type* T) {
    assert(!isa<FunctionType>(T));
    assert(!isa<TypedPointerType>(T));
    assert(T != FlightPtrTy);
    assert(!isa<ScalableVectorType>(T));

    if (isa<PointerType>(T))
      return 1;

    if (StructType* ST = dyn_cast<StructType>(T)) {
      size_t Result = 0;
      for (unsigned Index = ST->getNumElements(); Index--;)
        Result += countPtrs(ST->getElementType(Index));
      return Result;
    }

    if (ArrayType* AT = dyn_cast<ArrayType>(T))
      return AT->getNumElements() * countPtrs(AT->getElementType());

    if (FixedVectorType* VT = dyn_cast<FixedVectorType>(T))
      return VT->getElementCount().getFixedValue() * countPtrs(VT->getElementType());

    assert(!hasPtrs(T));
    return 0;
  }

  PointerKind pointerKindDirect(Value* V) {
    if (AllocaInst* AI = dyn_cast<AllocaInst>(V)) {
      assert(AllocaKinds.count(AI));
      return AllocaKinds[AI];
    }
    return PointerKind::Escaping;
  }

  PointerKind underlyingPointerKind(Value* V) {
    return pointerKindDirect(underlyingPtr(V).P);
  }

  bool allocaHasSizeForUs(AllocaInst* AI) {
    std::optional<TypeSize> AllocaSize = AI->getAllocationSize(DL);
    if (!AllocaSize)
      return false;
    if (AllocaSize->isScalable())
      return false;
    uint64_t Result = AllocaSize->getFixedValue();
    return (int64_t)Result >= 0 && (int32_t)Result == (int64_t)Result;
  }

  size_t originalAllocaSize(AllocaInst* AI) {
    std::optional<TypeSize> AllocaSize = AI->getAllocationSize(DL);
    assert(AllocaSize);
    assert(!AllocaSize->isScalable());
    return AllocaSize->getFixedValue();
  }

  size_t alignedAllocaSize(AllocaInst* AI) {
    return (originalAllocaSize(AI) + GCMinAlign - 1) & -GCMinAlign;
  }

  size_t countPtrsForAlloca(AllocaInst* AI) {
    return alignedAllocaSize(AI) / WordSize;
  }
  
  size_t countPtrsForValue(Value* V) {
    // FIXME: We should not be dealing with GEPs here at all.
    Value* UP = underlyingPtr(V).P;
    switch (pointerKindDirect(UP)) {
    case PointerKind::Escaping:
      return countPtrs(V->getType());
    case PointerKind::LocalExplicit:
      return 0;
    case PointerKind::LocalNaked:
      if (UP == V)
        return countPtrsForAlloca(cast<AllocaInst>(V));
      return 0;
    }
    llvm_unreachable("Bad pointer kind");
    return 0;
  }

  bool usesVariadicCC(Function* F) {
    for (BasicBlock& BB : *F) {
      for (Instruction& I : BB) {
        if (CallBase* CI = dyn_cast<CallBase>(&I)) {
          if (Function* F = dyn_cast<Function>(CI->getCalledOperand())) {
            if (F->getName() == "zargs" || F->getName() == "zreturn")
              return true;
          }
        }
        if (IntrinsicInst* II = dyn_cast<IntrinsicInst>(&I)) {
          if (II->getIntrinsicID() == Intrinsic::vastart)
            return true;
        }
      }
    }
    return false;
  }

  bool usesCallee(Function* F) {
    for (BasicBlock& BB : *F) {
      for (Instruction& I : BB) {
        if (CallBase* CI = dyn_cast<CallBase>(&I)) {
          if (Function* F = dyn_cast<Function>(CI->getCalledOperand())) {
            if (F->getName() == "zcallee" || F->getName() == "zcallee_closure_data")
              return true;
          }
        }
      }
    }
    return false;
  }

  void computeFrameIndexMap(const std::vector<BasicBlock*>& Blocks) {
    assert(FrameSize == SIZE_MAX);
    assert(NumStackAuxes == SIZE_MAX);
    
    FrameIndexMap.clear();
    FrameSize = NumSpecialFrameObjects;
    NumStackAuxes = 0;
    HasSetjmps = false;
    UsesVastartOrZargs = false;
    SetjmpSetFrameIndex = SIZE_MAX;

    assert(!Blocks.empty());

    auto LiveCast = [&] (Value* V) -> Value* {
      if (underlyingPointerKind(V) != PointerKind::Escaping)
        return nullptr;
      if (isa<Instruction>(V))
        return V;
      if (Argument* A = dyn_cast<Argument>(V)) {
        if (A->hasByValAttr())
          return A;
      }
      if (!isa<Argument>(V) && !isa<Constant>(V) && !isa<MetadataAsValue>(V) && !isa<InlineAsm>(V)
          && !isa<BasicBlock>(V)) {
        errs() << "V = " << *V << "\n";
        assert(isa<Constant>(V) || isa<MetadataAsValue>(V) || isa<InlineAsm>(V) || isa<BasicBlock>(V));
      }
      return nullptr;
    };

    std::unordered_map<BasicBlock*, std::unordered_set<Value*>> LiveAtTail;
    bool Changed = true;
    while (Changed) {
      Changed = false;

      for (size_t BlockIndex = Blocks.size(); BlockIndex--;) {
        BasicBlock* BB = Blocks[BlockIndex];
        std::unordered_set<Value*> Live = LiveAtTail[BB];

        for (auto It = BB->rbegin(); It != BB->rend(); ++It) {
          Instruction* I = &*It;
          if (verbose)
            errs() << "Liveness dealing with: " << *I << "\n";

          if (LifetimeMarker LM = analyzeLifetimeMarker(I)) {
            switch (LM.LMK) {
            case LifetimeMarkerKind::Start:
              Live.erase(LM.AI);
              break;
            case LifetimeMarkerKind::End:
              Live.insert(LM.AI);
              break;
            }
            continue;
          }

          Live.erase(I);
          
          if (PHINode* Phi = dyn_cast<PHINode>(I)) {
            for (unsigned Index = Phi->getNumIncomingValues(); Index--;) {
              Value* V = Phi->getIncomingValue(Index);
              if (Value* LV = LiveCast(V)) {
                BasicBlock* PBB = Phi->getIncomingBlock(Index);
                Changed |= LiveAtTail[PBB].insert(LV).second;
              }
            }
            continue;
          }

          for (Value* V : I->operand_values()) {
            if (Value* LV = LiveCast(V))
              Live.insert(LV);
          }
        }

        // NOTE: We might be given IR with unreachable blocks. Those will have live-at-head. Like,
        // whatever. But if it's the root block and it has live-at-head then what the fugc.
        if (!BlockIndex) {
          for (Value* V : Live) {
            if (!isa<Argument>(V))
              errs() << "Unexpected live: " << *V << "\n";
            assert(isa<Argument>(V));
            assert(cast<Argument>(V)->hasByValAttr());
          }
        }

        for (BasicBlock* PBB : predecessors(BB)) {
          for (Value* LV : Live)
            Changed |= LiveAtTail[PBB].insert(LV).second;
        }
      }
    }

    // Make sure that AlwaysLive allocas are always live. This is very hacky, but is obviously
    // correct.
    for (BasicBlock* BB : Blocks) {
      for (Instruction& I : *BB) {
        if (AllocaInst* AI = dyn_cast<AllocaInst>(&I)) {
          if (AlwaysLive.count(AI)) {
            for (auto& Pair : LiveAtTail)
              Pair.second.insert(&I);
          }
        }
      }
    }

    std::unordered_map<ValuePtr, std::unordered_set<ValuePtr>> Interference;

    for (size_t BlockIndex = Blocks.size(); BlockIndex--;) {
      BasicBlock* BB = Blocks[BlockIndex];
      std::unordered_set<Value*> Live = LiveAtTail[BB];
      
      for (auto It = BB->rbegin(); It != BB->rend(); ++It) {
        Instruction* I = &*It;

        Instruction* Defined = nullptr;
        if (LifetimeMarker LM = analyzeLifetimeMarker(I)) {
          if (LM.LMK == LifetimeMarkerKind::Start) {
            Live.erase(LM.AI);
            Defined = LM.AI;
          }
        } else {
          Live.erase(I);
          Defined = I;
        }

        if (Defined) {
          size_t NumIPtrs = countPtrsForValue(Defined);
          if (NumIPtrs) {
            for (Value* LV : Live) {
              size_t NumVIPtrs = countPtrsForValue(LV);
              for (size_t LVPtrIndex = NumVIPtrs; LVPtrIndex--;) {
                for (size_t IPtrIndex = NumIPtrs; IPtrIndex--;) {
                  Interference[ValuePtr(Defined, IPtrIndex)].insert(ValuePtr(LV, LVPtrIndex));
                  Interference[ValuePtr(LV, LVPtrIndex)].insert(ValuePtr(Defined, IPtrIndex));
                }
              }
            }
          }
        }

        if (LifetimeMarker LM = analyzeLifetimeMarker(I)) {
          if (LM.LMK == LifetimeMarkerKind::End)
            Live.insert(LM.AI);
        } else {
          for (Value* V : I->operand_values()) {
            if (Value* LV = LiveCast(V))
              Live.insert(LV);
          }
        }
      }
    }

    // The arguments interfere with one another.
    for (Argument& A1 : OldF->args()) {
      size_t NumA1Ptrs = countPtrs(A1.getType());
      if (!NumA1Ptrs)
        continue;
      for (Argument& A2 : OldF->args()) {
        size_t NumA2Ptrs = countPtrs(A2.getType());
        for (size_t A2PtrIndex = NumA2Ptrs; A2PtrIndex--;) {
          for (size_t A1PtrIndex = NumA1Ptrs; A1PtrIndex--;)
            Interference[ValuePtr(&A1, A1PtrIndex)].insert(ValuePtr(&A2, A2PtrIndex));
        }
      }
    }

    // All indices for a value interfere with one another.
    for (Argument& A : OldF->args()) {
      for (size_t PtrIndex = countPtrs(A.getType()); PtrIndex--;) {
        for (size_t APtrIndex = countPtrs(A.getType()); APtrIndex--;)
          Interference[ValuePtr(&A, PtrIndex)].insert(ValuePtr(&A, APtrIndex));
      }
    }
    for (BasicBlock* BB : Blocks) {
      for (Instruction& I : *BB) {
        for (size_t PtrIndex = countPtrsForValue(&I); PtrIndex--;) {
          for (size_t APtrIndex = countPtrsForValue(&I); APtrIndex--;)
            Interference[ValuePtr(&I, PtrIndex)].insert(ValuePtr(&I, APtrIndex));
        }
      }
    }

    // Make this deterministic by having a known order in which we process stuff.
    std::vector<ValuePtr> Order;
    for (Argument& A : OldF->args()) {
      for (size_t PtrIndex = countPtrs(A.getType()); PtrIndex--;)
        Order.push_back(ValuePtr(&A, PtrIndex));
    }
    for (BasicBlock* BB : Blocks) {
      for (Instruction& I : *BB) {
        Value* UP = underlyingPtr(&I).P;
        PointerKind PK = pointerKindDirect(UP);
        if ((PK == PointerKind::LocalNaked || PK == PointerKind::LocalExplicit) && UP != &I)
          FrameIndexMap[ValuePtr(&I, 0)] = FrameEntry(FrameEntryKind::Ignored, 0);
        else {
          for (size_t PtrIndex = countPtrsForValue(&I); PtrIndex--;)
            Order.push_back(ValuePtr(&I, PtrIndex));
        }
      }
    }

    for (ValuePtr IP : Order) {
      const std::unordered_set<ValuePtr>& Adjacency = Interference[IP];
      for (size_t FrameIndex = NumSpecialFrameObjects; ; FrameIndex++) {
        bool Ok = true;
        for (ValuePtr AIP : Adjacency) {
          if (FrameIndexMap.count(AIP) && FrameIndexMap[AIP].Index == FrameIndex) {
            Ok = false;
            break;
          }
        }
        if (Ok) {
          FrameEntryKind FEK;
          PointerKind PK = pointerKindDirect(IP.V);
          assert(PK == PointerKind::Escaping || PK == PointerKind::LocalNaked);
          if (PK == PointerKind::LocalNaked)
            FEK = FrameEntryKind::LowerFromStackAux;
          else
            FEK = FrameEntryKind::Lower;
          FrameIndexMap[IP] = FrameEntry(FEK, FrameIndex);
          FrameSize = std::max(FrameSize, FrameIndex + 1);
          break;
        }
      }
    }

    for (BasicBlock* BB : Blocks) {
      for (Instruction& I : *BB) {
        if (CallBase* CI = dyn_cast<CallBase>(&I)) {
          if (Function* F = dyn_cast<Function>(CI->getCalledOperand())) {
            if (F->getName() == "zargs") {
              UsesVastartOrZargs = true;
              break;
            }
          }
        }
        if (IntrinsicInst* II = dyn_cast<IntrinsicInst>(&I)) {
          if (II->getIntrinsicID() == Intrinsic::vastart) {
            UsesVastartOrZargs = true;
            break;
          }
        }
      }
      if (UsesVastartOrZargs)
        break;
    }
    if (UsesVastartOrZargs) {
      assert(UsesVariadicCC);
      SnapshottedArgsFrameIndex = FrameSize++;
    }

    std::unordered_map<AllocaInst*, std::unordered_set<AllocaInst*>> StackAuxInterference;

    for (size_t BlockIndex = Blocks.size(); BlockIndex--;) {
      BasicBlock* BB = Blocks[BlockIndex];
      std::unordered_set<AllocaInst*> Live;
      for (Value* V : LiveAtTail[BB]) {
        if (pointerKindDirect(V) == PointerKind::LocalExplicit)
          Live.insert(cast<AllocaInst>(V));
      }
      
      for (auto It = BB->rbegin(); It != BB->rend(); ++It) {
        Instruction* I = &*It;
        
        if (LifetimeMarker LM = analyzeLifetimeMarker(I)) {
          if (LM.LMK == LifetimeMarkerKind::Start) {
            Live.erase(LM.AI);
            for (AllocaInst* LAI : Live) {
              StackAuxInterference[LM.AI].insert(LAI);
              StackAuxInterference[LAI].insert(LM.AI);
            }
          }
        }

        if (LifetimeMarker LM = analyzeLifetimeMarker(I)) {
          if (LM.LMK == LifetimeMarkerKind::End)
            Live.insert(LM.AI);
        }
      }
    }

    if (verbose)
      errs() << "Before explicit stack aux allocation, FrameSize = " << FrameSize << "\n";
    
    std::vector<AllocaInst*> StackAuxOrder;
    for (BasicBlock* BB : Blocks) {
      for (Instruction& I : *BB) {
        if (pointerKindDirect(&I) == PointerKind::LocalExplicit)
          StackAuxOrder.push_back(cast<AllocaInst>(&I));
      }
    }

    for (AllocaInst* AI : StackAuxOrder) {
      const std::unordered_set<AllocaInst*>& Adjacency = StackAuxInterference[AI];
      for (size_t FrameIndex = FrameSize; ; FrameIndex++) {
        bool Ok = true;
        for (AllocaInst* AAI : Adjacency) {
          if (FrameIndexMap.count(ValuePtr(AAI, 0))
              && FrameIndexMap[ValuePtr(AAI, 0)].Index == FrameIndex) {
            Ok = false;
            break;
          }
        }
        if (Ok) {
          if (verbose) {
            errs() << "For explicit local " << AI->getName() << " using lowers index " << FrameIndex
                   << "\n";
          }
          FrameIndexMap[ValuePtr(AI, 0)] = FrameEntry(FrameEntryKind::ExplicitStackAux, FrameIndex);
          NumStackAuxes = std::max(NumStackAuxes, FrameIndex + 1 - FrameSize);
          break;
        }
      }
    }

    FrameSize += NumStackAuxes;
    if (verbose) {
      errs() << "After explicit stack aux allocation, NumStackAuxes = " << NumStackAuxes
             << ", FrameSize = " << FrameSize << "\n";
    }

    for (BasicBlock* BB : Blocks) {
      for (Instruction& I : *BB) {
        if (CallBase* CI = dyn_cast<CallBase>(&I)) {
          if (Function* F = dyn_cast<Function>(CI->getCalledOperand())) {
            if (isSetjmp(F)) {
              assert(CI->hasFnAttr(Attribute::ReturnsTwice));
              assert(F->hasFnAttribute(Attribute::ReturnsTwice));
              if (!HasSetjmps) {
                SetjmpSetFrameIndex = FrameSize++;
                HasSetjmps = true;
              }
            }
          }
        }
      }
    }
  }

  Value* recordedLowerPtrAtIndex(size_t FrameIndex, Instruction* InsertBefore) {
    assert(FrameIndex < FrameSize);
    Instruction* LowersPtr = GetElementPtrInst::Create(
      FrameTy, Frame, { ConstantInt::get(Int32Ty, 0), ConstantInt::get(Int32Ty, 2) },
      "filc_frame_lowers", InsertBefore);
    LowersPtr->setDebugLoc(InsertBefore->getDebugLoc());
    Instruction* LowerPtr = GetElementPtrInst::Create(
      RawPtrTy, LowersPtr, { ConstantInt::get(IntPtrTy, FrameIndex) }, "filc_frame_lower",
      InsertBefore);
    LowerPtr->setDebugLoc(InsertBefore->getDebugLoc());
    return LowerPtr;
  }

  void recordLowerAtIndex(Value* Lower, size_t FrameIndex, Instruction* InsertBefore) {
    if (verbose)
      errs() << "Recording lower at " << FrameIndex << "\n";
    (new StoreInst(Lower, recordedLowerPtrAtIndex(FrameIndex, InsertBefore), InsertBefore))
      ->setDebugLoc(InsertBefore->getDebugLoc());
  }

  void recordLowersRecurse(
    Value* ValueKey, Type* T, Value* V, size_t& PtrIndex, Instruction* InsertBefore) {
    assert(!isa<FunctionType>(T));
    assert(!isa<TypedPointerType>(T));
    assert(!isa<ScalableVectorType>(T));
    assert(T != FlightPtrTy);

    if (T == RawPtrTy) {
      assert(FrameIndexMap.count(ValuePtr(ValueKey, PtrIndex)));
      FrameEntry FE = FrameIndexMap[ValuePtr(ValueKey, PtrIndex)];
      switch (FE.FEK) {
      case FrameEntryKind::Ignored:
        break;
      case FrameEntryKind::Lower:
        assert(FE.Index >= NumSpecialFrameObjects);
        recordLowerAtIndex(flightPtrLower(V, InsertBefore), FE.Index, InsertBefore);
        break;
      default:
        llvm_unreachable("Invalid FrameEntryKind");
        break;
      }
      PtrIndex++;
      return;
    }

    if (StructType* ST = dyn_cast<StructType>(T)) {
      for (unsigned Index = ST->getNumElements(); Index--;) {
        Type* InnerT = ST->getElementType(Index);
        Value* InnerV = ExtractValueInst::Create(
          toFlightType(InnerT), V, { Index }, "filc_extract_struct", InsertBefore);
        recordLowersRecurse(ValueKey, InnerT, InnerV, PtrIndex, InsertBefore);
      }
      return;
    }
      
    if (ArrayType* AT = dyn_cast<ArrayType>(T)) {
      assert(static_cast<unsigned>(AT->getNumElements()) == AT->getNumElements());
      for (unsigned Index = static_cast<unsigned>(AT->getNumElements()); Index--;) {
        Value* InnerV = ExtractValueInst::Create(
          toFlightType(AT->getElementType()), V, { Index }, "filc_extract_array", InsertBefore);
        recordLowersRecurse(ValueKey, AT->getElementType(), InnerV, PtrIndex, InsertBefore);
      }
      return;
    }
      
    if (FixedVectorType* VT = dyn_cast<FixedVectorType>(T)) {
      for (unsigned Index = VT->getElementCount().getFixedValue(); Index--;) {
        Value* InnerV = ExtractElementInst::Create(
          V, ConstantInt::get(IntPtrTy, Index), "filc_extract_vector", InsertBefore);
        recordLowersRecurse(ValueKey, VT->getElementType(), InnerV, PtrIndex, InsertBefore);
      }
      return;
    }

    assert(!hasPtrs(T));
  }

  void recordLowers(Value* ValueKey, Type* T, Value* V, Instruction* InsertBefore) {
    if (verbose) {
      errs() << "Recording objects for " << *ValueKey << ", T = " << *T << ", V = " << *V
             << "\n";
    }
    size_t PtrIndex = 0;
    recordLowersRecurse(ValueKey, T, V, PtrIndex, InsertBefore);
    assert(PtrIndex == countPtrs(ValueKey->getType()));
  }

  void recordLowers(Value* V, Instruction* InsertBefore) {
    recordLowers(V, V->getType(), V, InsertBefore);
  }

  Constant* optimizedAccessCheckOrigin(
    int64_t Size, int64_t Alignment, int64_t AlignmentOffset, bool NeedsWrite, DebugLoc ScheduledDI,
    const CombinedDI* SemanticDI) {
    if (verbose) {
      errs() << "Creating access check origin with size = " << Size << ", alignment = " << Alignment
             << ", alignment offset = " << AlignmentOffset << ", needs write = " << NeedsWrite
             << "\n";
    }
    
    assert(static_cast<uint32_t>(Size) == Size);
    assert(static_cast<uint8_t>(Alignment) == Alignment);
    assert(static_cast<uint8_t>(AlignmentOffset) == AlignmentOffset);

    OptimizedAccessCheckOriginKey OACOK(
      static_cast<uint32_t>(Size), static_cast<uint8_t>(Alignment),
      static_cast<uint8_t>(AlignmentOffset), NeedsWrite, ScheduledDI, SemanticDI);
    auto Iter = OptimizedAccessCheckOrigins.find(OACOK);
    if (Iter != OptimizedAccessCheckOrigins.end())
      return Iter->second;

    size_t SemanticDILength = SemanticDI ? SemanticDI->Locations.size() : 0;
    ArrayType* AT = ArrayType::get(RawPtrTy, SemanticDILength + 1);
    StructType* ST = StructType::get(C, { Int32Ty, Int8Ty, Int8Ty, Int8Ty, RawPtrTy, AT });
    std::vector<Constant*> SemanticDICs;
    if (SemanticDI) {
      for (DILocation* DI : SemanticDI->Locations)
        SemanticDICs.push_back(getOrigin(DI));
    }
    SemanticDICs.push_back(RawNull);
    Constant* CS = ConstantStruct::get(
      ST,
      { ConstantInt::get(Int32Ty, Size), ConstantInt::get(Int8Ty, Alignment),
        ConstantInt::get(Int8Ty, AlignmentOffset),
        ConstantInt::get(Int8Ty, static_cast<int>(NeedsWrite)), getOrigin(ScheduledDI),
        ConstantArray::get(AT, SemanticDICs) });
    GlobalVariable* Result = new GlobalVariable(
      M, ST, true, GlobalVariable::PrivateLinkage, CS, "filc_optimized_access_check_origin");
    Result->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    OptimizedAccessCheckOrigins[OACOK] = Result;
    return Result;
  }

  Constant* optimizedAlignmentContradictionOrigin(
    const std::vector<AlignmentAndOffset>& Alignments, DebugLoc ScheduledDI,
    const CombinedDI* SemanticDI) {
    assert(Alignments.size() >= 2);

    OptimizedAlignmentContradictionOriginKey OACOK(Alignments, ScheduledDI, SemanticDI);
    auto Iter = OptimizedAlignmentContradictionOrigins.find(OACOK);
    if (Iter != OptimizedAlignmentContradictionOrigins.end())
      return Iter->second;

    std::vector<Constant*> AlignmentConsts;
    for (AlignmentAndOffset Alignment : Alignments) {
      AlignmentConsts.push_back(
        ConstantStruct::get(
          AlignmentAndOffsetTy,
          { ConstantInt::get(Int8Ty, Alignment.Alignment),
            ConstantInt::get(Int8Ty, Alignment.AlignmentOffset) }));
    }
    AlignmentConsts.push_back(
      ConstantStruct::get(
        AlignmentAndOffsetTy, { ConstantInt::get(Int8Ty, 0), ConstantInt::get(Int8Ty, 0) }));
    ArrayType* AT = ArrayType::get(AlignmentAndOffsetTy, AlignmentConsts.size());
    GlobalVariable* AlignmentsG = new GlobalVariable(
      M, AT, true, GlobalVariable::PrivateLinkage, ConstantArray::get(AT, AlignmentConsts),
      "filc_alignments_and_offsets");
    AlignmentsG->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);

    size_t SemanticDILength = SemanticDI ? SemanticDI->Locations.size() : 0;
    AT = ArrayType::get(RawPtrTy, SemanticDILength + 1);
    StructType* ST = StructType::get(C, { RawPtrTy, RawPtrTy, AT });
    std::vector<Constant*> SemanticDICs;
    if (SemanticDI) {
      for (DILocation* DI : SemanticDI->Locations)
        SemanticDICs.push_back(getOrigin(DI));
    }
    SemanticDICs.push_back(RawNull);
    Constant* CS = ConstantStruct::get(
      ST, { AlignmentsG, getOrigin(ScheduledDI), ConstantArray::get(AT, SemanticDICs) });
    GlobalVariable* Result = new GlobalVariable(
      M, ST, true, GlobalVariable::PrivateLinkage, CS,
      "filc_optimized_alignment_contradiction_origin");
    Result->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
    OptimizedAlignmentContradictionOrigins[OACOK] = Result;
    return Result;
  }

  template<typename CheckT>
  void checkCanonicalizedAccessChecks(const std::vector<CheckT>& Checks) {
    for (size_t Index = 0; Index < Checks.size();) {
      Value* CanonicalPtr = Checks[Index].CanonicalPtr;

      auto failAssert = [&] (const char* File, int Line, const char* Expression) {
        errs() << File << ":" << Line << ": assertion " << Expression << " failed for checks on "
               << CanonicalPtr << ":" << CanonicalPtr->getName() << ":\n"
               << "    " << Checks << "\n";
        llvm_unreachable("checkCanonicalizedAccessChecks found an issue with checks!");
      };

#define acAssert(exp) do { \
        if (!(exp)) \
          failAssert(__FILE__, __LINE__, #exp); \
      } while (false)
    
      int64_t Alignment = 0;
      int64_t AlignmentOffset = 0;
      bool HasLowerBound = false;
      bool HasUpperBound = false;
      
      size_t BeginIndex = Index;
      size_t EndIndex;
      for (EndIndex = BeginIndex;
           EndIndex < Checks.size() && Checks[EndIndex].CanonicalPtr == CanonicalPtr;
           ++EndIndex) {
        CheckT AC = Checks[EndIndex];
        switch (AC.CK) {
        case CheckKind::Alignment:
        case CheckKind::KnownAlignment:
          if (Alignment)
            acAssert((AlignmentOffset % AC.Size) != AC.Offset);
          else {
            Alignment = AC.Size;
            AlignmentOffset = AC.Offset;
            acAssert(AlignmentOffset >= 0);
            acAssert(AlignmentOffset < Alignment);
          }
          break;
        case CheckKind::KnownLowerBound:
        case CheckKind::LowerBound:
          HasLowerBound = true;
          break;
        case CheckKind::UpperBound:
          HasUpperBound = true;
          break;
        default:
          break;
        }
      }

      if (HasUpperBound)
        acAssert(HasLowerBound);

      Index = EndIndex;
    }
  }

  AllocaInst* canonicalPtrAuxBaseVar(Value* P) {
    auto Iter = CanonicalPtrAuxBaseVars.find(P);
    if (Iter != CanonicalPtrAuxBaseVars.end())
      return Iter->second;

    assert(AuxBaseVarCreationAllowed);

    Instruction* InsertBefore = &NewF->getEntryBlock().front();
    AllocaInst* Result = new AllocaInst(RawPtrTy, 0, nullptr, "filc_aux_base_var", InsertBefore);
    new StoreInst(RawNull, Result, InsertBefore);
    
    CanonicalPtrAuxBaseVars[P] = Result;
    return Result;
  }

  void storeOrigin(Value* Origin, Instruction* InsertBefore) {
    Instruction* OriginPtr = GetElementPtrInst::Create(
      FrameTy, Frame, { ConstantInt::get(Int32Ty, 0), ConstantInt::get(Int32Ty, 1) },
      "filc_frame_origin_ptr", InsertBefore);
    OriginPtr->setDebugLoc(InsertBefore->getDebugLoc());
    (new StoreInst(Origin, OriginPtr, InsertBefore))->setDebugLoc(InsertBefore->getDebugLoc());
  }

  FullMemoryAccessData accessDataForOperand(Value* P, Instruction* I, unsigned OperandIdx,
                                            Instruction* InsertBefore) {
    if (verbose)
      errs() << "Looking at " << *I << " and operand index " << OperandIdx << "\n";
    assert(PtrOperandDatas.count(I));
    assert(OperandIdx < PtrOperandDatas[I].size());
    FullMemoryAccessData FMAD;
    FMAD.POD = PtrOperandDatas[I][OperandIdx];
    FMAD.MAD.P = flightPtrPtr(P, InsertBefore);
    Value* Offset;
    if (FMAD.POD.PK == PointerKind::Escaping) {
      FMAD.MAD.Lower = flightPtrLower(P, InsertBefore);
      if (verbose) {
        errs() << "For " << *I << " and operand index " << OperandIdx << " got AuxBaseVar = "
               << *FMAD.POD.AuxBaseVar << "\n";
      }
      Instruction* AuxBasePI = new LoadInst(
        RawPtrTy, FMAD.POD.AuxBaseVar, "filc_aux_base_ptr", InsertBefore);
      AuxBasePI->setDebugLoc(InsertBefore->getDebugLoc());
      FMAD.MAD.AuxBaseP = AuxBasePI;
      FMAD.MAD.MK = MemoryKind::Heap;
      Offset = flightPtrOffset(P, InsertBefore);
    } else {
      if (verbose) {
        errs() << "For " << *I << " and operand index " << OperandIdx << " got OrigAI  = "
               << *FMAD.POD.LAD.OrigAI << "\n";
      }
      FMAD.MAD.AuxBaseP = FMAD.POD.LAD.Aux;
      if (FMAD.POD.PK == PointerKind::LocalExplicit) {
        FMAD.MAD.MK = MemoryKind::LocalExplicit;
        Instruction* PayloadAsInt = new PtrToIntInst(
          FMAD.POD.LAD.Payload, IntPtrTy, "filc_local_payload_as_int", InsertBefore);
        PayloadAsInt->setDebugLoc(InsertBefore->getDebugLoc());
        Instruction* OffsetI = BinaryOperator::Create(
          Instruction::Sub, flightPtrPtrAsInt(P, InsertBefore), PayloadAsInt, "filc_local_ptr_offset",
          InsertBefore);
        OffsetI->setDebugLoc(OffsetI->getDebugLoc());
        Offset = OffsetI;
      } else {
        assert(FMAD.POD.PK == PointerKind::LocalNaked);
        assert(FMAD.POD.PAR.R == PtrRandomness::NotRandom);
        FMAD.MAD.OrigAI = FMAD.POD.LAD.OrigAI;
        FMAD.MAD.MK = MemoryKind::LocalNaked;
        FMAD.MAD.LocalOffset = FMAD.POD.PAR.Offset;
        FMAD.MAD.Size = FMAD.POD.LAD.Size;
        assert(!(FMAD.MAD.Size % WordSize));
        Offset = ConstantInt::get(IntPtrTy, FMAD.MAD.LocalOffset);
      }
    }
    Instruction* AuxPI = GetElementPtrInst::Create(
      Int8Ty, FMAD.MAD.AuxBaseP, Offset, "filc_aux_ptr", InsertBefore);
    AuxPI->setDebugLoc(InsertBefore->getDebugLoc());
    FMAD.MAD.AuxP = AuxPI;
    FMAD.MAD.validate();
    return FMAD;
  }

  void emitChecks(std::vector<AccessCheckWithDI> Checks, Instruction* Inst) {
    if (verbose)
      errs() << "Raw checks: " << Checks << "\n";
    canonicalizeAccessChecks(Checks);
    if (verbose)
      errs() << "Canonicalized checks: " << Checks << "\n";

    for (size_t Index = 0; Index < Checks.size();) {
      Value* CanonicalPtr = Checks[Index].CanonicalPtr;
      Value* FlightPtr = lowerConstantValue(CanonicalPtr, Inst);
      Value* UnderlyingPtr = underlyingPtr(CanonicalPtr).P;
      PointerKind PK = pointerKindDirect(UnderlyingPtr);
      LocalAllocaData LAD;
      if (PK != PointerKind::Escaping) {
        AllocaInst* AI = cast<AllocaInst>(UnderlyingPtr);
        assert(LocalAllocaDatas.count(AI));
        LAD = LocalAllocaDatas[AI];
      }

      auto ptrWithOffset = [&] (int64_t Offset, Instruction* InsertBefore) {
        assert((int32_t)Offset == Offset);
        assert(FlightPtr);
        
        return flightPtrWithOffset(FlightPtr, ConstantInt::get(IntPtrTy, Offset), InsertBefore);
      };
      
      int64_t Alignment = 0;
      int64_t AlignmentOffset = 0;
      bool AlignmentContradiction = false;
      bool NeedsWritable = false;
      int64_t LowerBoundOffset = 0;
      bool HasLowerBound = false;
      int64_t UpperBoundOffset = 0;
      bool HasUpperBound = false;
      bool HasRangeCheck = false;
      const CombinedDI* RangeDI = nullptr;
      bool HasFreeCheck = false;

      size_t BeginIndex = Index;
      size_t EndIndex;
      for (EndIndex = BeginIndex;
           EndIndex < Checks.size() && Checks[EndIndex].CanonicalPtr == CanonicalPtr;
           ++EndIndex) {
        AccessCheckWithDI AC = Checks[EndIndex];
        switch (AC.CK) {
        case CheckKind::ValidObject:
          HasRangeCheck = true;
          RangeDI = combineDI(RangeDI, AC.DI);
          break;
        case CheckKind::Alignment:
        case CheckKind::KnownAlignment:
          if (Alignment)
            AlignmentContradiction = true;
          Alignment = AC.Size;
          AlignmentOffset = AC.Offset;
          HasRangeCheck = true;
          RangeDI = combineDI(RangeDI, AC.DI);
          break;
        case CheckKind::CanWrite:
          NeedsWritable = true;
          HasRangeCheck = true;
          RangeDI = combineDI(RangeDI, AC.DI);
          break;
        case CheckKind::KnownLowerBound:
        case CheckKind::LowerBound:
          LowerBoundOffset = AC.Offset;
          HasLowerBound = true;
          HasRangeCheck = true;
          RangeDI = combineDI(RangeDI, AC.DI);
          break;
        case CheckKind::UpperBound:
          UpperBoundOffset = AC.Offset;
          HasUpperBound = true;
          HasRangeCheck = true;
          RangeDI = combineDI(RangeDI, AC.DI);
          break;
        case CheckKind::NotFree:
          HasRangeCheck = true;
          HasFreeCheck = true;
          RangeDI = combineDI(RangeDI, AC.DI);
          break;
        case CheckKind::GetAuxPtr:
        case CheckKind::EnsureAuxPtr:
          break;
        }
      }
      assert(EndIndex > BeginIndex);
      assert(EndIndex <= Checks.size());

      Index = EndIndex;

      if (AlignmentContradiction) {
        std::vector<AlignmentAndOffset> Alignments;
        for (size_t SubIndex = BeginIndex; SubIndex < EndIndex; ++SubIndex) {
          AccessCheckWithDI AC = Checks[SubIndex];
          switch (AC.CK) {
          case CheckKind::Alignment:
          case CheckKind::KnownAlignment:
            assert(static_cast<uint8_t>(AC.Offset) == AC.Offset);
            assert(static_cast<uint8_t>(AC.Size) == AC.Size);
            Alignments.push_back(AlignmentAndOffset(AC.Size, AC.Offset));
            break;

          default:
            break;
          }
        }

        if (PK == PointerKind::Escaping) {
          CallInst::Create(
            OptimizedAlignmentContradiction,
            { FlightPtr,
              optimizedAlignmentContradictionOrigin(Alignments, Inst->getDebugLoc(), RangeDI) },
            "", Inst)->setDebugLoc(Inst->getDebugLoc());
          continue;
        }

        Instruction* PayloadAsInt = new PtrToIntInst(
          LAD.Payload, IntPtrTy, "filc_local_payload_as_int", Inst);
        PayloadAsInt->setDebugLoc(Inst->getDebugLoc());
        Instruction* Offset = BinaryOperator::Create(
          Instruction::Sub, flightPtrPtrAsInt(FlightPtr, Inst), PayloadAsInt, "filc_local_offset",
          Inst);
        Offset->setDebugLoc(Inst->getDebugLoc());
        CallInst::Create(
          OptimizedStackAlignmentContradiction,
          { Offset, ConstantInt::get(IntPtrTy, LAD.Size),
            optimizedAlignmentContradictionOrigin(Alignments, Inst->getDebugLoc(), RangeDI) },
          "", Inst)->setDebugLoc(Inst->getDebugLoc());
        continue;
      }

      if (!Alignment) {
        assert(!AlignmentOffset);
        Alignment = 1;
      }
      assert(Alignment == 1 || Alignment == 2 || Alignment == 4 || Alignment == 8);
      assert(AlignmentOffset < Alignment);

      if (HasUpperBound) {
        // It's not clear if we're actually doing this right.
        //
        // Forward propagation by itself does the right thing: if it eliminates a lower bound check,
        // it replaces it with KnownLowerBound. That's fine.
        //
        // But does backward propagation do the right thing? In backward propagation, we convert the
        // known lower bound checks to just plain lower bound checks. Do they then survive correctly?
        //
        // I guess we'll find out!
        assert(HasLowerBound);

        // What does this mean? Like, what if this is false?
        //
        // Say we have UpperBoundOffset = LowerBoundOffset. This is like saying:
        //
        // P > Lower && P <= Upper
        //
        // Effectively, we're asking that a zero-byte entry is within bounds. That's a fine, albeit
        // dumb, thing to assert./
        //
        // Say we have UpperBoundOffset < LowerBoundOffset. This is like saying:
        //
        // P + 1 > Lower && P <= Upper
        //
        // Since Lower <= Upper for sure, This means that a pointer that is 1 byte below lower is
        // going to pass this test. Again, pointless.
        assert(UpperBoundOffset > LowerBoundOffset);
      }

      BasicBlock* RangeFailB = nullptr;
      if (HasRangeCheck) {
        RangeFailB = BasicBlock::Create(C, "filc_range_fail_block", NewF);
        Instruction* RangeFailTerm = new UnreachableInst(C, RangeFailB);
        if (PK == PointerKind::Escaping) {
          CallInst::Create(
            OptimizedAccessCheckFail,
            { ptrWithOffset(LowerBoundOffset, RangeFailTerm),
              optimizedAccessCheckOrigin(
                HasUpperBound ? UpperBoundOffset - LowerBoundOffset : 0, Alignment,
                PositiveModulo(AlignmentOffset - LowerBoundOffset, Alignment),
                NeedsWritable, Inst->getDebugLoc(), RangeDI) },
            "", RangeFailTerm)->setDebugLoc(Inst->getDebugLoc());
        } else {
          Instruction* PayloadAsInt = new PtrToIntInst(
            LAD.Payload, IntPtrTy, "filc_local_payload_as_int", RangeFailTerm);
          PayloadAsInt->setDebugLoc(Inst->getDebugLoc());
          Instruction* Offset = BinaryOperator::Create(
            Instruction::Sub,
            flightPtrPtrAsInt(ptrWithOffset(LowerBoundOffset, RangeFailTerm), RangeFailTerm),
            PayloadAsInt, "filc_local_offset", RangeFailTerm);
          Offset->setDebugLoc(Inst->getDebugLoc());
          CallInst::Create(
            OptimizedStackAccessCheckFail,
            { Offset, ConstantInt::get(IntPtrTy, LAD.Size),
              optimizedAccessCheckOrigin(
                HasUpperBound ? UpperBoundOffset - LowerBoundOffset : 0, Alignment,
                PositiveModulo(AlignmentOffset - LowerBoundOffset, Alignment),
                NeedsWritable, Inst->getDebugLoc(), RangeDI) },
            "", RangeFailTerm)->setDebugLoc(Inst->getDebugLoc());
        }
      }

      for (size_t SubIndex = BeginIndex; SubIndex < EndIndex; ++SubIndex) {
        AccessCheckWithDI AC = Checks[SubIndex];
        switch (AC.CK) {
        case CheckKind::ValidObject: {
          if (PK != PointerKind::Escaping)
            break;
          ICmpInst* NullObject = new ICmpInst(
            Inst, ICmpInst::ICMP_EQ, flightPtrLower(FlightPtr, Inst), RawNull,
            "filc_null_access_object");
          NullObject->setDebugLoc(Inst->getDebugLoc());
          SplitBlockAndInsertIfThen(
            expectFalse(NullObject, Inst), Inst, false, nullptr, nullptr, nullptr, RangeFailB);
          break;
        }

        case CheckKind::KnownAlignment:
          break;

        case CheckKind::Alignment: {
          assert(AC.Size == 1 || AC.Size == 2 || AC.Size == 4 || AC.Size == 8 || AC.Size == 16);
          if (AC.Size == 1)
            break;
          Instruction* PtrInt = new PtrToIntInst(
            flightPtrPtr(ptrWithOffset(AC.Offset, Inst), Inst), IntPtrTy, "filc_ptr_as_int", Inst);
          PtrInt->setDebugLoc(Inst->getDebugLoc());
          Instruction* Masked = BinaryOperator::Create(
            Instruction::And, PtrInt, ConstantInt::get(IntPtrTy, AC.Size - 1),
            "filc_ptr_alignment_masked", Inst);
          Masked->setDebugLoc(Inst->getDebugLoc());
          ICmpInst* IsAligned = new ICmpInst(
            Inst, ICmpInst::ICMP_EQ, Masked, ConstantInt::get(IntPtrTy, 0),
            "filc_ptr_is_aligned");
          IsAligned->setDebugLoc(Inst->getDebugLoc());
          SplitBlockAndInsertIfElse(
            expectTrue(IsAligned, Inst), Inst, false, nullptr, nullptr, nullptr, RangeFailB);
          break;
        }

        case CheckKind::CanWrite: {
          if (PK != PointerKind::Escaping)
            break;
          assert(NeedsWritable);
          BinaryOperator* Masked = BinaryOperator::Create(
            Instruction::And, flagsForLower(flightPtrLower(FlightPtr, Inst), Inst),
            ConstantInt::get(IntPtrTy, ObjectFlagReadonly | (HasFreeCheck ? ObjectFlagFree : 0)),
            "filc_flags_masked", Inst);
          Masked->setDebugLoc(Inst->getDebugLoc());
          ICmpInst* IsNotReadOnly = new ICmpInst(
            Inst, ICmpInst::ICMP_EQ, Masked, ConstantInt::get(IntPtrTy, 0),
            "filc_object_is_not_read_only");
          IsNotReadOnly->setDebugLoc(Inst->getDebugLoc());
          SplitBlockAndInsertIfElse(
            expectTrue(IsNotReadOnly, Inst), Inst, false, nullptr, nullptr, nullptr, RangeFailB);
          break;
        }

        case CheckKind::NotFree: {
          if (PK != PointerKind::Escaping)
            break;
          assert(HasFreeCheck);
          if ((HasLowerBound && HasUpperBound) || NeedsWritable)
            break;
          BinaryOperator* Masked = BinaryOperator::Create(
            Instruction::And, flagsForLower(flightPtrLower(FlightPtr, Inst), Inst),
            ConstantInt::get(IntPtrTy, ObjectFlagFree), "filc_flags_masked", Inst);
          Masked->setDebugLoc(Inst->getDebugLoc());
          ICmpInst* IsNotFree = new ICmpInst(
            Inst, ICmpInst::ICMP_EQ, Masked, ConstantInt::get(IntPtrTy, 0),
            "filc_object_is_not_free");
          IsNotFree->setDebugLoc(Inst->getDebugLoc());
          SplitBlockAndInsertIfElse(
            expectTrue(IsNotFree, Inst), Inst, false, nullptr, nullptr, nullptr, RangeFailB);
          break;
        }
          
        case CheckKind::UpperBound: {
          assert(HasLowerBound);
          assert(HasUpperBound);
          assert(UpperBoundOffset > LowerBoundOffset);
          Value* Upper;
          if (PK == PointerKind::Escaping)
            Upper = upperForLower(flightPtrLower(FlightPtr, Inst), Inst);
          else {
            assert(LAD.Size != UINT64_MAX);
            Instruction* GEP = GetElementPtrInst::Create(
              Int8Ty, LAD.Payload, { ConstantInt::get(IntPtrTy, LAD.Size) }, "filc_stack_upper", Inst);
            GEP->setDebugLoc(Inst->getDebugLoc());
            Upper = GEP;
          }
          Value* Ptr = flightPtrPtr(ptrWithOffset(LowerBoundOffset, Inst), Inst);
          Instruction* IsBelowUpper;
          if (UpperBoundOffset - LowerBoundOffset == Alignment
              && !AlignmentContradiction
              // It's possible for the distance between upper bound and lower bound to be 16, but
              // the KnownAlignment to be 16, but we're actually checking a 16-sized range of bytes
              // not aligned to 16 (that straddle two 16 byte words).
              && PositiveModulo(UpperBoundOffset, Alignment) == AlignmentOffset) {
            assert(PositiveModulo(LowerBoundOffset, Alignment) == AlignmentOffset);
            IsBelowUpper = new ICmpInst(
              Inst, ICmpInst::ICMP_ULT, Ptr, Upper, "filc_ptr_below_upper");
          } else {
            Instruction* UpperMinus = GetElementPtrInst::Create(
              Int8Ty, Upper, { ConstantInt::get(IntPtrTy, LowerBoundOffset - UpperBoundOffset) },
              "filc_upper_minus", Inst);
            UpperMinus->setDebugLoc(Inst->getDebugLoc());
            IsBelowUpper = new ICmpInst(
              Inst, ICmpInst::ICMP_ULE, Ptr, UpperMinus, "filc_ptr_below_equal_upper");
          }
          IsBelowUpper->setDebugLoc(Inst->getDebugLoc());
          SplitBlockAndInsertIfElse(
            expectTrue(IsBelowUpper, Inst), Inst, false, nullptr, nullptr, nullptr, RangeFailB);
          break;
        }

        case CheckKind::KnownLowerBound:
          assert(HasLowerBound);
          break;

        case CheckKind::LowerBound: {
          assert(HasLowerBound);
          Value* Lower;
          if (PK == PointerKind::Escaping)
            Lower = flightPtrLower(FlightPtr, Inst);
          else
            Lower = LAD.Payload;
          Instruction* IsBelowLower = new ICmpInst(
            Inst, ICmpInst::ICMP_ULT, flightPtrPtr(ptrWithOffset(LowerBoundOffset, Inst), Inst),
            Lower, "filc_ptr_below_lower");
          IsBelowLower->setDebugLoc(Inst->getDebugLoc());
          SplitBlockAndInsertIfThen(
            expectFalse(IsBelowLower, Inst), Inst, false, nullptr, nullptr, nullptr, RangeFailB);
          break;
        }

        case CheckKind::GetAuxPtr: {
          if (PK != PointerKind::Escaping)
            break;
          AllocaInst* AuxBaseVar = canonicalPtrAuxBaseVar(CanonicalPtr);
          (new StoreInst(auxPtrForLower(flightPtrLower(FlightPtr, Inst), Inst), AuxBaseVar,
                         Inst))->setDebugLoc(Inst->getDebugLoc());
          break;
        }

        case CheckKind::EnsureAuxPtr: {
          if (PK != PointerKind::Escaping)
            break;
          AllocaInst* AuxBaseVar = canonicalPtrAuxBaseVar(CanonicalPtr);
          Instruction* AuxBase = new LoadInst(RawPtrTy, AuxBaseVar, "filc_get_aux_ptr", Inst);
          AuxBase->setDebugLoc(Inst->getDebugLoc());
          Instruction* AuxBaseIsNull = new ICmpInst(
            Inst, ICmpInst::ICMP_EQ, AuxBase, RawNull, "filc_aux_ptr_is_null");
          AuxBaseIsNull->setDebugLoc(Inst->getDebugLoc());
          Instruction* ThenTerm =
            SplitBlockAndInsertIfThen(expectFalse(AuxBaseIsNull, Inst), Inst, false);
          // FIXME: Is this the right origin? I think it is, because we aren't hoisting these.
          storeOrigin(getOrigin(Inst->getDebugLoc()), ThenTerm);
          Instruction* CreatedAuxBase = CallInst::Create(
            ObjectEnsureAuxPtrOutline,
            { MyThread, objectForLower(flightPtrLower(FlightPtr, ThenTerm), ThenTerm) },
            "filc_object_ensure_aux_ptr_outline", ThenTerm);
          CreatedAuxBase->setDebugLoc(Inst->getDebugLoc());
          (new StoreInst(CreatedAuxBase, AuxBaseVar, ThenTerm))->setDebugLoc(Inst->getDebugLoc());
          break;
        } }
      }
    }
  }

  void emitChecksForInst(Instruction* Inst) {
    if (verbose)
      errs() << "Emitting checks for " << *Inst << "\n";
    auto Iter = ChecksForInst.find(Inst);
    if (Iter == ChecksForInst.end())
      return;
    emitChecks(Iter->second, Inst);
  }

  void buildCheck(int64_t Size, int64_t Alignment, Value* CanonicalPtr, int64_t Offset,
                  AccessKind AK, const CombinedDI* DI, std::vector<AccessCheckWithDI>& Checks) {
    if (verbose) {
      errs() << "Building check: Size = " << Size << ", Alignment = " << Alignment
             << ", CanonicalPtr = " << CanonicalPtr->getName() << ", Offset = " << Offset
             << ", AK = " << accessKindString(AK) << ", DI = " << DI << "\n";
    }
    assert(Size);
    assert(Alignment);
    assert(!((Alignment - 1) & Alignment));
    assert((int32_t)Offset == Offset);
    Alignment = std::min(Alignment, static_cast<int64_t>(WordSize));
    Checks.push_back(
      AccessCheckWithDI(CanonicalPtr, 0, 0, CheckKind::ValidObject, DI));
    Checks.push_back(
      AccessCheckWithDI(CanonicalPtr, PositiveModulo(Offset, Alignment), Alignment,
                        CheckKind::Alignment, DI));
    if (AK == AccessKind::Write) {
      Checks.push_back(
        AccessCheckWithDI(CanonicalPtr, 0, 0, CheckKind::CanWrite, DI));
    }
    Checks.push_back(
      AccessCheckWithDI(CanonicalPtr, Offset, 0, CheckKind::LowerBound, DI));
    assert(Offset + Size > Offset);
    Checks.push_back(
      AccessCheckWithDI(CanonicalPtr, Offset + Size, 0, CheckKind::UpperBound, DI));
    Checks.push_back(
      AccessCheckWithDI(CanonicalPtr, 0, 0, CheckKind::NotFree, DI));
  }

  bool needToCheckAlignment(Type* T) {
    // FIXME: This is very X86-specific.
    
    assert(T != FlightPtrTy);

    if (isa<FunctionType>(T))
      return false;

    if (isa<TypedPointerType>(T)) {
      llvm_unreachable("Shouldn't ever see typed pointers");
      return false;
    }

    if (isa<PointerType>(T))
      return true;

    if (T->isIntegerTy())
      return false;

    if (T->isFloatTy() || T->isDoubleTy())
      return false;

    if (StructType* ST = dyn_cast<StructType>(T)) {
      assert(!ST->isOpaque());
      for (Type* InnerT : ST->elements()) {
        if (needToCheckAlignment(InnerT))
          return true;
      }
      return false;
    }

    if (ArrayType* AT = dyn_cast<ArrayType>(T))
      return needToCheckAlignment(AT->getElementType());

    return true;
  }

  void buildChecksRecurse(Type* T, Value* HighP, int64_t Offset, size_t Alignment, AtomicOrdering AO,
                          AccessKind AK, const CombinedDI* DI,
                          std::vector<AccessCheckWithDI>& Checks) {
    if (!hasPtrs(T)) {
      size_t CheckedAlignment;
      if (needToCheckAlignment(T) || AO != AtomicOrdering::NotAtomic)
        CheckedAlignment = MinAlign(DL.getABITypeAlign(T).value(), Alignment);
      else
        CheckedAlignment = 1;
      buildCheck(DL.getTypeStoreSize(T), CheckedAlignment, HighP, Offset, AK, DI, Checks);
      return;
    }
    
    if (isa<FunctionType>(T)) {
      llvm_unreachable("shouldn't see function types in buildChecksRecurse");
      return;
    }

    if (isa<TypedPointerType>(T)) {
      llvm_unreachable("Shouldn't ever see typed pointers");
      return;
    }

    assert(T != FlightPtrTy);

    if (T == RawPtrTy) {
      // FIXME: atomic accesses do checks in native code, so it's kinda redundant that we also do them
      // in compiled code. The aux ptr shenanigans are especially redundant.
      buildCheck(WordSize, WordSize, HighP, Offset, AK, DI, Checks);
      Checks.push_back(AccessCheckWithDI(HighP, 0, 0, CheckKind::GetAuxPtr, DI));
      if (AK == AccessKind::Write)
        Checks.push_back(AccessCheckWithDI(HighP, 0, 0, CheckKind::EnsureAuxPtr, DI));
      return;
    }

    assert(!isa<PointerType>(T));

    if (StructType* ST = dyn_cast<StructType>(T)) {
      const StructLayout* SL = DL.getStructLayout(ST);
      for (unsigned Index = ST->getNumElements(); Index--;) {
        Type* InnerT = ST->getElementType(Index);
        buildChecksRecurse(
          InnerT, HighP, Offset + SL->getElementOffset(Index), Alignment, AO, AK, DI, Checks);
      }
      return;
    }
      
    if (ArrayType* AT = dyn_cast<ArrayType>(T)) {
      Type* ET = AT->getElementType();
      size_t ESize = DL.getTypeAllocSize(ET);
      for (int64_t Index = AT->getNumElements(); Index--;)
        buildChecksRecurse(ET, HighP, Offset + Index * ESize, Alignment, AO, AK, DI, Checks);
      return;
    }
      
    if (FixedVectorType* VT = dyn_cast<FixedVectorType>(T)) {
      Type* ET = VT->getElementType();
      size_t ESize = DL.getTypeAllocSize(ET);
      for (int64_t Index = VT->getElementCount().getFixedValue(); Index--;)
        buildChecksRecurse(ET, HighP, Offset + Index * ESize, Alignment, AO, AK, DI, Checks);
      return;
    }

    if (isa<ScalableVectorType>(T)) {
      llvm_unreachable("Shouldn't see scalable vector types in buildChecksRecurse");
      return;
    }

    llvm_unreachable("Should not get here.");
  }

  PtrAndRandom underlyingPtr(Value* P) {
    PtrAndRandom PAR(P, PtrRandomness::NotRandom, 0);
    while (GetElementPtrInst* GEP = dyn_cast<GetElementPtrInst>(PAR.P)) {
      PAR.P = GEP->getPointerOperand();
      APInt OffsetAP(64, 0, false);
      if (GEP->accumulateConstantOffset(DLBefore, OffsetAP))
        PAR.Offset += OffsetAP.getZExtValue();
      else
        PAR.R = PtrRandomness::Random;
    }
    if ((int32_t)PAR.Offset != PAR.Offset)
      PAR.R = PtrRandomness::Random;
    if (PAR.R == PtrRandomness::Random)
      PAR.Offset = 0;
    return PAR;
  }

  PtrAndRandom underlyingPtr(PtrAndOffset PAO) {
    return underlyingPtr(PAO.HighP).plus(PAO.Offset);
  }

  Instruction* getAsInstruction(ConstantExpr* CE) {
    Instruction* I = CE->getAsInstruction();
    dropUB(I);
    return I;
  }

  PtrAndOffset canonicalizePtr(Value* HighP) {
    Value* OriginalHighP = HighP;
    int64_t Offset = 0;

    if (GetElementPtrInst* GEP = dyn_cast<GetElementPtrInst>(HighP)) {
      APInt OffsetAP(64, 0, false);
      if (GEP->accumulateConstantOffset(DLBefore, OffsetAP)) {
        Offset += OffsetAP.getZExtValue();
        HighP = GEP->getPointerOperand();
      }
    }

    if (ConstantExpr* CE = dyn_cast<ConstantExpr>(HighP)) {
      if (CE->getOpcode() == Instruction::GetElementPtr) {
        GetElementPtrInst* GEP = cast<GetElementPtrInst>(getAsInstruction(CE));
        APInt OffsetAP(64, 0, false);
        if (GEP->accumulateConstantOffset(DLBefore, OffsetAP)) {
          Offset += OffsetAP.getZExtValue();
          HighP = GEP->getPointerOperand();
        }
        GEP->deleteValue();
      }
    }

    // As a way of preventing any overflow shenanigans, we carry around offsets in int64's but give
    // up if the offset can't fit in a int32. This is more conservative than necessary, but also
    // likely harmless - it's not clear that we need to worry at all about "optimizations" for
    // programs that use ginormous field offsets or ginormous constant array indices.
    if ((int32_t)Offset == Offset)
      return PtrAndOffset(HighP, Offset);
    
    return PtrAndOffset(OriginalHighP, 0);
  }

  IntrinsicAccessDetails analyzeIntrinsicLoadStore(Instruction* I) {
    if (IntrinsicInst* II = dyn_cast<IntrinsicInst>(I)) {
      IntrinsicAccessDetails IAD;
      switch (II->getIntrinsicID()) {
      case Intrinsic::masked_load:
      case Intrinsic::masked_store:
        assert(II->arg_size() == 4);
        IAD.AK = II->getIntrinsicID() == Intrinsic::masked_store
          ? AccessKind::Write : AccessKind::Read;
        if (IAD.AK == AccessKind::Write) {
          IAD.T = cast<FixedVectorType>(II->getArgOperand(0)->getType());
          IAD.Ptr = II->getArgOperand(1);
          IAD.Mask = II->getArgOperand(3);
          IAD.Alignment = cast<ConstantInt>(II->getArgOperand(2))->getZExtValue();
        } else {
          IAD.T = cast<FixedVectorType>(II->getType());
          IAD.Ptr = II->getArgOperand(0);
          IAD.Mask = II->getArgOperand(2);
          IAD.Alignment = cast<ConstantInt>(II->getArgOperand(1))->getZExtValue();
        }
        assert(!hasPtrs(IAD.T));
        break;
      case Intrinsic::x86_avx_maskload_pd:
        assert(II->arg_size() == 2);
        IAD.AK = AccessKind::Read;
        IAD.T = FixedVectorType::get(DoubleTy, 2);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx_maskload_ps:
        assert(II->arg_size() == 2);
        IAD.AK = AccessKind::Read;
        IAD.T = FixedVectorType::get(FloatTy, 4);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx_maskload_pd_256:
        assert(II->arg_size() == 2);
        IAD.AK = AccessKind::Read;
        IAD.T = FixedVectorType::get(DoubleTy, 4);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx_maskload_ps_256:
        assert(II->arg_size() == 2);
        IAD.AK = AccessKind::Read;
        IAD.T = FixedVectorType::get(FloatTy, 8);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx_maskstore_pd:
        assert(II->arg_size() == 3);
        IAD.AK = AccessKind::Write;
        IAD.T = FixedVectorType::get(DoubleTy, 2);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx_maskstore_ps:
        assert(II->arg_size() == 3);
        IAD.AK = AccessKind::Write;
        IAD.T = FixedVectorType::get(FloatTy, 4);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx_maskstore_pd_256:
        assert(II->arg_size() == 3);
        IAD.AK = AccessKind::Write;
        IAD.T = FixedVectorType::get(DoubleTy, 4);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx_maskstore_ps_256:
        assert(II->arg_size() == 3);
        IAD.AK = AccessKind::Write;
        IAD.T = FixedVectorType::get(FloatTy, 8);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx512_mask_pmov_wb_mem_512:
        IAD.AK = AccessKind::Write;
        IAD.T = FixedVectorType::get(Int8Ty, 32);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(2);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx2_maskload_d:
        assert(II->arg_size() == 2);
        IAD.AK = AccessKind::Read;
        IAD.T = FixedVectorType::get(Int32Ty, 4);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx2_maskload_q:
        assert(II->arg_size() == 2);
        IAD.AK = AccessKind::Read;
        IAD.T = FixedVectorType::get(Int64Ty, 2);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx2_maskload_d_256:
        assert(II->arg_size() == 2);
        IAD.AK = AccessKind::Read;
        IAD.T = FixedVectorType::get(Int32Ty, 8);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx2_maskload_q_256:
        assert(II->arg_size() == 2);
        IAD.AK = AccessKind::Read;
        IAD.T = FixedVectorType::get(Int64Ty, 4);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx2_maskstore_d:
        assert(II->arg_size() == 3);
        IAD.AK = AccessKind::Write;
        IAD.T = FixedVectorType::get(Int32Ty, 4);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx2_maskstore_q:
        assert(II->arg_size() == 3);
        IAD.AK = AccessKind::Write;
        IAD.T = FixedVectorType::get(Int64Ty, 2);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx2_maskstore_d_256:
        assert(II->arg_size() == 3);
        IAD.AK = AccessKind::Write;
        IAD.T = FixedVectorType::get(Int32Ty, 8);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_avx2_maskstore_q_256:
        assert(II->arg_size() == 3);
        IAD.AK = AccessKind::Write;
        IAD.T = FixedVectorType::get(Int64Ty, 4);
        IAD.Ptr = II->getArgOperand(0);
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_sse2_maskmov_dqu:
        assert(II->arg_size() == 3);
        IAD.AK = AccessKind::Write;
        IAD.T = FixedVectorType::get(Int8Ty, 16);
        IAD.Ptr = II->getArgOperand(2);  // Note: maskmov uses (data, mask, ptr) order
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_mmx_maskmovq:
        assert(II->arg_size() == 3);
        IAD.AK = AccessKind::Write;
        IAD.T = FixedVectorType::get(Int8Ty, 8);
        IAD.Ptr = II->getArgOperand(2);  // Note: maskmov uses (data, mask, ptr) order
        IAD.Mask = II->getArgOperand(1);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_sse3_ldu_dq:
        IAD.AK = AccessKind::Read;
        IAD.T = cast<FixedVectorType>(II->getType());
        IAD.Ptr = II->getArgOperand(0);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_sse_stmxcsr:
        IAD.AK = AccessKind::Write;
        IAD.T = Int32Ty;
        IAD.Ptr = II->getArgOperand(0);
        IAD.Alignment = 1;
        break;
      case Intrinsic::x86_sse_ldmxcsr:
        IAD.AK = AccessKind::Read;
        IAD.T = Int32Ty;
        IAD.Ptr = II->getArgOperand(0);
        IAD.Alignment = 1;
        break;
      default:
        break;
      }
      return IAD;
    }
    return IntrinsicAccessDetails();
  }

  IntrinsicAccessDetails analyzeIntrinsicLoadStore(Instruction* I, AccessKind AK) {
    IntrinsicAccessDetails IAD = analyzeIntrinsicLoadStore(I);
    if (IAD)
      assert(IAD.AK == AK);
    return IAD;
  }

  void buildChecksImpl(Instruction* I, Type* T, Value* HighP, Align Alignment, AtomicOrdering AO,
                       AccessKind AK, std::vector<AccessCheckWithDI>& Checks) {
    if (verbose) {
      errs() << "Building checks for " << *I << " with T = ";
      if (!T)
        errs() << "null";
      else
        errs() << *T;
      errs() << ", HighP = " << *HighP << ", Alignment = " << Alignment.value() << ", AK = "
             << accessKindString(AK) << "\n";
    }

    PtrAndOffset PAO = canonicalizePtr(HighP);
    if (verbose)
      errs() << "PAO = " << *PAO.HighP << ", offset = " << PAO.Offset << "\n";

    IntrinsicAccessDetails IAD = analyzeIntrinsicLoadStore(I, AK);
    if (IAD.Mask) {
      assert(IAD.Ptr == HighP); /* It's possible that this should eventually be turned into an extra
                                   condition in the above `if`, rather than an assert. */
      Checks.push_back(
        AccessCheckWithDI(PAO.HighP, 0, 0, CheckKind::ValidObject, basicDI(I->getDebugLoc())));
      Checks.push_back(
        AccessCheckWithDI(
          PAO.HighP, PositiveModulo(PAO.Offset, Alignment.value()), Alignment.value(),
          CheckKind::Alignment, basicDI(I->getDebugLoc())));
      if (AK == AccessKind::Write) {
        Checks.push_back(
          AccessCheckWithDI(PAO.HighP, 0, 0, CheckKind::CanWrite, basicDI(I->getDebugLoc())));
      }
      return;
    }

    if (isInlineableMemmoveCall(I)) {
      CallBase* CI = cast<CallBase>(I);
      size_t Count = cast<ConstantInt>(CI->getArgOperand(2))->getZExtValue();
      bool CouldHavePtrs = Count >= WordSize;
      if (AK == AccessKind::Read)
        assert(HighP == CI->getArgOperand(1));
      else
        assert(HighP == CI->getArgOperand(0));
      const CombinedDI* DI = basicDI(I->getDebugLoc());
      buildCheck(Count, Alignment.value(), PAO.HighP, PAO.Offset, AK, DI, Checks);
      if (CouldHavePtrs || AK == AccessKind::Write)
        Checks.push_back(AccessCheckWithDI(PAO.HighP, 0, 0, CheckKind::GetAuxPtr, DI));
      return;
    }

    buildChecksRecurse(T, PAO.HighP, PAO.Offset, Alignment.value(), AO, AK, basicDI(I->getDebugLoc()),
                       Checks);
  }

  void buildChecks(Instruction* I, Type* T, Value* HighP, Align Alignment, AtomicOrdering AO,
                   AccessKind AK, std::vector<AccessCheckWithDI>& Checks) {
    buildChecksImpl(I, T, HighP, Alignment, AO, AK, Checks);
    canonicalizeAccessChecks(Checks);
  }

  Value* ptrOperandForAccess(Instruction* I) {
    if (LoadInst* LI = dyn_cast<LoadInst>(I))
      return LI->getPointerOperand();
    
    if (StoreInst* SI = dyn_cast<StoreInst>(I))
      return SI->getPointerOperand();
    
    if (AtomicCmpXchgInst* AI = dyn_cast<AtomicCmpXchgInst>(I))
      return AI->getPointerOperand();
    
    AtomicRMWInst* AI = cast<AtomicRMWInst>(I);
    return AI->getPointerOperand();
  }

  bool isHasUnionFT(FunctionType* FT) {
    return FT->getNumParams() == 1 && !FT->isVarArg() &&
      FT->getReturnType() == VoidTy && FT->getParamType(0) == RawPtrTy;
  }

  bool isMemmoveFT(FunctionType* FT) {
    return FT->getNumParams() == 3 &&
      !FT->isVarArg() &&
      FT->getReturnType() == VoidTy &&
      FT->getParamType(0) == RawPtrTy &&
      FT->getParamType(1) == RawPtrTy &&
      FT->getParamType(2) == IntPtrTy;
  }

  bool isCallToRecognizableMemmove(CallBase* CI) {
    if (Function* F = dyn_cast<Function>(CI->getCalledOperand())) {
      FunctionType* FT = CI->getFunctionType();
      return ((F->isIntrinsic()
               && (F->getIntrinsicID() == Intrinsic::memcpy ||
                   F->getIntrinsicID() == Intrinsic::memcpy_inline ||
                   F->getIntrinsicID() == Intrinsic::memmove)) ||
              ((F->getName() == "zmemmove_union" || F->getName() == "zmemmove_builtin")
               && isMemmoveFT(FT)));
    }
    return false;
  }

  bool isCallToNonescapingMemmove(CallBase* CI) {
    if (!isCallToRecognizableMemmove(CI))
      return false;
    if (IntrinsicInst* II = dyn_cast<IntrinsicInst>(CI)) {
      assert(II->getIntrinsicID() == Intrinsic::memcpy ||
             II->getIntrinsicID() == Intrinsic::memcpy_inline ||
             II->getIntrinsicID() == Intrinsic::memmove);
      return isa<ConstantInt>(CI->getArgOperand(3))
        && cast<ConstantInt>(CI->getArgOperand(3))->isZero();
    }
    return true;
  }

  bool isCallToInlineableMemmove(CallBase* CI) {
    if (isCallToNonescapingMemmove(CI)) {
      if (ConstantInt* C = dyn_cast<ConstantInt>(CI->getArgOperand(2))) {
        if (C->getBitWidth() <= 64) {
          size_t Count = C->getZExtValue();
          // This returns false for zero-length memmoves. We shouldn't see those ever, but if we
          // did then we don't want to optimize them since the optimizations strongly (but subtly)
          // assume that the count is not zero. Also return zero for any count that might
          // overflow.
          return Count && (int32_t)Count >= 0 && (uint32_t)(int32_t)Count == Count;
        }
      }
    }
    return false;
  }

  bool isRecognizableMemmoveCall(Instruction* I) {
    if (CallBase* CI = dyn_cast<CallBase>(I))
      return isCallToRecognizableMemmove(CI);
    return false;
  }

  bool isNonescapingMemmoveCall(Instruction* I) {
    if (CallBase* CI = dyn_cast<CallBase>(I))
      return isCallToNonescapingMemmove(CI);
    return false;
  }

  bool isInlineableMemmoveCall(Instruction* I) {
    if (CallBase* CI = dyn_cast<CallBase>(I))
      return isCallToInlineableMemmove(CI);
    return false;
  }

  bool functionWillReturn(Function* F) {
    return F->willReturn() ||
      F->getName() == "zmemmove_union" ||
      F->getName() == "zmemmove_builtin" ||
      F->getName() == "zhas_union" ||
      F->getName() == "zgc_alloc" ||
      F->getName() == "malloc" ||
      F->getName() == "zgc_aligned_alloc" ||
      F->getName() == "zgc_realloc" ||
      F->getName() == "zgc_aligned_realloc" ||
      F->getName() == "zgc_realloc_preserving_alignment" ||
      F->getName() == "zgc_free" ||
      F->getName() == "free" ||
      F->getName() == "zgc_finq_new" ||
      F->getName() == "zgc_finq_poll" ||
      F->getName() == "zgc_finq_alloc" ||
      F->getName() == "zgc_finq_aligned_alloc" ||
      F->getName() == "zgetlower" ||
      F->getName() == "zgetupper" ||
      F->getName() == "zis_readonly" ||
      F->getName() == "zhasvalidcap" ||
      F->getName() == "zsetcap" ||
      F->getName() == "zptr_to_new_string" ||
      F->getName() == "zptr_contents_to_new_string" ||
      F->getName() == "zptrtable_new" ||
      F->getName() == "zptrtable_encode" ||
      F->getName() == "zptrtable_decode" ||
      F->getName() == "zexact_ptrtable_new" ||
      F->getName() == "zexact_ptrtable_encode" ||
      F->getName() == "zexact_ptrtable_decode" ||
      F->getName() == "zweak_new" ||
      F->getName() == "zweak_get" ||
      F->getName() == "zweak_map_new" ||
      F->getName() == "zweak_map_set" ||
      F->getName() == "zweak_map_get" ||
      F->getName() == "zweak_map_size" ||
      F->getName() == "zweak_map_get_iter" ||
      F->getName() == "zweak_map_iter_next" ||
      F->getName() == "zweak_map_iter_key" ||
      F->getName() == "zweak_map_iter_value" ||
      F->getName() == "zstrlen" ||
      F->getName() == "zisdigit" ||
      F->getName() == "zargs" ||
      F->getName() == "zget_jmp_buf_impl_frame" ||
      F->getName() == "zcallee" ||
      F->getName() == "zclosure_new" ||
      F->getName() == "zclosure_get_data" ||
      F->getName() == "zclosure_set_data" ||
      F->getName() == "zcallee_closure_data" ||
      F->getName() == "zgc_completed_cycle" ||
      F->getName() == "zgc_requested_cycle" ||
      F->getName() == "zgc_try_request" ||
      F->getName() == "zgc_request_fresh" ||
      F->getName() == "zthread_self_id" ||
      F->getName() == "zxgetbv" ||
      F->getName() == "zis_unsafe_signal_for_kill" ||
      F->getName() == "zis_unsafe_signal_for_handlers";
  }

  bool callWillReturn(CallBase* CI) {
    if (Function* F = dyn_cast<Function>(CI->getCalledOperand()))
      return functionWillReturn(F);
    return false;
  }

  bool isPossiblyNonReturningCall(Instruction* I) {
    if (CallBase* CI = dyn_cast<CallBase>(I))
      return !callWillReturn(CI);
    return false;
  }

  bool functionMayFree(Function* F) {
    return !functionWillReturn(F) || 
      F->getName() == "zgc_realloc" ||
      F->getName() == "zgc_aligned_realloc" ||
      F->getName() == "zgc_realloc_preserving_alignment" ||
      F->getName() == "zgc_free" ||
      F->getName() == "free";
  }

  bool callMayFree(CallBase* CI) {
    if (Function* F = dyn_cast<Function>(CI->getCalledOperand()))
      return functionMayFree(F);
    return false;
  }

  bool instructionMayFree(Instruction* I) {
    if (CallBase* CI = dyn_cast<CallBase>(I))
      return callMayFree(CI);
    return false;
  }
  
  template<typename FuncT>
  void forEachCheck(Instruction* I, const FuncT& Func) {
    if (LoadInst* LI = dyn_cast<LoadInst>(I)) {
      Type* T = LI->getType();
      Value* HighP = LI->getPointerOperand();
      Func(LI, T, HighP, LI->getAlign(), LI->getOrdering(), AccessKind::Read);
      return;
    }
    
    if (StoreInst* SI = dyn_cast<StoreInst>(I)) {
      Type* T = SI->getValueOperand()->getType();
      Value* HighP = SI->getPointerOperand();
      Func(SI, T, HighP, SI->getAlign(), SI->getOrdering(), AccessKind::Write);
      return;
    }
    
    if (AtomicCmpXchgInst* AI = dyn_cast<AtomicCmpXchgInst>(I)) {
      assert(AI->getMergedOrdering() != AtomicOrdering::NotAtomic);
      Type* T = AI->getNewValOperand()->getType();
      Value* HighP = AI->getPointerOperand();
      Func(AI, T, HighP, AI->getAlign(), AI->getMergedOrdering(), AccessKind::Write);
      return;
    }
    
    if (AtomicRMWInst* AI = dyn_cast<AtomicRMWInst>(I)) {
      assert(AI->getOrdering() != AtomicOrdering::NotAtomic);
      Type* T = AI->getValOperand()->getType();
      Value* HighP = AI->getPointerOperand();
      Func(AI, T, HighP, AI->getAlign(), AI->getOrdering(), AccessKind::Write);
      return;
    }

    if (isInlineableMemmoveCall(I)) {
      CallBase* CI = cast<CallBase>(I);
      // FIXME: This callback situation is confusing and imperfect.
      // FIXME: We actually know more about the alignment than what we're passing here.
      Func(CI, nullptr, CI->getArgOperand(0), Align(1), AtomicOrdering::NotAtomic,
           AccessKind::Write);
      Func(CI, nullptr, CI->getArgOperand(1), Align(1), AtomicOrdering::NotAtomic,
           AccessKind::Read);
      return;
    }

    if (IntrinsicInst* II = dyn_cast<IntrinsicInst>(I)) {
      switch (II->getIntrinsicID()) {
      case Intrinsic::vastart:
        Func(II, RawPtrTy, II->getArgOperand(0), Align(WordSize), AtomicOrdering::NotAtomic,
             AccessKind::Write);
        return;
      case Intrinsic::vacopy:
        Func(II, RawPtrTy, II->getArgOperand(0), Align(WordSize), AtomicOrdering::NotAtomic,
             AccessKind::Write);
        Func(II, RawPtrTy, II->getArgOperand(1), Align(WordSize), AtomicOrdering::NotAtomic,
             AccessKind::Read);
        return;
      default:
        if (IntrinsicAccessDetails IAD = analyzeIntrinsicLoadStore(I))
          Func(II, IAD.T, IAD.Ptr, Align(IAD.Alignment), AtomicOrdering::NotAtomic, IAD.AK);
        return;
      }
    }

    if (CallBase* CI = dyn_cast<CallBase>(I)) {
      if (Function* F = dyn_cast<Function>(CI->getCalledOperand())) {
        if (isSetjmp(F)) {
          Func(CI, RawPtrTy, CI->getArgOperand(0), Align(WordSize), AtomicOrdering::NotAtomic,
               AccessKind::Write);
          return;
        }
      }
      for (size_t Idx = 0; Idx < CI->arg_size(); ++Idx) {
        if (!CI->isByValArgument(Idx))
          continue;
        Func(CI, CI->getParamByValType(Idx), CI->getArgOperand(Idx), Align(1),
             AtomicOrdering::NotAtomic, AccessKind::Read);
      }
    }
  }

  void buildChecks(Instruction* I, std::vector<AccessCheckWithDI>& Checks) {
    forEachCheck(I, [&] (Instruction* I, Type* T, Value* HighP, Align Alignment, AtomicOrdering AO,
                         AccessKind AK) {
      buildChecks(I, T, HighP, Alignment, AO, AK, Checks);
    });
  }

  template<typename FuncT>
  void forEachCanonicalPtrOperand(Instruction* I, const FuncT& Func) {
    forEachCheck(I, [&] (Instruction*, Type*, Value* HighP, Align, AtomicOrdering, AccessKind) {
      Func(canonicalizePtr(HighP));
    });
  }

  void capturePointerOperands(const std::vector<Instruction*>& Instructions) {
    AuxBaseVarCreationAllowed = true;
    for (Instruction* I : Instructions) {
      auto DealWithPtr = [&] (PtrAndOffset PAO) {
        PtrOperandData POD;
        POD.PAR = underlyingPtr(PAO);
        POD.PK = pointerKindDirect(POD.PAR.P);
        if (POD.PK == PointerKind::Escaping)
          POD.AuxBaseVar = canonicalPtrAuxBaseVar(PAO.HighP);
        else {
          AllocaInst* AI = cast<AllocaInst>(POD.PAR.P);
          assert(LocalAllocaDatas.count(AI));
          POD.LAD = LocalAllocaDatas[AI];
          if (POD.PK == PointerKind::LocalNaked)
            assert(POD.PAR.R == PtrRandomness::NotRandom);
        }
        PtrOperandDatas[I].push_back(POD);
      };

      // The forEachChecks path won't consider non-opt memmoves, but our memmove lowering for stack
      // locals needs us to consider the non-opt ones. YUCK!
      if (isRecognizableMemmoveCall(I)) {
        CallBase* CI = cast<CallBase>(I);
        DealWithPtr(canonicalizePtr(CI->getArgOperand(0)));
        DealWithPtr(canonicalizePtr(CI->getArgOperand(1)));
        continue;
      }
      
      forEachCanonicalPtrOperand(I, DealWithPtr);
    }
    AuxBaseVarCreationAllowed = false;
  }

  const CombinedDI* hashConsDI(const CombinedDI& DI) {
    return &*CombinedDIs.insert(DI).first;
  }

  const CombinedDI* combineDI(const CombinedDI* A, const CombinedDI* B) {
    if (A == B)
      return A;
    if (!A)
      return B;
    if (!B)
      return A;
    const CombinedDI* First = std::min(A, B);
    const CombinedDI* Second = std::max(A, B);
    auto Iter = CombinedDIMaker.find(std::make_pair(First, Second));
    if (Iter != CombinedDIMaker.end())
      return Iter->second;
    CombinedDI NewDI;
    NewDI.Locations.insert(A->Locations.begin(), A->Locations.end());
    NewDI.Locations.insert(B->Locations.begin(), B->Locations.end());
    const CombinedDI* Result = hashConsDI(NewDI);
    CombinedDIMaker[std::make_pair(First, Second)] = Result;
    return Result;
  }

  const CombinedDI* basicDI(DILocation* Loc) {
    auto Iter = BasicDIs.find(Loc);
    if (Iter != BasicDIs.end())
      return Iter->second;
    const CombinedDI* Result = hashConsDI(CombinedDI(Loc));
    BasicDIs[Loc] = Result;
    return Result;
  }

  // Notes about backwards propagation.
  //
  // Some examples of how this works. Here's an example where we haven't split critical edges. Note
  // that this analysis is sound if we don't, but in fact we do (see prepare()). However, it's
  // possible for some edges to be unsplittable in LLVM IR, and we design this pass so that it works
  // soundly (albeit more conservatively) if we ever choose not to split some edges as a matter of
  // policy.
  //
  //     a:
  //         abody
  //         br c
  //     b:
  //         bbody
  //         br c, d
  //     c:
  //         cbody
  //         br e
  //     d:
  //         dbody
  //         br e
  //
  // Say that `cbody` has a check that doesn't appear anywhere else. When propagating it backwards
  // to predecessors (a and b), we merge all of the predecessors attail states together with c'd
  // athead state. This will remove the check, since d didn't have the check.
  //
  // Now consider this criss-cross loop-that-isn't monster.
  //
  //     a:
  //         abody
  //         br c, d
  //     b:
  //         bbody
  //         br c, d
  //     c:
  //         check1
  //         cbody
  //         br c, d, e, f
  //     d:
  //         check2
  //         dbody
  //         br c, d, e, f
  //     e:
  //         ebody
  //         br somewhere
  //     f:
  //         fbody
  //         br somewhere
  //
  // There are three backedges here, c->c, d->d, and d->c (assuming c came before d in the
  // traversal). Also, critical edges aren't split. The backedges mean that we add bonus edges:
  // c->exit, d->exit. The exit block has no checks. This means that check1 and check2 can't get
  // propagated, because both c and d have c and d as predecessors, and c and d have attails that
  // will be forced empty by the exit block.
  //
  // Note that if we did split critical edges here, then check1 and check2 would experience some
  // backward motion.
  //
  // Now consider what happens in a simple loop where we had split critical edges.
  //
  //         header
  //         br loop
  //     reloop:
  //         br loop
  //     loop:
  //         check
  //         effect
  //         br reloop, end
  //     end:
  //         footer
  //
  // The reloop block exists because of critical edge splitting. Note that reloop->loop is a
  // backedge, so we will add a bonus edge reloop->exit. This forces reloop's attail state to be
  // empty and prevents the check from being hoisted. Also, the footer lacks the check anyway, so
  // the loop's attail state will be forced empty, too.
  //
  // FIXME: Looks like we need to run the forward analysis before the backward analysis! Also, it's
  // not obvious that pushing checks backward in a way that duplicates them is a good idea; it seems
  // like it'll cause code bloat. Maybe we should avoid pushing them back at all unless it leads to
  // a reduction of work.
  //
  // Let's consider what'll happen in the simple loop if we do forward propagation first. The
  // forward propagation will put the check in the attail of loop and reloop (minus the NotFree
  // part of the check). Then, I guess, we say that the forward attail overrides the backward
  // attail, or that the backward attail of a block always includes everything from the forward
  // attail (the forward part cannot be removed, somehow). So when we backprop the check from loop,
  // the merge of predecessor attails will include the check, so the check will end up in the
  // header. But, concretely, what does the equation look like?
  //
  // Say that forward propagation produces ForwardChecksAtTail and backward propagation is solving
  // for BackwardChecksAtTail. We need to answer three questions: how does the backwards execution
  // of a block start, how does the backwards execution of a block end, and how do checks get
  // scheduled?
  //
  // How to start backward block execution: the running Checks are bootstrapped from
  // BackwardChecksAtTail - ForwardChecksAtTail.
  //
  // How to end backward block execution: for each predecessor, compute the combined ChecksAtTail by
  // adding BackwardChecksAtTail to ForwardChecksAtTail. Merge the running Checks with each
  // ChecksAtTail. Then merge the running Checks into each predecessor's BackwardChecksAtTail.
  //
  // How checks are scheduled (assuming we ended backward execution in a block): for each
  // predecessor, compute the combined ChecksAtTail by adding BackwardChecksAtTail to
  // ForwardChecksAtTail. Save the running Checks as ExpectedChecks. Merge the running Checks with
  // each ChecksAtTail. Now, ExpectedChecks - Checks is the set of checks that needs to be executed
  // at the block's head.
  //
  // Alternative formulations:
  //
  // - Could alternatively say that backwards execution starts with BackwardChecksAtTail, without
  //   the subtraction. Note that in the loop case, the loop and reloop blocks will both have empty
  //   BackwardChecksAtTail anyway. But, I don't think that's right in general, since we want to
  //   backwards propagate the minimal set of checks on the grounds that we want a minimal schedule.
  //
  // - Could have end of backward execution subtract ForwardChecksAtTail from BackwardChecksAtTail
  //   after the merging of Checks into BackwardChecksAtTail.

  void updateLastDI(AccessCheck&, const AccessCheck&) {
  }

  void updateLastDI(AccessCheckWithDI& LastACRef, const AccessCheckWithDI& NewAC) {
    if (LastACRef == NewAC)
      LastACRef.DI = NewAC.DI;
  }

  // Removes check combinations that are not useful. In particular, removes lower/upper bounds checks
  // where the lower bounds is higher than the lower bounds.
  //
  // Note that this is not optional since emitChecks asserts that there are no upper bounds checks
  // with lower offset than the lower bounds check.
  void removeUnprofitableChecks(std::vector<AccessCheckWithDI>& Checks) {
    std::vector<AccessCheckWithDI> NewChecks;
    for (size_t Index = 0; Index < Checks.size();) {
      Value* CanonicalPtr = Checks[Index].CanonicalPtr;
      size_t BeginIndex = Index;
      size_t EndIndex;
      int64_t LowerBoundOffset = 0;
      bool HasLowerBound = false;
      int64_t UpperBoundOffset = 0;
      bool HasUpperBound = false;
      for (EndIndex = BeginIndex;
           EndIndex < Checks.size() && Checks[EndIndex].CanonicalPtr == CanonicalPtr;
           ++EndIndex) {
        AccessCheckWithDI AC = Checks[EndIndex];
        switch (AC.CK) {
        case CheckKind::KnownLowerBound:
        case CheckKind::LowerBound:
          LowerBoundOffset = AC.Offset;
          HasLowerBound = true;
          break;
        case CheckKind::UpperBound:
          UpperBoundOffset = AC.Offset;
          HasUpperBound = true;
          break;
        default:
          break;
        }
      }
      if (HasUpperBound) {
        // This should have been guaranteed for other reasons.
        assert(HasLowerBound);
      }

      // Keep range checks if we have both lower and upper bound checks, and the upper bounds is above
      // the lower bounds.
      bool RangeCheckOK = HasUpperBound && HasLowerBound && UpperBoundOffset > LowerBoundOffset;

      for (EndIndex = BeginIndex;
           EndIndex < Checks.size() && Checks[EndIndex].CanonicalPtr == CanonicalPtr;
           ++EndIndex) {
        AccessCheckWithDI AC = Checks[EndIndex];
        switch (AC.CK) {
        case CheckKind::KnownLowerBound:
        case CheckKind::LowerBound:
        case CheckKind::UpperBound:
          if (RangeCheckOK)
            NewChecks.push_back(AC);
          break;
        default:
          NewChecks.push_back(AC);
          break;
        }
      }

      Index = EndIndex;
    }
    Checks = NewChecks;
  }

  // Produces a minimal set of access checks in an order that is suitable for executing them. Assumes
  // that given two equal checks, the DI of the latest one wins. This has no effect on forward prop,
  // since forward prop uses AccessCheck, not AccessCheckWithDI. For backward prop, it means that the
  // checks that dominate win the DI battle.
  template<typename CheckT>
  void canonicalizeAccessChecks(std::vector<CheckT>& Checks) {
    std::stable_sort(Checks.begin(), Checks.end());
  
    size_t DstIndex = 0;
    size_t SrcIndex = 0;
    CheckT LastAC;
    Value* CanonicalPtr = nullptr;
    int64_t Alignment = 0;
    int64_t AlignmentOffset = 0;
    bool AlignmentContradiction = false;
    while (SrcIndex < Checks.size()) {
      CheckT AC = Checks[SrcIndex++];
      if (AC.CanonicalPtr == LastAC.CanonicalPtr &&
          FundamentalCheckKind(AC.CK) == FundamentalCheckKind(LastAC.CK)) {
        switch (AC.CK) {
        case CheckKind::ValidObject:
        case CheckKind::CanWrite:
        case CheckKind::NotFree:
        case CheckKind::GetAuxPtr:
        case CheckKind::EnsureAuxPtr:
          // For these, we really only need one of these for each CanonicalPtr.
          updateLastDI(Checks[DstIndex - 1], AC);
          continue;
          
        case CheckKind::KnownLowerBound:
        case CheckKind::LowerBound:
        case CheckKind::UpperBound:
          // For these, the sort order is such that only the first one in the list matters.
          // FIXME: Need to assert that if there's an UpperBound, then we need a LowerBound.
          // And, that:
          // - The lower bound is below-equal to the type check offsets.
          // - If we have an upper bound, it's above-equal to the type check offsets.
          // And, these checks have to happen outside this switch, since this switch is for the case
          // where we have a check redundancy. Maybe that means having the check happen in some other
          // function?
          updateLastDI(Checks[DstIndex - 1], AC);
          continue;
  
        case CheckKind::KnownAlignment:
        case CheckKind::Alignment:
          assert(Alignment);
          assert(AlignmentOffset < Alignment);
          assert(AC.Size <= LastAC.Size);
          assert(AC.Size <= Alignment);
          if ((AlignmentOffset % AC.Size) == AC.Offset) {
            if (AC.Size == Alignment)
              updateLastDI(Checks[DstIndex - 1], AC);
            continue;
          }
          AlignmentContradiction = true;
          break;
        }
      }
      Checks[DstIndex++] = AC;
      LastAC = AC;
      if (AC.CanonicalPtr != CanonicalPtr) {
        CanonicalPtr = AC.CanonicalPtr;
        Alignment = 0;
        AlignmentOffset = 0;
        AlignmentContradiction = false;
      }
      if (FundamentalCheckKind(AC.CK) == CheckKind::Alignment && !AlignmentContradiction) {
        Alignment = AC.Size;
        AlignmentOffset = AC.Offset;
      }
    }
  
    Checks.resize(DstIndex);

    checkCanonicalizedAccessChecks(Checks);
  }
  
  // Given two lists of checks, produces the set of checks that is the superset of the two. This is the
  // opposite of merging.
  template<typename ToCheckT, typename FromCheckT>
  void addAccessChecks(std::vector<ToCheckT>& ToChecks,
                       const std::vector<FromCheckT>& FromChecks) {
    ToChecks.insert(ToChecks.end(), FromChecks.begin(), FromChecks.end());
    canonicalizeAccessChecks(ToChecks);
  }

  void mergeCheckDI(AccessCheck&, const AccessCheck&, const AccessCheck&) {
  }
  
  void mergeCheckDI(AccessCheckWithDI& NewAC,
                    const AccessCheckWithDI& AC1, const AccessCheckWithDI& AC2) {
    NewAC.DI = combineDI(AC1.DI, AC2.DI);
  }
  
  // Given two lists of canonicalized access checks, merges the second one into the first one. The
  // outcome may shrink ToChecks, or make some checks in ToChecks weaker. It will never grow ToChecks.
  //
  // Returns true if ToChecks is changed.
  template<typename CheckT>
  bool mergeAccessChecks(std::vector<CheckT>& ToChecks,
                         const std::vector<CheckT>& FromChecks,
                         AIDirection Direction) {
    size_t DstToIndex = 0;
    size_t SrcToIndex = 0;
    size_t FromIndex = 0;
    bool Result = false;
    Value* LastLowerBoundPtr = nullptr;
    while (SrcToIndex < ToChecks.size() && FromIndex < FromChecks.size()) {
      CheckT ToAC = ToChecks[SrcToIndex];
      CheckT FromAC = FromChecks[FromIndex];
  
      assert(!IsKnownCheckKind(ToAC.CK));
      assert(!IsKnownCheckKind(FromAC.CK));
      
      if (ToAC.CanonicalPtr == FromAC.CanonicalPtr &&
          FundamentalCheckKind(ToAC.CK) == FundamentalCheckKind(FromAC.CK)) {

        auto mergeToAC = [&] () {
          CheckT NewAC = ToAC;
          mergeCheckDI(NewAC, ToAC, FromAC);
          ToChecks[DstToIndex++] = NewAC;
          SrcToIndex++;
          FromIndex++;
        };
        
        auto mergeMaxAC = [&] () {
          CheckT NewAC = std::max(ToAC, FromAC);
          mergeCheckDI(NewAC, ToAC, FromAC);
          Result |= NewAC != ToAC;
          ToChecks[DstToIndex++] = NewAC;
          SrcToIndex++;
          FromIndex++;
        };
        
        switch (ToAC.CK) {
        case CheckKind::ValidObject:
        case CheckKind::CanWrite:
        case CheckKind::NotFree:
        case CheckKind::GetAuxPtr:
        case CheckKind::EnsureAuxPtr: {
          mergeToAC();
          continue;
        }
        case CheckKind::LowerBound:
        case CheckKind::UpperBound: {
          switch (Direction) {
          case AIDirection::Forward: {
            mergeMaxAC();
            continue;
          }
          case AIDirection::Backward: {
            // If we kept the lower bound check, then we can keep the upper bound check. We don't
            // keep either check if they're not exactly the same.
            if (ToAC.Offset != FromAC.Offset)
              break;
            if (ToAC.CK == CheckKind::LowerBound)
              LastLowerBoundPtr = ToAC.CanonicalPtr;
            else {
              assert(ToAC.CK == CheckKind::UpperBound);
              if (ToAC.CanonicalPtr != LastLowerBoundPtr)
                break;
            }
            mergeToAC();
            continue;
          } }
          break;
        }
        case CheckKind::Alignment: {
          switch (Direction) {
          case AIDirection::Forward: {
            int64_t Size = std::min(ToAC.Size, FromAC.Size);
            if ((ToAC.Offset % Size) == (FromAC.Offset % Size)) {
              CheckT NewAC = ToAC;
              NewAC.Size = Size;
              NewAC.Offset = NewAC.Offset % Size;
              mergeCheckDI(NewAC, ToAC, FromAC);
              Result |= NewAC != ToAC;
              ToChecks[DstToIndex++] = NewAC;
              SrcToIndex++;
              FromIndex++;
              continue;
            }
            break;
          }
          case AIDirection::Backward: {
            if (ToAC.Size == FromAC.Size &&
                ToAC.Offset == FromAC.Offset) {
              mergeToAC();
              continue;
            }
            break;
          } }
          break;
        }
        case CheckKind::KnownAlignment:
        case CheckKind::KnownLowerBound:
          llvm_unreachable("Should never see Known kinds in merging");
          break;
        }
      }
  
      if (ToAC < FromAC)
        SrcToIndex++;
      else
        FromIndex++;
    }
  
    Result |= DstToIndex != ToChecks.size();
    ToChecks.resize(DstToIndex);
  
    return Result;
  }

  // Removes checks in ToChecks that are subsumed by FromChecks. Conservatively keeps ToChecks.
  template<typename ToCheckT, typename FromCheckT>
  void subtractChecks(std::vector<ToCheckT>& ToChecks,
                      const std::vector<FromCheckT>& FromChecks,
                      bool CreateKnowns = false) {
    size_t DstToIndex = 0;
    size_t SrcToIndex = 0;
    size_t FromIndex = 0;
    while (SrcToIndex < ToChecks.size() && FromIndex < FromChecks.size()) {
      ToCheckT ToAC = ToChecks[SrcToIndex];
      FromCheckT FromAC = FromChecks[FromIndex];
  
      assert(!IsKnownCheckKind(ToAC.CK));
      assert(!IsKnownCheckKind(FromAC.CK));
      
      if (ToAC.CanonicalPtr == FromAC.CanonicalPtr &&
          FundamentalCheckKind(ToAC.CK) == FundamentalCheckKind(FromAC.CK)) {
        switch (ToAC.CK) {
        case CheckKind::ValidObject:
        case CheckKind::CanWrite:
        case CheckKind::NotFree:
        case CheckKind::GetAuxPtr:
        case CheckKind::EnsureAuxPtr:
          SrcToIndex++;
          FromIndex++;
          continue;
  
        case CheckKind::LowerBound:
        case CheckKind::UpperBound:
          if (ToAC < FromAC)
            ToChecks[DstToIndex++] = ToAC;
          else if (ToAC.CK == CheckKind::LowerBound) {
            if (CreateKnowns) {
              // We can keep the ToAC LowerBound and make it Known, since there's nothing to be gained
              // from knowing that we proved a more aggressive lower bound.
              ToAC.CK = CheckKind::KnownLowerBound;
            }

            // If we're not creating knowns, then keep the lower bound as-is.
            ToChecks[DstToIndex++] = ToAC;
          }
          SrcToIndex++;
          FromIndex++;
          continue;
  
        case CheckKind::Alignment:
          if (CreateKnowns && FromAC.Size >= ToAC.Size
              && (FromAC.Offset % ToAC.Size) == ToAC.Offset) {
            FromAC.CK = CheckKind::KnownAlignment;
            ToChecks[DstToIndex++] = FromAC;
          } else
            ToChecks[DstToIndex++] = ToAC;
          SrcToIndex++;
          FromIndex++;
          continue;
          
        case CheckKind::KnownAlignment:
        case CheckKind::KnownLowerBound:
          llvm_unreachable("Should never see Known kinds in merging");
          break;
        }
      }
  
      if (ToAC < FromAC) {
        ToChecks[DstToIndex++] = ToAC;
        SrcToIndex++;
      } else
        FromIndex++;
    }

    while (SrcToIndex < ToChecks.size())
      ToChecks[DstToIndex++] = ToChecks[SrcToIndex++];
  
    ToChecks.resize(DstToIndex);
    checkCanonicalizedAccessChecks(ToChecks);
  }

  template<typename ToCheckT, typename FromCheckT>
  void subtractChecksCreatingKnowns(std::vector<ToCheckT>& ToChecks,
                                    const std::vector<FromCheckT>& FromChecks) {
    subtractChecks(ToChecks, FromChecks, /*CreateKnowns=*/true);
  }
  
  void removeRedundantChecksUsingForwardAI(
    const std::vector<BasicBlock*>& Blocks,
    const std::unordered_set<const BasicBlock*>& BackEdgePreds,
    const std::unordered_map<BasicBlock*, std::unordered_set<Value*>> CanonicalPtrLiveAtTail,
    std::unordered_map<BasicBlock*, ChecksOrBottom>& ForwardChecksAtHead,
    std::unordered_map<BasicBlock*, std::vector<AccessCheck>>& ForwardChecksAtTail) {

    ForwardChecksAtHead.clear();
    ForwardChecksAtTail.clear();

    ForwardChecksAtHead[&NewF->getEntryBlock()].Bottom = false;

    bool Changed = true;
    while (Changed) {
      Changed = false;
      for (BasicBlock* BB : Blocks) {
        if (verbose)
          errs() << "Forward propagating in " << BB->getName() << "\n";
        
        const ChecksOrBottom& COB = ForwardChecksAtHead[BB];
        if (COB.Bottom)
          continue;

        std::vector<AccessCheck> Checks = COB.Checks;
        if (verbose)
          errs() << "Checks at head: " << Checks << "\n";

        for (Instruction& I : *BB) {
          auto HandleEffects = [&] () {
            EraseIf(Checks, [&] (const AccessCheck& AC) -> bool {
              return AC.CK == CheckKind::NotFree || AC.CK == CheckKind::GetAuxPtr;
            });
          };

          if (&I == BB->getTerminator() && BackEdgePreds.count(BB)) {
            // Execute the pollcheck.
            HandleEffects();
          }

          auto Iter = ChecksForInst.find(&I);
          if (Iter != ChecksForInst.end())
            Checks.insert(Checks.end(), Iter->second.begin(), Iter->second.end());
          
          // For now, just conservatively assume that all calls may free stuff.
          // FIXME: Could be a bit more precise here for memmoves, but it's probably not worth it
          if (isa<CallBase>(&I))
            HandleEffects();
          else if (isa<StoreInst>(&I) || isa<AtomicCmpXchgInst>(&I) || isa<AtomicRMWInst>(&I)) {
            Value* Ptr = canonicalizePtr(ptrOperandForAccess(&I)).HighP;
            EraseIf(Checks, [&] (const AccessCheck& AC) -> bool {
              return AC.CK == CheckKind::GetAuxPtr && AC.CanonicalPtr != Ptr;
            });
          }

          if (verbose)
            errs() << "Checks after " << I << ":\n    " << Checks << "\n";
        }

        auto Iter = CanonicalPtrLiveAtTail.find(BB);
        assert(Iter != CanonicalPtrLiveAtTail.end());
        const std::unordered_set<Value*>& Live = Iter->second;
        EraseIf(Checks, [&] (const AccessCheck& AC) -> bool {
          assert(AC.CanonicalPtr);
          return !Live.count(AC.CanonicalPtr);
        });
        canonicalizeAccessChecks(Checks);
        if (verbose)
          errs() << "Liveness-pruned and canonicalized checks at tail: " << Checks << "\n";
        ForwardChecksAtTail[BB] = Checks;

        for (BasicBlock* SBB : successors(BB)) {
          ChecksOrBottom& SCOB = ForwardChecksAtHead[SBB];
          if (SCOB.Bottom) {
            SCOB.Bottom = false;
            SCOB.Checks = Checks;
            Changed = true;
          } else
            Changed |= mergeAccessChecks(SCOB.Checks, Checks, AIDirection::Forward);
        }
      }
    }

    // We have to pick a check schedule at this point. If we don't then we'll forget what we learned
    // from forward propagation.

    std::unordered_map<Instruction*, std::vector<AccessCheckWithDI>> NewChecksForInst;
    
    for (BasicBlock* BB : Blocks) {
      if (verbose)
        errs() << "Optimizing " << BB->getName() << " using forward propagation results.\n";
      const ChecksOrBottom& StateWithBottom = ForwardChecksAtHead[BB];
      
      // It's weird, but possible, that we have an unreachable block.
      if (StateWithBottom.Bottom)
        continue;
      
      std::vector<AccessCheck> Checks = StateWithBottom.Checks;
      bool NeedToCanonicalize = false;

      if (verbose)
        errs() << "Checks at head: " << Checks << "\n";
      
      for (Instruction& I : *BB) {
        auto HandleEffects = [&] () {
          EraseIf(Checks, [&] (const AccessCheck& AC) -> bool {
            return AC.CK == CheckKind::NotFree || AC.CK == CheckKind::GetAuxPtr;
          });
        };
        
        if (&I == BB->getTerminator() && BackEdgePreds.count(BB)) {
          // Execute the pollcheck.
          HandleEffects();
        }

        auto Iter = ChecksForInst.find(&I);
        if (Iter != ChecksForInst.end()) {
          std::vector<AccessCheckWithDI> NewChecks = Iter->second;
          if (verbose) {
            errs() << "Dealing with previously scheduled checks at " << I << ":\n    "
                   << NewChecks << "\n";
          }
          if (NeedToCanonicalize) {
            canonicalizeAccessChecks(Checks);
            NeedToCanonicalize = false;
          }
          subtractChecksCreatingKnowns(NewChecks, Checks);
          if (!NewChecks.empty()) {
            assert(!NewChecksForInst.count(&I));
            if (verbose)
              errs() << "Checks scheduled at " << I << ":\n    " << NewChecks << "\n";
            NewChecksForInst[&I] = NewChecks;
            EraseIf(NewChecks, [&] (const AccessCheck& AC) -> bool {
              return IsKnownCheckKind(AC.CK);
            });
            if (!NewChecks.empty()) {
              Checks.insert(Checks.end(), NewChecks.begin(), NewChecks.end());
              NeedToCanonicalize = true;
            }
          }
        }
        
        // For now, just conservatively assume that all calls may free stuff.
        if (isa<CallBase>(&I))
          HandleEffects();
        else if (isa<StoreInst>(&I) || isa<AtomicCmpXchgInst>(&I) || isa<AtomicRMWInst>(&I)) {
          Value* Ptr = canonicalizePtr(ptrOperandForAccess(&I)).HighP;
          EraseIf(Checks, [&] (const AccessCheck& AC) -> bool {
            return AC.CK == CheckKind::GetAuxPtr && AC.CanonicalPtr != Ptr;
          });
        }
        
        if (verbose)
          errs() << "Checks after " << I << ":\n    " << Checks << "\n";
      }
    }

    ChecksForInst = std::move(NewChecksForInst);
  }
  
  void scheduleChecks(
    const std::vector<BasicBlock*>& Blocks,
    const std::unordered_set<const BasicBlock*>& BackEdgePreds) {

    if (verbose)
      errs() << "Scheduling checks for " << OldF->getName() << "\n";

    ChecksForInst.clear();
    
    for (BasicBlock* BB : Blocks) {
      for (Instruction& I : *BB) {
        std::vector<AccessCheckWithDI> Checks;
        buildChecks(&I, Checks);
        if (!Checks.empty()) {
          assert(!ChecksForInst.count(&I));
          if (verbose)
            errs() << "Checks for " << I << ":\n    " << Checks << "\n";
          ChecksForInst[&I] = std::move(Checks);
        }
      }
    }
    
    if (!optimizeChecks) {
      if (verbose)
        errs() << "Not optimizing the check schedule.\n";
      return;
    }
    
    bool Changed;
    
    // Liveness of canonical ptrs.
    //
    // We need this so that we can GC the abstract state. Without this, the abstract state is likely
    // to get very large, causing memory usage issues and long running times.
    
    std::unordered_map<BasicBlock*, std::unordered_set<Value*>> CanonicalPtrLiveAtTail;
    Changed = true;
    while (Changed) {
      Changed = false;
      for (size_t BlockIndex = Blocks.size(); BlockIndex--;) {
        BasicBlock* BB = Blocks[BlockIndex];
        std::unordered_set<Value*> Live = CanonicalPtrLiveAtTail[BB];

        for (auto It = BB->rbegin(); It != BB->rend(); ++It) {
          Instruction* I = &*It;
          Live.erase(I);
          forEachCanonicalPtrOperand(I, [&] (PtrAndOffset PAO) {
            Value* P = PAO.HighP;
            assert(isa<Instruction>(P) || isa<Argument>(P) || isa<Constant>(P));
            Live.insert(P);
          });
        }

        if (!BlockIndex) {
          for (Value* P : Live)
            assert(isa<Argument>(P) || isa<Constant>(P));
        }

        for (BasicBlock* PBB : predecessors(BB)) {
          for (Value* P : Live)
            Changed |= CanonicalPtrLiveAtTail[PBB].insert(P).second;
        }
      }
    }

    if (verbose)
      errs() << "Liveness done, doing forward propagation.\n";
    
    // Forward propagation
    //
    // This eliminates redundant checks and also shows us which checks are definitely performed along
    // which paths, which aids in making good choices during backward propagation.
    
    std::unordered_map<BasicBlock*, ChecksOrBottom> ForwardChecksAtHead;
    std::unordered_map<BasicBlock*, std::vector<AccessCheck>> ForwardChecksAtTail;

    removeRedundantChecksUsingForwardAI(
      Blocks, BackEdgePreds, CanonicalPtrLiveAtTail, ForwardChecksAtHead, ForwardChecksAtTail);

    if (!propagateChecksBackward) {
      if (verbose)
        errs() << "Not doing backwards propagation.\n";
      return;
    }

    if (verbose)
      errs() << "Forward propagation done, proceeding to backward propagation.\n";

    // For the purpose of backward propagation, we revert known checks to unknown.
    for (auto& Pair : ChecksForInst) {
      for (AccessCheck& AC : Pair.second)
        AC.CK = FundamentalCheckKind(AC.CK);
    }
    
    std::unordered_map<BasicBlock*, ChecksWithDIOrBottom> BackwardChecksAtTail;
    std::unordered_map<BasicBlock*, std::vector<AccessCheckWithDI>> BackwardChecksAtHead;

    if (verbose) {
      for (const BasicBlock* BB : BackEdgePreds)
        errs() << "Backwards edge predecessor: " << BB->getName() << "\n";
    }

    for (BasicBlock* BB : Blocks) {
      // The following cases mean that the block cannot have any checks hoisted into the tail of it:
      //
      // - It's a block that backward-branches.
      // - The terminator is a call.
      //
      // And if the block's terminator represents an exit, then we need to bootstrap it with no
      // checks.
      if (BackEdgePreds.count(BB) ||
          isa<ReturnInst>(BB->getTerminator()) ||
          isa<ResumeInst>(BB->getTerminator()) ||
          isa<UnreachableInst>(BB->getTerminator()) ||
          isa<CallBase>(BB->getTerminator())) {
        if (verbose)
          errs() << "Labeling " << BB->getName() << " as being a non-bottom.\n";
        BackwardChecksAtTail[BB].Bottom = false;
      }
    }

    Changed = true;
    while (Changed) {
      Changed = false;
      for (size_t BlockIndex = Blocks.size(); BlockIndex--;) {
        BasicBlock* BB = Blocks[BlockIndex];

        if (verbose)
          errs() << "Backwards propagation at " << BB->getName() << "\n";
        
        ChecksWithDIOrBottom& COB = BackwardChecksAtTail[BB];
        if (COB.Bottom)
          continue;

        std::vector<AccessCheckWithDI> Checks = COB.Checks;

        if (verbose)
          errs() << "Starting with checks: " << Checks << "\n";
        
        subtractChecks(Checks, ForwardChecksAtTail[BB]);

        if (verbose)
          errs() << "Checks after forward-pruning and removing unprofitable: " << Checks << "\n";

        for (auto It = BB->rbegin(); It != BB->rend(); ++It) {
          Instruction* I = &*It;

          EraseIf(Checks, [&] (const AccessCheck& AC) -> bool {
            return AC.CanonicalPtr == I;
          });

          if (isPossiblyNonReturningCall(I)) {
            // Conservatively assume that the call might not return!
            // FIXME: Don't do this if we know that the call definitely returns (like memmoves).
            Checks.clear();
          } else if (instructionMayFree(I)) {
            EraseIf(Checks, [&] (const AccessCheck& AC) -> bool {
              return AC.CK == CheckKind::NotFree;
            });
          }

          auto Iter = ChecksForInst.find(I);
          if (Iter != ChecksForInst.end()) {
            std::vector<AccessCheckWithDI> ChecksForThisInst = Iter->second;
            EraseIf(ChecksForThisInst, [&] (const AccessCheckWithDI& AC) ->bool {
              return AC.CK == CheckKind::GetAuxPtr || AC.CK == CheckKind::EnsureAuxPtr;
            });
            Checks.insert(Checks.end(), ChecksForThisInst.begin(), ChecksForThisInst.end());
          }

          if (I == BB->getTerminator() && BackEdgePreds.count(BB)) {
            EraseIf(Checks, [&] (const AccessCheck& AC) -> bool {
              return AC.CK == CheckKind::NotFree;
            });
          }

          if (verbose)
            errs() << "Checks after " << *I << ":\n    " << Checks << "\n";
        }
        
        if (pred_empty(BB))
          Checks.clear();
        else {
          canonicalizeAccessChecks(Checks);
          for (BasicBlock* PBB : predecessors(BB)) {
            ChecksWithDIOrBottom& PCOB = BackwardChecksAtTail[PBB];
            if (PCOB.Bottom)
              continue;
            if (verbose)
              errs() << "Considering non-bottom predecessor " << PBB->getName() << "\n";
            std::vector<AccessCheckWithDI> ChecksAtTail = PCOB.Checks;
            if (verbose)
              errs() << "    Backwards checks at tail: " << ChecksAtTail << "\n";
            // Avoid having the merged Checks include all of the DI's if the predecessors' merged
            // Checks.
            for (AccessCheckWithDI& AC : ChecksAtTail)
              AC.DI = nullptr;
            if (verbose)
              errs() << "    Forward checks at tail: " << ForwardChecksAtTail[PBB] << "\n";
            addAccessChecks(ChecksAtTail, ForwardChecksAtTail[PBB]);
            if (verbose)
              errs() << "    Combined checks at tail: " << ChecksAtTail << "\n";
            mergeAccessChecks(Checks, ChecksAtTail, AIDirection::Forward);
            if (verbose)
              errs() << "    Merged checks: " << Checks << "\n";
          }
        }
        if (verbose)
          errs() << "Checks at head after merging with predecessors: " << Checks << "\n";
        removeUnprofitableChecks(Checks);
        if (verbose)
          errs() << "Checks at head after removing unprofitable: " << Checks << "\n";
        BackwardChecksAtHead[BB] = Checks;
        for (BasicBlock* PBB : predecessors(BB)) {
          ChecksWithDIOrBottom& PCOB = BackwardChecksAtTail[PBB];
          if (PCOB.Bottom) {
            PCOB.Bottom = false;
            PCOB.Checks = Checks;
            Changed = true;
          } else
            Changed |= mergeAccessChecks(PCOB.Checks, Checks, AIDirection::Backward);
        }
      }
    }

    std::unordered_map<Instruction*, std::vector<AccessCheckWithDI>> NewChecksForInst;
    
    for (BasicBlock* BB : Blocks) {
      if (verbose) {
        errs() << "Scheduling checks in " << BB->getName()
               << " using results of backward propagation\n";
      }
      std::vector<AccessCheckWithDI> Checks = BackwardChecksAtTail[BB].Checks;
      if (verbose)
        errs() << "Starting with checks: " << Checks << "\n";
      subtractChecks(Checks, ForwardChecksAtTail[BB]);
      if (verbose)
        errs() << "Checks after forward-pruning and removing unprofitable: " << Checks << "\n";

      Instruction* LastI = nullptr;
      std::vector<AccessCheckWithDI> ChecksToEmit;
      bool SawPhi = false;
      for (auto It = BB->rbegin(); It != BB->rend(); ++It) {
        Instruction* I = &*It;

        if (isPossiblyNonReturningCall(I)) {
          // Conservatively assume that the call might not return!
          ChecksToEmit.insert(ChecksToEmit.end(), Checks.begin(), Checks.end());
          Checks.clear();
        } else {
          if (instructionMayFree(I)) {
            EraseIf(Checks, [&] (const AccessCheckWithDI& AC) -> bool {
              if (AC.CK == CheckKind::NotFree) {
                ChecksToEmit.push_back(AC);
                return true;
              }
              return false;
            });
          }
          EraseIf(Checks, [&] (const AccessCheckWithDI& AC) -> bool {
            if (AC.CanonicalPtr == I) {
              ChecksToEmit.push_back(AC);
              return true;
            }
            return false;
          });
        }
        
        auto Iter = ChecksForInst.find(I);
        if (Iter != ChecksForInst.end()) {
          std::vector<AccessCheckWithDI> ChecksForThisInst = Iter->second;
          EraseIf(ChecksForThisInst, [&] (const AccessCheckWithDI& AC) -> bool {
            return AC.CK == CheckKind::GetAuxPtr || AC.CK == CheckKind::EnsureAuxPtr;
          });
          Checks.insert(Checks.end(), ChecksForThisInst.begin(), ChecksForThisInst.end());
        }

        if (isa<PHINode>(I))
          SawPhi = true;
        if (SawPhi)
          assert(isa<PHINode>(I));
        else {
          assert(!isa<PHINode>(I));
          if (!ChecksToEmit.empty()) {
            assert(LastI);
            assert(!NewChecksForInst.count(LastI));
            canonicalizeAccessChecks(ChecksToEmit);
            if (verbose)
              errs() << "Checks scheduled at " << *LastI << ":\n    " << ChecksToEmit << "\n";
            NewChecksForInst[LastI] = std::move(ChecksToEmit);
          }
          LastI = I;
        }

        if (I == BB->getTerminator() && BackEdgePreds.count(BB)) {
          EraseIf(Checks, [&] (const AccessCheckWithDI& AC) -> bool {
            if (AC.CK == CheckKind::NotFree) {
              // FIXME: We should be able to put these checks into the slow path of the pollcheck!
              ChecksToEmit.push_back(AC);
              return true;
            }
            return false;
          });
        }
        
        if (verbose)
          errs() << "Checks after " << *I << ":\n    " << Checks << "\n";
      }

      assert(LastI);
      if (LastI == BB->getTerminator() && BackEdgePreds.count(BB)) {
        assert(ChecksToEmit.empty());
        assert(Checks.empty());
      }
      canonicalizeAccessChecks(Checks);
      subtractChecks(Checks, BackwardChecksAtHead[BB]);
      ChecksToEmit.insert(ChecksToEmit.end(), Checks.begin(), Checks.end());
      assert(!NewChecksForInst.count(LastI));
      canonicalizeAccessChecks(ChecksToEmit);
      if (!ChecksToEmit.empty()) {
        if (verbose)
          errs() << "Checks scheduled at " << *LastI << ":\n    " << ChecksToEmit << "\n";
        NewChecksForInst[LastI] = std::move(ChecksToEmit);
      }
    }

    for (auto& Pair : ChecksForInst) {
      std::vector<AccessCheckWithDI>* Checks = nullptr;
      for (const AccessCheckWithDI& AC : Pair.second) {
        if (AC.CK == CheckKind::GetAuxPtr || AC.CK == CheckKind::EnsureAuxPtr) {
          if (!Checks)
            Checks = &NewChecksForInst[Pair.first];
          Checks->push_back(AC);
        }
      }
      if (Checks)
        canonicalizeAccessChecks(*Checks);
    }
    ChecksForInst = std::move(NewChecksForInst);

    if (verbose)
      errs() << "Backwards propagation done, doing another round of forward propagation.\n";

    // Now we need to remove redundant checks again. Note that this primarily solves the problem of
    // identifying cases of known alignment and known lower bound.

    removeRedundantChecksUsingForwardAI(
      Blocks, BackEdgePreds, CanonicalPtrLiveAtTail, ForwardChecksAtHead, ForwardChecksAtTail);
  }
  
  template<typename FuncTy>
  size_t iterateArgs(const std::vector<ArgInfo>& Elements,
                     const FuncTy& Func) {
    size_t Offset = 0;
    for (size_t Idx = 0; Idx < Elements.size(); ++Idx) {
      Type* T = Elements[Idx].T;
      size_t Alignment = std::max(WordSize, DL.getABITypeAlign(T).value());
      Offset = (Offset + Alignment - 1) & -Alignment;
      Func(Idx, Elements[Idx], Offset);
      Offset += DL.getTypeAllocSize(T);
    }
    return (Offset + WordSize - 1) & -WordSize;
  }

  size_t argsSize(const std::vector<ArgInfo>& Elements) {
    return iterateArgs(Elements, [] (size_t, ArgInfo, size_t) { });
  }

  size_t argsAlignment(const std::vector<ArgInfo>& Elements) {
    size_t Alignment = WordSize;
    iterateArgs(Elements, [&] (size_t Idx, ArgInfo AI, size_t Offset) {
      Alignment = std::max(Alignment, AI.A.value());
    });
    return Alignment;
  }

  std::vector<ArgInfo> argInfosForFunction(Function* F) {
    std::vector<ArgInfo> Elements;
    for (Argument& A : F->args()) {
      if (A.hasByValAttr()) {
        Elements.push_back(
          ArgInfo(
            A.getParamByValType(),
            ArgKind::ByVal,
            std::max(DL.getABITypeAlign(A.getParamByValType()),
                     A.getParamAlign().valueOrOne())));
      } else {
        assert(!A.hasPassPointeeByValueCopyAttr());
        Type* T = normalizeArgType(A.getType());
        Elements.push_back(ArgInfo(T, ArgKind::Direct, DL.getABITypeAlign(T)));
      }
    }
    return Elements;
  }

  Type* normalizeArgType(Type* T) {
    assert(!isa<FunctionType>(T));
    assert(!isa<TypedPointerType>(T));
    assert(!isa<ScalableVectorType>(T));
    assert(T != FlightPtrTy);

    if (IntegerType* IntT = dyn_cast<IntegerType>(T)) {
      if (IntT->getBitWidth() < 64)
        return Int64Ty;
    }

    return T;
  }

  Value* convertToNormalizedArgType(Type* T, Value* V, Instruction* Before) {
    assert(!isa<FunctionType>(T));
    assert(!isa<TypedPointerType>(T));
    assert(!isa<ScalableVectorType>(T));
    assert(T != FlightPtrTy);

    if (IntegerType* IntT = dyn_cast<IntegerType>(T)) {
      if (IntT->getBitWidth() < 64) {
        Instruction* ZExt = new ZExtInst(V, Int64Ty, "filc_arg_zext", Before);
        ZExt->setDebugLoc(Before->getDebugLoc());
        return ZExt;
      }
    }

    return V;
  }

  Value* convertFromNormalizedArgType(Type* T, Value* V, Instruction* Before) {
    assert(!isa<FunctionType>(T));
    assert(!isa<TypedPointerType>(T));
    assert(!isa<ScalableVectorType>(T));
    assert(T != FlightPtrTy);

    if (IntegerType* IntT = dyn_cast<IntegerType>(T)) {
      if (IntT->getBitWidth() < 64) {
        Instruction* Trunc = new TruncInst(V, T, "filc_arg_trunc", Before);
        Trunc->setDebugLoc(Before->getDebugLoc());
        return Trunc;
      }
    }

    return V;
  }

  Type* normalizeRetType(Type* T) {
    if (StructType* ST = dyn_cast<StructType>(T)) {
      assert(!ST->isOpaque());
      if (ST->isLiteral() && !ST->isPacked()) {
        std::vector<Type*> Elements;
        for (Type* InnerT : ST->elements())
          Elements.push_back(normalizeArgType(InnerT));
        return StructType::get(C, Elements, false);
      }
      return ST;
    }

    return normalizeArgType(T);
  }

  Value* insertAndNormalizeReturn(Type* T, Value* V, Value* Result, Instruction* Before) {
    if (StructType* ST = dyn_cast<StructType>(T)) {
      assert(!ST->isOpaque());
      assert(ST->isLiteral());
      assert(!ST->isPacked());
      for (unsigned Index = ST->getNumElements(); Index--;) {
        Type* InnerT = ST->getElementType(Index);
        Instruction* InnerV = ExtractValueInst::Create(
          toFlightType(InnerT), V, { Index }, "filc_insert_and_normalize_return_extract",
          Before);
        InnerV->setDebugLoc(Before->getDebugLoc());
        Instruction* Insert = InsertValueInst::Create(
          Result, convertToNormalizedArgType(InnerT, InnerV, Before), Index + 1,
          "filc_insert_and_normalize_return_insert", Before);
        Insert->setDebugLoc(Before->getDebugLoc());
        Result = Insert;
      }
      return Result;
    }

    Instruction* Insert = InsertValueInst::Create(
      Result, convertToNormalizedArgType(T, V, Before), 1,
      "filc_insert_and_normalize_return_insert_one", Before);
    Insert->setDebugLoc(Before->getDebugLoc());
    return Insert;
  }

  Value* normalizeReturn(Type* T, Value* V, Instruction* Before) {
    if (StructType* ST = dyn_cast<StructType>(T)) {
      assert(!ST->isOpaque());
      if (ST->isLiteral() && !ST->isPacked()) {
        Value* Result = UndefValue::get(normalizeRetType(T));
        for (unsigned Index = ST->getNumElements(); Index--;) {
          Type* InnerT = ST->getElementType(Index);
          Instruction* InnerV = ExtractValueInst::Create(
            toFlightType(InnerT), V, { Index }, "filc_normalize_return_extract",
            Before);
          InnerV->setDebugLoc(Before->getDebugLoc());
          Instruction* Insert = InsertValueInst::Create(
            Result, convertToNormalizedArgType(InnerT, InnerV, Before), Index,
            "filc_normalize_return_insert", Before);
          Insert->setDebugLoc(Before->getDebugLoc());
          Result = Insert;
        }
        return Result;
      }
      return V;
    }

    return convertToNormalizedArgType(T, V, Before);
  }

  Value* extractAndDenormalizeReturn(Type* T, Value* V, Instruction* Before) {
    if (StructType* ST = dyn_cast<StructType>(T)) {
      assert(!ST->isOpaque());
      assert(ST->isLiteral());
      assert(!ST->isPacked());
      Value* Result = UndefValue::get(toFlightType(T));
      for (unsigned Index = ST->getNumElements(); Index--;) {
        Type* InnerT = ST->getElementType(Index);
        Instruction* InnerV = ExtractValueInst::Create(
          toFlightType(normalizeArgType(InnerT)), V, { Index + 1 },
          "filc_extract_and_denormalize_return_extract", Before);
        InnerV->setDebugLoc(Before->getDebugLoc());
        Instruction* Insert = InsertValueInst::Create(
          Result, convertFromNormalizedArgType(InnerT, InnerV, Before), { Index },
          "filc_extract_and_denormalize_return_insert", Before);
        Insert->setDebugLoc(Before->getDebugLoc());
        Result = Insert;
      }
      return Result;
    }

    Instruction* Extract = ExtractValueInst::Create(
      toFlightType(normalizeArgType(T)), V, { 1 },
      "filc_extract_and_dernomalize_return_extract_one", Before);
    Extract->setDebugLoc(Before->getDebugLoc());
    return convertFromNormalizedArgType(T, Extract, Before);
  }

  Value* denormalizeReturn(Type* T, Value* V, Instruction* Before) {
    if (StructType* ST = dyn_cast<StructType>(T)) {
      assert(!ST->isOpaque());
      if (ST->isLiteral() && !ST->isPacked()) {
        Value* Result = UndefValue::get(toFlightType(T));
        for (unsigned Index = ST->getNumElements(); Index--;) {
          Type* InnerT = ST->getElementType(Index);
          Instruction* InnerV = ExtractValueInst::Create(
            toFlightType(normalizeArgType(InnerT)), V, { Index },
            "filc_extract_and_denormalize_return_extract", Before);
          InnerV->setDebugLoc(Before->getDebugLoc());
          Instruction* Insert = InsertValueInst::Create(
            Result, convertFromNormalizedArgType(InnerT, InnerV, Before), { Index },
            "filc_extract_and_denormalize_return_insert", Before);
          Insert->setDebugLoc(Before->getDebugLoc());
          Result = Insert;
        }
        return Result;
      }
      return V;
    }

    return convertFromNormalizedArgType(T, V, Before);
  }

  FastArgType fastArgType(Type* T) {
    assert(T != FlightPtrTy);
    if (T == Int64Ty)
      return FastArgType::Int64;
    if (T == FloatTy)
      return FastArgType::Float;
    if (T == DoubleTy)
      return FastArgType::Double;
    if (T->isX86_FP80Ty())
      return FastArgType::LongDouble;
    if (VectorType* VecT = dyn_cast<VectorType>(T)) {
      if (DL.getTypeAllocSize(VecT) == 64)
        return FastArgType::Vec512;
      if (DL.getTypeAllocSize(VecT) == 32)
        return FastArgType::Vec256;
      if (DL.getTypeAllocSize(VecT) == 16)
        return FastArgType::Vec128;
      return FastArgType::Invalid;
    }
    if (T == RawPtrTy)
      return FastArgType::Ptr;
    return FastArgType::Invalid;
  }

  template<typename Func>
  void iterateReturnType(Type* T, const Func& func) {
    if (T == VoidTy)
      return;
    if (StructType* ST = dyn_cast<StructType>(T)) {
      assert(!ST->isOpaque());
      if (ST->isLiteral() && !ST->isPacked()) {
        for (unsigned Index = 0; Index < ST->getNumElements(); ++Index)
          func(ST->getElementType(Index));
        return;
      }
    }
    func(T);
  }

  uint64_t computeSignature(const std::vector<Type*>& NormalizedArgTypes, Type* NormalizedRetType) {
    FastTypeAccumulator RetAcc;
    iterateReturnType(NormalizedRetType, [&] (Type* T) {
      RetAcc.addType(fastArgType(T));
    });
    if (!RetAcc.isValid() || RetAcc.getNum() > 2)
      return GenericSignature;
    FastTypeAccumulator ArgAcc;
    for (Type* ArgT : NormalizedArgTypes)
      ArgAcc.addType(fastArgType(ArgT));
    if (!ArgAcc.isValid() || ArgAcc.getNum() > 16)
      return GenericSignature;
    assert(RetAcc.getResult() < 133);
    return SafeAdd64(1, SafeAdd64(RetAcc.getResult(), SafeMul64(ArgAcc.getResult(), 133)));
  }

  uint64_t computeSignature(const std::vector<ArgInfo>& AIs, Type* NormalizedRetType) {
    std::vector<Type*> NormalizedArgTypes;
    for (ArgInfo AI : AIs) {
      if (AI.AK != ArgKind::Direct)
        return GenericSignature;
      assert(normalizeArgType(AI.T) == AI.T);
      NormalizedArgTypes.push_back(AI.T);
    }
    return computeSignature(NormalizedArgTypes, NormalizedRetType);
  }

  std::vector<Value*> loadCC(const std::vector<ArgInfo>& AIs, Value* PassedSize,
                             FunctionCallee CCCheckFailure, Instruction* InsertBefore,
                             DebugLoc DI) {
    size_t TotalSize = argsSize(AIs);
    size_t Alignment = argsAlignment(AIs);
    assert(TotalSize);
    Instruction* PassedEnough = new ICmpInst(
      InsertBefore, ICmpInst::ICMP_ULE, ConstantInt::get(IntPtrTy, TotalSize), PassedSize,
      "filc_cc_passed_enough");
    PassedEnough->setDebugLoc(DI);
    Instruction* FailTerm = SplitBlockAndInsertIfElse(
      expectTrue(PassedEnough, InsertBefore), InsertBefore, true);
    CallInst::Create(
      CCCheckFailure, { PassedSize, ConstantInt::get(IntPtrTy, TotalSize), getOrigin(DI) }, "",
      FailTerm)->setDebugLoc(DI);
    AllocaInst* PayloadAlloca = new AllocaInst(
      Int8Ty, 0, ConstantInt::get(IntPtrTy, TotalSize), Align(Alignment), "filc_payload_alloca",
      &NewF->getEntryBlock().front());
    AllocaInst* AuxAlloca = new AllocaInst(
      Int8Ty, 0, ConstantInt::get(IntPtrTy, TotalSize), Align(WordSize),
      "filc_payload_alloca", &NewF->getEntryBlock().front());
    PayloadAlloca->setDebugLoc(DI);
    AuxAlloca->setDebugLoc(DI);
    CallInst::Create(LifetimeStart, { ConstantInt::get(IntPtrTy, TotalSize), PayloadAlloca }, "",
                     InsertBefore)->setDebugLoc(DI);
    CallInst::Create(LifetimeStart, { ConstantInt::get(IntPtrTy, TotalSize), AuxAlloca }, "",
                     InsertBefore)->setDebugLoc(DI);
    Value* InlineBuffer = threadCCInlineBufferPtr(MyThread, InsertBefore);
    CallInst* Call = CallInst::Create(
      RealMemcpy,
      { PayloadAlloca, InlineBuffer, ConstantInt::get(IntPtrTy, std::min(TotalSize, CCInlineSize)),
        ConstantInt::getBool(Int1Ty, false) },
      "", InsertBefore);
    Call->addParamAttr(0, Attribute::getWithAlignment(C, Align(WordSize)));
    Call->addParamAttr(
      1, Attribute::getWithAlignment(C, Align(MinAlign(CCAlignment, Alignment))));
    Call->setDebugLoc(DI);
    Value* InlineAuxBuffer = threadCCInlineAuxBufferPtr(MyThread, InsertBefore);
    Call = CallInst::Create(
      RealMemcpy,
      { AuxAlloca, InlineAuxBuffer, ConstantInt::get(IntPtrTy, std::min(TotalSize, CCInlineSize)),
        ConstantInt::getBool(Int1Ty, false) },
      "", InsertBefore);
    Call->addParamAttr(0, Attribute::getWithAlignment(C, Align(WordSize)));
    Call->addParamAttr(1, Attribute::getWithAlignment(C, Align(CCAlignment)));
    Call->setDebugLoc(DI);
    if (TotalSize > CCInlineSize) {
      Instruction* PayloadAtOutlineOffset = GetElementPtrInst::Create(
        Int8Ty, PayloadAlloca, { ConstantInt::get(IntPtrTy, CCInlineSize) },
        "filc_payload_at_outline_offset", InsertBefore);
      PayloadAtOutlineOffset->setDebugLoc(DI);
      Value* OutlineBufferPtr = threadCCOutlineBufferPtr(MyThread, InsertBefore);
      Instruction* OutlineBuffer = new LoadInst(
        RawPtrTy, OutlineBufferPtr, "filc_cc_outline_buffer", InsertBefore);
      OutlineBuffer->setDebugLoc(DI);
      Call = CallInst::Create(
        RealMemcpy,
        { PayloadAtOutlineOffset, OutlineBuffer, ConstantInt::get(IntPtrTy, TotalSize - CCInlineSize),
          ConstantInt::getBool(Int1Ty, false) },
        "", InsertBefore);
      Call->addParamAttr(0, Attribute::getWithAlignment(C, Align(WordSize)));
      Call->addParamAttr(
        1, Attribute::getWithAlignment(C, Align(MinAlign(CCAlignment, Alignment))));
      Call->setDebugLoc(DI);
      Instruction* AuxAtOutlineOffset = GetElementPtrInst::Create(
        Int8Ty, AuxAlloca, { ConstantInt::get(IntPtrTy, CCInlineSize) },
        "filc_aux_at_outline_offset", InsertBefore);
      AuxAtOutlineOffset->setDebugLoc(DI);
      Value* OutlineAuxBufferPtr = threadCCOutlineAuxBufferPtr(MyThread, InsertBefore);
      Instruction* OutlineAuxBuffer = new LoadInst(
        RawPtrTy, OutlineAuxBufferPtr, "filc_cc_outline_aux_buffer", InsertBefore);
      OutlineAuxBuffer->setDebugLoc(DI);
      Call = CallInst::Create(
        RealMemcpy,
        { AuxAtOutlineOffset, OutlineAuxBuffer, ConstantInt::get(IntPtrTy, TotalSize - CCInlineSize),
          ConstantInt::getBool(Int1Ty, false) },
        "", InsertBefore);
      Call->addParamAttr(0, Attribute::getWithAlignment(C, Align(WordSize)));
      Call->addParamAttr(1, Attribute::getWithAlignment(C, Align(CCAlignment)));
      Call->setDebugLoc(DI);
    }
    std::vector<Value*> Results;
    iterateArgs(AIs, [&] (size_t Index, ArgInfo AI, size_t Offset) {
      Value* PayloadPtr = GetElementPtrInst::Create(
        Int8Ty, PayloadAlloca, { ConstantInt::get(IntPtrTy, Offset) }, "filc_offset_payload",
        InsertBefore);
      Value* AuxPtr = GetElementPtrInst::Create(
        Int8Ty, AuxAlloca, { ConstantInt::get(IntPtrTy, Offset) }, "filc_offset_aux",
        InsertBefore);
      Type* ArgT = AI.T;
      Value* Result;

      switch (AI.AK) {
      case ArgKind::Direct:
        Result = loadValueRecurseAfterCheck(
          ArgT, MemoryAccessData(nullptr, PayloadPtr, AuxPtr, AuxAlloca, MemoryKind::CC), false,
          DL.getABITypeAlign(ArgT), AtomicOrdering::NotAtomic, SyncScope::System, InsertBefore);
        break;
      case ArgKind::ByVal:
        if (AI.A.value() > GCMinAlign) {
          Result = CallInst::Create(
            PromoteAlreadyCheckedStackToHeapWithAlignmentWithoutExiting,
            { MyThread, PayloadPtr, AuxPtr,
              ConstantInt::get(IntPtrTy, (DL.getTypeAllocSize(ArgT) + WordSize - 1) & -WordSize),
              ConstantInt::get(IntPtrTy, AI.A.value()) },
            "filc_promote_stack", InsertBefore);
          break;
        }
        Result = CallInst::Create(
          PromoteAlreadyCheckedStackToHeapWithoutExiting,
          { MyThread, PayloadPtr, AuxPtr,
            ConstantInt::get(IntPtrTy, (DL.getTypeAllocSize(ArgT) + WordSize - 1) & -WordSize) },
          "filc_promote_stack", InsertBefore);
        break;
      }
      
      Results.push_back(Result);
    });
    CallInst::Create(LifetimeEnd, { ConstantInt::get(IntPtrTy, TotalSize), PayloadAlloca }, "",
                     InsertBefore)->setDebugLoc(DI);
    CallInst::Create(LifetimeEnd, { ConstantInt::get(IntPtrTy, TotalSize), AuxAlloca }, "",
                     InsertBefore)->setDebugLoc(DI);
    return Results;
  }

  Value* loadCC(Type* T, Value* PassedSize, FunctionCallee CCCheckFailure, Instruction* InsertBefore,
                DebugLoc DI) {
    std::vector<ArgInfo> AIs;
    AIs.push_back(ArgInfo(T, ArgKind::Direct, DL.getABITypeAlign(T)));
    std::vector<Value*> Vs = loadCC(AIs, PassedSize, CCCheckFailure, InsertBefore, DI);
    assert(Vs.size() == 1);
    return Vs[0];
  }

  // Returns the passed CC size. That's just for convenience, since you could calculate it yourself
  // from the type.
  Value* storeCC(const std::vector<ArgInfo>& AIs, const std::vector<Value*>& Vs,
                 Instruction* InsertBefore, DebugLoc DI) {
    size_t TotalSize = argsSize(AIs);
    size_t Alignment = argsAlignment(AIs);
    assert(TotalSize);
    if (TotalSize > CCInlineSize) {
      size_t DesiredOutlineSize = TotalSize - CCInlineSize;
      Instruction* ActualOutlineSize = new LoadInst(
        IntPtrTy, threadCCOutlineSizePtr(MyThread, InsertBefore), "filc_thread_cc_outline_size",
        InsertBefore);
      ActualOutlineSize->setDebugLoc(DI);
      Instruction* OutlineBigEnough = new ICmpInst(
        InsertBefore, ICmpInst::ICMP_ULE, ConstantInt::get(IntPtrTy, DesiredOutlineSize),
        ActualOutlineSize, "filc_cc_passed_enough");
      OutlineBigEnough->setDebugLoc(DI);
      Instruction* SlowTerm = SplitBlockAndInsertIfElse(
        expectTrue(OutlineBigEnough, InsertBefore), InsertBefore, false);
      CallInst::Create(
        ThreadEnsureCCOutlineBufferSlow,
        { MyThread, ConstantInt::get(IntPtrTy, DesiredOutlineSize) }, "", SlowTerm)->setDebugLoc(DI);
    }
    AllocaInst* PayloadAlloca = new AllocaInst(
      Int8Ty, 0, ConstantInt::get(IntPtrTy, TotalSize), Align(Alignment), "filc_payload_alloca",
      &NewF->getEntryBlock().front());
    AllocaInst* AuxAlloca = new AllocaInst(
      Int8Ty, 0, ConstantInt::get(IntPtrTy, TotalSize), Align(Alignment),
      "filc_payload_alloca", &NewF->getEntryBlock().front());
    PayloadAlloca->setDebugLoc(DI);
    AuxAlloca->setDebugLoc(DI);
    CallInst::Create(LifetimeStart, { ConstantInt::get(IntPtrTy, TotalSize), PayloadAlloca }, "",
                     InsertBefore)->setDebugLoc(DI);
    CallInst::Create(LifetimeStart, { ConstantInt::get(IntPtrTy, TotalSize), AuxAlloca }, "",
                     InsertBefore)->setDebugLoc(DI);
    CallInst* Call = CallInst::Create(
      RealMemset,
      { PayloadAlloca, ConstantInt::get(Int8Ty, 0), ConstantInt::get(IntPtrTy, TotalSize),
        ConstantInt::getBool(Int1Ty, false) }, "", InsertBefore);
    Call->addParamAttr(0, Attribute::getWithAlignment(C, Align(Alignment)));
    Call->setDebugLoc(DI);
    Call = CallInst::Create(
      RealMemset,
      { AuxAlloca, ConstantInt::get(Int8Ty, 0), ConstantInt::get(IntPtrTy, TotalSize),
        ConstantInt::getBool(Int1Ty, false) }, "", InsertBefore);
    Call->addParamAttr(0, Attribute::getWithAlignment(C, Align(WordSize)));
    Call->setDebugLoc(DI);
    iterateArgs(AIs, [&] (size_t Index, ArgInfo AI, size_t Offset) {
      Value* PayloadPtr = GetElementPtrInst::Create(
        Int8Ty, PayloadAlloca, { ConstantInt::get(IntPtrTy, Offset) }, "filc_offset_payload",
        InsertBefore);
      Value* AuxPtr = GetElementPtrInst::Create(
        Int8Ty, AuxAlloca, { ConstantInt::get(IntPtrTy, Offset) }, "filc_offset_aux",
        InsertBefore);
      Type* ArgT = AI.T;
      switch (AI.AK) {
      case ArgKind::Direct: {
        storeValueRecurseAfterCheck(
          ArgT, Vs[Index], MemoryAccessData(nullptr, PayloadPtr, AuxPtr, AuxAlloca, MemoryKind::CC),
          false, DL.getABITypeAlign(ArgT), AtomicOrdering::NotAtomic, SyncScope::System,
          InsertBefore);
        break;
      }
      case ArgKind::ByVal: {
        FunctionCallee DemoteFunc;
        if (DL.getABITypeAlign(AI.T).value() >= WordSize) {
          assert(!(DL.getTypeAllocSize(AI.T) & (WordSize - 1)));
          DemoteFunc = DemoteWordAlignedAlreadyCheckedHeapToStackWithoutExiting;
        } else
          DemoteFunc = DemoteAlreadyCheckedHeapToStackWithoutExiting;
        CallInst::Create(
          DemoteFunc,
          { Vs[Index], PayloadPtr, AuxPtr, ConstantInt::get(IntPtrTy, DL.getTypeAllocSize(AI.T)) },
          "", InsertBefore);
        break;
      } }
    });
    Call = CallInst::Create(
      RealMemcpy,
      { threadCCInlineBufferPtr(MyThread, InsertBefore), PayloadAlloca,
        ConstantInt::get(IntPtrTy, std::min(TotalSize, CCInlineSize)),
        ConstantInt::getBool(Int1Ty, false) },
      "", InsertBefore);
    Call->addParamAttr(
      0, Attribute::getWithAlignment(C, Align(MinAlign(CCAlignment, Alignment))));
    Call->addParamAttr(1, Attribute::getWithAlignment(C, Align(WordSize)));
    Call->setDebugLoc(DI);
    Call = CallInst::Create(
      RealMemcpy,
      { threadCCInlineAuxBufferPtr(MyThread, InsertBefore), AuxAlloca,
        ConstantInt::get(IntPtrTy, std::min(TotalSize, CCInlineSize)),
        ConstantInt::getBool(Int1Ty, false) },
      "", InsertBefore);
    Call->addParamAttr(0, Attribute::getWithAlignment(C, Align(CCAlignment)));
    Call->addParamAttr(1, Attribute::getWithAlignment(C, Align(WordSize)));
    Call->setDebugLoc(DI);
    if (TotalSize > CCInlineSize) {
      Instruction* PayloadAtOutlineOffset = GetElementPtrInst::Create(
        Int8Ty, PayloadAlloca, { ConstantInt::get(IntPtrTy, CCInlineSize) },
        "filc_payload_at_outline_offset", InsertBefore);
      PayloadAtOutlineOffset->setDebugLoc(DI);
      Value* OutlineBufferPtr = threadCCOutlineBufferPtr(MyThread, InsertBefore);
      Instruction* OutlineBuffer = new LoadInst(
        RawPtrTy, OutlineBufferPtr, "filc_cc_outline_buffer", InsertBefore);
      OutlineBuffer->setDebugLoc(DI);
      Call = CallInst::Create(
        RealMemcpy,
        { OutlineBuffer, PayloadAtOutlineOffset, ConstantInt::get(IntPtrTy, TotalSize - CCInlineSize),
          ConstantInt::getBool(Int1Ty, false) },
        "", InsertBefore);
      Call->addParamAttr(
        0, Attribute::getWithAlignment(C, Align(MinAlign(CCAlignment, Alignment))));
      Call->addParamAttr(1, Attribute::getWithAlignment(C, Align(WordSize)));
      Call->setDebugLoc(DI);
      Instruction* AuxAtOutlineOffset = GetElementPtrInst::Create(
        Int8Ty, AuxAlloca, { ConstantInt::get(IntPtrTy, CCInlineSize) },
        "filc_aux_at_outline_offset", InsertBefore);
      AuxAtOutlineOffset->setDebugLoc(DI);
      Value* OutlineAuxBufferPtr = threadCCOutlineAuxBufferPtr(MyThread, InsertBefore);
      Instruction* OutlineAuxBuffer = new LoadInst(
        RawPtrTy, OutlineAuxBufferPtr, "filc_cc_outline_aux_buffer", InsertBefore);
      OutlineAuxBuffer->setDebugLoc(DI);
      Call = CallInst::Create(
        RealMemcpy,
        { OutlineAuxBuffer, AuxAtOutlineOffset, ConstantInt::get(IntPtrTy, TotalSize - CCInlineSize),
          ConstantInt::getBool(Int1Ty, false) },
        "", InsertBefore);
      Call->addParamAttr(0, Attribute::getWithAlignment(C, Align(CCAlignment)));
      Call->addParamAttr(1, Attribute::getWithAlignment(C, Align(WordSize)));
      Call->setDebugLoc(DI);
    }
    CallInst::Create(LifetimeEnd, { ConstantInt::get(IntPtrTy, TotalSize), PayloadAlloca }, "",
                     InsertBefore)->setDebugLoc(DI);
    CallInst::Create(LifetimeEnd, { ConstantInt::get(IntPtrTy, TotalSize), AuxAlloca }, "",
                     InsertBefore)->setDebugLoc(DI);
    return ConstantInt::get(IntPtrTy, TotalSize);
  }

  Value* storeCC(Type* T, Value* V, Instruction* InsertBefore, DebugLoc DI) {
    std::vector<ArgInfo> AIs;
    AIs.push_back(ArgInfo(T, ArgKind::Direct, DL.getABITypeAlign(T)));
    std::vector<Value*> Vs;
    Vs.push_back(V);
    return storeCC(AIs, Vs, InsertBefore, DI);
  }

  bool hasNonNullPtrs(Constant* C) {
    if (isa<ConstantData>(C))
      return false;
    if (ConstantAggregate* CA = dyn_cast<ConstantAggregate>(C)) {
      for (size_t Index = CA->getNumOperands(); Index--;) {
        if (hasNonNullPtrs(CA->getOperand(Index)))
          return true;
      }
      return false;
    }
    return hasPtrs(C->getType());
  }

  Constant* paddedConstant(Constant* C) {
    Type* LowT = C->getType();
    size_t TypeSize = DL.getTypeStoreSize(LowT);
    if (!(TypeSize % WordSize))
      return C;
    size_t PaddingSize = WordSize - (TypeSize % WordSize);
    if (verbose)
      errs() << "TypeSize = " << TypeSize << ", PaddingSize = " << PaddingSize << "\n";
    ArrayType* PaddingTy = ArrayType::get(Int8Ty, PaddingSize);
    StructType* PaddedTy = StructType::get(this->C, { LowT, PaddingTy });
    return ConstantStruct::get(PaddedTy, { C, ConstantAggregateZero::get(PaddingTy) });
  }

  enum class TryLowerConstantImplMode {
    NeedFullFlightConstant,
    NeedRestConstantWithPtrPlaceholders
  };
  Constant* tryLowerConstantImpl(Constant* C, TryLowerConstantImplMode LM) {
    assert(C->getType() != FlightPtrTy);

    bool needsFlight = LM == TryLowerConstantImplMode::NeedFullFlightConstant;
    Type* ResultT;
    if (needsFlight)
      ResultT = toFlightType(C->getType());
    else
      ResultT = C->getType();
    
    if (isa<UndefValue>(C)) {
      if (isa<IntegerType>(C->getType()))
        return ConstantInt::get(C->getType(), 0);
      if (C->getType()->isFloatingPointTy())
        return ConstantFP::get(C->getType(), 0.);
      if (C->getType() == RawPtrTy)
        return needsFlight ? FlightNull : RawNull;
      return ConstantAggregateZero::get(ResultT);
    }
    
    if (isa<ConstantPointerNull>(C))
      return needsFlight ? FlightNull : RawNull;

    if (isa<ConstantAggregateZero>(C))
      return ConstantAggregateZero::get(ResultT);

    if (GlobalValue* G = dyn_cast<GlobalValue>(C)) {
      assert(!shouldPassThrough(G));
      switch (LM) {
      case TryLowerConstantImplMode::NeedFullFlightConstant:
        return nullptr;
      case TryLowerConstantImplMode::NeedRestConstantWithPtrPlaceholders:
        return RawNull;
      }
      llvm_unreachable("should not get here");
    }

    if (isa<ConstantData>(C))
      return C;

    if (ConstantArray* CA = dyn_cast<ConstantArray>(C)) {
      std::vector<Constant*> Args;
      for (size_t Index = 0; Index < CA->getNumOperands(); ++Index) {
        Constant* LowC = tryLowerConstantImpl(CA->getOperand(Index), LM);
        if (!LowC)
          return nullptr;
        Args.push_back(LowC);
      }
      return ConstantArray::get(cast<ArrayType>(ResultT), Args);
    }
    if (ConstantStruct* CS = dyn_cast<ConstantStruct>(C)) {
      if (verbose)
        errs() << "Dealing with CS = " << *CS << "\n";
      std::vector<Constant*> Args;
      for (size_t Index = 0; Index < CS->getNumOperands(); ++Index) {
        Constant* LowC = tryLowerConstantImpl(CS->getOperand(Index), LM);
        if (!LowC)
          return nullptr;
        if (verbose)
          errs() << "Index = " << Index << ", LowC = " << *LowC << "\n";
        Args.push_back(LowC);
      }
      return ConstantStruct::get(cast<StructType>(ResultT), Args);
    }
    if (ConstantVector* CV = dyn_cast<ConstantVector>(C)) {
      std::vector<Constant*> Args;
      for (size_t Index = 0; Index < CV->getNumOperands(); ++Index) {
        Constant* LowC = tryLowerConstantImpl(CV->getOperand(Index), LM);
        if (!LowC)
          return nullptr;
        Args.push_back(LowC);
      }
      return ConstantVector::get(Args);
    }

    assert(isa<ConstantExpr>(C));
    ConstantExpr* CE = cast<ConstantExpr>(C);

    if (verbose)
      errs() << "Lowering CE = " << *CE << "\n";
    switch (LM) {
    case TryLowerConstantImplMode::NeedFullFlightConstant:
      return nullptr;
    case TryLowerConstantImplMode::NeedRestConstantWithPtrPlaceholders:
      if (isa<IntegerType>(CE->getType()))
        return ConstantInt::get(CE->getType(), 0);
      if (CE->getType() == RawPtrTy)
        return RawNull;
      
      llvm_unreachable("wtf kind of CE is that");
      return nullptr;
    }
    llvm_unreachable("bad RM");
  }

  Constant* constantToRestConstantWithPtrPlaceholders(Constant* C) {
    return tryLowerConstantImpl(C, TryLowerConstantImplMode::NeedRestConstantWithPtrPlaceholders);
  }

  Constant* tryConstantToFlightConstant(Constant* C) {
    return tryLowerConstantImpl(C, TryLowerConstantImplMode::NeedFullFlightConstant);
  }

  ConstantTarget constexprRecurse(Constant* C) {
    assert(C->getType() != FlightPtrTy);
    
    if (GlobalValue* G = dyn_cast<GlobalValue>(C)) {
      assert(!shouldPassThrough(G));
      assert(!Getters.count(G));
      assert(GlobalToGetter.count(G));
      Function* Getter = GlobalToGetter[G];
      return ConstantTarget(ConstantKind::Global, Getter);
    }

    if (ConstantExpr* CE = dyn_cast<ConstantExpr>(C)) {
      switch (CE->getOpcode()) {
      case Instruction::GetElementPtr: {
        ConstantTarget Target = constexprRecurse(CE->getOperand(0));
        APInt OffsetAP(64, 0, false);
        GetElementPtrInst* GEP = cast<GetElementPtrInst>(getAsInstruction(CE));
        bool result = GEP->accumulateConstantOffset(DL, OffsetAP);
        GEP->deleteValue();
        if (!result)
          return ConstantTarget();
        uint64_t Offset = OffsetAP.getZExtValue();
        Constant* CS = ConstantStruct::get(
          ConstexprNodeTy,
          { ConstantInt::get(Int32Ty, static_cast<unsigned>(ConstexprOpcode::AddPtrImmediate)),
            ConstantInt::get(Int32Ty, static_cast<unsigned>(Target.Kind)),
            Target.Target,
            ConstantInt::get(IntPtrTy, Offset) });
        GlobalVariable* ExprG = new GlobalVariable(
          M, ConstexprNodeTy, true, GlobalVariable::PrivateLinkage, CS, "filc_constexpr_gep_node");
        ExprG->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
        return ConstantTarget(ConstantKind::Expr, ExprG);
      }
      default:
        return ConstantTarget();
      }
    }

    return ConstantTarget();
  }

  bool computeConstantRelocations(Constant* C, std::vector<ConstantRelocation>& Result, size_t Offset = 0) {
    assert(C->getType() != FlightPtrTy);

    if (ConstantTarget CT = constexprRecurse(C)) {
      assert(!(Offset % WordSize));
      Result.push_back(ConstantRelocation(Offset, CT.Kind, CT.Target));
      return true;
    }
    
    assert(!isa<GlobalValue>(C)); // Should have been caught by constexprRecurse.

    if (isa<UndefValue>(C))
      return true;
    
    if (isa<ConstantPointerNull>(C))
      return true;

    if (isa<ConstantAggregateZero>(C))
      return true;

    if (isa<ConstantData>(C))
      return true;

    if (ConstantArray* CA = dyn_cast<ConstantArray>(C)) {
      size_t ElementSize = DL.getTypeAllocSize(CA->getType()->getElementType());
      for (size_t Index = 0; Index < CA->getNumOperands(); ++Index) {
        if (!computeConstantRelocations(CA->getOperand(Index), Result, Offset + Index * ElementSize))
          return false;
      }
      return true;
    }
    if (ConstantStruct* CS = dyn_cast<ConstantStruct>(C)) {
      if (verbose)
        errs() << "Dealing with CS = " << *CS << "\n";
      const StructLayout* SL = DL.getStructLayout(CS->getType());
      for (size_t Index = 0; Index < CS->getNumOperands(); ++Index) {
        if (!computeConstantRelocations(
              CS->getOperand(Index), Result, Offset + SL->getElementOffset(Index)))
          return false;
      }
      return true;
    }
    if (ConstantVector* CV = dyn_cast<ConstantVector>(C)) {
      size_t ElementSize = DL.getTypeAllocSize(CV->getType()->getElementType());
      for (size_t Index = 0; Index < CV->getNumOperands(); ++Index) {
        if (!computeConstantRelocations(CV->getOperand(Index), Result, Offset + Index * ElementSize))
          return false;
      }
      return true;
    }

    assert(isa<ConstantExpr>(C));
    if (verbose)
      errs() << "Failing to handle CE: " << *C << "\n";
    return false;
  }

  Value* constantToFlightValue(Constant* C, Instruction* InsertBefore) {
    if (ultraVerbose)
      errs() << "constantToFlightValue(" << *C << ")\n";
    assert(C->getType() != FlightPtrTy);

    if (Constant* LowC = tryConstantToFlightConstant(C))
      return LowC;
    
    if (GlobalValue* G = dyn_cast<GlobalValue>(C)) {
      assert(!shouldPassThrough(G)); // This is a necessary safety check, at least for setjmp, probably for other things too.
      assert(!GlobalToGetter.count(nullptr));
      assert(!Getters.count(nullptr));
      assert(!Getters.count(G));
      if (!GlobalToGetter.count(G))
        errs() << "Cannot find getter for: " << *G << "\n";
      assert(GlobalToGetter.count(G));
      assert(MyThread);
      Function* Getter = GlobalToGetter[G];
      assert(Getter);
      if (((Getter->hasWeakLinkage() || Getter->hasLinkOnceLinkage()) &&
           GlobalToComdat.count(G)) ||
          Getter->hasExternalWeakLinkage()) {
        ICmpInst* IsNull = new ICmpInst(
          InsertBefore, ICmpInst::ICMP_EQ, Getter, RawNull, "filc_weak_symbol_is_null");
        IsNull->setDebugLoc(InsertBefore->getDebugLoc());
        Instruction* NotNullTerm = SplitBlockAndInsertIfElse(IsNull, InsertBefore, false);
        Instruction* NotNullResult = CallInst::Create(
          PizlonatedGetterTy, Getter, { MyThread, getOrigin(InsertBefore->getDebugLoc()) },
          "filc_call_weak_getter", NotNullTerm);
        NotNullResult->setDebugLoc(InsertBefore->getDebugLoc());
        PHINode* Result = PHINode::Create(FlightPtrTy, 2, "filc_weak_getter_result", InsertBefore);
        Result->setDebugLoc(InsertBefore->getDebugLoc());
        Result->addIncoming(FlightNull, IsNull->getParent());
        Result->addIncoming(NotNullResult, NotNullResult->getParent());
        return Result;
      }
      Instruction* Result = CallInst::Create(
        PizlonatedGetterTy, Getter, { MyThread, getOrigin(InsertBefore->getDebugLoc()) },
        "filc_call_getter", InsertBefore);
      Result->setDebugLoc(InsertBefore->getDebugLoc());
      return Result;
    }

    if (isa<ConstantData>(C))
      return C;

    if (ConstantArray* CA = dyn_cast<ConstantArray>(C)) {
      Value* Result = UndefValue::get(toFlightType(CA->getType()));
      for (size_t Index = 0; Index < CA->getNumOperands(); ++Index) {
        Instruction* Insert = InsertValueInst::Create(
          Result, constantToFlightValue(CA->getOperand(Index), InsertBefore),
          static_cast<unsigned>(Index), "filc_insert_array", InsertBefore);
        Insert->setDebugLoc(InsertBefore->getDebugLoc());
        Result = Insert;
      }
      return Result;
    }
    if (ConstantStruct* CS = dyn_cast<ConstantStruct>(C)) {
      if (verbose)
        errs() << "Dealing with CS = " << *CS << "\n";
      Value* Result = UndefValue::get(toFlightType(CS->getType()));
      for (size_t Index = 0; Index < CS->getNumOperands(); ++Index) {
        Value* LowC = constantToFlightValue(CS->getOperand(Index), InsertBefore);
        if (verbose)
          errs() << "Index = " << Index << ", LowC = " << *LowC << "\n";
        Instruction* Insert = InsertValueInst::Create(
          Result, LowC, static_cast<unsigned>(Index), "filc_insert_struct", InsertBefore);
        Insert->setDebugLoc(InsertBefore->getDebugLoc());
        Result = Insert;
      }
      return Result;
    }
    if (ConstantVector* CV = dyn_cast<ConstantVector>(C)) {
      Value* Result = UndefValue::get(toFlightType(CV->getType()));
      for (size_t Index = 0; Index < CV->getNumOperands(); ++Index) {
        Instruction* Insert = InsertElementInst::Create(
          Result, constantToFlightValue(CV->getOperand(Index), InsertBefore),
          ConstantInt::get(IntPtrTy, Index), "filc_insert_vector", InsertBefore);
        Insert->setDebugLoc(InsertBefore->getDebugLoc());
        Result = Insert;
      }
      return Result;
    }

    assert(isa<ConstantExpr>(C));
    ConstantExpr* CE = cast<ConstantExpr>(C);

    if (verbose)
      errs() << "Lowering CE = " << *CE << "\n";

    Instruction* CEInst = getAsInstruction(CE);
    CEInst->insertBefore(InsertBefore);
    CEInst->setDebugLoc(InsertBefore->getDebugLoc());

    // I am the worst compiler programmer.
    StoreInst* DummyUser = new StoreInst(
      CEInst, RawNull, false, Align(), AtomicOrdering::NotAtomic, SyncScope::System);
    lowerInstruction(CEInst);
    Value* Result = DummyUser->getOperand(0);
    DummyUser->deleteValue();
    return Result;
  }

  Value* castInt(Value* V, Type* T, Instruction *InsertionPoint) {
    if (V->getType() == T)
      return V;
    Instruction* Result =
      CastInst::CreateIntegerCast(V, T, false, "filc_makeintptr", InsertionPoint);
    Result->setDebugLoc(InsertionPoint->getDebugLoc());
    return Result;
  }

  Value* makeIntPtr(Value* V, Instruction *InsertionPoint) {
    return castInt(V, IntPtrTy, InsertionPoint);
  }

  template<typename Func>
  void hackRAUW(Value* V, const Func& GetNewValue) {
    assert(!Dummy->getNumUses());
    V->replaceAllUsesWith(Dummy);
    Dummy->replaceAllUsesWith(GetNewValue());
  }

  void captureTypesIfNecessary(Instruction* I) {
    if (StoreInst* SI = dyn_cast<StoreInst>(I)) {
      InstTypes[I] = SI->getValueOperand()->getType();
      return;
    }

    if (AtomicCmpXchgInst* AI = dyn_cast<AtomicCmpXchgInst>(I)) {
      InstTypes[I] = AI->getNewValOperand()->getType();
      return;
    }

    if (AtomicRMWInst* AI = dyn_cast<AtomicRMWInst>(I)) {
      InstTypes[I] = AI->getValOperand()->getType();
      return;
    }

    if (CallBase* CI = dyn_cast<CallBase>(I)) {
      std::vector<Type*> Types;
      for (size_t Index = 0; Index < CI->arg_size(); ++Index)
        Types.push_back(CI->getArgOperand(Index)->getType());
      InstTypeVectors[I] = std::move(Types);
      if (InvokeInst* II = dyn_cast<InvokeInst>(I))
        LPIs[II] = II->getLandingPadInst();
      return;
    }
  }

  Value* lowerConstantValue(Value* V, Instruction* I) {
    assert(!isa<PHINode>(I));
    if (Constant* C = dyn_cast<Constant>(V)) {
      if (ultraVerbose)
        errs() << "Got V = " << *V << ", C = " << *C << "\n";
      Value* NewC = constantToFlightValue(C, I);
      if (ultraVerbose)
        errs() << "Got NewC = " << *NewC <<"\n";
      return NewC;
    }
    if (Argument* A = dyn_cast<Argument>(V)) {
      if (ultraVerbose) {
        errs() << "A = " << *A << "\n";
        errs() << "A->getArgNo() == " << A->getArgNo() << "\n";
        errs() << "Args[A->getArgNo()] == " << *Args[A->getArgNo()] << "\n";
      }
      return Args[A->getArgNo()];
    }
    return V;
  }

  void lowerConstantOperand(Use& U, Instruction* I) {
    U = lowerConstantValue(U, I);
  }

  void lowerConstantOperands(Instruction* I) {
    if (verbose)
      errs() << "Before arg lowering: " << *I << "\n";

    for (unsigned Index = I->getNumOperands(); Index--;) {
      Use& U = I->getOperandUse(Index);
      lowerConstantOperand(U, I);
      if (ultraVerbose)
        errs() << "After Index = " << Index << ", I = " << *I << "\n";
    }
    
    if (verbose)
      errs() << "After arg lowering: " << *I << "\n";
  }

  void lowerIntrinsicAccess(IntrinsicInst* II, const IntrinsicAccessDetails& IAD) {
    if (!IAD.Mask)
      return;
    bool isStore = IAD.AK == AccessKind::Write;
    FixedVectorType* T = cast<FixedVectorType>(IAD.T);
    Value* Ptr = IAD.Ptr;
    Value* Mask = IAD.Mask;
    assert(!hasPtrs(T));
        
    // FIXME: We could query the abstract state to see if this is already above this lower bound.
    Instruction* IsBelowLowerFast = new ICmpInst(
      II, ICmpInst::ICMP_ULT, flightPtrPtr(Ptr, II), flightPtrLower(Ptr, II),
      "filc_ptr_below_lower_masked_fast");
    IsBelowLowerFast->setDebugLoc(II->getDebugLoc());
    Instruction* BelowLowerTerm =
      SplitBlockAndInsertIfThen(expectFalse(IsBelowLowerFast, II), II, false);
    // NOTE: The codegen for BelowLowerTerm is further down below.
        
    // FIXME: Could check the alignment that was passed in, and use that to make this more
    // efficient.
    Instruction* UpperMinus = GetElementPtrInst::Create(
      Int8Ty, upperForLower(flightPtrLower(Ptr, II), II),
      { ConstantInt::get(IntPtrTy, -DL.getTypeStoreSize(T)) },
      "filc_upper_minus_masked_fast", II);
    UpperMinus->setDebugLoc(II->getDebugLoc());
    Instruction* IsBelowUpperFast = new ICmpInst(
      II, ICmpInst::ICMP_ULE, flightPtrPtr(Ptr, II), UpperMinus,
      "filc_ptr_below_equal_upper_masked_fast");
    IsBelowUpperFast->setDebugLoc(II->getDebugLoc());
    Instruction* AboveUpperTerm = SplitBlockAndInsertIfElse(
      expectTrue(IsBelowUpperFast, II), II, false);

    // First we identify the ptr Offset. Then we do:
    // Ptr >= Lower - Offset
    //
    // This is an overflow-free way of saying:
    // Ptr + Offset >= Lower
    uint64_t MaskSize = T->getElementCount().getFixedValue();
    // Currently filc_masked_access_check_fail can only handle 64 bit masks max.
    assert(MaskSize <= 64);
    assert(MaskSize * DL.getTypeAllocSize(T->getElementType()) == DL.getTypeAllocSize(T));
    Type* MaskIntTy = Type::getIntNTy(C, MaskSize);
    auto FixMask = [&] (Instruction* Before) -> Value* {
      Value* Result = Mask;
      if (FixedVectorType* MaskVT = dyn_cast<FixedVectorType>(Result->getType())) {
        assert(MaskVT->getElementCount().getFixedValue() == MaskSize);
        if (MaskVT->getElementType() != Int1Ty) {
          Instruction* Bools = new ICmpInst(
            Before, ICmpInst::ICMP_SLT, Result, ConstantAggregateZero::get(Result->getType()),
            "filc_mask_get_bools");
          Bools->setDebugLoc(II->getDebugLoc());
          Result = Bools;
        }
      }
      if (Result->getType() != MaskIntTy) {
        Instruction* MaskInt = new BitCastInst(Result, MaskIntTy, "filc_mask_as_int", Before);
        MaskInt->setDebugLoc(II->getDebugLoc());
        Result = MaskInt;
      }
      return Result;
    };
    Value* MaskInt = FixMask(BelowLowerTerm);
    Instruction* MaskIsZero = new ICmpInst(
      BelowLowerTerm, ICmpInst::ICMP_EQ, MaskInt, ConstantInt::get(MaskIntTy, 0),
      "filc_mask_is_zero");
    MaskIsZero->setDebugLoc(II->getDebugLoc());
    SplitBlockAndInsertIfThen(
      MaskIsZero, BelowLowerTerm, false, nullptr, nullptr, nullptr, II->getParent());
    Instruction* TrailingZeroes = CallInst::Create(
      Intrinsic::getOrInsertDeclaration(&M, Intrinsic::cttz, MaskIntTy),
      { MaskInt, ConstantInt::getTrue(Int1Ty) }, "filc_mask_trailing_zeroes", BelowLowerTerm);
    TrailingZeroes->setDebugLoc(II->getDebugLoc());
    Instruction* Offset = BinaryOperator::Create(
      Instruction::Mul, makeIntPtr(TrailingZeroes, BelowLowerTerm),
      ConstantInt::get(IntPtrTy, DL.getTypeAllocSize(T->getElementType())),
      "filc_mask_offset", BelowLowerTerm);
    Offset->setDebugLoc(II->getDebugLoc());
    Instruction* NegativeOffset = BinaryOperator::Create(
      Instruction::Sub, ConstantInt::get(IntPtrTy, 0), Offset, "filc_mask_neg_offset",
      BelowLowerTerm);
    NegativeOffset->setDebugLoc(II->getDebugLoc());
    Instruction* LowerMinus = GetElementPtrInst::Create(
      Int8Ty, flightPtrLower(Ptr, BelowLowerTerm), { NegativeOffset },
      "filc_lower_minus_masked_offset", BelowLowerTerm);
    LowerMinus->setDebugLoc(II->getDebugLoc());
    Instruction* IsBelowLower = new ICmpInst(
      BelowLowerTerm, ICmpInst::ICMP_ULT, flightPtrPtr(Ptr, BelowLowerTerm), LowerMinus,
      "filc_ptr_below_lower_masked");
    IsBelowLower->setDebugLoc(II->getDebugLoc());
    Instruction* FailTerm = SplitBlockAndInsertIfThen(
      expectFalse(IsBelowLower, BelowLowerTerm), BelowLowerTerm, true);
    PHINode* MaskIntPhi = PHINode::Create(MaskIntTy, 2, "filc_mask_as_int_phi", FailTerm);
    MaskIntPhi->addIncoming(MaskInt, IsBelowLower->getParent());
        
    // First we identify the true Size of the access. Then we do:
    // Ptr <= Upper - Size
    //
    // This is an overflow-free way of saying:
    // Ptr + Size <= Upper
    MaskInt = FixMask(AboveUpperTerm);
    MaskIsZero = new ICmpInst(
      AboveUpperTerm, ICmpInst::ICMP_EQ, MaskInt, ConstantInt::get(MaskIntTy, 0),
      "filc_mask_is_zero");
    MaskIsZero->setDebugLoc(II->getDebugLoc());
    SplitBlockAndInsertIfThen(
      MaskIsZero, AboveUpperTerm, false, nullptr, nullptr, nullptr, II->getParent());
    Instruction* LeadingZeroes = CallInst::Create(
      Intrinsic::getOrInsertDeclaration(&M, Intrinsic::ctlz, MaskIntTy),
      { MaskInt, ConstantInt::getTrue(Int1Ty) }, "filc_mask_trailing_zeroes", AboveUpperTerm);
    LeadingZeroes->setDebugLoc(II->getDebugLoc());
    Instruction* OffsetFromEnd = BinaryOperator::Create(
      Instruction::Mul, makeIntPtr(LeadingZeroes, AboveUpperTerm),
      ConstantInt::get(IntPtrTy, DL.getTypeAllocSize(T->getElementType())),
      "filc_mask_offset_from_end", AboveUpperTerm);
    OffsetFromEnd->setDebugLoc(II->getDebugLoc());
    UpperMinus = GetElementPtrInst::Create(
      Int8Ty, UpperMinus, { OffsetFromEnd }, "filc_upper_minus_masked_offset",
      AboveUpperTerm);
    UpperMinus->setDebugLoc(II->getDebugLoc());
    Instruction* IsBelowUpper = new ICmpInst(
      AboveUpperTerm, ICmpInst::ICMP_ULE, flightPtrPtr(Ptr, AboveUpperTerm), UpperMinus,
      "filc_ptr_below_equal_upper_masked");
    IsBelowUpper->setDebugLoc(II->getDebugLoc());
    SplitBlockAndInsertIfElse(
      expectTrue(IsBelowUpper, AboveUpperTerm), AboveUpperTerm, false, nullptr, nullptr, nullptr,
      FailTerm->getParent());
    MaskIntPhi->addIncoming(MaskInt, IsBelowUpper->getParent());

    CallInst::Create(
      MaskedAccessCheckFail,
      { Ptr, castInt(MaskIntPhi, Int64Ty, FailTerm),
        ConstantInt::get(IntPtrTy, DL.getTypeAllocSize(T)), ConstantInt::get(Int32Ty, isStore),
        getOrigin(II->getDebugLoc()) },
      "", FailTerm);
  }
  
  static constexpr unsigned InlineMemmoveDstSizeLimit = 40;
  
  void emitOptMemmove(Value* Dst, Value* Src, size_t Count, Instruction* I) {
    FullMemoryAccessData DstFMAD = accessDataForOperand(Dst, I, 0, I);
    FullMemoryAccessData SrcFMAD = accessDataForOperand(Src, I, 1, I);
    
    if (Count <= InlineMemmoveDstSizeLimit
        || DstFMAD.MAD.MK == MemoryKind::LocalNaked
        || SrcFMAD.MAD.MK == MemoryKind::LocalNaked) {
      // Let's consider the number of ptr words that have to be loaded and stored for different
      // counts.
      //
      // 16: load 1 or 2, store 2 or 3
      // 15: load 1, store 2 or 3
      // 14: load 0 or 1, store 2 or 3
      // 13: load 0 or 1, store 2 or 3
      // 12: load 0 or 1, store 2 or 3
      // 11: load 0 or 1, store 2 or 3
      // 10: load 0 or 1, store 2 or 3
      // 9: load 0 or 1, store 2
      // 8: load 0 or 1, store 1 or 2
      // 7: load 0, store 1 or 2
      // 6: load 0, store 1 or 2
      // 5: load 0, store 1 or 2
      // 4: load 0, store 1 or 2
      // 3: load 0, store 1 or 2
      // 2: load 0, store 1 or 2
      // 1: load 0, store 1
      //
      // One way to compute the number of loaded words is:
      //
      //     Max((RoundDown(Ptr + Count, 8) - RoundUp(Ptr, 8)), 0) / 8
      // or: Max(Floor((Ptr + Count) / 8) - Ceil(Ptr / 8), 0)
      //
      // One way to compute the number of stored words is:
      //
      //     (RoundUp(Ptr + Count, 8) - RoundDown(Ptr, 8)) / 8
      // or: Ceil((Ptr + Count) / 8) - Floor(Ptr / 8)
      //
      // Let's simplify the expression for Count = 16:
      //
      //     Max(Floor((Ptr + 16) / 8) - Ceil(Ptr / 8), 0)
      //   = Max(Floor(Ptr / 8 + 2) - Ceil(Ptr / 8), 0)
      //   = Max(Floor(Ptr / 8) - Ceil(Ptr / 8) + 2, 0)
      //   = Floor(Ptr / 8) - Ceil(Ptr / 8) + 2
      //   = 2 + Floor(Ptr / 8) - Ceil(Ptr / 8)
      //   = 2 - (Ptr is misaligned ? 1 : 0)
      //
      //     Ceil((Ptr + 16) / 8) - Floor(Ptr / 8)
      //   = Ceil(Ptr / 8) + 2 - Floor(Ptr / 8)
      //   = 2 + Ceil(Ptr / 8) - Floor(Ptr / 8)
      //   = 2 + (Ptr is misaligned ? 1 : 0)
      //
      // It gets weirder if Count is not aligned, since then we can't pull off the simplification.
      // Except if Count is 1 less or 1 more than aligned, since RoundDown(Ptr + 7, 8) is really
      // just RoundUp(Ptr, 8) and RoundUp(Ptr + 1, 8) is really RoundDown(Ptr, 8) + 1.
      //
      // But to really understand what is going on, it's best to consider some examples, and try to
      // optimize each of them.
      //
      // Let's consider the aligned Count algo:
      //
      // track Count / 8 ptrs
      // if (src and dst have different align)
      //     set all ptrs to null
      // else if (aligned)
      //     load Count / 8 ptrs
      // else
      //     set first ptr to null
      //     load Count / 8 - 1 ptrs
      // if (all ptrs null)
      //     if (no dst aux)
      //         done
      // else
      //     if (no dst aux or barrier)
      //         slow
      // store Count / 8 ptrs
      // if (not dst aligned)
      //     store extra null
      //
      // Now let's consider the aligned Count - 1 algo (so for example Count = 15 or Count = 8):
      //
      // track Floor(Count / 8) ptrs
      // if (src and dst have different align)
      //     set all ptrs to null
      // else
      //     load Floor(Count / 8) ptrs
      // if (all ptrs null)
      //     if (no dst aux)
      //         done
      // else
      //     if (no dst aux or barrier)
      //         slow
      // store Floor(Count / 8) ptrs
      // if (dst mod 8 != 0)
      //     store extra null below
      // if (dst mod 8 != 1)
      //     store extra null above
      //
      // Now let's consider aligned Count - 2 algo (so for example Count = 14):
      //
      // track Floor(Count / 8) ptrs
      // if (src and dst have different align)
      //     set all ptrs to null
      // else if (dst mod 8 != 1)
      //     load Floor(Count / 8) ptrs
      // else
      //     set first ptr to null
      //     load Floor(Count / 8) - 1 ptrs
      // if (all ptrs null)
      //     if (no dst aux)
      //         done
      // else
      //     if (no dst aux or barrier)
      //         slow
      // store Floor(Count / 8) ptrs
      // if (dst mod 8 > 1)
      //     store extra null below
      // if (dst mod 8 != 2)
      //     store extra null above
      //
      // Now let's consider aligned Count - 3 algo (so for example Count = 13):
      //
      // track Floor(Count / 8) ptrs
      // if (src and dst have different align)
      //     set all ptrs to null
      // else if (dst mod 8 == 0 or dst mod 8 > 2)
      //     load Floor(Count / 8) ptrs
      // else
      //     set first ptr to null
      //     load Floor(Count / 8) - 1 ptrs
      // if (all ptrs null)
      //     if (no dst aux)
      //         done
      // else
      //     if (no dst aux or barrier)
      //         slow
      // store Floor(Count / 8) ptrs
      // if (dst mod 8 > 2)
      //     store extra null below
      // if (dst mod 8 != 3)
      //     store extra null above
      //
      // Now let's consider aligned Count - 5 algo (so for example Count = 11):
      //
      // track Floor(Count / 8) ptrs
      // if (src and dst have different align)
      //     set all ptrs to null
      // else if (dst mod 8 == 0 or dst mod 8 > 4)
      //     load Floor(Count / 8) ptrs
      // else
      //     set first ptr to null
      //     load Floor(Count / 8) - 1 ptrs
      // if (all ptrs null)
      //     if (no dst aux)
      //         done
      // else
      //     if (no dst aux or barrier)
      //         slow
      // store Floor(Count / 8) ptrs
      // if (dst mod 8 > 4)
      //     store extra null below
      // if (dst mod 8 != 5)
      //     store extra null above
      //
      // Now let's consider aligned Count - 7 algo (so for example Count = 9):
      //
      // track Floor(Count / 8) ptrs
      // if (src and dst have different align)
      //     set all ptrs to null
      // else if (dst mod 8 == 0 or dst mod 8 == 7)
      //     load Floor(Count / 8) ptrs
      // else
      //     set first ptr to null
      //     load Floor(Count / 8) - 1 ptrs
      // if (all ptrs null)
      //     if (no dst aux)
      //         done
      // else
      //     if (no dst aux or barrier)
      //         slow
      // store Floor(Count / 8) ptrs
      // if (dst mod 8 == 7)
      //     store extra null below
      // if (dst mod 8 != 7)
      //     store extra null above
      //
      // Note that the above algorithms has extra conditionalizing.
      //
      // OK, so the general algorithm is:
      //
      // track Ceil(Count / 8) ptrs, initialize to null
      // if (src and dst have same align && src has aux && Count >= 8)
      //     if (dst mod 8 == 0)
      //         load Floor(Count / 8) ptrs
      //     else (dst mod 8 >= 8 - (Count mod 8))
      //         load Floor(Count / 8) ptrs starting at dst rounded up to 8
      //     else
      //         load Floor(Count / 8) - 1 ptrs starting at dst rounded up to 8
      // if (all ptrs null)
      //     if (no dst aux)
      //         goto done
      // else
      //     if (no dst aux or barrier)
      //         slow
      //         goto storeExtra
      // store Ceil(Count / 8) ptrs at dst rounded down to 8
      // storeExtra:
      // if (dst mod 8 > (8 - Count mod 8) mod 8)
      //     store extra null above
      // done:

      // Copy the payload.
      Value* DstP = flightPtrPtr(Dst, I);
      Value* SrcP = flightPtrPtr(Src, I);
      CallInst::Create(
        RealMemmove, { DstP, SrcP, ConstantInt::get(IntPtrTy, Count), ConstantInt::getFalse(Int1Ty) },
        "", I)->setDebugLoc(I->getDebugLoc());

      // Copy the lowers.
      std::vector<AllocaInst*> LowerAllocas;
      for (size_t Index = (Count + 7) / 8; Index--;) {
        AllocaInst* Alloca = new AllocaInst(
          RawPtrTy, 0, ConstantInt::get(IntPtrTy, 1), Align(WordSize), "filc_memove_lower_alloca",
          &NewF->getEntryBlock().front());
        Alloca->setDebugLoc(I->getDebugLoc());
        LowerAllocas.push_back(Alloca);
        (new StoreInst(RawNull, Alloca, I))->setDebugLoc(I->getDebugLoc());
      }
      Instruction* SrcPInt = new PtrToIntInst(
        SrcP, IntPtrTy, "filc_memmove_src_ptr_int", I);
      SrcPInt->setDebugLoc(I->getDebugLoc());
      Instruction* DstPInt = new PtrToIntInst(
        DstP, IntPtrTy, "filc_memmove_dst_ptr_int", I);
      DstPInt->setDebugLoc(I->getDebugLoc());
      Instruction* SrcPhase = BinaryOperator::Create(
        Instruction::And, SrcPInt, ConstantInt::get(IntPtrTy, WordSize - 1),
        "filc_memmove_src_phase", I);
      SrcPhase->setDebugLoc(I->getDebugLoc());
      Instruction* DstPhase = BinaryOperator::Create(
        Instruction::And, DstPInt, ConstantInt::get(IntPtrTy, WordSize - 1),
        "filc_memmove_dst_phase", I);
      DstPhase->setDebugLoc(I->getDebugLoc());
      Instruction* BeforeNullCheck = nullptr;
      Value* AllNull = ConstantInt::getTrue(Int1Ty);
      for (Value* LowerAlloca : LowerAllocas) {
        Instruction* Lower = new LoadInst(RawPtrTy, LowerAlloca, "filc_memmove_lower", I);
        Lower->setDebugLoc(I->getDebugLoc());
        if (!BeforeNullCheck)
          BeforeNullCheck = Lower;
        Instruction* IsNull = new ICmpInst(
          I, ICmpInst::ICMP_EQ, Lower, RawNull, "filc_memmove_lower_is_null");
        IsNull->setDebugLoc(I->getDebugLoc());
        Instruction* AllNullInst = BinaryOperator::Create(
          Instruction::And, AllNull, IsNull, "filc_memmove_all_lowers_are_null", I);
        AllNullInst->setDebugLoc(I->getDebugLoc());
        AllNull = AllNullInst;
      }
      Instruction* AllNullTerm;
      Instruction* NonNullTerm;
      SplitBlockAndInsertIfThenElse(AllNull, I, &AllNullTerm, &NonNullTerm);
      BasicBlock* AllNullB = AllNullTerm->getParent();
      Instruction* SrcAuxIsNull = new ICmpInst(
        BeforeNullCheck, ICmpInst::ICMP_EQ, SrcFMAD.MAD.AuxBaseP, RawNull,
        "filc_memmove_src_aux_is_null");
      SrcAuxIsNull->setDebugLoc(I->getDebugLoc());
      SplitBlockAndInsertIfThen(
        SrcAuxIsNull, BeforeNullCheck, false, nullptr, nullptr, nullptr, AllNullB);
      Instruction* SamePhase = new ICmpInst(
        BeforeNullCheck, ICmpInst::ICMP_EQ, SrcPhase, DstPhase, "filc_memmove_same_phase");
      SamePhase->setDebugLoc(I->getDebugLoc());
      SplitBlockAndInsertIfElse(
        SamePhase, BeforeNullCheck, false, nullptr, nullptr, nullptr, AllNullB);
      if (Count >= WordSize) {
        Instruction* IsAligned = new ICmpInst(
          BeforeNullCheck, ICmpInst::ICMP_EQ, DstPhase, ConstantInt::get(IntPtrTy, 0),
          "filc_memmove_is_aligned");
        IsAligned->setDebugLoc(I->getDebugLoc());
        // We are trying to do:
        //
        //     if (dst mod 8 == 0)
        //         load Floor(Count / 8) ptrs
        //     else (dst mod 8 >= 8 - (Count mod 8))
        //         load Floor(Count / 8) ptrs starting at dst rounded up to 8
        //     else
        //         load Floor(Count / 8) - 1 ptrs starting at dst rounded up to 8
        //
        // But a better way to say it is:
        //
        //     if (dst mod 8 == 0)
        //         load first ptr
        //     load Floor(Count / 8) - 1 ptrs starting at dst plus 1 rounded up to 8 (i.e. dst plus 8
        //         rounded down to 8)
        //     if (dst mod 8 >= 8 - (Count mod 8))
        //         load last ptr
        //
        // Which means that an even better way to say it is:
        //
        //     if (dst mod 8 == 0)
        //         load first ptr
        //         baseSrc = src
        //     else
        //         baseSrc = src & -8
        //     load Floor(Count / 8) - 1 ptrs starting at baseSrc + 8
        //     if (dst mod 8 >= 8 - (Count mod 8))
        //         load last ptr
        Instruction* AlignedTerm;
        Instruction* MisalignedTerm;
        SplitBlockAndInsertIfThenElse(
          IsAligned, BeforeNullCheck, &AlignedTerm, &MisalignedTerm);
        auto LoadLower = [&] (Value* SrcAuxP, size_t Index, Instruction* Before) {
          assert(Index < LowerAllocas.size());
          Instruction* Load = new LoadInst(
            IntPtrTy, SrcAuxP, "filc_memmove_load_lower", false, Align(WordSize),
            AtomicOrdering::Monotonic, SyncScope::System, Before);
          Load->setDebugLoc(I->getDebugLoc());
          if (SrcFMAD.MAD.MK == MemoryKind::Heap) {
            Instruction* Masked = BinaryOperator::Create(
              Instruction::And, Load, ConstantInt::get(IntPtrTy, AtomicBoxBit),
              "filc_memmove_lower_atomic_box_bit", Before);
            Masked->setDebugLoc(I->getDebugLoc());
            Instruction* IsNotBox = new ICmpInst(
              Before, ICmpInst::ICMP_EQ, Masked, ConstantInt::get(IntPtrTy, 0),
              "filc_memmove_lower_is_not_box");
            IsNotBox->setDebugLoc(I->getDebugLoc());
            Instruction* NotBoxTerm;
            Instruction* BoxTerm;
            SplitBlockAndInsertIfThenElse(expectTrue(IsNotBox, Before), Before, &NotBoxTerm, &BoxTerm);
            (new StoreInst(Load, LowerAllocas[Index], NotBoxTerm))->setDebugLoc(I->getDebugLoc());
            Instruction* BoxAsInt = BinaryOperator::Create(
              Instruction::And, Load, ConstantInt::get(IntPtrTy, ~AtomicBoxBit),
              "filc_memmove_box_as_int", BoxTerm);
            BoxAsInt->setDebugLoc(I->getDebugLoc());
            Instruction* Box = new IntToPtrInst(BoxAsInt, RawPtrTy, "filc_memmove_box", BoxTerm);
            Box->setDebugLoc(I->getDebugLoc());
            Instruction* LowerFromBoxLoad = new LoadInst(
              IntPtrTy, flightPtrLowerPtr(Box, BoxTerm), "filc_memmove_lower_from_box", false,
              Align(WordSize), AtomicOrdering::Monotonic, SyncScope::System, BoxTerm);
            LowerFromBoxLoad->setDebugLoc(I->getDebugLoc());
            (new StoreInst(LowerFromBoxLoad, LowerAllocas[Index], BoxTerm))
              ->setDebugLoc(I->getDebugLoc());
          } else {
            assert(SrcFMAD.MAD.MK == MemoryKind::LocalExplicit ||
                   SrcFMAD.MAD.MK == MemoryKind::LocalNaked);
            (new StoreInst(Load, LowerAllocas[Index], Before))->setDebugLoc(I->getDebugLoc());
          }
        };
        LoadLower(SrcFMAD.MAD.AuxP, 0, AlignedTerm);
        Instruction* SrcAuxPInt =
          new PtrToIntInst(SrcFMAD.MAD.AuxP, IntPtrTy, "filc_memmove_src_aux_int", MisalignedTerm);
        SrcAuxPInt->setDebugLoc(I->getDebugLoc());
        Instruction* SrcRoundedDownInt = BinaryOperator::Create(
          Instruction::And, SrcAuxPInt, ConstantInt::get(IntPtrTy, -WordSize),
          "filc_memmove_src_aux_int_rounded_down", MisalignedTerm);
        SrcRoundedDownInt->setDebugLoc(I->getDebugLoc());
        Instruction* SrcRoundedDown = new IntToPtrInst(
          SrcRoundedDownInt, RawPtrTy, "filc_memmove_src_aux_rounded_down", MisalignedTerm);
        SrcRoundedDown->setDebugLoc(I->getDebugLoc());
        PHINode* BaseSrc = PHINode::Create(RawPtrTy, 2, "filc_memmove_src_aux_base", BeforeNullCheck);
        BaseSrc->addIncoming(SrcFMAD.MAD.AuxP, AlignedTerm->getParent());
        BaseSrc->addIncoming(SrcRoundedDown, MisalignedTerm->getParent());
        BaseSrc->setDebugLoc(I->getDebugLoc());
        for (size_t Index = 1; Index < Count / WordSize; ++Index) {
          GetElementPtrInst* BaseGEP = GetElementPtrInst::Create(
            RawPtrTy, BaseSrc, { ConstantInt::get(IntPtrTy, Index) }, "filc_memmove_src_aux_gep",
            BeforeNullCheck);
          BaseGEP->setDebugLoc(I->getDebugLoc());
          LoadLower(BaseGEP, Index, BeforeNullCheck);
        }
        Instruction* NeedLast = new ICmpInst(
          BeforeNullCheck, ICmpInst::ICMP_UGE, DstPhase,
          ConstantInt::get(IntPtrTy, WordSize - (Count & (WordSize - 1))),
          "filc_memmove_need_last_load");
        NeedLast->setDebugLoc(I->getDebugLoc());
        Instruction* LastTerm = SplitBlockAndInsertIfThen(NeedLast, BeforeNullCheck, false);
        GetElementPtrInst* BaseGEP = GetElementPtrInst::Create(
          RawPtrTy, BaseSrc, { ConstantInt::get(IntPtrTy, LowerAllocas.size() - 1) },
          "filc_memmove_src_aux_last_gep", LastTerm);
        BaseGEP->setDebugLoc(I->getDebugLoc());
        LoadLower(BaseGEP, LowerAllocas.size() - 1, LastTerm);
      }
      BasicBlock* StoreLowersB = I->getParent();
      BasicBlock* StoreExtraB = SplitBlock(StoreLowersB, I);
      assert(StoreExtraB == I->getParent());
      BasicBlock* DoneB = SplitBlock(StoreExtraB, I);
      assert(StoreLowersB != StoreExtraB);
      assert(StoreLowersB != DoneB);
      assert(StoreExtraB != DoneB);
      Instruction* StoreLowersTerm = StoreLowersB->getTerminator();
      Instruction* StoreExtraTerm = StoreExtraB->getTerminator();
      assert(StoreLowersTerm != StoreExtraTerm);
      Instruction* SlowAuxP = nullptr;
      if (DstFMAD.MAD.MK == MemoryKind::Heap) {
        Instruction* DstAuxIsNull = new ICmpInst(
          AllNullTerm, ICmpInst::ICMP_EQ, DstFMAD.MAD.AuxBaseP, RawNull,
          "filc_memmove_dst_aux_is_null");
        DstAuxIsNull->setDebugLoc(I->getDebugLoc());
        SplitBlockAndInsertIfThen(
          DstAuxIsNull, AllNullTerm, false, nullptr, nullptr, nullptr, DoneB);
        DstAuxIsNull = new ICmpInst(
          NonNullTerm, ICmpInst::ICMP_EQ, DstFMAD.MAD.AuxBaseP, RawNull,
          "filc_memmove_dst_aux_is_null");
        DstAuxIsNull->setDebugLoc(I->getDebugLoc());
        Instruction* SlowTerm = SplitBlockAndInsertIfThen(DstAuxIsNull, NonNullTerm, false);
        BasicBlock* SlowB = SlowTerm->getParent();
        storeOrigin(getOrigin(I->getDebugLoc()), SlowTerm);
        std::vector<Value*> Args;
        FunctionCallee Callee;
        switch (LowerAllocas.size()) {
        case 1:
          Callee = FinishMemmoveSmall1;
          break;
        case 2:
          Callee = FinishMemmoveSmall2;
          break;
        case 3:
          Callee = FinishMemmoveSmall3;
          break;
        case 4:
          Callee = FinishMemmoveSmall4;
          break;
        case 5:
          Callee = FinishMemmoveSmall5;
          break;
        default:
          llvm_unreachable("Bad value of LowerAllocas.size()");
          break;
        }
        Args.push_back(MyThread);
        Args.push_back(Dst);
        for (AllocaInst* LowerAlloca : LowerAllocas) {
          Instruction* Load = new LoadInst(RawPtrTy, LowerAlloca, "filc_memmove_load_lower", SlowTerm);
          Load->setDebugLoc(I->getDebugLoc());
          Args.push_back(Load);
        }
        Instruction* Slow = CallInst::Create(Callee, Args, "filc_memmove_slow", SlowTerm);
        Slow->setDebugLoc(I->getDebugLoc());
        Instruction* SlowAuxBase = ExtractValueInst::Create(
          RawPtrTy, Slow, { 0 }, "filc_memmove_slow_aux_base", SlowTerm);
        SlowAuxBase->setDebugLoc(I->getDebugLoc());
        SlowAuxP = ExtractValueInst::Create(
          RawPtrTy, Slow, { 1 }, "filc_memmove_slow_aux_ptr", SlowTerm);
        SlowAuxP->setDebugLoc(I->getDebugLoc());
        assert(DstFMAD.POD.PK == PointerKind::Escaping);
        assert(DstFMAD.MAD.MK == MemoryKind::Heap);
        (new StoreInst(SlowAuxBase, DstFMAD.POD.AuxBaseVar, SlowTerm))->setDebugLoc(I->getDebugLoc());
        ReplaceInstWithInst(SlowTerm, BranchInst::Create(StoreExtraB));
        LoadInst* MarkingState = new LoadInst(Int32Ty, CurrentMarkingState,
                                              "filc_memmove_marking_state", NonNullTerm);
        MarkingState->setDebugLoc(I->getDebugLoc());
        ICmpInst* IsNotMarking = new ICmpInst(
          NonNullTerm, ICmpInst::ICMP_EQ, MarkingState, ConstantInt::get(Int32Ty, 0),
          "filc_memmove_is_not_marking");
        IsNotMarking->setDebugLoc(I->getDebugLoc());
        SplitBlockAndInsertIfElse(
          IsNotMarking, NonNullTerm, false, nullptr, nullptr, nullptr, SlowB);
      }
      Instruction* DstAuxPInt =
        new PtrToIntInst(DstFMAD.MAD.AuxP, IntPtrTy, "filc_memmove_dst_aux_int", StoreLowersTerm);
      DstAuxPInt->setDebugLoc(I->getDebugLoc());
      Instruction* DstRoundedDownInt = BinaryOperator::Create(
        Instruction::And, DstAuxPInt, ConstantInt::get(IntPtrTy, -WordSize),
        "filc_memmove_dst_aux_rounded_down_int", StoreLowersTerm);
      DstRoundedDownInt->setDebugLoc(I->getDebugLoc());
      Instruction* DstRoundedDown = new IntToPtrInst(
        DstRoundedDownInt, RawPtrTy, "filc_memmove_dst_aux_rounded_down", StoreLowersTerm);
      DstRoundedDown->setDebugLoc(I->getDebugLoc());
      for (size_t Index = 0; Index < LowerAllocas.size(); ++Index) {
        Instruction* Load = new LoadInst(RawPtrTy, LowerAllocas[Index], "filc_memmove_load_lower",
                                         StoreLowersTerm);
        Load->setDebugLoc(I->getDebugLoc());
        GetElementPtrInst* BaseGEP = GetElementPtrInst::Create(
          RawPtrTy, DstRoundedDown, { ConstantInt::get(IntPtrTy, Index) }, "filc_memmove_dst_aux_gep",
          StoreLowersTerm);
        BaseGEP->setDebugLoc(I->getDebugLoc());
        (new StoreInst(Load, BaseGEP, false, Align(WordSize), AtomicOrdering::Monotonic,
                       SyncScope::System, StoreLowersTerm))->setDebugLoc(I->getDebugLoc());
      }
      PHINode* DstBasePhi = PHINode::Create(RawPtrTy, 2, "filc_memmove_dst_base", StoreExtraTerm);
      DstBasePhi->addIncoming(DstRoundedDown, DstRoundedDown->getParent());
      if (SlowAuxP)
        DstBasePhi->addIncoming(SlowAuxP, SlowAuxP->getParent());
      Instruction* NeedExtra = new ICmpInst(
        StoreExtraTerm, ICmpInst::ICMP_UGT, DstPhase,
        ConstantInt::get(IntPtrTy, (WordSize - (Count & (WordSize - 1))) & (WordSize - 1)),
        "filc_memmove_need_extra_store");
      NeedExtra->setDebugLoc(I->getDebugLoc());
      Instruction* ReallyStoreExtraTerm = SplitBlockAndInsertIfThen(NeedExtra, StoreExtraTerm, false);
      GetElementPtrInst* BaseGEP = GetElementPtrInst::Create(
        RawPtrTy, DstBasePhi, { ConstantInt::get(IntPtrTy, LowerAllocas.size()) },
        "filc_memmove_dst_aux_gep", ReallyStoreExtraTerm);
      BaseGEP->setDebugLoc(I->getDebugLoc());
      (new StoreInst(RawNull, BaseGEP, false, Align(WordSize), AtomicOrdering::Monotonic,
                     SyncScope::System, ReallyStoreExtraTerm))->setDebugLoc(I->getDebugLoc());
      if (DstFMAD.MAD.MK == MemoryKind::LocalNaked) {
        assert(!(DstFMAD.MAD.Size % WordSize));
        assert(DstFMAD.MAD.LocalOffset != INT64_MIN);
        assert(DstFMAD.MAD.OrigAI);
        for (int64_t Offset = std::max(static_cast<int64_t>(0), DstFMAD.MAD.LocalOffset) & -WordSize;
             Offset < static_cast<int64_t>(
               (std::min(static_cast<int64_t>(DstFMAD.MAD.Size),
                         DstFMAD.MAD.LocalOffset + static_cast<int64_t>(Count))
                + WordSize - 1)
               & -WordSize);
             Offset += WordSize) {
          assert(Offset >= 0);
          assert(!(Offset % WordSize));
          assert(static_cast<uint64_t>(Offset) < DstFMAD.POD.LAD.Size);
          size_t Index = Offset / WordSize;
          GetElementPtrInst* GEP = GetElementPtrInst::Create(
            RawPtrTy, DstFMAD.MAD.AuxBaseP, { ConstantInt::get(IntPtrTy, Index) },
            "filc_memmove_naked_gc_aux_gep", I);
          GEP->setDebugLoc(I->getDebugLoc());
          LoadInst* Load = new LoadInst(RawPtrTy, GEP, "filc_memmove_naked_gc_aux_load", I);
          Load->setDebugLoc(I->getDebugLoc());
          FrameEntry FE = FrameIndexMap[ValuePtr(DstFMAD.MAD.OrigAI, Index)];
          assert(FE.FEK == FrameEntryKind::LowerFromStackAux);
          recordLowerAtIndex(Load, FE.Index, I);
        }
      }
      return;
    }
    if (DstFMAD.MAD.MK == MemoryKind::LocalExplicit) {
      if (SrcFMAD.MAD.MK == MemoryKind::LocalExplicit) {
        CallInst::Create(
          MemmoveAlreadyCheckedStack,
          { MyThread, flightPtrPtr(Dst, I), DstFMAD.MAD.AuxP, flightPtrPtr(Src, I), SrcFMAD.MAD.AuxP,
            ConstantInt::get(IntPtrTy, Count) }, "", I)->setDebugLoc(I->getDebugLoc());
        return;
      }
      assert(SrcFMAD.MAD.MK == MemoryKind::Heap);
      CallInst::Create(
        MemmoveAlreadyCheckedHeapToStack,
        { MyThread, flightPtrPtr(Dst, I), DstFMAD.MAD.AuxP, Src, ConstantInt::get(IntPtrTy, Count) },
        "", I)->setDebugLoc(I->getDebugLoc());
      return;
    }
    assert(DstFMAD.MAD.MK == MemoryKind::Heap);
    if (SrcFMAD.MAD.MK == MemoryKind::LocalExplicit) {
      CallInst::Create(
        MemmoveAlreadyCheckedStackToHeap,
        { MyThread, Dst, flightPtrPtr(Src, I), SrcFMAD.MAD.AuxP, ConstantInt::get(IntPtrTy, Count),
          getOrigin(I->getDebugLoc()) }, "", I)->setDebugLoc(I->getDebugLoc());
      return;
    }
    assert(SrcFMAD.MAD.MK == MemoryKind::Heap);
    FunctionCallee MemmoveFunc;
    if (Count <= MaxBytesBetweenPollchecks)
      MemmoveFunc = MemmoveAlreadyCheckedSmall;
    else
      MemmoveFunc = MemmoveAlreadyChecked;
    CallInst::Create(
      MemmoveFunc,
      { MyThread, Dst, Src, ConstantInt::get(IntPtrTy, Count), getOrigin(I->getDebugLoc()) },
      "", I)->setDebugLoc(I->getDebugLoc());
  }

  void lowerMemmoveCall(Instruction* I) {
    CallBase* CI = cast<CallBase>(I);
    lowerConstantOperand(CI->getArgOperandUse(0), I);
    lowerConstantOperand(CI->getArgOperandUse(1), I);
    lowerConstantOperand(CI->getArgOperandUse(2), I);
    Value* Dst = CI->getArgOperand(0);
    Value* Src = CI->getArgOperand(1);
    Value* Count = CI->getArgOperand(2);
    if (isInlineableMemmoveCall(I)) {
      emitOptMemmove(Dst, Src, cast<ConstantInt>(Count)->getZExtValue(), I);
      return;
    }

    FullMemoryAccessData DstFMAD = accessDataForOperand(Dst, I, 0, I);
    FullMemoryAccessData SrcFMAD = accessDataForOperand(Src, I, 1, I);

    if (DstFMAD.MAD.MK == MemoryKind::LocalExplicit) {
      if (SrcFMAD.MAD.MK == MemoryKind::LocalExplicit) {
        CallInst::Create(
          MemmoveStack,
          { MyThread, flightPtrPtr(Dst, I), DstFMAD.MAD.AuxP, DstFMAD.POD.LAD.AuxAlloca,
            flightPtrPtr(Src, I), SrcFMAD.MAD.AuxP, SrcFMAD.POD.LAD.AuxAlloca,
            Count, getOrigin(I->getDebugLoc()) },
          "", I)->setDebugLoc(I->getDebugLoc());
        return;
      }
      assert(SrcFMAD.MAD.MK == MemoryKind::Heap);
      CallInst::Create(
        MemmoveHeapToStack,
        { MyThread, flightPtrPtr(Dst, I), DstFMAD.MAD.AuxP, DstFMAD.POD.LAD.AuxAlloca, Src,
          Count, getOrigin(I->getDebugLoc()) },
        "", I)->setDebugLoc(I->getDebugLoc());
      return;
    }
    assert(DstFMAD.MAD.MK == MemoryKind::Heap);
    if (SrcFMAD.MAD.MK == MemoryKind::LocalExplicit) {
      CallInst::Create(
        MemmoveStackToHeap,
        { MyThread, Dst, flightPtrPtr(Src, I), SrcFMAD.MAD.AuxP, SrcFMAD.POD.LAD.AuxAlloca,
          Count, getOrigin(I->getDebugLoc()) },
        "", I)->setDebugLoc(I->getDebugLoc());
      return;
    }
    assert(SrcFMAD.MAD.MK == MemoryKind::Heap);
    
    CallInst::Create(
      Memmove,
      { MyThread, Dst, Src, makeIntPtr(Count, I), getOrigin(I->getDebugLoc()) },
      "", I)->setDebugLoc(I->getDebugLoc());
  }

  void initializeNonescapingAlloca(const LocalAllocaData& LAD, Instruction* Before) {
    if (LAD.Explicit) {
      FrameEntry FE;
      assert(FrameIndexMap.count(ValuePtr(LAD.OrigAI, 0)));
      FE = FrameIndexMap[ValuePtr(LAD.OrigAI, 0)];
      assert(FE.FEK == FrameEntryKind::ExplicitStackAux);
      assert(FE.Index != SIZE_MAX);
      assert(!(LAD.Size % WordSize));
      (new StoreInst(ConstantInt::get(IntPtrTy, LAD.Size / WordSize), LAD.AuxAlloca, Before))
        ->setDebugLoc(Before->getDebugLoc());
      recordLowerAtIndex(LAD.AuxAlloca, FE.Index, Before);
    }
    CallInst::Create(
      RealMemset,
      { LAD.Payload, ConstantInt::get(Int8Ty, 0), ConstantInt::get(IntPtrTy, LAD.Size),
        ConstantInt::getFalse(Int1Ty) },
      "", Before)->setDebugLoc(Before->getDebugLoc());
    CallInst::Create(
      RealMemset,
      { LAD.Aux, ConstantInt::get(Int8Ty, 0), ConstantInt::get(IntPtrTy, LAD.Size),
        ConstantInt::getFalse(Int1Ty) },
      "", Before)->setDebugLoc(Before->getDebugLoc());
  }

  bool earlyLowerInstruction(Instruction* I) {
    if (verbose)
      errs() << "Early lowering: " << *I << "\n";

    if (IntrinsicInst* II = dyn_cast<IntrinsicInst>(I)) {
      if (verbose)
        errs() << "It's an intrinsic.\n";
      switch (II->getIntrinsicID()) {
      case Intrinsic::memset:
      case Intrinsic::memset_inline: {
        lowerConstantOperand(II->getArgOperandUse(0), I);
        lowerConstantOperand(II->getArgOperandUse(1), I);
        lowerConstantOperand(II->getArgOperandUse(2), I);
        Instruction* CI = CallInst::Create(
          Memset,
          { MyThread, II->getArgOperand(0), castInt(II->getArgOperand(1), Int32Ty, II),
            makeIntPtr(II->getArgOperand(2), II), getOrigin(II->getDebugLoc()) });
        ReplaceInstWithInst(II, CI);
        return true;
      }
      case Intrinsic::memcpy:
      case Intrinsic::memcpy_inline:
      case Intrinsic::memmove: {
        lowerMemmoveCall(I);
        II->eraseFromParent();
        return true;
      }

      case Intrinsic::lifetime_start:
      case Intrinsic::lifetime_end: {
        AllocaInst* AI = cast<AllocaInst>(II->getArgOperand(1));
        assert(LocalAllocaDatas.count(AI));
        LocalAllocaData LAD = LocalAllocaDatas[AI];
        FrameEntry FE;
        if (LAD.Explicit) {
          assert(FrameIndexMap.count(ValuePtr(AI, 0)));
          FE = FrameIndexMap[ValuePtr(AI, 0)];
          assert(FE.FEK == FrameEntryKind::ExplicitStackAux);
          assert(FE.Index != SIZE_MAX);
          if (verbose)
            errs() << "Using frame index " << FE.Index << "\n";
        }
        if (LAD.Explicit && II->getIntrinsicID() == Intrinsic::lifetime_end)
          recordLowerAtIndex(RawNull, FE.Index, II);
        CallInst::Create(
          II->getFunctionType(), II->getCalledOperand(),
          { ConstantInt::get(IntPtrTy, LAD.Size), LAD.Payload },
          "", II)->setDebugLoc(II->getDebugLoc());
        CallInst::Create(
          II->getFunctionType(), II->getCalledOperand(),
          { ConstantInt::get(IntPtrTy, LAD.Size), LAD.AuxAlloca },
          "", II)->setDebugLoc(II->getDebugLoc());
        if (II->getIntrinsicID() == Intrinsic::lifetime_start)
          initializeNonescapingAlloca(LAD, II);
        II->eraseFromParent();
        return true;
      }
        
      case Intrinsic::stacksave:
      case Intrinsic::stackrestore:
      case Intrinsic::assume:
      case Intrinsic::experimental_noalias_scope_decl:
      case Intrinsic::donothing:
      case Intrinsic::dbg_declare:
      case Intrinsic::dbg_value:
      case Intrinsic::dbg_assign:
      case Intrinsic::dbg_label:
      case Intrinsic::invariant_start:
      case Intrinsic::invariant_end:
      case Intrinsic::launder_invariant_group:
      case Intrinsic::strip_invariant_group:
        llvm_unreachable("Should have already been erased");
        return true;

      case Intrinsic::vastart:
        assert(UsesVastartOrZargs);
        lowerConstantOperand(II->getArgOperandUse(0), I);
        storePtr(SnapshottedArgsPtrForVastart,
                 accessDataForOperand(II->getArgOperand(0), II, 0, II).MAD, II);
        II->eraseFromParent();
        return true;
        
      case Intrinsic::vacopy: {
        lowerConstantOperand(II->getArgOperandUse(0), I);
        lowerConstantOperand(II->getArgOperandUse(1), I);
        Value* Load = loadPtr(accessDataForOperand(II->getArgOperand(1), II, 1, II).MAD, II);
        storePtr(Load, accessDataForOperand(II->getArgOperand(0), II, 0, II).MAD, II);
        II->eraseFromParent();
        return true;
      }
        
      case Intrinsic::vaend:
        II->eraseFromParent();
        return true;

      case Intrinsic::eh_typeid_for: {
        Constant* TypeConstant = cast<Constant>(II->getArgOperand(0));
        assert(EHTypeIDs.count(TypeConstant));
        II->replaceAllUsesWith(ConstantInt::get(Int32Ty, EHTypeIDs[TypeConstant]));
        II->eraseFromParent();
        return true;
      }

      case Intrinsic::eh_sjlj_functioncontext:
      case Intrinsic::eh_sjlj_setjmp:
      case Intrinsic::eh_sjlj_longjmp:
      case Intrinsic::eh_sjlj_setup_dispatch:
        llvm_unreachable("Cannot use sjlj intrinsics.");
        return true;

      case Intrinsic::x86_xgetbv:
      case Intrinsic::x86_sse2_pause:
      case Intrinsic::x86_rdtsc:
        return true;

      case Intrinsic::returnaddress:
      case Intrinsic::frameaddress: {
        lowerConstantOperand(II->getArgOperandUse(0), I);
        Instruction* IsZero = new ICmpInst(
          I, ICmpInst::ICMP_EQ, II->getArgOperand(0), ConstantInt::get(Int32Ty, 0),
          "filc_frameaddress_arg_is_zero");
        IsZero->setDebugLoc(I->getDebugLoc());
        Instruction* NotZeroTerm = SplitBlockAndInsertIfElse(IsZero, I, true);
        std::string str;
        raw_string_ostream outs(str);
        outs << "cannot use ";
        switch (II->getIntrinsicID()) {
        case Intrinsic::returnaddress:
          outs << "__builtin_return_address";
          break;
        case Intrinsic::frameaddress:
          outs << "__builtin_frame_address";
          break;
        default:
          llvm_unreachable("Unexpected intrinsic");
          break;
        }
        outs << " with nonzero argument.";
        CallInst::Create(Error, { getString(str), getOrigin(I->getDebugLoc()) }, "", NotZeroTerm)
          ->setDebugLoc(I->getDebugLoc());
        switch (II->getIntrinsicID()) {
        case Intrinsic::returnaddress:
          hackRAUW(I, [&] () { return badFlightPtr(I, I->getNextNode()); });
          break;
        case Intrinsic::frameaddress:
          assert(Frame);
          I->replaceAllUsesWith(badFlightPtr(Frame, I));
          I->eraseFromParent();
          break;
        default:
          llvm_unreachable("Unexpected intrinsic");
          break;
        }
        return true;
      }

      case Intrinsic::threadlocal_address: {
        lowerConstantOperand(II->getArgOperandUse(0), II);
        II->replaceAllUsesWith(II->getArgOperand(0));
        II->eraseFromParent();
        return true;
      }

      case Intrinsic::trap: {
        CallInst::Create(
          Error, { getString("llvm trap intrinsic"), getOrigin(I->getDebugLoc()) }, "", I)
          ->setDebugLoc(I->getDebugLoc());
        return true;
      }

      default: {
        for (Use& U : II->data_ops())
          lowerConstantOperand(U, I);

        IntrinsicAccessDetails IAD = analyzeIntrinsicLoadStore(II);
        
        if (!IAD
            && !II->getCalledFunction()->doesNotAccessMemory()
            && !isa<ConstrainedFPIntrinsic>(II)
            && II->getIntrinsicID() != Intrinsic::prefetch
            && II->getIntrinsicID() != Intrinsic::get_rounding
            && II->getIntrinsicID() != Intrinsic::set_rounding
            && II->getIntrinsicID() != Intrinsic::x86_sse_sfence) {
          if (verbose)
            llvm::errs() << "Unhandled intrinsic: " << *II << "\n";
          std::string str;
          raw_string_ostream outs(str);
          outs << "Unhandled intrinsic: " << *II;
          CallInst::Create(Error, { getString(str), getOrigin(I->getDebugLoc()) }, "", II)
            ->setDebugLoc(II->getDebugLoc());
        }

        for (Use& U : II->data_ops()) {
          if (hasPtrs(U->getType()))
            U = flightPtrPtr(U, II);
        }

        if (IAD)
          lowerIntrinsicAccess(II, IAD);
        
        if (hasPtrs(II->getType()))
          hackRAUW(II, [&] () { return badFlightPtr(II, II->getNextNode()); });
        return true;
      } }
    }
    
    if (CallBase* CI = dyn_cast<CallBase>(I)) {
      if (verbose) {
        errs() << "It's a call!\n";
        errs() << "Callee = " << CI->getCalledOperand() << "\n";
        if (CI->getCalledOperand())
          errs() << "Callee name = " << CI->getCalledOperand()->getName() << "\n";
      }

      if (Function* F = dyn_cast<Function>(CI->getCalledOperand())) {
        FunctionType* FT = CI->getFunctionType();

        auto Erasify = [&] () {
          if (InvokeInst* II = dyn_cast<InvokeInst>(CI)) {
            BranchInst::Create(II->getNormalDest(), CI)->setDebugLoc(CI->getDebugLoc());
            II->getUnwindDest()->removePredecessor(II->getParent(), /*KeepOneInputPHIs=*/true);
          }
          CI->eraseFromParent();
        };
        
        if (F->isIntrinsic() &&
            F->getIntrinsicID() == Intrinsic::donothing) {
          llvm_unreachable("Should not see donothing intrinsic here.");
          return true;
        }
        
        if (isSetjmp(F)) {
          if (verbose)
            errs() << "Lowering some kind of setjmp\n";
          for (Use& Arg : CI->args())
            lowerConstantOperand(Arg, CI);
          assert(FT == F->getFunctionType());
          assert(CI->hasFnAttr(Attribute::ReturnsTwice));
          assert(HasSetjmps);
          assert(SetjmpSetFrameIndex != SIZE_MAX);
          Value* ValueArg;
          if (F->getName() == "sigsetjmp") {
            assert(CI->arg_size() == 2);
            ValueArg = CI->getArgOperand(1);
          } else {
            assert(CI->arg_size() == 1);
            ValueArg = ConstantInt::get(Int32Ty, 0);
          }
          storeOrigin(getOrigin(CI->getDebugLoc()), CI);
          CallInst* Create = CallInst::Create(
            JmpBufCreate,
            { MyThread, ConstantInt::get(Int32Ty, static_cast<int>(getJmpBufKindForSetjmp(F))),
              ValueArg },
            "filc_jmp_buf_create", CI);
          Create->setDebugLoc(CI->getDebugLoc());
          Value* UserJmpBuf = CI->getArgOperand(0);
          storePtr(flightPtrForPayload(Create, CI), accessDataForOperand(UserJmpBuf, CI, 0, CI).MAD,
                   CI);
          Instruction* Set = new LoadInst(
            RawPtrTy, recordedLowerPtrAtIndex(SetjmpSetFrameIndex, CI), "filc_setjmp_set", CI);
          Set->setDebugLoc(CI->getDebugLoc());
          CallInst::Create(
            WeakMapSet,
            { MyThread, Set, flightPtrForPayload(Create, CI),
              createFlightPtr(
                RawNull,
                ConstantExpr::getIntToPtr(ConstantInt::get(IntPtrTy, 1), RawPtrTy),
                CI) },
            "", CI)->setDebugLoc(CI->getDebugLoc());
          CallInst* NewCI = CallInst::Create(_Setjmp, { Create }, "filc_setjmp", CI);
          CI->replaceAllUsesWith(NewCI);
          NewCI->setDebugLoc(CI->getDebugLoc());
          Erasify();
          return true;
        }

        if (F->getName() == "zargs" &&
            !FT->getNumParams() &&
            FT->getReturnType() == RawPtrTy) {
          assert(UsesVastartOrZargs);
          CI->replaceAllUsesWith(SnapshottedArgsPtrForZargs);
          Erasify();
          return true;
        }

        if (F->getName() == "zreturn" &&
            FT->getNumParams() == 1 &&
            FT->getReturnType() == VoidTy &&
            FT->getParamType(0) == RawPtrTy) {
          assert(UsesVariadicCC);
          assert(RetSizePhi);
          assert(ReallyReturnB);
          Instruction* Prepare = CallInst::Create(
            PrepareToReturnWithData, { MyThread, CI->getArgOperand(0), getOrigin(CI->getDebugLoc()) },
            "filc_prepare_to_return_with_data", CI);
          Prepare->setDebugLoc(CI->getDebugLoc());
          RetSizePhi->addIncoming(Prepare, Prepare->getParent());
          BasicBlock* OriginalB = CI->getParent();
          SplitBlock(OriginalB, CI);
          cast<BranchInst>(OriginalB->getTerminator())->setSuccessor(0, ReallyReturnB);
          Erasify();
          return true;
        }

        if ((((F->getName() == "zunsafe_call" || F->getName() == "zunsafe_fast_call") &&
              FT->getNumParams() == 1 &&
              FT->getParamType(0) == RawPtrTy) ||
             (F->getName() == "zunsafe_buf_call" &&
              FT->getNumParams() == 2 &&
              FT->getParamType(0) == IntPtrTy &&
              FT->getParamType(1) == RawPtrTy)) &&
            FT->isVarArg() &&
            FT->getReturnType() == IntPtrTy) {
          Value* BufSize;
          unsigned FirstArg;
          if (F->getName() == "zunsafe_buf_call") {
            FirstArg = 1;
            BufSize = lowerConstantValue(CI->getArgOperand(0), CI);
          } else {
            FirstArg = 0;
            if (F->getName() == "zunsafe_call")
              BufSize = ConstantInt::get(IntPtrTy, MaxBytesBetweenPollchecks + 1);
            else
              BufSize = ConstantInt::get(IntPtrTy, 0);
          }
          GlobalVariable* StrConstGV = cast<GlobalVariable>(CI->getArgOperand(FirstArg));
          assert(StrConstGV->hasInitializer());
          ConstantDataSequential* StrConstCDS =
            cast<ConstantDataSequential>(StrConstGV->getInitializer());
          assert(StrConstCDS->isCString());
          std::string StrConst = StrConstCDS->getAsCString().str();
          BitCastInst* Dummy;
          if (UnsafeFuncs.count(StrConst))
            Dummy = UnsafeFuncs[StrConst];
          else {
            Dummy = makeDummy(RawPtrTy);
            UnsafeFuncs[StrConst] = Dummy;
          }
          std::vector<Value*> Args;
          assert(InstTypeVectors.count(CI));
          std::vector<Type*> ArgTypes = InstTypeVectors[CI];
          assert(ArgTypes.size() == CI->arg_size());
          for (unsigned Idx = FirstArg + 1; Idx < CI->arg_size(); ++Idx) {
            Value* Arg = lowerConstantValue(CI->getArgOperand(Idx), CI);
            if (ArgTypes[Idx] == RawPtrTy) {
              Args.push_back(flightPtrPtr(Arg, CI));
              continue;
            }
            assert(!hasPtrs(ArgTypes[Idx]));
            Args.push_back(Arg);
          }
          Instruction* IsSmallEnough = new ICmpInst(
            CI, ICmpInst::ICMP_ULE, BufSize, ConstantInt::get(IntPtrTy, MaxBytesBetweenPollchecks),
            "filc_is_small_enough");
          IsSmallEnough->setDebugLoc(CI->getDebugLoc());
          Instruction* LargeTerm = SplitBlockAndInsertIfElse(IsSmallEnough, CI, false);
          storeOrigin(getOrigin(CI->getDebugLoc()), LargeTerm);
          CallInst::Create(Exit, { MyThread }, "", LargeTerm)->setDebugLoc(CI->getDebugLoc());
          CallInst* Result = CallInst::Create(UnsafeFuncTy, Dummy, Args, "filc_unsafe_call", CI);
          Result->setDebugLoc(CI->getDebugLoc());
          LargeTerm = SplitBlockAndInsertIfElse(IsSmallEnough, CI, false);
          CallInst::Create(Enter, { MyThread }, "", LargeTerm)->setDebugLoc(CI->getDebugLoc());
          CI->replaceAllUsesWith(Result);
          Erasify();
          return true;
        }

        if (F->getName() == "zcallee" &&
            !FT->getNumParams() &&
            FT->getReturnType() == RawPtrTy) {
          Value* CalleeLower = NewF->getArg(1);
          CI->replaceAllUsesWith(createFlightPtr(CalleeLower, CalleeLower, CI));
          Erasify();
          return true;
        }

        if (F->getName() == "zcallee_closure_data" &&
            !FT->getNumParams() &&
            FT->getReturnType() == RawPtrTy) {
          Value* CalleeLower = NewF->getArg(1);
          BinaryOperator* Masked = BinaryOperator::Create(
            Instruction::And, flagsForLower(CalleeLower, CI),
            ConstantInt::get(IntPtrTy, ObjectFlagReadonly), "filc_flags_masked", CI);
          Masked->setDebugLoc(CI->getDebugLoc());
          ICmpInst* IsClosure = new ICmpInst(
            CI, ICmpInst::ICMP_EQ, Masked, ConstantInt::get(IntPtrTy, 0),
            "filc_object_is_not_closure");
          Instruction* FailTerm = SplitBlockAndInsertIfElse(expectTrue(IsClosure, CI), CI, true);
          CallInst::Create(
            CheckClosureFail, { CalleeLower, getOrigin(CI->getDebugLoc()) }, "", FailTerm)
            ->setDebugLoc(CI->getDebugLoc());
          Instruction* DataPtr = GetElementPtrInst::Create(
            ClosureTy, CalleeLower, { ConstantInt::get(Int32Ty, 0), ConstantInt::get(Int32Ty, 2) },
            "filc_closure_data_ptr_ptr", CI);
          CI->replaceAllUsesWith(loadFlightPtr(DataPtr, CI));
          Erasify();
          return true;
        }

        if ((F->getName() == "zgetlower" || F->getName() == "zgetupper") &&
            FT->getNumParams() == 1 &&
            !FT->isVarArg() &&
            FT->getParamType(0) == RawPtrTy &&
            FT->getReturnType() == RawPtrTy) {
          lowerConstantOperand(CI->getArgOperandUse(0), CI);
          Value* Lower = flightPtrLower(CI->getArgOperand(0), CI);
          ICmpInst* NullLower = new ICmpInst(
            CI, ICmpInst::ICMP_EQ, Lower, RawNull, "filc_is_null_lower");
          NullLower->setDebugLoc(CI->getDebugLoc());
          Instruction* NotNullTerm = SplitBlockAndInsertIfElse(NullLower, CI, false);
          Value* LowerUpper;
          if (F->getName() == "zgetlower")
            LowerUpper = Lower;
          else {
            assert(F->getName() == "zgetupper");
            LowerUpper = upperForLower(Lower, NotNullTerm);
          }
          PHINode* Phi = PHINode::Create(RawPtrTy, 2, "filc_lower_upper_phi", CI);
          Phi->addIncoming(RawNull, NullLower->getParent());
          Phi->addIncoming(LowerUpper, NotNullTerm->getParent());
          CI->replaceAllUsesWith(flightPtrWithPtr(CI->getArgOperand(0), Phi, CI));
          Erasify();
          return true;
        }

        if (F->getName() == "zis_readonly" &&
            FT->getNumParams() == 1 &&
            !FT->isVarArg() &&
            FT->getParamType(0) == RawPtrTy &&
            FT->getReturnType() == Int1Ty) {
          lowerConstantOperand(CI->getArgOperandUse(0), CI);
          Value* Lower = flightPtrLower(CI->getArgOperand(0), CI);
          ICmpInst* NullLower = new ICmpInst(
            CI, ICmpInst::ICMP_EQ, Lower, RawNull, "filc_is_null_lower");
          NullLower->setDebugLoc(CI->getDebugLoc());
          Instruction* NotNullTerm = SplitBlockAndInsertIfElse(NullLower, CI, false);
          BinaryOperator* Masked = BinaryOperator::Create(
            Instruction::And, flagsForLower(Lower, NotNullTerm),
            ConstantInt::get(IntPtrTy, ObjectFlagReadonly),
            "filc_flags_masked", NotNullTerm);
          Masked->setDebugLoc(CI->getDebugLoc());
          ICmpInst* IsReadOnly = new ICmpInst(
            NotNullTerm, ICmpInst::ICMP_NE, Masked, ConstantInt::get(IntPtrTy, 0),
            "filc_object_is_read_only");
          IsReadOnly->setDebugLoc(CI->getDebugLoc());
          PHINode* Phi = PHINode::Create(Int1Ty, 2, "filc_readonly_phi", CI);
          Phi->addIncoming(ConstantInt::getFalse(Int1Ty), NullLower->getParent());
          Phi->addIncoming(IsReadOnly, NotNullTerm->getParent());
          CI->replaceAllUsesWith(Phi);
          Erasify();
          return true;
        }
        
        if (F->getName() == "zthread_self_id" &&
            FT->getNumParams() == 0 &&
            !FT->isVarArg() &&
            FT->getReturnType() == Int32Ty) {
          Value* TidPtr = threadTIDPtr(MyThread, CI);
          Instruction* Tid = new LoadInst(Int32Ty, TidPtr, "filc_tid", CI);
          Tid->setDebugLoc(CI->getDebugLoc());
          CI->replaceAllUsesWith(Tid);
          Erasify();
          return true;
        }

        if (F->getName() == "zthread_self" &&
            FT->getNumParams() == 0 &&
            !FT->isVarArg() &&
            FT->getReturnType() == RawPtrTy) {
          CI->replaceAllUsesWith(flightPtrForPayload(MyThread, CI));
          Erasify();
          return true;
        }

        if (F->getName() == "zthread_self_cookie" &&
            FT->getNumParams() == 0 &&
            !FT->isVarArg() &&
            FT->getReturnType() == RawPtrTy) {
          Value* CookiePtr = threadCookiePtrPtr(MyThread, CI);
          Instruction* Cookie = new LoadInst(FlightPtrTy, CookiePtr, "filc_cookie", CI);
          Cookie->setDebugLoc(CI->getDebugLoc());
          CI->replaceAllUsesWith(Cookie);
          Erasify();
          return true;
        }

        if ((F->getName() == "zmemmove_union" || F->getName() == "zmemmove_builtin")
            && isMemmoveFT(FT)) {
          lowerMemmoveCall(CI);
          Erasify();
          return true;
        }

        if (F->getName() == "zhas_union"
            && isHasUnionFT(FT)) {
          Erasify();
          return true;
        }

        if ((F->getName() == "zgc_alloc" || F->getName() == "malloc") &&
            FT->getNumParams() == 1 &&
            !FT->isVarArg() &&
            FT->getReturnType() == RawPtrTy &&
            FT->getParamType(0) == IntPtrTy) {
          lowerConstantOperand(CI->getArgOperandUse(0), CI);
          CI->replaceAllUsesWith(allocate(CI->getArgOperand(0), ConstantInt::get(IntPtrTy, 1), CI));
          Erasify();
          return true;
        }

        if ((F->getName() == "zgc_free" || F->getName() == "free") &&
            FT->getNumParams() == 1 &&
            !FT->isVarArg() &&
            FT->getReturnType() == VoidTy &&
            FT->getParamType(0) == RawPtrTy) {
          lowerConstantOperand(CI->getArgOperandUse(0), CI);
          CallInst* Free = CallInst::Create(FreeWithChecks, { CI->getArgOperand(0) }, "", CI);
          Free->setDebugLoc(CI->getDebugLoc());
          Erasify();
          return true;
        }

        if (F->getName() == "zstack_pointer" &&
            FT->getNumParams() == 0 &&
            !FT->isVarArg() &&
            FT->getReturnType() == RawPtrTy) {
          CallInst* RawStackAddr = CallInst::Create(
            InlineAsm::get(
              FunctionType::get(RawPtrTy, { }, false),
              "mov %rsp, $0",
              "=r,~{dirflag},~{fpsr},~{flags}",
              /*hasSideEffects=*/true),
            { }, "", CI);
          RawStackAddr->setDebugLoc(CI->getDebugLoc());
          CI->replaceAllUsesWith(badFlightPtr(RawStackAddr, CI));
          Erasify();
          return true;
        }
        
        if (shouldPassThrough(F)) {
          for (Use& Arg : CI->args())
            lowerConstantOperand(Arg, CI);
          return true;
        }
      }
    }
    
    if (isa<LandingPadInst>(I)) {
      CallInst* CI = CallInst::Create(LandingPad, { MyThread }, "filc_landing_pad", I);
      CI->setDebugLoc(I->getDebugLoc());
      SplitBlockAndInsertIfElse(CI, I, false, nullptr, nullptr, nullptr, ResumeB);
      StructType* ST = cast<StructType>(I->getType());
      assert(!ST->isOpaque());
      assert(ST->getNumElements() <= NumUnwindRegisters);
      Value* Result = UndefValue::get(toFlightType(ST));
      for (unsigned Idx = ST->getNumElements(); Idx--;) {
        Value* UnwindRegisterPtr = threadUnwindRegisterPtr(MyThread, Idx, I);
        LoadInst* RawUnwindRegister = new LoadInst(
          FlightPtrTy, UnwindRegisterPtr, "filc_load_unwind_register", I);
        (new StoreInst(FlightNull, UnwindRegisterPtr, I))->setDebugLoc(I->getDebugLoc());
        Value* UnwindRegister;
        if (ST->getElementType(Idx) == RawPtrTy)
          UnwindRegister = RawUnwindRegister;
        else {
          assert(isa<IntegerType>(ST->getElementType(Idx)));
          Instruction* Trunc = new TruncInst(
            flightPtrPtrAsInt(RawUnwindRegister, I), ST->getElementType(Idx),
            "filc_trunc_unwind_register", I);
          Trunc->setDebugLoc(I->getDebugLoc());
          UnwindRegister = Trunc;
        }
        Instruction* InsertValue = InsertValueInst::Create(
          Result, UnwindRegister, { Idx }, "filc_insert_unwind_register", I);
        InsertValue->setDebugLoc(I->getDebugLoc());
        Result = InsertValue;
      }
      I->replaceAllUsesWith(Result);
      I->removeFromParent();
      ToErase.push_back(I);
      return true;
    }

    return false;
  }

  bool validateSafeInlineAsm(CallBase* CI, InlineAsm* IA, std::string& Reason) {
    // Whitespace helpers.
    auto isSpace = [](char c) -> bool {
      return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
    };
    auto trim = [&](StringRef s) -> std::string {
      size_t start = 0;
      while (start < s.size() && isSpace(s[start]))
        ++start;
      size_t end = s.size();
      while (end > start && isSpace(s[end - 1]))
        --end;
      return s.substr(start, end - start).str();
    };
    auto toLowerStr = [&](StringRef s) -> std::string {
      std::string r = s.str();
      for (char& c : r)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      return r;
    };

    // Map a register name to its family (ax, cx, r8, ...).
    auto getRegFamily = [&](StringRef reg) -> std::string {
      std::string r = toLowerStr(reg);
      static const std::unordered_map<std::string, std::string> familyMap = []{
        std::unordered_map<std::string, std::string> m;
        m["al"] = "ax"; m["ah"] = "ax"; m["ax"] = "ax"; m["eax"] = "ax"; m["rax"] = "ax";
        m["bl"] = "bx"; m["bh"] = "bx"; m["bx"] = "bx"; m["ebx"] = "bx"; m["rbx"] = "bx";
        m["cl"] = "cx"; m["ch"] = "cx"; m["cx"] = "cx"; m["ecx"] = "cx"; m["rcx"] = "cx";
        m["dl"] = "dx"; m["dh"] = "dx"; m["dx"] = "dx"; m["edx"] = "dx"; m["rdx"] = "dx";
        m["sil"] = "si"; m["si"] = "si"; m["esi"] = "si"; m["rsi"] = "si";
        m["dil"] = "di"; m["di"] = "di"; m["edi"] = "di"; m["rdi"] = "di";
        m["bpl"] = "bp"; m["bp"] = "bp"; m["ebp"] = "bp"; m["rbp"] = "bp";
        m["spl"] = "sp"; m["sp"] = "sp"; m["esp"] = "sp"; m["rsp"] = "sp";
        for (int i = 8; i <= 15; ++i) {
          std::string base = "r" + std::to_string(i);
          m[base] = base;
          m[base + "b"] = base;
          m[base + "w"] = base;
          m[base + "d"] = base;
          m[base + "l"] = base;
        }
        return m;
      }();
      auto it = familyMap.find(r);
      if (it != familyMap.end())
        return it->second;
      return "";
    };

    // Parse the constraint string.
    struct ParsedConstraint {
      enum Kind { Clobber, Output, Input } Kind;
      std::string String;
      bool IsRegister = false;
      std::string Family; // non-empty for fixed-register constraints
      int MatchingTarget = -1;
    };

    std::vector<ParsedConstraint> Constraints;
    bool HasCCClobber = false;

    std::string ConstraintStr = IA->getConstraintString();
    for (size_t idx = 0, end = ConstraintStr.size(); idx < end; ) {
      size_t comma = ConstraintStr.find(',', idx);
      if (comma == std::string::npos)
        comma = end;
      std::string cstr = trim(ConstraintStr.substr(idx, comma - idx));
      if (!cstr.empty()) {
        ParsedConstraint pc;
        pc.String = cstr;
        pc.Kind = ParsedConstraint::Input;
        pc.IsRegister = false;
        pc.MatchingTarget = -1;

        if (cstr[0] == '~') {
          pc.Kind = ParsedConstraint::Clobber;
          if (cstr.size() < 3 || cstr[1] != '{' || cstr.back() != '}') {
            Reason = "malformed clobber constraint: " + cstr;
            return false;
          }
          std::string name = toLowerStr(cstr.substr(2, cstr.size() - 3));
          if (name == "cc")
            HasCCClobber = true;
          else if (name == "memory" || name == "dirflag" || name == "fpsr" ||
                   name == "flags" || !getRegFamily(name).empty()) {
            // memory and fixed-register clobbers are allowed.
          } else {
            Reason = "unsupported clobber for safe inline asm: " + cstr;
            return false;
          }
        } else {
          size_t pos = 0;
          bool isOutput = false;
          while (pos < cstr.size()) {
            if (cstr[pos] == '=') {
              isOutput = true;
              ++pos;
            } else if (cstr[pos] == '&' || cstr[pos] == '%') {
              ++pos;
            } else if (cstr[pos] == '+') {
              isOutput = true;
              ++pos;
            } else {
              break;
            }
          }
          if (pos >= cstr.size()) {
            Reason = "empty constraint after prefixes: " + cstr;
            return false;
          }
          if (cstr[pos] == '*') {
            Reason = "indirect constraint not allowed in safe inline asm: " + cstr;
            return false;
          }
          pc.Kind = isOutput ? ParsedConstraint::Output : ParsedConstraint::Input;
          std::string rest = cstr.substr(pos);
          if (rest == "r") {
            pc.IsRegister = true;
          } else if (rest.size() >= 2 && rest.front() == '{' && rest.back() == '}') {
            std::string regname = toLowerStr(rest.substr(1, rest.size() - 2));
            std::string family = getRegFamily(regname);
            if (family.empty()) {
              Reason = "unknown fixed register constraint: " + cstr;
              return false;
            }
            pc.IsRegister = true;
            pc.Family = family;
          } else {
            bool allDigits = true;
            for (char c : rest) {
              if (!std::isdigit(static_cast<unsigned char>(c))) {
                allDigits = false;
                break;
              }
            }
            if (allDigits && !rest.empty()) {
              int target = 0;
              for (char c : rest)
                target = target * 10 + (c - '0');
              pc.MatchingTarget = target;
            } else {
              Reason = "unsupported constraint for safe inline asm: " + cstr;
              return false;
            }
          }
        }
        Constraints.push_back(pc);
      }
      idx = comma + 1;
    }

    // Resolve matching constraints.
    for (auto& pc : Constraints) {
      if (pc.MatchingTarget < 0)
        continue;
      size_t target = static_cast<size_t>(pc.MatchingTarget);
      if (target >= Constraints.size()) {
        Reason = "matching constraint out of range: " + pc.String;
        return false;
      }
      const ParsedConstraint& targetPc = Constraints[target];
      if (targetPc.Kind != ParsedConstraint::Output) {
        Reason = "matching constraint does not point at output: " + pc.String;
        return false;
      }
      if (!targetPc.IsRegister) {
        Reason = "matching constraint points at non-register output: " + pc.String;
        return false;
      }
      pc.IsRegister = true;
      pc.Family = targetPc.Family;
    }

    // Build sets of register families covered by input/output constraints.
    std::unordered_set<std::string> InputFamilies;
    std::unordered_set<std::string> OutputFamilies;
    for (const auto& pc : Constraints) {
      if (!pc.IsRegister)
        continue;
      if (pc.Kind == ParsedConstraint::Input && !pc.Family.empty())
        InputFamilies.insert(pc.Family);
      else if (pc.Kind == ParsedConstraint::Output && !pc.Family.empty())
        OutputFamilies.insert(pc.Family);
    }

    if (verbose) {
      errs() << "Safe inline asm constraint analysis:\n";
      errs() << "  Input families:";
      for (const auto& f : InputFamilies)
        errs() << " " << f;
      errs() << "\n";
      errs() << "  Output families:";
      for (const auto& f : OutputFamilies)
        errs() << " " << f;
      errs() << "\n";
      errs() << "  Has cc clobber: " << HasCCClobber << "\n";
    }

    enum OperandRole { RoleInput, RoleOutput, RoleBoth };

    auto parseMnemonic = [&](const std::string& mnem, std::string& baseMnemonic,
                             bool& setsFlags, std::vector<OperandRole>& roles) -> bool {
      StringRef m(mnem);

      if (m.starts_with("cmov")) {
        StringRef suffix = m.drop_front(4);
        if (suffix.empty())
          return false;
        static const std::unordered_set<std::string> conds = {
          "o", "no",
          "b", "c", "nae", "nb", "nc", "ae",
          "e", "z", "ne", "nz",
          "be", "na", "nbe", "a",
          "s", "ns",
          "p", "pe", "np", "po",
          "l", "nge", "nl", "ge",
          "le", "ng", "nle", "g"
        };
        StringRef cond = suffix;
        if (suffix.size() >= 2 && (suffix.back() == 'b' || suffix.back() == 'w' ||
                                   suffix.back() == 'l' || suffix.back() == 'q')) {
          StringRef maybeCond = suffix.drop_back();
          if (conds.count(maybeCond.str()))
            cond = maybeCond;
        }
        if (!conds.count(cond.str()))
          return false;
        baseMnemonic = "cmov";
        setsFlags = false;
        roles = {RoleInput, RoleOutput};
        return true;
      }

      if (m == "cpuid" || m == "xgetbv") {
        baseMnemonic = mnem;
        setsFlags = false;
        roles.clear();
        return true;
      }

      StringRef base = m;
      if (!base.empty() && (base.back() == 'b' || base.back() == 'w' ||
                            base.back() == 'l' || base.back() == 'q'))
        base = base.drop_back();

      static const std::unordered_map<std::string, std::pair<bool, std::vector<OperandRole>>> info = {
        {"sar", {true, {RoleInput, RoleBoth}}},
        {"shr", {true, {RoleInput, RoleBoth}}},
        {"and", {true, {RoleInput, RoleBoth}}},
        {"shl", {true, {RoleInput, RoleBoth}}},
        {"xor", {true, {RoleInput, RoleBoth}}},
        {"mov", {false, {RoleInput, RoleOutput}}},
        {"test", {true, {RoleInput, RoleInput}}},
        {"cmp", {true, {RoleInput, RoleInput}}},
        {"bsf", {true, {RoleInput, RoleOutput}}},
      };
      auto it = info.find(base.str());
      if (it == info.end())
        return false;
      baseMnemonic = base.str();
      setsFlags = it->second.first;
      roles = it->second.second;
      return true;
    };

    // Split asm string into lines on \n and ;.
    std::string AsmStr = IA->getAsmString();
    std::vector<std::string> lines;
    {
      std::string cur;
      for (char c : AsmStr) {
        if (c == '\n' || c == ';') {
          lines.push_back(trim(cur));
          cur.clear();
        } else
          cur += c;
      }
      lines.push_back(trim(cur));
    }

    enum OperandKind { OKReg, OKPlaceholder, OKImmediate, OKMemory };
    auto classifyOperand = [&](const std::string& op, int& placeholderIndex,
                               std::string& family,
                               size_t numConstraints) -> OperandKind {
      placeholderIndex = -1;
      family.clear();
      if (op.empty())
        return OKMemory;
      if (op[0] == '%') {
        StringRef reg = StringRef(op).drop_front(1);
        family = getRegFamily(reg);
        if (family.empty())
          return OKMemory;
        return OKReg;
      }
      if (op[0] == '$') {
        if (op.size() >= 2 && op[1] == '$')
          return OKImmediate;
        size_t pos = 1;
        if (pos < op.size() && op[pos] == '{') {
          size_t end = op.find('}', pos);
          if (end == std::string::npos)
            return OKImmediate;
          std::string inner = op.substr(pos + 1, end - pos - 1);
          size_t colon = inner.find(':');
          if (colon != std::string::npos)
            inner = inner.substr(0, colon);
          if (inner.empty())
            return OKImmediate;
          for (char c : inner) {
            if (!std::isdigit(static_cast<unsigned char>(c)))
              return OKImmediate;
          }
          int idx = 0;
          for (char c : inner)
            idx = idx * 10 + (c - '0');
          placeholderIndex = idx;
        } else if (pos < op.size() && std::isdigit(static_cast<unsigned char>(op[pos]))) {
          size_t numStart = pos;
          while (pos < op.size() && std::isdigit(static_cast<unsigned char>(op[pos])))
            ++pos;
          if (pos != op.size())
            return OKImmediate;
          int idx = 0;
          for (size_t i = numStart; i < pos; ++i)
            idx = idx * 10 + (op[i] - '0');
          if (static_cast<size_t>(idx) >= numConstraints)
            return OKImmediate;
          placeholderIndex = idx;
        } else {
          return OKImmediate;
        }
        return OKPlaceholder;
      }
      return OKMemory;
    };

    bool AnySetsFlags = false;

    for (const std::string& rawLine : lines) {
      std::string line = trim(rawLine);
      if (line.empty())
        continue;

      size_t sp = line.find_first_of(" \t\r\v\f");
      std::string mnemonic = toLowerStr(trim(line.substr(0, sp)));
      std::string rest = (sp == std::string::npos) ? "" : trim(line.substr(sp));

      if (mnemonic.empty()) {
        Reason = "missing mnemonic in asm line";
        return false;
      }

      std::string baseMnemonic;
      bool setsFlags = false;
      std::vector<OperandRole> roles;
      if (!parseMnemonic(mnemonic, baseMnemonic, setsFlags, roles)) {
        Reason = "unsupported mnemonic for safe inline asm: " + mnemonic;
        return false;
      }

      std::vector<std::string> operands;
      {
        std::string cur;
        for (char c : rest) {
          if (c == ',') {
            operands.push_back(trim(cur));
            cur.clear();
          } else
            cur += c;
        }
        operands.push_back(trim(cur));
      }
      while (!operands.empty() && operands.back().empty())
        operands.pop_back();

      if (baseMnemonic == "cpuid") {
        if (!operands.empty()) {
          Reason = "cpuid takes no operands";
          return false;
        }
        if (!InputFamilies.count("ax")) {
          Reason = "cpuid input eax not covered by input constraint";
          return false;
        }
        if (!InputFamilies.count("cx")) {
          Reason = "cpuid input ecx not covered by input constraint";
          return false;
        }
        if (!OutputFamilies.count("ax")) {
          Reason = "cpuid output eax not covered by output constraint";
          return false;
        }
        if (!OutputFamilies.count("bx")) {
          Reason = "cpuid output ebx not covered by output constraint";
          return false;
        }
        if (!OutputFamilies.count("cx")) {
          Reason = "cpuid output ecx not covered by output constraint";
          return false;
        }
        if (!OutputFamilies.count("dx")) {
          Reason = "cpuid output edx not covered by output constraint";
          return false;
        }
        continue;
      }

      if (baseMnemonic == "xgetbv") {
        if (!operands.empty()) {
          Reason = "xgetbv takes no operands";
          return false;
        }
        if (!InputFamilies.count("cx")) {
          Reason = "xgetbv input ecx not covered by input constraint";
          return false;
        }
        if (!OutputFamilies.count("ax")) {
          Reason = "xgetbv output eax not covered by output constraint";
          return false;
        }
        if (!OutputFamilies.count("dx")) {
          Reason = "xgetbv output edx not covered by output constraint";
          return false;
        }
        continue;
      }

      if (baseMnemonic == "sar" || baseMnemonic == "shr" ||
          baseMnemonic == "shl") {
        if (operands.size() == 1)
          roles = {RoleBoth};
        else if (operands.size() != 2) {
          Reason = mnemonic + " expects one or two operands";
          return false;
        }
      } else if (operands.size() != 2) {
        Reason = mnemonic + " expects two operands";
        return false;
      }

      if (setsFlags)
        AnySetsFlags = true;

      for (size_t i = 0; i < operands.size(); ++i) {
        const std::string& op = operands[i];
        if (op.find_first_of("()[]") != std::string::npos) {
          Reason = "memory/indirect operand not allowed in safe inline asm: " + op;
          return false;
        }
        int ph = -1;
        std::string family;
        OperandKind kind = classifyOperand(op, ph, family, Constraints.size());
        switch (kind) {
        case OKMemory:
          Reason = "unsupported operand in safe inline asm: " + op;
          return false;
        case OKImmediate:
          break;
        case OKReg: {
          if (i >= roles.size()) {
            Reason = "unexpected register operand position";
            return false;
          }
          OperandRole role = roles[i];
          if (role == RoleInput) {
            if (!InputFamilies.count(family)) {
              Reason = "literal register " + op + " not covered by input constraint";
              return false;
            }
          } else if (role == RoleOutput) {
            if (!OutputFamilies.count(family)) {
              Reason = "literal register " + op + " not covered by output constraint";
              return false;
            }
          } else {
            if (!InputFamilies.count(family) || !OutputFamilies.count(family)) {
              Reason = "literal register " + op + " not covered by input/output constraints";
              return false;
            }
          }
          break;
        }
        case OKPlaceholder:
          if (ph < 0 || static_cast<size_t>(ph) >= Constraints.size()) {
            Reason = "operand placeholder out of range: " + op;
            return false;
          }
          if (!Constraints[ph].IsRegister) {
            Reason = "operand placeholder refers to non-register constraint: " + op;
            return false;
          }
          if ((baseMnemonic == "sar" || baseMnemonic == "shr" || baseMnemonic == "shl") &&
              roles[i] == RoleInput) {
            if (Constraints[ph].Family != "cx") {
              Reason = "variable shift count requires cl/cx/ecx/rcx input constraint";
              return false;
            }
            if (!InputFamilies.count("cx")) {
              Reason = "variable shift count requires cl/cx/ecx/rcx input constraint";
              return false;
            }
          }
          break;
        }
      }
    }

    if (AnySetsFlags && !HasCCClobber) {
      Reason = "assembly sets flags but \"cc\" clobber is missing";
      return false;
    }

    return true;
  }

  bool handleInlineAsm(CallBase* CI, std::string& Reason) {
    if (verbose)
      errs() << "Dealing with inline asm call: " << *CI << "\n";

    InlineAsm* IA = cast<InlineAsm>(CI->getCalledOperand());
    bool IsEmptyAsm = true;
    for (char c : IA->getAsmString()) {
      if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
        IsEmptyAsm = false;
        break;
      }
    }

    if (!IsEmptyAsm) {
      if (IA->getDialect() != InlineAsm::AD_ATT) {
        Reason = "only AT&T dialect inline assembly is supported";
        return false;
      }
      if (hasPtrs(CI->getType())) {
        Reason = "inline assembly with pointer return type is not supported";
        return false;
      }
      for (size_t Index = CI->arg_size(); Index--;) {
        if (hasPtrs(CI->getArgOperand(Index)->getType())) {
          Reason = "inline assembly with pointer argument is not supported";
          return false;
        }
      }
      if (!validateSafeInlineAsm(CI, IA, Reason))
        return false;
      if (verbose)
        errs() << "Passing through safe inline asm call.\n";
      return true;
    }

    // If the inline asm doesn't deal in pointers, then we can just pass it through.
    if (!hasPtrs(CI->getType())) {
      bool hasPtrArg = false;
      for (size_t Index = CI->arg_size(); Index--;) {
        if (hasPtrs(CI->getArgOperand(Index)->getType())) {
          hasPtrArg = true;
          break;
        }
      }
      if (!hasPtrArg)
        return true;
    }

    if (verbose)
      errs() << "It has pointers, going to parse the constraints.\n";

    // The inline asm does have pointers. So, we need to analyze the constraints. We do our own
    // analysis rather than relying on InlineAsm::ParseConstraints because we cannot handle the
    // more complex ones and this parser rejects the ones we cannot handle.

    std::vector<Type*> OutputTypes;
    
    auto checkType = [&] (Type* T) {
      if (T->isIntegerTy() || T->isFloatTy() || T->isPointerTy() || T->isVectorTy())
        return true;
      std::string str;
      raw_string_ostream buf(str);
      buf << "unexpected return type: " << *T;
      Reason = str;
      return false;
    };
    
    if (!CI->getType()->isVoidTy()) {
      if (StructType* ST = dyn_cast<StructType>(CI->getType())) {
        for (unsigned Index = 0; Index < ST->getNumElements(); ++Index) {
          Type* T = ST->getElementType(Index);
          if (!checkType(T))
            return false;
          OutputTypes.push_back(T);
        }
      } else {
        if (!checkType(CI->getType()))
          return false;
        OutputTypes.push_back(CI->getType());
      }
    }

    std::vector<AsmConstraint> ConstraintsArray;
    std::vector<AsmIO> Outputs;
    std::vector<AsmIO> Inputs;

    std::string Constraints = IA->getConstraintString();
    for (size_t Index = 0, EndIndex; Index < Constraints.size(); Index = EndIndex + 1) {
      EndIndex = Constraints.find(',', Index);
      if (EndIndex == std::string::npos)
        EndIndex = Constraints.size();

      std::string ThisConstraint = Constraints.substr(Index, EndIndex - Index);
      ConstraintsArray.push_back(AsmConstraint(AsmConstraintKind::Other, 0, ThisConstraint));

      if (verbose)
        errs() << "Considering constraint: " << ThisConstraint << "\n";

      if (Constraints[Index] == '!') {
        Reason = "unsupported constraint: !, aka label";
        return false;
      }

      if (Constraints[Index] == '~') {
        Index++;
        if (Index >= EndIndex) {
          Reason = "malformed constraint: ~ at end";
          return false;
        }
        if (Constraints[Index] != '{') {
          Reason = "malformed constraint: ~ not followed by {";
          return false;
        }
        Index++;
        if (Index >= EndIndex) {
          Reason = "malformed constraint: ~{ at end";
          return false;
        }
        size_t EndClobberIndex = Constraints.find('}', Index);
        if (EndClobberIndex == std::string::npos || EndClobberIndex >= EndIndex) {
          Reason = "malformed constraint: unmatched ~{ and }";
          return false;
        }
        if (EndClobberIndex != EndIndex - 1) {
          Reason = "malformed constraint: junk after }";
          return false;
        }
        continue;
      }

      bool isOutput = false;
      bool isOutputLogically = false;
      bool isPtr = false;
      
      if (Constraints[Index] == '=') {
        Index++;
        if (Index >= EndIndex) {
          Reason = "malformed constraint: = at end";
          return false;
        }
        isOutput = true;
        isOutputLogically = true;
      }

      if (Constraints[Index] == '*') {
        Index++;
        if (Index >= EndIndex) {
          Reason = "malformed constraint: * at end";
          return false;
        }
        // This is an indirect operand, so from our standpoint, it's an input and that input is a
        // pointer.
        isOutput = false;
        isPtr = true;
        ConstraintsArray.back().IsIndirect = true;
      }

      if (isdigit(Constraints[Index])) {
        if (isOutputLogically) {
          std::ostringstream buf;
          buf << "output matching constraint: " << ThisConstraint;
          Reason = buf.str();
          return false;
        }

        Type* T = CI->getArgOperand(Inputs.size())->getType();
        if (isPtr)
          assert(isSomePtr(T));

        for (size_t SubIndex = Index; SubIndex < EndIndex; ++SubIndex) {
          if (!isdigit(Constraints[Index])) {
            std::ostringstream buf;
            buf << "nontrivial matching constraint: " << ThisConstraint;
            Reason = buf.str();
            return false;
          }
        }
        int MatchingIndex = atoi(ThisConstraint.c_str());
        assert(MatchingIndex >= 0);
        if (static_cast<size_t>(MatchingIndex) >= ConstraintsArray.size()) {
          std::ostringstream buf;
          buf << "matching constraint out of bounds: " << ThisConstraint;
          Reason = buf.str();
          return false;
        }
        ConstraintsArray.back().MatchingIndex = MatchingIndex;
        if (ConstraintsArray[MatchingIndex].Kind == AsmConstraintKind::Input) {
          // This happens in situations like this: "=*r|m,0". The matching constraint is an indirect
          // input in these cases.
          assert(static_cast<size_t>(ConstraintsArray[MatchingIndex].Index) < Inputs.size());
          if (!isSomePtr(Inputs[ConstraintsArray[MatchingIndex].Index].T)) {
            std::ostringstream buf;
            buf << "matching constraint points at input that isn't a pointer: " << ThisConstraint;
            Reason = buf.str();
            return false;
          }
          ConstraintsArray.back().Kind = AsmConstraintKind::Input;
          ConstraintsArray.back().Index = Inputs.size();
          Inputs.push_back(AsmIO(T, -1, ConstraintsArray.size() - 1));
          continue;
        }
        if (ConstraintsArray[MatchingIndex].Kind != AsmConstraintKind::Output) {
          std::ostringstream buf;
          buf << "matching constraint doesn't point at output: " << ThisConstraint;
          Reason = buf.str();
          return false;
        }
        if (Outputs[ConstraintsArray[MatchingIndex].Index].Matching != -1) {
          std::ostringstream buf;
          buf << "matching constraint points at taken output: " << ThisConstraint;
          Reason = buf.str();
          return false;
        }
        assert(Inputs.size() < CI->arg_size());
        ConstraintsArray.back().Kind = AsmConstraintKind::Input;
        ConstraintsArray.back().Index = Inputs.size();
        Outputs[ConstraintsArray[MatchingIndex].Index].Matching = Inputs.size();
        Inputs.push_back(
          AsmIO(T, ConstraintsArray[MatchingIndex].Index, ConstraintsArray.size() - 1));
        continue;
      }

      // I don't think we care about the rest of this string so long as it doesn't have digits.
      for (size_t SubIndex = Index; SubIndex < EndIndex; ++SubIndex) {
        if (isdigit(Constraints[Index])) {
          std::ostringstream buf;
          buf << "nontrivial matching constraint: " << ThisConstraint;
          Reason = buf.str();
          return false;
        }
      }

      Type* T;
      if (isOutput) {
        assert(Outputs.size() < OutputTypes.size());
        T = OutputTypes[Outputs.size()];
      } else {
        assert(Inputs.size() < CI->arg_size());
        T = CI->getArgOperand(Inputs.size())->getType();
      }

      if (isPtr)
        assert(isSomePtr(T));

      if (isOutput) {
        ConstraintsArray.back().Kind = AsmConstraintKind::Output;
        ConstraintsArray.back().Index = Outputs.size();
        Outputs.push_back(AsmIO(T, -1, ConstraintsArray.size() - 1));
      } else {
        ConstraintsArray.back().Kind = AsmConstraintKind::Input;
        ConstraintsArray.back().Index = Inputs.size();
        Inputs.push_back(AsmIO(T, -1, ConstraintsArray.size() - 1));
      }
    }

    assert(Outputs.size() == OutputTypes.size());
    assert(Inputs.size() == CI->arg_size());

    std::vector<Type*> NewOutputTypes;
    std::vector<Value*> NewInputOperands;
    std::vector<Type*> NewInputElementTypes;
    std::ostringstream NewConstraintBuf;
    size_t NumNewConstraints = 0;

    for (size_t Index = 0; Index < ConstraintsArray.size(); ++Index) {
      AsmConstraint& AC = ConstraintsArray[Index];
      if (Index)
        NewConstraintBuf << ",";
      AC.NewIndex = NumNewConstraints;
      assert(AC.MatchingIndex >= -1);
      if (AC.MatchingIndex >= 0) {
        if (AC.IsIndirect)
          NewConstraintBuf << "*";
        NewConstraintBuf << ConstraintsArray[AC.MatchingIndex].NewIndex;
      } else
        NewConstraintBuf << AC.String;
      NumNewConstraints++;
      switch (AC.Kind) {
      case AsmConstraintKind::Other:
        // We ignore these.
        break;
      case AsmConstraintKind::Input: {
        assert(AC.Index < Inputs.size());
        const AsmIO& AIO = Inputs[AC.Index];
        Value* V = CI->getArgOperand(AC.Index);
        assert(AIO.T == V->getType());
        if (isSomePtr(AIO.T)) {
          NewInputOperands.push_back(flightPtrPtr(V, CI));
          NewInputElementTypes.push_back(CI->getAttributes().getParamElementType(AC.Index));
          if (AIO.Matching >= 0) {
            NewInputOperands.push_back(flightPtrLower(V, CI));
            NewInputElementTypes.push_back(nullptr);
            assert(static_cast<size_t>(AIO.Matching) < Outputs.size());
            assert(static_cast<unsigned>(Outputs[AIO.Matching].Matching) == AC.Index);
            unsigned NewMatchingIndex = ConstraintsArray[Outputs[AIO.Matching].Index].NewIndex;
            assert(NewMatchingIndex < NumNewConstraints);
            assert(NewMatchingIndex < AC.NewIndex);
            assert(NewMatchingIndex + 1 < NumNewConstraints);
            assert(NewMatchingIndex + 1 < AC.NewIndex);
            NewConstraintBuf << "," << NewMatchingIndex + 1;
            NumNewConstraints++;
          } else {
            // If we have an input constraint involving a pointer, then we don't bother passing the
            // pointer's capability. I think that's right, but it's hard to tell.
            //
            // Reason why it might be right: the inline assembly is asking us to please compute this
            // pointer. We are still asking for that.
            //
            // Reason why it might be wrong: the inline assembly we lower to isn't asking to compute
            // the capability.
            //
            // But I can't think of any reason why failing to ask to compute the capability hurts us
            // in any way.
          }
        } else {
          assert(!hasPtrs(AIO.T));
          NewInputOperands.push_back(V);
          NewInputElementTypes.push_back(nullptr);
        }
        break;
      }
      case AsmConstraintKind::Output: {
        assert(AC.Index < Outputs.size());
        const AsmIO& AIO = Outputs[AC.Index];
        assert(AIO.T == OutputTypes[AC.Index]);
        if (isSomePtr(AIO.T)) {
          NewOutputTypes.push_back(RawPtrTy);
          if (AIO.Matching >= 0) {
            assert(static_cast<size_t>(AIO.Matching) < Inputs.size());
            assert(static_cast<unsigned>(Inputs[AIO.Matching].Matching) == AC.Index);
            NewOutputTypes.push_back(RawPtrTy);
            NewConstraintBuf << ",=r";
            NumNewConstraints++;
          }
        } else {
          assert(!hasPtrs(AIO.T));
          NewOutputTypes.push_back(AIO.T);
        }
        break;
      } }
    }

    if (verbose) {
      errs() << "New constraints: " << NewConstraintBuf.str() << "\n";
      errs() << "Num new inputs: " << NewInputOperands.size() << "\n";
      errs() << "Num new outputs: " << NewOutputTypes.size() << "\n";
    }

    assert(NewInputOperands.size() == NewInputElementTypes.size());

    Type* NewRetT;
    if (NewOutputTypes.empty())
      NewRetT = VoidTy;
    else if (NewOutputTypes.size() == 1)
      NewRetT = NewOutputTypes[0];
    else
      NewRetT = StructType::get(C, NewOutputTypes);

    std::vector<Type*> NewInputTypes;
    for (Value* V : NewInputOperands)
      NewInputTypes.push_back(V->getType());

    FunctionType* NewFT = FunctionType::get(NewRetT, NewInputTypes, false);

    InlineAsm* NewIA = InlineAsm::get(
      NewFT, "", NewConstraintBuf.str(), IA->hasSideEffects(), IA->isAlignStack(), IA->getDialect(),
      IA->canThrow());

    CallInst* NewCI = CallInst::Create(
      NewIA, NewInputOperands, NewRetT == VoidTy ? "" : "filc_inline_asm_call", CI);

    for (size_t Index = 0; Index < NewInputElementTypes.size(); ++Index) {
      if (Type* T = NewInputElementTypes[Index])
        NewCI->addParamAttr(Index, Attribute::get(C, Attribute::ElementType, T));
    }

    Instruction* InsertionPoint = CI->getNextNode();

    if (hasPtrs(NewRetT)) {
      assert(NewOutputTypes.size() >= Outputs.size());
      
      auto GetReturnValue = [&] (size_t Index) -> Value* {
        assert(static_cast<unsigned>(Index) == Index);
        assert(NewOutputTypes.size());
        if (NewOutputTypes.size() == 1) {
          assert(!Index);
          return NewCI;
        }
        assert(Index < NewOutputTypes.size());
        Instruction* Result = ExtractValueInst::Create(
          NewOutputTypes[Index], NewCI, { static_cast<unsigned>(Index) },
          "filc_inline_asm_return_value", InsertionPoint);
        Result->setDebugLoc(CI->getDebugLoc());
        return Result;
      };

      std::vector<Value*> NewOutputValues;
      for (size_t Index = 0, ReturnValueIndex = 0; Index < Outputs.size(); ++Index) {
        assert(ReturnValueIndex < NewOutputTypes.size());
        const AsmIO& AIO = Outputs[Index];
        Value* V = GetReturnValue(ReturnValueIndex++);
        if (!isSomePtr(AIO.T)) {
          assert(!hasPtrs(AIO.T));
          NewOutputValues.push_back(V);
          continue;
        }
        if (AIO.Matching < 0) {
          assert(AIO.Matching == -1);
          NewOutputValues.push_back(badFlightPtr(V, InsertionPoint));
          continue;
        }
        NewOutputValues.push_back(
          createFlightPtr(GetReturnValue(ReturnValueIndex++), V, InsertionPoint));
      }

      assert(NewOutputValues.size());
      assert(NewOutputValues.size() == Outputs.size());
      assert(NewOutputValues.size() <= NewOutputTypes.size());

      if (NewOutputValues.size() == 1) {
        assert(toFlightType(CI->getType()) == NewOutputValues[0]->getType());
        CI->replaceAllUsesWith(NewOutputValues[0]);
      } else {
        std::vector<Type*> NewOutputValueTypes;
        for (Value* V : NewOutputValues)
          NewOutputValueTypes.push_back(V->getType());
        StructType* ST = StructType::get(C, NewOutputValueTypes);
        assert(ST == toFlightType(CI->getType()));
        Value* Result = UndefValue::get(ST);
        for (size_t Index = 0; Index < NewOutputValues.size(); ++Index) {
          assert(static_cast<unsigned>(Index) == Index);
          Instruction* Insert = InsertValueInst::Create(
            Result, NewOutputValues[Index], { static_cast<unsigned>(Index) },
            "filc_inline_asm_insert", InsertionPoint);
          Insert->setDebugLoc(CI->getDebugLoc());
          Result = Insert;
        }
        CI->replaceAllUsesWith(Result);
      }
    } else
      CI->replaceAllUsesWith(NewCI);
    CI->eraseFromParent();
    return true;
  }

  FunctionType* fastFunctionTypeForSignature(const std::vector<Type*>& NormalizedArgTypes,
                                             Type* NormalizedRetType) {
    std::vector<Type*> ArgTypes;
    ArgTypes.push_back(RawPtrTy); // thread
    ArgTypes.push_back(RawPtrTy); // callee function object
    for (Type* T : NormalizedArgTypes)
      ArgTypes.push_back(toFlightType(T));
    std::vector<Type*> RetTypes;
    RetTypes.push_back(Int1Ty); // has_exception
    iterateReturnType(NormalizedRetType, [&] (Type* T) -> void {
      RetTypes.push_back(toFlightType(T));
    });
    return FunctionType::get(StructType::get(C, RetTypes), ArgTypes, false);
  }

  std::vector<Type*> argTypesForDirectInfos(const std::vector<ArgInfo>& AIs) {
    std::vector<Type*> NormalizedArgTypes;
    for (ArgInfo AI : AIs) {
      assert(AI.AK == ArgKind::Direct);
      assert(normalizeArgType(AI.T) == AI.T);
      NormalizedArgTypes.push_back(AI.T);
    }
    return NormalizedArgTypes;
  }

  FunctionType* fastFunctionTypeForSignature(const std::vector<ArgInfo>& AIs,
                                             Type* NormalizedRetType) {
    return fastFunctionTypeForSignature(argTypesForDirectInfos(AIs), NormalizedRetType);
  }

  Value* genericEntrypointForFunctionPayload(Value* FunctionPayload, Instruction* Before) {
    Instruction* GenericEntrypointPtr = GetElementPtrInst::Create(
      FunctionPayloadTy, FunctionPayload,
      { ConstantInt::get(Int32Ty, 0), ConstantInt::get(Int32Ty, 1) },
      "filc_generic_entrypoint_ptr", Before);
    GenericEntrypointPtr->setDebugLoc(Before->getDebugLoc());
    Instruction* GenericEntrypoint = new LoadInst(
      RawPtrTy, GenericEntrypointPtr, "filc_generic_entrypoint_load", Before);
    GenericEntrypoint->setDebugLoc(Before->getDebugLoc());
    return GenericEntrypoint;
  }

  Value* signatureForFunctionPayload(Value* FunctionPayload, Instruction* Before) {
    Instruction* SignaturePtr = GetElementPtrInst::Create(
      FunctionPayloadTy, FunctionPayload,
      { ConstantInt::get(Int32Ty, 0), ConstantInt::get(Int32Ty, 2) },
      "filc_signature_ptr", Before);
    SignaturePtr->setDebugLoc(Before->getDebugLoc());
    Instruction* Signature = new LoadInst(Int64Ty, SignaturePtr, "filc_signature_load", Before);
    Signature->setDebugLoc(Before->getDebugLoc());
    return Signature;
  }

  Value* fastEntrypointForFunctionPayload(Value* FunctionPayload, Instruction* Before) {
    Instruction* FastEntrypointPtr = GetElementPtrInst::Create(
      FunctionPayloadTy, FunctionPayload,
      { ConstantInt::get(Int32Ty, 0), ConstantInt::get(Int32Ty, 0) },
      "filc_fast_entrypoint_ptr", Before);
    FastEntrypointPtr->setDebugLoc(Before->getDebugLoc());
    Instruction* FastEntrypoint = new LoadInst(
      RawPtrTy, FastEntrypointPtr, "filc_fast_entrypoint_load", Before);
    FastEntrypoint->setDebugLoc(Before->getDebugLoc());
    return FastEntrypoint;
  }

  Value* callGenericFromFastThunk(Value* CalleeLower, Function* Result,
                                  const std::vector<ArgInfo>& AIs, Type* NormalizedRetType,
                                  Instruction* Before) {
    assert(Result->getFunctionType()->getNumParams() == AIs.size() + 2);
    std::vector<Value*> Args;
    for (size_t Idx = 0; Idx < AIs.size(); ++Idx) {
      assert(AIs[Idx].AK == ArgKind::Direct);
      assert(toFlightType(AIs[Idx].T) == Result->getArg(Idx + 2)->getType());
      Args.push_back(Result->getArg(Idx + 2));
    }
    Value* ArgSize;
    if (!AIs.empty())
      ArgSize = storeCC(AIs, Args, Before, DebugLoc());
    else
      ArgSize = ConstantInt::get(IntPtrTy, 0);
    Instruction* TheCall = CallInst::Create(
      PizlonatedFuncTy, genericEntrypointForFunctionPayload(CalleeLower, Before),
      { MyThread, CalleeLower, ArgSize },
      "filc_generic_call", Before);
    Instruction* HasException = ExtractValueInst::Create(
      Int1Ty, TheCall, { 0 }, "filc_has_exception", Before);
    Instruction* FastResult = InsertValueInst::Create(
      UndefValue::get(Result->getFunctionType()->getReturnType()), HasException,
      { 0 }, "filc_insert_has_exception", Before);
    Instruction* ElseTerm = SplitBlockAndInsertIfElse(
      expectFalse(HasException, Before), Before, false);
    Instruction* RetSize = ExtractValueInst::Create(
      IntPtrTy, TheCall, { 1 }, "filc_ret_size", ElseTerm);
    Value* FastValueResult = FastResult;
    if (NormalizedRetType != VoidTy) {
      Value* GenericResult = loadCC(
        NormalizedRetType, RetSize, CCRetsCheckFailure, ElseTerm, DebugLoc());
      FastValueResult = insertAndNormalizeReturn(
        NormalizedRetType, GenericResult, FastResult, ElseTerm);
    }
    PHINode* FastResultPHI = PHINode::Create(
      Result->getFunctionType()->getReturnType(), 2, "filc_result_value_phi", Before);
    FastResultPHI->addIncoming(FastResult, FastResult->getParent());
    FastResultPHI->addIncoming(FastValueResult, ElseTerm->getParent());
    return FastResultPHI;
  }

  Function* callerEntrypointThunk(uint64_t Signature, const std::vector<ArgInfo>& AIs,
                                  Type* NormalizedRetType) {
    assert(Signature != GenericSignature);
    assert(computeSignature(AIs, NormalizedRetType) == Signature);

    if (CallerEntrypointThunks.count(Signature))
      return CallerEntrypointThunks[Signature];

    std::ostringstream buf;
    buf << "pizlonated1ET" << Signature;

    FunctionType* FuncTy = fastFunctionTypeForSignature(AIs, NormalizedRetType);
    FunctionCallee Callee = M.getOrInsertFunction(buf.str(), FuncTy);
    assert(Callee.getFunctionType() == FuncTy);
    Function* Result = cast<Function>(Callee.getCallee());
    assert(Result->isDeclaration());
    assert(Result->getFunctionType() == FuncTy);
    Result->setLinkage(GlobalValue::LinkOnceODRLinkage);
    Result->addFnAttr(Attribute::NoInline);
    Result->addFnAttr(Attribute::NoUnwind);

    Value* OldMyThread = MyThread;
    Function* OldOldF = OldF;
    Function* OldNewF = NewF;
    MyThread = Result->getArg(0);
    OldF = nullptr;
    NewF = Result;
    
    Value* CalleeLower = Result->getArg(1);

    BasicBlock* RootB = BasicBlock::Create(C, "filc_caller_entrypoint_thunk_root", Result);
    ReturnInst* Return = ReturnInst::Create(C, UndefValue::get(FuncTy->getReturnType()), RootB);

    Return->getOperandUse(0) =
      callGenericFromFastThunk(CalleeLower, Result, AIs, NormalizedRetType, Return);
    
    MyThread = OldMyThread;
    OldF = OldOldF;
    NewF = OldNewF;
    
    CallerEntrypointThunks[Signature] = Result;
    return Result;
  }
  
  Function* calleeEntrypointThunk(uint64_t Signature, const std::vector<ArgInfo>& AIs,
                                  Type* NormalizedRetType) {
    assert(Signature != GenericSignature);
    assert(computeSignature(AIs, NormalizedRetType) == Signature);

    if (CalleeEntrypointThunks.count(Signature))
      return CalleeEntrypointThunks[Signature];

    std::ostringstream buf;
    buf << "pizlonated2ET" << Signature;

    FunctionType* FuncTy = fastFunctionTypeForSignature(AIs, NormalizedRetType);
    FunctionCallee Callee = M.getOrInsertFunction(buf.str(), PizlonatedFuncTy);
    assert(Callee.getFunctionType() == PizlonatedFuncTy);
    Function* Result = cast<Function>(Callee.getCallee());
    assert(Result->isDeclaration());
    Result->setLinkage(GlobalValue::LinkOnceODRLinkage);
    Result->addFnAttr(Attribute::NoInline);
    Result->addFnAttr(Attribute::NoUnwind);

    Value* OldMyThread = MyThread;
    Function* OldOldF = OldF;
    Function* OldNewF = NewF;
    MyThread = Result->getArg(0);
    OldF = nullptr;
    NewF = Result;
    
    Value* CalleeLower = Result->getArg(1);
    Value* ArgSize = Result->getArg(2);

    BasicBlock* RootB = BasicBlock::Create(C, "filc_callee_entrypoint_thunk_root", Result);
    ReturnInst* Return = ReturnInst::Create(C, UndefValue::get(FuncTy->getReturnType()), RootB);

    std::vector<Value*> IncomingArgs;
    if (!AIs.empty())
      IncomingArgs = loadCC(AIs, ArgSize, CCArgsCheckFailure, Return, DebugLoc());
    assert(IncomingArgs.size() == AIs.size());
    std::vector<Value*> Args;
    Args.push_back(MyThread);
    Args.push_back(CalleeLower);
    for (size_t Idx = 0; Idx < AIs.size(); ++Idx) {
      assert(AIs[Idx].AK == ArgKind::Direct);
      Args.push_back(IncomingArgs[Idx]);
    }
    Instruction* TheCall = CallInst::Create(
      FuncTy, fastEntrypointForFunctionPayload(CalleeLower, Return), Args, "filc_fast_call", Return);
    Instruction* HasException = ExtractValueInst::Create(
      Int1Ty, TheCall, { 0 }, "filc_has_exception", Return);
    Instruction* GenericResult = InsertValueInst::Create(
      UndefValue::get(PizlonatedReturnValueTy), HasException, { 0 }, "filc_insert_has_exception",
      Return);
    Instruction* ElseTerm = SplitBlockAndInsertIfElse(
      expectFalse(HasException, Return), Return, false);
    Value* RetSize;
    if (NormalizedRetType == VoidTy)
      RetSize = storeCC(IntPtrTy, ConstantInt::get(IntPtrTy, 0), ElseTerm, DebugLoc());
    else {
      RetSize = storeCC(
        NormalizedRetType, extractAndDenormalizeReturn(NormalizedRetType, TheCall, ElseTerm),
        ElseTerm, DebugLoc());
    }
    Instruction* GenericValueResult =
      InsertValueInst::Create(GenericResult, RetSize, { 1 }, "filc_insert_ret_size", ElseTerm);
    PHINode* GenericResultPHI = PHINode::Create(
      PizlonatedReturnValueTy, 2, "filc_result_value_phi", Return);
    GenericResultPHI->addIncoming(GenericResult, GenericResult->getParent());
    GenericResultPHI->addIncoming(GenericValueResult, GenericValueResult->getParent());
    Return->getOperandUse(0) = GenericResultPHI;
    
    MyThread = OldMyThread;
    OldF = OldOldF;
    NewF = OldNewF;
    
    CalleeEntrypointThunks[Signature] = Result;
    return Result;
  }

  Value* checkFunctionAndGetLower(Value* CalledOperand, Instruction* Before) {
    Value* CalledLower = flightPtrLower(CalledOperand, Before);
    ICmpInst* NullLower = new ICmpInst(
      Before, ICmpInst::ICMP_EQ, CalledLower, RawNull, "filc_null_called_lower");
    NullLower->setDebugLoc(Before->getDebugLoc());
    Instruction* NewBlockTerm = SplitBlockAndInsertIfThen(
      expectFalse(NullLower, Before), Before, true);
    BasicBlock* NewBlock = NewBlockTerm->getParent();
    CallInst::Create(
      CheckFunctionCallFail, { CalledOperand }, "", NewBlockTerm)
      ->setDebugLoc(Before->getDebugLoc());
    ICmpInst* AtAuxPtr = new ICmpInst(
      Before, ICmpInst::ICMP_EQ, flightPtrPtr(CalledOperand, Before),
      auxPtrForLower(CalledLower, Before), "filc_call_at_lower");
    AtAuxPtr->setDebugLoc(Before->getDebugLoc());
    SplitBlockAndInsertIfElse(
      expectTrue(AtAuxPtr, Before), Before, false, nullptr, nullptr, nullptr, NewBlock);
    BinaryOperator* Masked = BinaryOperator::Create(
      Instruction::And, flagsForLower(CalledLower, Before),
      ConstantInt::get(IntPtrTy, SpecialTypeMask << ObjectFlagsSpecialShift),
      "filc_call_mask_special_type", Before);
    Masked->setDebugLoc(Before->getDebugLoc());
    ICmpInst* IsFunction = new ICmpInst(
      Before, ICmpInst::ICMP_EQ, Masked,
      ConstantInt::get(IntPtrTy, SpecialTypeFunction << ObjectFlagsSpecialShift),
      "filc_call_is_function");
    IsFunction->setDebugLoc(Before->getDebugLoc());
    SplitBlockAndInsertIfElse(
      expectTrue(IsFunction, Before), Before, false, nullptr, nullptr, nullptr, NewBlock);
    return CalledLower;
  }

  Value* knownTargetCallsiteThunk(Function* F, uint64_t Signature, const std::vector<ArgInfo>& AIs,
                                  Type* NormalizedRetType, Instruction* Before) {
    assert(computeSignature(AIs, NormalizedRetType) == Signature);

    // In the case of linkonce, which is used for C++ inline functions, we will have a definition in
    // the local module.
    if (FunctionToSignature.count(F)) {
      assert(FunctionToHiddenFunction.count(F));
      Function* Result = FunctionToHiddenFunction[F];
      if (FunctionToSignature[F] == Signature) {
        assert(F->getLinkage() == GlobalValue::ExternalLinkage ||
               F->getLinkage() == GlobalValue::InternalLinkage ||
               F->getLinkage() == GlobalValue::PrivateLinkage ||
               F->getLinkage() == GlobalValue::LinkOnceAnyLinkage ||
               F->getLinkage() == GlobalValue::WeakAnyLinkage);
        if ((F->getLinkage() == GlobalValue::LinkOnceAnyLinkage ||
             F->getLinkage() == GlobalValue::WeakAnyLinkage)
            && GlobalToComdat.count(F)) {
          Instruction* IsNull = new ICmpInst(
            Before, ICmpInst::ICMP_EQ, Result, RawNull, "filc_global_is_null");
          IsNull->setDebugLoc(Before->getDebugLoc());
          Instruction* ThenTerm = SplitBlockAndInsertIfThen(
            expectFalse(IsNull, Before), Before, true);
          CallInst::Create(
            ComdatLinkFail, { getString(getFunctionName(F)), ConstantInt::get(Int64Ty, Signature) },
            "", ThenTerm)
            ->setDebugLoc(Before->getDebugLoc());
        }
        return Result;
      }
    }

    NameAndSignature Key(std::string(F->getName()), Signature);
    if (KnownTargetCallsiteThunks.count(Key))
      return KnownTargetCallsiteThunks[Key];

    std::ostringstream buf;
    buf << "pizlonatedFI" << Signature << "_" << std::string(F->getName());
    FunctionType* FuncTy;
    if (Signature == GenericSignature)
      FuncTy = PizlonatedFuncTy;
    else
      FuncTy = fastFunctionTypeForSignature(AIs, NormalizedRetType);
    Function* Result = M.getFunction(buf.str());
    if (Result) {
      assert(!F->isDeclaration());
      assert(Result->getFunctionType() == FuncTy);
      assert(!Result->isDeclaration());
    } else {
      FunctionCallee ResultCallee = M.getOrInsertFunction(buf.str(), FuncTy);
      assert(ResultCallee.getFunctionType() == FuncTy);
      Result = cast<Function>(ResultCallee.getCallee());
      assert(Result->isDeclaration());
      switch (F->getLinkage()) {
      case GlobalValue::ExternalLinkage:
      case GlobalValue::LinkOnceAnyLinkage:
      case GlobalValue::WeakAnyLinkage:
      case GlobalValue::ExternalWeakLinkage:
        Result->setLinkage(GlobalValue::WeakAnyLinkage);
        Result->setVisibility(GlobalValue::HiddenVisibility);
        break;
      case GlobalValue::InternalLinkage:
      case GlobalValue::PrivateLinkage:
        Result->setLinkage(F->getLinkage());
        break;
      default:
        llvm_unreachable("Invalid linkage type");
        break;
      }
      Result->addFnAttr(Attribute::NoInline);
      Result->addFnAttr(Attribute::NoUnwind);
      
      Value* OldMyThread = MyThread;
      Function* OldOldF = OldF;
      Function* OldNewF = NewF;
      MyThread = Result->getArg(0);
      OldF = nullptr;
      NewF = Result;

      BasicBlock* RootB = BasicBlock::Create(C, "filc_callee_entrypoint_thunk_root", Result);
      ReturnInst* Return = ReturnInst::Create(C, UndefValue::get(FuncTy->getReturnType()), RootB);
      
      Value* Callee = constantToFlightValue(F, Return);
      Value* CalledLower = checkFunctionAndGetLower(Callee, Return);
      if (Signature == GenericSignature) {
        Return->getOperandUse(0) = CallInst::Create(
          PizlonatedFuncTy, genericEntrypointForFunctionPayload(CalledLower, Return),
          { MyThread, CalledLower, Result->getArg(2) },
          "filc_generic_call", Return);
      } else {
        Instruction* SignatureMatches = new ICmpInst(
          Return, ICmpInst::ICMP_EQ, signatureForFunctionPayload(CalledLower, Return),
          ConstantInt::get(Int64Ty, Signature), "filc_signature_matches");
        Instruction* ThenTerm = SplitBlockAndInsertIfThen(
          expectTrue(SignatureMatches, Return), Return, true);
        std::vector<Value*> FastArgs;
        FastArgs.push_back(MyThread);
        FastArgs.push_back(CalledLower);
        for (unsigned Idx = 2; Idx < FuncTy->getNumParams(); ++Idx)
          FastArgs.push_back(Result->getArg(Idx));
        Instruction* FastCall = CallInst::Create(
          fastFunctionTypeForSignature(AIs, NormalizedRetType),
          fastEntrypointForFunctionPayload(CalledLower, ThenTerm),
          FastArgs, "filc_fast_call", ThenTerm);
        ReplaceInstWithInst(ThenTerm, ReturnInst::Create(C, FastCall));
        
        Return->getOperandUse(0) =
          callGenericFromFastThunk(CalledLower, Result, AIs, NormalizedRetType, Return);
      }
    
      MyThread = OldMyThread;
      OldF = OldOldF;
      NewF = OldNewF;
    }
    
    KnownTargetCallsiteThunks[Key] = Result;
    return Result;
  }
  
  // This lowers the instruction "in place", so all references to it are fixed up after this runs.
  void lowerInstruction(Instruction *I) {
    if (verbose)
      errs() << "Lowering: " << *I << "\n";

    if (PHINode* P = dyn_cast<PHINode>(I)) {
      for (unsigned Index = P->getNumIncomingValues(); Index--;) {
        lowerConstantOperand(
          P->getOperandUse(Index), P->getIncomingBlock(Index)->getTerminator());
      }
      P->mutateType(toFlightType(P->getType()));
      return;
    }

    if (CallBase* CI = dyn_cast<CallBase>(I)) {
      assert(isa<CallInst>(CI) || isa<InvokeInst>(CI));

      // FIXME: It would be cool to emit a direct call to the function, if:
      // - We know who the callee is.
      // - The original called signature according to the call instruction matches the original
      //   function type of the callee.
      // - The callee is a definition in this module, so we can call the hidden function.
      //
      // The trouble with this is that to make this totally effective:
      // - We'd want to eliminate the constant lowering of the callee, so we don't end up with a call
      //   to the pizlonated_getter.
      // - We'd want to call a version of the hidden function that "just" takes the arguments, without
      //   any CC shenanigans.
      //
      // To achieve the latter requirement, I'd probably want to emit all functions as a collection of
      // three functions:
      // - Hidden function that is the actual implementation. It takes its arguments and a frame
      //   pointer. It expects the caller to set up its frame and do all argument/return checking.
      // - Hidden function that uses the Fil-C CC and sets up the frame, then calls the
      //   implementation.
      // - Hidden function that uses a direct CC and sets up the frame, then calls the implementation.
      //
      // We can rely on the implementation to get inlined into the other functions whenever either of
      // these things is true:
      // 1. The implementation is small.
      // 2. Only the Fil-C CC, or only the direct CC, version are used.
      //
      // But this risks suboptimal codegen if the implementation isn't inlined. Yuck! The trick is that
      // we want the Fil-C CC shenanigans to happen with the frame already set up, so we can't simply
      // have the Fil-C CC version wrap the direct version.
      
      if (CI->isInlineAsm()) {
        std::string Reason = "";
        
        lowerConstantOperands(CI);
        
        if (handleInlineAsm(CI, Reason))
          return;

        assert(!Reason.empty());
        
        assert(isa<CallInst>(CI));
        std::string str;
        raw_string_ostream outs(str);
        outs << "cannot handle inline asm (" << Reason << "): " << *CI;
        CallInst::Create(Error, { getString(str), getOrigin(I->getDebugLoc()) }, "", I)
          ->setDebugLoc(I->getDebugLoc());
        if (I->getType() != VoidTy) {
          // We need to produce something to RAUW the call with, but it cannot be a constant, since
          // that would upset lowerConstant.
          Type* LowT = toFlightType(I->getType());
          LoadInst* LI = new LoadInst(LowT, RawNull, "filc_fake_load", I);
          LI->setDebugLoc(I->getDebugLoc());
          CI->replaceAllUsesWith(LI);
        }
        CI->eraseFromParent();
        return;
      }

      for (unsigned Index = CI->getNumOperands(); Index--;) {
        if (&CI->getOperandUse(Index) == &CI->getCalledOperandUse())
          continue;
        lowerConstantOperand(CI->getOperandUse(Index), CI);
      }
      
      if (verbose)
        errs() << "Dealing with called operand: " << *CI->getCalledOperand() << "\n";

      FunctionType *FT = CI->getFunctionType();
      assert(InstTypeVectors.count(CI));
      std::vector<Type*> ArgTypes = InstTypeVectors[CI];
      assert(ArgTypes.size() == CI->arg_size());
      std::vector<ArgInfo> AIs;
      std::vector<Value*> Vs;
      for (size_t Idx = 0; Idx < CI->arg_size(); ++Idx) {
        if (CI->isByValArgument(Idx)) {
          AIs.push_back(ArgInfo(CI->getParamByValType(Idx),
                                ArgKind::ByVal,
                                std::max(DL.getABITypeAlign(CI->getParamByValType(Idx)),
                                         CI->getParamAlign(Idx).valueOrOne())));
          Vs.push_back(CI->getArgOperand(Idx));
        } else {
          assert(!CI->isPassPointeeByValueArgument(Idx));
          Type* ArgType = normalizeArgType(ArgTypes[Idx]);
          AIs.push_back(ArgInfo(ArgType, ArgKind::Direct, DL.getABITypeAlign(ArgType)));
          Vs.push_back(convertToNormalizedArgType(ArgTypes[Idx], CI->getArgOperand(Idx), CI));
        }
      }
      assert(AIs.size() == Vs.size());
      Type* NormalizedRetType = normalizeRetType(FT->getReturnType());
      uint64_t Signature = computeSignature(AIs, NormalizedRetType);
      Value* ArgSize = nullptr;
      if (Signature == GenericSignature)
        ArgSize = storeCC(AIs, Vs, CI, CI->getDebugLoc());

      bool CanCatch;
      LandingPadInst* LPI;
      if (isa<CallInst>(CI)) {
        CanCatch = true;
        LPI = nullptr;
      } else {
        CanCatch = true;
        assert(LPIs.count(cast<InvokeInst>(CI)));
        LPI = LPIs[cast<InvokeInst>(CI)];
      }

      storeOrigin(getOrigin(CI->getDebugLoc(), CanCatch, LPI), CI);

      CallInst* TheCall = nullptr;
      Instruction* RetSize = nullptr;

      auto CallGeneric = [&] (Value* CalledLower, Value* Callee) {
        TheCall = CallInst::Create(
          PizlonatedFuncTy, Callee,
          { MyThread, CalledLower, ArgSize },
          "filc_generic_call", CI);
        RetSize = ExtractValueInst::Create(
          IntPtrTy, TheCall, { 1 }, "filc_ret_size", CI);
        RetSize->setDebugLoc(CI->getDebugLoc());
      };

      auto CallFast = [&] (Value* CalledLower, Value* Callee) {
        std::vector<Value*> Args;
        Args.push_back(MyThread);
        Args.push_back(CalledLower);
        for (Value* V : Vs)
          Args.push_back(V);
        TheCall = CallInst::Create(
          fastFunctionTypeForSignature(AIs, NormalizedRetType), Callee, Args,
          "filc_fast_call", CI);
        RetSize = nullptr;
      };
      
      if (Function* F = dyn_cast<Function>(CI->getCalledOperand())) {
        assert(!shouldPassThrough(F));

        Value* Callee = knownTargetCallsiteThunk(F, Signature, AIs, NormalizedRetType, CI);
        
        if (Signature == GenericSignature)
          CallGeneric(UndefValue::get(RawPtrTy), Callee);
        else
          CallFast(UndefValue::get(RawPtrTy), Callee);
      } else {
        lowerConstantOperand(CI->getCalledOperandUse(), CI);

        Value* CalledLower = checkFunctionAndGetLower(CI->getCalledOperand(), CI);

        assert(!CI->hasOperandBundles());
        if (Signature == GenericSignature)
          CallGeneric(CalledLower, genericEntrypointForFunctionPayload(CalledLower, CI));
        else {
          Instruction* SignatureMatches = new ICmpInst(
            CI, ICmpInst::ICMP_EQ, signatureForFunctionPayload(CalledLower, CI),
            ConstantInt::get(Int64Ty, Signature), "filc_signature_matches");
          SignatureMatches->setDebugLoc(CI->getDebugLoc());
          Instruction* ThenTerm = SplitBlockAndInsertIfThen(
            expectTrue(SignatureMatches, CI), CI, false);
          Value* FastEntrypoint = fastEntrypointForFunctionPayload(CalledLower, ThenTerm);
          PHINode* Entrypoint = PHINode::Create(RawPtrTy, 2, "filc_entrypoint_phi", CI);
          Entrypoint->addIncoming(FastEntrypoint, ThenTerm->getParent());
          Entrypoint->addIncoming(callerEntrypointThunk(Signature, AIs, NormalizedRetType),
                                  SignatureMatches->getParent());
          CallFast(CalledLower, Entrypoint);
        }
      }
      
      TheCall->setDebugLoc(CI->getDebugLoc());
      Instruction* HasException = ExtractValueInst::Create(
        Int1Ty, TheCall, { 0 }, "filc_has_exception", CI);
      HasException->setDebugLoc(CI->getDebugLoc());

      if (isa<CallInst>(CI) && CanCatch) {
        SplitBlockAndInsertIfThen(
          expectFalse(HasException, CI), CI, false, nullptr, nullptr, nullptr, ResumeB);
      } else if (InvokeInst* II = dyn_cast<InvokeInst>(CI)) {
        BranchInst::Create(
          II->getUnwindDest(), II->getNormalDest(), expectFalse(HasException, II), II)
          ->setDebugLoc(II->getDebugLoc());
      }
      
      Instruction* PostInsertionPt;
      if (isa<CallInst>(CI))
        PostInsertionPt = CI;
      else
        PostInsertionPt = &*cast<InvokeInst>(CI)->getNormalDest()->getFirstInsertionPt();

      if (FT->getReturnType() != VoidTy) {
        if (Signature == GenericSignature) {
          CI->replaceAllUsesWith(
            denormalizeReturn(
              FT->getReturnType(),
              loadCC(NormalizedRetType, RetSize, CCRetsCheckFailure, PostInsertionPt,
                     CI->getDebugLoc()),
              PostInsertionPt));
        } else {
          CI->replaceAllUsesWith(
            extractAndDenormalizeReturn(FT->getReturnType(), TheCall, PostInsertionPt));
        }
      }

      CI->eraseFromParent();
      return;
    }

    lowerConstantOperands(I);
    
    if (AllocaInst* AI = dyn_cast<AllocaInst>(I)) {
      if (!AI->hasNUsesOrMore(1)) {
        // By this point we may have dead allocas, due to earlyLowerInstruction. Only happens for allocas
        // used as type hacks for stdfil API.
        return;
      }
      
      Type* T = AI->getAllocatedType();
      Value* Size;
      std::optional<TypeSize> TSO = AI->getAllocationSize(DL);
      if (TSO && !TSO->isScalable())
        Size = ConstantInt::get(IntPtrTy, TSO->getFixedValue());
      else {
        Value* Length = AI->getArraySize();
        if (Length->getType() != IntPtrTy) {
          Instruction* ZExt = new ZExtInst(Length, IntPtrTy, "filc_alloca_length_zext", AI);
          ZExt->setDebugLoc(AI->getDebugLoc());
          Length = ZExt;
        }
        Instruction* SizeI = BinaryOperator::Create(
          Instruction::Mul, Length, ConstantInt::get(IntPtrTy, DL.getTypeAllocSize(T)),
          "filc_alloca_size", AI);
        SizeI->setDebugLoc(AI->getDebugLoc());
        Size = SizeI;
      }
      AI->replaceAllUsesWith(
        allocate(
          Size,
          ConstantInt::get(IntPtrTy, std::max(DL.getABITypeAlign(T).value(), AI->getAlign().value())),
          AI));
      AI->eraseFromParent();
      return;
    }

    if (LoadInst* LI = dyn_cast<LoadInst>(I)) {
      Type *T = LI->getType();
      Value* HighP = LI->getPointerOperand();
      Value* Result = loadValueRecurseAfterCheck(
        T, accessDataForOperand(HighP, LI, 0, LI).MAD, LI->isVolatile(),
        LI->getAlign(), LI->getOrdering(), LI->getSyncScopeID(), LI);
      LI->replaceAllUsesWith(Result);
      LI->eraseFromParent();
      return;
    }

    if (StoreInst* SI = dyn_cast<StoreInst>(I)) {
      Value* HighP = SI->getPointerOperand();
      storeValueRecurseAfterCheck(
        InstTypes[SI], SI->getValueOperand(), accessDataForOperand(HighP, SI, 0, SI).MAD,
        SI->isVolatile(), SI->getAlign(), SI->getOrdering(), SI->getSyncScopeID(), SI);
      SI->eraseFromParent();
      return;
    }

    if (isa<FenceInst>(I)) {
      // We don't need to do anything because it doesn't take operands.
      return;
    }

    if (AtomicCmpXchgInst* AI = dyn_cast<AtomicCmpXchgInst>(I)) {
      if (InstTypes[AI] == RawPtrTy) {
        storeOrigin(getOrigin(AI->getDebugLoc()), AI);
        Instruction* CAS = CallInst::Create(
          StrongCasPtr,
          { MyThread, AI->getPointerOperand(), ConstantInt::get(IntPtrTy, 0), AI->getCompareOperand(),
            AI->getNewValOperand() }, "filc_strong_cas_ptr", AI);
        StructType* ResultTy = StructType::get(C, { FlightPtrTy, Int1Ty });
        Instruction* Result = InsertValueInst::Create(
          UndefValue::get(ResultTy), CAS, { 0 }, "filc_cas_create_result_word", AI);
        Result->setDebugLoc(AI->getDebugLoc());
        Instruction* Equal = new ICmpInst(
          AI, ICmpInst::ICMP_EQ, flightPtrPtr(CAS, AI), flightPtrPtr(AI->getCompareOperand(), AI),
          "filc_cas_succeeded");
        Equal->setDebugLoc(AI->getDebugLoc());
        Result = InsertValueInst::Create(Result, Equal, { 1 }, "filc_cas_create_result_bit", AI);
        Result->setDebugLoc(AI->getDebugLoc());
        AI->replaceAllUsesWith(Result);
        AI->eraseFromParent();
        return;
      }
      assert(!hasPtrs(InstTypes[AI]));
      AI->getOperandUse(AtomicCmpXchgInst::getPointerOperandIndex()) =
        flightPtrPtr(AI->getPointerOperand(), AI);
      return;
    }

    if (AtomicRMWInst* AI = dyn_cast<AtomicRMWInst>(I)) {
      if (InstTypes[AI] == RawPtrTy) {
        assert(AI->getOperation() == AtomicRMWInst::Xchg);
        storeOrigin(getOrigin(AI->getDebugLoc()), AI);
        Instruction* Xchg = CallInst::Create(
          XchgPtr,
          { MyThread, AI->getPointerOperand(), ConstantInt::get(IntPtrTy, 0), AI->getValOperand() },
          "filc_xchg_ptr", AI);
        Xchg->setDebugLoc(AI->getDebugLoc());
        AI->replaceAllUsesWith(Xchg);
        AI->eraseFromParent();
        return;
      }
      AI->getOperandUse(AtomicRMWInst::getPointerOperandIndex()) =
        flightPtrPtr(AI->getPointerOperand(), AI);
      assert(!hasPtrs(InstTypes[AI]));
      return;
    }

    if (GetElementPtrInst* GI = dyn_cast<GetElementPtrInst>(I)) {
      Value* HighP = GI->getOperand(0);
      GI->getOperandUse(0) = flightPtrPtr(HighP, GI);
      if (GI->isInBounds())
        errs() << "GI is in bounds: " << *GI << "\n";
      assert(!GI->isInBounds()); // Should have been cleared by dropUB().
      hackRAUW(GI, [&] () { return flightPtrWithPtr(HighP, GI, GI->getNextNode()); });
      return;
    }

    if (ICmpInst* CI = dyn_cast<ICmpInst>(I)) {
      if (hasPtrs(CI->getOperand(0)->getType())) {
        CI->getOperandUse(0) = flightPtrPtr(CI->getOperand(0), CI);
        CI->getOperandUse(1) = flightPtrPtr(CI->getOperand(1), CI);
      }
      return;
    }

    if (isa<FCmpInst>(I) ||
        isa<BranchInst>(I) ||
        isa<SwitchInst>(I) ||
        isa<TruncInst>(I) ||
        isa<ZExtInst>(I) ||
        isa<SExtInst>(I) ||
        isa<FPTruncInst>(I) ||
        isa<FPExtInst>(I) ||
        isa<UIToFPInst>(I) ||
        isa<SIToFPInst>(I) ||
        isa<FPToUIInst>(I) ||
        isa<FPToSIInst>(I) ||
        isa<BinaryOperator>(I) ||
        isa<UnaryOperator>(I)) {
      // We're gucci.
      return;
    }

    if (isa<ReturnInst>(I)) {
      if (OldF->getReturnType() != VoidTy)
        ReturnPhi->addIncoming(I->getOperand(0), I->getParent());
      ReplaceInstWithInst(I, BranchInst::Create(ReturnB));
      return;
    }

    if (isa<CallBrInst>(I)) {
      llvm_unreachable("Don't support CallBr yet (and maybe never will)");
      return;
    }

    if (VAArgInst* VI = dyn_cast<VAArgInst>(I)) {
      Type* T = VI->getType();
      Type* CanonicalT = T;
      size_t Size = DL.getTypeAllocSize(CanonicalT);
      size_t Alignment = DL.getABITypeAlign(CanonicalT).value();
      storeOrigin(getOrigin(VI->getDebugLoc()), VI);
      // FIXME: We could optimize this by calling GetNextBytesForVAArg (sans the Ptr part) in cases
      // where we're loading a non-ptr type.
      CallInst* Call = CallInst::Create(
        GetNextPtrBytesForVAArg,
        { VI->getPointerOperand(), ConstantInt::get(IntPtrTy, Size),
          ConstantInt::get(IntPtrTy, Alignment) },
        "filc_va_arg", VI);
      Call->setDebugLoc(VI->getDebugLoc());
      Instruction* P = ExtractValueInst::Create(RawPtrTy, Call, { 0 }, "filc_ptr_pair_raw_ptr", VI);
      P->setDebugLoc(VI->getDebugLoc());
      Instruction* AuxP = ExtractValueInst::Create(
        RawPtrTy, Call, { 1 }, "filc_ptr_pair_aux_ptr", VI);
      P->setDebugLoc(VI->getDebugLoc());
      Value* Load = loadValueRecurseAfterCheck(
        CanonicalT, MemoryAccessData(nullptr, P, AuxP, AuxP, MemoryKind::Heap), false,
        DL.getABITypeAlign(CanonicalT), AtomicOrdering::NotAtomic, SyncScope::System, VI);
      VI->replaceAllUsesWith(Load);
      VI->eraseFromParent();
      return;
    }

    if (isa<ExtractElementInst>(I) ||
        isa<InsertElementInst>(I) ||
        isa<ShuffleVectorInst>(I) ||
        isa<ExtractValueInst>(I) ||
        isa<InsertValueInst>(I) ||
        isa<SelectInst>(I) ||
        isa<FreezeInst>(I)) {
      I->mutateType(toFlightType(I->getType()));
      return;
    }

    if (isa<LandingPadInst>(I)) {
      llvm_unreachable("Shouldn't see LandingPad because it should have been handled by "
                       "earlyLowerInstruction.");
      return;
    }

    if (isa<ResumeInst>(I)) {
      // NOTE: This function call is only necessary for checks. If our unwind machinery is working
      // correctly, then these checks should never fire. But I'm paranoid.
      CallInst::Create(ResumeUnwind, { MyThread, getOrigin(I->getDebugLoc()) }, "", I);
      BranchInst* BI = BranchInst::Create(ResumeB, I);
      BI->setDebugLoc(I->getDebugLoc());
      I->eraseFromParent();
      return;
    }

    if (isa<IndirectBrInst>(I)) {
      llvm_unreachable("Don't support IndirectBr yet (and maybe never will)");
      return;
    }

    if (isa<CatchSwitchInst>(I)) {
      llvm_unreachable("Don't support CatchSwitch yet");
      return;
    }

    if (isa<CleanupPadInst>(I)) {
      llvm_unreachable("Don't support CleanupPad yet");
      return;
    }

    if (isa<CatchPadInst>(I)) {
      llvm_unreachable("Don't support CatchPad yet");
      return;
    }

    if (isa<CatchReturnInst>(I)) {
      llvm_unreachable("Don't support CatchReturn yet");
      return;
    }

    if (isa<CleanupReturnInst>(I)) {
      llvm_unreachable("Don't support CleanupReturn yet");
      return;
    }

    if (isa<UnreachableInst>(I)) {
      CallInst::Create(
        Error, { getString("llvm unreachable instruction"), getOrigin(I->getDebugLoc()) }, "", I)
        ->setDebugLoc(I->getDebugLoc());
      return;
    }

    if (isa<IntToPtrInst>(I)) {
      hackRAUW(I, [&] () { return badFlightPtr(I, I->getNextNode()); });
      return;
    }

    if (isa<PtrToIntInst>(I)) {
      I->getOperandUse(0) = flightPtrPtr(I->getOperand(0), I);
      return;
    }

    if (isa<BitCastInst>(I)) {
      if (hasPtrs(I->getType())) {
        assert(hasPtrs(I->getOperand(0)->getType()));
        assert(I->getType() == RawPtrTy || I->getType() == FlightPtrTy);
        assert(I->getOperand(0)->getType() == RawPtrTy ||
               I->getOperand(0)->getType() == FlightPtrTy);
        I->replaceAllUsesWith(I->getOperand(0));
        I->eraseFromParent();
      } else
        assert(!hasPtrs(I->getOperand(0)->getType()));
      return;
    }

    if (isa<AddrSpaceCastInst>(I)) {
      if (hasPtrs(I->getType())) {
        if (hasPtrs(I->getOperand(0)->getType())) {
          I->replaceAllUsesWith(I->getOperand(0));
          I->eraseFromParent();
        } else
          hackRAUW(I, [&] () { return badFlightPtr(I, I->getNextNode()); });
      } else if (hasPtrs(I->getOperand(0)->getType()))
        I->getOperandUse(0) = flightPtrPtr(I->getOperand(0), I);
      return;
    }

    errs() << "Unrecognized instruction: " << *I << "\n";
    llvm_unreachable("Unknown instruction");
  }

  bool isSetjmp(Function* F) {
    return (F->getName() == "setjmp" ||
            F->getName() == "_setjmp" ||
            F->getName() == "sigsetjmp");
  }

  JmpBufKind getJmpBufKindForSetjmp(Function* F) {
    if (F->getName() == "setjmp")
      return JmpBufKind::setjmp;
    if (F->getName() == "_setjmp")
      return JmpBufKind::_setjmp;
    if (F->getName() == "sigsetjmp")
      return JmpBufKind::sigsetjmp;
    llvm_unreachable("Bad setjmp kind");
    return JmpBufKind::setjmp;
  }

  bool shouldPassThrough(Function* F) {
    return (F->getName() == "__divdc3" ||
            F->getName() == "__muldc3" ||
            F->getName() == "__divsc3" ||
            F->getName() == "__mulsc3" ||
            F->getName() == "__mulxc3" ||
            isSetjmp(F));
  }

  bool shouldPassThrough(GlobalVariable* G) {
    return (G->getName() == "llvm.global_ctors" ||
            G->getName() == "llvm.global_dtors" ||
            G->getName() == "llvm.used" ||
            G->getName() == "llvm.compiler.used");
  }

  bool shouldPassThrough(GlobalValue* G) {
    if (Function* F = dyn_cast<Function>(G))
      return shouldPassThrough(F);
    if (GlobalVariable* V = dyn_cast<GlobalVariable>(G))
      return shouldPassThrough(V);
    return false;
  }

  void stackOverflowCheck(Instruction* InsertBefore) {
    assert(MyThread);
    Value* GEP = threadStackLimitPtr(MyThread, InsertBefore);
    CallInst* CI = CallInst::Create(StackCheckAsm, { GEP }, "", InsertBefore);
    CI->addParamAttr(0, Attribute::get(C, Attribute::ElementType, RawPtrTy));
  }

  Value* flightPtrForLocalFunction(Function* F, Instruction* InsertBefore) {
    Constant* Lower = FunctionToLower[F];
    assert(Lower);
    return createFlightPtr(Lower, Lower, InsertBefore);
  }

  void lowerIndirectBrForFunction(Function& F) {
    // Code taken from IndirectBrExpandPass and modified.
    
    SmallVector<IndirectBrInst *, 1> IndirectBrs;

    // Set of all potential successors for indirectbr instructions.
    SmallPtrSet<BasicBlock *, 4> IndirectBrSuccs;

    // Build a list of indirectbrs that we want to rewrite.
    for (BasicBlock &BB : F)
      if (auto *IBr = dyn_cast<IndirectBrInst>(BB.getTerminator())) {
        // Handle the degenerate case of no successors by replacing the indirectbr
        // with unreachable as there is no successor available.
        if (IBr->getNumSuccessors() == 0) {
          (void)new UnreachableInst(F.getContext(), IBr);
          IBr->eraseFromParent();
          continue;
        }

        IndirectBrs.push_back(IBr);
        for (BasicBlock *SuccBB : IBr->successors())
          IndirectBrSuccs.insert(SuccBB);
      }

    if (IndirectBrs.empty())
      return;

    // If we need to replace any indirectbrs we need to establish integer
    // constants that will correspond to each of the basic blocks in the function
    // whose address escapes. We do that here and rewrite all the blockaddress
    // constants to just be those integer constants cast to a pointer type.
    SmallVector<BasicBlock *, 4> BBs;

    for (BasicBlock &BB : F) {
      // Skip blocks that aren't successors to an indirectbr we're going to
      // rewrite.
      if (!IndirectBrSuccs.count(&BB))
        continue;

      auto IsBlockAddressUse = [&](const Use &U) {
        return isa<BlockAddress>(U.getUser());
      };
      auto BlockAddressUseIt = llvm::find_if(BB.uses(), IsBlockAddressUse);
      if (BlockAddressUseIt == BB.use_end())
        continue;

      assert(std::find_if(std::next(BlockAddressUseIt), BB.use_end(),
                          IsBlockAddressUse) == BB.use_end() &&
             "There should only ever be a single blockaddress use because it is "
             "a constant and should be uniqued.");

      auto *BA = cast<BlockAddress>(BlockAddressUseIt->getUser());

      // Skip if the constant was formed but ended up not being used (due to DCE
      // or whatever).
      if (!BA->isConstantUsed())
        continue;

      // Compute the index we want to use for this basic block. We can't use zero
      // because null can be compared with block addresses.
      int BBIndex = BBs.size() + 1;
      BBs.push_back(&BB);

      auto *ITy = cast<IntegerType>(DL.getIntPtrType(BA->getType()));
      ConstantInt *BBIndexC = ConstantInt::get(ITy, BBIndex);

      // Now rewrite the blockaddress to an integer constant based on the index.
      // FIXME: This part doesn't properly recognize other uses of blockaddress
      // expressions, for instance, where they are used to pass labels to
      // asm-goto. This part of the pass needs a rework.
      BA->replaceAllUsesWith(ConstantExpr::getIntToPtr(BBIndexC, BA->getType()));
    }

    if (BBs.empty()) {
      for (auto *IBr : IndirectBrs) {
        (void)new UnreachableInst(F.getContext(), IBr);
        IBr->eraseFromParent();
      }
      return;
    }

    BasicBlock *SwitchBB;
    Value *SwitchValue;

    // Compute a common integer type across all the indirectbr instructions.
    IntegerType *CommonITy = nullptr;
    for (auto *IBr : IndirectBrs) {
      auto *ITy =
        cast<IntegerType>(DL.getIntPtrType(IBr->getAddress()->getType()));
      if (!CommonITy || ITy->getBitWidth() > CommonITy->getBitWidth())
        CommonITy = ITy;
    }

    auto GetSwitchValue = [CommonITy](IndirectBrInst *IBr) {
      return CastInst::CreatePointerCast(
        IBr->getAddress(), CommonITy,
        Twine(IBr->getAddress()->getName()) + ".switch_cast", IBr);
    };

    if (IndirectBrs.size() == 1) {
      // If we only have one indirectbr, we can just directly replace it within
      // its block.
      IndirectBrInst *IBr = IndirectBrs[0];
      SwitchBB = IBr->getParent();
      SwitchValue = GetSwitchValue(IBr);
      IBr->eraseFromParent();
    } else {
      // Otherwise we need to create a new block to hold the switch across BBs,
      // jump to that block instead of each indirectbr, and phi together the
      // values for the switch.
      SwitchBB = BasicBlock::Create(F.getContext(), "switch_bb", &F);
      auto *SwitchPN = PHINode::Create(CommonITy, IndirectBrs.size(),
                                       "switch_value_phi", SwitchBB);
      SwitchValue = SwitchPN;

      // Now replace the indirectbr instructions with direct branches to the
      // switch block and fill out the PHI operands.
      for (auto *IBr : IndirectBrs) {
        SwitchPN->addIncoming(GetSwitchValue(IBr), IBr->getParent());
        BranchInst::Create(SwitchBB, IBr);
        IBr->eraseFromParent();
      }
    }

    // Now build the switch in the block. The block will have no terminator
    // already.
    auto *SI = SwitchInst::Create(SwitchValue, BBs[0], BBs.size(), SwitchBB);

    // Add a case for each block.
    for (int i : llvm::seq<int>(1, BBs.size()))
      SI->addCase(ConstantInt::get(CommonITy, i + 1), BBs[i]);
  }

  void lowerIndirectBr() {
    std::vector<BlockAddress*> ToDelete;
    for (Function& F : M.functions()) {
      lowerIndirectBrForFunction(F);
      for (Value* User : F.users()) {
        if (BlockAddress* BA = dyn_cast<BlockAddress>(User))
          ToDelete.push_back(BA);
      }
    }
    for (BlockAddress* BA : ToDelete) {
      BA->replaceAllUsesWith(RawNull); // It's possible that a BA was used but not for indirectbr.
      BA->destroyConstant();
    }
  }
  
  void lockDownLinkage() {
    for (GlobalVariable& G : M.globals()) {
      if (G.getLinkage() == GlobalValue::AvailableExternallyLinkage) {
        G.setInitializer(nullptr);
        G.setLinkage(GlobalValue::ExternalLinkage);
      }
    }
    for (Function& F : M.functions()) {
      if (F.getLinkage() == GlobalValue::AvailableExternallyLinkage) {
        F.deleteBody();
        F.setIsMaterializable(false);
      }
    }

    for (GlobalValue& G : M.global_values()) {
      /* FIXME: Should be able to do something for AvailableExternally GlobalAliases and IFuncs. */
      assert(G.getLinkage() != GlobalValue::AvailableExternallyLinkage);

      /* FIXME: Should be possible to handle apppending linkage eventually. */
      assert(G.getLinkage() != GlobalValue::AppendingLinkage ||
             G.getName() == "llvm.global_ctors" ||
             G.getName() == "llvm.global_dtors" ||
             G.getName() == "llvm.used" ||
             G.getName() == "llvm.compiler.used");

      /* FIXME: Don't even know what this is? */
      assert(G.getLinkage() != GlobalValue::CommonLinkage);

      if (G.getLinkage() == GlobalValue::LinkOnceODRLinkage)
        G.setLinkage(GlobalValue::LinkOnceAnyLinkage);
      else if (G.getLinkage() == GlobalValue::WeakODRLinkage)
        G.setLinkage(GlobalValue::WeakAnyLinkage);
    }
  }

  void expandConstantExprOperands(Instruction* I) {
    for (unsigned Index = I->getNumOperands(); Index--;) {
      Instruction* InsertBefore;
      if (PHINode* P = dyn_cast<PHINode>(I))
        InsertBefore = P->getIncomingBlock(Index)->getTerminator();
      else
        InsertBefore = I;
      
      Use& U = I->getOperandUse(Index);
      if (ConstantExpr* CE = dyn_cast<ConstantExpr>(U)) {
        Instruction* NewI = getAsInstruction(CE);
        NewI->insertBefore(InsertBefore);
        expandConstantExprOperands(NewI);
        U = NewI;
      }
    }
  }

  void expandConstantExprs() {
    for (Function& F : M.functions()) {
      if (F.isDeclaration())
        continue;
      
      for (BasicBlock& BB : F) {
        std::vector<Instruction*> Insts;
        for (Instruction& I : BB)
          Insts.push_back(&I);
        for (Instruction* I : Insts)
          expandConstantExprOperands(I);
      }
    }
  }

  void inferPointerAsIntLaunderingForFunction(Function& F) {
    if (verbose)
      errs() << "Inferring pointer-as-int laundering in:\n" << F << "\n";
    
    // If an inttoptr's inputs unambiguously lead to a single ptrtoint, then we can take that
    // ptrtoint's capability.
    //
    // If an inttoptr's inputs unambigiously lead to a phi and that phi knows how to account for the
    // capability of each of its inputs, then we can construct a phi to collect those capabilities. In
    // fact, we can even construct such a phi if some of the inputs don't have capabilities.
    //
    // This suggests a simple kind of fixpoint where for each instruction, we either:
    //
    // - Don't know whether we can deduce the capability for that instruction because we haven't
    //   considered it yet, or we know that there are zero possible capabilities (BOTTOM).
    // - Know that there is a capability for that instruction.
    // - Know that there is a conflict of capabilities for that instruction (TOP).
    //
    // It's cool how easy it is to write this as an abstract interpreter.

    std::unordered_map<Instruction*, InferredCapability> InferredCapabilities;

    std::vector<Instruction*> AllInsts;
    for (BasicBlock& BB : F) {
      for (Instruction& I : BB)
        AllInsts.push_back(&I);
    }
    std::vector<Instruction*> InferInsts;
    for (Instruction* I : AllInsts) {
      if (PtrToIntInst* PtrToInt = dyn_cast<PtrToIntInst>(I)) {
        InferredCapabilities[I] = InferredCapability(
          CapabilityInferenceState::Definite, PtrToInt->getPointerOperand());
        continue;
      }
      if (!I->getType()->isIntegerTy())
        continue;
      // Maybe we should cause all casts to lose the capability, but it's not obvious. For sure,
      // truncing is suspect.
      if (isa<CallBase>(I) || isa<LoadInst>(I) || isa<AtomicCmpXchgInst>(I) ||
          isa<AtomicRMWInst>(I) || isa<CmpInst>(I) || isa<VAArgInst>(I) ||
          isa<ExtractElementInst>(I) || isa<ExtractValueInst>(I) || isa<LandingPadInst>(I) ||
          isa<FPToUIInst>(I) || isa<FPToSIInst>(I)) {
        InferredCapabilities[I] = InferredCapability(CapabilityInferenceState::Bottom, nullptr);
        continue;
      }
      InferInsts.push_back(I);
    }

    bool Changed = true;
    while (Changed) {
      Changed = false;
      for (Instruction* I : InferInsts) {
        for (Value* V : I->operands()) {
          if (Instruction* I2 = dyn_cast<Instruction>(V)) {
            InferredCapability I2Cap = InferredCapabilities[I2];
            if ((isa<PHINode>(I) || isa<SelectInst>(I))
                && I2Cap.State != CapabilityInferenceState::Bottom) {
              InferredCapability ICap = InferredCapabilities[I];
              assert(ICap.State == CapabilityInferenceState::Bottom ||
                     ICap.State == CapabilityInferenceState::Definite);
              Value* Capability = ICap.Capability;
              if (!Capability) {
                assert(ICap.State == CapabilityInferenceState::Bottom);
                if (PHINode* Phi = dyn_cast<PHINode>(I)) {
                  PHINode* NewPhi = PHINode::Create(
                    RawPtrTy, Phi->getNumOperands(), "filc_capability_phi", Phi);
                  NewPhi->setDebugLoc(Phi->getDebugLoc());
                  Capability = NewPhi;
                } else {
                  SelectInst* Select = dyn_cast<SelectInst>(I);
                  assert(Select);
                  SelectInst* NewSelect = SelectInst::Create(
                    Select->getCondition(), RawNull, RawNull, "filc_capability_select", Select);
                  Capability = NewSelect;
                }
              }
              assert(Capability);
              I2Cap = InferredCapability(CapabilityInferenceState::Definite, Capability);
            }
            Changed |= InferredCapabilities[I].merge(I2Cap);
          }
        }
      }
    }
    
    auto CapabilityOf = [&] (Value* Incoming) -> Value* {
      Instruction* I2 = dyn_cast<Instruction>(Incoming);
      if (!I2)
        return RawNull;
      InferredCapability I2Cap = InferredCapabilities[I2];
      if (I2Cap.Capability) {
        assert(I2Cap.State == CapabilityInferenceState::Definite);
        assert(I2Cap.Capability->getType() == RawPtrTy);
        return I2Cap.Capability;
      }
      return RawNull;
    };
      
    for (auto& Pair : InferredCapabilities) {
      Instruction* I = Pair.first;
      InferredCapability ICap = Pair.second;
      if (!ICap.Capability)
        continue;

      if (PHINode* Phi = dyn_cast<PHINode>(I)) {
        PHINode* NewPhi = cast<PHINode>(ICap.Capability);
        for (unsigned Index = Phi->getNumIncomingValues(); Index--;) {
          NewPhi->addIncoming(CapabilityOf(Phi->getIncomingValue(Index)),
                              Phi->getIncomingBlock(Index));
        }
      }
      if (SelectInst* Select = dyn_cast<SelectInst>(I)) {
        SelectInst* NewSelect = cast<SelectInst>(ICap.Capability);
        NewSelect->setTrueValue(CapabilityOf(Select->getTrueValue()));
        NewSelect->setFalseValue(CapabilityOf(Select->getFalseValue()));
      }
    }

    for (Instruction* I : AllInsts) {
      IntToPtrInst* IntToPtr = dyn_cast<IntToPtrInst>(I);
      if (!IntToPtr)
        continue;

      Value* Capability = CapabilityOf(IntToPtr->getOperand(0));
      if (Capability == RawNull)
        continue;
      
      Instruction* PtrToInt = new PtrToIntInst(Capability, IntPtrTy, "filc_ptr_to_int", I);
      PtrToInt->setDebugLoc(I->getDebugLoc());
      Instruction* Neg = BinaryOperator::Create(
        Instruction::Sub, ConstantInt::get(IntPtrTy, 0), PtrToInt, "filc_ptr_neg", I);
      Neg->setDebugLoc(I->getDebugLoc());
      Instruction* Sub = GetElementPtrInst::Create(Int8Ty, Capability, { Neg }, "filc_ptr_sub", I);
      Sub->setDebugLoc(I->getDebugLoc());
      Instruction* Add = GetElementPtrInst::Create(Int8Ty, Sub, { IntToPtr->getOperand(0) },
                                                   "filc_ptr_add", I);
      Add->setDebugLoc(I->getDebugLoc());
      IntToPtr->replaceAllUsesWith(Add);
      IntToPtr->eraseFromParent();
    }
  }

  void inferPointerAsIntLaundering() {
    for (Function& F : M.functions()) {
      if (!F.isDeclaration())
        inferPointerAsIntLaunderingForFunction(F);
    }
  }

  void makeEHDatas() {
    std::vector<LandingPadInst*> LPIs;
    
    for (Function& F : M.functions()) {
      for (BasicBlock& BB : F) {
        LandingPadInst* LPI = BB.getLandingPadInst();
        if (LPI)
          LPIs.push_back(LPI);
      }
    }

    if (LPIs.empty())
      return;
    
    std::vector<Constant*> LowTypesAndFilters;
    unsigned NumTypes = 0;
    unsigned NumFilters = 0;
    std::unordered_map<Constant*, int> TypeOrFilterToAction;

    for (LandingPadInst* LPI : LPIs) {
      for (unsigned Idx = LPI->getNumClauses(); Idx--;) {
        if (!LPI->isCatch(Idx))
          continue;

        if (TypeOrFilterToAction.count(LPI->getClause(Idx)))
          continue;

        int EHTypeID = NumTypes++ + 1;
        TypeOrFilterToAction[LPI->getClause(Idx)] = EHTypeID;
        EHTypeIDs[LPI->getClause(Idx)] = EHTypeID;
        LowTypesAndFilters.push_back(LPI->getClause(Idx));
      }
    }

    for (LandingPadInst* LPI : LPIs) {
      for (unsigned ClauseIdx = LPI->getNumClauses(); ClauseIdx--;) {
        if (!LPI->isFilter(ClauseIdx))
          continue;

        Constant* C = LPI->getClause(ClauseIdx);
        if (TypeOrFilterToAction.count(C))
          continue;

        StructType* FilterTy;
        Constant* FilterCS;
        if (cast<ArrayType>(C->getType())->getNumElements()) {
          FilterTy = StructType::get(this->C, { Int32Ty, C->getType() });
          FilterCS = ConstantStruct::get(
            FilterTy,
            { ConstantInt::get(Int32Ty, cast<ArrayType>(C->getType())->getNumElements()), C });
        } else {
          FilterTy = StructType::get(this->C, ArrayRef<Type*>(Int32Ty));
          FilterCS = ConstantStruct::get(FilterTy, ConstantInt::get(Int32Ty, 0));
        }
        GlobalVariable* LowC = new GlobalVariable(
          M, FilterTy, true, GlobalValue::PrivateLinkage, FilterCS, "filc_eh_filter");
        LowC->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
        TypeOrFilterToAction[C] = -(NumFilters++ + 1);
        LowTypesAndFilters.push_back(LowC);
      }
    }

    Constant* TypeTableC;
    if (LowTypesAndFilters.empty())
      TypeTableC = RawNull;
    else {
      ArrayType* TypeTableArrayTy = ArrayType::get(RawPtrTy, LowTypesAndFilters.size());
      StructType* TypeTableTy = StructType::get(C, { Int32Ty, TypeTableArrayTy });
      Constant* TypeTableCS = ConstantStruct::get(
        TypeTableTy,
        { ConstantInt::get(Int32Ty, NumTypes),
          ConstantArray::get(TypeTableArrayTy, LowTypesAndFilters) });
      GlobalVariable* TypeTableG = new GlobalVariable(
        M, TypeTableTy, true, GlobalValue::PrivateLinkage, TypeTableCS, "filc_type_table");
      TypeTableG->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
      TypeTableC = TypeTableG;
    }

    for (LandingPadInst* LPI : LPIs) {
      if (EHDatas.count(EHDataKey(LPI)))
        continue;

      std::vector<int> Actions;

      for (unsigned Idx = 0; Idx < LPI->getNumClauses(); ++Idx) {
        assert(TypeOrFilterToAction.count(LPI->getClause(Idx)));
        Actions.push_back(TypeOrFilterToAction[LPI->getClause(Idx)]);
      }
      
      if (LPI->isCleanup())
        Actions.push_back(0);

      StructType* EHDataTy = StructType::get(
        C, { RawPtrTy, Int32Ty, ArrayType::get(Int32Ty, Actions.size()) });
      Constant* EHDataCS = ConstantStruct::get(
        EHDataTy,
        { TypeTableC, ConstantInt::get(Int32Ty, Actions.size()), ConstantDataArray::get(C, Actions) });
      EHDatas[EHDataKey(LPI)] = new GlobalVariable(
        M, EHDataTy, true, GlobalValue::PrivateLinkage, EHDataCS, "filc_eh_data");
      EHDatas[EHDataKey(LPI)]->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
      if (verbose)
        errs() << "For " << *LPI << " created eh data: " << *EHDatas[EHDataKey(LPI)] << "\n";
    }
  }

  GlobalValue* getGlobal(StringRef Name) {
    if (GlobalVariable* GV = M.getNamedGlobal(Name))
      return GV;
    if (GlobalAlias* GA = M.getNamedAlias(Name))
      return GA;
    if (Function* F = M.getFunction(Name))
      return F;
    return nullptr;
  }

  void compileModuleAsm() {
    MATokenizer MAT(M, M.getModuleInlineAsm());

    std::ostringstream NewModuleAsm;

    for (;;) {
      MAToken Tok = MAT.getNext();
      if (Tok.Kind == MATokenKind::None)
        break;
      if (Tok.Kind == MATokenKind::Error) {
        errs() << "Cannot parse module asm: " << Tok.Str << "\n";
        MAT.error();
      }
      if (Tok.Kind == MATokenKind::EndLine)
        continue;
      if (Tok.Kind == MATokenKind::Directive) {
        if (Tok.Str == ".filc_weak" ||
            Tok.Str == ".filc_globl") {
          std::string Name = MAT.getNextSpecific(MATokenKind::Identifier).Str;
          MAT.getNextSpecific(MATokenKind::EndLine);
          GlobalValue* GV = getGlobal(Name);
          if (Tok.Str == ".filc_weak") {
            if (!GV) {
              errs() << "Cannot do .filc_weak to " << Name << " because it doesn't exist.\n";
              MAT.error();
            }
            GV->setLinkage(GlobalValue::WeakAnyLinkage);
          } else {
            assert(Tok.Str == ".filc_globl");
            if (GV)
              GV->setLinkage(GlobalValue::ExternalLinkage);
            NewModuleAsm << ".globl pizlonated_" << Name << "\n";
          }
          continue;
        }
        if (Tok.Str == ".filc_weak_alias" ||
            Tok.Str == ".filc_alias") {
          std::string OldName = MAT.getNextSpecific(MATokenKind::Identifier).Str;
          MAToken PlusTok = MAT.getNext();
          int64_t Offset = 0;
          if (PlusTok.Kind == MATokenKind::Plus) {
            std::string OffsetAsStr = MAT.getNextSpecific(MATokenKind::Integer).Str;
            int Result = sscanf(OffsetAsStr.c_str(), "%ld", &Offset);
            assert(Result == 1);
            MAT.getNextSpecific(MATokenKind::Comma);
          } else if (PlusTok.Kind != MATokenKind::Comma) {
            errs() << "Expected plus or comma but got: " << PlusTok.Str << "\n";
            MAT.error();
          }
          std::string NewName = MAT.getNextSpecific(MATokenKind::Identifier).Str;
          MAT.getNextSpecific(MATokenKind::EndLine);
          GlobalValue* GV = getGlobal(OldName);
          Type* T;
          GlobalValue* ExistingGV = getGlobal(NewName);
          if (ExistingGV) {
            T = ExistingGV->getValueType();
            if (!ExistingGV->isDeclaration()) {
              if (GlobalAlias* GA = dyn_cast<GlobalAlias>(ExistingGV)) {
                // FIXME: Also handle coalescing with existing global aliases even if there's an
                // offset (and the offset matches).
                GlobalValue* TargetGV = dyn_cast<GlobalValue>(GA->getAliasee());
                if (!Offset &&
                    (Tok.Str == ".filc_weak_alias") == (GA->hasWeakAnyLinkage() ||
                                                        GA->hasExternalWeakLinkage()) &&
                    TargetGV &&
                    TargetGV->getName() == OldName)
                  continue;
              }
              errs() << "New alias name " << NewName << " is already taken by a definition.\n";
              MAT.error();
            }
            assert(!Dummy->getNumUses());
            if (!GV) {
              if (isa<Function>(ExistingGV)) {
                assert(isa<FunctionType>(T));
                GV = Function::Create(
                  cast<FunctionType>(T), GlobalValue::ExternalLinkage, OldName, &M);
              } else {
                assert(isa<GlobalVariable>(ExistingGV));
                GV = new GlobalVariable(M, T, false, GlobalValue::ExternalLinkage, nullptr, OldName);
              }
            }
            ExistingGV->replaceAllUsesWith(Dummy);
            ExistingGV->eraseFromParent();
          } else if (GV)
            T = GV->getValueType();
          else {
            T = Int8Ty;
            GV = new GlobalVariable(M, Int8Ty, false, GlobalValue::ExternalLinkage, nullptr, OldName);
          }
          GlobalValue::LinkageTypes Linkage;
          if (Tok.Str == ".filc_weak_alias")
            Linkage = GlobalValue::WeakAnyLinkage;
          else {
            assert(Tok.Str == ".filc_alias");
            Linkage = GlobalValue::ExternalLinkage;
          }
          GlobalAlias* NewGA = GlobalAlias::create(
            T, GV->getAddressSpace(), Linkage, NewName,
            ConstantExpr::getGetElementPtr(Int8Ty, GV, ConstantInt::get(IntPtrTy, Offset)), &M);
          if (ExistingGV)
            Dummy->replaceAllUsesWith(NewGA);
          continue;
        }
        if (Tok.Str == ".filc_unsafe_export") {
          std::string Name = MAT.getNextSpecific(MATokenKind::Identifier).Str;
          MAT.getNextSpecific(MATokenKind::EndLine);
          GlobalValue* GV = getGlobal(Name);
          assert(isa<GlobalVariable>(GV));
          assert(!GV->isThreadLocal());
          UnsafeExportGVs.insert(cast<GlobalVariable>(GV));
          continue;
        }
        if (Tok.Str == ".filc_rename") {
          std::string OldName = MAT.getNextSpecific(MATokenKind::Identifier).Str;
          MAT.getNextSpecific(MATokenKind::Comma);
          std::string NewName = MAT.getNextSpecific(MATokenKind::Identifier).Str;
          MAT.getNextSpecific(MATokenKind::EndLine);
          GlobalValue* GV = getGlobal(OldName);
          if (!GV) {
            // If the source symbol doesn't exist, then it's likely because it's been killed by some
            // kind of tree shake. Ignore it!
            continue;
          }
          GV->setName(NewName);
          if (GV->getName() != NewName) {
            errs() << "Cannot rename " << OldName << " to " << NewName;
            if (getGlobal(NewName))
              errs() << " (because " << NewName << " is taken)";
            errs() << "\n";
            MAT.error();
          }
          continue;
        }
        if (Tok.Str == ".symver" || Tok.Str == ".filc_symver") {
          std::string LocalName = MAT.getNextSpecific(MATokenKind::Identifier).Str;
          MAT.getNextSpecific(MATokenKind::Comma);
          std::string VersionedName = MAT.getNextSpecific(MATokenKind::Identifier).Str;
          MAT.getNextSpecific(MATokenKind::EndLine);
          NewModuleAsm << ".symver pizlonated_" << LocalName << ", pizlonated_" << VersionedName
                       << "\n";
          continue;
        }
        errs() << "Invalid directive: " << Tok.Str << "\n";
        MAT.error();
      }
      errs() << "Unexpected token: " << Tok.Str << "\n";
      MAT.error();
    }

    M.setModuleInlineAsm(NewModuleAsm.str());
  }

  LifetimeMarker analyzeLifetimeMarker(Value* V) {
    LifetimeIntrinsic* LI = dyn_cast<LifetimeIntrinsic>(V);
    if (!LI)
      return LifetimeMarker();

    AllocaInst* AI = dyn_cast<AllocaInst>(LI->getArgOperand(1));
    if (!AI)
      return LifetimeMarker();

    if (!allocaHasSizeForUs(AI))
      return LifetimeMarker();

    ConstantInt* MarkerSize = dyn_cast<ConstantInt>(LI->getArgOperand(0));
    if (!MarkerSize)
      return LifetimeMarker();

    int64_t MarkerSizeInt = MarkerSize->getSExtValue();
    if (MarkerSizeInt != -1 && static_cast<uint64_t>(MarkerSizeInt) != originalAllocaSize(AI))
      return LifetimeMarker();

    switch (LI->getIntrinsicID()) {
    case Intrinsic::lifetime_start:
      return LifetimeMarker(AI, LifetimeMarkerKind::Start);
    case Intrinsic::lifetime_end:
      return LifetimeMarker(AI, LifetimeMarkerKind::End);
    default:
      llvm_unreachable("Bad lifetime marker");
      return LifetimeMarker();
    }
  }

  void findNonescapingAllocasInFunction(Function& F) {
    if (F.isDeclaration())
      return;

    if (verbose) {
      errs() << "Finding nonescaping allocas in:\n";
      errs() << F;
    }

    if (F.callsFunctionThatReturnsTwice()) {
      // Punt on this analysis because we currently don't have a story for how we would soundly handle
      // GC of pointers inside of nonescaping allocas if you longjmped.
      //
      // Note that it's the kind of thing that maybe just works or is easy to fix. I just didn't want
      // to get bogged down by this case when implementing the nonescaping alloca optimization.
      for (BasicBlock& BB : F) {
        for (Instruction& I : BB) {
          if (AllocaInst* AI = dyn_cast<AllocaInst>(&I))
            AllocaKinds[AI] = PointerKind::Escaping;
        }
      }
      if (verbose)
        errs() << "Giving up due to call to functions that returns twice\n.";
      return;
    }

    std::unordered_set<AllocaInst*> Allocas;

    for (BasicBlock& BB : F) {
      for (Instruction& I : BB) {
        if (AllocaInst* AI = dyn_cast<AllocaInst>(&I)) {
          // Non-static allocas might reexecute any number of times, so I don't think they can
          // qualify for this analysis. With enough effort, they could probably be made to. But then
          // we'd have other problems, like having to find a way to detect stack overflow.
          //
          // FIXME: Is the size limit I'm picking for the largest alloca we'll escape analyze sensible?
          // Maybe it should be a smaller size?
          if (verbose) {
            errs() << "Considering " << AI->getName() << "\n";
            errs() << "    Is static: " << AI->isStaticAlloca() << "\n";
            errs() << "    Has size for us: " << allocaHasSizeForUs(AI) << "\n";
            if (allocaHasSizeForUs(AI))
              errs() << "    Size: " << originalAllocaSize(AI) << "\n";
          }
          if (AI->isStaticAlloca() && allocaHasSizeForUs(AI)
              && originalAllocaSize(AI) <= MaxBytesBetweenPollchecks) {
            bool HasLifetimeStart = false;
            bool HasLifetimeEnd = false;
            for (User* U : AI->users()) {
              if (verbose)
                errs() << "    Considering user: " << *U << "\n";
              if (LifetimeMarker LM = analyzeLifetimeMarker(U)) {
                if (verbose)
                  errs() << "    Is lifetime marker.\n";
                assert(LM.AI == AI);
                switch (LM.LMK) {
                case LifetimeMarkerKind::Start:
                  HasLifetimeStart = true;
                  break;
                case LifetimeMarkerKind::End:
                  HasLifetimeEnd = true;
                  break;
                }
              }
            }
            if (HasLifetimeStart == HasLifetimeEnd) {
              if (verbose)
                errs() << "Going to try to see if " << AI->getName() << " is nonescaping.\n";
              AllocaKinds[AI] = PointerKind::LocalNaked;
              if (!HasLifetimeStart)
                AlwaysLive.insert(AI);
              Allocas.insert(AI);
              continue;
            }
          }

          if (verbose)
            errs() << "Assuming that " << AI->getName() << " is escaping.\n";
          AllocaKinds[AI] = PointerKind::Escaping;
        }
      }
    }

    // Do our own stack lifetime analysis. We do this, instead of using StackLifetime, because when we
    // go to transform the nonescaping allocas we need to preserve the stack lifetime markers across
    // the pizlonation lowering. To do that, we have to confine ourselves to understanding just exactly
    // those lifetime markers that we would then know how to pizlonate.
    //
    // Also, I kinda suspect this lifetime analysis is faster. And it's less code. Vive
    // l'interprétation abstraite!

    std::unordered_map<BasicBlock*, std::unordered_map<AllocaInst*, LifetimeState>> LifetimeAtTail;
    for (BasicBlock& BB : F) {
      LifetimeState LS;
      if (!BB.getTerminator()->getNumSuccessors())
        LS = LifetimeState::Zombie;
      else
        LS = LifetimeState::Undetermined;
      for (AllocaInst* AI : Allocas) {
        if (!AlwaysLive.count(AI))
          LifetimeAtTail[&BB][AI] = LS;
      }
    }
    
    auto ExecuteLifetime = [&] (std::unordered_map<AllocaInst*, LifetimeState>& Lifetime,
                                Instruction* I) {
      LifetimeMarker LM = analyzeLifetimeMarker(I);
      if (!LM)
        return;
      assert(Lifetime.count(LM.AI) == Allocas.count(LM.AI));
      if (!Lifetime.count(LM.AI))
        return;
      switch (LM.LMK) {
      case LifetimeMarkerKind::Start:
        Lifetime[LM.AI] = LifetimeState::Zombie;
        break;
      case LifetimeMarkerKind::End:
        Lifetime[LM.AI] = LifetimeState::Live;
        break;
      }
    };
    
    std::vector<BasicBlock*> Blocks;
    for (BasicBlock& BB : F)
      Blocks.push_back(&BB);
    bool Changed = true;
    while (Changed) {
      Changed = false;
      for (size_t Index = Blocks.size(); Index--;) {
        BasicBlock* BB = Blocks[Index];
        std::unordered_map<AllocaInst*, LifetimeState> Lifetime = LifetimeAtTail[BB];
        if (verbose) {
          errs() << "Liveness at tail of " << BB->getName() << ":";
          for (auto& Pair : Lifetime)
            errs() << " " << Pair.first->getName() << "=" << Pair.second;
          errs() << "\n";
        }
        for (auto It = BB->rbegin(); It != BB->rend(); ++It) {
          Instruction* I = &*It;
          ExecuteLifetime(Lifetime, I);
        }
        if (verbose) {
          errs() << "Liveness at head of " << BB->getName() << ":";
          for (auto& Pair : Lifetime)
            errs() << " " << Pair.first->getName() << "=" << Pair.second;
          errs() << "\n";
        }
        if (pred_empty(BB)) {
          for (auto& Pair : Lifetime) {
            AllocaInst* AI = Pair.first;
            LifetimeState LS = Pair.second;
            if (LS == LifetimeState::Live)
              AllocaKinds[AI] = PointerKind::Escaping;
          }
        }
        for (BasicBlock* PBB : predecessors(BB)) {
          for (auto& Pair : Lifetime) {
            AllocaInst* AI = Pair.first;
            LifetimeState LS = Pair.second;
            LifetimeState MergedLS = mergeLifetimeState(LifetimeAtTail[PBB][AI], LS);
            if (MergedLS != LifetimeAtTail[PBB][AI]) {
              Changed = true;
              LifetimeAtTail[PBB][AI] = MergedLS;
            }
          }
        }
      }
    }

    auto mergeKind = [&] (Value* P, PointerKind Kind) -> bool {
      P = underlyingPtr(P).P;
      AllocaInst* AI = dyn_cast<AllocaInst>(P);
      if (!AI)
        return false;
      if (!Allocas.count(AI))
        return false;
      PointerKind OldKind = AllocaKinds[AI];
      PointerKind NewKind = mergePointerKinds(OldKind, Kind);
      if (OldKind == NewKind)
        return false;
      AllocaKinds[AI] = NewKind;
      return true;
    };

    std::vector<CallBase*> MemmovesToReconsider;
    for (BasicBlock& BB : F) {
      std::unordered_map<AllocaInst*, LifetimeState> Lifetime = LifetimeAtTail[&BB];
      for (auto It = BB.rbegin(); It != BB.rend(); ++It) {
        Instruction* I = &*It;
        if (verbose)
          errs() << "Escaping analysis considering " << *I << "\n";
        
        if (isa<LifetimeIntrinsic>(I)) {
          ExecuteLifetime(Lifetime, I);
          continue;
        }
        
        if (CallBase* CI = dyn_cast<CallBase>(I)) {
          if (Function* F = dyn_cast<Function>(CI->getCalledOperand())) {
            FunctionType* FT = CI->getFunctionType();
            if (F->getName() == "zhas_union" && isHasUnionFT(FT))
              continue;
          }
        }

        for (Value* V : I->operands()) {
          PtrAndRandom PAR = underlyingPtr(V);
          AllocaInst* AI = dyn_cast<AllocaInst>(PAR.P);
          if (AI && Allocas.count(AI)) {
            if (AllocaKinds[AI] != PointerKind::Escaping &&
                Lifetime[AI] != LifetimeState::Live && !AlwaysLive.count(AI)) {
              if (verbose)
                errs() << "Escaping " << AI->getName() << " because it's used and not live.\n";
              AllocaKinds[AI] = PointerKind::Escaping;
            } else if (PAR.R == PtrRandomness::Random) {
              if (verbose)
                errs() << "Making explicit " << AI->getName() << " because it's random accessed.\n";
              AllocaKinds[AI] = mergePointerKinds(AllocaKinds[AI], PointerKind::LocalExplicit);
            }
          }
        }
        
        if (isa<GetElementPtrInst>(I))
          continue;

        if (isInlineableMemmoveCall(I)) {
          MemmovesToReconsider.push_back(cast<CallBase>(I));
          continue;
        }

        if (isNonescapingMemmoveCall(I)) {
          if (verbose) {
            errs() << "Making explicit " << cast<CallBase>(I)->getArgOperand(0)->getName() << " and "
                   << cast<CallBase>(I)->getArgOperand(1)->getName() << " because they're random "
                   << "accessed.\n";
          }
          mergeKind(cast<CallBase>(I)->getArgOperand(0), PointerKind::LocalExplicit);
          mergeKind(cast<CallBase>(I)->getArgOperand(1), PointerKind::LocalExplicit);
          continue;
        }

        if (LoadInst* LI = dyn_cast<LoadInst>(I)) {
          if (!LI->isVolatile())
            continue;
        }
        
        if (StoreInst* SI = dyn_cast<StoreInst>(I)) {
          if (!SI->isVolatile()) {
            if (verbose)
              errs() << "Escaping " << SI->getValueOperand()->getName() << " because it's stored.\n";
            mergeKind(SI->getValueOperand(), PointerKind::Escaping);
            continue;
          }
        }

        // FIXME: We could include AtomicRMW and CAS instructions here.
        
        for (Value* V : I->operands()) {
          if (verbose)
            errs() << "Escaping " << V->getName() << " because it's used in a shady way.\n";
          mergeKind(V, PointerKind::Escaping);
        }
      }
    }

    Changed = true;
    while (Changed) {
      Changed = false;
      for (CallBase* CI : MemmovesToReconsider) {
        // If the destination does not escape, then we can emit inline code for memmoves, and so
        // it's fine if the memmove operands are naked.
        if (underlyingPointerKind(CI->getArgOperand(0)) != PointerKind::Escaping)
          continue;
        // If the destination does escape but the size of the memmove is small enough for us to emit
        // the slow path code, then it's fine if the memmove operands are naked.
        if (cast<ConstantInt>(CI->getArgOperand(2))->getZExtValue() <= InlineMemmoveDstSizeLimit)
          continue;
        // We're memmoving to an escaping destination with a size that the inline memmove slow path
        // cannot handle on the destination side. So, force the source to be explicit. This is still
        // nonescaping, but it just means that the alloca really is stack allocated.
        Changed |= mergeKind(CI->getArgOperand(1), PointerKind::LocalExplicit);
      }
    }

    if (verbose) {
      errs() << "Escape analysis result:";
      for (auto& Pair : AllocaKinds)
        errs() << " " << Pair.first->getName() << "=" << Pair.second;
      errs() << "\n";
    }
  }

  void findNonescapingAllocas() {
    for (Function& F : M.functions())
      findNonescapingAllocasInFunction(F);
  }

  void removeIrrelevantIntrinsics() {
    for (Function& F : M) {
      if (F.isDeclaration())
        continue;
      for (BasicBlock& BB : F) {
        std::vector<Instruction*> ToErase;
        for (Instruction& I : BB) {
          CallBase* CI = dyn_cast<CallBase>(&I);
          if (!CI)
            continue;
          Function* Callee = dyn_cast<Function>(CI->getCalledOperand());
          if (!Callee || !Callee->isIntrinsic())
            continue;
          bool ShouldErase = false;
          switch (Callee->getIntrinsicID()) {
          case Intrinsic::stackrestore:
          case Intrinsic::assume:
          case Intrinsic::dbg_declare:
          case Intrinsic::dbg_value:
          case Intrinsic::dbg_assign:
          case Intrinsic::dbg_label:
          case Intrinsic::donothing:
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
          case Intrinsic::stacksave:
            CI->replaceAllUsesWith(RawNull);
            ShouldErase = true;
            break;
          default:
            break;
          }
          if (ShouldErase) {
            if (InvokeInst* II = dyn_cast<InvokeInst>(CI))
              BranchInst::Create(II->getNormalDest(), CI)->setDebugLoc(CI->getDebugLoc());
            ToErase.push_back(CI);
          }
        }
        for (Instruction* I : ToErase) {
          if (verbose)
            errs() << "Removing " << *I << "\n";
          I->eraseFromParent();
        }
      }
    }
  }

  void removeLifetimeIntrinsics() {
    for (Function& F : M) {
      if (F.isDeclaration())
        continue;
      for (BasicBlock& BB : F) {
        std::vector<Instruction*> ToErase;
        for (Instruction& I : BB) {
          CallBase* CI = dyn_cast<CallBase>(&I);
          if (!CI)
            continue;
          Function* Callee = dyn_cast<Function>(CI->getCalledOperand());
          if (!Callee || !Callee->isIntrinsic())
            continue;
          bool ShouldErase = false;
          switch (Callee->getIntrinsicID()) {
          case Intrinsic::lifetime_start:
          case Intrinsic::lifetime_end:
            if (LifetimeMarker LM = analyzeLifetimeMarker(CI)) {
              assert(AllocaKinds.count(LM.AI));
              if (AllocaKinds[LM.AI] != PointerKind::Escaping)
                break;
            }
            ShouldErase = true;
            break;
          default:
            break;
          }
          if (ShouldErase) {
            if (InvokeInst* II = dyn_cast<InvokeInst>(CI))
              BranchInst::Create(II->getNormalDest(), CI)->setDebugLoc(CI->getDebugLoc());
            ToErase.push_back(CI);
          }
        }
        for (Instruction* I : ToErase)
          I->eraseFromParent();
      }
    }
  }

  void lazifyAllocasInFunction(Function& F) {
    if (F.isDeclaration())
      return;
    
    std::unordered_set<AllocaInst*> Allocas;

    for (Instruction& I : F.getEntryBlock()) {
      if (AllocaInst* AI = dyn_cast<AllocaInst>(&I)) {
        if (AllocaKinds[AI] != PointerKind::Escaping)
          continue;
        
        // For now, don't bother with AllocaInsts that flow into PHINodes. Pretty sure that doesn't
        // happen and it would be annoying to deal with.
        bool FoundPhi = false;

        for (User* U : AI->users()) {
          if (isa<PHINode>(U)) {
            FoundPhi = true;
            break;
          }
        }
        if (!FoundPhi)
          Allocas.insert(AI);
      }
    }

    std::unordered_map<BasicBlock*, std::unordered_map<AllocaInst*, AIState>> AtHeadForBB;
    for (BasicBlock& BB : F) {
      AIState State;
      if (&BB == &F.getEntryBlock())
        State = AIState::Uninitialized;
      else
        State = AIState::Unknown;
      std::unordered_map<AllocaInst*, AIState>& AtHead = AtHeadForBB[&BB];
      for (AllocaInst* AI : Allocas)
        AtHead[AI] = State;
    }

    bool Changed = true;
    while (Changed) {
      Changed = false;
      for (BasicBlock& BB : F) {
        std::unordered_map<AllocaInst*, AIState> State = AtHeadForBB[&BB];
        for (Instruction& I : BB) {
          for (Use& U : I.operands()) {
            AllocaInst* AI = dyn_cast<AllocaInst>(U);
            if (!AI || !Allocas.count(AI))
              continue;
            State[AI] = AIState::Initialized;
          }
        }
        for (BasicBlock* SBB : successors(&BB)) {
          std::unordered_map<AllocaInst*, AIState>& AtSuccessorHead = AtHeadForBB[SBB];
          for (auto& Pair : State) {
            AIState MyState = Pair.second;
            AIState SuccessorState = AtSuccessorHead[Pair.first];
            AIState NewSuccessorState = mergeAIState(MyState, SuccessorState);
            if (NewSuccessorState != SuccessorState) {
              AtSuccessorHead[Pair.first] = NewSuccessorState;
              Changed = true;
            }
          }
        }
      }
    }

    std::unordered_map<AllocaInst*, AllocaInst*> ToLazyAlloca;
    std::vector<AllocaInst*> LazyAllocas;
    for (AllocaInst* AI : Allocas) {
      AllocaInst* LazyAI = new AllocaInst(RawPtrTy, 0, nullptr, "filc_lazy_alloca", AI);
      LazyAI->setDebugLoc(AI->getDebugLoc());
      (new StoreInst(RawNull, LazyAI, AI))->setDebugLoc(AI->getDebugLoc());
      ToLazyAlloca[AI] = LazyAI;
      LazyAllocas.push_back(LazyAI);
    }

    std::vector<Use*> MaybeInitialized;
    std::vector<Use*> Uninitialized;
    std::vector<Use*> NeedsLoad;
    
    for (BasicBlock& BB : F) {
      std::unordered_map<AllocaInst*, AIState> State = AtHeadForBB[&BB];
      for (Instruction& I : BB) {
        for (Use& U : I.operands()) {
          AllocaInst* AI = dyn_cast<AllocaInst>(U);
          if (!AI || !Allocas.count(AI))
            continue;
          AIState OldState = State[AI];
          NeedsLoad.push_back(&U);
          if (OldState == AIState::Initialized)
            continue;
          if (OldState == AIState::MaybeInitialized)
            MaybeInitialized.push_back(&U);
          else
            Uninitialized.push_back(&U);
          State[AI] = AIState::Initialized;
        }
      }
    }

    auto CloneAI = [&] (AllocaInst* AI, Instruction* InsertBefore) -> AllocaInst* {
      AllocaInst* Result = new AllocaInst(
        AI->getAllocatedType(), AI->getAddressSpace(), AI->getArraySize(), AI->getAlign(),
        "filc_lazy_clone_" + AI->getName(), InsertBefore);
      AllocaKinds[Result] = PointerKind::Escaping;
      Result->setDebugLoc(InsertBefore->getDebugLoc());
      return Result;
    };

    for (Use* U : MaybeInitialized) {
      Instruction* I = cast<Instruction>(U->getUser());
      AllocaInst* AI = cast<AllocaInst>(*U);
      assert(Allocas.count(AI));
      assert(!isa<PHINode>(I));
      AllocaInst* LazyAI = ToLazyAlloca[AI];
      assert(LazyAI);
      LoadInst* LI = new LoadInst(RawPtrTy, LazyAI, "filc_load_lazy_alloca_for_check", I);
      LI->setDebugLoc(I->getDebugLoc());
      ICmpInst* NotInitialized =
        new ICmpInst(I, ICmpInst::ICMP_EQ, LI, RawNull, "filc_lazy_alloca_not_initialized");
      NotInitialized->setDebugLoc(I->getDebugLoc());
      Instruction* InitializeTerm = SplitBlockAndInsertIfThen(NotInitialized, I, false);
      (new StoreInst(CloneAI(AI, InitializeTerm), LazyAI, InitializeTerm))
        ->setDebugLoc(I->getDebugLoc());
    }

    for (Use* U : Uninitialized) {
      Instruction* I = cast<Instruction>(U->getUser());
      AllocaInst* AI = cast<AllocaInst>(*U);
      assert(Allocas.count(AI));
      assert(!isa<PHINode>(I));
      AllocaInst* LazyAI = ToLazyAlloca[AI];
      assert(LazyAI);
      (new StoreInst(CloneAI(AI, I), LazyAI, I))->setDebugLoc(I->getDebugLoc());
    }

    for (Use* U : NeedsLoad) {
      Instruction* I = cast<Instruction>(U->getUser());
      AllocaInst* AI = cast<AllocaInst>(*U);
      assert(Allocas.count(AI));
      assert(!isa<PHINode>(I));
      AllocaInst* LazyAI = ToLazyAlloca[AI];
      assert(LazyAI);
      LoadInst* LI = new LoadInst(RawPtrTy, LazyAI, "filc_load_lazy_alloca", I);
      LI->setDebugLoc(I->getDebugLoc());
      *U = LI;
    }

    for (AllocaInst* AI : Allocas) {
      assert(!AI->hasNUsesOrMore(1));
      AI->eraseFromParent();
    }

    DominatorTree DT(F);
    PromoteMemToReg(LazyAllocas, DT);
  }
  
  void lazifyAllocas() {
    for (Function& F : M.functions())
      lazifyAllocasInFunction(F);
  }

  void canonicalizeGEP(Instruction* I) {
    GetElementPtrInst* GEP = dyn_cast<GetElementPtrInst>(I);
    if (!GEP)
      return;
    
    unsigned ConstantOperandIndexStart = GEP->getNumOperands();
    for (unsigned Index = GEP->getNumOperands(); Index-- > 1;) {
      if (!isa<Constant>(GEP->getOperand(Index)))
        break;
      ConstantOperandIndexStart = Index;
    }

    assert(ConstantOperandIndexStart >= 1);
    assert(ConstantOperandIndexStart <= GEP->getNumOperands());
    if (ConstantOperandIndexStart == GEP->getNumOperands()) {
      // It's not constant at all.
      return;
    }
    if (ConstantOperandIndexStart == 1) {
      // It's all constants.
      return;
    }

    if (verbose)
      errs() << "Canonicalizing GEP = " << *GEP << "\n";

    std::vector<Value*> LeadingOperands;
    std::vector<Value*> TrailingOperands;
    for (unsigned Index = 1; Index < ConstantOperandIndexStart; ++Index)
      LeadingOperands.push_back(GEP->getOperand(Index));
    TrailingOperands.push_back(ConstantInt::get(Int32Ty, 0));
    for (unsigned Index = ConstantOperandIndexStart; Index < GEP->getNumOperands(); ++Index)
      TrailingOperands.push_back(GEP->getOperand(Index));

    Type* LeadingResult = GetElementPtrInst::getIndexedType(GEP->getSourceElementType(),
                                                            LeadingOperands);

    GetElementPtrInst* LeadingGEP = GetElementPtrInst::Create(
      GEP->getSourceElementType(), GEP->getPointerOperand(), LeadingOperands, "filc_leading_gep", I);
    LeadingGEP->setDebugLoc(I->getDebugLoc());
    GetElementPtrInst* TrailingGEP = GetElementPtrInst::Create(
      LeadingResult, LeadingGEP, TrailingOperands, "filc_trailing_gep", I);
    TrailingGEP->setDebugLoc(I->getDebugLoc());
    GEP->replaceAllUsesWith(TrailingGEP);
    GEP->eraseFromParent();

    if (verbose)
      errs() << "New GEPs:\n" << "    " << *LeadingGEP << "\n    " << *TrailingGEP << "\n";
  }

  void canonicalizeGEPsInFunction(Function& F) {
    if (F.isDeclaration())
      return;

    for (BasicBlock& BB : F) {
      std::vector<Instruction*> Instructions;
      for (Instruction& I : BB)
        Instructions.push_back(&I);
      for (Instruction* I : Instructions)
        canonicalizeGEP(I);
    }
  }

  void canonicalizeGEPs() {
    for (Function& F : M.functions()) {
      canonicalizeGEPsInFunction(F);
      optimizeGEPsInFunction(F);
    }
  }

  void dropUB(Instruction* I) {
    I->dropUnknownNonDebugMetadata();
    I->dropPoisonGeneratingAnnotations();
    I->dropUBImplyingAttrsAndUnknownMetadata();
  }

  void dropUB() {
    for (Function& F : M.functions()) {
      for (BasicBlock& BB : F) {
        for (Instruction& I : BB)
          dropUB(&I);
      }
    }
  }

  void prepare() {
    for (Function& F : M.functions()) {
      for (BasicBlock& BB : F) {
        for (Instruction& I : BB) {
          if (isa<IndirectBrInst>(&I))
            llvm_unreachable("Don't support IndirectBr yet (and maybe never will)");
          if (LoadInst* LI = dyn_cast<LoadInst>(&I))
            assert(!LI->getPointerAddressSpace());
          if (StoreInst* SI = dyn_cast<StoreInst>(&I))
            assert(!SI->getPointerAddressSpace());
          if (AtomicCmpXchgInst* AI = dyn_cast<AtomicCmpXchgInst>(&I))
            assert(!AI->getPointerAddressSpace());
          if (AtomicRMWInst* AI = dyn_cast<AtomicRMWInst>(&I))
            assert(!AI->getPointerAddressSpace());
        }
      }

      // FIXME: Maybe we don't need to split critical edges in those cases where the target is a
      // landing pad?
      //
      // We split critical edges to handle the case where a phi has a constant input, and we need to
      // lower the constant in the predecessor block. However, it's not clear if it's even necessary
      // to split critical edges for that case. If we don't split, then at worst, the predecessors of
      // phis, where the phi is reached via a critical edge, will have some constant lowering that is
      // dead along the other edges.
      SplitAllCriticalEdges(F);
    }
  }

  template<typename KeyType>
  void simpleCSE(Function& F, std::unordered_map<KeyType, std::vector<Instruction*>>& InstMap) {
    DominatorTree DT(F);

    for (auto& Pair : InstMap) {
      std::vector<Instruction*>& Insts = Pair.second;

      if (verbose) {
        errs() << "Considering insts:\n";
        for (Instruction* I : Insts)
          errs() << "    " << *I << "\n";
      }

      assert(Insts.size() >= 1);
      for (size_t Index = Insts.size(); Index--;) {
        if (!Insts[Index])
          continue;
        for (size_t Index2 = Insts.size(); Index2--;) {
          if (!Insts[Index2])
            continue;
          if (Insts[Index] == Insts[Index2])
            continue;
          if (verbose)
            errs() << "Considering " << *Insts[Index] << " and " << *Insts[Index2] << "\n";
          if (DT.dominates(Insts[Index], Insts[Index2])) {
            if (verbose)
              errs() << "    Dominates!\n";
            Insts[Index2]->replaceAllUsesWith(Insts[Index]);
            Insts[Index2]->eraseFromParent();
            Insts[Index2] = nullptr;
          }
        }
      }
    }
  }

  // This does one pass of GEP CSE and is exactly what we need after canonicalizing GEPs.
  void optimizeGEPsInFunction(Function& F) {
    if (F.isDeclaration())
      return;

    std::unordered_map<GEPKey, std::vector<Instruction*>> GEPMap;

    for (BasicBlock& BB : F) {
      for (Instruction& I : BB) {
        GetElementPtrInst* GEP = dyn_cast<GetElementPtrInst>(&I);
        if (!GEP)
          continue;
        GEPMap[GEP].push_back(GEP);
      }
    }

    simpleCSE(F, GEPMap);
  }

  void optimizeGetters() {
    std::unordered_map<Value*, std::vector<Instruction*>> GetterCallsForGetter;

    for (BasicBlock& BB : *NewF) {
      for (Instruction& I : BB) {
        CallInst* CI = dyn_cast<CallInst>(&I);
        if (!CI)
          continue;

        Function* Getter = CI->getCalledFunction();
        if (!Getter)
          continue;

        if (!Getters.count(Getter))
          continue;

        assert(CI->getFunctionType() == PizlonatedGetterTy);

        GetterCallsForGetter[Getter].push_back(CI);
      }
    }

    simpleCSE(*NewF, GetterCallsForGetter);
  }

public:
  Pizlonator(Module &M)
    : C(M.getContext()), M(M), DLBefore(M.getDataLayout()), DL(M.getDataLayoutAfterFilC()) {
  }

  void run() {
    if (lightVerbose || verbose)
      errs() << "Going to town on module:\n" << M << "\n";

    assert(DLBefore.getPointerSizeInBits(TargetAS) == 64);
    assert(DLBefore.getPointerABIAlignment(TargetAS) == 8);
    assert(DLBefore.isNonIntegralAddressSpace(TargetAS));
    assert(DL.getPointerSizeInBits(TargetAS) == 64);
    assert(DL.getPointerABIAlignment(TargetAS) == 8);
    assert(!DL.isNonIntegralAddressSpace(TargetAS));

    PtrBits = DL.getPointerSizeInBits(TargetAS);
    VoidTy = Type::getVoidTy(C);
    Int1Ty = Type::getInt1Ty(C);
    Int8Ty = Type::getInt8Ty(C);
    Int16Ty = Type::getInt16Ty(C);
    Int32Ty = Type::getInt32Ty(C);
    IntPtrTy = Type::getIntNTy(C, PtrBits);
    assert(IntPtrTy == Type::getInt64Ty(C)); // FilC is 64-bit-only, for now.
    Int64Ty = IntPtrTy;
    Int128Ty = Type::getInt128Ty(C);
    FloatTy = Type::getFloatTy(C);
    DoubleTy = Type::getDoubleTy(C);
    RawPtrTy = PointerType::get(C, TargetAS);
    CtorDtorTy = FunctionType::get(VoidTy, false);
    SetjmpTy = FunctionType::get(Int32Ty, RawPtrTy, false);
    SigsetjmpTy = FunctionType::get(Int32Ty, { RawPtrTy, Int32Ty }, false);
    RawNull = ConstantPointerNull::get(RawPtrTy);
    ThreadlocalAddress = Intrinsic::getOrInsertDeclaration(
      &M, Intrinsic::threadlocal_address, { RawPtrTy });

    Dummy = makeDummy(Int32Ty);

    lowerIndirectBr();

    if (verbose)
      errs() << "Module with indirectbr lowered:\n" << M << "\n";
    
    lockDownLinkage();
    removeIrrelevantIntrinsics();
    findNonescapingAllocas();
    removeLifetimeIntrinsics();
    expandConstantExprs();
    inferPointerAsIntLaundering();

    if (verbose) {
      errs() << "Module with irrelevant intrinsics removed, constexprs expanded, "
             << "and pointer laundering inferred:\n" << M << "\n";
    }
    
    makeEHDatas();
    compileModuleAsm();
    lazifyAllocas();
    canonicalizeGEPs();
    dropUB();
    
    if (verbose) {
      errs() << "Module with lowered EH data, lowered module asm, lazified allocas, and UB "
             << "dropped:\n" << M << "\n";
    }

    prepare();

    // FIXME: We could probably do this anywhere in the pass, and really all we're doing is turning
    // off the non-integralness of address space 0.
    M.setDataLayout(M.getDataLayoutAfterFilC());
    
    if (verbose)
      errs() << "Prepared module:\n" << M << "\n";

    FunctionName = "<internal>";
    
    FlightPtrTy = StructType::create({ RawPtrTy, RawPtrTy }, "filc_flight_ptr");
    OriginNodeTy = StructType::create({ RawPtrTy, RawPtrTy, Int32Ty }, "filc_origin_node");
    FunctionOriginTy = StructType::create(
      { OriginNodeTy, RawPtrTy, Int8Ty, Int8Ty, Int8Ty, Int32Ty }, "filc_function_origin");
    OriginTy = StructType::create({ RawPtrTy, Int32Ty, Int32Ty }, "filc_origin");
    InlineFrameTy = StructType::create({ OriginNodeTy, OriginTy }, "filc_inline_frame");
    OriginWithEHTy = StructType::create(
      { RawPtrTy, Int32Ty, Int32Ty, RawPtrTy }, "filc_origin_with_eh");
    ObjectTy = StructType::create({ RawPtrTy, RawPtrTy }, "filc_object");
    FrameTy = StructType::create({ RawPtrTy, RawPtrTy, RawPtrTy }, "filc_frame");
    UnsafeFuncTy = FunctionType::get(IntPtrTy, true);

    std::vector<Type*> ThreadMembers;
    ThreadMembers.push_back(IntPtrTy); // stack_limit, index 0
    ThreadMembers.push_back(Int8Ty); // state, index 1
    ThreadMembers.push_back(Int32Ty); // tid, index 2
    ThreadMembers.push_back(RawPtrTy); // top_frame, index 3
    ThreadMembers.push_back(RawPtrTy); // alignment word, index 4
    ThreadMembers.push_back(
      ArrayType::get(FlightPtrTy, NumUnwindRegisters)); // unwind_registers, index 5
    ThreadMembers.push_back(FlightPtrTy); // cookie_ptr, index 6
    ThreadMembers.push_back(Int8Ty); // alignment point, index 7
    ThreadTy = StructType::create(ThreadMembers, "filc_thread_ish_base2");
    const StructLayout* ThreadLayout = DL.getStructLayout(ThreadTy);
    size_t AlignmentPoint = ThreadLayout->getElementOffset(7);
    ThreadMembers.pop_back();
    // Create an alignment buffer at index 7.
    ThreadMembers.push_back(
      ArrayType::get(
        Int8Ty, ((AlignmentPoint + CCAlignment) & -CCAlignment) - AlignmentPoint));
    ThreadMembers.push_back(ArrayType::get(Int8Ty, CCInlineSize)); // cc_inline_buffer, index 8
    ThreadMembers.push_back(ArrayType::get(Int8Ty, CCInlineSize)); // cc_inline_aux_buffer, index 9
    ThreadMembers.push_back(RawPtrTy); // cc_outline_buffer, index 10
    ThreadMembers.push_back(RawPtrTy); // cc_outline_aux_buffer, index 11
    ThreadMembers.push_back(IntPtrTy); // cc_outline_size, index 12
    ThreadTy = StructType::create(ThreadMembers, "filc_thread_ish");
    ThreadLayout = DL.getStructLayout(ThreadTy);
    assert(!(ThreadLayout->getElementOffset(8) & (CCAlignment - 1)));
    
    ConstantRelocationTy = StructType::create(
      { IntPtrTy, Int32Ty, RawPtrTy }, "filc_constant_relocation");
    ConstexprNodeTy = StructType::create(
      { Int32Ty, Int32Ty, RawPtrTy, IntPtrTy }, "filc_constexpr_node");
    AlignmentAndOffsetTy = StructType::create({ Int8Ty, Int8Ty }, "filc_alignment_and_offset");
    PizlonatedReturnValueTy = StructType::create({ Int1Ty, IntPtrTy }, "pizlonated_return_value");
    PtrPairTy = StructType::create({ RawPtrTy, RawPtrTy }, "filc_ptr_pair");
    FunctionPayloadTy = StructType::create({ RawPtrTy, RawPtrTy, Int64Ty }, "filc_function");
    FunctionObjectTy = StructType::create({ ObjectTy, FunctionPayloadTy }, "filc_function_object");
    ClosureTy = StructType::create({
        FunctionPayloadTy,
        RawPtrTy, // Padding
        FlightPtrTy
      }, "filc_closure");
    PizlonatedFuncTy = FunctionType::get(
      PizlonatedReturnValueTy, { RawPtrTy, RawPtrTy, IntPtrTy }, false);
    PizlonatedGetterTy = FunctionType::get(FlightPtrTy, { RawPtrTy, RawPtrTy }, false);
    ThreadLocalEnsureTy = FunctionType::get(RawPtrTy, { RawPtrTy }, false);

    // FIXME: Eventually, we'll want to do something with the DSO handle. But for now it doesn't
    // matter because we don't support dlclose.
    if (GlobalVariable* DSO = M.getGlobalVariable("__dso_handle")) {
      assert(DSO->isDeclaration());
      DSO->replaceAllUsesWith(RawNull);
      DSO->eraseFromParent();
    }

    for (GlobalObject& G : M.global_objects()) {
      if (Comdat* OldComdat = G.getComdat()) {
        assert(G.getLinkage() == GlobalValue::LinkOnceAnyLinkage ||
               G.getLinkage() == GlobalValue::WeakAnyLinkage ||
               G.getLinkage() == GlobalValue::InternalLinkage);
        Comdat* NewComdat;
        if (ComdatMap.count(OldComdat))
          NewComdat = ComdatMap[OldComdat];
        else {
          std::string str = ("pizlonatedC_" + OldComdat->getName()).str();
          NewComdat = M.getOrInsertComdat(str);
          ComdatMap[OldComdat] = NewComdat;
        }
        NewComdat->setSelectionKind(OldComdat->getSelectionKind());
        GlobalToComdat[&G] = NewComdat;
      } else if (G.hasLinkOnceLinkage()) {
        std::string str = ("pizlonatedMC_" + G.getName()).str();
        Comdat* NewComdat = M.getOrInsertComdat(str);
        NewComdat->setSelectionKind(Comdat::Any);
        GlobalToComdat[&G] = NewComdat;
      }
    }
    
    for (GlobalVariable &G : M.globals()) {
      if (shouldPassThrough(&G))
        continue;
      Globals.push_back(&G);
    }
    for (Function &F : M.functions()) {
      if (shouldPassThrough(&F)) {
        if (!F.isDeclaration()) {
          errs() << "Cannot define " << F.getName() << "\n";
          llvm_unreachable("Attempt to define pass-through function.");
        }
        if (isSetjmp(&F)) {
          assert(F.hasFnAttribute(Attribute::ReturnsTwice));
          if (F.getName() == "setjmp" || F.getName() == "_setjmp") {
            if (F.getFunctionType() != SetjmpTy) {
              errs() << "Unexpected setjmp signature: " << *F.getFunctionType()
                     << ", expected: " << *SetjmpTy << "\n";
            }
            assert(F.getFunctionType() == SetjmpTy);
          } else {
            if (F.getFunctionType() != SigsetjmpTy) {
              errs() << "Unexpected setjmp signature: " << *F.getFunctionType()
                     << ", expected: " << *SigsetjmpTy << "\n";
            }
            assert(F.getName() == "sigsetjmp");
            assert(F.getFunctionType() == SigsetjmpTy);
          }
        }
        continue;
      }
      Functions.push_back(&F);
    }
    for (GlobalAlias &G : M.aliases())
      Aliases.push_back(&G);
    for (GlobalIFunc &G : M.ifuncs())
      IFuncs.push_back(&G);

    FlightNull = ConstantAggregateZero::get(FlightPtrTy);
    if (verbose)
      errs() << "FlightNull = " << *FlightNull << "\n";

    Pollcheck = M.getOrInsertFunction(
      "filc_pollcheck", VoidTy, RawPtrTy, RawPtrTy);
    Enter = M.getOrInsertFunction(
      "filc_enter", VoidTy, RawPtrTy);
    Exit = M.getOrInsertFunction(
      "filc_exit", VoidTy, RawPtrTy);
    StoreBarrierForLowerSlow = M.getOrInsertFunction(
      "filc_store_barrier_for_lower_slow", VoidTy, RawPtrTy, RawPtrTy);
    StorePtrAtomicOutline = M.getOrInsertFunction(
      "filc_store_ptr_atomic_outline", VoidTy, RawPtrTy, FlightPtrTy, FlightPtrTy);
    LoadPtrAtomicOutline = M.getOrInsertFunction(
      "filc_load_ptr_atomic_with_manual_tracking_outline", FlightPtrTy, FlightPtrTy);
    ObjectEnsureAuxPtrOutline = M.getOrInsertFunction(
      "filc_object_ensure_aux_ptr_outline", RawPtrTy, RawPtrTy, RawPtrTy);
    ThreadEnsureCCOutlineBufferSlow = M.getOrInsertFunction(
      "filc_thread_ensure_cc_outline_buffer_slow", VoidTy, RawPtrTy, IntPtrTy);
    StrongCasPtr = M.getOrInsertFunction(
      "filc_strong_cas_ptr_with_manual_tracking",
      FlightPtrTy, RawPtrTy, FlightPtrTy, IntPtrTy, FlightPtrTy, FlightPtrTy);
    XchgPtr = M.getOrInsertFunction(
      "filc_xchg_ptr_with_manual_tracking",
      FlightPtrTy, RawPtrTy, FlightPtrTy, IntPtrTy, FlightPtrTy);
    GetNextPtrBytesForVAArg = M.getOrInsertFunction(
      "filc_get_next_ptr_bytes_for_va_arg", PtrPairTy, FlightPtrTy, IntPtrTy, IntPtrTy);
    Allocate = M.getOrInsertFunction(
      "filc_allocate", RawPtrTy, RawPtrTy, IntPtrTy);
    AllocateWithAlignment = M.getOrInsertFunction(
      "filc_allocate_with_alignment", RawPtrTy, RawPtrTy, IntPtrTy, IntPtrTy);
    LocalAllocatorAllocate = M.getOrInsertFunction(
      "verse_local_allocator_allocate", RawPtrTy, RawPtrTy);
    FreeWithChecks = M.getOrInsertFunction(
      "filc_free_with_checks", VoidTy, FlightPtrTy);
    LogAllocate = M.getOrInsertFunction(
      "filc_log_allocate", VoidTy, RawPtrTy, RawPtrTy, IntPtrTy, IntPtrTy);
    CheckFunctionCallFail = M.getOrInsertFunction(
      "filc_check_function_call_fail", VoidTy, FlightPtrTy);
    CheckClosureFail = M.getOrInsertFunction(
      "filc_check_closure_fail", VoidTy, RawPtrTy, RawPtrTy);
    ComdatLinkFail = M.getOrInsertFunction(
      "filc_comdat_link_fail", VoidTy, RawPtrTy, Int64Ty);
    OptimizedAlignmentContradiction = M.getOrInsertFunction(
      "filc_optimized_alignment_contradiction", VoidTy, FlightPtrTy, RawPtrTy);
    OptimizedAccessCheckFail = M.getOrInsertFunction(
      "filc_optimized_access_check_fail", VoidTy, FlightPtrTy, RawPtrTy);
    OptimizedStackAlignmentContradiction = M.getOrInsertFunction(
      "filc_optimized_stack_alignment_contradiction", VoidTy, IntPtrTy, IntPtrTy, RawPtrTy);
    OptimizedStackAccessCheckFail = M.getOrInsertFunction(
      "filc_optimized_stack_access_check_fail", VoidTy, IntPtrTy, IntPtrTy, RawPtrTy);
    MaskedAccessCheckFail = M.getOrInsertFunction(
      "filc_masked_access_check_fail", VoidTy, FlightPtrTy, Int64Ty, IntPtrTy, Int32Ty, RawPtrTy);
    Memset = M.getOrInsertFunction(
      "filc_memset", VoidTy, RawPtrTy, FlightPtrTy, Int32Ty, IntPtrTy, RawPtrTy);
    Memmove = M.getOrInsertFunction(
      "filc_memmove", VoidTy, RawPtrTy, FlightPtrTy, FlightPtrTy, IntPtrTy, RawPtrTy);
    MemmoveAlreadyChecked = M.getOrInsertFunction(
      "filc_memmove_already_checked", VoidTy, RawPtrTy, FlightPtrTy, FlightPtrTy, IntPtrTy, RawPtrTy);
    MemmoveAlreadyCheckedSmall = M.getOrInsertFunction(
      "filc_memmove_already_checked_small",
      VoidTy, RawPtrTy, FlightPtrTy, FlightPtrTy, IntPtrTy, RawPtrTy);
    FinishMemmoveSmall1 = M.getOrInsertFunction(
      "filc_finish_memmove_small_1", PtrPairTy, RawPtrTy, FlightPtrTy, RawPtrTy);
    FinishMemmoveSmall2 = M.getOrInsertFunction(
      "filc_finish_memmove_small_2", PtrPairTy, RawPtrTy, FlightPtrTy, RawPtrTy, RawPtrTy);
    FinishMemmoveSmall3 = M.getOrInsertFunction(
      "filc_finish_memmove_small_3", PtrPairTy, RawPtrTy, FlightPtrTy, RawPtrTy, RawPtrTy, RawPtrTy);
    FinishMemmoveSmall4 = M.getOrInsertFunction(
      "filc_finish_memmove_small_4",
      PtrPairTy, RawPtrTy, FlightPtrTy, RawPtrTy, RawPtrTy, RawPtrTy, RawPtrTy);
    FinishMemmoveSmall5 = M.getOrInsertFunction(
      "filc_finish_memmove_small_5",
      PtrPairTy, RawPtrTy, FlightPtrTy, RawPtrTy, RawPtrTy, RawPtrTy, RawPtrTy, RawPtrTy);
    MemmoveAlreadyCheckedStackToHeap = M.getOrInsertFunction(
      "filc_memmove_already_checked_stack_to_heap",
      VoidTy, RawPtrTy, FlightPtrTy, RawPtrTy, RawPtrTy, IntPtrTy, RawPtrTy);
    MemmoveStackToHeap = M.getOrInsertFunction(
      "filc_memmove_stack_to_heap",
      VoidTy, RawPtrTy, FlightPtrTy, RawPtrTy, RawPtrTy, RawPtrTy, IntPtrTy, RawPtrTy);
    MemmoveAlreadyCheckedHeapToStack = M.getOrInsertFunction(
      "filc_memmove_already_checked_heap_to_stack",
      VoidTy, RawPtrTy, RawPtrTy, RawPtrTy, FlightPtrTy, IntPtrTy);
    MemmoveHeapToStack = M.getOrInsertFunction(
      "filc_memmove_heap_to_stack",
      VoidTy, RawPtrTy, RawPtrTy, RawPtrTy, RawPtrTy, FlightPtrTy, IntPtrTy, RawPtrTy);
    MemmoveAlreadyCheckedStack = M.getOrInsertFunction(
      "filc_memmove_already_checked_stack",
      VoidTy, RawPtrTy, RawPtrTy, RawPtrTy, RawPtrTy, RawPtrTy, IntPtrTy);
    MemmoveStack = M.getOrInsertFunction(
      "filc_memmove_stack",
      VoidTy, RawPtrTy, RawPtrTy, RawPtrTy, RawPtrTy, RawPtrTy, RawPtrTy, RawPtrTy, IntPtrTy,
      RawPtrTy);
    GlobalInitializationStart = M.getOrInsertFunction(
      "filc_global_initialization_start", Int1Ty, RawPtrTy, RawPtrTy, RawPtrTy, RawPtrTy);
    GlobalInitializationEnd = M.getOrInsertFunction(
      "filc_global_initialization_end", VoidTy, RawPtrTy);
    CallIfunc = M.getOrInsertFunction(
      "filc_call_ifunc", FlightPtrTy, RawPtrTy, RawPtrTy, RawPtrTy, FlightPtrTy);
    ExecuteConstantRelocations = M.getOrInsertFunction(
      "filc_execute_constant_relocations", VoidTy, RawPtrTy, RawPtrTy, RawPtrTy, IntPtrTy);
    DeferOrRunGlobalCtor = M.getOrInsertFunction(
      "filc_defer_or_run_global_ctor", VoidTy, FlightPtrTy);
    RunGlobalDtor = M.getOrInsertFunction(
      "filc_run_global_dtor", VoidTy, FlightPtrTy);
    Error = M.getOrInsertFunction(
      "filc_error", VoidTy, RawPtrTy, RawPtrTy);
    RealMemset = M.getOrInsertFunction(
      "llvm.memset.p0.i64", VoidTy, RawPtrTy, Int8Ty, IntPtrTy, Int1Ty);
    RealMemcpy = M.getOrInsertFunction(
      "llvm.memcpy.p0.p0.i64", VoidTy, RawPtrTy, RawPtrTy, IntPtrTy, Int1Ty);
    RealMemmove = M.getOrInsertFunction(
      "llvm.memmove.p0.p0.i64", VoidTy, RawPtrTy, RawPtrTy, IntPtrTy, Int1Ty);
    LandingPad = M.getOrInsertFunction(
      "filc_landing_pad", Int1Ty, RawPtrTy);
    ResumeUnwind = M.getOrInsertFunction(
      "filc_resume_unwind", VoidTy, RawPtrTy, RawPtrTy);
    JmpBufCreate = M.getOrInsertFunction(
      "filc_jmp_buf_create", RawPtrTy, RawPtrTy, Int32Ty, Int32Ty);
    PromoteAlreadyCheckedStackToHeapWithoutExiting = M.getOrInsertFunction(
      "filc_promote_already_checked_stack_to_heap_without_exiting",
      FlightPtrTy, RawPtrTy, RawPtrTy, RawPtrTy, IntPtrTy);
    PromoteAlreadyCheckedStackToHeapWithAlignmentWithoutExiting = M.getOrInsertFunction(
      "filc_promote_already_checked_stack_to_heap_with_alignment_without_exiting",
      FlightPtrTy, RawPtrTy, RawPtrTy, RawPtrTy, IntPtrTy, IntPtrTy);
    DemoteWordAlignedAlreadyCheckedHeapToStackWithoutExiting = M.getOrInsertFunction(
      "filc_demote_word_aligned_already_checked_heap_to_stack_without_exiting",
      VoidTy, FlightPtrTy, RawPtrTy, RawPtrTy, IntPtrTy);
    DemoteAlreadyCheckedHeapToStackWithoutExiting = M.getOrInsertFunction(
      "filc_demote_already_checked_heap_to_stack_without_exiting",
      VoidTy, FlightPtrTy, RawPtrTy, RawPtrTy, IntPtrTy);
    PromoteArgsToHeap = M.getOrInsertFunction(
      "filc_promote_args_to_heap", FlightPtrTy, RawPtrTy, IntPtrTy);
    PrepareToReturnWithData = M.getOrInsertFunction(
      "filc_prepare_to_return_with_data", IntPtrTy, RawPtrTy, FlightPtrTy, RawPtrTy);
    CCArgsCheckFailure = M.getOrInsertFunction(
      "filc_cc_args_check_failure", VoidTy, IntPtrTy, IntPtrTy, RawPtrTy);
    CCRetsCheckFailure = M.getOrInsertFunction(
      "filc_cc_rets_check_failure", VoidTy, IntPtrTy, IntPtrTy, RawPtrTy);
    AllocateThreadLocal = M.getOrInsertFunction(
      "filc_allocate_thread_local", RawPtrTy, RawPtrTy, IntPtrTy, IntPtrTy);
    AllocateThreadLocalWithPtrs = M.getOrInsertFunction(
      "filc_allocate_thread_local_with_ptrs", RawPtrTy, RawPtrTy, IntPtrTy, IntPtrTy);
    WeakMapCreate = M.getOrInsertFunction(
      "filc_weak_map_create", RawPtrTy, RawPtrTy);
    WeakMapSet = M.getOrInsertFunction(
      "filc_weak_map_set", RawPtrTy, RawPtrTy, RawPtrTy, FlightPtrTy, FlightPtrTy);
    _Setjmp = M.getOrInsertFunction(
      "_setjmp", Int32Ty, RawPtrTy);
    cast<Function>(_Setjmp.getCallee())->addFnAttr(Attribute::ReturnsTwice);
    ExpectI1 = Intrinsic::getOrInsertDeclaration(&M, Intrinsic::expect, Int1Ty);
    LifetimeStart = Intrinsic::getOrInsertDeclaration(&M, Intrinsic::lifetime_start, { RawPtrTy });
    LifetimeEnd = Intrinsic::getOrInsertDeclaration(&M, Intrinsic::lifetime_end, { RawPtrTy });
    StackCheckAsm = InlineAsm::get(
      FunctionType::get(VoidTy, { RawPtrTy }, false),
      "cmp %rsp, $0\n\t"
      "jae filc_stack_overflow_failure@PLT",
      "*m,~{memory},~{dirflag},~{fpsr},~{flags}",
      /*hasSideEffects=*/true);
    DoNothing = Intrinsic::getOrInsertDeclaration(&M, Intrinsic::donothing, { });

    cast<Function>(OptimizedAlignmentContradiction.getCallee())->addFnAttr(Attribute::NoReturn);
    cast<Function>(OptimizedAccessCheckFail.getCallee())->addFnAttr(Attribute::NoReturn);
    cast<Function>(OptimizedStackAlignmentContradiction.getCallee())->addFnAttr(Attribute::NoReturn);
    cast<Function>(OptimizedStackAccessCheckFail.getCallee())->addFnAttr(Attribute::NoReturn);

    CurrentMarkingState = M.getOrInsertGlobal("filc_current_marking_state", Int32Ty);

    std::vector<GlobalValue*> ToDelete;
    auto HandleGlobal = [&] (GlobalValue* G) {
      if (verbose)
        errs() << "Handling global: " << G->getName() << "\n";
      Function* NewF = Function::Create(PizlonatedGetterTy, G->getLinkage(), G->getAddressSpace(),
                                        "pizlonated_" + G->getName(), &M);
      NewF->addFnAttr(Attribute::NoUnwind);
      if (GlobalToComdat.count(G))
        NewF->setComdat(GlobalToComdat[G]);
      NewF->setVisibility(G->getVisibility());
      GlobalToGetter[G] = NewF;
      Getters.insert(NewF);
      ToDelete.push_back(G);
    };

    auto PutImplIntoComdat = [&] (GlobalValue* OrigG, GlobalObject* NewG) {
      assert(NewG->getLinkage() == GlobalValue::InternalLinkage ||
             NewG->getLinkage() == GlobalValue::PrivateLinkage ||
             NewG->getLinkage() == OrigG->getLinkage());
      
      if (!GlobalToComdat.count(OrigG))
        return;

      NewG->setLinkage(OrigG->getLinkage());
      NewG->setVisibility(OrigG->getVisibility());
      NewG->setDSOLocal(OrigG->isDSOLocal());
      NewG->setComdat(GlobalToComdat[OrigG]);
    };
    
    for (GlobalVariable* G : Globals)
      HandleGlobal(G);
    for (GlobalAlias* G : Aliases)
      HandleGlobal(G);
    for (GlobalIFunc* G : IFuncs) {
      assert(!G->isThreadLocal());
      HandleGlobal(G);
    }
    for (Function* F : Functions) {
      assert(!F->isThreadLocal());
      if (F->isIntrinsic())
        continue;
      HandleGlobal(F);
      if (!F->isDeclaration()) {
        // Couple of things going on here.
        //
        // - If the global is in a comdat, then we need to preserve the comdat.
        // - If the global has fancy linkage like linkonce_odr, then we need to put the hidden
        //   function in linkonce_odr, too.
        // - It's most likely better in that case to create a comdat if there isn't one already, and
        //   put the hidden function in that comdat, using whatever linkage the global had.
        //
        // Otherwise, we end up in situations where we inline references to the hidden function and
        // then the hidden function gets duplicated even if the global was linkonce.

        std::vector<ArgInfo> AIs = argInfosForFunction(F);
        Type* NormalizedRetType = normalizeRetType(F->getReturnType());
        uint64_t Signature;
        if (usesVariadicCC(F))
          Signature = GenericSignature;
        else
          Signature = computeSignature(AIs, NormalizedRetType);
        FunctionType* ImplFuncTy;
        if (Signature == GenericSignature)
          ImplFuncTy = PizlonatedFuncTy;
        else
          ImplFuncTy = fastFunctionTypeForSignature(AIs, NormalizedRetType);
        bool UsesCallee = usesCallee(F);
        std::ostringstream buf;
        buf << "pizlonatedFIP" << Signature << "_" << std::string(F->getName());
        GlobalValue::LinkageTypes Linkage;
        if (UsesCallee)
          Linkage = GlobalValue::InternalLinkage;
        else
          Linkage = F->getLinkage();
        Function* NewF = Function::Create(
          ImplFuncTy, Linkage, F->getAddressSpace(),
          buf.str(), &M);
        FunctionToHiddenFunction[F] = NewF;
        if (!UsesCallee)
          FunctionToSignature[F] = Signature;
        NewF->setSubprogram(F->getSubprogram());

        PutImplIntoComdat(F, NewF);

        if (F->getLinkage() == GlobalValue::ExternalLinkage && !UsesCallee) {
          std::ostringstream buf;
          buf << "pizlonatedFI" << Signature << "_" << std::string(F->getName());
          GlobalAlias::create(ImplFuncTy, 0, F->getLinkage(), buf.str(), NewF, &M);
        }

        GlobalVariable* NewObjectG = new GlobalVariable(
          M, FunctionObjectTy, true, GlobalValue::InternalLinkage, nullptr,
          "pizlonatedFO_" + F->getName());
        PutImplIntoComdat(F, NewObjectG);
        Constant* LowerAndUpper =
          ConstantExpr::getGetElementPtr(ObjectTy, NewObjectG, ConstantInt::get(IntPtrTy, 1));
        uint16_t ObjectFlags =
          ObjectFlagGlobal |
          ObjectFlagReadonly |
          (SpecialTypeFunction << ObjectFlagsSpecialShift);
        Constant* NewObjC = ConstantStruct::get(
          FunctionObjectTy,
          { ConstantStruct::get(
              ObjectTy,
              { LowerAndUpper,
                ConstantExpr::getGetElementPtr(
                  Int8Ty, LowerAndUpper,
                  ConstantInt::get(
                    IntPtrTy, static_cast<uintptr_t>(ObjectFlags) << ObjectAuxFlagsShift)) }),
            ConstantStruct::get(
              FunctionPayloadTy,
              { Signature == GenericSignature ? RawNull : NewF,
                Signature == GenericSignature ? NewF : calleeEntrypointThunk(
                  Signature, AIs, NormalizedRetType),
                ConstantInt::get(Int64Ty, Signature) }) });
        NewObjectG->setInitializer(NewObjC);
        FunctionToLower[F] = LowerAndUpper;
      }
    }
    if (verbose) {
      errs() << "ToDelete values:";
      for (GlobalValue* GV : ToDelete)
        errs() << " " << GV->getName();
      errs() << "\n";
    }

    auto HandleThingy = [&] (Value* Thingy) -> Constant* {
      if (Thingy == RawNull)
        return RawNull;
      GlobalValue* G = cast<GlobalValue>(Thingy);
      assert(GlobalToGetter.count(G));
      Function* Getter = GlobalToGetter[G];
      assert(Getter);
      return Getter;
    };
    
    if (GlobalVariable* GlobalCtors = M.getGlobalVariable("llvm.global_ctors")) {
      ConstantArray* Array = cast<ConstantArray>(GlobalCtors->getInitializer());
      std::vector<Constant*> Args;
      for (size_t Index = 0; Index < Array->getNumOperands(); ++Index) {
        ConstantStruct* Struct = cast<ConstantStruct>(Array->getOperand(Index));
        Function* Ctor = cast<Function>(Struct->getOperand(1));
        Function* NewF = Function::Create(
          CtorDtorTy, GlobalValue::PrivateLinkage, 0, "filc_ctor_forwarder", &M);
        NewF->addFnAttr(Attribute::NoUnwind);
        NewF->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
        PutImplIntoComdat(Ctor, NewF);
        BasicBlock* RootBB = BasicBlock::Create(C, "filc_ctor_forwarder_root", NewF);
        ReturnInst* Return = ReturnInst::Create(C, RootBB);
        CallInst::Create(
          DeferOrRunGlobalCtor, { flightPtrForLocalFunction(Ctor, Return) }, "", Return);
        Args.push_back(ConstantStruct::get(Struct->getType(),
                                           Struct->getOperand(0),
                                           NewF,
                                           HandleThingy(Struct->getOperand(2))));
      }
      GlobalCtors->setInitializer(ConstantArray::get(Array->getType(), Args));
    }

    // NOTE: This *might* be dead code, since modern C/C++ says that the compiler has to do
    // __cxa_atexit from a global constructor instead of registering a global destructor.
    if (GlobalVariable* GlobalDtors = M.getGlobalVariable("llvm.global_dtors")) {
      ConstantArray* Array = cast<ConstantArray>(GlobalDtors->getInitializer());
      std::vector<Constant*> Args;
      for (size_t Index = 0; Index < Array->getNumOperands(); ++Index) {
        ConstantStruct* Struct = cast<ConstantStruct>(Array->getOperand(Index));
        Function* Dtor = cast<Function>(Struct->getOperand(1));
        Function* NewF = Function::Create(
          CtorDtorTy, GlobalValue::PrivateLinkage, 0, "filc_dtor_forwarder", &M);
        NewF->addFnAttr(Attribute::NoUnwind);
        NewF->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
        PutImplIntoComdat(Dtor, NewF);
        BasicBlock* RootBB = BasicBlock::Create(C, "filc_dtor_forwarder_root", NewF);
        ReturnInst* Return = ReturnInst::Create(C, RootBB);
        CallInst::Create(RunGlobalDtor, { flightPtrForLocalFunction(Dtor, Return) }, "", Return);
        Args.push_back(ConstantStruct::get(Struct->getType(),
                                           Struct->getOperand(0),
                                           NewF,
                                           HandleThingy(Struct->getOperand(2))));
      }
      GlobalDtors->setInitializer(ConstantArray::get(Array->getType(), Args));
    }

    auto HandleUsed = [&] (GlobalVariable* Used) {
      ConstantArray* Array = cast<ConstantArray>(Used->getInitializer());
      std::vector<Constant*> Args;
      for (size_t Index = 0; Index < Array->getNumOperands(); ++Index) {
        // NOTE: This could have a GEP, supposedly. Pretend it can't for now.
        Function* GetterF = GlobalToGetter[cast<GlobalValue>(Array->getOperand(Index))];
        assert(GetterF);
        Args.push_back(GetterF);
      }
      Used->setInitializer(ConstantArray::get(Array->getType(), Args));
    };
    if (GlobalVariable* Used = M.getGlobalVariable("llvm.used"))
      HandleUsed(Used);
    if (GlobalVariable* Used = M.getGlobalVariable("llvm.compiler.used"))
      HandleUsed(Used);

    for (GlobalVariable* G : Globals) {
      size_t Alignment = 0;
      if (!G->isDeclaration())
        Alignment = std::max(G->getAlignment(), DL.getABITypeAlign(G->getValueType()).value());
      
      Function* NewF = GlobalToGetter[G];
      assert(NewF);
      assert(NewF->isDeclaration());

      if (verbose)
        errs() << "Dealing with global: " << *G << "\n";

      if (G->isDeclaration())
        continue;

      assert(Alignment);
      
      if (G->isThreadLocal()) {
        Function* SlowF = Function::Create(ThreadLocalEnsureTy, GlobalValue::PrivateLinkage,
                                           G->getAddressSpace(), "pizlonatedGS_" + G->getName(), &M);
        SlowF->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
        SlowF->addFnAttr(Attribute::NoUnwind);
        PutImplIntoComdat(G, SlowF);
        SlowF->addFnAttr(Attribute::NoInline);
      
        GlobalVariable* NewG = new GlobalVariable(
          M, RawPtrTy, false, G->getLinkage(), G->isDeclaration() ? nullptr : RawNull,
          "pizlonatedTP_" + G->getName(), nullptr, G->getThreadLocalMode());
        if (GlobalToComdat.count(G))
          NewG->setComdat(GlobalToComdat[G]);

        assert(!MyThread);
        MyThread = SlowF->getArg(0);
        BasicBlock* RootB = BasicBlock::Create(C, "filc_thread_local_ensure_root", SlowF);
        ReturnInst* Return = ReturnInst::Create(C, UndefValue::get(RawPtrTy), RootB);
        Instruction* Object = CallInst::Create(
          hasPtrs(G->getValueType()) ? AllocateThreadLocalWithPtrs : AllocateThreadLocal,
          { MyThread, ConstantInt::get(IntPtrTy, DL.getTypeAllocSize(G->getValueType())),
            ConstantInt::get(IntPtrTy, Alignment) },
          "filc_thread_local_allocate", Return);
        Value* Lower = lowerForObject(Object, Return);
        Value* AuxP;
        if (hasPtrs(G->getValueType()))
          AuxP = auxPtrForLower(Lower, Return);
        else
          AuxP = RawNull;
        Return->getOperandUse(0) = Lower;
        if (!isa<ConstantAggregateZero, ConstantPointerNull>(G->getInitializer())) {
          Value* Const = constantToFlightValue(G->getInitializer(), Return);
          storeValueRecurseAfterCheck(
            G->getValueType(), Const,
            MemoryAccessData(nullptr, Lower, AuxP, AuxP, MemoryKind::ThreadLocalInit),
            false, Align(Alignment), AtomicOrdering::NotAtomic, SyncScope::System, Return);
        }
        new StoreInst(Lower, NewG, Return);

        MyThread = NewF->getArg(0);
        RootB = BasicBlock::Create(C, "filc_thread_local_getter_root", NewF);
        Return = ReturnInst::Create(C, UndefValue::get(RawPtrTy), RootB);
        Instruction* FastLower =
          new LoadInst(RawPtrTy, NewG, "filc_fast_load_thread_local_ptr", Return);
        Instruction* FastIsNull = new ICmpInst(
          Return, ICmpInst::ICMP_EQ, FastLower, RawNull, "filc_fast_load_is_null");
        Instruction* NullTerm =
          SplitBlockAndInsertIfThen(expectFalse(FastIsNull, Return), Return, false);
        Instruction* SlowLower = CallInst::Create(
          ThreadLocalEnsureTy, SlowF, { MyThread }, "filc_ensure_thread_local", NullTerm);
        PHINode* Result = PHINode::Create(RawPtrTy, 2, "filc_thread_local_lower", Return);
        Result->addIncoming(FastLower, FastLower->getParent());
        Result->addIncoming(SlowLower, SlowLower->getParent());
        Return->getOperandUse(0) = flightPtrForPayload(Result, Return);
        MyThread = nullptr;
        continue;
      }

      Function* SlowF = Function::Create(PizlonatedGetterTy, GlobalValue::PrivateLinkage,
                                         G->getAddressSpace(), "pizlonatedGS_" + G->getName(), &M);
      SlowF->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
      SlowF->addFnAttr(Attribute::NoUnwind);
      SlowF->addFnAttr(Attribute::NoInline);
      PutImplIntoComdat(G, SlowF);
      
      Constant* NewC = paddedConstant(
        constantToRestConstantWithPtrPlaceholders(G->getInitializer()));
      assert(NewC);
      size_t CSize = DL.getTypeStoreSize(NewC->getType());
      assert(!(CSize % WordSize));

      std::vector<ConstantRelocation> Relocations;
      bool RelocationsSucceeded = computeConstantRelocations(G->getInitializer(), Relocations);
      bool IsConstant = G->isConstant() && RelocationsSucceeded && !Relocations.size();

      GlobalVariable* AuxG;
      Constant* AuxPtr;
      if (hasNonNullPtrs(G->getInitializer())) {
        ArrayType* AuxTy = ArrayType::get(RawPtrTy, CSize / WordSize);
        AuxG = new GlobalVariable(
          M, AuxTy, IsConstant, GlobalValue::InternalLinkage, ConstantAggregateZero::get(AuxTy),
          "pizlonatedDA_" + G->getName());
        PutImplIntoComdat(G, AuxG);
        AuxPtr = AuxG;
      } else {
        AuxG = nullptr;
        AuxPtr = RawNull;
      }

      std::vector<Type*> ObjectGTyFields;
      size_t AlignmentOffset = 0;
      ArrayType* AlignmentTy = nullptr;
      if (Alignment > ObjectSize) {
        AlignmentOffset = Alignment - ObjectSize;
        assert(AlignmentOffset);
        AlignmentTy = ArrayType::get(Int8Ty, AlignmentOffset);
        ObjectGTyFields.push_back(AlignmentTy);
      }
      ObjectGTyFields.push_back(RawPtrTy);
      ObjectGTyFields.push_back(RawPtrTy);
      ObjectGTyFields.push_back(NewC->getType());
      StructType* ObjectGTy = StructType::get(C, ObjectGTyFields);

      GlobalVariable* NewDataG = new GlobalVariable(
        M, ObjectGTy, IsConstant, GlobalValue::InternalLinkage, nullptr, "pizlonatedDO_" + G->getName());
      PutImplIntoComdat(G, NewDataG);

      uint16_t ObjectFlags = ObjectFlagGlobal;
      if (AuxG)
        ObjectFlags |= ObjectFlagGlobalAux;
      if (G->isConstant())
        ObjectFlags |= ObjectFlagReadonly;
      std::vector<Constant*> NewObjCFields;
      if (AlignmentTy)
        NewObjCFields.push_back(ConstantAggregateZero::get(AlignmentTy));
      NewObjCFields.push_back(
        ConstantExpr::getGetElementPtr(ObjectGTy, NewDataG, ConstantInt::get(IntPtrTy, 1)));
      NewObjCFields.push_back(
        ConstantExpr::getGetElementPtr(
          Int8Ty, AuxPtr,
          ConstantInt::get(IntPtrTy, static_cast<uintptr_t>(ObjectFlags) << ObjectAuxFlagsShift)));
      NewObjCFields.push_back(NewC);
      Constant* NewObjC = ConstantStruct::get(ObjectGTy, NewObjCFields);
      NewDataG->setInitializer(NewObjC);

      if (Alignment < WordSize)
        NewDataG->setAlignment(Align(WordSize));
      else
        NewDataG->setAlignment(Align(Alignment));

      Constant* NewDataPayloadC = ConstantExpr::getGetElementPtr(
        Int8Ty, NewDataG, ConstantInt::get(IntPtrTy, ObjectSize + AlignmentOffset));

      if (UnsafeExportGVs.count(G))
        UnsafeExports.push_back(UnsafeExport(G->getName().str(), NewDataPayloadC));
      
      Constant* NewDataObjectC = ConstantExpr::getGetElementPtr(
        Int8Ty, NewDataG, ConstantInt::get(IntPtrTy, AlignmentOffset));
      
      GlobalVariable* NewPtrG = new GlobalVariable(
        M, FlightPtrTy, false, GlobalValue::PrivateLinkage, FlightNull,
        "pizlonatedGP_" + G->getName());
      PutImplIntoComdat(G, NewPtrG);
      NewPtrG->setAlignment(Align(FlightPtrAlign));
      
      BasicBlock* RootBB = BasicBlock::Create(C, "filc_global_getter_root", NewF);
      BasicBlock* OtherCheckBB = BasicBlock::Create(C, "filc_global_getter_other_check", NewF);
      BasicBlock* FastBB = BasicBlock::Create(C, "filc_global_getter_fast", NewF);
      BasicBlock* SlowBB = BasicBlock::Create(C, "filc_global_getter_slow", NewF);
      BasicBlock* SlowRootBB = BasicBlock::Create(C, "filc_global_getter_slow_root", SlowF);
      BasicBlock* RecurseBB = BasicBlock::Create(C, "filc_global_getter_recurse", SlowF);
      BasicBlock* BuildBB = BasicBlock::Create(C, "filc_global_getter_build", SlowF);

      // We can load the NewPtrG in two 64-bit chunks and then check if either the lower or the
      // raw ptr as NULL. If either are NULL, then it's either not initialized yet, or we experienced
      // ptr tearing. This allows us to avoid an expensive 128-bit atomic.
      
      Instruction* Branch = BranchInst::Create(SlowBB, OtherCheckBB, UndefValue::get(Int1Ty), RootBB);
      Value* LoadPtr = loadFlightPtr(NewPtrG, Branch);
      Branch->getOperandUse(0) = new ICmpInst(
        Branch, ICmpInst::ICMP_EQ, flightPtrPtr(LoadPtr, Branch), RawNull, "filc_check_global");

      Instruction* OtherBranch = BranchInst::Create(
        SlowBB, FastBB, UndefValue::get(Int1Ty), OtherCheckBB);
      OtherBranch->getOperandUse(0) = new ICmpInst(
        OtherBranch, ICmpInst::ICMP_EQ, flightPtrLower(LoadPtr, OtherBranch), RawNull,
        "filc_check_global");
      
      ReturnInst::Create(C, LoadPtr, FastBB);

      ReturnInst::Create(
        C,
        CallInst::Create(PizlonatedGetterTy, SlowF, { NewF->getArg(0), NewF->getArg(1) },
                         "filc_call_getter_slow", SlowBB),
        SlowBB);

      Branch = BranchInst::Create(BuildBB, RecurseBB, UndefValue::get(Int1Ty), SlowRootBB);
      Value* Ptr = flightPtrForPayload(NewDataPayloadC, Branch);
      // NOTE: This function call is necessary even for the cases of globals with no meaningful
      // initializer or an initializer we could just optimize to pure data, since we have to tell the
      // runtime about this global so that the GC can find it.
      assert(!MyThread);
      MyThread = SlowF->getArg(0);
      Instruction* Start = CallInst::Create(
        GlobalInitializationStart,
        { MyThread, SlowF->getArg(1), NewPtrG, NewDataObjectC },
        "filc_context_start", Branch);
      Branch->getOperandUse(0) = Start;

      ReturnInst::Create(C, Ptr, RecurseBB);

      Instruction* Return = ReturnInst::Create(C, Ptr, BuildBB);
      if (verbose)
        errs() << "Lowering constant " << *G->getInitializer() << "\n";
      if (RelocationsSucceeded) {
        if (Relocations.size()) {
          std::vector<Constant*> Constants;
          for (const ConstantRelocation& Relocation : Relocations) {
            Constants.push_back(
              ConstantStruct::get(
                ConstantRelocationTy,
                { ConstantInt::get(IntPtrTy, Relocation.Offset),
                  ConstantInt::get(Int32Ty, static_cast<unsigned>(Relocation.Kind)),
                  Relocation.Target }));
          }
          ArrayType* AT = ArrayType::get(ConstantRelocationTy, Constants.size());
          Constant* CA = ConstantArray::get(AT, Constants);
          GlobalVariable* RelocG = new GlobalVariable(
            M, AT, true, GlobalVariable::PrivateLinkage, CA, "filc_constant_relocations");
          CallInst::Create(
            ExecuteConstantRelocations,
            { MyThread, NewDataObjectC, RelocG, ConstantInt::get(IntPtrTy, Constants.size()) },
            "", Return);
        }
      } else {
        Value* C = constantToFlightValue(G->getInitializer(), Return);
        storeValueRecurseAfterCheck(
          G->getInitializer()->getType(), C,
          MemoryAccessData(nullptr, NewDataPayloadC, AuxPtr, AuxPtr, MemoryKind::GlobalInit), false,
          Align(Alignment), AtomicOrdering::NotAtomic, SyncScope::System, Return);
      }
      
      CallInst::Create(GlobalInitializationEnd, { MyThread }, "", Return);
      MyThread = nullptr;
    }
    for (Function* F : Functions) {
      if (F->isIntrinsic())
        continue;
      
      if (verbose)
        errs() << "Function before lowering: " << *F << "\n";

      if (!F->isDeclaration()) {
        FunctionName = getFunctionName(F);
        OldF = F;
        NewF = FunctionToHiddenFunction[F];
        AttrBuilder AB(C, OldF->getAttributes().getFnAttrs());
        AB.removeAttribute(Attribute::AllocKind);
        AB.removeAttribute(Attribute::AllocSize);
        AB.removeAttribute(Attribute::AlwaysInline);
        AB.removeAttribute(Attribute::Builtin);
        AB.removeAttribute(Attribute::Convergent);
        AB.removeAttribute(Attribute::FnRetThunkExtern);
        AB.removeAttribute(Attribute::InlineHint);
        AB.removeAttribute(Attribute::JumpTable);
        AB.removeAttribute(Attribute::Memory);
        AB.removeAttribute(Attribute::Naked);
        AB.removeAttribute(Attribute::NoBuiltin);
        AB.removeAttribute(Attribute::NoCallback);
        AB.removeAttribute(Attribute::NoDuplicate);
        AB.removeAttribute(Attribute::NoFree);
        AB.removeAttribute(Attribute::NonLazyBind);
        AB.removeAttribute(Attribute::NoMerge);
        AB.removeAttribute(Attribute::NoRecurse);
        AB.removeAttribute(Attribute::NoRedZone);
        AB.removeAttribute(Attribute::NoReturn);
        AB.removeAttribute(Attribute::NoSync);
        AB.addAttribute(Attribute::NoUnwind);
        AB.removeAttribute(Attribute::NullPointerIsValid);
        AB.removeAttribute(Attribute::Preallocated);
        AB.removeAttribute(Attribute::ReturnsTwice);
        AB.removeAttribute(Attribute::UWTable);
        AB.removeAttribute(Attribute::WillReturn);
        AB.removeAttribute(Attribute::MustProgress);
        AB.removeAttribute(Attribute::PresplitCoroutine);
        NewF->addFnAttrs(AB);
        assert(NewF);
        OptimizedAccessCheckOrigins.clear();
        InstTypes.clear();
        InstTypeVectors.clear();
        CanonicalPtrAuxBaseVars.clear();
        PtrOperandDatas.clear();
        LocalAllocaDatas.clear();

        UsesVariadicCC = usesVariadicCC(F);

        SmallVector<std::pair<const BasicBlock*, const BasicBlock*>> BackEdges;
        FindFunctionBackedges(*F, BackEdges);
        std::unordered_set<const BasicBlock*> BackEdgePreds;
        for (std::pair<const BasicBlock*, const BasicBlock*>& Edge : BackEdges)
          BackEdgePreds.insert(Edge.first);

        assert(!MyThread);
        MyThread = NewF->getArg(0);
        std::vector<BasicBlock*> Blocks;
        for (BasicBlock& BB : *F)
          Blocks.push_back(&BB);
        assert(!Blocks.empty());
        Args.clear();
        for (BasicBlock* BB : Blocks) {
          BB->removeFromParent();
          BB->insertInto(NewF);
        }
        computeFrameIndexMap(Blocks);
        scheduleChecks(Blocks, BackEdgePreds);
        // Snapshot the instructions before we do crazy stuff.
        std::vector<Instruction*> Instructions;
        std::vector<AllocaInst*> LocalAllocas;
        for (BasicBlock* BB : Blocks) {
          for (Instruction& I : *BB) {
            PointerKind PK = pointerKindDirect(&I);
            if (PK != PointerKind::Escaping) {
              assert(PK == PointerKind::LocalExplicit || PK == PointerKind::LocalNaked);
              assert(BB == Blocks[0]);
              LocalAllocas.push_back(cast<AllocaInst>(&I));
              continue;
            }
            Instructions.push_back(&I);
            captureTypesIfNecessary(&I);
          }

          if (BackEdgePreds.count(BB)) {
            DebugLoc DL = BB->getTerminator()->getDebugLoc();
            // Pollchecks consist of a load and conditional branch to slow path.
            // We temporarily represent pollchecks as unconditional and add the
            // condition in the DeleteRedundantPollchecks pass that runs after
            // optimizations.
            CallInst::Create(Pollcheck, {MyThread, getOrigin(DL)}, "",
                             BB->getTerminator())
                ->setDebugLoc(DL);
          }
        }

        // Make sure that when folks want to add allocas to the root block, they get a pristine block.
        BasicBlock* RootB = Blocks[0];
        Instruction* InsertionPoint = &*Blocks[0]->getFirstInsertionPt();
        SplitBlock(RootB, InsertionPoint);
        Instruction* AllocaInsertionPoint = RootB->getTerminator();

        for (AllocaInst* AI : LocalAllocas) {
          PointerKind PK = pointerKindDirect(AI);
          assert(PK != PointerKind::Escaping);
          assert(PK == PointerKind::LocalExplicit || PK == PointerKind::LocalNaked);
          LocalAllocaData LAD;
          LAD.Explicit = PK == PointerKind::LocalExplicit;
          LAD.OrigAI = AI;
          LAD.Size = alignedAllocaSize(AI);
          if (verbose)
            errs() << "LAD size for " << AI->getName() << " = " << LAD.Size << "\n";
          assert(!(LAD.Size % WordSize));
          LAD.Payload = new AllocaInst(
            Int8Ty, 0, ConstantInt::get(IntPtrTy, LAD.Size),
            Align(std::max(GCMinAlign,
                           std::max(DL.getABITypeAlign(AI->getAllocatedType()).value(),
                                    AI->getAlign().value()))),
            "filc_local_payload", AllocaInsertionPoint);
          LAD.Payload->setDebugLoc(AI->getDebugLoc());
          LAD.AuxAlloca = new AllocaInst(
            Int8Ty, 0, ConstantInt::get(IntPtrTy, LAD.Size + (LAD.Explicit ? WordSize : 0)),
            Align(WordSize), "filc_local_aux", AllocaInsertionPoint);
          LAD.AuxAlloca->setDebugLoc(AI->getDebugLoc());
          if (LAD.Explicit) {
            LAD.Aux = GetElementPtrInst::Create(
              RawPtrTy, LAD.AuxAlloca, { ConstantInt::get(IntPtrTy, 1) }, "filc_aux_lowers",
              AllocaInsertionPoint);
            LAD.Aux->setDebugLoc(AI->getDebugLoc());
          } else
            LAD.Aux = LAD.AuxAlloca;
          LocalAllocaDatas[AI] = LAD;
        }

        ReturnB = BasicBlock::Create(C, "filc_return_block", NewF);
        if (F->getReturnType() != VoidTy) {
          ReturnPhi = PHINode::Create(
            toFlightType(F->getReturnType()), 1, "filc_return_value", ReturnB);
        }

        std::vector<ArgInfo> AIs = argInfosForFunction(F);
        Type* NormalizedRetType = normalizeRetType(F->getReturnType());
        uint64_t Signature;
        if (UsesVariadicCC)
          Signature = GenericSignature;
        else
          Signature = computeSignature(AIs, NormalizedRetType);
        FunctionType* ImplFuncTy;
        if (Signature == GenericSignature)
          ImplFuncTy = PizlonatedFuncTy;
        else
          ImplFuncTy = fastFunctionTypeForSignature(AIs, NormalizedRetType);
        assert(ImplFuncTy == NewF->getFunctionType());

        ReturnInst* Return;
        if (Signature == GenericSignature) {
          assert(ImplFuncTy == PizlonatedFuncTy);
          ReallyReturnB = BasicBlock::Create(C, "filc_really_return_block", NewF);
          BranchInst* ReturnBranch = BranchInst::Create(ReallyReturnB, ReturnB);
          RetSizePhi = PHINode::Create(IntPtrTy, 1, "filc_ret_size", ReallyReturnB);
          Instruction* ReturnValue = InsertValueInst::Create(
            UndefValue::get(PizlonatedReturnValueTy), ConstantInt::getFalse(Int1Ty), { 0 },
            "filc_insert_has_exception", ReallyReturnB);
          ReturnValue = InsertValueInst::Create(
            ReturnValue, RetSizePhi, { 1 }, "filc_insert_ret_size", ReallyReturnB);
          Return = ReturnInst::Create(C, ReturnValue, ReallyReturnB);
          
          if (F->getReturnType() != VoidTy) {
            Type* T = F->getReturnType();
            RetSizePhi->addIncoming(storeCC(T, ReturnPhi, ReturnBranch, DebugLoc()), ReturnB);
          } else {
            RetSizePhi->addIncoming(
              storeCC(IntPtrTy, ConstantInt::get(IntPtrTy, 0), ReturnBranch, DebugLoc()), ReturnB);
          }
        } else {
          assert(!UsesVariadicCC);
          ReallyReturnB = nullptr;
          RetSizePhi = nullptr;
          Return = ReturnInst::Create(
            C, UndefValue::get(ImplFuncTy->getReturnType()), ReturnB);
          Instruction* ReturnValue = InsertValueInst::Create(
            UndefValue::get(ImplFuncTy->getReturnType()), ConstantInt::getFalse(Int1Ty), { 0 },
            "filc_insert_has_exception", Return);
          if (F->getReturnType() == VoidTy)
            Return->getOperandUse(0) = ReturnValue;
          else {
            Return->getOperandUse(0) = insertAndNormalizeReturn(
              F->getReturnType(), ReturnPhi, ReturnValue, Return);
          }
        }

        ResumeB = BasicBlock::Create(C, "filc_resume_block", NewF);
        Instruction* ReturnValue = InsertValueInst::Create(
          UndefValue::get(ImplFuncTy->getReturnType()), ConstantInt::getTrue(Int1Ty), { 0 },
          "filc_insert_has_exception", ResumeB);
        ReturnInst* ResumeReturn = ReturnInst::Create(C, ReturnValue, ResumeB);

        StructType* MyFrameTy = StructType::get(
          C, { RawPtrTy, RawPtrTy, ArrayType::get(RawPtrTy, FrameSize) });
        Frame = new AllocaInst(MyFrameTy, 0, "filc_my_frame", AllocaInsertionPoint);
        stackOverflowCheck(InsertionPoint);
        Value* ThreadTopFramePtr = threadTopFramePtr(MyThread, InsertionPoint);
        new StoreInst(
          new LoadInst(RawPtrTy, ThreadTopFramePtr, "filc_thread_top_frame", InsertionPoint),
          GetElementPtrInst::Create(
            FrameTy, Frame, { ConstantInt::get(Int32Ty, 0), ConstantInt::get(Int32Ty, 0) },
            "filc_frame_parent_ptr", InsertionPoint),
          InsertionPoint);
        new StoreInst(Frame, ThreadTopFramePtr, InsertionPoint);
        new StoreInst(
          getOrigin(DebugLoc()),
          GetElementPtrInst::Create(
            FrameTy, Frame, { ConstantInt::get(Int32Ty, 0), ConstantInt::get(Int32Ty, 1) },
            "filc_frame_parent_ptr", InsertionPoint),
          InsertionPoint);
        for (size_t FrameIndex = FrameSize; FrameIndex--;)
          recordLowerAtIndex(RawNull, FrameIndex, InsertionPoint);

        for (AllocaInst* AI : LocalAllocas) {
          if (!AlwaysLive.count(AI))
            continue;
          PointerKind PK = pointerKindDirect(AI);
          assert(PK != PointerKind::Escaping);
          assert(PK == PointerKind::LocalExplicit || PK == PointerKind::LocalNaked);
          LocalAllocaData LAD = LocalAllocaDatas[AI];
          initializeNonescapingAlloca(LAD, InsertionPoint);
        }
        
        auto PopFrame = [&] (Instruction* Return) {
          new StoreInst(
            new LoadInst(
              RawPtrTy,
              GetElementPtrInst::Create(
                FrameTy, Frame, { ConstantInt::get(Int32Ty, 0), ConstantInt::get(Int32Ty, 0) },
                "filc_frame_parent_ptr", Return),
              "filc_frame_parent", Return),
            threadTopFramePtr(MyThread, Return),
            Return);
        };
        PopFrame(Return);
        PopFrame(ResumeReturn);

        size_t LastOffset = 0;
        if (F->getFunctionType()->getNumParams()) {
          if (Signature == GenericSignature) {
            LastOffset = argsSize(AIs);
            std::vector<Value*> Vs = loadCC(
              AIs, NewF->getArg(2), CCArgsCheckFailure, InsertionPoint, DebugLoc());
            for (unsigned Index = 0; Index < F->getFunctionType()->getNumParams(); ++Index) {
              Value* V = Vs[Index];
              if (AIs[Index].AK == ArgKind::ByVal) {
                recordLowers(F->getArg(Index), F->getArg(Index)->getType(), V, InsertionPoint);
                Args.push_back(V);
              } else {
                Args.push_back(convertFromNormalizedArgType(
                                 F->getArg(Index)->getType(), V, InsertionPoint));
              }
            }
          } else {
            assert(NewF->getFunctionType()->getNumParams()
                   == F->getFunctionType()->getNumParams() + 2);
            for (unsigned Index = 0; Index < F->getFunctionType()->getNumParams(); ++Index) {
              assert(AIs[Index].AK == ArgKind::Direct);
              Args.push_back(convertFromNormalizedArgType(
                               F->getArg(Index)->getType(),
                               NewF->getArg(2 + Index),
                               InsertionPoint));
            }
          }
        }
        if (UsesVastartOrZargs) {
          assert(Signature == GenericSignature);
          // Do this after we have recorded all the args for GC, so it's safe to have a pollcheck.
          Value* SnapshottedArgsPtr = CallInst::Create(
            PromoteArgsToHeap, { MyThread, NewF->getArg(2) }, "filc_promote_args", InsertionPoint);
          recordLowerAtIndex(
            flightPtrLower(SnapshottedArgsPtr, InsertionPoint), SnapshottedArgsFrameIndex,
            InsertionPoint);
          SnapshottedArgsPtrForVastart = flightPtrWithOffset(
            SnapshottedArgsPtr, ConstantInt::get(IntPtrTy, LastOffset), InsertionPoint);
          SnapshottedArgsPtrForZargs = SnapshottedArgsPtr;
        }

        if (HasSetjmps) {
          assert(SetjmpSetFrameIndex != SIZE_MAX);
          Value* Set = CallInst::Create(
            WeakMapCreate, { MyThread }, "filc_setjmp_set_create", InsertionPoint);
          recordLowerAtIndex(Set, SetjmpSetFrameIndex, InsertionPoint);
        }

        FirstRealBlock = InsertionPoint->getParent();

        std::vector<PHINode*> Phis;
        for (Instruction* I : Instructions) {
          if (PHINode* Phi = dyn_cast<PHINode>(I)) {
            Phis.push_back(Phi);
            continue;
          }
          if (!Phis.empty()) {
            for (PHINode* Phi : Phis)
              recordLowers(Phi, I);
            Phis.clear();
          }
          if (InvokeInst* II = dyn_cast<InvokeInst>(I))
            recordLowers(I, &*II->getNormalDest()->getFirstInsertionPt());
          else if (I->isTerminator())
            assert(I->getType()->isVoidTy());
          else
            recordLowers(I, I->getNextNode());
        }
        assert(Phis.empty());
        capturePointerOperands(Instructions);
        for (Instruction* I : Instructions)
          emitChecksForInst(I);
        for (Instruction* I : LocalAllocas)
          emitChecksForInst(I);
        erase_if(Instructions, [&] (Instruction* I) { return earlyLowerInstruction(I); });
        for (Instruction* I : Instructions)
          lowerInstruction(I);

        for (AllocaInst* AI : LocalAllocas) {
          LocalAllocaData LAD = LocalAllocaDatas[AI];
          AI->replaceAllUsesWith(
            createFlightPtr(
              ConstantExpr::getIntToPtr(ConstantInt::get(IntPtrTy, 0xfee1dead), RawPtrTy),
              LAD.Payload, AI));
          AI->eraseFromParent();
        }
        
        MyThread = nullptr;

        Function* GetterF = GlobalToGetter[OldF];
        assert(GetterF);
        assert(GetterF->isDeclaration());
        BasicBlock* RootBB = BasicBlock::Create(C, "filc_function_getter_root", GetterF);
        Return = ReturnInst::Create(C, UndefValue::get(FlightPtrTy), RootBB);
        Return->getOperandUse(0) = flightPtrForLocalFunction(OldF, Return);

        if (HasSetjmps)
          assert(NewF->callsFunctionThatReturnsTwice());
        
        if (verbose)
          errs() << "New function: " << *NewF << "\n";

        optimizeGetters();

        if (verbose)
          errs() << "New function after getter optimization: " << *NewF << "\n";

        FrameSize = SIZE_MAX;
        NumStackAuxes = SIZE_MAX;
      }
      
      FunctionName = "<internal>";
      OldF = nullptr;
      NewF = nullptr;
    }
    for (GlobalAlias* G : Aliases) {
      Constant* C = G->getAliasee();
      GlobalValue* Aliasee;
      int64_t Offset = 0;
      if (ConstantExpr* CE = dyn_cast<ConstantExpr>(C)) {
        assert(CE->getOpcode() == Instruction::GetElementPtr);
        GetElementPtrInst* GEP = cast<GetElementPtrInst>(getAsInstruction(CE));
        APInt OffsetAP(64, 0, false);
        bool Result = GEP->accumulateConstantOffset(DLBefore, OffsetAP);
        assert(Result);
        Offset += OffsetAP.getZExtValue();
        Aliasee = cast<GlobalValue>(GEP->getPointerOperand());
        GEP->deleteValue();
      } else
        Aliasee = cast<GlobalValue>(C);
      Function* NewF = GlobalToGetter[G];
      Function* TargetF = GlobalToGetter[Aliasee];
      assert(NewF);
      assert(TargetF);
      BasicBlock* BB = BasicBlock::Create(this->C, "filc_alias_global", NewF);
      ReturnInst* Return = ReturnInst::Create(this->C, UndefValue::get(FlightPtrTy), BB);
      Return->getOperandUse(0) = flightPtrWithOffset(
        CallInst::Create(PizlonatedGetterTy, TargetF, { NewF->getArg(0), NewF->getArg(1) },
                         "filc_forward_global", Return),
        ConstantInt::get(IntPtrTy, Offset), Return);
    }
    for (GlobalIFunc* G : IFuncs) {
      assert(!G->isThreadLocal());
      Function* ResolveF = cast<Function>(G->getResolver());
      assert(ResolveF);
      Function* NewF = GlobalToGetter[G];
      assert(NewF);

      GlobalVariable* NewPtrG = new GlobalVariable(
        M, FlightPtrTy, false, GlobalValue::PrivateLinkage, FlightNull,
        "pizlonatedGP_" + G->getName());
      PutImplIntoComdat(G, NewPtrG);
      NewPtrG->setAlignment(Align(FlightPtrAlign));
      
      BasicBlock* RootBB = BasicBlock::Create(C, "filc_global_ifunc_getter_root", NewF);
      BasicBlock* OtherCheckBB = BasicBlock::Create(C, "filc_global_ifunc_getter_other_check", NewF);
      BasicBlock* FastBB = BasicBlock::Create(C, "filc_global_ifunc_getter_fast", NewF);
      BasicBlock* SlowBB = BasicBlock::Create(C, "filc_global_ifunc_getter_slow", NewF);

      // We can load the NewPtrG in two 64-bit chunks and then check if either the lower or the
      // raw ptr as NULL. If either are NULL, then it's either not initialized yet, or we experienced
      // ptr tearing. This allows us to avoid an expensive 128-bit atomic.
      
      Instruction* Branch = BranchInst::Create(SlowBB, OtherCheckBB, UndefValue::get(Int1Ty), RootBB);
      Value* LoadPtr = loadFlightPtr(NewPtrG, Branch);
      Branch->getOperandUse(0) = new ICmpInst(
        Branch, ICmpInst::ICMP_EQ, flightPtrPtr(LoadPtr, Branch), RawNull, "filc_check_global");

      Instruction* OtherBranch = BranchInst::Create(
        SlowBB, FastBB, UndefValue::get(Int1Ty), OtherCheckBB);
      OtherBranch->getOperandUse(0) = new ICmpInst(
        OtherBranch, ICmpInst::ICMP_EQ, flightPtrLower(LoadPtr, OtherBranch), RawNull,
        "filc_check_global_func");
      
      ReturnInst::Create(C, LoadPtr, FastBB);

      ReturnInst* Return = ReturnInst::Create(C, UndefValue::get(FlightPtrTy), SlowBB);
      Return->getOperandUse(0) = CallInst::Create(
          CallIfunc,
          { NewF->getArg(0),
            NewF->getArg(1),
            NewPtrG,
            flightPtrForLocalFunction(ResolveF, Return) },
          "filc_call_ifunc_slow", Return);
    }

    if (lightVerbose || verbose)
      errs() << "Here's the pizlonated module before the final replace/delete pass:\n" << M << "\n";

    Dummy->deleteValue();

    if (verbose)
      errs() << "Deleting ToErase values.\n";
    for (Instruction* I : ToErase)
      I->deleteValue();
    if (verbose)
      errs() << "RAUWing ToDelete values.\n";
    for (GlobalValue* G : ToDelete) {
      if (verbose)
        errs() << "RAUWing " << G->getName() << "\n";
      G->replaceAllUsesWith(UndefValue::get(G->getType())); // FIXME - should be zero
    }
    if (verbose)
      errs() << "Erasing ToDelete values.\n";
    for (GlobalValue* G : ToDelete) {
      if (verbose)
        errs() << "Erasing " << G->getName() << "\n";
      G->eraseFromParent();
    }

    for (auto& Pair : UnsafeFuncs) {
      assert(!M.getNamedValue(Pair.first));
      Pair.second->replaceAllUsesWith(
        Function::Create(UnsafeFuncTy, GlobalVariable::ExternalLinkage, 0, Pair.first, &M));
      Pair.second->deleteValue();
    }
    for (UnsafeExport UE : UnsafeExports) {
      assert(!M.getNamedValue(UE.Name));
      GlobalAlias::create(Int8Ty, 0, GlobalVariable::ExternalLinkage, UE.Name, UE.Aliasee, &M);
    }

    if (lightVerbose || verbose)
      errs() << "Here's the pizlonated module:\n" << M << "\n";
    verifyModule(M);
  }
};

} // anonymous namespace

PreservedAnalyses FilPizlonatorPass::run(Module &M, ModuleAnalysisManager&) {
  Pizlonator P(M);
  P.run();
  return PreservedAnalyses::none();
}

