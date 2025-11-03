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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#ifdef _WIN32
	#include <Windows.h>
	#include <stringapiset.h>
	#include <io.h>
	#include "pfgetopt.h"
#else
	#include <unistd.h>
	#include <pwd.h>
#endif
#include <wchar.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "common.h"
#include "qxw.h"
#include "filler.h"
#include "dicts.h"
#include "treatment.h"
#include "gui.h"
#include "draw.h"
#include "deck.h"
#include "alphabets.h"

// GLOBAL PARAMETERS

// these are saved:

int width=12,height=12;          // grid size
int gtype=0;                     // grid type: 0=square, 1=hexH, 2=hexV, 3=circleA (edge at 12 o'clock), 4=circleB (cell at 12 o'clock)
                                 // 5=cylinder L-R, 6=cylinder T-B, 7=Moebius L-R, 8=Moebius T-B, 9=torus
int symmr=2,symmm=0,symmd=0;     // symmetry mode flags (rotation, mirror, up-and-down/left-and-right)
int nvl=0;                       // number of virtual lights
struct sprop dsp;                // default square properties
struct lprop dlp;                // default light properties

char gtitle[SLEN]="";
char gauthor[SLEN]="";

// these are not saved:
int debug=0;                     // debug level
int usegui=1;                    // 0=use text console; 1=use GUI
unsigned int fseed;              // force filler srand() argument if non-zero

char filename[SLEN+50]; // result from filedia()
char filenamebase[SLEN]; // base to use for constructing default filenames
int havesavefn; // do we have a validated filename to save to?

int curx=0,cury=0,curdir=0,curmux=0;  // cursor position, direction and multiplex
int curent=-1;                        // entry pointed to by cursor (set by bldstructs(); -1 if not set)
int curword=-1;                       // word pointed to by cursor (set by bldstructs(); -1 if not set)
int unsaved=0;                        // edited-since-save flag
int cwperm[MAXICC];

int ndir[NGTYPE]  ={2,3,3,2,2, 2,2,2,2,2,2,2,2};  // number of directions per grid type
int gshape[NGTYPE]={0,1,2,3,4, 0,0,0,0,0,0,0,0};  // basic shape of grid
int dhflip[NGTYPE][MAXNDIR*2]={
{2,1,0,3},
{4,3,2,1,0,5},
{3,2,1,0,5,4},
{2,1,0,3},
{2,1,0,3},
{2,1,0,3},
{2,1,0,3},
{2,1,0,3},
{2,1,0,3},
{2,1,0,3},
{2,1,0,3},
{2,1,0,3},
{2,1,0,3}
};
int dvflip[NGTYPE][MAXNDIR*2]={
{0,3,2,1},
{1,0,5,4,3,2},
{0,5,4,3,2,1},
{2,1,0,3},
{2,1,0,3},
{0,3,2,1},
{0,3,2,1},
{0,3,2,1},
{0,3,2,1},
{0,3,2,1},
{0,3,2,1},
{0,3,2,1},
{0,3,2,1}
};

static unsigned char log2lut[65536];

int cbits(ABM x) {ABM i; int j; for(i=1,j=0;i<ABM_ALL;i+=i) if(x&i) j++; return j;} // count set bits
int onebit(ABM x) {return x!=0&&(x&(x-1))==0;}
int logbase2(ABM x) {int u;    // find lowest set bit
  u= x     &0xffff; if(u) return log2lut[u];
  u=(x>>16)&0xffff; if(u) return log2lut[u]+16;
  u=(x>>32)&0xffff; if(u) return log2lut[u]+32;
  u=(x>>48)&0xffff; if(u) return log2lut[u]+48;
  return -1;
  }

int*llistp=0;    // ptr to matching lights
int llistn=0;    // number of matching lights
unsigned int llistdm=0;   // dictionary mask applicable to matching lights
int llistwlen=0; // word length (less tags if any) applicable to matching lights
int llistem=0;   // entry method mask applicable to matching lights
int*llist=0;     // buffer for light index list

int ifamode=2;                   // interactive fill assist mode

// GRID

struct square gsq[MXSZ][MXSZ];
struct vl vls[NVL];

int getnumber(int x,int y) {
  return gsq[x][y].number;
  }

int getflags(int x,int y) {int xr,yr;
  getmergerep(&xr,&yr,x,y);
  return gsq[xr][yr].fl;
  }

int getbgcol(int x,int y) {int xr,yr;
  getmergerep(&xr,&yr,x,y);
  return (gsq[xr][yr].sp.spor?gsq[xr][yr].sp.bgcol:dsp.bgcol)&0xffffff;
  }

int getfgcol(int x,int y) {int xr,yr;
  getmergerep(&xr,&yr,x,y);
  return (gsq[xr][yr].sp.spor?gsq[xr][yr].sp.fgcol:dsp.fgcol)&0xffffff;
  }

int getmkcol(int x,int y) {int xr,yr;
  getmergerep(&xr,&yr,x,y);
  return (gsq[xr][yr].sp.spor?gsq[xr][yr].sp.mkcol:dsp.mkcol)&0xffffff;
  }

int getfstyle(int x,int y) {int xr,yr;
  getmergerep(&xr,&yr,x,y);
  return gsq[xr][yr].sp.spor?gsq[xr][yr].sp.fstyle:dsp.fstyle;
  }

int getdech(int x,int y) {int xr,yr;
  getmergerep(&xr,&yr,x,y);
  return gsq[xr][yr].sp.spor?gsq[xr][yr].sp.dech:dsp.dech;
  }

int getctlen(int x,int y,int d) {int xr,yr;
  getmergerep(&xr,&yr,x,y);
  if((gsq[xr][yr].sp.spor?gsq[xr][yr].sp.dech:dsp.dech)==0) d=0;
  return gsq[xr][yr].ctlen[d];
  }

struct lprop*getvlprop(int d) { return vls[d].lp.lpor?&vls[d].lp:&dlp; }
struct lprop*getlprop(int x,int y,int d) {
  if(d>=100) return getvlprop(d-100);
  return gsq[x][y].lp[d].lpor?&gsq[x][y].lp[d]:&dlp;
  }

// returns 0 for error
int getmaxmux(int x,int y,int d) {
  int tx[MXCL],ty[MXCL],i,l,m,u;
  if(!ismux(x,y,d)) return 1;
  m=1;
  l=getlight(tx,ty,x,y,d);
  if(l<1) return 0;
  for(i=0;i<l;i++) {
    u=getctlen(tx[i],ty[i],d);
    if(u>m) m=u;
    }
  return m;
  }

void getmk(char*s,int x,int y,int c) {int d,nd,nd2,xr,yr; char*t;
  strcpy(s,"");
  getmergerep(&xr,&yr,x,y);
  t=gsq[xr][yr].sp.spor?gsq[xr][yr].sp.mk[c]:dsp.mk[c];
  if(!strcmp(t,"\\#")) goto ew0; // if number, put it wherever necessary
  if(!strcmp(t,"\\o")) { // if circle, only do in first cell of group
    if(x!=xr||y!=yr) return;
    goto ew0;
    }
  d=getmergedir(x,y); // otherwise only in extreme corners of merge group
  nd=ndir[gtype];
  nd2=nd+nd;
  if(d>=0) {
    if(!ismerge(x,y,d   )&&(c-1-d+nd2)%nd2< nd) goto ew0;  // end of group   d=0:1,2/d=1:2,3
    if(!ismerge(x,y,d+nd)&&(c-1-d+nd2)%nd2>=nd) goto ew0;
    else return;
    }
ew0:
  strcpy(s,t);
  }

// move backwards one cell in direction d
void stepback(int*x,int*y,int d) {
//  printf("stepback(%d,%d,%d)\n",*x,*y,d);
  if(d>=ndir[gtype]) {stepforw(x,y,d-ndir[gtype]); return;}
  if(gtype==1) {
    switch(d) {
      case 0: if(((*x)&1)==1) (*y)++; (*x)--; break;
      case 1: if(((*x)&1)==0) (*y)--; (*x)--; break;
      case 2: (*y)--;break;
        }
    return;
    }
  if(gtype==2) {
    switch(d) {
      case 2: if(((*y)&1)==1) (*x)++; (*y)--; break;
      case 1: if(((*y)&1)==0) (*x)--; (*y)--; break;
      case 0: (*x)--;break;
        }
    return;
    }
  // rectangular grid cases
  if(d) (*y)--; else (*x)--;
  if(gtype==3||gtype==4||gtype==5||gtype==7||gtype==9||gtype==10||gtype==11||gtype==12) { // loop in x direction
    if(*x<0) {
      *x+=width;
      if(gtype==7||gtype==10||gtype==12) *y=height-1-*y;
      }
    }
  if(gtype==6||gtype==8||gtype==9||gtype==10||gtype==11||gtype==12) { // loop in y direction
    if(*y<0) {
      *y+=height;
      if(gtype==8||gtype==11||gtype==12) *x=width-1-*x;
      }
    }
//  printf("=(%d,%d)\n",*x,*y);
  }

// move forwards one cell in direction d
void stepforw(int*x,int*y,int d) {
//  printf("stepforw(%d,%d,%d)\n",*x,*y,d);
  if(d>=ndir[gtype]) {stepback(x,y,d-ndir[gtype]); return;}
  if(gtype==1) {
    switch(d) {
      case 0: (*x)++; if(((*x)&1)==1) (*y)--; break;
      case 1: (*x)++; if(((*x)&1)==0) (*y)++; break;
      case 2: (*y)++;break;
        }
    return;
    }
  if(gtype==2) {
    switch(d) {
      case 2: (*y)++; if(((*y)&1)==1) (*x)--; break;
      case 1: (*y)++; if(((*y)&1)==0) (*x)++; break;
      case 0: (*x)++;break;
        }
    return;
    }
  // rectangular grid cases
  if(d) (*y)++; else (*x)++;
  if(gtype==3||gtype==4||gtype==5||gtype==7||gtype==9||gtype==10||gtype==11||gtype==12) { // loop in x direction
    if(*x>=width) {
      *x-=width;
      if(gtype==7||gtype==10||gtype==12) *y=height-1-*y;
      }
    }
  if(gtype==6||gtype==8||gtype==9||gtype==10||gtype==11||gtype==12) { // loop in y direction
    if(*y>=height) {
      *y-=height;
      if(gtype==8||gtype==11||gtype==12) *x=width-1-*x;
      }
    }
//  printf("=(%d,%d)\n",*x,*y);
  }

int isingrid(int x,int y) {
  if(x<0||y<0||x>=width||y>=height) return 0;
  if(gshape[gtype]==1&&ODD(width )&&ODD(x)&&y==height-1) return 0;
  if(gshape[gtype]==2&&ODD(height)&&ODD(y)&&x==width -1) return 0;
  return 1;
  }

int isclear(int x,int y) {
  if(!isingrid(x,y)) return 0;
  if(gsq[x][y].fl&0x09) return 0;
  return 1;
  }

int isbar(int x,int y,int d) {int u;
  if(d>=ndir[gtype]) {d-=ndir[gtype];stepback(&x,&y,d);}
  if(!isingrid(x,y)) return 0;
  u=(gsq[x][y].bars>>d)&1;
//  DEB_GR printf("  x=%d y=%d d=%d u=%d g[x,y].bars=%08x\n",x,y,d,u,gsq[x][y].bars);
  stepforw(&x,&y,d);
  if(!isingrid(x,y)) return 0;
  return u;
  }

int ismerge(int x,int y,int d) {int u;
  if(d>=ndir[gtype]) {d-=ndir[gtype];stepback(&x,&y,d);}
  if(!isingrid(x,y)) return 0;
  u=(gsq[x][y].merge>>d)&1;
  stepforw(&x,&y,d);
  if(!isingrid(x,y)) return 0;
  return u;
  }

int sqexists(int i,int j) { // is a square within the grid (and not cut out)?
  if(!isingrid(i,j)) return 0;
  return !(gsq[i][j].fl&8);
  }

// is a step backwards clear? (not obstructed by a bar, block, cutout or edge of grid)
int clearbefore(int x,int y,int d) {int tx,ty;
  tx=x;ty=y;stepback(&tx,&ty,d);
  if(!isingrid(tx,ty)) return 0;
  if(!isclear(tx,ty)) return 0;
  if(isbar(tx,ty,d)) return 0;
  return 1;
  }

// is a step forwards clear?
int clearafter(int x,int y,int d) {int tx,ty;
  tx=x;ty=y;stepforw(&tx,&ty,d);
  if(!isingrid(tx,ty)) return 0;
  if(!isclear(tx,ty)) return 0;
  if(isbar(x,y,d)) return 0;
  return 1;
  }

// move forwards one (merged) cell in direction d
static void stepforwm(int*x,int*y,int d) {int x0,y0;
  x0=*x;y0=*y;
  while(ismerge(*x,*y,d)) {stepforw(x,y,d);if(*x==x0&&*y==y0) return;} // can loop e.g. in circular grids
  stepforw(x,y,d);
  }

