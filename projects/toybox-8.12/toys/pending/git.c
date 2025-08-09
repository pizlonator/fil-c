/* git.c - A minimal git clone
 *
 * Copyright 2022 Moritz C. Weber <mo.c.weber@gmail.com>
 *
 * See https://git-scm.com/docs/git-init
 * https://git-scm.com/docs/git-remote
 * https://git-scm.com/docs/git-fetch
 * https://git-scm.com/docs/git-checkout
 * https://git-scm.com/docs/pack-format
 * https://git-scm.com/docs/index-format
 * https://www.alibabacloud.com/blog/a-detailed-explanation-of-the-underlying-data-structures-and-principles-of-git_597391
 * https://github.com/git/git/blob/master/Documentation/gitformat-pack.txt
 * https://stackoverflow.com/a/14303988
 * https://stackoverflow.com/a/21599232
 * https://github.com/tarruda/node-git-core/blob/master/src/js/delta.js


USE_GITCLONE(NEWTOY(gitclone, "<1", TOYFLAG_USR|TOYFLAG_BIN))
USE_GITINIT(NEWTOY(gitinit, "<1", TOYFLAG_USR|TOYFLAG_BIN))
USE_GITREMOTE(NEWTOY(gitremote, "<1", TOYFLAG_USR|TOYFLAG_BIN))
USE_GITFETCH(NEWTOY(gitfetch, 0, TOYFLAG_USR|TOYFLAG_BIN))
USE_GITCHECKOUT(NEWTOY(gitcheckout, "<1", TOYFLAG_USR|TOYFLAG_BIN))

config GITCOMPAT
  bool "gitcompat"
  default n
  help
    Enable git compatible repos instead of minimal clone downloader.

config GITCLONE
  bool "gitclone"
  default n
  help
    usage: gitclone URL
    A minimal git clone.

config GITINIT
  bool "gitinit"
  default n
  help
    usage: gitinit NAME
    A minimal git init.

config GITREMOTE
  bool "gitremote"
  default n
  help
    usage: gitremote URL
    A minimal git remote add origin.

config GITFETCH
  bool "gitfetch"
  default n
  help
    usage: gitfetch
    A minimal git fetch.

config GITCHECKOUT
  bool "gitcheckout"
  default n
  help
    usage: gitcheckout <branch>
    A minimal git checkout.
*/

#define FOR_gitclone
#include "toys.h"
#include "openssl/sha.h" //ToDo: borrowed from OpenSSL to not pipe or refactor the SHA1SUM in toybox
#include "zlib.h"  //ToDo: borrowed from libz to not refactor deflate.c

GLOBALS(
  char *url, *name; //git repo remote url and init directory name
  struct IndexV2 *i; //git creates a index for each pack file, git clone just needs one index for the received pack file
)

//git index format v2 described at https://github.com/git/git/blob/master/Documentation/gitformat-pack.txt#L266
struct IndexV2 {
  char header[8];// Git 4 byte magic number and 4 byte version number
  unsigned fot[256];//A fan-out table
  char (*sha1)[20];//Table of sorted object names(SHA1 hashes)
  unsigned *crc, *offset;//Table of 4-bit CRC32 values and object offsets in pack file
  long long *offset64; //8 byte offests -- not supported yet
  char packsha1[20], idxsha1[20];//SHA1 hash of pack file and SHA1 hash of index file
};

//TODO:This function is not used before git clone persists an index V2
static void read_index(struct IndexV2 *i)
{
  FILE *fpi;

  i = xmalloc(sizeof(i));
  i->sha1 = malloc(20);
  i->crc = malloc(sizeof(unsigned));
  i->offset = malloc(sizeof(unsigned));
  i->offset64 = malloc(sizeof(long long));
  //TODO: not used yet as index is not persisted yet
  if (access(".git/object/pack/temp.idx", F_OK)==0) {
    //persistance needed for other git commands (not clone)
    fpi = fopen(".git/object/pack/temp.idx", "rb");
    printf("read header\n");
    fread(i->header, sizeof(i->header), 1, fpi);
    printf("Header: %s..Read fot\n", i->header);
    fread(i->fot, 4, 256, fpi);
    printf("Elements %d..Read sha1\n", i->fot[255]);
    fread(i->sha1, sizeof(i->fot), i->fot[255], fpi);
    printf("read crc\n");
    fread(i->crc, sizeof(i->fot), i->fot[255], fpi);
    printf("read offset\n");
    fread(i->offset, sizeof(i->fot), i->fot[255], fpi);
    //TODO: Offsets for file size 2G missing here
    printf("read packsha\n");
    fread(i->packsha1, 20, 1, fpi);
    printf("read idxsha\n");
    fread(i->idxsha1, 20, 1, fpi);
    fclose(fpi);
  }
}

