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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_OSX
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL SO_NOSIGPIPE
#endif
#endif

#include "shmpipe.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <assert.h>

#include "shmalloc.h"

/*
 * The protocol over the pipe is in packets
 *
 * The defined types are:
 * type 1: new shm area
 * Area length
 * Size of path (followed by path)
 *
 * type 2: Close shm area:
 * No payload
 *
 * type 3: shm buffer
 * offset
 * bufsize
 *
 * type 4: ack buffer
 * offset
 *
 * Type 4 goes from the client to the server
 * The rest are from the server to the client
 * The client should never write in the SHM
 */


#define LISTEN_BACKLOG 10

enum
{
  COMMAND_NEW_SHM_AREA = 1,
  COMMAND_CLOSE_SHM_AREA = 2,
  COMMAND_NEW_BUFFER = 3,
  COMMAND_ACK_BUFFER = 4
};

typedef struct _ShmArea ShmArea;

struct _ShmArea
{
  int id;

  int use_count;
  int is_writer;

  int shm_fd;

  char *shm_area_buf;
  size_t shm_area_len;

  char *shm_area_name;

  ShmAllocSpace *allocspace;

  ShmArea *next;
};

struct _ShmBuffer
{
  int use_count;

  ShmArea *shm_area;
  unsigned long offset;
  size_t size;

  ShmAllocBlock *ablock;

  ShmBuffer *next;

  void *tag;

  int num_clients;
  /* This must ALWAYS stay last in the struct */
  int clients[0];
};


struct _ShmPipe
{
  int main_socket;
  char *socket_path;
  int use_count;
  void *data;

  ShmArea *shm_area;

  int next_area_id;

  ShmBuffer *buffers;

  int num_clients;
  ShmClient *clients;

  mode_t perms;
};

struct _ShmClient
{
  int fd;

  ShmClient *next;
};

struct _ShmBlock
{
  ShmPipe *pipe;
  ShmArea *area;
  ShmAllocBlock *ablock;
};

struct CommandBuffer
{
  unsigned int type;
  int area_id;

  union
  {
    struct
    {
      size_t size;
      unsigned int path_size;
      /* Followed by path */
    } new_shm_area;
    struct
    {
      unsigned long offset;
      unsigned long size;
    } buffer;
    struct
    {
      unsigned long offset;
    } ack_buffer;
  } payload;
};

static ShmArea *sp_open_shm (char *path, int id, mode_t perms, size_t size);
static void sp_close_shm (ShmArea * area);
static int sp_shmbuf_dec (ShmPipe * self, ShmBuffer * buf,
    ShmBuffer * prev_buf, ShmClient * client, void **tag);
static void sp_shm_area_dec (ShmPipe * self, ShmArea * area);



#define RETURN_ERROR(format, ...) do {                  \
  fprintf (stderr, format, __VA_ARGS__);                \
  sp_writer_close (self, NULL, NULL);                   \
  return NULL;                                          \
  } while (0)