// move backwards one (merged) cell in direction d
static void stepbackm(int*x,int*y,int d) {int x0,y0,od;
  x0=*x;y0=*y;
  od=(d+ndir[gtype])%(ndir[gtype]*2); // opposite direction
  stepback(x,y,d); if(*x==x0&&*y==y0) return;
  while(ismerge(*x,*y,od)) {stepback(x,y,d);if(*x==x0&&*y==y0) return;}
  }

// is a step forwards clear?
static int clearafterm(int x,int y,int d) {int tx,ty;
  tx=x;ty=y;stepforwm(&tx,&ty,d);
  if(x==tx&&y==ty) return 0;
  if(!isingrid(tx,ty)) return 0;
  if(!isclear(tx,ty)) return 0;
  stepback(&tx,&ty,d);
  if(isbar(tx,ty,d)) return 0;
  return 1;
  }

int stepbackifingrid (int*x,int*y,int d) {int tx,ty; tx=*x;ty=*y; stepback (&tx,&ty,d);if(!isingrid(tx,ty)) return 0; *x=tx;*y=ty; return 1;}
//static int stepforwifingrid (int*x,int*y,int d) {int tx,ty; tx=*x;ty=*y; stepforw (&tx,&ty,d);if(!isingrid(tx,ty)) return 0; *x=tx;*y=ty; return 1;} // not currently used
int stepforwmifingrid(int*x,int*y,int d) {int tx,ty; tx=*x;ty=*y; stepforwm(&tx,&ty,d);if(!isingrid(tx,ty)) return 0; *x=tx;*y=ty; return 1;}
int stepbackmifingrid(int*x,int*y,int d) {int tx,ty; tx=*x;ty=*y; stepbackm(&tx,&ty,d);if(!isingrid(tx,ty)) return 0; *x=tx;*y=ty; return 1;}


void getmergerepd(int*mx,int*my,int x,int y,int d) {int x0,y0; // find merge representative, direction d (0<=d<ndir[gtype]) only
  assert(0<=d);
  assert(d<ndir[gtype]);
  *mx=x;*my=y;
  if(!isclear(x,y)) return;
  if(!ismerge(x,y,d+ndir[gtype])) return;
  x0=x;y0=y;
  do {
    stepback(&x,&y,d);
    if(!isclear(x,y)) goto ew1;
    if(x+y*MXSZ<*mx+*my*MXSZ) *mx=x,*my=y; // first lexicographically if loop
    if(x==x0&&y==y0) goto ew1; // loop detected
    } while(ismerge(x,y,d+ndir[gtype]));
  *mx=x;*my=y;
  ew1: ;
  }

int getmergedir(int x,int y) {int d; // get merge direction
  if(!isclear(x,y)) return -1;
  for(d=0;d<ndir[gtype];d++) if(ismerge(x,y,d)||ismerge(x,y,d+ndir[gtype])) return d;
  return 0;
  }

void getmergerep(int*mx,int*my,int x,int y) {int d; // find merge representative, any direction
  *mx=x;*my=y;
  d=getmergedir(x,y); if(d<0) return;
  getmergerepd(mx,my,x,y,d);
  }

int isownmergerep(int x,int y) {int x0,y0;
  getmergerep(&x0,&y0,x,y);
  return x==x0&&y==y0;
  }

// get coordinates of cells (max MXCL) in merge group with (x,y) in direction d
int getmergegroupd(int*gx,int*gy,int x,int y,int d) {int l,x0,y0;
  if(!isclear(x,y)) {if(gx) *gx=x;if(gy) *gy=y;return 1;}
  getmergerepd(&x,&y,x,y,d);
  x0=x;y0=y;l=0;
  for(;;) {
    assert(l<MXCL);
    if(gx) gx[l]=x;
    if(gy) gy[l]=y;
    l++;
    if(!ismerge(x,y,d)) break;
    stepforw(&x,&y,d);
    if(!isclear(x,y)) break;
    if(x==x0&&y==y0) break;
    }
  return l;
  }

// get coordinates of cells (max MXCL) in merge group with (x,y) in merge direction
int getmergegroup(int*gx,int*gy,int x,int y) {int d;
  if(!isclear(x,y)) {if(gx) *gx=x;if(gy) *gy=y;return 1;}
  d=getmergedir(x,y); assert(d>=0);
  return getmergegroupd(gx,gy,x,y,d);
  }

int isstartoflight(int x,int y,int d) {
  if(!isclear(x,y)) return 0;
  if(clearbefore(x,y,d)) return 0;
  if(clearafterm(x,y,d)) return 1;
  return 0;
  }

// returns:
// -1: looped, no light found
//  0: blocked, no light found
//  1: light found, start (not mergerep'ed) stored in *lx,*ly
static int getstartoflight(int*lx,int*ly,int x,int y,int d) {int x0,y0;
  if(!isclear(x,y)) return 0;
  x0=x;y0=y;
  while(clearbefore(x,y,d)) {
    stepback(&x,&y,d); // to start of light
    if(x==x0&&y==y0) return -1; // loop found
    }
  *lx=x;*ly=y;
  return 1;
  }

int ismux(int x,int y,int d) {
  int l,lx,ly;
  if(d>=100) return 0; // virtual light?
  l=getstartoflight(&lx,&ly,x,y,d);
  if(l<1) return 0;
  if(!isstartoflight(lx,ly,d)) return 0; // not actually a light
  return getlprop(lx,ly,d)->mux;
  }

// is light selected?
int issellight(int x,int y,int d) {int l,lx,ly;
  l=getstartoflight(&lx,&ly,x,y,d);
  if(l<1) return 0;
  if(!isstartoflight(lx,ly,d)) return 0; // not actually a light
  return (gsq[lx][ly].dsel>>d)&1;
  }

// set selected flag on a light to k
void sellight(int x,int y,int d,int k) {int l,lx,ly;
  DEB_GR printf("sellight(%d,%d,%d,%d)\n",x,y,d,k);
  l=getstartoflight(&lx,&ly,x,y,d);
  if(l<1) return;
  if(!isstartoflight(lx,ly,d)) return; // not actually a light
  gsq[lx][ly].dsel=(gsq[lx][ly].dsel&(~(1<<d)))|(k<<d);
  }

// extract mergerepd'ed grid squares (max MXCL) forming light running through (x,y) in direction d
// returns:
//   -1: loop detected
//    0: (x,y) blocked
//    1: no light in this direction (lx[0], ly[0] still set)
// n>=2: length of light (lx[0..n-1], ly[0..n-1] set), can be up to MXCL for VL, up to MXSZ*2 in Mobius case
// if d>=100 returns data for VL #d-100
// result is independent of multiplex property of light
int getlightd(int*lx,int*ly,int x,int y,int d) {int i,j;
  if(d>=100) {
    d-=100;
    for(i=0,j=0;i<vls[d].l;i++) if(isclear(vls[d].x[i],vls[d].y[i])) lx[j]=vls[d].x[i],ly[j]=vls[d].y[i],j++; // skip invalid squares
    return j;
    }
  i=getstartoflight(&x,&y,x,y,d);
  if(i<1) return i;
  i=0;
  for(;;) {
    assert(i<MXSZ*2);
    lx[i]=x;ly[i]=y;i++;
    if(!clearafterm(x,y,d)) break;
    stepforwm(&x,&y,d);
    }
  return i;
  }

// extract merge-representative grid squares (up to MXCL) forming light running through (x,y) in direction d
// if d>=100 returns data for VL #d-100
// errors as for getlightd()
// result is independent of multiplex property of light
int getlight(int*lx,int*ly,int x,int y,int d) {int i,l;
  l=getlightd(lx,ly,x,y,d);
  for(i=0;i<l;i++) getmergerep(lx+i,ly+i,lx[i],ly[i]);
  return l;
  }

// extract data for light running through (x,y) in direction d
// if d>=100 returns data for VL #d-100
// for non-mux light mx=0; for multiplexed light mx=desired multiplex number
// returns -2 if light overflows plus errors as for getlightd()
// lp: bitmap ptrs (<=MXLE)
// lx: mergerep square x (<=MXLE)
// ly: mergerep square y (<=MXLE)
// ls: index of contributing string (<=MXLE)
// lo: offset in contributing string (<=MXLE)
// le: entry number (<=MXLE)
int getlightdat(ABM**lp,int*lx,int*ly,int*ls,int*lo,struct entry**le,int x0,int y0,int d,int mx) {
  int c,i,j,k,k0,k1,l,m,tx[MXCL],ty[MXCL],x,y;
  l=getlight(tx,ty,x0,y0,d);
  if(l<1) return l;

  if(!ismux(x0,y0,d)) mx=-1;
  for(i=0,m=0;i<l;i++) { // for each square in the light...
    x=tx[i]; y=ty[i];
    // compute contribution mask
    if(getdech(x,y)==0) c=1; // normal checking
    else if(d<100)      c=1<<d; // de-checked: not VL
    else                c=(1<<ndir[gtype])-1; // all directions contribute to VL:s
    for(j=0;j<ndir[gtype];j++) {
      if(c&(1<<j)) { // does this direction contribute to light?
        if(gsq[x][y].ctlen[j]==0) continue; // empty contribution? skip
        if(mx<0) {k0=0;                     k1=gsq[x][y].ctlen[j]; } // non-multiplexed light? use entire contents
        else     {k0=mx%gsq[x][y].ctlen[j]; k1=k0+1;               } // multiplexed light: extract single character from contents
        for(k=k0;k<k1;k++) {
          if(m==MXLE) return -2;
          if(lp) lp[m]=gsq[x][y].ctbm[j]+k;
          if(lx) lx[m]=x;
          if(ly) ly[m]=y;
          if(ls) ls[m]=j;
          if(lo) lo[m]=k;
          if(le) le[m]=gsq[x][y].ents[j][k];
          m++;
          }
        }
      }
    }
  return m;
  }

// extract pointers to bitmaps forming light running through (x,y) in direction d
// for non-mux light mx=0; for multiplexed light mx=desired multiplex number
// returns errors as getlightdat
int getlightbmp(ABM**p,int x,int y,int d,int mx) {return getlightdat(p,0,0,0,0,0,x,y,d,mx);}

// extract word running through x,y in direction d from grid in UTF-8
// s must have space for MXLE*16+1 chars
// for non-mux light mx=0; for multiplexed light mx=desired multiplex number
// returns length of light or errors as getlightdat
int getwordutf8(int x,int y,int d,char*s,int mx) {
  int i,l,u;
  ABM*chp[MXLE];
  l=getlightbmp(chp,x,y,d,mx);
  s[0]='\0';
  for(i=0;i<l;i++) {
    u=abmtoicc(chp[i][0]);
    if(u==-2) strcat(s,".");
    else if(u==-1) strcat(s,"?");
    else strcat(s,icctoutf8[u]);
    }
  return l;
  }

// if normally-checked, single-character square, get character in mergerep square
//   return 0..MAXICC-1
//   -1: bitmap is zero
//   -2: more than one bit set
// else return -3
int geteicc(int x,int y) {
  if(getdech(x,y)) return -3;
  getmergerep(&x,&y,x,y);
  if(gsq[x][y].ctlen[0]!=1) return -3;
  return abmtoicc(gsq[x][y].ctbm[0][0]);
  }

// if normally-checked, single-character square or
// dechecked but only single character in direction d,
// set character in mergrep square and return 0; else return 1
// c is either in internal character code or -1 to clear cell
int seteicc(int x,int y,int d,int c) {
  if(!getdech(x,y)) d=0;
  getmergerep(&x,&y,x,y);
  if(gsq[x][y].ctlen[d]!=1) return 1;
  gsq[x][y].ctbm[d][0]=(c==-1)?ABM_NRM:ICCTOABM(c);
  return 0;
  }

// clear contents of cell
void clrcont(int x,int y) {int i,j;
  getmergerep(&x,&y,x,y);
  for(i=0;i<MAXNDIR;i++) for(j=0;j<gsq[x][y].ctlen[i];j++) gsq[x][y].ctbm[i][j]=ABM_NRM;
  }


static void setmerge(int x,int y,int d,int k) { // set merge state in direction d to k, make bar state consistent
  if(d>=ndir[gtype]) {d-=ndir[gtype];stepback(&x,&y,d);}
  if(!isingrid(x,y)) return;
  if(k) k=1;
  gsq[x][y].merge&=~(1<<d);
  gsq[x][y].merge|=  k<<d;
  gsq[x][y].bars &=~gsq[x][y].merge;
  }

static void setbars(int x,int y,int d,int k) { // set bar state in direction d to k, make merge state consistent
  if(d>=ndir[gtype]) {d-=ndir[gtype];stepback(&x,&y,d);}
  if(!isingrid(x,y)) return;
  if(k) k=1;
  gsq[x][y].bars &=~(1<<d);
  gsq[x][y].bars |=  k<<d;
  gsq[x][y].merge&=~gsq[x][y].bars;
  }

static void demerge(int x,int y) {int i; // demerge from all neighbours
  for(i=0;i<ndir[gtype]*2;i++) setmerge(x,y,i,0);
  }

