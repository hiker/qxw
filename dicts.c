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


#include <pcre.h>
#include <glib.h>   // required for string conversion functions
#include <glib/gstdio.h>

#include "common.h"
#include "qxw.h"
#include "gui.h"
#include "dicts.h"
#include "alphabets.h"

// default dictionaries
#ifdef _WIN32
  // Windows default dictionaries stored in subfolder of {app} folder
  #define NDEFDICTS 1
  char*defdictfn[NDEFDICTS]={
    "\\Dictionaries\\UKACD18plus.txt"
    };
#else
  #define NDEFDICTS 4
  char*defdictfn[NDEFDICTS]={
  "/usr/dict/words",
  "/usr/share/dict/words",
  "/usr/share/dict/british-english",
  "/usr/share/dict/american-english"};
#endif

// UTILITY FUNCTIONS

// utf-8 string to uchar string, printable characters only
// there appears to be no efficient cross-platform library function to do this
// s points to utf-8 string, ucs to output buffer of length l
// return number of output uchars
// assumes valid input; output guaranteed 0-terminated if l>1; if l==1 but no recognised utf8 writes 0
int utf8touchars(uchar*ucs,const char*p,int l) {
  int i,j,l0,n;
  uchar c;
  unsigned char*s=(unsigned char*)p;

  l0=l-1; if(l0==0) l0=1;
  for(i=0;i<l0;) {
    if(*s==0) goto done;
    if(*s>=0x20&&*s<0x7f) {ucs[i++]=*s++; continue;} // ASCII-7 printable
    if(*s<0xc0) {s++; continue;}
         if(*s<0xe0) {n=1; c=*s&0x1f;}
    else if(*s<0xf0) {n=2; c=*s&0x0f;}
    else if(*s<0xf8) {n=3; c=*s&0x07;}
    else if(*s<0xfc) {n=4; c=*s&0x03;}
    else if(*s<0xfe) {n=5; c=*s&0x01;}
    else {s++; continue;}
    s++;
    for(j=0;j<n;j++) {
      if(*s==0) goto done;
      c=(c<<6)+(*s++&0x3f);
      }
    if(ISUPRINT(c)) ucs[i++]=c;
    }
done:
  if(i<l) ucs[i]=0;
  return i;
  }

// uchar to utf-8 string, printable characters only; up to 6 bytes + zero termination output
char*uchartoutf8(char*q,uchar c) {
  int n;
  *q=0;
  if(!ISUPRINT(c)) return q;
  if     (c<      0x80U) *q++= c          ,n=-1;
  else if(c<     0x800U) *q++=(c>> 6)+0xc0,n= 0;
  else if(c<   0x10000U) *q++=(c>>12)+0xe0,n= 1;
  else if(c<  0x200000U) *q++=(c>>18)+0xf0,n= 2;
  else if(c< 0x4000000U) *q++=(c>>24)+0xf8,n= 3;
  else if(c<0x80000000U) *q++=(c>>30)+0xfc,n= 4;
  else return q;
  for(;n>=0;n--) *q++=((c>>(n*6))&0x3f)+0x80;
  *q=0;
  return q;
  }

// uchar string to utf-8 string, printable characters only; up to 6 bytes per input uchar + zero termination output
char*ucharstoutf8(char*q,uchar*s) {
  *q=0;
  for(;*s;s++) q=uchartoutf8(q,*s);
  return q;
  }

// length of a 0-terminated uchar string
int ucharslen(uchar*s) {
  int i;
  for(i=0;s[i];i++) ;
  return i;
  }

// number of bytes in UTF-8 char pointed to by p
int q_mblen(char*p) {
  if(*p == '\0') return 0;
  if(!(*p & 0x80)) return 1;
  else if((*p & 0xe0) == 0xc0) {
        if ((*(p+1) & 0xc0) != 0x80) return -1;
        else return 2;
    }
  else if((*p & 0xf0) == 0xe0) {
        if((*(p+1) & 0xc0) != 0x80) return -1;
        else if((*(p+2) & 0xc0) != 0x80) return -1;
        else return 3;
    }
  else if((*p & 0xf8) == 0xf0) {
        if((*(p+1) & 0xc0) != 0x80) return -1;
        else if((*(p+2) & 0xc0) != 0x80) return -1;
        else if((*(p+3) & 0xc0) != 0x80) return -1;
        else return 4;
    }
  else if((*p & 0xfc) == 0xf8) {
        if((*(p+1) & 0xc0) != 0x80) return -1;
        else if((*(p+2) & 0xc0) != 0x80) return -1;
        else if((*(p+3) & 0xc0) != 0x80) return -1;
        else if((*(p+4) & 0xc0) != 0x80) return -1;
        else return 5;
    }
  else if((*p & 0xfe) == 0xfc) {
        if((*(p+1) & 0xc0) != 0x80) return -1;
        else if((*(p+2) & 0xc0) != 0x80) return -1;
        else if((*(p+3) & 0xc0) != 0x80) return -1;
        else if((*(p+4) & 0xc0) != 0x80) return -1;
        else if((*(p+5) & 0xc0) != 0x80) return -1;
        else return 6;
    }
  else return -1;
  }

