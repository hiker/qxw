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

#include "common.h"
#include "qxw.h"
#include "dicts.h"
#include "treatment.h"
#include "gui.h"

#ifdef _WIN32  // using wrapper for Windows dynamic linking functions
  #include "pfdlfcn.h"
#else
  #include <dlfcn.h>
#endif

#define HTABSZ 4194304

// INITIAL FEASIBLE LIST GENERATION

static int curans,curem,curten,curdm;

int treatmode=0,treatorder[NMSG]={0,0};
char tpifname[SLEN]="";
ABM treatcstr[NMSG][MXFL]={{0},{0}};
int tambaw=0;

char  treatmsg       [NMSG][MXLT*8+1]; // unfiltered, as provided by GUI, utf-8
uchar treatmsgU      [NMSG][MXLT+1];   // converted to uchars
char  treatmsgICC    [NMSG][MXLT+1];   // converted to ICCs
char  treatmsgICCAZ  [NMSG][MXLT+1];   // alphabetic only
char  treatmsgICCAZ09[NMSG][MXLT+1];   // alphabetic+digits
uchar treatmsgUAZ    [NMSG][MXLT+1];   // converted to uchars
uchar treatmsgUAZ09  [NMSG][MXLT+1];   // converted to uchars
char  treatmsgAZ     [NMSG][MXLT+1];   // 7-bit clean *no transliteration*
char  treatmsgAZ09   [NMSG][MXLT+1];

char *treatmessage       [NMSG]={"",""};   // pointer copies of above
uchar*treatmessageU      [NMSG];
char *treatmessageICC    [NMSG];
char *treatmessageICCAZ  [NMSG];
char *treatmessageICCAZ09[NMSG];
uchar*treatmessageUAZ    [NMSG];
uchar*treatmessageUAZ09  [NMSG];
char *treatmessageAZ     [NMSG];
char *treatmessageAZ09   [NMSG];

char  msgchar[NMSG];
uchar msgcharU[NMSG];               // converted to uchar
uchar msgcharUAZ[NMSG];             // converted to uchar, alphabetic, made upper case
uchar msgcharUAZ09[NMSG];           // ... plus digits
char  msgcharICC[NMSG];             // converted to ICC
char  msgcharICCAZ[NMSG];
char  msgcharICCAZ09[NMSG];
char  msgcharAZ[NMSG];              // 7-bit clean *no transliteration*
char  msgcharAZ09[NMSG];

const char*answerICC;
const uchar*answerU;

int clueorderindex;
int gridorderindex[MXLE];
int checking[MXLE];
int lightlength;
int lightx;
int lighty;
int lightdir;

// STATICS FOR VARIOUS TREATMENTS

// Playfair square
static char psq[25];    // square position to ICC, 0 if cell not filled
static int psc[MAXICC]; // ICC to square position, -1 if not present

static int*tfl=0; // temporary feasible list
static int ctfl,ntfl;

static int clts;
static int hstab[HTABSZ];
static int haestab[HTABSZ];
static struct memblk*lstrings=0;
static struct memblk*lmp=0;
static int lml=MEMBLK;

static void dohistdata(struct light*l) {
  int i,j,m,n;
  memset(l->hist,0,sizeof(l->hist));
  l->lbm=0;
  m=0;
  n=strlen(l->s);
  if(l->tagged) n-=NMSG;
  for(i=0;i<n;i++) {
    j=(int)l->s[i];
    l->hist[j]++;
    if(l->hist[j]>m) m=l->hist[j];
    l->lbm|=ICCTOABM(j);
    }
  }

