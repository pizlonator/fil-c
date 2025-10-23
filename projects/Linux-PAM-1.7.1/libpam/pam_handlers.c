/* pam_handlers.c -- pam config file parsing and module loading */

/*
 * created by Marc Ewing.
 * Currently maintained by Andrew G. Morgan <morgan@kernel.org>
 *
 */

#include "pam_private.h"
#include "pam_inline.h"

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "pam_line.h"

#define MODULE_CHUNK              4
#define UNKNOWN_MODULE       "<*unknown module*>"
#ifndef _PAM_ISA
#define _PAM_ISA "."
#endif

static void _pam_free_handlers_aux(struct handler **hp);

static int _pam_add_handler(pam_handle_t *pamh
		     , int must_fail, int other, int stack_level, int type
		     , int *actions, const char *mod_path
		     , int argc, char **argv, size_t argvlen);

/* Values for module type */

#define PAM_T_ANY     0
#define PAM_T_AUTH    1
#define PAM_T_SESS    2
#define PAM_T_ACCT    4
#define PAM_T_PASS    8

static int _pam_load_conf_file(pam_handle_t *pamh, const char *config_name
				, const char *service /* specific file */
				, int module_type /* specific type */
				, int include_level /* level of include */
				, int stack_level /* level of substack */
#ifdef PAM_READ_BOTH_CONFS
				, int not_other
#endif /* PAM_READ_BOTH_CONFS */
    );

