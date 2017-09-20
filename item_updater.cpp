#include <string>
#include <experimental/filesystem>
#include <fstream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include "xyz/openbmc_project/Common/error.hpp"
#include <xyz/openbmc_project/Software/Version/server.hpp>
#include "version.hpp"
#include "config.h"
#include "item_updater.hpp"
#include "activation.hpp"
#include "serialize.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;
namespace fs = std::experimental::filesystem;

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;

constexpr auto squashFSImage = "pnor.xz.squashfs";
constexpr auto CHASSIS_STATE_PATH = "/xyz/openbmc_project/state/chassis0";
constexpr auto CHASSIS_STATE_OBJ = "xyz.openbmc_project.State.Chassis";
constexpr auto CHASSIS_STATE_OFF =
                        "xyz.openbmc_project.State.Chassis.PowerState.Off";
constexpr auto SYSTEMD_PROPERTY_INTERFACE = "org.freedesktop.DBus.Properties";

void ItemUpdater::createActivation(sdbusplus::message::message& m)
{
    using SVersion = server::Version;
    using VersionPurpose = SVersion::VersionPurpose;
    namespace msg = sdbusplus::message;
    namespace variant_ns = msg::variant_ns;

    sdbusplus::message::object_path objPath;
    std::map<std::string,
             std::map<std::string, msg::variant<std::string>>> interfaces;
    m.read(objPath, interfaces);

    std::string path(std::move(objPath));
    std::string filePath;
    auto purpose = VersionPurpose::Unknown;
    std::string version;

    for (const auto& intf : interfaces)
    {
        if (intf.first == VERSION_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Purpose")
                {
                    // Only process the Host and System images
                    auto value = SVersion::convertVersionPurposeFromString(
                            variant_ns::get<std::string>(property.second));

                    if (value == VersionPurpose::Host ||
                        value == VersionPurpose::System)
                    {
                        purpose = value;
                    }
                }
                else if (property.first == "Version")
                {
                    version = variant_ns::get<std::string>(property.second);
                }
            }
        }
        else if (intf.first == FILEPATH_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Path")
                {
                    filePath = variant_ns::get<std::string>(property.second);
                }
            }
        }
    }
    if ((filePath.empty()) || (purpose == VersionPurpose::Unknown))
    {
        return;
    }

    // Version id is the last item in the path
    auto pos = path.rfind("/");
    if (pos == std::string::npos)
    {
        log<level::ERR>("No version id found in object path",
                        entry("OBJPATH=%s", path));
        return;
    }

    auto versionId = path.substr(pos + 1);

    if (activations.find(versionId) == activations.end())
    {
        // Determine the Activation state by processing the given image dir.
        auto activationState = server::Activation::Activations::Invalid;
        AssociationList associations = {};
        if (ItemUpdater::validateSquashFSImage(filePath) == 0)
        {
            activationState = server::Activation::Activations::Ready;
            // Create an association to the host inventory item
            associations.emplace_back(std::make_tuple(
                                              ACTIVATION_FWD_ASSOCIATION,
                                              ACTIVATION_REV_ASSOCIATION,
                                              HOST_INVENTORY_PATH));
        }

        fs::path manifestPath(filePath);
        manifestPath /= MANIFEST_FILE;
        std::string extendedVersion = (Version::getValue(manifestPath.string(),
                 std::map<std::string, std::string>
                 {{"extended_version", ""}})).begin()->second;

        activations.insert(std::make_pair(
                versionId,
                std::make_unique<Activation>(
                        bus,
                        path,
                        *this,
                        versionId,
                        extendedVersion,
                        activationState,
                        associations)));
        versions.insert(std::make_pair(
                            versionId,
                            std::make_unique<Version>(
                                bus,
                                path,
                                version,
                                purpose,
                                filePath,
                                *this)));
    }
    else
    {
        log<level::INFO>("Software Object with the same version already exists",
                         entry("VERSION_ID=%s", versionId));
    }
    return;
}