// return index of light, creating if it doesn't exist; -1 on no memory
static int findlight(const char*s,int tagged,int a,int e) {
  unsigned int h0,h1;
  int f,i,u,l0;
  int l;
  int len0,len1;
  struct light*p;
  struct memblk*q;

  len0=strlen(s);
  if(tagged) len0-=NMSG;
  assert(len0>0);
  h0=0;
  for(i=0;i<len0;i++) h0=h0*113+s[i];
  h1=h0*113+a*27+e;
  h0=h0%HTABSZ; // h0 is hash of string only
  h1=h1%HTABSZ; // h1 is hash of string+treatment+entry method
  l=haestab[h1];
  while(l!=-1) {
    if(lts[l].ans==a&&lts[l].em==e&&!strcmp(s,lts[l].s)) return l; // exact hit in all particulars? return it
    l=lts[l].hashaeslink;
    }
  if(ltotal>=clts) { // out of space to store light structures? (always happens first time)
    clts=clts*2+5000; // try again a bit bigger
    p=realloc(lts,clts*sizeof(struct light));
    if(!p) return -1;
    lts=p;
    DEB_FL printf("lts realloc: %d\n",clts);
    }
  l=hstab[h0];
  u=-1; // look for the light string, independent of how it arose
  f=0;
  while(l!=-1) {
    len1=strlen(lts[l].s);
    if(lts[l].tagged) len1-=NMSG;
    assert(len1>0);
    if(len0==len1&&!strncmp(s,lts[l].s,len0)) { // match as far as non-tag part is concerned
      u=lts[l].uniq;
      if(!strcmp(s,lts[l].s)) {f=1; break;} // exact match including possible tags
      }
    l=lts[l].hashslink;
    }
  if(f==0) { // we do not have a full-string match
    l0=strlen(s)+1;
    if(lml+l0>MEMBLK) { // make space to store copy of light string
      DEB_FL printf("memblk alloc\n");
      q=(struct memblk*)malloc(sizeof(struct memblk));
      if(!q) return -1;
      q->next=NULL;
      if(lmp==NULL) lstrings=q; else lmp->next=q; // link into list
      lmp=q;
      lml=0;
      }
    if(u==-1) u=ultotal++; // allocate new uniquifying number if needed
    lts[ltotal].s=lmp->s+lml;
    strcpy(lmp->s+lml,s);lml+=l0;
  } else {
    lts[ltotal].s=lts[l].s;
    }
  lts[ltotal].ans=a;
  lts[ltotal].em=e;
  lts[ltotal].uniq=u;
  lts[ltotal].tagged=tagged;
  lts[ltotal].hashslink  =hstab  [h0]; hstab  [h0]=ltotal; // insert into hash tables
  lts[ltotal].hashaeslink=haestab[h1]; haestab[h1]=ltotal;
  dohistdata(lts+ltotal);
  return ltotal++;
  }
 
// is word in dictionaries specified by curdm?
int iswordICC(const char*s) { return iswordindm(s,curdm); }

// as above, but s converted from uchars
int iswordU(const uchar*s) {
  int i,j,u;
  char s0[MXFL+1];
  for(i=0,j=0;s[i]&&j<MXFL;i++) {
    u=uchartoICC(s[i]);
    if(u) s0[j++]=u;
    }
  s0[j]=0;
  return iswordICC(s0);
  }

// as above, but s converted from UTF-8 (or just 7-bit ASCII): compatible with previous versions
int isword(const char*s) {
  uchar s0[MXFL+1];
  utf8touchars(s0,s,MXFL+1);
  return iswordU(s0);
  }

int ICCtoclass(char c) {
  if(c<0||c>MAXICC) return -1;
  return icctogroup[(int)c];
  }

// add light to feasible list: s=text of light (in internal character code), a=answer from which treated (-ve for msgword), e=entry method
// returns 0 if OK, !=0 on (out of memory) error
static int addlight(const char*s,int a,int e) {
  int l;
  int*p;
  char t[MXFL+1]; // curten should never be set when adding msgword[]:s (got from msglprop); as MXLE+NMSG<=MXFL this never overflows

  l=strlen(s);
  if(l<1) return 0; // is this test needed?
  memcpy(t,s,l);
  if(curten) memcpy(t+l,msgcharICC,NMSG),l+=NMSG; // append tag characters if any
  t[l]=0;
  l=findlight(t,curten,a,e);
  if(l<0) return l;
  if(ntfl>=ctfl) {
    ctfl=ctfl*3/2+500;
    p=realloc(tfl,ctfl*sizeof(int));
    if(!p) return -1;
    tfl=p;
    DEB_FL printf("tfl realloc: %d\n",ctfl);
    }
  tfl[ntfl++]=l;
  return 0;
  }

