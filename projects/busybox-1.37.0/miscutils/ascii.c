/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2021 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config ASCII
//config:	bool "ascii (784 bytes)"
//config:	default y
//config:	help
//config:	Print ascii table.
//config:

//applet:IF_ASCII(APPLET(ascii, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_ASCII) += ascii.o

//usage:#define ascii_trivial_usage NOUSAGE_STR
//usage:#define ascii_full_usage ""

#include "libbb.h"

int ascii_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ascii_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	const char *ctrl =
		"NUL""SOH""STX""ETX""EOT""ENQ""ACK""BEL"
		"BS ""HT ""NL ""VT ""FF ""CR ""SO ""SI "
		"DLE""DC1""DC2""DC3""DC4""NAK""SYN""ETB"
		"CAN""EM ""SUB""ESC""FS ""GS ""RS ""US "
	;
//TODO: od has a similar table, can we reuse it?
	char last[2];
	unsigned i;

	last[1] = '\0';
	printf("Dec Hex    Dec Hex    Dec Hex  Dec Hex  Dec Hex  Dec Hex   Dec Hex   Dec Hex\n");
	for (i = 0; i < 16; i++) {
		printf("%3u %02x %.3s%4u %02x %.3s%4u %02x %c%4u %02x %c%4u %02x %c%4u %02x %c%5u %02x %c%5u %02x %s\n",
			i+0x00, i+0x00, ctrl + i*3,
			i+0x10, i+0x10, ctrl + i*3 + 16*3,
			i+0x20, i+0x20, i+0x20,
			i+0x30, i+0x30, i+0x30,
			i+0x40, i+0x40, i+0x40,
			i+0x50, i+0x50, i+0x50,
			i+0x60, i+0x60, i+0x60,
			i+0x70, i+0x70, (i+0x70 == 0x7f ? "DEL" : (last[0] = i+0x70, last))
		);
	}
	return EXIT_SUCCESS;
}
