// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/mman.h>

#include <memory>

#include <set>

#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/InlineAdvisor.h"
#include "llvm/Analysis/InlineCost.h"
#include "llvm/Analysis/InlineOrder.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"
#include "llvm/ExecutionEngine/JITLink/JITLinkMemoryManager.h"
#include <llvm/ExecutionEngine/JITLink/aarch64.h>
#include <llvm/ExecutionEngine/JITLink/x86_64.h>
#include "llvm/ExecutionEngine/Orc/AbsoluteSymbols.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/Shared/AllocationActions.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include <llvm/IR/LegacyPassManager.h>
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/RegisterFilPasses.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBufferRef.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Coroutines/CoroCleanup.h"
#include "llvm/Transforms/Coroutines/CoroConditionalWrapper.h"
#include "llvm/Transforms/Coroutines/CoroEarly.h"
#include "llvm/Transforms/Coroutines/CoroSplit.h"
#include "llvm/Transforms/IPO/FunctionAttrs.h"
#include "llvm/Transforms/IPO/GlobalDCE.h"
#include "llvm/Transforms/IPO/Inliner.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Instrumentation/DeleteRedundantPollchecks.h"
#include "llvm/Transforms/Instrumentation/FilPizlonator.h"
#include "llvm/Transforms/Instrumentation/KillUB.h"
#include "llvm/Transforms/Scalar/ADCE.h"
#include "llvm/Transforms/Scalar/CorrelatedValuePropagation.h"
#include "llvm/Transforms/Scalar/DeadStoreElimination.h"
#include "llvm/Transforms/Scalar/EarlyCSE.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/JumpThreading.h"
#include "llvm/Transforms/Scalar/LowerExpectIntrinsic.h"
#include "llvm/Transforms/Scalar/SCCP.h"
#include "llvm/Transforms/Scalar/SROA.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include <llvm/Transforms/Utils/Cloning.h>
#include "llvm-c/Core.h"

#include "filc_llvm_jit_api.h"

using namespace llvm;
using namespace llvm::jitlink;

struct ShimJIT {
    std::unordered_map<std::string, std::string> options;
    FilJITConfiguration filConfig;
};

struct ShimExportMapping {
    std::unordered_map<std::string, void*> mapping;
};

namespace {

const shim_runtime_api* runtime;

// This is an always-on assert. The code below strongly expects it to be always on.
#define ASSERT(exp) do { \
    if ((exp)) \
        break; \
    runtime->pas_panic("%s:%d: %s: assertion %s failed.\n", \
                       __FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
} while (0)

Error makeErr(const Twine &Msg) {
    return make_error<StringError>(Msg, inconvertibleErrorCode());
}

std::unordered_map<std::string, void*> RuntimeMapping;

void initOnce() {
    // TargetRegistry is process-global and registration isn't race-safe.
    static std::once_flag Flag;
    std::call_once(Flag, [] {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();
        InitializeNativeTargetAsmParser();
#include "mapping_init.inc"
    });
}

void* AllocateSection(void* my_thread, void* executable_module,
                      uint64_t size64, uint64_t alignment64, bool executable,
                      const std::string& name)
{
    size_t size = (size_t)size64;
    size_t alignment = (size_t)alignment64;
    ASSERT(size == size64);
    ASSERT(alignment == alignment64);
    
    bool isObject = name.starts_with("filc_jit_object_section");
    bool isAux = name.starts_with("filc_jit_aux_section");

    if (isObject) {
        ASSERT(!executable);
        ASSERT(!isAux);
        return runtime->shim_executable_module_allocate_object(
            my_thread, executable_module, size, alignment);
    }

    if (executable) {
        ASSERT(!isObject);
        ASSERT(!isAux);
        ASSERT(alignment <= 16);
        return runtime->shim_executable_module_allocate_executable(
            my_thread, executable_module, size);
    }

    if (isAux) {
        ASSERT(!isObject);
        ASSERT(!executable);
        ASSERT(alignment <= 8);
        return runtime->shim_executable_module_allocate_aux(my_thread, executable_module, size);
    }

    return runtime->shim_executable_module_allocate_data(
        my_thread, executable_module, size, alignment);
}

void* ResolveSymbol(const std::string& name)
{
    auto iter = RuntimeMapping.find(name);
    if (iter == RuntimeMapping.end())
        return nullptr;
    ASSERT(iter->second);
    return iter->second;
}

// JITLink's default GOT/PLT builder (aarch64 and x86_64 both) puts its GOT in
// this section. Its PLT stubs reach their GOT slot with a short PC-relative
// reference (ADRP+LDR, +-4GB / jmp *x(%rip), +-2GB), so the GOT must live in the
// 128MB executable pool alongside the stubs. Everything else non-executable can
// live anywhere: under the large code model, code reaches data via 64-bit
// absolute materialization.
constexpr StringLiteral GOTSectionName = "$__GOT";

// True for sections that get allocated in the 128MB executable pool.
bool isPoolSection(const Section &Sec) {
    return (Sec.getMemProt() & orc::MemProt::Exec) != orc::MemProt::None ||
        Sec.getName() == GOTSectionName;
}

// ---------------------------------------------------------------------------
// Memory manager: one AllocateSection call per section, no ownership retained.
// ---------------------------------------------------------------------------
class HostSectionAllocator final : public JITLinkMemoryManager {
    class InFlight final : public InFlightAlloc {
        LinkGraph &G;

