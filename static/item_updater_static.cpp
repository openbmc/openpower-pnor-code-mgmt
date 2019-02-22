#include "config.h"

#include "item_updater_static.hpp"

#include "activation_static.hpp"
#include "version.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <string>

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;
namespace fs = std::filesystem;

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
    // A signed version partition will have an extra 4K header starting with
    // the magic number 17082011 in big endian:
    // https://github.com/open-power/skiboot/blob/master/libstb/container.h#L47

    constexpr uint8_t MAGIC[] = {0x17, 0x08, 0x20, 0x11};
    constexpr auto MAGIC_SIZE = sizeof(MAGIC);
    static_assert(MAGIC_SIZE == 4);

    auto tmp = fs::temp_directory_path();
    std::string tmpDir(tmp / "versionXXXXXX");
    if (!mkdtemp(tmpDir.data()))
    {
        log<level::ERR>("Failed to create temp dir");
        return {};
    }

    fs::path versionFile = tmpDir;
    versionFile /= "version";

    pflash("-P VERSION -r", versionFile.string(), "2>&1 > /dev/null");
    std::ifstream f(versionFile.c_str(), std::ios::in | std::ios::binary);
    uint8_t magic[MAGIC_SIZE];
    std::string version;

    f.read(reinterpret_cast<char*>(magic), MAGIC_SIZE);
    f.seekg(0, std::ios::beg);
    if (std::memcmp(magic, MAGIC, MAGIC_SIZE) == 0)
    {
        // Skip the first 4K header
        f.ignore(4096);
    }

    getline(f, version, '\0');
    f.close();

    // Clear the temp dir
    std::error_code ec;
    fs::remove_all(tmpDir, ec);
    if (ec)
    {
        log<level::ERR>("Failed to remove temp dir",
                        entry("DIR=%s", tmpDir.c_str()),
                        entry("ERR=%s", ec.message().c_str()));
    }

    return version;
}

} // namespace utils

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
    return std::make_unique<ActivationStatic>(
        bus, path, *this, versionId, extVersion, activationStatus, assocs);
}

std::unique_ptr<Version> ItemUpdaterStatic::createVersionObject(
    const std::string& objPath, const std::string& versionId,
    const std::string& versionString,
    sdbusplus::xyz::openbmc_project::Software::server::Version::VersionPurpose
        versionPurpose,
    const std::string& filePath)
{
    auto version = std::make_unique<Version>(
        bus, objPath, *this, versionId, versionString, versionPurpose, filePath,
        std::bind(&ItemUpdaterStatic::erase, this, std::placeholders::_1));
    version->deleteObject = std::make_unique<Delete>(bus, objPath, *version);
    return version;
}

bool ItemUpdaterStatic::validateImage(const std::string& path)
{
    // There is no need to validate static layout pnor
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

    // Create Activation instance for this version.
    activations.insert(std::make_pair(
        id, std::make_unique<ActivationStatic>(bus, path, *this, id,
                                               extendedVersion, activationState,
                                               associations)));

    // If Active, create RedundancyPriority instance for this version.
    if (activationState == server::Activation::Activations::Active)
    {
        // For now only one PNOR is supported with static layout
        activations.find(id)->second->redundancyPriority =
            std::make_unique<RedundancyPriority>(
                bus, path, *(activations.find(id)->second), 0);
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
