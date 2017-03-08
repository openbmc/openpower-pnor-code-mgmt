AM_DEFAULT_SOURCE_EXT = .cpp

sbin_PROGRAMS = \
	phosphor-version-host-software-manager

phosphor_version_host_software_manager_SOURCES = \
	version_host_software_manager.cpp \
	version_host_software_manager_main.cpp

generic_cxxflags = $(SYSTEMD_CFLAGS) $(PHOSPHOR_DBUS_INTERFACES_CFLAGS) $(SDBUSPLUS_CFLAGS)
generic_ldflags = $(SYSTEMD_LIBS) $(PHOSPHOR_DBUS_INTERFACES_LIBS) $(SDBUSPLUS_LIBS)

phosphor_version_host_software_manager_CXXFLAGS = $(generic_cxxflags)
phosphor_version_host_software_manager_LDFLAGS = $(generic_ldflags)

