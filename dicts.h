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




#ifndef __DICTS_H__
#define __DICTS_H__

#define MAXNDICTS 9 // must fit in a word
extern char dfnames[MAXNDICTS][SLEN];
extern char dsfilters[MAXNDICTS][SLEN];
extern char dafilters[MAXNDICTS][SLEN];

extern int dst_lines[MAXNDICTS];
extern int dst_lines_f[MAXNDICTS];
extern int dst_lines_fa[MAXNDICTS];
extern int dst_u_rejline[MAXNDICTS][MAXUCHAR];
extern int dst_u_rejline_count[MAXNDICTS][MAXUCHAR];

extern char lemdesc[NLEM][LEMDESCLEN];
extern char*lemdescADVP[NLEM];

extern int atotal;                // total answers
extern int ltotal;                // total lights
extern int ultotal;               // total uniquified lights
extern unsigned int dusedmask;    // set of dictionaries used

extern void initalphamap(struct alphaentry*a);
extern int initalphamapbycode(char*s);
extern void clearalphamap();
extern int addalphamapentry(int icc,char*rep,char*equiv,int vow,int con,int seq);
extern int isdisallowed(uchar uc);
extern int loaddicts(int sil);
extern void freedicts(void);
extern int loaddefdicts(void);
extern int iswordindm(const char*s,int dm);

extern int ucharslen(uchar*s);
extern int utf8touchars(uchar*ucs,const char*s,int l);
extern char*uchartoutf8(char*q,uchar c);
extern char*ucharstoutf8(char*q,uchar*s);
extern int q_mblen(char*p);
extern int abmtoicc(ABM b);
extern char uchartoICC(int c); // convert UTF-32 to internal character code
extern uchar ICCtouchar(char c); // convert internal character code to UTF-32

extern void printU(uchar c);
extern void printUs(const uchar*s);
extern void printICC(char c);
extern void printICCs(const char*s);

extern uchar icctouchar[MAXICC+1]; // convert internal character code to UTF-32 of canonical representative
extern char icctoutf8[MAXICC+1][16]; // convert internal character code to UTF-8 string of canonical representative, 0-terminated
extern char iccequivs[MAXICC][MAXEQUIV*8+1];
extern char iccseq[MAXICC+1];

#define MAXICCGROUP 3
extern int niccused;
extern char iccused[MAXICC+1];
extern int icctousedindex[MAXICC+1];
extern int iccusedtogroup[MAXICC+1];
extern int icctogroup[MAXICC+1];
extern int iccgroupstart[MAXICCGROUP+1];

extern ABM abm_vow,abm_con,abm_use;

#endif
