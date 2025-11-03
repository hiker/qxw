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


#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <wctype.h>
#include <time.h>
#include <math.h>

// OS-SPECIFIC

#ifdef _WIN32
  #include <direct.h>
  #include <shlobj.h>
  #include <windows.h>
  #define _USE_MATH_DEFINES
  #include <math.h>
  #define DIR_SEP_STR "\\"
  #define DIR_SEP_CHAR '\\'
  #define STRCASECMP _stricmp
#else
  #define DIR_SEP_STR "/"
  #define DIR_SEP_CHAR '/'
  #define STRCASECMP strcasecmp
#endif

#define RELEASE "20200708"

// GENERAL

#define NGTYPE 13                    // number of different grid types
#define MAXNDIR 3                    // maximum number of directions
#define MXSZ 63                      // max grid size X and Y
#define MXCL 250                     // max number of cells in light; must be >=MXSZ*2; allow more for VL:s
#define MXCT 31                      // max contents in one cell per direction
#define MXMUX MXCT                   // maximum degree of multiplexing
#define NMSG 2                       // number of messages passed to treatment code
#define MXLE 250                     // max entries in a light before tags added
#define MXFL 255                     // max entries in a word that filler can cope with: light plus tags, >=MXLE+NMSG; <256 to allow poscnt[] not to overflow
#define MXLT (MAXNDIR*MXSZ*MXSZ+NVL) // max number of lights (conservative estimate)
#define NVL (MXSZ*4)                 // max number of virtual lights
#define MAXNMK (MAXNDIR*2)           // max number of corner marks
#define MXMK 30                      // max length of mark string

// note that a light (in a Mobius grid with all squares full) can contain up to MXSZ*2*MXCT entries, which is >MXLE; and that
// if filler discretion is enabled, the number of treated lights must be <=MXFL, otherwise only limited by MXLT
// MXFL<=255 to prevent unsigned char histograms overflowing

#define NLEM 5 // number of light entry methods
#define NATREAT 12 // number of treatments
#define TREAT_PLUGIN 9

#define UNDOS 50

// MISC

#define STRSTARTS(s,t) (!strncmp((s),(t),strlen(t)))

#define MEMBLK 100000     // unit of memory allocation

struct memblk {
  struct memblk*next;
  int ct;
  char s[MEMBLK];
  };

#define PI (M_PI)
#define ODD(x) ((x)&1)
#define EVEN(x) (!((x)&1))
#define FREEX(p) if(p) {free(p);p=0;}
#define SLEN 1000      // maximum length for filenames, strings etc.; Windows has MAX_PATH around 260; should be >>MXFL
#define LEMDESCLEN 20  // maximum length of entry method description

#define MX(a,b) ((a)>(b)?(a):(b))

extern int debug;
#define DEB_F0 if(debug&0x00000001) // filler: start and stop
#define DEB_F1 if(debug&0x00000002) // filler: basics
#define DEB_F2 if(debug&0x00000004) // more detail
#define DEB_F3 if(debug&0x00000008) // spreads and jumbles
#define DEB_GU if(debug&0x00000010) // general GUI
#define DEB_RF if(debug&0x00000020) // refresh, events
#define DEB_DI if(debug&0x00000040) // dictionaries
#define DEB_DV if(debug&0x00000080) // dictionaries, verbose
#define DEB_AL if(debug&0x00000100) // alphabets
#define DEB_FL if(debug&0x00000200) // feasible list building
#define DEB_TR if(debug&0x00000400) // treatments and plug-ins
#define DEB_GR if(debug&0x00000800) // grid operations, save, load
#define DEB_BS if(debug&0x00001000) // buildstructs
#define DEB_DE if(debug&0x00002000) // decks
#define DEB_EX if(debug&0x00004000) // export
// bit 31 is used to force printing of version information etc. without other output

// ALPHABETS

typedef unsigned int uchar; // to hold UTF-32 character; must be unsigned
#define MAXUCHAR 0x30000    // maximum Unicode character accepted: first three planes only
#define MAXEQUIV 100        // maximum equivalent unicode charaacters mapping to given internal code
#define MAXICC 61           // internal character codes from 1..MAXICC-1 are allowed for normal characters; 0 reserved for 0-termination; code MAXICC used for "dash" in treatments (to mean "no treatment")
#define ICC_DASH MAXICC     // internal character code for dash

#define TOUUPPER(c) g_unichar_toupper(c)
#define TOULOWER(c) g_unichar_tolower(c)
#define ISUUPPER(c) g_unichar_isupper(c)
#define ISULOWER(c) g_unichar_islower(c)
#define ISUDIGIT(c) g_unichar_isdigit(c)
#define ISUALPHA(c) g_unichar_isalpha(c)
#define ISUPRINT(c) g_unichar_isprint(c)
#define ISUGRAPH(c) g_unichar_isgraph(c)