ShmPipe *
sp_writer_create (const char *path, size_t size, mode_t perms)
{
  ShmPipe *self = spalloc_new (ShmPipe);
  int flags;
  struct sockaddr_un sock_un;
  int i = 0;

  memset (self, 0, sizeof (ShmPipe));

  self->main_socket = socket (PF_UNIX, SOCK_STREAM, 0);
  self->use_count = 1;

  if (self->main_socket < 0)
    RETURN_ERROR ("Could not create socket (%d): %s\n", errno,
        strerror (errno));

  flags = fcntl (self->main_socket, F_GETFL, 0);
  if (flags < 0)
    RETURN_ERROR ("fcntl(F_GETFL) failed (%d): %s\n", errno, strerror (errno));

  if (fcntl (self->main_socket, F_SETFL, flags | O_NONBLOCK | FD_CLOEXEC) < 0)
    RETURN_ERROR ("fcntl(F_SETFL) failed (%d): %s\n", errno, strerror (errno));

  sock_un.sun_family = AF_UNIX;
  strncpy (sock_un.sun_path, path, sizeof (sock_un.sun_path) - 1);

  while (bind (self->main_socket, (struct sockaddr *) &sock_un,
          sizeof (struct sockaddr_un)) < 0) {
    if (errno != EADDRINUSE)
      RETURN_ERROR ("bind() failed (%d): %s\n", errno, strerror (errno));

    if (i > 256)
      RETURN_ERROR ("Could not find a free socket name for %s", path);

    snprintf (sock_un.sun_path, sizeof (sock_un.sun_path), "%s.%d", path, i);
    i++;
  }

  self->socket_path = strdup (sock_un.sun_path);

  if (chmod (self->socket_path, perms) < 0)
    RETURN_ERROR ("failed to set socket permissions (%d): %s\n", errno,
        strerror (errno));

  if (listen (self->main_socket, LISTEN_BACKLOG) < 0)
    RETURN_ERROR ("listen() failed (%d): %s\n", errno, strerror (errno));

  self->shm_area = sp_open_shm (NULL, ++self->next_area_id, perms, size);

  self->perms = perms;

  if (!self->shm_area)
    RETURN_ERROR ("Could not open shm area (%d): %s", errno, strerror (errno));

  return self;
}

#undef RETURN_ERROR

#define RETURN_ERROR(format, ...)  do {                   \
  fprintf (stderr, format, __VA_ARGS__);                  \
  area->use_count--;                                      \
  sp_close_shm (area);                                    \
  return NULL;                                            \
  } while (0)

/* sp_open_shm:
 * @path: Path of the shm area for a reader,
 *  NULL if this is a writer (then it will allocate its own path)
 *
 * Opens a ShmArea
 */

static ShmArea *
sp_open_shm (char *path, int id, mode_t perms, size_t size)
{
  ShmArea *area = spalloc_new (ShmArea);
  char tmppath[32];
  int flags;
  int prot;
  int i = 0;

  memset (area, 0, sizeof (ShmArea));

  area->shm_area_buf = MAP_FAILED;
  area->use_count = 1;

  area->shm_area_len = size;

  area->is_writer = (path == NULL);


  if (path)
    flags = O_RDONLY;
  else
#ifdef HAVE_OSX
    flags = O_RDWR | O_CREAT | O_EXCL;
#else
    flags = O_RDWR | O_CREAT | O_TRUNC | O_EXCL;
#endif

  area->shm_fd = -1;

  if (path) {
    area->shm_fd = shm_open (path, flags, perms);
  } else {
    do {
      snprintf (tmppath, sizeof (tmppath), "/shmpipe.%5d.%5d", getpid (), i++);
      area->shm_fd = shm_open (tmppath, flags, perms);
    } while (area->shm_fd < 0 && errno == EEXIST);
  }

  if (area->shm_fd < 0)
    RETURN_ERROR ("shm_open failed on %s (%d): %s\n",
        path ? path : tmppath, errno, strerror (errno));

  if (!path) {
    area->shm_area_name = strdup (tmppath);

    if (ftruncate (area->shm_fd, size))
      RETURN_ERROR ("Could not resize memory area to header size,"
          " ftruncate failed (%d): %s\n", errno, strerror (errno));

    prot = PROT_READ | PROT_WRITE;
  } else {
    area->shm_area_name = strdup (path);
    prot = PROT_READ;
  }

  area->shm_area_buf = mmap (NULL, size, prot, MAP_SHARED, area->shm_fd, 0);

  if (area->shm_area_buf == MAP_FAILED)
    RETURN_ERROR ("mmap failed (%d): %s\n", errno, strerror (errno));

  area->id = id;

  if (!path)
    area->allocspace = shm_alloc_space_new (area->shm_area_len);

  return area;
}

#undef RETURN_ERROR

