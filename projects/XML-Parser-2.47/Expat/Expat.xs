/*****************************************************************
** Expat.xs
**
** Copyright 1998 Larry Wall and Clark Cooper
** All rights reserved.
**
** This program is free software; you can redistribute it and/or
** modify it under the same terms as Perl itself.
**
*/

#include <expat.h>

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#undef convert

#include "patchlevel.h"
#include "encoding.h"


/* Version 5.005_5x (Development version for 5.006) doesn't like sv_...
   anymore, but 5.004 doesn't know about PL_sv..
   Don't want to push up required version just for this. */

#if PATCHLEVEL < 5
#define PL_sv_undef	sv_undef
#define PL_sv_no	sv_no
#define PL_sv_yes	sv_yes
#define PL_na		na
#endif

#define BUFSIZE 32768

#define NSDELIM  '|'

/* Macro to update handler fields. Used in the various handler setting
   XSUBS */

#define XMLP_UPD(fld) \
  RETVAL = cbv->fld ? newSVsv(cbv->fld) : &PL_sv_undef;\
  if (cbv->fld) {\
    if (cbv->fld != fld)\
      sv_setsv(cbv->fld, fld);\
  }\
  else\
    cbv->fld = newSVsv(fld)

/* Macro to push old handler value onto return stack. This is done here
   to get around a bug in 5.004 sv_2mortal function. */

#define PUSHRET \
  ST(0) = RETVAL;\
  if (RETVAL != &PL_sv_undef && SvREFCNT(RETVAL)) sv_2mortal(RETVAL)

typedef struct {
  SV* self_sv;
  XML_Parser p;

  AV* context;
  AV* new_prefix_list;
  HV *nstab;
  AV *nslst;

  unsigned int st_serial;
  unsigned int st_serial_stackptr;
  unsigned int st_serial_stacksize;
  unsigned int * st_serial_stack;

  unsigned int skip_until;

  SV *recstring;
  char * delim;
  STRLEN delimlen;

  unsigned ns:1;
  unsigned no_expand:1;
  unsigned parseparam:1;

  /* Callback handlers */

  SV* start_sv;
  SV* end_sv;
  SV* char_sv;
  SV* proc_sv;
  SV* cmnt_sv;
  SV* dflt_sv;

  SV* entdcl_sv;
  SV* eledcl_sv;
  SV* attdcl_sv;
  SV* doctyp_sv;
  SV* doctypfin_sv;
  SV* xmldec_sv;

  SV* unprsd_sv;
  SV* notation_sv;

  SV* extent_sv;
  SV* extfin_sv;

  SV* startcd_sv;
  SV* endcd_sv;
} CallbackVector;


static HV* EncodingTable = NULL;

static XML_Char nsdelim[] = {NSDELIM, '\0'};

static char *QuantChar[] = {"", "?", "*", "+"};

/* Forward declarations */

static void suspend_callbacks(CallbackVector *);
static void resume_callbacks(CallbackVector *);

#if PATCHLEVEL < 5 && SUBVERSION < 5

/* ================================================================
** This is needed where the length is explicitly given. The expat
** library may sometimes give us zero-length strings. Perl's newSVpv
** interprets a zero length as a directive to do a strlen. This
** function is used when we want to force length to mean length, even
** if zero.
*/

static SV *
newSVpvn(char *s, STRLEN len)
{
  register SV *sv;

  sv = newSV(0);
  sv_setpvn(sv, s, len);
  return sv;
}  /* End newSVpvn */

#define ERRSV GvSV(errgv)
#endif

#ifdef SvUTF8_on

static SV *
newUTF8SVpv(char *s, STRLEN len) {
  register SV *sv;

  sv = newSVpv(s, len);
  SvUTF8_on(sv);
  return sv;
}  /* End new UTF8SVpv */

static SV *
newUTF8SVpvn(char *s, STRLEN len) {
  register SV *sv;

  sv = newSV(0);
  sv_setpvn(sv, s, len);
  SvUTF8_on(sv);
  return sv;
}

#else  /* SvUTF8_on not defined */

#define newUTF8SVpv newSVpv
#define newUTF8SVpvn newSVpvn

#endif

static void*
mymalloc(size_t size) {
#ifndef LEAKTEST
  return safemalloc(size);
#else
  return safexmalloc(328,size);
#endif
}

static void*
myrealloc(void *p, size_t s) {
#ifndef LEAKTEST
  return saferealloc(p, s);
#else
  return safexrealloc(p, s);
#endif
}

static void
myfree(void *p) {
  Safefree(p);
}

static XML_Memory_Handling_Suite ms = {mymalloc, myrealloc, myfree};

static void
append_error(XML_Parser parser, char * err)
{
  dSP;
  CallbackVector * cbv;
  SV ** errstr;

  cbv = (CallbackVector*) XML_GetUserData(parser);
  errstr = hv_fetch((HV*)SvRV(cbv->self_sv),
		    "ErrorMessage", 12, 0);

  if (errstr && SvPOK(*errstr)) {
    SV ** errctx = hv_fetch((HV*) SvRV(cbv->self_sv),
			    "ErrorContext", 12, 0);
    int dopos = !err && errctx && SvOK(*errctx);

    if (! err)
      err = (char *) XML_ErrorString(XML_GetErrorCode(parser));

    sv_catpvf(*errstr, "\n%s at line %ld, column %ld, byte %ld%s",
	      err,
	      (long)XML_GetCurrentLineNumber(parser),
	      (long)XML_GetCurrentColumnNumber(parser),
	      (long)XML_GetCurrentByteIndex(parser),
	      dopos ? ":\n" : "");
	      /* See https://rt.cpan.org/Ticket/Display.html?id=92030
	         It explains why type conversion is used. */
	      
    if (dopos)
      {
	int count;

	ENTER ;
	SAVETMPS ;
	PUSHMARK(sp);
	XPUSHs(cbv->self_sv);
	XPUSHs(*errctx);
	PUTBACK ;

	count = perl_call_method("position_in_context", G_SCALAR);

	SPAGAIN ;

	if (count >= 1) {
	  sv_catsv(*errstr, POPs);
	}

	PUTBACK ;
	FREETMPS ;
	LEAVE ;
      }
  }
}  /* End append_error */

static SV *
generate_model(XML_Content *model) {
  HV * hash = newHV();
  SV * obj = newRV_noinc((SV *) hash);

  sv_bless(obj, gv_stashpv("XML::Parser::ContentModel", 1));

  hv_store(hash, "Type", 4, newSViv(model->type), 0);
  if (model->quant != XML_CQUANT_NONE) {
    hv_store(hash, "Quant", 5, newSVpv(QuantChar[model->quant], 1), 0);
  }

  switch(model->type) {
  case XML_CTYPE_NAME:
    hv_store(hash, "Tag", 3, newUTF8SVpv((char *)model->name, 0), 0);
    break;

  case XML_CTYPE_MIXED:
  case XML_CTYPE_CHOICE:
  case XML_CTYPE_SEQ:
    if (model->children && model->numchildren)
      {
	AV * children = newAV();
	int i;

	for (i = 0; i < model->numchildren; i++) {
	  av_push(children, generate_model(&model->children[i]));
	}

	hv_store(hash, "Children", 8, newRV_noinc((SV *) children), 0);
      }
    break;
  }

  return obj;
}  /* End generate_model */

