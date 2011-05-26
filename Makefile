.PHONY: all clean gcc configure

all:
	@waf

configure:
	@waf configure

gcc:
	@waf configure --use-gcc
	@waf

clean:
	@waf clean
