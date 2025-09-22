import os
import sys
import stat
import errno

# use "import libmount" for in a standard way installed python binding
import pylibmount as mnt

def usage(tss):
	print("\nUsage:\n\t{:s} <test> [testoptions]\nTests:\n".format(sys.argv[0]))
	for i in tss:
		print("\t{15:-s}".format(i[0]))
		if i[2] != "":
			print(" {:s}\n".format(i[2]))

	print("\n")
	return 1

def mnt_run_test(tss, argv):
	rc = -1
	if ((len(argv) < 2) or (argv[1] == "--help") or (argv[1] == "-h")):
		return usage(tss)

	#mnt_init_debug(0)

	i=()
	for i in tss:
		if i[0] == argv[1]:
			rc = i[1](i, argv[1:])
			if rc:
				print("FAILED [rc={:d}]".format(rc))
			break

	if ((rc < 0) and (i == ())):
		return usage(tss)
	return not not rc #because !!rc is too mainstream for python

def test_replace(ts, argv):
	fs = mnt.Fs()
	tb = mnt.Table()

	if (len(argv) < 3):
		return -1
	tb.enable_comments(True)
	tb.parse_fstab()

	fs.source = argv[1]
	fs.target = argv[2]
	#TODO: resolve None + string
	fs.comment = "# this is new filesystem\n"
	tb.add_fs(fs)
	tb.replace_file(os.environ["LIBMOUNT_FSTAB"])
	return 0

tss = (
	( "--replace",test_replace, "<src> <target>                Add a line to LIBMOUNT_FSTAB and replace the original file" ),
)

sys.exit(mnt_run_test(tss, sys.argv))
