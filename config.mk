# Program version
PROGRAM_NAME = avolt
VERSION      = 0.4.4a
SRC_POSTFIX  = .c


# Customize below to fit your system

### Extra compile-time configuration options
# -DUSE_SEMAPHORE=[true|false]
#  Use semaphores to prevent synchronous volume setting. Synchronism can cause
#  some undesirable effects with alsa.
# -DSEMAPHORE_NAME=\"<name>\"
#  Name for the semaphore to use, change if one with current name already
#  exists, or is used by some other program.

CONFIG_OPTS=-DUSE_SEMAPHORE=true -DSEMAPHORE_NAME=\"avolt\"


# Install Paths
PREFIX = /usr
MANPREFIX = ${PREFIX}/share/man


# Build files/Paths

# Program binary name
BIN        ?= $(PROGRAM_NAME)
# default build dir
BUILDDIR   := build
# Source dir
SRCDIR     := src
# dir to store automatically generated dependency info files
DEPDIR     := .deps


# includes and libs

ALSAINC = `pkg-config --cflags alsa`
INCS = ${ALSAINC}

ALSALIB = `pkg-config --libs alsa`
LIBS = -lm -pthread ${ALSALIB}

ifdef EFENCE
	LIBS = ${LIBS} -lefence
endif

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\" -D_POSIX_C_SOURCE=200809L ${CONFIG_OPTS}
CFLAGS   = -mtune=native -march=native -std=c99 -pedantic -Wall -O3 ${INCS} ${CPPFLAGS}
LDFLAGS  = -ggdb ${LIBS}

# Compiler and linker
ifdef CLANG
	CC      := clang
	LINKER  := clang
else
	CC      := cc
	LINKER  := cc
endif
