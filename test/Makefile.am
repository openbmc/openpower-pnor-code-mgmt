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
	../activation.cpp \
	../version.cpp \
	../serialize.cpp \
	../item_updater.cpp \
	../org/openbmc/Associations/server.cpp \
	../image_verify.cpp \
	../msl_verify.cpp \
	../ubi/activation_ubi.cpp \
	../ubi/item_updater_ubi.cpp \
	../ubi/watch.cpp \
	../static/item_updater_static.cpp \
	../static/activation_static.cpp \
	utest.cpp \
	msl_verify.cpp

utest_LDADD = -lstdc++fs
