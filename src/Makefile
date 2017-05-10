CC        = gcc
CFLAGS    = -g -O2 -Wall
XROOT     = /usr/X11R6
INCLUDES  = -I$(XROOT)/include
LIBS      = -lX11 -lpython2.7
LDPATH    = -L$(XROOT)/lib

PROG      = xpybind

# Next two lines were edited/added for Debian packaging
DESTDIR	  =
PREFIX    = $(DESTDIR)/usr
RCFILE    = .$(PROG)rc
DEFINES   = -DRCFILE=\"$(RCFILE)\" #-DDEBUG

MOREFILES = xpybind.py
OBJS      = $(PROG).o  

all: $(PROG) 

$(PROG): $(OBJS)
	$(CC) $(OBJS) $(LDPATH) $(LIBS) -o $@

$(OBJS): %.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

install: all
	install $(PROG) $(PREFIX)/bin
	install $(MOREFILES) $(DESTDIR)/usr/lib/python2.5/site-packages

clean:
	rm -f $(PROG) $(OBJS)