static int _pam_parse_conf_file(pam_handle_t *pamh, FILE *f
				, const char *known_service /* specific file */
				, int requested_module_type /* specific type */
				, int include_level /* level of include */
				, int stack_level /* level of substack */
#ifdef PAM_READ_BOTH_CONFS
				, int not_other
#endif /* PAM_READ_BOTH_CONFS */
    )
{
    struct pam_line_buffer buffer;
    int x;                    /* read a line from the FILE *f ? */

    _pam_line_buffer_init(&buffer);
    /*
     * read a line from the configuration (FILE *) f
     */
    while ((x = _pam_line_assemble(f, &buffer, ' ')) > 0) {
	char *buf = buffer.assembled;
	char *tok, *nexttok=NULL;
	const char *this_service;
	const char *mod_path;
	int module_type, actions[_PAM_RETURN_VALUES];
	int other;            /* set if module is for PAM_DEFAULT_SERVICE */
	int res;              /* module added successfully? */
	int handler_type = PAM_HT_MODULE; /* regular handler from a module */
	int argc;
	char **argv;
	size_t argvlen;

	D(("LINE: %s", buf));
	if (known_service != NULL) {
	    nexttok = buf;
	    /* No service field: all lines are for the known service. */
	    this_service = known_service;
	} else {
	    this_service = tok = _pam_tokenize(buf, &nexttok);
	}

#ifdef PAM_READ_BOTH_CONFS
	if (not_other)
	    other = 0;
	else
#endif /* PAM_READ_BOTH_CONFS */
	other = !strcasecmp(this_service, PAM_DEFAULT_SERVICE);

	/* accept "service name" or PAM_DEFAULT_SERVICE modules */
	if (!strcasecmp(this_service, pamh->service_name) || other) {
	    int pam_include = 0;
	    int substack = 0;

	    /* This is a service we are looking for */
	    D(("Found PAM config entry for: %s", this_service));

	    tok = _pam_tokenize(NULL, &nexttok);
	    if (tok == NULL) {
	        /* module type does not exist */
	        D(("empty module type for %s", this_service));
	        pam_syslog(pamh, LOG_ERR,
			   "(%s) empty module type", this_service);
	        module_type = (requested_module_type != PAM_T_ANY) ?
		  requested_module_type : PAM_T_AUTH;	/* most sensitive */
	        handler_type = PAM_HT_MUST_FAIL; /* install as normal but fail when dispatched */
	    } else {
		if (tok[0] == '-') { /* do not log module load errors */
		    handler_type = PAM_HT_SILENT_MODULE;
		    ++tok;
		}
		if (!strcasecmp("auth", tok)) {
		    module_type = PAM_T_AUTH;
		} else if (!strcasecmp("session", tok)) {
		    module_type = PAM_T_SESS;
		} else if (!strcasecmp("account", tok)) {
		    module_type = PAM_T_ACCT;
		} else if (!strcasecmp("password", tok)) {
		    module_type = PAM_T_PASS;
		} else {
		    /* Illegal module type */
		    D(("bad module type: %s", tok));
		    pam_syslog(pamh, LOG_ERR, "(%s) illegal module type: %s",
			    this_service, tok);
		    module_type = (requested_module_type != PAM_T_ANY) ?
			    requested_module_type : PAM_T_AUTH;	/* most sensitive */
		    handler_type = PAM_HT_MUST_FAIL; /* install as normal but fail when dispatched */
		}
	    }
	    D(("Using %sconfig entry: %s", handler_type?"BAD ":"", tok));
	    if (requested_module_type != PAM_T_ANY &&
	        module_type != requested_module_type) {
		D(("Skipping config entry: %s (requested=%d, found=%d)",
		   tok, requested_module_type, module_type));
		continue;
	    }

	    /* reset the actions to .._UNDEF's -- this is so that
               we can work out which entries are not yet set (for default). */
	    {
		int i;
		for (i=0; i<_PAM_RETURN_VALUES;
		     actions[i++] = _PAM_ACTION_UNDEF);
	    }
	    tok = _pam_tokenize(NULL, &nexttok);
	    if (tok == NULL) {
		/* no module name given */
		D(("no control flag supplied"));
		pam_syslog(pamh, LOG_ERR,
			   "(%s) no control flag supplied", this_service);
		_pam_set_default_control(actions, _PAM_ACTION_BAD);
		handler_type = PAM_HT_MUST_FAIL;
	    } else if (!strcasecmp("required", tok)) {
		D(("*PAM_F_REQUIRED*"));
		actions[PAM_SUCCESS] = _PAM_ACTION_OK;
		actions[PAM_NEW_AUTHTOK_REQD] = _PAM_ACTION_OK;
                actions[PAM_IGNORE] = _PAM_ACTION_IGNORE;
		_pam_set_default_control(actions, _PAM_ACTION_BAD);
	    } else if (!strcasecmp("requisite", tok)) {
		D(("*PAM_F_REQUISITE*"));
		actions[PAM_SUCCESS] = _PAM_ACTION_OK;
		actions[PAM_NEW_AUTHTOK_REQD] = _PAM_ACTION_OK;
                actions[PAM_IGNORE] = _PAM_ACTION_IGNORE;
		_pam_set_default_control(actions, _PAM_ACTION_DIE);
	    } else if (!strcasecmp("optional", tok)) {
		D(("*PAM_F_OPTIONAL*"));
		actions[PAM_SUCCESS] = _PAM_ACTION_OK;
		actions[PAM_NEW_AUTHTOK_REQD] = _PAM_ACTION_OK;
		_pam_set_default_control(actions, _PAM_ACTION_IGNORE);
	    } else if (!strcasecmp("sufficient", tok)) {
		D(("*PAM_F_SUFFICIENT*"));
		actions[PAM_SUCCESS] = _PAM_ACTION_DONE;
		actions[PAM_NEW_AUTHTOK_REQD] = _PAM_ACTION_DONE;
		_pam_set_default_control(actions, _PAM_ACTION_IGNORE);
	    } else if (!strcasecmp("include", tok)) {
		D(("*PAM_F_INCLUDE*"));
		pam_include = 1;
		substack = 0;
	    } else if (!strcasecmp("substack", tok)) {
		D(("*PAM_F_SUBSTACK*"));
		pam_include = 1;
		substack = 1;
	    } else {
		D(("will need to parse %s", tok));
		_pam_parse_control(actions, tok);
		/* by default the default is to treat as failure */
		_pam_set_default_control(actions, _PAM_ACTION_BAD);
	    }

	    tok = _pam_tokenize(NULL, &nexttok);
	    if (pam_include) {
		if (substack) {
		    res = _pam_add_handler(pamh, PAM_HT_SUBSTACK, other,
				stack_level, module_type, actions, tok,
				0, NULL, 0);
		    if (res != PAM_SUCCESS) {
			pam_syslog(pamh, LOG_ERR, "error adding substack %s", tok);
			D(("failed to load module - aborting"));
			return PAM_ABORT;
		    }
		}
		if (_pam_load_conf_file(pamh, tok, this_service, module_type,
		    include_level + 1, stack_level + substack
#ifdef PAM_READ_BOTH_CONFS
					      , !other
#endif /* PAM_READ_BOTH_CONFS */
		    ) == PAM_SUCCESS)
		    continue;
		_pam_set_default_control(actions, _PAM_ACTION_BAD);
		mod_path = NULL;
		handler_type = PAM_HT_MUST_FAIL;
		nexttok = NULL;
	    } else if (tok != NULL) {
		mod_path = tok;
		D(("mod_path = %s",mod_path));
	    } else {
		/* no module name given */
		D(("no module name supplied"));
		pam_syslog(pamh, LOG_ERR,
		           "(%s) no module name supplied", this_service);
		mod_path = NULL;
		handler_type = PAM_HT_MUST_FAIL;
	    }

	    /* nexttok points to remaining arguments... */

	    if (nexttok != NULL) {
		D(("list: %s",nexttok));
	        argvlen = _pam_mkargv(nexttok, &argv, &argc);
		D(("argvlen = %zu",argvlen));
		if (argvlen == 0) {
		    /* memory allocation failed */
		    D(("failed to allocate argument vector"));
		    pam_syslog(pamh, LOG_ERR,
			       "(%s) argument vector allocation failed",
			       this_service);
		    mod_path = NULL;
		    handler_type = PAM_HT_MUST_FAIL;
		}
	    } else {               /* there are no arguments so fix by hand */
		D(("empty argument list"));
		argvlen = 0;
		argc = 0;
		argv = NULL;
	    }

#ifdef PAM_DEBUG
	    {
		int y;

		D(("CONF%s: %s%s %d %s %d"
		   , handler_type==PAM_HT_MUST_FAIL?"<*will fail*>":""
		   , this_service, other ? "(backup)":""
		   , module_type
		   , mod_path, argc));
		for (y = 0; y < argc; y++) {
		    D(("CONF: %s", argv[y]));
		}
		for (y = 0; y<_PAM_RETURN_VALUES; ++y) {
		    D(("RETURN %s(%d) -> %d %s",
		       _pam_token_returns[y], y, actions[y],
		       actions[y]>0 ? "jump":
			_pam_token_actions[-actions[y]]));
		}
	    }
#endif

	    res = _pam_add_handler(pamh, handler_type, other, stack_level
				   , module_type, actions, mod_path
				   , argc, argv, argvlen);
	    if (res != PAM_SUCCESS) {
		pam_syslog(pamh, LOG_ERR, "error loading %s", mod_path);
		D(("failed to load module - aborting"));
		return PAM_ABORT;
	    }
	}
    }

    return ( (x < 0) ? PAM_ABORT:PAM_SUCCESS );
}

