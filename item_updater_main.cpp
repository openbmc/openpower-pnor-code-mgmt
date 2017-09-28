#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>
#include <system_error>
#include "config.h"
#include "item_updater.hpp"
#include <phosphor-logging/log.hpp>
#include "watch.hpp"

int main(int argc, char* argv[])
{
    using namespace openpower::software::updater;
    using namespace phosphor::logging;
    auto bus = sdbusplus::bus::new_default();

    sd_event* loop = nullptr;
    auto rc = sd_event_default(&loop);
    if (rc < 0)
    {
        log<level::ERR>("Error occurred during the sd_event_default",
                        entry("RC=%d", rc));
        return -1;
    }

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager::manager objManager(bus, SOFTWARE_OBJPATH);

    ItemUpdater updater(bus, SOFTWARE_OBJPATH);

    bus.request_name(BUSNAME_UPDATER);
    try
    {
        openpower::software::updater::Watch watch(
                loop,
                std::bind(std::mem_fn(
                                  &ItemUpdater::updateFunctionalAssociation),
                          &updater,
                          std::placeholders::_1));
        bus.attach_event(loop, SD_EVENT_PRIORITY_NORMAL);
        auto rc = sd_event_loop(loop);
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

    sd_event_unref(loop);

    return 0;
}
