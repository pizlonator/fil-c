#!/bin/bash

set -e
set -x

# pygobject 3.48.2 rebuild — the postlc4 build had a latent Fil-C bug that
# made `python3 -c "import gi"` panic (blocking any meson project that
# requires python3 module 'gi', e.g. libgweather's gen_locations_variant.py).
#
# Fil-C fix (projects/pygobject.projeny):
# - gi/pygobject-object.c pygobject_register_class(): PyGObject_MetaType is
#   still NULL while the _gi module executes (it is installed later from
#   Python via gi/types.py's _gi._install_metaclass).  The pizlonated CPython
#   asserts zhasvalidcap() inside Py_SET_TYPE, so storing the NULL metatype
#   panics ("filc user error: object.h:275 Py_SET_TYPE ... zhasvalidcap(type)
#   failed").  Guard it: only set the metatype when it is non-NULL —
#   PyType_Ready() fixes ob_type up from the base type's metatype anyway,
#   which is what upstream's transient NULL relied on.
cd /sources
rm -rf pizlonated-pygobject
tar -xf pizlonated-pygobject.tar.gz
cd pizlonated-pygobject
mkdir -v filc-build
cd filc-build
meson setup ..                  \
      --prefix=/usr             \
      --buildtype=debugoptimized
ninja
ninja install
cd ../..
rm -rf pizlonated-pygobject
hash -r
