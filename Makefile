CC=gcc
PMILTER_ROOT=$(shell pwd)
PMILTER_BUILD_DIR=$(PMILTER_ROOT)/build
PMILTER_INCLUDE_DIR=$(PMILTER_BUILD_DIR)/include
PMILTER_LIB_DIR=$(PMILTER_BUILD_DIR)/lib
PMILTER_BIN_DIR=$(PMILTER_BUILD_DIR)/bin

#   suport mrbgems
MRUBY_MAK_FILE := $(PMILTER_ROOT)/src/mruby/build/host/lib/libmruby.flags.mak
ifeq ($(wildcard $(MRUBY_MAK_FILE)),)
  MRUBY_CFLAGS =
  MRUBY_LDFLAGS =
  MRUBY_LIBS =
  MRUBY_LDFLAGS_BEFORE_LIBS =
else
  include $(MRUBY_MAK_FILE)
endif

PMILTER_LIBS=-lmilter -lmruby -lpthread -ltoml -licuuc -licudata -lm $(MRUBY_LIBS)
PMILTER_CFLAGS=-g -O0 -I$(PMILTER_INCLUDE_DIR)
PMILTER_LDFLAGS=-L$(PMILTER_LIB_DIR) $(MRUBY_LDFLAGS)
PMILTER_BIN=pmilter
PMILTER_SRC=\
src/pmilter.c \
src/pmilter_init.c \
src/pmilter_config.c \
src/pmilter_log.c \
src/pmilter_utils.c \
src/pmilter_mruby_core.c \
src/pmilter_mruby_session.c

#   the default target
all: pmilter-all

#   compile binary
pmilter-all: libmilter libtoml mruby
	$(CC) $(PMILTER_CFLAGS) $(PMILTER_LDFLAGS) $(PMILTER_SRC) $(MRUBY_LDFLAGS_BEFORE_LIBS) $(PMILTER_LIBS) -o $(PMILTER_BIN)

#    compile libmilter
libmilter:
	test -f $(PMILTER_BUILD_DIR)/lib/libmilter.a || (cd src/libmilter && autoreconf -i && automake && \
		autoconf && ./configure --enable-static=yes --enable-shared=no --prefix=$(PMILTER_BUILD_DIR) --includedir=$(PMILTER_BUILD_DIR)/include/libmilter && \
		ln -sf include/sm/os/sm_os_linux.h sm_os.h)
	test -f $(PMILTER_BUILD_DIR)/lib/libmilter.a || (cd src/libmilter && make && make install)

#    compile libtoml
libtoml: icu
	test -f $(PMILTER_BUILD_DIR)/lib/libtoml.a || (cd src/libtoml && cmake -G "Unix Makefiles" . && make && \
		cp libtoml.a $(PMILTER_BUILD_DIR)/lib/. && cp -r toml.h config.h ccan toml_private.h $(PMILTER_BUILD_DIR)/include/.)

#   compile icu
icu:
	test -f $(PMILTER_BUILD_DIR)/lib/libicuuc.a || (cd src/icu/source && ./configure --disable-shared --enable-static --prefix=$(PMILTER_BUILD_DIR) && make && make install)

#    compile mruby
mruby:
	test -f $(PMILTER_BUILD_DIR)/lib/libmruby.a || (cd src/mruby && \
		make && cp build/host/lib/libmruby.a $(PMILTER_BUILD_DIR)/lib/. && \
		cp -r include/* $(PMILTER_BUILD_DIR)/include/.)


#   run
run:
	./pmilter -c pmilter.conf

#   test
test:
	cd test && bundle install --path vendor/bundle && ./test/run-test.rb

#    update subtree
subtree:
	sh ./misc/update-subtree

#   clean
clean:
	-rm -rf pmilter test/vendor test/.bundle build/include build/lib build/bin build/sbin build/share
	cd src/libmilter && make clean
	cd src/libtoml && make clean
	cd src/mruby && make clean
	cd src/icu/source && make clean

.PHONY: test