// CHARACTER MAPPING

static unsigned char uchartoicctab[MAXUCHAR]; // convert UTF-32 to internal character code
uchar icctouchar[MAXICC+1]; // convert internal character code to UTF-32 of canonical representative
char icctoutf8[MAXICC+1][16]; // convert internal character code to UTF-8 string of canonical representative, 0-terminated
char iccequivs[MAXICC][MAXEQUIV*8+1];
char iccseq[MAXICC+1];
static unsigned char uchartopair[MAXUCHAR];
static uchar pairtouchar[MAXICC][2];

// The following are largely to help with answer treatments etc.
// ICCs are divided into three groups: basically alphabetic (0), numeric (1), symbols (2). These occur in that order
// in the alphabet specification. Some answer treatments only look at the alphabetic group, or only
// (possibly separately) at alphabetic + numeric.
int niccused;                      // number of ICCs actually used
char iccused[MAXICC+1];            // list of ICCs actually used
int icctousedindex[MAXICC+1];      // convert ICC to index into iccused[]
int iccusedtogroup[MAXICC+1];      // convert iccusedindex to group number
int icctogroup[MAXICC+1];          // convert ICC to group number, -1 if not used
int iccgroupstart[MAXICCGROUP+1];  // starts of groups within iccused[], plus one after the end
int iccgroup=0;

// in the "pair" case, icctoutf8 contains two characters into which dictionary characters in iccequivs will be expanded


uchar ICCtouchar(char c) { // convert internal character code to UTF-32 or 0 if not represented
  uchar x;
  if(c<0||c>MAXICC) return 0;
  if(c==ICC_DASH) return '-';
  x=icctouchar[(int)c];
  if(x=='?') return 0;
  return icctouchar[(int)c];
  }

char uchartoICC(int c) { // convert UTF-32 to internal character code or 0 if not represented
  if(c<0||c>=MAXUCHAR) return 0;
  return uchartoicctab[c];
  }

// convert bitmap to internal character code
// return >0: internal character code
// -1: bitmap is zero
// -2: more than one bit set
int abmtoicc(ABM b) {
  if(b==0) return -1;
  if(onebit(b)) return logbase2(b)+1;
  return -2;
  }

int isdisallowed(uchar uc) {
  if(uc>=MAXUCHAR) return 1;
  if(!ISUGRAPH(uc)) return 1;
  if(uc>0x7f) return 0; // all other disallowed characters are 7-bit
  if(strchr(".-?,[]^\"@#",uc)) return 1;
  return 0;
  }

ABM abm_vow,abm_con,abm_use;

// add an entry to the current alphabet
// characters must be added in ascending order of internal code, so that
// iccused[] is set correctly for Caesar ciphers etc.
// Returns:
// 0: OK (including case of empty rep string or bad icc)
// 1: disallowed character in rep string (entire entry ignored)
// 2: one or more disallowed character(s) in equiv string (character(s) ignored)
int addalphamapentry(int icc,char*rep,char*equiv,int vow,int con,int seq) {
  uchar uc,s0[3],s1[MAXEQUIV+1];
  int f,g,i,j;

  if(icc<1||icc>=MAXICC) return 0;
DEB_AL printf("rep=%s equiv=%s\n",rep,equiv);
  i=utf8touchars(s0,rep,3); // convert rep to UTF-32, up to 2 characters
DEB_AL {
    printf("  rep -> %d char(s):",i);
    for(j=0;j<i;j++) printf(" U+%08x",s0[j]);
    printf("\n");
    }
  if(i==1) { // normal case where representative is single character
    uc=s0[0];
    if(isdisallowed(uc)) {
      DEB_AL printf("  uc=%08x: disallowed\n",uc);
      icctouchar[icc]='?';
      strcpy(icctoutf8[icc],"?");
      return 1;
      }
    icctouchar[icc]=uc;
    if(vow)               abm_vow|=ICCTOABM(icc);
    if(con)               abm_con|=ICCTOABM(icc);
         if(ISUALPHA(uc)) g=0;
    else if(ISUDIGIT(uc)) g=1;
    else                  g=2;
    abm_use|=ICCTOABM(icc);
    if(niccused<MAXICC) {
      if(g<iccgroup) g=2; // if groups do not occur in the right order, count everything from first error as a symbol
      while(iccgroup<g) iccgroupstart[++iccgroup]=niccused;
      for(j=iccgroup+1;j<MAXICCGROUP+1;j++) iccgroupstart[j]=niccused+1;
      iccusedtogroup[niccused]=iccgroup;
      icctogroup[icc]=iccgroup;
      icctousedindex[icc]=niccused;
      iccused[niccused]=icc;
      DEB_AL printf("  icc=%d usedindex=%d group=%d\n",icc,niccused,iccgroup);
      niccused++;
      }
    iccseq[icc]=seq;
    strncpy(icctoutf8[icc],rep,7); // guaranteed 0-terminated by memset below
    i=utf8touchars(s1,equiv,MAXEQUIV+1);
    strncpy(iccequivs[icc],equiv,MAXEQUIV*8); // guaranteed 0-terminated by memset below
    uchartoicctab[uc]=icc; // include the character itself
    f=0;
    for(i=0;s1[i]&&i<MAXEQUIV;i++) {
DEB_AL   printf(" %08x",s1[i]);
      if(s1[i]==0xfeff) continue; // 0xfeff is BOM
      if(isdisallowed(s1[i])) {f=1; continue;}
      uchartoicctab[s1[i]]=icc;
      }
DEB_AL printf("\n");
    if(f) return 2;
    return 0;
    }
  else if(i==2) { // special case of a pair of characters: save pre-map (e.g. of A-umlaut to AE)
    if(isdisallowed(s0[0])) return 1; // both characters of pair valid?
    if(isdisallowed(s0[1])) return 1;
    iccequivs[icc][0]=0;
    strncpy(icctoutf8[icc],rep,15); // guaranteed 0-terminated by memset below
    i=utf8touchars(s1,equiv,MAXEQUIV+1);
    strncpy(iccequivs[icc],equiv,MAXEQUIV*8); // guaranteed 0-terminated by memset below
DEB_AL printf("pair %08x %08x: ",s0[0],s0[1]);
    f=0;
    for(i=0;s1[i]&&i<MAXEQUIV;i++) {
DEB_AL   printf(" %08x",s1[i]);
      if(s1[i]==0xfeff) continue; // 0xfeff is BOM
      if(isdisallowed(s1[i])) {f=1; continue;}
      uchartopair[s1[i]]=icc;
      }
    pairtouchar[icc][0]=s0[0];
    pairtouchar[icc][1]=s0[1];
DEB_AL printf("\n");
    if(f) return 2;
    }
  else {
    DEB_AL printf("  replacing with '?'\n");
    icctouchar[icc]='?';
    strcpy(icctoutf8[icc],"?");
    return 0; // empty: OK
    }
  return 0;
  }


