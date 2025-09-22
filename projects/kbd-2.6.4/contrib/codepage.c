/*
 * CPI.C: A program to examine MSDOS codepage files (*.cpi)
 * and extract specific codepages.
 * Compiles under Linux & DOS (using BC++ 3.1).
 *
 * Compile: gcc -o cpi cpi.c
 * Call: codepage [-a|-l|nnn] file.cpi
 *
 * Author: Ahmed M. Naas (ahmed@oea.xs4all.nl)
 * Many changes: aeb@cwi.nl  [changed until it would handle all
 *	*.cpi files people have sent me; I have no documentation,
 *	so all this is experimental]
 * Remains to do: DRDOS fonts.
 *
 * Copyright: Public domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int handle_codepage(int);
void handle_fontfile(void);

#define PACKED __attribute__((packed))
/* Use this (instead of the above) to compile under MSDOS */
/*#define PACKED  */

struct {
	unsigned char id0 PACKED;
	unsigned char id[7] PACKED;
	unsigned char res[8] PACKED;
	unsigned short pnum PACKED;      /* number of pointers */
	unsigned char ptyp PACKED;       /* type of pointers */
	unsigned long fih_offset PACKED; /* FontInfoHeader offset */
} FontFileHeader;

int drfont = 0;

#define N 4
struct {
	unsigned char num_fonts PACKED; /* N = 4 fonts per code page*/
	unsigned char font_height[N] PACKED;
	unsigned long dfd_offset[N] PACKED; /* DisplayFontData offset */
} DRDOS_ExtendedFontFileHeader;

struct {
	unsigned short num_codepages PACKED;
} FontInfoHeader;

struct {
	unsigned short size PACKED;
	unsigned long off_nexthdr PACKED;
	unsigned short device_type PACKED; /* screen=1; printer=2 */
	unsigned char device_name[8] PACKED;
	unsigned short codepage PACKED;
	unsigned char res[6] PACKED;
	unsigned long off_font PACKED;
} CPEntryHeader;

struct {
	unsigned short reserved PACKED;
	unsigned short num_fonts PACKED;
	unsigned short size PACKED;
} CPInfoHeader;

struct {
	unsigned char height PACKED;
	unsigned char width PACKED;
	unsigned short reserved PACKED;
	unsigned short num_chars PACKED;
} ScreenFontHeader;

struct {
	unsigned short printer_type PACKED;
	unsigned short seqlength PACKED;
} PrinterFontHeader;

FILE *in, *out;
void usage(void);

int opta, optc, optl, optL, optx;
extern int optind;
extern char *optarg;

unsigned short codepage;

int main(int argc, char *argv[])
{
	if (argc < 2)
		usage();

	opta = optc = optl = optL = optx = 0;

	if (argc == 2)
		optl = 1;
	else
		while (1) {
			switch (getopt(argc, argv, "alLc")) {
				case 'a':
					opta = 1;
					continue;
				case 'c':
					optc = 1;
					continue;
				case 'L':
					optL = 1;
					continue;
				case 'l':
					optl = 1;
					continue;
				case '?':
				default:
					usage();
				case -1:
					break;
			}
			break;
		}

	if (optind < argc) {
		if ((in = fopen(argv[optind], "r")) == NULL) {
			printf("\nUnable to open file %s.\n", argv[optind]);
			exit(0);
		}
		optind++;
	} else
		usage();

	if (optind != argc) {
		if (optind != argc - 1 || opta)
			usage();
		codepage = atoi(argv[optind]);
		optx     = 1;
	}

	if (optc)
		handle_codepage(0);
	else
		handle_fontfile();

	if (optx) {
		printf("no page %d found\n", codepage);
		exit(1);
	}

	fclose(in);
	return (0);
}

void handle_fontfile()
{
	int i, j;

	j = fread(&FontFileHeader, 1, sizeof(FontFileHeader), in);
	if (j != sizeof(FontFileHeader)) {
		printf("error reading FontFileHeader - got %d chars\n", j);
		exit(1);
	}
	if (optL)
		printf("FontFileHeader: id=0x%x \"%7.7s\" res=%8.8s "
		       "num=%d typ=%d fih_offset=%ld\n\n",
		       FontFileHeader.id0, FontFileHeader.id, FontFileHeader.res,
		       FontFileHeader.pnum, FontFileHeader.ptyp,
		       FontFileHeader.fih_offset);

	if (!strcmp(FontFileHeader.id, "DRFONT ")) {
		drfont = 1;
		j      = fread(&DRDOS_ExtendedFontFileHeader, 1,
		          sizeof(DRDOS_ExtendedFontFileHeader), in);
		if (j != sizeof(DRDOS_ExtendedFontFileHeader)) {
			printf("error reading ExtendedFontFileHeader - "
			       "got %d chars\n",
			       j);
			exit(1);
		}
		if (DRDOS_ExtendedFontFileHeader.num_fonts != N) {
			printf("found %d instead of 4 fonts in drfont\n",
			       DRDOS_ExtendedFontFileHeader.num_fonts);
			exit(1);
		}
		if (optL) {
			printf("ExtendedFontFileHeader:\n");
			for (j = 0; j < N; j++) {
				printf("font%d: height %d dfd_offset %d\n", j,
				       DRDOS_ExtendedFontFileHeader.font_height[j],
				       DRDOS_ExtendedFontFileHeader.dfd_offset[j]);
			}
			printf("\n");
		}
	}

	j = fread(&FontInfoHeader, 1, sizeof(FontInfoHeader), in);
	if (j != sizeof(FontInfoHeader)) {
		printf("error reading FontInfoHeader - got %d chars\n", j);
		exit(1);
	}
	if (optL)
		printf("FontInfoHeader: num_codepages=%d\n\n",
		       FontInfoHeader.num_codepages);

#if 1
	if (drfont) {
		printf("this program cannot handle DRDOS font files\n");
		exit(1);
	}
#endif

	for (i = FontInfoHeader.num_codepages; i; i--)
		if (handle_codepage(i - 1))
			break;
}

