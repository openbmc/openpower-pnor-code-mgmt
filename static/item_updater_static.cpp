#include "config.h"

#include "item_updater_static.hpp"

#include "activation.hpp"
#include "version.hpp"

#include <filesystem>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <string>

namespace utils
{

template <typename... Ts>
std::string concat_string(Ts const&... ts)
{
    std::stringstream s;
    ((s << ts << " "), ...) << std::endl;
    return s.str();
}

// Helper function to run pflash command
template <typename... Ts>
std::string pflash(Ts const&... ts)
{
    std::array<char, 512> buffer;
    std::string cmd = concat_string("pflash", ts...);
    std::stringstream result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                  pclose);
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    {
        result << buffer.data();
    }
    return result.str();
}

std::string getPNORVersion()
{
    // Read the VERSION partition skipping the first 4K
    auto r = pflash("-P", "VERSION", "-r", "/dev/stderr", "--skip=4096",
                    "2>&1 > /dev/null");
    return r;
}

} // namespace utils

namespace openpower
{
namespace software
{
namespace updater
{
// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;
namespace fs = std::filesystem;

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;

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
    auto fullVersion = utils::getPNORVersion();

    const auto& [version, extendedVersion] = Version::getVersions(fullVersion);
    auto id = Version::getId(version);

    auto activationState = server::Activation::Activations::Active;
    if (version.empty())
    {
        log<level::ERR>("Failed to read version",
                        entry("VERSION=%s", fullVersion.c_str()));
        activationState = server::Activation::Activations::Invalid;
    }

    if (extendedVersion.empty())
    {
        log<level::ERR>("Failed to read extendedVersion",
                        entry("VERSION=%s", fullVersion.c_str()));
        activationState = server::Activation::Activations::Invalid;
    }

    auto purpose = server::Version::VersionPurpose::Host;
    auto path = fs::path(SOFTWARE_OBJPATH) / id;
    AssociationList associations = {};

    if (activationState == server::Activation::Activations::Active)
    {
        // Create an association to the host inventory item
        associations.emplace_back(std::make_tuple(ACTIVATION_FWD_ASSOCIATION,
                                                  ACTIVATION_REV_ASSOCIATION,
                                                  HOST_INVENTORY_PATH));

        // Create an active association since this image is active
        createActiveAssociation(path);
    }

    // Create Version instance for this version.
    auto versionPtr = std::make_unique<Version>(
        bus, path, *this, id, version, purpose, "",
        std::bind(&ItemUpdaterStatic::erase, this, std::placeholders::_1));
    versionPtr->deleteObject = std::make_unique<Delete>(bus, path, *versionPtr);
    versions.insert(std::make_pair(id, std::move(versionPtr)));

    if (!id.empty())
    {
        updateFunctionalAssociation(id);
    }
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
