/*
 * Copyright © 2002 Keith Packard
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "xcursor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

/*
 * From libXcursor/include/X11/extensions/Xcursor.h
 */

#define XcursorTrue	1
#define XcursorFalse	0

/*
 * Cursor files start with a header.  The header
 * contains a magic number, a version number and a
 * table of contents which has type and offset information
 * for the remaining tables in the file.
 *
 * File minor versions increment for compatible changes
 * File major versions increment for incompatible changes (never, we hope)
 *
 * Chunks of the same type are always upward compatible.  Incompatible
 * changes are made with new chunk types; the old data can remain under
 * the old type.  Upward compatible changes can add header data as the
 * header lengths are specified in the file.
 *
 *  File:
 *	FileHeader
 *	LISTofChunk
 *
 *  FileHeader:
 *	CARD32		magic	    magic number
 *	CARD32		header	    bytes in file header
 *	CARD32		version	    file version
 *	CARD32		ntoc	    number of toc entries
 *	LISTofFileToc   toc	    table of contents
 *
 *  FileToc:
 *	CARD32		type	    entry type
 *	CARD32		subtype	    entry subtype (size for images)
 *	CARD32		position    absolute file position
 */

#define XCURSOR_MAGIC	0x72756358  /* "Xcur" LSBFirst */

/*
 * Current Xcursor version number.  Will be substituted by configure
 * from the version in the libXcursor configure.ac file.
 */

#define XCURSOR_LIB_MAJOR 1
#define XCURSOR_LIB_MINOR 1
#define XCURSOR_LIB_REVISION 13
#define XCURSOR_LIB_VERSION	((XCURSOR_LIB_MAJOR * 10000) + \
				 (XCURSOR_LIB_MINOR * 100) + \
				 (XCURSOR_LIB_REVISION))

/*
 * This version number is stored in cursor files; changes to the
 * file format require updating this version number
 */
#define XCURSOR_FILE_MAJOR	1
#define XCURSOR_FILE_MINOR	0
#define XCURSOR_FILE_VERSION	((XCURSOR_FILE_MAJOR << 16) | (XCURSOR_FILE_MINOR))
#define XCURSOR_FILE_HEADER_LEN	(4 * 4)
#define XCURSOR_FILE_TOC_LEN	(3 * 4)

typedef struct _XcursorFileToc {
    XcursorUInt	    type;	/* chunk type */
    XcursorUInt	    subtype;	/* subtype (size for images) */
    XcursorUInt	    position;	/* absolute position in file */
} XcursorFileToc;

typedef struct _XcursorFileHeader {
    XcursorUInt	    magic;	/* magic number */
    XcursorUInt	    header;	/* byte length of header */
    XcursorUInt	    version;	/* file version number */
    XcursorUInt	    ntoc;	/* number of toc entries */
    XcursorFileToc  *tocs;	/* table of contents */
} XcursorFileHeader;

/*
 * The rest of the file is a list of chunks, each tagged by type
 * and version.
 *
 *  Chunk:
 *	ChunkHeader
 *	<extra type-specific header fields>
 *	<type-specific data>
 *
 *  ChunkHeader:
 *	CARD32	    header	bytes in chunk header + type header
 *	CARD32	    type	chunk type
 *	CARD32	    subtype	chunk subtype
 *	CARD32	    version	chunk type version
 */

#define XCURSOR_CHUNK_HEADER_LEN    (4 * 4)

typedef struct _XcursorChunkHeader {
    XcursorUInt	    header;	/* bytes in chunk header */
    XcursorUInt	    type;	/* chunk type */
    XcursorUInt	    subtype;	/* chunk subtype (size for images) */
    XcursorUInt	    version;	/* version of this type */
} XcursorChunkHeader;

/*
 * Here's a list of the known chunk types
 */

/*
 * Comments consist of a 4-byte length field followed by
 * UTF-8 encoded text
 *
 *  Comment:
 *	ChunkHeader header	chunk header
 *	CARD32	    length	bytes in text
 *	LISTofCARD8 text	UTF-8 encoded text
 */

#define XCURSOR_COMMENT_TYPE	    0xfffe0001
#define XCURSOR_COMMENT_VERSION	    1
#define XCURSOR_COMMENT_HEADER_LEN  (XCURSOR_CHUNK_HEADER_LEN + (1 *4))
#define XCURSOR_COMMENT_COPYRIGHT   1
#define XCURSOR_COMMENT_LICENSE	    2
#define XCURSOR_COMMENT_OTHER	    3
#define XCURSOR_COMMENT_MAX_LEN	    0x100000

