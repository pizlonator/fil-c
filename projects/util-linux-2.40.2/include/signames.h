/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef SIGNAMES_H
#define SIGNAMES_H

int signame_to_signum(const char *sig);
const char *signum_to_signame(int signum);
int get_signame_by_idx(size_t idx, const char **signame, int *signum);

#endif /* SIGNAMES_H */