void ItemUpdater::processPNORImage()
{
    // Read pnor.toc from folders under /media/
    // to get Active Software Versions.
    for (const auto& iter : fs::directory_iterator(MEDIA_DIR))
    {
        auto activationState = server::Activation::Activations::Active;

        static const auto PNOR_RO_PREFIX_LEN = strlen(PNOR_RO_PREFIX);

        // Check if the PNOR_RO_PREFIX is the prefix of the iter.path
        if (0 == iter.path().native().compare(0, PNOR_RO_PREFIX_LEN,
                                              PNOR_RO_PREFIX))
        {
            auto pnorTOC = iter.path() / PNOR_TOC_FILE;
            if (!fs::is_regular_file(pnorTOC))
            {
                log<level::ERR>("Failed to read pnorTOC\n",
                                entry("FileName=%s", pnorTOC.string()));
                activationState = server::Activation::Activations::Invalid;
            }
            auto keyValues =
                    Version::getValue(pnorTOC,
                                      {{ "version", "" },
                                       { "extended_version", "" } });
            auto& version = keyValues.at("version");
            if (version.empty())
            {
                log<level::ERR>("Failed to read version from pnorTOC",
                                entry("FILENAME=%s", pnorTOC.string()));
                activationState = server::Activation::Activations::Invalid;
            }

            auto& extendedVersion = keyValues.at("extended_version");
            if (extendedVersion.empty())
            {
                log<level::ERR>("Failed to read extendedVersion from pnorTOC",
                                entry("FILENAME=%s", pnorTOC.string()));
                activationState = server::Activation::Activations::Invalid;
            }

            // The versionId is extracted from the path
            // for example /media/pnor-ro-2a1022fe
            auto id = iter.path().native().substr(PNOR_RO_PREFIX_LEN);
            auto purpose = server::Version::VersionPurpose::Host;
            auto path = fs::path(SOFTWARE_OBJPATH) / id;
            AssociationList associations = {};

            if (activationState == server::Activation::Activations::Active)
            {
                // Create an association to the host inventory item
                associations.emplace_back(std::make_tuple(
                                                  ACTIVATION_FWD_ASSOCIATION,
                                                  ACTIVATION_REV_ASSOCIATION,
                                                  HOST_INVENTORY_PATH));

                // Create an active association since this image is active
                createActiveAssociation(path);
            }

            // Create Activation instance for this version.
            activations.insert(std::make_pair(
                                   id,
                                   std::make_unique<Activation>(
                                       bus,
                                       path,
                                       *this,
                                       id,
                                       extendedVersion,
                                       activationState,
                                       associations)));

            // If Active, create RedundancyPriority instance for this version.
            if (activationState == server::Activation::Activations::Active)
            {
                uint8_t priority = std::numeric_limits<uint8_t>::max();
                if (!restoreFromFile(id, priority))
                {
                    log<level::ERR>("Unable to restore priority from file.",
                                    entry("VERSIONID=%s", id));
                }
                activations.find(id)->second->redundancyPriority =
                         std::make_unique<RedundancyPriority>(
                             bus,
                             path,
                             *(activations.find(id)->second),
                             priority);
            }

            // Create Version instance for this version.
            versions.insert(std::make_pair(
                                id,
                                std::make_unique<Version>(
                                     bus,
                                     path,
                                     version,
                                     purpose,
                                     "",
                                     *this)));
        }
    }

    // Look at the RO symlink to determine if there is a functional image
    auto id = determineId(PNOR_RO_ACTIVE_PATH);
    if (!id.empty())
    {
        updateFunctionalAssociation(std::string{SOFTWARE_OBJPATH} + '/' + id);
    }
    return;
}

int ItemUpdater::validateSquashFSImage(const std::string& filePath)
{
    auto file = fs::path(filePath) / squashFSImage;
    if (fs::is_regular_file(file))
    {
        return 0;
    }
    else
    {
        log<level::ERR>("Failed to find the SquashFS image.");
        return -1;
    }
}