static int
parse_stream(XML_Parser parser, SV * ioref)
{
  dSP;
  SV *		tbuff;
  SV *		tsiz;
  char *	linebuff;
  STRLEN	lblen;
  STRLEN	br = 0;
  int		buffsize;
  int		done = 0;
  int		ret = 1;
  char *	msg = NULL;
  CallbackVector * cbv;
  char		*buff = (char *) 0;

  cbv = (CallbackVector*) XML_GetUserData(parser);

  ENTER;
  SAVETMPS;

  if (cbv->delim) {
    int cnt;
    SV * tline;

    PUSHMARK(SP);
    XPUSHs(ioref);
    PUTBACK ;

    cnt = perl_call_method("getline", G_SCALAR);

    SPAGAIN;

    if (cnt != 1)
      croak("getline method call failed");

    tline = POPs;

    if (! SvOK(tline)) {
      lblen = 0;
    }
    else {
      char *	chk;
      linebuff = SvPV(tline, lblen);
      chk = &linebuff[lblen - cbv->delimlen - 1];

      if (lblen > cbv->delimlen + 1
	  && *chk == *cbv->delim
	  && chk[cbv->delimlen] == '\n'
	  && strnEQ(++chk, cbv->delim + 1, cbv->delimlen - 1))
	lblen -= cbv->delimlen + 1;
    }

    PUTBACK ;
    buffsize = lblen;
    done = lblen == 0;
  }
  else {
    tbuff = newSV(0);
    tsiz = newSViv(BUFSIZE); /* in UTF-8 characters */
    buffsize = BUFSIZE * 6; /* in bytes that encode an UTF-8 string */
  }

  while (! done)
    {
      char *buffer = XML_GetBuffer(parser, buffsize);

      if (! buffer)
	croak("Ran out of memory for input buffer");

      SAVETMPS;

      if (cbv->delim) {
	Copy(linebuff, buffer, lblen, char);
	br = lblen;
	done = 1;
      }
      else {
	int cnt;
	SV * rdres;
	char * tb;

	PUSHMARK(SP);
	EXTEND(SP, 3);
	PUSHs(ioref);
	PUSHs(tbuff);
	PUSHs(tsiz);
	PUTBACK ;

	cnt = perl_call_method("read", G_SCALAR);

	SPAGAIN ;

	if (cnt != 1)
	  croak("read method call failed");

	rdres = POPs;

	if (! SvOK(rdres))
	  croak("read error");

	tb = SvPV(tbuff, br);
	if (br > 0) {
	  if (br > buffsize)
	    croak("The input buffer is not large enough for read UTF-8 decoded string");
	  Copy(tb, buffer, br, char);
	} else
	  done = 1;

	PUTBACK ;
      }

      ret = XML_ParseBuffer(parser, br, done);

      SPAGAIN; /* resync local SP in case callbacks changed global stack */

      if (! ret)
	break;

      FREETMPS;
    }

  if (! ret)
    append_error(parser, msg);

  if (! cbv->delim) {
    SvREFCNT_dec(tsiz);
    SvREFCNT_dec(tbuff);
  }
      
  FREETMPS;
  LEAVE;

  return ret;
}  /* End parse_stream */

static SV *
gen_ns_name(const char * name, HV * ns_table, AV * ns_list)
{
  char	*pos = strchr(name, NSDELIM);
  SV * ret;

  if (pos && pos > name)
    {
      SV ** name_ent = hv_fetch(ns_table, (char *) name,
				pos - name, TRUE);
      ret = newUTF8SVpv(&pos[1], 0);

      if (name_ent)
	{
	  int index;

	  if (SvOK(*name_ent))
	    {
	      index = SvIV(*name_ent);
	    }
	  else
	    {
	      av_push(ns_list,  newUTF8SVpv((char *) name, pos - name));
	      index = av_len(ns_list);
	      sv_setiv(*name_ent, (IV) index);
	    }

	  sv_setiv(ret, (IV) index);
	  SvPOK_on(ret);
	}
    }
  else
    ret = newUTF8SVpv((char *) name, 0);

  return ret;
}  /* End gen_ns_name */

static void
characterData(void *userData, const char *s, int len)
{
  dSP;
  CallbackVector* cbv = (CallbackVector*) userData;

  ENTER;
  SAVETMPS;

  PUSHMARK(sp);
  EXTEND(sp, 2);
  PUSHs(cbv->self_sv);
  PUSHs(sv_2mortal(newUTF8SVpvn((char*)s,len)));
  PUTBACK;
  perl_call_sv(cbv->char_sv, G_DISCARD);

  FREETMPS;
  LEAVE;
}  /* End characterData */

static void
startElement(void *userData, const char *name, const char **atts)
{
  dSP;
  CallbackVector* cbv = (CallbackVector*) userData;
  SV ** pcontext;
  unsigned   do_ns = cbv->ns;
  unsigned   skipping = 0;
  SV ** pnstab;
  SV ** pnslst;
  SV *  elname;

  cbv->st_serial++;

  if (cbv->skip_until) {
    skipping = cbv->st_serial < cbv->skip_until;
    if (! skipping) {
      resume_callbacks(cbv);
      cbv->skip_until = 0;
    }
  }

  if (cbv->st_serial_stackptr >= cbv->st_serial_stacksize) {
    unsigned int newsize = cbv->st_serial_stacksize + 512;

    Renew(cbv->st_serial_stack, newsize, unsigned int);
    cbv->st_serial_stacksize = newsize;
  }

  cbv->st_serial_stack[++cbv->st_serial_stackptr] =  cbv->st_serial;
  
  if (do_ns)
    elname = gen_ns_name(name, cbv->nstab, cbv->nslst);
  else
    elname = newUTF8SVpv((char *)name, 0);

  if (! skipping && SvTRUE(cbv->start_sv))
    {
      const char **attlim = atts;

      while (*attlim)
	attlim++;

      ENTER;
      SAVETMPS;

      PUSHMARK(sp);
      EXTEND(sp, attlim - atts + 2);
      PUSHs(cbv->self_sv);
      PUSHs(elname);
      while (*atts)
	{
	  SV * attname;

	  attname = (do_ns ? gen_ns_name(*atts, cbv->nstab, cbv->nslst)
		     : newUTF8SVpv((char *) *atts, 0));
	    
	  atts++;
	  PUSHs(sv_2mortal(attname));
	  if (*atts)
	    PUSHs(sv_2mortal(newUTF8SVpv((char*)*atts++,0)));
	}
      PUTBACK;
      perl_call_sv(cbv->start_sv, G_DISCARD);

      FREETMPS;
      LEAVE;
    }

  av_push(cbv->context, elname);

  if (cbv->ns) {
    av_clear(cbv->new_prefix_list);
  }
} /* End startElement */

static void
endElement(void *userData, const char *name)
{
  dSP;
  CallbackVector* cbv = (CallbackVector*) userData;
  SV *elname;

  elname = av_pop(cbv->context);
  
  if (! cbv->st_serial_stackptr) {
    croak("endElement: Start tag serial number stack underflow");
  }

  if (! cbv->skip_until && SvTRUE(cbv->end_sv))
    {
      ENTER;
      SAVETMPS;

      PUSHMARK(sp);
      EXTEND(sp, 2);
      PUSHs(cbv->self_sv);
      PUSHs(elname);
      PUTBACK;
      perl_call_sv(cbv->end_sv, G_DISCARD);

      FREETMPS;
      LEAVE;
    }

  cbv->st_serial_stackptr--;

  SvREFCNT_dec(elname);
}  /* End endElement */

static void
processingInstruction(void *userData, const char *target, const char *data)
{
  dSP;
  CallbackVector* cbv = (CallbackVector*) userData;

  ENTER;
  SAVETMPS;

  PUSHMARK(sp);
  EXTEND(sp, 3);
  PUSHs(cbv->self_sv);
  PUSHs(sv_2mortal(newUTF8SVpv((char*)target,0)));
  PUSHs(sv_2mortal(newUTF8SVpv((char*)data,0)));
  PUTBACK;
  perl_call_sv(cbv->proc_sv, G_DISCARD);

  FREETMPS;
  LEAVE;
}  /* End processingInstruction */

static void
commenthandle(void *userData, const char *string)
{
  dSP;
  CallbackVector * cbv = (CallbackVector*) userData;

  ENTER;
  SAVETMPS;

  PUSHMARK(sp);
  EXTEND(sp, 2);
  PUSHs(cbv->self_sv);
  PUSHs(sv_2mortal(newUTF8SVpv((char*) string, 0)));
  PUTBACK;
  perl_call_sv(cbv->cmnt_sv, G_DISCARD);

  FREETMPS;
  LEAVE;
}  /* End commenthandler */