// Add treated answer (in internal character code) to feasible light list if suitable
// returns !=0 for error
int treatedanswerICC(const char*s) {
  char s0[MXFL+1];
  int j,k,l,u;

  l=strlen(s);
//  DEB_FL printf("treatedanswerICC l=%d\n",l);
  if(l!=lightlength) return 0;
  DEB_FL assert(l>0);
  if(tambaw&&!iswordICC(s)) return 0;
  if(curem&EM_JUM) { // jumbled entry method
    return addlight(s,curans,4); // just store normal entry
    }
  if(curem&EM_FWD) { // forwards entry method
    u=addlight(s,curans,0);if(u) return u;
    }
  if(curem&EM_REV) {
    for(j=0;j<l;j++) s0[j]=s[l-j-1]; // reversed entry method
    s0[j]=0;
    u=addlight(s0,curans,1);if(u) return u;
    }
  if(curem&EM_CYC) { // cyclic permutation
    if(!(l==2&&(curem&EM_REV))) { // not if already covered by reversal
      for(k=1;k<l;k++) {
        for(j=0;j<l;j++) s0[j]=s[(j+k)%l];
        s0[j]=0;
        u=addlight(s0,curans,2);if(u) return u;
        }
      }
    }
  if(curem&EM_RCY) { // reversed cyclic permutation
    if(!(l==2&&(curem&EM_FWD))) { // not if already covered by forwards entry
      for(k=1;k<l;k++) {
        for(j=0;j<l;j++) s0[j]=s[(l-j-1+k)%l];
        s0[j]=0;
        u=addlight(s0,curans,3);if(u) return u;
        }
      }
    }
  return 0;
  }

// as above, but s converted from uchars
int treatedanswerU(const uchar*s) {
  int i,j,u;
  char s0[MXFL+1];
  for(i=0,j=0;s[i]&&j<MXFL;i++) {
    u=uchartoICC(s[i]);
    if(u) s0[j++]=u;
    }
  if(j!=lightlength) return 0;
  s0[j]=0;
  return treatedanswerICC(s0);
  }

// as above, but s converted from UTF-8 (or just 7-bit ASCII): compatible with previous versions
int treatedanswer(const char*s) {
  uchar s0[MXFL+1];
  DEB_TR printf("treatedanswer(\"%s\")\n",s);
  utf8touchars(s0,s,MXFL+1);
  return treatedanswerU(s0);
  }

// returns !=0 on error
static int inittreat(void) {int i,k;
  int c;

  switch(treatmode) {
case 1: // Playfair
      for(i=1;i<MAXICC;i++) psc[i]=-1; // generate code square
      for(i=0;i<25;i++) psq[i]=0;
      for(i=0,k=0;treatmsgICC[0][i]&&k<25;i++) {
        c=treatmsgICC[0][i];
        if(icctouchar[c]=='J'&&uchartoICC('I')!=0) c=uchartoICC('I'); // replace J by I if possible
        if(psc[c]==-1) {psc[c]=k;psq[k]=c;k++;} // character not placed in square yet? add it
        }
      for(i=0;i<niccused&&k<25;i++) { // fill up the rest of the square
        c=iccused[i];
        if(c==0) continue; // not used in alphabet?
        if(icctouchar[c]=='J'&&uchartoICC('I')!=0) continue; // skip J if we have I
        if(psc[c]==-1) {psc[c]=k;psq[k]=c;k++;}
        }
      if(uchartoICC('I')!=0&&uchartoICC('J')!=0) psc[(int)uchartoICC('J')]=psc[(int)uchartoICC('I')];
      DEB_TR for(i=0;i<25;i++) {
        printf("%s ",icctoutf8[(int)psq[i]]);
        if(i%5==4) printf("\n");
        }
      return 0;
case 9: // custom plug-in
      for(i=0;i<NMSG;i++) {
        treatmessage       [i]=treatmsg       [i];
        treatmessageU      [i]=treatmsgU      [i];
        treatmessageICC    [i]=treatmsgICC    [i];
        treatmessageICCAZ  [i]=treatmsgICCAZ  [i];
        treatmessageICCAZ09[i]=treatmsgICCAZ09[i];
        treatmessageUAZ    [i]=treatmsgUAZ    [i];
        treatmessageUAZ09  [i]=treatmsgUAZ09  [i];
        treatmessageAZ     [i]=treatmsgAZ     [i];
        treatmessageAZ09   [i]=treatmsgAZ09   [i];
        }
      break;
default:break;
    }
  return 0;
  }

