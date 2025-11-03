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



// GTK

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "common.h"
#include "qxw.h"
#include "filler.h"
#include "dicts.h"
#include "treatment.h"
#include "gui.h"
#include "draw.h"
#include "alphabets.h"

#define MAINWWIDTH 800
#define MAINWHEIGHT 628
#define FLISTWIDTH 218

int selmode=0;
int nsel=0; // number of things selected
int pxsq;
int zoomf=8;
int zoompx[]={18,20,22,24, 26,28,30,33, 36,39,43,47, 52,57,62,67, 72};
int curmf=1; // cursor has moved since last redraw

static char*gtypedesc[NGTYPE]={
  "Plain rectangular",
  "Hex with vertical lights",
  "Hex with horizontal lights",
  "Circular",
  "Circular with half-cell offset",
  "Join left and right edges",
  "Join top and bottom edges",
  "Join left and right edges with flip",
  "Join top and bottom edges with flip",
  "Torus",
  "Klein bottle (left and right edges joined with flip, top and bottom without)",
  "Klein bottle (top and bottom edges joined with flip, left and right without)",
  "Projective plane (both pairs of edges joined with flip)",
  };

static int spropdia(int g);
static int lpropdia(int g);
static int mvldia(int v);
static int alphadia(void);
static int dictldia(void);
static int dstatsdia(void);
static int treatdia(void);
static int gpropdia(void);
static int ccontdia(void);
static int editlightdia(void);
// static int writecluedia(void);
static int prefsdia(void);
static int statsdia(void);
static int filedia(char*res,char*but,char*ext,char*filetype,int write);
static void gridchangen(void);
static void syncsymmmenu(void);
static void syncselmenu(void);
static void syncifamenu(void);


static GtkWidget *grid_da;
static GtkItemFactory *item_factory; // for menus
static GtkWidget *mainw,*grid_sw,*paned,*list_sw,*poss_label,*clist; // main window and content
static GtkWidget*stats=NULL; // statistics window (if visible)
static GtkWidget *hist_da;
static GtkWidget*(st_te[MXLE+2][5]),*(st_r[10]); // statistics table

static char*treat_lab[NATREAT][NMSG]={
  {0,                        0,                       },
  {"Keyword: ",              0,                       },
  {"Encode ABC...Z as: ",    0,                       },
  {"Key character/word: ",   0,                       },
  {"Encodings of A: ",       0,                       },
  {"Correct characters: ",   "Incorrect characters: " },
  {"Characters to delete: ", 0,                       },
  {"Characters to delete: ", 0,                       },
  {"Characters to insert: ", 0,                       },
  {"Message 1: ",            "Message 2: "            },
  {"Correct characters: ",   0,                       },
  {"Incorrect characters: ", 0                        },
  };
static int treatpermok[NATREAT][NMSG]={
  {0,0},
  {0,0},
  {0,0},
  {0,0},
  {1,0},
  {0,0},
  {1,0},
  {1,0},
  {1,0},
  {1,1},
  {1,0},
  {1,0},
  };
static char*tnames[NATREAT]={
  " None",
  " Playfair cipher",
  " Substitution cipher",
  " Fixed Caesar/Vigenère cipher",
  " Variable Caesar cipher",
  " Misprint (general, clue order)",
  " Delete single occurrence of character",
  " Letters latent: delete all occurrences of character",
  " Insert single character",
  " Custom plug-in",
  " Misprint (correct characters specified)",
  " Misprint (incorrect characters specified)",
  };

static char*mklabel[NGTYPE][MAXNDIR*2]={
  {"NW mark: ","NE mark: ","SE mark: ","SW mark: "},
  {"NW mark: ","NE mark: ","E mark: ", "SE mark: ","SW mark: ","W mark: "},
  {"NW mark: ","N mark: ","NE mark: ", "SE mark: ","S mark: ","SW mark: "},
  {"First mark: ","Second mark: ","Third mark: ","Fourth mark: "},
  {"First mark: ","Second mark: ","Third mark: ","Fourth mark: "},
  {"NW mark: ","NE mark: ","SE mark: ","SW mark: "},
  {"NW mark: ","NE mark: ","SE mark: ","SW mark: "},
  {"NW mark: ","NE mark: ","SE mark: ","SW mark: "},
  {"NW mark: ","NE mark: ","SE mark: ","SW mark: "},
  {"NW mark: ","NE mark: ","SE mark: ","SW mark: "},
  {"NW mark: ","NE mark: ","SE mark: ","SW mark: "},
  {"NW mark: ","NE mark: ","SE mark: ","SW mark: "},
  {"NW mark: ","NE mark: ","SE mark: ","SW mark: "},
  };



// set main window title
static void setwintitle() {char t[SLEN*3];
  sprintf(t,"Qxw: %s",titlebyauthor());
  gtk_window_set_title(GTK_WINDOW(mainw),t);
  }

// set filenamebase from given string, which conventionally ends ".qxw"
void setfilenamebase(char*s) {
  strncpy(filenamebase,s,SLEN-1);filenamebase[SLEN-1]='\0';
  if(strlen(filenamebase)>=4&&!strcmp(filenamebase+strlen(filenamebase)-4,".qxw")) filenamebase[strlen(filenamebase)-4]='\0';
  }

// general question-and-answer box: title, question, yes-text, no-text
static int box(int type,const char*t,const char*u,const char*v) {
  GtkWidget*dia;
  int i;

  dia=gtk_message_dialog_new(
    GTK_WINDOW(mainw),
    GTK_DIALOG_DESTROY_WITH_PARENT,
    type,
    GTK_BUTTONS_NONE,
    "%s",t
    );
  gtk_dialog_add_buttons(GTK_DIALOG(dia),u,GTK_RESPONSE_ACCEPT,v,GTK_RESPONSE_REJECT,NULL);
  i=gtk_dialog_run(GTK_DIALOG(dia));
  gtk_widget_destroy(dia);
  return i==GTK_RESPONSE_ACCEPT;
  }

// box for information purposes only
static void okbox(const char*t) {box(GTK_MESSAGE_INFO,t,GTK_STOCK_CLOSE,0);}

// box for warnings
void repwarn(const char*s) {
  if(usegui) 
    box(GTK_MESSAGE_WARNING,s,GTK_STOCK_CLOSE,0);
  else {
    fprintf(stderr,"Warning: %s\n",s);
    }
  }

// box for errors
void reperr(const char*s) {
  if(usegui) 
    box(GTK_MESSAGE_ERROR,s,GTK_STOCK_CLOSE,0);
  else {
    fprintf(stderr,"Error: %s\n",s);
    exit(16);
    }
  }

void fsgerr() {reperr("A filing system error occurred");}

void fserror() {char s[SLEN],t[SLEN*2];
  #ifdef _WIN32
    if(strerror_s(s,SLEN,errno)) strcpy(s,"general error");  // Windows version of threadsafe strerror()
  #else
    if(strerror_r(errno,s,SLEN)) strcpy(s,"general error");
  #endif
  sprintf(t,"Filing system error: %s",s);
  reperr(t);
  }

static int areyousure(char*action) { // general-purpose are-you-sure dialogue
  char s[1000];
  sprintf(s,"\n  Your work is not saved.  \n  Are you sure you want to %s?  \n",action);
  return box(GTK_MESSAGE_QUESTION,s,"  Proceed  ",GTK_STOCK_CANCEL);
  }

GtkWidget*fipdia;
static volatile int killfdflag=0;

static int fiptimeout(gpointer data) {
  DEB_GU {printf("<fiptimeout> fipdia=%p %d\n",(void*)fipdia,killfdflag); fflush(stdout);}
  if(killfdflag==0) return 1;
  killfdflag=2;
  gtk_dialog_response(GTK_DIALOG(fipdia),GTK_RESPONSE_ACCEPT);
  return 1;
  }

static void createfipdia() {
  DEB_GU printf("entering createfipdia()...\n");
  killfdflag=0;
  fipdia=gtk_message_dialog_new(GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,GTK_MESSAGE_INFO,GTK_BUTTONS_NONE,"\n  Filling in progress  \n");
  DEB_GU printf("fipdia=%p\n",(void*)fipdia);
  gtk_dialog_add_buttons(GTK_DIALOG(fipdia),"  Stop  ",GTK_RESPONSE_ACCEPT,NULL);
  gtk_widget_show_now(fipdia);
  while(gtk_events_pending()) gtk_main_iteration_do(0);
  }

static void runfipdia() {
  int id;
  DEB_GU printf("entering runfipdia %p...\n",(void*)fipdia);
  id=g_timeout_add(100,fiptimeout,0);
  gtk_dialog_run(GTK_DIALOG(fipdia));
  DEB_GU printf("leaving runfipdia %p...\n",(void*)fipdia);
  g_source_remove(id);
  gtk_widget_destroy(fipdia);
  }

// note this function is called from a different thread
void killfipdia(void) {
  if(!usegui) return;
  DEB_GU printf("killfipdia()\n");
  killfdflag=1;
  }

// GTK MENU HANDLERS

// simple menu handlers

static int checkoverwrite() {FILE*fp;
  fp=q_fopen(filename,"r");
  if(!fp) return 1;
  fclose(fp);
  return box(GTK_MESSAGE_QUESTION,"\n  That file already exists.  \n  Overwrite it?  \n","  Overwrite  ",GTK_STOCK_CANCEL);
  }

static void m_filenew(GtkWidget*w,gpointer data)      {
  if(!unsaved||areyousure("start again")) {
    a_filenew((intptr_t)data);
    strcpy(filenamebase,"");
    setwintitle();
    syncgui();
    compute(0);
    }
  }

static void m_fileopen(GtkWidget*w,gpointer data)     {
  if(!unsaved||areyousure("proceed")) if(filedia(0,"Open",".qxw","Qxw",0)) {
    a_load();
    setwintitle();
    syncgui();
    compute(0);
    }
  }

static void m_filesaveas(GtkWidget*w,gpointer data)   {
  if(filedia(0,"Save",".qxw","Qxw",1)&&checkoverwrite()) {
    a_save();
    setwintitle();
    }
  }

static void m_filesave(GtkWidget*w,gpointer data)     {
//  printf("m_filesave: filenamebase=%s havesavefn=%d\n",filenamebase,havesavefn);
  if(havesavefn&&strcmp(filenamebase,"")) {
    strcpy(filename,filenamebase);
    strcat(filename,".qxw");
    a_save();
    }
  else                        m_filesaveas(w,data);
  }

static void m_exportvls(GtkWidget*w,gpointer data)    {char t[SLEN+50]; if(filedia(t,"Export free light paths",".fl.txt","Plain text",1)) a_exportvls(t);}
static void m_importvls(GtkWidget*w,gpointer data)    {char t[SLEN+50]; if(filedia(t,"Import free light paths",".fl.txt","Plain text",0)) a_importvls(t);}

static void m_filequit(void)                          {if(!unsaved||areyousure("quit")) gtk_main_quit();}
static void m_undo(GtkWidget*w,gpointer data)         {if((uhead+UNDOS-1)%UNDOS==utail) return;undo_pop();gridchangen();syncgui();}
static void m_redo(GtkWidget*w,gpointer data)         {if(uhead==uhwm) return;uhead=(uhead+2)%UNDOS;undo_pop();gridchangen();syncgui();}
static void m_editgprop(GtkWidget*w,gpointer data)    {gpropdia();}
static void m_dsprop(GtkWidget*w,gpointer data)       {spropdia(1);}
static void m_sprop(GtkWidget*w,gpointer data)        {spropdia(0);}
static void m_dlprop(GtkWidget*w,gpointer data)       {lpropdia(1);}
static void m_lprop(GtkWidget*w,gpointer data)        {lpropdia(0);}
static void m_cellcont(GtkWidget*w,gpointer data)     {ccontdia();}
static void m_editlight(GtkWidget*w,gpointer data)    {editlightdia();}
// static void m_writeclue(GtkWidget*w,gpointer data)    {writecluedia();}
static void m_afctreat(GtkWidget*w,gpointer data)     {treatdia();}
static void m_editprefs(GtkWidget*w,gpointer data)    {prefsdia();}
static void m_showstats(GtkWidget*w,gpointer data)    {statsdia();}

static void m_symmclr(void)                           {symmr=1; symmm=0; symmd=0; syncsymmmenu();}
static void m_symm0(GtkWidget*w,gpointer data)        {symmr=(intptr_t)data&0xff;}
static void m_symm1(GtkWidget*w,gpointer data)        {symmm=(intptr_t)data&0xff;}
static void m_symm2(GtkWidget*w,gpointer data)        {symmd=(intptr_t)data&0xff;}

static void m_ifamode(GtkWidget*w,gpointer data)      {
  ifamode=(intptr_t)data&0xff;
  if(ifamode==0) gtk_paned_set_position(GTK_PANED(paned),mainw->allocation.width+100);
  else           gtk_paned_set_position(GTK_PANED(paned),mainw->allocation.width-FLISTWIDTH);
  gridchangen();
  }

static void m_zoom(GtkWidget*w,gpointer data) {int u;
  u=(intptr_t)data;
  if(u==-1) zoomf++;
  else if(u==-2) zoomf--;
  else zoomf=u*4;
  if(zoomf<0) zoomf=0;
  if(zoomf>16) zoomf=16;
  pxsq=zoompx[zoomf];
  syncgui();
  compute(0);
  }

static void m_fileexport(GtkWidget*w,gpointer data)  {
  switch((intptr_t)data) {
  case 0x401: if(filedia(0,"Export blank grid as EPS",".blank.eps","Encapsulated PostScript",1)) a_exportg(filename,4,0);break;
  case 0x402: if(filedia(0,"Export blank grid as SVG",".blank.svg","Scalable Vector Graphics",1)) a_exportg(filename,4,1);break;
  case 0x403: if(filedia(0,"Export blank grid as PNG",".blank.png","Portable Network Graphics",1)) a_exportg(filename,4,2);break;
  case 0x404:
    if(gshape[gtype]!=0) break;
    if(filedia(0,"Export blank grid as HTML",".blank.html","HyperText Markup Language",1)) a_exportgh(0x05,"");
    break;
  case 0x411: if(filedia(0,"Export filled grid as EPS",".eps","Encapsulated PostScript",1)) a_exportg(filename,lnis?6:2,0);break;
  case 0x412: if(filedia(0,"Export filled grid as SVG",".svg","Scalable Vector Graphics",1)) a_exportg(filename,lnis?6:2,1);break;
  case 0x413: if(filedia(0,"Export filled grid as PNG",".png","Portable Network Graphics",1)) a_exportg(filename,lnis?6:2,2);break;
  case 0x414:
    if(gshape[gtype]!=0) break;
    if(filedia(0,"Export filled grid as HTML",".html","HyperText Markup Language",1)) a_exportgh(lnis?7:3,"");
    break;
  case 0x420: if(filedia(0,"Export answers as plain text",".ans.txt","Plain text",1)) a_exporta(0);break;
  case 0x423: if(filedia(0,"Export answers as HTML",".ans.html","HyperText Markup Language",1)) a_exporta(1);break;
  case 0x433:
    if(gshape[gtype]!=0) break;
    if(filedia(0,"Export puzzle as HTML",".html","HyperText Markup Language",1)) a_exportgh(0x0d,"");
    break;
  case 0x434:if(filedia(0,"Export puzzle as HTML+SVG",".html","HyperText Markup Language",1)) a_exporthp(0,4,1);break;
  case 0x435:if(filedia(0,"Export puzzle as HTML+PNG",".html","HyperText Markup Language",1)) a_exporthp(0,4,2);break;
  case 0x443:
    if(gshape[gtype]!=0) break;
    if(filedia(0,"Export solution as HTML",".html","HyperText Markup Language",1)) a_exportgh(lnis?0x17:0x13,"");
    break;
  case 0x444:if(filedia(0,"Export solution as HTML+SVG",".html","HyperText Markup Language",1)) a_exporthp(1,lnis?6:2,1);break;
  case 0x445:if(filedia(0,"Export solution as HTML+PNG",".html","HyperText Markup Language",1)) a_exporthp(1,lnis?6:2,2);break;
  case 0x451:
    if(gshape[gtype]!=0) break;
    if(filedia(0,"Export as Crossword Compiler XML","-ccw.xml","Crossword Compiler Extensible Markup Language",1)) a_exportccwxml(filename);
    break;
  default: assert(0);
    }
  }

static void selchange(void) {int i,j,d;
  nsel=0;
  if     (selmode==0) for(i=0;i<width;i++) for(j=0;j<height;j++) {if(gsq[i][j].fl&16) nsel++;}
  else if(selmode==1) for(i=0;i<width;i++) for(j=0;j<height;j++) for(d=0;d<ndir[gtype];d++) {if(isstartoflight(i,j,d)&&(gsq[i][j].dsel&(1<<d))) nsel++;}
  else if(selmode==2) for(i=0;i<nvl;i++) {if(vls[i].sel) nsel++;}
  syncselmenu();
  refreshsel();
  }

static void selcell(int x,int y,int f) {int i,l,gx[MXCL],gy[MXCL];
  l=getmergegroup(gx,gy,x,y);
  if(f) f=16;
  for(i=0;i<l;i++) gsq[gx[i]][gy[i]].fl=(gsq[gx[i]][gy[i]].fl&~16)|f;
  }

static void selnocells() {int i,j;
  for(i=0;i<width;i++) for(j=0;j<height;j++) gsq[i][j].fl&=~16;
  }

static void selnolights() {int i,j;
  for(i=0;i<width;i++) for(j=0;j<height;j++) gsq[i][j].dsel=0;
  }

static void selnovls() {int i;
  for(i=0;i<nvl;i++) vls[i].sel=0;
  }

// clear selection
static void m_selnone(GtkWidget*w,gpointer data) {
  selnolights();
  selnocells();
  selnovls();
  selchange();
  }

// invert selection
static void m_selinv(GtkWidget*w,gpointer data) {int d,i,j;
  for(i=0;i<width;i++) for(j=0;j<height;j++) {
    if     (selmode==0) {if(isclear(i,j)) gsq[i][j].fl^=16;}
    else if(selmode==1) for(d=0;d<ndir[gtype];d++) {if(isstartoflight(i,j,d)) gsq[i][j].dsel^=1<<d;}
    else if(selmode==2) for(i=0;i<nvl;i++) vls[i].sel=!vls[i].sel;
    }
  selchange();
  }

// select everything
static void m_selall(GtkWidget*w,gpointer data) {int d,i,j;
  for(i=0;i<width;i++) for(j=0;j<height;j++) {
    if     (selmode==0) {if(isclear(i,j)) gsq[i][j].fl|=16;}
    else if(selmode==1) {for(d=0;d<ndir[gtype];d++) if(isstartoflight(i,j,d)) gsq[i][j].dsel|=1<<d;}
    }
  if(selmode==2) for(i=0;i<nvl;i++) vls[i].sel=1;
  selchange();
  }

// switch to light mode
static void sel_tol(void) {int d,i,j;
  if(selmode==1||selmode==2) return;
  DEB_GU printf("seltol()\n");
  selnolights();
  for(i=0;i<width;i++) for(j=0;j<height;j++) if(gsq[i][j].fl&16) for(d=0;d<ndir[gtype];d++) {
    DEB_GU printf("  %d %d %d\n",i,j,d);
    sellight(i,j,d,1);
    }
  selmode=1;
  }

// switch to cell mode
static void sel_toc(void) {int i,j,d,k,l,m,n,gx[MXCL],gy[MXCL],lx[MXCL],ly[MXCL];
  DEB_GU printf("seltoc()\n");
  if(selmode==0) return;
  selnocells();
  if(selmode==1) {
    for(i=0;i<width;i++) for(j=0;j<height;j++) for(d=0;d<ndir[gtype];d++)
      if(isstartoflight(i,j,d)&&issellight(i,j,d)) {
      DEB_GU printf("  %d %d %d\n",i,j,d);
      l=getlight(lx,ly,i,j,d);
      for(k=0;k<l;k++) {
        m=getmergegroup(gx,gy,lx[k],ly[k]);
        for(n=0;n<m;n++) gsq[gx[n]][gy[n]].fl|=16;
        }
      }
    }
  else if(selmode==2) {
    for(i=0;i<nvl;i++) for(j=0;j<vls[i].l;j++) {
      m=getmergegroup(gx,gy,vls[i].x[j],vls[i].y[j]);
      for(n=0;n<m;n++) gsq[gx[n]][gy[n]].fl|=16;
      }
    }
  selmode=0;
  }

// selection commands: single light
static void m_sellight(GtkWidget*w,gpointer data) {
  if(curdir>=100) {
    if(curdir>=100+nvl) return;
    if(selmode!=2) m_selnone(w,data);
    selmode=2;
    vls[curdir-100].sel=!vls[curdir-100].sel;
    }
  else {
    if(selmode!=1) m_selnone(w,data);
    selmode=1;
    sellight(curx,cury,curdir,!issellight(curx,cury,curdir));
    }
  selchange();
  }

// all lights parallel
static void m_sellpar(GtkWidget*w,gpointer data) {int f,i,j;
  if(curdir>=100) return;
  if(selmode!=1) m_selnone(w,data);
  selmode=1;
  f=issellight(curx,cury,curdir);
  for(i=0;i<width;i++) for(j=0;j<height;j++) if(isstartoflight(i,j,curdir)) sellight(i,j,curdir,!f);
  selchange();
  }

// single cell
static void m_selcell(GtkWidget*w,gpointer data) {int f;
  if(selmode!=0) m_selnone(w,data);
  selmode=0;
  if(!isclear(curx,cury)) return;
  f=(~gsq[curx][cury].fl)&16;
  selcell(curx,cury,f);
  selchange();
  }

static void m_selcover(GtkWidget*w,gpointer data) {int i,j;
  if(selmode!=0) m_selnone(w,data);
  selmode=0;
  for(i=0;i<width;i++) for(j=0;j<height;j++)
    if(isownmergerep(i,j)) selcell(i,j,gsq[i][j].sp.spor);
  selchange();
  }

static void m_selcunch(GtkWidget*w,gpointer data) {int i,j,k,nd;
  if(selmode!=0) m_selnone(w,data);
  selmode=0;
  for(i=0;i<width;i++) for(j=0;j<height;j++)
    if(isownmergerep(i,j)) {
      if(getdech(i,j)) nd=ndir[gtype];
      else             nd=1;
      for(k=0;k<nd;k++) {
        if(gsq[i][j].ctlen[k]>0&&gsq[i][j].ents[k][0]!=0) {
          selcell(i,j,gsq[i][j].ents[k][0]->checking==1);
          break;
          }
        }
      }
  selchange();
  }

static void m_selctreat(GtkWidget*w,gpointer data) {int i,j;
  if(selmode!=0) m_selnone(w,data);
  selmode=0;
  for(i=0;i<width;i++) for(j=0;j<height;j++)
    if(isownmergerep(i,j)) selcell(i,j,gsq[i][j].sp.spor?gsq[i][j].sp.ten:dsp.ten);
  selchange();
  }

static void m_sellover(GtkWidget*w,gpointer data) {int d,i,j;
  if(selmode!=1) m_selnone(w,data);
  selmode=1;
  for(i=0;i<width;i++) for(j=0;j<height;j++) for(d=0;d<ndir[gtype];d++)
    if(isstartoflight(i,j,d)) sellight(i,j,d,gsq[i][j].lp[d].lpor);
  selchange();
  }

static void m_selltreat(GtkWidget*w,gpointer data) {int d,i,j;
  if(selmode!=1) m_selnone(w,data);
  selmode=1;
  for(i=0;i<width;i++) for(j=0;j<height;j++) for(d=0;d<ndir[gtype];d++)
    if(isstartoflight(i,j,d)) sellight(i,j,d,getlprop(i,j,d)->ten);
  selchange();
  }

// select violating lights
static void m_selviol(GtkWidget*w,gpointer data) {int d,i,j,v;
  if(selmode!=1) m_selnone(w,data);
  selmode=1;
  v=(intptr_t)data;
  DEB_GU printf("selviol(%d)\n",v);
  for(i=0;i<width;i++) for(j=0;j<height;j++) for(d=0;d<ndir[gtype];d++)
    if(isstartoflight(i,j,d)) sellight(i,j,d,!!(gsq[i][j].vflags[d]&v));
  selchange();
  }

static int issqinvl(int x,int y,int v) {int cx,cy,gx,gy,j;
  getmergerep(&cx,&cy,x,y);
  for(j=0;j<vls[v].l;j++) {
    getmergerep(&gx,&gy,vls[v].x[j],vls[v].y[j]);
    if(cx==gx&&cy==gy) return 1;
    }
  return 0;
  }

