# ###################################################################
# Author: Esa Määttä, 2011
# ###################################################################

# Include helper scripts for makefile environment options
include Makefile_options

# Include project specific make configuration
include config.mk

# ###################################################################
# Prepare build by creating needed dirs if they don't exist

# Create the dependency dir
ifeq ($(wildcard $(DEPDIR)/),)
	__NULL  := $(shell mkdir -p ${DEPDIR})
endif

# Create the build dir
ifeq ($(wildcard $(BUILDDIR)/),)
	__NULL  := $(shell mkdir -p $(BUILDDIR))
endif

# Some colors for the output
WHITE_H     := "\033[1;37;40m"
PURPLE_H    := "\033[1;35;40m"
CLR_COLOR   := "\033[1;0m"

_info       := $(DEPDIR)/.info

# ###################################################################
# Defined options, options handling code is in included makefile.
# 	Option is enabled by existing environment variable by the same name (see
# 	`make help`).
# 	Options which start with `DEBUG_` or `D_` end up as passed to the program as
# 	predefined macros (defines) with the compile `-D` flag.
# 	Other options are intended to use with compilation configuration for example
# 	inside this makefile.

# Makefile configuration options
OPTIONS__BIN     = "Binary file to create"
OPTIONS__EFENCE  = "Link against efence to ease memory debugging."
OPTIONS__DESTDIR = "Install destination directory."
OPTIONS__GCC     = "Use gcc as a compiler (default is clang)."
OPTIONS__DEBUG_AVOLT = "Enable debug printing."

# ###################################################################
# Compiler flags

# Add debug options as -D flags to CXXFLAGS
$(eval CPPFLAGS := $(CPPFLAGS) $(defined_define_option_name_flags))

# ###################################################################
# Files

# Rake all *$(SRC_POSTFIX) files from the $(SRCDIR) dir, existing and generated
SOURCES := $(wildcard $(SRCDIR)/*$(SRC_POSTFIX))
SOURCES_WITHOUT_PATH := $(SOURCES:$(SRCDIR)/%=%)
OBJECTS = $(SOURCES_WITHOUT_PATH:%$(SRC_POSTFIX)=$(BUILDDIR)/%.o)

# For debug
#$(info SOURCES:)
#$(info $(SOURCES))
#$(info OBJECTS:)
#$(info $(OBJECTS))

# ###################################################################
# Targets and rules
# ###################################################################


# Default target
#all: $(if $(wildcard $(BUILDDIR)/$(BIN)),,info) $(BUILDDIR)/$(BIN)
.PHONY: all
all: $(_info) $(BUILDDIR)/$(BIN)

# Link
$(BUILDDIR)/$(BIN): $(OBJECTS)
	@echo -e ${WHITE_H}Linking to $@...${CLR_COLOR}
	@$(LINKER) -o $(BUILDDIR)/$(BIN) $(LDFLAGS) $^

config.mk:
	$(error config.mk file is missing)

# Pull in dependency info for *existing* .o files
-include $(SOURCES:%$(SRC_POSTFIX)=$(DEPDIR)/%.d)


########### compile objects with some autodep magic


$(BUILDDIR)/%.o: $(SRCDIR)/%$(SRC_POSTFIX) config.mk
	@echo -e ${PURPLE_H}Compiling $<...${CLR_COLOR}
	@$(COMPILE$(SRC_POSTFIX)) -MMD -MP -MF $(DEPDIR)/$*.d $(SRCDIR)/$*$(SRC_POSTFIX) -o $(BUILDDIR)/$*.o


########### Additional PHONY targets


# Info target, provide info when compiling, not to be called
.ONESHELL: $(_info)
$(_info): config.mk
	@echo -e ${WHITE_H}Building binary $(BUILDDIR)/$(BIN)${CLR_COLOR} v$(VERSION);
	$(options_check)
	$(used_options)
	touch $(_info)

# Let's be quite careful when cleaning (definitely no rm -rf :))
.PHONY: clean_build_dir
clean_build_dir:
	@rm -f -- $(BUILDDIR)/*.o $(BUILDDIR)/$(BIN) ${PROGRAM_NAME}-${VERSION}.tar.gz
	@if [[ "${BUILDDIR}" != "." && "${BUILDDIR}" != "./" ]]; then rmdir -- $(BUILDDIR); fi;

# Let's be quite careful when cleaning (definitely no rm -rf :))
.PHONY: clean_dep_dir
clean_dep_dir:
	@rm -f -- $(DEPDIR)/*.d
	@rm -f -- $(_info)
	@rmdir -- $(DEPDIR)

.PHONY: clean
clean: clean_build_dir clean_dep_dir
	@rm -f -- ${PROGRAM_NAME}-${VERSION}.tar.gz

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
	@mkdir -p -- ${DESTDIR}${PREFIX}/bin
	@cp -f -- $(BUILDDIR)/${BIN} ${DESTDIR}${PREFIX}/bin
	@chmod 755 -- ${DESTDIR}${PREFIX}/bin/${BIN}
	@# Man page installing
	#@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	#@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	#@sed "s/VERSION/${VERSION}/g" < ${BIN}.1 > ${DESTDIR}${MANPREFIX}/man1/${BIN}.1
	#@chmod 644 ${DESTDIR}${MANPREFIX}/man1/${BIN}.1

.PHONY: uninstall
uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f -- ${DESTDIR}${PREFIX}/bin/${BIN}
	@# Man page uninstalling
	#@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	#@rm -f -- ${DESTDIR}${MANPREFIX}/man1/${BIN}.1

.PHONY: help
help:
	@echo -e  "Environment variables to control the build:"
	$(options)
	@$(if $(defined_define_option_names),echo; echo "These debug options are now defined: "; echo "$(defined_define_option_names)")
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
	#@echo
	#@echo "To be manpage install dir ({DESTDIR}{MANPREFIX}/man1):"
	#@echo ${DESTDIR}${MANPREFIX}/man1


.PHONY: test
test: all
	@echo "Running tests.."
	@./testrunner
