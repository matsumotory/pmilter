#   the default target
all: pmilter

#   compile binary
pmilter: libmilter libtoml mruby
	gcc src/pmilter.c -lmilter -lpthread -o pmilter

#    compile libmilter
libmilter:
	cd src/libmilter && autoreconf -i && automake && autoconf && ./configure --prefix=`pwd`/build && ln -sf include/sm/os/sm_os_linux.h sm_os.h
	cd src/libmilter && make && make install

#    compile libtoml
libtoml:
	cd src/libtoml && cmake . && make

#    compile mruby
mruby:
	cd src/mruby && make

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
	-rm -rf pmilter test/vendor test/.bundle

.PHONY: test
