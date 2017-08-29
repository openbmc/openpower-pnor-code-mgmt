AM_DEFAULT_SOURCE_EXT = .cpp

ACLOCAL_AMFLAGS = -Im4

sbin_PROGRAMS = \
	openpower-update-manager

openpower_update_manager_SOURCES = \
	activation.cpp \
	version.cpp \
	serialize.cpp \
	item_updater.cpp \
	item_updater_main.cpp

nodist_openpower_update_manager_SOURCES = \
	org/openbmc/Associations/server.cpp

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
	$(PHOSPHOR_LOGGING_CFLAGS)
generic_ldflags = \
	$(SYSTEMD_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	$(SDBUSPLUS_LIBS) \
	$(PHOSPHOR_LOGGING_LIBS) \
	-lstdc++fs

org/openbmc/Associations/server.cpp: org/openbmc/Associations.interface.yaml
	@mkdir -p `dirname $@`
	$(SDBUSPLUSPLUS) -r $(srcdir) interface server-cpp org.openbmc.Associations > $@

org/openbmc/Associations/server.hpp: org/openbmc/Associations.interface.yaml
	@mkdir -p `dirname $@`
	$(SDBUSPLUSPLUS) -r $(srcdir) interface server-header org.openbmc.Associations > $@

openpower_update_manager_CXXFLAGS = $(generic_cxxflags)
openpower_update_manager_LDFLAGS = $(generic_ldflags)

SUBDIRS = test
