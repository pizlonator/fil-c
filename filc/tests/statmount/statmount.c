/* Tests the pizlonated statmount(2) wrapper, reached via syscall(SYS_statmount, ...).

   The wrapper mirrors the kernel's request validation (EINVAL for request sizes below
   MNT_ID_REQ_SIZE_VER0 and E2BIG for sizes above PAGE_SIZE) before the kernel ever sees the
   request, and it checks that we can actually read the whole request and write the whole reply
   buffer.  Like util-linux's mountutils.h, we define the mount API constants and structs
   locally so that this test works with any libc and any kernel headers. */

#define _GNU_SOURCE

#include <errno.h>
#include <stdint.h>
#include <stdfil.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef SYS_statmount
#define SYS_statmount 457
#endif
#ifndef SYS_listmount
#define SYS_listmount 458
#endif

#ifndef MNT_ID_REQ_SIZE_VER0
#define MNT_ID_REQ_SIZE_VER0 24 /* sizeof first published struct */
#endif
#ifndef MNT_ID_REQ_SIZE_VER1
#define MNT_ID_REQ_SIZE_VER1 32 /* sizeof second published struct */
#endif

#ifndef LSMT_ROOT
#define LSMT_ROOT 0xffffffffffffffffULL /* root mount */
#endif

#ifndef STATMOUNT_MNT_POINT
#define STATMOUNT_MNT_POINT 0x00000010U
#endif

/* Local copy of struct mnt_id_req (see mountutils.h). */
struct mnt_id_req {
    uint32_t size;
    uint32_t spare;
    uint64_t mnt_id;
    uint64_t param;
    uint64_t mnt_ns_id;
};

/* Local copy of struct statmount (see mountutils.h). */
struct statmount {
    uint32_t size;            /* Total size, including strings */
    uint32_t mnt_opts;        /* [str] Mount options of the mount */
    uint64_t mask;            /* What results were written */
    uint32_t sb_dev_major;    /* Device ID */
    uint32_t sb_dev_minor;
    uint64_t sb_magic;        /* ..._SUPER_MAGIC */
    uint32_t sb_flags;        /* SB_{RDONLY,SYNCHRONOUS,DIRSYNC,LAZYTIME} */
    uint32_t fs_type;         /* [str] Filesystem type */
    uint64_t mnt_id;          /* Unique ID of mount */
    uint64_t mnt_parent_id;   /* Unique ID of parent (for root == mnt_id) */
    uint32_t mnt_id_old;      /* Reused IDs used in proc/.../mountinfo */
    uint32_t mnt_parent_id_old;
    uint64_t mnt_attr;        /* MOUNT_ATTR_... */
    uint64_t mnt_propagation; /* MS_{SHARED,SLAVE,PRIVATE,UNBINDABLE} */
    uint64_t mnt_peer_group;  /* ID of shared peer group */
    uint64_t mnt_master;      /* Mount receives propagation from this ID */
    uint64_t propagate_from;  /* Propagation from in current namespace */
    uint32_t mnt_root;        /* [str] Root of mount relative to root of fs */
    uint32_t mnt_point;       /* [str] Mountpoint relative to current root */
    uint64_t mnt_ns_id;       /* ID of the mount namespace */
    uint32_t fs_subtype;      /* [str] Subtype of fs_type (if any) */
    uint32_t sb_source;       /* [str] Source string of the mount */
    uint32_t opt_num;         /* Number of fs options */
    uint32_t opt_array;       /* [str] Array of nul terminated fs options */
    uint32_t opt_sec_num;     /* Number of security options */
    uint32_t opt_sec_array;   /* [str] Array of nul terminated security options */
    uint64_t supported_mask;  /* Mask flags that this kernel supports */
    uint32_t mnt_uidmap_num;  /* Number of uid mappings */
    uint32_t mnt_uidmap;      /* [str] Array of uid mappings (as seen from callers namespace) */
    uint32_t mnt_gidmap_num;  /* Number of gid mappings */
    uint32_t mnt_gidmap;      /* [str] Array of gid mappings (as seen from callers namespace) */
    uint64_t __spare2[43];
    char str[];               /* Variable size part containing strings */
};

