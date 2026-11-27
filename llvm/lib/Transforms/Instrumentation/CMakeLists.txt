add_llvm_component_library(LLVMInstrumentation
  AddressSanitizer.cpp
  BoundsChecking.cpp
  CGProfile.cpp
  ControlHeightReduction.cpp
  DataFlowSanitizer.cpp
  DeleteRedundantPollchecks.cpp
  FilPizlonator.cpp
  GCOVProfiling.cpp
  BlockCoverageInference.cpp
  MemProfiler.cpp
  MemorySanitizer.cpp
  NumericalStabilitySanitizer.cpp
  IndirectCallPromotion.cpp
  InstrOrderFile.cpp
  InstrProfiling.cpp
  KCFI.cpp
  KillUB.cpp
  LowerAllowCheckPass.cpp
  PGOCtxProfFlattening.cpp
  PGOCtxProfLowering.cpp
  PGOForceFunctionAttrs.cpp
  PGOInstrumentation.cpp
  PGOMemOPSizeOpt.cpp
  SanitizerCoverage.cpp
  SanitizerBinaryMetadata.cpp
  ValueProfileCollector.cpp
  ThreadSanitizer.cpp
  TypeSanitizer.cpp
  HWAddressSanitizer.cpp
  RealtimeSanitizer.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms

  DEPENDS
  intrinsics_gen

  LINK_COMPONENTS
  Analysis
  Core
  Demangle
  MC
  Support
  TargetParser
  TransformUtils
  ProfileData
  )

option(FILC_ENABLE_PASS_ASSERTIONS
  "Keep internal assertions active in Fil-C instrumentation passes even when LLVM_ENABLE_ASSERTIONS=OFF"
  OFF)
if(FILC_ENABLE_PASS_ASSERTIONS)
  set_source_files_properties(
    FilPizlonator.cpp
    KillUB.cpp
    PROPERTIES COMPILE_DEFINITIONS "FILC_PASS_ASSERTIONS=1"
  )
endif()
