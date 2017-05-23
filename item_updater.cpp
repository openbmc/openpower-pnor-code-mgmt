#include <string>
#include <fstream>
#include <phosphor-logging/log.hpp>
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

using namespace phosphor::logging;

constexpr auto squashFSImage = "pnor.xz.squashfs";

int ItemUpdater::createActivation(sd_bus_message* msg,
                                  void* userData,
                                  sd_bus_error* retErr)
{
    auto* updater = static_cast<ItemUpdater*>(userData);
    auto mapper = updater->busItem.new_method_call(
            MAPPER_BUSNAME,
            MAPPER_PATH,
            MAPPER_INTERFACE,
            "GetSubTreePaths");
    mapper.append(SOFTWARE_OBJPATH,
                  1, // Depth
                  std::vector<std::string>({VERSION_IFACE}));

    auto mapperResponseMsg = updater->busItem.call(mapper);
    if (mapperResponseMsg.is_method_error())
    {
        log<level::ERR>("Error in mapper call",
                        entry("PATH=%s", SOFTWARE_OBJPATH),
                        entry("INTERFACE=%s", VERSION_IFACE));
        return -1;
    }

    std::vector<std::string> mapperResponse;
    mapperResponseMsg.read(mapperResponse);
    if (mapperResponse.empty())
    {
        log<level::ERR>("Error reading mapper response",
                        entry("PATH=%s", SOFTWARE_OBJPATH),
                        entry("INTERFACE=%s", VERSION_IFACE));
        return -1;
    }

    auto extendedVersion = ItemUpdater::getExtendedVersion(MANIFEST_FILE);
    for (const auto& resp : mapperResponse)
    {
        // Version id is the last item in the path
        auto pos = resp.rfind("/");
        if (pos == std::string::npos)
        {
            log<level::ERR>("No version id found in object path",
                    entry("OBJPATH=%s", resp));
            return -1;
        }

        auto versionId = resp.substr(pos + 1);

        // Determine the Activation state by processing the given image dir.
        auto activationState = server::Activation::Activations::Invalid;
        if (ItemUpdater::validateSquashFSImage(versionId) == 0)
        {
            activationState = server::Activation::Activations::Ready;
        }

        if (updater->activations.find(versionId) == updater->activations.end())
        {
            updater->activations.insert(std::make_pair(
                    versionId,
                    std::make_unique<Activation>(
                            updater->busItem,
                            resp,
                            versionId,
                            extendedVersion,
                            activationState)));
        }
    }
    return 0;
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
    for(const auto& it : activations)
    {
        auto serviceFile = "obmc-flash-bios-ubiumount-rw@" + it.first +
                ".service";

        // Remove the read-write partitions.
        auto method = busItem.new_method_call(
                SYSTEMD_BUSNAME,
                SYSTEMD_PATH,
                SYSTEMD_INTERFACE,
                "StartUnit");
        method.append(serviceFile, "replace");
        busItem.call_noreply(method);
    }

    // Remove the preserved partition.
    auto method = busItem.new_method_call(
            SYSTEMD_BUSNAME,
            SYSTEMD_PATH,
            SYSTEMD_INTERFACE,
            "StartUnit");
    method.append("obmc-flash-bios-ubiumount-prsv.service", "replace");
    busItem.call_noreply(method);

    return;
}

} // namespace updater
} // namespace software
} // namespace openpower