long bsearchpos(const char *k, const char *a, size_t h, size_t w)
{
  long l = 0, m = 0, r = 0;

  if (!h) return 0;
  while (h>0) {
    m = l+(h/2);
    r = strncmp(k, a+(m*w), 20);
    if (!r||h==1) break; //match on search or position for insert
    if (r<0) { h /= 2; } else { l = m; h -= h/2; }
  }

  //For inserts check if insert is bigger  obj at identified position
  return m += (r>0) ? 1 : 0;
}

//find offset position in packfile for given SHA1 hash
long get_index(struct IndexV2 *i, char *h)
{
  long pos = bsearchpos(h, i->sha1[0], i->fot[255], 20);
 return i->offset[pos];
}

//https://github.com/git/git/blob/master/Documentation/gitformat-pack.txt#L35
//https://yqintl.alicdn.com/eef7fe4f22cc97912cee011c99d3fe5821ae9e88.png
//read type and length of an packed object at a given offset
unsigned long long unpack(FILE *fpp, int *type, long *offset)
{
  int bitshift= 4;
  unsigned long long length = 0;
  char data;

  printf("Start unpack\n");
  fseek(fpp, *offset, SEEK_SET);
  printf("Offset set to: %ld\n", *offset);
  fread(&data, 1, 1, fpp);
  printf("Data: %d\n", data);
  *type = ((data & 0x70)>>4);
  printf("Type: %d\n", *type);
  length |= data & 0x0F;
  while ((data & 0x80) && fread(&data, 1, 1, fpp)!=-1)
  {
    length |= (unsigned long long)(data & 0x7F) << bitshift;
    bitshift += 7; // (*offset)++;
  }
  printf("Length: %llu\n", length);

  return length;
}

//   ToDo: borrowed from int inf(FILE *source, FILE *dest) in
//   zpipe.c: example of proper use of zlib's inflate() and deflate()
//   Not copyrighted -- provided to the public domain
//   Version 1.4  11 December 2005  Mark Adler */
#define CHUNK 4096
int inf(FILE *source, char *dest) //modified signature to ease use
{
    int ret;
    char *position = dest;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK) return ret;

    // decompress until deflate stream ends or end of file
    do {

        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)inflateEnd(&strm);

            return Z_ERRNO;
        }
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;


        // run inflate() on input until output buffer not full
        do {

            strm.avail_out = CHUNK;
            strm.next_out = out;

            ret = inflate(&strm, Z_NO_FLUSH);
            //assert(ret != Z_STREAM_ERROR);  // state not clobbered
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     // and fall through
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);

                return ret;
            }

            have = CHUNK - strm.avail_out;
            memcpy(position, out, have); //added to original
            position += have; //added to original
            //if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
            //    (void)inflateEnd(&strm);
            //    return Z_ERRNO;
            //}
        } while (strm.avail_out == 0);
       // done when inflate() says it's done
    } while (ret != Z_STREAM_END);
    // modified from zpipe.c to set FP to end of zlib object
    fseek(source, ftell(source)-strm.avail_in, SEEK_SET);
    // clean up and return
    (void)inflateEnd(&strm);

    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

