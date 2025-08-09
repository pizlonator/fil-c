/* httpd.c - Web server.
 *
 * Copyright 2022 Rob Landley <rob@landley.net>
 *
 * See https://www.ietf.org/rfc/rfc2616.txt
 *
 * TODO: multiple domains, https, actual inetd with ratelimit...
 * range, gzip, ETag (If-None-Match:, Last-Modified:), Date:
 * "Accept-Ranges: bytes"/"Range: bytes=xxx-[yyy]"
 * .htaccess (auth, forward)
 * optional conf file, error pages
 * -ifv -p [IP:]PORT -u [USER][:GRP] -c CFGFILE
 * cgi: SERVER_PORT SERVER_NAME REMOTE_ADDR REMOTE_HOST REQUEST_METHOD

USE_HTTPD(NEWTOY(httpd, ">1v", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_LINEBUF))

config HTTPD
  bool "httpd"
  default y
  help
    usage: httpd [-de STR] [-v] [DIR]

    Serve contents of directory as static web pages.

    -e	Escape STR as URL, printing result and exiting.
    -d	Decode escaped STR, printing result and exiting.
    -v	Verbose
*/

#define FOR_httpd
#include "toys.h"

char *rfc1123(char *buf, time_t t)
{
  strftime(buf, 64, "%a, %d %b %Y %T GMT", gmtime(&t));

  return buf;
}

// She never told me...
char *mime(char *file)
{
  char *s = strrchr(file, '.'), *types[] = {
    "asc\0text/plain", "bin\0application/octet-stream", "bmp\0image/bmp",
    "cpio\0application/x-cpio", "css\0text/css", "doc\0application/msword",
    "dtd\0text/xml", "dvi\0application/x-dvi", "gif\0image/gif",
    "htm\0text/html", "html\0text/html", "jar\0applicat/x-java-archive",
    "jpeg\0image/jpeg", "jpg\0image/jpeg", "js\0application/x-javascript",
    "mp3\0audio/mpeg", "mp4\0video/mp4", "mpg\0video/mpeg",
    "ogg\0application/ogg", "pbm\0image/x-portable-bitmap",
    "pdf\0application/pdf", "png\0image/png",
    "ppt\0application/vnd.ms-powerpoint", "ps\0application/postscript",
    "rtf\0text/rtf", "sgml\0text/sgml", "svg\0image/svg+xml",
    "tar\0application/x-tar", "tex\0application/x-tex", "tiff\0image/tiff",
    "txt\0text/plain", "wav\0audio/x-wav", "xls\0application/vnd.ms-excel",
    "xml\0tet/xml", "zip\0application/zip"
  };
  int i;

  strcpy(toybuf, "text/plain");
  if (s++) for (i = 0; i<ARRAY_LEN(types); i++) {
    if (strcasecmp(s, types[i])) continue;
    strcpy(toybuf, types[i]+strlen(types[i])+1);
    break;
  }
  if (!strncmp(toybuf, "text/", 5)) strcat(toybuf, "; charset=UTF-8");

  return toybuf;
}

// Stop: header time.
static void header_time(int stat, char *str, char *more)
{
  char buf[64];

  if (!more) more = "";
  if (FLAG(v)) dprintf(2, "REPLY: %d %s\n%s\n", stat, str, more);
  xprintf("HTTP/1.1 %d %s\r\nServer: toybox httpd/%s\r\nDate: %s\r\n%s"
    "Connection: close\r\n\r\n", stat, str, TOYBOX_VERSION,
    rfc1123(buf, time(0)), more);
}

static void error_time(int stat, char *str)
{
  header_time(stat, str, 0);
  xprintf("<html><head><title>%d %s</title></head>"
    "<body><h3>%d %s</h3></body></html>", stat, str, stat, str);
}

static int isunder(char *file, char *dir)
{
  char *s1 = xabspath(dir, ABS_FILE), *s2 = xabspath(file, 0), *ss = s2;
  int rc = s1 && s2 && strstart(&ss, s1) && (!*ss || *ss=='/' || ss[-1]=='/');

  free(s2);
  free(s1);

  return rc;
}

