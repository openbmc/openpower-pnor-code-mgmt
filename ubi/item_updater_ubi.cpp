#include "config.h"

#include "item_updater_ubi.hpp"

#include "activation_ubi.hpp"
#include "serialize.hpp"
#include "utils.hpp"
#include "version.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>

#include <filesystem>
#include <fstream>
#include <queue>
#include <string>

namespace openpower
{
namespace software
{
namespace updater
{

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;

std::unique_ptr<Activation> ItemUpdaterUbi::createActivationObject(
    const std::string& path, const std::string& versionId,
    const std::string& extVersion,
    sdbusplus::xyz::openbmc_project::Software::server::Activation::Activations
        activationStatus,
    AssociationList& assocs)
{
    return std::make_unique<ActivationUbi>(
        bus, path, *this, versionId, extVersion, activationStatus, assocs);
}

std::unique_ptr<Version> ItemUpdaterUbi::createVersionObject(
    const std::string& objPath, const std::string& versionId,
    const std::string& versionString,
    sdbusplus::xyz::openbmc_project::Software::server::Version::VersionPurpose
        versionPurpose,
    const std::string& filePath)
{
    auto version = std::make_unique<Version>(
        bus, objPath, *this, versionId, versionString, versionPurpose, filePath,
        std::bind(&ItemUpdaterUbi::erase, this, std::placeholders::_1));
    version->deleteObject = std::make_unique<Delete>(bus, objPath, *version);
    return version;
}

bool ItemUpdaterUbi::validateImage(const std::string& path)
{
    return validateSquashFSImage(path) == 0;
}

void ItemUpdaterUbi::processPNORImage()
{
    // Read pnor.toc from folders under /media/
    // to get Active Software Versions.
    for (const auto& iter : std::filesystem::directory_iterator(MEDIA_DIR))
    {
        auto activationState = server::Activation::Activations::Active;

        static const auto PNOR_RO_PREFIX_LEN = strlen(PNOR_RO_PREFIX);
        static const auto PNOR_RW_PREFIX_LEN = strlen(PNOR_RW_PREFIX);

        // Check if the PNOR_RO_PREFIX is the prefix of the iter.path
        if (0 ==
            iter.path().native().compare(0, PNOR_RO_PREFIX_LEN, PNOR_RO_PREFIX))
        {
            // The versionId is extracted from the path
            // for example /media/pnor-ro-2a1022fe.
            auto id = iter.path().native().substr(PNOR_RO_PREFIX_LEN);
            auto pnorTOC = iter.path() / PNOR_TOC_FILE;
            if (!std::filesystem::is_regular_file(pnorTOC))
            {
                log<level::ERR>("Failed to read pnorTOC.",
                                entry("FILENAME=%s", pnorTOC.c_str()));
                ItemUpdaterUbi::erase(id);
                continue;
            }
            auto keyValues = Version::getValue(
                pnorTOC, {{"version", ""}, {"extended_version", ""}});
            auto& version = keyValues.at("version");
            if (version.empty())
            {
                log<level::ERR>("Failed to read version from pnorTOC",
                                entry("FILENAME=%s", pnorTOC.c_str()));
                activationState = server::Activation::Activations::Invalid;
            }

            auto& extendedVersion = keyValues.at("extended_version");
            if (extendedVersion.empty())
            {
                log<level::ERR>("Failed to read extendedVersion from pnorTOC",
                                entry("FILENAME=%s", pnorTOC.c_str()));
                activationState = server::Activation::Activations::Invalid;
            }

            auto purpose = server::Version::VersionPurpose::Host;
            auto path = std::filesystem::path(SOFTWARE_OBJPATH) / id;
            AssociationList associations = {};

            if (activationState == server::Activation::Activations::Active)
            {
                // Create an association to the host inventory item
                associations.emplace_back(std::make_tuple(
                    ACTIVATION_FWD_ASSOCIATION, ACTIVATION_REV_ASSOCIATION,
                    HOST_INVENTORY_PATH));

                // Create an active association since this image is active
                createActiveAssociation(path);
            }

            // All updateable firmware components must expose the updateable
            // association.
            createUpdateableAssociation(path);

            // Create Activation instance for this version.
            activations.insert(
                std::make_pair(id, std::make_unique<ActivationUbi>(
                                       bus, path, *this, id, extendedVersion,
                                       activationState, associations)));

            // If Active, create RedundancyPriority instance for this version.
            if (activationState == server::Activation::Activations::Active)
            {
                uint8_t priority = std::numeric_limits<uint8_t>::max();
                if (!restoreFromFile(id, priority))
                {
                    log<level::ERR>("Unable to restore priority from file.",
                                    entry("VERSIONID=%s", id.c_str()));
                }
                activations.find(id)->second->redundancyPriority =
                    std::make_unique<RedundancyPriorityUbi>(
                        bus, path, *(activations.find(id)->second), priority);
            }

            // Create Version instance for this version.
            auto versionPtr = std::make_unique<Version>(
                bus, path, *this, id, version, purpose, "",
                std::bind(&ItemUpdaterUbi::erase, this, std::placeholders::_1));
            versionPtr->deleteObject = std::make_unique<Delete>(bus, path,
                                                                *versionPtr);
            versions.insert(std::make_pair(id, std::move(versionPtr)));
        }
        else if (0 == iter.path().native().compare(0, PNOR_RW_PREFIX_LEN,
                                                   PNOR_RW_PREFIX))
        {
            auto id = iter.path().native().substr(PNOR_RW_PREFIX_LEN);
            auto roDir = PNOR_RO_PREFIX + id;
            if (!std::filesystem::is_directory(roDir))
            {
                log<level::ERR>("No corresponding read-only volume found.",
                                entry("DIRNAME=%s", roDir.c_str()));
                ItemUpdaterUbi::erase(id);
            }
        }
    }

    // Look at the RO symlink to determine if there is a functional image
    auto id = determineId(PNOR_RO_ACTIVE_PATH);
    if (!id.empty())
    {
        updateFunctionalAssociation(id);
    }
    return;
}

int ItemUpdaterUbi::validateSquashFSImage(const std::string& filePath)
{
    auto file = std::filesystem::path(filePath) / squashFSImage;
    if (std::filesystem::is_regular_file(file))
    {
        return 0;
    }
    else
    {
        log<level::ERR>("Failed to find the SquashFS image.");
        return -1;
    }
}

void ItemUpdaterUbi::removeReadOnlyPartition(const std::string& versionId)
{
    auto serviceFile = "obmc-flash-bios-ubiumount-ro@" + versionId + ".service";

    // Remove the read-only partitions.
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(serviceFile, "replace");
    bus.call_noreply(method);
}

void ItemUpdaterUbi::removeReadWritePartition(const std::string& versionId)
{
    auto serviceFile = "obmc-flash-bios-ubiumount-rw@" + versionId + ".service";

    // Remove the read-write partitions.
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(serviceFile, "replace");
    bus.call_noreply(method);
}

void ItemUpdaterUbi::reset()
{
    utils::hiomapdSuspend(bus);

    constexpr static auto patchDir = "/usr/local/share/pnor";
    if (std::filesystem::is_directory(patchDir))
    {
        for (const auto& iter : std::filesystem::directory_iterator(patchDir))
        {
            std::filesystem::remove_all(iter);
        }
    }

    // Clear the read-write partitions.
    for (const auto& it : activations)
    {
        auto rwDir = PNOR_RW_PREFIX + it.first;
        if (std::filesystem::is_directory(rwDir))
        {
            for (const auto& iter : std::filesystem::directory_iterator(rwDir))
            {
                std::filesystem::remove_all(iter);
            }
        }
    }

    // Clear the preserved partition, except for SECBOOT that contains keys
    // provisioned for the system.
    if (std::filesystem::is_directory(PNOR_PRSV))
    {
        for (const auto& iter : std::filesystem::directory_iterator(PNOR_PRSV))
        {
            auto secbootPartition = "SECBOOT";
            if (iter.path().stem() == secbootPartition)
            {
                continue;
            }
            std::filesystem::remove_all(iter);
        }
    }

    utils::hiomapdResume(bus);
}

bool ItemUpdaterUbi::isVersionFunctional(const std::string& versionId)
{
    if (!std::filesystem::exists(PNOR_RO_ACTIVE_PATH))
    {
        return false;
    }

    std::filesystem::path activeRO =
        std::filesystem::read_symlink(PNOR_RO_ACTIVE_PATH);

    if (!std::filesystem::is_directory(activeRO))
    {
        return false;
    }

    if (activeRO.string().find(versionId) == std::string::npos)
    {
        return false;
    }

    // active PNOR is the version we're checking
    return true;
}

void ItemUpdaterUbi::freePriority(uint8_t value, const std::string& versionId)
{
    //  Versions with the lowest priority in front
    std::priority_queue<std::pair<int, std::string>,
                        std::vector<std::pair<int, std::string>>,
                        std::greater<std::pair<int, std::string>>>
        versionsPQ;

    for (const auto& intf : activations)
    {
        if (intf.second->redundancyPriority)
        {
            versionsPQ.push(std::make_pair(
                intf.second->redundancyPriority.get()->priority(),
                intf.second->versionId));
        }
    }

    while (!versionsPQ.empty())
    {
        if (versionsPQ.top().first == value &&
            versionsPQ.top().second != versionId)
        {
            // Increase priority by 1 and update its value
            ++value;
            storeToFile(versionsPQ.top().second, value);
            auto it = activations.find(versionsPQ.top().second);
            it->second->redundancyPriority.get()->sdbusplus::xyz::
                openbmc_project::Software::server::RedundancyPriority::priority(
                    value);
        }
        versionsPQ.pop();
    }
}

bool ItemUpdaterUbi::erase(std::string entryId)
{
    if (!ItemUpdater::erase(entryId))
    {
        return false;
    }

    // Remove priority persistence file
    removeFile(entryId);

    // Removing read-only and read-write partitions
    removeReadWritePartition(entryId);
    removeReadOnlyPartition(entryId);

    return true;
}

void ItemUpdaterUbi::deleteAll()
{
    auto chassisOn = isChassisOn();

    for (const auto& activationIt : activations)
    {
        if (isVersionFunctional(activationIt.first) && chassisOn)
        {
            continue;
        }
        else
        {
            ItemUpdaterUbi::erase(activationIt.first);
        }
    }

    // Remove any remaining pnor-ro- or pnor-rw- volumes that do not match
    // the current version.
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("obmc-flash-bios-cleanup.service", "replace");
    bus.call_noreply(method);
}

// TODO: openbmc/openbmc#1402 Monitor flash usage
bool ItemUpdaterUbi::freeSpace()
{
    bool isSpaceFreed = false;
    //  Versions with the highest priority in front
    std::priority_queue<std::pair<int, std::string>,
                        std::vector<std::pair<int, std::string>>,
                        std::less<std::pair<int, std::string>>>
        versionsPQ;

    std::size_t count = 0;
    for (const auto& iter : activations)
    {
        if (iter.second.get()->activation() ==
            server::Activation::Activations::Active)
        {
            count++;
            // Don't put the functional version on the queue since we can't
            // remove the "running" PNOR version if it allows multiple PNORs
            // But removing functional version if there is only one PNOR.
            if (ACTIVE_PNOR_MAX_ALLOWED > 1 &&
                isVersionFunctional(iter.second->versionId))
            {
                continue;
            }
            versionsPQ.push(std::make_pair(
                iter.second->redundancyPriority.get()->priority(),
                iter.second->versionId));
        }
    }

    // If the number of PNOR versions is over ACTIVE_PNOR_MAX_ALLOWED -1,
    // remove the highest priority one(s).
    while ((count >= ACTIVE_PNOR_MAX_ALLOWED) && (!versionsPQ.empty()))
    {
        erase(versionsPQ.top().second);
        versionsPQ.pop();
        count--;
        isSpaceFreed = true;
    }
    return isSpaceFreed;
}

std::string ItemUpdaterUbi::determineId(const std::string& symlinkPath)
{
    if (!std::filesystem::exists(symlinkPath))
    {
        return {};
    }

    auto target = std::filesystem::canonical(symlinkPath).string();

    // check to make sure the target really exists
    if (!std::filesystem::is_regular_file(target + "/" + PNOR_TOC_FILE))
    {
        return {};
    }
    // Get the image <id> from the symlink target
    // for example /media/ro-2a1022fe
    static const auto PNOR_RO_PREFIX_LEN = strlen(PNOR_RO_PREFIX);
    return target.substr(PNOR_RO_PREFIX_LEN);
}

void GardResetUbi::reset()
{
    // The GARD partition is currently misspelled "GUARD." This file path will
    // need to be updated in the future.
    auto path = std::filesystem::path(PNOR_PRSV_ACTIVE_PATH);
    path /= "GUARD";

    utils::hiomapdSuspend(bus);

    if (std::filesystem::is_regular_file(path))
    {
        std::filesystem::remove(path);
    }

    utils::hiomapdResume(bus);
}

} // namespace updater
} // namespace software
} // namespace openpower