struct alphaentry {
  char*rep; // representative as UTF-8
  char*eqv;  // UTF-8 string of equivalents (need not include rep)
  int vow;   // is it a vowel?
  int con;   // is it a consonant?
  int seq;   // forms sequence with next?
  };

#define ICCTOABM(x) (1ULL<<((x)-1))
typedef unsigned long long int ABM; // alphabet bitmap type
#define ABM_NRM ((1ULL<<(MAXICC-1))-1) // normal characters
#define ABM_DASH (1ULL<<(ICC_DASH-1))
#define ABM_ALL (ABM_NRM|ABM_DASH) // normal characters plus dash

// Terminology:
// A `square' is the quantum of area in the grid; one or more squares (a `merge group') form
// an `entry', which is a single enclosed white area in the grid where a letter (or group of letters) will appear.
// A `word' is a sequence of entries to be filled.
// bldstructs() constructs the words and entries from the square data produced
// by the editor.

struct sprop { // square properties
  unsigned int bgcol; // background colour
  unsigned int fgcol; // foreground colour
  unsigned int mkcol; // mark colour
  unsigned char ten;  // treatment enable: flag to plug-in
  unsigned char spor; // global square property override (not used in dsp)
  unsigned char fstyle; // font style: b0=bold, b1=italic
  unsigned char dech; // dechecked: 0=normal, 1=contributions stacked atop one another, 2=contributions side-by-side
  char mk[MAXNMK][MXMK+1]; // square corner mark strings in each direction
  };

// entry methods
#define EM_FWD 1
#define EM_REV 2
#define EM_CYC 4
#define EM_RCY 8
#define EM_JUM 16
#define EM_ALL 31
#define EM_SPR 32 // for internal use only

struct lprop { // light properties
  unsigned int dmask; // mask of allowed dictionaries; special values 1<<MAXNDICTS and above for "implicit" words
  unsigned int emask; // mask of allowed entry methods
  unsigned char ten;  // treatment enable
  unsigned char lpor; // global light property override (not used in dlp)
  unsigned char dnran; // does not receive a number
  unsigned char mux; // is multiplexed
  };

struct jdata { // per-feasible-dictionary-entry information relating to words with jumbled entry mode
  int nuf; // total number of unforced letters
  unsigned char ufhist[MAXICC+1]; // histogram of unforced letters
  unsigned char poscnt[MAXICC+1]; // number of positions where each unforced letter can go
  };

struct sdata { // per-feasible-dictionary-entry information relating to words with spread entry mode
  ABM flbm[MXFL]; // feasible letter bitmaps // could economise on storage here as done for jdata, but spreading only used with one-word dictionaries for now
  double ct[MXFL][MXFL]; // [i][j] is the number of ways char i in string goes through entry slot j in word // must economise on storage if normal dictionaries are ever used
  double ctd[MXFL]; // [j] is number of ways '-' goes through entry slot j in word
  };

struct word {
  int nent; // number of entries in word, <=MXFL
  int wlen; // length of strings in feasible list (<=nent in spread entry case)
  int jlen; // number of entries subject to jumble (<nent if tagged)
  int gx0,gy0; // start position (not necessarily mergerep)
  int ldir;
  int*flist; // start of feasible list
  int flistlen; // length of feasible list
  struct jdata*jdata;
  ABM*jflbm;
  struct sdata*sdata;
  int commitdep; // depth at which this word committed during fill, or -1 if not committed
  struct entry*e[MXFL]; // list of nent entries making up this word
  struct lprop*lp; // applicable properties for this word
  unsigned char upd; // updated flag
  int fe; // fully-entered flag
  int goi[MXLE]; // grid order indices for each entry
  };
struct entry {
  ABM flbm; // feasible letter bitmap
  ABM flbmh; // copy of flbm provided by solver to running display
  int gx,gy; // corresponding grid position (indices to gsq) of representative
  int checking; // count of intersecting words
  double score[MAXICC+1]; // includes dash character
  double crux; // priority
  unsigned char sel; // selected flag
  unsigned char upd; // updated flag
  };
struct square {
// source grid information (saved in undo buffers)
  unsigned int bars; // bar presence in each direction
  unsigned int merge; // merge flags in each direction
  unsigned char fl; // flags: b0=blocked, b3=not part of grid (for irregular shapes); b4=cell selected
  unsigned char dsel; // one light-selected flag for each direction
  int ctlen[MAXNDIR]; // length of contents in each direction
  ABM ctbm[MAXNDIR][MXCT]; // square contents bitmap strings in each direction
  struct sprop sp;
  struct lprop lp[MAXNDIR];
// derived information from here on
  struct entry*ents[MAXNDIR][MXCT]; // entries corresponding 1:1 to ctbm:s
  struct word*w[MAXNDIR][MXMUX];
  unsigned int vflags[MAXNDIR]; // violation flags: b0=double unch, b1=triple+ unch, b2=underchecked, b3=overchecked
  int number; // number in square (-1 for no number)
  int goi; // grid order index of treated squares (-1 in untreated squares)
  };
