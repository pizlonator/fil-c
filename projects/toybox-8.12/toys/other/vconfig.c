/* vconfig.c - Creates virtual ethernet devices.
 *
 * Copyright 2012 Sandeep Sharma <sandeep.jack2756@gmail.com>
 * Copyright 2012 Kyungwan Han <asura321@gmail.com>
 *
 * No standard

USE_VCONFIG(NEWTOY(vconfig, "<2>4", TOYFLAG_NEEDROOT|TOYFLAG_SBIN))

config VCONFIG
  bool "vconfig"
  default y
  help
    usage: vconfig COMMAND [OPTIONS]

    Create and remove virtual ethernet devices

    add             [interface-name] [vlan_id]
    rem             [vlan-name]
    set_flag        [interface-name] [flag-num]       [0 | 1]
    set_egress_map  [vlan-name]      [skb_priority]   [vlan_qos]
    set_ingress_map [vlan-name]      [skb_priority]   [vlan_qos]
    set_name_type   [name-type]
*/

#include "toys.h"
#include <linux/if_vlan.h>
#include <linux/sockios.h>

void vconfig_main(void)
{
  struct vlan_ioctl_args request;
  char *cmd = *toys.optargs;
  int fd = xsocket(AF_INET, SOCK_STREAM, 0);

  memset(&request, 0, sizeof(struct vlan_ioctl_args));

  if (!strcmp(cmd, "set_name_type")) {
    char *types[] = {"VLAN_PLUS_VID", "DEV_PLUS_VID", "VLAN_PLUS_VID_NO_PAD",
                     "DEV_PLUS_VID_NO_PAD"};
    int i, j = ARRAY_LEN(types);

    for (i=0; i<j; i++) if (!strcmp(toys.optargs[1], types[i])) break;
    if (i == j) {
      for (i=0; i<j; i++) puts(types[i]);
      error_exit("%s: unknown '%s'", cmd, toys.optargs[1]);
    }

    request.u.name_type = i;
    request.cmd = SET_VLAN_NAME_TYPE_CMD;
    xioctl(fd, SIOCSIFVLAN, &request);

    return;
  }

  // Store interface name
  xstrncpy(request.device1, toys.optargs[1], sizeof(request.device1));

  if (!strcmp(cmd, "add")) {
    request.cmd = ADD_VLAN_CMD;
    if (toys.optargs[2]) request.u.VID = atolx_range(toys.optargs[2], 0, 4094);
    if (request.u.VID == 1)
      xprintf("WARNING: VLAN 1 does not work with many switches.\n");
  } else if (!strcmp(cmd, "rem")) request.cmd = DEL_VLAN_CMD;
  else if (!strcmp(cmd, "set_flag")) {
    request.cmd = SET_VLAN_FLAG_CMD;
    if (toys.optargs[2]) request.u.flag = atolx_range(toys.optargs[2], 0, 1);
    if (toys.optargs[3]) request.vlan_qos = atolx_range(toys.optargs[3], 0, 7);
  } else if(strcmp(cmd, "set_egress_map") == 0) {
    request.cmd = SET_VLAN_EGRESS_PRIORITY_CMD;
    if (toys.optargs[2])
      request.u.skb_priority = atolx_range(toys.optargs[2], 0, INT_MAX);
    if (toys.optargs[3]) request.vlan_qos = atolx_range(toys.optargs[3], 0, 7);
  } else if(strcmp(cmd, "set_ingress_map") == 0) {
    request.cmd = SET_VLAN_INGRESS_PRIORITY_CMD;
    if (toys.optargs[2])
      request.u.skb_priority = atolx_range(toys.optargs[2], 0, INT_MAX);
    //To set flag we must have to set vlan_qos
    if (toys.optargs[3]) request.vlan_qos = atolx_range(toys.optargs[3], 0, 7);
  } else {
    xclose(fd);
    perror_exit("Unknown command %s", cmd);
  }

  xioctl(fd, SIOCSIFVLAN, &request);
  xprintf("Successful %s on device %s\n", cmd, toys.optargs[1]);
}
