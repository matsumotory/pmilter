PMILTER_ROOT=$(shell pwd)
PMILTER_BUILD_DIR=$(PMILTER_ROOT)/build

#   the default target
all: pmilter-all

#   compile binary
pmilter-all: libmilter libtoml mruby
	gcc -I$(PMILTER_BUILD_DIR)/include -L$(PMILTER_BUILD_DIR)/lib src/pmilter.c -o pmilter -lmilter -lmruby -lpthread -ltoml -licuuc -licudata

#    compile libmilter
libmilter:
	test -f $(PMILTER_BUILD_DIR)/lib/libmilter.a || (cd src/libmilter && autoreconf -i && automake && \
		autoconf && ./configure --enable-static=yes --enable-shared=no --prefix=$(PMILTER_BUILD_DIR) && \
		ln -sf include/sm/os/sm_os_linux.h sm_os.h)
	test -f $(PMILTER_BUILD_DIR)/lib/libmilter.a || (cd src/libmilter && make && make install)

#    compile libtoml
libtoml:
	test -f $(PMILTER_BUILD_DIR)/lib/libtoml.a || (cd src/libtoml && cmake -G "Unix Makefiles" . && make && \
		cp libtoml.a $(PMILTER_BUILD_DIR)/lib/. && cp toml.h $(PMILTER_BUILD_DIR)/include/.)

#    compile mruby
mruby:
	test -f $(PMILTER_BUILD_DIR)/lib/libmruby.a || (cd src/mruby && \
		make && cp build/host/lib/libmruby.a $(PMILTER_BUILD_DIR)/lib/. && \
		cp -r include/* $(PMILTER_BUILD_DIR)/include/.)

#   run
run:
	./pmilter -p hoge.sock

#   test
test:
	cd test && bundle install --path vendor/bundle && ./test/run-test.rb

#    update subtree
subtree:
	sh ./misc/update-subtree

#   clean
clean:
	-rm -rf pmilter test/vendor test/.bundle build/include build/lib
	cd src/libmilter && make clean
	cd src/libtoml && make clean
	cd src/mruby && make clean

.PHONY: test
