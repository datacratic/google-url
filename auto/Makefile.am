ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = \
  -I../../

googleurl_include_HEADERS = \
  ../src/gurl.h \
  ../src/url_canon_internal_file.h \
  ../src/url_file.h \
  ../src/url_util.h \
  ../src/url_canon.h \
  ../src/url_canon_ip.h \
  ../src/url_parse.h \
  ../src/url_util_internal.h \
  ../src/url_canon_icu.h \
  ../src/url_canon_stdstring.h \
  ../src/url_parse_internal.h \
  ../src/url_canon_internal.h \
  ../src/url_common.h \
  ../src/url_test_utils.h

googleurl_includedir = $(includedir)/googleurl

lib_LTLIBRARIES = libgoogleurl.la

libgoogleurl_la_LDFLAGS = -avoid-version $(ICU_LIBS)
libgoogleurl_la_SOURCES = \
  ../src/gurl.cc \
  ../src/url_canon_etc.cc \
  ../src/url_parse_file.cc \
  ../src/url_canon_mailtourl.cc \
  ../src/url_canon_icu.cc \
  ../src/url_canon_fileurl.cc \
  ../src/url_canon_pathurl.cc \
  ../src/url_parse.cc \
  ../src/url_canon_host.cc \
  ../src/url_canon_relative.cc \
  ../src/url_canon_ip.cc \
  ../src/url_util.cc \
  ../src/url_canon_filesystemurl.cc \
  ../src/url_canon_internal.cc \
  ../src/url_canon_stdurl.cc \
  ../src/url_canon_path.cc \
  ../src/url_canon_query.cc