static void
startCdata(void *userData)
{
  dSP;
  CallbackVector* cbv = (CallbackVector*) userData;

  if (cbv->startcd_sv) {
    ENTER;
    SAVETMPS;

    PUSHMARK(sp);
    XPUSHs(cbv->self_sv);
    PUTBACK;
    perl_call_sv(cbv->startcd_sv, G_DISCARD);

    FREETMPS;
    LEAVE;
  }
}  /* End startCdata */

static void
endCdata(void *userData)
{
  dSP;
  CallbackVector* cbv = (CallbackVector*) userData;

  if (cbv->endcd_sv) {
    ENTER;
    SAVETMPS;

    PUSHMARK(sp);
    XPUSHs(cbv->self_sv);
    PUTBACK;
    perl_call_sv(cbv->endcd_sv, G_DISCARD);

    FREETMPS;
    LEAVE;
  }
}  /* End endCdata */

static void
nsStart(void *userdata, const XML_Char *prefix, const XML_Char *uri){
  dSP;
  CallbackVector* cbv = (CallbackVector*) userdata;

  ENTER;
  SAVETMPS;

  PUSHMARK(sp);
  EXTEND(sp, 3);
  PUSHs(cbv->self_sv);
  PUSHs(prefix ? sv_2mortal(newUTF8SVpv((char *)prefix, 0)) : &PL_sv_undef);
  PUSHs(uri ? sv_2mortal(newUTF8SVpv((char *)uri, 0)) : &PL_sv_undef);
  PUTBACK;
  perl_call_method("NamespaceStart", G_DISCARD);

  FREETMPS;
  LEAVE;
}  /* End nsStart */

static void
nsEnd(void *userdata, const XML_Char *prefix) {
  dSP;
  CallbackVector* cbv = (CallbackVector*) userdata;

  ENTER;
  SAVETMPS;

  PUSHMARK(sp);
  EXTEND(sp, 2);
  PUSHs(cbv->self_sv);
  PUSHs(prefix ? sv_2mortal(newUTF8SVpv((char *)prefix, 0)) : &PL_sv_undef);
  PUTBACK;
  perl_call_method("NamespaceEnd", G_DISCARD);

  FREETMPS;
  LEAVE;
}  /* End nsEnd */

static void
defaulthandle(void *userData, const char *string, int len)
{
  dSP;
  CallbackVector* cbv = (CallbackVector*) userData;

  ENTER;
  SAVETMPS;

  PUSHMARK(sp);
  EXTEND(sp, 2);
  PUSHs(cbv->self_sv);
  PUSHs(sv_2mortal(newUTF8SVpvn((char*)string, len)));
  PUTBACK;
  perl_call_sv(cbv->dflt_sv, G_DISCARD);

  FREETMPS;
  LEAVE;
}  /* End defaulthandle */

static void
elementDecl(void *data,
	    const char *name,
	    XML_Content *model) {
  dSP;
  CallbackVector *cbv = (CallbackVector*) data;
  SV *cmod;

  ENTER;
  SAVETMPS;


  cmod = generate_model(model);

  Safefree(model);
  PUSHMARK(sp);
  EXTEND(sp, 3);
  PUSHs(cbv->self_sv);
  PUSHs(sv_2mortal(newUTF8SVpv((char *)name, 0)));
  PUSHs(sv_2mortal(cmod));
  PUTBACK;
  perl_call_sv(cbv->eledcl_sv, G_DISCARD);
  FREETMPS;
  LEAVE;

}  /* End elementDecl */

static void
attributeDecl(void *data,
	      const char * elname,
	      const char * attname,
	      const char * att_type,
	      const char * dflt,
	      int          reqorfix) {
  dSP;
  CallbackVector *cbv = (CallbackVector*) data;
  SV * dfltsv;

  if (dflt) {
    dfltsv = newUTF8SVpv("'", 1);
    sv_catpv(dfltsv, (char *) dflt);
    sv_catpv(dfltsv, "'");
  }
  else {
    dfltsv = newUTF8SVpv(reqorfix ? "#REQUIRED" : "#IMPLIED", 0);
  }

  ENTER;
  SAVETMPS;
  PUSHMARK(sp);
  EXTEND(sp, 5);
  PUSHs(cbv->self_sv);
  PUSHs(sv_2mortal(newUTF8SVpv((char *)elname, 0)));
  PUSHs(sv_2mortal(newUTF8SVpv((char *)attname, 0)));
  PUSHs(sv_2mortal(newUTF8SVpv((char *)att_type, 0)));
  PUSHs(sv_2mortal(dfltsv));
  if (dflt && reqorfix)
    XPUSHs(&PL_sv_yes);
  PUTBACK;
  perl_call_sv(cbv->attdcl_sv, G_DISCARD);

  FREETMPS;
  LEAVE;
}  /* End attributeDecl */

static void
entityDecl(void *data,
	   const char *name,
	   int isparam,
	   const char *value,
	   int vlen,
	   const char *base,
	   const char *sysid,
	   const char *pubid,
	   const char *notation) {
  dSP;
  CallbackVector *cbv = (CallbackVector*) data;

  ENTER;
  SAVETMPS;

  PUSHMARK(sp);
  EXTEND(sp, 6);
  PUSHs(cbv->self_sv);
  PUSHs(sv_2mortal(newUTF8SVpv((char*)name, 0)));
  PUSHs(value ? sv_2mortal(newUTF8SVpvn((char*)value, vlen)) : &PL_sv_undef);
  PUSHs(sysid ? sv_2mortal(newUTF8SVpv((char *)sysid, 0)) : &PL_sv_undef);
  PUSHs(pubid ? sv_2mortal(newUTF8SVpv((char *)pubid, 0)) : &PL_sv_undef);
  PUSHs(notation ? sv_2mortal(newUTF8SVpv((char *)notation, 0)) : &PL_sv_undef);
  if (isparam)
    XPUSHs(&PL_sv_yes);
  PUTBACK;
  perl_call_sv(cbv->entdcl_sv, G_DISCARD);

  FREETMPS;
  LEAVE;
}  /* End entityDecl */

static void
doctypeStart(void *userData,
	     const char* name,
	     const char* sysid,
	     const char* pubid,
	     int hasinternal) {
  dSP;
  CallbackVector *cbv = (CallbackVector*) userData;

  ENTER;
  SAVETMPS;

  PUSHMARK(sp);
  EXTEND(sp, 5);
  PUSHs(cbv->self_sv);
  PUSHs(sv_2mortal(newUTF8SVpv((char*)name, 0)));
  PUSHs(sysid ? sv_2mortal(newUTF8SVpv((char*)sysid, 0)) : &PL_sv_undef);
  PUSHs(pubid ? sv_2mortal(newUTF8SVpv((char*)pubid, 0)) : &PL_sv_undef);
  PUSHs(hasinternal ? &PL_sv_yes : &PL_sv_no);
  PUTBACK;
  perl_call_sv(cbv->doctyp_sv, G_DISCARD);
  FREETMPS;
  LEAVE;
}  /* End doctypeStart */

static void
doctypeEnd(void *userData) {
  dSP;
  CallbackVector *cbv = (CallbackVector*) userData;

  ENTER;
  SAVETMPS;

  PUSHMARK(sp);
  EXTEND(sp, 1);
  PUSHs(cbv->self_sv);
  PUTBACK;
  perl_call_sv(cbv->doctypfin_sv, G_DISCARD);
  FREETMPS;
  LEAVE;
}  /* End doctypeEnd */

static void
xmlDecl(void *userData,
	const char *version,
	const char *encoding,
	int standalone) {
  dSP;
  CallbackVector *cbv = (CallbackVector*) userData;

  ENTER;
  SAVETMPS;

  PUSHMARK(sp);
  EXTEND(sp, 4);
  PUSHs(cbv->self_sv);
  PUSHs(version ? sv_2mortal(newUTF8SVpv((char *)version, 0))
	: &PL_sv_undef);
  PUSHs(encoding ? sv_2mortal(newUTF8SVpv((char *)encoding, 0))
	: &PL_sv_undef);
  PUSHs(standalone == -1 ? &PL_sv_undef
	: (standalone ? &PL_sv_yes : &PL_sv_no));
  PUTBACK;
  perl_call_sv(cbv->xmldec_sv, G_DISCARD);
  FREETMPS;
  LEAVE;
}  /* End xmlDecl */