static void m_selfvl(GtkWidget*w,gpointer data) {int i,j;
  if(nvl==0) {reperr("No free lights\nhave been defined");return;}
  if(selmode!=2) {
    m_selnone(w,data);
    for(i=0;i<nvl;i++) if(issqinvl(curx,cury,i)) break;
    j=i%nvl; // otherwise, select the first one
    }
  else {
    j=-1;
    for(i=0;i<nvl;i++) if(vls[i].sel) j=i; // find last selected vl
    j=(j+1)%nvl; // one after
    }
  for(i=0;i<nvl;i++) vls[i].sel=0;
  vls[j].sel=1;
  selmode=2;
  selchange();
  }

static void m_selmode(GtkWidget*w,gpointer data) {
  if(selmode==0) sel_tol();
  else           sel_toc();
  selchange();
  }

static void curmoved(void) {
  static int odir=0;
  refreshcur();
  if(odir>=100||curdir>=100) refreshsel();
  odir=curdir;
  curmf=1;
  }

static void gridchangen(void) { // change of grid, but without an "undo push"
  int d,i,j;
  // only squares at start of lights can have dsel information
  for(i=0;i<width;i++) for(j=0;j<height;j++) for(d=0;d<ndir[gtype];d++) if(!isstartoflight(i,j,d)) gsq[i][j].dsel&=~(1<<d);
  selchange();
  curmoved();
  donumbers();
  refreshnum();
  compute(0);
  }

static void gridchange(void) {
  gridchangen();
  undo_push();
  }

static void chkresetat() {int i,j;
  for(i=0;i<NMSG;i++) if(treatorder[i]>0&&treat_lab[treatmode][i]) for(j=0;j<MXFL;j++) if(treatcstr[i][j]!=ABM_ALL) goto ew0;
  return; // nothing to reset
ew0:
  if(box(GTK_MESSAGE_QUESTION,"\n  Reset answer  \n  treatment constraints?  \n",GTK_STOCK_YES,GTK_STOCK_NO)==0) return;
  for(i=0;i<NMSG;i++) if(treatorder[i]>0&&treat_lab[treatmode][i]) for(j=0;j<MXFL;j++) treatcstr[i][j]=ABM_ALL;
  }

static void m_eraseall(void) {int i,j;
  for(i=0;i<width;i++) for(j=0;j<height;j++) {clrcont(i,j); refreshsqmg(i,j);}
  chkresetat();
  gridchange();
  }

static void m_erasesel(void) {int i,j;
  if(selmode!=0) {
    sel_toc();
    selchange();
    }
  for(i=0;i<width;i++) for(j=0;j<height;j++) {if(gsq[i][j].fl&16) {clrcont(i,j);refreshsqmg(i,j);}}
  chkresetat();
  gridchange();
  }

static void m_dictionaries(GtkWidget*w,gpointer data) {dictldia();}
static void m_dstats(GtkWidget*w,gpointer data) {dstatsdia();}
static void m_alphabet(GtkWidget*w,gpointer data) {alphadia();}

// convert tentative entries (where feasible letter bitmap has exactly one bit set) to firm entries
static void m_accept(GtkWidget*w,gpointer data) {
  int de,nd,f,i,j,x,y;
  ABM m;
  struct entry*e;
  ABM b;

  DEB_GU printf("m_accept\n");
  for(x=0;x<width;x++) for(y=0;y<height;y++) {


    de=getdech(x,y);
    if(de==0) nd=1,de=1; else nd=ndir[gtype];
    for(i=0;i<nd;i++) {
      for(j=0;j<gsq[x][y].ctlen[i];j++) {
        if(gsq[x][y].ents[i][j]!=0) m=gsq[x][y].ents[i][j]->flbmh; // get hints bitmap
        else                        m=0;
//          printf("%d %d %16llx\n",x,y,m);
        if(onebit(m)) gsq[x][y].ctbm[i][j]=m; // could consider removing the onebit() test but potentially confusing?
        }
      }
    refreshsqmg(x,y);

    }
  f=0;
  for(i=0;i<NMSG;i++) if(treatorder[i]>0&&treat_lab[treatmode][i]) for(j=0;j<MXFL;j++) { // apply new constraints to treatment messages
    e=treatmsge0[i][j];  // get entry for message character
    if(e) {
      b=treatcstr[i][j]&e->flbmh;
      if(b!=treatcstr[i][j]) treatcstr[i][j]=b,f=1;
      }
    }
  if(f) {okbox("\n  Answer treatment  \n  constraints updated  \n");}
  gridchange();
  }

static void m_unban(GtkWidget*w,gpointer data) {
  int i;
  for(i=0;i<atotal;i++) ansp[i]->banned=0;
  gridchange();
  }

// run filler
static void m_autofill(GtkWidget*w,gpointer data) {
  int mode;
  if(filler_status==0) return; // already running?
  mode=(intptr_t)data; // selected mode (1=all, 2=selection)
  if(mode==2&&selmode!=0) {
    sel_toc();
    selchange();
    }
  createfipdia();
  if(compute(mode)) { reperr("Could not start filler"); return; }
  runfipdia();
  DEB_GU printf("stopping filler...\n");
  filler_stop();
  DEB_GU printf("stopped filler, status=%d\n",filler_status);
  setposslabel("");
  switch(filler_status) {
  case -4:
  case -3: reperr("Error generating lists of feasible lights");break;
  case -2: reperr("Out of stack space");break;
  case -1: reperr("Out of memory");break;
  case 1: reperr("No fill found");break;
    }
  }

// grid edit operations

static void editblock (int x,int y,int adv) {             symmdo(a_editblock ,0,x,y,0); if(adv&&curdir<100) stepforwmifingrid(&curx,&cury,curdir); gridchange();}
static void editempty (int x,int y,int adv) {clrcont(x,y);symmdo(a_editempty ,0,x,y,0); if(adv&&curdir<100) stepforwmifingrid(&curx,&cury,curdir); gridchange();}
static void editcutout(int x,int y,int adv) {             symmdo(a_editcutout,0,x,y,0); if(adv&&curdir<100) stepforwmifingrid(&curx,&cury,curdir); gridchange();}

// called from menu
static void m_editblock (GtkWidget*w,gpointer data) {editblock (curx,cury,1);}
static void m_editempty (GtkWidget*w,gpointer data) {editempty (curx,cury,1);}
static void m_editcutout(GtkWidget*w,gpointer data) {editcutout(curx,cury,1);}

// bar behind cursor
static void m_editbarb(GtkWidget*w,gpointer data) {
  if(curdir>=100) return;
  symmdo(a_editbar,!isbar(curx,cury,curdir+ndir[gtype]),curx,cury,curdir+ndir[gtype]);
  gridchange();
  }

// merge/join ahead
static void m_editmerge(GtkWidget*w,gpointer data) {
  if(curdir>=100) return;
  symmdo(a_editmerge,!ismerge(curx,cury,curdir),curx,cury,curdir);
  gridchange();
  }

static void m_insrow(GtkWidget*w,gpointer data) {int i,j,y;
  y=cury+(intptr_t)data; // now always "insert above"
  if(height>=MXSZ) return;
  for(j=height;j>y;j--) for(i=0;i<width;i++) gsq[i][j]=gsq[i][j-1];
  for(i=0;i<width;i++) initsquare(i,y);
  for(i=0;i<nvl;i++) for(j=0;j<vls[i].l;j++) if(vls[i].y[j]>=y) vls[i].y[j]++;
  height++;
  if((intptr_t)data==0) {cury++; curmoved();}
  gridchange();
  syncgui();
  }

static void m_inscol(GtkWidget*w,gpointer data) {int i,j,x;
  x=curx+(intptr_t)data; // now always "insert to left"
  if(width>=MXSZ) return;
  for(i=width;i>x;i--) for(j=0;j<height;j++) gsq[i][j]=gsq[i-1][j];
  for(j=0;j<height;j++) initsquare(x,j);
  for(i=0;i<nvl;i++) for(j=0;j<vls[i].l;j++) if(vls[i].x[j]>=x) vls[i].x[j]++;
  width++;
  if((intptr_t)data==0) {curx++; curmoved();}
  gridchange();
  syncgui();
  }

static void m_delrow(GtkWidget*w,gpointer data) {int bm,i,j,i0,j0;
  if(height<2) return;
       if(gtype==1) bm=4;
  else if(gtype==2) bm=6;
  else              bm=2;
  if(cury<height-1) for(i=0;i<width;i++) gsq[i][cury+1].bars|=gsq[i][cury].bars&bm; // preserve bars on both sides of row
  for(j=cury;j<height-1;j++) for(i=0;i<width;i++) gsq[i][j]=gsq[i][j+1];
  for(i=0,i0=0;i<nvl;i++) {
    for(j=0,j0=0;j<vls[i].l;j++) {
           if(vls[i].y[j]<cury) vls[i0].y[j0++]=vls[i].y[j];
      else if(vls[i].y[j]>cury) vls[i0].y[j0++]=vls[i].y[j]-1;
      }
    vls[i0].l=j0;
    if(j0>0) i0++; // have we lost the free light completely?
    }
  nvl=i0;
  height--;
  gridchange();
  syncgui();
  }

static void m_delcol(GtkWidget*w,gpointer data) {int bm,i,j,i0,j0;
  if(width<2) return;
  if(gtype==1) bm=3;
  else         bm=1;
  if(curx<width-1) for(j=0;j<height;j++) gsq[curx+1][j].bars|=gsq[curx][j].bars&bm; // preserve bars on both sides of column
  for(i=curx;i<width-1;i++) for(j=0;j<height;j++) gsq[i][j]=gsq[i+1][j];
  for(i=0,i0=0;i<nvl;i++) {
    for(j=0,j0=0;j<vls[i].l;j++) {
           if(vls[i].x[j]<curx) vls[i0].x[j0++]=vls[i].x[j];
      else if(vls[i].x[j]>curx) vls[i0].x[j0++]=vls[i].x[j]-1;
      }
    vls[i0].l=j0;
    if(j0>0) i0++; // have we lost the free light completely?
    }
  nvl=i0;
  width--;
  gridchange();
  syncgui();
  }

// flip grid in main diagonal
static void m_editflip(GtkWidget*w,gpointer data) {int i,j,k,t;struct square s;struct lprop l;ABM p[MXCT];
  if(gshape[gtype]>2) return;
  for(i=0;i<MXSZ;i++) for(j=i+1;j<MXSZ;j++) {s=gsq[j][i];gsq[j][i]=gsq[i][j];gsq[i][j]=s;}
  for(i=0;i<MXSZ;i++) for(j=0;j<MXSZ;j++) {
    t=gsq[i][j].bars;
    if(gshape[gtype]==0) t=((t&1)<<1)|((t&2)>>1); // swap bars around
    else         t=((t&1)<<2)|( t&2    )|((t&4)>>2);
    gsq[i][j].bars=t;
    t=gsq[i][j].merge;
    if(gshape[gtype]==0) t=((t&1)<<1)|((t&2)>>1); // swap merges around
    else         t=((t&1)<<2)|( t&2    )|((t&4)>>2);
    gsq[i][j].merge=t;
    t=gsq[i][j].dsel;
    if(gshape[gtype]==0) t=((t&1)<<1)|((t&2)>>1); // swap directional selects around
    else         t=((t&1)<<2)|( t&2    )|((t&4)>>2);
    gsq[i][j].dsel=t;
    k=(gshape[gtype]==0)?1:2;
    l=gsq[i][j].lp[0];gsq[i][j].lp[0]=gsq[i][j].lp[k];gsq[i][j].lp[k]=l;
    // if dechecked we want to swap the across and down contents
    if(getdech(i,j)) {
      memcpy(p,gsq[i][j].ctbm[0],sizeof(p));
      memcpy(gsq[i][j].ctbm[0],gsq[i][j].ctbm[k],sizeof(p));
      memcpy(gsq[i][j].ctbm[k],p,sizeof(p));
      t=gsq[i][j].ctlen[0]; gsq[i][j].ctlen[0]=gsq[i][j].ctlen[k]; gsq[i][j].ctlen[k]=t;
      }
    }
  for(i=0;i<nvl;i++) for(j=0;j<vls[i].l;j++) t=vls[i].x[j],vls[i].x[j]=vls[i].y[j],vls[i].y[j]=t;
  t=height;height=width;width=t; // exchange width and height
  if(symmm==1||symmm==2) symmm=3-symmm; // flip symmetries
  if(symmd==1||symmd==2) symmd=3-symmd;
  if     (gtype== 1) gtype= 2;
  else if(gtype== 2) gtype= 1;
  else if(gtype== 5) gtype= 6;
  else if(gtype== 6) gtype= 5;
  else if(gtype== 7) gtype= 8;
  else if(gtype== 8) gtype= 7;
  else if(gtype==10) gtype=11;
  else if(gtype==11) gtype=10;
  gridchange();
  syncgui();
  }

// rotate (circular) grid
static void m_editrot(GtkWidget*w,gpointer data) {int i,j; struct square t,u;
  if(gshape[gtype]<3) return;
  if((intptr_t)data==0xf001)
    for(j=0;j<height;j++) {t=gsq[width-1][j];for(i=      0;i<width;i++) u=gsq[i][j],gsq[i][j]=t,t=u;}
  else
    for(j=0;j<height;j++) {t=gsq[      0][j];for(i=width-1;i>=0   ;i--) u=gsq[i][j],gsq[i][j]=t,t=u;}
  gridchange();
  syncgui();
  }

static void m_vlnew(GtkWidget*w,gpointer data) {
  if(nvl>=NVL) {reperr("Limit on number of\nfree lights reached");return;}
  m_selnone(w,data);
  selmode=2;
  vls[nvl].x[0]=curx;
  vls[nvl].y[0]=cury;
  vls[nvl].l=1;
  vls[nvl].sel=1;
  resetlp(&vls[nvl].lp);
  nvl++;
  gridchange();
  }

static int getselvl() {int i;
  if(selmode!=2) return -1;
  if(nsel!=1) return -1;
  for(i=0;i<nvl;i++) if(vls[i].sel) return i;
  return -1;
  }

static void m_vlextend(GtkWidget*w,gpointer data) {int i;
  i=getselvl();
  if(i==-1) return;
  if(vls[i].l>=MXCL) {reperr("Limit on length of\nfree light reached");return;}
  vls[i].x[vls[i].l]=curx;
  vls[i].y[vls[i].l]=cury;
  vls[i].l++;
  DEB_GU printf("VL %d l=%d\n",i,vls[i].l);
  gridchange();
  }

static void m_vldelete(GtkWidget*w,gpointer data) {int i,j,d;
  if(selmode!=2) return;
  d=-1;
  for(i=0,j=0;i<nvl;i++)
    if(!vls[i].sel) {
      if(i+100==curdir) d=j; // keep track of where current vl (if any) ends up
      memmove(vls+j,vls+i,sizeof(struct vl)),j++;
      }
  if(curdir>=100) {
    if(d>=0) curdir=d+100;
    else     curdir=0;
    }
  nvl=j;
  gridchange();
  }

static void m_vlcurtail(GtkWidget*w,gpointer data) {int i;
  i=getselvl();
  if(i==-1) return;
  if(vls[i].l<2) {m_vldelete(w,data);return;}
  vls[i].l--;
  DEB_GU printf("VL %d l=%d\n",i,vls[i].l);
  gridchange();
  }

static void m_vlmodify(GtkWidget*w,gpointer data) {int i;
  i=getselvl();
  if(i==-1) return;
  mvldia(i);
  }




static void m_helpabout(GtkWidget*w,gpointer data) {char s[4000];
  sprintf(s,
  "Qxw release %s\n\n"
  "Copyright 2011-2020 Mark Owen; Windows port by Peter Flippant\n"
  "\n"
  "This program is free software; you can redistribute it and/or modify "
  "it under the terms of version 2 of the GNU General Public License as "
  "published by the Free Software Foundation.\n"
  "\n"
  "This program is distributed in the hope that it will be useful, "
  "but WITHOUT ANY WARRANTY; without even the implied warranty of "
  "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
  "GNU General Public License for more details.\n"
  "\n"
  "You should have received a copy of the GNU General Public License along "
  "with this program; if not, write to the Free Software Foundation, Inc., "
  "51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.\n"
  "\n"
  "Visit http://www.quinapalus.com/ for more information and to download "
  "the PDF manual.\n"
  "\n"
  "Please e-mail qxw@quinapalus.com with bug reports and suggestions."
  ,RELEASE);
  okbox(s);}

static int w_destroy(void) {gtk_main_quit();return 0;}
static int w_delete(void) {return !(!unsaved||areyousure("quit"));}






static void moveleft(int*x,int*y) {
  switch(gtype) {
    case 0:case 1:case 2:case 6:case 8:(*x)--;break;
    case 7:case 10:case 12:
      if(*x==0) *y=height-1-*y;   // fall through
    case 3:case 4:case 5:case 9:case 11:
      *x=(*x+width-1)%width;
      break;
    }
  }

static void moveright(int*x,int*y) {
  switch(gtype) {
    case 0:case 1:case 2:case 6:case 8:(*x)++;break;
    case 7:case 10:case 12:
      if(*x==width-1) *y=height-1-*y;   // fall through
    case 3:case 4:case 5:case 9:case 11:
      *x=(*x+1)%width;
      break;
    }
  }

static void moveup(int*x,int*y) {
  switch(gtype) {
    case 0:case 1:case 2:case 3:case 4:case 5:case 7:(*y)--;break;
    case 8:case 11:case 12:
      if(*y==0) *x=width-1-*x; // fall through
    case 6:case 9:case 10:
      *y=(*y+height-1)%height;
      break;
    }
  }

static void movedown(int*x,int*y) {
  switch(gtype) {
    case 0:case 1:case 2:case 3:case 4:case 5:case 7:(*y)++;break;
    case 8:case 11:case 12:
      if(*y==height-1) *x=width-1-*x; // fall through
    case 6:case 9:case 10:
      *y=(*y+1)%height;
      break;
    }
  }

static void movehome(int*x,int*y) {int l,lx[MXCL],ly[MXCL];
  l=getlight(lx,ly,*x,*y,curdir);
  if(l<1) return;
  *x=lx[0];*y=ly[0];
  }

static void moveend(int*x,int*y) {int l,lx[MXCL],ly[MXCL];
  l=getlight(lx,ly,*x,*y,curdir);
  if(l<1) return;
  *x=lx[l-1];*y=ly[l-1];
  }

static void nextdir() {
  int m;
  if(ismux(curx,cury,curdir)) {
    m=getmaxmux(curx,cury,curdir);
    if(m>1) {
      curmux++;
      if(curmux<m) return;
      }
    }
  do{
    curdir++;
    curmux=0;
    if(curdir==ndir[gtype]) curdir=100;
    if(curdir>=100+nvl) curdir=0;
    if(curdir<ndir[gtype]) return;
    } while(!issqinvl(curx,cury,curdir-100));
  }

static void prevdir() {
  int m;
  if(ismux(curx,cury,curdir)) {
    m=getmaxmux(curx,cury,curdir);
    if(m>1) {
      curmux--;
      if(curmux>=m) {curmux=m-1; return;}
      if(curmux>=0) return;
      }
    }
  do{
    curdir--;
    curmux=0;
    if(curdir<0) curdir=100+nvl-1;
    if(curdir==99) curdir=ndir[gtype]-1;
    if(ismux(curx,cury,curdir)) {
      m=getmaxmux(curx,cury,curdir);
      if(m>1) curmux=m-1;
      }
    if(curdir<ndir[gtype]) return;
    } while(!issqinvl(curx,cury,curdir-100));
  }



// GTK EVENT HANDLERS

static void entericc(int i) {
  if(seteicc(curx,cury,curdir,i)) ccontdia(); // open dialogue if not a simple case
  refreshsqmg(curx,cury);
  refreshnum();
  if(curdir<100) stepforwmifingrid(&curx,&cury,curdir);
  undo_push();
  }

static int tryuchar(uchar uc) {
  int i;
  if(!ISUGRAPH(uc)) return 0;
  if(isdisallowed(uc)) return 0;
  for(i=1;i<MAXICC;i++) if(icctouchar[i]==uc) {entericc(i); return 1;}
  return 0;
  }

static void curleft () {int x,y; if(curdir>=100) curdir=0; x=curx;y=cury;moveleft (&x,&y);if(isingrid(x,y)) curx=x,cury=y;}
static void curright() {int x,y; if(curdir>=100) curdir=0; x=curx;y=cury;moveright(&x,&y);if(isingrid(x,y)) curx=x,cury=y;}
static void curup   () {int x,y; if(curdir>=100) curdir=0; x=curx;y=cury;moveup   (&x,&y);if(isingrid(x,y)) curx=x,cury=y;}
static void curdown () {int x,y; if(curdir>=100) curdir=0; x=curx;y=cury;movedown (&x,&y);if(isingrid(x,y)) curx=x,cury=y;}

// process keyboard
static int keypress(GtkWidget *widget, GdkEventKey *event) {
  int k,x,y;
  uchar uc;
  k=event->keyval;
  uc=gdk_keyval_to_unicode(k);
  if(tryuchar(uc)) goto done;
  if(tryuchar(TOUUPPER(uc))) goto done;
  if(tryuchar(TOULOWER(uc))) goto done;
  if((event->state&12)!=0) return 0; // don't handle anything with CTRL or ALT modifiers

       if(k==' '                    ) {if(curdir<100) stepforwmifingrid(&curx,&cury,curdir);}
  else if(k==GDK_Tab                ) {clrcont(curx,cury); refreshsqmg(curx,cury);refreshnum(); stepforwmifingrid(&curx,&cury,curdir); undo_push();}
  else if(k==GDK_ISO_Left_Tab       ) {if(stepbackmifingrid(&curx,&cury,curdir)) {clrcont(curx,cury); refreshsqmg(curx,cury);refreshnum(); undo_push();}}
  else if(k==GDK_Left               ) curleft ();
  else if(k==GDK_Right              ) curright();
  else if(k==GDK_Up                 ) curup   ();
  else if(k==GDK_Down               ) curdown ();

  else if(k==GDK_Home               ) {x=curx;y=cury;movehome (&x,&y);if(isingrid(x,y)) curx=x,cury=y;}
  else if(k==GDK_End                ) {x=curx;y=cury;moveend  (&x,&y);if(isingrid(x,y)) curx=x,cury=y;}

  else if((k==GDK_Page_Up          )) {prevdir();}
  else if((k==GDK_Page_Down||k=='/')) {nextdir();}

  else if(k=='.'                    ) {editempty (curx,cury,1);}
  else if(k==','                    ) {editblock (curx,cury,1);}

  else if(k==GDK_BackSpace          ) {if(curdir<100) stepbackifingrid(&curx,&cury,curdir);}
  else return 0;
done:
  curmoved();
  compute(0);
  return 1;
  }

// convert screen coords to internal square coords; k is bitmap of sufficiently nearby edges
static void ptrtosq(int*x,int*y,int*k,int x0,int y0) {double u,v,u0,v0,r=0,t=0,xa=0,ya=0,xb=0,yb=0;int i,j;
  u0=((double)x0-bawdpx)/pxsq;v0=((double)y0-bawdpx)/pxsq;
  *k=0;
  switch(gshape[gtype]) {
  case 0:
    i=(int)floor(u0);u=u0-i;
    j=(int)floor(v0);v=v0-j;
    *x=i;*y=j;
    break;
  case 1:
    i=(int)floor(u0/1.2);u=u0-i*1.2;
    if((i&1)==0) {
      j=(int)floor(v0/1.4);v=v0-j*1.4;
      if(u>=0.4) {*x=i;*y=j;break;}
      if( 0.7*u+0.4*v<0.28) {*x=i-1;*y=j-1;break;}
      if(-0.7*u+0.4*v>0.28) {*x=i-1;*y=j;  break;}
      *x=i;*y=j;break;
    } else {
      j=(int)floor((v0-0.7)/1.4);v=v0-0.7-j*1.4;
      if(u>=0.4) {*x=i;*y=j;break;}
      if( 0.7*u+0.4*v<0.28) {*x=i-1;*y=j;  break;}
      if(-0.7*u+0.4*v>0.28) {*x=i-1;*y=j+1;break;}
      *x=i;*y=j;break;
      }
  case 2:
    j=(int)floor(v0/1.2);v=v0-j*1.2;
    if((j&1)==0) {
      i=(int)floor(u0/1.4);u=u0-i*1.4;
      if(v>=0.4) {*y=j;*x=i;break;}
      if( 0.7*v+0.4*u<0.28) {*y=j-1;*x=i-1;break;}
      if(-0.7*v+0.4*u>0.28) {*y=j-1;*x=i;  break;}
      *y=j;*x=i;break;
    } else {
      i=(int)floor((u0-0.7)/1.4);u=u0-0.7-i*1.4;
      if(v>=0.4) {*y=j;*x=i;break;}
      if( 0.7*v+0.4*u<0.28) {*y=j-1;*x=i;  break;}
      if(-0.7*v+0.4*u>0.28) {*y=j-1;*x=i+1;break;}
      *y=j;*x=i;break;
      }
  case 3:case 4:
    u=u0-height;v=v0-height;
    r=sqrt(u*u+v*v);
    if(r<1e-3) {*x=-1;*y=-1;return;}
    r=height-r;
    t=atan2(u,-v)*width/2/PI;
    if(gtype==4) t+=.5;
    if(t<0) t+=width;
    *x=(int)floor(t);
    *y=(int)floor(r);
    break;
    }
  switch(gshape[gtype]) { // click-near-edge bitmap
  case 0:case 1:case 2:
    for(i=0;i<ndir[gtype]*2;i++) {
      edgecoords(&xa,&ya,&xb,&yb,*x,*y,i);
      xb-=xa;yb-=ya;
      r=sqrt(xb*xb+yb*yb);xb/=r;yb/=r;
      xa=u0-xa;ya=v0-ya;
      if(fabs(xb*ya-xa*yb)<PXEDGE) *k|=1<<i;
      }
    break;
  case 3:case 4:
    if(r-*y>1-PXEDGE            ) *k|=2;
    if(r-*y<  PXEDGE            ) *k|=8;
    r=height-r;
    if(t-*x>1-PXEDGE/((r<2)?1:(r-1))) *k|=1;
    if(t-*x<  PXEDGE/((r<2)?1:(r-1))) *k|=4;
    break;
    }
  }

