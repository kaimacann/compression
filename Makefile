CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic
PROGRAM := compression-demo
LDLIBS ?= -lm

SOURCES := \
	src/c/main/main.c \
	src/c/compression/compression.c \
	src/c/compression/pdf.c \
	src/c/compression/codec.c \
	src/c/compression/jpeg.c \
	src/c/compression/vector.c \
	src/c/compression/media.c

HEADERS := \
	src/c/compression/compression.h \
	src/c/compression/codec.h \
	src/c/compression/media.h

.PHONY: all run clean format-c format-c-check check-doc-params

all: $(PROGRAM)

$(PROGRAM): $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

run: $(PROGRAM)
	./$(PROGRAM)

format-c:
	clang-format --style=file:config/.clang-format -i $(SOURCES) $(HEADERS)

format-c-check:
	clang-format --style=file:config/.clang-format --dry-run --Werror $(SOURCES) $(HEADERS)

check-doc-params:
	node scripts/check-doc-params.mjs

clean:
	rm -f $(PROGRAM) *.plist