void ItemUpdater::removeReadOnlyPartition(std::string versionId)
{
        auto serviceFile = "obmc-flash-bios-ubiumount-ro@" + versionId +
                ".service";

        // Remove the read-only partitions.
        auto method = bus.new_method_call(
                SYSTEMD_BUSNAME,
                SYSTEMD_PATH,
                SYSTEMD_INTERFACE,
                "StartUnit");
        method.append(serviceFile, "replace");
        bus.call_noreply(method);
}

void ItemUpdater::removeReadWritePartition(std::string versionId)
{
        auto serviceFile = "obmc-flash-bios-ubiumount-rw@" + versionId +
                ".service";

        // Remove the read-write partitions.
        auto method = bus.new_method_call(
                SYSTEMD_BUSNAME,
                SYSTEMD_PATH,
                SYSTEMD_INTERFACE,
                "StartUnit");
        method.append(serviceFile, "replace");
        bus.call_noreply(method);
}

void ItemUpdater::removePreservedPartition()
{
    // Remove the preserved partition.
    auto method = bus.new_method_call(
            SYSTEMD_BUSNAME,
            SYSTEMD_PATH,
            SYSTEMD_INTERFACE,
            "StartUnit");
    method.append("obmc-flash-bios-ubiumount-prsv.service", "replace");
    bus.call_noreply(method);

    return;
}

void ItemUpdater::reset()
{
    for (const auto& it : activations)
    {
        removeReadWritePartition(it.first);
        removeFile(it.first);
    }
    removePreservedPartition();
    return;
}

