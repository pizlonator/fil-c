/* fdisk.c -  fdisk program to modify partitions on disk.
 *
 * Copyright 2012 Ashwini Kumar <ak.ashwini@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * No Standard.

USE_FDISK(NEWTOY(fdisk, "C#<0H#<0S#<0b#<512ul", TOYFLAG_SBIN))

config FDISK
  bool "fdisk"
  default n
  help
    usage: fdisk [-lu] [-C CYLINDERS] [-H HEADS] [-S SECTORS] [-b SECTSZ] DISK

    Change partition table

    -u            Start and End are in sectors (instead of cylinders)
    -l            Show partition table for each DISK, then exit
    -b size       sector size (512, 1024, 2048 or 4096)
    -C CYLINDERS  Set number of cylinders/heads/sectors
    -H HEADS
    -S SECTORS
*/

#define FOR_fdisk
#include "toys.h"
#include <linux/hdreg.h>

GLOBALS(
  long sect_sz;
  long sectors;
  long heads;
  long cylinders;
)

#define EXTENDED        0x05
#define WIN98_EXTENDED  0x0f
#define LINUX_NATIVE    0x83
#define LINUX_EXTENDED  0x85

#define SECTOR_SIZE 512
#define ONE_K       1024
#define PARTITION_MAX  60  //partition max is modifiable
#define IS_EXTENDED(i) ((i) == EXTENDED || (i) == WIN98_EXTENDED || (i) == LINUX_EXTENDED)
#define sector(s) ((s) & 0x3f)
#define cylinder(s, c) ((c) | (((s) & 0xc0) << 2))

typedef unsigned long long sector_t;

struct partition {
  unsigned char boot_ind, head, sector, cyl, sys_ind, end_head,
                end_sector, end_cyl, start4[4], size4[4];
};

struct part_entry {
  struct partition *part;
  char *sec_buffer;
  sector_t  start_offset;
  int modified;
};

struct part_types {
  int id;
  char type[24];
} sys_types[] = {
  {0x00, "Empty"}, {0x01, "FAT12"}, {0x04, "FAT16 <32M"}, {0x05, "Extended"},
  {0x06, "FAT16"}, {0x07, "HPFS/NTFS"}, {0x0a, "OS/2 Boot Manager"},
  {0x0b, "Win95 FAT32"}, {0x0c, "Win95 FAT32 (LBA)"}, {0x0e, "Win95 FAT16 (LBA)"},
  {0x0f, "Win95 Ext'd (LBA)"}, {0x11, "Hidden FAT12"}, {0x12, "Compaq diagnostics"},
  {0x14, "Hidden FAT16 <32M"}, {0x16, "Hidden FAT16"}, {0x17, "Hidden HPFS/NTFS"},
  {0x1b, "Hidden Win95 FAT32"}, {0x1c, "Hidden W95 FAT32 (LBA)"}, {0x1e, "Hidden W95 FAT16 (LBA)"},
  {0x3c, "Part.Magic recovery"}, {0x41, "PPC PReP Boot"}, {0x42, "SFS"},
  {0x63, "GNU HURD or SysV"}, {0x80, "Old Minix"}, {0x81, "Minix / old Linux"},
  {0x82, "Linux swap"}, {0x83, "Linux"}, {0x84, "OS/2 hidden C: drive"},
  {0x85, "Linux extended"}, {0x86, "NTFS volume set"}, {0x87, "NTFS volume set"},
  {0x8e, "Linux LVM"}, {0x9f, "BSD/OS"}, {0xa0, "Thinkpad hibernation"},
  {0xa5, "FreeBSD"}, {0xa6, "OpenBSD"}, {0xa8, "Darwin UFS"}, {0xa9, "NetBSD"},
  {0xab, "Darwin boot"}, {0xb7, "BSDI fs"}, {0xb8, "BSDI swap"},
  {0xbe, "Solaris boot"}, {0xeb, "BeOS fs"}, {0xee, "EFI GPT"},
  {0xef, "EFI (FAT-12/16/32)"}, {0xf0, "Linux/PA-RISC boot"},
  {0xf2, "DOS secondary"}, {0xfd, "Linux raid autodetect"},
};

static int num_parts, disp_unit_cyl, dos_flag, dev_fd = 3;
static long g_cylinders, g_heads, g_sectors, g_sect_size;
static sector_t total_number_sectors, extended_offset;
static char MBRbuf[2048], *disk_device;
struct part_entry partitions[PARTITION_MAX];

static struct partition* part_offset(char *secbuf, int i)
{
  return (struct partition*)(secbuf + 0x1be + i*(sizeof(struct partition)));
}

static void set_levalue(unsigned char *cp, sector_t value )
{
  uint32_t val = SWAP_LE32(value);
  memcpy(cp, (void*)&val, 4);
}

static void set_hsc(struct partition *p, sector_t start, sector_t end)
{
  if (dos_flag && (start / (g_sectors * g_heads) > 1023))
    start = g_heads * g_sectors * ONE_K - 1;
  p->sector = (start % g_sectors) + 1;
  start /= g_sectors;
  p->head = start % g_heads;
  start /= g_heads;
  p->cyl = start & 0xFF;
  p->sector |= (start >> 2) & 0xc0;

  if (dos_flag && (end / (g_sectors * g_heads) > 1023))
    end = g_heads * g_sectors * ONE_K - 1;
  p->end_sector = (end % g_sectors) + 1;
  end /= g_sectors;
  p->end_head = end % g_heads;
  end /= g_heads;
  p->end_cyl = end & 0xFF;
  p->end_sector |= (end >> 2) & 0xc0;
}

static int chs_warn(void)
{
  if (g_heads && g_sectors && g_cylinders)
    return 0;

  printf("Unknown value(s) for:");
  if (!g_heads) printf(" heads");
  if (!g_sectors) printf(" sectors");
  if (!g_cylinders) printf(" cylinders");
  printf(". can set in the expert menu.\n");
  return 1;
}

static void list_types(void)
{
  int i, adjust = 0, size = ARRAY_LEN(sys_types);

  if(size % 2) adjust = 1;
  for (i = 0; i < (size - adjust); i+=2)
    xprintf("%2x %-22s\t\t%2x %-22.22s\n", sys_types[i].id, sys_types[i].type,
        sys_types[i+1].id, sys_types[i+1].type);
  if (adjust) xprintf("%2x %-22s\n",sys_types[size-1].id, sys_types[size-1].type);
  xputc('\n');
}

static void read_sec_sz()
{
  int arg;
  if (ioctl(dev_fd, BLKSSZGET, &arg) == 0) g_sect_size = arg;
  if (FLAG(b)) {
    if (TT.sect_sz !=  512 && TT.sect_sz != 1024 && TT.sect_sz != 2048 &&
        TT.sect_sz != 4096)
    {
      help_exit("bad sector size");
    }
    g_sect_size = TT.sect_sz;
  }
}