//https://github.com/git/git/blob/master/Documentation/gitformat-pack.txt#L72
//Set object to the index after adding prefix, calculating the hash and finding the position
long set_object(struct IndexV2 *idx, int type, char *o, unsigned count,
  unsigned ofs)
{
// TODO: Too many allocs in here 1) to concat the search string for hashing
// 2) to insert into the array (can be reduce to a single malloc in fetch as
// the pack header contains the number of objects in pack
  char *c, *p = "", *h = xmalloc(20); //composition,prefix,hash
  long pos = 0;

  printf("Alloc... ");
  //append a object prefix based on its type (not included in packfile)
  switch(type) {
    case 1: p = xmprintf("commit %d", count); break; //count is used as o can contain \0 in the  string
    case 2: p = xmprintf("tree %d", count); break;
    case 3: p = xmprintf("blob %d", count); break;
    case 4: p = xmprintf("tag %d", count); break;
    case 6: printf("REF_DELTA"); break; //not expected in fetch packs as fetch packs are self-containing
    case 7: printf("OBJ_DELTA\n"); break;
  }
  c = xmalloc(strlen(p)+count+2);
  memcpy(c, p, strlen(p)+1);
  memcpy(c+strlen(p)+1, o, count+1);
  h = SHA1(c, strlen(p)+count+1, h); //ToDo: borrowed from OpenSSL to not to pipe or refactor SHA1SUM in toybox
  printf("..Binary search\n");
  for (int j = 0; j<20; j++) printf("%02x", h[j]); //find insert position
  pos = bsearchpos(h, idx->sha1[0], idx->fot[255], 20);
  printf("\n..Insert pos %ld\n", pos);
  printf("..Preloop\n");

  //adjust of fanout table https://github.com/git/git/blob/master/Documentation/gitformat-pack.txt#L204
  for (int i = h[0]; i<=255; i++) idx->fot[i] += 1;
  printf("Post loop\n");
  printf("Resize sha1 array..idx->fot[255]%d\n", idx->fot[255]); //Memory management for insert
  //TODO:Could be also a single malloc at gitfetch based on the nbr of objects in pack

  //Did not fix the TODO yet, because set_object could be reused for other commands adding single objects to the index
  idx->sha1 = realloc(idx->sha1, (idx->fot[255]+1)*20*sizeof(char));
  printf("Mem copy sha1 array..sizeof(idx->sha1)%zu\n", sizeof(idx->sha1));
  memmove(&idx->sha1[pos+1], &idx->sha1[pos], (idx->fot[255]-pos)*20*sizeof(char));
  printf("Resize offset\n");
  idx->offset = realloc(idx->offset, (idx->fot[255]+1)*sizeof(unsigned));
  printf("Mem copy offset\n");
  memmove(&idx->offset[pos+1], &idx->offset[pos], sizeof(unsigned)*(idx->fot[255]-pos));
  printf("Set offset value\n");
  memcpy(&idx->sha1[pos], h, 20); //insert SHA1
  idx->offset[pos] = ofs; //insert offset of SHA1
  //ToDo: id->crc[idx->fot[h[0]]]=;
  printf("Write object\n");
  free(h);
  free(c);

  return ofs;
}

//init a git repository in a given directory name
static void gitinit(char *name)
{
  //For git clone actually only refs and object/pack are needed
  if (mkdir(name, 0755)!=0){
    //I create the others for a git compliant folder structure
    mkdir(xmprintf("%s%s", name, "/.git"), 0755);
    mkdir(xmprintf("%s%s", name, "/.git/objects"), 0755);
    mkdir(xmprintf("%s%s", name, "/.git/objects/pack"), 0755);
    mkdir(xmprintf("%s%s", name, "/.git/branches"), 0755);
    mkdir(xmprintf("%s%s", name, "/.git/hooks"), 0755); //hook files skipped as implementations does not support hooks
    mkdir(xmprintf("%s%s", name, "/.git/info"), 0755);
    mkdir(xmprintf("%s%s", name, "/.git/objects/info"), 0755);
    mkdir(xmprintf("%s%s", name, "/.git/refs"), 0755);
    mkdir(xmprintf("%s%s", name, "/.git/heads"), 0755);
    mkdir(xmprintf("%s%s", name, "/.git/tags"), 0755);
    xcreate(xmprintf("%s%s", name, "/.git/config"), O_CREAT, 0644);
    xcreate(xmprintf("%s%s", name, "/.git/description"), O_CREAT, 0644);
    xcreate(xmprintf("%s%s", name, "/.git/HEAD"), O_CREAT, 0644);
    xcreate(xmprintf("%s%s", name, "/.git/info/exclude"), O_CREAT, 0644);
  }
}