void clearalphamap() {
  int i;
  memset(uchartoicctab,0,sizeof(uchartoicctab));
  memset(icctouchar,0,sizeof(icctouchar));
  memset(icctoutf8,0,sizeof(icctoutf8));
  memset(iccequivs,0,sizeof(iccequivs));
  memset(iccseq,0,sizeof(iccseq));
  memset(uchartopair,0,sizeof(uchartopair));
  memset(pairtouchar,0,sizeof(pairtouchar));
  abm_vow=abm_con=abm_use=0;
  niccused=0;
  memset(iccused,0,sizeof(iccused));
  for(i=0;i<MAXICC+1;i++) {
    icctousedindex[i]=-1;
    iccusedtogroup[i]=-1;
    icctogroup[i]=-1;
    }
  memset(iccgroupstart,0,sizeof(iccgroupstart));
  strcpy(icctoutf8[MAXICC],"-");
  icctouchar[MAXICC]='-';
  iccgroup=0;
  }

void initalphamap(struct alphaentry*a) {
  int i;
  clearalphamap();
  for(i=1;i<MAXICC;i++) if(a[i].rep) addalphamapentry(i,a[i].rep,a[i].eqv,a[i].vow,a[i].con,a[i].seq);
  initcodeword();
  }

// non-zero on not found
int initalphamapbycode(char*s) {
  int i,j;
  clearalphamap();
  if(!s) return 1;
  for(i=0;i<NALPHAINIT;i++)
    for(j=1;alphaname[i][j][0];j++) {
      if(!STRCASECMP(s,alphaname[i][j])) {
        initalphamap(alphainitdata[i]);
        return 0;
        }
      }
  return 1;
  }

// print a character represented as uchar
void printU(uchar c) { char s[10]; uchartoutf8(s,c); printf("%s",s); }

// print a string represented as uchars
void printUs(const uchar*s) {for(;*s;s++) printU(*s); }

// print a character represented in internal character code
void printICC(char c) { printf("%s",icctoutf8[(int)c]); }

// print a string represented in internal character code
void printICCs(const char*s) { for(;*s;s++) printICC(*s); }


// DICTIONARY FILES

// set up possible file encodings for dictionary files
#define NFILEENC 6  

struct fileenc {
  char nenc[12];  // name of encoding for g_convert()
  char bom[4];    // potential byte order mark at start of file
  int lbom;       // length of byte order mark
  };

static struct fileenc fenc[NFILEENC] = {
  {"ISO-8859-1",{'\x00'},                     0},
  {"UTF-8"     ,{'\xEF','\xBB','\xBF'},       3},
  {"UTF-16LE"  ,{'\xFF','\xFE'},              2},
  {"UTF-16BE"  ,{'\xFE','\xFF'},              2},
  {"UTF-32LE"  ,{'\xFF','\xFE','\x00','\x00'},4},
  {"UTF-32BE"  ,{'\x00','\x00','\xFE','\xFF'},4}        
};

