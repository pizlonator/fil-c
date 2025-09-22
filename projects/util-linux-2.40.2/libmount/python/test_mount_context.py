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

def test_mount(ts, argv):
	idx = 1
	rc = 0

	if len(argv) < 2:
		return -errno.EINVAL

	cxt = mnt.Context()

	if argv[idx] == "-o":
		cxt.options = argv[idx+1]
		idx += 2
	if argv[idx] == "-t":
		cxt.fstype = argv[idx+1]
		idx += 2
	if len(argv) == idx + 1:
		cxt.target = argv[idx]
		idx+=1
	elif (len(argv) == idx + 2):
		cxt.source = argv[idx]
		idx += 1
		cxt.target = argv[idx]
		idx += 1

	try:
		cxt.mount()
	except Exception:
		print("failed to mount")
		return -1
	print("successfully mounted")
	return rc

def test_umount(ts, argv):
	idx = 1
	rc = 0
	if len(argv) < 2:
		return -errno.EINVAL

	cxt = mnt.Context()

	if argv[idx] == "-t":
		cxt.options = argv[idx+1]
		idx += 2

	if argv[idx] == "-f":
		cxt.enable_force(True)

	if argv[idx] == "-l":
		cxt.enable_lazy(True)
		idx += 1
	elif argv[idx] ==  "-r":
		cxt.enable_rdonly_umount(True)
		idx += 1

	if len(argv) == idx + 1:
		cxt.target = argv[idx]
		idx += 1
	else:
		return -errno.EINVAL
	try:
		cxt.umount()
	except Exception:
		print("failed to umount")
		return 1
	print("successfully umounted")
	return rc

def test_flags(ts, argv):
	idx = 1
	rc = 0
	opt = ""
	flags = 0
	cxt = mnt.Context()

	if argv[idx] == "-o":
		cxt.options = argv[idx + 1]
		idx += 2
	if len(argv) == idx + 1:
		cxt.target = argv[idx]
		idx += 1

	try:
		cxt.prepare_mount()
	# catch ioerror here
	except IOError as e:
		print("failed to prepare mount {:s}".format(e.strerror))

	opt = cxt.fs.options
	if (opt):
		print("options: {:s}", opt)

	print("flags: {08:lx}".format(cxt.mflags()))
	return rc

def test_mountall(ts, argv):
	mntrc = 1
	ignored = 1
	idx = 1
	cxt = mnt.Context()

	if len(argv) > 2:
		if argv[idx] == "-O":
			cxt.options_pattern = argv[idx+1]
			idx += 2
		if argv[idx] == "-t":
			cxt.fstype_pattern = argv[idx+1]
			idx += 2

	i = ()
	while (cxt.next_mount()):
		tgt = i.target
		if (ignored == 1):
			print("{:s}: ignored: not match".format(tgt))
		elif (ignored == 2):
			print("{:s}: ignored: already mounted".format(tgt))
		elif (not cxt.status):
			if (mntrc > 0):
				# ?? errno = mntrc
				print("{:s}: mount failed".format(tgt))
			else:
				print("{:s}: mount failed".format(tgt))
		else:
			print("{:s}: successfully mounted".format(tgt))

	return 0


tss = (
	( "--mount",  test_mount,  "[-o <opts>] [-t <type>] <spec>|<src> <target>" ),
	( "--umount", test_umount, "[-t <type>] [-f][-l][-r] <src>|<target>" ),
	( "--mount-all", test_mountall,  "[-O <pattern>] [-t <pattern] mount all filesystems from fstab" ),
	( "--flags", test_flags,   "[-o <opts>] <spec>" )
)
os.umask(stat.S_IWGRP | stat.S_IWOTH) #to be compatible with mount(8)

sys.exit(mnt_run_test(tss, sys.argv))
