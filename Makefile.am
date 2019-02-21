AM_DEFAULT_SOURCE_EXT = .cpp

ACLOCAL_AMFLAGS = -Im4

sbin_PROGRAMS = \
	openpower-update-manager \
	openpower-pnor-msl

openpower_update_manager_SOURCES = \
	activation.cpp \
	version.cpp \
	serialize.cpp \
	watch.cpp \
	item_updater.cpp \
	item_updater_main.cpp

if UBIFS_LAYOUT
include ubi/Makefile.am.include
else
include static/Makefile.am.include
endif

if WANT_SIGNATURE_VERIFY_BUILD
openpower_update_manager_SOURCES += \
	image_verify.cpp \
	utils.cpp
endif

nodist_openpower_update_manager_SOURCES = \
	org/openbmc/Associations/server.cpp

openpower_pnor_msl_SOURCES = \
	msl_verify.cpp \
	msl_verify_main.cpp

CLEANFILES = \
	org/openbmc/Associations/server.cpp \
	org/openbmc/Associations/server.hpp

BUILT_SOURCES = \
	org/openbmc/Associations/server.cpp \
	org/openbmc/Associations/server.hpp

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

org/openbmc/Associations/server.cpp: org/openbmc/Associations.interface.yaml
	@mkdir -p `dirname $@`
	$(SDBUSPLUSPLUS) -r $(srcdir) interface server-cpp org.openbmc.Associations > $@

org/openbmc/Associations/server.hpp: org/openbmc/Associations.interface.yaml
	@mkdir -p `dirname $@`
	$(SDBUSPLUSPLUS) -r $(srcdir) interface server-header org.openbmc.Associations > $@

openpower_update_manager_CXXFLAGS = $(generic_cxxflags)
openpower_update_manager_LDFLAGS = $(generic_ldflags)
openpower_pnor_msl_CXXFLAGS = $(generic_cxxflags)
openpower_pnor_msl_LDFLAGS = $(generic_ldflags)

SUBDIRS = test