// calculate mask of feasible symmetries
int symmrmask(void) {int i,m;
  switch(gshape[gtype]) {
case 0: return 0x16;break;
case 1:
    if(EVEN(width))          return 0x06;
    if(EVEN(width/2+height)) return 0x06;
    else                     return 0x5e;
    break;
case 2:
    if(EVEN(height))          return 0x06;
    if(EVEN(height/2+width))  return 0x06;
    else                      return 0x5e;
    break;
case 3: case 4:
    m=0;
    for(i=1;i<=12;i++) if(width%i==0) m|=1<<i;
    return m;
    break;
    }
  return 0x02;
  }

int symmmmask(void) {
  switch(gshape[gtype]) {
case 0: case 1: case 2: return 0x0f;break;
case 3: case 4:
    if(width%2) return 0x03;
    else        return 0x0f;
    break;
    }
  return 0x01;
  }

int symmdmask(void) {
  switch(gshape[gtype]) {
case 0: case 1: case 2:return 0x0f;break;
case 3: case 4:return 0x01;break;
    }
  return 0x01;
  }

int nw,nw0,ntw,nc,ne,ne0; // count of words, treated words, cells, entries
struct word*words=0;
struct entry*entries=0;
struct entry*treatmsge0[NMSG][MXFL]={{0}};

struct lprop msglprop[NMSG];
char msgword[NMSG][MXFL+1]; // in internal character code

// statistics calculated en passant by bldstructs()
int st_lc[MXLE+1];             // total lights, by length
int st_lucc[MXLE+1];           // underchecked lights, by length
int st_locc[MXLE+1];           // overchecked lights, by length
int st_lsc[MXLE+1];            // total of checked entries in lights, by length
int st_lmnc[MXLE+1];           // minimum checked entries in lights, by length
int st_lmxc[MXLE+1];           // maximum checked entries in lights, by length
int st_hist[MAXICC+2];         // entry letter histogram: [MAXICC]=uncommitted, [MAXICC+1]=partially committed
int st_sc;                     // total checked cells
int st_ce;                     // total checked entries (i.e., squares)
int st_2u,st_3u;               // count of double+ and triple+ unches
int st_tlf,st_vltlf,st_tmtlf;  // count of (free) lights too long for filler



// calculate numbers for squares and grid-order-indices of treated squares
void donumbers(void) {
  int d,i,i0,j,v,num=1,goi=0;

  for(j=0;j<height;j++) for(i=0;i<width;i++) gsq[i][j].number=-1,gsq[i][j].goi=-1;
  for(j=0;j<height;j++) for(i0=0;i0<width;i0++) {
    if(gshape[gtype]==1) {i=i0*2;if(i>=width) i=(i-width)|1;} // special case for hexH grid
    else i=i0;
    if(isclear(i,j)) {
      if(isownmergerep(i,j)&&(gsq[i][j].sp.spor?gsq[i][j].sp.ten:dsp.ten)) gsq[i][j].goi=goi++;
      for(d=0;d<ndir[gtype];d++){
        if(isstartoflight(i,j,d)) {
          if(!getlprop(i,j,d)->dnran) {gsq[i][j].number=num++; break;}
        }   // if
      }   // for d
    }   // isclear
    // Check for the start of a virtual light
    for(v=0;v<nvl;v++) {
      if(vls[v].x[0] == i &&  vls[v].y[0] == j)
      {
        if(!getvlprop(v)->dnran) {gsq[i][j].number=num++; break;}
      }
    }
    }   // for i0
  }   // for j

// add word to filler data, starting at (i,j) in direction d
// if d>=100, add virtual light #d-100
// for non-mux light mx=0; for multiplexed light mx=desired multiplex number
// return:
// 0: OK
// 1: too long
// 2: ignored as dmask is zero
static int addwordfd(int i,int j,int d,int mx) {
  int k,l,lx[MXLE],ly[MXLE];
  struct entry*le[MXLE];
  ABM*lcp[MXLE];
  struct lprop*lp;

  lp=getlprop(i,j,d);
  if(lp->dmask==0) return 2; // skip if dictionary mask empty
  l=getlightdat(lcp,lx,ly,0,0,le,i,j,d,mx);
  if(l<1) return 1; // too long: ignore
  DEB_BS printf("new word %d: l=%d mx=%d\n",nw,l,mx);
  words[nw].gx0=i;
  words[nw].gy0=j;
  if(d<100) {
    gsq[i][j].w[d][mx]=words+nw;
    words[nw].ldir=d;
    }
  else {
    vls[d-100].w=words+nw;
    words[nw].ldir=d;
    }
  words[nw].lp=lp;
  for(k=0;k<l;k++) {
    assert(le[k]!=0);
    words[nw].e[k]=le[k];
    if(le[k]==entries+curent&&d==curdir&&mx==curmux) curword=nw;
    words[nw].goi[k]=gsq[lx[k]][ly[k]].goi;
    DEB_BS printf("  d=%d x=%d y=%d k=%d e=%d goi=%d\n",d,lx[k],ly[k],k,(int)(le[k]-entries),words[nw].goi[k]);
    nc++;
    }
  words[nw].nent=k;
  words[nw].wlen=k;
  words[nw].jlen=k;
  DEB_BS printf("  nent=%d\n",k);
  nw++;
  if(lp->ten&&treatmode>0) ntw++;
  return 0;
  }

void freewords() {
  int i;
  if(words)
    for(i=0;i<nw;i++) {
      FREEX(words[i].flist);
      FREEX(words[i].jdata);
      FREEX(words[i].jflbm);
      FREEX(words[i].sdata);
      words[i].flistlen=0;
      }
  FREEX(words);
  }

void initstructs() {
  int c,g,i,j,k0,k1,k2,k3,k4,d,m,u;
  freewords();
  FREEX(entries);
  nw=0; ntw=0; nc=0; ne=0; ne0=0;
  for(i=0;i<NVL;i++) vls[i].w=0;
  for(j=0;j<MXSZ;j++) for(i=0;i<MXSZ;i++) memset(gsq[i][j].ents,0,sizeof(gsq[i][j].ents)); // make sure all ents[][] are NULL
  for(j=0;j<height;j++) for(i=0;i<width;i++) {
    for(d=0;d<MAXNDIR;d++) {
      for(m=0;m<MXMUX;m++) gsq[i][j].w[d][m]=0;
      gsq[i][j].vflags[d]=0;
      }
    gsq[i][j].number=-1;
    }
  for(i=0;i<NMSG;i++) for(j=0;j<MXFL;j++) treatmsge0[i][j]=0;
  for(i=0;i<MXLE+1;i++) st_lc[i]=st_lucc[i]=st_locc[i]=st_lsc[i]=0,st_lmxc[i]=-1,st_lmnc[i]=99; // initialise stats
  for(i=0;i<MAXICC+2;i++) st_hist[i]=0;
  st_tlf=st_vltlf=st_tmtlf=0;

  for(i=0;i<NMSG;i++) {
    utf8touchars(treatmsgU[i],treatmsg[i],MXLT+1);
    for(j=0,k0=k1=k2=k3=k4=0;treatmsgU[i][j];j++) {
      u=treatmsgU[i][j];
      c=uchartoICC(u);
      if(c<1) continue;
      u=icctouchar[c];
      g=icctogroup[c];
                              treatmsgICC    [i][k0++]=c; // everything, ICC
      if (g==0)               treatmsgICCAZ  [i][k1  ]=c, // alphabetic only
                              treatmsgUAZ    [i][k1++]=u;
      if (g==0       &&u<128) treatmsgAZ     [i][k2++]=u; // alphabetic only, 7-bit clean
      if (g==0||g==1)         treatmsgICCAZ09[i][k3  ]=c, // alphabetic+digits
                              treatmsgUAZ09  [i][k3++]=u;
      if((g==0||g==1)&&u<128) treatmsgAZ09   [i][k4++]=u; // alphabetic+digits, 7-bit clean
      }
    treatmsgICC    [i][k0]=0; // zero-termination
    treatmsgICCAZ  [i][k1]=0;
    treatmsgUAZ    [i][k1]=0;
    treatmsgAZ     [i][k2]=0;
    treatmsgICCAZ09[i][k3]=0;
    treatmsgUAZ09  [i][k3]=0;
    treatmsgAZ09   [i][k4]=0;
    }
  }

void addimplicitwords() {
  int i,j,k,l;
  ne0=ne; nw0=nw; // number of entries and words before implicit words added: needed for stats and deck dump
  k=nw;
  if(ntw) { // if there are any treated words add in "implicit" words as needed
    for(i=0;i<NMSG;i++) {
      for(j=0,l=0;j<k;j++) if(words[j].lp->ten) {
        if(treatorder[i]>0) entries[ne].flbm=treatcstr[i][l];
        else                entries[ne].flbm=ABM_ALL;
        entries[ne].sel=1; // make sure we fill even if only in "fill selection" mode
        entries[ne].checking=2;
        if(l==MXFL) st_tmtlf=1; // too many treated lights for filler to use discretion
        if(l<MXFL) {
          treatmsge0[i][l]=entries+ne;
          words[nw].e[l++]=entries+ne; // build up new implicit word
          }
        words[j].e[words[j].nent]=entries+ne; // add tag to end of word in grid
        words[j].nent++; // note that we do not increment jlen here
        ne++;
        }
      words[nw].nent=l;
      words[nw].lp=msglprop+i;
      memset(msglprop+i,0,sizeof(struct lprop));
      msglprop[i].dmask=1<<(MAXNDICTS+i); // special dictionary mask flag value
      strncpy(msgword[i],treatmsgICC[i],l); // truncate message if necessary
      msgword[i][l]=0;
      if(treatorder[i]==0) { // first come first served mode
        msglprop[i].emask=EM_FWD;
        words[nw].wlen=strlen(msgword[i]);
      } else if(treatorder[i]==1) { // "spread" mode
        msglprop[i].emask=EM_SPR;
        words[nw].wlen=strlen(msgword[i]);
      } else {
        msglprop[i].emask=EM_ALL;
        for(j=strlen(msgword[i]);j<l;j++) msgword[i][j]=ICC_DASH; // pad message if necessary with dashes
        words[nw].wlen=l;
        words[nw].jlen=l;
        }
      if(l&&treatorder[i]>0) nw++; // don't create an empty word, and don't create one for first come first served entry
      }
    }
  }

// build structures for solver and compile statistics
// 0: OK
// 1: too complex
// Ensure filler is stopped before calling this (reallocates e.g. words[])
static int bldstructs(void) {
  int cnw,cne,d,i,j,k,l,m,nd,v;
  int ml,mm,curk,curl;
  unsigned char unch[MXLE];
  ABM c;

  curent=-1;
  curword=-1;
  initstructs();
  cnw=0; // count words
  for(d=0;d<ndir[gtype];d++) for(j=0;j<height;j++) for(i=0;i<width;i++)
    if(isstartoflight(i,j,d)) cnw+=getmaxmux(i,j,d); // look for normal lights
  cnw+=nvl; // plus virtual lights
  cne=0; // count entries: entry count is independent of multiplexing
  for(j=0;j<height;j++) for(i=0;i<width;i++) if(isclear(i,j)&&isownmergerep(i,j)) {
    if(getdech(i,j)) nd=ndir[gtype];
    else             nd=1;
    for(k=0;k<nd;k++) cne+=gsq[i][j].ctlen[k];
    }

  words=calloc(cnw+NMSG,sizeof(struct word)); // one spare word for each message
  if(!words) return 1;
  entries=calloc(cne+NMSG*cnw,sizeof(struct entry)); // cnw spares for each message
  if(!entries) return 1;

  for(j=0;j<height;j++) for(i=0;i<width;i++) if(isclear(i,j)&&isownmergerep(i,j)) { // loop over all mergerep cells and assign entries to them
    mm=getmaxmux(i,j,curdir);
    if(mm<1) mm=1;
    if(getdech(i,j)) nd=ndir[gtype],curk=curdir;
    else             nd=1,          curk=0;
    if(curk>=100) curk=0; // cursor "points" at first character in case of VL
    for(k=0;k<nd;k++) {
      ml=gsq[i][j].ctlen[k];
      if(ml) curl=curmux%ml;
      else   curl=0;
      for(l=0;l<ml;l++) { // one entry for each potentially different character in this cell
        if(i==curx&&j==cury&&k==curk&&l==curl) curent=ne;
        entries[ne].gx=i;
        entries[ne].gy=j;
        entries[ne].sel=!!(gsq[i][j].fl&16); // selected?
        c=gsq[i][j].ctbm[k][l]&ABM_NRM; // letter in square
        entries[ne].flbm=c; // bitmap of feasible letters
        if(c==ABM_NRM) st_hist[MAXICC]++; // uncommitted
        else if(onebit(c)) st_hist[abmtoicc(c)]++;
        else st_hist[MAXICC+1]++; // partially committed
        gsq[i][j].ents[k][l]=entries+ne;
        ne++;
        }
      }
    }
  DEB_BS printf("total entries=%d\n",ne);
  assert(ne==cne);

  for(d=0;d<ndir[gtype];d++) for(j=0;j<height;j++) for(i=0;i<width;i++) if(isstartoflight(i,j,d)) { // look for normal lights
    m=getmaxmux(i,j,d);
    for(k=0;k<m;k++) if(addwordfd(i,j,d,k)==1) st_tlf++;
    }
  for(i=0;i<nvl;i++) if(addwordfd(0,0,i+100,0)==1) st_vltlf++; // add in virtual lights

  for(i=0;i<ne;i++) entries[i].checking=0; // now calculate checking (and other) statistics
  for(i=0;i<nw;i++) for(j=0;j<words[i].nent;j++) words[i].e[j]->checking++;
  st_ce=0;
  for(i=0;i<ne;i++) if(entries[i].checking>1) st_ce++; // number of checked squares
  st_sc=0;st_2u=0;st_3u=0;
  for(i=0;i<nw;i++) {
    l=words[i].nent;
    st_lc[l]++;
    v=0;
    for(j=0,k=0;j<l;j++) {
      unch[j]=(words[i].e[j]->checking<=1);
      if(!unch[j]) k++; // count checked cells in word
      if(j>0&&unch[j]&&unch[j-1]           ) v|=1; // double unch?
      if(j>1&&unch[j]&&unch[j-1]&&unch[j-2]) v|=2; // triple+ unch?
      }
    if(v&1) st_2u++; // double+ unch?
    if(v&2) st_3u++; // triple+ unch?
  // k is now number of checked cells (out of l total)
    if( k   *100<l*mincheck) st_lucc[l]++,v|=4; // violation flags
    if((k-1)*100>l*maxcheck) st_locc[l]++,v|=8;
    st_lsc[l]+=k;
    if(k<st_lmnc[l]) st_lmnc[l]=k;
    if(k>st_lmxc[l]) st_lmxc[l]=k;
    st_sc+=k;
    if(words[i].ldir<ndir[gtype])
      for(j=0;j<l;j++) gsq[words[i].gx0][words[i].gy0].vflags[words[i].ldir]|=v; // update violation flags for non VLs
    }

  addimplicitwords();

  donumbers();
  stats_upd(); // refresh statistics window if it is in view
  DEB_BS printf("nw=%d ntw=%d nc=%d ne=%d curent=%d curword=%d\n",nw,ntw,nc,ne,curent,curword);
  return 0;
  }