static sector_t read_size()
{
  uint64_t sec64 = 0;
  unsigned long sectors = 0;
  if (ioctl(dev_fd, BLKGETSIZE64, &sec64) == 0) {
    sec64 = sec64 >> 9; //convert to 512 block size.
    if (sec64 != (uint32_t) sec64) {
      perror_msg("device has more than 2^32 sectors, can't use all of them");
      sec64 = (uint32_t) - 1L;
    }
    return sec64;
  }
  if (ioctl(dev_fd, BLKGETSIZE, &sectors) == 0)
    if (sizeof(long) > sizeof(sector_t) && sectors != (sector_t)sectors)
      sectors = (uint32_t) - 1L;
  return sectors;
}

static int validate_part_buff(char *buffer)
{
  if ((buffer[510] != 0x55) || (buffer[511] != 0xAA)) return 0;
  return 1;
}

static int is_partition_clear(struct partition* p)
{
  int i = 0;
  unsigned char res = 0;
  const char *ptr = (const char*)p;

  for (i = 0; i < sizeof(struct partition); i++) res |= (unsigned char)ptr[i];
  return (res == 0x00);
}

static uint32_t swap_le32toh(unsigned char *cp)
{
  uint32_t val;
  memcpy((void*)&val, cp, 4);
  return le32toh(val);
}

static int check_order(void)
{
  sector_t first[num_parts], last_seen_val = 0;
  int i;
  struct part_entry *pe;
  struct partition *px;

  for (i = 0; i < num_parts; i++) {
    if (i == 4) last_seen_val = 0;
    pe = &partitions[i];
    px = pe->part;
    if (px->sys_ind) {
      first[i] = swap_le32toh(px->start4) + pe->start_offset;
      if (last_seen_val > first[i]) return 1;
      last_seen_val = first[i];
    }
  }
  return 0;
}

static void read_geometry(struct hd_geometry *disk)
{
  struct hd_geometry geometry;

  if (ioctl(dev_fd, HDIO_GETGEO, &geometry)) return;
  disk->heads = geometry.heads;
  disk->sectors = geometry.sectors;
}

/* Read the extended boot record for the
 * logical partion details.
 */
static void read_ebr(int idx)
{
  char *sec_buf = NULL;
  sector_t offset = 0, local_start_off = 0;
  struct partition *p, *q;

  q = p = partitions[idx].part;
  local_start_off = swap_le32toh(p->start4);

  if (!extended_offset) extended_offset = local_start_off;
  do {
    if (num_parts >= 60) {
      xprintf("Warning: deleting partitions after 60\n");
      memset(q, 0, sizeof(struct partition)); //clear_partition
      partitions[num_parts-1].modified = 1;
      break;
    }

    sec_buf = xzalloc(g_sect_size);
    partitions[num_parts].part = part_offset(sec_buf, 0);
    partitions[num_parts].sec_buffer = sec_buf;
    offset = swap_le32toh(q->start4);

    if (num_parts > 4) offset += local_start_off;
    partitions[num_parts].start_offset = offset;
    xlseek(dev_fd, (off_t)(offset * g_sect_size), SEEK_SET);

    if (g_sect_size != readall(dev_fd, sec_buf, g_sect_size)) {
      close(dev_fd);
      error_exit("Couldn't read sector zero\n");
    }
    num_parts++; //extended partions present.
    q = part_offset(sec_buf, 1);
  } while (!is_partition_clear(q) && IS_EXTENDED(q->sys_ind));
}

static void physical_HS(int* h, int *s)
{
  struct partition *p;
  int i, end_h, end_s, e_hh = 0, e_ss = 0, ini = 1, dirty = 0;
  const unsigned char *bufp = (const unsigned char *)MBRbuf;

  if (!(validate_part_buff((char*)bufp))) return;

  for (i = 0; i < 4; i++) {
    p = part_offset((char*)bufp, i);
    if (p->sys_ind) {
      end_h = p->end_head + 1;
      end_s = (p->end_sector & 077);
      if (ini) {
        e_hh = end_h;
        e_ss = end_s;
        ini = 0;
      } else if (e_hh !=end_h || e_ss != end_s)
        dirty = 1;
    }
  }
  if (!dirty && !ini) {
    *h = e_hh;
    *s = e_ss;
  }
}

//Reset the primary partition table
static void reset_boot(int change)
{
  int i;
  for(i = 0; i < 4; i++) {
    struct part_entry *pe = &partitions[i];
    pe->part = part_offset(MBRbuf, i);
    pe->start_offset = 0;
    pe->sec_buffer = MBRbuf;
    pe->modified = change;
  }
}

static inline void write_table_flag(char *buf)
{
  buf[510] = 0x55;
  buf[511] = 0xaa;
}

/* free the buffers used for holding details of
 * extended logical partions
*/
static void free_bufs(void)
{
  int i = 4;
  for (; i < num_parts; i++) free(partitions[i].sec_buffer);
}

static void create_empty_doslabel(void)
{
  xprintf("Building a new DOS Disklabel. The changes will\n"
      "remain in memory only, until you write it.\n");

  num_parts = 4;
  extended_offset = 0;
  memset(&MBRbuf[510 - 4*16], 0, 4*16);
  write_table_flag(MBRbuf);
  partitions[0].modified = 1;
  reset_boot(1);
}

/* Read the Master Boot sector of the device for the
 * partition table entries/details.
 * If any extended partition is found then read the EBR
 * for logical partition details
 */
static int read_mbr(char *device, int validate)
{
  int fd, sector_fac, i, h = 0, s = 0;
  struct hd_geometry disk;
  fd = open(device, O_RDWR);
  if(fd < 0) {
    perror_msg("can't open '%s'",device);
    return 1;
  }

  disk_device = strdup(device);
  if(fd != dev_fd) {
    if(dup2(fd, dev_fd) != dev_fd) perror_exit("Can't dup2");
    close(fd);
  }

  //read partition table - MBR
  if (SECTOR_SIZE != readall(dev_fd, MBRbuf, SECTOR_SIZE)) {
    close(dev_fd);
    perror_exit("Couldn't read sector zero\n");
  }
  if (validate && !validate_part_buff(MBRbuf)) {
    xprintf("Device contains neither a valid DOS "
        "partition table, nor Sun, SGI, OSF or GPT "
        "disklabel\n");
    create_empty_doslabel();
  }

  disk.heads = disk.sectors = 0;
  read_geometry(&disk); //CHS values
  total_number_sectors = read_size(); //Device size
  read_sec_sz();
  sector_fac = g_sect_size/SECTOR_SIZE; //512 is hardware sector size.
  physical_HS(&h, &s); //physical dimensions may be diferent from HDIO_GETGEO
  g_sectors = (FLAG(S) && TT.sectors) ? TT.sectors : s ? s : disk.sectors ? disk.sectors : 63;
  g_heads = (FLAG(H) && TT.heads) ? TT.heads : h ? h : disk.heads ? disk.heads : 255;
  g_cylinders = total_number_sectors/(g_heads * g_sectors * sector_fac);

  if (!g_cylinders) g_cylinders = FLAG(C) ? TT.cylinders : 0;
  if ((g_cylinders > ONE_K) && !(FLAG(l) || FLAG(S)))
    xprintf("\nThe number of cylinders for this disk is set to %lu.\n"
        "There is nothing wrong with that, but this is larger than 1024,\n"
        "and could in certain setups cause problems.\n", g_cylinders);
  for (i = 0; i < num_parts; i++) {
    if (IS_EXTENDED(partitions[i].part->sys_ind)) {
      read_ebr(i);
      break;
    }
  }
  chs_warn();

  return 0;
}

