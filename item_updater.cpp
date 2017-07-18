#include <string>
#include <experimental/filesystem>
#include <fstream>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>
#include "version.hpp"
#include "config.h"
#include "item_updater.hpp"
#include "activation.hpp"
#include <dirent.h>

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
                                filePath)));
    }
    return;
}

void ItemUpdater::processPNORImage()
{

    // Get the current PNOR version
    fs::path pnorTOC(PNOR_RO_ACTIVE_PATH);
    pnorTOC /= PNOR_TOC_FILE;
    std::ifstream efile(pnorTOC.c_str());
    if (efile.good() != 1)
    {
        log<level::INFO>("Error PNOR current version is empty");
        return;
    }
    std::string currentVersion = (Version::getValue(pnorTOC.string(),
                 std::map<std::string, std::string>
                 {{"version", ""}})).begin()->second;

    // Read pnor.toc from folders under under /media/
    // to get Active Software Versions.
    const std::string prefix("pnor-ro-");
    const char* PATH = "/media/";
    DIR *dir = opendir(PATH);
    struct dirent *entry = readdir(dir);
    while (entry != NULL)
    {
        if (entry->d_type == DT_DIR &&
            !(std::string(entry->d_name)).compare(0, prefix.size(), prefix))
        {
            fs::path pnorTOC(std::string(PATH) + std::string(entry->d_name));
            pnorTOC /= PNOR_TOC_FILE;
            std::ifstream efile(pnorTOC.c_str());
            if (efile.good() != 1)
            {
                log<level::INFO>("Failed to read pnorTOC\n");
                entry = readdir(dir);
                break;
            }
            auto keyValues = Version::getValue(pnorTOC.string(),
                std::map<std::string, std::string> {{"version", ""},
                {"extended_version", ""}});
            std::string version = keyValues.at("version");
            std::string extendedVersion = keyValues.at("extended_version");
            auto id = Version::getId(version);
            auto purpose = server::Version::VersionPurpose::Host;
            auto path =  std::string{SOFTWARE_OBJPATH} + '/' + id;
            auto activationState = server::Activation::Activations::Active;

            // Current PNOR needs the lowest Priority, so setting to 0.
            auto priority = 1;
            if (currentVersion == version)
            {
                priority = 0;
            }

            activations.insert(std::make_pair(
                                   id,
                                   std::make_unique<Activation>(
                                       bus,
                                       path,
                                       *this,
                                       id,
                                       extendedVersion,
                                       activationState)));
            activations.find(id)->second->redundancyPriority =
                     std::make_unique<RedundancyPriority>(
                         bus,
                         path,
                         *(activations.find(id)->second),
                         priority);
            versions.insert(std::make_pair(
                                id,
                                std::make_unique<Version>(
                                     bus,
                                     path,
                                     version,
                                     purpose,
                                     "")));
        }
        entry = readdir(dir);
    }
    closedir(dir);
    return;
}

int ItemUpdater::validateSquashFSImage(const std::string& filePath)
{
    fs::path file(filePath);
    file /= squashFSImage;
    std::ifstream efile(file.c_str());

    if (efile.good() == 1)
    {
        return 0;
    }
    else
    {
        log<level::ERR>("Failed to find the SquashFS image.");
        return -1;
    }
}

void ItemUpdater::reset()
{
    for(const auto& it : activations)
    {
        auto serviceFile = "obmc-flash-bios-ubiumount-rw@" + it.first +
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

} // namespace updater
} // namespace software
} // namespace openpower
