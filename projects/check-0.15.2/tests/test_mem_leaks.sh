#!/usr/bin/env sh

SCRIPT_PATH="$(dirname "$(readlink -f "$0")")"
if test -z "${1}"; then
    UNIT_TEST="${SCRIPT_PATH}/check_mem_leaks"
else
    UNIT_TEST="${1}"
fi
VALGRIND_LOG_FILE=${UNIT_TEST}.valgrind
LEAK_MESSAGE="are definitely lost"

# This test runs valgrind against the check_mem_leaks unit test
# program, looking for memory leaks. If any are found, "exit 1"
# is invoked, and one must look through the resulting valgrind log
# file for details on the leak.

rm -f ${VALGRIND_LOG_FILE}
libtool --mode=execute valgrind --leak-check=full ${UNIT_TEST} 2>&1 | tee ${VALGRIND_LOG_FILE}

NUM_LEAKS=$(grep "${LEAK_MESSAGE}" ${VALGRIND_LOG_FILE} | wc -l)

if test ${NUM_LEAKS} -gt 0; then
    echo "ERROR: ${NUM_LEAKS} memory leaks were detected by valgrind."
    echo "       Look through ${VALGRIND_LOG_FILE} for details,"
    echo "       searching for \"${LEAK_MESSAGE}\"."
    exit 1
else
    echo "No memory leaks found"
    exit 0
fi
