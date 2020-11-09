project(
    'openpower-pnor-code-mgmt',
    'cpp',
    default_options: [
        'warning_level=3',
        'werror=true',
        'cpp_std=c++17',
        'buildtype=debugoptimized',
        'b_ndebug=if-release',
        'b_lto=true',
    ],
    license: 'Apache-2.0',
    version: '1.0',
)

if get_option('cpp_std') != 'c++17'
    error('This project requires c++17')
endif

if(get_option('buildtype') == 'minsize')
    add_project_arguments('-DNDEBUG', language : 'cpp')
endif

# Disable lto when compiling with no optimization
if(get_option('optimization') == '0')
    add_project_arguments('-fno-lto', language: 'cpp')
    message('Disabling lto because optimization is disabled')
endif

extra_sources = []
extra_unit_files = []
extra_scripts = []

build_vpnor = get_option('vpnor').enabled()
build_verify_signature = get_option('verify-signature').enabled()

summary('building for device type', '@0@'.format(get_option('device-type')))
summary('building vpnor', build_vpnor)
summary('building signature verify', build_verify_signature)

subs = configuration_data()
subs.set_quoted('ACTIVATION_FWD_ASSOCIATION', 'inventory')
subs.set_quoted('ACTIVATION_REV_ASSOCIATION', 'activation')
subs.set_quoted('ACTIVE_FWD_ASSOCIATION', 'active')
subs.set('ACTIVE_PNOR_MAX_ALLOWED', 2)
subs.set_quoted('ACTIVE_REV_ASSOCIATION', 'software_version')
subs.set_quoted('ASSOCIATIONS_INTERFACE', 'xyz.openbmc_project.Association.Definitions')
subs.set_quoted('BUSNAME_UPDATER', 'xyz.openbmc_project.Software.Host.Updater')
subs.set_quoted('CHASSIS_STATE_OBJ', 'xyz.openbmc_project.State.Chassis')
subs.set_quoted('CHASSIS_STATE_OFF', 'xyz.openbmc_project.State.Chassis.PowerState.Off')
subs.set_quoted('CHASSIS_STATE_PATH', '/xyz/openbmc_project/state/chassis0')
subs.set_quoted('FILEPATH_IFACE', 'xyz.openbmc_project.Common.FilePath')
subs.set_quoted('FUNCTIONAL_FWD_ASSOCIATION', 'functional')
subs.set_quoted('FUNCTIONAL_REV_ASSOCIATION', 'software_version')
subs.set_quoted('HASH_FILE_NAME', 'hashfunc')
subs.set_quoted('HOST_INVENTORY_PATH', '/xyz/openbmc_project/inventory/system/chassis')
subs.set_quoted('IMG_DIR', '/tmp/images')
subs.set_quoted('MANIFEST_FILE', 'MANIFEST')
subs.set_quoted('MAPPER_BUSNAME', 'xyz.openbmc_project.ObjectMapper')
subs.set_quoted('MAPPER_INTERFACE', 'xyz.openbmc_project.ObjectMapper')
subs.set_quoted('MAPPER_PATH', '/xyz/openbmc_project/object_mapper')
subs.set_quoted('MEDIA_DIR', '/media/')
subs.set('MMC_LAYOUT', get_option('device-type') == 'mmc')
subs.set_quoted('PERSIST_DIR', '/var/lib/obmc/openpower-pnor-code-mgmt/')
subs.set_quoted('PNOR_ACTIVE_PATH', '/var/lib/phosphor-software-manager/pnor/')
subs.set_quoted('PNOR_MSL', get_option('msl'))
subs.set_quoted('PNOR_PRSV', '/media/pnor-prsv')
subs.set_quoted('PNOR_PRSV_ACTIVE_PATH', '/var/lib/phosphor-software-manager/pnor/prsv')
subs.set_quoted('PNOR_RO_ACTIVE_PATH', '/var/lib/phosphor-software-manager/pnor/ro')
subs.set_quoted('PNOR_RO_PREFIX', '/media/pnor-ro-')
subs.set_quoted('PNOR_RW_ACTIVE_PATH', '/var/lib/phosphor-software-manager/pnor/rw')
subs.set_quoted('PNOR_RW_PREFIX', '/media/pnor-rw-')
subs.set_quoted('PNOR_SIGNED_IMAGE_CONF_PATH', '/etc/activationdata/')
subs.set_quoted('PNOR_TOC_FILE', 'pnor.toc')
subs.set_quoted('PNOR_VERSION_PARTITION', 'VERSION')
subs.set_quoted('PUBLICKEY_FILE_NAME', 'publickey')
subs.set_quoted('SIGNATURE_FILE_EXT', '.sig')
subs.set_quoted('SOFTWARE_OBJPATH', '/xyz/openbmc_project/software')
subs.set_quoted('SYSTEMD_BUSNAME', 'org.freedesktop.systemd1')
subs.set_quoted('SYSTEMD_INTERFACE', 'org.freedesktop.systemd1.Manager')
subs.set_quoted('SYSTEMD_PATH', '/org/freedesktop/systemd1')
subs.set_quoted('SYSTEMD_PROPERTY_INTERFACE', 'org.freedesktop.DBus.Properties')
subs.set('UBIFS_LAYOUT', get_option('device-type') == 'ubi')
subs.set_quoted('UPDATEABLE_FWD_ASSOCIATION', 'updateable')
subs.set_quoted('UPDATEABLE_REV_ASSOCIATION', 'software_version')
subs.set_quoted('VERSION_IFACE', 'xyz.openbmc_project.Software.Version')
subs.set('WANT_SIGNATURE_VERIFY', build_verify_signature)
configure_file(
    output: 'config.h',
    configuration: subs)

