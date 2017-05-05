VERSION=1.0.0

# We need a parsed version number
xx_ver_xx=$(subst ., ,$(VERSION))
VER_MAJOR=$(word 1,$(xx_ver_xx))
VER_MINOR=$(word 2,$(xx_ver_xx))
VER_PATCH=$(word 3,$(xx_ver_xx))
VER_MAJMIN=$(VER_MAJOR).$(VER_MINOR)

BIN=porter
LIB_BASE=libporter.so
LIB=$(LIB_BASE).$(VERSION)
SONAME=$(LIB_BASE).$(VER_MAJOR)

CC=gcc
CFLAGS=-Wall -O2
INCLUDES=-I.

ifeq ($(DESTDIR),)
DESTDIR=/
endif

ifeq ($(prefix),)
prefix=/usr
endif

bindir=$(DESTDIR)/$(prefix)/bin/
libdir=$(DESTDIR)/$(prefix)/lib/
incdir=$(DESTDIR)/$(prefix)/include/


all:	$(LIB) $(BIN)

$(LIB):	porter.c
	$(CC) $(CFLAGS) $(INCLUDES) -fPIC -o porter.o -c porter.c
	$(CC) -shared -Wl,-soname,$(SONAME) -o $(LIB) porter.o
	ln -s $(LIB) $(LIB_BASE)
	ln -s $(LIB) $(SONAME)

$(BIN):	main.c
	$(CC) $(CFLAGS) $(INCLUDES) -L. -o $(BIN) main.c -lporter

install:	$(BIN) $(LIB)
	if [ ! -d $(bindir) ]; then mkdir -p $(bindir); fi
	cp $(BIN) $(DESTDIR)/$(prefix)/bin/
	if [ ! -d $(libdir) ]; then mkdir -p $(libdir); fi
	cp -a $(LIB) $(LIB_BASE) $(SONAME) $(libdir)
	if [ ! -d $(incdir) ]; then mkdir -p $(incdir); fi
	cp porter.h $(incdir)/

clean:	
	rm -f $(BIN)
	rm -f $(LIB) $(SONAME) $(LIB_BASE)

.PHONY:	all clean install