static void
sp_close_shm (ShmArea * area)
{
  assert (area->use_count == 0);

  if (area->allocspace)
    shm_alloc_space_free (area->allocspace);

  if (area->shm_area_buf != MAP_FAILED)
    munmap (area->shm_area_buf, area->shm_area_len);

  if (area->shm_fd >= 0)
    close (area->shm_fd);

  if (area->shm_area_name) {
    if (area->is_writer)
      shm_unlink (area->shm_area_name);
    free (area->shm_area_name);
  }

  spalloc_free (ShmArea, area);
}

static void
sp_shm_area_inc (ShmArea * area)
{
  area->use_count++;
}

static void
sp_shm_area_dec (ShmPipe * self, ShmArea * area)
{
  assert (area->use_count > 0);
  area->use_count--;

  if (area->use_count == 0) {
    ShmArea *item = NULL;
    ShmArea *prev_item = NULL;

    for (item = self->shm_area; item; item = item->next) {
      if (item == area) {
        if (prev_item)
          prev_item->next = item->next;
        else
          self->shm_area = item->next;
        break;
      }
      prev_item = item;
    }
    assert (item);

    sp_close_shm (area);
  }
}

void *
sp_get_data (ShmPipe * self)
{
  return self->data;
}

void
sp_set_data (ShmPipe * self, void *data)
{
  self->data = data;
}

static void
sp_inc (ShmPipe * self)
{
  self->use_count++;
}

static void
sp_dec (ShmPipe * self)
{
  self->use_count--;

  if (self->use_count > 0)
    return;

  while (self->shm_area)
    sp_shm_area_dec (self, self->shm_area);

  spalloc_free (ShmPipe, self);
}

void
sp_writer_close (ShmPipe * self, sp_buffer_free_callback callback,
    void *user_data)
{
  if (self->main_socket >= 0) {
    shutdown (self->main_socket, SHUT_RDWR);
    close (self->main_socket);
  }

  if (self->socket_path) {
    unlink (self->socket_path);
    free (self->socket_path);
  }

  while (self->clients)
    sp_writer_close_client (self, self->clients, callback, user_data);

  sp_dec (self);
}

void
sp_client_close (ShmPipe * self)
{
  sp_writer_close (self, NULL, NULL);
}


int
sp_writer_setperms_shm (ShmPipe * self, mode_t perms)
{
  int ret = 0;
  ShmArea *area;

  self->perms = perms;
  for (area = self->shm_area; area; area = area->next)
    ret |= fchmod (area->shm_fd, perms);

  ret |= chmod (self->socket_path, perms);

  return ret;
}

static int
send_command (int fd, struct CommandBuffer *cb, unsigned short int type,
    int area_id)
{
  cb->type = type;
  cb->area_id = area_id;

  if (send (fd, cb, sizeof (struct CommandBuffer), MSG_NOSIGNAL) !=
      sizeof (struct CommandBuffer))
    return 0;

  return 1;
}

int
sp_writer_resize (ShmPipe * self, size_t size)
{
  ShmArea *newarea;
  ShmArea *old_current;
  ShmClient *client;
  int c = 0;
  int pathlen;

  if (self->shm_area->shm_area_len == size)
    return 0;

  newarea = sp_open_shm (NULL, ++self->next_area_id, self->perms, size);

  if (!newarea)
    return -1;

  old_current = self->shm_area;
  newarea->next = self->shm_area;
  self->shm_area = newarea;

  pathlen = strlen (newarea->shm_area_name) + 1;

  for (client = self->clients; client; client = client->next) {
    struct CommandBuffer cb = { 0 };

    if (!send_command (client->fd, &cb, COMMAND_CLOSE_SHM_AREA,
            old_current->id))
      continue;

    cb.payload.new_shm_area.size = newarea->shm_area_len;
    cb.payload.new_shm_area.path_size = pathlen;
    if (!send_command (client->fd, &cb, COMMAND_NEW_SHM_AREA, newarea->id))
      continue;

    if (send (client->fd, newarea->shm_area_name, pathlen, MSG_NOSIGNAL) !=
        pathlen)
      continue;
    c++;
  }

  sp_shm_area_dec (self, old_current);


  return c;
}