int dragflag=-1; // store selectedness state of square where shift-drag starts, or -1 if none in progress

static void mousel(int x,int y) { // left button
  if(selmode!=0) m_selnone(0,0);
  selmode=0;
  if(dragflag==-1) dragflag=gsq[x][y].fl&16; // set f if at beginning of shift-drag
  if((gsq[x][y].fl&16)==dragflag) {
    selcell(x,y,!dragflag);
    selchange();
    }
  }

static void mouser(int x,int y) { // right button
  if(curdir>=100) return;
  if(selmode!=1) m_selnone(0,0);
  selmode=1;
  if(dragflag==-1) dragflag=issellight(x,y,curdir);
  if(issellight(x,y,curdir)==dragflag) {
    sellight(x,y,curdir,!dragflag);
    selchange();
    }
  }

// pointer motion
static gint mousemove(GtkWidget*widget,GdkEventMotion*event) {
  int e,k,x,y;
  ptrtosq(&x,&y,&e,(int)floor(event->x+.5),(int)floor(event->y));
  k=(int)(event->state); // buttons and modifiers
//  DEB_RF printf("mouse move event (%d,%d) %d e=%02x\n",x,y,k,e);
  if(!isingrid(x,y)) return 0;
  if((k&GDK_SHIFT_MASK)==0) {dragflag=-1;return 0;} // shift not held down: reset f
  if     (k&GDK_BUTTON3_MASK) mouser(x,y);
  else if(k&GDK_BUTTON1_MASK) mousel(x,y);
  else dragflag=-1; // button not held down: reset dragflag
  return 0;
  }

// click in grid area
static gint button_press_event(GtkWidget*widget,GdkEventButton*event) {
  int b,e,ee,k,x,y;
  ptrtosq(&x,&y,&e,(int)floor(event->x+.5),(int)floor(event->y));
  k=event->state;
  DEB_RF printf("button press event (%f,%f) -> (%d,%d) e=%02x button=%08x type=%08x state=%08x\n",event->x,event->y,x,y,e,(int)event->button,(int)event->type,k);
  if(event->type==GDK_BUTTON_RELEASE) dragflag=-1;
  if(event->type!=GDK_BUTTON_PRESS) goto ew0;
  if(k&GDK_SHIFT_MASK) {
    if     (event->button==3) mouser(x,y);
    else if(event->button==1) mousel(x,y);
    return 0;
    }
  if(event->button!=1) goto ew0; // only left clicks do anything now
  if(!isingrid(x,y)) goto ew0;
  ee=!!(e&(e-1)); // more than one bit set in e?

  if(clickblock&&ee) { // flip between block and space when a square is clicked near a corner
    if((gsq[x][y].fl&1)==0) {editblock(x,y,0);goto ew1;}
    else                    {editempty(x,y,0);goto ew1;}
    }
  else if(clickbar&&e&&!ee) { // flip bar if clicked in middle of edge
    b=logbase2(e);
    DEB_RF printf("  x=%d y=%d e=%d b=%d isbar(x,y,b)=%d\n",x,y,e,b,isbar(x,y,b));
    symmdo(a_editbar,!isbar(x,y,b),x,y,b);
    gridchange();
    goto ew1;
    }

  if(x==curx&&y==cury) nextdir(); // flip direction for click in square of cursor
  else {
    curx=x; cury=y; // not trapped as producing bar or block, so move cursor
    if(curdir>=100) curdir=0;
    }
  curmoved();
  gridchangen();

  ew1:
  gtk_window_set_focus(GTK_WINDOW(mainw),grid_da);
  ew0:
  DEB_RF printf("Exiting button_press_event()\n");
  return FALSE;
  }

// scroll
static gint mousescroll(GtkWidget*widget,GdkEventScroll*event) {
  int d,k;
  k=(int)(event->state); // buttons and modifiers
  d=event->direction;
  if(k&GDK_CONTROL_MASK) { // control held down
    if(d==GDK_SCROLL_UP   ) m_zoom(0,(void*)-1);
    if(d==GDK_SCROLL_DOWN ) m_zoom(0,(void*)-2);
    }
  return 0;
  }

// word list entry selected
static int selrow(GtkWidget*widget,gint row,gint column, GdkEventButton*event,gpointer data) {
  int i,l,l0,lx[MXCL],ly[MXCL],nc;
  ABM*abmp[MXLE];
  DEB_RF printf("row select event\n");
  if(llistem&EM_JUM) return 1; // don't allow click-to-enter for jumbles
  l=getlight(lx,ly,curx,cury,curdir);
  if(l<2) return 1;
  nc=getlightbmp(abmp,curx,cury,curdir,curmux);
  if(nc<2) return 1;
  if(!llistp) return 1;
  if(row<0||row>=llistn) return 1;
  if(llistp[row]>=ltotal) return 1; // light building has not caught up yet, so ignore click
  l0=strlen(lts[llistp[row]].s);
  if(lts[llistp[row]].tagged) l0-=NMSG;
  if(l0!=nc) return 1;
  for(i=0;i<nc;i++) *abmp[i]=ICCTOABM((int)(lts[llistp[row]].s[i]));
  for(i=0;i<l;i++) refreshsqmg(lx[i],ly[i]);
  gridchange();
  gtk_window_set_focus(GTK_WINDOW(mainw),grid_da);
  return 1;
  }

static char lookupuri[NLOOKUP][SLEN+1];

static int banword(GtkWidget*menuitem,gpointer data) {
  int u;
  u=(int)(intptr_t)data;
//  printf("Ban %d (atotal=%d)\n",u,atotal);
  if(u>=0&&u<atotal) ansp[u]->banned=1;
  gridchange();
  gtk_window_set_focus(GTK_WINDOW(mainw),grid_da);
  return 1;
  }

static int copyword(GtkWidget*menuitem,gpointer data) {
  int u;
  u=(int)(intptr_t)data;
  if(u>=0&&u<atotal) gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),ansp[u]->cf,-1);
  gtk_window_set_focus(GTK_WINDOW(mainw),grid_da);
  return 1;
  }

static int lookupword(GtkWidget*menuitem,gpointer data) {
  char*p=(char*)data;
  DEB_GU printf("Lookup %s\n",p);
  if(!STRSTARTS(p,"http://")&&!STRSTARTS(p,"https://")) {
    reperr("Only http:// and https:// URIs can be opened.\nPlease check your Preferences settings.");
    return 1;
    }
#ifdef _WIN32
  {
    wchar_t sw[SLEN];
    MultiByteToWideChar(CP_UTF8, 0, p, -1, sw, SLEN);
    ShellExecuteW(0,L"open",sw,0,0,SW_SHOWNORMAL);
  }
#else
  gtk_show_uri(NULL,p,GDK_CURRENT_TIME,NULL);
#endif
  gtk_window_set_focus(GTK_WINDOW(mainw),grid_da);
  gdk_window_set_cursor(mainw->window,NULL);
  return 1;
  }

static int clist_button(GtkWidget*widget,GdkEventButton*event,gpointer data) {
  int row,col,i,j,k;
  char s[SLEN*3],*p,*t;
  GtkWidget*menu,*mi,*mi0[NLOOKUP];
//  printf("clist_button(%d) x=%f y=%f\n",(int)(intptr_t)data,event->x,event->y);
  if(event->button!=3) return 0;
  if(gtk_clist_get_selection_info(GTK_CLIST(clist),(int)floor(event->x+.5),(int)floor(event->y),&row,&col)==0) return 1;
//  printf(" row=%d col=%d\n",row,col);
  if(!llistp) return 1;
  if(row<0||row>=llistn) return 1;
  if(llistp[row]>=ltotal) return 1; // light building has not caught up yet, so ignore click
//  printf("llistp[row]=%d\n",llistp[row]);
//  printf("lts[llistp[row]].ans=%d ansp[a].cf=<%s>\n",lts[llistp[row]].ans,ansp[lts[llistp[row]].ans]->cf);
  menu=gtk_menu_new();
  sprintf(s,"Ban \"%s\"",ansp[lts[llistp[row]].ans]->cf);
  mi=gtk_menu_item_new_with_label(s);
  g_signal_connect(mi,"activate",GTK_SIGNAL_FUNC(banword),(gpointer)(intptr_t)lts[llistp[row]].ans);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi);
  mi=gtk_menu_item_new_with_label("Copy");
  g_signal_connect(mi,"activate",GTK_SIGNAL_FUNC(copyword),(gpointer)(intptr_t)lts[llistp[row]].ans);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi);
  t=ansp[lts[llistp[row]].ans]->cf;
  if(t&&strlen(t)<SLEN) {
    for(i=0;i<NLOOKUP;i++) {
      mi0[i]=gtk_menu_item_new_with_label(lookupname(i));
      for(j=0,k=0;lookup(i)[j];) {
        if(lookup(i)[j]=='%') {
          j++;
          if(lookup(i)[j]=='s') {
            j++;
            if(k+strlen(t)*3>SLEN*3-10) goto skip; // each byte could become %XX
            for(p=t;*p;p++)
              if(isalnum((unsigned char)*p)) lookupuri[i][k++]=*p;
              else                           sprintf(lookupuri[i]+k,"%%%02X",(unsigned char)*p),k+=3;
            continue;
            }
          }
        lookupuri[i][k++]=lookup(i)[j++];
        if(k>=SLEN*3-1) break;
        }
      lookupuri[i][k]=0;
      DEB_GU printf("lookupuri[%d]=%s\n",i,lookupuri[i]);
      g_signal_connect(mi0[i],"activate",GTK_SIGNAL_FUNC(lookupword),lookupuri[i]);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu),mi0[i]);
skip: ;
      }
    }
  gtk_widget_show_all(menu);
  gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,event->button,gdk_event_get_time((GdkEvent*)event));
  return 1;
  }

// grid update
static gint configure_event(GtkWidget*widget,GdkEventConfigure*event) {
  DEB_RF printf("config event: new w=%d h=%d\n",widget->allocation.width,widget->allocation.height);
  return 1;
  }

// redraw the grid area
static gint expose_event(GtkWidget*widget,GdkEventExpose*event) {double u,v;
  cairo_t*cr;
  DEB_RF printf("expose event x=%d y=%d w=%d h=%d\n",event->area.x,event->area.y,event->area.width,event->area.height),fflush(stdout);
  if(widget==grid_da) {
    cr=gdk_cairo_create(widget->window);
    cairo_rectangle(cr,event->area.x,event->area.y,event->area.width,event->area.height);
    cairo_clip(cr);
    repaint(cr);
    cairo_destroy(cr);
    if(curmf) {
      mgcentre(&u,&v,curx,cury,0,1);
      DEB_RF printf("curmoved: %f %f %d %d\n",u,v,pxsq,bawdpx);
      gtk_adjustment_clamp_page(gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(grid_sw)),(u-.5)*pxsq+bawdpx-3,(u+.5)*pxsq+bawdpx+3); // scroll window to follow cursor
      gtk_adjustment_clamp_page(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(grid_sw)),(v-.5)*pxsq+bawdpx-3,(v+.5)*pxsq+bawdpx+3);
      curmf=0;
      }
    }
  return FALSE;
  }

void invaldarect(int x0,int y0,int x1,int y1) {GdkRectangle r;
  if(!usegui) return;
  r.x=x0; r.y=y0; r.width=x1-x0; r.height=y1-y0;
  DEB_RF printf("invalidate(%d,%d - %d,%d)\n",x0,y0,x1,y1);
  gdk_window_invalidate_rect(grid_da->window,&r,0);
  }

void invaldaall() {
  DEB_RF printf("invalidate all\n");
  if(!usegui) return;
  gdk_window_invalidate_rect(grid_da->window,0,0);
  }





// GTK DIALOGUES

// general filename dialogue: button text in but, default file extension in ext
// result copied to res if non-NULL, "filename" otherwise
static int filedia(char*res,char*but,char*ext,char*filetype,int write) {
  GtkWidget*dia;
  int i;
  char*p,t[SLEN+50];
  GtkFileFilter*filt0,*filt1;
  
  if(!res) res=filename;
  filt0=gtk_file_filter_new();
  if(!filt0) return 0;
  gtk_file_filter_add_pattern(filt0,"*");
  gtk_file_filter_set_name(filt0,"All files");
  filt1=gtk_file_filter_new();
  if(!filt1) return 0;
  strcpy(t,"*");
  strcat(t,strrchr(ext,'.'));
  // printf("<%s>\n",t);
  gtk_file_filter_add_pattern(filt1,t);
  for(i=1;t[i];i++) t[i]=toupper(t[i]);
  // printf("<%s>\n",t);
  gtk_file_filter_add_pattern(filt1,t);
  strcpy(t,filetype);
  strcat(t," files");
  gtk_file_filter_set_name(filt1,t);

  dia=gtk_file_chooser_dialog_new(but,0,
    write?GTK_FILE_CHOOSER_ACTION_SAVE:GTK_FILE_CHOOSER_ACTION_OPEN,
    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
    GTK_STOCK_OK, GTK_RESPONSE_OK,
    NULL);
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dia),filt1);
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dia),filt0);
  if(strcmp(filenamebase,"")) strcpy(res,filenamebase);
  else                        strcpy(res,"untitled");
  strcat(res,ext);
  strcpy(t,res);
  if(write) gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dia),t);
ew0:
  i=gtk_dialog_run(GTK_DIALOG(dia));
  i=i==GTK_RESPONSE_OK;
  if(i) {
    p=gtk_file_chooser_get_filename((GtkFileChooser*)dia);
    if(strlen(p)>SLEN) {reperr("Filename too long");goto ew0;}
    strcpy(res,p);
    }
  else strcpy(res,"");
  gtk_widget_destroy(dia);
  return i; // return 1 if got name successfully (although may be empty string)
  }

// cell contents dialogue
static int ccontdia() {
  GtkWidget*dia,*vb,*l0,*m[MAXNDIR],*lm[MAXNDIR],*e[MAXNDIR];
  int i,u,x,y;
  char s[100],t[MXCT*(MAXICC*16+4)];

  x=curx; y=cury;
  getmergerep(&x,&y,x,y);
  if(!isclear(x,y)) {reperr("Please place the cursor on a cell\nthat can contain characters");return 1;}

  dia=gtk_dialog_new_with_buttons("Cell contents",
    GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_OK,GTK_RESPONSE_OK,NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(dia),GTK_RESPONSE_OK);
  vb=gtk_vbox_new(0,2); // box to hold everything
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),vb,TRUE,TRUE,0);

  u=getdech(x,y);
  if(u) {
    l0=gtk_label_new("Contribution from cell");
    gtk_misc_set_alignment(GTK_MISC(l0),0,0.5);
    gtk_box_pack_start(GTK_BOX(vb),l0,TRUE,TRUE,0);
    }
  for(i=0;i<ndir[gtype];i++) {
    m[i]=gtk_hbox_new(0,2);
    if(u) sprintf(s,"to %s lights: ",dname[gtype][i]);
    else  sprintf(s,"Contents of cell: ");
    lm[i]=gtk_label_new(s);
    gtk_label_set_width_chars(GTK_LABEL(lm[i]),20);
    gtk_misc_set_alignment(GTK_MISC(lm[i]),1,0.5);
    gtk_box_pack_start(GTK_BOX(m[i]),lm[i],FALSE,FALSE,0);
    e[i]=gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(e[i]),30);
    gtk_entry_set_max_length(GTK_ENTRY(e[i]),sizeof(t));
    abmstostr(t,gsq[x][y].ctbm[i],gsq[x][y].ctlen[i],0);
    gtk_entry_set_text(GTK_ENTRY(e[i]),t);
    gtk_box_pack_start(GTK_BOX(m[i]),e[i],FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vb),m[i],TRUE,TRUE,0);
    if(i==curdir) gtk_window_set_focus(GTK_WINDOW(dia),e[i]);
    gtk_entry_set_activates_default(GTK_ENTRY(e[i]),1);
    if(!u) break;
    }
  l0=gtk_label_new("(use '.' for any characters yet to be filled or enter a pattern)");
  gtk_box_pack_start(GTK_BOX(vb),l0,TRUE,TRUE,0);

  gtk_widget_show_all(dia);
  i=gtk_dialog_run(GTK_DIALOG(dia));
  if(i==GTK_RESPONSE_OK) {
    for(i=0;i<ndir[gtype];i++) {
      gsq[x][y].ctlen[i]=strtoabms(gsq[x][y].ctbm[i],MXCT,(char*)gtk_entry_get_text(GTK_ENTRY(e[i])),0);
      if(!u) break;
      }
    gridchange();
    }
  gtk_widget_destroy(dia);
  refreshsqmg(x,y);
  return 1;
  }

// edit light dialogue
static int editlightdia() {
  ABM*abmp[MXLE];
  GtkWidget*dia,*vb,*l0,*m,*lm,*e;
  int i,u;
  char*p,t[MXLE*(MAXICC*16+4)];
  ABM b[MXLE];

  nc=getlightbmp(abmp,curx,cury,curdir,curmux);
  if(nc<2) { reperr("Please point the cursor along a light"); return 1; }
  if(nc>MXLE) { reperr("Light is too long"); return 1; }
  t[0]=0;
  for(i=0,p=t;i<nc;i++) {
    abmtostr(p,*(abmp[i]),0);
    p+=strlen(p);
    }
  dia=gtk_dialog_new_with_buttons("Light contents",
    GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_OK,GTK_RESPONSE_OK,NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(dia),GTK_RESPONSE_OK);
  vb=gtk_vbox_new(0,2); // box to hold everything
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),vb,TRUE,TRUE,0);

  m=gtk_hbox_new(0,2);
  lm=gtk_label_new("Contents of light: ");
  gtk_box_pack_start(GTK_BOX(m),lm,FALSE,FALSE,5);
  e=gtk_entry_new();
  gtk_entry_set_width_chars(GTK_ENTRY(e),60);
  gtk_entry_set_max_length(GTK_ENTRY(e),sizeof(t));
  gtk_entry_set_text(GTK_ENTRY(e),t);
  gtk_box_pack_start(GTK_BOX(m),e,FALSE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(vb),m,TRUE,TRUE,0);
  gtk_window_set_focus(GTK_WINDOW(dia),e);
  gtk_entry_set_activates_default(GTK_ENTRY(e),1);

  l0=gtk_label_new("(use '.' or enter a pattern for any characters yet to be filled)");
  gtk_box_pack_start(GTK_BOX(vb),l0,TRUE,TRUE,0);

  gtk_widget_show_all(dia);
  i=gtk_dialog_run(GTK_DIALOG(dia));
  if(i==GTK_RESPONSE_OK) {
    u=strtoabms(b,nc,(char*)gtk_entry_get_text(GTK_ENTRY(e)),0);
    for(i=u;i<nc;i++) b[i]=ABM_NRM; // pad with uncommitted letters
    for(i=0;i<nc;i++) {
      *(abmp[i])=b[i];
      }
    gridchange();
    }
  gtk_widget_destroy(dia);
  refreshall(); // could just refreshmg() cells along light
  return 1;
  }

// static GtkWidget*wctb; // textbuf
// static GtkTextTag*wctag[6];
// static char*fmtname[6]={"Bold","Italic","Subscript","Superscript","Strikethrough","Underline"};
// static char*fmtprops[6]={"weight","style","rise","rise","strikethrough","underline"};
// static int fmtvals[6]={PANGO_WEIGHT_BOLD,PANGO_STYLE_ITALIC,PANGO_SCALE*-4,PANGO_SCALE*4,1,1};
// static char*fmt_b[6]={ "<b>", "<i>", "<sub>", "<sup>", "<s>", "<u>"};
// static char*fmt_e[6]={"</b>","</i>","</sub>","</sup>","</s>","</u>"};
// 
// 
// static int applyformat(GtkWidget*w,void*data) {
//   int f,u;
//   GtkTextIter fi,ti;
//   f=(intptr_t)data;
//   if(!gtk_text_buffer_get_selection_bounds(GTK_TEXT_BUFFER(wctb),&fi,&ti)) return 1;
//   u=gtk_text_iter_has_tag(&fi,wctag[f]);
//   if(f==2||f==3) {
//     gtk_text_buffer_remove_tag(GTK_TEXT_BUFFER(wctb),wctag[2],&fi,&ti);
//     gtk_text_buffer_remove_tag(GTK_TEXT_BUFFER(wctb),wctag[3],&fi,&ti);
//     }
//   if(!u) gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER(wctb),wctag[f],&fi,&ti);
//   else   gtk_text_buffer_remove_tag(GTK_TEXT_BUFFER(wctb),wctag[f],&fi,&ti);
//   return 1;
//   }
// 
// static GtkWidget*wcdia;
// 
// static int fmt_key(GtkWidget*widget,GdkEventKey*event) {
//   if(event->keyval==GDK_Return||event->keyval==GDK_KP_Enter) {
//     g_signal_emit_by_name(G_OBJECT(wcdia),"response",GTK_RESPONSE_OK);
//     return 1;
//     }
//   if((event->state&GDK_CONTROL_MASK)&&event->keyval==GDK_b) {applyformat(0,(void*)0); return 1;}
//   if((event->state&GDK_CONTROL_MASK)&&event->keyval==GDK_i) {applyformat(0,(void*)1); return 1;}
//   if((event->state&GDK_CONTROL_MASK)&&event->keyval==GDK_s) {applyformat(0,(void*)4); return 1;}
//   if((event->state&GDK_CONTROL_MASK)&&event->keyval==GDK_u) {applyformat(0,(void*)5); return 1;}
//   return 0;
//   }
// 
// // write clue dialogue
// static int writecluedia() {
//   GtkWidget*l0,*vb,*hb;
//   GtkWidget*fmt[6];
//   GtkWidget*tv;
//   GtkTextIter ti;
//   char s[MXLE*16+1];
//   int i,l;
//   uchar u;
// 
//   l=getwordutf8(curx,cury,curdir,s,curmux);
//   if(l<2) {reperr("Please point the cursor along a light");return 1;}
// 
//   wcdia=gtk_dialog_new_with_buttons("Write clue",
//     GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,
//     GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_OK,GTK_RESPONSE_OK,NULL);
//   gtk_dialog_set_default_response(GTK_DIALOG(wcdia),GTK_RESPONSE_OK);
//   vb=gtk_vbox_new(0,2); // box to hold everything
//   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wcdia)->vbox),vb,TRUE,TRUE,0);
//   hb=gtk_hbox_new(FALSE,0);
//   l0=gtk_label_new("Light:"); gtk_box_pack_start(GTK_BOX(hb),l0,FALSE,FALSE,2);
//   l0=gtk_label_new(s);        gtk_box_pack_start(GTK_BOX(hb),l0,TRUE,TRUE,2);
//   gtk_misc_set_alignment(GTK_MISC(l0),0,0);
//   gtk_box_pack_start(GTK_BOX(vb),hb,TRUE,TRUE,0);
// 
//   tv=gtk_text_view_new();
//   wctb=(GtkWidget*)gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
//   gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(tv),3);
//   gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(tv),3);
//   gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv),3);
//   gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tv),3);
//   gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv),GTK_WRAP_WORD_CHAR);
//   gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(tv),0);
//   gtk_signal_connect(GTK_OBJECT(tv),"key_press_event",GTK_SIGNAL_FUNC(fmt_key),NULL);
// 
//   hb=gtk_hbox_new(FALSE,0);
//   for(i=0;i<6;i++) {
//     fmt[i]=gtk_button_new_with_label(fmtname[i]);   gtk_box_pack_start(GTK_BOX(hb),fmt[i],TRUE,TRUE,0);
//     gtk_signal_connect(GTK_OBJECT(fmt[i]),"clicked",GTK_SIGNAL_FUNC(applyformat),(gpointer)(intptr_t)i);
//     wctag[i]=gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(wctb),fmtname[i],fmtprops[i],fmtvals[i],0);
//     }
//     gtk_box_pack_start(GTK_BOX(vb),hb,FALSE,FALSE,0);
// 
//   gtk_box_pack_start(GTK_BOX(vb),tv,TRUE,TRUE,0);
// 
//   gtk_widget_show_all(wcdia);
//   gtk_window_set_focus(GTK_WINDOW(wcdia),tv);
//   i=gtk_dialog_run(GTK_DIALOG(wcdia));
//   if(i==GTK_RESPONSE_OK) {
//     gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(wctb),&ti);
//     for(;;) {
//       u=gtk_text_iter_get_char(&ti);
//   //    for(i=0;i<6;i++) printf("%d %d; ",gtk_text_iter_begins_tag(&ti,wctag[i]),gtk_text_iter_ends_tag(&ti,wctag[i])); printf("%08x\n",u);
//       for(i=0;i<6;i++) if(gtk_text_iter_ends_tag(&ti,wctag[i])) printf("%s",fmt_e[i]);
//       for(i=0;i<6;i++) if(gtk_text_iter_begins_tag(&ti,wctag[i])) printf("%s",fmt_b[i]);
//       if(!u) break;
//       gtk_text_iter_forward_char(&ti);
//       }
//     printf("\n");
//     // gridchange(); //?
//     }
//   gtk_widget_destroy(wcdia);
//   return 1;
//   }