static struct memblk*dstrings[MAXNDICTS]={0}; // dictionary string pools

struct answer*ans=0,**ansp=0;
struct light*lts=0;

int atotal=0;                // total answers
int ltotal=0;                // total lights
int ultotal=0;               // total uniquified lights
unsigned int dusedmask;      // set of dictionaries used

char dfnames[MAXNDICTS][SLEN];
char dsfilters[MAXNDICTS][SLEN];
char dafilters[MAXNDICTS][SLEN];
char lemdesc[NLEM][LEMDESCLEN]={""," (rev.)"," (cyc.)"," (cyc., rev.)","*"};
char*lemdescADVP[NLEM]={"normally","reversed","cyclically permuted","cyclically permuted and reversed","with any other permutation"};

static int line;
int dst_lines[MAXNDICTS];
int dst_lines_f[MAXNDICTS];
int dst_lines_fa[MAXNDICTS];
int dst_u_rejline[MAXNDICTS][MAXUCHAR];
int dst_u_rejline_count[MAXNDICTS][MAXUCHAR];

#define HTABSZ 1048576
static int ahtab[HTABSZ]={0};

static int cmpans(const void*p,const void*q) {int u; // string comparison for qsort
  u=strcmp( (*(struct answer**)p)->ul,(*(struct answer**)q)->ul); if(u) return u;
  u=strcmp( (*(struct answer**)p)->cf,(*(struct answer**)q)->cf);       return u;
  }

static void clearcounts(int dn) {
  int i;
  line=0;
  dst_lines[dn]=0;
  dst_lines_f[dn]=0;
  dst_lines_fa[dn]=0;
  for(i=0;i<MAXUCHAR;i++) dst_u_rejline[dn][i]=-1;
  memset(dst_u_rejline_count[dn],0,sizeof(dst_u_rejline_count[dn]));
  }

static void freedstrings(int d) { // free string pool associated with dictionary d
  struct memblk*p;
  while(dstrings[d]) {p=dstrings[d]->next;free(dstrings[d]);dstrings[d]=p;}
  }

void freedicts(void) { // free all memory allocated by loaddicts, even if it aborted mid-load
  int i;
  for(i=0;i<MAXNDICTS;i++) freedstrings(i);
  FREEX(ans);
  FREEX(ansp);
  atotal=0;
  }

// answer pool is linked list of `struct memblk's containing
// strings:
//   char 0 of string is word score*10+128 (in range 28..228);
//   char 1 of string is dictionary number
//   char 2 onwards:
//     (0-terminated) citation form, in UTF-8
//     (0-terminated) untreated light form, in chars

static struct memblk*memblkp=0;
static int memblkl=0;

// Add a new dictionary word with UTF-8 citation form s0
// dictionary number dn, score f. Return 1 if added, 0 if not, -2 for out of memory
static int adddictword(char*s0,int dn,pcre*sre,pcre*are,float f) {
  int c,c0,i,l0,l1,l2;
  uchar t[MXLE+1],u;
  char s1[MXLE*16+1];
  char s2[MXLE+1];
  struct memblk*q;
  int pcreov[120];

  l0=strlen(s0);
  utf8touchars(t,s0,MXLE+1);
  for(i=0,l1=0,l2=0;t[i];i++) {
    u=t[i];
    if(!ISUGRAPH(u)) continue; // printable, not a space?
    if(u>=MAXUCHAR) continue;
    c=uchartoICC(u);
    if(c) { // characters is in basic alphabet; could check for "reject" chars here
      if(l2>=MXLE) return 0; // too long?
      strcpy(s1+l1,icctoutf8[c]); l1+=strlen(s1+l1);
      s2[l2++]=c;
      continue;
      }
    else {
      c=uchartopair[t[i]];
      if(c) { // represents a pair of characters?
        c0=uchartoICC(pairtouchar[c][0]);
        if(c0) {
          if(l2>=MXLE) return 0; // too long?
          strcpy(s1+l1,icctoutf8[c0]); l1+=strlen(s1+l1);
          s2[l2++]=c0;
          }
        c0=uchartoICC(pairtouchar[c][1]);
        if(c0) {
          if(l2>=MXLE) return 0; // too long?
          strcpy(s1+l1,icctoutf8[c0]); l1+=strlen(s1+l1);
          s2[l2++]=c0;
          }
        }
      else {
        if(dst_u_rejline_count[dn][u]++==0) dst_u_rejline[dn][u]=line; // record first instance of rejected Unicode
        }
      }
    }
  s1[l1]=0;
  s2[l2]=0;
  DEB_DV {
    printf("adddictword: citation=<%s> canonical UTF-8=<%s> canonical ICC=<",s0,s1);
    printICCs(s2);
    printf(">\n  Unicode:");
    for(i=0;s2[i];i++) printf(" U+%06X",icctouchar[(unsigned int)s2[i]]);
    printf("\n");
    }
  if(l2<1) { // too short?
    DEB_DV printf("  too short\n");
    return 0;
    }
  // here s0 contains citation form in UTF-8
  //      s1 contains canonicalised form in UTF-8, length l1
  //      s2 contains canonicalised form in internal character code, length l2 1<=l2<=MXLE

  dst_lines[dn]++;
  if(sre) {
    i=pcre_exec(sre,0,s0,l0,0,0,pcreov,120);
    DEB_DI if(i<-1) printf("PCRE error %d\n",i);
    if(i<0) {
      DEB_DV printf("  failed file filter\n");
      return 0; // failed match
      }
    }
  dst_lines_f[dn]++;
  if(are) {
    i=pcre_exec(are,0,s1,l1,0,0,pcreov,120);
    DEB_DI if(i<-1) printf("PCRE error %d\n",i);
    if(i<0) {
      DEB_DV printf("  failed answer filter\n");
      return 0; // failed match
      }
    }
  dst_lines_fa[dn]++;

  if(memblkp==NULL||memblkl+2+l0+1+l2+1>MEMBLK) { // allocate more memory if needed (this always happens on first pass round loop)
    q=(struct memblk*)malloc(sizeof(struct memblk));
    if(q==NULL) {return -2;}
    q->next=NULL;
    if(memblkp==NULL) dstrings[dn]=q; else memblkp->next=q; // link into list
    memblkp=q;
    memblkp->ct=0;
    memblkl=0;
    }
  *(memblkp->s+memblkl++)=(char)floor(f*10.0+128.5); // score with rounding
  *(memblkp->s+memblkl++)=dn;
  strcpy(memblkp->s+memblkl,s0);memblkl+=l0+1; // citation form, UTF-8
  strcpy(memblkp->s+memblkl,s2);memblkl+=l2+1; // canonical form, internal character code
  memblkp->ct++; // count words in this memblk
  return 1;
  }
 