ShmBlock *
sp_writer_alloc_block (ShmPipe * self, size_t size)
{
  ShmBlock *block;
  ShmAllocBlock *ablock =
      shm_alloc_space_alloc_block (self->shm_area->allocspace, size);

  if (!ablock)
    return NULL;

  block = spalloc_new (ShmBlock);
  sp_shm_area_inc (self->shm_area);
  block->pipe = self;
  block->area = self->shm_area;
  block->ablock = ablock;
  sp_inc (self);
  return block;
}

char *
sp_writer_block_get_buf (ShmBlock * block)
{
  return block->area->shm_area_buf +
      shm_alloc_space_alloc_block_get_offset (block->ablock);
}

ShmPipe *
sp_writer_block_get_pipe (ShmBlock * block)
{
  return block->pipe;
}

void
sp_writer_free_block (ShmBlock * block)
{
  shm_alloc_space_block_dec (block->ablock);
  sp_shm_area_dec (block->pipe, block->area);
  sp_dec (block->pipe);
  spalloc_free (ShmBlock, block);
}

/* Returns the number of client this has successfully been sent to */

int
sp_writer_send_buf (ShmPipe * self, char *buf, size_t size, void *tag)
{
  ShmArea *area = NULL;
  unsigned long offset = 0;
  unsigned long bsize = size;
  ShmBuffer *sb;
  ShmClient *client = NULL;
  ShmAllocBlock *ablock = NULL;
  int i = 0;
  int c = 0;

  if (self->num_clients == 0)
    return 0;

  for (area = self->shm_area; area; area = area->next) {
    if (buf >= area->shm_area_buf &&
        buf < (area->shm_area_buf + area->shm_area_len)) {
      offset = buf - area->shm_area_buf;
      ablock = shm_alloc_space_block_get (area->allocspace, offset);
      assert (ablock);
      break;
    }
  }

  if (!ablock)
    return -1;

  sb = spalloc_alloc (sizeof (ShmBuffer) + sizeof (int) * self->num_clients);
  memset (sb, 0, sizeof (ShmBuffer));
  memset (sb->clients, -1, sizeof (int) * self->num_clients);
  sb->shm_area = area;
  sb->offset = offset;
  sb->size = size;
  sb->num_clients = self->num_clients;
  sb->ablock = ablock;
  sb->tag = tag;

  for (client = self->clients; client; client = client->next) {
    struct CommandBuffer cb = { 0 };
    cb.payload.buffer.offset = offset;
    cb.payload.buffer.size = bsize;
    if (!send_command (client->fd, &cb, COMMAND_NEW_BUFFER, self->shm_area->id))
      continue;
    sb->clients[i++] = client->fd;
    c++;
  }

  if (c == 0) {
    spalloc_free1 (sizeof (ShmBuffer) + sizeof (int) * sb->num_clients, sb);
    return 0;
  }

  sp_shm_area_inc (area);
  shm_alloc_space_block_inc (ablock);

  sb->use_count = c;

  sb->next = self->buffers;
  self->buffers = sb;

  return c;
}

static int
recv_command (int fd, struct CommandBuffer *cb)
{
  int retval;

  retval = recv (fd, cb, sizeof (struct CommandBuffer), MSG_DONTWAIT);
  if (retval == sizeof (struct CommandBuffer)) {
    return 1;
  } else {
    return 0;
  }
}

