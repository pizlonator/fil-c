/* eject.c - eject device.
 *
 * Copyright 2012 Harvind Singh <harvindsingh1981@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gamil.com>
 *
 * No standard.

USE_EJECT(NEWTOY(eject, ">1stT[!tT]", TOYFLAG_USR|TOYFLAG_BIN))

config EJECT
  bool "eject"
  default y
  help
    usage: eject [-stT] [DEVICE]

    Eject DEVICE or default /dev/cdrom

    -s	SCSI device
    -t	Close tray
    -T	Open/close tray (toggle)
*/

#define FOR_eject
#include "toys.h"
#include <scsi/sg.h>
#include <scsi/scsi.h>
#include <linux/cdrom.h>

// SCSI's overcomplicated way of requesting eject
static void remove_scsi(int fd)
{
  unsigned i;
  sg_io_hdr_t *header = (sg_io_hdr_t *)(toybuf+64);
  char sg_driver_cmd[][6] = {
    { ALLOW_MEDIUM_REMOVAL, 0, 0, 0, 0, 0 },
    { START_STOP, 0, 0, 0, 1, 0 }, //start the motor
    { START_STOP, 0, 0, 0, 2, 0 } //eject the media
  };

  header->interface_id = 'S';
  header->cmd_len = 6;
  header->mx_sb_len = 32;
  header->dxfer_direction = SG_DXFER_NONE;
  header->dxferp = toybuf + 32;
  header->sbp = (void *)toybuf;
  header->timeout = 2000;

  for (i = 0; i < ARRAY_LEN(sg_driver_cmd); i++) {
    header->cmdp = (void *)sg_driver_cmd[i];
    xioctl(fd, SG_IO, header);
  }

  // force kernel to reread partition table when new disc is inserted
  ioctl(fd, BLKRRPART);
}

void eject_main(void)
{
  int fd = xopen(*toys.optargs ? : "/dev/cdrom", O_RDONLY | O_NONBLOCK);

  if (FLAG(s)) remove_scsi(fd);
  else if (FLAG(T) && CDS_TRAY_OPEN == ioctl(fd, CDROM_DRIVE_STATUS, toybuf))
    xioctl(fd, CDROMCLOSETRAY, toybuf);
  else xioctl(fd, CDROMEJECT, toybuf);
  if (CFG_TOYBOX_FREE) xclose(fd);
}
