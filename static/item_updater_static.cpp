#include "config.h"

#include "item_updater_static.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

std::unique_ptr<Activation> ItemUpdaterStatic::createActivationObject(
    const std::string& path, const std::string& versionId,
    const std::string& extVersion,
    sdbusplus::xyz::openbmc_project::Software::server::Activation::Activations
        activationStatus,
    AssociationList& assocs)
{
    return {};
}

std::unique_ptr<Version> ItemUpdaterStatic::createVersionObject(
    const std::string& objPath, const std::string& versionId,
    const std::string& versionString,
    sdbusplus::xyz::openbmc_project::Software::server::Version::VersionPurpose
        versionPurpose,
    const std::string& filePath)
{
    return {};
}

bool ItemUpdaterStatic::validateImage(const std::string& path)
{
    return true;
}

void ItemUpdaterStatic::processPNORImage()
{
}

void ItemUpdaterStatic::reset()
{
}

bool ItemUpdaterStatic::isVersionFunctional(const std::string& versionId)
{
    return true;
}

void ItemUpdaterStatic::freePriority(uint8_t value,
                                     const std::string& versionId)
{
}

void ItemUpdaterStatic::deleteAll()
{
}

void ItemUpdaterStatic::freeSpace()
{
}

void GardReset::reset()
{
}

} // namespace updater
} // namespace software
} // namespace openpower
