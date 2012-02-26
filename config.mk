# Program version
PROGRAM_NAME = avolt
VERSION      = 0.4.2
SRC_POSTFIX  = .c


# Customize below to fit your system


# Install Paths
PREFIX = /usr
MANPREFIX = ${PREFIX}/share/man


# Build files/Paths

# Program binary name
BIN        ?= $(PROGRAM_NAME)
# default build dir
BUILDDIR   := build
# dir to store automatically generated dependency info files
DEPDIR     := .deps


# includes and libs

ALSAINC = `pkg-config --cflags alsa`
INCS = ${ALSAINC}

ALSALIB = `pkg-config --libs alsa`
LIBS = -lm ${ALSALIB}

ifdef EFENCE
	LIBS = ${LIBS} -lefence
endif

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\" -D_POSIX_C_SOURCE=200112L
CFLAGS   = -mtune=core2 -march=core2 -std=c99 -pedantic -Wall -O3 ${INCS} ${CPPFLAGS}
LDFLAGS  = -ggdb ${LIBS}

# Compiler and linker
ifdef CLANG
	CC      := clang
	LINKER  := clang
else
	CC      := cc
	LINKER  := cc
endif