static void finittreat(void) {}


static void *tpih=0;
static int (*treatf)(const char*)=0;

// returns error string or 0 for OK
char*loadtpi(void) {
  int (*f)(void);
  unloadtpi();
  dlerror(); // clear any existing error
  DEB_TR printf("dlopen(\"%s\")\n",tpifname);
  tpih=dlopen(tpifname,RTLD_LAZY);
  if(!tpih) return dlerror();
  dlerror();
  *(void**)(&f)=dlsym(tpih,"init"); // see man dlopen for the logic behind this
  if(!dlerror()) (*f)(); // initialise the plug-in
  *(void**)(&treatf)=dlsym(tpih,"treat");
  return dlerror();
  }

void unloadtpi(void) {void (*f)();
  if(tpih) {
    dlerror(); // clear any existing error
    *(void**)(&f)=dlsym(tpih,"finit");
    if(!dlerror()) (*f)(); // finalise it before unloading
    dlclose(tpih);
    }
  tpih=0;
  treatf=0;
  }

void reloadtpi(void) {
  char s[300],*p;
  if(treatmode==TREAT_PLUGIN) {
    if((p=loadtpi())) {
      sprintf(s,"Error loading custom plug-in\n%.200s",p);
      reperr(s);
      }
    }
  else unloadtpi();
  }

// offset-encrypt c0 by c1: "group" of result will be same as group of c0
// works entirely on "iccused" indices
static int offsetenc(int c0,int c1) {
  int g0,g1,n,u;
  g0=iccusedtogroup[c0];
  g1=iccusedtogroup[c1];
  if(g0<0||g0>1||g1<0||g1>1) return c0; // don't encode symbols
  u=iccgroupstart[g0];
  n=iccgroupstart[g0+1]-u; // size of group g0
  assert(n>0);
  return u+(c0-u+c1-iccgroupstart[g1])%n;
  }