static int
_pam_open_config_file(pam_handle_t *pamh
			, const char *service
			, char **path
			, FILE **file)
{
    const char *const pamd_dirs[] = { PAM_CONFIG_DF, PAM_CONFIG_DIST_DF
#ifdef VENDORDIR
                               , PAM_CONFIG_DIST2_DF
#endif
    };
    char *p = NULL;
    FILE *f;
    size_t i;

    /* Absolute path */
    if (service[0] == '/') {
	p = _pam_strdup(service);
	if (p == NULL) {
	    pam_syslog(pamh, LOG_CRIT, "strdup failed");
	    return PAM_BUF_ERR;
	}
    } else if (pamh->confdir != NULL) {
	if ((p = pam_asprintf("%s/%s", pamh->confdir, service)) == NULL) {
	    pam_syslog(pamh, LOG_CRIT, "asprintf failed");
	    return PAM_BUF_ERR;
	}
    }

    if (p != NULL) {
	D(("opening %s", p));
	f = fopen(p, "r");
	if (f != NULL) {
	    *path = p;
	    *file = f;
	    return PAM_SUCCESS;
	}
	_pam_drop(p);
	return PAM_ABORT;
    }

    for (i = 0; i < PAM_ARRAY_SIZE(pamd_dirs); i++) {
	DIAG_PUSH_IGNORE_FORMAT_NONLITERAL
	if ((p = pam_asprintf(pamd_dirs[i], service)) == NULL) {
	    pam_syslog(pamh, LOG_CRIT, "asprintf failed");
	    return PAM_BUF_ERR;
	}
	DIAG_POP_IGNORE_FORMAT_NONLITERAL

	D(("opening %s", p));
	f = fopen(p, "r");
	if (f != NULL) {
	    *path = p;
	    *file = f;
	    return PAM_SUCCESS;
	}
	_pam_drop(p);
    }

    return PAM_ABORT;
}

