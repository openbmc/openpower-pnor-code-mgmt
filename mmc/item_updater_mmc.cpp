#include "config.h"

#include "item_updater_mmc.hpp"

#include "activation_mmc.hpp"
#include "version.hpp"

#include <filesystem>
#include <iostream>

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
    const std::vector<std::string> exclusionList = {
        "alternate", "hostfw-a", "hostfw-b", "lost+found", "running-ro"};
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

    // Remove files related to the Hardware Management Console / BMC web app
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
    const std::string services[6] = {"obmc-flash-bios-init.service",
                                     "obmc-flash-bios-patch.service",
                                     "openpower-process-host-firmware.service",
                                     "openpower-update-bios-attr-table.service",
                                     "pldm-reset-phyp-nvram.service",
                                     "pldm-reset-phyp-nvram-cksum.service"};

    auto bus = sdbusplus::bus::new_default();
    for (int i = 0; i < 6; i++)
    {
        const auto service = services[i];
        auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                          SYSTEMD_INTERFACE, "StartUnit");
        method.append(service, "replace");
        // Ignore errors if the service is not found - not all systems
        // may have these services
        try
        {
            bus.call_noreply(method);
        }
        catch (const std::exception& e)
        {}
    }
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