// read n bytes from fp and interpret as little-endian integer
static int getint(FILE*fp,int n) { int i,u; for(i=0,u=0;i<n;i++) u+=fgetc(fp)<<(i*8); return u; }

#define MXHNODES 512
static int hnodep[2][MXHNODES]; // "0" and "1" pointers or -1 for none
static int hnodec[MXHNODES]; // char represented or -1 for none
static int nhn;

// add a new Huffman tree entry for code m of length l representing i
static int addhcode(int m,int l,int i) {
  int *p;
  int j,n;

DEB_DI printf("addhcode(%d,%d,%d) nhn=%d\n",m,l,i,nhn);
  for(n=0,j=l-1;j>=0;j--) { // big-endian...
    p=hnodep[(m>>j)&1]+n;
    if(*p==-1) { // node does not exist yet
      if(nhn>=MXHNODES) return -1;
      *p=nhn;
      hnodep[0][nhn]=-1;
      hnodep[1][nhn]=-1;
      hnodec[nhn]=-1;
      nhn++;
      }
    n=*p;
    }
  hnodec[n]=i;
  return 0;
  }

// Attempt to load a .TSD file. Return number of words >=0 on success, <0 on error.
static int loadtsd(FILE*fp,int format,int dn,pcre*sre,pcre*are) {
  int c,i,j,l,m,ml,n,u,nw;
  int hoff[MXLE+1]; // file offsets into Huffman coded block
  int dcount[MXLE+1]; // number of words of each length
  char s1[SLEN];
  GError*error=NULL;
  gchar*sp=NULL;

  DEB_DI printf("attempting to load TSD format=0x%x\n",format);
  clearcounts(dn);
  if(format!='0'&&format!='1') return -1; // only TSD0 and TSD1 supported
  if(fseek(fp,4,SEEK_SET)<0) return -1;
  ml=getint(fp,2); // maximum length
  if(ml<1) return -1;
  DEB_DI printf("ml=%d\n",ml);
  if(format=='1') getint(fp,12*ml+12); // skip some bytes
  for(i=0;i<ml;i++) {
    u=getint(fp,4);
    if(u<0) return -1;
    if(i<=MXLE) hoff[i]=u;
    }
  for(i=0;i<ml;i++) {
    u=getint(fp,4);
    if(u<0) return -1;
    if(i<=MXLE) {
      dcount[i]=u;
DEB_DI printf("dcount[%d]=%d\n",i,u);
      }
    }
  if(fseek(fp,format=='0'?80:1000,SEEK_CUR)<0) return -1; // skip comment
  u=getint(fp,2); // ?
  u=getint(fp,2); // ?
DEB_DI printf("hcode array at %08lx\n",ftell(fp));
  nhn=1;
  hnodep[0][0]=-1;
  hnodep[1][0]=-1;
  hnodec[0]=-1;
  for(i=0;i<256;i++) { // loop over ISO-8859-1 characters
    c=getint(fp,1); // character
    l=getint(fp,1); // code length
    u=getint(fp,2); // ?
    m=getint(fp,4); // code
    if(l>0) {
DEB_DI {
        printf("%02x %c ",i,isprint(i)?i:'?');
        for(j=l-1;j>=0;j--) printf("%d",(m>>j)&1);
        printf("\n");
        }
      if(addhcode(m,l,i)<0) return -1;
      }
    }
  nw=0;
  for(l=1;l<ml;l++) {
    if(l>MXLE) break;
    if(hoff[l]==0) continue;
    if(fseek(fp,hoff[l],SEEK_SET)<0) return -1;
    DEB_DI printf("starting to read length %d at offset %08x; dstrings[%d]=%p memblkp=%p\n",l,hoff[l],dn,(void*)dstrings[dn],(void*)memblkp);
    j=8;
    c=0;
    for(i=0;i<dcount[l];i++) {
      m=0;
      n=0;
      for(;;) {
        if(j==8) {j=0;c=fgetc(fp);if(c==EOF) return -1;}
        DEB_DI printf("%d",(c>>j)&1);
        n=hnodep[(c>>j)&1][n];
        j++;
        u=hnodec[n];
        if(u!=-1) {
          DEB_DI printf(" %02x %c\n",u,isprint(u)?u:'?');
          if((u>=0x01&&u<0x7f)||(u>=0xa0&&u<=0xff)) {
            s1[m]=u;
            if(m<SLEN-1) m++; // don't overflow
            }
          n=0;
          }
        if(u==0) break;
        }
      if(format=='1') {
        if(j==8) {j=0;c=fgetc(fp);if(c==EOF) return -1;} // skip a zero bit for some reason
        j++;
        }
      if(m==SLEN-1) continue; // hit string limit? discard
      s1[m]=0;
      for(u=0;u<m;u++) if((unsigned char)s1[u]<0x20) break; // look for any non-printable character
      s1[u]=0; // stop there
      sp=g_convert(s1,-1,"UTF-8","ISO-8859-1",NULL,NULL,&error);
      if(error) continue;
      u=adddictword(sp,dn,sre,are,0.0);
      g_free(sp),sp=0;
      if(u==-2) return -2;
      if(u==1) nw++;
      }
    }
  return nw;
  }