static void
unparsedEntityDecl(void *userData,
		   const char* entity,
		   const char* base,
		   const char* sysid,
		   const char* pubid,
		   const char* notation)
{
  dSP;
  CallbackVector* cbv = (CallbackVector*) userData;

  ENTER;
  SAVETMPS;

  PUSHMARK(sp);
  EXTEND(sp, 6);
  PUSHs(cbv->self_sv);
  PUSHs(sv_2mortal(newUTF8SVpv((char*) entity, 0)));
  PUSHs(base ? sv_2mortal(newUTF8SVpv((char*) base, 0)) : &PL_sv_undef);
  PUSHs(sv_2mortal(newUTF8SVpv((char*) sysid, 0)));
  PUSHs(pubid ? sv_2mortal(newUTF8SVpv((char*) pubid, 0)) : &PL_sv_undef);
  PUSHs(sv_2mortal(newUTF8SVpv((char*) notation, 0)));
  PUTBACK;
  perl_call_sv(cbv->unprsd_sv, G_DISCARD);

  FREETMPS;
  LEAVE;
}  /* End unparsedEntityDecl */

static void
notationDecl(void *userData,
	     const char *name,
	     const char *base,
	     const char *sysid,
	     const char *pubid)
{
  dSP;
  CallbackVector* cbv = (CallbackVector*) userData;

  PUSHMARK(sp);
  XPUSHs(cbv->self_sv);
  XPUSHs(sv_2mortal(newUTF8SVpv((char*) name, 0)));
  if (base)
    {
      XPUSHs(sv_2mortal(newUTF8SVpv((char *) base, 0)));
    }
  else if (sysid || pubid)
    {
      XPUSHs(&PL_sv_undef);
    }

  if (sysid)
    {
      XPUSHs(sv_2mortal(newUTF8SVpv((char *) sysid, 0)));
    }
  else if (pubid)
    {
      XPUSHs(&PL_sv_undef);
    }
  
  if (pubid)
    XPUSHs(sv_2mortal(newUTF8SVpv((char *) pubid, 0)));

  PUTBACK;
  perl_call_sv(cbv->notation_sv, G_DISCARD);
}  /* End notationDecl */

static int
externalEntityRef(XML_Parser parser,
		  const char* open,
		  const char* base,
		  const char* sysid,
		  const char* pubid)
{
  dSP;
#if defined(USE_THREADS) && PATCHLEVEL==6
  dTHX;
#endif

  int count;
  int ret = 0;
  int parse_done = 0;

  CallbackVector* cbv = (CallbackVector*) XML_GetUserData(parser);

  if (! cbv->extent_sv)
    return 0;

  ENTER ;
  SAVETMPS ;
  PUSHMARK(sp);
  EXTEND(sp, pubid ? 4 : 3);
  PUSHs(cbv->self_sv);
  PUSHs(base ? sv_2mortal(newUTF8SVpv((char*) base, 0)) : &PL_sv_undef);
  PUSHs(sv_2mortal(newSVpv((char*) sysid, 0)));
  if (pubid)
    PUSHs(sv_2mortal(newUTF8SVpv((char*) pubid, 0)));
  PUTBACK ;
  count = perl_call_sv(cbv->extent_sv, G_SCALAR);

  SPAGAIN ;

  if (count >= 1) {
    SV * result = POPs;
    int type;

    if (result && (type = SvTYPE(result)) > 0) {
      SV **pval = hv_fetch((HV*) SvRV(cbv->self_sv), "Parser", 6, 0);

      if (! pval || ! SvIOK(*pval))
	append_error(parser, "Can't find parser entry in XML::Parser object");
      else {
	XML_Parser entpar;
	char *errmsg = (char *) 0;

	entpar = XML_ExternalEntityParserCreate(parser, open, 0);

	XML_SetBase(entpar, XML_GetBase(parser));

	sv_setiv(*pval, (IV) entpar);

	cbv->p = entpar;

	PUSHMARK(sp);
	EXTEND(sp, 2);
	PUSHs(*pval);
	PUSHs(result);
	PUTBACK;
	count = perl_call_pv("XML::Parser::Expat::Do_External_Parse",
			     G_SCALAR | G_EVAL);
	SPAGAIN;

	if (SvTRUE(ERRSV)) {
	  char *hold;
	  STRLEN   len;

	  POPs;
	  hold = SvPV(ERRSV, len);
	  New(326, errmsg, len + 1, char);
	  if (len)
	    Copy(hold, errmsg, len, char);
	  goto Extparse_Cleanup;
	}

	if (count > 0)
	  ret = POPi;
	  
	parse_done = 1;

      Extparse_Cleanup:
	cbv->p = parser;
	sv_setiv(*pval, (IV) parser);
	XML_ParserFree(entpar);
	     
	if (cbv->extfin_sv) {
	  PUSHMARK(sp);
	  PUSHs(cbv->self_sv);
	  PUTBACK;
	  perl_call_sv(cbv->extfin_sv, G_DISCARD);
	  SPAGAIN;
	}

	if (SvTRUE(ERRSV))
	  append_error(parser, SvPV_nolen(ERRSV));
      }
    }
  }

  if (! ret && ! parse_done)
    append_error(parser, "Handler couldn't resolve external entity");

  PUTBACK ;
  FREETMPS ;
  LEAVE ;

  return ret;
}  /* End externalEntityRef */

/*================================================================
** This is the function that expat calls to convert multi-byte sequences
** for external encodings. Each byte in the sequence is used to index
** into the current map to either set the next map or, in the case of
** the final byte, to get the corresponding Unicode scalar, which is
** returned.
*/

static int
convert_to_unicode(void *data, const char *seq) {
  Encinfo *enc = (Encinfo *) data;
  PrefixMap *curpfx;
  int count;
  int index = 0;

  for (count = 0; count < 4; count++) {
    unsigned char byte = (unsigned char) seq[count];
    unsigned char bndx;
    unsigned char bmsk;
    int offset;

    curpfx = &enc->prefixes[index];
    offset = ((int) byte) - curpfx->min;
    if (offset < 0)
      break;
    if (offset >= curpfx->len && curpfx->len != 0)
      break;

    bndx = byte >> 3;
    bmsk = 1 << (byte & 0x7);

    if (curpfx->ispfx[bndx] & bmsk) {
      index = enc->bytemap[curpfx->bmap_start + offset];
    }
    else if (curpfx->ischar[bndx] & bmsk) {
      return enc->bytemap[curpfx->bmap_start + offset];
    }
    else
      break;
  }

  return -1;
}  /* End convert_to_unicode */

static int
unknownEncoding(void *unused, const char *name, XML_Encoding *info)
{
  SV ** encinfptr;
  Encinfo *enc;
  int namelen;
  int i;
  char buff[42];

  namelen = strlen(name);
  if (namelen > 40)
    return 0;

  /* Make uppercase */
  for (i = 0; i < namelen; i++) {
    char c = name[i];
    if (c >= 'a' && c <= 'z')
      c -= 'a' - 'A';
    buff[i] = c;
  }

  if (! EncodingTable) {
    EncodingTable = perl_get_hv("XML::Parser::Expat::Encoding_Table", FALSE);
    if (! EncodingTable)
      croak("Can't find XML::Parser::Expat::Encoding_Table");
  }

  encinfptr = hv_fetch(EncodingTable, buff, namelen, 0);

  if (! encinfptr || ! SvOK(*encinfptr)) {
    /* Not found, so try to autoload */
    dSP;
    int count;

    ENTER;
    SAVETMPS;
    PUSHMARK(sp);
    XPUSHs(sv_2mortal(newSVpvn(buff,namelen)));
    PUTBACK;
    perl_call_pv("XML::Parser::Expat::load_encoding", G_DISCARD);
    
    encinfptr = hv_fetch(EncodingTable, buff, namelen, 0);
    FREETMPS;
    LEAVE;

    if (! encinfptr || ! SvOK(*encinfptr))
      return 0;
  }

  if (! sv_derived_from(*encinfptr, "XML::Parser::Encinfo"))
    croak("Entry in XML::Parser::Expat::Encoding_Table not an Encinfo object");

  enc = (Encinfo *) SvIV((SV*)SvRV(*encinfptr));
  Copy(enc->firstmap, info->map, 256, int);
  info->release = NULL;
  if (enc->prefixes_size) {
    info->data = (void *) enc;
    info->convert = convert_to_unicode;
  }
  else {
    info->data = NULL;
    info->convert = NULL;
  }

  return 1;
}  /* End unknownEncoding */


