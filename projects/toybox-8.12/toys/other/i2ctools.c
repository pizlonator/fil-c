/* i2ctools.c - i2c tools
 *
 * Copyright 2018 The Android Open Source Project
 *
 * https://www.kernel.org/doc/Documentation/i2c/
 *
 * Note: -y must have the same value in each toy for `confirm`.
 *
 * TODO: i2cdetect -q/-r and the "auto" mode?
 * TODO: i2cdump non-byte modes, -r FIRST-LAST?
 * TODO: i2cget non-byte modes? default to current read address?
 * TODO: i2cset -r? -m MASK? c/s modes, p mode modifier?
 * TODO: I2C_M_TEN bit addressing (-t, larger range in probe...)

// note: confirm() needs "y" to be in same place for all commands
USE_I2CDETECT(NEWTOY(i2cdetect, ">3aF#<0>63lqry[!qr][!Fl]", TOYFLAG_USR|TOYFLAG_SBIN))
USE_I2CDUMP(NEWTOY(i2cdump, "<2>2fy", TOYFLAG_USR|TOYFLAG_SBIN))
USE_I2CGET(NEWTOY(i2cget, "<2>3fy", TOYFLAG_USR|TOYFLAG_SBIN))
USE_I2CSET(NEWTOY(i2cset, "<4fy", TOYFLAG_USR|TOYFLAG_SBIN))
USE_I2CTRANSFER(NEWTOY(i2ctransfer, "<2vfy", TOYFLAG_USR|TOYFLAG_SBIN))

config I2CDETECT
  bool "i2cdetect"
  default y
  help
    usage: i2cdetect [-aqry] BUS [FIRST LAST]
    usage: i2cdetect -F BUS
    usage: i2cdetect -l

    Detect i2c devices.

    -a	All addresses (0x00-0x7f rather than 0x03-0x77 or FIRST-LAST)
    -F	Show functionality
    -l	List available buses
    -q	Probe with SMBus Quick Write (default)
    -r	Probe with SMBus Read Byte
    -y	Skip confirmation prompts (yes to all)

config I2CDUMP
  bool "i2cdump"
  default y
  help
    usage: i2cdump [-fy] BUS CHIP

    Dump i2c registers.

    -f	Force access to busy devices
    -y	Skip confirmation prompts (yes to all)

config I2CGET
  bool "i2cget"
  default y
  help
    usage: i2cget [-fy] BUS CHIP [ADDR]

    Read an i2c register.

    -f	Force access to busy devices
    -y	Skip confirmation prompts (yes to all)

config I2CSET
  bool "i2cset"
  default y
  help
    usage: i2cset [-fy] BUS CHIP ADDR VALUE... MODE

    Write an i2c register. MODE is b for byte, w for 16-bit word, i for I2C block.

    -f	Force access to busy devices
    -y	Skip confirmation prompts (yes to all)

config I2CTRANSFER
  bool "i2ctransfer"
  default y
  help
    usage: i2ctransfer [-fy] BUS DESC [DATA...]...

    Make i2c transfers. DESC is 'r' for read or 'w' for write, followed by
    the number of bytes to read or write, followed by '@' and a 7-bit address.
    For any message after the first, the '@' and address can be omitted to
    reuse the previous address. A 'w' DESC must be followed by the number of
    DATA bytes that was specified in the DESC.

    -f	Force access to busy devices
    -v	Verbose (show messages sent, not just received)
    -y	Skip confirmation prompts (yes to all)
*/

#define FOR_i2cdetect
#define FORCE_FLAGS
#include "toys.h"

GLOBALS(
  long F;
)

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

printf_format static void confirm(const char *fmt, ...)
{
  va_list va;

  if (FLAG(y)) return;

  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
  if (!yesno(1)) error_exit("Exiting");
}

static int i2c_open(int bus, int slave, long chip)
{
  int fd;

  sprintf(toybuf, "/dev/i2c-%d", bus);
  fd = xopen(toybuf, O_RDONLY);
  if (slave) xioctl(fd, slave, (void *)chip);

  return fd;
}

