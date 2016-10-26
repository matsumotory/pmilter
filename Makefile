#   the default target
all: pmilter

#   compile binary
pmilter:
	gcc src/pmilter.c -lmilter -lpthread -o pmilter

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
