#include "config.h"

#include "item_updater_static.hpp"

#include "activation_static.hpp"
#include "version.hpp"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <sstream>
#include <string>
#include <tuple>
#include <xyz/openbmc_project/Common/error.hpp>

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;
using sdbusplus::exception::SdBusError;

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
// Returns return code and the stdout
template <typename... Ts>
std::pair<int, std::string> pflash(Ts const&... ts)
{
    std::array<char, 512> buffer;
    std::string cmd = concat_string("pflash", ts...);
    std::stringstream result;
    int rc;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    {
        result << buffer.data();
    }
    rc = pclose(pipe);
    return {rc, result.str()};
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

    auto [rc, r] =
        pflash("-P VERSION -r", versionFile.string(), "2>&1 > /dev/null");
    if (rc != 0)
    {
        log<level::ERR>("Failed to read VERSION", entry("RETURNCODE=%d", rc));
        return {};
    }

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

void pnorClear(const std::string& part, bool shouldEcc = true)
{
    int rc;
    std::tie(rc, std::ignore) =
        utils::pflash("-P", part, shouldEcc ? "-c" : "-e", "-f >/dev/null");
    if (rc != 0)
    {
        log<level::ERR>("Failed to clear partition",
                        entry("PART=%s", part.c_str()),
                        entry("RETURNCODE=%d", rc));
    }
    else
    {
        log<level::INFO>("Clear partition successfully",
                         entry("PART=%s", part.c_str()));
    }
}

// The pair contains the partition name and if it should use ECC clear
using PartClear = std::pair<std::string, bool>;

std::vector<PartClear> getPartsToClear(const std::string& info)
{
    std::vector<PartClear> ret;
    std::istringstream iss(info);
    std::string line;

    while (std::getline(iss, line))
    {
        // Each line looks like
        // ID=06 MVPD 0x0012d000..0x001bd000 (actual=0x00090000) [E--P--F-C-]
        // Flag 'F' means REPROVISION
        // Flag 'C' means CLEARECC
        auto flags = line.substr(line.find('['));
        if (flags.find('F') != std::string::npos)
        {
            // This is a partition to be cleared
            line = line.substr(line.find_first_of(' '));     // Skiping "ID=xx"
            line = line.substr(line.find_first_not_of(' ')); // Skipping spaces
            line = line.substr(0, line.find_first_of(' '));  // The part name
            bool ecc = flags.find('C') != std::string::npos;
            ret.emplace_back(line, ecc);
        }
    }
    return ret;
}

// Get partitions that should be cleared
std::vector<PartClear> getPartsToClear()
{
    const auto& [rc, pflashInfo] = pflash("-i | grep ^ID | grep 'F'");
    return getPartsToClear(pflashInfo);
}

} // namespace utils

namespace openpower
{
namespace software
{
namespace updater
{
// TODO: Change paths once openbmc/openbmc#1663 is completed.
constexpr auto MBOXD_INTERFACE = "org.openbmc.mboxd";
constexpr auto MBOXD_PATH = "/org/openbmc/mboxd";

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
    auto partitions = utils::getPartsToClear();
    std::vector<uint8_t> mboxdArgs;

    // Suspend mboxd - no args required.
    auto dbusCall = bus.new_method_call(MBOXD_INTERFACE, MBOXD_PATH,
                                        MBOXD_INTERFACE, "cmd");
    dbusCall.append(static_cast<uint8_t>(3), mboxdArgs);

    try
    {
        bus.call_noreply(dbusCall);
    }
    catch (const SdBusError& e)
    {
        log<level::ERR>("Error in mboxd suspend call",
                        entry("ERROR=%s", e.what()));
        elog<InternalFailure>();
    }
    for (auto p : partitions)
    {
        utils::pnorClear(p.first, p.second);
    }

    // Resume mboxd with arg 1, indicating that the flash was modified.
    dbusCall = bus.new_method_call(MBOXD_INTERFACE, MBOXD_PATH, MBOXD_INTERFACE,
                                   "cmd");
    mboxdArgs.push_back(1);
    dbusCall.append(static_cast<uint8_t>(4), mboxdArgs);

    try
    {
        bus.call_noreply(dbusCall);
    }
    catch (const SdBusError& e)
    {
        log<level::ERR>("Error in mboxd resume call",
                        entry("ERROR=%s", e.what()));
        elog<InternalFailure>();
    }
}

bool ItemUpdaterStatic::isVersionFunctional(const std::string& versionId)
{
    return versionId == functionalVersionId;
}

void ItemUpdaterStatic::freePriority(uint8_t value,
                                     const std::string& versionId)
{
}

void ItemUpdaterStatic::deleteAll()
{
    // Static layout has only one active and function pnor
    // There is no implementation for this interface
}

bool ItemUpdaterStatic::freeSpace()
{
    // For now assume static layout only has 1 active PNOR,
    // so erase the active PNOR
    for (const auto& iter : activations)
    {
        if (iter.second.get()->activation() ==
            server::Activation::Activations::Active)
        {
            return erase(iter.second->versionId);
        }
    }
    return false;
}

void ItemUpdaterStatic::updateFunctionalAssociation(
    const std::string& versionId)
{
    functionalVersionId = versionId;
    ItemUpdater::updateFunctionalAssociation(versionId);
}

void GardReset::reset()
{
    // Clear gard partition
    std::vector<uint8_t> mboxdArgs;

    auto dbusCall = bus.new_method_call(MBOXD_INTERFACE, MBOXD_PATH,
                                        MBOXD_INTERFACE, "cmd");
    // Suspend mboxd - no args required.
    dbusCall.append(static_cast<uint8_t>(3), mboxdArgs);

    try
    {
        bus.call_noreply(dbusCall);
    }
    catch (const SdBusError& e)
    {
        log<level::ERR>("Error in mboxd suspend call",
                        entry("ERROR=%s", e.what()));
        elog<InternalFailure>();
    }

    // Clear guard partition
    utils::pnorClear("GUARD");

    dbusCall = bus.new_method_call(MBOXD_INTERFACE, MBOXD_PATH, MBOXD_INTERFACE,
                                   "cmd");
    // Resume mboxd with arg 1, indicating that the flash is modified.
    mboxdArgs.push_back(1);
    dbusCall.append(static_cast<uint8_t>(4), mboxdArgs);

    try
    {
        bus.call_noreply(dbusCall);
    }
    catch (const SdBusError& e)
    {
        log<level::ERR>("Error in mboxd resume call",
                        entry("ERROR=%s", e.what()));
        elog<InternalFailure>();
    }
}

} // namespace updater
} // namespace software
} // namespace openpower