long int
sp_client_recv (ShmPipe * self, char **buf)
{
  char *area_name = NULL;
  ShmArea *newarea;
  ShmArea *area;
  struct CommandBuffer cb;
  int retval;

  if (!recv_command (self->main_socket, &cb))
    return -1;

  switch (cb.type) {
    case COMMAND_NEW_SHM_AREA:
      assert (cb.payload.new_shm_area.path_size > 0);
      assert (cb.payload.new_shm_area.size > 0);

      area_name = malloc (cb.payload.new_shm_area.path_size + 1);
      retval = recv (self->main_socket, area_name,
          cb.payload.new_shm_area.path_size, 0);
      if (retval != cb.payload.new_shm_area.path_size) {
        free (area_name);
        return -3;
      }
      /* Ensure area_name is NULL terminated */
      area_name[retval] = 0;

      newarea = sp_open_shm (area_name, cb.area_id, 0,
          cb.payload.new_shm_area.size);
      free (area_name);
      if (!newarea)
        return -4;

      newarea->next = self->shm_area;
      self->shm_area = newarea;
      break;

    case COMMAND_CLOSE_SHM_AREA:
      for (area = self->shm_area; area; area = area->next) {
        if (area->id == cb.area_id) {
          sp_shm_area_dec (self, area);
          break;
        }
      }
      break;

    case COMMAND_NEW_BUFFER:
      assert (buf);
      for (area = self->shm_area; area; area = area->next) {
        if (area->id == cb.area_id) {
          *buf = area->shm_area_buf + cb.payload.buffer.offset;
          sp_shm_area_inc (area);
          return cb.payload.buffer.size;
        }
      }
      return -23;

    default:
      return -99;
  }

  return 0;
}

int
sp_writer_recv (ShmPipe * self, ShmClient * client, void **tag)
{
  ShmBuffer *buf = NULL, *prev_buf = NULL;
  struct CommandBuffer cb;

  if (!recv_command (client->fd, &cb))
    return -1;

  switch (cb.type) {
    case COMMAND_ACK_BUFFER:

      for (buf = self->buffers; buf; buf = buf->next) {
        if (buf->shm_area->id == cb.area_id &&
            buf->offset == cb.payload.ack_buffer.offset) {
          return sp_shmbuf_dec (self, buf, prev_buf, client, tag);
        }
        prev_buf = buf;
      }

      return -2;
    default:
      return -99;
  }

  return 0;
}

int
sp_client_recv_finish (ShmPipe * self, char *buf)
{
  ShmArea *shm_area = NULL;
  unsigned long offset;
  struct CommandBuffer cb = { 0 };

  for (shm_area = self->shm_area; shm_area; shm_area = shm_area->next) {
    if (buf >= shm_area->shm_area_buf &&
        buf < shm_area->shm_area_buf + shm_area->shm_area_len)
      break;
  }

  assert (shm_area);

  offset = buf - shm_area->shm_area_buf;

  sp_shm_area_dec (self, shm_area);

  cb.payload.ack_buffer.offset = offset;
  return send_command (self->main_socket, &cb, COMMAND_ACK_BUFFER,
      self->shm_area->id);
}

ShmPipe *
sp_client_open (const char *path)
{
  ShmPipe *self = spalloc_new (ShmPipe);
  struct sockaddr_un sock_un;
  int flags;

  memset (self, 0, sizeof (ShmPipe));

  self->main_socket = socket (PF_UNIX, SOCK_STREAM, 0);
  self->use_count = 1;

  if (self->main_socket < 0)
    goto error;

  flags = fcntl (self->main_socket, F_GETFL, 0);
  if (flags < 0)
    goto error;

  if (fcntl (self->main_socket, F_SETFL, flags | FD_CLOEXEC) < 0)
    goto error;

  sock_un.sun_family = AF_UNIX;
  strncpy (sock_un.sun_path, path, sizeof (sock_un.sun_path) - 1);

  if (connect (self->main_socket, (struct sockaddr *) &sock_un,
          sizeof (struct sockaddr_un)) < 0)
    goto error;

  return self;

error:
  sp_client_close (self);
  return NULL;
}


