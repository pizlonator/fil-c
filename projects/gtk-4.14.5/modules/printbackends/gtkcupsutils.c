/* GTK - The GIMP Toolkit
 * gtkcupsutils.h: Statemachine implementation of POST and GET 
 * cups calls which can be used to create a non-blocking cups API
 * Copyright (C) 2006, 2007 Red Hat, Inc.
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

#include "config.h"
#include <gtk/gtk.h>
#include "gtkcupsutils.h"
#include "gtkprivate.h"

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>

typedef void (*GtkCupsRequestStateFunc) (GtkCupsRequest *request);

static void _connect            (GtkCupsRequest *request);
static void _post_send          (GtkCupsRequest *request);
static void _post_write_request (GtkCupsRequest *request);
static void _post_write_data    (GtkCupsRequest *request);
static void _post_check         (GtkCupsRequest *request);
static void _post_auth          (GtkCupsRequest *request);
static void _post_read_response (GtkCupsRequest *request);

static void _get_send           (GtkCupsRequest *request);
static void _get_check          (GtkCupsRequest *request);
static void _get_auth           (GtkCupsRequest *request);
static void _get_read_data      (GtkCupsRequest *request);

struct _GtkCupsResult
{
  char *error_msg;
  ipp_t *ipp_response;
  GtkCupsErrorType error_type;

  /* some error types like HTTP_ERROR have a status and a code */
  int error_status;            
  int error_code;

  guint is_error : 1;
  guint is_ipp_response : 1;
};


#define _GTK_CUPS_MAX_ATTEMPTS 10 
#define _GTK_CUPS_MAX_CHUNK_SIZE 8192

static GtkCupsRequestStateFunc post_states[] = {
  _connect,
  _post_send,
  _post_write_request,
  _post_write_data,
  _post_check,
  _post_auth,
  _post_read_response
};

static GtkCupsRequestStateFunc get_states[] = {
  _connect,
  _get_send,
  _get_check,
  _get_auth,
  _get_read_data
};

static void G_GNUC_PRINTF (5, 6)
gtk_cups_result_set_error (GtkCupsResult    *result,
                           GtkCupsErrorType  error_type,
                           int               error_status,
                           int               error_code,
                           const char       *error_msg,
                           ...)
{
  va_list args;

  result->is_ipp_response = FALSE;
  result->is_error = TRUE;
  result->error_type = error_type;
  result->error_status = error_status;
  result->error_code = error_code;

  va_start (args, error_msg);
  result->error_msg = g_strdup_vprintf (error_msg, args);
  va_end (args);
}

GtkCupsRequest *
gtk_cups_request_new_with_username (http_t             *connection,
                                    GtkCupsRequestType  req_type, 
                                    int                 operation_id,
                                    GIOChannel         *data_io,
                                    const char         *server,
                                    const char         *resource,
                                    const char         *username)
{
  GtkCupsRequest *request;
  cups_lang_t *language;
  
  request = g_new0 (GtkCupsRequest, 1);
  request->result = g_new0 (GtkCupsResult, 1);

  request->result->error_msg = NULL;
  request->result->ipp_response = NULL;

  request->result->is_error = FALSE;
  request->result->is_ipp_response = FALSE;

  request->type = req_type;
  request->state = GTK_CUPS_REQUEST_START;

  request->password_state = GTK_CUPS_PASSWORD_NONE;

   if (server)
    request->server = g_strdup (server);
  else
    request->server = g_strdup (cupsServer ());


  if (resource)
    request->resource = g_strdup (resource);
  else
    request->resource = g_strdup ("/");
 
  if (connection != NULL)
    {
      request->http = connection;
      request->own_http = FALSE;
    }
  else
    {
      request->http = httpConnect2 (request->server, ippPort (),
                                    NULL, AF_UNSPEC,
                                    cupsEncryption (),
                                    1, 30000, NULL);

      if (request->http)
        httpBlocking (request->http, 0);
        
      request->own_http = TRUE;
    }

  request->last_status = HTTP_CONTINUE;

  request->attempts = 0;
  request->data_io = data_io;

  request->ipp_request = ippNew ();
  ippSetOperation (request->ipp_request, operation_id);
  ippSetRequestId (request->ipp_request, 1);

  language = cupsLangDefault ();

  gtk_cups_request_ipp_add_string (request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                                   "attributes-charset", 
                                   NULL, "utf-8");
	
  gtk_cups_request_ipp_add_string (request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                                   "attributes-natural-language", 
                                   NULL, language->language);

  if (username != NULL)
    gtk_cups_request_ipp_add_string (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                                     "requesting-user-name",
                                     NULL, username);
  else
    gtk_cups_request_ipp_add_string (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                                     "requesting-user-name",
                                     NULL, cupsUser ());

  request->auth_info_required = NULL;
  request->auth_info = NULL;
  request->need_auth_info = FALSE;

  cupsLangFree (language);

  return request;
}

GtkCupsRequest *
gtk_cups_request_new (http_t             *connection,
                      GtkCupsRequestType  req_type, 
                      int                 operation_id,
                      GIOChannel         *data_io,
                      const char         *server,
                      const char         *resource)
{
  return gtk_cups_request_new_with_username (connection,
                                             req_type,
                                             operation_id,
                                             data_io,
                                             server,
                                             resource,
                                             NULL);
}

static void
gtk_cups_result_free (GtkCupsResult *result)
{
  g_free (result->error_msg);

  if (result->ipp_response)
    ippDelete (result->ipp_response);

  g_free (result);
}

