
EXTRA_DIST = impl utf CMakeLists.txt

AM_CPPFLAGS = -I${top_srcdir}/avhttp/include -I${top_srcdir}/avproxy/include -I${top_srcdir}

noinst_LTLIBRARIES = libwebqq.la

libwebqq_la_SOURCES = webqq.cpp webqq.hpp urlencode.hpp

libwebqq_la_LDFLAGS = -static

libwebqq_la_LIBADD = ${BOOST_THREAD_LIBS} ${BOOST_SYSTEM_LIBS} ${BOOST_REGEX_LIBS} ${BOOST_SIGNALS_LIBS}
