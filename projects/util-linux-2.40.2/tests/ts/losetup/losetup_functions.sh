
function lo_print {
	local lo=$1
	echo "offset:    $( $TS_CMD_LOSETUP --list --raw -n -O OFFSET $lo )"
	echo "sizelimit: $( $TS_CMD_LOSETUP --list --raw -n -O SIZELIMIT $lo )"
	echo "size:      $( $TS_CMD_LSBLK -o SIZE -b -n -r $lo )"
}