void
gtk_cups_request_free (GtkCupsRequest *request)
{
  if (request->own_http)
    {
      if (request->http)
        httpClose (request->http);
    }
  
  if (request->ipp_request)
    ippDelete (request->ipp_request);

  g_free (request->server);
  g_free (request->resource);
  if (request->password != NULL)
    {
      memset (request->password, 0, strlen (request->password));
      g_free (request->password);
    }

  g_free (request->username);
  g_strfreev (request->auth_info_required);

  gtk_cups_result_free (request->result);

  g_free (request);
}

gboolean 
gtk_cups_request_read_write (GtkCupsRequest *request, gboolean connect_only)
{
  if (connect_only && request->state != GTK_CUPS_REQUEST_START)
    return FALSE;

  do
    {
      if (request->type == GTK_CUPS_POST)
        post_states[request->state] (request);
      else if (request->type == GTK_CUPS_GET)
        get_states[request->state] (request);

      if (gtk_cups_result_is_error (request->result))
        request->state = GTK_CUPS_REQUEST_DONE;

      if (request->attempts > _GTK_CUPS_MAX_ATTEMPTS &&
          request->state != GTK_CUPS_REQUEST_DONE)
        {
          /* TODO: should add a status or error code for too many failed attempts */
          gtk_cups_result_set_error (request->result,
                                     GTK_CUPS_ERROR_GENERAL,
                                     0,
                                     0,
                                     "Too many failed attempts");

          request->state = GTK_CUPS_REQUEST_DONE;
        }

      if (request->state == GTK_CUPS_REQUEST_DONE)
        {
          request->poll_state = GTK_CUPS_HTTP_IDLE;
          return TRUE;
        }
    }
  /* We need to recheck using httpCheck if the poll_state is read, because
   * Cups has an internal read buffer. And if this buffer is filled, we may
   * never get a poll event again. */
  while (request->poll_state == GTK_CUPS_HTTP_READ && request->http && httpCheck(request->http));

  return FALSE;
}

GtkCupsPollState 
gtk_cups_request_get_poll_state (GtkCupsRequest *request)
{
  return request->poll_state;
}



GtkCupsResult *
gtk_cups_request_get_result (GtkCupsRequest *request)
{
  return request->result;
}

void            
gtk_cups_request_ipp_add_string (GtkCupsRequest *request,
                                 ipp_tag_t       group,
                                 ipp_tag_t       tag,
                                 const char     *name,
                                 const char     *charset,
                                 const char     *value)
{
  ippAddString (request->ipp_request,
                group,
                tag,
                name,
                charset,
                value);
}

void            
gtk_cups_request_ipp_add_strings (GtkCupsRequest    *request,
				  ipp_tag_t          group,
				  ipp_tag_t          tag,
				  const char        *name,
				  int                num_values,
				  const char        *charset,
				  const char *const *values)
{
  ippAddStrings (request->ipp_request,
		 group,
		 tag,
		 name,
		 num_values,
		 charset,
		 values);
}

const char *
gtk_cups_request_ipp_get_string (GtkCupsRequest *request,
                                 ipp_tag_t       tag,
                                 const char     *name)
{
  ipp_attribute_t *attribute = NULL;

  if (request != NULL && request->ipp_request != NULL)
    attribute = ippFindAttribute (request->ipp_request,
                                  name,
                                  tag);

  if (attribute != NULL && ippGetCount (attribute) > 0)
      return ippGetString (attribute, 0, NULL);
  else
    return NULL;
}


typedef struct
{
  const char	*name;
  ipp_tag_t	value_tag;
} ipp_option_t;

static const ipp_option_t ipp_options[] = {
  { "blackplot",		IPP_TAG_BOOLEAN },
  { "brightness",		IPP_TAG_INTEGER },
  { "columns",			IPP_TAG_INTEGER },
  { "copies",			IPP_TAG_INTEGER },
  { "finishings",		IPP_TAG_ENUM },
  { "fitplot",			IPP_TAG_BOOLEAN },
  { "gamma",			IPP_TAG_INTEGER },
  { "hue",			IPP_TAG_INTEGER },
  { "job-k-limit",		IPP_TAG_INTEGER },
  { "job-page-limit",		IPP_TAG_INTEGER },
  { "job-priority",		IPP_TAG_INTEGER },
  { "job-quota-period",		IPP_TAG_INTEGER },
  { "landscape",		IPP_TAG_BOOLEAN },
  { "media",			IPP_TAG_KEYWORD },
  { "mirror",			IPP_TAG_BOOLEAN },
  { "natural-scaling",		IPP_TAG_INTEGER },
  { "number-up",		IPP_TAG_INTEGER },
  { "orientation-requested",	IPP_TAG_ENUM },
  { "page-bottom",		IPP_TAG_INTEGER },
  { "page-left",		IPP_TAG_INTEGER },
  { "page-ranges",		IPP_TAG_RANGE },
  { "page-right",		IPP_TAG_INTEGER },
  { "page-top",			IPP_TAG_INTEGER },
  { "penwidth",			IPP_TAG_INTEGER },
  { "ppi",			IPP_TAG_INTEGER },
  { "prettyprint",		IPP_TAG_BOOLEAN },
  { "printer-resolution",	IPP_TAG_RESOLUTION },
  { "print-quality",		IPP_TAG_ENUM },
  { "saturation",		IPP_TAG_INTEGER },
  { "scaling",			IPP_TAG_INTEGER },
  { "sides",			IPP_TAG_KEYWORD },
  { "wrap",			IPP_TAG_BOOLEAN },
  { "number-up-layout",		IPP_TAG_INTEGER }
};


