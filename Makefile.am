
EXTRA_DIST = utf/checked.h  utf/core.h  utf/unchecked.h CMakeLists.txt

AM_CPPFLAGS = -I${top_srcdir}/avhttp/include -I${top_srcdir}/avproxy/include -I${top_srcdir}

noinst_LTLIBRARIES = libwebqq.la

libwebqq_la_SOURCES = process_group_msg.hpp webqq.cpp webqq.h md5.cpp  url.cpp  webqq_face.cpp  webqq_impl.h webqq_impl.cpp \
		constant.hpp  httpagent.hpp  md5.hpp  url.hpp  utf8.hpp  webqq_login.hpp clean_cache.hpp

libwebqq_la_LDFLAGS = -static

libwebqq_la_LIBADD = ${BOOST_THREAD_LIBS} ${BOOST_SYSTEM_LIBS} ${BOOST_REGEX_LIBS} ${BOOST_SIGNALS_LIBS}