static int _pam_load_conf_file(pam_handle_t *pamh, const char *config_name
				, const char *service /* specific file */
				, int module_type /* specific type */
				, int include_level /* level of include */
				, int stack_level /* level of substack */
#ifdef PAM_READ_BOTH_CONFS
				, int not_other
#endif /* PAM_READ_BOTH_CONFS */
    )
{
    FILE *f;
    char *path = NULL;
    int retval = PAM_ABORT;

    D(("called."));

    if (include_level >= PAM_SUBSTACK_MAX_LEVEL) {
	D(("maximum level of inclusions reached"));
	pam_syslog(pamh, LOG_ERR, "maximum level of inclusions reached");
	return PAM_ABORT;
    }

    if (config_name == NULL) {
	D(("no config file supplied"));
	pam_syslog(pamh, LOG_ERR, "(%s) no config name supplied", service);
	return PAM_ABORT;
    }

    if (_pam_open_config_file(pamh, config_name, &path, &f) == PAM_SUCCESS) {
	retval = _pam_parse_conf_file(pamh, f, service, module_type, include_level, stack_level
#ifdef PAM_READ_BOTH_CONFS
					      , not_other
#endif /* PAM_READ_BOTH_CONFS */
	    );
	if (retval != PAM_SUCCESS)
	    pam_syslog(pamh, LOG_ERR,
		       "_pam_load_conf_file: error reading %s: %s",
		       path, pam_strerror(pamh, retval));
	_pam_drop(path);
	fclose(f);
    } else {
	D(("unable to open %s", config_name));
	pam_syslog(pamh, LOG_ERR,
		   "_pam_load_conf_file: unable to open config for %s",
		   config_name);
    }

    return retval;
}

