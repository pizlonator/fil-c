#ifndef PAM_UNIX_AUDIT_H
#define PAM_UNIX_AUDIT_H

int
audit_log(int type, const char *uname, int rc);

#endif /* PAM_UNIX_AUDIT_H */
