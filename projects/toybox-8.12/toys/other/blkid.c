/* blkid.c - Prints type, label and UUID of filesystem(s).
 *
 * Copyright 2013 Brad Conroy <bconroy@uis.edu>
 *
 * See ftp://ftp.kernel.org/pub/linux/utils/util-linux/v2.24/libblkid-docs/api-index-full.html
 * TODO: -U and -L should require arguments

USE_BLKID(NEWTOY(blkid, "ULo:s*[!LU]", TOYFLAG_BIN|TOYFLAG_LINEBUF))
USE_FSTYPE(NEWTOY(fstype, "<1", TOYFLAG_BIN|TOYFLAG_LINEBUF))

config BLKID
  bool "blkid"
  default y
  help
    usage: blkid [-o TYPE] [-s TAG] [-UL] DEV...

    Print type, label and UUID of filesystem on a block device or image.

    -U	Show UUID only (or device with that UUID)
    -L	Show LABEL only (or device with that LABEL)
    -o TYPE	Output format (full, value, export)
    -s TAG	Only show matching tags (default all)

config FSTYPE
  bool "fstype"
  default y
  help
    usage: fstype DEV...

    Print type of filesystem on a block device or image.
*/

#define FOR_blkid
#include "toys.h"

GLOBALS(
  struct arg_list *s;
  char *o;
)

struct fstype {
  char *name;
  uint64_t magic;
  int magic_len, magic_offset, uuid_off, label_len, label_off;
} static const fstypes[] = {
  {"ext2", 0xEF53, 2, 1080, 1128, 16, 1144}, // keep this first for ext3/4 check
  {"swap", 0x4341505350415753LL, 8, 4086, 1036, 15, 1052},
  // NTFS label actually 8/16 0x4d80 but horrible: 16 bit wide characters via
  // codepage, something called a uuid that's only 8 bytes long...
  {"ntfs", 0x5346544e, 4, 3, 0x48, 0, 0},

  {"adfs", 0xadf5, 2, 0xc00, 0,0,0},
  {"bfs", 0x1badface, 4, 0, 0,0,0},
  {"btrfs", 0x4D5F53665248425FULL, 8, 65600, 65803, 256, 65819},
  {"cramfs", 0x28cd3d45, 4, 0, 0, 16, 48},
  {"f2fs", 0xF2F52010, 4, 1024, 1132, 512, 0x47c},
  {"iso9660", 0x444301, 3, 0x8000, 0x832d, 32, 0x8028},
  {"jfs", 0x3153464a, 4, 32768, 32920, 16, 32904},
  {"nilfs", 0x3434, 2, 1030, 1176, 80, 1192},
  {"reiserfs", 0x724573496552ULL, 6, 8244, 8276, 16, 8292},
  {"reiserfs", 0x724573496552ULL, 6, 65588, 65620, 16, 65636},
  {"romfs", 0x2d6d6f72, 4, 0, 0,0,0},
  {"squashfs", 0x73717368, 4, 0, 0,0,0},
  {"xiafs", 0x012fd16d, 4, 572, 0,0,0},
  {"xfs", 0x42534658, 4, 0, 32, 12, 108},
  {"vfat", 0x3233544146ULL, 5, 82, 67, 11, 71},  // fat32
  {"vfat", 0x31544146, 4, 54, 39, 11, 43}     // fat1
};

static void escape(char *str, int force)
{
  if (!force && str[strcspn(str, "\" \\\n\t$<>|&;`'~()!#?")]) force++;
  if (!force) return xputsn(str);

  putchar('"');
  while (*str) {
    if (strchr("\" \\", *str)) putchar('\\');
    putchar(*str++);
  }
  putchar('"');
}

static void show_tag(char *key, char *value)
{
  int show = 0;
  struct arg_list *al;

  if (TT.s) {
    for (al = TT.s; al; al = al->next) if (!strcmp(key, al->arg)) show = 1;
  } else show = 1;

  if (!show || !*value) return;
  if (!strcasecmp(TT.o, "full") || !strcasecmp(TT.o, "export")) {
    printf(" %s="+!(*TT.o=='f'), key);
    escape(value, *TT.o=='f');
    if (*TT.o=='e') xputc('\n');
  } else if (!strcasecmp(TT.o, "value")) xputs(value);
  else error_exit("bad -o %s", TT.o);
}

static void flagshow(char *s, char *name)
{
  if (*toys.optargs && strcmp(s, *toys.optargs)) return;
  printf("%s\n", *toys.optargs ? name : s);
  if (*toys.optargs) xexit();
}

