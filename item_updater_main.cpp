#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>
#include "config.h"
#include "item_updater.hpp"
#include <phosphor-logging/log.hpp>
#include "watch.hpp"

int main(int argc, char* argv[])
{
    using namespace openpower::software::updater;
    auto bus = sdbusplus::bus::new_default();

    sd_event* loop = nullptr;
    sd_event_default(&loop);

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
        sd_event_loop(loop);
    }
    catch (std::exception& e)
    {
        using namespace phosphor::logging;
        log<level::ERR>(e.what());
        return -1;
    }

    sd_event_unref(loop);

    return 0;
}