// symmetry functions
// call f on (x,y) if in range
static void symmdo5(void f(int,int,int,int),int k,int x,int y,int d) {
  if(isingrid(x,y)) f(k,x,y,d);
  }

// call symmdo5 on (x,y) and other square implied by up-and-down symmetry flag (if any)
static void symmdo4(void f(int,int,int,int),int k,int x,int y,int d) {
  if(symmd&2) switch(gshape[gtype]) {
case 0: case 1: case 2:
    symmdo5(f,k,x,(y+(height+1)/2)%((height+1)&~1),d);break;
case 3: case 4: break; // not for circular grids
    }
  symmdo5(f,k,x,y,d);
  }

// call symmdo4 on (x,y) and other square implied by left-and-right symmetry flag (if any)
static void symmdo3(void f(int,int,int,int),int k,int x,int y,int d) {
  if(symmd&1) switch(gshape[gtype]) { 
case 0: case 1: case 2:
    symmdo4(f,k,(x+(width+1)/2)%((width+1)&~1),y,d);break;
case 3: case 4: break; // not for circular grids
    }
  symmdo4(f,k,x,y,d);
  }

// call symmdo3 on (x,y) and other square implied by vertical mirror flag (if any)
static void symmdo2(void f(int,int,int,int),int k,int x,int y,int d) {int h;
  h=height;
  if(gshape[gtype]==1&&ODD(width)&&ODD(x)) h--;
  if(symmm&2) switch(gshape[gtype]) {
case 0: case 1: case 2:
    symmdo3(f,k,x,h-y-1,dvflip[gtype][d]);break;
case 3:
    if(width%2) break; // not for odd-size circular grids
    symmdo3(f,k,(width*3/2-x-1)%width,y,dvflip[gtype][d]);
    break;
case 4:
    if(width%2) break; // not for odd-size circular grids
    symmdo3(f,k,(width*3/2-x)%width,y,dvflip[gtype][d]);
    break;
    } 
  symmdo3(f,k,x,y,d);
  }

// call symmdo2 on (x,y) and other square implied by horizontal mirror flag (if any)
static void symmdo1(void f(int,int,int,int),int k,int x,int y,int d) {int w;
  w=width;
  if(gshape[gtype]==2&&ODD(height)&&ODD(y)) w--;
  if(symmm&1) switch(gshape[gtype]) {
case 0: case 1: case 2: case 3:
    symmdo2(f,k,w-x-1,y,dhflip[gtype][d]);break;
case 4:
    symmdo2(f,k,(w-x)%w,y,dhflip[gtype][d]);break;
    break;
    } 
  symmdo2(f,k,x,y,d);
  }

// get centre of rotation in 6x h-units, 4x v-units (gshape[gtype]=1; mutatis mutandis for gshape[gtype]=2)
static void getcrot(int*cx,int*cy) {int w,h;
  w=width;h=height;
  if(gshape[gtype]==1) {
    *cx=(w-1)*3;
    if(EVEN(w)) *cy=h*2-1;
    else        *cy=h*2-2;
  } else {
    *cy=(h-1)*3;
    if(EVEN(h)) *cx=w*2-1;
    else        *cx=w*2-2;
    }
  }

// rotate by d/6 of a revolution in 6x h-units, 4x v-units (gshape[gtype]=1; mutatis mutandis for gshape[gtype]=2)
static void rot6(int*x0,int*y0,int x,int y,int d) {int u,v;
  u=1;v=1;
  switch(d) {
case 0:u= x*2    ;v=   2*y;break;
case 1:u= x  -3*y;v= x+  y;break;
case 2:u=-x  -3*y;v= x-  y;break;
case 3:u=-x*2    ;v=  -2*y;break;
case 4:u=-x  +3*y;v=-x-  y;break;
case 5:u= x  +3*y;v=-x+  y;break;
    }
  assert(EVEN(u));assert(EVEN(v));
  *x0=u/2;*y0=v/2;
  }

// call f on (x,y) and any other squares implied by symmetry flags by
// calling symmdo1 on (x,y) and any other squares implied by rotational symmetry flags
void symmdo(void f(int,int,int,int),int k,int x,int y,int d) {int i,mx,my,x0,y0;
  switch(gshape[gtype]) {
case 0:
    switch(symmr) {
    case 4:symmdo1(f,k,width-y-1,x,         (d+1)%4);
           symmdo1(f,k,y,        height-x-1,(d+3)%4); // fall through
    case 2:symmdo1(f,k,width-x-1,height-y-1,(d+2)%4); // fall through
    case 1:symmdo1(f,k,x,        y,          d);
      }
    break;
case 1:
    getcrot(&mx,&my);
    y=y*4+(x&1)*2-my;x=x*6-mx;
    for(i=5;i>=0;i--) {
      if((i*symmr)%6==0) {
        rot6(&x0,&y0,x,y,i);
        x0+=mx;y0+=my;
        assert(x0%6==0); x0/=6;
        y0-=(x0&1)*2;
        assert(y0%4==0); y0/=4;
        symmdo1(f,k,x0,y0,(d+i)%6);
        }
      }
    break;
case 2:
    getcrot(&mx,&my);
    x=x*4+(y&1)*2-mx;y=y*6-my;
    for(i=5;i>=0;i--) {
      if((i*symmr)%6==0) {
        rot6(&y0,&x0,y,x,i);
        x0+=mx;y0+=my;
        assert(y0%6==0); y0/=6;
        x0-=(y0&1)*2;
        assert(x0%4==0); x0/=4;
        symmdo1(f,k,x0,y0,(d-i+6)%6);
        }
      }
    break;
case 3: case 4:
    if(symmr==0) break; // assertion
    for(i=symmr-1;i>=0;i--) symmdo1(f,k,(x+i*width/symmr)%width,y,d);
    break;
    }
  }

// basic grid editing commands (candidates for f() in symmdo above)
void a_editblock (int k,int x,int y,int d) {int l,gx[MXCL],gy[MXCL];
  l=getmergegroup(gx,gy,x,y);
  gsq[x][y].fl=(gsq[x][y].fl&0x06)|1;
  demerge(x,y);
  refreshsqlist(l,gx,gy);
  }

void a_editempty (int k,int x,int y,int d) {
  gsq[x][y].fl= gsq[x][y].fl&0x16;
  refreshsqmg(x,y);
  }

void a_editcutout(int k,int x,int y,int d) {int l,gx[MXCL],gy[MXCL];
  l=getmergegroup(gx,gy,x,y);
  gsq[x][y].fl=(gsq[x][y].fl&0x06)|8;
  demerge(x,y);
  refreshsqlist(l,gx,gy);
  }

// set bar state in direction d to k
void a_editbar(int k,int x,int y,int d) {int tx,ty;
  if(!isingrid(x,y)) return;
  tx=x;ty=y;
  stepforw(&tx,&ty,d);
//  printf("<%d,%d %d,%d %d>\n",x,y,tx,ty,d);
  if(!isingrid(tx,ty)) return;
  setbars(x,y,d,k);
  refreshsqmg(x,y);
  refreshsqmg(tx,ty);
  donumbers();
  }

// set merge state in direction d to k, deal with consequences
void a_editmerge(int k,int x,int y,int d) {int f,i,l,tl,tx,ty,gx[MXCL],gy[MXCL],tgx[MXCL],tgy[MXCL];
  if(!isclear(x,y)) return;
  tx=x;ty=y;
  stepforw(&tx,&ty,d);
  if(!isclear(tx,ty)) return;
  l=getmergegroup(gx,gy,x,y);
  tl=getmergegroup(tgx,tgy,tx,ty);
  if(k) for(i=0;i<ndir[gtype];i++) if(i!=d&&i+ndir[gtype]!=d) {
    setmerge(x,y,i,0);
    setmerge(x,y,i+ndir[gtype],0);
    setmerge(tx,ty,i,0);
    setmerge(tx,ty,i+ndir[gtype],0);
    }
  setmerge(x,y,d,k);
  refreshsqlist(l,gx,gy);
  refreshsqlist(tl,tgx,tgy);
  f=gsq[x][y].fl&16; // make selection flags consistent
  l=getmergegroup(gx,gy,x,y);
  for(i=0;i<l;i++) gsq[gx[i]][gy[i]].fl=(gsq[gx[i]][gy[i]].fl&~16)|f;
  refreshsqmg(x,y);
  donumbers();
  }





// PREFERENCES


static int preftype[NPREFS]=       {0,0,0,  0,  0,0,0, 0, 0,0, 1,1,1,1,1,1, 1,1,1,1,1,1, 1,1,1,0            }; // 0=int, 1=string
static int prefminv[NPREFS]=       {0,0,0,  0,  0,0,0,10,10,0, 0,0,0,0,0,0, 0,0,0,0,0,0, 0,0,0,0            }; // legal range
static int prefmaxv[NPREFS]=       {1,1,1,100,100,1,2,72,72,1, 0,0,0,0,0,0, 0,0,0,0,0,0, 0,0,0,NALPHAINIT-1 };
static int prefdatadefault[NPREFS]={0,0,1, 66, 75,1,0,36,36,0, 0,0,0,0,0,0, 0,0,0,0,0,0, 0,0,0,ALPHABET_AZ09};
static char prefstringdefault[NPREFS][SLEN+1]={
  "", "", "", "", "",
  "", "", "", "", "",
  "Look up at Collins online",
  "Look up at Oxford online",
  "Search at English Wiktionary",
  "Search at English Wikipedia",
  "Search at Startpage",
  "Look up at Dictionary.com",
  "https://www.collinsdictionary.com/search/?dictCode=english&q=%s",
  "https://en.oxforddictionaries.com/definition/%s",
  "https://en.wiktionary.org/w/index.php?search=%s",
  "https://en.wikipedia.org/w/index.php?search=%s",
  "https://www.startpage.com/do/search?q=%s",
  "https://www.dictionary.com/browse/%s?s=t",
  "","","",""
  };

int prefdata[NPREFS]={0};
char prefstring[NPREFS][SLEN+1]={{0}};

static char*prefname[NPREFS]={ // names as used in preferences file
  "edit_click_for_block", "edit_click_for_bar", "edit_show_numbers",
  "stats_min_check_percent", "stats_max_check_percent",
  "autofill_no_duplicates", "autofill_random",
  "export_EPS_square_points","export_HTML_square_pixels",
  "light_numbers_in_solutions",
  "lookupname_0", "lookupname_1", "lookupname_2", "lookupname_3", "lookupname_4", "lookupname_5",
  "lookupuri_0",  "lookupuri_1",  "lookupuri_2",  "lookupuri_3",  "lookupuri_4",  "lookupuri_5", 
  "startup_defdict1", "startup_ff1", "startup_af1", "startup_al"
  };

static void loadprefdefaults() {
  memcpy(prefdata,prefdatadefault,sizeof(prefdata));
  memcpy(prefstring,prefstringdefault,sizeof(prefstring));
  }

// open preferences file, mode=0 for writing, mode=1 for reading
FILE*openprefs(int mode) {
#ifdef _WIN32		// Folder for preferences file
  wchar_t sw[SLEN];
  if (!SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, sw))) return 0;
  wcscat_s(sw, SLEN, L"\\Qxw");
  if(mode==0) _wmkdir(sw);
  wcscat_s(sw, SLEN, L"\\Qxw.ini");
  return _wfopen(sw,mode?L"r":L"w");
