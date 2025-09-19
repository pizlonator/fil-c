FROM registry.gitlab.gnome.org/gnome/glib/fedora:v25

USER root

RUN dnf -y install \
    mingw64-gcc \
    mingw64-gcc-c++ \
    mingw64-gettext \
    mingw64-libffi \
    mingw64-zlib \
 && dnf clean all

WORKDIR /opt
COPY cross_file_mingw64.txt /opt

USER user
WORKDIR /home/user