static char* get_type(int sys_ind)
{
  int i, size = ARRAY_LEN(sys_types);
  for (i = 0; i < size; i++)
    if (sys_ind == sys_types[i].id)
      return sys_types[i].type;
  return "Unknown";
}

static void consistency_check(const struct partition *p, int partition)
{
  unsigned physbc, physbh, physbs, physec, physeh, physes;
  unsigned lbc, lbh, lbs, lec, leh, les;
  sector_t start, end;

  if (!g_heads || !g_sectors || (partition >= 4)) return;
  // physical beginning c, h, s
  physbc = cylinder(p->sector,p->cyl);
  physbh = p->head;
  physbs = sector(p->sector);
  // physical ending c, h, s
  physec = cylinder(p->end_sector, p->end_cyl);
  physeh = p->end_head;
  physes = sector(p->end_sector);
  // logical begin and end CHS values
  start = swap_le32toh((unsigned char*)(p->start4));
  end = start + swap_le32toh((unsigned char*)(p->size4)) -1;

  lbc = start/(g_sectors * g_heads);
  lbh = (start/g_sectors) % g_heads;
  lbs = (start % g_sectors) + 1;

  lec = end/(g_sectors * g_heads);
  leh = (end/g_sectors) % g_heads;
  les = (end % g_sectors) + 1;

  //Logical and Physical diff
  if (g_cylinders <= ONE_K && (physbc != lbc || physbh != lbh || physbs != lbs)) {
    xprintf("Partition %u has different physical/logical beginnings (Non-Linux?): \n", partition+1);
    xprintf("phys = (%u %u %u) ",physbc, physbh, physbs);
    xprintf("logical = (%u %u %u)\n", lbc, lbh, lbs);
  }
  if (g_cylinders <= ONE_K && (physec != lec || physeh != leh || physes != les)) {
    xprintf("Partition %u has different physical/logical endings: \n", partition+1);
    xprintf("phys = (%u %u %u) ",physec, physeh, physes);
    xprintf("logical = (%u %u %u)\n", lec, leh, les);
  }
  // Ending on cylinder boundary?
  if (physeh != (g_heads - 1) || physes != g_sectors)
    xprintf("Partition %u does not end on cylinder boundary\n", partition + 1);
}

// List the partition details
static void list_partitions(int validate)
{
  struct partition *p;
  uint32_t start_cyl, end_cyl, start_sec, end_sec, blocks, secs;
  char boot, lastchar = '\0', *dev = disk_device;
  int i = 0, len = strlen(disk_device), odds = 0;

  if (validate && !validate_part_buff(MBRbuf)) {
    close(dev_fd);
    toys.exitval = 1;
    xprintf("Device %s: doesn't contain a valid partition table\n", disk_device);
    return;
  }
  if (isdigit(dev[len - 1])) lastchar = 'p';

  xprintf("%*s Boot      Start         End      Blocks  Id System\n", len+1, "Device");
  for (i = 0; i < num_parts; i++) {
    p = partitions[i].part;
    if (is_partition_clear(p)) continue;

    boot = ((p->boot_ind == 0x80)?'*':(!p->boot_ind)?' ':'?');
    start_sec = swap_le32toh(p->start4) + partitions[i].start_offset;
    secs = swap_le32toh(p->size4);

    if ((start_sec + secs) == 0) end_sec = 0;
    else end_sec = start_sec + secs -1;
    start_cyl = start_sec/(g_heads * g_sectors) + 1;
    end_cyl = end_sec/(g_heads * g_sectors) + 1;
    blocks = secs;
    if (g_sect_size < ONE_K) {
      blocks /= (ONE_K/g_sect_size);
      odds = secs %(ONE_K/g_sect_size);
    } else if (g_sect_size > ONE_K) blocks *= (g_sect_size/ONE_K);

    if (lastchar) xprintf("%s%c%d",dev, lastchar, i+1);
    else xprintf("%s%d",dev, i+1);

    xprintf("   %c %11u %11u %11u%c %2x %s\n",
        boot,
        disp_unit_cyl == 0? start_sec: start_cyl,
        disp_unit_cyl == 0? end_sec: end_cyl,
        blocks,odds?'+':' ', p->sys_ind, get_type(p->sys_ind));

    consistency_check(p, i);
  }
  if (check_order()) xprintf("\nPartition table entries are not in disk order");
}

//Print device details
static void print_mbr(int validate)
{
  unsigned long long bytes = ((unsigned long long)total_number_sectors << 9);
  long mbytes = bytes/1000000;

  if (mbytes < 10000) xprintf("Disk %s: %lu MB, %llu bytes\n", disk_device, mbytes, bytes);
  else xprintf("Disk %s: %lu.%lu GB, %llu bytes\n", disk_device, mbytes/1000, (mbytes/100)%10, bytes);
  xprintf("%ld heads, %ld sectors/track, %ld cylinders", g_heads, g_sectors, g_cylinders);
  if (!disp_unit_cyl) {
    xprintf(", total %lld sectors\n", total_number_sectors/(g_sect_size/SECTOR_SIZE));
    xprintf("Units = sectors of 1 * %ld = %ld bytes\n",g_sect_size, g_sect_size);
  } else xprintf("\nUnits = cylinders of %ld * %ld = %ld bytes\n\n",
      g_heads * g_sectors, g_sect_size, g_heads * g_sectors * g_sect_size);
  list_partitions(validate);
  xputc('\n');
}

static void init_members(void)
{
  int i = 0;
  num_parts = 4; //max of primaries in a part table
  disp_unit_cyl = dos_flag = 1;
  extended_offset = 0;
  g_sect_size = SECTOR_SIZE;
  for (i = 0; i < num_parts; i++) {
    partitions[i].part = part_offset(MBRbuf, i);
    partitions[i].sec_buffer = MBRbuf;
    partitions[i].modified = 0;
    partitions[i].start_offset = 0;
  }
}

