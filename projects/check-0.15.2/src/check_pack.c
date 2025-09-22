/*
 * Check: a unit test framework for C
 * Copyright (C) 2001, 2002 Arien Malec
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "../lib/libcompat.h"
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <limits.h>
#include "check_stdint.h"        /* Need SIZE_MAX */

#include "check.h"
#include "check_error.h"
#include "check_list.h"
#include "check_impl.h"
#include "check_pack.h"

#ifndef HAVE_PTHREAD
#define pthread_cleanup_push(f, a) {
#define pthread_cleanup_pop(e) }
#endif

#ifndef PTHREAD_MUTEX_INITIALIZER
#define pthread_mutex_lock(mutex)
#define pthread_mutex_unlock(mutex)
#endif

/* Maximum size for one message in the message stream. */
static size_t ck_max_msg_size = 0;
#ifndef DEFAULT_MAX_MSG_SIZE
#define DEFAULT_MAX_MSG_SIZE 4096
#endif
/* This is used to implement a sliding window on the receiving
 * side. When sending messages, we assure that no single message
 * is bigger than this.
 * The usual size for a message is less than 80 bytes.
 * All this is done instead of the previous approach to allocate (actually
 * continuously reallocate) one big chunk for the whole message stream.
 * Problems were seen in the wild with up to 4 GB reallocations.
 */

void check_set_max_msg_size(size_t max_msg_size)
{
    ck_max_msg_size = max_msg_size;
}

static size_t get_max_msg_size(void)
{
    size_t value = 0;
    char *env = getenv("CK_MAX_MSG_SIZE");
    if (env)
        value = (size_t)strtoul(env, NULL, 10); /* Cast in case size_t != unsigned long. */
    if (value == 0)
        value = ck_max_msg_size;
    if (value == 0)
        value = DEFAULT_MAX_MSG_SIZE;
    return value;
}

/* typedef an unsigned int that has exactly 4 bytes, as required by pack_int() */
typedef uint32_t ck_uint32;
#define CK_UINT32_MAX UINT32_MAX


static void pack_int(char **buf, ck_uint32 val);
static ck_uint32 upack_int(char **buf);
static void pack_str(char **buf, const char *str);
static char *upack_str(char **buf);

static size_t pack_ctx(char **buf, CtxMsg * cmsg);
static size_t pack_loc(char **buf, LocMsg * lmsg);
static size_t pack_fail(char **buf, FailMsg * fmsg);
static size_t pack_duration(char **buf, DurationMsg * fmsg);
static void upack_ctx(char **buf, CtxMsg * cmsg);
static void upack_loc(char **buf, LocMsg * lmsg);
static void upack_fail(char **buf, FailMsg * fmsg);
static void upack_duration(char **buf, DurationMsg * fmsg);

static void check_type(int type, const char *file, int line);
static enum ck_msg_type upack_type(char **buf);
static void pack_type(char **buf, enum ck_msg_type type);

static size_t read_buf(FILE * fdes, size_t size, char *buf);
static size_t get_result(char *buf, RcvMsg * rmsg);
static void rcvmsg_update_ctx(RcvMsg * rmsg, enum ck_result_ctx ctx);
static void rcvmsg_update_loc(RcvMsg * rmsg, const char *file, int line);
static RcvMsg *rcvmsg_create(void);
void rcvmsg_free(RcvMsg * rmsg);

typedef size_t (*pfun) (char **, CheckMsg *);
typedef void (*upfun) (char **, CheckMsg *);

static pfun pftab[] = {
    (pfun) pack_ctx,
    (pfun) pack_fail,
    (pfun) pack_loc,
    (pfun) pack_duration
};

static upfun upftab[] = {
    (upfun) upack_ctx,
    (upfun) upack_fail,
    (upfun) upack_loc,
    (upfun) upack_duration
};

int pack(enum ck_msg_type type, char **buf, CheckMsg * msg)
{
    size_t len;

    if(buf == NULL)
        return -1;
    if(msg == NULL)
        return 0;

    check_type(type, __FILE__, __LINE__);

    len = pftab[type] (buf, msg);
    if(len > (size_t) INT_MAX)
        eprintf("Value of len (%zu) too big, max allowed %u\n",
                __FILE__, __LINE__ - 3, len, INT_MAX);
    return (int) len;
}