// returns !=0 on error
// s points to answer to be treated in internal character code
static int treatans(const char*s) {
  int c0,c1,c2,d0,d1,g,i,j,l,l0,l1,o,u;
  char ansutf8[MXFL*8],*p;
  uchar ansU[MXFL+1];
  char t[MXLE+2]; // enough for "insert single character"
  l=strlen(s);
  // printf("treatans("); printICCs(s); printf(")");fflush(stdout);
  for(i=0;s[i];i++) if(s[i]>=MAXICC) return 0;
  switch(treatmode) {
case 0: // no treatment
    return treatedanswerICC(s);
case 1: // Playfair
    if(l!=lightlength) return 0;
    strcpy(t,s);
    for(i=0;i<l-1;i+=2) {
      c0=s[i];c1=s[i+1]; // letter pair to encode
      l0=psc[c0];l1=psc[c1]; // positions of chars in the square
      if(l0==-1||l1==-1) continue; // leave unencoded if either letter is not in square
      if(l0==l1)         continue; // leave double letters (including 'IJ' etc.) unencoded
      if     (l0/5==l1/5) d0=psq[ l0/5     *5+(l0+1)%5],d1=psq[ l1/5     *5+(l1+1)%5]; // same row
      else if(l0%5==l1%5) d0=psq[(l0/5+1)%5*5+ l0   %5],d1=psq[(l1/5+1)%5*5+ l1   %5]; // same col
      else                d0=psq[ l0/5     *5+ l1   %5],d1=psq[ l1/5     *5+ l0   %5]; // rectangle
      if(d0!=0&&d1!=0) t[i]=d0,t[i+1]=d1; // successfully encoded?
      } // if l is odd last character is not encoded
    return treatedanswerICC(t);
case 2: // substitution
    if(l!=lightlength) return 0;
    l0=strlen(treatmsgICC[0]);
    strcpy(t,s);
    for(i=0;s[i];i++) {
      c0=icctousedindex[(int)s[i]];
      if(c0<0) continue;
      g=iccusedtogroup[c0];
      if(g<0||g>1) continue; // only affect letters and digits
      j=c0-iccgroupstart[g];
      if(j>=l0) continue;
      t[i]=treatmsgICC[0][j];
      }
    return treatedanswerICC(t);
case 3: // fixed Caesar/Vigenère
    if(l!=lightlength) return 0;
    l0=strlen(treatmsgICC[0]);
    if(l0==0) return treatedanswerICC(s); // no keyword, so leave as plaintext
    if(niccused<1) return treatedanswerICC(s); // prevent divide-by-0
    strcpy(t,s);
    for(i=0;s[i];i++) {
      c0=icctousedindex[(int)s[i]];
      c1=icctousedindex[(int)treatmsgICC[0][i%l0]];
      if(c0>=0&&c1>=0) t[i]=iccused[offsetenc(c0,c1)];
      }
    return treatedanswerICC(t);
case 4: // variable Caesar
    if(l!=lightlength) return 0;
    if(niccused<1) return treatedanswerICC(s); // prevent divide-by-0
    if(treatorder[0]==0) { // for backwards compatibility
      l0=strlen(treatmsgICC[0]);
      if(l0==0) return treatedanswerICC(s); // no keyword, so leave as plaintext
      o=treatmsgICC[0][clueorderindex%l0];
    } else {
      o=msgcharICC[0];
      if(o==ICC_DASH) return treatedanswerICC(s); // leave as plaintext
      }
    c1=icctousedindex[o];
    if(c1<0) return treatedanswerICC(s);
    strcpy(t,s);
    for(i=0;s[i];i++) {
      c0=icctousedindex[(int)s[i]];
      if(c0>=0) t[i]=iccused[offsetenc(c0,c1)];
      }
    return treatedanswerICC(t);
case 10: // misprint, correct letters specified
    if(l!=lightlength) return 0;
    c0=msgcharICC[0];
    if(c0==ICC_DASH) return treatedanswerICC(s); // unmisprinted
    c1=0;
    goto misp0;
case 11: // misprint, misprinted letters specified
    if(l!=lightlength) return 0;
    c1=msgcharICC[0];
    if(c1==ICC_DASH) return treatedanswerICC(s); // unmisprinted
    c0=0;
    goto misp0;
case 5: // misprint
    if(l!=lightlength) return 0;
    l0=ucharslen(treatmsgU[0]);
    l1=ucharslen(treatmsgU[1]);
    c0=0; if(clueorderindex<l0) c0=uchartoICC(treatmsgU[0][clueorderindex]); // will be left at 0 if not a recognised character
    c1=0; if(clueorderindex<l1) c1=uchartoICC(treatmsgU[1][clueorderindex]);
    if(c0==0&&c1==0) return treatedanswerICC(s); // allowing this case would slow things down too much for now
misp0: // here we want to misprint c0 as c1, where 0 indicates any character
    strcpy(t,s);
    for(i=0;s[i];i++) if(c0==0||s[i]==c0) {
      if(c1==0) {
        for(j=0;j<niccused;j++) {
          c2=iccused[j];
          if(s[i]==c2) continue; // not a *mis*print
          t[i]=c2;
          u=treatedanswerICC(t); if(u) return u;
          t[i]=s[i]; // restore modified character
          }
      } else {
        if(c0==0&&s[i]==c1) continue; // not a *mis*print unless specifically instructed otherwise
        t[i]=c1;
        u=treatedanswerICC(t); if(u) return u;
        t[i]=s[i]; // restore modified character
        if(c0==c1) break; // only one entry for the `misprint as self' case
        }
      }
    return 0;
case 6: // delete single occurrence
    c0=msgcharICC[0];
    if(c0==ICC_DASH) {
      if(l==lightlength) return treatedanswerICC(s);
      return 0;
      }
    if(l!=lightlength+1) return 0;
    for(i=0;s[i];i++) if(s[i]==c0) {
      for(j=0;j<i;j++) t[j]=s[j];
      for(;s[j+1];j++) t[j]=s[j+1];
      t[j]=0;
      u=treatedanswerICC(t); if(u) return u;
      while(s[i+1]==c0) i++; // skip duplicate outputs
      }
    return 0;
case 7: // delete all occurrences (letters latent)
    c0=msgcharICC[0];
    if(c0==ICC_DASH) {
      if(l==lightlength) return treatedanswerICC(s);
      return 0;
      }
    if(l<=lightlength) return 0;
    for(i=0,j=0;s[i];i++) if(s[i]!=c0) t[j++]=s[i];
    t[j]=0;
    if(j!=lightlength) return 0; // not necessary, but improves speed slightly
    return treatedanswerICC(t);
case 8: // insert single character
    c0=msgcharICC[0];
    if(c0==ICC_DASH) {
      if(l==lightlength) return treatedanswerICC(s);
      return 0;
      }
    if(l!=lightlength-1) return 0;
    for(i=0;i<=l;i++) { // try inserting c0 before position i
      for(j=0;j<i;j++) t[j]=s[j];
      t[j++]=c0;
      for(;s[j-1];j++) t[j]=s[j-1];
      t[j]=0;
      u=treatedanswerICC(t); if(u) return u;
      while(s[i]==c0) i++; // skip duplicate outputs
      }
    return 0;
case 9: // custom plug-in
    answerICC=s;
    for(i=0,p=ansutf8;s[i];i++) strcpy(p,icctoutf8[(int)s[i]]),p+=strlen(p),ansU[i]=icctouchar[(int)s[i]];
    ansU[i]=0;
    answerU=ansU;
    DEB_TR printf("ansutf8=>%s<\n",ansutf8);
    if(treatf) return (*treatf)(ansutf8);
    return 1;
default:break;
    }
  return 0;
  }