static int read_input(char *mesg, char *outp)
{
  char *p;
  int size = 0;
  do {
    xprintf("%s", mesg);
    p = fgets(toybuf, 80, stdin);

    if (!p || !(size = strlen(p))) exit(0);
    if (p[size-1] == '\n') p[--size] = '\0';
  } while (!size);

  while (*p != '\0' && *p <= ' ') p++;
  if (outp) memcpy(outp, p, strlen(p) + 1); //1 for nul
  return *p;
}

static int read_hex(char *mesg)
{
  int val;
  char input[80], *endp;
  while (1) {
    read_input(mesg, input);
    if ((*input | 0x20) == 'l') {
      list_types();
      memset(input, 0, 80);
      continue;
    }
    val = strtoul(input, &endp, 16);
    if (endp && *endp) continue;
    if (val <= 0xff) return val;
  }
}

/* Delete an exiting partition,
 * if its primary, then just clear the partition details
 * if extended, then clear the partition details, also for logical
 * if only logical, then move the later partitions backwards 1 step
 */
void delete_partition(int i)
{
  int sys_id, looper = 0;
  struct partition *p, *q, *ext_p, *ext_q;
  sector_t new_start;
  struct part_entry *pe = &partitions[i];

  if (chs_warn()) return;
  p = pe->part;
  sys_id = p->sys_ind;
  if (!sys_id) xprintf("Partition %u is empty\n", i+1);

  if (i < 4 && !IS_EXTENDED(sys_id)) {
    memset(p, 0, sizeof(struct partition)); //clear_partition
    pe->modified = 1;
  } else if (i < 4 && IS_EXTENDED(sys_id)) {
    memset(p, 0, sizeof(struct partition)); //clear_partition
    pe->modified = 1;
    for (looper = 4; looper < num_parts; looper++) {
      pe = &partitions[looper];
      p = pe->part;
      if (is_partition_clear(p)) break;
      else {
        memset(p, 0, sizeof(struct partition)); //clear_partition
        pe->modified = 1;
        free(pe->sec_buffer);
      }
    }
    extended_offset = 0;
    num_parts = 4;
  } else {
    //only logical is delete, need to move the rest of them backwards
    if (i == 4) { //move partiton# 6 to 5.
      partitions[i].modified = 1;
      if (num_parts > i+1) {
        q = partitions[i + 1].part;
        *p = *q; //copy the part table
        ext_p = part_offset(partitions[i].sec_buffer, 1);
        ext_q = part_offset(partitions[i + 1].sec_buffer, 1);
        *ext_p = *ext_q; //copy the extended info pointer
        // change the start of the 4th partiton.
        new_start = partitions[i + 1].start_offset + swap_le32toh(q->start4) - extended_offset;
        new_start = SWAP_LE32(new_start);
        memcpy(p->start4, (void *)&new_start, 4);
      } else {
        memset(partitions[i].part, 0, sizeof(struct partition));
        return; //only logical
      }
    } else if (i > 4) {
      ext_p = part_offset(partitions[i-1].sec_buffer, 1);
      ext_q = part_offset(partitions[i].sec_buffer, 1);
      memcpy((void*)ext_p, (void *)ext_q, sizeof(struct partition));
      partitions[i-1].modified = 1;
    }
    if (i == 4) looper = i+2;
    else if (i > 4) looper = i+1;
    for (; looper < num_parts; looper++)
      partitions[looper-1] = partitions[looper];
    num_parts--;
  }
}

static int ask_partition(int num_parts)
{
  int val;
  while (1) {
    do {
      xprintf("Partition (%u - %u):", 1, num_parts);
      fgets(toybuf, 80, stdin);
    } while (!isdigit(*toybuf));
    val = atoi(toybuf);
    if (val > 0 && val <= num_parts) return val;
    else xprintf("Invalid number entered\n");
  }
}

static void toggle_active_flag(int i)
{
  struct partition *p = partitions[i].part;
  if (is_partition_clear(p)) xprintf("Partition %u is empty\n", i+1);

  if (IS_EXTENDED(p->sys_ind) && !p->boot_ind)
    xprintf("WARNING: Partition %u is an extended partition\n", i + 1);
  p->boot_ind = p->boot_ind == 0x80?0 : 0x80;
  partitions[i].modified = 1;
}

//Write the partition details from Buffer to Disk.
void write_table(void)
{
  int i =0;
  struct part_entry *pe;
  sector_t offset;

  for (i = 0; i < 4; i++)
    if (partitions[i].modified) partitions[3].modified = 1;

  for (i = 3; i < num_parts; i++) {
    pe = &partitions[i];
    write_table_flag(pe->sec_buffer);
    offset = pe->start_offset;
    if (pe->modified == 1) {
      xlseek(dev_fd, offset * g_sect_size, SEEK_SET);
      xwrite(dev_fd, pe->sec_buffer, g_sect_size);
    }
  }
  xprintf("The partition table has been altered.\n");
  xprintf("Calling ioctl() to re-read partition table\n");
  sync();
  for (i = 4; i < num_parts; i++) free(partitions[i].sec_buffer);
  if(ioctl(dev_fd, BLKRRPART, NULL) < 0)
    perror_exit("WARNING: rereading partition table failed, kernel still uses old table");

}

/* try to find a partition for deletion, if only
 * one, then select the same, else ask from USER
 */
static int get_non_free_partition(int max)
{
  int num = -1, i = 0;

  for (i = 0; i < max; i++) {
    if (!is_partition_clear(partitions[i].part)) {
      if (num >= 0)
        return ask_partition(num_parts)-1;
      num = i;
    }
  }
  (num >= 0) ? xprintf("Selected partition %d\n",num+1):
    xprintf("No partition is defined yet!\n");
  return num;
}

/* a try at autodetecting an empty partition table entry,
 * if multiple options then get USER's choce.
 */
static int get_free_partition(int max)
{
  int num = -1, i = 0;

  for (i = 0; i < max; i++) {
    if (is_partition_clear(partitions[i].part)) {
      if (num >= 0)
        return ask_partition(4)-1;
      num = i;
    }
  }
  (num >= 0) ? xprintf("Selected partition %d\n",num+1):
    xprintf("All primary partitions have been defined already!\n");
  return num;
}

