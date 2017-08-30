AC_PREREQ([2.69])
AC_INIT([openpower-pnor-code-mgmt], [1.0], [https://github.com/openbmc/openpower-pnor-code-mgmt/issues])
AC_LANG([C++])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign dist-xz])
AM_SILENT_RULES([yes])

AC_CONFIG_MACRO_DIRS([m4])

# Checks for programs
AC_PROG_CXX
AC_PROG_INSTALL #Checks/sets the install variable to be used
AC_PROG_MAKE_SET

# Check for libraries
PKG_CHECK_MODULES([PHOSPHOR_DBUS_INTERFACES], [phosphor-dbus-interfaces],,\
    AC_MSG_ERROR(["Requires phosphor-dbus-interfaces package."]))
PKG_CHECK_MODULES([SDBUSPLUS], [sdbusplus],,
    AC_MSG_ERROR(["Requires sdbusplus package."]))
PKG_CHECK_MODULES([PHOSPHOR_LOGGING], [phosphor-logging],,
    [AC_MSG_ERROR([The openbmc/phosphor-logging package is required])])

AC_PATH_PROG([SDBUSPLUSPLUS], [sdbus++])

# Checks for library functions
LT_INIT # Required for systemd linking

# Check/set gtest specific functions.
AX_PTHREAD([GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=1"],[GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=0"])
AC_SUBST(GTEST_CPPFLAGS)
AC_ARG_ENABLE([oe-sdk],
    AS_HELP_STRING([--enable-oe-sdk], [Link testcases absolutely against OE SDK so they can be ran within it.])
)
AC_ARG_VAR(OECORE_TARGET_SYSROOT,
    [Path to the OE SDK SYSROOT])
AS_IF([test "x$enable_oe_sdk" == "xyes"],
    AS_IF([test "x$OECORE_TARGET_SYSROOT" == "x"],
          AC_MSG_ERROR([OECORE_TARGET_SYSROOT must be set with --enable-oe-sdk])
    )
    AC_MSG_NOTICE([Enabling OE-SDK at $OECORE_TARGET_SYSROOT])
    [
        testcase_flags="-Wl,-rpath,\${OECORE_TARGET_SYSROOT}/lib"
        testcase_flags="${testcase_flags} -Wl,-rpath,\${OECORE_TARGET_SYSROOT}/usr/lib"
        testcase_flags="${testcase_flags} -Wl,-dynamic-linker,`find \${OECORE_TARGET_SYSROOT}/lib/ld-*.so | sort -r -n | head -n1`"
    ]
    AC_SUBST([OESDK_TESTCASE_FLAGS], [$testcase_flags])
)

# Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX_14([noext])
AX_APPEND_COMPILE_FLAGS([-Wall -Werror], [CXXFLAGS])

AC_ARG_VAR(SOFTWARE_OBJPATH, [The software manager Dbus root])
AS_IF([test "x$SOFTWARE_OBJPATH" == "x"], [SOFTWARE_OBJPATH="/xyz/openbmc_project/software"])
AC_DEFINE_UNQUOTED([SOFTWARE_OBJPATH], ["$SOFTWARE_OBJPATH"], [The software manager Dbus root])

AC_DEFINE(ACTIVATION_FWD_ASSOCIATION, "inventory", [The name of the activation's forward association.])
AC_DEFINE(ACTIVATION_REV_ASSOCIATION, "activation", [The name of the activation's reverse association.])
AC_DEFINE(HOST_INVENTORY_PATH, "/xyz/openbmc_project/inventory/system/chassis", [The host inventory path.])

AC_DEFINE(ACTIVE_FWD_ASSOCIATION, "active", [The name of the active's forward association.])
AC_DEFINE(ACTIVE_REV_ASSOCIATION, "software_version", [The name of the active's reverse association.])

AC_DEFINE(VERSION_IFACE, "xyz.openbmc_project.Software.Version",
    [The software version manager interface])
AC_DEFINE(FILEPATH_IFACE, "xyz.openbmc_project.Common.FilePath",
    [The common file path interface])

AC_DEFINE(BUSNAME_UPDATER, "org.open_power.Software.Host.Updater",
    [The item updater DBus busname to own.])

AC_ARG_VAR(MANIFEST_FILE, [The path to the MANIFEST file])
AS_IF([test "x$MANIFEST_FILE" == "x"], [MANIFEST_FILE="MANIFEST"])
AC_DEFINE_UNQUOTED([MANIFEST_FILE], ["$MANIFEST_FILE"], [The path to the MANIFEST file])

AC_DEFINE(MAPPER_BUSNAME, "xyz.openbmc_project.ObjectMapper",
    [The object mapper busname.])
AC_DEFINE(MAPPER_PATH, "/xyz/openbmc_project/object_mapper",
    [The object mapper path.])
AC_DEFINE(MAPPER_INTERFACE, "xyz.openbmc_project.ObjectMapper",
    [The object mapper interface.])

AC_DEFINE(SYSTEMD_BUSNAME, "org.freedesktop.systemd1",
    [systemd busname.])
AC_DEFINE(SYSTEMD_PATH, "/org/freedesktop/systemd1",
    [systemd path.])
AC_DEFINE(SYSTEMD_INTERFACE, "org.freedesktop.systemd1.Manager",
    [systemd interface.])

AC_DEFINE(PNOR_TOC_FILE, "pnor.toc",
    [The name of the PNOR table of contents file])
AC_DEFINE(MEDIA_DIR, "/media/",
    [The base dir where all PNOR RO AND RW partitions are mounted])
AC_DEFINE(PERSIST_DIR, "/var/lib/obmc/openpower-pnor-code-mgmt/",
    [The dir where activation data is stored in files])
AC_DEFINE(PNOR_RO_PREFIX, "/media/pnor-ro-",
    [The prefix path for the versioned read-only pnor partitions])
AC_DEFINE(PNOR_RW_PREFIX, "/media/pnor-rw-",
    [The prefix path for the versioned read-write pnor partitions])
AC_DEFINE(PNOR_PRSV, "/media/pnor-prsv",
    [The path for the preserved pnor partitions])
AC_DEFINE(PNOR_ACTIVE_PATH, "/var/lib/phosphor-software-manager/pnor/",
    [Path to the active pnor partitions])
AC_DEFINE(PNOR_RO_ACTIVE_PATH, "/var/lib/phosphor-software-manager/pnor/ro",
    [Path to the active read-only pnor partitions])
AC_DEFINE(PNOR_RW_ACTIVE_PATH, "/var/lib/phosphor-software-manager/pnor/rw",
    [Path to the active read-write pnor partitions])
AC_DEFINE(PNOR_PRSV_ACTIVE_PATH, "/var/lib/phosphor-software-manager/pnor/prsv",
    [Path to the active preserved pnor partitions])

AC_ARG_VAR(ACTIVE_PNOR_MAX_ALLOWED, [The maximum allowed active pnor versions])
AS_IF([test "x$ACTIVE_PNOR_MAX_ALLOWED" == "x"], [ACTIVE_PNOR_MAX_ALLOWED=2])
AC_DEFINE_UNQUOTED([ACTIVE_PNOR_MAX_ALLOWED], [$ACTIVE_PNOR_MAX_ALLOWED], [The maximum allowed active pnor versions])

AC_CONFIG_FILES([Makefile test/Makefile])
AC_OUTPUT