struct vl { // virtual light
  int l; // number of constituent entries, <=MXCL
  int x[MXCL],y[MXCL]; // constituent entry coords
  struct lprop lp; // properties
  unsigned char sel; // is selected?
  struct word*w;
  };

extern struct word*words;
extern struct entry*entries;
extern struct entry*treatmsge0[NMSG][MXFL]; // to help passing info from filler to constraints
extern struct square gsq[MXSZ][MXSZ];
extern struct vl vls[NVL];
extern int nc,nw,nw0,ntw,ne,ne0,ns,nvl;

extern int cwperm[MAXICC]; // "codeword" permutation

// DICTIONARY

struct answer { // a word found in one or more dictionaries
  int ahlink; // hash table link for isword()
  unsigned int dmask; // mask of dictionaries where word found
  unsigned int cfdmask; // mask of dictionaries where word found with this citation form
//  int light[NLEM]; // light indices of treated versions
  double score;
  char*cf; // citation form of the word in dstrings
  struct answer*acf; // alternative citation form (linked list)
  char*ul; // untreated light in dstrings: ansp is uniquified by this; in internal character code
  char banned; // 0:not banned; 1:banned
  };

struct light { // a string that can appear in the grid, the result of treating an answer; not uniquified
  int hashslink; // hash table (s only) linked list
  int hashaeslink; // hash table (a,e,s) linked list
  int ans; // answer giving rise to this light; negative to represent message words
  int em; // mode of entry giving rise to this light
  char*s; // the light in dstrings, containing only chars in alphabet
  int uniq; // uniquifying number (by string s only), used as index into lused[]
  int tagged; // does s include NMSG tag characters?
  ABM lbm; // bitmap of letters used
  unsigned char hist[MAXICC+1]; // letter histogram, indexed by internal character code
  };

// FILLER

extern int*llist;               // buffer for word index list
extern int*llistp;              // ptr to matching word indices
extern int llistn;              // number of matching words
extern int llistwlen;           // word length applicable to matching lights
extern unsigned int llistdm;    // dictionary mask applicable to matching lights
extern int llistem;             // entry method mask applicable to matching lights

extern int ifamode;             // interactive fill assist mode: 0=none; 1=current light only; 2=full grid
extern volatile int abort_flag; // abort word list building?

extern struct answer**ansp;
extern struct light*lts;

extern int atotal;              // total answers in dict
extern int ultotal;             // total unique lights in dict

extern char tpifname[SLEN];
extern int treatmode; // 0=none, TREAT_PLUGIN=custom plug-in
extern int treatorder[NMSG]; // 0=first come first served, 1=spread, 2=jumble
extern char treatmsg[NMSG][MXLT*8+1];
extern ABM treatcstr[NMSG][MXFL];
extern int tambaw; // treated answer must be a word

extern uchar treatmsgU      [NMSG][MXLT+1]; // converted to uchars
extern char  treatmsgICC    [NMSG][MXLT+1]; // converted to ICCs
extern char  treatmsgICCAZ  [NMSG][MXLT+1]; // ICCs, alphabetic only
extern char  treatmsgICCAZ09[NMSG][MXLT+1]; // ... plus digits
extern uchar treatmsgUAZ    [NMSG][MXLT+1]; // converted to uchars, alphabetic, mapped to entries
extern uchar treatmsgUAZ09  [NMSG][MXLT+1]; // ... plus digits

extern char  treatmsgAZ     [NMSG][MXLT+1]; // 7-bit clean *no transliteration*
extern char  treatmsgAZ09   [NMSG][MXLT+1];

extern char msgword[NMSG][MXFL+1]; // for "one-word dictionary" created for tagging; in internal character code

extern int clueorderindex;
extern int gridorderindex[MXLE];
extern int checking[MXLE];
extern int lightx;
extern int lighty;
extern int lightdir;

// PREFERENCES

#define NLOOKUP 6
#define NPREFS (14+NLOOKUP*2)
extern int prefdata[NPREFS];
extern char prefstring[NPREFS][SLEN+1];
#define clickblock (prefdata[0])
#define clickbar (prefdata[1])
#define shownums (prefdata[2])
#define mincheck (prefdata[3])
#define maxcheck (prefdata[4])
#define afunique (prefdata[5])
#define afrandom (prefdata[6])
#define eptsq (prefdata[7])
#define hpxsq (prefdata[8])
#define lnis (prefdata[9])
#define lookupname(i) (prefstring[10+(i)])
#define lookup(i) (prefstring[10+NLOOKUP+(i)])
#define startup_dict1 (prefstring[10+NLOOKUP*2])
#define startup_ff1 (prefstring[10+NLOOKUP*2+1])
#define startup_af1 (prefstring[10+NLOOKUP*2+2])
#define startup_al (prefdata[10+NLOOKUP*2+3])

#endif
