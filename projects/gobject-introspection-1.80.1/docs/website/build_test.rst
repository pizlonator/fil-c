Build & Test
============

Clone gobject-introspection with git:

.. code:: shell

    git clone https://gitlab.gnome.org/GNOME/gobject-introspection.git
    cd gobject-introspection



Meson
-----

Build:
    .. code:: shell

        meson setup _build
        cd _build
        # To see the build options run "meson configure"
        meson compile

Test:
    .. code:: shell

        meson test  # run tests
        flake8 ..  # run code quality checks


Dependencies
------------

gobject-introspection depends on a row of other packages, either strictly,
optionally or only for testing. The following installation instructions should
over all cases for some common Distributions.

Debian/Ubuntu:
    .. code:: shell

        sudo apt install pkg-config python3-dev flex bison libglib2.0-dev \
            libcairo2-dev libffi-dev python3-mako \
            python3-markdown python3-distutils meson build-essential \
            gtk-doc-tools

Fedora:
    .. code:: shell

        sudo dnf install pkg-config flex bison cairo-devel
            cairo-gobject-devel python3-mako gcc \
            python3-markdown meson libffi-devel python3-devel \
            python3 gtk-doc 
