#!/usr/bin/python3

from setuptools import Extension, setup

setup(
    name="selinux",
    version="3.9",
    description="SELinux python 3 bindings",
    author="SELinux Project",
    author_email="selinux@vger.kernel.org",
    ext_modules=[
        Extension('selinux._selinux',
                  sources=['selinuxswig_python.i'],
                  include_dirs=['../include'],
                  library_dirs=['.'],
                  libraries=['selinux']),
        Extension('selinux.audit2why',
                  sources=['audit2why.c'],
                  include_dirs=['../include'],
                  library_dirs=['.'],
                  libraries=['selinux'],
                  extra_link_args=['-l:libsepol.a', '-Wl,--version-script=audit2why.map'])
    ],
)