/* Parse config file, allocate handler structures, dlopen() */
int _pam_init_handlers(pam_handle_t *pamh)
{
    FILE *f;
    int retval;

    D(("called."));
    IF_NO_PAMH(pamh,PAM_SYSTEM_ERR);

    /* Return immediately if everything is already loaded */
    if (pamh->handlers.handlers_loaded) {
	return PAM_SUCCESS;
    }

    D(("initializing"));

    /* First clean the service structure */

    _pam_free_handlers(pamh);
    if (! pamh->handlers.module) {
	if ((pamh->handlers.module =
	     malloc(MODULE_CHUNK * sizeof(struct loaded_module))) == NULL) {
	    pam_syslog(pamh, LOG_CRIT,
		       "_pam_init_handlers: no memory loading module");
	    return PAM_BUF_ERR;
	}
	pamh->handlers.modules_allocated = MODULE_CHUNK;
	pamh->handlers.modules_used = 0;
    }

    if (pamh->service_name == NULL) {
	return PAM_BAD_ITEM;                /* XXX - better error? */
    }

#ifdef PAM_LOCKING
    /* Is the PAM subsystem locked? */
    {
	 int fd_tmp;

	 if ((fd_tmp = open( PAM_LOCK_FILE, O_RDONLY )) != -1) {
	     pam_syslog(pamh, LOG_ERR,
			"_pam_init_handlers: PAM lockfile ("
			PAM_LOCK_FILE ") exists - aborting");
	      (void) close(fd_tmp);
	      /*
	       * to avoid swamping the system with requests
	       */
	      _pam_start_timer(pamh);
	      pam_fail_delay(pamh, 5000000);
	      _pam_await_timer(pamh, PAM_ABORT);

	      return PAM_ABORT;
	 }
    }
#endif /* PAM_LOCKING */

    /*
     * Now parse the config file(s) and add handlers
     */
    {
	struct stat test_d;

	/* Is there a PAM_CONFIG_D directory? */
	if (pamh->confdir != NULL ||
	    (stat(PAM_CONFIG_D, &test_d) == 0 && S_ISDIR(test_d.st_mode)) ||
	    (stat(PAM_CONFIG_DIST_D, &test_d) == 0 && S_ISDIR(test_d.st_mode))
#ifdef PAM_CONFIG_DIST2_D
	    || (stat(PAM_CONFIG_DIST2_D, &test_d) == 0
		&& S_ISDIR(test_d.st_mode))
#endif
	   ) {
	    char *path = NULL;
	    int read_something=0;

	    if (_pam_open_config_file(pamh, pamh->service_name, &path, &f) == PAM_SUCCESS) {
		retval = _pam_parse_conf_file(pamh, f, pamh->service_name,
		    PAM_T_ANY, 0, 0
#ifdef PAM_READ_BOTH_CONFS
					      , 0
#endif /* PAM_READ_BOTH_CONFS */
		    );
		if (retval != PAM_SUCCESS) {
		    pam_syslog(pamh, LOG_ERR,
				    "_pam_init_handlers: error reading %s",
				    path);
		    pam_syslog(pamh, LOG_ERR, "_pam_init_handlers: [%s]",
				    pam_strerror(pamh, retval));
		} else {
		    read_something = 1;
		}
		_pam_drop(path);
		fclose(f);
	    } else {
		D(("unable to open configuration for %s", pamh->service_name));
#ifdef PAM_READ_BOTH_CONFS
		D(("checking %s", PAM_CONFIG));

		if (pamh->confdir == NULL
		    && (f = fopen(PAM_CONFIG,"r")) != NULL) {
		    retval = _pam_parse_conf_file(pamh, f, NULL, PAM_T_ANY, 0, 0, 1);
		    fclose(f);
		} else
#endif /* PAM_READ_BOTH_CONFS */
		retval = PAM_SUCCESS;
		/*
		 * XXX - should we log an error? Some people want to always
		 * use "other"
		 */
	    }

	    if (retval == PAM_SUCCESS) {
		/* now parse the PAM_DEFAULT_SERVICE */

		if (_pam_open_config_file(pamh, PAM_DEFAULT_SERVICE, &path, &f) == PAM_SUCCESS) {
		    /* would test magic here? */
		    retval = _pam_parse_conf_file(pamh, f, PAM_DEFAULT_SERVICE,
			PAM_T_ANY, 0, 0
#ifdef PAM_READ_BOTH_CONFS
						  , 0
#endif /* PAM_READ_BOTH_CONFS */
			);
		    if (retval != PAM_SUCCESS) {
			pam_syslog(pamh, LOG_ERR,
					"_pam_init_handlers: error reading %s",
					path);
			pam_syslog(pamh, LOG_ERR,
					"_pam_init_handlers: [%s]",
					pam_strerror(pamh, retval));
		    } else {
			read_something = 1;
		    }
		    _pam_drop(path);
		    fclose(f);
		} else {
		    D(("unable to open configuration for %s", PAM_DEFAULT_SERVICE));
		    pam_syslog(pamh, LOG_ERR,
				    "_pam_init_handlers: no default config %s",
				    PAM_DEFAULT_SERVICE);
		}
		if (!read_something) {          /* nothing read successfully */
		    retval = PAM_ABORT;
		}
	    }
	} else {
	    if ((f = fopen(PAM_CONFIG, "r")) == NULL) {
		pam_syslog(pamh, LOG_ERR, "_pam_init_handlers: could not open "
				PAM_CONFIG );
		return PAM_ABORT;
	    }

	    retval = _pam_parse_conf_file(pamh, f, NULL, PAM_T_ANY, 0, 0
#ifdef PAM_READ_BOTH_CONFS
					  , 0
#endif /* PAM_READ_BOTH_CONFS */
		);

	    D(("closing configuration file"));
	    fclose(f);
	}
    }

    if (retval != PAM_SUCCESS) {
	/* Read error */
	pam_syslog(pamh, LOG_ERR, "error reading PAM configuration file");
	return PAM_ABORT;
    }

    pamh->handlers.handlers_loaded = 1;

    D(("exiting"));
    return PAM_SUCCESS;
}

static char *
extract_modulename(const char *mod_path)
{
  const char *p = strrchr (mod_path, '/');
  char *dot, *retval;

  if (p == NULL)
    p = mod_path;
  else
    p++;

  if ((retval = _pam_strdup (p)) == NULL)
    return NULL;

  dot = strrchr (retval, '.');
  if (dot)
    *dot = '\0';

  if (*retval == '\0' || strcmp(retval, "?") == 0) {
    /* do not allow empty module name or "?" to avoid confusing audit trail */
    _pam_drop(retval);
    return NULL;
  }

  return retval;
}