static ipp_tag_t
_find_option_tag (const char *option)
{
  int lower_bound, upper_bound, num_options;
  int current_option;
  ipp_tag_t result;

  result = IPP_TAG_ZERO;

  lower_bound = 0;
  upper_bound = num_options = (int) G_N_ELEMENTS (ipp_options) - 1;
  
  while (1)
    {
      int match;
      current_option = (int) (((upper_bound - lower_bound) / 2) + lower_bound);

      match = strcasecmp (option, ipp_options[current_option].name);
      if (match == 0)
        {
	  result = ipp_options[current_option].value_tag;
	  return result;
	}
      else if (match < 0)
        {
          upper_bound = current_option - 1;
	}
      else
        {
          lower_bound = current_option + 1;
	}

      if (upper_bound == lower_bound && upper_bound == current_option)
        return result;

      if (upper_bound < 0)
        return result;

      if (lower_bound > num_options)
        return result;

      if (upper_bound < lower_bound)
        return result;
    }
}

/*
 * Note that this function uses IPP_TAG_JOB, so it is
 * only suitable for IPP Group 2 attributes.
 * See RFC 2911.
 */
void
gtk_cups_request_encode_option (GtkCupsRequest *request,
                                const char     *option,
			        const char     *value)
{
  ipp_tag_t option_tag;

  g_return_if_fail (option != NULL);
  g_return_if_fail (value != NULL);

  option_tag = _find_option_tag (option);

  if (option_tag == IPP_TAG_ZERO)
    {
      option_tag = IPP_TAG_NAME;
      if (strcasecmp (value, "true") == 0 ||
          strcasecmp (value, "false") == 0)
        {
          option_tag = IPP_TAG_BOOLEAN;
        }
    }
        
  switch ((guint)option_tag)
    {
      case IPP_TAG_INTEGER:
      case IPP_TAG_ENUM:
        ippAddInteger (request->ipp_request,
                       IPP_TAG_JOB,
                       option_tag,
                       option,
                       strtol (value, NULL, 0));
        break;

      case IPP_TAG_BOOLEAN:
        {
          char b;
          
          if (strcasecmp (value, "true") == 0 ||
	      strcasecmp (value, "on") == 0 ||
	      strcasecmp (value, "yes") == 0) 
	    b = 1;
	  else
            b = 0;

          ippAddBoolean (request->ipp_request,
                         IPP_TAG_JOB,
                         option,
                         b);
        
          break;
        }
        
      case IPP_TAG_RANGE:
        {
          char	*s;
          int lower;
          int upper;
          
          if (*value == '-')
	    {
	      lower = 1;
	      s = (char *)value;
	    }
	  else
	    lower = strtol (value, &s, 0);

	  if (*s == '-')
	    {
	      if (s[1])
		upper = strtol (s + 1, NULL, 0);
	      else
		upper = 2147483647;
            }
	  else
	    upper = lower;
         
          ippAddRange (request->ipp_request,
                       IPP_TAG_JOB,
                       option,
                       lower,
                       upper);

          break;
        }

      case IPP_TAG_RESOLUTION:
        {
          char *s;
          int xres;
          int yres;
          ipp_res_t units;
          
          xres = strtol (value, &s, 0);

	  if (*s == 'x')
	    yres = strtol (s + 1, &s, 0);
	  else
	    yres = xres;

	  if (strcasecmp (s, "dpc") == 0)
            units = IPP_RES_PER_CM;
          else
            units = IPP_RES_PER_INCH;
          
          ippAddResolution (request->ipp_request,
                            IPP_TAG_JOB,
                            option,
                            units,
                            xres,
                            yres);

          break;
        }

      default:
        {
          char *values;
          char *s;
          int in_quotes;
          char *next;
          GPtrArray *strings;
          
          values = g_strdup (value);
          strings = NULL;
	  in_quotes = 0;

          for (s = values, next = s; *s != '\0'; s++)
            {
              if (in_quotes != 2 && *s == '\'')
                {
                  /* skip quoted value */
                  if (in_quotes == 0)
                    in_quotes = 1;
                  else
                    in_quotes = 0;
                }
              else if (in_quotes != 1 && *s == '\"')
                {
                  /* skip quoted value */
                  if (in_quotes == 0)
                    in_quotes = 2;
                  else
                    in_quotes = 0;
                }
              else if (in_quotes == 0 && *s == ',')
                {
                  /* found delimiter, add to value array */
                  *s = '\0';
                  if (strings == NULL)
                    strings = g_ptr_array_new ();
                  g_ptr_array_add (strings, next);
                  next = s + 1;
                }
              else if (in_quotes == 0 && *s == '\\' && s[1] != '\0')
                {
                  /* skip escaped character */
                  s++;
                }
            }
          
          if (strings == NULL)
            {
              /* single value */
              ippAddString (request->ipp_request,
                            IPP_TAG_JOB,
                            option_tag,
                            option,
                            NULL,
                            value);
            }
          else
            {
              /* multiple values */
              
              /* add last value */
              g_ptr_array_add (strings, next);
              
              ippAddStrings (request->ipp_request,
                             IPP_TAG_JOB,
                             option_tag,
                             option,
                             strings->len,
                             NULL,
                             (const char **) strings->pdata);
              g_ptr_array_free (strings, TRUE);
            }

          g_free (values);
        }

        break;
    }
}
				
