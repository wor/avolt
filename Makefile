# ###################################################################
# Author: Esa Määttä, 2011
# ###################################################################
#	For reference about autodep magic look:
#	http://scottmcpeak.com/autodepend/autodepend.html
#	http://make.paulandlesley.org/autodep.html

#	For some help about gnu make:
#	http://singhkunal.com/website/techstuff/tutorials/GNUMakeTutorial.htm

#	For some help about gnu make:
#	http://singhkunal.com/website/techstuff/tutorials/GNUMakeTutorial.htm

# Include helper scripts for makefile environment options
include Makefile_options

# Include project specific make configuration
include config.mk

# ###################################################################
# Prepare build by creating needed dirs if they don't exist

# create the dependency dir
ifeq ($(wildcard $(DEPDIR)/),)
	__NULL  := $(shell mkdir -p ${DEPDIR})
endif

# create the build dir
ifeq ($(wildcard $(BUILDDIR)/),)
	__NULL  := $(shell mkdir -p $(BUILDDIR))
endif

# some colors for the output
WHITE_H     := "\033[1;37;40m"
PURPLE_H    := "\033[1;35;40m"
CLR_COLOR   := "\033[1;0m"

# ###################################################################
# Defined options, options handling code is in included makefile.
# 	Option is enabled by existing environment variable by the same name (see
# 	`make help`).

OPTIONS__BIN = "Binary file to create"
OPTIONS__EFENCE = "Link against efence to ease memory debugging."
OPTIONS__DESTDIR = "Install destination directory."
OPTIONS__GCC = "Use gcc as a compiler (default is clang)."
OPTIONS__DEBUG_TEST = "Example debug flag, if 'DEBUG_TEST' env variable defined then flag '-DDEBUG_TEST' is given to the compiler."
# Add a new option here..

# ###################################################################
# Compiler flags

# Add debug options as -D flags to CXXFLAGS
$(eval CPPFLAGS := $(CPPFLAGS) $(defined_debug_option_name_flags))

# ###################################################################
# Files

# take all *$(SRC_POSTFIX) files from current dir, existing and generated
SOURCES := $(wildcard *$(SRC_POSTFIX))
OBJECTS = $(SOURCES:%$(SRC_POSTFIX)=$(BUILDDIR)/%.o)


# ###################################################################
# Targets and rules
# ###################################################################


# Default target
.PHONY: all
all: $(if $(wildcard $(BUILDDIR)/$(BIN)),,info) $(BUILDDIR)/$(BIN)

# link
$(BUILDDIR)/$(BIN): $(OBJECTS)
	@echo -e ${WHITE_H}Linking to $@...${CLR_COLOR}
	@$(LINKER) -o $(BUILDDIR)/$(BIN) $(LDFLAGS) $^

# pull in dependency info for *existing* .o files
-include $(SOURCES:%$(SRC_POSTFIX)=$(DEPDIR)/%.d)


########### compile objects with some autodep magic


$(BUILDDIR)/%.o: %$(SRC_POSTFIX)
	@echo -e ${PURPLE_H}Compiling $<...${CLR_COLOR}
	@$(COMPILE$(SRC_POSTFIX)) -MMD -MP -MF $(DEPDIR)/$*.d $*$(SRC_POSTFIX) -o $(BUILDDIR)/$*.o


########### Additional PHONY targets


# Info target, provide info when compiling, not to be called
.ONESHELL: info
.PHONY: info
info:
	@echo -e ${WHITE_H}Building binary $(BUILDDIR)/$(BIN)${CLR_COLOR};
	$(options_check)
	$(used_options)

# Let's be quite careful when cleaning (definitely no rm -rf :))
.PHONY: clean
clean:
	@rm -f -- $(BUILDDIR)/*.o $(BUILDDIR)/$(BIN) ${PROGRAM_NAME}-${VERSION}.tar.gz
	@if [[ "${BUILDDIR}" != "." && "${BUILDDIR}" != "./" ]]; then rmdir -- $(BUILDDIR); fi;
	@rm -f -- $(DEPDIR)/*.d
	@rmdir -- $(DEPDIR)

.PHONY: doc
doxygen doc:
	doxygen .Doxyfile

.PHONY: dist
dist: clean
	@echo creating dist tarball
	@mkdir -p -- ${PROGRAM_NAME}-${VERSION}
	@cp -R -- LICENSE Makefile README config.mk \
		${PROGRAM_NAME}.1 ${SOURCES} ${PROGRAM_NAME}-${VERSION}
	@tar -cf -- ${PROGRAM_NAME}-${VERSION}.tar ${PROGRAM_NAME}-${VERSION}
	@gzip -- ${PROGRAM_NAME}-${VERSION}.tar
	@rm -f -- ${PROGRAM_NAME}-${VERSION}/*
	@rmdir -- ${PROGRAM_NAME}-${VERSION}

.PHONY: install
install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f $(BUILDDIR)/${BIN} ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/${BIN}
	@# Man page installing
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < ${BIN}.1 > ${DESTDIR}${MANPREFIX}/man1/${BIN}.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/${BIN}.1

.PHONY: uninstall
uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/${BIN}
	@# Man page uninstalling
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/${BIN}.1

.PHONY: help
help:
	@echo -e  "Environment variables to control the build:"
	$(options)
	@$(if $(defined_debug_option_names),echo; echo "These debug options are now defined: "; echo "$(defined_debug_option_names)")
	@echo -e "\nTargets:"
	@make -rpn | grep "^\.PHONY:" | sed 's/.PHONY: //'
	@echo
	@echo "'${BIN}' build options for the project:"
	@if [[ "${CXXFLAGS}" ]]; then
	@echo "CXXFLAGS = ${CXXFLAGS}"
	@echo "CXX      = ${CXX}"
	@fi
	@if [[ "${CFLAGS}" ]]; then
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "CC       = ${CC}"
	@fi
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "LINKER   = ${LINKER}"
	@echo
	@echo "To be binary install dir ({DESTDIR}{PREFIX}/bin):"
	@echo ${DESTDIR}${PREFIX}/bin
	@echo
	@echo "To be manpage install dir ({DESTDIR}{MANPREFIX}/man1):"
	@echo ${DESTDIR}${MANPREFIX}/man1


.PHONY: test
test: all
	@echo "Running tests.."
	@./testrunner