static struct loaded_module *
_pam_load_module(pam_handle_t *pamh, const char *mod_path, int handler_type)
{
    int x = 0;
    int success;
    struct loaded_module *mod;

    D(("loading module `%s'", mod_path));

    mod = pamh->handlers.module;

    /* First, ensure the module is loaded */
    while (x < pamh->handlers.modules_used) {
	if (!strcmp(mod[x].name, mod_path)) {  /* case sensitive ! */
	    break;
	}
	x++;
    }
    if (x == pamh->handlers.modules_used) {
	/* Not found */
	if (pamh->handlers.modules_allocated == pamh->handlers.modules_used) {
	    /* will need more memory */
	    void *tmp = realloc(pamh->handlers.module,
                               (pamh->handlers.modules_allocated+MODULE_CHUNK)
                               *sizeof(struct loaded_module));
	    if (tmp == NULL) {
		D(("cannot enlarge module pointer memory"));
		pam_syslog(pamh, LOG_CRIT,
				"realloc returned NULL in _pam_load_module");
		return NULL;
	    }
	    pamh->handlers.module = tmp;
	    pamh->handlers.modules_allocated += MODULE_CHUNK;
	}
	mod = &(pamh->handlers.module[x]);
	/* Be pessimistic... */
	success = PAM_ABORT;

	D(("_pam_dlopen(%s)", mod_path));
	mod->dl_handle = _pam_dlopen(mod_path);
	D(("_pam_dlopen'ed"));
	D(("dlopen'ed"));
	if (mod->dl_handle == NULL) {
	    const char *isa = strstr(mod_path, "$ISA");
	    size_t isa_len = strlen("$ISA");

	    if (isa != NULL) {
		char *mod_full_isa_path = strlen(mod_path) >= INT_MAX ? NULL :
			pam_asprintf("%.*s%s%s", (int)(isa - mod_path),
				     mod_path, _PAM_ISA, isa + isa_len);
		if (mod_full_isa_path == NULL) {
		    D(("couldn't get memory for mod_path"));
		    pam_syslog(pamh, LOG_CRIT, "no memory for module path");
		    success = PAM_ABORT;
		} else {
		    mod->dl_handle = _pam_dlopen(mod_full_isa_path);
		    _pam_drop(mod_full_isa_path);
		}
	    }
	}
	if (mod->dl_handle == NULL) {
	    D(("_pam_dlopen(%s) failed", mod_path));
	    if (handler_type != PAM_HT_SILENT_MODULE)
		pam_syslog(pamh, LOG_ERR, "unable to dlopen(%s): %s", mod_path,
		    _pam_dlerror());
	    /* Don't abort yet; static code may be able to find function.
	     * But defaults to abort if nothing found below... */
	} else {
	    D(("module added successfully"));
	    success = PAM_SUCCESS;
	    mod->type = PAM_MT_DYNAMIC_MOD;
	    pamh->handlers.modules_used++;
	}

	if (success != PAM_SUCCESS) {	         /* add a malformed module */
	    mod->dl_handle = NULL;
	    mod->type = PAM_MT_FAULTY_MOD;
	    pamh->handlers.modules_used++;
	    if (handler_type != PAM_HT_SILENT_MODULE)
		pam_syslog(pamh, LOG_ERR, "adding faulty module: %s", mod_path);
	    success = PAM_SUCCESS;  /* We have successfully added a module */
	}

	/* indicate its name - later we will search for it by this */
	if ((mod->name = _pam_strdup(mod_path)) == NULL) {
	    D(("couldn't get memory for mod_path"));
	    pam_syslog(pamh, LOG_CRIT, "no memory for module path");
	    success = PAM_ABORT;
	}

    } else {                           /* x != pamh->handlers.modules_used */
	mod += x;                                    /* the located module */
	success = PAM_SUCCESS;
    }
    return success == PAM_SUCCESS ? mod : NULL;
}