int upack(char *buf, CheckMsg * msg, enum ck_msg_type *type)
{
    char *obuf;
    ptrdiff_t diff;

    if(buf == NULL)
        return -1;

    obuf = buf;

    *type = upack_type(&buf);

    check_type(*type, __FILE__, __LINE__);

    upftab[*type] (&buf, msg);

    diff = buf - obuf;
    if(diff > (ptrdiff_t) INT_MAX)
        eprintf("Value of diff (%td) too big, max allowed %d\n",
                __FILE__, __LINE__ - 3, diff, INT_MAX);
    if(diff > (ptrdiff_t) INT_MAX || diff < (ptrdiff_t) INT_MIN)
        eprintf("Value of diff (%td) too small, min allowed %d\n",
                __FILE__, __LINE__ - 6, diff, INT_MIN);
    return (int) diff;
}

static void pack_int(char **buf, ck_uint32 val)
{
    unsigned char *ubuf = (unsigned char *)*buf;
    ck_uint32 uval = val;

    ubuf[0] = (unsigned char)((uval >> 24) & 0xFF);
    ubuf[1] = (unsigned char)((uval >> 16) & 0xFF);
    ubuf[2] = (unsigned char)((uval >> 8) & 0xFF);
    ubuf[3] = (unsigned char)(uval & 0xFF);

    *buf += 4;
}

static ck_uint32 upack_int(char **buf)
{
    unsigned char *ubuf = (unsigned char *)*buf;
    ck_uint32 uval;

    uval =
        (ck_uint32) ((ubuf[0] << 24) | (ubuf[1] << 16) | (ubuf[2] << 8) |
                     ubuf[3]);

    *buf += 4;

    return uval;
}

static void pack_str(char **buf, const char *val)
{
    size_t strsz;

    if(val == NULL)
        strsz = 0;
    else
        strsz = strlen(val);
    if(strsz > CK_UINT32_MAX)
        eprintf("Value of strsz (%zu) too big, max allowed %u\n",
                __FILE__, __LINE__, strsz, CK_UINT32_MAX);

    pack_int(buf, (ck_uint32) strsz);

    if(strsz > 0)
    {
        memcpy(*buf, val, strsz);
        *buf += strsz;
    }
}

static char *upack_str(char **buf)
{
    char *val;
    size_t strsz;

    strsz = (size_t) upack_int(buf);

    if(strsz > 0)
    {
        val = (char *)emalloc(strsz + 1);
        memcpy(val, *buf, strsz);
        val[strsz] = 0;
        *buf += strsz;
    }
    else
    {
        val = (char *)emalloc(1);
        *val = 0;
    }

    return val;
}

static void pack_type(char **buf, enum ck_msg_type type)
{
    pack_int(buf, (ck_uint32) type);
}

static enum ck_msg_type upack_type(char **buf)
{
CK_DIAGNOSTIC_PUSH_IGNORE(-Wbad-function-cast)
    return (enum ck_msg_type)upack_int(buf);
CK_DIAGNOSTIC_POP()
}


static size_t pack_ctx(char **buf, CtxMsg * cmsg)
{
    char *ptr;
    size_t len;

    len = 4 + 4;
    *buf = ptr = (char *)emalloc(len);

    pack_type(&ptr, CK_MSG_CTX);
    pack_int(&ptr, (ck_uint32) cmsg->ctx);

    return len;
}

static void upack_ctx(char **buf, CtxMsg * cmsg)
{
CK_DIAGNOSTIC_PUSH_IGNORE(-Wbad-function-cast)
    cmsg->ctx = (enum ck_result_ctx)upack_int(buf);
CK_DIAGNOSTIC_POP()
}

static size_t pack_duration(char **buf, DurationMsg * cmsg)
{
    char *ptr;
    size_t len;

    len = 4 + 4;
    *buf = ptr = (char *)emalloc(len);

    pack_type(&ptr, CK_MSG_DURATION);
    if((size_t) cmsg->duration > (size_t) CK_UINT32_MAX)
        eprintf("Value of cmsg->duration (%d) too big to pack, max allowed %u\n",
                __FILE__, __LINE__, cmsg->duration, CK_UINT32_MAX);
    pack_int(&ptr, (ck_uint32) cmsg->duration);

    return len;
}

