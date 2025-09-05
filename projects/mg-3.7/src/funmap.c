/*	$OpenBSD: funmap.c,v 1.67 2023/04/21 13:39:37 op Exp $	*/

/* This file is in the public domain */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def.h"
#include "funmap.h"
#include "kbd.h"

/*
 * funmap structure: a list of functions and their command-names/#parameters.
 *
 * If the function is NULL, it must be listed with the same name in the
 * map_table.
 */
struct funmap {
	PF		 fn_funct;
	const		 char *fn_name;
	int		 fn_nparams;
	struct funmap	*fn_next;
};
static struct funmap *funs;

/*
 * 3rd column in the functnames structure indicates how many parameters the
 * function takes in 'normal' usage. This column is only used to identify
 * function profiles when lines of a buffer are being evaluated via excline().
 *
 *  0 = a toggle, non-modifiable insert/delete, region modifier, etc
 *  1 = value can be string or number value (like: file/buf name, search string)
 *  2 = multiple type value required, see auto-execute, or global-set-key, etc
 * -1 = error: interactive command, unsuitable for interpreter
 *
 * Some functions when used interactively may ask for a 'y' or 'n' (or another
 * character) to continue, in excline, a 'y' is assumed. Functions like this
 * have '0' in the 3rd column below.
 */
