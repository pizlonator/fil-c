#!/usr/bin/env sh

OUTPUT_FILE=test.xml
CK_DEFAULT_TIMEOUT=4

. ./test_vars
. $(dirname $0)/test_output_strings

rm -f ${OUTPUT_FILE}
export CK_DEFAULT_TIMEOUT
./ex_output${EXEEXT} CK_MINIMAL XML NORMAL > /dev/null
actual_xml=`cat ${OUTPUT_FILE} | tr -d "\r" | grep -v \<duration\> | grep -v \<datetime\> | grep -v \<path\>`
if [ x"${expected_xml}" != x"${actual_xml}" ]; then
    echo "Problem with ex_xml_output${EXEEXT}";
    echo "Expected:";
    echo "${expected_xml}";
    echo "Got:";
    echo "${actual_xml}";
    exit 1;
fi

actual_duration_count=`grep -c \<duration\> ${OUTPUT_FILE}`
if [ x"${expected_duration_count}" != x"${actual_duration_count}" ]; then
    echo "Wrong number of <duration> elements in ${OUTPUT_FILE}, ${expected_duration_count} vs ${actual_duration_count}";
    exit 1;
fi

num_durations=`grep "\<duration\>" ${OUTPUT_FILE} | wc -l`

i=1
while [ ${i} -le ${num_durations} ]; do
   duration=`grep "\<duration\>" ${OUTPUT_FILE} | head -n ${i} | tail -n 1 | cut -d ">" -f 2 | cut -d "<" -f 1`
   int_duration=`echo $duration | cut -d "." -f 1`
   if [ "${int_duration}" -ne "-1" ] && [ "${int_duration}" -gt "${CK_DEFAULT_TIMEOUT}" ]; then
       echo "Problem with duration ${duration}; is not valid. Should be -1 or in [0, ${CK_DEFAULT_TIMEOUT}]"
       exit 1
   fi

   i=$((i+1))
done

if [ ! -z `which xmllint` ]; then
    xmllint_output=`xmllint ${OUTPUT_FILE}`
    if [ $? -ne 0 ]; then
        echo "xmllint found an issue"
        echo ${xmllint_output}
        exit 1
    fi
fi

exit 0