// return number of answers loaded or:
// -1: file not found or problem with encoding
// -2: out of memory error from adddictword
static int loadonedict(int dn,int sil) {
  int at,i,j;
  FILE *fp=0;
  char s0[SLEN]; // citation form
  gchar t[SLEN+1]; // input buffer

  float f;
  int mode,owd,rc;
  pcre*sre,*are;
  const char*pcreerr;
  int pcreerroff;
  char sfilter[SLEN+1];
  char afilter[SLEN+1];
  GError *error = NULL;
  gchar *sp = NULL;
  gsize l0;
  char bom[5];

  rc=0;
  owd=0;
  at=0;
  freedstrings(dn);
  memblkl=0; // number of bytes stored so far in current memory block
  memblkp=0; // current memblk being filled
  if(dfnames[dn][0]=='\0') {
    if(dafilters[dn][0]=='\0') return 0; // dictionary slot unused
    owd=1; // one-word dictionary
    sre=0;
    are=0;
    goto ew3;
    }
  strcpy(sfilter,dsfilters[dn]);
  if(!strcmp(sfilter,"")) sre=0;
  else {
    sre=pcre_compile(sfilter,PCRE_CASELESS|PCRE_UTF8|PCRE_UCP,&pcreerr,&pcreerroff,0);
    if(pcreerr) {
      sprintf(t,"Dictionary %d\nBad file filter syntax: %.100s",dn+1,pcreerr);
      if(!sil) reperr(t);
      }
    }
  strcpy(afilter,dafilters[dn]);
  if(!strcmp(afilter,"")) are=0;
  else {
    are=pcre_compile(afilter,PCRE_CASELESS|PCRE_UTF8|PCRE_UCP,&pcreerr,&pcreerroff,0);
    if(pcreerr) {
      sprintf(t,"Dictionary %d\nBad answer filter syntax: %.100s",dn+1,pcreerr);
      if(!sil) reperr(t);
      }
    }

ew3:
  mode=-1;    // Indicates file encoding not set by BOM in file
  if(owd) { mode=1; goto retry; }   // Always set file encoding to UTF-8 for one-word dictionary
  fp=q_fopen(dfnames[dn],"rb"); // binary mode
  if(!fp) {
    sprintf(t,"Dictionary %d\nFile not found",dn+1);
    if(!sil) reperr(t);
    rc=-1; goto exit;
    }
  if(fread(bom,1,4,fp)<4) {rewind(fp); goto retry;} // too short for a BOM, so use mode -1
  if(!strncmp(bom,"TSD",3)) {
    i=loadtsd(fp,bom[3],dn,sre,are);
    if(i>=0) {at=i; goto exit;} // successfully read
    }
  rewind(fp);
  if(fgets(bom,5,fp)!=NULL) {       // re-read BOM at start of file
    for(i=0; i<NFILEENC; i++) {
      if(fenc[i].lbom>0) {
        if(!(memcmp(bom,fenc[i].bom,fenc[i].lbom))) {
          mode=i;                             // BOM found so set mode
          fseek(fp,fenc[i].lbom,SEEK_SET);    // and skip past BOM 
          break;
          }
        }
      }   // end of for loop
    if(mode>1) {    // Cannot read this file encoding mode
      sprintf(t,"Dictionary %d\nCannot read file encoding:\ntry UTF-8 or ISO-8859-1 encoding",dn+1);
      if(!sil) reperr(t);
      rc=-1; goto exit;
      }
    if(mode==-1) rewind(fp);    // No BOM, so read words from start of file
    }
retry:
  at=0;
  freedstrings(dn);
  memblkl=0; // number of bytes stored so far in current memory block
  memblkp=0; // current memblk being filled
  dstrings[dn]=0;
  clearcounts(dn);
  DEB_DI printf("Reading dictionary file %s (in mode %d), owd=%d\n",dfnames[dn],mode,owd);
  for(;;) {       // Loop through dictionary 'words' until EOF
    if(owd) strcpy(t,dafilters[dn]);
    else {
      if(feof(fp)) break;
      if(fgets(t,SLEN,fp)==NULL) break;
      line++;
      }
    j=strlen(t)-1;
    if(j<0) goto skipword;
    while(j>=0&&t[j]>=0&&t[j]<=' ') t[j--]=0;     // Strip control characters/white space from end of string
    while (j>=0&&((t[j]>='0'&&t[j]<='9')||t[j]=='.'||t[j]=='+'||t[j]=='-')) j--;  // get score (if any) from end
    j++;
    f=0.0;
    if(j==0||t[j-1]!=' ') j=strlen(t); // all digits, or no space? treat it as a 'word'
    else {
      sscanf(t+j,"%f",&f);
      if(f>= 10.0) f= 10.0;
      if(f<=-10.0) f=-10.0;
      t[j--]=0; // rest of input is treated as a 'word' (which may contain spaces)
      }
    while(j>=0&&t[j]>=0&&t[j]<=' ') t[j--]=0;  // remove trailing white space
    j++;
    if(j<1) goto skipword;

    // t now contains 'word' for conversion; convert it first to UTF-8 for use as citation form
    if(mode>-1) {     // Use encoding mode based on BOM
      sp=g_convert(t,-1,"UTF-8",fenc[mode].nenc,NULL,&l0,&error);
      if(error) {
        sprintf(t,"Dictionary %d\nFile not encoded in accordance with its BOM",dn+1);
        if(!sil) reperr(t);
        rc=-1; goto exit;
        }
    } else { // try fallback encodings in turn
      if(mode==-1) sp=g_convert(t,-1,"UTF-8","UTF-8",NULL,&l0,&error); // effectively a check for valid UTF-8
      if(mode==-2) sp=g_convert(t,-1,"UTF-8","ISO-8859-1",NULL,&l0,&error);
      if(error) {
        DEB_DI {
          printf("error: %s\n",error->message);
          for(i=0;t[i];i++) printf(" %02x",t[i]);
          printf("\n");
          }
        g_clear_error(&error);
        freedstrings(dn);
        rewind(fp);
        mode--;
        if(mode>-3) goto retry; // go round again and try next encoding
        // here if none of the encodings worked 
        sprintf(t,"Dictionary %d\nFile does not use a recognised encoding:\ntry UTF-8 or ISO-8859-1",dn+1);
        if(!sil) reperr(t);
        rc=-1; goto exit;
        }
      }
    // here we have UTF-8 citation form of string in sp, length in l0
    if(l0>=SLEN) goto skipword;
    strcpy(s0,sp);
    g_free(sp),sp=0;

    i=adddictword(s0,dn,sre,are,f);
    if(i==-2) {rc=-2; goto exit;}
    if(i==1) at++;

skipword:
    if(sp) g_free(sp),sp=0;
    g_clear_error(&error);
    if(owd) break;
    }  // End of 'word' for loop

exit:
  if(sp) g_free(sp),sp=0;
  if(sre) pcre_free(sre);
  if(are) pcre_free(are);
  g_clear_error(&error);
  if(!owd) if(fp) fclose(fp);
  if(rc<0) return rc;
  else return at;
  }