//taking user input for partition start/end sectors/cyinders
static uint32_t ask_value(char *mesg, sector_t left, sector_t right, sector_t defalt)
{
  char *str = toybuf;
  uint32_t val;
  int use_default = 1;

  while (1) {
    use_default = 1;
    do {
      xprintf("%s",mesg);
      fgets(str, 80, stdin);
    } while (!isdigit(*str) && (*str != '\n')
        && (*str != '-') && (*str != '+') && (!isblank(*str)));
    while (isblank(*str)) str++; //remove leading white spaces
    if (*str == '+' || *str == '-') {
      int minus = (*str == '-');
      int absolute = 0;

      val = atoi(str + 1);
      while (isdigit(*++str)) use_default = 0;

      switch (*str) {
        case 'c':
        case 'C':
          if (!disp_unit_cyl) val *= g_heads * g_sectors;
          break;
        case 'K':
          absolute = ONE_K;
          break;
        case 'k':
          absolute = 1000;
          break;
        case 'm':
        case 'M':
          absolute = 1000000;
          break;
        case 'g':
        case 'G':
          absolute = 1000000000;
          break;
        default:
          break;
      }
      if (absolute) {
        unsigned long long bytes = (unsigned long long) val * absolute;
        unsigned long unit = (disp_unit_cyl && (g_heads * g_sectors))? g_heads * g_sectors : 1;

        unit = unit * g_sect_size;
        bytes += unit/2; // rounding
        bytes /= unit;
        val = bytes;
      }
      if (minus)
        val = -val;
      val += left;
    } else {
      val = atoi(str);
      while (isdigit(*str)) {
        str++;
        use_default = 0;
      }
    }
    if(use_default) {
      val = defalt;
      xprintf("Using default value %lld\n", defalt);
    }
    if (val >= left && val <= right) return val;
    else xprintf("Value out of range\n");
  }
}

//validating if the start given falls in a limit or not
static int validate(int start_index, sector_t* begin,sector_t* end, sector_t start
    , int asked)
{
  int i, valid = 0;
  for (i = start_index; i < num_parts; i++) {
    if (start >= begin[i] && start <= end[i]) {
      if (asked) xprintf("Sector %lld is already allocated\n",start);
      valid = 0;
      break;
    } else valid = 1;
  }
  return valid;
}

//get the start sector/cylinder of a new partition
static sector_t ask_start_sector(int idx, sector_t* begin, sector_t* end, int ext_idx)
{
  sector_t start, limit, temp = 0, start_cyl, limit_cyl, offset = 1;
  char mesg[256];
  int i, asked = 0, valid = 0, start_index = 0;

  if (dos_flag) offset = g_sectors;
  start = offset;
  if (disp_unit_cyl) limit = (sector_t)g_sectors * g_heads * g_cylinders - 1;
  else limit = total_number_sectors - 1;

  if (disp_unit_cyl) //make the begin of every partition to cylnder boundary
    for (i = 0; i < num_parts; i++)
      begin[i] = (begin[i]/(g_heads* g_sectors)) * (g_heads* g_sectors);

  if (idx >= 4) {
    if (!begin[ext_idx] && extended_offset) begin[ext_idx] = extended_offset;
    start = begin[ext_idx] + offset;
    limit = end[ext_idx];
    start_index = 4;
  }
  do {
    if (asked) valid = validate(start_index, begin, end, start, asked);
    if (valid) break;

    do {
      for (i = start_index; i < num_parts; i++)
        if (start >= begin[i] && start <= end[i])
          start = end[i] + 1 + ((idx >= 4)? offset : 0);
    } while (!validate(start_index, begin, end, start, 0));

    start_cyl = start/(g_sectors * g_heads) + 1;
    limit_cyl = limit/(g_sectors * g_heads) + 1;

    if (start > limit) break;
    sprintf(mesg, "First %s (%lld - %lld, default %lld): ", disp_unit_cyl? "cylinder" : "sector",
        (long long int)(disp_unit_cyl? start_cyl : start),
        (long long int)(disp_unit_cyl? limit_cyl : limit),
        (long long int)(disp_unit_cyl? start_cyl : start));
    temp = ask_value(mesg, disp_unit_cyl? start_cyl : start,
        disp_unit_cyl? limit_cyl : limit, disp_unit_cyl? start_cyl : start);
    asked = 1;

    if (disp_unit_cyl) {
      // point to the cylinder start sector
      temp = (temp-1) * g_heads * g_sectors;
      if (temp < start) //the boundary is falling in the already used sectors.
        temp = start;
    }
    start = temp;
  } while (asked && !valid);
  return start;
}

//get the end sector/cylinder of a new partition
static sector_t ask_end_sector(int idx, sector_t* begin, sector_t* end, int ext_idx, sector_t start_sec)
{
  sector_t limit, temp = 0, start_cyl, limit_cyl, start = start_sec;
  char mesg[256];
  int i;

  if (disp_unit_cyl) limit = (sector_t)g_sectors * g_heads * g_cylinders - 1;
  else limit = total_number_sectors - 1;

  if (disp_unit_cyl) //make the begin of every partition to cylnder boundary
    for (i = 0; i < num_parts; i++)
      begin[i] = (begin[i]/(g_heads* g_sectors)) * (g_heads* g_sectors);

  if (idx >= 4) limit = end[ext_idx];

  for (i = 0; i < num_parts; i++) {
    struct part_entry *pe = &partitions[i];
    if (start < pe->start_offset && limit >= pe->start_offset) limit = pe->start_offset - 1;
    if (start < begin[i] && limit >= begin[i]) limit = begin[i] - 1;
  }

  start_cyl = start/(g_sectors * g_heads) + 1;
  limit_cyl = limit/(g_sectors * g_heads) + 1;
  if (limit < start) { //the boundary is falling in the already used sectors.
    xprintf("No Free sectors available\n");
    return 0;
  }
  sprintf(mesg, "Last %s or +size or +sizeM or +sizeK (%lld - %lld, default %lld): ",
      disp_unit_cyl? "cylinder" : "sector",
      (long long int)(disp_unit_cyl? start_cyl : start),
      (long long int)(disp_unit_cyl? limit_cyl : limit),
      (long long int)(disp_unit_cyl? limit_cyl : limit));
  temp = ask_value(mesg, disp_unit_cyl? start_cyl : start,
      disp_unit_cyl? limit_cyl : limit, disp_unit_cyl? limit_cyl : limit);

  if (disp_unit_cyl) { // point to the cylinder start sector
    temp = temp * g_heads * g_sectors - 1;
    if (temp > limit) temp = limit;
  }
  if (temp < start) { //the boundary is falling in the already used sectors.
    xprintf("No Free sectors available\n");
    return 0;
  }
  return temp;
}

