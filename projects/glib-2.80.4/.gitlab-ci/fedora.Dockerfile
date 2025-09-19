FROM fedora:37

RUN dnf -y update \
 && dnf -y install \
    bindfs \
    clang \
    clang-analyzer \
    dbus-daemon \
    dbus-devel \
    desktop-file-utils \
    elfutils-libelf-devel \
    findutils \
    fuse \
    gcc \
    gcc-c++ \
    gdb \
    gettext \
    gi-docgen \
    git \
    glibc-devel \
    glibc-gconv-extra \
    glibc-headers \
    glibc-langpack-de \
    glibc-langpack-el \
    glibc-langpack-en \
    glibc-langpack-es \
    glibc-langpack-fa \
    glibc-langpack-fr \
    glibc-langpack-gu \
    glibc-langpack-hr \
    glibc-langpack-ja \
    glibc-langpack-lt \
    glibc-langpack-pl \
    glibc-langpack-ru \
    glibc-langpack-th \
    glibc-langpack-tr \
    "gnome-desktop-testing >= 2018.1" \
    gobject-introspection \
    gobject-introspection-devel \
    gtk-doc \
    itstool \
    lcov \
    libattr-devel \
    libffi-devel \
    libmount-devel \
    libselinux-devel \
    libxslt \
    ncurses-compat-libs \
    ninja-build \
    pcre2-devel \
    "python3-dbusmock >= 0.18.3-2" \
    python3-docutils \
    python3-packaging \
    python3-pip \
    python3-pygments \
    python3-wheel \
    shared-mime-info \
    systemtap-sdt-devel \
    unzip \
    valgrind \
    wget \
    xdg-desktop-portal \
    xz \
    zlib-devel \
 && dnf -y install \
    meson \
    flex \
    bison \
    python3-devel \
    autoconf \
    automake \
    gettext-devel \
    libtool \
    diffutils \
    fontconfig-devel \
    json-glib-devel \
    geoclue2-devel \
    pipewire-devel \
    fuse-devel \
    make \
 && dnf clean all

RUN pip3 install meson==1.2.3

COPY install-gitlab-cobertura-tools.sh .
RUN ./install-gitlab-cobertura-tools.sh

# Set /etc/machine-id as it’s needed for some D-Bus tests
RUN systemd-machine-id-setup

# Enable sudo for wheel users
RUN sed -i -e 's/# %wheel/%wheel/' -e '0,/%wheel/{s/%wheel/# %wheel/}' /etc/sudoers

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -G wheel -ms /bin/bash user

COPY android-ndk.sh .
RUN ./android-ndk.sh

USER user
WORKDIR /home/user

COPY cache-subprojects.sh .
RUN ./cache-subprojects.sh

ENV LANG C.UTF-8
