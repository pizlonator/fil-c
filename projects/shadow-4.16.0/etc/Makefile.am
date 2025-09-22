# This is a dummy Makefile.am to get automake work flawlessly,
# and also cooperate to make a distribution for `make dist'

sysconf_DATA = login.defs

defaultdir = $(sysconfdir)/default
default_DATA =

nonpam_files = \
	limits \
	login.access

if !USE_PAM
nonpamdir = $(sysconfdir)
nonpam_DATA = $(nonpam_files)
endif

EXTRA_DIST = \
	$(nonpam_files) \
	$(sysconf_DATA) \
	$(default_DATA)

SUBDIRS = pam.d shadow-maint
