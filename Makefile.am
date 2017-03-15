AM_DEFAULT_SOURCE_EXT = .cpp

sbin_PROGRAMS = \
	openpower-version-host-software-manager \
	openpower-update-manager

openpower_version_host_software_manager_SOURCES = \
	version_host_software_manager.cpp \
	version_host_software_manager_main.cpp

openpower_update_manager_SOURCES = \
	item_updater.cpp \
	item_updater_main.cpp

generic_cxxflags = \
	$(SYSTEMD_CFLAGS) \
	$(PHOSPHOR_DBUS_INTERFACES_CFLAGS) \
	$(SDBUSPLUS_CFLAGS)
generic_ldflags = \
	$(SYSTEMD_LIBS) \
	$(PHOSPHOR_DBUS_INTERFACES_LIBS) \
	$(SDBUSPLUS_LIBS)

openpower_version_host_software_manager_CXXFLAGS = $(generic_cxxflags)
openpower_version_host_software_manager_LDFLAGS = $(generic_ldflags)
openpower_update_manager_CXXFLAGS = $(generic_cxxflags)
openpower_update_manager_LDFLAGS = $(generic_ldflags)