    public:
        InFlight(LinkGraph &G) : G(G) {}

        void finalize(OnFinalizedFunction OnFinalized) override {
            // No ORC platform, so nothing should have registered alloc actions.
            if (!G.allocActions().empty())
                return OnFinalized(makeErr("unexpected JITLink allocation actions"));
            OnFinalized(FinalizedAlloc(orc::ExecutorAddr()));
        }

        void abandon(OnAbandonedFunction OnAbandoned) override {
            // No free hook: sections allocated for a failed link are leaked.
            OnAbandoned(Error::success());
        }
    };

    void* my_thread;
    void* executable_module;

public:
    HostSectionAllocator(void* my_thread, void* executable_module)
        : my_thread(my_thread), executable_module(executable_module) { }
    
    void allocate(const JITLinkDylib *, LinkGraph &G,
                  OnAllocatedFunction OnAllocated) override {
        for (auto &Sec : G.sections()) {
            if (Sec.blocks().empty())
                continue;
            if (Sec.getMemLifetime() == orc::MemLifetime::NoAlloc)
                return OnAllocated(
                    makeErr("unexpected NoAlloc section " + Sec.getName()));

            SmallVector<Block *, 8> Blocks(Sec.blocks().begin(), Sec.blocks().end());
            llvm::sort(Blocks, [](Block *L, Block *R) {
                if (L->getAddress() != R->getAddress())
                    return L->getAddress() < R->getAddress();
                return L->getSize() < R->getSize();
            });
            std::stable_partition(Blocks.begin(), Blocks.end(),
                                  [](Block *B) { return !B->isZeroFill(); });

            uint64_t Size = 0, Align = 1;
            SmallVector<uint64_t, 8> Offsets;
            for (auto *B : Blocks) {
                Size = alignToBlock(Size, *B);
                Offsets.push_back(Size);
                Size += B->getSize();
                Align = std::max<uint64_t>(Align, B->getAlignment());
            }
            Size = std::max<uint64_t>(Size, 1);

            auto *Base = static_cast<char *>(
                AllocateSection(my_thread, executable_module, Size, Align,
                                /*executable=*/isPoolSection(Sec),
                                Sec.getName().str()));
            if (!Base)
                return OnAllocated(
                    makeErr("AllocateSection failed for " + Sec.getName()));

            std::memset(Base, 0, Size);
            for (size_t I = 0; I != Blocks.size(); ++I) {
                Block *B = Blocks[I];
                char *P = Base + Offsets[I];
                B->setAddress(orc::ExecutorAddr::fromPtr(P));
                if (!B->isZeroFill()) {
                    std::memcpy(P, B->getContent().data(), B->getSize());
                    B->setMutableContent({P, static_cast<size_t>(B->getSize())});
                }
            }
        }

        OnAllocated(std::make_unique<InFlight>(G));
    }

