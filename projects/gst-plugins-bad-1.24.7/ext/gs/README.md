# Install the Google Cloud Storage dependencies.
```
sudo apt-get install \
    cmake \
    libcurl3-gnutls-dev \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc \
    flex bison pkg-config \
    curl
```

# Build the Google Cloud Storage library

```
export cmake_prefix=/usr/local

mkdir crc32c
cd crc32c
curl -sSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1
cmake -S . -B build \
    -GNinja \
    -DCMAKE_INSTALL_PREFIX:PATH=$cmake_prefix \
    -DCMAKE_INSTALL_LIBDIR:PATH=lib \
    -DBUILD_SHARED_LIBS=YES \
    -DCRC32C_USE_GLOG=NO \
    -DCRC32C_BUILD_TESTS=NO \
    -DCRC32C_BUILD_BENCHMARKS=NO
cmake --build build --target install
cd ..

mkdir abseil-cpp
cd abseil-cpp
curl -sSL https://github.com/abseil/abseil-cpp/archive/20210324.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' "absl/base/options.h"
cmake -S . -B build \
    -GNinja \
    -DBUILD_TESTING=NO \
    -DCMAKE_INSTALL_PREFIX:PATH=$cmake_prefix \
    -DCMAKE_INSTALL_LIBDIR:PATH=lib \
    -DBUILD_SHARED_LIBS=YES
cmake --build build --target install
cd ..

# Nlohman/json
mkdir json
cd json
curl -sSL https://github.com/nlohmann/json/archive/v3.10.4.tar.gz | \
    tar -xzf - --strip-components=1
cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -H. -Bcmake-out/nlohmann/json && \
    cmake --build cmake-out/nlohmann/json --target install -- -j ${NCPU} && \
    ldconfig
cd ..

mkdir google-cloud-cpp
cd google-cloud-cpp
curl -sSL https://github.com/googleapis/google-cloud-cpp/archive/v1.31.1.tar.gz | \
   tar --strip-components=1 -zxf -
cmake -S . -B build \
    -GNinja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_STANDARD=14 \
    -DCMAKE_INSTALL_PREFIX:PATH=$cmake_prefix \
    -DCMAKE_INSTALL_LIBDIR:PATH=lib \
    -DBUILD_SHARED_LIBS=YES \
    -DBUILD_TESTING=NO \
    -DGOOGLE_CLOUD_CPP_ENABLE=storage
cmake --build build --target install -- -v
cd ..
```

# Running the gs elements locally

When running from the command line or in a container running locally, simply set the credentials by exporting
GOOGLE_APPLICATION_CREDENTIALS. If you are not familiar with this environment variable, check the documentation
https://cloud.google.com/docs/authentication/getting-started
Note that you can restrict a service account to the role Storage Admin or Storage Object Creator instead of the Project
Owner role from the above documentation.

# Running the gs elements in Google Cloud Run

Add the Storage Object Viewer role to the service account assigned to the Cloud Run service where gssrc runs. For gssink
add the role Storage Object Creator. Then just set the service-account-email property on the element.

# Running the gs elements in Google Cloud Kubernetes

You need to set GOOGLE_APPLICATION_CREDENTIALS in the container and ship the json file to which the environment variable
points to.
