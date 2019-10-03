AM_DEFAULT_SOURCE_EXT = .cpp

ACLOCAL_AMFLAGS = -Im4

bin_PROGRAMS = \
	openpower-update-manager \
	openpower-pnor-msl

openpower_update_manager_SOURCES = \
	activation.cpp \
	version.cpp \
	item_updater.cpp \
	item_updater_main.cpp \
	utils.cpp

if HAVE_SYSTEMD
systemdsystemunit_DATA = \
	op-pnor-msl.service \
	org.open_power.Software.Host.Updater.service
endif

if UBIFS_LAYOUT
include ubi/Makefile.am.include
else
include static/Makefile.am.include
if HAVE_SYSTEMD
  systemdsystemunit_DATA += \
    openpower-pnor-update@.service
endif
endif

if SINGLE_IMAGE_LAYOUT
include single-image/Makefile.am.include
endif

if WANT_SIGNATURE_VERIFY_BUILD
openpower_update_manager_SOURCES += \
	image_verify.cpp
endif


openpower_pnor_msl_SOURCES = \
	msl_verify.cpp \
	msl_verify_main.cpp

generic_cxxflags = \
	$(SYSTEMD_CFLAGS) \
	$(PHOSPHOR_DBUS_INTERFACES_CFLAGS) \
	$(SDBUSPLUS_CFLAGS) \
	$(PHOSPHOR_LOGGING_CFLAGS) \
	-flto
generic_ldflags = \
	$(SYSTEMD_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	$(SDBUSPLUS_LIBS) \
	$(PHOSPHOR_LOGGING_LIBS) \
	-lstdc++fs \
	-lssl \
	-lcrypto

openpower_update_manager_CXXFLAGS = $(generic_cxxflags)
openpower_update_manager_LDFLAGS = $(generic_ldflags)
openpower_pnor_msl_CXXFLAGS = $(generic_cxxflags)
openpower_pnor_msl_LDFLAGS = $(generic_ldflags)

SUBDIRS = test