ShmClient *
sp_writer_accept_client (ShmPipe * self)
{
  ShmClient *client = NULL;
  int fd;
  struct CommandBuffer cb = { 0 };
  int pathlen = strlen (self->shm_area->shm_area_name) + 1;


  fd = accept (self->main_socket, NULL, NULL);

  if (fd < 0) {
    fprintf (stderr, "Could not client connection");
    return NULL;
  }

  cb.payload.new_shm_area.size = self->shm_area->shm_area_len;
  cb.payload.new_shm_area.path_size = pathlen;
  if (!send_command (fd, &cb, COMMAND_NEW_SHM_AREA, self->shm_area->id)) {
    fprintf (stderr, "Sending new shm area failed: %s", strerror (errno));
    goto error;
  }

  if (send (fd, self->shm_area->shm_area_name, pathlen, MSG_NOSIGNAL) !=
      pathlen) {
    fprintf (stderr, "Sending new shm area path failed: %s", strerror (errno));
    goto error;
  }

  client = spalloc_new (ShmClient);
  client->fd = fd;

  /* Prepend ot linked list */
  client->next = self->clients;
  self->clients = client;
  self->num_clients++;

  return client;

error:
  shutdown (fd, SHUT_RDWR);
  close (fd);
  return NULL;
}

static int
sp_shmbuf_dec (ShmPipe * self, ShmBuffer * buf, ShmBuffer * prev_buf,
    ShmClient * client, void **tag)
{
  int i;
  int had_client = 0;

  /* Remove client from the list of buffer users. Here we make sure that
   * if a client closes connection but already decremented the use count
   * for this buffer, but other clients didn't have time to decrement
   * buffer will not be freed too early in sp_writer_close_client.
   */
  for (i = 0; i < buf->num_clients; i++) {
    if (buf->clients[i] == client->fd) {
      buf->clients[i] = -1;
      had_client = 1;
      break;
    }
  }
  assert (had_client);

  buf->use_count--;

  if (buf->use_count == 0) {
    /* Remove from linked list */
    if (prev_buf)
      prev_buf->next = buf->next;
    else
      self->buffers = buf->next;

    if (tag)
      *tag = buf->tag;
    shm_alloc_space_block_dec (buf->ablock);
    sp_shm_area_dec (self, buf->shm_area);
    spalloc_free1 (sizeof (ShmBuffer) + sizeof (int) * buf->num_clients, buf);
    return 0;
  }
  return 1;
}

void
sp_writer_close_client (ShmPipe * self, ShmClient * client,
    sp_buffer_free_callback callback, void *user_data)
{
  ShmBuffer *buffer = NULL, *prev_buf = NULL;
  ShmClient *item = NULL, *prev_item = NULL;

  shutdown (client->fd, SHUT_RDWR);
  close (client->fd);

again:
  for (buffer = self->buffers; buffer; buffer = buffer->next) {
    int i;
    void *tag = NULL;

    for (i = 0; i < buffer->num_clients; i++) {
      if (buffer->clients[i] == client->fd) {
        if (!sp_shmbuf_dec (self, buffer, prev_buf, client, &tag)) {
          if (callback)
            callback (tag, user_data);
          goto again;
        }
        break;
      }
    }
    prev_buf = buffer;
  }

  for (item = self->clients; item; item = item->next) {
    if (item == client)
      break;
    prev_item = item;
  }
  assert (item);

  if (prev_item)
    prev_item->next = client->next;
  else
    self->clients = client->next;

  self->num_clients--;

  spalloc_free (ShmClient, client);
}

int
sp_get_fd (ShmPipe * self)
{
  return self->main_socket;
}

const char *
sp_get_shm_area_name (ShmPipe * self)
{
  if (self->shm_area)
    return self->shm_area->shm_area_name;

  return NULL;
}

int
sp_writer_get_client_fd (ShmClient * client)
{
  return client->fd;
}

int
sp_writer_pending_writes (ShmPipe * self)
{
  return (self->buffers != NULL);
}

const char *
sp_writer_get_path (ShmPipe * pipe)
{
  return pipe->socket_path;
}

ShmBuffer *
sp_writer_get_pending_buffers (ShmPipe * self)
{
  return self->buffers;
}

ShmBuffer *
sp_writer_get_next_buffer (ShmBuffer * buffer)
{
  return buffer->next;
}

void *
sp_writer_buf_get_tag (ShmBuffer * buffer)
{
  return buffer->tag;
}

size_t
sp_writer_get_max_buf_size (ShmPipe * self)
{
  if (self->shm_area == NULL)
    return 0;

  return self->shm_area->shm_area_len;
}
