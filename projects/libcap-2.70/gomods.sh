#!/bin/bash

version="${1}"
if [[ -z "${version}" ]]; then
    echo "usage: supply a cap/psx module version to target"
    exit 1
fi

for x in $(find . -name 'go.mod'); do
    sed -i -e 's@kernel.org/\([^ ]*\) v.*$@kernel.org/\1 '"${version}@" "${x}"
done