int loaddicts(int sil) { // load (or reload) dictionaries from dfnames[]
  // sil=1 suppresses error reporting
  // returns: 0=success; 1=bad file; 2=no words; 4=out of memory
  // sets dusedmask to list of dictionaries with anything in them
  struct answer*ap;
  struct memblk*p;
  int at,dn,i,j,k,l,rc,u;
  char t[SLEN];
  unsigned int h;

  freedicts();
  at=0;
  rc=0;
  dusedmask=0;

  for(dn=0;dn<MAXNDICTS;dn++) {
    u=loadonedict(dn,sil);
    DEB_DI printf("loadonedict(%d) returned %d\n",dn,u);
    if(u==-2) goto ew4; // out of memory
    if(u<0) {
      rc=1;
      freedstrings(dn);
      }
    else if(u>0) at+=u,dusedmask|=1<<dn;
    }

  if(at==0) {  // No words from any dictionary
    sprintf(t,"No words available from any dictionary");
    if(!sil) reperr(t);
    freedicts();
    return 2;
    }
  // allocate array space from counts
  atotal=at;
  DEB_DI printf("atotal=%9d\n",atotal);
  ans =(struct answer* )malloc(atotal*sizeof(struct answer));  if(ans ==NULL) goto ew4;
  ansp=(struct answer**)malloc(atotal*sizeof(struct answer*)); if(ansp==NULL) goto ew4; // pointer array for sorting

  k=0;
  for(dn=0;dn<MAXNDICTS;dn++) {
    p=dstrings[dn];
    while(p!=NULL) {
      for(i=0,l=0;i<p->ct;i++) { // loop over all words
        ans[k].score  =pow(10.0,((float)(*(unsigned char*)(p->s+l++))-128.0)/10.0);
        ans[k].cfdmask=
        ans[k].dmask  =1<<*(unsigned char*)(p->s+l++);
        ans[k].cf     =p->s+l; l+=strlen(p->s+l)+1;
        ans[k].acf    =0;
        ans[k].ul     =p->s+l; l+=strlen(p->s+l)+1; // in internal character code
        ans[k].banned =0;
        k++;
        }
      p=p->next;
      }
    }
  DEB_DI printf("k=%9d\n",k);
  assert(k==atotal);
  for(i=0;i<atotal;i++) ansp[i]=ans+i;

  qsort(ansp,atotal,sizeof(struct answer*),cmpans);
  ap=0; // ap points to first of each group of matching citation forms
  for(i=0,j=-1;i<atotal;i++) { // now remove duplicate entries
    if(i==0||strcmp(ansp[i]->ul,ansp[i-1]->ul)) {j++;ap=ansp[j]=ansp[i];}
    else {
      ansp[j]->dmask|=ansp[i]->dmask; // union masks
      ansp[j]->score*=ansp[i]->score; // multiply scores over duplicate entries
      if(strcmp(ansp[i]->cf,ansp[i-1]->cf)) ap->acf=ansp[i],ap=ansp[i]; // different citation forms? link them together
      else ap->cfdmask|=ansp[i]->cfdmask; // cf:s the same: union masks
      }
    }
  atotal=j+1;

  for(i=0;i<HTABSZ;i++) ahtab[i]=-1;
  for(i=0;i<atotal;i++) {
    h=0;
    for(j=0;ansp[i]->ul[j];j++) h=h*113+ansp[i]->ul[j];
    h=h%HTABSZ;
    ansp[i]->ahlink=ahtab[h];
    ahtab[h]=i;
    }

  for(i=0;i<atotal;i++) {
    if(ansp[i]->score>= 1e10) ansp[i]->score= 1e10; // clamp scores
    if(ansp[i]->score<=-1e10) ansp[i]->score=-1e10;
    }
  for(i=0;i<atotal;i++) {
    if(ansp[i]->acf==0) continue;
    ap=ansp[i];
    do {
  //    printf(" \"%s\"",ap->cf);
      ap=ap->acf;
      } while(ap);
  //  printf("\n");
    }

  DEB_DI printf("Total unique answers by entry: %d\n",atotal);
  return rc; // return 1 if any file failed to load
ew4:
  freedicts();
  if(!sil) reperr("Out of memory loading dictionaries");
  return 4;
  }

