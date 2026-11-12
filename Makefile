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
	src/c/compression/media.c \
	src/c/encryption/encryption.c \
	src/c/login/login.c

HEADERS := \
	src/c/compression/compression.h \
	src/c/compression/codec.h \
	src/c/compression/media.h \
	src/c/encryption/encryption.h \
	src/c/login/login.h

.PHONY: all run clean format-c format-c-check check-doc-params test-media-output \
	test-archive-roundtrip test-codec-primitives test-vector-transform test-media-jpeg-io

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

test-media-output:
	@set -e; binary=$$(mktemp /tmp/compression-media-output.XXXXXX); trap 'rm -f "$$binary"' EXIT; \
		$(CC) $(CPPFLAGS) $(CFLAGS) -o "$$binary" tests/c/media-output-directory.c \
			$(filter-out src/c/main/main.c src/c/compression/media.c,$(SOURCES)) $(LDLIBS); \
		"$$binary"

test-archive-roundtrip:
	@set -e; binary=$$(mktemp /tmp/compression-archive-roundtrip.XXXXXX); trap 'rm -f "$$binary"' EXIT; \
		$(CC) $(CPPFLAGS) $(CFLAGS) -o "$$binary" tests/c/archive-roundtrip.c \
			$(filter-out src/c/main/main.c,$(SOURCES)) $(LDLIBS); \
		"$$binary"

test-codec-primitives:
	@set -e; binary=$$(mktemp /tmp/compression-codec-primitives.XXXXXX); trap 'rm -f "$$binary"' EXIT; \
		$(CC) $(CPPFLAGS) $(CFLAGS) -o "$$binary" tests/c/codec-primitives.c \
			$(filter-out src/c/main/main.c,$(SOURCES)) $(LDLIBS); \
		"$$binary"

test-vector-transform:
	@set -e; binary=$$(mktemp /tmp/compression-vector-transform.XXXXXX); trap 'rm -f "$$binary"' EXIT; \
		$(CC) $(CPPFLAGS) $(CFLAGS) -o "$$binary" tests/c/vector-transform.c \
			$(filter-out src/c/main/main.c,$(SOURCES)) $(LDLIBS); \
		"$$binary"

test-media-jpeg-io:
	@set -e; binary=$$(mktemp /tmp/compression-media-jpeg-io.XXXXXX); trap 'rm -f "$$binary"' EXIT; \
		$(CC) $(CPPFLAGS) $(CFLAGS) -o "$$binary" tests/c/media-jpeg-io.c \
			$(filter-out src/c/main/main.c,$(SOURCES)) $(LDLIBS); \
		"$$binary"

clean:
	rm -f $(PROGRAM) *.plist