static void upack_duration(char **buf, DurationMsg * cmsg)
{
    ck_uint32 duration = upack_int(buf);
    if(duration > INT_MAX)
        eprintf("Unpacked value (%u) too big for cmsg->duration, max allowed %d\n",
                __FILE__, __LINE__ - 3, duration, INT_MAX);
    cmsg->duration = (int) duration;
}

static size_t pack_loc(char **buf, LocMsg * lmsg)
{
    char *ptr;
    size_t len;

    len = 4 + 4 + (lmsg->file ? strlen(lmsg->file) : 0) + 4;
    *buf = ptr = (char *)emalloc(len);

    pack_type(&ptr, CK_MSG_LOC);
    pack_str(&ptr, lmsg->file);
    if((size_t) lmsg->line > (size_t) CK_UINT32_MAX)
        eprintf("Value of lmsg->line (%d) too big, max allowed %u\n",
                __FILE__, __LINE__, lmsg->line, CK_UINT32_MAX);
    pack_int(&ptr, (ck_uint32) lmsg->line);

    return len;
}

static void upack_loc(char **buf, LocMsg * lmsg)
{
    ck_uint32 line;
    lmsg->file = upack_str(buf);
    line = upack_int(buf);
    if(line > INT_MAX)
        eprintf("Unpacked value (%u) too big for lmsg->line, max allowed %d\n",
                __FILE__, __LINE__ - 3, line, INT_MAX);
    lmsg->line = (int) line;
}

static size_t pack_fail(char **buf, FailMsg * fmsg)
{
    char *ptr;
    size_t len;

    len = 4 + 4 + (fmsg->msg ? strlen(fmsg->msg) : 0);
    *buf = ptr = (char *)emalloc(len);

    pack_type(&ptr, CK_MSG_FAIL);
    pack_str(&ptr, fmsg->msg);

    return len;
}

static void upack_fail(char **buf, FailMsg * fmsg)
{
    fmsg->msg = upack_str(buf);
}

static void check_type(int type, const char *file, int line)
{
    if(type < 0 || type >= CK_MSG_LAST)
        eprintf("Bad message type arg %d", file, line, type);
}

#ifdef PTHREAD_MUTEX_INITIALIZER
static pthread_mutex_t ck_mutex_lock = PTHREAD_MUTEX_INITIALIZER;

static void ppack_cleanup(void *mutex)
{
    pthread_mutex_unlock((pthread_mutex_t *)mutex);
}
#else
#define ppack_cleanup(mutex)
#endif

void ppack(FILE * fdes, enum ck_msg_type type, CheckMsg * msg)
{
    char *buf = NULL;
    int n;
    size_t r;

    n = pack(type, &buf, msg);
    if(n < 0)
        eprintf("pack failed", __FILE__, __LINE__ - 2);
    /* Keep it on the safe side to not send too much data. */
    if((size_t) n > get_max_msg_size())
        eprintf("Message string too long", __FILE__, __LINE__ - 5);

    pthread_cleanup_push(ppack_cleanup, &ck_mutex_lock);
    pthread_mutex_lock(&ck_mutex_lock);
    r = fwrite(buf, 1, (size_t) n, fdes);
    fflush(fdes);
    pthread_mutex_unlock(&ck_mutex_lock);
    pthread_cleanup_pop(0);
    if(r != (size_t) n)
        eprintf("Error in call to fwrite:", __FILE__, __LINE__ - 5);

    free(buf);
}

static size_t read_buf(FILE * fdes, size_t size, char *buf)
{
    size_t n;

    n = fread(buf, 1, size, fdes);

    if(ferror(fdes))
    {
        eprintf("Error in call to fread:", __FILE__, __LINE__ - 4);
    }

    return n;
}