// add a new partition to the partition table
static int add_partition(int idx, int sys_id)
{
  int i, ext_idx = -1;
  sector_t start, end, begin_sec[num_parts], end_sec[num_parts];
  struct part_entry *pe = &partitions[idx];
  struct partition *p = pe->part;

  if (p && !is_partition_clear(p)) {
    xprintf("Partition %u is already defined, delete it to re-add\n", idx+1);
    return 0;
  }
  for (i = 0; i < num_parts; i++) {
    pe = &partitions[i];
    p = pe->part;
    if (is_partition_clear(p)) {
      begin_sec[i] = 0xffffffff;
      end_sec[i] = 0;
    } else {
      begin_sec[i] = swap_le32toh(p->start4) + pe->start_offset;
      end_sec[i] = begin_sec[i] + swap_le32toh(p->size4) - 1;
    }
    if (IS_EXTENDED(p->sys_ind)) ext_idx = i;
  }
  start = ask_start_sector(idx, begin_sec, end_sec, ext_idx);
  end = ask_end_sector(idx, begin_sec, end_sec, ext_idx, start);
  if (!end) return 0;
  //Populate partition table entry  - 16 bytes
  pe = &partitions[idx];
  p = pe->part;

  if (idx > 4) {
    if (dos_flag) pe->start_offset = start - (sector_t)g_sectors;
    else pe->start_offset = start - 1;
    if (pe->start_offset == extended_offset) pe->start_offset++;
    if (!dos_flag) start++;
  }

  set_levalue(p->start4, start - pe->start_offset);
  set_levalue(p->size4, end - start + 1);
  set_hsc(p, start, end);
  p->boot_ind = 0;
  p->sys_ind = sys_id;
  pe->modified = 1;

  if (idx > 4) {
    p = partitions[idx-1].part + 1; //extended pointer for logical partitions
    set_levalue(p->start4, pe->start_offset - extended_offset);
    set_levalue(p->size4, end - start + 1 + (dos_flag? g_sectors: 1));
    set_hsc(p, pe->start_offset, end);
    p->boot_ind = 0;
    p->sys_ind = EXTENDED;
    partitions[idx-1].modified = 1;
  }
  if (IS_EXTENDED(sys_id)) {
    pe = &partitions[4];
    pe->modified = 1;
    pe->sec_buffer = xzalloc(g_sect_size);
    pe->part = part_offset(pe->sec_buffer, 0);
    pe->start_offset = extended_offset = start;
    num_parts = 5;
  }
  return 1;
}

static void add_logical_partition(void)
{
  struct part_entry *pe;
  if (num_parts > 5 || !is_partition_clear(partitions[4].part)) {
    pe = &partitions[num_parts];
    pe->modified = 1;
    pe->sec_buffer = xzalloc(g_sect_size);
    pe->part = part_offset(pe->sec_buffer, 0);
    pe->start_offset = 0;
    num_parts++;
    if (!add_partition(num_parts - 1, LINUX_NATIVE)) {
      num_parts--;
      free(pe->sec_buffer);
    }
  }
  else add_partition(num_parts -1, LINUX_NATIVE);
}

/* Add a new partiton to the partition table.
 * MAX partitions limit is taken to be 60, can be changed
 */
static void add_new_partition(void)
{
  int choice, idx, i, free_part = 0;
  char *msg = NULL;

  if (chs_warn()) return;
  for (i = 0; i < 4; i++) if(is_partition_clear(partitions[i].part)) free_part++;

  if (!free_part && num_parts >= 60) {
    xprintf("The maximum number of partitions has been created\n");
    return;
  }
  if (!free_part) {
    if (extended_offset) add_logical_partition();
    else xprintf("You must delete some partition and add "
          "an extended partition first\n");
    return;
  }

  msg = xmprintf("  %s\n  p  primary partition(1-4)\n",
          extended_offset? "l  logical (5 or over)" : "e  extended");

  choice = 0x20 | read_input(msg, NULL);
  free(msg);
  if (choice == 'p') {
    idx = get_free_partition(4);
    if (idx >= 0) add_partition(idx, LINUX_NATIVE);
    return;
  }
  if (choice =='l' && extended_offset) {
    add_logical_partition();
    return;
  }
  if (choice == 'e' && !extended_offset) {
    idx = get_free_partition(4);
    if (idx >= 0) add_partition(idx, EXTENDED);
    return;
  }
}

static void change_systype(void )
{
  int i, sys_id;
  struct partition *p;
  struct part_entry *pe;

  i = ask_partition(num_parts);
  pe = &partitions[i-1];
  p = pe->part;
  if (is_partition_clear(p)) {
    xprintf("Partition %d doesn't exist yet!\n", i);
    return;
  }
  sys_id = read_hex("Hex code (L to list codes): ");
  if ((IS_EXTENDED(p->sys_ind) && !IS_EXTENDED(sys_id)) ||
      (!IS_EXTENDED(p->sys_ind) && IS_EXTENDED(sys_id))) {
    xprintf("you can't change a  partition to an extended or vice-versa\n");
    return;
  }

  xprintf("Changed system type of partition %u to %0x (%s)\n",i, sys_id, get_type(sys_id));
  p->sys_ind = sys_id;
  pe->modified = 1;
}

static void check(int n, unsigned h, unsigned s, unsigned c, sector_t start)
{
  sector_t total, real_s, real_c;

  real_s = sector(s) - 1;
  real_c = cylinder(s, c);
  total = (real_c * g_sectors + real_s) * g_heads + h;
  if (!total) xprintf("Partition %u contains sector 0\n", n);
  if (h >= g_heads)
    xprintf("Partition %u: head %u greater than maximum %lu\n", n, h + 1, g_heads);
  if (real_s >= g_sectors)
    xprintf("Partition %u: sector %u greater than maximum %lu\n", n, s, g_sectors);
  if (real_c >= g_cylinders)
    xprintf("Partition %u: cylinder %lld greater than maximum %lu\n", n, real_c + 1, g_cylinders);
  if (g_cylinders <= ONE_K && start != total)
    xprintf("Partition %u: previous sectors %lld disagrees with total %lld\n", n, start, total);
}

static void verify_table(void)
{
  int i, j, ext_idx = -1;
  sector_t begin_sec[num_parts], end_sec[num_parts], total = 1;
  struct part_entry *pe;
  struct partition *p;

  for (i = 0; i < num_parts; i++) {
    pe = &partitions[i];
    p = pe->part;
    if (is_partition_clear(p) || IS_EXTENDED(p->sys_ind)) {
      begin_sec[i] = 0xffffffff;
      end_sec[i] = 0;
    } else {
      begin_sec[i] = swap_le32toh(p->start4) + pe->start_offset;
      end_sec[i] = begin_sec[i] + swap_le32toh(p->size4) - 1;
    }
    if (IS_EXTENDED(p->sys_ind)) ext_idx = i;
  }
  for (i = 0; i < num_parts; i++) {
    pe = &partitions[i];
    p = pe->part;
    if (p->sys_ind && !IS_EXTENDED(p->sys_ind)) {
      consistency_check(p, i);
      if ((swap_le32toh(p->start4) + pe->start_offset) < begin_sec[i])
        xprintf("Warning: bad start-of-data in partition %u\n", i + 1);
      check(i + 1, p->end_head, p->end_sector, p->end_cyl, end_sec[i]);
      total += end_sec[i] + 1 - begin_sec[i];
      for (j = 0; j < i; j++) {
        if ((begin_sec[i] >= begin_sec[j] && begin_sec[i] <= end_sec[j])
            || ((end_sec[i] <= end_sec[j] && end_sec[i] >= begin_sec[j]))) {
          xprintf("Warning: partition %u overlaps partition %u\n", j + 1, i + 1);
          total += begin_sec[i] >= begin_sec[j] ? begin_sec[i] : begin_sec[j];
          total -= end_sec[i] <= end_sec[j] ? end_sec[i] : end_sec[j];
        }
      }
    }
  }
  if (extended_offset) {
    struct part_entry *pex = &partitions[ext_idx];
    sector_t e_last = swap_le32toh(pex->part->start4) +
      swap_le32toh(pex->part->size4) - 1;

    for (i = 4; i < num_parts; i++) {
      total++;
      p = partitions[i].part;
      if (!p->sys_ind) {
        if (i != 4 || i + 1 < num_parts)
          xprintf("Warning: partition %u is empty\n", i + 1);
      } else if (begin_sec[i] < extended_offset || end_sec[i] > e_last)
        xprintf("Logical partition %u not entirely in partition %u\n", i + 1, ext_idx + 1);
    }
  }
  if (total > g_heads * g_sectors * g_cylinders)
    xprintf("Total allocated sectors %lld greater than the maximum "
        "%lu\n", total, g_heads * g_sectors * g_cylinders);
  else {
    total = g_heads * g_sectors * g_cylinders - total;
    if (total) xprintf("%lld unallocated sectors\n", total);
  }
}

