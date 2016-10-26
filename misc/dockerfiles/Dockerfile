FROM ubuntu:xenial

RUN apt -y update
RUN apt -y install \
    build-essential automake m4 autoconf ragel \
    libtool cmake pkg-config libcunit1-dev libicu-dev \
    ruby bison

VOLUME /pmilter

WORKDIR /pmilter

CMD ["/usr/bin/make"]