int handle_codepage(int more_to_come)
{
	int j;
	char outfile[20];
	unsigned char *fonts;
	long inpos, nexthdr;

	j = fread(&CPEntryHeader, 1, sizeof(CPEntryHeader), in);
	if (j != sizeof(CPEntryHeader)) {
		printf("error reading CPEntryHeader - got %d chars\n", j);
		exit(1);
	}
	if (optL) {
		int t = CPEntryHeader.device_type;
		printf("CPEntryHeader: size=%d dev=%d [%s] name=%8.8s "
		       "codepage=%d\n\t\tres=%6.6s nxt=%ld off_font=%ld\n\n",
		       CPEntryHeader.size,
		       t, (t == 1) ? "screen" : (t == 2) ? "printer" : "?",
		       CPEntryHeader.device_name,
		       CPEntryHeader.codepage,
		       CPEntryHeader.res,
		       CPEntryHeader.off_nexthdr, CPEntryHeader.off_font);
	} else if (optl) {
		printf("\nCodepage = %d\n", CPEntryHeader.codepage);
		printf("Device = %.8s\n", CPEntryHeader.device_name);
	}
#if 0
	if (CPEntryHeader.size != sizeof(CPEntryHeader)) {
	    /* seen 26 and 28, so that the difference below is -2 or 0 */
	    if (optl)
	      printf("Skipping %d bytes of garbage\n",
		     CPEntryHeader.size - sizeof(CPEntryHeader));
	    fseek(in, CPEntryHeader.size - sizeof(CPEntryHeader),
		  SEEK_CUR);
	}
#endif
	if (!opta && (!optx || CPEntryHeader.codepage != codepage) && !optc)
		goto next;

	inpos = ftell(in);
	if (inpos != CPEntryHeader.off_font && !optc) {
		if (optL)
			printf("pos=%ld font at %ld\n", inpos, CPEntryHeader.off_font);
		fseek(in, CPEntryHeader.off_font, SEEK_SET);
	}

	j = fread(&CPInfoHeader, 1, sizeof(CPInfoHeader), in);
	if (j != sizeof(CPInfoHeader)) {
		printf("error reading CPInfoHeader - got %d chars\n", j);
		exit(1);
	}
	if (optl) {
		printf("Number of Fonts = %d\n", CPInfoHeader.num_fonts);
		printf("Size of Bitmap = %d\n", CPInfoHeader.size);
	}
	if (CPInfoHeader.num_fonts == 0)
		goto next;
	if (optc)
		return 0;

	sprintf(outfile, "%d.cp", CPEntryHeader.codepage);
	if ((out = fopen(outfile, "w")) == NULL) {
		printf("\nUnable to open file %s.\n", outfile);
		exit(1);
	} else
		printf("\nWriting %s\n", outfile);

	fonts = (unsigned char *)malloc(CPInfoHeader.size);

	fread(fonts, CPInfoHeader.size, 1, in);
	fwrite(&CPEntryHeader, sizeof(CPEntryHeader), 1, out);
	fwrite(&CPInfoHeader, sizeof(CPInfoHeader), 1, out);
	j = fwrite(fonts, 1, CPInfoHeader.size, out);
	if (j != CPInfoHeader.size) {
		printf("error writing %s - wrote %d chars\n", outfile, j);
		exit(1);
	}
	fclose(out);
	free(fonts);
	if (optx)
		exit(0);
next:
	/*
	 * It seems that if entry headers and fonts are interspersed,
	 * then nexthdr will point past the font, regardless of
	 * whether more entries follow.
	 * Otherwise, first all entry headers are given, and then
	 * all fonts; in this case nexthdr will be 0 in the last entry.
	 */
	nexthdr = CPEntryHeader.off_nexthdr;
	if (nexthdr == 0 || nexthdr == -1) {
		if (more_to_come) {
			printf("more codepages expected, but nexthdr=%ld\n",
			       nexthdr);
			exit(1);
		} else
			return 1;
	}

	inpos = ftell(in);
	if (inpos != CPEntryHeader.off_nexthdr) {
		if (optL)
			printf("pos=%ld nexthdr at %ld\n", inpos, nexthdr);
		if (opta && !more_to_come) {
			printf("no more code pages, but nexthdr != 0\n");
			return 1;
		}

		fseek(in, CPEntryHeader.off_nexthdr, SEEK_SET);
	}

	return 0;
}

void usage(void)
{
	printf("\nUsage: cpi code_page_file [-c] [-L] [-l] [-a|nnn]\n");
	printf(" -c: input file is a single codepage\n");
	printf(" -L: print header info (you don't want to see this)\n");
	printf(" -l or no option: list all codepages contained in the file\n");
	printf(" -a: extract all codepages from the file\n");
	printf(" nnn (3 digits): extract codepage nnn from the file\n");
	printf("Example: cpi ega.cpi 850 \n");
	printf(" will create a file 850.cp containing the requested codepage.\n\n");
	exit(1);
}