static void move_begning(int idx)
{
  sector_t start, num, new_start, end;
  char mesg[256];
  struct part_entry *pe = &partitions[idx];
  struct partition *p = pe->part;

  if (chs_warn()) return;
  start = swap_le32toh(p->start4) + pe->start_offset;
  num = swap_le32toh(p->size4);
  end = start + num -1;

  if (!num || IS_EXTENDED(p->sys_ind)) {
    xprintf("Partition %u doesn't have data area\n", idx+1);
    return;
  }
  sprintf(mesg, "New beginning of data (0 - %lld, default %lld): ",
      (long long int)(end), (long long int)(start));
  new_start = ask_value(mesg, 0, end, start);
  if (new_start != start) {
    set_levalue(p->start4, new_start - pe->start_offset);
    set_levalue(p->size4, end - new_start +1);
    if ((read_input("Recalculate C/H/S (Y/n): ", NULL) | 0x20) == 'y')
      set_hsc(p, new_start, end);
    pe->modified = 1;
  }
}

static void print_raw_sectors()
{
  int i, j;
  struct part_entry *pe;

  xprintf("Device: %s\n", disk_device);
  for (i = 3; i < num_parts; i++) {
    pe = &partitions[i];
    for (j = 0; j < g_sect_size; j++) {
      if (!(j % 16)) xprintf("\n0x%03X: ",j);
      xprintf("%02X ",pe->sec_buffer[j]);
    }
    xputc('\n');
  }
}

static void print_partitions_list(int ext)
{
  int i;
  struct part_entry *pe;
  struct partition *p;

  xprintf("Disk %s: %lu heads, %lu sectors, %lu cylinders\n\n", disk_device, g_heads, g_sectors, g_cylinders);
  xprintf("Nr AF  Hd Sec  Cyl  Hd Sec  Cyl      Start       Size ID\n");

  for (i = 0; i < num_parts; i++) {
    pe = &partitions[i];
    p = pe->part;
    if (p) {
      if (ext && (i >= 4)) p = pe->part + 1;
      if(ext && i < 4 && !IS_EXTENDED(p->sys_ind)) continue;

      xprintf("%2u %02x%4u%4u%5u%4u%4u%5u%11u%11u %02x\n",
          i+1, p->boot_ind, p->head,
          sector(p->sector), cylinder(p->sector, p->cyl),
          p->end_head,
          sector(p->end_sector), cylinder(p->end_sector, p->end_cyl),
          swap_le32toh(p->start4),
          swap_le32toh(p->size4),
          p->sys_ind);
      if (p->sys_ind) consistency_check(p, i);
    }
  }
}

//fix the partition table order to ascending
static void fix_order(void)
{
  sector_t first[num_parts], min;
  int i, j, oj, ojj, sj, sjj;
  struct part_entry *pe;
  struct partition *px, *py, temp, *pj, *pjj, tmp;

  for (i = 0; i < num_parts; i++) {
    pe = &partitions[i];
    px = pe->part;
    if (is_partition_clear(px)) first[i] = 0xffffffff;
    else first[i] = swap_le32toh(px->start4) + pe->start_offset;
  }

  if (!check_order()) {
    xprintf("Ordering is already correct\n\n");
    return;
  }
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 3; j++) {
      if (first[j] > first[j+1]) {
        py = partitions[j+1].part;
        px = partitions[j].part;
        memcpy(&temp, py, sizeof(struct partition));
        memcpy(py, px, sizeof(struct partition));
        memcpy(px, &temp, sizeof(struct partition));
        min = first[j+1];
        first[j+1] = first[j];
        first[j] = min;
        partitions[j].modified = 1;
      }
    }
  }
  for (i = 5; i < num_parts; i++) {
    for (j = 5; j < num_parts - 1; j++) {
      oj = partitions[j].start_offset;
      ojj = partitions[j+1].start_offset;
      if (oj > ojj) {
        partitions[j].start_offset = ojj;
        partitions[j+1].start_offset = oj;
        pj = partitions[j].part;
        set_levalue(pj->start4, swap_le32toh(pj->start4)+oj-ojj);
        pjj = partitions[j+1].part;
        set_levalue(pjj->start4, swap_le32toh(pjj->start4)+ojj-oj);
        set_levalue((partitions[j-1].part+1)->start4, ojj-extended_offset);
        set_levalue((partitions[j].part+1)->start4, oj-extended_offset);
      }
    }
  }
  for (i = 4; i < num_parts; i++) {
    for (j = 4; j < num_parts - 1; j++) {
      pj = partitions[j].part;
      pjj = partitions[j+1].part;
      sj = swap_le32toh(pj->start4);
      sjj = swap_le32toh(pjj->start4);
      oj = partitions[j].start_offset;
      ojj = partitions[j+1].start_offset;
      if (oj+sj > ojj+sjj) {
        tmp = *pj;
        *pj = *pjj;
        *pjj = tmp;
        set_levalue(pj->start4, ojj+sjj-oj);
        set_levalue(pjj->start4, oj+sj-ojj);
      }
    }
  }
  // If anything changed
  for (j = 4; j < num_parts; j++) partitions[j].modified = 1;
  xprintf("Done!\n");
}

static void print_menu(void)
{
  xprintf("a\ttoggle a bootable flag\n"
  "b\tedit bsd disklabel\n"
  "c\ttoggle the dos compatibility flag\n"
  "d\tdelete a partition\n"
  "l\tlist known partition types\n"
  "n\tadd a new partition\n"
  "o\tcreate a new empty DOS partition table\n"
  "p\tprint the partition table\n"
  "q\tquit without saving changes\n"
  "s\tcreate a new empty Sun disklabel\n"
  "t\tchange a partition's system id\n"
  "u\tchange display/entry units\n"
  "v\tverify the partition table\n"
  "w\twrite table to disk and exit\n"
  "x\textra functionality (experts only)\n");
}