static size_t get_result(char *buf, RcvMsg * rmsg)
{
    enum ck_msg_type type;
    CheckMsg msg;
    ptrdiff_t n;
    size_t msgsz;

    n = upack(buf, &msg, &type);
    if(n < 0)
        eprintf("Error in call to upack", __FILE__, __LINE__ - 2);
    msgsz = (size_t) n;

    if(type == CK_MSG_CTX)
    {
        CtxMsg *cmsg = (CtxMsg *) & msg;

        rcvmsg_update_ctx(rmsg, cmsg->ctx);
    }
    else if(type == CK_MSG_LOC)
    {
        LocMsg *lmsg = (LocMsg *) & msg;

        if(rmsg->failctx == CK_CTX_INVALID)
        {
            rcvmsg_update_loc(rmsg, lmsg->file, lmsg->line);
        }
        free(lmsg->file);
    }
    else if(type == CK_MSG_FAIL)
    {
        FailMsg *fmsg = (FailMsg *) & msg;

        if(rmsg->msg == NULL)
        {
            rmsg->msg = strdup(fmsg->msg);
            rmsg->failctx = rmsg->lastctx;
        }
        else
        {
            /* Skip subsequent failure messages, only happens for CK_NOFORK */
        }
        free(fmsg->msg);
    }
    else if(type == CK_MSG_DURATION)
    {
        DurationMsg *cmsg = (DurationMsg *) & msg;

        rmsg->duration = cmsg->duration;
    }
    else
        check_type(type, __FILE__, __LINE__);

    return msgsz;
}

static void reset_rcv_test(RcvMsg * rmsg)
{
    rmsg->test_line = -1;
    rmsg->test_file = NULL;
}

static void reset_rcv_fixture(RcvMsg * rmsg)
{
    rmsg->fixture_line = -1;
    rmsg->fixture_file = NULL;
}

static RcvMsg *rcvmsg_create(void)
{
    RcvMsg *rmsg;

    rmsg = (RcvMsg *)emalloc(sizeof(RcvMsg));
    rmsg->lastctx = CK_CTX_INVALID;
    rmsg->failctx = CK_CTX_INVALID;
    rmsg->msg = NULL;
    rmsg->duration = -1;
    reset_rcv_test(rmsg);
    reset_rcv_fixture(rmsg);
    return rmsg;
}

void rcvmsg_free(RcvMsg * rmsg)
{
    free(rmsg->fixture_file);
    free(rmsg->test_file);
    free(rmsg->msg);
    free(rmsg);
}

static void rcvmsg_update_ctx(RcvMsg * rmsg, enum ck_result_ctx ctx)
{
    if(rmsg->lastctx != CK_CTX_INVALID)
    {
        free(rmsg->fixture_file);
        reset_rcv_fixture(rmsg);
    }
    rmsg->lastctx = ctx;
}

static void rcvmsg_update_loc(RcvMsg * rmsg, const char *file, int line)
{
    if(rmsg->lastctx == CK_CTX_TEST)
    {
        free(rmsg->test_file);
        rmsg->test_line = line;
        rmsg->test_file = strdup(file);
    }
    else
    {
        free(rmsg->fixture_file);
        rmsg->fixture_line = line;
        rmsg->fixture_file = strdup(file);
    }
}

RcvMsg *punpack(FILE * fdes)
{
    size_t nread, nparse;
    char *buf;
    RcvMsg *rmsg;

    rmsg = rcvmsg_create();

    /* Allcate a buffer */
    buf = (char *)emalloc(get_max_msg_size() * 2);
    /* Fill the buffer from the file */
    nread = read_buf(fdes, get_max_msg_size() * 2, buf);
    nparse = nread;
    /* While not all parsed */
    while(nparse > 0)
    {
        /* Parse one message */
        size_t n = get_result(buf, rmsg);
        if ( ((ssize_t) nparse - (ssize_t) n) < 0)    /* casting necessary to be able to get proper negative integers */
            eprintf("Error in call to get_result", __FILE__, __LINE__ - 5);
        nparse -= n;
        /* Move remaining data in buffer to the beginning */
        memmove(buf, buf + n, nparse);
        /* If EOF has not been seen */
        if(nread > 0)
        {
            /* Read more data into empty space at end of the buffer */
            nread = read_buf(fdes, (size_t) n, buf + nparse);
            nparse += nread;
        }
    }
    free(buf);

    if(rmsg->lastctx == CK_CTX_INVALID)
    {
        free(rmsg);
        rmsg = NULL;
    }

    return rmsg;
}
