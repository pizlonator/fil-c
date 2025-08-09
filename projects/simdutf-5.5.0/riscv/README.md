# RISC-V 

We assume that you have a docker or docker-like system.

First initialize the required docker image. Stay in the root directory of the simdutf project and type:

```
./riscv/run-docker-station bash
```

You will now be inside the container.

You can now build and run the RISC-V code as needed:

```
CXX=clang++-18 CXXFLAGS="--target=riscv64-linux-gnu -march=rv64gcv" cmake -DCMAKE_CROSSCOMPILING_EMULATOR=qemu-riscv64-static -DCMAKE_SYSTEM_PROCESSOR=riscv64 -DCMAKE_BUILD_TYPE=Release -B buildriscv
cmake --build buildriscv
export QEMU_LD_PREFIX="/usr/riscv64-linux-gnu"
export QEMU_CPU="rv64,v=on,vlen=128,rvv_ta_all_1s=on,rvv_ma_all_1s=on"
ctest --output-on-failure --test-dir buildriscv
/usr/bin/qemu-riscv64-static buildriscv/tools/sutf
```

Quitting the shell will exit from the container.