static struct funmap functnames[] = {
	{apropos_command, "apropos", 1, NULL},
	{toggleaudiblebell, "audible-bell", 0, NULL},
#ifdef ENABLE_AUTOEXEC
	{auto_execute, "auto-execute", 2, NULL},
#endif
	{fillmode, "auto-fill-mode", 0, NULL},
	{indentmode, "auto-indent-mode", 0, NULL},
	{backtoindent, "back-to-indentation", 0, NULL},
	{backuptohomedir, "backup-to-home-directory", 0, NULL},
	{backchar, "backward-char", 1, NULL},
	{delbword, "backward-kill-word", 1, NULL},
	{gotobop, "backward-paragraph", 1, NULL},
	{backword, "backward-word", 1, NULL},
	{gotobob, "beginning-of-buffer", 0, NULL},
	{gotobol, "beginning-of-line", 0, NULL},
	{showmatch, "blink-and-insert", 1, NULL},
	{bsmap, "bsmap-mode", 0, NULL},
	{NULL, "c-x 4 prefix", 0, NULL},
	{NULL, "c-x prefix", 0, NULL},
	{executemacro, "call-last-kbd-macro", 0, NULL},
	{applymacro, "apply-macro-to-region-lines", 0, NULL},
	{capword, "capitalize-word", 1, NULL},
	{changedir, "cd", 0, NULL},
	{clearmark, "clear-mark", 0, NULL},
	{colnotoggle, "column-number-mode", 0, NULL},
	{copyregion, "copy-region-as-kill", 0, NULL},
#ifdef	REGEX
	{cntmatchlines, "count-matches", 1, NULL},
	{cntnonmatchlines, "count-non-matches", 1, NULL},
#endif /* REGEX */
#ifdef CSCOPE
	{cscreatelist, "cscope-create-list-of-files-to-index", 1, NULL},
	{csfuncalled, "cscope-find-called-functions", 1, NULL},
	{csegrep, "cscope-find-egrep-pattern", 1, NULL},
	{csfindinc, "cscope-find-files-including-file", 1, NULL},
	{cscallerfuncs, "cscope-find-functions-calling-this-function", 1, NULL},
	{csdefinition, "cscope-find-global-definition", 1, NULL},
	{csfindfile, "cscope-find-this-file", 1, NULL},
	{cssymbol, "cscope-find-this-symbol", 1, NULL},
	{csfindtext, "cscope-find-this-text-string", 1, NULL},
	{csnextfile, "cscope-next-file", 0, NULL},
	{csnextmatch, "cscope-next-symbol", 0, NULL},
	{csprevfile, "cscope-prev-file", 0, NULL},
	{csprevmatch, "cscope-prev-symbol", 0, NULL},
#endif /* CSCOPE */
	{redefine_key, "define-key", 3, NULL},
	{backdel, "delete-backward-char", 1, NULL},
	{deblank, "delete-blank-lines", 0, NULL},
	{forwdel, "delete-char", 1, NULL},
	{delwhite, "delete-horizontal-space", 0, NULL},
	{delleadwhite, "delete-leading-space", 0, NULL},
#ifdef	REGEX
	{delmatchlines, "delete-matching-lines", 1, NULL},
	{delnonmatchlines, "delete-non-matching-lines", 1, NULL},
#endif /* REGEX */
	{onlywind, "delete-other-windows", 0, NULL},
	{deltrailwhite, "delete-trailing-space", 0, NULL},
	{delwind, "delete-window", 0, NULL},
	{wallchart, "describe-bindings", 0, NULL},
	{desckey, "describe-key-briefly", 1, NULL},
	{diffbuffer, "diff-buffer-with-file", 0, NULL},
	{digit_argument, "digit-argument", 1, NULL},
	{dired_jump, "dired-jump", 1, NULL},
	{helptoggle, "display-help-mode", 0, NULL},
	{timetoggle, "display-time-mode", 0, NULL},
	{lowerregion, "downcase-region", 0, NULL},
	{lowerword, "downcase-word", 1, NULL},
	{showversion, "emacs-version", 0, NULL},
	{showversion, "version", 0, NULL},
	{finishmacro, "end-kbd-macro", 0, NULL},
	{gotoeob, "end-of-buffer", 0, NULL},
	{gotoeol, "end-of-line", 0, NULL},
	{endorexecmacro, "end-or-call-last-kbd-macro", 0, NULL},
	{enlargewind, "enlarge-window", 0, NULL},
	{NULL, "esc prefix", 0, NULL},
	{evalbuffer, "eval-current-buffer", 0, NULL},
	{evalexpr, "eval-expression", 0, NULL},
	{swapmark, "exchange-point-and-mark", 0, NULL},
	{extend, "execute-extended-command", 1, NULL},
	{fillpara, "fill-paragraph", 0, NULL},
	{filevisitalt, "find-alternate-file", 1, NULL},
	{filevisit, "find-file", 1, NULL},
	{poptofile, "find-file-other-window", 1, NULL},
	{filevisitro, "find-file-read-only", 1, NULL},
#ifdef ENABLE_CTAGS
	{findtag, "find-tag", 1, NULL},
#endif
	{forwchar, "forward-char", 1, NULL},
	{gotoeop, "forward-paragraph", 1, NULL},
	{forwword, "forward-word", 1, NULL},
	{bindtokey, "global-set-key", 2, NULL},
	{unbindtokey, "global-unset-key", 1, NULL},
#ifdef ENABLE_COMPILE_GREP
	{globalwdtoggle, "global-wd-mode", 0, NULL},
#endif
	{gotoline, "goto-line", 1, NULL},
	{help_help, "help-help", 0, NULL},
	{indent, "indent-current-line", 0, NULL},
	{insert, "insert", 1, NULL},
	{bufferinsert, "insert-buffer", 1, NULL},
	{fileinsert, "insert-file", 1, NULL},
	{fillword, "insert-with-wrap", 1, NULL},
	{backisearch, "isearch-backward", 1, NULL},
	{forwisearch, "isearch-forward", 1, NULL},
	{joinline, "join-line", 0, NULL},
	{justone, "just-one-space", 0, NULL},
	{ctrlg, "keyboard-quit", 0, NULL},
	{killbuffer_cmd, "kill-buffer", 1, NULL},
	{killline, "kill-line", 1, NULL},
	{killpara, "kill-paragraph", 1, NULL},
	{zaptochar, "zap-to-char", 1, NULL},
	{zapuptochar, "zap-up-to-char", 1, NULL},
	{killregion, "kill-region", 0, NULL},
	{delfword, "kill-word", 1, NULL},
	{toggleleavetmp, "leave-tmpdir-backups", 0, NULL},
	{linenotoggle, "line-number-mode", 0, NULL},
	{listbuffers, "list-buffers", 0, NULL},
	{evalfile, "load", 1, NULL},
	{localbind, "local-set-key", 1, NULL},
	{localunbind, "local-unset-key", 1, NULL},
	{makebkfile, "make-backup-files", 0, NULL},
	{make_dir, "make-directory", 1, NULL},
	{markpara, "mark-paragraph", 1, NULL},
	{markbuffer, "mark-whole-buffer", 0, NULL},
	{do_meta, "meta-key-mode", 0, NULL},	/* better name, anyone? */
	{negative_argument, "negative-argument", 1, NULL},
	{enewline, "newline", 1, NULL},
	{lfindent, "newline-and-indent", 1, NULL},
	{forwline, "next-line", 1, NULL},
	{notabmode, "no-tab-mode", 0, NULL},
	{notmodified, "not-modified", 0, NULL},
	{openline, "open-line", 1, NULL},
	{nextwind, "other-window", 0, NULL},
	{overwrite_mode, "overwrite-mode", 0, NULL},
#ifdef ENABLE_CTAGS
	{poptag, "pop-tag-mark", 0, NULL},
#endif
	{prefixregion, "prefix-region", 0, NULL},
	{backline, "previous-line", 1, NULL},
	{prevwind, "previous-window", 0, NULL},
	{spawncli, "push-shell", 0, NULL},
	{showcwdir, "pwd", 0, NULL},
	{queryrepl, "query-replace", -1, NULL},
#ifdef REGEX
	{re_queryrepl, "query-replace-regexp", -1, NULL},
#endif /* REGEX */
	{quickhelp, "quick-help", 0, NULL},
	{quote, "quoted-insert", 1, NULL},
#ifdef REGEX
	{re_searchagain, "re-search-again", 0, NULL},
	{re_backsearch, "re-search-backward", 0, NULL},
	{re_forwsearch, "re-search-forward", 0, NULL},
#endif /* REGEX */
	{reposition, "recenter", 0, NULL},
	{redraw, "redraw-display", 0, NULL},
#ifdef REGEX
	{re_repl, "replace-regexp", 2, NULL},
	{replstr, "replace-string", 2, NULL},
#endif /* REGEX */
	{revertbuffer, "revert-buffer", 0, NULL},
	{filesave, "save-buffer", 1, NULL},
	{quit, "save-buffers-kill-emacs", 0, NULL},
	{savebuffers, "save-some-buffers", 0, NULL},
	{backpage, "scroll-down", 1, NULL},
	{back1page, "scroll-one-line-down", 1, NULL},
	{forw1page, "scroll-one-line-up", 1, NULL},
	{pagenext, "scroll-other-window", 1, NULL},
	{forwpage, "scroll-up", 1, NULL},
	{searchagain, "search-again", 0, NULL},
	{backsearch, "search-backward", 0, NULL},
	{forwsearch, "search-forward", 0, NULL},
	{ask_selfinsert, "self-insert-char", 1, NULL},
	{selfinsert, "self-insert-command", 1, NULL},		/* startup only */
	{sentencespace, "sentence-end-double-space", 0, NULL},
	{settabw, "set-tab-width", 1, NULL},
#ifdef REGEX
	{setcasefold, "set-case-fold-search", 0, NULL},
#endif /* REGEX */
	{setcasereplace, "set-case-replace", 0, NULL},
	{set_default_mode, "set-default-mode", 1, NULL},
	{setfillcol, "set-fill-column", 1, NULL},
	{setmark, "set-mark-command", 0, NULL},
	{setprefix, "set-prefix-string", 1, NULL},
	{shellcommand, "shell-command", 1, NULL},
	{piperegion, "shell-command-on-region", 1, NULL},
	{shrinkwind, "shrink-window", 1, NULL},
	{space_to_tabstop, "space-to-tabstop", 0, NULL},
	{splitwind, "split-window-vertically", 0, NULL},
	{definemacro, "start-kbd-macro", 0, NULL},
	{spawncli, "suspend-emacs", 0, NULL},
	{spawncli, "suspend", 0, NULL},
	{usebuffer, "switch-to-buffer", 1, NULL},
	{poptobuffer, "switch-to-buffer-other-window", 1, NULL},
#ifdef TOGGLENL
	{togglenewlineprompt, "toggle-newline-prompt", 0, NULL},
#endif /* TOGGLENL */
	{togglereadonly, "toggle-read-only", 0, NULL},
	{togglereadonlyall, "toggle-read-only-all", 0, NULL},
	{twiddle, "transpose-chars", 0, NULL},
	{transposepara, "transpose-paragraphs", 0, NULL},
	{transposeword, "transpose-words", 0, NULL},
	{tutorial, "tutorial", 0, NULL},
	{undo, "undo", 0, NULL},
	{undo_add_boundary, "undo-boundary", 0, NULL},
	{undo_boundary_enable, "undo-boundary-toggle", 0, NULL},
	{undo_enable, "undo-enable", 0, NULL},
	{undo_dump, "undo-list", 0, NULL},
	{universal_argument, "universal-argument", 1, NULL},
	{upperregion, "upcase-region", 0, NULL},
	{upperword, "upcase-word", 1, NULL},
	{togglevisiblebell, "visible-bell", 0, NULL},
#ifdef ENABLE_CTAGS
	{tagsvisit, "visit-tags-table", 0, NULL},
#endif
	{showcpos, "what-cursor-position", 0, NULL},
	{filewrite, "write-file", 1, NULL},
	{yank, "yank", 1, NULL},
	{NULL, NULL, 0, NULL}
};