//set basic configuration and add remote URL
static void gitremote(char *url)
{
  if (access(".git/config", F_OK)!=0) {
    FILE *fp = fopen(".git/config", "wb");

    fwrite("[core]\n", 1, 7, fp);
    fwrite("\trepositoryformatversion = 0\n", 1, 29, fp);
    fwrite("\tfilemode = false\n", 1, 18, fp);
    fwrite("\tbare = false\n", 1, 14, fp);
    fwrite("\tlogallrefupdates = true\n", 1, 25, fp);
    fwrite("\tsymlinks = false\n", 1, 18, fp);
    fwrite("\tignorecase = true\n", 1, 19, fp);
    fwrite("[remote \"origin\"]\n", 1, 18, fp);
    fwrite(xmprintf("\turl = %s/refs\n", TT.url), 1, strlen(TT.url)+13, fp);
    fwrite("\tfetch = +ref/heads/*:refs/remotes/origin/*\n", 1, 44, fp);
    fclose(fp);
  }
}

// this is most likely still buggy and create a late observable heap overflow larger deltafied repos
// https://stackoverflow.com/a/14303988
// resolve deltafied objects in the pack file, see URL in comments for further explainations
char *resolve_delta(char *s, char *d, long dsize, unsigned *count)
{
  long pos = 0, bitshift = 0;
  //https://github.com/git/git/blob/master/Documentation/gitformat-pack.txt#L113
  // Skipping source size; did not find out why it is  on the delta header as the source object header contains it too; maybe misunderstood and this makes things buggy, but I dont need it here
  while ((d[pos] & 0x80)) pos++;
  pos++; //fixes https://github.com/git/git/blob/master/Documentation/gitformat-pack.txt#L114
  *count = 0;
  bitshift = 0;
  while ((d[pos] & 0x80)) { //reading target_size from header
    *count |= (unsigned long long)(d[pos++]& 0x7F) << bitshift;
    bitshift += 7; // (*offset)++;
  }

  *count |= (unsigned long long)(d[pos++]& 0x7F) << bitshift;
  printf("Target Count %d:\n", *count);
  char *t = xmalloc(*count+1);
  *count = 0;
  while (pos<dsize) {
    int i = 0, j = 1;
    unsigned offset = 0, size = 0;

    if ((d[pos]&0x80)) {//https://github.com/git/git/blob/master/Documentation/gitformat-pack.txt#L128
    //https://stackoverflow.com/a/14303988
      printf("Case 1\n");
      while (i<4) {
        if (d[pos]&(1<<i)) {
          offset |= d[pos+j]<<(i*8);
          j++;
        }
        i++;
      }
      while (i<7) {
        if (d[pos]&(1<<i)) {
          size |= d[pos+j]<<(i*8);
          j++;
        }
        i++;
      }

      // packfomat: size zero is automatically converted to 0x10000.
      // https://github.com/git/git/blob/master/Documentation/gitformat-pack.txt#L156
      if (size==0) size = 0x10000;
      memcpy(t+*count, s+offset, size);
      //t[size] = '\0';
      pos += j;
      printf("Pos\n");
    } else {
      //https://github.com/git/git/blob/master/Documentation/gitformat-pack.txt#L158
    printf("Case 0\n");
      size = d[pos++]; //incrememt
      memcpy(t+*count, d+pos, size);
      pos += size;
    }
    *count += size;

    printf("Target: \n");
  }
  free(s);
  free(d);

  return t;
}

//unpack object (,resolve deltafied objects recursively) and return the unpacked object
char *unpack_object(FILE *fpp, struct IndexV2 *i, long offset, unsigned *count,
  int *type)
{
  unsigned dcount = unpack(fpp, type, &offset);
  char *object = xmalloc(dcount);

  object[*count] = '\0';
  printf("Count: %d \n", *count);
// see OBJ_REF_DELTA here https://yqintl.alicdn.com/eef7fe4f22cc97912cee011c99d3fe5821ae9e88.png
  if (*type==7) {
      char *h = xmalloc(20);

      fread(h, 20, 1, fpp); //fseek(fpp, 20, SEEK_CUR);
      printf("Read base object\n");
      for (int j = 0; j<20; j++) printf("%02x", h[j]);
      printf("\n");
      inf(fpp, object); //TO CHECK IF INF OR PLAIN TEXT:
      long toffset = ftell(fpp); //save original file offset
      char *source = unpack_object(fpp, i, get_index(i, h), count, type);
      printf("Inflate delta data\n");
      fseek(fpp, toffset, SEEK_SET); //return to original file offset
      printf("Resolve delta data\n");
      free(h);//recursion due to https://github.com/git/git/blob/master/Documentation/gitformat-pack.txt#L58
      return resolve_delta(source, object, dcount, count);
    } else {
      printf("Type Else:\n");
      inf(fpp, object);
      *count = dcount;
      printf("Type Else end:\n");
      //printf("Unpacked Object: %s\n", object);

      return object;
    }
}