bool ItemUpdater::isVersionFunctional(std::string versionId)
{
    if (!fs::exists(PNOR_RO_ACTIVE_PATH))
    {
        return false;
    }

    fs::path activeRO = fs::read_symlink(PNOR_RO_ACTIVE_PATH);

    if (!fs::is_directory(activeRO))
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

bool ItemUpdater::isChassisOn()
{
    auto mapperCall = bus.new_method_call(
            MAPPER_BUSNAME,
            MAPPER_PATH,
            MAPPER_INTERFACE,
            "GetObject");

    mapperCall.append(CHASSIS_STATE_PATH,
                      std::vector<std::string>({CHASSIS_STATE_OBJ}));
    auto mapperResponseMsg = bus.call(mapperCall);
    if (mapperResponseMsg.is_method_error())
    {
        log<level::ERR>("Error in Mapper call");
        elog<InternalFailure>();
    }
    using MapperResponseType = std::map<std::string, std::vector<std::string>>;
    MapperResponseType mapperResponse;
    mapperResponseMsg.read(mapperResponse);
    if (mapperResponse.empty())
    {
        log<level::ERR>("Invalid Response from mapper");
        elog<InternalFailure>();
    }

    auto method = bus.new_method_call((mapperResponse.begin()->first).c_str(),
                                      CHASSIS_STATE_PATH,
                                      SYSTEMD_PROPERTY_INTERFACE,
                                      "Get");
    method.append(CHASSIS_STATE_OBJ, "CurrentPowerState");
    auto response = bus.call(method);
    if (response.is_method_error())
    {
        log<level::ERR>("Error in fetching current Chassis State",
                        entry("MapperResponse=%s",
                              (mapperResponse.begin()->first).c_str()));
        elog<InternalFailure>();
    }
    sdbusplus::message::variant<std::string> currentChassisState;
    response.read(currentChassisState);
    auto strParam =
        sdbusplus::message::variant_ns::get<std::string>(currentChassisState);
    return (strParam != CHASSIS_STATE_OFF);
}

void ItemUpdater::freePriority(uint8_t value, const std::string& versionId)
{
    //TODO openbmc/openbmc#1896 Improve the performance of this function
    for (const auto& intf : activations)
    {
        if (intf.second->redundancyPriority)
        {
            if (intf.second->redundancyPriority.get()->priority() == value &&
                intf.second->versionId != versionId)
            {
                intf.second->redundancyPriority.get()->priority(value + 1);
            }
        }
    }
}

bool ItemUpdater::isLowestPriority(uint8_t value)
{
    for (const auto& intf : activations)
    {
        if (intf.second->redundancyPriority)
        {
            if (intf.second->redundancyPriority.get()->priority() < value)
            {
                return false;
            }
        }
    }
    return true;
}

void ItemUpdater::erase(std::string entryId)
{
    if (isVersionFunctional(entryId) && isChassisOn()) {
        log<level::ERR>(("Error: Version " + entryId + \
                         " is currently active and running on the host." \
                         " Unable to remove.").c_str());
        return;
    }
    // Remove priority persistence file
    removeFile(entryId);

    // Removing read-only and read-write partitions
    removeReadWritePartition(entryId);
    removeReadOnlyPartition(entryId);

    // Removing entry in versions map
    auto it = versions.find(entryId);
    if (it == versions.end())
    {
        log<level::ERR>(("Error: Failed to find version " + entryId + \
                         " in item updater versions map." \
                         " Unable to remove.").c_str());
        return;
    }
    versions.erase(entryId);

    // Removing entry in activations map
    auto ita = activations.find(entryId);
    if (ita == activations.end())
    {
        log<level::ERR>(("Error: Failed to find version " + entryId + \
                         " in item updater activations map." \
                         " Unable to remove.").c_str());
        return;
    }
    activations.erase(entryId);
}

// TODO: openbmc/openbmc#1402 Monitor flash usage
void ItemUpdater::freeSpace()
{
    std::size_t count = 0;
    decltype(activations.begin()->second->redundancyPriority.get()->priority())
            highestPriority = 0;
    decltype(activations.begin()->second->versionId) highestPriorityVersion;
    for (const auto& iter : activations)
    {
        if (iter.second.get()->activation() == server::Activation::Activations::Active)
        {
            count++;
            if (iter.second->redundancyPriority.get()->priority() > highestPriority)
            {
                highestPriority = iter.second->redundancyPriority.get()->priority();
                highestPriorityVersion = iter.second->versionId;
            }
        }
    }
    // Remove the pnor version with highest priority since the PNOR
    // can't hold more than 2 versions.
    if (count >= ACTIVE_PNOR_MAX_ALLOWED)
    {
        erase(highestPriorityVersion);
    }
}

void ItemUpdater::createActiveAssociation(std::string path)
{
    assocs.emplace_back(std::make_tuple(ACTIVE_FWD_ASSOCIATION,
                                        ACTIVE_REV_ASSOCIATION,
                                        path));
    associations(assocs);
}

void ItemUpdater::updateFunctionalAssociation(const std::string& path)
{
    // remove all functional associations
    for (auto iter = assocs.begin(); iter != assocs.end();)
    {
        if ((std::get<0>(*iter)).compare(FUNCTIONAL_FWD_ASSOCIATION) == 0)
        {
            iter = assocs.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
    assocs.emplace_back(std::make_tuple(FUNCTIONAL_FWD_ASSOCIATION,
                                        FUNCTIONAL_REV_ASSOCIATION,
                                        path));
    associations(assocs);
}

void ItemUpdater::removeActiveAssociation(std::string path)
{
    for (auto iter = assocs.begin(); iter != assocs.end();)
    {
        if ((std::get<0>(*iter)).compare(ACTIVE_FWD_ASSOCIATION) == 0 &&
            (std::get<2>(*iter)).compare(path) == 0)
        {
            iter = assocs.erase(iter);
            associations(assocs);
        }
        else
        {
            ++iter;
        }
    }
}

std::string ItemUpdater::determineId(const std::string& symlinkPath)
{
    if (!fs::exists(symlinkPath))
    {
        return {};
    }

    auto target = fs::canonical(symlinkPath).string();

    // check to make sure the target really exists
    if (!fs::is_regular_file(target + "/" + PNOR_TOC_FILE))
    {
        return {};
    }
    // Get the image <id> from the symlink target
    // for example /media/ro-2a1022fe
    static const auto PNOR_RO_PREFIX_LEN = strlen(PNOR_RO_PREFIX);
    return target.substr(PNOR_RO_PREFIX_LEN);
}

} // namespace updater
} // namespace software
} // namespace openpower
