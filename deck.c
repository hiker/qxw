/*
Qxw is a program to help construct and publish crosswords.

Copyright 2011-2020 Mark Owen; Windows port by Peter Flippant
http://www.quinapalus.com
E-mail: qxw@quinapalus.com

This file is part of Qxw.

Qxw is free software: you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License
as published by the Free Software Foundation.

Qxw is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Qxw.  If not, see <http://www.gnu.org/licenses/> or
write to the Free Software Foundation, Inc., 51 Franklin Street,
Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdio.h>
#include <string.h>
#include "common.h"
#include "qxw.h"
#include "filler.h"
#include "alphabets.h"
#include "dicts.h"
#include "draw.h"
#include "treatment.h"

static FILE*dkfp=0;
static int line;
#define DBUFSZ 10000
char dbuf[DBUFSZ];
#define DELIMS " \t\r\n"
#define DELIMEOL "\r\n"

#define ENHTABSZ 4096
#define MAXENAMELEN 31

static int enhtab[ENHTABSZ];

// deck is assembled using these structs before copying across to struct entry etc. for filler
struct dkentry {
  char name[MAXENAMELEN+1];
  ABM flbm;
  int hashlink;
  }*dkentries=0;
int entsalloc=0;

struct dkword {
  int nent;
  struct lprop prop;
  int entries[MXFL];
  }*dkwords=0;
int wordsalloc=0;

static int dknw,dkne;
static int dspec;

static void batcherr(char*s) {
  if(line) fprintf(stderr,"Error at line %d: %s\n",line,s);
  else     fprintf(stderr,"Error: %s\n",s);
  }

static void batchwarn(char*s) {
  fprintf(stderr,"Warning: %s",s);
  if(line) fprintf(stderr," at line %d of deck\n",line);
  fprintf(stderr,"\n");
  }

// check whether a token is a given directive; if c1!=NULL also check against abbreviated form
static int cmdcmp(char*s,char*c0,char*c1) {
  if(s[0]!='.') return 1;
  if(!STRCASECMP(s+1,c0)) return 0;
  if(c1&&!STRCASECMP(s+1,c1)) return 0;
  return 2;
  }

// 0=not a valid entry name; 1=valid
static int isvalidename(char*t) {
  int i;
  if(!t) return 0;
  if(strlen(t)<1||strlen(t)>MAXENAMELEN) return 0;
  for(i=0;t[i];i++) {
    if(isalnum((unsigned char)t[i])) continue;
    if(t[i]=='$') continue;
    if(t[i]=='_') continue;
    return 0;
    }
  return 1;
  }

static void initenhtab() {int i; for(i=0;i<ENHTABSZ;i++) enhtab[i]=-1; }

// return entry index corresponding to name, creating new if necessary
// -1 on error
static int findent(char*t) {
  unsigned int h;
  int i,n;
  void*p;

  for(i=0,h=0;t[i];i++) h=h*113+t[i];
  h=h%ENHTABSZ;
  n=enhtab[h];
  while(n!=-1) {
    if(!strcmp(dkentries[n].name,t)) return n;
    n=dkentries[n].hashlink;
    }
  if(entsalloc<dkne+1) {
    entsalloc=entsalloc*2+1000;
    p=realloc(dkentries,sizeof(struct dkentry)*entsalloc);
    if(!p) {batcherr("Out of memory"); return -1;}
    dkentries=p;
    }
  strcpy(dkentries[dkne].name,t);
  dkentries[dkne].flbm=ABM_ALL; // should this be ABM_NRM?
  dkentries[dkne].hashlink=enhtab[h];
  enhtab[h]=dkne;
  return dkne++;
  }

static int newword() {
  void*p;
  if(wordsalloc<dknw+1) {
    wordsalloc=wordsalloc*2+1000;
    p=realloc(dkwords,sizeof(struct dkword)*wordsalloc);
    if(!p) {batcherr("Out of memory"); return -1;}
    dkwords=p;
    }
  memset(dkwords+dknw,0,sizeof(struct dkword));
  return dknw++;
  }

static int readblock(int depth,int dmask,int emask,int ten) {
  char*tok;
  int d,e,i,rc,u,w;
#define FIRSTTOK tok=strtok(dbuf,DELIMS)
#define NEXTTOK tok=strtok(0,DELIMS)
#define TOKEOL tok=strtok(0,DELIMEOL); if(tok) while(isspace((unsigned char)*tok)) tok++;
  for(;;) {
    line++;
    if(!fgets(dbuf,DBUFSZ,dkfp)) {
      if(depth) {batcherr("End of file encountered within block"); return 16;}
      return 0;
      }
    FIRSTTOK;

// miscellanea
    if(!tok) continue;            // blank line
    if(tok[0]=='#') continue;     // comment

// block syntax
    if(!strcmp(tok,"{")) {        // start of nested block
      rc=readblock(depth+1,dmask,emask,ten);
      if(rc) return rc;
      continue;
      }
    if(!strcmp(tok,"}")) {        // end of nested block
      if(!depth) {batcherr("End of block encountered at top level"); return 16;}
      return 0;
      }

// the following must occur at the start of a file (so that e.g. the alphabet is known for cell assignments)
#define ATSTART (dknw==0&&dkne==0)
#define CHECKSTART(s) \
  if(depth) {batcherr(s " encountered inside block"); return 16;} \
  if(!ATSTART) {batcherr(s " must occur at beginning of deck"); return 16;}

    if(!cmdcmp(tok,"ALPHABET","AL")) {
      CHECKSTART("Alphabet directive")
      NEXTTOK;
      if(!tok) {batcherr("Syntax error in alphabet directive"); return 16;}
      u=initalphamapbycode(tok);
      if(u) {batcherr("Alphabet not recognised"); return 16;}
      continue;
      }
    if(!cmdcmp(tok,"DICTIONARY","DI")) {
      CHECKSTART("Dictionary directive")
      NEXTTOK;
      if(!tok||strlen(tok)!=1||*tok<='0'||*tok>'9') {batcherr("Dictionary number must be 1..9"); return 16;}
      d=*tok-'1';
      TOKEOL;
      if(!tok) strcpy(dfnames[d],""); // allow empty dictionary filename
      else     strncpy(dfnames[d],tok,SLEN-1);
      dfnames[d][SLEN-1]=0;
      DEB_DE printf("dictionary %d: >%s<\n",d,dfnames[d]);
      dspec=1;
      continue;
      }
    if(!cmdcmp(tok,"FILEFILTER","FF")) {
      CHECKSTART("Dictionary file filter directive")
      NEXTTOK;
      if(!tok||strlen(tok)!=1||*tok<='0'||*tok>'9') {batcherr("Dictionary number must be 1..9"); return 16;}
      d=*tok-'1';
      TOKEOL;
      if(!tok) strcpy(dsfilters[d],""); // allow empty file filter
      else     strncpy(dsfilters[d],tok,SLEN-1);
      dsfilters[d][SLEN-1]=0;
      DEB_DE printf("dsfilter %d: >%s<\n",d,dsfilters[d]);
      dspec=1;
      continue;
      }
    if(!cmdcmp(tok,"ANSWERFILTER","AF")) {
      CHECKSTART("Dictionary answer filter directive")
      NEXTTOK;
      if(!tok||strlen(tok)!=1||*tok<='0'||*tok>'9') {batcherr("Dictionary number must be 1..9"); return 16;}
      d=*tok-'1';
      TOKEOL;
      if(!tok) strcpy(dafilters[d],"");
      else     strncpy(dafilters[d],tok,SLEN-1);
      dafilters[d][SLEN-1]=0;
      DEB_DE printf("dafilter %d: >%s<\n",d,dafilters[d]);
      dspec=1;
      continue;
      }
    if(!cmdcmp(tok,"RANDOM","RA")) {
      CHECKSTART("Random fill directive")
      NEXTTOK;
      if(!tok||strlen(tok)!=1||*tok<'0'||*tok>'2') {batcherr("Syntax error in random fill directive"); return 16;}
      afrandom=*tok-'0';
      continue;
      }
    if(!cmdcmp(tok,"UNIQUE","UN")) {
      CHECKSTART("Unique fill directive")
      NEXTTOK;
      if(!tok||strlen(tok)!=1||*tok<'0'||*tok>'1') {batcherr("Syntax error in unique fill directive"); return 16;}
      afunique=*tok-'0';
      continue;
      }
    if(!cmdcmp(tok,"TREATMENT","TR")) {
      CHECKSTART("Treatment directive")
      NEXTTOK;
      if(!tok||!isdigit((unsigned char)*tok)) {batcherr("Syntax error in treatment directive"); return 16;}
      u=atoi(tok);
      if(u<0||u>=NATREAT) {batcherr("Invalid treatment number"); return 16;}
      treatmode=u;
      continue;
      }
    if(!cmdcmp(tok,"MESSAGE","ME")) {
      CHECKSTART("Message directive")
      NEXTTOK;
      if(!tok||strlen(tok)!=1||*tok<'0'||*tok>='0'+NMSG) {batcherr("Syntax error in message directive"); return 16;}
      u=*tok-'0';
      TOKEOL;
      if(!tok) {batcherr("Syntax error in message directive"); return 16;}
      strncpy(treatmsg[u],tok,sizeof(treatmsg[0])-1);
      treatmsg[u][sizeof(treatmsg[0])-1]=0;
      continue;
      }
    if(!cmdcmp(tok,"MESSAGEALLOCATE","MA")) {
      CHECKSTART("Message allocate directive")
      NEXTTOK;
      if(!tok||strlen(tok)!=1||*tok<'0'||*tok>='0'+NMSG) {batcherr("Syntax error in message allocate directive"); return 16;}
      u=*tok-'0';
      NEXTTOK;
      if(!tok||strlen(tok)!=1||*tok<'0'||*tok>'2') {batcherr("Syntax error in message allocate directive"); return 16;}
      treatorder[u]=*tok-'0';
      continue;
      }
    if(!cmdcmp(tok,"MESSAGECONSTRAINTS","MC")) {
      CHECKSTART("Message constraints directive")
      NEXTTOK;
      if(!tok||strlen(tok)!=1||*tok<'0'||*tok>='0'+NMSG) {batcherr("Syntax error in message constraints directive"); return 16;}
      u=*tok-'0';
      TOKEOL;
      if(!tok) {batcherr("Syntax error in  message constraints directive"); return 16;}
      i=strtoabms(treatcstr[u],MXFL,tok,1);
      for(;i<MXFL;i++) treatcstr[u][i]=ABM_ALL;
      continue;
      }
    if(!cmdcmp(tok,"TREATEDANSWERMUSTBEAWORD","TW")) {
      CHECKSTART("Word constraint directive")
      NEXTTOK;
      if(!tok||strlen(tok)!=1||*tok<'0'||*tok>'1') {batcherr("Syntax error in word constraint directive"); return 16;}
      tambaw=*tok-'0';
      continue;
      }
    if(!cmdcmp(tok,"PLUGIN","PI")) {
      CHECKSTART("Plugin directive")
      TOKEOL;
      if(!tok) {batcherr("Syntax error in plugin directive"); return 16;}
      strncpy(tpifname,tok,SLEN-1);
      tpifname[SLEN-1]=0;
      continue;
      }

// the following are stackable using block syntax
    if(!cmdcmp(tok,"USEDICTIONARY","UD")) {
      NEXTTOK;
      if(!tok) {batcherr("Syntax error in dictionary list"); return 16;}
      for(dmask=0,i=0;tok[i];i++) {
        if(tok[i]<='0'||tok[i]>'9') {batcherr("Syntax error in dictionary list"); return 16;}
        dmask|=1<<(tok[i]-'1'); // internally count from 0
        }
      if(!dmask) batchwarn("Empty dictionary list");
      continue;
      }
    if(!cmdcmp(tok,"ENTRYMETHOD","EM")) {
      NEXTTOK;
      if(!tok) {batcherr("Syntax error in entry method specification"); return 16;}
      for(emask=0,i=0;tok[i];i++) {
        u=tok[i];
        if(isupper((unsigned char)u)) u=tolower((unsigned char)u);
        switch(u) {
      case 'f': case '>': emask|=EM_FWD; break;
      case 'r': case '<': emask|=EM_REV; break;
      case 'c': case ')': emask|=EM_CYC; break;
      case 'a': case '(': emask|=EM_RCY; break;
      case 'j': case '@': emask|=EM_JUM; break;
      default: batcherr("Syntax error in entry method specification"); return 16;
          }
        }
      if(!emask) batchwarn("No entry methods specified");
      continue;
      }
    if(!cmdcmp(tok,"TREATMENTENABLE" ,"TE")) { ten=1; continue; }
    if(!cmdcmp(tok,"TREATMENTDISABLE","TD")) { ten=0; continue; }

    if(*tok=='.') {batcherr("Unrecognised directive"); return 16;}

// word/entry creation
    if(isvalidename(tok)) {
      w=newword();
      if(w<0) return 16; // out of memory etc.
      dkwords[w].prop.dmask=dmask;
      dkwords[w].prop.emask=emask;
      dkwords[w].prop.ten=ten;
      for(;;) {
        DEB_DE printf("word entry tok=>%s<\n",tok);
        e=findent(tok);
        if(e<0) return 16; // out of memory etc.
        if(dkwords[w].nent>=MXLE) {batcherr("Too many entries in word"); return 16;}
        dkwords[w].entries[dkwords[w].nent]=e;
        dkwords[w].nent++;
 ew0:
        NEXTTOK;
        if(!tok) break;
        if(*tok=='=') {
          ABM a[MXLE];
          int l;
          char s[MAXENAMELEN+100];
          l=strtoabms(a,MXLE,tok+1,1);
          if(l>dkwords[w].nent) {batcherr("Constraint too long"); return 16;}
          for(i=0;i<l;i++) {
            dkentries[dkwords[w].entries[dkwords[w].nent-l+i]].flbm&=a[i];
            if(dkentries[dkwords[w].entries[dkwords[w].nent-l+i]].flbm==0) {
              sprintf(s,"No possible assignment to entry %s",dkentries[dkwords[w].entries[dkwords[w].nent-l+i]].name);
              batcherr(s);
              return 16;
              }
            }
          goto ew0;
          }
        if(!isvalidename(tok)) {batcherr("Invalid name for entry"); return 16;}
        }
      continue;
      }

    batcherr("Syntax error");
    return 16;
    }
  }

// cldict is flag indicating if any dictionaries were specified on the command line
int loaddeck(int cldict) {
  int e,rc=0,w;
  char*p;
  DEB_DE printf("loaddeck() filename=%s...\n",filename);
  line=0;
  dknw=0; dkne=0; dspec=0;
  initenhtab();
  dkfp=q_fopen(filename,"r");
  if(!dkfp) {batcherr("Failed to open input deck"); return 16;}
  rc=readblock(0,1,1,1);
  fclose(dkfp);
  if(treatmode==TREAT_PLUGIN) {
    if((p=loadtpi())) {
      line=0;
      batcherr("Failed to load custom plug-in");
      DEB_DE printf("%s\n",p);
      return 16;
      }
    }
  if(dspec==0&&!cldict) loaddefdicts();
  else if(loaddicts(0)) return 16;
  // compare the following with the code in bldstructs()
  initstructs(); // this sorts out all the treatmsg[] variables
  nw=dknw; ne=dkne;

  words=calloc(nw+NMSG,sizeof(struct word)); // one spare word for each message
  if(!words) {batcherr("Out of memory"); return 16;}
  entries=calloc(ne+NMSG*nw,sizeof(struct entry)); // nw spares for each message
  if(!entries) {batcherr("Out of memory"); return 16;}

  for(w=0;w<nw;w++) {
    words[w].nent=dkwords[w].nent;
    words[w].wlen=dkwords[w].nent;
    words[w].jlen=dkwords[w].nent;
    for(e=0;e<dkwords[w].nent;e++) {
      words[w].e[e]=entries+dkwords[w].entries[e];
      entries[dkwords[w].entries[e]].checking++;
      }
    words[w].wlen=dkwords[w].nent;
    words[w].lp=&dkwords[w].prop;
    if(words[w].lp->ten&&treatmode>0) ntw++; // count treated words
    }
  for(e=0;e<ne;e++) {
    entries[e].flbm=dkentries[e].flbm;
    }

  ne0=ne; nw0=nw;
  addimplicitwords();

  return rc;
  }

int dumpdeck() {
  int h,e,w,k;
  char t0[MXFL*10+100];

  DEB_DE {
    printf("dumpdeck(): dkne=%d dknw=%d\n",dkne,dknw);
    for(h=0;h<ENHTABSZ;h++) if(enhtab[h]>=0) printf("H%4d>%4d\n",h,enhtab[h]);
    for(e=0;e<dkne;e++) printf("E%3d %10s: %016llx H>%d\n",e,dkentries[e].name,dkentries[e].flbm,dkentries[e].hashlink);
    for(w=0;w<dknw;w++) {
      printf("W%2d dmask=%08x emask=%08x ten=%d:",w,
        dkwords[w].prop.dmask,dkwords[w].prop.emask,dkwords[w].prop.ten);
      for(e=0;e<dkwords[w].nent;e++) printf(" %3d",dkwords[w].entries[e]);
      printf("\n");
      }
//    for(;w<nw;w++) {
//      printf("W%2d:",w);
//      for(e=0;e<words[w].nent;e++) printf(" %3d",(int)(words[w].e[e]-entries));
//      printf("\n");
//      }

    }
  DEB_DE printf("filler_status=%d\n",filler_status);
  if(filler_status==1) {
    fprintf(stderr,"No fill found\n");
    return 4;
    }

  for(e=0;e<ne;e++) entries[e].flbm=entries[e].flbmh; // "accept all the hints"
  if(filler_start(3)) { // re-run filler to get feasible word lists
    fprintf(stderr,"Internal error A\n");
    return 16;
    }
  filler_wait();

  DEB_DE {
    for(e=0;e<ne;e++) printf("E%3d flbmh=%016llx\n",e,entries[e].flbmh);
    }

  for(w=0;w<nw0;w++) {
    printf("W%d ",w);
    for(e=0;e<words[w].jlen;e++) if(dkwords[w].entries[e]<ne0) pabm(words[w].e[e]->flbmh,1);
    if(words[w].flistlen) {
      printf("\n# ");
      for(k=0;k<words[w].flistlen;k++) {
        if(k>0) printf("; ");
        ansform(t0,sizeof(t0),words[w].flist[k],words[w].wlen,words[w].lp->dmask);
        printf("%s",t0);
        }
      }
    printf("\n");
    }
  for(;w<nw;w++) {
    printf("M%d ",w-nw0);
    for(e=0;e<words[w].nent;e++) pabm(words[w].e[e]->flbmh,1);
    printf("\n");
    }

  return 0;
  }
