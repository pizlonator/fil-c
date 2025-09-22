if INSTALL_CHECKMK
bin_SCRIPTS = checkmk
TESTS = test/check_checkmk
endif
EXTRA_DIST = test examples doc/checkmk.1
CONFIG_STATUS_DEPENDENCIES = checkmk.in

man_MANS = doc/checkmk.1

clean-local:
	rm -rf test.out

dist-hook:
	rm -rf `find $(distdir)/ -name '.svn'`
