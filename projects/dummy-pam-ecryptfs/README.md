# Stub pam_ecryptfs.so

A minimal PAM module that provides `pam_ecryptfs.so` compatibility without requiring Mozilla NSS or actual ecryptfs support.

## What is this?

This is a stub implementation of the ecryptfs PAM module that does nothing except allow PAM configurations to load successfully when they reference `pam_ecryptfs.so`.

## Why does this exist?

**The Problem:**
- Ubuntu's `/etc/pam.d/common-auth` includes `pam_ecryptfs.so` as a required module
- The real `pam_ecryptfs` from `ecryptfs-utils` depends on Mozilla NSS (Network Security Services)
- Mozilla NSS is a large cryptography library with its own dependency chain (NSPR, etc.)
- ecryptfs is **legacy technology** - replaced by LUKS and fscrypt in modern Linux systems
- Most modern users don't have ecryptfs-encrypted home directories

**The Solution:**
- Provide a stub `pam_ecryptfs.so` that returns `PAM_IGNORE` for all operations
- This allows the PAM stack to continue processing without errors
- Satisfies Ubuntu's PAM configuration requirements
- Avoids porting a large deprecated codebase

## What does it do?

This stub module implements all six PAM entry points:
- `pam_sm_authenticate` - Authentication phase
- `pam_sm_setcred` - Credential setting
- `pam_sm_acct_mgmt` - Account management
- `pam_sm_open_session` - Session setup
- `pam_sm_close_session` - Session teardown
- `pam_sm_chauthtok` - Password changes

**All entry points return `PAM_IGNORE`**, which means:
- "I have no opinion on this operation"
- "Continue processing other modules in the PAM stack"
- Does not cause authentication to fail
- Does not interfere with other PAM modules

## Who should use this?

**Safe for:**
- Modern Linux systems where users don't use ecryptfs
- Systems migrated from ecryptfs to LUKS or plain home directories
- Compatibility testing with Ubuntu/Debian PAM configurations
- Building self-contained PAM distributions (like Fil-C's /opt/fil)

**Not safe for:**
- Systems with actual ecryptfs-encrypted home directories
- Users who need their encrypted home to auto-mount on login

If you have real ecryptfs users, you need the real `pam_ecryptfs` from `ecryptfs-utils`.

## How to build

### With system compiler:
```bash
make
sudo make install PREFIX=/usr/local
```

### With Fil-C compiler:
```bash
make CC=/opt/fil/bin/filcc
sudo make install PREFIX=/opt/fil
```

### For packaging:
```bash
make CC=/opt/fil/bin/filcc
make install PREFIX=/opt/fil DESTDIR=/tmp/staging
```

## Installation

The module installs to `$(PREFIX)/lib/security/pam_ecryptfs.so`, which is where PAM looks for modules by default.

After installation, any PAM configuration that references `pam_ecryptfs.so` will load successfully, even though the module does nothing.

## Testing

To verify the module loads correctly:

```bash
# Create a test PAM config
cat > /tmp/pam-ecryptfs-test <<EOF
auth    optional    pam_ecryptfs.so
session optional    pam_ecryptfs.so
EOF

# Check system logs for PAM errors
# If the module loads, you won't see "unable to dlopen" errors
```

## Limitations

This is a **stub implementation**. It:
- Does **not** mount ecryptfs filesystems
- Does **not** manage encryption keys
- Does **not** detect if users have ecryptfs home directories
- Simply allows PAM configurations to load without errors

Users with actual ecryptfs-encrypted home directories will find their encrypted homes are not mounted automatically.

## License

This code is in the public domain. It's too simple to be copyrightable.

## See Also

- [eCryptfs on Ubuntu Wiki](https://help.ubuntu.com/community/eCryptfs)
- [Linux-PAM Documentation](http://www.linux-pam.org/)
- [Fil-C Project](https://github.com/pizlonator/llvm-project-deluge)

## History

Created as part of the Fil-C project to enable Ubuntu PAM compatibility without requiring Mozilla NSS to be ported to Fil-C. The real `pam_ecryptfs` is from the `ecryptfs-utils` package, which has been deprecated in favor of LUKS and fscrypt.
