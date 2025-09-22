#!/bin/sh

set -e

cd $(dirname $0)

testname=$(basename $0)

. ../../common/config.sh
. ../../common/log.sh

export HOME=/root  # seems to be set to /home/travis, breaking some tests

command=""

case  "$testname" in
	*_bash)
		log_start "$0" "propagation of environment variable FOO in command bash: $testname"
		testname=$(echo "$testname" | sed -s 's/_bash$//')
		command="-c bash"
		echo testname: $testname
		;;
	*)
		log_start "$0" "propagation of environment variable FOO: $test"
		;;
esac

save_config

# restore the files on exit
trap 'log_status "$0" "FAILURE"; restore_config' 0

change_config

"./$testname.exp" "$command"

log_status "$0" "SUCCESS"
restore_config
trap '' 0

