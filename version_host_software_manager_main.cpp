#include <sdbusplus/bus.hpp>
#include <string>
#include "config.h"
#include "version_host_software_manager.hpp"

int main(int argc, char* argv[])
{
    auto bus = sdbusplus::bus::new_default();

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager::manager objManager(bus,
            SOFTWARE_OBJPATH);

    auto version = openpower::software::manager::Version::getVersion(MANIFEST_FILE);
    auto id = openpower::software::manager::Version::getId(version);

    openpower::software::manager::Version manager(bus,
            std::string{SOFTWARE_OBJPATH} + '/' + id, version);

    bus.request_name(VERSION_BUSNAME);

    while (true)
    {
        bus.process_discard();
        bus.wait();
    }
    return 0;
}