// square properties dialogue

static GtkWidget*sprop_cbg,*sprop_cfg,*sprop_cmk,*sprop_l0,*sprop_l1,*sprop_l2,*sprop_l3,*sprop_l4,*sprop_tr,*sprop_w20,*sprop_w21,*sprop_or;
static GtkWidget*sprop_mkl[MAXNDIR*2],*sprop_mke[MAXNDIR*2];
static int sprop_g;

int sprop_setactive() {int i,k;
  k=sprop_g||gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sprop_or));
  gtk_widget_set_sensitive(sprop_cbg,k);
  gtk_widget_set_sensitive(sprop_cfg,k);
  gtk_widget_set_sensitive(sprop_cmk,k);
  gtk_widget_set_sensitive(sprop_l0,k);
  gtk_widget_set_sensitive(sprop_l1,k);
  gtk_widget_set_sensitive(sprop_l2,k);
  gtk_widget_set_sensitive(sprop_l3,k);
  gtk_widget_set_sensitive(sprop_l4,k);
  gtk_widget_set_sensitive(sprop_w20,k);
  gtk_widget_set_sensitive(sprop_tr,k);
  gtk_widget_set_sensitive(sprop_w21,k);
  for(i=0;i<ndir[gtype]*2;i++) {
    gtk_widget_set_sensitive(sprop_mkl[i],k);
    gtk_widget_set_sensitive(sprop_mke[i],k);
    }
  return 1;
  }

static int spropdia(int g) {
  GtkWidget*dia,*w,*vb;
  GdkColor gcbg,gcfg,gcmk;
  struct sprop*sps[MXSZ*MXSZ];
  int nsps;
  int d,i,j;

  sprop_g=g;
  nsps=0;
  if(g) {sps[0]=&dsp;nsps=1;}
  else {
    for(j=0;j<height;j++) for(i=0;i<width;i++) if(gsq[i][j].fl&16) sps[nsps++]=&gsq[i][j].sp;
    if(selmode!=0||nsps==0) {reperr("Please select one or more cells\nwhose properties you wish to change");return 1;}
    }
  i=sps[0]->bgcol;
  gcbg.red  =((i>>16)&255)*257;
  gcbg.green=((i>> 8)&255)*257;
  gcbg.blue =((i    )&255)*257;
  i=sps[0]->fgcol;
  gcfg.red  =((i>>16)&255)*257;
  gcfg.green=((i>> 8)&255)*257;
  gcfg.blue =((i    )&255)*257;
  i=sps[0]->mkcol;
  gcmk.red  =((i>>16)&255)*257;
  gcmk.green=((i>> 8)&255)*257;
  gcmk.blue =((i    )&255)*257;

  dia=gtk_dialog_new_with_buttons(g?"Default cell properties":"Selected cell properties",
    GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_APPLY,GTK_RESPONSE_OK,NULL);
  vb=gtk_vbox_new(0,2); // box to hold everything
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),vb,TRUE,TRUE,0);

  if(!g) {
    sprop_or=gtk_check_button_new_with_mnemonic("_Override default cell properties");
    gtk_box_pack_start(GTK_BOX(vb),sprop_or,TRUE,TRUE,0);
    gtk_signal_connect(GTK_OBJECT(sprop_or),"clicked",GTK_SIGNAL_FUNC(sprop_setactive),0);
    w=gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
    }

  w=gtk_hbox_new(0,2);
  sprop_l0=gtk_label_new("Background colour: ");
  gtk_label_set_width_chars(GTK_LABEL(sprop_l0),18);
  gtk_misc_set_alignment(GTK_MISC(sprop_l0),1,0.5);
  gtk_box_pack_start(GTK_BOX(w),sprop_l0,FALSE,FALSE,0);
  sprop_cbg=gtk_color_button_new_with_color(&gcbg);
  gtk_box_pack_start(GTK_BOX(w),sprop_cbg,FALSE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);

  w=gtk_hbox_new(0,2);
  sprop_l1=gtk_label_new("Foreground colour: ");
  gtk_label_set_width_chars(GTK_LABEL(sprop_l1),18);
  gtk_misc_set_alignment(GTK_MISC(sprop_l1),1,0.5);
  gtk_box_pack_start(GTK_BOX(w),sprop_l1,FALSE,FALSE,0);
  sprop_cfg=gtk_color_button_new_with_color(&gcfg);
  gtk_box_pack_start(GTK_BOX(w),sprop_cfg,FALSE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);

  w=gtk_hbox_new(0,2);                                                              gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  sprop_l2=gtk_label_new("Font style: ");                                                 gtk_box_pack_start(GTK_BOX(w),sprop_l2,FALSE,FALSE,0);
  gtk_label_set_width_chars(GTK_LABEL(sprop_l2),18);
  gtk_misc_set_alignment(GTK_MISC(sprop_l2),1,0.5);
  sprop_w20=gtk_combo_box_new_text();                                                     gtk_box_pack_start(GTK_BOX(w),sprop_w20,FALSE,FALSE,0);
  gtk_combo_box_append_text(GTK_COMBO_BOX(sprop_w20),"Normal");
  gtk_combo_box_append_text(GTK_COMBO_BOX(sprop_w20),"Bold");
  gtk_combo_box_append_text(GTK_COMBO_BOX(sprop_w20),"Italic");
  gtk_combo_box_append_text(GTK_COMBO_BOX(sprop_w20),"Bold italic");
  i=sps[0]->fstyle;
  if(i<0) i=0;
  if(i>3) i=3;
  gtk_combo_box_set_active(GTK_COMBO_BOX(sprop_w20),i);

  w=gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);

  w=gtk_hbox_new(0,2);
  sprop_l4=gtk_label_new("Mark colour: ");
  gtk_label_set_width_chars(GTK_LABEL(sprop_l4),18);
  gtk_misc_set_alignment(GTK_MISC(sprop_l4),1,0.5);
  gtk_box_pack_start(GTK_BOX(w),sprop_l4,FALSE,FALSE,0);
  sprop_cmk=gtk_color_button_new_with_color(&gcmk);
  gtk_box_pack_start(GTK_BOX(w),sprop_cmk,FALSE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);

  for(i=0;i<2;i++) {
    w=gtk_hbox_new(0,2);
    for(j=0;j<ndir[gtype];j++) {
      if(i==0) d=j;
      else     d=ndir[gtype]*2-j-1; // "clockwise"
      sprop_mkl[d]=gtk_label_new(mklabel[gtype][d]);
      gtk_label_set_width_chars(GTK_LABEL(sprop_mkl[d]),12);
      gtk_misc_set_alignment(GTK_MISC(sprop_mkl[d]),1,0.5);
      gtk_box_pack_start(GTK_BOX(w),sprop_mkl[d],FALSE,FALSE,0);
      sprop_mke[d]=gtk_entry_new();
      gtk_entry_set_max_length(GTK_ENTRY(sprop_mke[d]),5);
      gtk_entry_set_width_chars(GTK_ENTRY(sprop_mke[d]),5);
      gtk_entry_set_text(GTK_ENTRY(sprop_mke[d]),sps[0]->mk[d]);
      gtk_box_pack_start(GTK_BOX(w),sprop_mke[d],FALSE,FALSE,0);
      }
    gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
    }

  w=gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);

  sprop_tr=gtk_check_button_new_with_mnemonic("_Flag for answer treatment");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sprop_tr),sps[0]->ten);
  gtk_box_pack_start(GTK_BOX(vb),sprop_tr,TRUE,TRUE,0);

  w=gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);

  w=gtk_hbox_new(0,2);                                                                    gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  sprop_l3=gtk_label_new("Lights intersecting here ");                                    gtk_box_pack_start(GTK_BOX(w),sprop_l3,FALSE,FALSE,0);
  gtk_misc_set_alignment(GTK_MISC(sprop_l3),1,0.5);
  sprop_w21=gtk_combo_box_new_text();                                                     gtk_box_pack_start(GTK_BOX(w),sprop_w21,FALSE,FALSE,0);
  gtk_combo_box_append_text(GTK_COMBO_BOX(sprop_w21),"must agree");
  gtk_combo_box_append_text(GTK_COMBO_BOX(sprop_w21),"need not agree: horizontal display");
  gtk_combo_box_append_text(GTK_COMBO_BOX(sprop_w21),"need not agree: vertical display");
  i=sps[0]->dech;
  if(i<0) i=0;
  if(i>2) i=2;
  gtk_combo_box_set_active(GTK_COMBO_BOX(sprop_w21),i);

  if(!g) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sprop_or),(sps[0]->spor)&1);
  sprop_setactive();

  gtk_widget_show_all(dia);
  i=gtk_dialog_run(GTK_DIALOG(dia));
  if(i==GTK_RESPONSE_OK) {
    gtk_color_button_get_color(GTK_COLOR_BUTTON(sprop_cbg),&gcbg);
    gtk_color_button_get_color(GTK_COLOR_BUTTON(sprop_cfg),&gcfg);
    gtk_color_button_get_color(GTK_COLOR_BUTTON(sprop_cmk),&gcmk);
    for(j=0;j<nsps;j++) {
      if(!g) sps[j]->spor=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sprop_or));
      sps[j]->bgcol=
        (((gcbg.red  >>8)&255)<<16)+
        (((gcbg.green>>8)&255)<< 8)+
        (((gcbg.blue >>8)&255)    );
      sps[j]->fgcol=
        (((gcfg.red  >>8)&255)<<16)+
        (((gcfg.green>>8)&255)<< 8)+
        (((gcfg.blue >>8)&255)    );
      sps[j]->mkcol=
        (((gcmk.red  >>8)&255)<<16)+
        (((gcmk.green>>8)&255)<< 8)+
        (((gcmk.blue >>8)&255)    );
      i=gtk_combo_box_get_active(GTK_COMBO_BOX(sprop_w20)); if(i>=0&&i<4) sps[j]->fstyle=i;
      sps[j]->ten=!!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sprop_tr));
      i=gtk_combo_box_get_active(GTK_COMBO_BOX(sprop_w21)); if(i>=0&&i<3) sps[j]->dech=i;
      for(d=0;d<ndir[gtype]*2;d++) {
        strncpy(sps[j]->mk[d],gtk_entry_get_text(GTK_ENTRY(sprop_mke[d])),MXMK);
        sps[j]->mk[d][MXMK]=0;
        }
      }
    gridchange();
    }
  gtk_widget_destroy(dia);
  refreshall();
  return 1;
  }

// get version of dictionary d filename up to l characters
static void getdfname(char*s,int d,int l) {
  if(strlen(dfnames[d])==0) {
    if(strlen(dafilters[d])==0) strcat(s,"<empty>");
    else {
      strcat(s,"\"");
      if((int)strlen(dafilters[d])>l) {strncat(s,dafilters[d],l-3); strcat(s,"...");}
      else strcat(s,dafilters[d]);
      strcat(s,"\"");
      }
    }
  else {
    if((int)strlen(dfnames[d])>l) {strcat(s,"...");strcat(s,dfnames[d]+strlen(dfnames[d])-l+3);}
    else strcat(s,dfnames[d]);
    }
  }

// light properties dialogue

static GtkWidget*lprop_e[MAXNDICTS],*lprop_f[NLEM],*lprop_or,*lprop_tr,*lprop_nn,*lprop_mux;
static int lprop_g;

static int lprop_setactive() {int i,k;
  k=lprop_g||gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lprop_or));
  for(i=0;i<MAXNDICTS;i++) gtk_widget_set_sensitive(lprop_e[i],k);
  for(i=0;i<NLEM;i++)   gtk_widget_set_sensitive(lprop_f[i],k);
  gtk_widget_set_sensitive(lprop_tr,k);
  gtk_widget_set_sensitive(lprop_nn,k);
  gtk_widget_set_sensitive(lprop_mux,k);
  return 1;
  }

static int lpropdia(int g) {
  GtkWidget*dia,*w,*vb;
  struct lprop*lps[MXLT];
  int nlps;
  int d,i,j;
  char s[SLEN];

  lprop_g=g;
  nlps=0;
  if(g) {lps[0]=&dlp;nlps=1;}
  else {
    if(selmode==1)
      for(d=0;d<ndir[gtype];d++) for(j=0;j<height;j++) for(i=0;i<width;i++) if(isstartoflight(i,j,d)&&issellight(i,j,d)) lps[nlps++]=&gsq[i][j].lp[d];
    if(selmode==2)
      for(i=0;i<nvl;i++) if(vls[i].sel) lps[nlps++]=&vls[i].lp;
    if(selmode==0||nlps==0) {reperr("Please select one or more lights\nwhose properties you wish to change");return 1;}
    }
  dia=gtk_dialog_new_with_buttons(g?"Default light properties":"Selected light properties",
    GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_APPLY,GTK_RESPONSE_OK,NULL);
  vb=gtk_vbox_new(0,2); // box to hold everything
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),vb,TRUE,TRUE,0);

  if(!g) {
    lprop_or=gtk_check_button_new_with_mnemonic("_Override default light properties");
    gtk_box_pack_start(GTK_BOX(vb),lprop_or,TRUE,TRUE,0);
    gtk_signal_connect(GTK_OBJECT(lprop_or),"clicked",GTK_SIGNAL_FUNC(lprop_setactive),0);
    w=gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
    }
  for(i=0;i<MAXNDICTS;i++) {
    sprintf(s,"Use dictionary _%d (",i+1);
    getdfname(s+strlen(s),i,25);

    strcat(s,")");
    lprop_e[i]=gtk_check_button_new_with_mnemonic(s);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lprop_e[i]),((lps[0]->dmask)>>i)&1);
    gtk_box_pack_start(GTK_BOX(vb),lprop_e[i],TRUE,TRUE,0);
    }
  w=gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  lprop_tr=gtk_check_button_new_with_mnemonic("_Enable answer treatment");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lprop_tr),lps[0]->ten);
  gtk_box_pack_start(GTK_BOX(vb),lprop_tr,TRUE,TRUE,0);

  w=gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  for(i=0;i<NLEM;i++) {
    sprintf(s,"Allow light to be entered _%s",lemdescADVP[i]);
    lprop_f[i]=gtk_check_button_new_with_mnemonic(s);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lprop_f[i]),((lps[0]->emask)>>i)&1);
    gtk_box_pack_start(GTK_BOX(vb),lprop_f[i],TRUE,TRUE,0);
    }
  w=gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  lprop_nn=gtk_check_button_new_with_mnemonic("Light _does not receive a number");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lprop_nn),lps[0]->dnran);
  gtk_box_pack_start(GTK_BOX(vb),lprop_nn,TRUE,TRUE,0);

  w=gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  lprop_mux=gtk_check_button_new_with_mnemonic("_Multiplex");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lprop_mux),lps[0]->mux);
  gtk_box_pack_start(GTK_BOX(vb),lprop_mux,TRUE,TRUE,0);

  if(!g) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lprop_or),(lps[0]->lpor)&1);
  lprop_setactive();
  gtk_widget_show_all(dia);
  i=gtk_dialog_run(GTK_DIALOG(dia));
  if(i==GTK_RESPONSE_OK) {
    for(j=0;j<nlps;j++) {
      if(!g) lps[j]->lpor=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lprop_or));
      lps[j]->dmask=0;
      for(i=0;i<MAXNDICTS;i++) if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lprop_e[i]))) lps[j]->dmask|=1<<i;
      lps[j]->emask=0;
      for(i=0;i<NLEM;i++) if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lprop_f[i]))) lps[j]->emask|=1<<i;
      lps[j]->ten=!!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lprop_tr));
      lps[j]->dnran=!!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lprop_nn));
      lps[j]->mux=!!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lprop_mux));
      if(lps[j]->emask==0) {
        reperr("No entry methods selected:\nthis would make fill impossible.\nAllowing lights to be entered normally.");
        lps[j]->emask=EM_FWD;
        }
      }
    gridchange();
    }
  gtk_widget_destroy(dia);
  return 1;
  }

// treatment dialogue

static GtkWidget*treat_a[NMSG],*treat_f,*treat_c,*treat_m[NMSG],*treat_lm[NMSG],*treat_bpiname,*treat_b1[NMSG],*treat_b2[NMSG],*treat_btambaw;

static int treatcomboorder[NATREAT]={0,1,2,3,4,10,11,5,6,7,8,9}; // put misprints in the right place

static int treat_browse(GtkWidget*w,gpointer p) {
  GtkWidget*dia;
  DEB_GU {printf("w=%p p=%p\n",(void*)w,(void*)p);fflush(stdout);}
  dia=gtk_file_chooser_dialog_new("Choose a treatment plug-in",0,
    GTK_FILE_CHOOSER_ACTION_OPEN,
    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
    GTK_STOCK_OPEN, GTK_RESPONSE_OK,
    NULL);
  if(gtk_dialog_run(GTK_DIALOG(dia))==GTK_RESPONSE_OK)
    gtk_entry_set_text(GTK_ENTRY(treat_f),gtk_file_chooser_get_filename((GtkFileChooser*)dia));
  gtk_widget_destroy(dia);
  return 1;
  }

// get (or construct) label for message m of treatment n
static char*gettreatlab(int n,int m) {
  static char s[1000];
  int i,j,j0,j1;
  if(n==2&&m==0) {
    if(iccgroupstart[2]-iccgroupstart[0]==0) return "Alphabet substitutions: ";
    strcpy(s,"Encode ");
    for(i=0;i<2;i++) {
      j0=iccgroupstart[i];
      j1=iccgroupstart[i+1];
      if(j0==j1) continue;
      if(j1-j0<7) for(j=j0;j<j1;j++) strcat(s,icctoutf8[(int)iccused[j]]);
      else {
        for(j=j0;j<j0+3;j++) strcat(s,icctoutf8[(int)iccused[j]]);
        strcat(s,"...");
        strcat(s,icctoutf8[(int)iccused[j1-1]]);
        }
      if(i==0&&iccgroupstart[2]!=j1) strcat(s,", ");
      }
    strcat(s," as: ");
    return s;
    }
  if(n==4&&m==0) {
    if(iccgroupstart[2]-iccgroupstart[0]==0) return "Encoding offsets: "; 
    strcpy(s,"Encodings of ");
    for(i=0;i<2;i++) {
      j0=iccgroupstart[i];
      j1=iccgroupstart[i+1];
      if(j0==j1) continue;
      strcat(s,icctoutf8[(int)iccused[j0]]);
      if(i==0&&iccgroupstart[2]!=j1) strcat(s,", ");
      }
    strcat(s,": ");
    return s;
    }
  return treat_lab[n][m];
  }

static int treat_setactive() {int i,j,k,l;
  k=treatcomboorder[gtk_combo_box_get_active(GTK_COMBO_BOX(treat_c))]; // get selected treatment type
  for(i=0;i<NMSG;i++) {
    j=treat_lab[k][i]!=NULL;
    l=gtk_combo_box_get_active(GTK_COMBO_BOX(treat_a[i]));
    gtk_widget_set_sensitive(treat_m[i],j); // desensitise if message is not used by this treatment
    gtk_widget_set_sensitive(treat_b1[i],treatpermok[k][i]);
    gtk_widget_set_sensitive(treat_b2[i],l>0&&treatpermok[k][i]);
    gtk_label_set_text(GTK_LABEL(treat_lm[i]),gettreatlab(j?k:TREAT_PLUGIN,i)); // use text from TREAT_PLUGIN ("Message 1:" etc.) if desensitised
    }
  gtk_widget_set_sensitive(treat_bpiname,k==TREAT_PLUGIN);
  gtk_widget_set_sensitive(treat_btambaw,k!=0);
  return 1;
  }

static int treatdia(void) {
  GtkWidget*dia,*e[NMSG],*c[NMSG],*l,*b,*vb;
  int i,j;
  char s[(MAXICC*16+4)*MXFL+1];

  filler_stop();
  dia=gtk_dialog_new_with_buttons("Answer treatment",GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_APPLY,GTK_RESPONSE_OK,NULL);
  vb=gtk_vbox_new(0,2); // box to hold everything
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),vb,TRUE,TRUE,0);

  treat_c=gtk_combo_box_new_text();
  for(i=0;i<NATREAT;i++) gtk_combo_box_append_text(GTK_COMBO_BOX(treat_c),tnames[treatcomboorder[i]]);

  gtk_box_pack_start(GTK_BOX(vb),treat_c,FALSE,FALSE,0);

  for(i=0;i<NMSG;i++) {
    treat_m[i]=gtk_hbox_new(0,2);
    treat_lm[i]=gtk_label_new(" ");
    gtk_label_set_width_chars(GTK_LABEL(treat_lm[i]),25);
    gtk_misc_set_alignment(GTK_MISC(treat_lm[i]),1,0.5);
    gtk_box_pack_start(GTK_BOX(treat_m[i]),treat_lm[i],FALSE,FALSE,0);
    e[i]=gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(e[i]),30);
    gtk_entry_set_max_length(GTK_ENTRY(e[i]),sizeof(treatmsg[0])-1);
    gtk_entry_set_text(GTK_ENTRY(e[i]),treatmsg[i]);
    gtk_box_pack_start(GTK_BOX(treat_m[i]),e[i],FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vb),treat_m[i],TRUE,TRUE,0);

    treat_b1[i]=gtk_hbox_new(0,2);
    l=gtk_label_new("Allocate characters ");
    gtk_label_set_width_chars(GTK_LABEL(l),25);
    gtk_misc_set_alignment(GTK_MISC(l),1,0.5);
    gtk_box_pack_start(GTK_BOX(treat_b1[i]),l,FALSE,FALSE,0);
    treat_a[i]=gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(treat_a[i]),"in clue order, first come first served");
    gtk_combo_box_append_text(GTK_COMBO_BOX(treat_a[i]),"in clue order, at discretion of filler");
    gtk_combo_box_append_text(GTK_COMBO_BOX(treat_a[i]),"in any order, at discretion of filler");
    gtk_box_pack_start(GTK_BOX(treat_b1[i]),treat_a[i],FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vb),treat_b1[i],TRUE,TRUE,0);

    treat_b2[i]=gtk_hbox_new(0,2);
    l=gtk_label_new("subject to constraints ");
    gtk_label_set_width_chars(GTK_LABEL(l),25);
    gtk_misc_set_alignment(GTK_MISC(l),1,0.5);
    gtk_box_pack_start(GTK_BOX(treat_b2[i]),l,FALSE,FALSE,0);
    c[i]=gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(c[i]),30);
    gtk_entry_set_max_length(GTK_ENTRY(c[i]),sizeof(s));
    abmstostr(s,treatcstr[i],MXFL,1);
    j=strlen(s);
    while(j>0&&s[j-1]=='?') j--;
    s[j]=0; // strip trailing "?"s
    gtk_entry_set_text(GTK_ENTRY(c[i]),s);
    gtk_box_pack_start(GTK_BOX(treat_b2[i]),c[i],FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(vb),treat_b2[i],TRUE,TRUE,0);
    }

  treat_bpiname=gtk_hbox_new(0,2);
  l=gtk_label_new("Treatment plug-in: ");
  gtk_label_set_width_chars(GTK_LABEL(l),25);
  gtk_misc_set_alignment(GTK_MISC(l),1,0.5);
  gtk_box_pack_start(GTK_BOX(treat_bpiname),l,FALSE,FALSE,0);
  treat_f=gtk_entry_new();
  gtk_entry_set_max_length(GTK_ENTRY(treat_f),SLEN-1);
  gtk_entry_set_text(GTK_ENTRY(treat_f),tpifname);
  gtk_box_pack_start(GTK_BOX(treat_bpiname),treat_f,FALSE,FALSE,0);
  b=gtk_button_new_with_label(" Browse... ");
  gtk_box_pack_start(GTK_BOX(treat_bpiname),b,FALSE,FALSE,0);
  gtk_signal_connect(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(treat_browse),0);
  gtk_box_pack_start(GTK_BOX(vb),treat_bpiname,TRUE,TRUE,0);
  treat_btambaw=gtk_check_button_new_with_mnemonic("Treated answer _must be a word");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(treat_btambaw),tambaw);
  gtk_box_pack_start(GTK_BOX(vb),treat_btambaw,TRUE,TRUE,0);

  gtk_signal_connect(GTK_OBJECT(treat_c),"changed",GTK_SIGNAL_FUNC(treat_setactive),0);
  for(i=0;i<NATREAT;i++) if(treatcomboorder[i]==treatmode) gtk_combo_box_set_active(GTK_COMBO_BOX(treat_c),i);
  for(i=0;i<NMSG;i++) {
    gtk_signal_connect(GTK_OBJECT(treat_a[i]),"changed",GTK_SIGNAL_FUNC(treat_setactive),0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(treat_a[i]),treatorder[i]);
    }
  gtk_widget_show_all(dia);
  i=gtk_dialog_run(GTK_DIALOG(dia));
  if(i==GTK_RESPONSE_OK) {
    strncpy(tpifname,gtk_entry_get_text(GTK_ENTRY(treat_f)),SLEN-1);
    tpifname[SLEN-1]=0;
    treatmode=treatcomboorder[gtk_combo_box_get_active(GTK_COMBO_BOX(treat_c))];
    for(i=0;i<NMSG;i++) {
      strncpy(treatmsg[i],gtk_entry_get_text(GTK_ENTRY(e[i])),sizeof(treatmsg[0])-1);
      treatmsg[i][sizeof(treatmsg[0])-1]=0;
      treatorder[i]=gtk_combo_box_get_active(GTK_COMBO_BOX(treat_a[i]));
      if(treatorder[i]<0) treatorder[i]=0;
      if(treatorder[i]>2) treatorder[i]=2;
      if(!treatpermok[treatmode][i]) treatorder[i]=0;
      j=strtoabms(treatcstr[i],MXFL,(char*)gtk_entry_get_text(GTK_ENTRY(c[i])),1);
      for(;j<MXFL;j++) treatcstr[i][j]=ABM_ALL;
      }
    tambaw=!!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(treat_btambaw));
    if(treatmode<0||treatmode>=NATREAT) treatmode=0;
    reloadtpi();
    }
  gridchange();
  gtk_widget_destroy(dia);
  return 1;
  }