void
funmap_init(void)
{
	struct funmap *fn;

	for (fn = functnames; fn->fn_name != NULL; fn++) {
		fn->fn_next = funs;
		funs = fn;
	}
}

int
funmap_add(PF fun, const char *fname, int fparams)
{
	struct funmap *fn;

	if ((fn = malloc(sizeof(*fn))) == NULL)
		return (FALSE);

	fn->fn_funct = fun;
	fn->fn_name = fname;
	fn->fn_nparams = fparams;
	fn->fn_next = funs;

	funs = fn;
	return (TRUE);
}

/*
 * Translate from function name to function pointer.
 */
PF
name_function(const char *fname)
{
	struct funmap *fn;

	for (fn = funs; fn != NULL; fn = fn->fn_next) {
		if (strcmp(fn->fn_name, fname) == 0)
			return (fn->fn_funct);
	}
	return (NULL);
}

const char *
function_name(PF fun)
{
	struct funmap *fn;

	for (fn = funs; fn != NULL; fn = fn->fn_next) {
		if (fn->fn_funct == fun)
			return (fn->fn_name);
	}
	return (NULL);
}

/*
 * List possible function name completions.
 */
struct list *
complete_function_list(const char *fname)
{
	struct funmap	*fn;
	struct list	*head, *el;
	int		 len;

	len = strlen(fname);
	head = NULL;
	for (fn = funs; fn != NULL; fn = fn->fn_next) {
		if (memcmp(fname, fn->fn_name, len) == 0) {
			if ((el = malloc(sizeof(*el))) == NULL) {
				free_file_list(head);
				return (NULL);
			}
			el->l_name = strdup(fn->fn_name);
			el->l_next = head;
			head = el;
		}
	}
	return (head);
}

/*
 * Find number of parameters for function name.
 */
int
numparams_function(PF fun)
{
	struct funmap *fn;

	for (fn = funs; fn != NULL; fn = fn->fn_next) {
		if (fn->fn_funct == fun)
			return (fn->fn_nparams);
	}
	return (FALSE);
}
