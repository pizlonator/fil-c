FROM ubuntu:24.04
 ARG USER_NAME
 ARG USER_ID
 ARG GROUP_ID
# We install some useful packages
 RUN apt-get update -qq
 RUN apt-get install -y vim git g++-riscv64-linux-gnu qemu-user-static ninja-build valgrind curl  clang++-18 gdb clang-format sudo  wget cmake g++
 RUN addgroup --gid $GROUP_ID user; exit 0
 RUN adduser --disabled-password --gecos '' --uid $USER_ID --gid $GROUP_ID $USER_NAME; exit 0
 RUN echo "$USER_NAME:$USER_NAME" | chpasswd && adduser $USER_NAME sudo
 RUN echo '----->'
 RUN echo 'root:Docker!' | chpasswd
 ENV TERM xterm-256color
 USER $USER_NAME
 RUN gcc --version
 RUN cmake --version