static unsigned long i2c_get_funcs(int bus)
{
  int fd = i2c_open(bus, 0, 0);
  unsigned long result;

  xioctl(fd, I2C_FUNCS, &result);
  close(fd);

  return result;
}

static int i2c_read_byte(int fd, int addr, char *byte)
{
  union i2c_smbus_data data;
  struct i2c_smbus_ioctl_data ioctl_data = { .read_write = I2C_SMBUS_READ,
    .size = I2C_SMBUS_BYTE_DATA, .command = addr, .data = &data };

  memset(&data, 0, sizeof(data));
  if (ioctl(fd, I2C_SMBUS, &ioctl_data)==-1) return -1;
  *byte = data.byte;

  return 0;
}

static int i2c_quick_write(int fd, int addr)
{
  struct i2c_smbus_ioctl_data ioctl_data = { .read_write = I2C_SMBUS_QUICK,
    .command = addr };

  return ioctl(fd, I2C_SMBUS, &ioctl_data);
}

static int i2cdetect_dash_l(struct dirtree *node)
{
  char *suffix = "/name", *fname, *p;
  int suffix_len = strlen(suffix), bus;
  unsigned long funcs;

  if (!node->parent) return DIRTREE_RECURSE; // Skip the directory itself.

  if (sscanf(node->name, "i2c-%d", &bus)!=1) return 0;
  funcs = i2c_get_funcs(bus) & I2C_FUNC_I2C;

  fname = dirtree_path(node, &suffix_len);
  strcat(fname, suffix);
  xreadfile(fname, toybuf, sizeof(toybuf));
  free(fname);
  if ((p = strchr(toybuf, '\n'))) *p = 0;

  // "i2c-1	i2c	Synopsys DesignWare I2C adapter		I2C adapter"
  printf("%s\t%-10s\t%-32s\t%s\n", node->name, funcs ? "i2c" : "?", toybuf,
         funcs ? "I2C Adapter" : "?");

  return 0;
}

void i2cdetect_main(void)
{
  int bus, first, last, fd, addr = 0;
  char byte;

  if (FLAG(l)|FLAG(F)) {
    if (toys.optc) error_exit("bad '%s'", *toys.optargs);
    if (FLAG(l))
      dirtree_flagread("/sys/class/i2c-dev", DIRTREE_SHUTUP, i2cdetect_dash_l);
    else {
      unsigned sup = i2c_get_funcs(TT.F), i;
      char *funcs[] = {
        "I2C", "10 bit", 0, "SMBus PEC", 0, 0, "SMBus Block Process Call",
        "SMBus Quick Command", "SMBus Receive Byte", "SMBus Send Byte",
        "SMBus Read Byte", "SMBus Write Byte", "SMBus Read Word",
        "SMBus Write Word", "SMBus Process Call", "SMBus Read Block",
        "SMBus Write Block", "I2C Read Block", "I2C Write Block" };

      printf("Functionalities implemented by %s:\n", toybuf);
      for (i = 0; i<ARRAY_LEN(funcs); i++)
        if (funcs[i])
          printf("%-32s %s\n", funcs[i], (sup&(1<<i)) ? "yes" : "no");
    }

    return;
  }

  if (!(toys.optc&1)) help_exit("Needs 1 or 3 arguments");
  bus = atolx_range(*toys.optargs, 0, 0x3f);
  if (toys.optc==3) {
    first = atolx_range(toys.optargs[1], 0, 0x7f);
    last = atolx_range(toys.optargs[2], 0, 0x7f);
    if (first > last) error_exit("first > last");
  } else {
    first = FLAG(a) ? 0 : 3;
    last = FLAG(a) ? 0x7f : 0x77;
  }

  confirm("Probe chips 0x%02x-0x%02x on bus %d?", first, last, bus);

  fd = i2c_open(bus, 0, 0);
  printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
  while (addr < 0x80) {
    if (!(addr&0xf)) xprintf("%02x:", addr);
    if (addr<first || addr>last) printf("   ");
    else if (ioctl(fd, I2C_SLAVE, addr) == -1) {
      if (errno == EBUSY) xprintf(" UU");
      else perror_exit("ioctl(I2C_SLAVE)");
    } else if ((FLAG(r) ? i2c_read_byte(fd, addr, &byte)
                        : i2c_quick_write(fd, addr)) == -1) xprintf(" --");
    else xprintf(" %02x", addr);
    if (!(++addr&0xf)) putchar('\n');
  }
  close(fd);
}

