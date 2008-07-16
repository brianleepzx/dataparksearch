/* Copyright (C) 2003-2008 Datapark corp. All rights reserved.
   Copyright (C) 2000-2002 Lavtech.com corp. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/
#include "dps_common.h"
#include "dps_store.h"
#include "dps_services.h"
#include "dps_xmalloc.h"
#include "dps_hash.h"
#include "dps_utils.h"
#include "dps_log.h"
#include "dps_vars.h"
#include "dps_parsehtml.h"
#include "dps_unicode.h"
#include "dps_unidata.h"
#include "dps_searchtool.h"
#include "dps_sgml.h"
#include "dps_sqldbms.h"
#include "dps_mutex.h"
#include "dps_base.h"
#include "dps_doc.h"
#include "dps_socket.h"
#include "dps_http.h"
#include "dps_charsetutils.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#ifdef   HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef HAVE_ZLIB
static int DoStore(DPS_AGENT *Agent, urlid_t rec_id, Byte *Doc, size_t DocSize, char *Client) {
  z_stream zstream;
  DPS_BASE_PARAM P;
  int rc = DPS_OK;
  Byte *CDoc = NULL;
  size_t dbnum = ((size_t)rec_id) % ((Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems);
  DPS_DB *db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[dbnum] : &Agent->dbl.db[dbnum];

            zstream.zalloc = Z_NULL;
            zstream.zfree = Z_NULL;
            zstream.opaque = Z_NULL;
            zstream.next_in = Doc;
          
            if (deflateInit2(&zstream, 9, Z_DEFLATED, 15, 9, Z_DEFAULT_STRATEGY) == Z_OK) {
          
              zstream.avail_in = DocSize;
              zstream.avail_out = 2 * DocSize;
              CDoc = zstream.next_out = (Byte *) DpsMalloc(2 * DocSize + 1);
              if (zstream.next_out == NULL) {
                return DPS_ERROR;
              }
              deflate(&zstream, Z_FINISH);
              deflateEnd(&zstream);


/* store operations */

              bzero(&P, sizeof(P));
              P.subdir = "store";
              P.basename = "doc";
              P.indname = "doc";
              P.rec_id = rec_id;
	      P.mode = DPS_WRITE_LOCK;
	      P.NFiles = (db->StoredFiles) ? db->StoredFiles : DpsVarListFindInt(&Agent->Vars, "StoredFiles", 0x100);
	      P.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
	      P.A = Agent;
	      if (DpsBaseWrite(&P, CDoc, zstream.total_out) != DPS_OK) {
		DpsLog(Agent, DPS_LOG_ERROR, "store/doc write error: %s", strerror(errno));
		rc = DPS_ERROR;
              }

	      DpsBaseClose(&P);
	      if (rc == DPS_OK) DpsLog(Agent, DPS_LOG_EXTRA, "[%s] Stored rec_id: %x Size: %d Ratio: %5.2f%%", Client,
				       rec_id, DocSize, 100.0 * zstream.total_out / DocSize);

	      if (Agent->Flags.OptimizeAtUpdate) {
		DpsBaseOptimize(&P, ((int)rec_id) >> DPS_BASE_BITS);
	      }

	      DPS_FREE(CDoc);
	      return rc;
/* /store operations */
	    }
	    return DPS_ERROR;
}


static int GetStore(DPS_AGENT *Agent, DPS_DOCUMENT *Doc, urlid_t rec_id, size_t dbnum, char *Client) {
  Byte *CDoc = NULL;
  z_stream zstream;
  DPS_BASE_PARAM P;
  DPS_DB *db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[dbnum] : &Agent->dbl.db[dbnum];

            DpsLog(Agent, DPS_LOG_EXTRA, "[%s] Retrieve rec_id: %x", Client, rec_id);
            bzero(&P, sizeof(P));
            P.subdir = "store";
            P.basename = "doc";
            P.indname = "doc";
            P.rec_id = rec_id;
	    P.NFiles = (db->StoredFiles) ? db->StoredFiles : DpsVarListFindInt(&Agent->Vars, "StoredFiles", 0x100);
	    P.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
	    P.A = Agent;
            if (DpsBaseOpen(&P, DPS_READ_LOCK) != DPS_OK) {
                Doc->Buf.size = 0;
		DpsBaseClose(&P);
		return DPS_ERROR;
            }
            
            if (P.Item.rec_id == rec_id) {
              if (lseek(P.Sfd, (off_t)P.Item.offset, SEEK_SET) == (off_t)-1) {
		DpsBaseClose(&P);
		return DPS_ERROR;
              }
              if ((zstream.avail_in = Doc->Buf.size = P.Item.size) != 0) {
		zstream.avail_out = 1 + ((P.Item.orig_size != 0) ? P.Item.orig_size : DPS_MAXDOCSIZE);
		CDoc = zstream.next_in = (Byte *) DpsMalloc(Doc->Buf.size + 1);
		Doc->Buf.buf = (char *) DpsRealloc(Doc->Buf.buf, zstream.avail_out + 1);
		zstream.next_out = (Byte *) Doc->Buf.buf;
		if (CDoc == NULL || Doc->Buf.buf == NULL) {
		  Doc->Buf.size = 0;
		  DpsBaseClose(&P);
		  DPS_FREE(CDoc);
		  return DPS_ERROR;
		}
		zstream.zalloc = Z_NULL;
		zstream.zfree = Z_NULL;
		zstream.opaque = Z_NULL;
		if ((read(P.Sfd, CDoc, Doc->Buf.size) != (ssize_t)Doc->Buf.size)
		    || (inflateInit2(&zstream, 15) != Z_OK)) {
		  Doc->Buf.size = 0;
		  DpsBaseClose(&P);
		  DPS_FREE(CDoc);
		  return DPS_ERROR;
		}
		inflate(&zstream, Z_FINISH);
		inflateEnd(&zstream);
		Doc->Buf.size = zstream.total_out;
		Doc->Buf.buf[Doc->Buf.size] = '\0';
		Doc->Buf.content = Doc->Buf.buf;

		DpsLog(Agent, DPS_LOG_EXTRA, "[%s] Retrieved rec_id: %x Size: %d Ratio: %5.2f%%", Client,
		       rec_id, Doc->Buf.size, 100.0 * zstream.total_in / Doc->Buf.size);
	      } else {
		DpsLog(Agent, DPS_LOG_DEBUG, "[%s] Zero size of rec_id: %x\n", Client, rec_id);
	      }
              
            } else {
	      DPS_FREE(Doc->Buf.buf);
	      Doc->Buf.size = 0;
              DpsLog(Agent, DPS_LOG_ERROR, "[%s] Not found rec_id: %x, StoredFiles: %d[x%x], VarDir: %s\n", 
		     Client, rec_id, P.NFiles, P.NFiles, P.vardir);
            }

	    DpsBaseClose(&P);
	    DPS_FREE(CDoc);
	    return DPS_OK;
}

