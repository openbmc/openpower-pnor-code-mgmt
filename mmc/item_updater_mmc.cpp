#include "config.h"

#include "item_updater_mmc.hpp"

#include "activation_mmc.hpp"
#include "version.hpp"

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

bool ItemUpdaterMMC::validateImage(const std::string& path)
{
    return true;
}

void ItemUpdaterMMC::processPNORImage()
{
}

void ItemUpdaterMMC::reset()
{
}

bool ItemUpdaterMMC::isVersionFunctional(const std::string& versionId)
{
    return versionId == functionalVersionId;
}

void ItemUpdaterMMC::freePriority(uint8_t value, const std::string& versionId)
{
}

void ItemUpdaterMMC::deleteAll()
{
}

bool ItemUpdaterMMC::freeSpace()
{
    return true;
}

void ItemUpdaterMMC::updateFunctionalAssociation(const std::string& versionId)
{
}

void GardResetMMC::reset()
{
}

} // namespace updater
} // namespace software
} // namespace openpower