#define FOR_i2cdump
#include "generated/flags.h"

void i2cdump_main(void)
{
  int fd, row, addr, bus = atolx_range(toys.optargs[0], 0, 0x3f),
      chip = atolx_range(toys.optargs[1], 0, 0x7f);
  char byte;

  confirm("Dump chip 0x%02x on bus %d?", chip, bus);

  fd = i2c_open(bus, FLAG(f) ? I2C_SLAVE_FORCE : I2C_SLAVE, chip);
  printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f    0123456789abcdef\n");
  for (row = 0; row<0x100; row += 16) {
    xprintf("%02x:", row & 0xf0);
    for (addr = row; addr<row+16; ++addr) {
      if (!i2c_read_byte(fd, addr, &byte)) printf(" %02x", byte);
      else {
        printf(" XX");
        byte = 'X';
      }
      toybuf[addr-row] = isprint(byte) ? byte : byte ? '?' : '.';
    }
    printf("    %16.16s\n", toybuf);
  }
  close(fd);
}

#define FOR_i2cget
#include "generated/flags.h"

void i2cget_main(void)
{
  int fd, bus = atolx_range(toys.optargs[0], 0, 0x3f),
      chip = atolx_range(toys.optargs[1], 0, 0x7f),
      addr = (toys.optc == 3) ? atolx_range(toys.optargs[2], 0, 0xff) : -1;
  char byte;

  confirm("Read register 0x%02x from chip 0x%02x on bus %d?", addr, chip, bus);

  fd = i2c_open(bus, FLAG(f) ? I2C_SLAVE_FORCE : I2C_SLAVE, chip);
  if (toys.optc == 3) {
    if (i2c_read_byte(fd, addr, &byte)==-1) perror_exit("i2c_read_byte");
  } else if (read(fd, &byte, 1) != 1) perror_exit("i2c_read");

  printf("0x%02x\n", byte);
  close(fd);
}

#define FOR_i2cset
#include "generated/flags.h"

void i2cset_main(void)
{
  int fd, i, bus = atolx_range(toys.optargs[0], 0, 0x3f),
      chip = atolx_range(toys.optargs[1], 0, 0x7f),
      addr = atolx_range(toys.optargs[2], 0, 0xff);
  char *mode = toys.optargs[toys.optc-1];
  struct i2c_smbus_ioctl_data ioctl_data;
  union i2c_smbus_data data;

  memset(&data, 0, sizeof(data));
  if (strlen(mode)!=1) help_exit("mode too long");
  if (*mode=='b' && toys.optc==5) {
    ioctl_data.size = I2C_SMBUS_BYTE_DATA;
    data.byte = atolx_range(toys.optargs[3], 0, 0xff);
  } else if (*mode=='w' && toys.optc==5) {
    ioctl_data.size = I2C_SMBUS_WORD_DATA;
    data.word = atolx_range(toys.optargs[3], 0, 0xffff);
  } else if (*mode=='i' && toys.optc>=5) {
    if (toys.optc-4>I2C_SMBUS_BLOCK_MAX) error_exit("too much data");
    ioctl_data.size = I2C_SMBUS_I2C_BLOCK_DATA;
    data.block[0] = toys.optc-4;
    for (i = 0; i<toys.optc-4; i++)
      data.block[i+1] = atolx_range(toys.optargs[3+i], 0, 0xff);
  } else help_exit("syntax error");

  confirm("Write register 0x%02x from chip 0x%02x on bus %d?", addr, chip, bus);

  // We open the device read-only and the write command works?
  fd = i2c_open(bus, FLAG(f) ? I2C_SLAVE_FORCE : I2C_SLAVE, chip);
  ioctl_data.read_write = I2C_SMBUS_WRITE;
  ioctl_data.command = addr;
  ioctl_data.data = &data;
  xioctl(fd, I2C_SMBUS, &ioctl_data);
  close(fd);
}

