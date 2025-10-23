%{

/*
 * $Id$
 *
 * Copyright (c) Andrew G. Morgan 1997 <morgan@parc.power.net>
 *
 * This file is covered by the Linux-PAM License (which should be
 * distributed with this file.)
 */

#include <config.h>

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>

#include <security/_pam_types.h>

    extern int yylex(void);

    unsigned long long current_line=0;
    extern char *yytext;

/* XXX - later we'll change this to be the specific conf file(s) */
#define newpamf stderr

#define PAM_D                "./pam.d"
#define PAM_D_MODE           0755
#define PAM_D_MAGIC_HEADER   \
    "#%%PAM-1.0\n" \
    "#[For version 1.0 syntax, the above header is optional]\n"

#define PAM_D_FILE_FMT       PAM_D "/%s"

    const char *old_to_new_ctrl_flag(const char *old);
    void yyerror(const char *format, ...);
%}

%union {
    int def;
    char *string;
}

%token NL EOFILE TOK

%type <string> tok path tokenls

%start complete

%%

complete
:
| complete NL
| complete line
| complete EOFILE {
    return 0;
}
;

line
: tok tok tok path tokenls NL {
    char *filename;
    FILE *conf;
    int i;

    /* make sure we have lower case */
    for (i=0; $1[i]; ++i) {
	$1[i] = tolower((unsigned char)$1[i]);
    }

    /* $1 = service-name */
    yyerror("Appending to " PAM_D "/%s", $1);

    if (asprintf(&filename, PAM_D_FILE_FMT, $1) < 0) {
	yyerror("unable to create filename - aborting");
	exit(1);
    }
    conf = fopen(filename, "r");
    if (conf == NULL) {
	/* new file */
	conf = fopen(filename, "w");
	if (conf != NULL) {
	    fprintf(conf, PAM_D_MAGIC_HEADER);
	    fprintf(conf,
		    "#\n"
		    "# The PAM configuration file for the `%s' service\n"
		    "#\n", $1);
	}
    } else {
	fclose(conf);
	conf = fopen(filename, "a");
    }
    if (conf == NULL) {
	yyerror("trouble opening %s - aborting", filename);
	exit(1);
    }
    free(filename);
    free($1);

    /* $2 = module-type */
    fprintf(conf, "%-10s", $2);
    free($2);

    /* $3 = required etc. */
    {
	const char *trans;

	trans = old_to_new_ctrl_flag($3);
	free($3);
	fprintf(conf, " %-10s", trans);
    }

    /* $4 = module-path */
    fprintf(conf, " %s", $4);
    free($4);

    /* $5 = arguments */
    if ($5 != NULL) {
	fprintf(conf, " \\\n\t\t%s", $5);
	free($5);
    }

    /* end line */
    fprintf(conf, "\n");

    fclose(conf);
}
| error NL {
    yyerror("malformed line");
}
;

tokenls
: {
    $$=NULL;
}
| tokenls tok {
    if ($1) {
	if (asprintf(&$$, "%s %s", $1, $2) < 0) {
	    yyerror("failed to assemble tokenls");
	    exit(1);
	}
	free($1);
	free($2);
    } else {
	$$ = $2;
    }
}
;

path
: TOK {
    /* XXX - this could be used to check if file present */
    $$ = strdup(yytext);
    if ($$ == NULL) {
	yyerror("failed to duplicate path");
	exit(1);
    }
}

tok
: TOK {
    $$ = strdup(yytext);
    if ($$ == NULL) {
	yyerror("failed to duplicate token");
	exit(1);
    }
}

%%

const char *old_to_new_ctrl_flag(const char *old)
{
    static const char *const clist[] = {
	"requisite",
	"required",
	"sufficient",
	"optional",
	NULL,
    };
    int i;

    for (i=0; clist[i]; ++i) {
	if (strcasecmp(clist[i], old) == 0) {
	    break;
	}
    }

    return clist[i];
}

PAM_FORMAT((printf, 1, 2))
void yyerror(const char *format, ...)
{
    va_list args;

    fprintf(stderr, "line %llu: ", current_line);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

int main(void)
{
    if (mkdir(PAM_D, PAM_D_MODE) != 0) {
	yyerror(PAM_D " already exists.. aborting");
	return 1;
    }
    yyparse();
    return 0;
}