// dictionary list dialogue

static GtkWidget*dictl_e[MAXNDICTS];

static int dictl_browse(GtkWidget*w,gpointer p) {
  GtkWidget*dia;
  DEB_GU {printf("w=%p p=%p\n",(void*)w,(void*)p);fflush(stdout);}
  dia=gtk_file_chooser_dialog_new("Choose a dictionary",0,
    GTK_FILE_CHOOSER_ACTION_OPEN,
    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
    GTK_STOCK_OPEN, GTK_RESPONSE_OK,
    NULL);
  if(gtk_dialog_run(GTK_DIALOG(dia))==GTK_RESPONSE_OK)
    gtk_entry_set_text(GTK_ENTRY(dictl_e[(intptr_t)p]),gtk_file_chooser_get_filename((GtkFileChooser*)dia));
  gtk_widget_destroy(dia);
  return 1;
}

static int dictldia(void) {
  GtkWidget*dia,*l,*b,*t,*f0[MAXNDICTS],*f1[MAXNDICTS];
  int d,i,j;
  unsigned int m,m0;
  char s[SLEN];

  filler_stop();
  dia=gtk_dialog_new_with_buttons("Dictionaries",GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_APPLY,GTK_RESPONSE_OK,NULL);
  t=gtk_table_new(MAXNDICTS+1,5,0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),t,TRUE,TRUE,0);
  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>File</b>"         ); gtk_table_attach(GTK_TABLE(t),l,1,2,0,1,0,0,0,0);
  l=gtk_label_new("");                                                             gtk_table_attach(GTK_TABLE(t),l,3,4,0,1,0,0,10,0);
  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>File filter</b>"  ); gtk_table_attach(GTK_TABLE(t),l,4,5,0,1,0,0,0,0);
  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Answer filter</b>"); gtk_table_attach(GTK_TABLE(t),l,5,6,0,1,0,0,0,0);
  for(i=0;i<MAXNDICTS;i++) {
    sprintf(s,"<b>%d</b>",i+1);
    l=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(l),s);
    gtk_table_attach(GTK_TABLE(t),l,0,1,i+1,i+2,0,0,5,0);
    dictl_e[i]=gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(dictl_e[i]),SLEN-1);
    gtk_entry_set_text(GTK_ENTRY(dictl_e[i]),dfnames[i]);
    gtk_table_attach(GTK_TABLE(t),dictl_e[i],1,2,i+1,i+2,0,0,0,0);
    b=gtk_button_new_with_label(" Browse... ");
    gtk_table_attach(GTK_TABLE(t),b,2,3,i+1,i+2,0,0,0,0);
    f0[i]=gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(f0[i]),SLEN-1);
    gtk_entry_set_text(GTK_ENTRY(f0[i]),dsfilters[i]);
    gtk_table_attach(GTK_TABLE(t),f0[i],4,5,i+1,i+2,0,0,5,0);
    f1[i]=gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(f1[i]),SLEN-1);
    gtk_entry_set_text(GTK_ENTRY(f1[i]),dafilters[i]);
    gtk_table_attach(GTK_TABLE(t),f1[i],5,6,i+1,i+2,0,0,5,0);
    gtk_signal_connect(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(dictl_browse),(void*)(intptr_t)i);
    }
  gtk_widget_show_all(dia);

  j=gtk_dialog_run(GTK_DIALOG(dia));
  if(j==GTK_RESPONSE_CANCEL||j==GTK_RESPONSE_DELETE_EVENT) goto ew0;
  for(j=0;j<MAXNDICTS;j++) {    // Use names etc from dialog, but keep originals 
    strncpy(dfnames[j],gtk_entry_get_text(GTK_ENTRY(dictl_e[j])),SLEN-1);
    dfnames[j][SLEN-1]=0;
    strncpy(dsfilters[j],gtk_entry_get_text(GTK_ENTRY(f0[j])),SLEN-1);
    dsfilters[j][SLEN-1]=0;
    strncpy(dafilters[j],gtk_entry_get_text(GTK_ENTRY(f1[j])),SLEN-1);
    dafilters[j][SLEN-1]=0;
    }
  loaddicts(0);

  // now check if user has any dictionaries loaded and possibly forgotten to set light properties to use them
  m=0;
  for(d=0;d<ndir[gtype];d++) for(j=0;j<height;j++) for(i=0;i<width;i++) if(isstartoflight(i,j,d)) m|=getlprop(i,j,d)->dmask;
  for(d=0;d<nvl;d++) m|=getvlprop(d)->dmask;
  m=dusedmask&~m;
  if(m) {
    if(onebit((ABM)m)) sprintf(s,"No lights have properties set to use\nwords from dictionary %d",logbase2((ABM)m)+1);
    else {
      sprintf(s,"No lights have properties set to use\nwords from dictionaries ");
      while(m) {
        m0=m&~(m-1);
        sprintf(s+strlen(s),"%d",logbase2((ABM)m0)+1);
        m&=~m0;
        if(m) {
          if(onebit((ABM)m)) strcat(s," or ");
          else strcat(s,", ");
          }
        }
      }
    repwarn(s);
    }

ew0:
  gridchange();
  gtk_widget_destroy(dia);
  return 1;
  }

// dictionary statistics
static int dstatsdia(void) {
  int d,i,j,k,n;
  GtkWidget*dia,*nb,*w0,*vb0[MAXNDICTS],*vb,*l0;
  char s[SLEN];

  dia=gtk_dialog_new_with_buttons("Dictionary statistics",GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,GTK_STOCK_OK,GTK_RESPONSE_OK,NULL);
  vb=gtk_vbox_new(0,3);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),vb,TRUE,TRUE,0);

  nb=gtk_notebook_new();
  gtk_box_pack_start(GTK_BOX(vb),nb,FALSE,TRUE,0);
  for(d=0;d<MAXNDICTS;d++) {

    w0=gtk_scrolled_window_new(0,0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(w0),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(w0,-1,300);

    sprintf(s,"Dictionary %d",d+1);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb),w0,gtk_label_new(s));

    vb0[d]=gtk_vbox_new(0,3);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(w0),vb0[d]);

    sprintf(s,"  Name: "); getdfname(s,d,80);                l0=gtk_label_new(s); gtk_misc_set_alignment(GTK_MISC(l0),0,0.5);   gtk_box_pack_start(GTK_BOX(vb0[d]),l0,FALSE,TRUE,0);
    if(strlen(dfnames[d])==0&&strlen(dafilters[d])==0) continue;
    sprintf(s,"  Usable entries: %d",dst_lines[d]);          l0=gtk_label_new(s); gtk_misc_set_alignment(GTK_MISC(l0),0,0.5);   gtk_box_pack_start(GTK_BOX(vb0[d]),l0,FALSE,TRUE,0);
    sprintf(s,"  After file filter: %d",dst_lines_f[d]);     l0=gtk_label_new(s); gtk_misc_set_alignment(GTK_MISC(l0),0,0.5);   gtk_box_pack_start(GTK_BOX(vb0[d]),l0,FALSE,TRUE,0);
    sprintf(s,"  After answer filter: %d",dst_lines_fa[d]);  l0=gtk_label_new(s); gtk_misc_set_alignment(GTK_MISC(l0),0,0.5);   gtk_box_pack_start(GTK_BOX(vb0[d]),l0,FALSE,TRUE,0);
    for(i=0,n=0;i<MAXUCHAR;i++) {
      if(!ISUGRAPH(i)) continue;
      if(dst_u_rejline_count[d][i]) n++; // count how many Unicode characters were rejected
      }
    if(n==0) {
      l0=gtk_label_new("  No Unicode characters were rejected"); gtk_misc_set_alignment(GTK_MISC(l0),0,0.5);   gtk_box_pack_start(GTK_BOX(vb0[d]),l0,FALSE,TRUE,0);
      }
    else {
      sprintf(s,"  Rejected Unicode character%s:",n==1?"":"s"); l0=gtk_label_new(s); gtk_misc_set_alignment(GTK_MISC(l0),0,0.5);   gtk_box_pack_start(GTK_BOX(vb0[d]),l0,FALSE,TRUE,0);
      k=0;
      for(i=0;i<MAXUCHAR;i++) {
        if(!ISUGRAPH(i)) continue;
        j=dst_u_rejline_count[d][i];
        if(j==0) continue;
        sprintf(s,"    U+%04X (",i);
        uchartoutf8(s+strlen(s),i);
        strcat(s,"): ");
        if(dst_u_rejline[d][i]) {
          if(j==1) sprintf(s+strlen(s)," one occurrence, at line %d",dst_u_rejline[d][i]);
          else     sprintf(s+strlen(s)," %d occurrences, first at line %d",j,dst_u_rejline[d][i]);
          }
        else {
          if(j==1) sprintf(s+strlen(s)," one occurrence");
          else     sprintf(s+strlen(s)," %d occurrences",j);
          }
        l0=gtk_label_new(s); gtk_misc_set_alignment(GTK_MISC(l0),0,0.5);   gtk_box_pack_start(GTK_BOX(vb0[d]),l0,FALSE,TRUE,0);
        k++;
  //      if(k==n||(k==10&&n>11)) break;
        }
  //    if(k<n) {
  //      sprintf(s,"    ... and %d more",n-k); l0=gtk_label_new(s); gtk_misc_set_alignment(GTK_MISC(l0),0,0.5);   gtk_box_pack_start(GTK_BOX(vb0[d]),l0,FALSE,TRUE,0);
  //      }
      }
    }
  gtk_widget_show_all(dia);
  gtk_dialog_run(GTK_DIALOG(dia));
  gtk_widget_destroy(dia);
  return 1;
  }

static GtkWidget*alpha_r[MAXICC],*alpha_a[MAXICC],*alpha_vs[MAXICC],*alpha_cs[MAXICC],*alpha_sq[MAXICC],*alpha_iccl[MAXICC];
static GtkWidget*al_w20,*al_w21;
static int aldiamode=0; // 0=text edit, 1=Unicode edit
static int rowtoicc[MAXICC]; // -1: nothing useful in this row; 1...MAXICC-1=ICC

// convert string of uchars to ordinary string of "U+xxxx"s
static void ucharstostr(char*q,const uchar*p) {
  int j;
  *q=0;
  for(j=0;p[j];j++) {
    if(j>0) strcat(q," ");
    sprintf(q+strlen(q),"U+%04X",p[j]);
    }
  }

// convert ordinary string of "U+xxxx"s to string of uchars
static void ustrtouchars(uchar*q,const char*p,int l) {
  int i;
  unsigned int u;
  *q=0; i=0;
  for(i=0;i<l-1;i++) {
    while(*p&&!isxdigit(*p)) p++;
    if(!*p) return;
    u=strtoul(p,(char**)&p,16); // convert from hex
    if(ISUPRINT(u)) q[i]=u;
    q[i+1]=0;
    }
  }

// switch between Unicode and printable mode
static int emchanged(GtkWidget*w,gpointer p) {
  int i;
  uchar s[MAXEQUIV+1];
  char t[MAXEQUIV*11+1];

  i=gtk_combo_box_get_active(GTK_COMBO_BOX(al_w21));
  if(i==aldiamode) return 1; // probably never happens
  switch(i) {
case 0: // convert U+xxxx... to text
    for(i=1;i<MAXICC;i++) {
      ustrtouchars(s,gtk_entry_get_text(GTK_ENTRY(alpha_r[i])),3);
      ucharstoutf8(t,s);
      gtk_entry_set_max_length(GTK_ENTRY(alpha_r[i]),2);
      gtk_entry_set_text(GTK_ENTRY(alpha_r[i]),t);
      ustrtouchars(s,gtk_entry_get_text(GTK_ENTRY(alpha_a[i])),MAXEQUIV+1);
      ucharstoutf8(t,s);
      gtk_entry_set_max_length(GTK_ENTRY(alpha_a[i]),MAXEQUIV);
      gtk_entry_set_text(GTK_ENTRY(alpha_a[i]),t);
      aldiamode=0;
      }
    break;
case 1: // convert text to U+xxxx...
    for(i=1;i<MAXICC;i++) {
      utf8touchars(s,gtk_entry_get_text(GTK_ENTRY(alpha_r[i])),3);
      ucharstostr(t,s);
      gtk_entry_set_max_length(GTK_ENTRY(alpha_r[i]),21);
      gtk_entry_set_text(GTK_ENTRY(alpha_r[i]),t);
      utf8touchars(s,gtk_entry_get_text(GTK_ENTRY(alpha_a[i])),MAXEQUIV+1);
      ucharstostr(t,s);
      gtk_entry_set_max_length(GTK_ENTRY(alpha_a[i]),MAXEQUIV*11);
      gtk_entry_set_text(GTK_ENTRY(alpha_a[i]),t);
      aldiamode=1;
      }
    break;
    }
  return 1;
  }

static int alphainit(GtkWidget*w,gpointer data) {
  int i;
  struct alphaentry*a;
  i=gtk_combo_box_get_active(GTK_COMBO_BOX(al_w20))-1; // the zeroth entry is the title
  if(i<0||i>=NALPHAINIT) return 1;
  a=alphainitdata[i];
  for(i=1;i<MAXICC;i++) {
    if(a[i].rep) gtk_entry_set_text(GTK_ENTRY(alpha_r[i]),a[i].rep);
    else         gtk_entry_set_text(GTK_ENTRY(alpha_r[i]),"");
    if(a[i].eqv) gtk_entry_set_text(GTK_ENTRY(alpha_a[i]),a[i].eqv);
    else         gtk_entry_set_text(GTK_ENTRY(alpha_a[i]),"");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_vs[i]),a[i].vow);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_cs[i]),a[i].con);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_sq[i]),a[i].seq);
    }
  aldiamode=0;
  emchanged(0,0);
  gtk_combo_box_set_active(GTK_COMBO_BOX(al_w20),0);
  return 1;
  }

static int al_insrow(GtkWidget*w,gpointer data) {
  int i,j;
  i=(GtkWidget**)data-alpha_r;
  for(j=MAXICC-2;j>=i;j--) {
    gtk_entry_set_text(GTK_ENTRY(alpha_r[j+1]),gtk_entry_get_text(GTK_ENTRY(alpha_r[j])));
    gtk_entry_set_text(GTK_ENTRY(alpha_a[j+1]),gtk_entry_get_text(GTK_ENTRY(alpha_a[j])));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_vs[j+1]),gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alpha_vs[j])));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_cs[j+1]),gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alpha_cs[j])));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_sq[j+1]),gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alpha_sq[j])));
    }
  gtk_entry_set_text(GTK_ENTRY(alpha_r[i]),"");
  gtk_entry_set_text(GTK_ENTRY(alpha_a[i]),"");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_vs[i]),0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_cs[i]),0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_sq[i]),0);
  return 1;
  }

static int al_shfrow(GtkWidget*w,gpointer data) {
  int i,j;
  char s[MAXEQUIV*100+1];
  i=(GtkWidget**)data-alpha_r;
  if(i<1||i>=MAXICC-1) return 1;
  strcpy(s,gtk_entry_get_text(GTK_ENTRY(alpha_r[i]))); gtk_entry_set_text(GTK_ENTRY(alpha_r[i]),gtk_entry_get_text(GTK_ENTRY(alpha_r[i+1]))); gtk_entry_set_text(GTK_ENTRY(alpha_r[i+1]),s);
  strcpy(s,gtk_entry_get_text(GTK_ENTRY(alpha_a[i]))); gtk_entry_set_text(GTK_ENTRY(alpha_a[i]),gtk_entry_get_text(GTK_ENTRY(alpha_a[i+1]))); gtk_entry_set_text(GTK_ENTRY(alpha_a[i+1]),s);
  j=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alpha_vs[i])); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_vs[i]),gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alpha_vs[i+1]))); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_vs[i+1]),j);
  j=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alpha_cs[i])); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_cs[i]),gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alpha_cs[i+1]))); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_cs[i+1]),j);
  j=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alpha_sq[i])); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_sq[i]),gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alpha_sq[i+1]))); gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_sq[i+1]),j);
  return 1;
  }

static int al_delrow(GtkWidget*w,gpointer data) {
  int i,j;
  i=(GtkWidget**)data-alpha_r;
  for(j=i;j<MAXICC-1;j++) {
    gtk_entry_set_text(GTK_ENTRY(alpha_r[j]),gtk_entry_get_text(GTK_ENTRY(alpha_r[j+1])));
    gtk_entry_set_text(GTK_ENTRY(alpha_a[j]),gtk_entry_get_text(GTK_ENTRY(alpha_a[j+1])));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_vs[j]),gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alpha_vs[j+1])));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_cs[j]),gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alpha_cs[j+1])));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_sq[j]),gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alpha_sq[j+1])));
    }
  i=MAXICC-1;
  gtk_entry_set_text(GTK_ENTRY(alpha_r[i]),"");
  gtk_entry_set_text(GTK_ENTRY(alpha_a[i]),"");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_vs[i]),0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_cs[i]),0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_sq[i]),0);
  return 1;
  }

static int doiccls(GtkWidget*w,gpointer data) {
  int f1,f2,i,j;
  char s[20];
  uchar t[3];
  for(i=1,j=1;i<MAXICC;i++) {
    if(aldiamode==0) utf8touchars(t,gtk_entry_get_text(GTK_ENTRY(alpha_r[i])),3);
    else             ustrtouchars(t,gtk_entry_get_text(GTK_ENTRY(alpha_r[i])),3);
    f1=(ISUPRINT(t[0])&&t[1]==0); // exactly one printable character?
    f2=(ISUPRINT(t[0])&&ISUPRINT(t[1])&&t[2]==0); // pair?

    sprintf(s,"<b>%d</b>",j);
    rowtoicc[i]=j; // currently the identity function
    j++;

// this code if we want to try to skip assigning ICCs to unused rows
//   if(f1||f2) {
//     sprintf(s,"<b>%d</b>",j);
//     rowtoicc[i]=j;
//     j++;
//     }
//   else {
//     strcpy(s,"<b>-</b>");
//     rowtoicc[i]=-1;
//     }

    gtk_label_set_markup(GTK_LABEL(alpha_iccl[i]),s);
    gtk_widget_set_sensitive(alpha_iccl[i],f1);
    gtk_widget_set_sensitive(alpha_vs[i],f1);
    gtk_widget_set_sensitive(alpha_cs[i],f1);
    gtk_widget_set_sensitive(alpha_sq[i],f1);
    gtk_widget_set_sensitive(alpha_a [i],f1||f2); // this entry needs to be sensitive for letter pairs too
    }
  return 1;
  }

