#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>


static void prepare (void);
#define PREPARE(argc, argv) prepare ()

static int do_test (void);
#define TEST_FUNCTION do_test ()

#include "../test-skeleton.c"

static int dir_fd;

static void
prepare (void)
{
  size_t test_dir_len = strlen (test_dir);
  static const char dir_name[] = "/tst-mkdirat.XXXXXX";

  size_t dirbuflen = test_dir_len + sizeof (dir_name);
  char *dirbuf = malloc (dirbuflen);
  if (dirbuf == NULL)
    {
      puts ("out of memory");
      exit (1);
    }

  snprintf (dirbuf, dirbuflen, "%s%s", test_dir, dir_name);
  if (mkdtemp (dirbuf) == NULL)
    {
      puts ("cannot create temporary directory");
      exit (1);
    }

  add_temp_file (dirbuf);

  dir_fd = open (dirbuf, O_RDONLY | O_DIRECTORY);
  if (dir_fd == -1)
    {
      puts ("cannot open directory");
      exit (1);
    }
}


static int
do_test (void)
{
  /* Find the current umask.  */
  mode_t mask = umask (022);
  umask (mask);

  /* fdopendir takes over the descriptor, make a copy.  */
  int dupfd = dup (dir_fd);
  if (dupfd == -1)
    {
      puts ("dup failed");
      return 1;
    }
  if (lseek (dupfd, 0, SEEK_SET) != 0)
    {
      puts ("1st lseek failed");
      return 1;
    }

  /* The directory should be empty safe the . and .. files.  */
  DIR *dir = fdopendir (dupfd);
  if (dir == NULL)
    {
      puts ("fdopendir failed");
      return 1;
    }
  struct dirent64 *d;
  while ((d = readdir64 (dir)) != NULL)
    if (strcmp (d->d_name, ".") != 0 && strcmp (d->d_name, "..") != 0)
      {
	printf ("temp directory contains file \"%s\"\n", d->d_name);
	return 1;
      }
  closedir (dir);

  /* Create a new directory.  */
  int e = mkdirat (dir_fd, "some-dir", 0777);
  if (e == -1)
    {
      if (errno == ENOSYS)
	{
	  puts ("*at functions not supported");
	  return 0;
	}

      puts ("directory creation failed");
      return 1;
    }

  struct stat64 st1;
  if (fstatat64 (dir_fd, "some-dir", &st1, 0) != 0)
    {
      puts ("fstat64 failed");
      return 1;
    }
  if (!S_ISDIR (st1.st_mode))
    {
      puts ("mkdirat did not create a directory");
      return 1;
    }
  if ((st1.st_mode & 01777) != (~mask & 0777))
    {
      printf ("mkdirat created directory with wrong mode %o, expected %o\n",
	      (unsigned int) (st1.st_mode & 01777),
	      (unsigned int) (~mask & 0777));
      return 1;
    }

  dupfd = dup (dir_fd);
  if (dupfd == -1)
    {
      puts ("dup failed");
      return 1;
    }
  if (lseek (dupfd, 0, SEEK_SET) != 0)
    {
      puts ("1st lseek failed");
      return 1;
    }

  dir = fdopendir (dupfd);
  if (dir == NULL)
    {
      puts ("2nd fdopendir failed");
      return 1;
    }
  bool has_some_dir = false;
  while ((d = readdir64 (dir)) != NULL)
    if (strcmp (d->d_name, "some-dir") == 0)
      {
	has_some_dir = true;
	if (d->d_type != DT_UNKNOWN && d->d_type != DT_DIR)
	  {
	    puts ("d_type for some-dir wrong");
	    return 1;
	  }
      }
    else if (strcmp (d->d_name, ".") != 0 && strcmp (d->d_name, "..") != 0)
      {
	printf ("temp directory contains file \"%s\"\n", d->d_name);
	return 1;
      }
  closedir (dir);

  if (!has_some_dir)
    {
      puts ("some-dir not in directory list");
      return 1;
    }

  if (unlinkat (dir_fd, "some-dir", AT_REMOVEDIR) != 0)
    {
      puts ("unlinkat failed");
      return 1;
    }

  /* Test again with a different mode.  */
  umask (0);
  e = mkdirat (dir_fd, "some-dir", 01755);
  umask (mask);
  if (e == -1)
    {
      puts ("directory creation (different mode) failed");
      return 1;
    }
  if (fstatat64 (dir_fd, "some-dir", &st1, 0) != 0)
    {
      puts ("fstat64 (different mode) failed");
      return 1;
    }
  if (!S_ISDIR (st1.st_mode))
    {
      puts ("mkdirat (different mode) did not create a directory");
      return 1;
    }
  if ((st1.st_mode & 01777) != 01755)
    {
      printf ("mkdirat (different mode) created directory with wrong mode %o\n",
	      (unsigned int) (st1.st_mode & 01777));
      return 1;
    }
  if (unlinkat (dir_fd, "some-dir", AT_REMOVEDIR) != 0)
    {
      puts ("unlinkat (different mode) failed");
      return 1;
    }

  close (dir_fd);

  return 0;
}