void
gtk_cups_request_set_ipp_version (GtkCupsRequest     *request,
				  int                 major,
				  int                 minor)
{
  ippSetVersion (request->ipp_request, major, minor);
}

static void
_connect (GtkCupsRequest *request)
{
  request->poll_state = GTK_CUPS_HTTP_IDLE;
  request->bytes_received = 0;

  if (request->http == NULL)
    {
      request->http = httpConnect2 (request->server, ippPort (),
                                    NULL, AF_UNSPEC,
                                    cupsEncryption (),
                                    1, 30000, NULL);
      if (request->http == NULL)
        request->attempts++;

      if (request->http)
        httpBlocking (request->http, 0);
        
      request->own_http = TRUE;
    }
  else
    {
      request->attempts = 0;
      request->state++;

      /* we always write to the socket after we get
         the connection */
      request->poll_state = GTK_CUPS_HTTP_WRITE;
    }
}

static void 
_post_send (GtkCupsRequest *request)
{
  char length[255];
  struct stat data_info;

  GTK_DEBUG (PRINTING, "CUPS Backend: %s", G_STRFUNC);

  request->poll_state = GTK_CUPS_HTTP_WRITE;

  if (request->data_io != NULL)
    {
      fstat (g_io_channel_unix_get_fd (request->data_io), &data_info);
      sprintf (length, "%lu", (unsigned long) (ippLength (request->ipp_request) + data_info.st_size));
    }
  else
    sprintf (length, "%lu", (unsigned long) ippLength (request->ipp_request));
	
  httpClearFields (request->http);
  httpSetField (request->http, HTTP_FIELD_CONTENT_LENGTH, length);
  httpSetField (request->http, HTTP_FIELD_CONTENT_TYPE, "application/ipp");
  httpSetField (request->http, HTTP_FIELD_AUTHORIZATION, httpGetAuthString (request->http));

  if (httpPost (request->http, request->resource))
    {
      int res;

      res = httpReconnect2 (request->http, 30000, NULL);

      if (res)
        {
          request->state = GTK_CUPS_POST_DONE;
          request->poll_state = GTK_CUPS_HTTP_IDLE;

          /* TODO: should add a status or error code for failed post */
          gtk_cups_result_set_error (request->result,
                                     GTK_CUPS_ERROR_GENERAL,
                                     0,
                                     0,
                                     "Failed Post");
        }

      request->attempts++;
      return;    
    }
        
    request->attempts = 0;

    request->state = GTK_CUPS_POST_WRITE_REQUEST;
    ippSetState (request->ipp_request, IPP_IDLE);
}

static void 
_post_write_request (GtkCupsRequest *request)
{
  ipp_state_t ipp_status;

  GTK_DEBUG (PRINTING, "CUPS Backend: %s", G_STRFUNC);

  request->poll_state = GTK_CUPS_HTTP_WRITE;
  
  ipp_status = ippWrite (request->http, request->ipp_request);

  if (ipp_status == IPP_ERROR)
    {
      int cups_error = cupsLastError ();
      request->state = GTK_CUPS_POST_DONE;
      request->poll_state = GTK_CUPS_HTTP_IDLE;
 
      gtk_cups_result_set_error (request->result, 
                                 GTK_CUPS_ERROR_IPP,
                                 ipp_status,
                                 cups_error,
                                 "%s", 
                                 ippErrorString (cups_error));
      return;
    }

  if (ipp_status == IPP_DATA)
    {
      if (request->data_io != NULL)
        request->state = GTK_CUPS_POST_WRITE_DATA;
      else
        {
          request->state = GTK_CUPS_POST_CHECK;
          request->poll_state = GTK_CUPS_HTTP_READ;
	}
    }
}

static void 
_post_write_data (GtkCupsRequest *request)
{
  gsize bytes;
  char buffer[_GTK_CUPS_MAX_CHUNK_SIZE];
  http_status_t http_status;

  GTK_DEBUG (PRINTING, "CUPS Backend: %s", G_STRFUNC);

  request->poll_state = GTK_CUPS_HTTP_WRITE;
  
  if (httpCheck (request->http))
    http_status = httpUpdate (request->http);
  else
    http_status = request->last_status;

  request->last_status = http_status;


  if (http_status == HTTP_CONTINUE || http_status == HTTP_OK)
    {
      GIOStatus io_status;
      GError *error;

      error = NULL;

      /* send data */
      io_status =
        g_io_channel_read_chars (request->data_io, 
	                         buffer, 
				 _GTK_CUPS_MAX_CHUNK_SIZE,
				 &bytes,
				 &error);

      if (io_status == G_IO_STATUS_ERROR)
        {
          request->state = GTK_CUPS_POST_DONE;
	  request->poll_state = GTK_CUPS_HTTP_IDLE;
     
          gtk_cups_result_set_error (request->result,
                                     GTK_CUPS_ERROR_IO,
                                     io_status,
                                     error->code, 
                                     "Error reading from cache file: %s",
                                     error->message);

	  g_error_free (error);
          return;
	}
      else if (bytes == 0 && io_status == G_IO_STATUS_EOF)
        {
          request->state = GTK_CUPS_POST_CHECK;
	  request->poll_state = GTK_CUPS_HTTP_READ;

          request->attempts = 0;
          return;
        }


      if (httpWrite2 (request->http, buffer, bytes) < bytes)
        {
          int http_errno;

          http_errno = httpError (request->http);

          request->state = GTK_CUPS_POST_DONE;
	  request->poll_state = GTK_CUPS_HTTP_IDLE;
     
          gtk_cups_result_set_error (request->result,
                                     GTK_CUPS_ERROR_HTTP,
                                     http_status,
                                     http_errno, 
                                     "Error writing to socket in Post %s", 
                                     g_strerror (http_errno));
          return;
        }
    }
  else if (http_status == HTTP_UNAUTHORIZED)
    {
      request->state = GTK_CUPS_POST_CHECK;
      request->poll_state = GTK_CUPS_HTTP_READ;

      request->attempts = 0;
      return;
    }
  else
    {
      request->attempts++;
    }
}