int pregetinitflist(void) {
  int i;
  struct memblk*p;
  while(lstrings) {p=lstrings->next;free(lstrings);lstrings=p;} lmp=0; lml=MEMBLK;
  FREEX(tfl);ctfl=0;ntfl=0;
  FREEX(lts);clts=0;ltotal=0;ultotal=0;
  for(i=0;i<HTABSZ;i++) hstab[i]=-1,haestab[i]=-1;
  if(inittreat()) return 1;
  return 0;
  }

// returns !=0 for error
int postgetinitflist(void) {
  finittreat();
  FREEX(tfl);ctfl=0;
  return 0;
  }

// construct an initial list of feasible lights for a given length and set of light properties
// caller's responsibility to free(*l)
// returns !=0 on error; -5 on abort
int getinitflist(int**l,int*ll,struct lprop*lp,int llen) {
  int g,i,j,u;
  ABM mfl[NMSG],ml[NMSG],b;

  ntfl=0;
  curdm=lp->dmask,curem=lp->emask,curten=lp->ten;
  for(i=0;i<NMSG;i++) if(curdm&(1<<(MAXNDICTS+i))) { // "special" word for message spreading/jumble?
    DEB_FL {
      printf("msgword[%d]=<",i);
      printICCs(msgword[i]);
      printf(">\n");
      }
    u=addlight(msgword[i],-1-i,0);
    if(u) return u;
    goto ex0;
    }
  if((curem&EM_ALL)==0) curem=EM_FWD; // force normal entry to be allowed if all are disabled
  lightlength=llen;
  DEB_FL printf("getinitflist(%p) llen=%d dmask=%08x emask=%08x ten=%d:\n",(void*)lp,llen,curdm,curem,curten);
  memset(mfl,0,sizeof(mfl));
  for(i=0;i<NMSG;i++) {
    if(clueorderindex<(int)strlen(treatmsg       [i])) msgchar       [i]=treatmsg       [i][clueorderindex]; else  msgchar       [i]='-';  // we set these up even if treatment is not enabled
    if(clueorderindex<  ucharslen(treatmsgU      [i])) msgcharU      [i]=treatmsgU      [i][clueorderindex]; else  msgcharU      [i]='-';
    if(clueorderindex<  ucharslen(treatmsgUAZ    [i])) msgcharUAZ    [i]=treatmsgUAZ    [i][clueorderindex]; else  msgcharUAZ    [i]='-';
    if(clueorderindex<  ucharslen(treatmsgUAZ09  [i])) msgcharUAZ09  [i]=treatmsgUAZ09  [i][clueorderindex]; else  msgcharUAZ09  [i]='-';
    if(clueorderindex<(int)strlen(treatmsgICC    [i])) msgcharICC    [i]=treatmsgICC    [i][clueorderindex]; else  msgcharICC    [i]=ICC_DASH;
    if(clueorderindex<(int)strlen(treatmsgICCAZ  [i])) msgcharICCAZ  [i]=treatmsgICCAZ  [i][clueorderindex]; else  msgcharICCAZ  [i]=ICC_DASH;
    if(clueorderindex<(int)strlen(treatmsgICCAZ09[i])) msgcharICCAZ09[i]=treatmsgICCAZ09[i][clueorderindex]; else  msgcharICCAZ09[i]=ICC_DASH;
    if(clueorderindex<(int)strlen(treatmsgAZ     [i])) msgcharAZ     [i]=treatmsgAZ     [i][clueorderindex]; else  msgcharAZ     [i]='-';
    if(clueorderindex<(int)strlen(treatmsgAZ09   [i])) msgcharAZ09   [i]=treatmsgAZ09   [i][clueorderindex]; else  msgcharAZ09   [i]='-';
    if(curten&&treatorder[i]>0) { // using discretionary mode - i.e., potentially shuffling treatment message?
      for(j=0;treatmsgICC[i][j];j++) mfl[i]|=ICCTOABM((int)treatmsgICC[i][j]); // bitmap of all letters present in message
      if(ntw>(int)strlen(treatmsgICC[i])) mfl[i]|=ABM_DASH; // add in "-" if message not long enough
      if(clueorderindex<MXFL) mfl[i]&=treatcstr[i][clueorderindex]; // apply constraints
      ml[i]=mfl[i]&~(mfl[i]-1); // find bottom set bit to initialise "counter"
      if(ml[i]==0) goto ex0; // no possibilities: exit with empty list
      }
    }
  for(;;) { // loop over all combinations of msgchar:s
    for(i=0;i<NMSG;i++) if(curten&&treatorder[i]>0) {
      j=abmtoicc(ml[i]); // extract msgchar:s from counter if in discretionary mode (overwriting previous values of msgcharICC and friends)
      u=icctouchar[j];
      msgcharICC[i]=j;
      msgcharU[i]=u;
      g=icctogroup[j];
      if((g==0||g==1)&&u<128) msgcharAZ09[i]=u;   // alphanumeric and 7-bit clean?
      else                    msgcharAZ09[i]='-'; // otherwise
      }
DEB_FL { printf("  building list with msgcharICC[]="); for(i=0;i<NMSG;i++) printf("%s",icctoutf8[(int)msgcharICC[i]]); printf("\n"); }
    for(i=0;i<atotal;i++) {
      curans=i;
      if((curdm&ansp[curans]->dmask)==0) continue; // not in a valid dictionary
      if(ansp[curans]->banned) continue;
      if(curten) u=treatans(ansp[curans]->ul);
      else       u=treatedanswerICC(ansp[curans]->ul);
      if(u) return u;
      if(abort_flag) return -5;
      }
    for(i=0;i<NMSG;i++) if(curten&&treatorder[i]>0) { // this increments the NMSG-digit "counter" in ml[i] where valid digits are the set bits in mfl[i]
      b=mfl[i]&~(ml[i]|(ml[i]-1)); // clear bits mf[] and below
      b&=~(b-1); // find new bottom set bit
      if(b) {ml[i]=b; break;} // try next feasible character
      ml[i]=mfl[i]&~(mfl[i]-1); // reset to bottom set bit and proceed to advance next character
      }
    if(i==NMSG) break; // finish when all combinations done
    }
ex0:
  *l=malloc(ntfl*sizeof(int)+1); // ensure we don't execute malloc(0)
  if(*l==0) return 1;
  memcpy(*l,tfl,ntfl*sizeof(int));
  *ll=ntfl;
  DEB_FL printf("%d entries\n",ntfl);
  return 0;
  }