#else
  char s[SLEN];
  struct passwd*p;
  p=getpwuid(getuid());
  if(!p) return 0;
  if(strlen(p->pw_dir)>SLEN-20) return 0;
  strcpy(s,p->pw_dir);
  strcat(s,"/.qxw");
  if(mode==0) mkdir(s,0777);
  strcat(s,"/preferences");
  return g_fopen(s,mode?"r":"w");
#endif
  }

// read preferences from file
// fail silently
static void loadprefs() {
  FILE*fp;
  char s[SLEN],*q;
  int i,u;

  fp=openprefs(1); if(!fp) return;
  while (fgets(s, SLEN, fp)) {
    q=strchr(s,' ');
    if(!q) continue;
    *q++=0;
    i=strlen(q);
    while(i>0&&iscntrl((unsigned char)q[i-1])) q[--i]=0;
    for(i=0;i<NPREFS;i++) {
      if(strcmp(s,prefname[i])) continue;
      switch(preftype[i]) {
    case 0:
        u=atoi(q);
        if(u<prefminv[i]) u=prefminv[i];
        if(u>prefmaxv[i]) u=prefmaxv[i];
        prefdata[i]=u;
        break;
    case 1:
        strcpy(prefstring[i],q);
        break;
    default:
        break;
        }
      }
    }
  fclose(fp);
  }

// write preferences to file
// fail silently
void saveprefs(void) {
  FILE*fp;
  int i;

  fp=openprefs(0); if(!fp) return;
  for(i=0;i<NPREFS;i++) {
    switch(preftype[i]) {
  case 0:
      fprintf(fp,"%s %d\n",prefname[i],prefdata[i]);
      break;
  case 1:
      fprintf(fp,"%s %s\n",prefname[i],prefstring[i]);
      break;
  default:
      break;
      }
    }
  fclose(fp);
  }




// UNDO

static struct square ugsq[UNDOS][MXSZ][MXSZ]; // circular undo buffers
static int ugtype[UNDOS];
static int uwidth[UNDOS];
static int uheight[UNDOS];
static struct sprop udsp[UNDOS];
static struct lprop udlp[UNDOS];
static int unvl[UNDOS];
static struct vl uvls[UNDOS][NVL];

static char utpifname[UNDOS][SLEN];
static int utreatmode[UNDOS]; // 0=none, TREAT_PLUGIN=custom plug-in
static int utreatorder[UNDOS][NMSG]; // 0=first come first served, 1=spread, 2=jumble
static char utreatmsg[UNDOS][NMSG][MXLT*8+1];
static ABM utreatcstr[UNDOS][NMSG][MXFL];
static int utambaw[UNDOS]; // treated answer must be a word

int uhead=0,utail=0,uhwm=0; // undos can be performed from uhead back to utail, redos from uhead up to uhwm (`high water mark')


void undo_push(void) {
  unsaved=1;
#define USAVE(p) memcpy(u##p+uhead,p,sizeof(p))
#define USAVES(p) memcpy(u##p+uhead,&p,sizeof(p))
  USAVE(gsq);
  USAVES(gtype);
  USAVES(width);
  USAVES(height);
  USAVES(dsp);
  USAVES(dlp);
  USAVES(nvl);
  USAVE(vls);
  USAVE(tpifname);
  USAVES(treatmode);
  USAVE(treatorder);
  USAVE(treatmsg);
  USAVE(treatcstr);
  USAVES(tambaw);
  uhead=(uhead+1)%UNDOS; // advance circular buffer pointer
  uhwm=uhead; // can no longer redo
  if(uhead==utail) utail=(utail+1)%UNDOS; // buffer full? delete one entry at tail
  DEB_GR printf("undo_push: head=%d tail=%d hwm=%d\n",uhead,utail,uhwm);
  }

void undo_pop(void) {int u;
  filler_stop();
  uhead=(uhead+UNDOS-1)%UNDOS; // back head pointer up one
  u=(uhead+UNDOS-1)%UNDOS; // get previous state index
#define ULOAD(p) memcpy(p,u##p+u,sizeof(p))
#define ULOADS(p) memcpy(&p,u##p+u,sizeof(p))
  ULOAD(gsq);
  ULOADS(gtype);
  ULOADS(width);
  ULOADS(height);
  ULOADS(dsp);
  ULOADS(dlp);
  ULOADS(nvl);
  ULOAD(vls);
  ULOAD(tpifname);
  ULOADS(treatmode);
  ULOAD(treatorder);
  ULOAD(treatmsg);
  ULOAD(treatcstr);
  ULOADS(tambaw);
  if(curdir>=ndir[gtype]) curdir=0;
  if(curx>=width)  curx=width -1;
  if(cury>=height) cury=height-1;
  }




static char*defaultmk(int k) {return k?"":"\\#";} // default mark is just number in NW corner

// convert ABM to UTF-8 choice string of the form [a-eghq-z0-39], using inverting notation if inv==1 and
// including "-" if dash==1
// return number of characters in UTF-8 string
static int abmtocs(char*s,ABM b,int inv,int dash) {
  int c0,c1,w=0;
  *s=0;
  strcat(s,"["),w++;
  if(inv) strcat(s,"^"),w++,b^=dash?ABM_ALL:ABM_NRM;
  b&=abm_use|ABM_DASH;
  for(c0=1;;) {
    for(;c0<MAXICC;c0++) if(b&ICCTOABM(c0)) break;
    if(c0==MAXICC) break;
    for(c1=c0+1;c1<MAXICC;c1++) {
      if((b&ICCTOABM(c1))==0) break;
      if(c1>1&&!iccseq[c1-1]) break;
      }
    strcat(s,icctoutf8[c0]),w++;
    if(c1-c0>1) {
      if(c1-c0>2) strcat(s,"-"),w++;
      strcat(s,icctoutf8[c1-1]),w++;
      }
    c0=c1;
    }
  if(dash&&(b&ABM_DASH)) strcat(s,"-"),w++;
  strcat(s,"]"),w++;
  return w;
  }

// convert ABM to UTF-8 string of the form "s" or "[aeiou]" or whatever
// MAXICC*16+4 is a very safe upper bound on the resulting length
void abmtostr(char*s,ABM b,int dash) {
  int w0,w1;
  char s0[MAXICC*16+4],s1[MAXICC*16+4];
  b&=dash?ABM_ALL:ABM_NRM;
  if((b&(abm_use|ABM_DASH))==(abm_use|ABM_DASH)) {strcpy(s,"?"); return;}
  if((b& abm_use          )== abm_use          ) {strcpy(s,"."); return;}
  if(b==abm_vow) {strcpy(s,"@"); return;}
  if(b==abm_con) {strcpy(s,"#"); return;}
  if(onebit(b)) {strcpy(s,icctoutf8[abmtoicc(b)]); return;}
  w0=abmtocs(s0,b,0,dash);
  w1=abmtocs(s1,b,1,dash);
//  printf("abmtostr: %016llx %s %s\n",b,s0,s1);
  if(w0<=w1) strcpy(s,s0); // choose shorter of the two representations
  else       strcpy(s,s1);
  }

// convert sequence of ABMs to UTF-8 string in compact format suitable for display
void abmstodispstr(char*s,ABM*b,int l) {int c,i;
  s[0]=0;
  for(i=0;i<l;i++) {
    c=abmtoicc(*b++);
    if(c==-2) strcat(s," ");
    else if(c==-1) strcat(s,"?");
    else strcat(s,icctoutf8[c]);
    }
  }

// convert sequence of ABMs to string: concantenates results of abmtostr()
void abmstostr(char*s,ABM*b,int l,int dash) {int i;
  *s=0;
  for(i=0;i<l;i++) abmtostr(s,*b++,dash),s+=strlen(s);
  }

// call abmtostr() and print the result
void pabm(ABM b,int dash) {
  char s[MAXICC*16+4];
  abmtostr(s,b,dash);
  printf("%s",s);
  }

// call abmstostr() and print the result
void pabms(ABM*b,int l,int dash) {
  int i;
  for(i=0;i<l;i++) pabm(b[i],dash);
  }


// convert string containing choice lists to array of ABMs
// if dash==1, dash is allowed at end of each choice list
// returns number of ABMs, maximum l
int strtoabms(ABM*p,int l,char*s0,int dash) {
  int c,c0,c1,f,i;
  ABM b;
  uchar t[SLEN],*s;

  utf8touchars(t,s0,SLEN);
//  printf("strtoabms: >%s<",s0);
//  for(i=0;t[i];i++) printf(" %08x",t[i]);
//  printf("\n");

  for(i=0,s=t;*s&&i<l;) {
    c=uchartoICC(*s);
    if(dash&&*s=='-') b=ABM_DASH,s++;
    else if(*s=='?') b=ABM_ALL,s++;
    else if(*s=='.') b=ABM_NRM,s++;
    else if(*s==' ') b=ABM_NRM,s++;
    else if(*s=='@') b=abm_vow,s++;
    else if(*s=='#') b=abm_con,s++;
    else if(*s=='[') {
      s++;
      f=0; b=0;
      if(*s=='^') f=1,s++;
      for(;;) {
        if(dash&&*s=='-') {b|=ABM_DASH; s++; continue;}
        if(*s==']') break;
        if(*s=='@') { b|=abm_vow; s++; continue; }
        if(*s=='#') { b|=abm_con; s++; continue; }
        c=uchartoICC(*s);
        if(c==0) break;
        c0=c1=c;
        if(s[1]=='-') {
          c=uchartoICC(s[2]);
          if(c) {
            if(c>c0) c1=c; else c0=c; // found a range: sort bounds
            s+=2;
            }
          }
        for(c=c0;c<=c1;c++) b|=ICCTOABM(c);
        s++;
        }
      if(*s==']') s++;
      if(f) b^=dash?ABM_ALL:ABM_NRM; // negation flag
      }
    else if(c>0) b=ICCTOABM(c),s++;
    else {s++; continue;} // skip unrecognised characters (includes BOM)
    b&=dash?ABM_ALL:ABM_NRM;
    p[i++]=b;
    }
  return i;
  }

static void resetsp(struct sprop*p) {int k;
  p->bgcol=0xffffff;
  p->fgcol=0x000000;
  p->mkcol=0x000000;
  p->fstyle=0;
  p->dech=0;
  p->ten=0;
//  for(k=0;k<MAXNMK;k++) sprintf(p->mk[k],"%c",k+'a'); // for testing
  for(k=0;k<MAXNMK;k++) strcpy(p->mk[k],defaultmk(k));
  p->spor=0;
  }

void resetlp(struct lprop*p) {
  p->dmask=1;
  p->emask=EM_FWD;
  p->ten=0;
  p->dnran=0;
  p->mux=0;
  p->lpor=0;
  }

// generate "codeword" permutation cwperm mapping ICC to ICC
void initcodeword() {
  int i,j,k,p[MAXICC],u0,u1;
  for(i=1,j=0;i<MAXICC;i++) {
    cwperm[i]=0; // default
    k=icctousedindex[i];
    if(k>=0&&iccusedtogroup[k]==0) { // skip non-letters
      cwperm[i]=j+1; // count from 1
      p[j]=i;
      j++;
      }
    }
  // here there are j permutable letters, indices in p[]
  if(j>1) // something worth permuting?
    for(i=0;i<j*j*10;i++) {u0=p[rand()%j]; u1=p[rand()%j]; k=cwperm[u0]; cwperm[u0]=cwperm[u1]; cwperm[u1]=k;}
  }

void initsquare(int x,int y) {
  int k;
  memset(&gsq[x][y],0,sizeof(gsq[x][y])); // inter alia ensure ents[][] are all NULL
  gsq[x][y].ctlen[0]=1;
  gsq[x][y].ctbm[0][0]=ABM_NRM;
  resetsp(&gsq[x][y].sp);
  for(k=0;k<MAXNDIR;k++) resetlp(&gsq[x][y].lp[k]);
  }

static void resetstate(void) {
  int i,j;
  for(i=0;i<MXSZ;i++) for(j=0;j<MXSZ;j++) initsquare(i,j);
  resetsp(&dsp);
  resetlp(&dlp);
  for(i=0;i<NVL;i++) vls[i].l=0,vls[i].sel=0;
  for(i=0;i<NVL;i++) resetlp(&vls[i].lp);
  for(i=0;i<NVL;i++) for(j=0;j<MXCL;j++) vls[i].x[j]=0,vls[i].y[j]=0;
  nvl=0;
  nsel=0;
  selmode=0;
  treatmode=0;
  for(i=0;i<NMSG;i++) {
    treatorder[i]=0;
    treatmsg[i][0]=0;
    for(j=0;j<MXFL;j++) treatcstr[i][j]=ABM_ALL;
    }
  tpifname[0]=0;
  tambaw=0;
  unsaved=0;
  bldstructs();
  }