static void print_xmenu(void)
{
  xprintf("b\tmove beginning of data in a partition\n"
  "c\tchange number of cylinders\n"
  "d\tprint the raw data in the partition table\n"
  "e\tlist extended partitions\n"
  "f\tfix partition order\n"
  "h\tchange number of heads\n"
  "p\tprint the partition table\n"
  "q\tquit without saving changes\n"
  "r\treturn to main menu\n"
  "s\tchange number of sectors/track\n"
  "v\tverify the partition table\n"
  "w\twrite table to disk and exit\n");
}

static void expert_menu(void)
{
  int choice, idx;
  sector_t value;
  char mesg[256];

  while (1) {
    xputc('\n');
    char *msg = "Expert Command ('m' for help): ";
    choice = 0x20 | read_input(msg, NULL);
    switch (choice) {
      case 'b': //move data beginning in partition
        idx = ask_partition(num_parts);
        move_begning(idx - 1);
        break;
      case 'c': //change cylinders
          sprintf(mesg, "Number of cylinders (1 - 1048576, default %lu): ", g_cylinders);
          value = ask_value(mesg, 1, 1048576, g_cylinders);
          g_cylinders = TT.cylinders = value;
          toys.optflags |= FLAG_C;
          if(g_cylinders > ONE_K)
            xprintf("\nThe number of cylinders for this disk is set to %lu.\n"
                "There is nothing wrong with that, but this is larger than 1024,\n"
                "and could in certain setups cause problems.\n", g_cylinders);
        break;
      case 'd': //print raw data in part tables
        print_raw_sectors();
        break;
      case 'e': //list extended partitions
        print_partitions_list(1);
        break;
      case 'f': //fix part order
        fix_order();
        break;
      case 'h': //change number of heads
          sprintf(mesg, "Number of heads (1 - 256, default %lu): ", g_heads);
          value = ask_value(mesg, 1, 256, g_heads);
          g_heads = TT.heads = value;
          toys.optflags |= FLAG_H;
        break;
      case 'p': //print partition table
        print_partitions_list(0);
        break;
      case 'q':
        free_bufs();
        close(dev_fd);
        xputc('\n');
        exit(0);
        break;
      case 'r':
        return;
        break;
      case 's': //change sector/track
          sprintf(mesg, "Number of sectors (1 - 63, default %lu): ", g_sectors);
          value = ask_value(mesg, 1, 63, g_sectors);
          g_sectors = TT.sectors = value;
          toys.optflags |= FLAG_H;
        break;
      case 'v':
        verify_table();
        break;
      case 'w':
        write_table();
        toys.exitval = 0;
        exit(0);
        break;
      case 'm':
        print_xmenu();
        break;
      default:
        xprintf("Unknown command '%c'\n",choice);
        print_xmenu();
        break;
    }
  } //while(1)
}

static int disk_proper(const char *device)
{
  unsigned length;
  int fd = open(device, O_RDONLY);

  if (fd != -1) {
    struct hd_geometry dev_geo;
    dev_geo.heads = 0;
    dev_geo.sectors = 0;
    int err = ioctl(fd, HDIO_GETGEO, &dev_geo);
    close(fd);
    if (!err) return (dev_geo.start == 0);
  }
  length = strlen(device);
  if (length != 0 && isdigit(device[length - 1])) return 0;
  return 1;
}

static void reset_entries()
{
  int i;

  memset(MBRbuf, 0, sizeof(MBRbuf));
  for (i = 4; i < num_parts; i++)
    memset(&partitions[i], 0, sizeof(struct part_entry));
}

//this will keep dev_fd = 3 always alive
static void move_fd()
{
  int fd = xopen("/dev/null", O_RDONLY);
  if(fd != dev_fd) {
    if(dup2(fd, dev_fd) != dev_fd) perror_exit("Can't dup2");
    close(fd);
  }
}

/* Read proc/partitions and then print the details
 * for partitions on each device
 */
static void read_and_print_parts()
{
  unsigned int ma, mi, sz;
  char *name = toybuf, *buffer = toybuf + ONE_K, *device = toybuf + 2048;
  FILE* fp = xfopen("/proc/partitions", "r");

  while (fgets(buffer, ONE_K, fp)) {
    reset_entries();
    num_parts = 4;
    memset(name, 0, sizeof(*name));
    if (sscanf(buffer, " %u %u %u %[^\n ]", &ma, &mi, &sz, name) != 4)
      continue;

    sprintf(device,"/dev/%s",name);
    if (disk_proper(device)) {
      if (read_mbr(device, 0)) continue;
      print_mbr(1);
      move_fd();
    }
  }
  fclose(fp);
}

void fdisk_main(void)
{
  int choice, p;

  init_members();
  move_fd();
  if (TT.heads >= 256) TT.heads = 0;
  if (TT.sectors >= 64) TT.sectors = 0;
  if (FLAG(u)) disp_unit_cyl = 0;
  if (FLAG(l)) {
    if (!toys.optc) read_and_print_parts();
    else {
      while(*toys.optargs){
        if (read_mbr(*toys.optargs, 0)) {
          toys.optargs++;
          continue;
        }
        print_mbr(1);
        move_fd();
        toys.optargs++;
      }
    }
    toys.exitval = 0;
    return;
  } else {
    if (toys.optc != 1) help_exit(0);
    if (read_mbr(toys.optargs[0], 1)) return;
    while (1) {
      xputc('\n');
      char *msg = "Command ('m' for help): ";
      choice = 0x20 | read_input(msg, NULL);
      switch (choice) {
        case 'a':
          p = ask_partition(num_parts);
          toggle_active_flag(p - 1); //partition table index start from 0.
          break;
        case 'b':
          break;
        case 'c':
          dos_flag = !dos_flag;
          xprintf("Dos compatible flag is %s\n", dos_flag?"Set" : "Not set");
          break;
        case 'd':
          p = get_non_free_partition(num_parts); //4 was here
          if(p >= 0) delete_partition(p);
          break;
        case 'l':
          list_types();
          break;
        case 'n': //add new partition
          add_new_partition();
          break;
        case 'o':
          create_empty_doslabel();
          break;
        case 'p':
          print_mbr(0);
          break;
        case 'q':
          free_bufs();
          close(dev_fd);
          xputc('\n');
          exit(0);
          break;
        case 's':
          break;
        case 't':
          change_systype();
          break;
        case 'u':
          disp_unit_cyl = !disp_unit_cyl;
          xprintf("Changing Display/Entry units to %s\n",disp_unit_cyl?"cylinders" : "sectors");
          break;
        case 'v':
          verify_table();
          break;
        case 'w':
          write_table();
          toys.exitval = 0;
          return;
          break;
        case 'x':
          expert_menu();
          break;
        case 'm':
          print_menu();
          break;
        default:
          xprintf("%c: Unknown command\n",choice);
          break;
      }
    } //while(1)
  }
}
