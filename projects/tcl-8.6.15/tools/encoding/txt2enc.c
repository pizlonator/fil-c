/*
 * txt2enc.c --
 *
 *	Simple program to compile up the encodings tables from the CD that
 *	came with "The Unicode Standard, Version 2.0" into a form that can
 *	be quickly loaded into Tcl.
 *
 * Copyright (c) 1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) txt2enc.c 1.1 98/01/28 11:42:09
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

typedef unsigned short Rune;

int
main(int argc, char **argv)
{
    FILE *fp;
    Rune *toUnicode[256];
    int i, multiByte, enc, uni, hi, lo, fixmissing, used, maxEnc;
    int ch, encColumn, uniColumn, fallbackKnown, width;
    char *fallbackString, *str, *rest, *dot;
    unsigned int magic, type, symbol, fallbackChar;
    Rune rune;
    char buf[256];
    extern char *optarg;
    extern int optind, opterr;
    static char *typeString[] = {"single", "double", "multi"};

    encColumn = 0;
    uniColumn = 1;
    fallbackString = "QUESTION MARK";
    fallbackChar = '\0';
    fallbackKnown = 0;
    type = -1;
    symbol = 0;
    fixmissing = 1;

    opterr = 0;
    while (1) {
	ch = getopt(argc, argv, "e:u:f:t:sm");
	if (ch == -1) {
	    break;
	}
	switch (ch) {
	case 'e':
	    encColumn = atoi(optarg);
	    break;

	case 'u':
	    uniColumn = atoi(optarg);
	    break;

	case 'f':
	    fallbackKnown = 1;
	    if (optarg[1] == '\0') {
		fallbackChar = optarg[0];
	    } else  {
		fallbackChar = strtol(optarg, &rest, 16);
		if (*rest != '\0') {
		    fallbackChar = '\0';
		    fallbackKnown = 0;
		    fallbackString = optarg;
		}
	    }

	case 't':
	    if (strcmp(optarg, "single") == 0) {
		type = 0;
	    } else if (strcmp(optarg, "double") == 0) {
		type = 1;
	    } else if (strcmp(optarg, "multi") == 0) {
		type = 2;
	    } else {
		goto usage;
	    }
	    break;

	case 's':
	    symbol = 1;
	    break;

	case 'm':
	    fixmissing = 0;
	    break;

	default:
	    goto usage;
	}
    }

    if ((optind < argc - 1) || (optind >= argc)) {
	usage:
	fputs("usage: mkencoding [-e column] [-u column] [-f fallback] [-t type] [-s] [-m] file\n", stderr);
	fputs("    -e\tcolumn containing characters in encoding (default: 0)\n", stderr);
	fputs("    -u\tcolumn containing characters in Unicode (default: 1)\n", stderr);
	fputs("    -f\tfallback character (default: QUESTION MARK)\n", stderr);
	fputs("    -t\toverride implicit type with single, double, or multi\n", stderr);
	fputs("    -s\tsymbol+ascii encoding\n", stderr);
	fputs("    -m\tdon't implicitly include range 0080 to 00FF\n", stderr);
	return 1;
    }

    fp = fopen(argv[argc - 1], "r");
    if (fp == NULL) {
        perror(argv[argc - 1]);
	return 1;
    }

    for (i = 0; i < 256; i++) {
        toUnicode[i] = NULL;
    }

    maxEnc = 0;
    width = 0;
    multiByte = 0;
    while (fgets(buf, sizeof(buf), fp) != NULL) {
	str = buf;
	enc = -1;
	uni = -1;
	while (isspace(*str)) {
	    str++;
	}
	if (str[0] == '#') {
	    continue;
	}
	for (i = 0; *str != '\0'; i++) {
	    if (*str == '#') {
		if (fallbackKnown == 0) {
		    str++;
		    while (isspace(*str)) {
			str++;
		    }
		    str[strlen(str) - 1] = '\0';
		    if (strcmp(str, fallbackString) == 0) {
			fallbackChar = enc;
			fallbackKnown = 1;
		    } else if (strstr(str, fallbackString) != NULL) {
			fallbackChar = enc;
		    }
		}
		break;
	    } else {
		rune = strtol(str, &rest, 16);
		if (rest == str) {
		    rest++;
		} else if (i == uniColumn) {
		    uni = rune;
		} else if (i == encColumn) {
		    enc = rune;
		    if ((width != 0) && (width != rest - str)) {
			multiByte = 1;
		    }
		    width = rest - str;
		    if (enc > maxEnc) {
			maxEnc = enc;
		    }
		}
	    }
	    while (isspace(*rest)) {
		rest++;
	    }
	    str = rest;
	}
	if (enc < 32 || uni < 32) {
	    continue;
	}

	hi = enc >> 8;
	lo = enc & 0xFF;
	if (toUnicode[hi] == NULL) {
	    toUnicode[hi] = (Rune *) malloc(256 * sizeof(Rune));
	    memset(toUnicode[hi], 0, 256 * sizeof(Rune));
	}
	toUnicode[hi][lo] = uni;
    }

    fclose(fp);

    dot = strrchr(argv[argc - 1], '.');
    if (dot != NULL) {
	*dot = '\0';
    }
    if (type == -1) {
	if (multiByte) {
	    type = 2;
	} else if (maxEnc > 255) {
	    type = 1;
	} else {
	    type = 0;
	}
    }
    if (type != 1) {
	if (toUnicode[0] == NULL) {
	    toUnicode[0] = (Rune *) malloc(256 * sizeof(Rune));
	    memset(toUnicode[0], 0, 256 * sizeof(Rune));
	}
	for (i = 0; i < 0x20; i++) {
	    toUnicode[0][i] = i;
	}
	if (fixmissing) {
	    for (i = 0x7F; i < 0xA0; i++) {
		if (toUnicode[i] == NULL && toUnicode[0][i] == 0) {
		    toUnicode[0][i] = i;
		}
	    }
	}
    }

    printf("# Encoding file: %s, %s-byte\n", argv[argc - 1], typeString[type]);

    if (fallbackChar == '\0') {
	fallbackChar = '?';
    }
    used = 0;
    for (hi = 0; hi < 256; hi++) {
	if (toUnicode[hi] != NULL) {
	    used++;
	}
    }
    printf("%c\n%04X %d %d\n", "SDM"[type], fallbackChar, symbol, used);

    for (hi = 0; hi < 256; hi++) {
	if (toUnicode[hi] != NULL) {
	    printf("%02X\n", hi);
	    for (lo = 0; lo < 256; lo++) {
		printf("%04X", toUnicode[hi][lo]);
		if ((lo & 0x0F) == 0x0F) {
		    putchar('\n');
		}
	    }
	}
    }

    for (hi = 0; hi < 256; hi++) {
        if (toUnicode[hi] != NULL) {
            free(toUnicode[hi]);
            toUnicode[hi] = NULL;
        }
    }
    return 0;
}