static int alphadia(void) {
  GtkWidget*dia,*l,*t,*b,*w0;
  GtkRequisition sr;
  int i,j,u;
  char s[SLEN];

  filler_stop();
  if(stats!=NULL) gtk_widget_destroy(stats);
  stats=NULL;
  dia=gtk_dialog_new_with_buttons("Alphabet",GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_APPLY,GTK_RESPONSE_OK,NULL);

  t=gtk_table_new(MAXICC+2,6,0);

//  l=gtk_label_new("          ");                                                                     gtk_table_attach(GTK_TABLE(t),l,1,2,0,1,0,0,5,5);
//  l=gtk_label_new("  Choose language default:  ");                                                   gtk_table_attach(GTK_TABLE(t),l,2,3,0,1,0,0,5,5);

  al_w20=gtk_combo_box_new_text();                                                                   gtk_table_attach(GTK_TABLE(t),al_w20,0,1,0,1,0,0,5,5);
  gtk_combo_box_append_text(GTK_COMBO_BOX(al_w20),"Initialise from language default...");
  for(i=0;i<NALPHAINIT;i++) gtk_combo_box_append_text(GTK_COMBO_BOX(al_w20),alphaname[i][0]);
  gtk_combo_box_set_active(GTK_COMBO_BOX(al_w20),0);
  gtk_signal_connect(GTK_OBJECT(al_w20),"changed",GTK_SIGNAL_FUNC(alphainit),NULL);


//  b=gtk_button_new_with_label(" Initialise ");                                                       gtk_table_attach(GTK_TABLE(t),b,4,5,0,1,0,0,5,5);
//  gtk_signal_connect(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(alphainit),0);

  al_w21=gtk_combo_box_new_text();                                                                   gtk_table_attach(GTK_TABLE(t),al_w21,3,4,0,1,0,0,5,5);
  gtk_combo_box_append_text(GTK_COMBO_BOX(al_w21),"Edit as printable characters");
  gtk_combo_box_append_text(GTK_COMBO_BOX(al_w21),"Edit as Unicode code points");
  gtk_signal_connect(GTK_OBJECT(al_w21),"changed",GTK_SIGNAL_FUNC(emchanged),NULL); // for update of labels

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),t,TRUE,TRUE,0);

  w0=gtk_scrolled_window_new(0,0);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(w0),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_widget_set_size_request(w0,-1,350);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),w0,TRUE,TRUE,0);
  t=gtk_table_new(MAXICC+2,9,0);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(w0),t);

  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Entry</b>"                          ); gtk_table_attach(GTK_TABLE(t),l,1,2,1,2,0,0,5,5);
  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Equivalents in dictionary files</b>"); gtk_table_attach(GTK_TABLE(t),l,2,3,1,2,0,0,5,5);
  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>In @ set?</b>"                      ); gtk_table_attach(GTK_TABLE(t),l,3,4,1,2,0,0,5,5);
  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>In # set?</b>"                      ); gtk_table_attach(GTK_TABLE(t),l,4,5,1,2,0,0,5,5);
  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Sequence\ncontinues?</b>"           ); gtk_table_attach(GTK_TABLE(t),l,5,6,1,2,0,0,5,5);

  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Edit rows</b>"                      ); gtk_table_attach(GTK_TABLE(t),l,6,9,1,2,0,0,5,5);

  for(i=1;i<MAXICC;i++) {
    sprintf(s,"<b>%d</b>",i);
    alpha_iccl[i]=gtk_label_new(NULL);                                                       gtk_table_attach(GTK_TABLE(t),alpha_iccl[i],0,1,i+1,i+2,0,0,5,0);
    alpha_r[i]=gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(alpha_r[i]),10);
    gtk_entry_set_alignment(GTK_ENTRY(alpha_r[i]),.5);                                       gtk_table_attach(GTK_TABLE(t),alpha_r[i]   ,1,2,i+1,i+2,0,0,0,0);
    gtk_entry_set_max_length(GTK_ENTRY(alpha_r[i]),2);
    if(icctouchar[i]==0x3f) gtk_entry_set_text(GTK_ENTRY(alpha_r[i]),""); // "?" indicates unused ICC
    else                    gtk_entry_set_text(GTK_ENTRY(alpha_r[i]),icctoutf8[i]);
    gtk_signal_connect(GTK_OBJECT(alpha_r[i]),"changed",GTK_SIGNAL_FUNC(doiccls),NULL); // for update of labels

    alpha_a[i]=gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(alpha_a[i]),50);                                     gtk_table_attach(GTK_TABLE(t),alpha_a[i]   ,2,3,i+1,i+2,0,0,0,0);
    gtk_entry_set_max_length(GTK_ENTRY(alpha_a[i]),MAXEQUIV);
    gtk_entry_set_text(GTK_ENTRY(alpha_a[i]),iccequivs[i]);

    alpha_vs[i]=gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_vs[i]),!!(ICCTOABM(i)&abm_vow));    gtk_table_attach(GTK_TABLE(t),alpha_vs[i]  ,3,4,i+1,i+2,0,0,0,0);
    alpha_cs[i]=gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_cs[i]),!!(ICCTOABM(i)&abm_con));    gtk_table_attach(GTK_TABLE(t),alpha_cs[i]  ,4,5,i+1,i+2,0,0,0,0);
    alpha_sq[i]=gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alpha_sq[i]),iccseq[i]);                  gtk_table_attach(GTK_TABLE(t),alpha_sq[i]  ,5,6,i+1,i+2,0,0,0,0);

    if(i<MAXICC-1) {
      b=gtk_button_new_with_label("+"); gtk_widget_set_tooltip_text(b,"Insert new row");                 gtk_table_attach(GTK_TABLE(t),b,6,7,i+1,i+2,0,0,2,0); // ✚✖✕⁁⏷↧⇐⇵∇⇅
      gtk_signal_connect(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(al_insrow),(void*)&alpha_r[i]);
      b=gtk_button_new_with_label("⇅"); gtk_widget_set_tooltip_text(b,"Shuffle row down");               gtk_table_attach(GTK_TABLE(t),b,7,8,i+1,i+2,0,0,2,0);
      gtk_signal_connect(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(al_shfrow),(void*)&alpha_r[i]);
      b=gtk_button_new_with_label("✕"); gtk_widget_set_tooltip_text(b,"Delete row");                     gtk_table_attach(GTK_TABLE(t),b,8,9,i+1,i+2,0,0,2,0);
      gtk_signal_connect(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(al_delrow),(void*)&alpha_r[i]);
      }
    }

  doiccls(0,0);
  gtk_combo_box_set_active(GTK_COMBO_BOX(al_w21),0);

  gtk_widget_show_all(dia);
  gtk_widget_hide(alpha_sq[MAXICC-1]);
  gtk_widget_get_requisition(t,&sr);
  gtk_widget_set_size_request(w0,sr.width+40,450);
  gtk_window_set_focus(GTK_WINDOW(dia),NULL);
  j=gtk_dialog_run(GTK_DIALOG(dia));
  if(j==GTK_RESPONSE_CANCEL||j==GTK_RESPONSE_DELETE_EVENT) goto ew0;
  gtk_combo_box_set_active(GTK_COMBO_BOX(al_w21),0); // force to normal text mode
  clearalphamap();
  for(i=1,j=0;i<MAXICC;i++) {
    if(rowtoicc[i]>=0) {
      u=addalphamapentry(rowtoicc[i],
                         (char*)gtk_entry_get_text(GTK_ENTRY(alpha_r[i])),
                         (char*)gtk_entry_get_text(GTK_ENTRY(alpha_a[i])),
                         !!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alpha_vs[i])),
                         !!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alpha_cs[i])),
                         !!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(alpha_sq[i])));
      j|=1<<u; // accumulate error codes
      }
    }
  if(j&2) reperr("Entries may not be space characters or\nany of the following: . - ? , [ ] ^ \"");
  if(j&4) reperr("Space characters and the following: . - ? , [ ] ^ \"\nare always ignored in dictionary files");
  DEB_AL {
    for(i=0;i<niccused;i++) {
      u=iccused[i];
      printf("%2d ICC:%2d %05x g%d\n",i,u,icctouchar[u],iccusedtogroup[i]);
      }
    printf("%d",iccgroupstart[0]);
    for(i=1;i<MAXICCGROUP;i++) printf("-%d",iccgroupstart[i]);
    printf("\n");
    }
  initcodeword();
  loaddicts(0);
  reloadtpi();
  gridchange();
  refreshall();
  unsaved=1;
ew0:
  gtk_widget_destroy(dia);
  return 1;
  }

// grid properties dialogue

static GtkWidget*gprop_lx,*gprop_ly,*gprop_w20;

static int gtchanged(GtkWidget*w,gpointer p) {
  int i;

  i=gtk_combo_box_get_active(GTK_COMBO_BOX(gprop_w20));
  switch(gshape[i]) {
  case 0: case 1: case 2:
    gtk_label_set_text(GTK_LABEL(gprop_lx)," columns");
    gtk_label_set_text(GTK_LABEL(gprop_ly)," rows");
    break;
  case 3: case 4:
    gtk_label_set_text(GTK_LABEL(gprop_lx)," radii");
    gtk_label_set_text(GTK_LABEL(gprop_ly)," annuli");
    break;
    }
  return 1;
  }

static int gpropdia(void) {
  GtkWidget*dia,*l,*w,*w30,*w31,*vb,*t,*ttl,*aut;
  int i,j,i0,j0,m,nw,nh;
  dia=gtk_dialog_new_with_buttons("Grid properties",GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_APPLY,GTK_RESPONSE_OK,NULL);
  vb=gtk_vbox_new(0,3); // box to hold all the options
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),vb,TRUE,TRUE,0);

  t=gtk_table_new(2,2,0);                                                           gtk_box_pack_start(GTK_BOX(vb),t,TRUE,TRUE,0);
  l=gtk_label_new("Title: ");                                                       gtk_table_attach(GTK_TABLE(t),l,0,1,0,1,0,0,0,0);
  gtk_label_set_width_chars(GTK_LABEL(l),10);
  gtk_misc_set_alignment(GTK_MISC(l),1,0.5);
  ttl=gtk_entry_new();                                                              gtk_table_attach(GTK_TABLE(t),ttl,1,2,0,1,0,0,0,0);
  gtk_entry_set_max_length(GTK_ENTRY(ttl),SLEN-1);
  gtk_entry_set_text(GTK_ENTRY(ttl),gtitle);
  l=gtk_label_new("Author: ");                                                      gtk_table_attach(GTK_TABLE(t),l,0,1,1,2,0,0,0,0);
  gtk_label_set_width_chars(GTK_LABEL(l),10);
  gtk_misc_set_alignment(GTK_MISC(l),1,0.5);
  aut=gtk_entry_new();                                                              gtk_table_attach(GTK_TABLE(t),aut,1,2,1,2,0,0,0,0);
  gtk_entry_set_max_length(GTK_ENTRY(aut),SLEN-1);
  gtk_entry_set_text(GTK_ENTRY(aut),gauthor);

  w=gtk_hseparator_new();                                                           gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  w=gtk_hbox_new(0,2);                                                              gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  l=gtk_label_new("Grid type: ");                                                   gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  gtk_label_set_width_chars(GTK_LABEL(l),10);
  gtk_misc_set_alignment(GTK_MISC(l),1,0.5);
  gprop_w20=gtk_combo_box_new_text();                                               gtk_box_pack_start(GTK_BOX(w),gprop_w20,FALSE,FALSE,0);
  for(i=0;i<NGTYPE;i++) gtk_combo_box_append_text(GTK_COMBO_BOX(gprop_w20),gtypedesc[i]);

  w=gtk_hseparator_new();                                                           gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  w=gtk_hbox_new(0,2);                                                              gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  l=gtk_label_new("Size: ");                                                        gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  gtk_label_set_width_chars(GTK_LABEL(l),10);
  gtk_misc_set_alignment(GTK_MISC(l),1,0.5);
  w30=gtk_spin_button_new_with_range(1,MXSZ,1);                                     gtk_box_pack_start(GTK_BOX(w),w30,FALSE,FALSE,0);
  gprop_lx=gtk_label_new(" ");                                                      gtk_box_pack_start(GTK_BOX(w),gprop_lx,FALSE,FALSE,0);
  w=gtk_hbox_new(0,2);                                                              gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);
  l=gtk_label_new("by ");                                                           gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  gtk_label_set_width_chars(GTK_LABEL(l),10);
  gtk_misc_set_alignment(GTK_MISC(l),1,0.5);
  w31=gtk_spin_button_new_with_range(1,MXSZ,1);                                     gtk_box_pack_start(GTK_BOX(w),w31,FALSE,FALSE,0);
  gprop_ly=gtk_label_new(" ");                                                      gtk_box_pack_start(GTK_BOX(w),gprop_ly,FALSE,FALSE,0);
  w=gtk_hseparator_new();                                                           gtk_box_pack_start(GTK_BOX(vb),w,TRUE,TRUE,0);

  gtk_signal_connect(GTK_OBJECT(gprop_w20),"changed",GTK_SIGNAL_FUNC(gtchanged),NULL); // for update of labels

  // set widgets from current values
  gtk_combo_box_set_active(GTK_COMBO_BOX(gprop_w20),gtype);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(w30),width);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(w31),height);

  gtk_widget_show_all(dia);

  i=gtk_dialog_run(GTK_DIALOG(dia));
  if(i==GTK_RESPONSE_OK) {
    // resync everything
    i=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w30)); if(i>=1&&i<=MXSZ) nw=i; else nw=width;
    i=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w31)); if(i>=1&&i<=MXSZ) nh=i; else nh=height;
    for(i=0;i<nw;i++) for(j=0;j<nh;j++) if(i>=width||j>=height) initsquare(i,j);
    width=nw; height=nh;

    for(i=0,i0=0;i<nvl;i++) {
      for(j=0,j0=0;j<vls[i].l;j++) if(vls[i].x[j]<width&&vls[i].y[j]<height) vls[i0].x[j0]=vls[i].x[j],vls[i0].y[j0]=vls[i].y[j],j0++;
      vls[i0].l=j0;
      if(j0>0) i0++; // have we lost the free light completely?
      }
    nvl=i0;

    i=gtk_combo_box_get_active(GTK_COMBO_BOX(gprop_w20)); if(i>=0&&i<NGTYPE) gtype=i;
    strncpy(gtitle ,gtk_entry_get_text(GTK_ENTRY(ttl)),SLEN-1); gtitle [SLEN-1]=0;
    strncpy(gauthor,gtk_entry_get_text(GTK_ENTRY(aut)),SLEN-1); gauthor[SLEN-1]=0;
    gridchange();
    }

  gtk_widget_destroy(dia);
  draw_init();
  m=symmrmask(); if(((1<<symmr)&m)==0) symmr=1;
  m=symmmmask(); if(((1<<symmm)&m)==0) symmm=0;
  m=symmdmask(); if(((1<<symmd)&m)==0) symmd=0;
  syncgui();
  return 1;
  }


static int mvldia(int v) {
  GtkWidget*dia,*vb,*v0,*w0,*l;
  GtkTextBuffer*tbuf;
  char*p,*q,s[MXLE*10+1];
  int x[MXCL],y[MXCL];
  int i;

  dia=gtk_dialog_new_with_buttons("Modify free light path",GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_APPLY,GTK_RESPONSE_OK,NULL);
  vb=gtk_vbox_new(0,3);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),vb,TRUE,TRUE,0);
  l=gtk_label_new("  Enter a coordinate pair for each cell in the path on a  ");   gtk_box_pack_start(GTK_BOX(vb),l,FALSE,TRUE,0);
  gtk_misc_set_alignment(GTK_MISC(l),0,0.5);
  l=gtk_label_new("  separate line. Coordinates are counted from zero.  ");        gtk_box_pack_start(GTK_BOX(vb),l,FALSE,TRUE,0);
  gtk_misc_set_alignment(GTK_MISC(l),0,0.5);
  for(i=0,p=s;i<vls[v].l;i++) sprintf(p,"%d,%d\n",vls[v].x[i],vls[v].y[i]),p+=strlen(p);
  w0=gtk_scrolled_window_new(0,0);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(w0),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start(GTK_BOX(vb),w0,TRUE,TRUE,0);
  v0=gtk_text_view_new();
  gtk_widget_set_size_request(v0,-1,200);
  tbuf=gtk_text_view_get_buffer(GTK_TEXT_VIEW(v0));
  gtk_text_buffer_set_text(tbuf,s,-1);
  gtk_container_add(GTK_CONTAINER(w0),v0);

  gtk_widget_show_all(dia);

ew0:
  i=gtk_dialog_run(GTK_DIALOG(dia));
  if(i==GTK_RESPONSE_OK) {
    GtkTextIter i0,i1;
    gtk_text_buffer_get_start_iter(tbuf,&i0);
    gtk_text_buffer_get_end_iter  (tbuf,&i1);
    p=gtk_text_buffer_get_text(tbuf,&i0,&i1,FALSE);
    for(i=0;i<=MXCL;i++) {
      while(isspace((unsigned char)*p)) p++; // skips blank lines
      if(*p==0) break;
      if(!isdigit((unsigned char)*p)) goto err0;
      if(i==MXCL) goto err1;
      x[i]=strtol(p,&q,10);
      if(x[i]<0) x[i]=0;
      if(x[i]>=MXSZ) x[i]=MXSZ-1;
      if(q==p) goto err0;
      p=q;
      while(isspace((unsigned char)*p)||*p==',') p++;
      y[i]=strtol(p,&q,10);
      if(y[i]<0) y[i]=0;
      if(y[i]>=MXSZ) y[i]=MXSZ-1;
      if(q==p) goto err0;
      p=q;
      }
    if(i>0) {
      vls[v].l=i;
      memcpy(vls[v].x,x,i*sizeof(int));
      memcpy(vls[v].y,y,i*sizeof(int));
      }
    gridchange();
    }
  gtk_widget_destroy(dia);
  return 1;

err0: reperr("Each line must contain\ntwo numeric coordinate values\nseparated by a comma or space"); goto ew0;
err1: reperr("Free light length limit reached"); goto ew0;
  }

GtkWidget*dictl_e1;

static int ddict_browse(GtkWidget*w,gpointer p) {
  GtkWidget*dia;
  dia=gtk_file_chooser_dialog_new("Choose a dictionary",0,
    GTK_FILE_CHOOSER_ACTION_OPEN,
    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
    GTK_STOCK_OPEN, GTK_RESPONSE_OK,
    NULL);
  if(gtk_dialog_run(GTK_DIALOG(dia))==GTK_RESPONSE_OK)
    gtk_entry_set_text(GTK_ENTRY(dictl_e1),gtk_file_chooser_get_filename((GtkFileChooser*)dia));
  gtk_widget_destroy(dia);
  return 1;
  }

// preferences dialogue
static int prefsdia(void) {
  GtkWidget*dia,*l,*w,*w30,*w31,*w32,*w00,*w01,*w02,*w20,*w21,*w10,*w11,*w12,*w14,*t,*le0[NLOOKUP],*le1[NLOOKUP],*vb,*nb,*dal,*b,*f01,*f11;
  int i;
  char s[SLEN+1];
  dia=gtk_dialog_new_with_buttons("Preferences",GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,
    GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_APPLY,GTK_RESPONSE_OK,NULL);
  nb=gtk_notebook_new();
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),nb,FALSE,TRUE,0);

  vb=gtk_vbox_new(0,3); // box to hold all the options
  gtk_notebook_append_page(GTK_NOTEBOOK(nb),vb,gtk_label_new("Export"));
//  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dia)->vbox),vb,TRUE,TRUE,0);

  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Export preferences</b>");         gtk_box_pack_start(GTK_BOX(vb),l,FALSE,FALSE,0);
  w=gtk_hbox_new(0,3);                                                                          gtk_box_pack_start(GTK_BOX(vb),w,FALSE,FALSE,0);
  l=gtk_label_new(" EPS/SVG export square size: ");                                             gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  w30=gtk_spin_button_new_with_range(10,72,1);                                                  gtk_box_pack_start(GTK_BOX(w),w30,FALSE,FALSE,0);
  l=gtk_label_new(" points");                                                                   gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  w=gtk_hseparator_new();                                                                       gtk_box_pack_start(GTK_BOX(vb),w,FALSE,FALSE,0);
  w=gtk_hbox_new(0,3);                                                                          gtk_box_pack_start(GTK_BOX(vb),w,FALSE,FALSE,0);
  l=gtk_label_new(" HTML/PNG export square size: ");                                            gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  w31=gtk_spin_button_new_with_range(10,72,1);                                                  gtk_box_pack_start(GTK_BOX(w),w31,FALSE,FALSE,0);
  l=gtk_label_new(" pixels");                                                                   gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  w=gtk_hseparator_new();                                                                       gtk_box_pack_start(GTK_BOX(vb),w,FALSE,FALSE,0);
  w32=gtk_check_button_new_with_label("Include numbers in filled grids");                       gtk_box_pack_start(GTK_BOX(vb),w32,FALSE,FALSE,0);

  vb=gtk_vbox_new(0,3); // box to hold all the options
  gtk_notebook_append_page(GTK_NOTEBOOK(nb),vb,gtk_label_new("Editing"));

  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Editing preferences</b>");        gtk_box_pack_start(GTK_BOX(vb),l,FALSE,FALSE,0);
  w00=gtk_check_button_new_with_label("Clicking corners makes blocks");                         gtk_box_pack_start(GTK_BOX(vb),w00,FALSE,FALSE,0);
  w=gtk_hseparator_new();                                                                       gtk_box_pack_start(GTK_BOX(vb),w,FALSE,FALSE,0);
  w01=gtk_check_button_new_with_label("Clicking edges makes bars");                             gtk_box_pack_start(GTK_BOX(vb),w01,FALSE,FALSE,0);
  w=gtk_hseparator_new();                                                                       gtk_box_pack_start(GTK_BOX(vb),w,FALSE,FALSE,0);
  w02=gtk_check_button_new_with_label("Show numbers while editing");                            gtk_box_pack_start(GTK_BOX(vb),w02,FALSE,FALSE,0);

  vb=gtk_vbox_new(0,3); // box to hold all the options
  gtk_notebook_append_page(GTK_NOTEBOOK(nb),vb,gtk_label_new("Statistics"));

  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Statistics preferences</b>");     gtk_box_pack_start(GTK_BOX(vb),l,FALSE,FALSE,0);
  l=gtk_label_new(" Desirable checking ratios");gtk_misc_set_alignment(GTK_MISC(l),0,0.5);      gtk_box_pack_start(GTK_BOX(vb),l,FALSE,FALSE,0);
  w=gtk_hbox_new(0,3);                                                                          gtk_box_pack_start(GTK_BOX(vb),w,FALSE,FALSE,0);
  l=gtk_label_new(" Minimum: ");                                                                gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  w20=gtk_spin_button_new_with_range(0,100,1);                                                  gtk_box_pack_start(GTK_BOX(w),w20,FALSE,FALSE,0);
  l=gtk_label_new("%   Maximum: ");                                                             gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  w21=gtk_spin_button_new_with_range(0,100,1);                                                  gtk_box_pack_start(GTK_BOX(w),w21,FALSE,FALSE,0);
  l=gtk_label_new("% plus one cell  ");                                                         gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);

  vb=gtk_vbox_new(0,3); // box to hold all the options
  gtk_notebook_append_page(GTK_NOTEBOOK(nb),vb,gtk_label_new("Autofill"));

  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Autofill preferences</b>");       gtk_box_pack_start(GTK_BOX(vb),l,FALSE,FALSE,0);
  w10=gtk_radio_button_new_with_label_from_widget(NULL,"Deterministic");                        gtk_box_pack_start(GTK_BOX(vb),w10,FALSE,FALSE,0);
  w11=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(w10),"Slightly randomised"); gtk_box_pack_start(GTK_BOX(vb),w11,FALSE,FALSE,0);
  w12=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(w11),"Highly randomised");   gtk_box_pack_start(GTK_BOX(vb),w12,FALSE,FALSE,0);
  w=gtk_hseparator_new();                                                                       gtk_box_pack_start(GTK_BOX(vb),w,FALSE,FALSE,0);
  w14=gtk_check_button_new_with_label("Prevent duplicate answers and lights");                  gtk_box_pack_start(GTK_BOX(vb),w14,FALSE,FALSE,0);

  vb=gtk_vbox_new(0,3); // box to hold all the options
  gtk_notebook_append_page(GTK_NOTEBOOK(nb),vb,gtk_label_new("Lookup"));

  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Lookup preferences</b>");         gtk_box_pack_start(GTK_BOX(vb),l,FALSE,FALSE,0);

  t=gtk_table_new(NLOOKUP+1,3,0);                                                               gtk_box_pack_start(GTK_BOX(vb),t,FALSE,FALSE,0);
  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Name</b>");                       gtk_table_attach(GTK_TABLE(t),l,1,2,0,1,0,0,0,0);
  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>URI (must start \"http://\" or \"https://\")</b>");      gtk_table_attach(GTK_TABLE(t),l,2,3,0,1,0,0,0,0);
  for(i=0;i<NLOOKUP;i++) {
    sprintf(s,"<b>%d</b>",i+1);
    l=gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(l),s);
    gtk_table_attach(GTK_TABLE(t),l,0,1,i+1,i+2,0,0,5,0);
    le0[i]=gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(le0[i]),SLEN-1);
    gtk_entry_set_width_chars(GTK_ENTRY(le0[i]),30);
    gtk_entry_set_text(GTK_ENTRY(le0[i]),lookupname(i));
    gtk_table_attach(GTK_TABLE(t),le0[i],1,2,i+1,i+2,0,0,0,0);
    le1[i]=gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(le1[i]),SLEN-1);
    gtk_entry_set_width_chars(GTK_ENTRY(le1[i]),50);
    gtk_entry_set_text(GTK_ENTRY(le1[i]),lookup(i));
    gtk_table_attach(GTK_TABLE(t),le1[i],2,3,i+1,i+2,0,0,5,0);
    }

  vb=gtk_vbox_new(0,3); // box to hold all the options
  gtk_notebook_append_page(GTK_NOTEBOOK(nb),vb,gtk_label_new("Startup"));

  l=gtk_label_new(NULL);gtk_label_set_markup(GTK_LABEL(l),"<b>Startup preferences</b>");        gtk_box_pack_start(GTK_BOX(vb),l,FALSE,FALSE,0);

  t=gtk_table_new(4,3,0);                                                                       gtk_box_pack_start(GTK_BOX(vb),t,FALSE,FALSE,0);
  l=gtk_label_new(" Default dictionary in slot 1 at startup: ");                                gtk_table_attach(GTK_TABLE(t),l,0,1,0,1,0,0,0,0);
  dictl_e1=gtk_entry_new(); gtk_entry_set_max_length(GTK_ENTRY(dictl_e1),SLEN-1);               gtk_table_attach(GTK_TABLE(t),dictl_e1,1,2,0,1,0,0,0,0);
  gtk_entry_set_text(GTK_ENTRY(dictl_e1),startup_dict1);
  b=gtk_button_new_with_label(" Browse... ");                                                   gtk_table_attach(GTK_TABLE(t),b,2,3,0,1,0,0,0,0);
  l=gtk_label_new(" Default file filter: "); gtk_misc_set_alignment(GTK_MISC(l),1.0,0.5);       gtk_table_attach(GTK_TABLE(t),l,0,1,1,2,0,0,0,0);
  f01=gtk_entry_new(); gtk_entry_set_max_length(GTK_ENTRY(f01),SLEN-1);                         gtk_table_attach(GTK_TABLE(t),f01,1,2,1,2,0,0,0,0);
  gtk_entry_set_text(GTK_ENTRY(f01),startup_ff1);
  l=gtk_label_new(" Default answer filter: ");                                                  gtk_table_attach(GTK_TABLE(t),l,0,1,2,3,0,0,0,0);
  f11=gtk_entry_new(); gtk_entry_set_max_length(GTK_ENTRY(f11),SLEN-1);                         gtk_table_attach(GTK_TABLE(t),f11,1,2,2,3,0,0,0,0);
  gtk_entry_set_text(GTK_ENTRY(f11),startup_af1);

  gtk_signal_connect(GTK_OBJECT(b),"clicked",GTK_SIGNAL_FUNC(ddict_browse),0);

  w=gtk_hseparator_new();                                                                       gtk_box_pack_start(GTK_BOX(vb),w,FALSE,FALSE,0);

  w=gtk_hbox_new(0,3);                                                                          gtk_box_pack_start(GTK_BOX(vb),w,FALSE,FALSE,0);
  l=gtk_label_new(" Default alphabet at startup: ");                                            gtk_box_pack_start(GTK_BOX(w),l,FALSE,FALSE,0);
  dal=gtk_combo_box_new_text();                                                                 gtk_box_pack_start(GTK_BOX(w),dal,FALSE,FALSE,0);
  for(i=0;i<NALPHAINIT;i++) gtk_combo_box_append_text(GTK_COMBO_BOX(dal),alphaname[i][0]);
  gtk_combo_box_set_active(GTK_COMBO_BOX(dal),startup_al);

  // set widgets from current preferences values
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(w30),eptsq);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(w31),hpxsq);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w32),lnis);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w00),clickblock);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w01),clickbar);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w02),shownums);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w14),afunique);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(w20),mincheck);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(w21),maxcheck);
  if(afrandom==0) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w10),1);
  if(afrandom==1) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w11),1);
  if(afrandom==2) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w12),1);

  gtk_widget_show_all(dia);
  i=gtk_dialog_run(GTK_DIALOG(dia));
  if(i==GTK_RESPONSE_OK) {
    // set preferences values back from values (with bounds checking)
    eptsq=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w30));
    if(eptsq<10) eptsq=10;
    if(eptsq>72) eptsq=72;
    hpxsq=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w31));
    if(hpxsq<10) hpxsq=10;
    if(hpxsq>72) hpxsq=72;
    lnis=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w32))&1;
    clickblock=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w00))&1;
    clickbar=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w01))&1;
    shownums=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w02))&1;
    mincheck=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w20));
    if(mincheck<  0) mincheck=  0;
    if(mincheck>100) mincheck=100;
    maxcheck=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w21));
    if(maxcheck<  0) maxcheck=  0;
    if(maxcheck>100) maxcheck=100;
    afunique=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w14));
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w10))) afrandom=0;
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w11))) afrandom=1;
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w12))) afrandom=2;
    for(i=0;i<NLOOKUP;i++) {
      strncpy(lookupname(i),gtk_entry_get_text(GTK_ENTRY(le0[i])),SLEN-1); lookupname(i)[SLEN]=0;
      strncpy(lookup(i)    ,gtk_entry_get_text(GTK_ENTRY(le1[i])),SLEN-1); lookup    (i)[SLEN]=0;
      }
    strncpy(startup_dict1,gtk_entry_get_text(GTK_ENTRY(dictl_e1)),SLEN-1); startup_dict1[SLEN]=0;
    strncpy(startup_ff1  ,gtk_entry_get_text(GTK_ENTRY(f01     )),SLEN-1); startup_ff1  [SLEN]=0;
    strncpy(startup_af1  ,gtk_entry_get_text(GTK_ENTRY(f11     )),SLEN-1); startup_af1  [SLEN]=0;
    startup_al=gtk_combo_box_get_active(GTK_COMBO_BOX(dal));
    if(startup_al<0||startup_al>=NALPHAINIT) startup_al=ALPHABET_AZ09;
    saveprefs(); // save to preferences file (failing silently)
    }
  gtk_widget_destroy(dia);
  refreshall();
  compute(0); // because of new checking ratios
  return 1;
  }



