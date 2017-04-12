AC_PREREQ([2.69])
AC_INIT([openpower-pnor-code-mgmt], [1.0], [https://github.com/openbmc/openpower-pnor-code-mgmt/issues])
AC_LANG([C++])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign dist-xz])
AM_SILENT_RULES([yes])

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

AC_ARG_VAR(VERSION_BUSNAME, [The Dbus busname to own])
AS_IF([test "x$VERSION_BUSNAME" == "x"],
    [VERSION_BUSNAME="org.open_power.Software.Host.Version"])
AC_DEFINE_UNQUOTED([VERSION_BUSNAME], ["$VERSION_BUSNAME"], [The DBus busname to own])

AC_ARG_VAR(SOFTWARE_OBJPATH, [The software manager Dbus root])
AS_IF([test "x$SOFTWARE_OBJPATH" == "x"], [SOFTWARE_OBJPATH="/xyz/openbmc_project/software"])
AC_DEFINE_UNQUOTED([SOFTWARE_OBJPATH], ["$SOFTWARE_OBJPATH"], [The software manager Dbus root])

AC_DEFINE(VERSION_IFACE, "xyz.openbmc_project.Software.Version",
    [The software version manager interface])

AC_DEFINE(BUSNAME_UPDATER, "org.open_power.Software.Host.Updater",
    [The item updater DBus busname to own.])

AC_ARG_VAR(MANIFEST_FILE, [The path to the MANIFEST file])
AS_IF([test "x$MANIFEST_FILE" == "x"], [MANIFEST_FILE="/tmp/images/MANIFEST"])
AC_DEFINE_UNQUOTED([MANIFEST_FILE], ["$MANIFEST_FILE"], [The path to the MANIFEST file])

AC_ARG_VAR(TARBALLE, [The path to the TARBALL file])
AS_IF([test "x$TARBALL" == "x"], [TARBALL="/tmp/images/pnor.squashfs.tar"])
AC_DEFINE_UNQUOTED([TARBALL], ["$TARBALL"], [The path to the TARBALL file])

AC_DEFINE(MAPPER_BUSNAME, "xyz.openbmc_project.ObjectMapper",
    [The object mapper busname.])
AC_DEFINE(MAPPER_PATH, "/xyz/openbmc_project/object_mapper",
    [The object mapper path.])
AC_DEFINE(MAPPER_INTERFACE, "xyz.openbmc_project.ObjectMapper",
    [The object mapper interface.])

AC_CONFIG_FILES([Makefile test/Makefile])
AC_OUTPUT

