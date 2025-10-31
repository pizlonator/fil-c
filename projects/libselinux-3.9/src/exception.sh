#!/bin/bash

function except() {
case $1 in
    selinux_file_context_cmp) # ignore
    ;;
    *)
echo "
%exception $1 {
  \$action
  if (result < 0) {
     PyErr_SetFromErrno(PyExc_OSError);
     SWIG_fail;
  }
}"
;;
esac
}

# Make sure that selinux.h is included first in order not to depend on the order
# in which "#include <selinux/selinux.h>" appears in other files.
FILE_LIST=(
    ../include/selinux/selinux.h
    ../include/selinux/avc.h
    ../include/selinux/context.h
    ../include/selinux/get_context_list.h
    ../include/selinux/get_default_type.h
    ../include/selinux/label.h
    ../include/selinux/restorecon.h
)
if ! cat "${FILE_LIST[@]}" | ${CC:-gcc} -x c -c -I../include -o temp.o - -aux-info temp.aux
then
    # clang does not support -aux-info so fall back to gcc
    cat "${FILE_LIST[@]}" | gcc -x c -c -I../include -o temp.o - -aux-info temp.aux
fi
for i in `awk '/<stdin>.*extern int/ { print $6 }' temp.aux`; do except $i ; done 
rm -f -- temp.aux temp.o
