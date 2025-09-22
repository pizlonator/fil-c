# kill tests, or command, will not when /proc is missing.
test -d /proc || ts_skip "/proc not available"

# The test_sigreceive is ready when signal process mask contains SIGHUP
function check_test_sigreceive {
	local rc=0
	local pid=$1

	for i in 0.01 0.1 1 1 1 1; do
		if [ ! -f /proc/$pid/status ]; then
			# The /proc exists, but not status file. Because the
			# process already started it is unlikely the file would
			# appear after any amount of waiting.  Try to sleep for
			# moment and hopefully test_sigreceive is ready to be
			# killed.
			echo "kill_functions.sh: /proc/$pid/status: No such file or directory"
			sleep 2
			rc=1
			break
		fi
		sigmask=$((16#$( awk '/SigCgt/ { print $2}' /proc/$pid/status) ))
		if [ $(( $sigmask & 1 )) == 1 ]; then
			rc=1
			break
		fi
		sleep $i
	done
	return $rc
}
