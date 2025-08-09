# This is a toolchain file designed to allow *developers*
# to locally run the RISC-V Vector extensions correctness tests.
#
# We assume the developer machine has two auxiliary programs:
# 1. Spike --- ISA-level simulator
# 2. Proxy kernel --- thin layer that allows to run simple
#    C programs on top of Spike.
#
# Usage:
# $ cmake -DCMAKE_TOOLCHAIN_FILE=toolchain-rvv-spike.cmake
set(CMAKE_SYSTEM_NAME Generic)

set(target       riscv64-linux-gnu)
set(version      13)
set(c_compiler   gcc)
set(cxx_compiler g++)

set(CMAKE_C_COMPILER   "${target}-${c_compiler}-${version}")
set(CMAKE_CXX_COMPILER "${target}-${cxx_compiler}-${version}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

find_file(RISCV_SPIKE "spike" REQUIRED
          DOC "Spike, a RISC-V ISA Simulator (https://github.com/riscv-software-src/riscv-isa-sim)")
find_file(RISCV_PK "pk" REQUIRED
          DOC "RISC-V Proxy Kernel (https://github.com/riscv-software-src/riscv-pk)")

set(RISCV_ISA "rv64gcv")
set(CMAKE_CROSSCOMPILING_EMULATOR "${RISCV_SPIKE};--isa=${RISCV_ISA};${RISCV_PK}")

# Reason: pk expects static linkage
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_compile_options(-march=rv64gcv)
add_compile_definitions(RUN_IN_SPIKE_SIMULATOR)