static int _pam_add_handler(pam_handle_t *pamh
			    , int handler_type, int other, int stack_level
			    , int type, int *actions, const char *mod_path
			    , int argc, char **argv, size_t argvlen)
{
    struct loaded_module *mod = NULL;
    struct handler **handler_p;
    struct handler **handler_p2;
    struct handlers *the_handlers;
    const char *sym, *sym2;
    char *mod_full_path;
    servicefn func, func2;
    int mod_type = PAM_MT_FAULTY_MOD;

    D(("called."));
    IF_NO_PAMH(pamh,PAM_SYSTEM_ERR);

    D(("adding type %d, handler_type %d, module `%s'",
	type, handler_type, mod_path));

    if ((handler_type == PAM_HT_MODULE || handler_type == PAM_HT_SILENT_MODULE) &&
	mod_path != NULL) {
	if (mod_path[0] == '/') {
	    mod = _pam_load_module(pamh, mod_path, handler_type);
	} else if ((mod_full_path = pam_asprintf("%s%s", DEFAULT_MODULE_PATH,
						 mod_path)) != NULL) {
	    mod = _pam_load_module(pamh, mod_full_path, handler_type);
	    _pam_drop(mod_full_path);
	} else {
	    pam_syslog(pamh, LOG_CRIT, "cannot malloc full mod path");
	    return PAM_ABORT;
	}

	if (mod == NULL) {
	    /* if we get here with NULL it means allocation error */
	    return PAM_ABORT;
	}

	mod_type = mod->type;
    }

    if (mod_path == NULL)
	mod_path = UNKNOWN_MODULE;

    /*
     * At this point 'mod' points to the stored/loaded module.
     */

    /* Now define the handler(s) based on mod->dlhandle and type */

    /* decide which list of handlers to use */
    the_handlers = (other) ? &pamh->handlers.other : &pamh->handlers.conf;

    handler_p = handler_p2 = NULL;
    func = func2 = NULL;
    sym2 = NULL;

    /* point handler_p's at the root addresses of the function stacks */
    switch (type) {
    case PAM_T_AUTH:
	handler_p = &the_handlers->authenticate;
	sym = "pam_sm_authenticate";
	handler_p2 = &the_handlers->setcred;
	sym2 = "pam_sm_setcred";
	break;
    case PAM_T_SESS:
	handler_p = &the_handlers->open_session;
	sym = "pam_sm_open_session";
	handler_p2 = &the_handlers->close_session;
	sym2 = "pam_sm_close_session";
	break;
    case PAM_T_ACCT:
	handler_p = &the_handlers->acct_mgmt;
	sym = "pam_sm_acct_mgmt";
	break;
    case PAM_T_PASS:
	handler_p = &the_handlers->chauthtok;
	sym = "pam_sm_chauthtok";
	break;
    default:
	/* Illegal module type */
	D(("illegal module type %d", type));
	return PAM_ABORT;
    }

    /* are the modules reliable? */
    if (mod_type != PAM_MT_DYNAMIC_MOD &&
	 mod_type != PAM_MT_FAULTY_MOD) {
	D(("illegal module library type; %d", mod_type));
	pam_syslog(pamh, LOG_ERR,
			"internal error: module library type not known: %s;%d",
			sym, mod_type);
	return PAM_ABORT;
    }

    /* now identify this module's functions - for non-faulty modules */

    if ((mod_type == PAM_MT_DYNAMIC_MOD) &&
        !(func = _pam_dlsym(mod->dl_handle, sym)) ) {
	pam_syslog(pamh, LOG_ERR, "unable to resolve symbol: %s", sym);
    }
    if (sym2) {
	if ((mod_type == PAM_MT_DYNAMIC_MOD) &&
	    !(func2 = _pam_dlsym(mod->dl_handle, sym2)) ) {
	    pam_syslog(pamh, LOG_ERR, "unable to resolve symbol: %s", sym2);
	}
    }

    /* here func (and perhaps func2) point to the appropriate functions */

    /* add new handler to end of existing list */
    while (*handler_p != NULL) {
	handler_p = &((*handler_p)->next);
    }

    if ((*handler_p = calloc(1, sizeof(struct handler))) == NULL) {
	pam_syslog(pamh, LOG_CRIT, "cannot allocate struct handler #1");
	return (PAM_ABORT);
    }

    (*handler_p)->handler_type = handler_type;
    (*handler_p)->stack_level = stack_level;
    (*handler_p)->func = func;
    memcpy((*handler_p)->actions,actions,sizeof((*handler_p)->actions));
    (*handler_p)->cached_retval = _PAM_INVALID_RETVAL;
    (*handler_p)->cached_retval_p = &((*handler_p)->cached_retval);
    (*handler_p)->argc = argc;
    (*handler_p)->argv = argv;                       /* not a copy */
    if (((*handler_p)->mod_name = extract_modulename(mod_path)) == NULL)
	return PAM_ABORT;

    /* some of the modules have a second calling function */
    if (handler_p2) {
	/* add new handler to end of existing list */
	while (*handler_p2) {
	    handler_p2 = &((*handler_p2)->next);
	}

	if ((*handler_p2 = calloc(1, sizeof(struct handler))) == NULL) {
	    pam_syslog(pamh, LOG_CRIT, "cannot allocate struct handler #2");
	    return (PAM_ABORT);
	}

	(*handler_p2)->handler_type = handler_type;
	(*handler_p2)->stack_level = stack_level;
	(*handler_p2)->func = func2;
	memcpy((*handler_p2)->actions,actions,sizeof((*handler_p2)->actions));
	(*handler_p2)->cached_retval =  _PAM_INVALID_RETVAL;     /* ignored */
	/* Note, this next entry points to the handler_p value! */
	(*handler_p2)->cached_retval_p = &((*handler_p)->cached_retval);
	(*handler_p2)->argc = argc;
	if (argv) {
	    if (((*handler_p2)->argv = malloc(argvlen)) == NULL) {
		pam_syslog(pamh, LOG_CRIT, "cannot malloc argv for handler #2");
		return (PAM_ABORT);
	    }
	    memcpy((*handler_p2)->argv, argv, argvlen);
	}
	if (((*handler_p2)->mod_name = extract_modulename(mod_path)) == NULL)
	    return PAM_ABORT;
    }

    D(("returning successfully"));

    return PAM_SUCCESS;
}