static void do_blkid(int fd, char *name)
{
  int off, i, j, len;
  char buf[128], *type, *s;

  off = i = 0;

  for (;;) {
    int pass = 0;

    // Read next block of data
    len = readall(fd, toybuf, sizeof(toybuf));
    if (len != sizeof(toybuf)) return;

    // Iterate through types in range
    for (i=0; i<ARRAY_LEN(fstypes); i++) {
      uint64_t test;

      // Skip tests not in this 4k block
      if (fstypes[i].magic_offset + fstypes[i].magic_len > off+sizeof(toybuf)) {
        pass++;
        continue;
      }
      if (fstypes[i].magic_offset < off) continue;

      // Populate 64 bit little endian magic value
      test = 0;
      for (j = 0; j < fstypes[i].magic_len; j++)
        test += ((uint64_t)toybuf[j+fstypes[i].magic_offset-off])<<(8*j);
      if (test == fstypes[i].magic) break;
    }

    if (i == ARRAY_LEN(fstypes)) {
      off += len;
      if (pass) continue;
      return;
    }
    break;
  }

  // distinguish ext2/3/4
  type = fstypes[i].name;
  if (!i) {
    if (toybuf[1116]&4) type = "ext3";
    if (toybuf[1120]&64) type = "ext4";
  }

  // Output for fstype
  if (*toys.which->name == 'f') {
    puts(type);
    return;
  }

  // output for blkid
  if (!FLAG(L) && !FLAG(U)) {
    if (!TT.o || !strcasecmp(TT.o, "full")) printf("%s:", name);
    else if (!strcasecmp(TT.o, "export")) show_tag("DEVNAME", name);
  }

  len = fstypes[i].label_len;
  if (!FLAG(U) && len) {
    s = toybuf+fstypes[i].label_off-off;
    if (!strcmp(type, "vfat") || !strcmp(type, "iso9660")) {
      if (*type=='v' && fstypes[i].magic_len==4 && !FLAG(L))
        show_tag("SEC_TYPE", "msdos");
      while (len && s[len-1]==' ') len--;
      if (strstart(&s, "NO NAME")) len=0;
    }
    // TODO: special case NTFS $VOLUME_NAME here...
    if (len) {
      if (!strcmp(type, "f2fs")) {
        // Convert UTF16LE to ASCII by replacing non-ASCII with '?'.
        // TODO: support non-ASCII.
        for (j=0; j<len; j++) {
          buf[j] = s[2*j];
          if (s[2*j+1]) buf[j]='?';
          if (!buf[j]) break;
        }
      } else sprintf(buf, "%.*s", len, s);
      if (FLAG(L)) return flagshow(buf, name);
      show_tag("LABEL", buf);
    }
  }

  len = fstypes[i].uuid_off;
  if (!FLAG(L) && len) {
    int uoff = len-off;

    // Assemble UUID with whatever size and set of dashes this filesystem uses
    s = buf;
    if (!strcmp(type, "ntfs")) {
      for (j = 7; j >= 0; --j) s += sprintf(s, "%02X", toybuf[uoff+j]);
    } else if (!strcmp(type, "vfat")) {
      s += sprintf(s, "%02X%02X-%02X%02X", toybuf[uoff+3], toybuf[uoff+2],
                   toybuf[uoff+1], toybuf[uoff]);
    } else if (!strcmp(type, "iso9660")) {
      s = stpncpy(s, toybuf+uoff, 4);
      for (i = 0, uoff += 4; i<12; i++) {
        if (!(i&1)) *s++ = '-';
        *s++ = toybuf[uoff++];
      }
      *s = 0;
    } else {
      for (j = 0; j < 16; j++)
        s += sprintf(s, "-%02x"+!(0x550 & (1<<j)), toybuf[uoff+j]);
    }

    if (FLAG(U)) return flagshow(buf, name);
    show_tag("UUID", buf);
  }

  if ((!strcmp(type, "ext3")||!strcmp(type,"ext4")) && !(toybuf[1120]&~0x12))
    show_tag("SEC_TYPE", "ext2");

  if (FLAG(U) || FLAG(L)) return;

  show_tag("TYPE", type);
  if (!strcasecmp(TT.o, "full")) xputc('\n');
}

void blkid_main(void)
{
  if (!TT.o) TT.o = "full";

  if (*toys.optargs && !FLAG(L) && !FLAG(U)) loopfiles(toys.optargs, do_blkid);
  else {
    unsigned int ma, mi, sz, fd;
    char name[32], device[5+32];
    FILE *fp = xfopen("/proc/partitions", "r");

    while (fgets(toybuf, sizeof(toybuf), fp)) {
      if (sscanf(toybuf, " %u %u %u %31s", &ma, &mi, &sz, name) != 4)
        continue;

      sprintf(device, "/dev/%.20s", name);
      if (-1 == (fd = open(device, O_RDONLY))) {
        if (errno != ENOMEDIUM) perror_msg_raw(device);
      } else {
        do_blkid(fd, device);
        close(fd);
      }
    }
    if (CFG_TOYBOX_FREE) fclose(fp);
  }

  if (FLAG(L) || FLAG(U)) toys.exitval = 2;
}

void fstype_main(void)
{
  loopfiles(toys.optargs, do_blkid);
}