static void
recString(void *userData, const char *string, int len)
{
  CallbackVector *cbv = (CallbackVector*) userData;

  if (cbv->recstring) {
    sv_catpvn(cbv->recstring, (char *) string, len);
  }
  else {
    cbv->recstring = newUTF8SVpvn((char *) string, len);
  }
}  /* End recString */

static void
suspend_callbacks(CallbackVector *cbv) {
  if (SvTRUE(cbv->char_sv)) {
    XML_SetCharacterDataHandler(cbv->p,
				(XML_CharacterDataHandler) 0);
  }

  if (SvTRUE(cbv->proc_sv)) {
    XML_SetProcessingInstructionHandler(cbv->p,
					(XML_ProcessingInstructionHandler) 0);
  }

  if (SvTRUE(cbv->cmnt_sv)) {
    XML_SetCommentHandler(cbv->p,
			  (XML_CommentHandler) 0);
  }

  if (SvTRUE(cbv->startcd_sv)
      || SvTRUE(cbv->endcd_sv)) {
    XML_SetCdataSectionHandler(cbv->p,
			       (XML_StartCdataSectionHandler) 0,
			       (XML_EndCdataSectionHandler) 0);
  }

  if (SvTRUE(cbv->unprsd_sv)) {
    XML_SetUnparsedEntityDeclHandler(cbv->p,
				     (XML_UnparsedEntityDeclHandler) 0);
  }

  if (SvTRUE(cbv->notation_sv)) {
    XML_SetNotationDeclHandler(cbv->p,
			       (XML_NotationDeclHandler) 0);
  }

  if (SvTRUE(cbv->extent_sv)) {
    XML_SetExternalEntityRefHandler(cbv->p,
				    (XML_ExternalEntityRefHandler) 0);
  }

}  /* End suspend_callbacks */

static void
resume_callbacks(CallbackVector *cbv) {
  if (SvTRUE(cbv->char_sv)) {
    XML_SetCharacterDataHandler(cbv->p, characterData);
  }

  if (SvTRUE(cbv->proc_sv)) {
    XML_SetProcessingInstructionHandler(cbv->p, processingInstruction);
  }

  if (SvTRUE(cbv->cmnt_sv)) {
    XML_SetCommentHandler(cbv->p, commenthandle);
  }

  if (SvTRUE(cbv->startcd_sv)
      || SvTRUE(cbv->endcd_sv)) {
    XML_SetCdataSectionHandler(cbv->p, startCdata, endCdata);
  }

  if (SvTRUE(cbv->unprsd_sv)) {
    XML_SetUnparsedEntityDeclHandler(cbv->p, unparsedEntityDecl);
  }

  if (SvTRUE(cbv->notation_sv)) {
    XML_SetNotationDeclHandler(cbv->p, notationDecl);
  }

  if (SvTRUE(cbv->extent_sv)) {
    XML_SetExternalEntityRefHandler(cbv->p, externalEntityRef);
  }

}  /* End resume_callbacks */


MODULE = XML::Parser::Expat PACKAGE = XML::Parser::Expat	PREFIX = XML_

XML_Parser
XML_ParserCreate(self_sv, enc_sv, namespaces)
        SV *                    self_sv
	SV *			enc_sv
	int			namespaces
    CODE:
	{
	  CallbackVector *cbv;
	  enum XML_ParamEntityParsing pep = XML_PARAM_ENTITY_PARSING_NEVER;
	  char *enc = (char *) (SvTRUE(enc_sv) ? SvPV_nolen(enc_sv) : 0);
	  SV ** spp;

	  Newz(320, cbv, 1, CallbackVector);
	  cbv->self_sv = SvREFCNT_inc(self_sv);
	  Newz(325, cbv->st_serial_stack, 1024, unsigned int);
	  spp = hv_fetch((HV*)SvRV(cbv->self_sv), "NoExpand", 8, 0);
	  if (spp && SvTRUE(*spp))
	    cbv->no_expand = 1;

	  spp = hv_fetch((HV*)SvRV(cbv->self_sv), "Context", 7, 0);
	  if (! spp || ! *spp || !SvROK(*spp))
	    croak("XML::Parser instance missing Context");

	  cbv->context = (AV*) SvRV(*spp);
	  
	  cbv->ns = (unsigned) namespaces;
	  if (namespaces)
	    {
	      spp = hv_fetch((HV*)SvRV(cbv->self_sv), "New_Prefixes", 12, 0);
	      if (! spp || ! *spp || !SvROK(*spp))
	        croak("XML::Parser instance missing New_Prefixes");

	      cbv->new_prefix_list = (AV *) SvRV(*spp);

	      spp = hv_fetch((HV*)SvRV(cbv->self_sv), "Namespace_Table",
			     15, FALSE);
	      if (! spp || ! *spp || !SvROK(*spp))
	        croak("XML::Parser instance missing Namespace_Table");

	      cbv->nstab = (HV *) SvRV(*spp);

	      spp = hv_fetch((HV*)SvRV(cbv->self_sv), "Namespace_List",
			     14, FALSE);
	      if (! spp || ! *spp || !SvROK(*spp))
	        croak("XML::Parser instance missing Namespace_List");

	      cbv->nslst = (AV *) SvRV(*spp);

	      RETVAL = XML_ParserCreate_MM(enc, &ms, nsdelim);
	      XML_SetNamespaceDeclHandler(RETVAL,nsStart, nsEnd);
	    }
	    else
	    {
	      RETVAL = XML_ParserCreate_MM(enc, &ms, NULL);
	    }
	    
	  cbv->p = RETVAL;
	  XML_SetUserData(RETVAL, (void *) cbv);
	  XML_SetElementHandler(RETVAL, startElement, endElement);
	  XML_SetUnknownEncodingHandler(RETVAL, unknownEncoding, 0);

	  spp = hv_fetch((HV*)SvRV(cbv->self_sv), "ParseParamEnt",
			 13, FALSE);

	  if (spp && SvTRUE(*spp)) {
	    pep = XML_PARAM_ENTITY_PARSING_UNLESS_STANDALONE;
	    cbv->parseparam = 1;
	  }	

	  XML_SetParamEntityParsing(RETVAL, pep);
	}
    OUTPUT:
	RETVAL

void
XML_ParserRelease(parser)
      XML_Parser parser
    CODE:
      {
        CallbackVector * cbv = (CallbackVector *) XML_GetUserData(parser);

	SvREFCNT_dec(cbv->self_sv);
      }

