#include <string>
#include <experimental/filesystem>
#include <fstream>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Software/Version/server.hpp>
#include "config.h"
#include "item_updater.hpp"
#include "activation.hpp"

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
    sdbusplus::message::object_path objPath;
    std::map<std::string,
             std::map<std::string,
                      sdbusplus::message::variant<std::string>>> interfaces;
    m.read(objPath, interfaces);
    std::string path(std::move(objPath));

    std::string filePath;

    for (const auto& intf : interfaces)
    {
        if (intf.first == VERSION_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Purpose")
                {
                    // Only process the Host and System images
                    std::string value = sdbusplus::message::variant_ns::get<
                            std::string>(property.second);
                    if ((value != convertForMessage(
                            server::Version::VersionPurpose::Host)) &&
                        (value != convertForMessage(
                            server::Version::VersionPurpose::System)))
                    {
                        return;
                    }
                }
            }
        }
        else if (intf.first == FILEPATH_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Path")
                {
                    filePath = sdbusplus::message::variant_ns::get<
                            std::string>(property.second);
                }
            }
        }
    }
    if (filePath.empty())
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

            // Load the squashfs image to pnor so that it is available to be
            // activated when requested.
            // This is done since the image on the BMC can be removed.
            constexpr auto squashfsMountService =
                                "obmc-flash-bios-squashfsmount@";
            auto squashfsMountServiceFile =
                                std::string(squashfsMountService) +
                                versionId +
                                ".service";
            auto method = bus.new_method_call(
                                SYSTEMD_BUSNAME,
                                SYSTEMD_PATH,
                                SYSTEMD_INTERFACE,
                                "StartUnit");
            method.append(squashfsMountServiceFile, "replace");
            bus.call_noreply(method);
        }

        fs::path manifestPath(filePath);
        manifestPath /= MANIFEST_FILE;
        auto extendedVersion = ItemUpdater::getExtendedVersion(manifestPath);
        activations.insert(std::make_pair(
                versionId,
                std::make_unique<Activation>(
                        bus,
                        path,
                        versionId,
                        extendedVersion,
                        activationState)));
    }
    return;
}

std::string ItemUpdater::getExtendedVersion(const std::string& manifestFilePath)
{
    constexpr auto extendedVersionKey = "extended_version=";
    constexpr auto extendedVersionKeySize = strlen(extendedVersionKey);

    if (manifestFilePath.empty())
    {
        log<level::ERR>("Error MANIFESTFilePath is empty");
        throw std::runtime_error("MANIFESTFilePath is empty");
    }

    std::string extendedVersion{};
    std::ifstream efile;
    std::string line;
    efile.exceptions(std::ifstream::failbit
                     | std::ifstream::badbit
                     | std::ifstream::eofbit);

    try
    {
        efile.open(manifestFilePath);
        while (getline(efile, line))
        {
            if (line.compare(0, extendedVersionKeySize,
                             extendedVersionKey) == 0)
            {
                extendedVersion = line.substr(extendedVersionKeySize);
                break;
            }
        }
        efile.close();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error in reading Host MANIFEST file");
    }
    return extendedVersion;
}

int ItemUpdater::validateSquashFSImage(const std::string& filePath)
{
    for(const auto& iter : fs::directory_iterator(filePath))
    {
        auto file = static_cast<std::string>(
                        iter.path().filename()).find(squashFSImage);
        if (std::string::npos != file)
        {
            std::ifstream efile(iter.path().filename().c_str());
            if (efile.good() == 1)
            {
                return 0;
            }
        }
    }
    log<level::ERR>("Failed to find the SquashFS image.");
    return -1;
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

} // namespace updater
} // namespace software
} // namespace openpower
