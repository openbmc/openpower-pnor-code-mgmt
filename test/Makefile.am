AM_CPPFLAGS = -I$(top_srcdir)

# gtest unit tests which run during a 'make check'
check_PROGRAMS = utest test_functions

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
	../item_updater.cpp \
	../image_verify.cpp \
	../utils.cpp \
	../msl_verify.cpp \
	../ubi/activation_ubi.cpp \
	../ubi/item_updater_ubi.cpp \
	../ubi/serialize.cpp \
	../ubi/watch.cpp \
	../static/item_updater_static.cpp \
	../static/activation_static.cpp \
	test_signature.cpp \
	test_version.cpp \
	test_item_updater_static.cpp \
	msl_verify.cpp

test_functions_CPPFLAGS = \
	-Igtest \
	$(GTEST_CPPFLAGS) \
	$(AM_CPPFLAGS)

test_functions_CXXFLAGS = \
	$(PTHREAD_CFLAGS) \
	$(SDBUSPLUS_CFLAGS) \
	$(SDEVENTPLUS_CFLAGS)

test_functions_LDFLAGS = \
	-lgtest_main \
	-lgtest \
	$(SDBUSPLUS_LIBS) \
	$(SDEVENTPLUS_LIBS)

test_functions_SOURCES = \
	test_functions.cpp \
	../functions.cpp
