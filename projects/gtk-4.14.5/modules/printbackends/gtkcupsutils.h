/* gtkcupsutils.h
 * Copyright (C) 2006 John (J5) Palmieri <johnp@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>
#include <cups/cups.h>
#include <cups/language.h>
#include <cups/http.h>
#include <cups/ipp.h>

G_BEGIN_DECLS

typedef struct _GtkCupsRequest        GtkCupsRequest;
typedef struct _GtkCupsResult         GtkCupsResult;
typedef struct _GtkCupsConnectionTest GtkCupsConnectionTest;

typedef enum
{
  GTK_CUPS_ERROR_HTTP,
  GTK_CUPS_ERROR_IPP,
  GTK_CUPS_ERROR_IO,
  GTK_CUPS_ERROR_AUTH,
  GTK_CUPS_ERROR_GENERAL
} GtkCupsErrorType;

typedef enum
{
  GTK_CUPS_POST,
  GTK_CUPS_GET
} GtkCupsRequestType;


/*
 * Direction we should be polling the http socket on.
 * We are either reading or writing at each state.
 * This makes it easy for mainloops to connect to poll.
 */
typedef enum
{
  GTK_CUPS_HTTP_IDLE,
  GTK_CUPS_HTTP_READ,
  GTK_CUPS_HTTP_WRITE
} GtkCupsPollState;

typedef enum
{
  GTK_CUPS_CONNECTION_AVAILABLE,
  GTK_CUPS_CONNECTION_NOT_AVAILABLE,
  GTK_CUPS_CONNECTION_IN_PROGRESS  
} GtkCupsConnectionState;

typedef enum
{
  GTK_CUPS_PASSWORD_NONE,
  GTK_CUPS_PASSWORD_REQUESTED,
  GTK_CUPS_PASSWORD_HAS,
  GTK_CUPS_PASSWORD_APPLIED,
  GTK_CUPS_PASSWORD_NOT_VALID
} GtkCupsPasswordState;

struct _GtkCupsRequest 
{
  GtkCupsRequestType type;

  http_t *http;
  http_status_t last_status;
  ipp_t *ipp_request;

  char *server;
  char *resource;
  GIOChannel *data_io;
  int attempts;

  GtkCupsResult *result;

  int state;
  GtkCupsPollState poll_state;
  guint64 bytes_received;

  char *password;
  char *username;

  unsigned int own_http : 1;
  unsigned int need_password : 1;
  unsigned int need_auth_info : 1;
  char **auth_info_required;
  char **auth_info;
  GtkCupsPasswordState password_state;
};

struct _GtkCupsConnectionTest
{
  GtkCupsConnectionState at_init;
  http_addrlist_t       *addrlist;
  http_addrlist_t       *current_addr;
  http_addrlist_t       *last_wrong_addr;
  int                    socket;
};

#define GTK_CUPS_REQUEST_START 0
#define GTK_CUPS_REQUEST_DONE 500

/* POST states */
enum 
{
  GTK_CUPS_POST_CONNECT = GTK_CUPS_REQUEST_START,
  GTK_CUPS_POST_SEND,
  GTK_CUPS_POST_WRITE_REQUEST,
  GTK_CUPS_POST_WRITE_DATA,
  GTK_CUPS_POST_CHECK,
  GTK_CUPS_POST_AUTH,
  GTK_CUPS_POST_READ_RESPONSE,
  GTK_CUPS_POST_DONE = GTK_CUPS_REQUEST_DONE
};

/* GET states */
enum
{
  GTK_CUPS_GET_CONNECT = GTK_CUPS_REQUEST_START,
  GTK_CUPS_GET_SEND,
  GTK_CUPS_GET_CHECK,
  GTK_CUPS_GET_AUTH,
  GTK_CUPS_GET_READ_DATA,
  GTK_CUPS_GET_DONE = GTK_CUPS_REQUEST_DONE
};

GtkCupsRequest        * gtk_cups_request_new_with_username (http_t             *connection,
                                                            GtkCupsRequestType  req_type,
                                                            int                 operation_id,
                                                            GIOChannel         *data_io,
                                                            const char         *server,
                                                            const char         *resource,
                                                            const char         *username);
GtkCupsRequest        * gtk_cups_request_new               (http_t             *connection,
                                                            GtkCupsRequestType  req_type,
                                                            int                 operation_id,
                                                            GIOChannel         *data_io,
                                                            const char         *server,
                                                            const char         *resource);
void                    gtk_cups_request_ipp_add_string    (GtkCupsRequest     *request,
                                                            ipp_tag_t           group,
                                                            ipp_tag_t           tag,
                                                            const char         *name,
                                                            const char         *charset,
                                                            const char         *value);
void                    gtk_cups_request_ipp_add_strings   (GtkCupsRequest     *request,
                                                            ipp_tag_t           group,
                                                            ipp_tag_t           tag,
                                                            const char         *name,
                                                            int                 num_values,
                                                            const char         *charset,
                                                            const char * const *values);
const char            * gtk_cups_request_ipp_get_string    (GtkCupsRequest     *request,
                                                            ipp_tag_t           tag,
                                                            const char         *name);
gboolean                gtk_cups_request_read_write        (GtkCupsRequest     *request,
                                                            gboolean            connect_only);
GtkCupsPollState        gtk_cups_request_get_poll_state    (GtkCupsRequest     *request);
void                    gtk_cups_request_free              (GtkCupsRequest     *request);
GtkCupsResult         * gtk_cups_request_get_result        (GtkCupsRequest     *request);
gboolean                gtk_cups_request_is_done           (GtkCupsRequest     *request);
void                    gtk_cups_request_encode_option     (GtkCupsRequest     *request,
                                                            const char         *option,
                                                            const char         *value);
void                    gtk_cups_request_set_ipp_version   (GtkCupsRequest     *request,
                                                            int                 major,
                                                            int                 minor);
gboolean                gtk_cups_result_is_error           (GtkCupsResult      *result);
ipp_t                 * gtk_cups_result_get_response       (GtkCupsResult      *result);
GtkCupsErrorType        gtk_cups_result_get_error_type     (GtkCupsResult      *result);
int                     gtk_cups_result_get_error_status   (GtkCupsResult      *result);
int                     gtk_cups_result_get_error_code     (GtkCupsResult      *result);
const char            * gtk_cups_result_get_error_string   (GtkCupsResult      *result);
GtkCupsConnectionTest * gtk_cups_connection_test_new       (const char         *server,
                                                            const int           port);
GtkCupsConnectionState  gtk_cups_connection_test_get_state (GtkCupsConnectionTest *test);
void                    gtk_cups_connection_test_free      (GtkCupsConnectionTest *test);

G_END_DECLS

