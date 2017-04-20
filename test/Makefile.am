AM_CPPFLAGS = -I$(top_srcdir)

# gtest unit tests which run during a 'make check'
check_PROGRAMS = utest

# Run all 'check' test programs
TESTS = $(check_PROGRAMS)

# Build/add utest to test suite
utest_CPPFLAGS = -Igtest $(GTEST_CPPFLAGS) $(AM_CPPFLAGS)
utest_CXXFLAGS = $(PTHREAD_CFLAGS) $(PHOSPHOR_LOGGING_CFLAGS)
utest_LDFLAGS = -lgtest_main -lgtest $(PTHREAD_LIBS) $(OESDK_TESTCASE_FLAGS) \
	$(PHOSPHOR_LOGGING_LIBS)

utest_SOURCES = utest.cpp
