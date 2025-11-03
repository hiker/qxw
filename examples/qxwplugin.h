/*
Qxw is a program to help construct and publish crosswords.

Copyright 2011-2019 Mark Owen; Windows port by Peter Flippant
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
#include <ctype.h>
#include <string.h>

typedef unsigned int uchar; 
#define ICC_DASH 61   // internal character code for `-'
#define MXSZ 63       // maximum grid dimension
#define MXLE 250      // maximum entries in light

extern int treatedanswer(const char*light);
extern int treatedanswerU(const uchar*light);
extern int treatedanswerICC(const char*light);
extern int isword(const char*light);
extern int iswordU(const uchar*light);
extern int iswordICC(const char*light);
extern int ICCtoclass(char c);
extern char uchartoICC(int c);
extern uchar ICCtouchar(char c);

extern void printU(uchar c);
extern void printUs(const uchar*s);
extern void printICC(char c);
extern void printICCs(const char*s);

#ifdef _WIN32

  __declspec(dllexport) void init();
  __declspec(dllexport) void finit();
  __declspec(dllexport) int treat(const char*answer);

  __declspec(dllimport) int clueorderindex;
  __declspec(dllimport) int gridorderindex[];
  __declspec(dllimport) int checking[];
  __declspec(dllimport) int lightlength;
  __declspec(dllimport) int lightx;
  __declspec(dllimport) int lighty;
  __declspec(dllimport) int lightdir;

  __declspec(dllimport) char *treatmessage[];
  __declspec(dllimport) uchar*treatmessageU[];
  __declspec(dllimport) char *treatmessageICC[];
  __declspec(dllimport) char *treatmessageICCAZ[];
  __declspec(dllimport) char *treatmessageICCAZ09[];
  __declspec(dllimport) uchar*treatmessageUAZ[];
  __declspec(dllimport) uchar*treatmessageUAZ09[];
  __declspec(dllimport) char *treatmessageAZ[];
  __declspec(dllimport) char *treatmessageAZ09[];

  __declspec(dllimport) char  msgchar[];
  __declspec(dllimport) uchar msgcharU[];
  __declspec(dllimport) uchar msgcharUAZ[];
  __declspec(dllimport) uchar msgcharUAZ09[];
  __declspec(dllimport) char  msgcharICC[];
  __declspec(dllimport) char  msgcharICCAZ[];
  __declspec(dllimport) char  msgcharICCAZ09[];
  __declspec(dllimport) char  msgcharAZ[];
  __declspec(dllimport) char  msgcharAZ09[];

  __declspec(dllimport) char *answerICC;
  __declspec(dllimport) uchar*answerU;

#else

  extern int clueorderindex;
  extern int*gridorderindex;
  extern int*checking;
  extern int lightlength;
  extern int lightx;
  extern int lighty;
  extern int lightdir;

  extern char *treatmessage[];
  extern uchar*treatmessageU[];
  extern char *treatmessageICC[];
  extern char* treatmessageICCAZ[];
  extern char* treatmessageICCAZ09[];
  extern uchar*treatmessageUAZ[];
  extern uchar*treatmessageUAZ09[];
  extern char *treatmessageAZ[];
  extern char *treatmessageAZ09[];

  extern char  msgchar[];
  extern uchar msgcharU[];
  extern uchar msgcharUAZ[];
  extern uchar msgcharUAZ09[];
  extern char  msgcharICC[];
  extern char  msgcharICCAZ[];
  extern char  msgcharICCAZ09[];
  extern char  msgcharAZ[];
  extern char  msgcharAZ09[];

  extern const char *answerICC;
  extern const uchar*answerU;

#endif

static char  light [MXLE+1];
static uchar lightU[MXLE+1];