//make 20byte SHA1 hash from 40 byte SHA1 string
char *txtoh(char *p)
{
  //TODO: Dont like the malloc here, but did not find a solution to sscanf into p again
  char *h = xmalloc(41);

  for (int c = 0; c<20; c++) {
    sscanf(&p[2*c], "%2hhx", &(h[c]));
  }
  h[20] = 0;
  printf("return");

  return h;
}

//traveres the commit tree for checkout
void write_children(char *hash, char *path, FILE *fpp)
{
  FILE *fc;
  char *object;
  int type;
  long offset;
  unsigned count=0;

  printf("seek index\n");

  offset = get_index(TT.i, hash);
  printf("Found index: %ld\n", offset);
  printf("read object\n");
  object = unpack_object(fpp, TT.i, offset, &count, &type);
  printf("%s\n", object);
  printf("Type %d\n", type);
  if (type==1) { //at commit object
    memcpy(hash, object+5, 40);
    write_children(txtoh(hash), path, fpp);
  } else if (type==2) { //at tree object https://stackoverflow.com/a/21599232
    char *hs, *name;
    int pos = 0;

    printf("process folder %s\n", path);
    while (pos<count){
      //find position where the next hash starts
      hs = object+pos;
      printf("Object+pos: %s\n", hs);
      // memcpy(mode, hs+2, 3)//TODO:String to umask
      if (*hs=='1') { //tree object reference is a file
        // concat file name
        name = *path ? xmprintf("%s/%s", path, hs+7) : hs+7;
        printf("prepare file %s\n", name);
      } else { //tree object reference is a folder
        // concat folder name
        name = *path ? xmprintf("%s/%s", path, hs+6) : hs+6;
        printf("create folder %s\n", name);
        mkdir(name, 0755); //TODO: umask
      }
      hs += strlen(hs)+1;
      memcpy(hash, hs, 20);
      write_children(hash, name, fpp);
      pos = hs-object+20;
      printf("Position/count for %s: %d/%u\n", path, pos, count);
    }
    printf("**EXIT WHILE**\n");
  } else { //at blob/file object
    printf("process file %s\n", path);
    fc = fopen(path, "w");
    printf("process opened \n");
    fputs(object, fc); //TODO:Not sure if length might be an issue here
    printf("process file written\n");
    fclose(fc);
  }
  free(object);
  printf("Child: %s done\n", path);
}

//fetches the meta data from the remote repository,requests a pack file for the remote master head,
//unpacks all objects and set objects to the index
static void gitfetch(void)
{
  printf("refs\n");

  // TODO:I use herein after two temp files for fetch which git does not offer
  // to 1) avoid a rewrite and 2) messing up the repo files while testing

  // TODO: Refactor wget into lib
  xrun((char *[]){"wget", "-O", ".git/refs/temp.refs",
      "https://github.com/landley/toybox/info/refs?service=git-upload-pack",
      0});
  //char h[] = "8cf1722f0fde510ea81d13b31bde1e48917a0306";
  //TODO: Replace static testing hash and uncomment the following line if rare delta resolve /?heap overflow? bug was found
  FILE *fpr = fopen(".git/refs/temp.refs", "r");
  char *h;
  size_t l = 0;

  getline(&h,&l,fpr);
  getline(&h,&l,fpr);
  getline(&h,&l,fpr);
  getline(&h,&l,fpr);
  fclose(fpr);
  strcpy(h,&h[4]);
  h[40] = 0;
  printf("Master HEAD hash: %s\n",h);
  //TODO: Persist hash to /refs/master/HEAD
  printf("pack\n");
  xrun((char *[]){"toybox", "wget", "-O", ".git/objects/pack/temp.pack", "-p", xmprintf("$'0032want %s\n00000009done\n'", h), "https://github.com/landley/toybox/git-upload-pack", 0});
  //TODO: does not skip 0008NAK  printf("init\n");
  FILE *fpp;
  printf("openpack\n");
  fpp = fopen(".git/objects/pack/temp.pack", "r");
  printf("read index\n");
  read_index(TT.i); //init index with out reading
  printf("init\n");
  unsigned ocount = 0, count;
  int type;
  char *object;

  printf("skip header\n");
  long offset = 12+8; //8byte from the wget post response are skipped too
  fseek(fpp, 8+8, SEEK_SET); //header check skipped //header check skipped https://github.com/git/git/blob/master/Documentation/gitformat-pack.txt#L37
  printf("read count\n");
  fread(&ocount, 4, 1, fpp);
  ocount = ntohl(ocount); //https://github.com/git/git/blob/master/Documentation/gitformat-pack.txt#L46
  printf("Count: %d ..Loop pack\n", ocount);
  for (int j = 0; j<ocount; j++){
    printf("Read object %d\n", j);
    count = 0;
    object = unpack_object(fpp, TT.i, offset, &count, &type);
    printf("Count: %d Offset: %ld  ..Set object\n", count, offset);
    set_object(TT.i, type, object, count, offset);

    free(object);

    printf("Adjust offset\n");
    offset = ftell(fpp); //adjust offset to new file position
    printf("Adjusted offset to: %ld\n", offset);
  }

  //TODO: Final pack checksum not calculated and checked
  fclose(fpp);
}