/* Free various allocated structures and dlclose() the libs */
int _pam_free_handlers(pam_handle_t *pamh)
{
    struct loaded_module *mod;

    D(("called."));
    IF_NO_PAMH(pamh,PAM_SYSTEM_ERR);

    mod = pamh->handlers.module;

    /* Close all loaded modules */

    while (pamh->handlers.modules_used) {
	D(("dlclose(%s)", mod->name));
	free(mod->name);
	if (mod->type == PAM_MT_DYNAMIC_MOD) {
	    _pam_dlclose(mod->dl_handle);
	}
	mod++;
	pamh->handlers.modules_used--;
    }

    /* Free all the handlers */

    _pam_free_handlers_aux(&(pamh->handlers.conf.authenticate));
    _pam_free_handlers_aux(&(pamh->handlers.conf.setcred));
    _pam_free_handlers_aux(&(pamh->handlers.conf.acct_mgmt));
    _pam_free_handlers_aux(&(pamh->handlers.conf.open_session));
    _pam_free_handlers_aux(&(pamh->handlers.conf.close_session));
    _pam_free_handlers_aux(&(pamh->handlers.conf.chauthtok));

    _pam_free_handlers_aux(&(pamh->handlers.other.authenticate));
    _pam_free_handlers_aux(&(pamh->handlers.other.setcred));
    _pam_free_handlers_aux(&(pamh->handlers.other.acct_mgmt));
    _pam_free_handlers_aux(&(pamh->handlers.other.open_session));
    _pam_free_handlers_aux(&(pamh->handlers.other.close_session));
    _pam_free_handlers_aux(&(pamh->handlers.other.chauthtok));

    /* no more loaded modules */

    _pam_drop(pamh->handlers.module);

    /* Indicate that handlers are not initialized for this pamh */

    pamh->handlers.handlers_loaded = 0;

    return PAM_SUCCESS;
}

void _pam_start_handlers(pam_handle_t *pamh)
{
    D(("called."));
    /* NB. There is no check for a NULL pamh here, since no return
     * value to communicate the fact!  */

    /* Indicate that handlers are not initialized for this pamh */
    pamh->handlers.handlers_loaded = 0;

    pamh->handlers.modules_allocated = 0;
    pamh->handlers.modules_used = 0;
    pamh->handlers.module = NULL;

    /* initialize the .conf and .other entries */

    pamh->handlers.conf.authenticate = NULL;
    pamh->handlers.conf.setcred = NULL;
    pamh->handlers.conf.acct_mgmt = NULL;
    pamh->handlers.conf.open_session = NULL;
    pamh->handlers.conf.close_session = NULL;
    pamh->handlers.conf.chauthtok = NULL;

    pamh->handlers.other.authenticate = NULL;
    pamh->handlers.other.setcred = NULL;
    pamh->handlers.other.acct_mgmt = NULL;
    pamh->handlers.other.open_session = NULL;
    pamh->handlers.other.close_session = NULL;
    pamh->handlers.other.chauthtok = NULL;
}

void _pam_free_handlers_aux(struct handler **hp)
{
    struct handler *h = *hp;
    struct handler *last;

    D(("called."));
    while (h) {
	last = h;
	_pam_drop(h->argv);  /* This is all allocated in a single chunk */
	_pam_drop(h->mod_name);
	h = h->next;
	pam_overwrite_object(last);
	free(last);
    }

    *hp = NULL;
}
