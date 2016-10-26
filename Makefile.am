ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

include_HEADERS = include/libmilter/mfapi.h include/libmilter/mfdef.h

lib_LTLIBRARIES = libmilter.la

libmilter_la_SOURCES = errstring.c strl.c comm.c engine.c handler.c listener.c main.c monitor.c signal.c smfi.c sm_gethost.c worker.c

libmilter_la_CPPFLAGS = -DNOT_SENDMAIL -Dsm_snprintf=snprintf -D_REENTRANT -DXP_MT -Ismapp -Iinclude -Wall

libmilter_la_LDFLAGS = -version-info 2:0:0