static void
_post_auth (GtkCupsRequest *request)
{
  if (request->password_state == GTK_CUPS_PASSWORD_HAS)
    {
      if (request->password == NULL)
        {
          request->state = GTK_CUPS_POST_DONE;
          request->poll_state = GTK_CUPS_HTTP_IDLE;

          gtk_cups_result_set_error (request->result, 
                                     GTK_CUPS_ERROR_AUTH,
                                     0,
                                     1,
                                     "Canceled by user");
        }
      else
        request->state = GTK_CUPS_POST_CHECK;
    }
}

static void
_get_auth (GtkCupsRequest *request)
{
  if (request->password_state == GTK_CUPS_PASSWORD_HAS)
    {
      if (request->password == NULL)
        {
          request->state = GTK_CUPS_GET_DONE;
          request->poll_state = GTK_CUPS_HTTP_IDLE;

          gtk_cups_result_set_error (request->result, 
                                     GTK_CUPS_ERROR_AUTH,
                                     0,
                                     1,
                                     "Canceled by user");
        }
      else
        request->state = GTK_CUPS_GET_CHECK;
    }
}

/* Very ugly hack: cups has a stupid synchronous password callback 
 * that doesn't even take the request or user data parameters, so 
 * we have to use a static variable to pass the password to it.
 * Not threadsafe !
 * The callback sets cups_password to NULL to signal that the 
 * password has been used.
 */
static char *cups_password = NULL;
static char *cups_username = NULL;

static const char *
passwordCB (const char *prompt)
{
  char *pwd = cups_password;
  cups_password = NULL;

  cupsSetUser (cups_username);

  return pwd;
}

static void 
_post_check (GtkCupsRequest *request)
{
  http_status_t http_status;

  http_status = request->last_status;

  GTK_DEBUG (PRINTING, "CUPS Backend: %s - status %i", G_STRFUNC, http_status);

  request->poll_state = GTK_CUPS_HTTP_READ;

  if (http_status == HTTP_CONTINUE)
    {
      goto again; 
    }
  else if (http_status == HTTP_UNAUTHORIZED)
    {
      int auth_result = -1;
      httpFlush (request->http);

      if (request->password_state == GTK_CUPS_PASSWORD_APPLIED)
        {
          request->poll_state = GTK_CUPS_HTTP_IDLE;
          request->password_state = GTK_CUPS_PASSWORD_NOT_VALID;
          request->state = GTK_CUPS_POST_AUTH;
          request->need_password = TRUE;

          return;
        }

      /* Negotiate */
      if (strncmp (httpGetField (request->http, HTTP_FIELD_WWW_AUTHENTICATE), "Negotiate", 9) == 0)
        {
          auth_result = cupsDoAuthentication (request->http, "POST", request->resource);
        }
      /* Basic, BasicDigest, Digest and PeerCred */
      else
        {
          if (request->password_state == GTK_CUPS_PASSWORD_NONE)
            {
              cups_username = request->username;
              cupsSetPasswordCB (passwordCB);

              /* This call success for PeerCred authentication */
              auth_result = cupsDoAuthentication (request->http, "POST", request->resource);

              if (auth_result != 0)
                {
                  /* move to AUTH state to let the backend 
                   * ask for a password
                   */ 
                  request->poll_state = GTK_CUPS_HTTP_IDLE;
                  request->state = GTK_CUPS_POST_AUTH;
                  request->need_password = TRUE;

                  return;
                }
            }
          else
            {
              cups_password = request->password;
              cups_username = request->username;

              auth_result = cupsDoAuthentication (request->http, "POST", request->resource);

              if (cups_password != NULL)
                return;

              if (request->password != NULL)
                {
                  memset (request->password, 0, strlen (request->password));
                  g_free (request->password);
                  request->password = NULL;
                }

              request->password_state = GTK_CUPS_PASSWORD_APPLIED;
            }
        }

      if (auth_result ||
          httpReconnect2 (request->http, 30000, NULL))
        {
          /* if the password has been used, reset password_state
           * so that we ask for a new one next time around
           */ 
          if (cups_password == NULL)
            request->password_state = GTK_CUPS_PASSWORD_NONE;

          request->state = GTK_CUPS_POST_DONE;
          request->poll_state = GTK_CUPS_HTTP_IDLE;
          gtk_cups_result_set_error (request->result, 
                                     GTK_CUPS_ERROR_AUTH,
                                     0,
                                     0,
                                     "Not authorized");
          return;
        }
      
      if (request->data_io != NULL)
        g_io_channel_seek_position (request->data_io, 0, G_SEEK_SET, NULL);

      request->state = GTK_CUPS_POST_CONNECT;
      request->poll_state = GTK_CUPS_HTTP_WRITE;
    }
  else if (http_status == HTTP_ERROR)
    {
      int error = httpError (request->http);
#ifdef G_OS_WIN32
      if (error != WSAENETDOWN && error != WSAENETUNREACH)
#else
      if (error != ENETDOWN && error != ENETUNREACH)	  
#endif /* G_OS_WIN32 */
        {
          request->attempts++;
          goto again;
        }
      else
        {
          request->state = GTK_CUPS_POST_DONE;
          request->poll_state = GTK_CUPS_HTTP_IDLE;
     
          gtk_cups_result_set_error (request->result,
                                     GTK_CUPS_ERROR_HTTP,
                                     http_status,
                                     error, 
                                     "Unknown HTTP error");

          return;
        }
    }
  else if (http_status == HTTP_UPGRADE_REQUIRED)
    {
      /* Flush any error message... */
      httpFlush (request->http);

      cupsSetEncryption (HTTP_ENCRYPT_REQUIRED);
      request->state = GTK_CUPS_POST_CONNECT;

      /* Reconnect... */
      httpReconnect2 (request->http, 30000, NULL);

      /* Upgrade with encryption... */
      httpEncryption (request->http, HTTP_ENCRYPT_REQUIRED);
 
      request->attempts++;
      goto again;
    }
  else if (http_status != HTTP_OK)
    {
      int http_errno;

      http_errno = httpError (request->http);

      if (http_errno == EPIPE)
        request->state = GTK_CUPS_POST_CONNECT;
      else
        {
          request->state = GTK_CUPS_POST_DONE;
          gtk_cups_result_set_error (request->result,
                                     GTK_CUPS_ERROR_HTTP,
                                     http_status,
                                     http_errno, 
                                     "HTTP Error in POST %s", 
                                     g_strerror (http_errno));
          request->poll_state = GTK_CUPS_HTTP_IDLE;
 
          httpFlush (request->http); 
          return;
        }

      request->poll_state = GTK_CUPS_HTTP_IDLE;
      request->last_status = HTTP_CONTINUE;

      httpFlush (request->http);
      if (request->own_http)
        httpClose (request->http);
      request->http = NULL;

      return;
    }
  else
    {
      request->state = GTK_CUPS_POST_READ_RESPONSE;
      return;
    }

 again:
  http_status = HTTP_CONTINUE;

  if (httpCheck (request->http))
    http_status = httpUpdate (request->http);

  request->last_status = http_status;
}

