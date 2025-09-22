#!/bin/bash
#
# Generate some Go code to make calling into the C code of the .syso
# file easier.

package="${1}"
syso="${2}"

if [[ -z "${syso}" ]]; then
    echo "usage: $0 <package> <.....syso>" >&2
    exit 1
fi

if [[ "${syso%.syso}" == "${syso}" ]]; then
    echo "2nd argument should be a .syso file" >&2
    exit 1
fi

cat<<EOF
package ${package}

import (
	"unsafe"
)

// syso is how we call, indirectly, into the C-code.
func syso(cFn, state unsafe.Pointer)

type sysoCaller struct {
	ptr unsafe.Pointer
}

// call calls the syso linked C-function, $sym().
func (s *sysoCaller) call(data unsafe.Pointer) {
	syso(s.ptr, data)
}
EOF

for sym in $(objdump -x "${syso}" | grep -F 'g     F' | awk '{print $6}'); do
    cat<<EOF

//go:linkname _${sym} ${sym}
var _${sym} byte
var syso__${sym} = &sysoCaller{ptr: unsafe.Pointer(&_${sym})}

EOF
done