if get_option('device-type') == 'ubi'
    extra_sources += [
        'ubi/activation_ubi.cpp',
        'ubi/item_updater_ubi.cpp',
        'ubi/serialize.cpp',
        'ubi/watch.cpp',
    ]
    extra_scripts += [
        'ubi/obmc-flash-bios',
    ]
    extra_unit_files += [
        'ubi/obmc-flash-bios-cleanup.service',
        'ubi/obmc-flash-bios-ubiattach.service',
        'ubi/obmc-flash-bios-ubimount@.service',
        'ubi/obmc-flash-bios-ubipatch.service',
        'ubi/obmc-flash-bios-ubiremount.service',
        'ubi/obmc-flash-bios-ubiumount-ro@.service',
        'ubi/obmc-flash-bios-ubiumount-rw@.service',
    ]
endif

if get_option('device-type') == 'mmc'
    extra_sources += [
        'mmc/activation_mmc.cpp',
        'mmc/item_updater_mmc.cpp',
    ]
    extra_scripts += [
        'mmc/obmc-flash-bios',
    ]
    extra_unit_files += [
        'mmc/obmc-flash-bios-init.service',
        'mmc/obmc-flash-bios-patch.service',
    ]
endif

if get_option('device-type') == 'static'
    extra_sources += [
        'static/item_updater_static.cpp',
        'static/activation_static.cpp',
    ]
    extra_unit_files += [
        'openpower-pnor-update@.service',
    ]
endif

if build_verify_signature
    extra_sources += [
        'image_verify.cpp'
    ]
endif

if build_vpnor
    extra_scripts += [
        'vpnor/obmc-vpnor-util',
    ]
    extra_unit_files += [
        'vpnor/obmc-vpnor-check-clearvolatile@.service',
        'vpnor/obmc-vpnor-enable-clearvolatile@.service',
        'vpnor/obmc-vpnor-updatesymlinks.service',
    ]
endif

executable(
    'openpower-update-manager',
    [
        'activation.cpp',
        'version.cpp',
        'item_updater.cpp',
        'item_updater_main.cpp',
        'utils.cpp',
    ] + extra_sources,
    dependencies: [
        dependency('libcrypto'),
        dependency('libsystemd'),
        dependency('openssl'),
        dependency('phosphor-dbus-interfaces'),
        dependency('phosphor-logging'),
        dependency('sdbusplus'),
        dependency('sdeventplus'),
    ],
    install: true
)

executable(
    'openpower-pnor-msl',
    [
        'msl_verify.cpp',
        'msl_verify_main.cpp',
    ],
    dependencies: [
        dependency('libsystemd'),
        dependency('phosphor-dbus-interfaces'),
        dependency('phosphor-logging'),
        dependency('sdbusplus'),
    ],
    install: true
)

foreach s : extra_scripts
    configure_file(
        input: s,
        copy: true,
        install: true,
        install_mode: 'rwxr-xr-x',
        install_dir: get_option('bindir'),
        output: '@BASENAME@'
    )
endforeach

unit_files = [
    'op-pnor-msl.service',
    'org.open_power.Software.Host.Updater.service',
] + extra_unit_files

systemd_system_unit_dir = dependency('systemd').get_pkgconfig_variable(
    'systemdsystemunitdir',
    define_variable: ['prefix', get_option('prefix')])
foreach u : unit_files
    configure_file(
        input: u,
        copy: true,
        install: true,
        install_dir: systemd_system_unit_dir,
        output: '@BASENAME@.service'
    )
endforeach

if not get_option('tests').disabled()
    test(
        'utest',
        executable(
            'utest',
            'activation.cpp',
            'version.cpp',
            'item_updater.cpp',
            'image_verify.cpp',
            'utils.cpp',
            'msl_verify.cpp',
            'ubi/activation_ubi.cpp',
            'ubi/item_updater_ubi.cpp',
            'ubi/serialize.cpp',
            'ubi/watch.cpp',
            'static/item_updater_static.cpp',
            'static/activation_static.cpp',
            'test/test_signature.cpp',
            'test/test_version.cpp',
            'test/test_item_updater_static.cpp',
            'msl_verify.cpp',
            dependencies: [
                dependency('libcrypto'),
                dependency('gtest', main: true),
                dependency('openssl'),
                dependency('phosphor-logging'),
                dependency('phosphor-dbus-interfaces'),
            ],
            implicit_include_directories: false,
            include_directories: '.',
        )
    )
endif