static int DpsStoreDeleteRec(DPS_AGENT *Agent, int sd, urlid_t rec_id, char *Client) {
  size_t DocSize = 0, dbnum = ((size_t)rec_id) % ((Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems);
  DPS_BASE_PARAM P;
  DPS_DB *db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[dbnum] : &Agent->dbl.db[dbnum];

  bzero(&P, sizeof(P));
  P.subdir = "store";
  P.basename = "doc";
  P.indname = "doc";
  P.rec_id = rec_id;
  P.NFiles = (db->StoredFiles) ? db->StoredFiles : DpsVarListFindInt(&Agent->Vars, "StoredFiles", 0x100);
  P.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
  P.A = Agent;
  if (DpsBaseDelete(&P) != DPS_OK) {
    if (sd > 0) DpsSend(sd, &DocSize, sizeof(DocSize), 0); 
    DpsBaseClose(&P);
    return DPS_ERROR;
  }
  DpsBaseClose(&P);
  return DPS_OK;
}

#endif


/** Store compressed copy of document
    \return DPS_OK if successful, DPS_ERROR on error
 */

__C_LINK int __DPSCALL DpsStoreDoc(DPS_AGENT *Agent, DPS_DOCUMENT *Doc) {

#ifdef HAVE_ZLIB
  const char *hello = "S\0";
  char result[8];
  int s, r;
/*  size_t content_size = Doc->Buf.size - (Doc->Buf.content-Doc->Buf.buf);*/
  size_t content_size = Doc->Buf.size;
  urlid_t rec_id = DpsURL_ID(Doc, NULL);
  size_t dbnum = ((size_t)rec_id) % ((Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems);

  if ((Agent->Demons.nitems == 0) || ((s = Agent->Demons.Demon[dbnum].stored_sd) <= 0)) {
/*    return (Agent->Flags.do_store) ? DoStore(Agent, rec_id, Doc->Buf.content, content_size, "") : DPS_OK;*/
    return (Agent->Flags.do_store) ? DoStore(Agent, rec_id, Doc->Buf.buf, content_size, "") : DPS_OK;
  }

  r = Agent->Demons.Demon[dbnum].stored_rv;

  /* FIXME: add checking of send() results */
  DpsSend(s, hello, 1, 0);
  DpsSend(s, &rec_id, sizeof(rec_id), 0);
  DpsSend(s, &content_size, sizeof(content_size), 0);
/*  DpsSend(s, Doc->Buf.content, content_size, 0);*/
  DpsSend(s, Doc->Buf.buf, content_size, 0);

  return DPS_OK;
/*  
  if (DpsRecvall(r, result, 1) < 0) {
    return DPS_ERROR;
  }
  
  return (*result == 'O') ? DPS_OK : DPS_ERROR;
*/  
#else
  return DPS_OK;
#endif

}


/** Retrieve cached copy
    Caller must alloc Doc->Buf.buf
 */

__C_LINK int __DPSCALL DpsUnStoreDoc(DPS_AGENT *Agent, DPS_DOCUMENT *Doc, const char *origurl) {
  
#ifdef HAVE_ZLIB
  const char *hello = "G\0";
  int s, r;
  dpshash32_t rec_id;
  size_t content_size = 0, dbnum;
  ssize_t nread = 0;
  
/*  rec_id = (origurl) ? DpsStrHash32(origurl) :  DpsVarListFindInt(&Doc->Sections, "URL_ID", 0);*/
  rec_id = DpsURL_ID(Doc, origurl);
  Doc->Buf.size=0;
  dbnum = ((size_t)rec_id) % ((Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems);
  
  if ((Agent->Demons.nitems == 0) || ((s = Agent->Demons.Demon[dbnum].stored_sd) <= 0)) {
    if (!Agent->Flags.do_store) return DPS_OK;
    GetStore(Agent, Doc, rec_id, dbnum, "");
  } else {
  
    r = Agent->Demons.Demon[dbnum].stored_rv;

    /* FIXME: add send() results checking */
    DpsSend(s, hello, 1, 0);
    DpsSend(s, &rec_id, sizeof(rec_id), 0);
  
    if (
	(DpsRecvall(r, &content_size, sizeof(content_size), 360) < 0) ||
	(content_size == 0)     )   {
      return -1;
    }
 
    if (Doc->Buf.buf == NULL) {
      Doc->Buf.buf = (char*) DpsMalloc(content_size + 1);
    } else {
      Doc->Buf.buf = (char*) DpsRealloc(Doc->Buf.buf, content_size + 1);
    }
    Doc->Buf.size = content_size;
    Doc->Buf.allocated_size = content_size;
    if ( (content_size > 0) && (
				(Doc->Buf.buf == NULL) ||
				((nread = DpsRecvall(r, Doc->Buf.buf, content_size, 360)) < 0)
				) ) {
      return -2;
    }
  
    Doc->Buf.buf[nread] = '\0';
    Doc->Buf.size = nread;
  }

  if(origurl != NULL) {
    DpsVarListReplaceStr(&Doc->Sections, "URL", origurl);
/*    DpsVarListReplaceInt(&Doc->Sections, "URL_ID", rec_id);*/
    DpsVarListDel(&Doc->Sections, "E_URL");
    DpsVarListDel(&Doc->Sections, "URL_ID");
    DpsURLParse(&Doc->CurURL,origurl);
  }

  if (strncmp(DPS_NULL2EMPTY(Doc->Buf.buf), "HTTP/", 5)) {
    Doc->Buf.content = Doc->Buf.buf; /* for compatibility with data of old version */
/*    fprintf(stderr, "Data of old version\n");*/
  } else {
#ifdef WITH_PARSER
    char *ct;
    DPS_PARSER	*Parser;
#endif
    DpsParseHTTPResponse(Agent, Doc);
#ifdef WITH_PARSER
    ct = DpsVarListFindStr(&Doc->Sections, "Content-Type", "");
    Parser = DpsParserFind(&Agent->Conf->Parsers, ct);
    if(Parser) {
      DpsVarListReplaceStr(&Doc->Sections, "Content-Type", Parser->to_mime ? Parser->to_mime : "unknown");
    }
#endif
/*    fprintf(stderr, "Data of new version\n");*/
  }
/*
  fprintf(stderr, "Doc.Buf.size: %d\nDoc.Buf.buf: %x\nDoc.Buf.content: %x\n", Doc->Buf.size, Doc->Buf.buf, Doc->Buf.content);
  fprintf(stderr, "1:%d 2:%d 3:%d 4:%d 5:%d 6:%d 7:%d 8:%d 9:%d 10:%d\n",
	  Doc->Buf.buf[1], Doc->Buf.buf[2], Doc->Buf.buf[3], Doc->Buf.buf[4], Doc->Buf.buf[5], 
	  Doc->Buf.buf[6], Doc->Buf.buf[7], Doc->Buf.buf[8], Doc->Buf.buf[9], Doc->Buf.buf[10]
	  );
*/
 return DPS_OK;

#else
  return -3;
#endif
}

/** Delete document from stores database
 */

__C_LINK int __DPSCALL DpsStoreDeleteDoc(DPS_AGENT *Agent, DPS_DOCUMENT *Doc) {

#ifdef HAVE_ZLIB
  const char *hello = "D\0";
  int s;
  urlid_t rec_id =  DpsURL_ID(Doc, NULL);
  size_t dbnum = ((size_t)rec_id) % ((Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems);

  if ( (Agent->Demons.nitems == 0) || ((s = Agent->Demons.Demon[dbnum].stored_sd) <= 0)) {
    return (Agent->Flags.do_store) ? DpsStoreDeleteRec(Agent, 0, rec_id, "") : DPS_OK;
  }
  
  DpsSend(s, hello, 1, 0);
  DpsSend(s, &rec_id, sizeof(rec_id), 0);
  
  return 0;
  

#else
  return -1;
#endif

}

/** Check document presence in stored database
 */

__C_LINK int __DPSCALL DpsStoreCheckUp(DPS_AGENT *Agent, int level) {

#ifdef HAVE_ZLIB
  const char *helloC = "C\0";
  const char *helloO = "O\0";
  int s, f = 1;
  size_t i, dbfrom = 0, dbto =  (Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems;

  for (i = dbfrom; i < dbto; i++) {
    if ((Agent->Demons.nitems == 0) || ((s = Agent->Demons.Demon[i].stored_sd) <= 0)) {
      if ((level == 1) && (Agent->Flags.do_store)) {
	DPS_BASE_PARAM P;
	DPS_DB *db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[i] : &Agent->dbl.db[i];
	bzero(&P, sizeof(P));
	P.subdir = "store";
	P.basename = "doc";
	P.indname = "doc";
	P.mode = DPS_WRITE_LOCK;
	P.NFiles = (db->StoredFiles) ? db->StoredFiles : DpsVarListFindInt(&Agent->Vars, "StoredFiles", 0x100);
	P.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
	P.A = Agent;
	DpsBaseOptimize(&P, -1);
	DpsBaseClose(&P);
      }
      if (f && level > 1 && (Agent->Flags.do_store)) DpsStoredCheck(Agent, 0, 0, "");
      f = 0;
    } else {
      if (level == 1) DpsSend(s, helloO, 1, 0);
      else DpsSend(s, helloC, 1, 0);
    }
  }
  return 0;

#else
  return -1;
#endif

}

/* ** ** ** */

static void DpsNextCharB_stored(void *d) {
  DPS_HTMLTOK *t = (DPS_HTMLTOK *)d;
  if (!t->finished && ((t->b - t->Content) > t->chunks * DPS_DOCHUNKSIZE - 32)) {
    char *OldContent = t->Content;
    size_t ChunkSize, i;
    t->Content = (char*)DpsRealloc(t->Content, (size_t)(t->chunks + 1) * DPS_DOCHUNKSIZE + 1);
    if (t->Content == NULL) return;
    t->chunks++;
    DpsSend(t->socket_sd, &t->chunks, sizeof(t->chunks), 0);
    DpsRecvall(t->socket_rv, &ChunkSize, sizeof(ChunkSize), 360);
    DpsRecvall(t->socket_rv, &t->Content[(t->chunks-1) * DPS_DOCHUNKSIZE], ChunkSize, 360);
    if (ChunkSize != DPS_DOCHUNKSIZE) {
      int z = 0;
      DpsSend(t->socket_sd, &z, sizeof(z), 0);
      t->finished = 1;
    }
    t->Content[(t->chunks-1) * DPS_DOCHUNKSIZE + ChunkSize] = '\0';
    if (t->Content != OldContent) {
      t->e = t->Content + (t->e - OldContent);
      t->b = t->Content + (t->b - OldContent);
      t->s = t->Content + (t->s - OldContent);
      *(t->lt) = t->Content + (*(t->lt) - OldContent);
      for (i = 0; i < t->ntoks; i++) {
     t->toks[i].name = (t->toks[i].name) ? t->Content + (t->toks[i].name - OldContent) : NULL;
     t->toks[i].val = (t->toks[i].val) ? t->Content + (t->toks[i].val - OldContent) : NULL;
      }
    }
  }
  (t->b)++;
}

static void DpsNextCharE_stored(void *d) {
  DPS_HTMLTOK *t = (DPS_HTMLTOK *)d;
  if (!t->finished && ((t->e - t->Content) > t->chunks * DPS_DOCHUNKSIZE - 32)) {
    char *OldContent = t->Content;
    size_t ChunkSize, i;
    t->Content = (char*)DpsRealloc(t->Content, (size_t)(t->chunks + 1) * DPS_DOCHUNKSIZE + 1);
    if (t->Content == NULL) return;
    t->chunks++;
    DpsSend(t->socket_sd, &t->chunks, sizeof(t->chunks), 0);
    DpsRecvall(t->socket_rv, &ChunkSize, sizeof(ChunkSize), 360);
    DpsRecvall(t->socket_rv, &t->Content[(t->chunks-1) * DPS_DOCHUNKSIZE], ChunkSize, 360);
    if (ChunkSize != DPS_DOCHUNKSIZE) {
      int z = 0;
      DpsSend(t->socket_sd, &z, sizeof(z), 0);
      t->finished = 1;
    }
    t->Content[(t->chunks-1) * DPS_DOCHUNKSIZE + ChunkSize] = '\0';
    if (t->Content != OldContent) {
      t->e = t->Content + (t->e - OldContent);
      t->b = t->Content + (t->b - OldContent);
      t->s = t->Content + (t->s - OldContent);
      *(t->lt) = t->Content + (*(t->lt) - OldContent);
      for (i = 0; i < t->ntoks; i++) {
     t->toks[i].name = (t->toks[i].name) ? t->Content + (t->toks[i].name - OldContent) : NULL;
     t->toks[i].val = (t->toks[i].val) ? t->Content + (t->toks[i].val - OldContent) : NULL;
      }
    }
  }
  (t->e)++;
}

/*
0009 0000 0000 0000 1001
000A 0000 0000 0000 1010
000D 0000 0000 0000 1101
0020 0000 0000 0010 0000
00A0 0000 0000 1010 0000
1680 0001 0110 1000 0000
2000 0010 0000 0000 0000
2001 0010 0000 0000 0001
2002 0010 0000 0000 0010
2003 0010 0000 0000 0011
2004 0010 0000 0000 0100
2005 0010 0000 0000 0101
2006 0010 0000 0000 0110
2007 0010 0000 0000 0111
2008 0010 0000 0000 1000
2009 0010 0000 0000 1001
200A 0010 0000 0000 1010
200B 0010 0000 0000 1011
202F 0010 0000 0010 1111
2420 0010 0100 0010 0000
3000 0011 0000 0000 0000

303F 0011 0000 0011 1111
---- -------------------
CB50 11?? 1?11 ?1?1 ???? - not space bits

*/


static int DpsUniNSpace(dpsunicode_t c) {
     if (c == 0x303F) return 0;
     if (c == 0xFEFF) return 0;
     if (c  & 0xCB50) return 1;
     if (c == 0x0009) return 0;
     if (c == 0x000A) return 0;
     if (c == 0x000D) return 0;
     if (c == 0x0020) return 0;
/*     if (c == 0x0026) return 0;
     if (c == 0x002C) return 0;*/
     if (c == 0x00A0) return 0;
     if (c == 0x1680) return 0;
     if ((c >= 0x2000) && (c <= 0x200B)) return 0;
     if (c == 0x202F) return 0;
     if (c == 0x2420) return 0;
     if (c == 0x3000) return 0;
     return 1;
}


static dpsunicode_t * DpsUniStrWWL(dpsunicode_t **p, DPS_WIDEWORDLIST *wwl, dpsunicode_t *c, size_t *len, size_t minwlen, int NOprefixHL) {
  register dpsunicode_t sc;
  register size_t i;
  register dpsunicode_t *s = *p;
/*  DPS_CHARSET *k = DpsGetCharSet("koi8-r"), *int_sys = DpsGetCharSet("sys-int");
  DPS_CONV uni_lc;
  char str[100000];

  DpsConvInit(&uni_lc, int_sys, k, DPS_RECODE_HTML);
  DpsConv(&uni_lc, str, sizeof(str), s, sizeof(int) * (DpsUniLen(s) + 1));

  fprintf(stdout, "WWL: %s\n", str);*/

  if (wwl->nwords == 0) return NULL;

  while((*s != 0) && (DpsUniCType(*s) > DPS_UNI_BUKVA)) s++;

  if (NOprefixHL) {
  
    while ((sc = DpsUniToLower(*s)) != 0) {
      s++;
      for(i = 0; i < wwl->nwords; i++) {
	if (sc != c[i]) continue;
	if (wwl->Word[i].origin & DPS_WORD_ORIGIN_STOP) continue;
	if ((DpsUniCType(s[len[i]]) <= DPS_UNI_BUKVA) && /*(s[len[i]] != 0) &&*/ (s[len[i]] >= 0x30 ) && DpsUniNSpace(s[len[i]])) continue;
	if ((len[i] > 0) && (DpsUniStrNCaseCmp(s, &(wwl->Word[i].uword[1]), len[i]) != 0)) continue;
	s--;
	return s;
      }
/*      for (i = 0; (i < minwlen) && (*s != 0); i++) s++;*/
      while(/*(*s != 0) &&*/ (DpsUniCType(*s) <= DPS_UNI_BUKVA)) s++;
      while((*s != 0) && (DpsUniCType(*s) > DPS_UNI_BUKVA)) s++;
    }

  } else {

    while ((sc = DpsUniToLower(*s)) != 0) {
      s++;
      for(i = 0; i < wwl->nwords; i++) {
	if (sc != c[i]) continue;
	if (wwl->Word[i].origin & DPS_WORD_ORIGIN_STOP) continue;
	if ((len[i] > 0) && (DpsUniStrNCaseCmp(s, &(wwl->Word[i].uword[1]), len[i]) != 0)) continue;
	s--;
	return s;
      }
    }
  }

  if (*p + minwlen < s) *p = s - minwlen;
  return NULL;
}



/** Make document excerpts on query words forms 
 */

__C_LINK char * __DPSCALL DpsExcerptDoc(DPS_AGENT *query, DPS_RESULT *Res, DPS_DOCUMENT *Doc, size_t size, size_t padding) {
  char *HDoc,*HEnd;
  const char *htok, *last;
  const char *lcharset;
  const char *doclang;
  DPS_CHARSET *bcs = NULL, *dcs = NULL, *sys_int;
  DPS_HTMLTOK tag;
  DPS_VAR ST;
  dpsunicode_t *start, *end, *prevend, *uni, ures, *p, *oi, dot[] = {0x20, 0x2e, 0x2e, 0x2e, 0}, *np, add;
  dpsunicode_t *c;
  char *os;
  int s = -1, r = -1;
  size_t *wlen, i, len, maxwlen = 0, minwlen = query->WordParam.max_word_len, ulen, prevlen, osl, index_limit;
  DPS_CONV dc_uni, uni_bc;
  const char *hello = "E\0";
  dpshash32_t rec_id;
  size_t ChunkSize, DocSize, dbnum;
  char *Source = NULL, *SourceToFree = NULL;
  int needFreeSource = 1;
  int NOprefixHL = 0;

  if (Res->WWList.nwords == 0) return NULL;
  bzero(&ST, sizeof(ST));
  ST.section = 255;
  ST.name = "ST";
  ST.val = "1";

  lcharset = DpsVarListFindStr(&query->Vars, "LocalCharset", "iso-8859-1");
  doclang = DpsVarListFindStr(&Doc->Sections, "Content-Language", "xx");
  if (strncasecmp(doclang, "zh", 2) && strncasecmp(doclang, "th", 2) 
      && strncasecmp(doclang, "ja", 2) && strncasecmp(doclang, "ko", 2) ) NOprefixHL = 1;

  bcs = DpsGetCharSet(lcharset);
  dcs = DpsGetCharSet(DpsVarListFindStr(&Doc->Sections,"Charset","iso-8859-1"));
  
  if (!bcs || !dcs) return NULL;
  if (!(sys_int=DpsGetCharSet("sys-int")))
    return NULL;
  
  DpsConvInit(&uni_bc, sys_int, bcs, query->Conf->CharsToEscape, DPS_RECODE_HTML);

  c = (dpsunicode_t *) DpsMalloc(Res->WWList.nwords * sizeof(dpsunicode_t) + 1);
  if (c == NULL) {  return NULL; }
  wlen = (size_t *) DpsMalloc(Res->WWList.nwords * sizeof(size_t) + 1);
  if (wlen == NULL) {
    DPS_FREE(c);
    return NULL;
  }
  for (i = 0; i < Res->WWList.nwords; i++) {
    wlen[i] = Res->WWList.Word[i].ulen - 1;
    c[i] = DpsUniToLower(Res->WWList.Word[i].uword[0]);
    if (wlen[i] > maxwlen) maxwlen = wlen[i];
    if (wlen[i] < minwlen) minwlen = wlen[i];
  }

  if ((oi = (dpsunicode_t *)DpsMalloc(2 * (dps_max(size + 2 * query->WordParam.max_word_len, Res->WWList.maxulen + 4 * (query->WordParam.max_word_len + padding) + 8) + 1) * sizeof(dpsunicode_t))) == NULL) {
    DPS_FREE(c); DPS_FREE(wlen);
    return NULL;
  }
  oi[0]=0;

  DocSize = DpsVarListFindInt(&Doc->Sections, "Content-Length", DPS_MAXDOCSIZE) + 2 * DPS_DOCHUNKSIZE;

  if ((DocSize == 0) ||  ((HEnd = HDoc = (char *)DpsMalloc(2 * DocSize + 4)) == NULL) ) {
    DPS_FREE(oi); DPS_FREE(c); DPS_FREE(wlen);
    return NULL;
  }
  HDoc[0]='\0';

  if ( (uni = (dpsunicode_t *)DpsMalloc((DocSize + 10) * sizeof(dpsunicode_t)) ) == NULL) {
    DPS_FREE(oi); DPS_FREE(c); DPS_FREE(wlen); DPS_FREE(HDoc);
    return NULL;
  }

  DpsHTMLTOKInit(&tag); 

  index_limit = (size_t)DpsVarListFindInt(&query->Vars, "IndexDocSizeLimit", 0);
  rec_id = DpsURL_ID(Doc, NULL);
/*  dbnum = ((size_t)rec_id) % ((query->flags & DPS_FLAG_UNOCON) ? query->Conf->dbl.nitems : query->dbl.nitems);*/
  if (query->flags & DPS_FLAG_UNOCON) {
    if (query->Conf->dbl.cnt_db) {
      dbnum = query->Conf->dbl.dbfrom + ((size_t)rec_id) % query->Conf->dbl.cnt_db;
    } else {
      dbnum = ((size_t)rec_id) % query->Conf->dbl.nitems;
    }
  } else {
    if (query->dbl.cnt_db) {
      dbnum = query->dbl.dbfrom + ((size_t)rec_id) % query->dbl.cnt_db;
    } else {
      dbnum = ((size_t)rec_id) % query->dbl.nitems;
    }
  }

  if ((tag.socket_sd = s = query->Demons.Demon[dbnum].stored_sd) <= 0)  {
#ifdef HAVE_ZLIB
    if ((query->Flags.do_store == 0) || (GetStore(query, Doc, rec_id, dbnum, "") != DPS_OK) || Doc->Buf.buf == NULL)
#endif
      {
/*    register int not_have_doc = (query->Flags.do_store == 0);
    if (not_have_doc) not_have_doc = (GetStore(query, Doc, rec_id, "") != DPS_OK);
    if (!not_have_doc) if (Doc->Buf.size == 0) not_have_doc = 1;
    if (not_have_doc) {*/
	DpsConvInit(&dc_uni, bcs, sys_int, query->Conf->CharsToEscape, DPS_RECODE_HTML);
	Source = SourceToFree = (char*)DpsStrdup(DpsVarListFindStr(&Doc->Sections, "body", ""));
	DpsVarListReplaceStr(&Doc->Sections, "Z", "");
      } 
#ifdef HAVE_ZLIB
    else {
      DpsConvInit(&dc_uni, dcs, sys_int, query->Conf->CharsToEscape, DPS_RECODE_HTML);
/*      DpsVarListReplaceStr(&Doc->Sections, "ST", "1");*/
      DpsVarListReplace(&Doc->Sections, &ST);
      if (strncmp(DPS_NULL2EMPTY(Doc->Buf.buf), "HTTP/", 5)) {
/*	Doc->Buf.content = Doc->Buf.buf;*/ /* for compatibility with data of old version */
	Source = Doc->Buf.buf;
      } else {
	DpsParseHTTPResponse(query, Doc);
	Source = Doc->Buf.content;
      }
      SourceToFree = Doc->Buf.buf;
      Doc->Buf.buf = Doc->Buf.content = NULL;
    }
#endif
  } else {
    DpsConvInit(&dc_uni, dcs, sys_int, query->Conf->CharsToEscape, DPS_RECODE_HTML);
    tag.next_b = &DpsNextCharB_stored;
    tag.next_e = &DpsNextCharE_stored;
    tag.chunks = 1;
    tag.socket_rv = r = query->Demons.Demon[dbnum].stored_rv;

    DpsSend(s, hello, 1, 0);
    DpsSend(s, &rec_id, sizeof(rec_id), 0);
    DpsRecvall(r, &ChunkSize, sizeof(ChunkSize), 360);

    if (ChunkSize == 0) {
      DPS_FREE(oi); DPS_FREE(c); DPS_FREE(wlen); DPS_FREE(HDoc); DPS_FREE(uni);
      return NULL;
    }
    DpsSend(s, &tag.chunks, sizeof(tag.chunks), 0);
    DpsRecvall(r, &ChunkSize, sizeof(ChunkSize), 360);
    if (ChunkSize == 0) {
      DPS_FREE(oi); DPS_FREE(c); DPS_FREE(wlen); DPS_FREE(HDoc); DPS_FREE(uni);
      return NULL;
    }

    if ((tag.Content = (char*)DpsMalloc(ChunkSize+10)) == NULL) {
      DPS_FREE(oi); DPS_FREE(c); DPS_FREE(wlen); DPS_FREE(HDoc); DPS_FREE(uni);
      return NULL;
    }
    DpsRecvall(r, tag.Content, ChunkSize, 360);
    tag.Content[ChunkSize] = '\0';
    if (strncmp(DPS_NULL2EMPTY(tag.Content), "HTTP/", 5)) {
      Source = SourceToFree = tag.Content;
    } else {
      Doc->Buf.buf = SourceToFree = tag.Content;
      Doc->Buf.size = ChunkSize;
      DpsParseHTTPResponse(query, Doc);
      Source = Doc->Buf.content;
    }
    Doc->Buf.buf = Doc->Buf.content = NULL;

/*    DpsVarListReplaceStr(&Doc->Sections, "ST", "1");*/
    DpsVarListReplace(&Doc->Sections, &ST);
    needFreeSource = 0;
  }


  htok = DpsHTMLToken(Source, &last, &tag);
  for (len = 0; (len < (size_t)(8 * maxwlen + 16 * padding + 1)) && htok; ) {
    switch(tag.type) {
    case DPS_HTML_TXT:
      if (tag.script == 0 && tag.comment == 0 && tag.style == 0 && tag.select == 0) {
	dps_memmove(HEnd, htok, (size_t)(last - htok));
	HEnd += (size_t)(last - htok);
	HEnd[0] = ' ';
	HEnd++;
	HEnd[0] = '\0';
	len = HEnd - HDoc;
      }
      break;
    case DPS_HTML_COM:
    case DPS_HTML_TAG:
    default:
      break;
    }
    htok = DpsHTMLToken(NULL, &last, &tag);
  }

  if (HEnd == HDoc) {
    DPS_FREE(oi); DPS_FREE(c); DPS_FREE(wlen); DPS_FREE(HDoc); DPS_FREE(uni);
    if (!tag.finished) {
      tag.chunks = 0;
      if (s >= 0) DpsSend(s, &tag.chunks, sizeof(tag.chunks), 0);
    }
    DPS_FREE(SourceToFree);
    return NULL;
  }

  add = DpsConv(&dc_uni, (char*)uni, sizeof(*uni)*(DocSize+10), HDoc, len + 1) / sizeof(*uni);
  prevlen = len;
  ulen = DpsUniLen(uni);
  if ((index_limit != 0) && (ulen > index_limit)) {
    ulen = index_limit;
    uni[ulen] = 0;
  }


  for (p = prevend = uni; DpsUniLen(oi) < size; ) {


    while(((np  = DpsUniStrWWL(&p, &(Res->WWList), c, wlen, minwlen, NOprefixHL)) == NULL) 
	  && (htok != NULL) && ((index_limit == 0) || (ulen < index_limit) )) {

      while(htok && ((len - prevlen) < (size_t)(8 * maxwlen + 16 * padding + 1)) ) {
	switch(tag.type) {
	case DPS_HTML_TXT:
	  if (tag.script == 0 && tag.comment == 0 && tag.style == 0 && tag.select == 0) {
	    dps_memmove(HEnd, htok, (size_t)(last-htok));
	    HEnd += (size_t)(last - htok);
	    HEnd[0] = ' ';
	    HEnd++;
	    HEnd[0] = '\0';
	    len = HEnd - HDoc;
	  }
	  break;
	case DPS_HTML_COM:
	case DPS_HTML_TAG:
	default:
	  break;
	}
	htok = DpsHTMLToken(NULL, &last, &tag);
      }

      add = DpsConv(&dc_uni, (char*)(uni + ulen), sizeof(*uni)*(DocSize + 10 - ulen), HDoc + prevlen, len - prevlen + 2) / sizeof(*uni);
      prevlen = len;
      ulen += DpsUniLen(uni+ulen);
    }
    if (np == NULL) break;

    p = np;
    start = dps_max(dps_max(p - padding, uni), prevend);
    end = dps_min(p + maxwlen + 1 + padding, uni + ulen);
    for (i = 0; (i < 2 * query->WordParam.max_word_len) && (start > uni) && DpsUniNSpace(*start); i++) start--;
    for (i = 0; (i < 2 * query->WordParam.max_word_len) && (end < uni + ulen) && DpsUniNSpace(*end); i++) end++;
    if ((start != uni) && (start != prevend)) DpsUniStrCat(oi, dot);
    ures = *end; *end = 0; DpsUniStrCat(oi, start); *end = ures;
    if ((end != uni + ulen)/* && (start != prevend)*/) DpsUniStrCat(oi, dot);
    p = prevend = end;
    if (end == np) p++;
/*    if (*p) p++;*/
  }

  {
    register dpsunicode_t *cc;
    for(cc = oi; *cc; cc++) {
      switch(*cc) {
      case 9:
      case 10:
      case 13:
      case 160:
	*cc = 32;
      default:
	break;
      }
    }
  }

  osl = (DpsUniLen(oi) + 1) * sizeof(char);
  if ((os = (char *)DpsMalloc(osl * 16)) == NULL) {
    DPS_FREE(oi); DPS_FREE(c); DPS_FREE(wlen); DPS_FREE(HDoc); DPS_FREE(uni);
    DPS_FREE(SourceToFree);
    return NULL;
  }

  
  DpsConv(&uni_bc, os, osl * 16, (char*)oi, sizeof(*oi) * osl);
  
  if (!tag.finished) {
    tag.chunks = 0;
    if (s >= 0) DpsSend(s, &tag.chunks, sizeof(tag.chunks), 0);
  }
  DPS_FREE(c); DPS_FREE(wlen); DPS_FREE(oi); DPS_FREE(HDoc); DPS_FREE(uni);
  if (needFreeSource) { DPS_FREE(SourceToFree); } else { DPS_FREE(tag.Content); }
  return os;
}




#define ABORT(x)    DPS_FREE(Doc); \
                    DPS_FREE(CDoc); \
                    DpsBaseClose(&P); \
              return (x);

int DpsStoreDelete(DPS_AGENT *Agent, int ns, int sd, char *Client) {
  urlid_t rec_id;

  if (DpsRecvall(ns, &rec_id, sizeof(rec_id), 360) < 0) {
    return DPS_ERROR;
  }
#ifdef HAVE_ZLIB
  return DpsStoreDeleteRec(Agent, sd, rec_id, Client);
#else
  return DPS_ERROR;
#endif
}

int DpsStoredOptimize(DPS_AGENT *Agent, int ns, char *Client) {
  DPS_ENV *Conf = Agent->Conf;
  unsigned int NFiles = DpsVarListFindInt(&Agent->Vars, "StoredFiles", 0x100);
  DPS_BASE_PARAM P;
  size_t i, dbfrom = 0, dbto =  (Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems;
  DPS_DB *db;

  for (i = dbfrom; i < dbto; i++) {
    db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[i] : &Agent->dbl.db[i];

    bzero(&P, sizeof(P));
    P.subdir = "store";
    P.basename = "doc";
    P.indname = "doc";
    P.mode = DPS_WRITE_LOCK;
    P.NFiles = (db->StoredFiles) ? db->StoredFiles : NFiles;
    P.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
    P.A = Agent;
    DpsBaseOptimize(&P, -1);
    DpsBaseClose(&P);
  }
  return DPS_OK;
}

int DpsStoredCheck(DPS_AGENT *Agent, int ns, int sd, char *Client) {

#if defined HAVE_SQL && defined HAVE_ZLIB
  DPS_ENV *Conf = Agent->Conf;
  size_t DocSize = 0;
  DPS_BASE_PARAM P;
  unsigned int i, NFiles = DpsVarListFindInt(&Agent->Vars, "StoredFiles", 0x100);
  urlid_t *todel = (int*)DpsMalloc(128 * sizeof(urlid_t));
  size_t ndel = 0, mdel = 128, totaldel = 0;
  char req[256];
  DPS_SQLRES   SQLRes;
  int res, notfound, recs, u = 1;
  size_t z, dbfrom = 0, dbto =  (Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems;
  DPS_DB  *db;
  unsigned long offset = 0;
  size_t nitems;
	const char      *url;
	char            *dc_url;
	size_t          len;
	int             prev_id = -1, charset_id;
	DPS_CHARSET	*doccs;
	DPS_CHARSET	*loccs;
	DPS_CONV        lc_dc;

  if (todel == NULL) return DPS_ERROR;

  DpsSQLResInit(&SQLRes);

  if (NFiles > DPS_STORE_BITS) NFiles = DPS_STORE_BITS + 1;

  recs = DpsVarListFindInt(&Agent->Conf->Vars, "URLDumpCacheSize", DPS_URL_DUMP_CACHE_SIZE);

  loccs = Agent->Conf->lcs;
  if(!loccs) loccs = DpsGetCharSet("iso-8859-1");

  DpsLog(Agent, DPS_LOG_EXTRA, "update storedchk table(s)");

  for (z = dbfrom; z < dbto; z++) {
    db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[z] : &Agent->dbl.db[z];
    
    if(DPS_OK != (res = DpsSQLAsyncQuery(db, NULL, "DELETE FROM storedchk")))
      return res;

    ndel = 0, mdel = 128, totaldel = 0;

    while (u) {
      dps_snprintf(req, sizeof(req), "SELECT rec_id,url,charset_id FROM url ORDER BY rec_id LIMIT %d OFFSET %ld", recs, offset);
      if(DPS_OK != (res = DpsSQLQuery(db, &SQLRes, req)))
	return res;
      nitems = DpsSQLNumRows(&SQLRes);
      for( i = 0; i < nitems; i++) {
	
	charset_id = DPS_ATOI(DpsSQLValue(&SQLRes, i, 2));

	if (charset_id != prev_id) {
	  doccs = DpsGetCharSetByID(prev_id = charset_id);
	  if(!doccs) doccs = DpsGetCharSet("iso-8859-1");
	  DpsConvInit(&lc_dc, loccs, doccs, Agent->Conf->CharsToEscape, DPS_RECODE_URL);
	}
	len = dps_strlen(url = DpsSQLValue(&SQLRes, i, 1));
	dc_url = (char*)DpsMalloc((size_t)(24 * len + 1));
	if (dc_url == NULL) continue;
	/* Convert URL from LocalCharset */
	DpsConv(&lc_dc, dc_url, (size_t)24 * len,  url, (size_t)(len + 1));

	dps_snprintf(req, sizeof(req), "INSERT INTO storedchk (rec_id, url_id) VALUES (%s, %d)", 
		     DpsSQLValue(&SQLRes, i, 0), DpsStrHash32(dc_url) );
	DPS_FREE(dc_url);

	if(DPS_OK != (res = DpsSQLAsyncQuery(db, NULL, req))) {
	  DpsSQLFree(&SQLRes);
	  return res;
	}
      }
      DpsSQLFree(&SQLRes);
      offset += nitems;
      u = (nitems == (size_t)recs);
      dps_setproctitle("[%d] storedchk: %ld records processed", Agent->handle, offset);
      DpsLog(Agent, DPS_LOG_EXTRA, "%ld records for storedchk written", offset);
      if (u) DPSSLEEP(0);
    }
  
    bzero(&P, sizeof(P));
    P.subdir = "store";
    P.basename = "doc";
    P.indname = "doc";
    P.mode = DPS_WRITE_LOCK;
    P.NFiles = (db->StoredFiles) ? db->StoredFiles : NFiles;
    P.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
    P.A = Agent;

  for (i = 0; i < P.NFiles; i++) {
    P.rec_id = i << DPS_BASE_BITS;
    if (DpsBaseOpen(&P, DPS_WRITE_LOCK) != DPS_OK) {
      if (sd > 0) DpsSend(sd, &DocSize, sizeof(DocSize), 0); 
      DpsBaseClose(&P);
      return DPS_ERROR;
    }
    if (lseek(P.Ifd, (off_t)0, SEEK_SET) == (off_t)-1) {
      DpsLog(Agent, DPS_LOG_ERROR, "Can't seeek for file %s", P.Ifilename);
      DpsBaseClose(&P);
      return DPS_ERROR;
    }
    while (read(P.Ifd, &P.Item, sizeof(DPS_BASEITEM)) == sizeof(DPS_BASEITEM)) {
      if (P.Item.rec_id != 0) {

	notfound = 1;
	for (z = dbfrom; notfound && (z < dbto); z++) {
	  db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[z] : &Agent->dbl.db[z];

	  dps_snprintf(req, sizeof(req), "SELECT rec_id FROM storedchk WHERE url_id=%d", P.Item.rec_id);
	  if(DPS_OK != (res = DpsSQLQuery(db, &SQLRes, req))) {
	    DpsBaseClose(&P);
	    return res;
	  }
	  if (DpsSQLNumRows(&SQLRes) > 0) {
	    notfound = 0;
	  }
	  DpsSQLFree(&SQLRes);
     
	}
	if (notfound) {
	  if (ndel >= mdel) {
	    mdel += 128;
	    todel = (urlid_t*)DpsRealloc(todel, mdel * sizeof(urlid_t));
	    if (todel == NULL) {
	      DpsBaseClose(&P);
	      return DPS_ERROR;
	    }
	  }
	  todel[ndel++] = P.Item.rec_id;
	}
      }
    }
    DpsBaseClose(&P);
    for (z = 0; z < ndel; z++) {
        DpsLog(Agent, DPS_LOG_DEBUG, "Store %03X: deleting url_id: %X", i, todel[z]);
     if ((res = DpsStoreDeleteRec(Agent, -1, todel[z], "Stored Check-up")) != DPS_OK) {
       return res;
     }
    }
    dps_setproctitle("Store %03X, %d lost records deleted", i, ndel);
    DpsLog(Agent, DPS_LOG_EXTRA, "Store %03X, %d lost records deleted", i, ndel);
    totaldel += ndel;
    ndel = 0;
  }
  dps_setproctitle("Total lost record(s) deleted: %d\n", totaldel);
  DpsLog(Agent, DPS_LOG_EXTRA, "Total lost record(s) deleted: %d\n", totaldel);
/*  for (z = dbfrom; z < dbto; z++) {
    db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[z] : &Agent->dbl.db[z];
*/
    if(DPS_OK != (res = DpsSQLAsyncQuery(db, NULL, "DELETE FROM storedchk")))
      return res;
/*  }*/
  DPS_FREE(todel);
  }
#endif
  return DPS_OK;
}

int DpsStoreFind(DPS_AGENT *Agent, int ns, int sd, char *Client) {
  urlid_t rec_id;
  size_t DocSize = 0, dbnum;
  DPS_BASE_PARAM P;
  int found = 0;
  DPS_DB *db;

  if (DpsRecvall(ns, &rec_id, sizeof(rec_id), 360) < 0) {
    return DPS_ERROR;
  }
  dbnum = ((size_t)rec_id) % ((Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems);
  db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[dbnum] : &Agent->dbl.db[dbnum];
  bzero(&P, sizeof(P));
  P.subdir = "store";
  P.basename = "doc";
  P.indname = "doc";
  P.mode = DPS_READ_LOCK;
  P.NFiles = (db->StoredFiles) ? db->StoredFiles : DpsVarListFindInt(&Agent->Vars, "StoredFiles", 0x100);
  P.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
  P.A = Agent;
  while (rec_id != 0) {
    P.rec_id = rec_id;
    if (DpsBaseSeek(&P, DPS_READ_LOCK) != DPS_OK) {
      DpsSend(sd, &DocSize, sizeof(DocSize), 0); 
      DpsBaseClose(&P);
      return DPS_ERROR;
    }
    if (P.Item.rec_id == rec_id) {
      found = 1;
      DpsLog(Agent, DPS_LOG_EXTRA, "[%s] Found rec_id: %x", Client, rec_id);
    } else {
      found = 0;
      DpsLog(Agent, DPS_LOG_EXTRA, "[%s] Not found rec_id: %x", Client, rec_id);
    }
    DpsSend(sd, &found, sizeof(found), 0);

    if (DpsRecvall(ns, &rec_id, sizeof(rec_id), 360) < 0) {
      DpsBaseClose(&P);
      return DPS_ERROR;
    }
  }
  DpsBaseClose(&P);
  return DPS_OK;
}

int DpsStoreGetByChunks(DPS_AGENT *Agent, int ns, int sd, char *Client) {

#ifdef HAVE_ZLIB
  urlid_t rec_id;
  size_t DocSize = 0, dbnum;
  Byte *Doc = NULL, *CDoc = NULL;
  z_stream zstream;
  DPS_BASE_PARAM P;
  DPS_DB *db;
  int chunk, i; size_t OldOut;

  if (DpsRecvall(ns, &rec_id, sizeof(rec_id), 360) < 0) {
    return DPS_ERROR;
  }
            
  DpsLog(Agent, DPS_LOG_EXTRA, "[%s] Retrieve by chunks: rec_id: %x", Client, rec_id);

  dbnum = ((size_t)rec_id) % ((Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems);
  db = (Agent->flags & DPS_FLAG_UNOCON) ? &Agent->Conf->dbl.db[dbnum] : &Agent->dbl.db[dbnum];
  bzero(&P, sizeof(P));
  P.subdir = "store";
  P.basename = "doc";
  P.indname = "doc";
  P.rec_id = rec_id;
  P.NFiles = (db->StoredFiles) ? db->StoredFiles : DpsVarListFindInt(&Agent->Vars, "StoredFiles", 0x100);
  P.vardir = (db->vardir) ? db->vardir : DpsVarListFindStr(&Agent->Vars, "VarDir", DPS_VAR_DIR);
  P.A = Agent;
  if (DpsBaseOpen(&P, DPS_READ_LOCK) != DPS_OK) {
    DpsLog(Agent, DPS_LOG_ERROR, "[%s] DpsBaseOpen error: rec_id: %x", Client, P.rec_id);
    DpsSend(sd, &DocSize, sizeof(DocSize), 0); 
    DpsBaseClose(&P);
    ABORT(DPS_ERROR);
  }

  if (P.Item.rec_id == rec_id) {
    if (lseek(P.Sfd, (off_t)P.Item.offset, SEEK_SET) == (off_t)-1) {
      DocSize = 0;
      DpsSend(sd, &DocSize, sizeof(DocSize), 0); 
      DpsLog(Agent, DPS_LOG_ERROR, "[%s] '%s' lseek [%x] error at %s:{%d}", Client, P.Sfilename, P.Item.offset, __FILE__, __LINE__);
      ABORT(DPS_ERROR);
    }
    zstream.avail_in = DocSize = P.Item.size;
    zstream.avail_out = 0;
    zstream.zalloc = Z_NULL;
    zstream.zfree = Z_NULL;
    zstream.opaque = Z_NULL;
    CDoc = zstream.next_in = (DocSize) ? (Byte *) DpsXmalloc(DocSize + 1) : NULL;
    Doc = zstream.next_out = (Byte *) DpsXmalloc(DPS_MAXDOCSIZE + 1);
    if (CDoc == NULL || Doc == NULL) {
      DocSize = 0;
      DpsSend(sd, &DocSize, sizeof(DocSize), 0);
      DpsLog(Agent, DPS_LOG_ERROR, "[%s] alloc error at %s {%d}", Client, __FILE__, __LINE__);
      ABORT(DPS_ERROR);
    }

    if ((read(P.Sfd, CDoc, DocSize) != (ssize_t)DocSize) || (inflateInit2(&zstream, 15) != Z_OK)) {
      DocSize = 0;
      DpsSend(sd, &DocSize, sizeof(DocSize), 0); 
      DpsLog(Agent, DPS_LOG_ERROR, "[%s] read or inflate error at %s:{%d}", Client, __FILE__, __LINE__);
      ABORT(DPS_ERROR);
    }

    OldOut = 0;
    DocSize = 1;
    DpsSend(sd, &DocSize, sizeof(DocSize), 0); 
    for(i = 1; 1; i++) {
      if (DpsRecvall(ns, &chunk, sizeof(chunk), 360) < 0) {
	DocSize = 0;
	DpsSend(sd, &DocSize, sizeof(DocSize), 0); 
	ABORT(DPS_ERROR);
      }
      if (chunk == 0) break;
      zstream.avail_out = DPS_DOCHUNKSIZE;
      inflate(&zstream, Z_SYNC_FLUSH);
                    
      DocSize = zstream.total_out - OldOut;
      DpsSend(sd, &DocSize, sizeof(DocSize), 0);
      DpsSend(sd, &Doc[OldOut], DocSize, 0);
      DpsLog(Agent, DPS_LOG_EXTRA, "[%s] rec_id: %x Chunk %i [%d bytes] sent", Client, rec_id, chunk, DocSize);
      OldOut = zstream.total_out;
      if (DocSize == 0) break;
    }
    DpsLog(Agent, DPS_LOG_EXTRA, "[%s] Retrieved by chunks rec_id: %x Size: %d Ratio: %5.2f%%", Client,
	   rec_id, zstream.total_out, (100.0 * zstream.total_in) / ((zstream.total_out > 0) ? zstream.total_out : 1));
    inflateEnd(&zstream);

  } else {
    DocSize = 0;
    DpsSend(sd, &DocSize, sizeof(DocSize), 0); 
    DpsLog(Agent, DPS_LOG_EXTRA, "[%s] Not found rec_id: %x", Client, rec_id);
    ABORT(DPS_OK);
  }
  ABORT(DPS_OK);

/**********************/
#else
  return DPS_ERROR;
#endif

}

int DpsStoreGet(DPS_AGENT *Agent, int ns, int sd, char *Client) {

#ifdef HAVE_ZLIB
  urlid_t rec_id;
  size_t DocSize, dbnum;
  int rc = DPS_OK;
  DPS_DOCUMENT *Doc = DpsDocInit(NULL);

  if (DpsRecvall(ns, &rec_id, sizeof(rec_id), 360) < 0) {
    return DPS_ERROR;
  }
  dbnum = ((size_t)rec_id) % ((Agent->flags & DPS_FLAG_UNOCON) ? Agent->Conf->dbl.nitems : Agent->dbl.nitems);
  rc = GetStore(Agent, Doc, rec_id, dbnum, Client);
  if (rc == DPS_OK) {
    DocSize = Doc->Buf.size;
    DpsSend(sd, &DocSize, sizeof(DocSize), 0);
    DpsSend(sd, Doc->Buf.buf, DocSize, 0);
  } else {
    DocSize = 0;
    DpsSend(sd, &DocSize, sizeof(DocSize), 0); 
  }

  DpsDocFree(Doc);
  return rc;
#else
  return DPS_ERROR;
#endif
}

int DpsStoreSave(DPS_AGENT *Agent, int ns, char *Client) {

#ifdef HAVE_ZLIB
  urlid_t rec_id;
  size_t DocSize;
  Byte *Doc = NULL;
  int rc = DPS_OK;

  if (DpsRecvall(ns, &rec_id, sizeof(rec_id), 3600) < 0) {
    return DPS_ERROR;
  }
	    DpsLog(Agent, DPS_LOG_DEBUG, "rec_id: %d [%x]", rec_id, rec_id);

            if (DpsRecvall(ns, &DocSize, sizeof(DocSize), 360) < 0) {
              return DPS_ERROR;
            }
	    DpsLog(Agent, DPS_LOG_DEBUG, "DocSize: %d", DocSize);

            Doc = (Byte *) DpsMalloc(DocSize + 1);
            if (Doc == NULL) {
              return DPS_ERROR;
            }
            if (DpsRecvall(ns, Doc, DocSize, 360) < 0) {
              return DPS_ERROR;
            }
	    DpsLog(Agent, DPS_LOG_DEBUG, "Document received");

	    if ((rc = DoStore(Agent, rec_id, Doc, DocSize, Client)) != DPS_OK) {
/*                DpsSend(ns, &DocSize, sizeof(DocSize), 0); ????? */
	    }

	    DPS_FREE(Doc);
	    return rc;
#else
	    return DPS_ERROR;
#endif

}



urlid_t DpsURL_ID(DPS_DOCUMENT *Doc, const char *url) {
  urlid_t url_id = DpsVarListFindInt(&Doc->Sections, "URL_ID", 0);
  const char     *accept_lang = DpsVarListFindStr(&Doc->Sections, "Content-Language", NULL);
  
  if (url_id != 0) return url_id;
  if (url == NULL) url = DpsVarListFindStr(&Doc->Sections, "URL", NULL);
  if (url != NULL) {
    char *str;
    size_t str_len = dps_strlen(url) + dps_strlen(DPS_NULL2EMPTY(accept_lang)) + 16;
    if ((str = (char*)DpsMalloc(str_len + 1)) == NULL) return 0;
    if (accept_lang != NULL && *accept_lang == '\0') accept_lang = NULL;
    if (accept_lang == NULL) accept_lang = DpsVarListFindStr(&Doc->RequestHeaders, "Accept-Language", NULL);
    dps_snprintf(str, str_len, "%s%s%s", (accept_lang == NULL) ? "" : accept_lang, (accept_lang == NULL) ? "" : ".", url);
    url_id = DpsStrHash32(str);
    DPS_FREE(str);
    DpsVarListAddInt(&Doc->Sections, "URL_ID", url_id);
  }
  return url_id;
}