// update statistics window if in view
// main widget table is st_te[][], rows below in st_r[]
void stats_upd(void) {
  int i,j;
  char s[SLEN];

  if(!usegui) return;
  if(!stats) return;
  for(i=2;i<=MXLE;i++) { // one row for each word length
    if(st_lc[i]>0) {
      sprintf(s,"%d",i);                                                                                      gtk_label_set_text(GTK_LABEL(st_te[i][0]),s);
      sprintf(s,"%d",st_lc[i]);                                                                               gtk_label_set_text(GTK_LABEL(st_te[i][1]),s);
      sprintf(s,st_lucc[i]?"%d (%.1f%%)":" ",st_lucc[i],100.0*st_lucc[i]/st_lc[i]);                           gtk_label_set_text(GTK_LABEL(st_te[i][2]),s);
      sprintf(s,st_locc[i]?"%d (%.1f%%)":" ",st_locc[i],100.0*st_locc[i]/st_lc[i]);                           gtk_label_set_text(GTK_LABEL(st_te[i][3]),s);
      sprintf(s,st_lc[i]?"%.2f:%.2f:%.2f":" "   ,1.0*st_lmnc[i]/i,1.0*st_lsc[i]/st_lc[i]/i,1.0*st_lmxc[i]/i); gtk_label_set_text(GTK_LABEL(st_te[i][4]),s);
      for(j=0;j<5;j++) gtk_widget_show(st_te[i][j]); // show row if non-empty...
      }
    else for(j=0;j<5;j++) {gtk_label_set_text(GTK_LABEL(st_te[i][j])," ");gtk_widget_hide(st_te[i][j]);} // ... and hide row if empty
    }
  sprintf(s,"  Total lights: %d",nw0);                                                       gtk_label_set_text(GTK_LABEL(st_r[0]),s);
  if(nw0>0) sprintf(s,"  Mean length: %.1f",(double)nc/nw0);
  else      strcpy (s,"  Mean length: -");
  gtk_label_set_text(GTK_LABEL(st_r[1]),s);
  if(nc>0 ) sprintf(s,"  Checked light characters: %d/%d (%.1f%%)",st_sc,nc,100.0*st_sc/nc);
  else      strcpy (s,"  Checked light characters: -");
  gtk_label_set_text(GTK_LABEL(st_r[2]),s);
  if(ne0>0) sprintf(s,"  Checked grid cells: %d/%d (%.1f%%)",st_ce,ne0,100.0*st_ce/ne0);
  else      strcpy (s,"  Checked grid cells: -");
  gtk_label_set_text(GTK_LABEL(st_r[3]),s);
  sprintf(s,"  Lights with double unches: %d",st_2u-st_3u);                                  gtk_label_set_text(GTK_LABEL(st_r[4]),s);
  sprintf(s,"  Lights with triple, quadruple etc. unches: %d",st_3u);                        gtk_label_set_text(GTK_LABEL(st_r[5]),s);
  sprintf(s,"  Lights too long for filler: %d",st_tlf);                                      gtk_label_set_text(GTK_LABEL(st_r[6]),s);
  sprintf(s,"  Total free lights: %d",nvl);                                                  gtk_label_set_text(GTK_LABEL(st_r[7]),s);
  sprintf(s,"  Free lights too long for filler: %d",st_vltlf);                               gtk_label_set_text(GTK_LABEL(st_r[8]),s);
  if(st_tmtlf) sprintf(s,"  There are too many treated lights to use filler discretionary modes");
  else         sprintf(s,"  ");
                                                                                             gtk_label_set_text(GTK_LABEL(st_r[9]),s);
  if(hist_da->window) gdk_window_invalidate_rect(hist_da->window,0,0);
  }

// histogram update
static gint hist_configure_event(GtkWidget*widget,GdkEventConfigure*event) {
  DEB_RF printf("hist config event: new w=%d h=%d\n",widget->allocation.width,widget->allocation.height);
  return TRUE;
  }

#define HBARW 12 // width of bar
#define HBARG 4  // gap between bars

// draw histogram (if non-empty)
static void drawhist(cairo_t*cr) {
  int x=5,y0=224,w=HBARW;
  int c,g,h,i,m,u,v,mni,mxi;
  char s[SLEN];

  for(i=0,u=0;i<niccused;i++) u+=st_hist[(int)iccused[i]];
  if(u==0) return; // empty so skip
  for(i=0,m=1;i<niccused;i++) if(st_hist[(int)iccused[i]]>m) m=st_hist[(int)iccused[i]]; // max in histogram

  for(g=0;g<MAXICCGROUP;g++) {
    mni=iccgroupstart[g];
    mxi=iccgroupstart[g+1];
    if(mni==mxi) continue;
    for(v=0,i=mni;i<mxi;i++) v+=st_hist[(int)iccused[i]];
    if(v==0) continue; // none in this group?

    for(i=mni;i<mxi;i++) {
      c=iccused[i];
      v=st_hist[c];
      h=(v*150)/m;
      cairo_rectangle(cr,x,y0,w,-h);
      cairo_set_source_rgb(cr,0,0,0);
      cairo_stroke_preserve(cr);
           if(g==0) cairo_set_source_rgb(cr,0,1,0); // group 0 in green
      else if(g==1) cairo_set_source_rgb(cr,0,1,1); // group 1 in cyan
      else          cairo_set_source_rgb(cr,1,1,0); // others in yellow
      cairo_fill(cr);
      if(v) {
        cairo_set_source_rgb(cr,0,0,0);
        gsave(cr);
        cairo_translate(cr,x+w/2+4,y0-h-4);
        cairo_rotate(cr,-PI/2);
        sprintf(s,"%d (%.1f%%)",v,(v*100.0)/u);
        ltext(cr,s,10,0);
        grestore(cr);
        }
      else cairo_set_source_rgb(cr,1,0,0);
      ctext(cr,icctoutf8[c],x+w/2,y0+20,16,0,0);
      x+=HBARW+HBARG;
      }

    }

  }


// redraw histogram
static gint hist_expose_event(GtkWidget*widget,GdkEventExpose*event) {
  cairo_t*cr;
  DEB_RF printf("hist expose event x=%d y=%d w=%d h=%d\n",event->area.x,event->area.y,event->area.width,event->area.height),fflush(stdout);
  if(widget==hist_da) {
    cr=gdk_cairo_create(widget->window);
    cairo_rectangle(cr,event->area.x,event->area.y,event->area.width,event->area.height);
    cairo_clip(cr);
    gscale=1;
    drawhist(cr);
    cairo_destroy(cr);
    }
  return FALSE;
  }

// create statistics dialogue
static void stats_init(void) {
  int i,j;
  GtkWidget*w0,*w1,*nb,*vb0,*vb1;

  for(i=0;i<MXLE+2;i++) for(j=0;j<5;j++) st_te[i][j]=0;
  stats=gtk_dialog_new_with_buttons("Statistics",GTK_WINDOW(mainw),GTK_DIALOG_DESTROY_WITH_PARENT,GTK_STOCK_CLOSE,GTK_RESPONSE_ACCEPT,NULL);
  gtk_window_set_policy(GTK_WINDOW(stats),FALSE,FALSE,TRUE);
  nb=gtk_notebook_new();
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(stats)->vbox),nb,FALSE,TRUE,0);

  vb0=gtk_vbox_new(0,3);
  gtk_notebook_append_page(GTK_NOTEBOOK(nb),vb0,gtk_label_new("General"));
  w0=gtk_table_new(MXLE+2,5,FALSE);
  st_te[0][0]=gtk_label_new("  Length  ");
  st_te[0][1]=gtk_label_new("  Count  ");
  st_te[0][2]=gtk_label_new("  Underchecked  ");
  st_te[0][3]=gtk_label_new("  Overchecked  ");
  st_te[0][4]=gtk_label_new("  Check ratio min:mean:max  ");
  for(j=0;j<5;j++) gtk_widget_show(st_te[0][j]);
  w1=gtk_hseparator_new();gtk_table_attach_defaults(GTK_TABLE(w0),w1,0,5,1,2);gtk_widget_show(w1);
  for(i=2;i<=MXLE;i++) for(j=0;j<5;j++) st_te[i][j]=gtk_label_new(""); // initialise all table entries to empty strings
  for(i=0;i<=MXLE;i++) for(j=0;j<5;j++) if(st_te[i][j]) gtk_table_attach_defaults(GTK_TABLE(w0),st_te[i][j],j,j+1,i,i+1);
  w1=gtk_hseparator_new();gtk_table_attach_defaults(GTK_TABLE(w0),w1,0,5,MXLE+1,MXLE+2);gtk_widget_show(w1);
  gtk_box_pack_start(GTK_BOX(vb0),w0,FALSE,TRUE,0);gtk_widget_show(w0);
  for(j=0;j<10;j++) {
    st_r[j]=gtk_label_new(" "); // blank rows at the bottom for now
    gtk_misc_set_alignment(GTK_MISC(st_r[j]),0,0.5);
    gtk_box_pack_start(GTK_BOX(vb0),st_r[j],FALSE,TRUE,0);
    gtk_widget_show(st_r[j]);
    }

  vb1=gtk_vbox_new(0,3);
  gtk_notebook_append_page(GTK_NOTEBOOK(nb),vb1,gtk_label_new("Entry histogram"));

  // drawing area for histogram and events it captures
  hist_da=gtk_drawing_area_new();
  gtk_drawing_area_size(GTK_DRAWING_AREA(hist_da),niccused*(HBARW+HBARG)+7,250);
  gtk_widget_set_events(hist_da,GDK_EXPOSURE_MASK);
  gtk_signal_connect(GTK_OBJECT(hist_da),"expose_event",GTK_SIGNAL_FUNC(hist_expose_event),NULL);
  gtk_signal_connect(GTK_OBJECT(hist_da),"configure_event",GTK_SIGNAL_FUNC(hist_configure_event),NULL);
  gtk_box_pack_start(GTK_BOX(vb1),hist_da,FALSE,TRUE,0);gtk_widget_show(hist_da);

  gtk_widget_show(hist_da);
  gtk_widget_show(vb1);
  gtk_widget_show(vb0);
  gtk_widget_show(nb);
  }

// remove statistics window (if not already gone or in the process of going)
static void stats_quit(GtkDialog*w,int i0,void*p0) {
  DEB_RF printf("stats_quit()\n"),fflush(stdout);
  if(i0!=GTK_RESPONSE_DELETE_EVENT&&stats!=NULL) gtk_widget_destroy(stats);
  stats=NULL;
  DEB_RF printf("stats_quit()\n"),fflush(stdout);
  }

// open stats window if not already open, and update it
static int statsdia(void) {
  if(!stats) stats_init();
  stats_upd();
  gtk_widget_show(stats);
  gtk_signal_connect(GTK_OBJECT(stats),"response",GTK_SIGNAL_FUNC(stats_quit),NULL); // does repeating this matter?
  return 0;
  }

void setposslabel(char*s) {
  if(!usegui) return;
  gtk_label_set_text(GTK_LABEL(poss_label),s);
  }

// update feasible list to screen
void updatefeas(void) {
  int i;
  char t0[MXFL*10+100],t1[MXLE+50],*u[2],p0[MAXICC+1],p1[MAXICC*16+SLEN];
  DEB_RF printf("updatefeas()\n");
  if(!usegui) return;
  if(ifamode==0) {setposslabel(""); return;}
  u[0]=t0;
  u[1]=t1;
  p1[0]='\0';
  gtk_clist_freeze(GTK_CLIST(clist)); // avoid glitchy-looking updates
  gtk_clist_clear(GTK_CLIST(clist));
  if(!isclear(curx,cury)) goto ew0;
  for(i=0;i<llistn;i++) { // add in list entries
    ansform(t0,sizeof(t0),llistp[i],llistwlen,llistdm);
    DEB_RF { printf("ansform : <"); printICCs(lts[llistp[i]].s); printf("> -> <%s>\n",t0); }
    if(lts[llistp[i]].ans<0) strcpy(t1,"");
    else sprintf(t1,"%+.1f",log10(ansp[lts[llistp[i]].ans]->score)); // negative ans values should not occur here
    DEB_RF printf("gtk_clist_append( <%s> <%s> )\n",u[0],u[1]);
    gtk_clist_append(GTK_CLIST(clist),u);
    DEB_RF {int j; for(j=0;t0[j];j++) printf("<%02X>",(unsigned char)t0[j]); printf("\n");}
    }
  if(curent>=0&&curword>=0&&words[curword].fe==0) {
    if(entries[curent].flbm==0) strcpy(p0,"");
    else getposs(entries+curent,p0,0,0); // get feasible letter list with dash suppressed
    if(strlen(p0)==0) sprintf(p1," No feasible characters");
    else {
      sprintf(p1," Feasible character%s: ",(strlen(p0)==1)?"":"s");
      for(i=0;p0[i];i++) strcat(p1,icctoutf8[(unsigned int)p0[i]]);
      }
  } else p1[0]=0;
ew0:
  gtk_clist_thaw(GTK_CLIST(clist)); // make list visible
  DEB_RF printf("Setting poss label to >%s<\n",p1);
  setposslabel(p1);
  return;
  }

static void syncselmenu() {
  DEB_GU printf("syncselmenu mode=%d,n=%d\n",selmode,nsel);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0xf100),selmode==0&&nsel>0);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0xf201),selmode==2&&nsel==1);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0xf202),selmode==2&&nsel==1);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0xf203),selmode==2&&nsel==1);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0xf204),selmode==2&&nsel>0);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0xf300),selmode==0&&nsel>0);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0xf301),(selmode==1||selmode==2)&&nsel>0);
  }

static void syncsymmmenu() {int i,m;
  gtk_menu_item_activate((GtkMenuItem*)gtk_item_factory_get_widget_by_action(item_factory,0x100+symmr));
  gtk_menu_item_activate((GtkMenuItem*)gtk_item_factory_get_widget_by_action(item_factory,0x200+symmm));
  gtk_menu_item_activate((GtkMenuItem*)gtk_item_factory_get_widget_by_action(item_factory,0x300+symmd));
  m=symmrmask(); for(i=1;i<=12;i++) gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0x100+i),(m>>i)&1);
  m=symmmmask(); for(i=0;i<= 3;i++) gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0x200+i),(m>>i)&1);
  m=symmdmask(); for(i=0;i<= 3;i++) gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0x300+i),(m>>i)&1);
  i=(gshape[gtype]==3||gshape[gtype]==4);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0xf000),!i);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0xf001), i);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0xf002), i);
  i=(gshape[gtype]>0);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0x404),!i);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0x414),!i);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0x433),!i);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0x443),!i);
  gtk_widget_set_sensitive((GtkWidget*)gtk_item_factory_get_widget_by_action(item_factory,0x451),!i);
  }

static void syncifamenu() {
  gtk_menu_item_activate((GtkMenuItem*)gtk_item_factory_get_widget_by_action(item_factory,0x0500+ifamode));
  }

// sync GUI with possibly new grid properties, flags, symmetry, width, height
void syncgui(void) {
  if(!usegui) return;
  if(!isingrid(curx,cury)) {curx=0,cury=0;} // keep cursor in grid
  if(curdir>=ndir[gtype]&&curdir<100) curdir=0; // make sure direction is feasible
  if(curdir>=100+nvl) curdir=0;
  gtk_drawing_area_size(GTK_DRAWING_AREA(grid_da),dawidth()+3,daheight()+3);
  gtk_widget_show(grid_da);
  syncsymmmenu();
  syncselmenu();
  syncifamenu();
  setwintitle();
  draw_init();
  refreshall();
  curmoved();
  }