// tidy up square properties structure
static void fixsp(struct sprop*sp) {
  sp->bgcol&=0xffffff;
  sp->fgcol&=0xffffff;
  sp->mkcol&=0xffffff;
//  if(sp->fstyle<0) sp->fstyle=0;
  if(sp->fstyle>3) sp->fstyle=3;
  sp->ten=!!sp->ten;
//  if(sp->dech<0) sp->dech=0;
  if(sp->dech>2) sp->dech=2;
  sp->spor=!!sp->spor;
  }

// tidy up light properties structure
static void fixlp(struct lprop*lp) {
  lp->dmask&=(1<<MAXNDICTS)-1;
  lp->emask&=(1<<NLEM)-1;
  if(lp->emask==0) lp->emask=EM_FWD;
  lp->ten=!!lp->ten;
  lp->lpor=!!lp->lpor;
  lp->dnran=!!lp->dnran;
  }

// FILE SAVE/LOAD

FILE* q_fopen(char* filename, char* mode) {
#ifdef _WIN32
	wchar_t wfilename[SLEN];
	wchar_t wmode[4];
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename, SLEN);
	MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, 4);
	return _wfopen(wfilename, wmode);
#else
	return g_fopen(filename, mode);
#endif
}

// flags b23..16: grid width
// flags  b15..8: grid height
// flags   b7..0: grain
void a_filenew(int flags) {
  char*p;
  int u,v,x,y;

  filler_stop();
  resetstate();
  u=(flags>>16)&0xff;
  v=(flags>>8)&0xff;
  if(u>0&&u<=MXSZ&&v>0&&v<MXSZ) {
    gtype=0,width=u,height=v;
    if((flags&0x80)==0) { // pre-fill with blocks?
      u=!!(flags&1); v=!!(flags&2);
      for(x=0;x<width;x++) for(y=0;y<height;y++) if((x+u)&(y+v)&1) gsq[x][y].fl|=1;
      }
    }
  if(!strcmp(filenamebase,"")) {   // brand new file
#ifdef _WIN32
    wchar_t sw[SLEN];
    size_t l;
    if(SHGetFolderPathW(NULL, CSIDL_MYDOCUMENTS, NULL, 0, sw)==S_OK) {
	  l = WideCharToMultiByte(CP_UTF8, 0, sw, -1, filenamebase, SLEN, NULL, NULL);
      if(l<1||l>SLEN-20) strcpy(filenamebase,"");
      else strcat(filenamebase,"\\");
      }
    else strcpy(filenamebase,"");
#else
    struct passwd*pw;
    pw=getpwuid(getuid());
    if(!pw||strlen(pw->pw_dir)>SLEN-20) strcpy(filenamebase,"");
    else                                strcpy(filenamebase,pw->pw_dir),strcat(filenamebase,"/");
#endif
  } else { // we have a path to start from
    p=strrchr(filenamebase,DIR_SEP_CHAR);
    if(p) strcpy(p,DIR_SEP_STR);
    else strcpy(filenamebase,"");
    }
  strcat(filenamebase,"untitled");
  havesavefn=0;
  donumbers();
  undo_push();
  unsaved=0;
  }

#define SAVE_VERSION 5

// read next line from file, strip terminators so compatible with MS-DOS and Unix
#define NEXTL  {if(!fgets(s,SLEN*4-1,fp)) goto ew1; l=strlen(s); while(l>0&&iscntrl((unsigned char)s[l-1])) s[--l]='\0';}
#define NEXTLN {if(!fgets(s,SLEN*4-1,fp)) goto ew4; l=strlen(s); while(l>0&&iscntrl((unsigned char)s[l-1])) s[--l]='\0';} // fail gracefully on EOF

// load state from file
void a_load(void) {
  int d,i,j,k,l,n,u,t0,t1,t2,t3,b,m,f,wf,ver;
  char *p,s[SLEN*4],s0[SLEN*4],*t,c;
  struct sprop sp;
  struct lprop lp;
  FILE*fp;

  filler_stop();
  DEB_GR printf("filler stopped: loading\n");
  setfilenamebase(filename);
  fp=q_fopen(filename,"r");
  if(!fp) {fserror();return;}
  resetstate();
  initalphamap(alphainitdata[ALPHABET_AZ09]); // default for legacy save files and .SYM
  *gtitle=0;
  *gauthor=0;
  NEXTL;
  wf=0;

  if(STRSTARTS(s,"#QXW2")) {
  // #QXW2 load
    ver=0;
    if(s[5]=='v'&&isdigit((unsigned char)s[6])) ver=atoi(s+6);
    if(ver>SAVE_VERSION) wf=1; // check if the file was saved using a newer version
    if(ver>=4) clearalphamap(); // ver>=4 will include alphabet information
    NEXTL;
    if(sscanf(s,"GP %d %d %d %d %d %d\n",&gtype,&width,&height,&symmr,&symmm,&symmd)!=6) goto ew1;
    if(gtype<0||gtype>=NGTYPE|| // validate basic parameters
       width<1||width>MXSZ||
       height<1||height>MXSZ) goto ew1;
    if(symmr<1||symmr>12) symmr=1;
    if(symmm<0||symmm>3) symmm=0;
    if(symmd<0||symmd>3) symmd=0;
    if((symmr&symmrmask())==0) symmr=1;
    draw_init();
    NEXTL;
    if(strcmp(s,"TTL")) goto ew1;
    NEXTL;
    if(s[0]!='+') goto ew1;
    strncpy(gtitle,s+1,SLEN-1);
    gtitle[SLEN-1]=0;
    NEXTL;
    if(strcmp(s,"AUT")) goto ew1;
    NEXTL;
    if(s[0]!='+') goto ew1;
    strncpy(gauthor,s+1,SLEN-1);
    gauthor[SLEN-1]=0;
    DEB_GR printf("L0\n");
    NEXTL;
    while(STRSTARTS(s,"ALP ")) {
      if(sscanf(s,"ALP %d %d %d %d\n",&i,&t0,&t1,&t2)<4) goto ew1;
      NEXTL;
      if(s[0]!='+') goto ew1;
      strcpy(s0,s);
      NEXTL;
      if(s[0]!='+') goto ew1;
      if(s0[1]) addalphamapentry(i,s0+1,s+1,!!t0,!!t1,!!t2); // add entry if non-empty
      NEXTL;
      }
    if(STRSTARTS(s,"GLP ")) {
      int ten,lpor,dnran=0,mux=0;
      resetlp(&dlp);
      if(sscanf(s,"GLP %u %u %d %d %d %d\n",&dlp.dmask,&dlp.emask,&ten,&lpor,&dnran,&mux)<4) goto ew1;
      dlp.ten=ten; // using %hhd above fails in Visual C
      dlp.lpor=0;
      dlp.dnran=dnran;
      dlp.mux=mux;
      fixlp(&dlp);
      NEXTL;
      }
    while(STRSTARTS(s,"GSP ")) {
      int ten,spor,fstyle=0,dech=0;
      resetsp(&dsp);
      if(sscanf(s,"GSP %x %x %d %d %d %d %x\n",&dsp.bgcol,&dsp.fgcol,&ten,&spor,&fstyle,&dech,&dsp.mkcol)<4) goto ew1;
      dsp.ten=ten;
      dsp.spor=0;
      dsp.fstyle=fstyle;
      dsp.dech=dech;
      fixsp(&dsp);
      NEXTL;
      }
    while(STRSTARTS(s,"GSPMK ")) {
      if(sscanf(s,"GSPMK %d\n",&k)<1) goto ew1;
      if(k<0||k>=MAXNMK) continue;
      NEXTL;
      if(s[0]=='+') {
        strncpy(dsp.mk[k],s+1,MXMK);
        dsp.mk[k][MXMK]='\0';
        }
      NEXTL;
      }
    while(STRSTARTS(s,"TM ")) {
      t2=0; t3=0;
      if(sscanf(s,"TM %d %d %d %d %d\n",&j,&t0,&t1,&t2,&t3)<3) goto ew1;
      NEXTL;
      if(j==0&&s[0]=='+') {
        if(t0<0||t0>=NATREAT) continue;
        treatmode=t0,tambaw=!!t1;
        if(t2<0||t2>2) t2=0;
       	treatorder[0]=t2;
        if(t3<0||t3>2) t3=0;
       	treatorder[1]=t3;
        if(treatmode==TREAT_PLUGIN) {
          strncpy(tpifname,s+1,SLEN-1);
          tpifname[SLEN-1]=0;
          }
        NEXTL;
        }
      }
    while(STRSTARTS(s,"TMSG ")) {
      if(sscanf(s,"TMSG %d %d\n",&j,&t0)!=2) goto ew1;
      NEXTL;
      if(j==0&&s[0]=='+') {
        if(t0<0||t0>=NMSG) continue;
        strncpy(treatmsg[t0],s+1,sizeof(treatmsg[0])-1);
        treatmsg[t0][sizeof(treatmsg[0])-1]=0;
        NEXTL;
        }
      }
    while(STRSTARTS(s,"TCST ")) {
      if(sscanf(s,"TCST %d %d %s\n",&i,&j,s0)!=3) goto ew1;
      if(i>=0&&i<NMSG&&j>=0&&j<MXFL)
      strtoabms(treatcstr[i]+j,1,s0,1);
      NEXTL;
      }
    while(STRSTARTS(s,"DFN ")) {
      if(sscanf(s,"DFN %d\n",&j)!=1) goto ew1;
      NEXTL;
      if(j<0||j>=MAXNDICTS)  continue;
      if(s[0]=='+') {
        strncpy(dfnames[j],s+1,SLEN-1);
        dfnames[j][SLEN-1]=0;
        NEXTL;
        }
      }
    while(STRSTARTS(s,"DSF ")) {
      if(sscanf(s,"DSF %d\n",&j)!=1) goto ew1;
      NEXTL;
      if(j<0||j>=MAXNDICTS)  continue;
      if(s[0]=='+') {
        strncpy(dsfilters[j],s+1,SLEN-1);
        dsfilters[j][SLEN-1]=0;
        NEXTL;
        }
      }
    while(STRSTARTS(s,"DAF ")) {
      if(sscanf(s,"DAF %d\n",&j)!=1) goto ew1;
      NEXTL;
      if(j<0||j>=MAXNDICTS)  continue;
      if(s[0]=='+') {
        strncpy(dafilters[j],s+1,SLEN-1);
        dafilters[j][SLEN-1]=0;
        NEXTL;
        }
      }
    DEB_GR printf("L1: %s\n",s);
    while(STRSTARTS(s,"SQ ")) {
      c=' ';
//      DEB_GR printf("SQ: ");
      if(sscanf(s,"SQ %d %d %d %d %d %c\n",&i,&j,&b,&m,&f,&c)<5) goto ew1;
//      DEB_GR printf("%d,%d %d\n",i,j,b);
      if(i<0||i>=MXSZ||j<0||j>=MXSZ) continue;
      gsq[i][j].bars    =b&((1<<ndir[gtype])-1);
      gsq[i][j].merge   =m&((1<<ndir[gtype])-1);
      gsq[i][j].fl      =f&0x09;
      if(c==' ') gsq[i][j].ctbm[0][0]=ABM_NRM;
      else       gsq[i][j].ctbm[0][0]=ICCTOABM(uchartoICC((int)c));
      if(!onebit(gsq[i][j].ctbm[0][0])) gsq[i][j].ctbm[0][0]=ABM_NRM;
      NEXTL;
      }
    DEB_GR printf("L2\n");
    while(STRSTARTS(s,"SQSP ")) {
      int ten,spor,fstyle=0,dech=0;
      resetsp(&sp);
      if(sscanf(s,"SQSP %d %d %x %x %d %d %d %d %x\n",&i,&j,&sp.bgcol,&sp.fgcol,&ten,&spor,&fstyle,&dech,&sp.mkcol)<6) goto ew1;
      if(i<0||i>=MXSZ||j<0||j>=MXSZ) continue;
      sp.ten=ten;
      sp.spor=spor;
      sp.fstyle=fstyle;
      sp.dech=dech;
      fixsp(&sp);
      gsq[i][j].sp=sp;
      NEXTL;
      }
    while(STRSTARTS(s,"SQSPMK ")) {
      if(sscanf(s,"SQSPMK %d %d %d\n",&i,&j,&k)<3) goto ew1;
      if(i<0||i>=MXSZ||j<0||j>=MXSZ||k<0||k>=MAXNMK) continue;
      NEXTL;
      if(s[0]=='+') {
        strncpy(gsq[i][j].sp.mk[k],s+1,MXMK);
        gsq[i][j].sp.mk[k][MXMK]='\0';
        }
      NEXTL;
      }
    while(STRSTARTS(s,"SQLP ")) {
      int ten,lpor,dnran=0,mux=0;
      resetlp(&lp);
      if(sscanf(s,"SQLP %d %d %d %u %u %d %d %d %d\n",&i,&j,&d,&lp.dmask,&lp.emask,&ten,&lpor,&dnran,&mux)<7) goto ew1;
      if(i<0||i>=MXSZ||j<0||j>=MXSZ||d<0||d>=ndir[gtype]) continue;
      lp.ten=ten;
      lp.lpor=lpor;
      lp.dnran=dnran;
      lp.mux=mux;
      fixlp(&lp);
      gsq[i][j].lp[d]=lp;
      NEXTL;
      }
    while(STRSTARTS(s,"VL ")) {
      if(sscanf(s,"VL %d %d %d %d\n",&d,&n,&i,&j)!=4) goto ew1;
      if(i<0||i>=MXSZ||j<0||j>=MXSZ||n<0||n>=MXCL||d<0||d>=NVL) continue;
      vls[d].x[n]=i;
      vls[d].y[n]=j;
      if(n>=vls[d].l) vls[d].l=n+1;
      if(d>=nvl) nvl=d+1;
      NEXTL;
      }
    while(STRSTARTS(s,"VLP ")) {
      int ten,lpor,dnran=0,mux=0;
      resetlp(&lp);
      if(sscanf(s,"VLP %d %u %u %d %d %d %d\n",&d,&lp.dmask,&lp.emask,&ten,&lpor,&dnran,&mux)<5) goto ew1;
      if(d<0||d>=NVL) continue;
      lp.ten=ten;
      lp.lpor=lpor;
      lp.dnran=dnran;
      lp.mux=mux;
      fixlp(&lp);
      vls[d].lp=lp;
      NEXTL;
      }
    while(STRSTARTS(s,"SQCT ")) {
      if(sscanf(s,"SQCT %d %d %d %s\n",&i,&j,&d,s0)!=4) goto ew1;
      if(i<0||i>=MXSZ||j<0||j>=MXSZ||d<0||d>=MAXNDIR) continue;
      p=s0;
      l=strlen(p);
      if(l>1&&*p=='\"'&&p[l-1]=='\"') p[l-1]=0,p++; // strip double quote marks if any
      gsq[i][j].ctlen[d]=strtoabms(gsq[i][j].ctbm[d],MXCT,p,0);
      NEXTL;
      }
    if(ver>=4) initcodeword(); // ver>=4 may have updated alphabet information
    }
  else if(strlen(s)>3&&STRSTARTS(s+2,"ympath")) { // necessarily incomplete as we don't have documentation
    int chkt=0,symt=0,blkh=0,x0,x1,y0,y1,dir;
    NEXTL;
    if(sscanf(s,"[Grid %d %d]",&width,&height)!=2) goto ew1;
    if(width<1||width>MXSZ|| // validate basic parameters
       height<1||height>MXSZ) goto ew1;
    draw_init();
    for(;;) {
      NEXTL;
      if(STRSTARTS(s,"[Light ")) break;
      if(STRSTARTS(s,"#Title=" )) { strncpy(gtitle ,s+7,SLEN-1); gtitle [SLEN-1]=0; continue; }
      if(STRSTARTS(s,"#Author=")) { strncpy(gauthor,s+8,SLEN-1); gauthor[SLEN-1]=0; continue; }
      if(STRSTARTS(s,"BlockHole="   )) { blkh=atoi(s+10); continue; }
      if(STRSTARTS(s,"CheckingType=")) { chkt=atoi(s+13); continue; }
      if(STRSTARTS(s,"SymmetryType=")) { symt=atoi(s+13); continue; }
      DEB_GR printf("Unprocessed: <%s>\n",s);
      }
    DEB_GR printf("chkt=%d symt=%d; now processing lights\n",chkt,symt);
    if(chkt==0) { // barred? add bars everywhere
      for(j=0;j<height  ;j++) for(i=0;i<width-1;i++) gsq[i][j].bars =1;
      for(j=0;j<height-1;j++) for(i=0;i<width  ;i++) gsq[i][j].bars|=2;
      }
    for(j=0;j<height;j++) for(i=0;i<width;i++) gsq[i][j].fl=blkh?8:1;
    for(;;) { // process the lights
      if(sscanf(s,"[Light %d %d %d %d]",&x0,&y0,&x1,&y1)!=4) goto ew1;
      DEB_GR printf("(%d,%d - %d,%d)\n",x0,y0,x1,y1);
      if(x0<0||x0>width||y0<0||y0>height) goto ew1;
      if(x1<0||x1>width||y1<0||y1>height) goto ew1;
      if(y0==y1) { // across
                    for(i=x0;i<x1  ;i++) gsq[i][y0].fl=0;
        if(chkt==0) for(i=x0;i<x1-1;i++) gsq[i][y0].bars&=~1;
        dir=0;
        }
      else if(x0==x1) { // down
                    for(j=y0;j<y1  ;j++) gsq[x0][j].fl=0;
        if(chkt==0) for(j=y0;j<y1-1;j++) gsq[x0][j].bars&=~2;
        dir=1;
        }
      else {
        DEB_GR printf("Light %d %d %d %d is not straight\n",x0,y0,x1,y1);
        goto ew1;
        }
      for(;;) {
        NEXTLN;
        if(STRSTARTS(s,"[Light ")) break; // process next light
        if(STRSTARTS(s,"Text=")) {
          i=x0; j=y0; p=s+5;
          while(*p) {
            gsq[i][j].ctbm[0][0]=ICCTOABM(uchartoICC((int)*p++));
            if(dir==0) i++;
            else       j++;
            if(i==x1&&j==y1) break;
            }
          continue;
          }
        if(STRSTARTS(s,"Number=")) { // set dnran
          if(!strcmp(s,"Number=")) { // only for exact match: presumably there are other possibilities than "empty"
            gsq[x0][y0].lp[dir].lpor =1;
            gsq[x0][y0].lp[dir].dnran=1;
            continue;
            }
          }
        if(STRSTARTS(s,"BackgroundColor=")) { // sic
          int r,g,b;
          if(sscanf(s,"BackgroundColor=%d,%d,%d",&r,&g,&b)==3) {
            i=x0; j=y0;
            for(;;) {
              gsq[i][j].sp.spor=1;
              gsq[i][j].sp.bgcol=((r&0xff)<<16) | ((g&0xff)<< 8) | (b&0xff);
              if(dir==0) i++;
              else       j++;
              if(i==x1&&j==y1) break;
              }
            continue;
            }
          }
        DEB_GR printf("Unprocessed: <%s>\n",s);
        }
      }
ew4:
    for(j=0;j<height;j++) for(i=0;i<width;i++) if(gsq[i][j].fl==8) {
              gsq[i  ][j  ].bars=0;
      if(i>0) gsq[i-1][j  ].bars&=~1;
      if(j>0) gsq[i  ][j-1].bars&=~2;
      }
    }
  else {
  // LEGACY LOAD
    gtype=0;
    if(sscanf(s,"%d %d %d %d %d\n",&width,&height,&symmr,&symmm,&symmd)!=5) goto ew1;
    if(width<1||width>MXSZ|| // validate basic parameters
       height<1||height>MXSZ||
       symmr<0||symmr>2||
       symmm<0||symmm>3||
       symmd<0||symmd>3) goto ew1;
    if     (symmr==1) symmr=2;
    else if(symmr==2) symmr=4;
    else symmr=1;
    draw_init();
    for(j=0;j<height;j++) { // read flags
      NEXTL;
      t=s;
      for(i=0;i<width;i++) {
        u=strtol(t,&t,10);
        if(u<0||u>31) goto ew1;
        gsq[i][j].fl=u&0x09;
        gsq[i][j].bars=(u>>1)&3;
        gsq[i][j].merge=0;
        }
      }
    for(j=0;j<height;j++) { // read grid
      for(i=0;i<width;i++) {
        c=fgetc(fp);
        if((c<'A'||c>'Z')&&(c<'0'||c>'9')&&c!=' ') goto ew1;
        gsq[i][j].ctbm[0][0]=ICCTOABM(uchartoICC((int)c));
        }
      if(fgetc(fp)!='\n') goto ew1;
      }
    }

  if(fclose(fp)) goto ew3;
  reloadtpi();
  donumbers();
  loaddicts(0);
  undo_push();unsaved=0;
  if(wf) repwarn("File was saved using\na newer version of Qxw.\nSome features may be lost.");
  havesavefn=1;
  bldstructs();
  return; // no errors

ew1:fclose(fp);syncgui();reperr("File format error");goto ew2;
ew3:fserror();
ew2:
  a_filenew(0);
  initalphamap(alphainitdata[startup_al]); // use preferences value of default alphabet
  loaddefdicts();
  bldstructs();
  }