typedef struct _XcursorComment {
    XcursorUInt	    version;
    XcursorUInt	    comment_type;
    char	    *comment;
} XcursorComment;

/*
 * Each cursor image occupies a separate image chunk.
 * The length of the image header follows the chunk header
 * so that future versions can extend the header without
 * breaking older applications
 *
 *  Image:
 *	ChunkHeader	header	chunk header
 *	CARD32		width	actual width
 *	CARD32		height	actual height
 *	CARD32		xhot	hot spot x
 *	CARD32		yhot	hot spot y
 *	CARD32		delay	animation delay
 *	LISTofCARD32	pixels	ARGB pixels
 */

#define XCURSOR_IMAGE_TYPE    	    0xfffd0002
#define XCURSOR_IMAGE_VERSION	    1
#define XCURSOR_IMAGE_HEADER_LEN    (XCURSOR_CHUNK_HEADER_LEN + (5*4))
#define XCURSOR_IMAGE_MAX_SIZE	    0x7fff	/* 32767x32767 max cursor size */

typedef struct _XcursorFile XcursorFile;

struct _XcursorFile {
    void    *closure;
    int	    (*read)  (XcursorFile *file, unsigned char *buf, int len);
    int	    (*write) (XcursorFile *file, unsigned char *buf, int len);
    int	    (*seek)  (XcursorFile *file, long offset, int whence);
};

typedef struct _XcursorComments {
    int		    ncomment;	/* number of comments */
    XcursorComment  **comments;	/* array of XcursorComment pointers */
} XcursorComments;

/*
 * From libXcursor/src/file.c
 */

static XcursorImage *
XcursorImageCreate (int width, int height)
{
    XcursorImage    *image;

    if (width < 0 || height < 0)
       return NULL;
    if (width > XCURSOR_IMAGE_MAX_SIZE || height > XCURSOR_IMAGE_MAX_SIZE)
       return NULL;

    image = malloc (sizeof (XcursorImage) +
		    width * height * sizeof (XcursorPixel));
    if (!image)
	return NULL;
    image->version = XCURSOR_IMAGE_VERSION;
    image->pixels = (XcursorPixel *) (image + 1);
    image->size = width > height ? width : height;
    image->width = width;
    image->height = height;
    image->delay = 0;
    return image;
}

static void
XcursorImageDestroy (XcursorImage *image)
{
    free (image);
}

static XcursorImages *
XcursorImagesCreate (int size)
{
    XcursorImages   *images;

    images = malloc (sizeof (XcursorImages) +
		     size * sizeof (XcursorImage *));
    if (!images)
	return NULL;
    images->nimage = 0;
    images->images = (XcursorImage **) (images + 1);
    images->name = NULL;
    return images;
}

static void
XcursorImagesDestroy (XcursorImages *images)
{
    int	n;

    if (!images)
        return;

    for (n = 0; n < images->nimage; n++)
	XcursorImageDestroy (images->images[n]);
    if (images->name)
	free (images->name);
    free (images);
}

static XcursorBool
_XcursorReadUInt (XcursorFile *file, XcursorUInt *u)
{
    unsigned char   bytes[4];

    if (!file || !u)
        return XcursorFalse;

    if ((*file->read) (file, bytes, 4) != 4)
	return XcursorFalse;
    *u = ((((unsigned int)(bytes[0])) << 0) |
	  (((unsigned int)(bytes[1])) << 8) |
	  (((unsigned int)(bytes[2])) << 16) |
	  (((unsigned int)(bytes[3])) << 24));
    return XcursorTrue;
}

static void
_XcursorFileHeaderDestroy (XcursorFileHeader *fileHeader)
{
    free (fileHeader);
}

static XcursorFileHeader *
_XcursorFileHeaderCreate (int ntoc)
{
    XcursorFileHeader	*fileHeader;

    if (ntoc > 0x10000)
	return NULL;
    fileHeader = malloc (sizeof (XcursorFileHeader) +
			 ntoc * sizeof (XcursorFileToc));
    if (!fileHeader)
	return NULL;
    fileHeader->magic = XCURSOR_MAGIC;
    fileHeader->header = XCURSOR_FILE_HEADER_LEN;
    fileHeader->version = XCURSOR_FILE_VERSION;
    fileHeader->ntoc = ntoc;
    fileHeader->tocs = (XcursorFileToc *) (fileHeader + 1);
    return fileHeader;
}

