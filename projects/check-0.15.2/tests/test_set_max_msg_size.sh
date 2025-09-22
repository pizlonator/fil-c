#!/usr/bin/env sh

UNIT_TEST=./check_set_max_msg_size
MAX_MESSAGE_LOG_FILE=${UNIT_TEST}.output
TOO_LONG_MESSAGE="Message string too long"

# This test reduces the maximal message size using the provided function,
# so that the assertion message becomes too long.
# We set the maximal size to 32, which is shorter than the message.

rm -f ${MAX_MESSAGE_LOG_FILE}
${UNIT_TEST} 32 2>&1 | tee ${MAX_MESSAGE_LOG_FILE}

NUM_TOO_LONG_MESSAGES=$(grep "${TOO_LONG_MESSAGE}" ${MAX_MESSAGE_LOG_FILE} | wc -l)

if test ${NUM_TOO_LONG_MESSAGES} -gt 0; then
    echo "Maximal message size was reduced by function call."
    rm -f ${MAX_MESSAGE_LOG_FILE}
else
    echo "ERROR: Maximal message size was not reduced by function call."
    echo "Test output was preserved in ${MAX_MESSAGE_LOG_FILE}"
    exit 1
fi

rm -f ${MAX_MESSAGE_LOG_FILE}
export CK_MAX_MSG_SIZE=32
${UNIT_TEST} 4096 2>&1 | tee ${MAX_MESSAGE_LOG_FILE}

NUM_TOO_LONG_MESSAGES=$(grep "${TOO_LONG_MESSAGE}" ${MAX_MESSAGE_LOG_FILE} | wc -l)

if test ${NUM_TOO_LONG_MESSAGES} -gt 0; then
    echo "Maximal message size was reduced by environment variable."
else
    echo "ERROR: Maximal message size was not reduced by environment variable."
    echo "Test output was preserved in ${MAX_MESSAGE_LOG_FILE}"
    exit 1
fi

rm -f ${MAX_MESSAGE_LOG_FILE}
exit 0
