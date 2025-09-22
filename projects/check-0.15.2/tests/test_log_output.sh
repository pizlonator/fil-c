#!/usr/bin/env sh

OUTPUT_FILE=test.log

. ./test_vars
. $(dirname $0)/test_output_strings

test_log_output ( ) {
    rm -f ${OUTPUT_FILE}
    ./ex_output${EXEEXT} "${1}" "LOG" "NORMAL" > /dev/null
    actual=`cat ${OUTPUT_FILE} | tr -d "\r"`
    expected=${2}
    if [ x"${expected}" != x"${actual}" ]; then
	echo "Problem with ex_log_output${EXEEXT} ${1} LOG NORMAL";
	echo "Expected:";
	echo "${expected}";
	echo "Got:";
	echo "${actual}";
	exit 1;
    fi
    
}

test_log_output "CK_SILENT"  "${expected_log_log}"
test_log_output "CK_MINIMAL" "${expected_log_log}"
test_log_output "CK_NORMAL"  "${expected_log_log}"
test_log_output "CK_VERBOSE" "${expected_log_log}"
exit 0
