#!/bin/sh

FILE_API_SYMBOLS="$1"
FILE_API_DOCS="$2"

if [ ! -f "$FILE_API_SYMBOLS" ]; then
	echo "File $FILE_API_SYMBOLS is missing."
	exit 1
fi

if [ ! -f "$FILE_API_DOCS" ]; then
	echo "File $FILE_API_DOCS is missing."
	exit 1
fi

fail_ct=0
api_symbols=$(awk '/^([[:space:]]+)([[:alnum:]_]+);/ { gsub(";",""); print $1; }' "$FILE_API_SYMBOLS" | sort)
doc_symbols=$(awk '/^([[:space:]])*$/ {next}; !/<.*>/ { print $1 }' "$FILE_API_DOCS" | sort)

echo -n "Checking $FILE_API_SYMBOLS documentation ... "

for sym in $api_symbols; do
	case "$doc_symbols" in
	*"$sym"*)
		#echo -ne "\n '$sym'"
		;;
	*)
		echo -ne "\n '$sym' undocumented"
		fail_ct=$(($fail_ct + 1))
		;;
	esac
done

if [ $fail_ct -ne 0 ]; then
	echo
	echo "$fail_ct symbols is missing in ${FILE_API_DOCS}."
	echo
	exit 1
else
	echo "OK"
fi

exit 0