static void 
_post_read_response (GtkCupsRequest *request)
{
  ipp_state_t ipp_status;

  GTK_DEBUG (PRINTING, "CUPS Backend: %s", G_STRFUNC);

  request->poll_state = GTK_CUPS_HTTP_READ;

  if (request->result->ipp_response == NULL)
    request->result->ipp_response = ippNew();

  ipp_status = ippRead (request->http, 
                        request->result->ipp_response);

  if (ipp_status == IPP_ERROR)
    {
      int ipp_error = cupsLastError ();
      gtk_cups_result_set_error (request->result,  
                                 GTK_CUPS_ERROR_IPP,
                                 ipp_status,
                                 ipp_error,
                                 "%s",
                                 ippErrorString (ipp_error));
      
      ippDelete (request->result->ipp_response);
      request->result->ipp_response = NULL;

      request->state = GTK_CUPS_POST_DONE;
      request->poll_state = GTK_CUPS_HTTP_IDLE;
    }
  else if (ipp_status == IPP_DATA)
    {
      request->state = GTK_CUPS_POST_DONE;
      request->poll_state = GTK_CUPS_HTTP_IDLE;
    }
}

static void 
_get_send (GtkCupsRequest *request)
{
  GTK_DEBUG (PRINTING, "CUPS Backend: %s", G_STRFUNC);

  request->poll_state = GTK_CUPS_HTTP_WRITE;

  if (request->data_io == NULL)
    {
      gtk_cups_result_set_error (request->result,
                                 GTK_CUPS_ERROR_IO,
                                 G_IO_STATUS_ERROR,
                                 G_IO_CHANNEL_ERROR_FAILED, 
                                 "Get requires an open io channel");

      request->state = GTK_CUPS_GET_DONE;
      request->poll_state = GTK_CUPS_HTTP_IDLE;

      return;
    }

  httpClearFields (request->http);
  httpSetField (request->http, HTTP_FIELD_AUTHORIZATION, httpGetAuthString (request->http));

  if (httpGet (request->http, request->resource))
    {
      int reconnect;

      reconnect = httpReconnect2 (request->http, 30000, NULL);
      if (reconnect)
        {
          request->state = GTK_CUPS_GET_DONE;
          request->poll_state = GTK_CUPS_HTTP_IDLE;
	 
          /* TODO: should add a status or error code for failed GET */ 
          gtk_cups_result_set_error (request->result, 
                                     GTK_CUPS_ERROR_GENERAL,
                                     0,
                                     0,
                                     "Failed Get");
        }

      request->attempts++;
      return;    
    }

  if (httpCheck (request->http))
    request->last_status = httpUpdate (request->http);
        
  request->attempts = 0;

  request->state = GTK_CUPS_GET_CHECK;
  request->poll_state = GTK_CUPS_HTTP_READ;
  
  ippSetState (request->ipp_request, IPP_IDLE);
}

