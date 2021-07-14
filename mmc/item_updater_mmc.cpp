#include "config.h"

#include "item_updater_mmc.hpp"

#include "activation_mmc.hpp"
#include "version.hpp"

#include <filesystem>

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
    const std::vector<std::string> exclusionList = {"hostfw-a", "hostfw-b",
                                                    "running-ro", "alternate"};
    std::error_code ec;
    std::string dirPath = "/media/hostfw/";
    // Delete all files in /media/hostfw/ except for those on exclusionList
    for (auto& p : std::filesystem::directory_iterator(dirPath))
    {
        std::string name(p.path(), dirPath.length(), std::string::npos);
        if (std::find(exclusionList.begin(), exclusionList.end(), name) ==
            exclusionList.end())
        {
            std::filesystem::remove_all(p, ec);
            if (ec)
            {
                int val = ec.value();
                fprintf(stderr, "Returned error code %n when clearing %s", &val,
                        name.c_str());
            }
        }
    }

    // Recreate default files
    auto bus = sdbusplus::bus::new_default();
    constexpr auto initService = "obmc-flash-bios-init.service";
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(initService, "replace");
    bus.call_noreply(method);

    constexpr auto patchService = "obmc-flash-bios-patch.service";
    method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                 SYSTEMD_INTERFACE, "StartUnit");
    method.append(patchService, "replace");
    bus.call_noreply(method);
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