// menus
static GtkItemFactoryEntry menu_items[] = {
 { "/_File",                                                   0,                   0,              0,        "<Branch>",                                     0                       },
 { "/File/_New",                                               0,                   0,              0,        "<Branch>",                                     0                       },
 { "/File/New/Current shape and size",                         "<control>N",        m_filenew,      0,        "<StockItem>",                                  GTK_STOCK_NEW           },
 { "/File/New/Blank 9x9",                                      0,                   m_filenew,      0x090980, 0,                                              0                       },
 { "/File/New/Blank 10x10",                                    0,                   m_filenew,      0x0a0a80, 0,                                              0                       },
 { "/File/New/Blank 11x11",                                    0,                   m_filenew,      0x0b0b80, 0,                                              0                       },
 { "/File/New/Blank 11x13",                                    0,                   m_filenew,      0x0b0d80, 0,                                              0                       },
 { "/File/New/Blank 12x12",                                    0,                   m_filenew,      0x0c0c80, 0,                                              0                       },
 { "/File/New/Blank 13x11",                                    0,                   m_filenew,      0x0d0b80, 0,                                              0                       },
 { "/File/New/Blank 13x13",                                    0,                   m_filenew,      0x0d0d80, 0,                                              0                       },
 { "/File/New/Blank 14x14",                                    0,                   m_filenew,      0x0e0e80, 0,                                              0                       },
 { "/File/New/Blank 15x15",                                    0,                   m_filenew,      0x0f0f80, 0,                                              0                       },
 { "/File/New/Blocked 13x13 template",                         0,                   0,              0,        "<Branch>",                                     0                       },
 { "/File/New/Blocked 13x13 template/No unches on edges",      0,                   m_filenew,      0x0d0d00, 0,                                              0                       },
 { "/File/New/Blocked 13x13 template/Unches left and right",   0,                   m_filenew,      0x0d0d01, 0,                                              0                       },
 { "/File/New/Blocked 13x13 template/Unches top and bottom",   0,                   m_filenew,      0x0d0d02, 0,                                              0                       },
 { "/File/New/Blocked 13x13 template/Unches on all edges",     0,                   m_filenew,      0x0d0d03, 0,                                              0                       },
 { "/File/New/Blocked 15x15 template",                         0,                   0,              0,        "<Branch>",                                     0                       },
 { "/File/New/Blocked 15x15 template/No unches on edges",      0,                   m_filenew,      0x0f0f00, 0,                                              0                       },
 { "/File/New/Blocked 15x15 template/Unches left and right",   0,                   m_filenew,      0x0f0f01, 0,                                              0                       },
 { "/File/New/Blocked 15x15 template/Unches top and bottom",   0,                   m_filenew,      0x0f0f02, 0,                                              0                       },
 { "/File/New/Blocked 15x15 template/Unches on all edges",     0,                   m_filenew,      0x0f0f03, 0,                                              0                       },
 { "/File/New/Blocked 17x17 template",                         0,                   0,              0,        "<Branch>",                                     0                       },
 { "/File/New/Blocked 17x17 template/No unches on edges",      0,                   m_filenew,      0x111100, 0,                                              0                       },
 { "/File/New/Blocked 17x17 template/Unches left and right",   0,                   m_filenew,      0x111101, 0,                                              0                       },
 { "/File/New/Blocked 17x17 template/Unches top and bottom",   0,                   m_filenew,      0x111102, 0,                                              0                       },
 { "/File/New/Blocked 17x17 template/Unches on all edges",     0,                   m_filenew,      0x111103, 0,                                              0                       },
 { "/File/New/Blocked 19x19 template",                         0,                   0,              0,        "<Branch>",                                     0                       },
 { "/File/New/Blocked 19x19 template/No unches on edges",      0,                   m_filenew,      0x131300, 0,                                              0                       },
 { "/File/New/Blocked 19x19 template/Unches left and right",   0,                   m_filenew,      0x131301, 0,                                              0                       },
 { "/File/New/Blocked 19x19 template/Unches top and bottom",   0,                   m_filenew,      0x131302, 0,                                              0                       },
 { "/File/New/Blocked 19x19 template/Unches on all edges",     0,                   m_filenew,      0x131303, 0,                                              0                       },
 { "/File/New/Blocked 21x21 template",                         0,                   0,              0,        "<Branch>",                                     0                       },
 { "/File/New/Blocked 21x21 template/No unches on edges",      0,                   m_filenew,      0x151500, 0,                                              0                       },
 { "/File/New/Blocked 21x21 template/Unches left and right",   0,                   m_filenew,      0x151501, 0,                                              0                       },
 { "/File/New/Blocked 21x21 template/Unches top and bottom",   0,                   m_filenew,      0x151502, 0,                                              0                       },
 { "/File/New/Blocked 21x21 template/Unches on all edges",     0,                   m_filenew,      0x151503, 0,                                              0                       },
 { "/File/New/Blocked 23x23 template",                         0,                   0,              0,        "<Branch>",                                     0                       },
 { "/File/New/Blocked 23x23 template/No unches on edges",      0,                   m_filenew,      0x171700, 0,                                              0                       },
 { "/File/New/Blocked 23x23 template/Unches left and right",   0,                   m_filenew,      0x171701, 0,                                              0                       },
 { "/File/New/Blocked 23x23 template/Unches top and bottom",   0,                   m_filenew,      0x171702, 0,                                              0                       },
 { "/File/New/Blocked 23x23 template/Unches on all edges",     0,                   m_filenew,      0x171703, 0,                                              0                       },
 { "/File/New/Blocked 25x25 template",                         0,                   0,              0,        "<Branch>",                                     0                       },
 { "/File/New/Blocked 25x25 template/No unches on edges",      0,                   m_filenew,      0x191900, 0,                                              0                       },
 { "/File/New/Blocked 25x25 template/Unches left and right",   0,                   m_filenew,      0x191901, 0,                                              0                       },
 { "/File/New/Blocked 25x25 template/Unches top and bottom",   0,                   m_filenew,      0x191902, 0,                                              0                       },
 { "/File/New/Blocked 25x25 template/Unches on all edges",     0,                   m_filenew,      0x191903, 0,                                              0                       },
 { "/File/sep0",                                               0,                   0,              0,        "<Separator>",                                  0                       },
 { "/File/_Open...",                                           "<control>O",        m_fileopen,     0,        "<StockItem>",                                  GTK_STOCK_OPEN          },
 { "/File/_Save",                                              "<control>S",        m_filesave,     0,        "<StockItem>",                                  GTK_STOCK_SAVE          },
 { "/File/Save _as...",                                        0,                   m_filesaveas,   0,        "<StockItem>",                                  GTK_STOCK_SAVE_AS       },
 { "/File/sep1",                                               0,                   0,              0,        "<Separator>",                                  0                       },
 { "/File/Export _blank grid image",                           0,                   0,              0,        "<Branch>",                                     0                       },
 { "/File/Export blank grid image/as _EPS...",                 0,                   m_fileexport,   0x401,    0,                                              0                       },
 { "/File/Export blank grid image/as _SVG...",                 0,                   m_fileexport,   0x402,    0,                                              0                       },
 { "/File/Export blank grid image/as _PNG...",                 0,                   m_fileexport,   0x403,    0,                                              0                       },
 { "/File/Export blank grid image/as _HTML...",                0,                   m_fileexport,   0x404,    0,                                              0                       },
 { "/File/Export _filled grid image",                          0,                   0,              0,        "<Branch>",                                     0                       },
 { "/File/Export filled grid image/as _EPS...",                0,                   m_fileexport,   0x411,    0,                                              0                       },
 { "/File/Export filled grid image/as _SVG...",                0,                   m_fileexport,   0x412,    0,                                              0                       },
 { "/File/Export filled grid image/as _PNG...",                0,                   m_fileexport,   0x413,    0,                                              0                       },
 { "/File/Export filled grid image/as _HTML...",               0,                   m_fileexport,   0x414,    0,                                              0                       },
 { "/File/Export ans_wers",                                    0,                   0,              0,        "<Branch>",                                     0                       },
 { "/File/Export answers/As _text...",                         0,                   m_fileexport,   0x420,    0,                                              0                       },
 { "/File/Export answers/As _HTML...",                         0,                   m_fileexport,   0x423,    0,                                              0                       },
 { "/File/Export _puzzle",                                     0,                   0,              0,        "<Branch>",                                     0                       },
 { "/File/Export puzzle/As _HTML...",                          0,                   m_fileexport,   0x433,    0,                                              0                       },
 { "/File/Export puzzle/As HTML+_SVG...",                      0,                   m_fileexport,   0x434,    0,                                              0                       },
 { "/File/Export puzzle/As HTML+_PNG...",                      0,                   m_fileexport,   0x435,    0,                                              0                       },
 { "/File/Export so_lution",                                   0,                   0,              0,        "<Branch>",                                     0                       },
 { "/File/Export solution/As _HTML...",                        0,                   m_fileexport,   0x443,    0,                                              0                       },
 { "/File/Export solution/As HTML+_SVG...",                    0,                   m_fileexport,   0x444,    0,                                              0                       },
 { "/File/Export solution/As HTML+_PNG...",                    0,                   m_fileexport,   0x445,    0,                                              0                       },
 { "/File/Export o_ther format",                               0,                   0,              0,        "<Branch>",                                     0                       },
 { "/File/Export other format/_Crossword Compiler XML...",     0,                   m_fileexport,   0x451,    0,                                              0                       },
 { "/File/sep2",                                               0,                   0,              0,        "<Separator>",                                  0                       },
 { "/File/I_mport free light paths",                           0,                   m_importvls,    0,        0,                                              0                       },
 { "/File/E_xport free light paths",                           0,                   m_exportvls,    0,        0,                                              0                       },
 { "/File/sep3",                                               0,                   0,              0,        "<Separator>",                                  0                       },
 { "/File/_Quit",                                              "<control>Q",        m_filequit,     0,        "<StockItem>",                                  GTK_STOCK_QUIT          },
 { "/_Edit",                                                   0,                   0,              0,        "<Branch>",                                     0                       },
 { "/Edit/_Undo",                                              "<control>Z",        m_undo,         0,        "<StockItem>",                                  GTK_STOCK_UNDO          },
 { "/Edit/_Redo",                                              "<control>Y",        m_redo,         0,        "<StockItem>",                                  GTK_STOCK_REDO          },
 { "/Edit/sep1",                                               0,                   0,              0,        "<Separator>",                                  0                       },
 { "/Edit/_Solid block",                                       "Insert",            m_editblock,    0,        0,                                              0                       },
 { "/Edit/_Bar before",                                        "Return",            m_editbarb,     0,        0,                                              0                       },
 { "/Edit/_Empty",                                             "Delete",            m_editempty,    0,        0,                                              0                       },
 { "/Edit/_Cutout",                                            "<control>C",        m_editcutout,   0,        0,                                              0                       },
 { "/Edit/_Merge with next",                                   "<control>M",        m_editmerge,    0,        0,                                              0                       },
 { "/Edit/sep2",                                               0,                   0,              0,        "<Separator>",                                  0                       },
 { "/Edit/Cell c_ontents...",                                  "<control>I",        m_cellcont,     0,        0,                                              0                       },
 { "/Edit/_Light contents...",                                 "<control>L",        m_editlight,    0,        0,                                              0                       },
 { "/Edit/Clear _all cells",                                   "<control>X",        m_eraseall,     0,        "<StockItem>",                                  GTK_STOCK_CLEAR         },
 { "/Edit/C_lear selected cells",                              "<shift><control>X", m_erasesel,     0xf100,   "<StockItem>",                                  GTK_STOCK_CLEAR         },
 { "/Edit/sep3",                                               0,                   0,              0,        "<Separator>",                                  0                       },
//  { "/Edit/sep4",                                            0,                   0,              0,        "<Separator>",                                  0                       },
//  { "/Edit/_Write clue",                                     "<control>W",        m_writeclue,    0,        0,                                              0                       },
//  { "/Edit/sep5",                                            0,                   0,              0,        "<Separator>",                                  0                       },
 { "/Edit/_Free light",                                        0,                   0,              0,        "<Branch>",                                     0                       },
 { "/Edit/Free light/_Start new",                              0,                   m_vlnew,        0xf200,   0,                                              0                       },
 { "/Edit/Free light/_Extend selected",                        "<control>E",        m_vlextend,     0xf201,   0,                                              0                       },
 { "/Edit/Free light/_Shorten selected",                       "<control>D",        m_vlcurtail,    0xf202,   0,                                              0                       },
 { "/Edit/Free light/_Modify selected",                        0,                   m_vlmodify,     0xf203,   0,                                              0                       },
 { "/Edit/Free light/_Delete selected",                        0,                   m_vldelete,     0xf204,   0,                                              0                       },
 { "/Edit/sep6",                                               0,                   0,              0,        "<Separator>",                                  0                       },
 { "/Edit/Flip in main dia_gonal",                             0,                   m_editflip,     0xf000,   0,                                              0                       },
 { "/Edit/Rotate cloc_kwise",                                  "greater",           m_editrot,      0xf001,   0,                                              0                       },
 { "/Edit/Rotate a_nticlockwise",                              "less",              m_editrot,      0xf002,   0,                                              0                       },
 { "/Edit/_Delete",                                            0,                   0,              0,        "<Branch>",                                     0                       },
 { "/Edit/Delete/_Row or annulus",                             0,                   m_delrow,       0,        0,                                              0                       },
 { "/Edit/Delete/_Column or radius",                           0,                   m_delcol,       0,        0,                                              0                       },
 { "/Edit/_Insert",                                            0,                   0,              0,        "<Branch>",                                     0                       },
 { "/Edit/Insert/Row _above or outer annulus",                 0,                   m_insrow,       0,        0,                                              0                       },
 { "/Edit/Insert/Row _below or inner annulus",                 0,                   m_insrow,       1,        0,                                              0                       },
 { "/Edit/Insert/Column to _left or previous radius",          0,                   m_inscol,       0,        0,                                              0                       },
 { "/Edit/Insert/Column to _right or next radius",             0,                   m_inscol,       1,        0,                                              0                       },
 { "/Edit/sep7",                                               0,                   0,              0,        "<Separator>",                                  0                       },
 { "/Edit/_Zoom",                                              0,                   0,              0,        "<Branch>",                                     0                       },
 { "/Edit/Zoom/_Out",                                          "<control>minus",    m_zoom,         -2,       0,                                              0                       },
 { "/Edit/Zoom/_1 50%",                                        "<control>8",        m_zoom,         0,        0,                                              0                       },
 { "/Edit/Zoom/_2 71%",                                        "<control>9",        m_zoom,         1,        0,                                              0                       },
 { "/Edit/Zoom/_3 100%",                                       "<control>0",        m_zoom,         2,        0,                                              0                       },
 { "/Edit/Zoom/_4 141%",                                       "<control>1",        m_zoom,         3,        0,                                              0                       },
 { "/Edit/Zoom/_5 200%",                                       "<control>2",        m_zoom,         4,        0,                                              0                       },
 { "/Edit/Zoom/_In",                                           "<control>plus",     m_zoom,         -1,       0,                                              0                       },
 { "/Edit/Show s_tatistics",                                   0,                   m_showstats,    0,        0,                                              0                       },
 { "/Edit/_Preferences...",                                    0,                   m_editprefs,    0,        "<StockItem>",                                  GTK_STOCK_PREFERENCES   },
 { "/_Properties",                                             0,                   0,              0,        "<Branch>",                                     0                       },
 { "/Properties/_Grid properties...",                          0,                   m_editgprop,    0,        "<StockItem>",                                  GTK_STOCK_PROPERTIES    },
 { "/Properties/Default _cell properties...",                  0,                   m_dsprop,       0,        0,                                              0                       },
 { "/Properties/Selected c_ell properties...",                 0,                   m_sprop,        0xf300,   0,                                              0                       },
 { "/Properties/Default _light properties...",                 0,                   m_dlprop,       0,        0,                                              0                       },
 { "/Properties/Selected l_ight properties...",                0,                   m_lprop,        0xf301,   0,                                              0                       },
 { "/_Select",                                                 0,                   0,              0,        "<Branch>",                                     0                       },
 { "/Select/Current _cell",                                    "<shift>C",          m_selcell,      0,        0,                                              0                       },
 { "/Select/Current _light",                                   "<shift>L",          m_sellight,     0,        0,                                              0                       },
 { "/Select/Cell _mode <> light mode",                         "<shift>M",          m_selmode,      0,        0,                                              0                       },
 { "/Select/_Free light",                                      "<shift>F",          m_selfvl,       0,        0,                                              0                       },
 { "/Select/sep0",                                             0,                   0,              0,        "<Separator>",                                  0                       },
 { "/Select/_All",                                             "<shift>A",          m_selall,       0,        0,                                              0                       },
 { "/Select/_Invert",                                          "<shift>I",          m_selinv,       0,        0,                                              0                       },
 { "/Select/_Nothing",                                         "<shift>N",          m_selnone,      0,        0,                                              0                       },
 { "/Select/sep1",                                             0,                   0,              0,        "<Separator>",                                  0                       },
 { "/Select/Cell_s",                                           0,                   0,              0,        "<Branch>",                                     0                       },
 { "/Select/Cells/overriding default _properties",             0,                   m_selcover,     0,        0,                                              0                       },
 { "/Select/Cells/flagged for _answer treatment",              0,                   m_selctreat,    0,        0,                                              0                       },
 { "/Select/Cells/that are _unchecked",                        0,                   m_selcunch,     0,        0,                                              0                       },
 { "/Select/Li_ghts",                                          0,                   0,              0,        "<Branch>",                                     0                       },
 { "/Select/Lights/_in current direction",                     0,                   m_sellpar,      0,        0,                                              0                       },
 { "/Select/Lights/overriding default _properties",            0,                   m_sellover,     0,        0,                                              0                       },
 { "/Select/Lights/with answer treatment _enabled",            0,                   m_selltreat,    0,        0,                                              0                       },
 { "/Select/Lights/with _double or more unches",               0,                   m_selviol,      1,        0,                                              0                       },
 { "/Select/Lights/with _triple or more unches",               0,                   m_selviol,      2,        0,                                              0                       },
 { "/Select/Lights/that are _underchecked",                    0,                   m_selviol,      4,        0,                                              0                       },
 { "/Select/Lights/that are _overchecked",                     0,                   m_selviol,      8,        0,                                              0                       },
 { "/Sy_mmetry",                                               0,                   0,              0,        "<Branch>",                                     0                       },
 { "/Symmetry/_None",                                          0,                   m_symmclr,      0,        0,                                              0                       },
 { "/Symmetry/sep1",                                           0,                   0,              0,        "<Separator>",                                  0                       },
 { "/Symmetry/N_o rotational",                                 0,                   m_symm0,        0x0101,   "<RadioItem>",                                  0                       },
 { "/Symmetry/_Twofold rotational",                            0,                   m_symm0,        0x0102,   "/Symmetry/No rotational"        ,              0                       },
 { "/Symmetry/Threefold rotational",                           0,                   m_symm0,        0x0103,   "/Symmetry/Twofold rotational"   ,              0                       },
 { "/Symmetry/_Fourfold rotational",                           0,                   m_symm0,        0x0104,   "/Symmetry/Threefold rotational" ,              0                       },
 { "/Symmetry/Fivefold rotational",                            0,                   m_symm0,        0x0105,   "/Symmetry/Fourfold rotational"  ,              0                       },
 { "/Symmetry/Sixfold rotational",                             0,                   m_symm0,        0x0106,   "/Symmetry/Fivefold rotational"  ,              0                       },
 { "/Symmetry/Sevenfold rotational",                           0,                   m_symm0,        0x0107,   "/Symmetry/Sixfold rotational"   ,              0                       },
 { "/Symmetry/Eightfold rotational",                           0,                   m_symm0,        0x0108,   "/Symmetry/Sevenfold rotational" ,              0                       },
 { "/Symmetry/Ninefold rotational",                            0,                   m_symm0,        0x0109,   "/Symmetry/Eightfold rotational" ,              0                       },
 { "/Symmetry/Tenfold rotational",                             0,                   m_symm0,        0x010a,   "/Symmetry/Ninefold rotational"  ,              0                       },
 { "/Symmetry/Elevenfold rotational",                          0,                   m_symm0,        0x010b,   "/Symmetry/Tenfold rotational"   ,              0                       },
 { "/Symmetry/Twelvefold rotational",                          0,                   m_symm0,        0x010c,   "/Symmetry/Elevenfold rotational",              0                       },
 { "/Symmetry/sep2",                                           0,                   0,              0,        "<Separator>",                                  0                       },
 { "/Symmetry/No mirror",                                      0,                   m_symm1,        0x0200,   "<RadioItem>",                                  0                       },
 { "/Symmetry/Left-right mirror",                              0,                   m_symm1,        0x0201,   "/Symmetry/No mirror",                          0                       },
 { "/Symmetry/Up-down mirror",                                 0,                   m_symm1,        0x0202,   "/Symmetry/Left-right mirror",                  0                       },
 { "/Symmetry/Both",                                           0,                   m_symm1,        0x0203,   "/Symmetry/Up-down mirror",                     0                       },
 { "/Symmetry/sep3",                                           0,                   0,              0,        "<Separator>",                                  0                       },
 { "/Symmetry/No duplication",                                 0,                   m_symm2,        0x0300,   "<RadioItem>",                                  0                       },
 { "/Symmetry/Left-right duplication",                         0,                   m_symm2,        0x0301,   "/Symmetry/No duplication",                     0                       },
 { "/Symmetry/Up-down duplication",                            0,                   m_symm2,        0x0302,   "/Symmetry/Left-right duplication",             0                       },
 { "/Symmetry/Both",                                           0,                   m_symm2,        0x0303,   "/Symmetry/Up-down duplication",                0                       },
 { "/_Autofill",                                               0,                   0,              0,        "<Branch>",                                     0                       },
 { "/Autofill/_Dictionaries...",                               0,                   m_dictionaries, 0,        0,                                              0                       },
 { "/Autofill/Anal_yse dictionaries...",                       0,                   m_dstats,       0,        0,                                              0                       },
 { "/Autofill/_Alphabet...",                                   0,                   m_alphabet,     0,        0,                                              0                       },
 { "/Autofill/Answer _treatment...",                           0,                   m_afctreat,     0,        0,                                              0                       },
 { "/Autofill/sep1",                                           0,                   0,              0,        "<Separator>",                                  0                       },
 { "/Autofill/Auto_fill",                                      "<control>G",        m_autofill,     1,        "<StockItem>",                                  GTK_STOCK_EXECUTE       },
 { "/Autofill/Autofill _selected cells",                       "<shift><control>G", m_autofill,     2,        "<StockItem>",                                  GTK_STOCK_EXECUTE       },
 { "/Autofill/sep2",                                           0,                   0,              0,        "<Separator>",                                  0                       },
 { "/Autofill/_Interactive assistance",                        0,                   0,              0,        "<Branch>",                                     0                       },
 { "/Autofill/Interactive assistance/_Off",                    0,                   m_ifamode,      0x0500,   "<RadioItem>",                                  0                       },
 { "/Autofill/Interactive assistance/_Light only",             0,                   m_ifamode,      0x0501,   "/Autofill/Interactive assistance/Off",         0                       },
 { "/Autofill/Interactive assistance/_Entire grid",            0,                   m_ifamode,      0x0502,   "/Autofill/Interactive assistance/Light only",  0                       },
 { "/Autofill/Accept _hints",                                  "<control>A",        m_accept,       0,        0,                                              0                       },
 { "/Autofill/sep2",                                           0,                   0,              0,        "<Separator>",                                  0                       },
 { "/Autofill/_Unban all answers",                             0,                   m_unban,        0,        0,                                              0                       },
 { "/_Help",                                                   0,                   0,              0,        "<LastBranch>",                                 0                       },
 { "/Help/_About",                                             0,                   m_helpabout,    0,        "<StockItem>",                                  GTK_STOCK_ABOUT         },
};

// build main window and other initialisation
void startgtk(void) {
  GtkAccelGroup*accel_group;
  GtkWidget *vbox,*hbox,*menubar,*zmin,*zmout; // main window and content
  int w,h;

  pxsq=zoompx[zoomf];

  mainw=gtk_window_new(GTK_WINDOW_TOPLEVEL); // main window
  gtk_widget_set_name(mainw,"Qxw");
  w=gdk_screen_get_width (gdk_display_get_default_screen(gdk_display_get_default()));
  h=gdk_screen_get_height(gdk_display_get_default_screen(gdk_display_get_default()));
  if(h<300||h>MAINWHEIGHT) h=MAINWHEIGHT;
  if(w<400||w>MAINWWIDTH ) w=MAINWWIDTH ;
  gtk_window_set_default_size(GTK_WINDOW(mainw),w,h);
  gtk_window_set_title(GTK_WINDOW(mainw),"Qxw");
  gtk_window_set_position(GTK_WINDOW(mainw),GTK_WIN_POS_CENTER);

  // box in the window
  vbox=gtk_vbox_new(FALSE,0);
  gtk_container_add(GTK_CONTAINER(mainw),vbox);

  // menu in the vbox
  accel_group=gtk_accel_group_new();
  item_factory=gtk_item_factory_new(GTK_TYPE_MENU_BAR,"<main>",accel_group);
  gtk_item_factory_create_items(item_factory,sizeof(menu_items)/sizeof(menu_items[0]),menu_items,NULL);
  gtk_window_add_accel_group(GTK_WINDOW(mainw),accel_group);
  menubar=gtk_item_factory_get_widget(item_factory,"<main>");
  gtk_box_pack_start(GTK_BOX(vbox),menubar,FALSE,TRUE,0);

  // window is divided into two parts, or `panes'
  paned=gtk_hpaned_new();
  gtk_box_pack_start(GTK_BOX(vbox),paned,TRUE,TRUE,0);

  // scrolled windows in the panes
  grid_sw=gtk_scrolled_window_new(NULL,NULL);
  gtk_container_set_border_width(GTK_CONTAINER(grid_sw),10);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(grid_sw),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_paned_pack1(GTK_PANED(paned),grid_sw,1,1);

  list_sw=gtk_scrolled_window_new(NULL,NULL);
  gtk_container_set_border_width(GTK_CONTAINER(list_sw),10);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(list_sw),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);
  gtk_paned_pack2(GTK_PANED(paned),list_sw,0,1);
  gtk_paned_set_position(GTK_PANED(paned),MAINWWIDTH-FLISTWIDTH);

  // drawing area for grid and events it captures
  grid_da=gtk_drawing_area_new();
  gtk_drawing_area_size(GTK_DRAWING_AREA(grid_da),100,100);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(grid_sw),grid_da);
  GTK_WIDGET_SET_FLAGS(grid_da,GTK_CAN_FOCUS);
  gtk_widget_set_events(grid_da,GDK_EXPOSURE_MASK|GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_KEY_PRESS_MASK|GDK_POINTER_MOTION_MASK|GDK_SCROLL_MASK);

  // list of feasible words
//  clist=gtk_clist_new(2);
  clist=gtk_clist_new(1);
  gtk_clist_set_column_width(GTK_CLIST(clist),0,FLISTWIDTH-38);
//   gtk_clist_set_column_width(GTK_CLIST(clist),1,80);
  gtk_clist_set_column_title(GTK_CLIST(clist),0,"Feasible words");
//  gtk_clist_set_column_title(GTK_CLIST(clist),1,"Scores");
  gtk_clist_column_titles_passive(GTK_CLIST(clist));
  gtk_clist_column_titles_show(GTK_CLIST(clist));
  gtk_clist_set_selection_mode(GTK_CLIST(clist),GTK_SELECTION_SINGLE);
  gtk_container_add(GTK_CONTAINER(list_sw),clist);

  g_signal_connect(clist,"button-press-event",GTK_SIGNAL_FUNC(clist_button),NULL);

  // box for widgets across the bottom of the window
  hbox=gtk_hbox_new(FALSE,0);
  zmout=gtk_button_new();  gtk_button_set_image(GTK_BUTTON(zmout),gtk_image_new_from_stock(GTK_STOCK_ZOOM_OUT,GTK_ICON_SIZE_MENU)); gtk_box_pack_start(GTK_BOX(hbox),zmout,FALSE,FALSE,0);
  gtk_signal_connect(GTK_OBJECT(zmout),"clicked",GTK_SIGNAL_FUNC(m_zoom),(gpointer)-2);
  zmin =gtk_button_new();  gtk_button_set_image(GTK_BUTTON(zmin ),gtk_image_new_from_stock(GTK_STOCK_ZOOM_IN ,GTK_ICON_SIZE_MENU)); gtk_box_pack_start(GTK_BOX(hbox),zmin ,FALSE,FALSE,0);
  gtk_signal_connect(GTK_OBJECT(zmin ),"clicked",GTK_SIGNAL_FUNC(m_zoom),(gpointer)-1);
  poss_label=gtk_label_new(" Feasible characters:");
  gtk_box_pack_start(GTK_BOX(hbox),poss_label,FALSE,FALSE,0);
  gtk_box_pack_end(GTK_BOX(vbox),hbox,FALSE,FALSE,0);

  gtk_signal_connect(GTK_OBJECT(grid_da),"expose_event",GTK_SIGNAL_FUNC(expose_event),NULL);
  gtk_signal_connect(GTK_OBJECT(grid_da),"configure_event",GTK_SIGNAL_FUNC(configure_event),NULL);
  gtk_signal_connect(GTK_OBJECT(clist),"select_row",GTK_SIGNAL_FUNC(selrow),NULL);

  gtk_signal_connect(GTK_OBJECT(grid_da),"button_press_event",GTK_SIGNAL_FUNC(button_press_event),NULL);
  gtk_signal_connect(GTK_OBJECT(grid_da),"button_release_event",GTK_SIGNAL_FUNC(button_press_event),NULL);
  gtk_signal_connect_after(GTK_OBJECT(grid_da),"key_press_event",GTK_SIGNAL_FUNC(keypress),NULL);
  gtk_signal_connect(GTK_OBJECT(grid_da),"motion_notify_event",GTK_SIGNAL_FUNC(mousemove),NULL);
  gtk_signal_connect(GTK_OBJECT(grid_da),"scroll-event",GTK_SIGNAL_FUNC(mousescroll),NULL);

  gtk_signal_connect(GTK_OBJECT(mainw),"delete_event",GTK_SIGNAL_FUNC(w_delete),NULL);
  gtk_signal_connect(GTK_OBJECT(mainw),"destroy",GTK_SIGNAL_FUNC(w_destroy),NULL);

  gtk_widget_show_all(mainw);
  gtk_window_set_focus(GTK_WINDOW(mainw),grid_da);
  }

void stopgtk(void) {
  }


