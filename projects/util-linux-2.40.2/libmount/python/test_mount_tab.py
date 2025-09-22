import os
import sys
import stat
import errno
import functools as ft

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

def parser_errcb(tb, fname, line):
	print("{:s}:{:d}: parse error".format(fname, line))
	return 1

def create_table(f, comments):
	if not f:
		return None

	tb = mnt.Table()
	tb.enable_comments(comments)
	tb.errcb = parser_errcb

	try:
		tb.parse_file(f)
	except Exception:
		print("{:s}: parsing failed".format(f))
		return None
	return tb

def test_copy_fs(ts, argv):
	rc = -1
	tb = create_table(argv[1], False)
	fs = tb.find_target("/", mnt.MNT_ITER_FORWARD)
	if not fs:
		return rc

	print("ORIGINAL:")
	fs.print_debug()

	fs = fs.copy_fs(None)
	if not fs:
		return rc
	print("COPY:")
	fs.print_debug()
	return 0

def test_parse(ts, argv):
	parse_comments = False

	if len(argv) == 3 and argv[2] == "--comments":
		parse_comments = True
	tb = create_table(argv[1], parse_comments)

	if tb.intro_comment:
		print("Initial comment:\n\"{:s}\"".format(tb.intro_comment))
	#while ((fs = tb.next_fs()) != None):
	for fs in iter(ft.partial(tb.next_fs), None):
		fs.print_debug()
	if tb.trailing_comment:
		print("Trailing comment:\n\"{:s}\"".format(tb.trailing_comment))
	return 0

def test_find(ts, argv, dr):
	if len(argv) != 4:
		print("try --help")
		return -errno.EINVAL

	f, find, what = argv[1:]

	tb = create_table(f, False)
	if find.lower() == "source":
		fs = tb.find_source(what, dr)
	elif find.lower() == "target":
		fs = tb.find_target(what, dr)

	if not fs:
		print("{:s}: not found {:s} '{:s}'".format(f, find, what))
	else:
		fs.print_debug()
	return 0

def test_find_fw(ts, argv):
	return test_find(ts, argv, mnt.MNT_ITER_FORWARD)

def test_find_bw(ts, argv):
	return test_find(ts, argv, mnt.MNT_ITER_BACKWARD)

def test_find_pair(ts, argv):
	rc = -1
	tb = create_table(argv[1], False)
	fs = tb.find_pair(argv[2], argv[3], mnt.MNT_ITER_FORWARD)
	if not fs:
		return rc
	fs.print_debug()
	return 0

def test_is_mounted(ts, argv):
	rc = -1
	tb = mnt.Tab(path="/proc/self/mountinfo")
	if not tb:
		print("failed to parse mountinto")
		return rc

	fstab = create_table(argv[1], False)
	if not fstab:
		return rc
	fs = ()
	for fs in ft.iter(tb.next_fs(), -1):
		if tb.is_fs_mounted(fs):
			print("{:s} already mounted on {:s}".format(fs.source, fs.target))
		else:
			print("{:s} not mounted on {:s}".format(fs.source, fs.target))
	return 0

def test_find_mountpoint(ts, argv):
	rc = -1
	tb = mnt.Table("/proc/self/mountinfo")
	if not tb:
		return rc
	fs = tb.find_mountpoint(argv[1], mnt.MNT_ITER_BACKWARD)
	if not fs:
		return rc
	fs.print_debug()
	return 0


tss = (
	( "--parse",    test_parse,        "<file> [--comments] parse and print(tab" ),
	( "--find-forward",  test_find_fw, "<file> <source|target> <string>" ),
	( "--find-backward", test_find_bw, "<file> <source|target> <string>" ),
	( "--find-pair",     test_find_pair, "<file> <source> <target>" ),
	( "--find-mountpoint", test_find_mountpoint, "<path>" ),
	( "--copy-fs",       test_copy_fs, "<file>  copy root FS from the file" ),
	( "--is-mounted",    test_is_mounted, "<fstab> check what from <file> are already mounted" ),
)
sys.exit(mnt_run_test(tss, sys.argv))