void
XML_ParserFree(parser)
	XML_Parser parser
    CODE:
	{
	  CallbackVector * cbv = (CallbackVector *) XML_GetUserData(parser);

	  Safefree(cbv->st_serial_stack);


	  /* Clean up any SVs that we have */
	  /* (Note that self_sv must already be taken care of
	     or we couldn't be here */

	  if (cbv->recstring)
	    SvREFCNT_dec(cbv->recstring);

	  if (cbv->start_sv)
	    SvREFCNT_dec(cbv->start_sv);

	  if (cbv->end_sv)
	    SvREFCNT_dec(cbv->end_sv);

	  if (cbv->char_sv)
	    SvREFCNT_dec(cbv->char_sv);

	  if (cbv->proc_sv)
	    SvREFCNT_dec(cbv->proc_sv);

	  if (cbv->cmnt_sv)
	    SvREFCNT_dec(cbv->cmnt_sv);

	  if (cbv->dflt_sv)
	    SvREFCNT_dec(cbv->dflt_sv);

	  if (cbv->entdcl_sv)
	    SvREFCNT_dec(cbv->entdcl_sv);

	  if (cbv->eledcl_sv)
	    SvREFCNT_dec(cbv->eledcl_sv);

	  if (cbv->attdcl_sv)
	    SvREFCNT_dec(cbv->attdcl_sv);

	  if (cbv->doctyp_sv)
	    SvREFCNT_dec(cbv->doctyp_sv);

	  if (cbv->doctypfin_sv)
	    SvREFCNT_dec(cbv->doctypfin_sv);

	  if (cbv->xmldec_sv)
	    SvREFCNT_dec(cbv->xmldec_sv);

	  if (cbv->unprsd_sv)
	    SvREFCNT_dec(cbv->unprsd_sv);

	  if (cbv->notation_sv)
	    SvREFCNT_dec(cbv->notation_sv);

	  if (cbv->extent_sv)
	    SvREFCNT_dec(cbv->extent_sv);

	  if (cbv->extfin_sv)
	    SvREFCNT_dec(cbv->extfin_sv);

	  if (cbv->startcd_sv)
	    SvREFCNT_dec(cbv->startcd_sv);

	  if (cbv->endcd_sv)
	    SvREFCNT_dec(cbv->endcd_sv);

	  /* ================ */
	    
	  Safefree(cbv);
	  XML_ParserFree(parser);
	}

int
XML_ParseString(parser, sv)
        XML_Parser			parser
	SV *				sv
    CODE:
        {
	  CallbackVector * cbv;
	  STRLEN len;
	  char *s = SvPV(sv, len);

          cbv = (CallbackVector *) XML_GetUserData(parser);
	  

	  RETVAL = XML_Parse(parser, s, len, 1);
	  SPAGAIN; /* XML_Parse might have changed stack pointer */
	  if (! RETVAL)
	    append_error(parser, NULL);
	}

    OUTPUT:
	RETVAL

int
XML_ParseStream(parser, ioref, delim)
	XML_Parser			parser
	SV *				ioref
	SV *				delim
    CODE:
	{
	  SV **delimsv;
	  CallbackVector * cbv;

	  cbv = (CallbackVector *) XML_GetUserData(parser);
	  if (SvOK(delim)) {
	    cbv->delim = SvPV(delim, cbv->delimlen);
	  }
	  else {
	    cbv->delim = (char *) 0;
	  }
	      
	  RETVAL = parse_stream(parser, ioref);
	  SPAGAIN; /* parse_stream might have changed stack pointer */
	}

    OUTPUT:
	RETVAL

int
XML_ParsePartial(parser, sv)
	XML_Parser			parser
	SV *				sv
    CODE:
	{
	  STRLEN len;
	  char *s = SvPV(sv, len);
	  CallbackVector * cbv = (CallbackVector *) XML_GetUserData(parser);

	  RETVAL = XML_Parse(parser, s, len, 0);
	  if (! RETVAL)
	    append_error(parser, NULL);
	}

    OUTPUT:
	RETVAL


int
XML_ParseDone(parser)
	XML_Parser			parser
    CODE:
	{
	  RETVAL = XML_Parse(parser, "", 0, 1);
	  if (! RETVAL)
	    append_error(parser, NULL);
	}

    OUTPUT:
	RETVAL

SV *
XML_SetStartElementHandler(parser, start_sv)
	XML_Parser			parser
	SV *				start_sv
    CODE:
	{
	  CallbackVector * cbv = (CallbackVector*) XML_GetUserData(parser);
	  XMLP_UPD(start_sv);
	  PUSHRET;
	}

SV *
XML_SetEndElementHandler(parser, end_sv)
	XML_Parser			parser
	SV *				end_sv
    CODE:
	{
	  CallbackVector *cbv = (CallbackVector*) XML_GetUserData(parser);
	  XMLP_UPD(end_sv);
	  PUSHRET;
	}

SV *
XML_SetCharacterDataHandler(parser, char_sv)
	XML_Parser			parser
	SV *				char_sv
    CODE:
	{
	  XML_CharacterDataHandler charhndl = (XML_CharacterDataHandler) 0;
	  CallbackVector * cbv = (CallbackVector*) XML_GetUserData(parser);

	  XMLP_UPD(char_sv);
	  if (SvTRUE(char_sv))
	    charhndl = characterData;

	  XML_SetCharacterDataHandler(parser, charhndl);
	  PUSHRET;
	}

SV *
XML_SetProcessingInstructionHandler(parser, proc_sv)
	XML_Parser			parser
	SV *				proc_sv
    CODE:
	{
	  XML_ProcessingInstructionHandler prochndl =
	    (XML_ProcessingInstructionHandler) 0;
	  CallbackVector* cbv = (CallbackVector*) XML_GetUserData(parser);

	  XMLP_UPD(proc_sv);
	  if (SvTRUE(proc_sv))
	    prochndl = processingInstruction;

	  XML_SetProcessingInstructionHandler(parser, prochndl);
	  PUSHRET;
	}

SV *
XML_SetCommentHandler(parser, cmnt_sv)
	XML_Parser			parser
	SV *				cmnt_sv
    CODE:
	{
	  XML_CommentHandler cmnthndl = (XML_CommentHandler) 0;
	  CallbackVector * cbv = (CallbackVector*) XML_GetUserData(parser);

	  XMLP_UPD(cmnt_sv);
	  if (SvTRUE(cmnt_sv))
	    cmnthndl = commenthandle;

	  XML_SetCommentHandler(parser, cmnthndl);
	  PUSHRET;
	}

SV *
XML_SetDefaultHandler(parser, dflt_sv)
	XML_Parser			parser
	SV *				dflt_sv
    CODE:
	{
	  XML_DefaultHandler dflthndl = (XML_DefaultHandler) 0;
	  CallbackVector * cbv = (CallbackVector*) XML_GetUserData(parser);

	  XMLP_UPD(dflt_sv);
	  if (SvTRUE(dflt_sv))
	    dflthndl = defaulthandle;

	  if (cbv->no_expand)
	    XML_SetDefaultHandler(parser, dflthndl);
	  else
	    XML_SetDefaultHandlerExpand(parser, dflthndl);

	  PUSHRET;
	}

SV *
XML_SetUnparsedEntityDeclHandler(parser, unprsd_sv)
	XML_Parser			parser
	SV *				unprsd_sv
    CODE:
	{
	  XML_UnparsedEntityDeclHandler unprsdhndl =
	    (XML_UnparsedEntityDeclHandler) 0;
	  CallbackVector * cbv = (CallbackVector*) XML_GetUserData(parser);

	  XMLP_UPD(unprsd_sv);
	  if (SvTRUE(unprsd_sv))
	    unprsdhndl = unparsedEntityDecl;

	  XML_SetUnparsedEntityDeclHandler(parser, unprsdhndl);
	  PUSHRET;
	}

SV *
XML_SetNotationDeclHandler(parser, notation_sv)
	XML_Parser			parser
	SV *				notation_sv
    CODE:
	{
	  XML_NotationDeclHandler nothndlr = (XML_NotationDeclHandler) 0;
	  CallbackVector * cbv = (CallbackVector*) XML_GetUserData(parser);

	  XMLP_UPD(notation_sv);
	  if (SvTRUE(notation_sv))
	    nothndlr = notationDecl;

	  XML_SetNotationDeclHandler(parser, nothndlr);
	  PUSHRET;
	}

SV *
XML_SetExternalEntityRefHandler(parser, extent_sv)
	XML_Parser			parser
	SV *				extent_sv
    CODE:
	{
	  XML_ExternalEntityRefHandler exthndlr =
	    (XML_ExternalEntityRefHandler) 0;
	  CallbackVector * cbv = (CallbackVector*) XML_GetUserData(parser);

	  XMLP_UPD(extent_sv);
	  if (SvTRUE(extent_sv))
	    exthndlr = externalEntityRef;

	  XML_SetExternalEntityRefHandler(parser, exthndlr);
	  PUSHRET;
	}