    void deallocate(std::vector<FinalizedAlloc> Allocs,
                    OnDeallocatedFunction OnDeallocated) override {
        for (auto &A : Allocs)
            (void)A.release();
        OnDeallocated(Error::success());
    }
};

// ---------------------------------------------------------------------------
// Graph passes.
// ---------------------------------------------------------------------------

// Nothing registers unwind info (no ORC platform), so .eh_frame and LSDAs are
// dead weight -- and they carry 32-bit pcrel fixups into code. Drop them.
Error removeUnwindSections(LinkGraph &G) {
    SmallPtrSet<Section *, 4> Dead;
    for (auto &Sec : G.sections())
        if (Sec.getName() == ".eh_frame" ||
            Sec.getName().starts_with(".gcc_except_table"))
            Dead.insert(&Sec);
    if (Dead.empty())
        return Error::success();

    for (auto *B : G.blocks()) {
        if (Dead.count(&B->getSection()))
            continue;
        for (auto &E : B->edges())
            if (E.getTarget().isDefined() &&
                Dead.count(&E.getTarget().getBlock().getSection()))
                return makeErr("code/data references unwind section " +
                               E.getTarget().getBlock().getSection().getName());
    }
    for (auto *S : Dead)
        G.removeSection(*S);
    return Error::success();
}

// Deterministic check: every relocation that leaves its own section must either
// stay within the 128MB executable pool (code, JITLink's stubs, and its GOT), or
// reach the full 64-bit address space. Otherwise failures would depend on where
// AllocateSection happened to put things.
Error verifyFarReach(LinkGraph &G) {
    auto Arch = G.getTargetTriple().getArch();
    auto FarSafe = [Arch](Edge::Kind K) {
        switch (Arch) {
        case Triple::aarch64:
            return K == aarch64::Pointer64 || K == aarch64::MoveWide16 ||
                K == aarch64::Delta64;
        case Triple::x86_64:
            return K == x86_64::Pointer64 || K == x86_64::Delta64;
        default:
            return false;
        }
    };
    // PC-relative kinds whose range is below 128MB. These must never cross
    // sections, even within the pool.
    auto TooShortForPool = [Arch](Edge::Kind K) {
        return Arch == Triple::aarch64 &&
            (K == aarch64::CondBranch19PCRel || K == aarch64::TestAndBranch14PCRel ||
             K == aarch64::LDRLiteral19 || K == aarch64::ADRLiteral21);
    };

    for (auto *B : G.blocks())
        for (auto &E : B->edges()) {
            if (!E.isRelocation())
                continue;
            auto &T = E.getTarget();
            if (T.isDefined()) {
                const Section &TS = T.getBlock().getSection();
                if (&TS == &B->getSection())
                    continue;
                if (isPoolSection(B->getSection()) && isPoolSection(TS) &&
                    !TooShortForPool(E.getKind()))
                    continue;
            }
            if (FarSafe(E.getKind()))
                continue;
            return makeErr(Twine("edge ") + G.getEdgeKindName(E.getKind()) +
                           " in " + B->getSection().getName() + " to " +
                           (T.hasName() ? (*T.getName()) : StringRef("<anon>")) +
                           " cannot reach an arbitrarily distant target");
        }
    return Error::success();
}

// ---------------------------------------------------------------------------
// Link context.
// ---------------------------------------------------------------------------
struct LinkOutput {
    Error Err = Error::success();
    bool Finalized = false;
    std::unordered_map<std::string, void *> Exports;
};

class LinkContext final : public JITLinkContext {
    JITLinkMemoryManager &MM;
    LinkOutput &Out;

public:
    LinkContext(JITLinkMemoryManager &MM, LinkOutput &Out)
        : JITLinkContext(/*JD=*/nullptr), MM(MM), Out(Out) {}

    JITLinkMemoryManager &getMemoryManager() override { return MM; }

    void notifyFailed(Error E) override {
        Out.Err = joinErrors(std::move(Out.Err), std::move(E));
    }

    void lookup(const LookupMap &Symbols,
                std::unique_ptr<JITLinkAsyncLookupContinuation> LC) override {
        AsyncLookupResult Result;
        for (auto &KV : Symbols) {
            std::string Name = (*KV.first).str();
            if (void *Addr = ResolveSymbol(Name))
                Result[KV.first] = orc::ExecutorSymbolDef(
                    orc::ExecutorAddr::fromPtr(Addr), JITSymbolFlags::Exported);
            else if (KV.second != SymbolLookupFlags::WeaklyReferencedSymbol)
                return LC->run(makeErr("unresolved external symbol: " + Name));
            // Missing weak references are omitted and resolve to null.
        }
        LC->run(std::move(Result));
    }

