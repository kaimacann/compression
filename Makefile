# BUILD CONFIGURATION
CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic

# DEMO SOURCES
# Keep the standalone build limited to its menu and compression flow.
C_SOURCES = \
	src/c/main/main.c \
	src/c/lib/file_io/input.c \
	src/c/lib/file_io/output.c \
	src/c/compression/compression.c

# Included declarations and shared file record
C_DEPS = \
	src/c/lib/file_io/file.h \
	src/c/lib/file_io/input.h \
	src/c/lib/file_io/output.h \
	src/c/compression/compression.h

# BUILD AND RUN TARGETS
.PHONY: all run clean

all: compression-demo

compression-demo: $(C_SOURCES) $(C_DEPS)
	$(CC) $(CFLAGS) -o $@ $(C_SOURCES)

run: compression-demo
	./compression-demo

clean:
	rm -f compression-demo *.plist
