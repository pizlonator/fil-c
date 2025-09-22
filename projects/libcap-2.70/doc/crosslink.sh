#!/bin/bash
#
# So many cross links to maintain. Here is a script that I've used to
# validate things at least conform to some structure:
#
for x in *.? ; do
    y=$(grep -F '.so m' ${x} | awk '{print $2}' | sed -e 's/man..//')
    if [ -z "${y}" ]; then
	continue
    fi
    echo
    echo "###########"
    echo "${x} => ${y}"
    grep -F "${x%.*}" "${y}"
done