    Error notifyResolved(LinkGraph &G) override {
        for (auto *Sym : G.defined_symbols())
            if (Sym->hasName() && Sym->getScope() == Scope::Default)
                Out.Exports[(*Sym->getName()).str()] =
                    Sym->getAddress().toPtr<void *>();
        return Error::success();
    }

    void notifyFinalized(JITLinkMemoryManager::FinalizedAlloc A) override {
        (void)A.release();
        Out.Finalized = true;
    }

    LinkGraphPassFunction getMarkLivePass(const Triple &) const override {
        return markAllSymbolsLive;
    }

    Error modifyPassConfig(LinkGraph &G, PassConfiguration &Config) override {
        Config.PrePrunePasses.insert(Config.PrePrunePasses.begin(),
                                     removeUnwindSections);
        // After the default GOT/PLT builder, so it sees the final edges.
        Config.PostPrunePasses.push_back(verifyFarReach);
        return Error::success();
    }
};

// ---------------------------------------------------------------------------
// Pipeline.
// ---------------------------------------------------------------------------
void optimize(Module &M, TargetMachine &TM, OptimizationLevel Level, FilJITConfiguration& filConfig) {
    LoopAnalysisManager LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    ModuleAnalysisManager MAM;

    PassBuilder PB(&TM);
    TargetLibraryInfoImpl TLII(TM.getTargetTriple());
    FAM.registerPass([&] { return TargetLibraryAnalysis(TLII); });

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    RegisterFilPasses(PB, &filConfig);

    ModulePassManager MPM = Level == OptimizationLevel::O0
        ? PB.buildO0DefaultPipeline(Level)
        : PB.buildPerModuleDefaultPipeline(Level);
    MPM.run(M, MAM);
}

} // anonymous namespace

ShimJIT* ShimJITCreate() {
    return new ShimJIT;
}

void ShimJITSetOption(ShimJIT* jit, const char* key, const char* value) {
    jit->options[key] = value;
}

void ShimJITSetGlobalMapping(ShimJIT* jit, const char* key, shim_ptr ptr) {
    jit->filConfig.GlobalMapping[key] = ptr;
}

void ShimJITSetPointerMapping(ShimJIT* jit, shim_ptr ptr) {
    jit->filConfig.PointerMapping[ptr.ptr] = ptr.lower;
}

struct FilCLayouts {
    const char *Before;
    const char *After;
};

bool filCLayoutsFor(const Triple &T, FilCLayouts &Out) {
    if (T.getArch() == Triple::x86_64) {
        Out.Before = "e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i64:"
                     "64-i128:128-f80:128-n8:16:32:64-S128";
        Out.After = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:"
                    "64-i128:128-f80:128-n8:16:32:64-S128";
        return true;
    }
    if (T.getArch() == Triple::aarch64) {
        Out.Before = "e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i8:8:32-"
                     "i16:16:32-i64:64-i128:128-n32:64-S128-Fn32";
        Out.After = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i8:8:32-"
                    "i16:16:32-i64:64-i128:128-n32:64-S128-Fn32";
        return true;
    }
    return false;
}

