BUILD_DIR=/tmp/avolt_build
.PHONY: all clean gcc configure

define CONFIGURE =
	@waf configure
endef

all:
ifeq "$(wildcard $(BUILD_DIR)/*)" ""
	$(call CONFIGURE)
endif
	@waf

configure:
	$(call CONFIGURE)

gcc:
	@waf configure --use-gcc
	@waf

clean:
	@waf clean
