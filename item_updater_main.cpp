#include "config.h"

#ifdef UBIFS_LAYOUT
#include "ubi/item_updater_ubi.hpp"
#include "ubi/watch.hpp"
#elif defined MMC_LAYOUT
#include "mmc/item_updater_mmc.hpp"
#else
#include "static/item_updater_static.hpp"
#endif

#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>
#include <sdeventplus/event.hpp>

#include <system_error>

int main(int, char*[])
{
    using namespace openpower::software::updater;
    using namespace phosphor::logging;
    auto bus = sdbusplus::bus::new_default();
    auto loop = sdeventplus::Event::get_default();

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager::manager objManager(bus, SOFTWARE_OBJPATH);

#ifdef UBIFS_LAYOUT
    ItemUpdaterUbi updater(bus, SOFTWARE_OBJPATH);
#elif defined MMC_LAYOUT
    ItemUpdaterMMC updater(bus, SOFTWARE_OBJPATH);
#else
    ItemUpdaterStatic updater(bus, SOFTWARE_OBJPATH);
#endif

    bus.request_name(BUSNAME_UPDATER);
    try
    {
#ifdef UBIFS_LAYOUT
        openpower::software::updater::Watch watch(
            loop.get(),
            std::bind(std::mem_fn(&ItemUpdater::updateFunctionalAssociation),
                      &updater, std::placeholders::_1));
#endif
        bus.attach_event(loop.get(), SD_EVENT_PRIORITY_NORMAL);
        auto rc = loop.loop();
        if (rc < 0)
        {
            log<level::ERR>("Error occurred during the sd_event_loop",
                            entry("RC=%d", rc));
            return -1;
        }
    }
    catch (const std::system_error& e)
    {
        log<level::ERR>(e.what());
        return -1;
    }

    return 0;
}