int main(void)
{
    static char buf[4096];
    static char bigreq[8192];
    struct mnt_id_req req;
    struct mnt_id_req* big_req = (struct mnt_id_req*)bigreq;
    uint64_t list[16];
    long rc;
    int supported = 1;

    /* statmount(2) with a null mount id must fail without doing anything.  This is also how we
       detect kernels that don't implement statmount(2) at all. */
    memset(&req, 0, sizeof(req));
    req.size = MNT_ID_REQ_SIZE_VER1;
    errno = 0;
    rc = syscall(SYS_statmount, &req, NULL, 0, 0);
    int err = errno;
    zprintf("statmount id 0 rc %ld errno %d\n", rc, err);
    ZASSERT(rc == -1);
    if (err == ENOSYS) {
        zprintf("statmount not supported, skipping\n");
        supported = 0;
    } else if (err == EPERM) {
        zprintf("statmount not permitted, skipping\n");
        supported = 0;
    } else {
        ZASSERT(err == EINVAL || err == ENOENT);
    }

    if (supported) {
        /* Get a real mount id by listing the mounts in our namespace, starting at the root
           mount. */
        memset(&req, 0, sizeof(req));
        req.size = MNT_ID_REQ_SIZE_VER1;
        req.mnt_id = LSMT_ROOT;
        errno = 0;
        rc = syscall(SYS_listmount, &req, list, 16, 0);
        zprintf("listmount root rc %ld errno %d\n", rc, errno);
        ZASSERT(rc >= 1);
        ZASSERT(rc <= 16);

        /* Request the basic masks and the strings. */
        req.param = 0x33; /* SB_BASIC | MNT_BASIC | MNT_POINT | FS_TYPE */

        /* A tiny buffer must result in EOVERFLOW. */
        errno = 0;
        rc = syscall(SYS_statmount, &req, buf, 8, 0);
        err = errno;
        zprintf("statmount small buffer rc %ld errno %d\n", rc, err);
        ZASSERT(rc == -1);
        ZASSERT(err == EOVERFLOW || err == ENOENT); /* When running in the pizlix build container,
                                                       this gets ENOENT. */

        /* Now the real thing. */
        memset(buf, 0, sizeof(buf));
        errno = 0;
        rc = syscall(SYS_statmount, &req, buf, sizeof(buf), 0);
        zprintf("statmount rc %ld errno %d\n", rc, errno);
        if (!rc) {
            struct statmount* sm = (struct statmount*)buf;
            ZASSERT(sm->size);
            ZASSERT(sm->size <= sizeof(buf));
            ZASSERT(sm->mnt_id == list[0]);
            if (sm->mask & STATMOUNT_MNT_POINT) {
                /* The string at buf + mnt_point must be inside the data that the kernel says it
                   wrote, and it must be non-empty and NUL-terminated. */
                ZASSERT(sm->mnt_point);
                ZASSERT(sm->mnt_point < sm->size);
                const char* point = buf + sm->mnt_point;
                ZASSERT(point[0]);
                ZASSERT(strlen(point) + 1 <= sm->size - sm->mnt_point);
                zprintf("statmount mnt_point %s\n", point);
             }
        } else {
            /* When running in the pizlix build container, this gets ENOENT. */
            ZASSERT(rc == -1);
            ZASSERT(errno == ENOENT);
        }

        /* The VER0-sized request must work, too.  It just doesn't ask for any strings. */
        memset(buf, 0, sizeof(buf));
        req.size = MNT_ID_REQ_SIZE_VER0;
        errno = 0;
        rc = syscall(SYS_statmount, &req, buf, sizeof(buf), 0);
        zprintf("statmount ver0 rc %ld errno %d\n", rc, errno);
        ZASSERT(!rc || (rc == -1 && errno == ENOENT)); /* When running in the pizlix build container,
                                                          this gets ENOENT. */
    }

    /* The wrapper mirrors the kernel's request validation, so the next two fail without the
       kernel seeing anything, no matter how old the kernel is. */

    /* Request sizes below MNT_ID_REQ_SIZE_VER0 get EINVAL. */
    memset(&req, 0, sizeof(req));
    req.size = 23;
    req.mnt_id = 1;
    errno = 0;
    rc = syscall(SYS_statmount, &req, buf, sizeof(buf), 0);
    err = errno;
    zprintf("statmount size 23 rc %ld errno %d\n", rc, err);
    ZASSERT(rc == -1);
    ZASSERT(err == EINVAL);

    /* Request sizes above PAGE_SIZE get E2BIG. */
    memset(bigreq, 0, sizeof(bigreq));
    big_req->size = 500000;
    big_req->mnt_id = 1;
    errno = 0;
    rc = syscall(SYS_statmount, bigreq, buf, sizeof(buf), 0);
    err = errno;
    zprintf("statmount size 500000 rc %ld errno %d\n", rc, err);
    ZASSERT(rc == -1);
    ZASSERT(err == E2BIG);

    zprintf("statmount ok\n");
    return 0;
}