SV *
XML_SetExtEntFinishHandler(parser, extfin_sv)
	XML_Parser			parser
	SV *				extfin_sv
    CODE:
	{
	  CallbackVector * cbv = (CallbackVector *) XML_GetUserData(parser);

	  /* There is no corresponding handler for this in expat. This is
	     called from the externalEntityRef function above after parsing
	     the external entity. */

	  XMLP_UPD(extfin_sv);
	  PUSHRET;
	}

	   
SV *
XML_SetEntityDeclHandler(parser, entdcl_sv)
	XML_Parser			parser
	SV *				entdcl_sv
    CODE:
	{
	  XML_EntityDeclHandler enthndlr =
	    (XML_EntityDeclHandler) 0;
	  CallbackVector * cbv = (CallbackVector*) XML_GetUserData(parser);

	  XMLP_UPD(entdcl_sv);
	  if (SvTRUE(entdcl_sv))
	    enthndlr = entityDecl;

	  XML_SetEntityDeclHandler(parser, enthndlr);
	  PUSHRET;
	}

SV *
XML_SetElementDeclHandler(parser, eledcl_sv)
	XML_Parser			parser
	SV *				eledcl_sv
    CODE:
	{
	  XML_ElementDeclHandler eldeclhndlr =
	    (XML_ElementDeclHandler) 0;
	  CallbackVector * cbv = (CallbackVector*) XML_GetUserData(parser);

	  XMLP_UPD(eledcl_sv);
	  if (SvTRUE(eledcl_sv))
	    eldeclhndlr = elementDecl;

	  XML_SetElementDeclHandler(parser, eldeclhndlr);
	  PUSHRET;
	}

SV *
XML_SetAttListDeclHandler(parser, attdcl_sv)
	XML_Parser			parser
	SV *				attdcl_sv
    CODE:
	{
	  XML_AttlistDeclHandler attdeclhndlr =
	    (XML_AttlistDeclHandler) 0;
	  CallbackVector * cbv = (CallbackVector*) XML_GetUserData(parser);

	  XMLP_UPD(attdcl_sv);
	  if (SvTRUE(attdcl_sv))
	    attdeclhndlr = attributeDecl;

	  XML_SetAttlistDeclHandler(parser, attdeclhndlr);
	  PUSHRET;
	}

SV *
XML_SetDoctypeHandler(parser, doctyp_sv)
	XML_Parser			parser
	SV *				doctyp_sv
    CODE:
	{
	  XML_StartDoctypeDeclHandler dtsthndlr =
	    (XML_StartDoctypeDeclHandler) 0;
	  CallbackVector * cbv = (CallbackVector*) XML_GetUserData(parser);
	  int set = 0;

	  XMLP_UPD(doctyp_sv);
	  if (SvTRUE(doctyp_sv))
	    dtsthndlr = doctypeStart;

	  XML_SetStartDoctypeDeclHandler(parser, dtsthndlr);
	  PUSHRET;
	}

SV *
XML_SetEndDoctypeHandler(parser, doctypfin_sv)
	XML_Parser parser
	SV * doctypfin_sv     
    CODE:
	{
	  XML_EndDoctypeDeclHandler dtendhndlr =
	    (XML_EndDoctypeDeclHandler) 0;
	  CallbackVector * cbv = (CallbackVector*) XML_GetUserData(parser);

	  XMLP_UPD(doctypfin_sv);
	  if (SvTRUE(doctypfin_sv))
	    dtendhndlr = doctypeEnd;

	  XML_SetEndDoctypeDeclHandler(parser, dtendhndlr);
	  PUSHRET;
	}


SV *
XML_SetXMLDeclHandler(parser, xmldec_sv)
	XML_Parser			parser
	SV *				xmldec_sv
    CODE:
	{
	  XML_XmlDeclHandler xmldechndlr =
	    (XML_XmlDeclHandler) 0;
	  CallbackVector * cbv = (CallbackVector *) XML_GetUserData(parser);

	  XMLP_UPD(xmldec_sv);
	  if (SvTRUE(xmldec_sv))
	    xmldechndlr = xmlDecl;

	  XML_SetXmlDeclHandler(parser, xmldechndlr);
	  PUSHRET;
	}


void
XML_SetBase(parser, base)
	XML_Parser			parser
	SV *				base
    CODE:
	{
	  char * b;

	  if (! SvOK(base)) {
	    b = (char *) 0;
	  }
	  else {
	    b = SvPV_nolen(base);
	  }

	  XML_SetBase(parser, b);
	}	


SV *
XML_GetBase(parser)
	XML_Parser			parser
    CODE:
	{
	  const char *ret = XML_GetBase(parser);
	  if (ret) {
	    ST(0) = sv_newmortal();
	    sv_setpv(ST(0), ret);
	  }
	  else {
	    ST(0) = &PL_sv_undef;
	  }
	}

void
XML_PositionContext(parser, lines)
	XML_Parser			parser
	int				lines
    PREINIT:
	int parsepos;
        int size;
        const char *pos = XML_GetInputContext(parser, &parsepos, &size);
	const char *markbeg;
	const char *limit;
	const char *markend;
	int length, relpos;
	int  cnt;

    PPCODE:
	  if (! pos)
            return;

	  for (markbeg = &pos[parsepos], cnt = 0; markbeg >= pos; markbeg--)
	    {
	      if (*markbeg == '\n')
		{
		  cnt++;
		  if (cnt > lines)
		    break;
		}
	    }

	  markbeg++;

          relpos = 0;
	  limit = &pos[size];
	  for (markend = &pos[parsepos + 1], cnt = 0;
	       markend < limit;
	       markend++)
	    {
	      if (*markend == '\n')
		{
		  if (cnt == 0)
                     relpos = (markend - markbeg) + 1;
		  cnt++;
		  if (cnt > lines)
		    {
		      markend++;
		      break;
		    }
		}
	    }

	  length = markend - markbeg;
          if (relpos == 0)
            relpos = length;

          EXTEND(sp, 2);
	  PUSHs(sv_2mortal(newSVpvn((char *) markbeg, length)));
	  PUSHs(sv_2mortal(newSViv(relpos)));

SV *
GenerateNSName(name, xml_namespace, table, list)
	SV *				name
	SV *				xml_namespace
	SV *				table
	SV *				list
    CODE:
	{
	  STRLEN	nmlen, nslen;
	  char *	nmstr;
	  char *	nsstr;
	  char *	buff;
	  char *	bp;
	  char *	blim;

	  nmstr = SvPV(name, nmlen);
	  nsstr = SvPV(xml_namespace, nslen);

	  /* Form a namespace-name string that looks like expat's */
	  New(321, buff, nmlen + nslen + 2, char);
	  bp = buff;
	  blim = bp + nslen;
	  while (bp < blim)
	    *bp++ = *nsstr++;
	  *bp++ = NSDELIM;
	  blim = bp + nmlen;
	  while (bp < blim)
	    *bp++ = *nmstr++;
	  *bp = '\0';

	  RETVAL = gen_ns_name(buff, (HV *) SvRV(table), (AV *) SvRV(list));
	  Safefree(buff);
	}	
    OUTPUT:
	RETVAL

void
XML_DefaultCurrent(parser)
	XML_Parser			parser
    CODE:
	{
	  CallbackVector * cbv = (CallbackVector *) XML_GetUserData(parser);

	  XML_DefaultCurrent(parser);
	}

