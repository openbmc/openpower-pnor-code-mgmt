#include "config.h"

#include "item_updater_static.hpp"

#include "activation_static.hpp"
#include "pnor_utils.hpp"
#include "version.hpp"

#include <array>
#include <filesystem>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <string>
#include <tuple>
#include <xyz/openbmc_project/Common/error.hpp>

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
using sdbusplus::exception::SdBusError;

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
    // The pair contains the partition name and if it should use ECC clear
    using PartClear = std::pair<const char*, bool>;
    constexpr std::array<PartClear, 11> partitions = {{
        {"HBEL", true},
        {"GUARD", true},
        {"NVRAM", false},
        {"DJVPD", true},
        {"MVPD", true},
        {"CVPD", true},
        {"FIRDATA", true},
        {"BMC_INV", false},
        {"ATTR_TMP", false},
        {"ATTR_PERM", true},
        {"HB_VOLATILE", true},
    }};

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

void ItemUpdaterStatic::freeSpace()
{
    // For now assume static layout only has 1 active PNOR,
    // so erase the active PNOR
    for (const auto& iter : activations)
    {
        if (iter.second.get()->activation() ==
            server::Activation::Activations::Active)
        {
            erase(iter.second->versionId);
            break;
        }
    }
}

void ItemUpdaterStatic::updateFunctionalAssociation(const std::string& id)
{
    functionalVersionId = id;
    ItemUpdater::updateFunctionalAssociation(id);
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
