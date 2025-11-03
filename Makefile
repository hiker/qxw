## REL+

# Copyright 2011-2025 Mark Owen; Windows port by Peter Flippant
# http://www.quinapalus.com
# E-mail: qxw@quinapalus.com
#
# This file is part of Qxw.
#
# Qxw is free software: you can redistribute it and/or modify
# it under the terms of version 2 of the GNU General Public License
# as published by the Free Software Foundation.
#
# Qxw is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Qxw.  If not, see <http://www.gnu.org/licenses/> or
# write to the Free Software Foundation, Inc., 51 Franklin Street,
# Fifth Floor, Boston, MA  02110-1301, USA.


all:: qxw
deb:: qxw

PKG_CONFIG ?= pkg-config

# According to https://github.com/klochner/Qxw the following flags
# "Fixed mac build".
## CFLAGS := -Wall -fstack-protector --param=ssp-buffer-size=4 -Wformat -Wformat-security -Werror=format-security -Wno-deprecated-declarations `$(PKG_CONFIG) --cflags glib-2.0` `$(PKG_CONFIG) --cflags gtk+-2.0` -Wpedantic -Wextra -Wno-unused-parameter
## CFLAGS := -Wall -fstack-protector --param=ssp-buffer-size=4 -Wformat -Wformat-security -Werror=format-security `$(PKG_CONFIG) --cflags glib-2.0` `$(PKG_CONFIG) --cflags gtk+-2.0` -I/usr/local/include
## LFLAGS := -L/usr/local/lib `$(PKG_CONFIG) --libs glib-2.0` `$(PKG_CONFIG) --libs gtk+-2.0` -lm -ldl -lpcre -pthread -lgthread-2.0

CFLAGS := -Wall -fstack-protector --param=ssp-buffer-size=4 -Wformat -Wformat-security -Werror=format-security -Wno-deprecated-declarations `$(PKG_CONFIG) --cflags glib-2.0` `$(PKG_CONFIG) --cflags gtk+-2.0` -I/opt/local/include `dpkg-buildflags --get CFLAGS` `dpkg-buildflags --get CPPFLAGS` -Wpedantic -Wextra -Wno-unused-parameter
# CFLAGS := -Wall -fstack-protector --param=ssp-buffer-size=4 -Wformat -Wformat-security -Werror=format-security `$(PKG_CONFIG) --cflags glib-2.0` `$(PKG_CONFIG) --cflags gtk+-2.0` -I/opt/local/include
LFLAGS := -Wl,-Bsymbolic-functions -Wl,-z,relro -L/opt/local/lib `$(PKG_CONFIG) --libs glib-2.0` `$(PKG_CONFIG) --libs gtk+-2.0` -lm -ldl -lpcre -pthread -lgthread-2.0 `dpkg-buildflags --get LDFLAGS`
# -lrt as well?
ifneq ($(filter deb,$(MAKECMDGOALS)),)
  CFLAGS:= $(CFLAGS) -g
else
  CFLAGS:= $(CFLAGS) -g -O3
endif

qxw: qxw.o filler.o treatment.o dicts.o gui.o draw.o alphabets.o deck.o Makefile
	$(CC) -rdynamic -Wall qxw.o filler.o treatment.o dicts.o gui.o draw.o alphabets.o deck.o $(LFLAGS) -o qxw

qxw.o: qxw.c common.h qxw.h filler.h dicts.h treatment.h gui.h draw.h deck.h alphabets.h Makefile
	$(CC) $(CFLAGS) -c qxw.c -o qxw.o

gui.o: gui.c common.h qxw.h filler.h dicts.h treatment.h gui.h draw.h alphabets.h Makefile
	$(CC) $(CFLAGS) -c gui.c -o gui.o

filler.o: filler.c common.h filler.h treatment.h qxw.h gui.h dicts.h Makefile
	$(CC) $(CFLAGS) -c filler.c -o filler.o

treatment.o: treatment.c common.h qxw.h dicts.h treatment.h gui.h Makefile
	$(CC) $(CFLAGS) -fno-strict-aliasing -c treatment.c -o treatment.o

dicts.o: dicts.c common.h qxw.h gui.h dicts.h alphabets.h Makefile
	$(CC) $(CFLAGS) -fno-strict-aliasing -c dicts.c -o dicts.o

draw.o: draw.c common.h qxw.h draw.h gui.h dicts.h Makefile
	$(CC) $(CFLAGS) -c draw.c -o draw.o

deck.o: deck.c common.h qxw.h filler.h alphabets.h dicts.h draw.h treatment.h deck.h Makefile
	$(CC) $(CFLAGS) -c deck.c -o deck.o

alphabets.o: alphabets.c common.h alphabets.h Makefile
	$(CC) $(CFLAGS) -c alphabets.c -o alphabets.o

.PHONY: clean
clean:
	rm -f treatment.o dicts.o draw.o filler.o gui.o qxw.o alphabets.o deck.o qxw

.PHONY: install
install:
	mkdir -p $(DESTDIR)/usr/games
	cp -a qxw $(DESTDIR)/usr/games/qxw
	mkdir -p $(DESTDIR)/usr/include/qxw
	cp -a qxwplugin.h $(DESTDIR)/usr/include/qxw/qxwplugin.h
	mkdir -p $(DESTDIR)/usr/share/applications
	cp -a qxw.desktop $(DESTDIR)/usr/share/applications/qxw.desktop
	mkdir -p $(DESTDIR)/usr/share/pixmaps
	cp -a qxw.xpm $(DESTDIR)/usr/share/pixmaps/qxw.xpm
	mkdir -p $(DESTDIR)/usr/share/icons/hicolor/48x48/apps
	cp -a icon-48x48.png $(DESTDIR)/usr/share/icons/hicolor/48x48/apps/qxw.png
	mkdir -p $(DESTDIR)/usr/share/icons/hicolor/64x64/apps
	cp -a icon-64x64.png $(DESTDIR)/usr/share/icons/hicolor/64x64/apps/qxw.png
	mkdir -p $(DESTDIR)/usr/share/icons/hicolor/96x96/apps
	cp -a icon-96x96.png $(DESTDIR)/usr/share/icons/hicolor/96x96/apps/qxw.png
	mkdir -p $(DESTDIR)/usr/share/icons/hicolor/128x128/apps
	cp -a icon-128x128.png $(DESTDIR)/usr/share/icons/hicolor/128x128/apps/qxw.png
	mkdir -p $(DESTDIR)/usr/share/icons/hicolor/256x256/apps
	cp -a icon-256x256.png $(DESTDIR)/usr/share/icons/hicolor/256x256/apps/qxw.png
#	mkdir -p $(DESTDIR)/usr/share/  /usr/share/lintian/overrides
#	cp -a lintian-binary-overrides $(DESTDIR)/usr/share/lintian/overrides/qxw
#	mkdir -p $(DESTDIR)/usr/share/menu
#	cp -a menufile $(DESTDIR)/usr/share/menu/qxw

## REL-
