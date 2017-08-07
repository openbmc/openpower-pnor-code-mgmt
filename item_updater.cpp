#include <string>
#include <experimental/filesystem>
#include <fstream>
#include <phosphor-logging/log.hpp>
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

using namespace phosphor::logging;

constexpr auto squashFSImage = "pnor.xz.squashfs";

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
        if (ItemUpdater::validateSquashFSImage(filePath) == 0)
        {
            activationState = server::Activation::Activations::Ready;
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
                        activationState)));
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
    for(const auto& iter : fs::directory_iterator(MEDIA_DIR))
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

            // Create Activation instance for this version.
            activations.insert(std::make_pair(
                                   id,
                                   std::make_unique<Activation>(
                                       bus,
                                       path,
                                       *this,
                                       id,
                                       extendedVersion,
                                       activationState)));

            // If Active, create RedundancyPriority instance for this version.
            if (activationState == server::Activation::Activations::Active)
            {
                if(fs::is_regular_file(PERSIST_DIR + id))
                {
                    uint8_t priority;
                    restoreFromFile(id, &priority);
                    activations.find(id)->second->redundancyPriority =
                             std::make_unique<RedundancyPriority>(
                                 bus,
                                 path,
                                 *(activations.find(id)->second),
                                 priority);
                }
                else
                {
                    activations.find(id)->second->activation(
                            server::Activation::Activations::Invalid);
                }

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
    for(const auto& it : activations)
    {
        removeReadWritePartition(it.first);
        removeFile(it.first);
    }
    removePreservedPartition();
    return;
}

void ItemUpdater::freePriority(uint8_t value)
{
    //TODO openbmc/openbmc#1896 Improve the performance of this function
    for (const auto& intf : activations)
    {
        if(intf.second->redundancyPriority)
        {
            if (intf.second->redundancyPriority.get()->priority() == value)
            {
                intf.second->redundancyPriority.get()->priority(value+1);
            }
        }
    }
}

bool ItemUpdater::isLowestPriority(uint8_t value)
{
    for (const auto& intf : activations)
    {
        if(intf.second->redundancyPriority)
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
    // Removing partitions
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
    removeFile(entryId);
}

} // namespace updater
} // namespace software
} // namespace openpower
