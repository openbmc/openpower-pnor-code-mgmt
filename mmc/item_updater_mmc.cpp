#include "config.h"

#include "item_updater_mmc.hpp"

#include "activation_mmc.hpp"
#include "utils.hpp"
#include "version.hpp"

#include <filesystem>
#include <iostream>
#include <thread>

namespace openpower
{
namespace software
{
namespace updater
{

// These functions are just a stub (empty) because the current eMMC
// implementation uses the BMC updater (repo phosphor-bmc-code-mgmt) to write
// the new host FW to flash since it's delivered as a "System" image in the
// same BMC tarball as the BMC image.

std::unique_ptr<Activation> ItemUpdaterMMC::createActivationObject(
    const std::string& path, const std::string& versionId,
    const std::string& extVersion,
    sdbusplus::xyz::openbmc_project::Software::server::Activation::Activations
        activationStatus,
    AssociationList& assocs)
{
    return std::make_unique<ActivationMMC>(
        bus, path, *this, versionId, extVersion, activationStatus, assocs);
}

std::unique_ptr<Version> ItemUpdaterMMC::createVersionObject(
    const std::string& objPath, const std::string& versionId,
    const std::string& versionString,
    sdbusplus::xyz::openbmc_project::Software::server::Version::VersionPurpose
        versionPurpose,
    const std::string& filePath)
{
    auto version = std::make_unique<Version>(
        bus, objPath, *this, versionId, versionString, versionPurpose, filePath,
        std::bind(&ItemUpdaterMMC::erase, this, std::placeholders::_1));
    version->deleteObject = std::make_unique<Delete>(bus, objPath, *version);
    return version;
}

bool ItemUpdaterMMC::validateImage(const std::string&)
{
    return true;
}

void ItemUpdaterMMC::processPNORImage()
{}

void ItemUpdaterMMC::reset()
{
    // Do not reset read-only files needed for reset or ext4 default files
    const std::vector<std::string> exclusionList = {"alternate", "hostfw-a",
                                                    "hostfw-b",  "lost+found",
                                                    "nvram",     "running-ro"};
    std::filesystem::path dirPath(std::string(MEDIA_DIR "hostfw/"));
    // Delete all files in /media/hostfw/ except for those on exclusionList
    for (const auto& p : std::filesystem::directory_iterator(dirPath))
    {
        if (std::find(exclusionList.begin(), exclusionList.end(),
                      p.path().stem().string()) == exclusionList.end())
        {
            std::filesystem::remove_all(p);
        }
    }

    // Delete all BMC error logs to avoid discrepancies with the host error logs
    utils::deleteAllErrorLogs(bus);

    // Set attribute to clear hypervisor NVRAM
    utils::setClearNvram(bus);

    // Remove files related to the Hardware Management Console / BMC web app
    utils::clearHMCManaged(bus);
    std::filesystem::path consolePath("/var/lib/bmcweb/ibm-management-console");
    if (std::filesystem::exists(consolePath))
    {
        std::filesystem::remove_all(consolePath);
    }
    std::filesystem::path bmcdataPath("/home/root/bmcweb_persistent_data.json");
    if (std::filesystem::exists(bmcdataPath))
    {
        std::filesystem::remove(bmcdataPath);
    }

    // Recreate default files.
    // std::tuple<method, service_name>
    const std::tuple<std::string, std::string> services[] = {
        {"StartUnit", "obmc-flash-bios-init.service"},
        {"StartUnit", "obmc-flash-bios-patch.service"},
        {"StartUnit", "openpower-process-host-firmware.service"},
        {"StartUnit", "openpower-update-bios-attr-table.service"},
        {"RestartUnit", "org.open_power.HardwareIsolation.service"}};

    for (const auto& service : services)
    {
        auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                          SYSTEMD_INTERFACE,
                                          std::get<0>(service).c_str());
        method.append(std::get<1>(service), "replace");
        // Ignore errors if the service is not found - not all systems
        // may have these services
        try
        {
            bus.call_noreply(method);
        }
        catch (const std::exception& e)
        {}
    }

    // Wait a few seconds for the service files and reset operations to finish,
    // otherwise the BMC may be rebooted and cause corruption.
    constexpr auto resetWait = std::chrono::seconds(5);
    std::this_thread::sleep_for(resetWait);
}

bool ItemUpdaterMMC::isVersionFunctional(const std::string& versionId)
{
    return versionId == functionalVersionId;
}

void ItemUpdaterMMC::freePriority(uint8_t, const std::string&)
{}

void ItemUpdaterMMC::deleteAll()
{}

bool ItemUpdaterMMC::freeSpace()
{
    return true;
}

void ItemUpdaterMMC::updateFunctionalAssociation(const std::string&)
{}

void GardResetMMC::reset()
{}

} // namespace updater
} // namespace software
} // namespace openpower
