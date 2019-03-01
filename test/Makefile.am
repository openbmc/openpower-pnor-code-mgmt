AM_CPPFLAGS = -I$(top_srcdir)

# gtest unit tests which run during a 'make check'
check_PROGRAMS = utest

# Run all 'check' test programs
TESTS = $(check_PROGRAMS)

# Build/add utest to test suite
utest_CPPFLAGS = \
	-Igtest \
	$(GTEST_CPPFLAGS) \
	$(AM_CPPFLAGS)

utest_CXXFLAGS = \
	$(PTHREAD_CFLAGS) \
	$(PHOSPHOR_LOGGING_CFLAGS) \
	$(PHOSPHOR_DBUS_INTERFACES_CFLAGS)

utest_LDFLAGS = \
	-lgtest_main \
	-lgtest \
	$(PTHREAD_LIBS) \
	$(OESDK_TESTCASE_FLAGS) \
	$(PHOSPHOR_LOGGING_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	-lssl \
	-lcrypto

utest_SOURCES = \
	$(top_builddir)/activation.cpp \
	$(top_builddir)/version.cpp \
	$(top_builddir)/serialize.cpp \
	$(top_builddir)/item_updater.cpp \
	$(top_builddir)/org/openbmc/Associations/server.cpp \
	$(top_builddir)/image_verify.cpp \
	$(top_builddir)/msl_verify.cpp \
	$(top_builddir)/ubi/activation_ubi.cpp \
	$(top_builddir)/ubi/item_updater_ubi.cpp \
	$(top_builddir)/ubi/watch.cpp \
	$(top_builddir)/static/item_updater_static.cpp \
	$(top_builddir)/static/activation_static.cpp \
	utest.cpp \
	msl_verify.cpp

utest_LDADD = -lstdc++fs