// write state to file
void a_save(void) {
  int d,i,j,k,l;
  char s0[MXCT*(MAXICC*16+4)];
  FILE*fp;

  setfilenamebase(filename);
  fp=q_fopen(filename,"w");
  if(!fp) {fserror();return;}
  if(fprintf(fp,"#QXW2v%d http://www.quinapalus.com\n",SAVE_VERSION)<0) goto ew0;
  if(fprintf(fp,"GP %d %d %d %d %d %d\n",gtype,width,height,symmr,symmm,symmd)<0) goto ew0;
  if(fprintf(fp,"TTL\n+%s\n",gtitle)<0) goto ew0;
  if(fprintf(fp,"AUT\n+%s\n",gauthor)<0) goto ew0;
  for(i=1;i<MAXICC;i++) {
    if(icctoutf8[i][0]==0) continue;
    if(!strcmp(icctoutf8[i],"�")) continue;
    if(fprintf(fp,"ALP %d %d %d %d\n+%s\n+%s\n",i,!!(abm_vow&ICCTOABM(i)),(int)!!(abm_con&ICCTOABM(i)),iccseq[i],icctoutf8[i],iccequivs[i])<0) goto ew0;
    }
  if(fprintf(fp,"GLP %d %d %d %d %d %d\n",dlp.dmask,dlp.emask,dlp.ten,dlp.lpor,dlp.dnran,dlp.mux)<0) goto ew0;
  if(fprintf(fp,"GSP %06x %06x %d %d %d %d %06x\n",dsp.bgcol,dsp.fgcol,dsp.ten,dsp.spor,dsp.fstyle,dsp.dech,dsp.mkcol)<0) goto ew0;
  for(k=0;k<MAXNMK;k++) if(fprintf(fp,"GSPMK %d\n+%s\n",k,dsp.mk[k])<0) goto ew0;
  if(fprintf(fp,"TM 0 %d %d %d %d\n+%s\n",treatmode,tambaw,treatorder[0],treatorder[1],tpifname)<0) goto ew0;
  for(i=0;i<NMSG;i++) if(fprintf(fp,"TMSG 0 %d\n+%s\n",i,treatmsg[i])<0) goto ew0;
  for(i=0;i<NMSG;i++) for(j=0;j<MXFL;j++) if(treatcstr[i][j]!=ABM_ALL) {
    abmtostr(s0,treatcstr[i][j],1);
    if(fprintf(fp,"TCST %d %d %s\n",i,j,s0)<0) goto ew0;
    }
  for(i=0;i<MAXNDICTS;i++) if(fprintf(fp,"DFN %d\n+%s\n",i,dfnames[i])<0) goto ew0;
  for(i=0;i<MAXNDICTS;i++) if(fprintf(fp,"DSF %d\n+%s\n",i,dsfilters[i])<0) goto ew0;
  for(i=0;i<MAXNDICTS;i++) if(fprintf(fp,"DAF %d\n+%s\n",i,dafilters[i])<0) goto ew0;
  for(j=0;j<height;j++) for(i=0;i<width;i++) if(fprintf(fp,"SQ %d %d %d %d %d .\n",i,j,gsq[i][j].bars,gsq[i][j].merge,gsq[i][j].fl)<0) goto ew0;
  for(j=0;j<height;j++) for(i=0;i<width;i++)
    if(fprintf(fp,"SQSP %d %d %06x %06x %d %d %d %d %06x\n",i,j,
      gsq[i][j].sp.bgcol,gsq[i][j].sp.fgcol,gsq[i][j].sp.ten,gsq[i][j].sp.spor,gsq[i][j].sp.fstyle,gsq[i][j].sp.dech,gsq[i][j].sp.mkcol)<0) goto ew0;
  for(j=0;j<height;j++) for(i=0;i<width;i++)
    for(k=0;k<MAXNMK;k++) if(fprintf(fp,"SQSPMK %d %d %d\n+%s\n",i,j,k,gsq[i][j].sp.mk[k])<0) goto ew0;
  for(j=0;j<height;j++) for(i=0;i<width;i++) for(d=0;d<ndir[gtype];d++)
    if(fprintf(fp,"SQLP %d %d %d %d %d %d %d %d %d\n",i,j,d,gsq[i][j].lp[d].dmask,gsq[i][j].lp[d].emask,gsq[i][j].lp[d].ten,gsq[i][j].lp[d].lpor,gsq[i][j].lp[d].dnran,gsq[i][j].lp[d].mux)<0) goto ew0;
  for(d=0;d<nvl;d++) for(i=0;i<vls[d].l;i++)
    if(fprintf(fp,"VL %d %d %d %d\n",d,i,vls[d].x[i],vls[d].y[i])<0) goto ew0;
  for(d=0;d<nvl;d++)
    if(fprintf(fp,"VLP %d %d %d %d %d %d %d\n",d,vls[d].lp.dmask,vls[d].lp.emask,vls[d].lp.ten,vls[d].lp.lpor,vls[d].lp.dnran,vls[d].lp.mux)<0) goto ew0;
  for(j=0;j<height;j++) for(i=0;i<width;i++) for(d=0;d<ndir[gtype];d++) {
    l=gsq[i][j].ctlen[d];
    abmstostr(s0,gsq[i][j].ctbm[d],l,0);
    if(fprintf(fp,"SQCT %d %d %d \"%s\"\n",i,j,d,s0)<0) goto ew0;
    }
  if(fprintf(fp,"END\n")<0) goto ew0;
  if(ferror(fp)) goto ew0;
  if(fclose(fp)) {fserror(); return;}
  havesavefn=1;
  unsaved=0;return; // saved successfully
  ew0:
  fserror();
  fclose(fp);
  }

