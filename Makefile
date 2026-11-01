CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic
PROGRAM := compression-demo
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LDLIBS ?= -lm
DESTDIR ?=

SOURCES := \
	src/c/main/main.c \
	src/c/fileIO/filePipeline.c \
	src/c/fileIO/fileInput/fileInput.c \
	src/c/fileIO/fileOutput/fileOutput.c \
	src/c/compression/compression.c \
	src/c/compression/pdf.c \
	src/c/compression/codec.c \
	src/c/compression/jpeg.c \
	src/c/compression/vector.c \
	src/c/compression/media.c \
	src/c/encryption/encryption.c

HEADERS := \
	src/c/compression/compression.h \
	src/c/compression/codec.h \
	src/c/compression/media.h \
	src/c/fileIO/filePipeline.h \
	src/c/fileIO/fileInput/fileInput.h \
	src/c/fileIO/fileOutput/fileOutput.h \
	src/c/encryption/encryption.h

.PHONY: all run debug release install uninstall clean

all: $(PROGRAM)

$(PROGRAM): $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

run: $(PROGRAM)
	./$(PROGRAM)

debug:
	$(CC) $(CPPFLAGS) -DDEBUG $(CFLAGS) -g -O0 -o $(PROGRAM) $(SOURCES) $(LDLIBS)

release:
	$(CC) $(CPPFLAGS) $(CFLAGS) -O2 -o $(PROGRAM) $(SOURCES) $(LDLIBS)

install: $(PROGRAM)
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 "$(PROGRAM)" "$(DESTDIR)$(BINDIR)/$(PROGRAM)"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/$(PROGRAM)"

clean:
	rm -f $(PROGRAM) *.plist
