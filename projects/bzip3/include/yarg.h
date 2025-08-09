/* Written by Kamila Szewczyk (kspalaiologos@gmail.com) */

#ifndef _YARG_H
#define _YARG_H

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef enum {
  no_argument,
  required_argument,
  optional_argument
} yarg_arg_type;

typedef struct {
  int opt;
  yarg_arg_type type;
  const char * long_opt;
} yarg_options;

typedef enum {
  YARG_STYLE_WINDOWS,
  YARG_STYLE_UNIX,
  YARG_STYLE_UNIX_SHORT
} yarg_style;

typedef struct {
  bool dash_dash;
  yarg_style style;
} yarg_settings;

typedef struct {
  int opt;
  const char * long_opt;
  char * arg;
} yarg_option;

typedef struct {
  yarg_option * args;
  int argc;
  char ** pos_args;
  int pos_argc;
  char * error;
} yarg_result;

static const char yarg_oom[] = "Out of memory";
static int yarg_asprintf(char ** strp, const char * fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int len = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (len < 0) {
    memcpy(*strp, yarg_oom, sizeof(yarg_oom));
    return sizeof(yarg_oom);
  }
  *strp = (char *) malloc(len + 1);
  if (!*strp) {
    memcpy(*strp, yarg_oom, sizeof(yarg_oom));
    return sizeof(yarg_oom);
  }
  va_start(ap, fmt);
  len = vsnprintf(*strp, len + 1, fmt, ap);
  va_end(ap);
  return len;
}

static char * yarg_strdup(const char * str) {
  char * new_str = (char *) calloc(strlen(str) + 1, 1);
  if (!new_str) return NULL;
  strcpy(new_str, str);
  return new_str;
}

static int yarg_parse_unix(int argc, char * argv[], yarg_options opt[],
                           yarg_result * res, bool dash_dash) {
  int no_args = 0, no_pos_args = 0;
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (argv[i][1] == '-') {
        if (dash_dash && argv[i][2] == '\0')
          { no_pos_args += argc - i - 1; break; }
        char * long_opt = argv[i] + 2; yarg_options * o = NULL;
        int len = 0; while (long_opt[len] && long_opt[len] != '=') len++;
        for (int j = 0; opt[j].opt; j++)
          if (opt[j].long_opt && !strncmp(opt[j].long_opt, long_opt, len))
            { o = &opt[j]; break; }
        if (!o) {
          yarg_asprintf(&res->error, "--%.*s -- unknown option\n", len, long_opt);
          return 0;
        }
        if (o->type == required_argument) {
          if (long_opt[len] == '=') {
            // Ignore.
          } else if (argv[i + 1] && argv[i + 1][0] != '-') {
            i++;
          } else {
            yarg_asprintf(&res->error, "--%s -- missing argument\n", o->long_opt);
            return 0;
          }
        } else if (o->type == optional_argument) {
          if (long_opt[len] == '=') {
          } else if (argv[i + 1] && argv[i + 1][0] != '-') {
            i++;
          }
        }
        no_args++;
      } else {
        for (int j = 1; argv[i][j]; j++) {
          char c = argv[i][j]; yarg_options * o = NULL;
          for (int k = 0; opt[k].opt; k++)
            if (opt[k].opt == c)
              { o = &opt[k]; break; }
          if (!o) {
            yarg_asprintf(&res->error, "-%c -- unknown option\n", c);
            return 0;
          }
          if (o->type == required_argument) {
            if (argv[i][j + 1]) {
              // Ignore.
            } else if (argv[i + 1] && argv[i + 1][0] != '-') {
              i++;
            } else {
              yarg_asprintf(&res->error, "-%c -- missing argument\n", c);
              return 0;
            }
            no_args++;
            break;
          } else if(o->type == optional_argument) {
            if (argv[i][j + 1]) {
              // Ignore.
              no_args++;
              break;
            } else if (argv[i + 1] && argv[i + 1][0] != '-') {
              i++;
              no_args++;
              break;
            }
          }
          no_args++;
        }
      }
    } else no_pos_args++;
  }

  res->args = (yarg_option *) calloc((no_args + 1) * sizeof(yarg_option), 1);
  res->pos_args = (char **) calloc((no_pos_args + 1) * sizeof(char *), 1);
  if(!res->args || !res->pos_args) {
    yarg_asprintf(&res->error, yarg_oom);
    return 0;
  }

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (argv[i][1] == '-') {
        if (dash_dash && argv[i][2] == '\0') {
          for (int j = i + 1; j < argc; j++)
            if(!(res->pos_args[res->pos_argc++] = yarg_strdup(argv[j]))) {
              yarg_asprintf(&res->error, yarg_oom);
              return 0;
            }
          break;
        }
        char * long_opt = argv[i] + 2; yarg_options * o = NULL;
        int len = 0; while (long_opt[len] && long_opt[len] != '=') len++;
        for (int j = 0; opt[j].opt; j++)
          if (opt[j].long_opt && !strncmp(opt[j].long_opt, long_opt, len))
            { o = &opt[j]; break; }
        res->args[res->argc].opt = o->opt;
        res->args[res->argc].long_opt = o->long_opt;
        if (o->type == required_argument || o->type == optional_argument) {
          if (long_opt[len] == '=') {
            if(!(res->args[res->argc].arg = yarg_strdup(long_opt + len + 1))) {
              yarg_asprintf(&res->error, yarg_oom);
              return 0;
            }
          } else if (argv[i + 1] && argv[i + 1][0] != '-') {
            if(!(res->args[res->argc].arg = yarg_strdup(argv[++i]))) {
              yarg_asprintf(&res->error, yarg_oom);
              return 0;
            }
          }
        }
        res->argc++;
      } else {
        for (int j = 1; argv[i][j]; j++) {
          char c = argv[i][j]; yarg_options * o = NULL;
          for (int k = 0; opt[k].opt; k++)
            if (opt[k].opt == c)
              { o = &opt[k]; break; }
          if (!o) {
            yarg_asprintf(&res->error, "-%c -- unknown option\n", c);
            return 0;
          }
          res->args[res->argc].opt = c;
          res->args[res->argc].long_opt = o->long_opt;
          if (o->type == required_argument || o->type == optional_argument) {
            if (argv[i][j + 1]) {
              if(!(res->args[res->argc++].arg = yarg_strdup(argv[i] + j + 1))) {
                yarg_asprintf(&res->error, yarg_oom);
                return 0;
              }
              break;
            } else if (argv[i + 1] && argv[i + 1][0] != '-') {
              if(!(res->args[res->argc++].arg = yarg_strdup(argv[++i]))) {
                yarg_asprintf(&res->error, yarg_oom);
                return 0;
              }
              break;
            }
          }
          res->argc++;
        }
      }
    } else if(!(res->pos_args[res->pos_argc++] = yarg_strdup(argv[i]))) {
      yarg_asprintf(&res->error, yarg_oom);
      return 0;
    }
  }

  return 1;
}