// Run through some likely candidates for default dictionaries
// Return
//   0: found something
// !=0: nothing found
int loaddefdicts() {int i;
  if(startup_dict1[0]) { // do we have a candidate preferred startup dictionary?
    strcpy(dfnames[0],startup_dict1);
    strcpy(dsfilters[0],startup_ff1);
    strcpy(dafilters[0],startup_af1);
    loaddicts(1);
    DEB_DI printf("read preferences default, atotal=%d\n",atotal);
    if(atotal!=0) return 0; // happy if we found any words at all
    }
  for(i=0;i<NDEFDICTS;i++) {
    #ifdef _WIN32   // Set the default dictionary path as subpath of {app} path
      GetCurrentDirectory(SLEN, dfnames[0]);
      strcat(dfnames[0],defdictfn[i]);
    #else
      strcpy(dfnames[0],defdictfn[i]);
    #endif
    strcpy(dsfilters[0],"^.*+(?<!'s)");
    strcpy(dafilters[0],"");
    loaddicts(1);
    if(atotal!=0) return 0; // happy if we found any words at all
    }
  // all failed
  strcpy(dfnames[0],"");
  strcpy(dsfilters[0],"");
  strcpy(dafilters[0],"");
  return 4;
  }

// is word (in internal character code) in dictionaries specified by dm?
int iswordindm(const char*s,int dm) {
  int i,p;
  unsigned int h;
  h=0;
  for(i=0;s[i];i++) h=h*113+s[i];
  h=h%HTABSZ;
  p=ahtab[h];
  while(p!=-1) {
    if(!strcmp(s,ansp[p]->ul)) return !!(ansp[p]->dmask&dm);
    p=ansp[p]->ahlink;
    }
  return 0;
  }
