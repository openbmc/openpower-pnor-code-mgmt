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

int ItemUpdater::createActivation(sd_bus_message* msg,
                                  void* userData,
                                  sd_bus_error* retErr)
{
fprintf(stderr, "Calling GetSubtree\n");
    auto* updater = static_cast<ItemUpdater*>(userData);
    auto mapper = updater->bus.new_method_call(
            MAPPER_BUSNAME,
            MAPPER_PATH,
            MAPPER_INTERFACE,
            "GetSubTree");
    mapper.append(SOFTWARE_OBJPATH,
                  0, // Depth
                  std::vector<std::string>({"xyz.openbmc_project.Software.Version"}));

    auto mapperResponseMsg = updater->bus.call(mapper);
    if (mapperResponseMsg.is_method_error())
    {
        log<level::ERR>("Error in mapper call",
                        entry("PATH=%s", SOFTWARE_OBJPATH),
                        entry("INTERFACE=%s", VERSION_IFACE));
        return 0;
    }

fprintf(stderr, "Reading response\n");
    std::map<std::string, std::map<
            std::string, std::vector<std::string>>> mapperResponse;
    mapperResponseMsg.read(mapperResponse);
    if (mapperResponse.empty())
    {
        log<level::ERR>("Error reading mapper response",
                        entry("PATH=%s", SOFTWARE_OBJPATH),
                        entry("INTERFACE=%s", VERSION_IFACE));
        return 0;
    }

    for (const auto& resp : mapperResponse)
    {
        const auto& path = resp.first;
        const auto& service = resp.second.begin()->first;
fprintf(stderr, "path=%s\n", path.c_str());

        auto method = updater->bus.new_method_call(
                service.c_str(),
                path.c_str(),
                "org.freedesktop.DBus.Properties",
                "Get");
        method.append(VERSION_IFACE, "Purpose");

        auto methodResponseMsg = updater->bus.call(method);
        if (methodResponseMsg.is_method_error())
        {
            log<level::ERR>("Error in method call",
                            entry("PATH=%s", path.c_str()),
                            entry("INTERFACE=%s", VERSION_IFACE));
            return 0;
        }

        sdbusplus::message::variant<std::string> purpose;
        methodResponseMsg.read(purpose);
        std::string purposeStr = sdbusplus::message::variant_ns::get<
                std::string>(purpose);
fprintf(stderr, "purpose=%s\n", purposeStr.c_str());

        // Only process the Host images
        if ((purposeStr.empty()) ||
            (purposeStr.compare(convertForMessage(
                server::Version::VersionPurpose::Host).c_str())))
        {
fprintf(stderr, "did not match purpose: %s\n", convertForMessage(server::Version::VersionPurpose::Host).c_str());
            continue;
        }

        // Version id is the last item in the path
        auto pos = path.rfind("/");
        if (pos == std::string::npos)
        {
            log<level::ERR>("No version id found in object path",
                    entry("OBJPATH=%s", path));
            return 0;
        }

        auto versionId = path.substr(pos + 1);

        if (updater->activations.find(versionId) == updater->activations.end())
        {
            // Determine the Activation state by processing the given image dir.
            auto activationState = server::Activation::Activations::Invalid;
            if (ItemUpdater::validateSquashFSImage(versionId) == 0)
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
                auto method = updater->bus.new_method_call(
                                    SYSTEMD_BUSNAME,
                                    SYSTEMD_PATH,
                                    SYSTEMD_INTERFACE,
                                    "StartUnit");
                method.append(squashfsMountServiceFile, "replace");
                updater->bus.call_noreply(method);
            }

            fs::path manifestPath = IMAGE_DIR;
            manifestPath /= versionId;
            manifestPath /= MANIFEST_FILE_NAME;
//Need to get from FilePath
fprintf(stderr, "Looking for manifest file %s\n", manifestPath.c_str());
            auto extendedVersion = getExtendedVersion(manifestPath);

            updater->activations.insert(std::make_pair(
                    versionId,
                    std::make_unique<Activation>(
                            updater->bus,
                            path,
                            versionId,
                            extendedVersion,
                            activationState)));
        }
    }
    return 0;
}

std::string ItemUpdater::getExtendedVersion(const std::string& manifestFilePath)
{
fprintf(stderr, "Opening manifest file %s\n", manifestFilePath.c_str());
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
fprintf(stderr, "%s\n", line.c_str());
            if (line.compare(0, extendedVersionKeySize,
                             extendedVersionKey) == 0)
            {
fprintf(stderr, "Found extended\n");
                extendedVersion = line.substr(extendedVersionKeySize);
                break;
            }
        }
        efile.close();
    }
    catch (const std::exception& e)
    {
fprintf(stderr, "exception: %s\n", e.what());
        log<level::ERR>("Error in reading Host MANIFEST file");
    }
    return extendedVersion;
}

int ItemUpdater::validateSquashFSImage(const std::string& versionId)
{
    auto file = IMAGE_DIR + versionId + "/" +
                std::string(squashFSImage);
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
    auto mapper = bus.new_method_call(
            MAPPER_BUSNAME,
            MAPPER_PATH,
            MAPPER_INTERFACE,
            "GetSubTreePaths");
    mapper.append(SOFTWARE_OBJPATH, 1,
                  std::vector<std::string>({BUSNAME_UPDATER}));

    auto mapperResponseMsg = bus.call(mapper);
    if (mapperResponseMsg.is_method_error())
    {
        log<level::ERR>("Error in mapper call",
                        entry("PATH=%s", SOFTWARE_OBJPATH),
                        entry("INTERFACE=%s", BUSNAME_UPDATER));
        return;
    }

    std::vector<std::string> mapperResponse;
    mapperResponseMsg.read(mapperResponse);
    if (mapperResponse.empty())
    {
        log<level::ERR>("Error reading mapper response",
                        entry("PATH=%s", SOFTWARE_OBJPATH),
                        entry("INTERFACE=%s", BUSNAME_UPDATER));
        return;
    }

    for (const auto& resp : mapperResponse)
    {
        auto pos = resp.rfind("/");
        if (pos == std::string::npos)
        {
            log<level::ERR>("No version id found in object path",
                    entry("OBJPATH=%s", resp));
            return;
        }

        auto serviceFile = "obmc-flash-bios-ubiumount@" +
                           resp.substr(pos + 1) + ".service";

        log<level::INFO>(serviceFile.c_str()); //TODO: REMOVE

        auto method = bus.new_method_call(
                SYSTEMD_BUSNAME,
                SYSTEMD_PATH,
                SYSTEMD_INTERFACE,
                "StartUnit");
        method.append(serviceFile,"replace");
        bus.call_noreply(method);

    }

    return;
}

} // namespace updater
} // namespace software
} // namespace openpower
