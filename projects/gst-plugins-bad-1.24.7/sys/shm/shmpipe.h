/* GStreamer
 * Copyright (C) <2009> Collabora Ltd
 *  @author: Olivier Crete <olivier.crete@collabora.co.uk
 * Copyright (C) <2009> Nokia Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * None of this code is thread safe, if you want to use it in a
 * multi-threaded context, please protect it with a mutex.
 *
 * First, create a writer with sp_writer_create(), then select() on
 * the socket returned by sp_get_fd(). If the socket is closed or any
 * function returns an error, the app should call sp_close() and
 * assume the other side is dead. The writer calls
 * sp_writer_accept_client() when there is something to read from the
 * main server fd. This returns a new ShmClient (representing a client
 * connection), the writer needs to do a select() on the socket
 * returned by sp_writer_get_client_fd(). If it gets an error on that
 * socket, it calls sp_writer_close_client(). If there is something to
 * read, it calls sp_writer_recv().
 *
 * The writer allocates a block containing a free buffer with
 * sp_writer_alloc_block(), then writes something in the buffer
 * (retrieved with sp_writer_block_get_buf(), then calls
 * sp_writer_send_buf() to send the buffer or a subsection to the
 * other side. When it is done with the block, it calls
 * sp_writer_free_block().  If alloc fails, then the server must wait
 * for events on the client fd (the ones where sp_writer_recv() is
 * called), and then try to re-alloc.
 *
 * The reader (client) connect to the writer with sp_client_open() And
 * select()s on the fd from sp_get_fd() until there is something to
 * read.  Then they must read using sp_client_recv() which will return
 * the size of the buffer (positive) if there is a valid buffer (which
 * is read only).  It will return 0 if it is an internal message and a
 * negative number if there was an error.  If there was an error, the
 * application must close the pipe with sp_close() and assume that all
 * buffers are no longer valid. If was valid buffer was received, the
 * client must release it with sp_client_recv_finish() when it is done
 * reading from it.
 */


#ifndef __SHMPIPE_H__
#define __SHMPIPE_H__

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ShmClient ShmClient;
typedef struct _ShmPipe ShmPipe;
typedef struct _ShmBlock ShmBlock;
typedef struct _ShmBuffer ShmBuffer;

typedef void (*sp_buffer_free_callback) (void * tag, void * user_data);

ShmPipe *sp_writer_create (const char *path, size_t size, mode_t perms);
const char *sp_writer_get_path (ShmPipe *pipe);
void sp_writer_close (ShmPipe * self, sp_buffer_free_callback callback,
    void * user_data);
void *sp_get_data (ShmPipe * self);
void sp_set_data (ShmPipe * self, void *data);

int sp_writer_setperms_shm (ShmPipe * self, mode_t perms);
int sp_writer_resize (ShmPipe * self, size_t size);

int sp_get_fd (ShmPipe * self);
const char *sp_get_shm_area_name (ShmPipe *self);
int sp_writer_get_client_fd (ShmClient * client);

ShmBlock *sp_writer_alloc_block (ShmPipe * self, size_t size);
void sp_writer_free_block (ShmBlock *block);
int sp_writer_send_buf (ShmPipe * self, char *buf, size_t size, void * tag);
char *sp_writer_block_get_buf (ShmBlock *block);
ShmPipe *sp_writer_block_get_pipe (ShmBlock *block);
size_t sp_writer_get_max_buf_size (ShmPipe * self);

ShmClient * sp_writer_accept_client (ShmPipe * self);
void sp_writer_close_client (ShmPipe *self, ShmClient * client,
    sp_buffer_free_callback callback, void * user_data);
int sp_writer_recv (ShmPipe * self, ShmClient * client, void ** tag);

int sp_writer_pending_writes (ShmPipe * self);

ShmBuffer *sp_writer_get_pending_buffers (ShmPipe * self);
ShmBuffer *sp_writer_get_next_buffer (ShmBuffer * buffer);
void *sp_writer_buf_get_tag (ShmBuffer * buffer);

ShmPipe *sp_client_open (const char *path);
long int sp_client_recv (ShmPipe * self, char **buf);
int sp_client_recv_finish (ShmPipe * self, char *buf);
void sp_client_close (ShmPipe * self);

#ifdef __cplusplus
}
#endif

#endif /* __SHMPIPE_H__ */