// Handle a connection on fd
void handle(int infd, int outfd)
{
  FILE *fp = fdopen(infd, "r");
  char *s = xgetline(fp), *cut, *ss, *esc, *path, *word[3];
  int i = sizeof(toybuf), fd;

  if (!s) return;

  if (!getsockname(0, (void *)&toybuf, &i)) {
    if (FLAG(v))
      dprintf(2, "Hello %s\n%s\n", ntop((void *)toybuf), s);
  }

  // Split line into method/path/protocol
  for (i = 0, ss = s;;) {
    word[i++] = ss;
    while (*ss && !strchr(" \r\n", *ss)) ss++;
    while (*ss && strchr(" \r\n", *ss)) *(ss++) = 0;
    if (i==3) break;
    if (!*ss) return header_time(400, "Bad Request", 0);
  }

  // Process additional http/1.1 lines
  while ((ss = xgetline(fp))) {
    i = *chomp(ss);
    if (FLAG(v)) dprintf(2, "%s\n", ss);
// TODO: any of
//User-Agent: Wget/1.20.1 (linux-gnu) - do we want to log anything?
//Accept: */* - 406 Too Snobbish
//Accept-Encoding: identity - we can gzip?
//Host: landley.net  - we could handle multiple domains?
//Connection: Keep-Alive - probably don't care

    free(ss);
    if (!i) break;
  }

  if (!strcasecmp(word[0], "get")) {
    struct stat st;

    if (*(ss = word[1])!='/') error_time(400, "Bad Request");
    while (*ss=='/') ss++;
    if (!*ss) ss = "./";
    else if ((cut = unescape_url(ss, 1))) setenv("QUERY_STRING", cut, 1);

    // TODO domain.com:/path/to/blah domain2.com:/path/to/that
    // TODO cgi PATH_INFO /path/to/filename.cgi/and/more/stuff?path&info
    if (!isunder(ss, ".") || stat(ss, &st)) error_time(404, "Not Found");
    else if (-1 == (fd = open(ss, O_RDONLY))) error_time(403, "Forbidden");
    else if (!S_ISDIR(st.st_mode)) {
      char buf[64];
file:
      header_time(200, "Ok", ss = xmprintf("Content-Type: %s\r\n"
        "Content-Length: %lld\r\nLast-Modified: %s\r\n",
        mime(ss), (long long)st.st_size, rfc1123(buf, st.st_mtime)));
      free(ss);
      xsendfile(fd, outfd);
    } else if (ss[strlen(ss)-1]!='/') {
      header_time(302, "Found", path = xmprintf("Location: %s/\r\n", word[1]));
      free(path);
    } else {
      DIR *dd;
      struct dirent *dir;

      // Do we have an index.html?
      path = ss;
      ss = "index.html";
      path = xmprintf("%s%s", path, ss);
      if (stat(path, &st) || !S_ISREG(st.st_mode)) i = -1;
      else if (-1 == (i = open(path, O_RDONLY))) error_time(403, "Forbidden");
      free(path);
      if (i != -1) {
        close(fd);
        fd = i;

        goto file;
      }

      // List directory contents
      header_time(200, "Ok", "Content-Type: text/html\r\n");
      dprintf(outfd, "<html><head><title>Index of %s</title></head>\n"
        "<body><h3>Index of %s</h3></body>\n", word[1], word[1]);
      for (dd = fdopendir(fd); (dir = readdir(dd));) {
        esc = escape_url(dir->d_name, "<>&\"");
        dprintf(outfd, "<a href=\"%s\">%s</a><br />\n", esc, esc);
        free(esc);
      }
      dprintf(outfd, "</body></html>\n");
    }
  } else error_time(501, "Not Implemented");
  free(s);
}

void httpd_main(void)
{
  if (toys.optc && chdir(*toys.optargs))
    return error_time(500, "Internal Error");
  // inetd only at the moment
  handle(0, 1);
}