static XcursorFileHeader *
_XcursorReadFileHeader (XcursorFile *file)
{
    XcursorFileHeader	head, *fileHeader;
    XcursorUInt		skip;
    unsigned int	n;

    if (!file)
        return NULL;

    if (!_XcursorReadUInt (file, &head.magic))
	return NULL;
    if (head.magic != XCURSOR_MAGIC)
	return NULL;
    if (!_XcursorReadUInt (file, &head.header))
	return NULL;
    if (!_XcursorReadUInt (file, &head.version))
	return NULL;
    if (!_XcursorReadUInt (file, &head.ntoc))
	return NULL;
    skip = head.header - XCURSOR_FILE_HEADER_LEN;
    if (skip)
	if ((*file->seek) (file, skip, SEEK_CUR) == EOF)
	    return NULL;
    fileHeader = _XcursorFileHeaderCreate (head.ntoc);
    if (!fileHeader)
	return NULL;
    fileHeader->magic = head.magic;
    fileHeader->header = head.header;
    fileHeader->version = head.version;
    fileHeader->ntoc = head.ntoc;
    for (n = 0; n < fileHeader->ntoc; n++)
    {
	if (!_XcursorReadUInt (file, &fileHeader->tocs[n].type))
	    break;
	if (!_XcursorReadUInt (file, &fileHeader->tocs[n].subtype))
	    break;
	if (!_XcursorReadUInt (file, &fileHeader->tocs[n].position))
	    break;
    }
    if (n != fileHeader->ntoc)
    {
	_XcursorFileHeaderDestroy (fileHeader);
	return NULL;
    }
    return fileHeader;
}

static XcursorBool
_XcursorSeekToToc (XcursorFile		*file,
		   XcursorFileHeader	*fileHeader,
		   int			toc)
{
    if (!file || !fileHeader || \
        (*file->seek) (file, fileHeader->tocs[toc].position, SEEK_SET) == EOF)
	return XcursorFalse;
    return XcursorTrue;
}

static XcursorBool
_XcursorFileReadChunkHeader (XcursorFile	*file,
			     XcursorFileHeader	*fileHeader,
			     int		toc,
			     XcursorChunkHeader	*chunkHeader)
{
    if (!file || !fileHeader || !chunkHeader)
        return XcursorFalse;
    if (!_XcursorSeekToToc (file, fileHeader, toc))
	return XcursorFalse;
    if (!_XcursorReadUInt (file, &chunkHeader->header))
	return XcursorFalse;
    if (!_XcursorReadUInt (file, &chunkHeader->type))
	return XcursorFalse;
    if (!_XcursorReadUInt (file, &chunkHeader->subtype))
	return XcursorFalse;
    if (!_XcursorReadUInt (file, &chunkHeader->version))
	return XcursorFalse;
    /* sanity check */
    if (chunkHeader->type != fileHeader->tocs[toc].type ||
	chunkHeader->subtype != fileHeader->tocs[toc].subtype)
	return XcursorFalse;
    return XcursorTrue;
}

#define dist(a,b)   ((a) > (b) ? (a) - (b) : (b) - (a))

static XcursorDim
_XcursorFindBestSize (XcursorFileHeader *fileHeader,
		      XcursorDim	size,
		      int		*nsizesp)
{
    unsigned int n;
    int		nsizes = 0;
    XcursorDim	bestSize = 0;
    XcursorDim	thisSize;

    if (!fileHeader || !nsizesp)
        return 0;

    for (n = 0; n < fileHeader->ntoc; n++)
    {
	if (fileHeader->tocs[n].type != XCURSOR_IMAGE_TYPE)
	    continue;
	thisSize = fileHeader->tocs[n].subtype;
	if (!bestSize || dist (thisSize, size) < dist (bestSize, size))
	{
	    bestSize = thisSize;
	    nsizes = 1;
	}
	else if (thisSize == bestSize)
	    nsizes++;
    }
    *nsizesp = nsizes;
    return bestSize;
}

static int
_XcursorFindImageToc (XcursorFileHeader	*fileHeader,
		      XcursorDim	size,
		      int		count)
{
    unsigned int	toc;
    XcursorDim		thisSize;

    if (!fileHeader)
        return 0;

    for (toc = 0; toc < fileHeader->ntoc; toc++)
    {
	if (fileHeader->tocs[toc].type != XCURSOR_IMAGE_TYPE)
	    continue;
	thisSize = fileHeader->tocs[toc].subtype;
	if (thisSize != size)
	    continue;
	if (!count)
	    break;
	count--;
    }
    if (toc == fileHeader->ntoc)
	return -1;
    return toc;
}