void* ShimJITCompile(void* my_thread, void* jit_session, ShimJIT* session, LLVMModuleRef CMod) {
    initOnce();

    void* executable_module = runtime->shim_allocate_executable_module(my_thread, jit_session);

    OptimizationLevel OL = OptimizationLevel::O2;
    CodeGenOptLevel CGL = CodeGenOptLevel::Default;

    auto iter = session->options.find("opt-level");
    if (iter != session->options.end()) {
        if (iter->second == "0") {
            OL = OptimizationLevel::O0;
            CGL = CodeGenOptLevel::None;
        } else if (iter->second == "1") {
            OL = OptimizationLevel::O1;
            CGL = CodeGenOptLevel::Less;
        } else if (iter->second == "3") {
            OL = OptimizationLevel::O3;
            CGL = CodeGenOptLevel::Aggressive;
        }
    }

    // Host triple/CPU/features, in this process's address space.
    auto JTMB = orc::JITTargetMachineBuilder::detectHost();
    ASSERT(JTMB);
    const Triple &TT = JTMB->getTargetTriple();
    ASSERT(TT.isOSBinFormatELF());
    ASSERT(TT.getArch() == Triple::aarch64 || TT.getArch() == Triple::x86_64);

    JTMB->setCodeModel(CodeModel::Large)
        .setRelocationModel(Reloc::Static)
        .setCodeGenOptLevel(CGL);
    auto TM = JTMB->createTargetMachine();
    ASSERT(TM);

    // Clone, optimize, codegen. The caller's module is never touched.
    SmallVector<char, 0> Obj;
    {
        std::unique_ptr<Module> M = CloneModule(*unwrap(CMod));
        M->setTargetTriple((*TM)->getTargetTriple().str());
        if (TT.getArch() == Triple::x86_64) {
            M->setDataLayout("e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i64:"
                             "64-i128:128-f80:128-n8:16:32:64-S128");
            M->setDataLayoutAfterFilC("e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:"
                                      "64-i128:128-f80:128-n8:16:32:64-S128");
        } else {
            ASSERT(TT.getArch() == Triple::aarch64);
            M->setDataLayout("e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i8:8:32-"
                             "i16:16:32-i64:64-i128:128-n32:64-S128-Fn32");
            M->setDataLayoutAfterFilC("e-m:e-p270:32:32-p271:32:32-p272:64:64-i8:8:32-"
                                      "i16:16:32-i64:64-i128:128-n32:64-S128-Fn32");
        }

        optimize(*M, **TM, OL, session->filConfig);

        raw_svector_ostream OS(Obj);
        legacy::PassManager CG;
        ASSERT(!(*TM)->addPassesToEmitFile(CG, OS, nullptr,
                                           CodeGenFileType::ObjectFile));
        CG.run(*M);
    } // clone destroyed; nothing below touches the LLVMContext

    // Link. Fully synchronous: returns only after finalize or failure.
    auto SSP = std::make_shared<orc::SymbolStringPool>();
    LinkOutput Out;
    {
        auto G = createLinkGraphFromObject(
            MemoryBufferRef(StringRef(Obj.data(), Obj.size()), "jit-object"), SSP);
        ASSERT(G);
        HostSectionAllocator MemMgr(my_thread, executable_module);
        link(std::move(*G), std::make_unique<LinkContext>(MemMgr, Out));
    } // graph, context, and all SymbolStringPtrs gone before SSP

    if (Out.Err) {
        std::string Msg = toString(std::move(Out.Err));
        errs() << "JIT link failed: " << Msg << "\n";
    }
    ASSERT(!Out.Err);
    ASSERT(Out.Finalized);

    ShimExportMapping* mapping = new ShimExportMapping();
    mapping->mapping = std::move(Out.Exports);

    runtime->shim_executable_module_finalize_objects(my_thread, executable_module);
    runtime->shim_executable_module_set_export_mapping(my_thread, executable_module, mapping);

    return executable_module;
}

void ShimJITDestroy(ShimJIT* jit) {
    delete jit;
}

void* ShimExportMappingLookup(ShimExportMapping* mapping, const char* name) {
    std::string mangledName = std::string("pizlonated_") + name;
    auto iter = mapping->mapping.find(mangledName);
    if (iter == mapping->mapping.end())
        return nullptr;
    return iter->second;
}

void ShimExportMappingDestroy(ShimExportMapping* mapping) {
    delete mapping;
}

static const struct shim_api my_api = {
#include "shim_api_init.inc"
};

// The single dynamically exported symbol of libfilcllvmjit.so.  Default
// visibility is spelled out because the TU compiles with
// -fvisibility=hidden (a version script alone cannot re-export a hidden
// symbol).
extern "C" __attribute__((visibility("default")))
const struct shim_api *FilLLVMJIT_GetApi(const shim_runtime_api* runtime_api,
                                         const char* options)
{
    if (options) {
        BumpPtrAllocator Alloc;
        StringSaver Saver(Alloc);
        SmallVector<const char *, 16> Argv{"llvm"};  // argv[0]; the parser skips it
        cl::TokenizeGNUCommandLine(options, Saver, Argv);
        
        cl::ParseCommandLineOptions(Argv.size(), Argv.data(),
                                    /*Overview=*/"", /*Errs=*/nullptr);
    }
    
    runtime = runtime_api;
    ASSERT(runtime->version == SHIM_API_VERSION);
    return &my_api;
}

extern "C" __attribute__((visibility("hidden")))
int atexit(void (*f)(void))
{
    return 0;
}

extern "C" __attribute__((visibility("hidden")))
int __cxa_atexit(void (*f)(void *), void *arg, void *dso)
{
    return 0;
}
