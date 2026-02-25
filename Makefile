.POSIX:

VERSION = 1.0
TARGET = box
MANPAGE = $(TARGET).1
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1
PKG_CONFIG = pkg-config

PKGS = libseccomp libcap
LIBS != $(PKG_CONFIG) --libs $(PKGS)
INCS != $(PKG_CONFIG) --cflags $(PKGS)
CFLAGS += -std=c99 -pedantic -Wall $(INCS)

.c.o:
	$(CC) -o $@ $(CFLAGS) $(LIBS) -c $<

$(TARGET): $(TARGET).o
	$(CC) -o $@ $(LIBS) $(TARGET).o

dist:
	mkdir -p $(TARGET)-$(VERSION)
	cp -R README.md $(MANPAGE) $(TARGET) $(TARGET)-$(VERSION)
	tar -czf $(TARGET)-$(VERSION).tar.gz $(TARGET)-$(VERSION)
	$(RM) -r $(TARGET)-$(VERSION)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)
	cp -p $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	chmod 755 $(DESTDIR)$(BINDIR)/$(TARGET)
	cp -p $(MANPAGE) $(DESTDIR)$(MANDIR)/$(MANPAGE)
	chmod 644 $(DESTDIR)$(MANDIR)/$(MANPAGE)

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/$(TARGET)
	$(RM) $(DESTDIR)$(MANDIR)/$(MANPAGE)

clean:
	$(RM) $(TARGET) *.o

all: $(TARGET)

.PHONY: all dist install uninstall clean
