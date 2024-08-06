#include "config.h"

#ifdef UBIFS_LAYOUT
#include "ubi/item_updater_ubi.hpp"
#include "ubi/watch.hpp"
#elif defined MMC_LAYOUT
#include "mmc/item_updater_mmc.hpp"
#else
#include "static/item_updater_static.hpp"
#endif
#include "functions.hpp"

#include <CLI/CLI.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>
#include <sdeventplus/event.hpp>

#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace openpower
{
namespace software
{
namespace updater
{
void initializeService(sdbusplus::bus_t& bus)
{
    static sdbusplus::server::manager_t objManager(bus, SOFTWARE_OBJPATH);
#ifdef UBIFS_LAYOUT
    static ItemUpdaterUbi updater(bus, SOFTWARE_OBJPATH);
    static Watch watch(
        bus.get_event(),
        std::bind(std::mem_fn(&ItemUpdater::updateFunctionalAssociation),
                  &updater, std::placeholders::_1));
#elif defined MMC_LAYOUT
    static ItemUpdaterMMC updater(bus, SOFTWARE_OBJPATH);
#else
    static ItemUpdaterStatic updater(bus, SOFTWARE_OBJPATH);
#endif
    bus.request_name(BUSNAME_UPDATER);
}
} // namespace updater
} // namespace software
} // namespace openpower

int main(int argc, char* argv[])
{
    using namespace openpower::software::updater;
    using namespace phosphor::logging;
    auto bus = sdbusplus::bus::new_default();
    auto loop = sdeventplus::Event::get_default();

    bus.attach_event(loop.get(), SD_EVENT_PRIORITY_NORMAL);

    CLI::App app{"OpenPOWER host firmware manager"};

    using namespace std::string_literals;
    std::map<std::string, std::vector<std::string>> extensionMap{{
        {"com.ibm.Hardware.Chassis.Model.BlueRidge2U"s,
         {".BLUERIDGE_2U_XML"s, ".P10"s}},
        {"com.ibm.Hardware.Chassis.Model.BlueRidge4U"s,
         {".BLUERIDGE_4U_XML"s, ".P10"s}},
        {"com.ibm.Hardware.Chassis.Model.BlueRidge1S4U"s,
         {".BLUERIDGE_4U_XML"s, ".P10"s}},
        {"com.ibm.Hardware.Chassis.Model.Bonnell"s, {".BONNELL_XML"s, ".P10"s}},
        {"com.ibm.Hardware.Chassis.Model.Everest"s, {".EVEREST_XML"s, ".P10"s}},
        {"com.ibm.Hardware.Chassis.Model.Fuji"s, {".FUJI_XML"s, ".P10"s}},
        {"com.ibm.Hardware.Chassis.Model.Rainier2U"s,
         {".RAINIER_2U_XML"s, ".P10"s}},
        {"com.ibm.Hardware.Chassis.Model.Rainier4U"s,
         {".RAINIER_4U_XML"s, ".P10"s}},
        {"com.ibm.Hardware.Chassis.Model.Rainier1S4U"s,
         {".RAINIER_4U_XML"s, ".P10"s}},
    }};

    // subcommandContext allows program subcommand callbacks to add loop event
    // callbacks (e.g. reception of a dbus signal) and then return to main,
    // without the loop event callback being destroyed until the loop event
    // callback has a chance to run and instruct the loop to exit.
    std::vector<std::shared_ptr<void>> subcommandContext;
    static_cast<void>(
        app.add_subcommand("process-host-firmware",
                           "Point the host firmware at its data.")
            ->callback([&bus, &loop, &subcommandContext, extensionMap]() {
        auto hostFirmwareDirectory = "/media/hostfw/running"s;
        auto logCallback = [](const auto& path, auto& ec) {
            std::cerr << path << ": " << ec.message() << "\n";
        };
        subcommandContext.push_back(
            functions::process_hostfirmware::processHostFirmware(
                bus, extensionMap, std::move(hostFirmwareDirectory),
                std::move(logCallback), loop));
    }));
    static_cast<void>(
        app.add_subcommand("update-bios-attr-table",
                           "Update the bios attribute table with the host "
                           "firmware data details.")
            ->callback([&bus, &loop, &subcommandContext, extensionMap]() {
        auto elementsJsonFilePath = "/usr/share/hostfw/elements.json"s;
        auto subcommands = functions::process_hostfirmware::updateBiosAttrTable(
            bus, extensionMap, std::move(elementsJsonFilePath), loop);
        for (const auto& subcommand : subcommands)
        {
            subcommandContext.push_back(subcommand);
        }
    }));

    CLI11_PARSE(app, argc, argv);

    if (app.get_subcommands().size() == 0)
    {
        initializeService(bus);
    }

    try
    {
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
