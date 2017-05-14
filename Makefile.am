AM_DEFAULT_SOURCE_EXT = .cpp

ACLOCAL_AMFLAGS = -Im4

sbin_PROGRAMS = \
	openpower-update-manager

openpower_update_manager_SOURCES = \
	activation.cpp \
	item_updater.cpp \
	item_updater_main.cpp

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

openpower_update_manager_CXXFLAGS = $(generic_cxxflags)
openpower_update_manager_LDFLAGS = $(generic_ldflags)

SUBDIRS = test

