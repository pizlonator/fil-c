#include "config.h"

#include <fcntl.h>
#include <libelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

int main (int argc, const char **argv)
{
  int fd;
  Elf *elf;
  size_t phnum;

  if (argc != 2)
    {
      fprintf (stderr, "usage: %s FILE\n", argv[0]);
      return EXIT_FAILURE;
    }

  fd = open (argv[1], O_RDONLY);
  if (fd == -1)
    {
      perror ("open");
      return EXIT_FAILURE;
    }
  elf_version (EV_CURRENT);
  elf = elf_begin (fd, ELF_C_READ, NULL);
  if (!elf)
    {
      fprintf (stderr, "elf_begin: %s\n", elf_errmsg (-1));
      return EXIT_FAILURE;
    }
  if (elf_getphdrnum (elf, &phnum))
    {
      fprintf(stderr, "elf_getphdrnum: %s\n", elf_errmsg (-1));
      return EXIT_FAILURE;
    }

  printf("%zu\n", phnum);

  elf_end (elf);
  close (fd);

  return EXIT_SUCCESS;
}
