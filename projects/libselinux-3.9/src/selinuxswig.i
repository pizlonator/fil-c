/* Authors: Dan Walsh
 *          James Athey
 */

%module selinux
%{
	#include "../include/selinux/avc.h"
	#include "../include/selinux/context.h"
	#include "../include/selinux/get_context_list.h"
	#include "../include/selinux/get_default_type.h"
	#include "../include/selinux/label.h"
	#include "../include/selinux/restorecon.h"
	#include "../include/selinux/selinux.h"
%}
%apply int *OUTPUT { int *enforce };
%apply int *OUTPUT { size_t * };

%typedef unsigned mode_t;
%typedef unsigned pid_t;

%typemap(in, numinputs=0) (char ***names, int *len) (char **temp1=NULL, int temp2) {
	$1 = &temp1;
	$2 = &temp2;
}

%typemap(freearg) (char ***names, int *len) {
	int i;
	if (*$1) {
		for (i = 0; i < *$2; i++) {
			free((*$1)[i]);
		}
		free(*$1);
	}
}

%typemap(in, numinputs=0) (char ***) (char **temp=NULL) {
	$1 = &temp;
}

%typemap(freearg) (char ***) {
	if (*$1) freeconary(*$1);
}

/* Ignore functions that don't make sense when wrapped */
%ignore freecon;
%ignore freeconary;

/* Ignore functions that take a function pointer as an argument */
%ignore set_matchpathcon_printf;
%ignore set_matchpathcon_invalidcon;
%ignore set_matchpathcon_canoncon;

%ignore avc_add_callback;

/* Ignore netlink stuff for now */
%ignore avc_netlink_acquire_fd;
%ignore avc_netlink_release_fd;
%ignore avc_netlink_check_nb;

%include "../include/selinux/avc.h"
%include "../include/selinux/context.h"
%include "../include/selinux/get_context_list.h"
%include "../include/selinux/get_default_type.h"
%include "../include/selinux/label.h"
%include "../include/selinux/restorecon.h"
%include "../include/selinux/selinux.h"
