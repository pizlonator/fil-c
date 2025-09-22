#!/usr/bin/env sh

OUTPUT_FILE=test.tap

. ./test_vars
. $(dirname $0)/test_output_strings

test_tap_output ( ) {
    rm -f ${OUTPUT_FILE}
    ./ex_output${EXEEXT} "CK_SILENT" "TAP" "${1}" > /dev/null
    actual_tap=`cat ${OUTPUT_FILE} | tr -d "\r"`
    expected_tap="${2}"
    if [ x"${expected_tap}" != x"${actual_tap}" ]; then
        echo "Problem with ex_tap_output${EXEEXT}";
        echo "Expected:";
        echo "${expected_tap}";
        echo
        echo "Got:";
        echo "${actual_tap}";
        exit 1;
    fi
}

test_tap_output "NORMAL"     "${expected_normal_tap}"
test_tap_output "EXIT_TEST" "${expected_aborted_tap}"

exit 0