void a_importvls(char*fn) {
  FILE*fp;
  struct vl tvls[NVL];
  char s[SLEN*4];
  int d,i,l;

  filler_stop();
  DEB_GR printf("filler stopped: importing VL:s\n");
  fp=q_fopen(fn,"r");
  if(!fp) goto ew3;
  for(d=0;d<NVL;) {
    tvls[d].l=0;
    resetlp(&tvls[d].lp);
    tvls[d].sel=0;
    tvls[d].w=0;
    for(i=0;;) {
      if(!fgets(s,SLEN*4-1,fp)) {
        if(feof(fp)) goto ew0;
        else         goto ew3;
        }
      l=strlen(s); while(l>0&&!isprint((unsigned char)s[l-1])) s[--l]='\0';
      if(l==0) break; // end of this vl
      if(s[0]=='#') continue; // comment line?
      if(i>=MXCL) goto ew1;
      if(sscanf(s,"%d %d",tvls[d].x+i,tvls[d].y+i)<2) goto ew1;
      if(tvls[d].x[i]<0||tvls[d].x[i]>=MXSZ) goto ew1;
      if(tvls[d].y[i]<0||tvls[d].y[i]>=MXSZ) goto ew1;
      i++;
      tvls[d].l=i;
      }
    if(tvls[d].l>0) d++;
    }
ew0:
  if(fclose(fp)) goto ew3;
  if(tvls[d].l>0) d++;
  memcpy(vls,tvls,d*sizeof(struct vl));
  nvl=d;
  undo_push();unsaved=0;syncgui();compute(0);
  return;
ew3:fserror();return;
ew1:fclose(fp);syncgui();reperr("File format error");
  }

void a_exportvls(char*fn) {
  int d,i;
  FILE*fp;

  fp=q_fopen(fn,"w");
  if(!fp) {fserror();return;}
  if(fprintf(fp,"# Free light export file created by Qxw %s http://www.quinapalus.com\n",RELEASE)<0) goto ew0;
  for(d=0;d<nvl;d++) {
    if(fprintf(fp,"# Free light %d\n",d)<0) goto ew0;
    for(i=0;i<vls[d].l;i++) if(fprintf(fp,"%d %d\n",vls[d].x[i],vls[d].y[i])<0) goto ew0;
    if(fprintf(fp,"\n")<0) goto ew0;
    }
  if(fprintf(fp,"# END\n")<0) goto ew0;
  if(ferror(fp)) goto ew0;
  if(fclose(fp)) goto ew0;
  return; // saved successfully
  ew0:
  fserror();
  fclose(fp);
  }


char*titlebyauthor(void) {static char t[SLEN*2+100];
       if(gtitle[0])       strcpy(t,gtitle);
  else if(filenamebase[0]) strcpy(t,filenamebase);
  else                     strcpy(t,"(untitled)");
  if(gauthor[0]) strcat(t," by "),strcat(t,gauthor);
  return t;
  }

// MAIN

#ifdef _WIN32
  extern wchar_t* optarg;
#else
  extern char* optarg;
#endif
extern int optind,opterr,optopt;

int main(int argc,char*argv[]) {
  int i,j,nd;
  char alphabet[SLEN+1]="";
  int deckmode=0;
  int rc=0; // return code
  unsigned int rseed;

  rseed=(unsigned int)time(0);
  fseed=0;
  log2lut[0]=log2lut[1]=0; for(i=2;i<65536;i+=2) { log2lut[i]=log2lut[i/2]+1; log2lut[i+1]=0; }
  resetstate();
  for(i=0;i<MAXNDICTS;i++) dfnames[i][0]='\0';
  for(i=0;i<MAXNDICTS;i++) strcpy(dsfilters[i],"");
  for(i=0;i<MAXNDICTS;i++) strcpy(dafilters[i],"");
  freedicts();

  nd=0;
  i=0;
  #ifdef _WIN32
		int wArgc;
		LPWSTR* wArgv = CommandLineToArgvW(GetCommandLineW(), &wArgc);
		for (;;) switch (getoptw(wArgc, wArgv, L"a:bd:?D:R:F:")) {
		case -1: goto ew0;
		case L'a':
			if (wcslen(optarg) < SLEN) WideCharToMultiByte(CP_UTF8, 0, optarg, -1, alphabet, SLEN, NULL, NULL);
			break;
		case L'b':deckmode = 1; break;
		case L'd':
			if (wcslen(optarg) < SLEN && nd < MAXNDICTS) WideCharToMultiByte(CP_UTF8, 0, optarg, -1, dfnames[nd++], SLEN, NULL, NULL);
			break;
		case L'D':debug = wcstol(optarg, 0, 0) | 0x80000000; break;
		case L'R':rseed = (unsigned int)wcstol(optarg, 0, 0); break;
		case L'F':fseed = (unsigned int)wcstol(optarg, 0, 0); break;
		case L'?':
		default:i = 1; break;
		}
  #else
		for (;;) switch (getopt(argc, argv, "a:bd:?D:R:F:")) {
		case -1: goto ew0;
		case 'a':
			if (strlen(optarg) < SLEN) strcpy(alphabet, optarg);
			break;
		case 'b':deckmode = 1; break;
		case 'd':
			if (strlen(optarg) < SLEN && nd < MAXNDICTS) strcpy(dfnames[nd++], optarg);
			break;
		case 'D':debug = strtol(optarg, 0, 0) | 0x80000000; break;
		case 'R':rseed = (unsigned int)strtol(optarg, 0, 0); break;
		case 'F':fseed = (unsigned int)strtol(optarg, 0, 0); break;
		case '?':
		default:i = 1; break;
		}
  #endif
  ew0:
  if(i) {
    printf("\nThis is Qxw, release %s.\n\n"
      "Copyright 2011-2020 Mark Owen; Windows port by Peter Flippant\n"
      "\n"
      "This program is free software; you can redistribute it and/or modify\n"
      "it under the terms of version 2 of the GNU General Public License as\n"
      "published by the Free Software Foundation.\n"
      "\n"
      "This program is distributed in the hope that it will be useful,\n"
      "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
      "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
      "GNU General Public License for more details.\n"
      "\n"
      "You should have received a copy of the GNU General Public License along\n"
      "with this program; if not, write to the Free Software Foundation, Inc.,\n"
      "51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.\n"
      "\n"
      "For more information visit http://www.quinapalus.com or\n"
      "e-mail qxw@quinapalus.com\n\n",RELEASE);
    printf("Usage: %s    [-a <initial alphabet code>] [-d <dictionary_file>]* [<qxw_file>]\n",argv[0]);
    printf("   OR: %s -b [-a <initial alphabet code>] [-d <dictionary_file>]* <qxw_deck>\n",argv[0]);
    printf("\n"
      "-b enables batch mode: GUI is disabled and a Qxw deck is read from the\n"
      "     specified file\n\n");
    printf("Available alphabets and corresponding names and codes:\n");
    for(i=0;i<NALPHAINIT;i++) {
      printf("%30s: ",alphaname[i][0]);
      for(j=1;alphaname[i][j][0];j++) {
        if(j>1) printf(", ");
        printf("%s",alphaname[i][j]);
        }
      printf("\n");
      }
    return 0;
    }

		#ifdef _WIN32
			if (optind < wArgc && wcslen(wArgv[optind]) < SLEN) WideCharToMultiByte(CP_UTF8, 0, wArgv[optind], -1, filename, SLEN, NULL, NULL);
			else																								strcpy(filename, "");
		#else
			if (optind < argc && strlen(argv[optind]) < SLEN) strcpy(filename, argv[optind]);
			else																							strcpy(filename, "");
		#endif
		if(deckmode) usegui=0;

  if(debug) {
    printf("Qxw release %s\n",RELEASE);
    printf("debug=0x%08x\n",debug);
    printf("rseed=0x%08x (set this using -R)\n",rseed);
    printf("glib version %d.%d.%d\n",glib_major_version,glib_minor_version,glib_micro_version);
    printf("gtk version %d.%d.%d\n",gtk_major_version,gtk_minor_version,gtk_micro_version);
    draw_printversions();
    }
  srand(rseed);
  g_thread_init(0);
  gdk_threads_init();
  gdk_threads_enter();
  filler_init();
  if(usegui) {
    gtk_init(&argc,&argv);
    startgtk();
    }
  a_filenew(0); // reset grid
  loadprefdefaults();
  if(usegui) loadprefs(); // load preferences file (silently failing to defaults)
  draw_init();
  if(alphabet[0]==0) { // no alphabet specified on command line
    initalphamap(alphainitdata[startup_al]); // so use preferences value
    }
  else if(initalphamapbycode(alphabet)) {
    rc=16;
    reperr("Unrecognised alphabet");
    if(deckmode) goto ew1;
    }

  if(deckmode) {
    rc=loaddeck(nd>0);
    if(rc==0) {
      if(filler_start(1)) {
        fprintf(stderr,"Failed to initialise filler\n");
        rc=16;
      } else {
        filler_wait();
        rc=dumpdeck();
        }
      }
  } else { // GUI mode
    if(filename[0]) { // we have a filename from the command line
      a_load();
    } else { // otherwise attempt to load dictionaries specified on command line
      if(nd) loaddicts(0);
      else // or, if not, try the preferences defaults and then final fallbacks
        if(loaddefdicts()) repwarn("No dictionaries loaded");
      strcpy(filenamebase,"");
      }
    syncgui();
    compute(0);
    if(debug==0&&!strcmp(RELEASE+strlen(RELEASE)-4,"beta")) {
      while(gtk_events_pending()) gtk_main_iteration_do(0);
      reperr("  This is a beta release of Qxw.  \n  Please do not distribute.  ");
      }
    gtk_main();
    filler_stop();
    }
ew1:
  draw_finit();
  if(usegui) stopgtk();
  filler_finit();
  gdk_threads_leave();
  freedicts();
  FREEX(llist);
  freewords();
  FREEX(entries);
  return rc;
  }




// INTERFACE TO FILLER

// (re-)start the filler when an edit has been made
// Return non-zero if cannot start filler
int compute(int mode) {
  filler_stop(); // stop if already running
  setposslabel("");
  if(bldstructs()) return 1; // failed?
  if(filler_start(mode)) return 1;
  if(ifamode>0) setposslabel(" Working...");
  return 0;
  }

// get all word lists up-to-date prior to exporting answers
// return 1 if something goes wrong and word lists are not valid
int preexport(void) {
  DEB_EX printf("preexport()\n");
  filler_stop(); // stop if already running
  if(bldstructs()) return 1; // failed?
  if(filler_start(3)) return 1; // need to run as far as init+settle done
  filler_wait();
  return 0;
  }
  
void postexport(void) {
  DEB_EX printf("postexport()\n");
  filler_stop();
  compute(0); // restore everything
  }

// comparison function for sorting feasible word list by score
static int cmpscores(const void*p,const void*q) {double f,g;
  if(lts[*(int*)p].ans<0) return 0;
  if(lts[*(int*)q].ans<0) return 0;
  f=ansp[lts[*(int*)p].ans]->score; // negative ans values should not occur here
  g=ansp[lts[*(int*)q].ans]->score;
  if(f<g) return  1;
  if(f>g) return -1;
  return (char*)p-(char*)q; // stabilise sort
  }

// called by filler when a list of feasible words through the cursor has been found
void mkfeas(void) {int l; int*p; struct word*w=0;
  llistp=NULL;llistn=0; // default answer: no list
  if(curword==-1||words[curword].flist==0||ifamode==0) {llistp=NULL;llistn=0;return;} // no list
  w=words+curword;
  p=w->flist;
  l=w->flistlen;
  FREEX(llist);
  llist=(int*)malloc(l*sizeof(int)); // space for feasible word list
  if(llist==NULL) return;
  memcpy(llist,p,l*sizeof(int));
  llistp=llist;
  llistn=l;
  llistwlen=w->wlen;
  llistdm=w->lp->dmask; // copy list across
  llistem=w->lp->emask;
  DEB_FL printf("llist=%p p=%p l=%d dm=%08x llistwlen=%d llistdm=%08x\n",(void*)llist,(void*)p,l,w->lp->dmask,llistwlen,llistdm);
  qsort(llistp,llistn,sizeof(int),&cmpscores);
//  DEB_FL printf("mkfeas: %d matches; dm=%08x\n",llistn,llistdm);
  }

// provide progress info to display
void updategrid(void) {int i;
  for(i=0;i<ne;i++) entries[i].flbmh=entries[i].flbm; // make back-up copy of hints
  if(usegui) refreshhin();
  }


#ifdef _WIN32			// For a Win32 app the entry point is WinMain
	int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
		return main (__argc, __argv); 
		} 
#endif
