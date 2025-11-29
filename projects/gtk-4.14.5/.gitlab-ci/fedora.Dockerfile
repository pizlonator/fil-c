FROM fedora:39

RUN dnf -y install \
    adwaita-icon-theme \
    avahi-gobject-devel \
    cairo-devel \
    cairo-gobject-devel \
    ccache \
    clang \
    clang-analyzer \
    clang-tools-extra \
    colord-devel \
    cups-devel \
    dbus-daemon \
    dbus-x11 \
    dejavu-sans-mono-fonts \
    desktop-file-utils \
    diffutils \
    elfutils-libelf-devel \
    expat-devel \
    fribidi-devel \
    gcc \
    gcc-c++ \
    gdk-pixbuf2-devel \
    gdk-pixbuf2-modules \
    gettext \
    git \
    glib2-devel \
    glib2-static \
    glibc-devel \
    glibc-headers \
    glslc \
    gnupg2 \
    gobject-introspection-devel \
    graphene-devel \
    graphviz \
    gstreamer1-devel \
    gstreamer1-plugins-good \
    gstreamer1-plugins-bad-free-devel \
    gstreamer1-plugins-base-devel \
    hicolor-icon-theme \
    iso-codes \
    itstool \
    json-glib-devel \
    lcov \
    libasan \
    libattr-devel \
    libcloudproviders-devel \
    libepoxy-devel \
    libffi-devel \
    libjpeg-turbo-devel \
    libmount-devel \
    libpng-devel \
    librsvg2 \
    libselinux-devel \
    libtiff-devel \
    libubsan \
    libXcomposite-devel \
    libXcursor-devel \
    libXcursor-devel \
    libXdamage-devel \
    libXfixes-devel \
    libXi-devel \
    libXinerama-devel \
    libxkbcommon-devel \
    libXrandr-devel \
    libXrender-devel \
    libXtst-devel \
    libxslt \
    mesa-dri-drivers \
    mesa-libEGL-devel \
    mesa-libGLES-devel \
    meson \
    mutter \
    ninja-build \
    pango-devel \
    pcre-devel \
    pcre-static \
    pipewire \
    pipewire-gstreamer \
    python3 \
    python3-docutils \
    python3-gobject \
    python3-jinja2 \
    python3-markdown \
    python3-packaging \
    python3-pip \
    python3-pydbus \
    python3-pygments \
    python3-typogrify \
    python3-wheel \
    redhat-rpm-config \
    sassc \
    vulkan-devel \
    wayland-devel \
    wayland-protocols-devel \
    weston \
    weston-libs \
    which \
    wireplumber \
    xorg-x11-server-Xvfb \
 && dnf clean all

# Enable sudo for wheel users
RUN sed -i -e 's/# %wheel/%wheel/' -e '0,/%wheel/{s/%wheel/# %wheel/}' /etc/sudoers

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -G wheel -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
