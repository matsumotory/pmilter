GENCCODE_ASSEMBLY_TYPE=-a gcc
SO=so
SOBJ=so
A=a
LIBPREFIX=lib
LIB_EXT_ORDER=.50.1
COMPILE=gcc -D_REENTRANT   -DU_ATTRIBUTE_DEPRECATED= -O2 -Wall -std=c99 -pedantic -Wshadow -Wpointer-arith -Wmissing-prototypes -Wwrite-strings  -c
LIBFLAGS=-I/home/ubuntu/pmilter/src/icu/source/build/include -DPIC -fPIC
GENLIB=gcc -O2 -Wall -std=c99 -pedantic -Wshadow -Wpointer-arith -Wmissing-prototypes -Wwrite-strings   -shared -Wl,-Bsymbolic
LDICUDTFLAGS=-nodefaultlibs -nostdlib
LD_SONAME=-Wl,-soname -Wl,
RPATH_FLAGS=
BIR_LDFLAGS=-Wl,-Bsymbolic
AR=ar
ARFLAGS=r
RANLIB=ranlib
INSTALL_CMD=/usr/bin/install -c