SV *
XML_RecognizedString(parser)
	XML_Parser			parser
    CODE:
	{
	  XML_DefaultHandler dflthndl = (XML_DefaultHandler) 0;
	  CallbackVector * cbv = (CallbackVector *) XML_GetUserData(parser);

	  if (cbv->dflt_sv) {
	    dflthndl = defaulthandle;
	  }

	  if (cbv->recstring) {
	    sv_setpvn(cbv->recstring, "", 0);
	  }

	  if (cbv->no_expand)
	    XML_SetDefaultHandler(parser, recString);
	  else
	    XML_SetDefaultHandlerExpand(parser, recString);
	      
	  XML_DefaultCurrent(parser);

	  if (cbv->no_expand)
	    XML_SetDefaultHandler(parser, dflthndl);
	  else
	    XML_SetDefaultHandlerExpand(parser, dflthndl);

	  RETVAL = newSVsv(cbv->recstring);
	}
    OUTPUT:
	RETVAL

int
XML_GetErrorCode(parser)
	XML_Parser			parser

int
XML_GetCurrentLineNumber(parser)
	XML_Parser			parser


int
XML_GetCurrentColumnNumber(parser)
	XML_Parser			parser

long
XML_GetCurrentByteIndex(parser)
	XML_Parser			parser

int
XML_GetSpecifiedAttributeCount(parser)
	XML_Parser			parser

char *
XML_ErrorString(code)
	int				code
    CODE:
	const char *ret = XML_ErrorString(code);
	ST(0) = sv_newmortal();
	sv_setpv((SV*)ST(0), ret);

SV *
XML_LoadEncoding(data, size)
	char *				data
	int				size
    CODE:
	{
	  Encmap_Header *emh = (Encmap_Header *) data;
	  unsigned pfxsize, bmsize;

	  if (size < sizeof(Encmap_Header)
	      || ntohl(emh->magic) != ENCMAP_MAGIC) {
	    RETVAL = &PL_sv_undef;
	  }
	  else {
	    Encinfo	*entry;
	    SV		*sv;
	    PrefixMap	*pfx;
	    unsigned short *bm;
	    int namelen;
	    int i;

	    pfxsize = ntohs(emh->pfsize);
	    bmsize  = ntohs(emh->bmsize);

	    if (size != (sizeof(Encmap_Header)
			 + pfxsize * sizeof(PrefixMap)
			 + bmsize * sizeof(unsigned short))) {
	      RETVAL = &PL_sv_undef;
	    }
	    else {
	      /* Convert to uppercase and get name length */

	      for (i = 0; i < sizeof(emh->name); i++) {
		char c = emh->name[i];

		  if (c == (char) 0)
		    break;

		if (c >= 'a' && c <= 'z')
		  emh->name[i] -= 'a' - 'A';
	      }
	      namelen = i;

	      RETVAL = newSVpvn(emh->name, namelen);

	      New(322, entry, 1, Encinfo);
	      entry->prefixes_size = pfxsize;
	      entry->bytemap_size  = bmsize;
	      for (i = 0; i < 256; i++) {
		entry->firstmap[i] = ntohl(emh->map[i]);
	      }

	      pfx = (PrefixMap *) &data[sizeof(Encmap_Header)];
	      bm = (unsigned short *) (((char *) pfx)
				       + sizeof(PrefixMap) * pfxsize);

	      New(323, entry->prefixes, pfxsize, PrefixMap);
	      New(324, entry->bytemap, bmsize, unsigned short);

	      for (i = 0; i < pfxsize; i++, pfx++) {
		PrefixMap *dest = &entry->prefixes[i];

		dest->min = pfx->min;
		dest->len = pfx->len;
		dest->bmap_start = ntohs(pfx->bmap_start);
		Copy(pfx->ispfx, dest->ispfx,
		     sizeof(pfx->ispfx) + sizeof(pfx->ischar), unsigned char);
	      }

	      for (i = 0; i < bmsize; i++)
		entry->bytemap[i] = ntohs(bm[i]);

	      sv = newSViv(0);
	      sv_setref_pv(sv, "XML::Parser::Encinfo", (void *) entry);
	  
	      if (! EncodingTable) {
		EncodingTable
		  = perl_get_hv("XML::Parser::Expat::Encoding_Table",
				FALSE);
		if (! EncodingTable)
		  croak("Can't find XML::Parser::Expat::Encoding_Table");
	      }

	      hv_store(EncodingTable, emh->name, namelen, sv, 0);
	    }
	  }
	}
    OUTPUT:
	RETVAL

void
XML_FreeEncoding(enc)
	Encinfo *			enc
    CODE:
	Safefree(enc->bytemap);
	Safefree(enc->prefixes);
	Safefree(enc);

SV *
XML_OriginalString(parser)
	XML_Parser			parser
    CODE:
	{
	  int parsepos, size;
	  const char *buff = XML_GetInputContext(parser, &parsepos, &size);
	  if (buff) {
	    RETVAL = newSVpvn((char *) &buff[parsepos],
			      XML_GetCurrentByteCount(parser));
	  }
	  else {
	    RETVAL = newSVpv("", 0);
	  }
	}
    OUTPUT:
	RETVAL

SV *
XML_SetStartCdataHandler(parser, startcd_sv)
	XML_Parser			parser
	SV *				startcd_sv
    CODE:
	{
	  CallbackVector * cbv = (CallbackVector *) XML_GetUserData(parser);
	  XML_StartCdataSectionHandler scdhndl =
	    (XML_StartCdataSectionHandler) 0;

	  XMLP_UPD(startcd_sv);
	  if (SvTRUE(startcd_sv))
	    scdhndl = startCdata;

	  XML_SetStartCdataSectionHandler(parser, scdhndl);
	  PUSHRET;
	}

SV *
XML_SetEndCdataHandler(parser, endcd_sv)
	XML_Parser			parser
	SV *				endcd_sv
    CODE:
	{
	  CallbackVector * cbv = (CallbackVector *) XML_GetUserData(parser);
	  XML_EndCdataSectionHandler ecdhndl =
	    (XML_EndCdataSectionHandler) 0;

	  XMLP_UPD(endcd_sv);
	  if (SvTRUE(endcd_sv))
	    ecdhndl = endCdata;

	  XML_SetEndCdataSectionHandler(parser, ecdhndl);
	  PUSHRET;
	}

void
XML_UnsetAllHandlers(parser)
	XML_Parser			parser
    CODE:
	{
	  CallbackVector * cbv = (CallbackVector *) XML_GetUserData(parser);
	  
	  suspend_callbacks(cbv);
	  if (cbv->ns) {
	    XML_SetNamespaceDeclHandler(cbv->p,
					(XML_StartNamespaceDeclHandler) 0,
					(XML_EndNamespaceDeclHandler) 0);
	  }

	  XML_SetElementHandler(parser,
				(XML_StartElementHandler) 0,
				(XML_EndElementHandler) 0);

	  XML_SetUnknownEncodingHandler(parser,
					(XML_UnknownEncodingHandler) 0,
					(void *) 0);
	}

int
XML_ElementIndex(parser)
        XML_Parser                      parser
    CODE:
        {
          CallbackVector * cbv = (CallbackVector *) XML_GetUserData(parser);
          RETVAL = cbv->st_serial_stack[cbv->st_serial_stackptr];
        }
    OUTPUT:
        RETVAL

void
XML_SkipUntil(parser, index)
         XML_Parser			parser
         unsigned int			index
    CODE:
        {
          CallbackVector * cbv = (CallbackVector *) XML_GetUserData(parser);
	  if (index <= cbv->st_serial)
	    return;
	  cbv->skip_until = index;
	  suspend_callbacks(cbv);
	}

int
XML_Do_External_Parse(parser, result)
	XML_Parser			parser
	SV *				result
    CODE:
	{
	  int type;

          CallbackVector * cbv = (CallbackVector *) XML_GetUserData(parser);
	  
	  if (SvROK(result) && SvOBJECT(SvRV(result))) {
	    RETVAL = parse_stream(parser, result);
	  }
	  else if (isGV(result)) {
	    RETVAL = parse_stream(parser,
				  sv_2mortal(newRV((SV*) GvIOp(result))));
	  }
	  else if (SvPOK(result)) {
	    STRLEN  eslen;
	    int pret;
	    char *entstr = SvPV(result, eslen);

	    RETVAL = XML_Parse(parser, entstr, eslen, 1);
	  }
	}
    OUTPUT:
        RETVAL


