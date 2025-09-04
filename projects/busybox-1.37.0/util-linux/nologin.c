//config:config NOLOGIN
//config:	bool "nologin"
//config:	default y
//config:	depends on FEATURE_SH_EMBEDDED_SCRIPTS
//config:	help
//config:	Politely refuse a login
//config:
//config:config NOLOGIN_DEPENDENCIES
//config:	bool "Enable dependencies for nologin"
//config:	default n  # Y default makes it harder to select single-applet test
//config:	depends on NOLOGIN
//config:	select CAT
//config:	select ECHO
//config:	select SLEEP
//config:	help
//config:	nologin is implemented as a shell script. It requires the
//config:	following in the runtime environment:
//config:		cat echo sleep
//config:	If you know these will be available externally you can
//config:	disable this option.

//applet:IF_NOLOGIN(APPLET_SCRIPTED(nologin, scripted, BB_DIR_USR_SBIN, BB_SUID_DROP, nologin))

//usage:#define nologin_trivial_usage
//usage:	""
//usage:#define nologin_full_usage "\n\n"
//usage:	"Politely refuse a login"