static void 
_get_check (GtkCupsRequest *request)
{
  http_status_t http_status;

  GTK_DEBUG (PRINTING, "CUPS Backend: %s", G_STRFUNC);

  http_status = request->last_status;

  request->poll_state = GTK_CUPS_HTTP_READ;

  if (http_status == HTTP_CONTINUE)
    {
      goto again; 
    }
  else if (http_status == HTTP_UNAUTHORIZED)
    {
      int auth_result = -1;
      httpFlush (request->http);

      if (request->password_state == GTK_CUPS_PASSWORD_APPLIED)
        {
          request->poll_state = GTK_CUPS_HTTP_IDLE;
          request->password_state = GTK_CUPS_PASSWORD_NOT_VALID;
          request->state = GTK_CUPS_GET_AUTH;
          request->need_password = TRUE;

          return;
        }

      /* Negotiate */
      if (strncmp (httpGetField (request->http, HTTP_FIELD_WWW_AUTHENTICATE), "Negotiate", 9) == 0)
        {
          auth_result = cupsDoAuthentication (request->http, "GET", request->resource);
        }
      /* Basic, BasicDigest, Digest and PeerCred */
      else
        {
          if (request->password_state == GTK_CUPS_PASSWORD_NONE)
            {
              cups_username = request->username;
              cupsSetPasswordCB (passwordCB);

              /* This call success for PeerCred authentication */
              auth_result = cupsDoAuthentication (request->http, "GET", request->resource);

              if (auth_result != 0)
                {
                  /* move to AUTH state to let the backend
                   * ask for a password
                   */
                  request->poll_state = GTK_CUPS_HTTP_IDLE;
                  request->state = GTK_CUPS_GET_AUTH;
                  request->need_password = TRUE;

                  return;
                }
            }
          else
            {
              cups_password = request->password;
              cups_username = request->username;

              auth_result = cupsDoAuthentication (request->http, "GET", request->resource);

              if (cups_password != NULL)
                return;

              if (request->password != NULL)
                {
                  memset (request->password, 0, strlen (request->password));
                  g_free (request->password);
                  request->password = NULL;
                }

              request->password_state = GTK_CUPS_PASSWORD_APPLIED;
            }
        }

      if (auth_result ||
          httpReconnect2 (request->http, 30000, NULL))
        {
          /* if the password has been used, reset password_state
           * so that we ask for a new one next time around
           */
          if (cups_password == NULL)
            request->password_state = GTK_CUPS_PASSWORD_NONE;

          request->state = GTK_CUPS_GET_DONE;
          request->poll_state = GTK_CUPS_HTTP_IDLE;
          gtk_cups_result_set_error (request->result, 
                                     GTK_CUPS_ERROR_AUTH,
                                     0,
                                     0,
                                     "Not authorized");
          return;
        }

      request->state = GTK_CUPS_GET_CONNECT;
      request->last_status = HTTP_CONTINUE;

     return;
    }
  else if (http_status == HTTP_UPGRADE_REQUIRED)
    {
      /* Flush any error message... */
      httpFlush (request->http);

      cupsSetEncryption (HTTP_ENCRYPT_REQUIRED);
      request->state = GTK_CUPS_GET_CONNECT;

      /* Reconnect... */
      httpReconnect2 (request->http, 30000, NULL);

      /* Upgrade with encryption... */
      httpEncryption (request->http, HTTP_ENCRYPT_REQUIRED);
 
      request->attempts++;
      goto again;
    }
  else if (http_status != HTTP_OK)
    {
      int http_errno;

      http_errno = httpError (request->http);

      if (http_errno == EPIPE)
        request->state = GTK_CUPS_GET_CONNECT;
      else
        {
          request->state = GTK_CUPS_GET_DONE;
          gtk_cups_result_set_error (request->result,
                                     GTK_CUPS_ERROR_HTTP,
                                     http_status,
                                     http_errno, 
                                     "HTTP Error in GET %s", 
                                     g_strerror (http_errno));
          request->poll_state = GTK_CUPS_HTTP_IDLE;
          httpFlush (request->http);

          return;
        }

      request->poll_state = GTK_CUPS_HTTP_IDLE;
      request->last_status = HTTP_CONTINUE;

      httpFlush (request->http);
      if (request->own_http)
        httpClose (request->http);
      request->http = NULL;

      return;
    }
  else
    {
      request->state = GTK_CUPS_GET_READ_DATA;
      return;
    }

 again:
  http_status = HTTP_CONTINUE;

  if (httpCheck (request->http))
    http_status = httpUpdate (request->http);

  request->last_status = http_status;

}

static void 
_get_read_data (GtkCupsRequest *request)
{
  char buffer[_GTK_CUPS_MAX_CHUNK_SIZE];
  gsize bytes;
  gsize bytes_written;
  GIOStatus io_status;
  GError *error;

  GTK_DEBUG (PRINTING, "CUPS Backend: %s", G_STRFUNC);

  error = NULL;

  request->poll_state = GTK_CUPS_HTTP_READ;

  bytes = httpRead2 (request->http, buffer, sizeof (buffer));
  request->bytes_received += bytes;

  GTK_DEBUG (PRINTING, "CUPS Backend: %"G_GSIZE_FORMAT" bytes read", bytes);

  io_status =
    g_io_channel_write_chars (request->data_io, 
                              buffer, 
			      bytes, 
			      &bytes_written,
			      &error);

  if (io_status == G_IO_STATUS_ERROR)
    {
      request->state = GTK_CUPS_GET_DONE;
      request->poll_state = GTK_CUPS_HTTP_IDLE;

      gtk_cups_result_set_error (request->result,
                                 GTK_CUPS_ERROR_IO,
                                 io_status,
                                 error->code,
                                 "%s",
                                 error->message);
      g_error_free (error);
    }

  /* Stop if we do not expect any more data or EOF was received. */
  if (httpGetLength2 (request->http) <= request->bytes_received || bytes == 0)
    {
      request->state = GTK_CUPS_GET_DONE;
      request->poll_state = GTK_CUPS_HTTP_IDLE;

      return;
    }
}