//Checkout HEAD to the  working directory by recursing write_children
//TODO: Replase static hashes with hash read from refs/<branch>/head
static void gitcheckout(char *name)
{

  FILE *fpp;
  //FILE *fh;

  printf("Find branch for checkout\n");
  //fh = fopen(xmprintf(".git/ref/heads/%s", name ? "master" : name), "r"); //TODO: Checkout master as in ref/heads
  printf("Read head\n");
  //hf[fread(&hf, 40, 1, fh)] = '\0';
  //fclose(fh);
  printf("Close heads and read pack\n");
  fpp = fopen(".git/objects/pack/temp.pack", "r");
  printf("set signature\n");
  char *p = "52fb04274b3491fdfe91b2e5acc23dc3f3064a86"; //static hashes for testing toybox 0.0.1";
  //char *p = "c555a0ca46e75097596274bf5e634127015aa144"; //static hashes for testing 0.0.2";
  //char *p = "4307a7b07cec4ad8cbab47a29ba941f8cb041812"; //static hashes for testing 0.0.3";
  //char *p = "3632d5d8fe05d14da983e37c7cd34db0769e6238"; //static hashes for testing 0.0.4";
  //char *p = "8cf1722f0fde510ea81d13b31bde1e48917a0306"; //3604ba4f42c3d83e2b14f6d0f423a33a3a8706c3";
  printf("enter tree root\n");
  write_children(txtoh(p), "", fpp);
  fclose(fpp);
}

void gitclone_main(void)
{
  TT.url = xstrdup(toys.optargs[0]);
  if (strend(TT.url, ".git")) TT.url[strlen(TT.url)-4] = '\0';
  TT.name = strrchr(TT.url, '/')+1;
  gitinit(TT.name);
  chdir(TT.name);
  TT.i = malloc(sizeof(struct IndexV2));
  gitremote(TT.url);
  gitfetch();
  gitcheckout("master");
  chdir("..");
}

#define FOR_gitinit
#include "generated/flags.h"

void gitinit_main(void)
{
  gitinit(xstrdup(toys.optargs[0]));
}

#define FOR_gitremote

void gitremote_main(void)
{
  TT.url = xstrdup(toys.optargs[0]);
  if (strend(TT.url, ".git")) TT.url[strlen(TT.url)-4] = '\0';
  gitremote(TT.url);
}

#define FOR_gitinit

void gitfetch_main(void)
{
  gitfetch();
}

#define FOR_gitcheckout

void gitcheckout_main(void)
{
  gitcheckout(xstrdup(toys.optargs[0]));
}

// Command to wget refs and pack file using toybox wget to place them manually into the repository
// ./toybox wget -O - -d https://github.com/landley/toybox/info/refs?service=git-upload-pack | less

// ./toybox wget -O pack.dat -d -p $'0032want 8cf1722f0fde510ea81d13b31bde1e48917a0306\n00000009done\n' https://github.com/landley/toybox/git-upload-pack