static int yarg_parse_unix_short(int argc, char * argv[], yarg_options opt[],
                                 yarg_result * res, bool dash_dash, char opt_char) {
  int no_args = 0, no_pos_args = 0;
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == opt_char) {
      if (dash_dash && argv[i][1] == '\0') {
        no_pos_args += argc - i - 1;
        break;
      }
      char * long_opt = argv[i] + 1; yarg_options * o = NULL;
      int len = 0; while (long_opt[len] && long_opt[len] != '=') len++;
      for (int j = 0; opt[j].opt; j++)
        if (opt[j].long_opt && !strncmp(opt[j].long_opt, long_opt, len))
          { o = &opt[j]; break; }
      if (!o) {
        yarg_asprintf(&res->error, "%c%.*s -- unknown option\n", opt_char, len, long_opt);
        return 0;
      }
      if (o->type == required_argument) {
        if (long_opt[len] == '=') {
          // Ignore.
        } else if (argv[i + 1] && argv[i + 1][0] != opt_char) {
          i++;
        } else {
          yarg_asprintf(&res->error, "%c%s -- missing argument\n", opt_char, o->long_opt);
          return 0;
        }
      } else if (o->type == optional_argument) {
        if (long_opt[len] == '=') {
          // Ignore.
        } else if (argv[i + 1] && argv[i + 1][0] != opt_char) {
          i++;
        }
      }
      no_args++;
    } else no_pos_args++;
  }
  
  res->args = (yarg_option *) calloc((no_args + 1) * sizeof(yarg_option), 1);
  res->pos_args = (char **) calloc((no_pos_args + 1) * sizeof(char *), 1);
  if (!res->args || !res->pos_args) {
    yarg_asprintf(&res->error, yarg_oom);
    return 0;
  }

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == opt_char) {
      if (dash_dash && argv[i][1] == '\0') {
        for (int j = i + 1; j < argc; j++)
          if(!(res->pos_args[res->pos_argc++] = yarg_strdup(argv[j]))) {
            yarg_asprintf(&res->error, yarg_oom);
            return 0;
          }
        break;
      }
      char * long_opt = argv[i] + 1; yarg_options * o = NULL;
      int len = 0; while (long_opt[len] && long_opt[len] != '=') len++;
      for (int j = 0; opt[j].opt; j++)
        if (opt[j].long_opt && !strncmp(opt[j].long_opt, long_opt, len))
          { o = &opt[j]; break; }
      res->args[res->argc].opt = o->opt;
      res->args[res->argc].long_opt = o->long_opt;
      if (o->type == required_argument || o->type == optional_argument) {
        if (long_opt[len] == '=') {
          if(!(res->args[res->argc].arg = yarg_strdup(long_opt + len + 1))) {
            yarg_asprintf(&res->error, yarg_oom);
            return 0;
          }
        } else if (argv[i + 1] && argv[i + 1][0] != opt_char) {
          if(!(res->args[res->argc].arg = yarg_strdup(argv[++i]))) {
            yarg_asprintf(&res->error, yarg_oom);
            return 0;
          }
        }
      }
      res->argc++;
    } else if(!(res->pos_args[res->pos_argc++] = yarg_strdup(argv[i]))) {
      yarg_asprintf(&res->error, yarg_oom);
      return 0;
    }
  }

  return 1;
}

void yarg_destroy(yarg_result * r) {
  if(r) {
    if(r->args) {
      for (int i = 0; i < r->argc; i++) {
        free(r->args[i].arg);
      }
    }
    free(r->args);
    if(r->pos_args) {
      for (int i = 0; i < r->pos_argc; i++) {
        free(r->pos_args[i]);
      }
    }
    free(r->pos_args);
    if (r->error != yarg_oom)
      free(r->error);
  }
  free(r);
}

yarg_result * yarg_parse(int argc, char * argv[], yarg_options opt[], yarg_settings settings) {
  yarg_result * res = (yarg_result *) calloc(sizeof(yarg_result), 1);
  if (!res) return NULL;
  switch (settings.style) {
    case YARG_STYLE_WINDOWS:
      yarg_parse_unix_short(argc, argv, opt, res, false, '/');
      break;
    case YARG_STYLE_UNIX:
      yarg_parse_unix(argc, argv, opt, res, settings.dash_dash);
      break;
    case YARG_STYLE_UNIX_SHORT:
      yarg_parse_unix_short(argc, argv, opt, res, settings.dash_dash, '-');
      break;
  }
  return res;
}

#endif
