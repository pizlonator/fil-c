#!/bin/bash
# This script generates some C code for inclusion in the capsh binary.
# The Makefile generally only generates the .c code and compares it
# with the checked in code in the progs directory.

cat<<EOF
#include <stdio.h>

#include "./capshdoc.h"

/*
 * A line by line explanation of each named capability value
 */
EOF

let x=0
while [ -f "../doc/values/${x}.txt" ]; do
    name=$(grep -F ",${x}}" ../libcap/cap_names.list.h|sed -e 's/{"//' -e 's/",/ = /' -e 's/},//')
    echo "static const char *explanation${x}[] = {  /* ${name} */"
    sed -e 's/"/\\"/g' -e 's/^/    "/' -e 's/$/",/' "../doc/values/${x}.txt"
    let x=1+${x}
    echo "    NULL"
    echo "};"
done

cat<<EOF
const char **explanations[] = {
EOF
let y=0
while [ "${y}" -lt "${x}" ]; do
    echo "    explanation${y},"
    let y=1+${y}
done
cat<<EOF
};

const int capsh_doc_limit = ${x};
EOF
