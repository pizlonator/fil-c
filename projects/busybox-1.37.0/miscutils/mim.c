//config:config MIM
//config:	bool "mim (0.5 kb)"
//config:	default y
//config:	depends on FEATURE_SH_EMBEDDED_SCRIPTS
//config:	help
//config:	Run a script from a Makefile-like specification file.
//config:	Unlike 'make' dependencies aren't supported.

//applet:IF_MIM(APPLET_SCRIPTED(mim, scripted, BB_DIR_USR_SBIN, BB_SUID_DROP, mim))

//usage:#define mim_trivial_usage
//usage:	"[-f FILE] [SHELL_OPTIONS] [TARGET] ..."
//usage:#define mim_full_usage "\n\n"
//usage:	"Run a script from a Makefile-like specification file\n"
//usage:     "\n	-f FILE		Spec file (default Mimfile)"
