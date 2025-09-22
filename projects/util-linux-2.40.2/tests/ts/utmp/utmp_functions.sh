ts_check_test_command "$TS_CMD_UTMPDUMP"
ts_check_test_command "$TS_HELPER_SYSINFO"

function utmp_struct_size {
	local size
	# probably "utmpdump -r" could be improved regarding white spaces ...
	local txt="[0] [00000] [    ] [        ] [            ] [                    ] [0.0.0.0        ] [                            ]"

	size=$(echo "$txt" | "$TS_CMD_UTMPDUMP" -r 2>/dev/null | wc -c \
		&& exit ${PIPESTATUS[1]})
	ret=$?
	[ $ret -eq 0 ] || size="0"
	echo "$size"
	return $ret
}

BYTE_ORDER=$($TS_HELPER_SYSINFO byte-order) || ts_failed "byte-order failed"
SIZEOF_UTMP=$(utmp_struct_size) || ts_failed "utmp_struct_size failed"