static XcursorImage *
_XcursorReadImage (XcursorFile		*file,
		   XcursorFileHeader	*fileHeader,
		   int			toc)
{
    XcursorChunkHeader	chunkHeader;
    XcursorImage	head;
    XcursorImage	*image;
    int			n;
    XcursorPixel	*p;

    if (!file || !fileHeader)
        return NULL;

    if (!_XcursorFileReadChunkHeader (file, fileHeader, toc, &chunkHeader))
	return NULL;
    if (!_XcursorReadUInt (file, &head.width))
	return NULL;
    if (!_XcursorReadUInt (file, &head.height))
	return NULL;
    if (!_XcursorReadUInt (file, &head.xhot))
	return NULL;
    if (!_XcursorReadUInt (file, &head.yhot))
	return NULL;
    if (!_XcursorReadUInt (file, &head.delay))
	return NULL;
    /* sanity check data */
    if (head.width > XCURSOR_IMAGE_MAX_SIZE  ||
	head.height > XCURSOR_IMAGE_MAX_SIZE)
	return NULL;
    if (head.width == 0 || head.height == 0)
	return NULL;
    if (head.xhot > head.width || head.yhot > head.height)
	return NULL;

    /* Create the image and initialize it */
    image = XcursorImageCreate (head.width, head.height);
    if (image == NULL)
	    return NULL;
    if (chunkHeader.version < image->version)
	image->version = chunkHeader.version;
    image->size = chunkHeader.subtype;
    image->xhot = head.xhot;
    image->yhot = head.yhot;
    image->delay = head.delay;
    n = image->width * image->height;
    p = image->pixels;
    while (n--)
    {
	if (!_XcursorReadUInt (file, p))
	{
	    XcursorImageDestroy (image);
	    return NULL;
	}
	p++;
    }
    return image;
}

static XcursorImages *
XcursorXcFileLoadImages (XcursorFile *file, int size)
{
    XcursorFileHeader	*fileHeader;
    XcursorDim		bestSize;
    int			nsize;
    XcursorImages	*images;
    int			n;
    int			toc;

    if (!file || size < 0)
	return NULL;
    fileHeader = _XcursorReadFileHeader (file);
    if (!fileHeader)
	return NULL;
    bestSize = _XcursorFindBestSize (fileHeader, (XcursorDim) size, &nsize);
    if (!bestSize)
    {
        _XcursorFileHeaderDestroy (fileHeader);
	return NULL;
    }
    images = XcursorImagesCreate (nsize);
    if (!images)
    {
        _XcursorFileHeaderDestroy (fileHeader);
	return NULL;
    }
    for (n = 0; n < nsize; n++)
    {
	toc = _XcursorFindImageToc (fileHeader, bestSize, n);
	if (toc < 0)
	    break;
	images->images[images->nimage] = _XcursorReadImage (file, fileHeader,
							    toc);
	if (!images->images[images->nimage])
	    break;
	images->nimage++;
    }
    _XcursorFileHeaderDestroy (fileHeader);
    if (images->nimage != nsize)
    {
	XcursorImagesDestroy (images);
	images = NULL;
    }
    return images;
}

static int
_XcursorStdioFileRead (XcursorFile *file, unsigned char *buf, int len)
{
    FILE    *f = file->closure;
    return fread (buf, 1, len, f);
}

static int
_XcursorStdioFileWrite (XcursorFile *file, unsigned char *buf, int len)
{
    FILE    *f = file->closure;
    return fwrite (buf, 1, len, f);
}

static int
_XcursorStdioFileSeek (XcursorFile *file, long offset, int whence)
{
    FILE    *f = file->closure;
    return fseek (f, offset, whence);
}

static void
_XcursorStdioFileInitialize (FILE *stdfile, XcursorFile *file)
{
    file->closure = stdfile;
    file->read = _XcursorStdioFileRead;
    file->write = _XcursorStdioFileWrite;
    file->seek = _XcursorStdioFileSeek;
}

static XcursorImages *
XcursorFileLoadImages (FILE *file, int size)
{
    XcursorFile	f;

    if (!file)
        return NULL;

    _XcursorStdioFileInitialize (file, &f);
    return XcursorXcFileLoadImages (&f, size);
}

XcursorImages *
xcursor_load_images (const char *path, int size)
{
  FILE    *f;
  XcursorImages *images;

  f = fopen (path, "r");
  if (!f)
    return NULL;

  images = XcursorFileLoadImages (f, size);
  fclose (f);

  return images;
}

void
xcursor_images_destroy (XcursorImages *images)
{
  XcursorImagesDestroy (images);
}