#define FOR_i2ctransfer
#include "generated/flags.h"

static void show_msgs(FILE *fp, struct i2c_rdwr_ioctl_data *data, int before)
{
  int i;

  for (i = 0; i < data->nmsgs; i++) {
    struct i2c_msg *msg = &data->msgs[i];
    int j, write = !msg->flags, hexdump;

    // Even with -v we can't show read data before it's read!
    hexdump = (before && write) || (!before && !write) || (!before && FLAG(v));
    if (!before && !FLAG(v) && !hexdump) continue;

    if (before || FLAG(v)) {
      fprintf(fp, "msg %d: addr 0x%02x, %s, length %u%s", i, msg->addr,
              write ? "write" : "read", msg->len, hexdump ? ", data " : "");
    }
    if (hexdump) {
      for (j = 0; j < msg->len; j++) fprintf(fp, "0x%02x ", msg->buf[j]);
    }
    fprintf(fp, "\n");
  }
}

void i2ctransfer_main(void)
{
  int fd, bus = atolx_range(toys.optargs[0], 0, 0x3f), i = 1, j;
  char *arg, *addr_str;
  struct i2c_rdwr_ioctl_data ioctl_data;
  struct i2c_msg msgs[I2C_RDWR_IOCTL_MAX_MSGS], *msg;

  ioctl_data.msgs = msgs;
  ioctl_data.nmsgs = 0;

  while ((arg = toys.optargs[i++])) {
    if (ioctl_data.nmsgs >= I2C_RDWR_IOCTL_MAX_MSGS) error_exit("too much!");

    msg = &msgs[ioctl_data.nmsgs];
    if (*arg == 'r') {
      msg->flags = I2C_M_RD;
    } else if (*arg == 'w') {
      msg->flags = 0;
    } else error_exit("expected read or write: %s", arg);

    addr_str = strchr(arg, '@');
    if (addr_str) {
      msg->addr = atolx_range(addr_str + 1, 0, 0x7f);
      *addr_str = '\0';
    } else {
      if (ioctl_data.nmsgs == 0) error_exit("missing address: %s", arg);
      msg->addr = msgs[ioctl_data.nmsgs - 1].addr;
    }

    // The struct field is 16 bits, but the kernel (as of 6.4) limits each
    // message to 8KiB. Either is far larger than you're likely to see in
    // practice.
    msg->len = atolx_range(arg + 1, 0, 0xffff);
    msg->buf = xzalloc(msg->len);
    if (*arg == 'w') {
      for (j = 0; j < msg->len; j++) {
        arg = toys.optargs[i++];
        if (!arg) error_exit("expected %d data bytes", msg->len);
        msg->buf[j] = atolx_range(arg, 0, 0xff);
      }
    }

    ioctl_data.nmsgs++;
  }

  fprintf(stderr, "Will send following messages on bus %d...\n", bus);
  show_msgs(stderr, &ioctl_data, 1);
  confirm("Send transfers on bus %d?", bus);

  fd = i2c_open(bus, 0, 0);
  xioctl(fd, I2C_RDWR, &ioctl_data);
  close(fd);

  show_msgs(stdout, &ioctl_data, 0);

  for (i = 0; i < ioctl_data.nmsgs; i++) free(msgs[i].buf);
}