gboolean
gtk_cups_request_is_done (GtkCupsRequest *request)
{
  return (request->state == GTK_CUPS_REQUEST_DONE);
}

gboolean
gtk_cups_result_is_error (GtkCupsResult *result)
{
  return result->is_error;
}

ipp_t *
gtk_cups_result_get_response (GtkCupsResult *result)
{
  return result->ipp_response;
}

GtkCupsErrorType
gtk_cups_result_get_error_type (GtkCupsResult *result)
{
  return result->error_type;
}

int
gtk_cups_result_get_error_status (GtkCupsResult *result)
{
  return result->error_status;
}

int
gtk_cups_result_get_error_code (GtkCupsResult *result)
{
  return result->error_code;
}

const char *
gtk_cups_result_get_error_string (GtkCupsResult *result)
{
  return result->error_msg; 
}

/* This function allocates new instance of GtkCupsConnectionTest() and creates
 * a socket for communication with a CUPS server 'server'.
 */
GtkCupsConnectionTest *
gtk_cups_connection_test_new (const char *server,
                              const int   port)
{
  GtkCupsConnectionTest *result = NULL;
  char                  *port_str = NULL;

  result = g_new (GtkCupsConnectionTest, 1);

  if (port >= 0)
    port_str = g_strdup_printf ("%d", port);
  else
    port_str = g_strdup_printf ("%d", ippPort ());

  if (server != NULL)
    result->addrlist = httpAddrGetList (server, AF_UNSPEC, port_str);
  else
    result->addrlist = httpAddrGetList (cupsServer (), AF_UNSPEC, port_str);

  g_free (port_str);

  result->socket = -1;
  result->current_addr = NULL;
  result->last_wrong_addr = NULL;
  result->at_init = GTK_CUPS_CONNECTION_NOT_AVAILABLE;

  result->at_init = gtk_cups_connection_test_get_state (result);

  return result;
}


/* A non-blocking test whether it is possible to connect to a CUPS server specified
 * inside of GtkCupsConnectionTest structure.
 *  - you need to check it more then once.
 * The connection is closed after a successful connection.
 */
GtkCupsConnectionState 
gtk_cups_connection_test_get_state (GtkCupsConnectionTest *test)
{
  GtkCupsConnectionState result = GTK_CUPS_CONNECTION_NOT_AVAILABLE;
  http_addrlist_t       *iter;
  int                    error_code;
  int                    flags;
  int                    code;

  if (test == NULL)
    return GTK_CUPS_CONNECTION_NOT_AVAILABLE;

  if (test->at_init == GTK_CUPS_CONNECTION_AVAILABLE)
    {
      test->at_init = GTK_CUPS_CONNECTION_NOT_AVAILABLE;
      return GTK_CUPS_CONNECTION_AVAILABLE;
    }
  else
    {
      if (test->socket == -1)
        {
          if (test->last_wrong_addr != NULL && test->last_wrong_addr->next != NULL)
            iter = test->last_wrong_addr->next;
          else
            {
              test->last_wrong_addr = NULL;
              iter = test->addrlist;
            }

          while (iter)
            {
              test->socket = socket (iter->addr.addr.sa_family,
                                     SOCK_STREAM,
                                     0);

              if (test->socket >= 0)
                {
                  flags = fcntl (test->socket, F_GETFL);

                  if (flags != -1)
                    flags |= O_NONBLOCK;

                  fcntl (test->socket, F_SETFL, flags);
              
                  test->current_addr = iter;
              
                  break;
                }
               iter = iter->next;
            }
        }

      if (test->socket >= 0)
        {
          code = connect (test->socket,
                          &test->current_addr->addr.addr,
                          httpAddrLength (&test->current_addr->addr));

          error_code = errno;

          if (code == 0 || error_code == EISCONN)
            {
              close (test->socket);
              test->socket = -1;
              test->current_addr = NULL;
              result = GTK_CUPS_CONNECTION_AVAILABLE;
            }
          else
            {
              if (error_code == EALREADY || error_code == EINPROGRESS)
                result = GTK_CUPS_CONNECTION_IN_PROGRESS;
              else
                {
                  close (test->socket);
                  test->socket = -1;
                  test->last_wrong_addr = test->current_addr;
                  result = GTK_CUPS_CONNECTION_NOT_AVAILABLE;
                }
            }
         }

      return result;
    }
}

/* This function frees memory used by the GtkCupsConnectionTest structure.
 */
void 
gtk_cups_connection_test_free (GtkCupsConnectionTest *test)
{
  if (test == NULL)
    return;

  test->current_addr = NULL;
  test->last_wrong_addr = NULL;
  httpAddrFreeList (test->addrlist);
  if (test->socket != -1)
    {
      close (test->socket);
      test->socket = -1;
    }
  g_free (test);
}
