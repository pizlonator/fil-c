ONNX Build Instructions


### Build

 1. do a recursive checkout of [onnxruntime tag 1.16.3](https://github.com/microsoft/onnxruntime)
 1. `$SRC_DIR` and `$BUILD_DIR` are local source and build directories
 1. To run with CUDA, both [CUDA](https://developer.nvidia.com/cuda-downloads) and [cuDNN](https://docs.nvidia.com/deeplearning/cudnn/archives/cudnn_762/cudnn-install/index.html) libraries must be installed.

```
$ cd $SRC_DIR
$ git clone --recursive https://github.com/microsoft/onnxruntime.git && cd onnxruntime && git checkout -b v1.16.3 refs/tags/v1.16.3
$ mkdir $BUILD_DIR/onnxruntime && cd $BUILD_DIR/onnxruntime

```

1. CPU
```
$ cmake -Donnxruntime_BUILD_SHARED_LIB=ON -DBUILD_TESTING=OFF -Donnxruntime_BUILD_UNIT_TESTS=OFF $SRC_DIR/onnxruntime/cmake && make -j$(nproc) && sudo make install
```
2. CUDA
```
cmake -Donnxruntime_BUILD_SHARED_LIB=ON -DBUILD_TESTING=OFF -Donnxruntime_BUILD_UNIT_TESTS=OFF -Donnxruntime_USE_CUDA=ON -Donnxruntime_CUDA_HOME=/usr/local/cuda -Donnxruntime_CUDNN_HOME=/usr/local/cuda -DCMAKE_CUDA_ARCHITECTURES=native -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc $SRC_DIR/onnxruntime/cmake && make -j$(nproc) && sudo make install
```
3. Intel oneDNN

3.0 install [intel oneDNN](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onednn.html)

3.1 clone, build and install [Khronos OpenCL SDK](https://github.com/KhronosGroup/OpenCL-SDK.git). Build dependencies for Fedora are

`sudo dnf install libudev-devel libXrandr-devel  mesa-libGLU-devel mesa-libGL-devel    libX11-devel intel-opencl`

3.2 build and install `onnxruntime` :
```
cmake -Donnxruntime_BUILD_SHARED_LIB=ON -DBUILD_TESTING=OFF -Donnxruntime_BUILD_UNIT_TESTS=OFF -Donnxruntime_USE_DNNL=ON -Donnxruntime_DNNL_GPU_RUNTIME=ocl -Donnxruntime_DNNL_OPENCL_ROOT=$SRC_DIR/OpenCL-SDK/install  $SRC_DIR/onnxruntime/cmake && make -j$(nproc) && sudo make install
```
