#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <phosphor-logging/log.hpp>
#include "config.h"
#include "item_updater.hpp"

namespace openpower
{
namespace software
{
namespace manager
{

using namespace phosphor::logging;

constexpr auto SOFTWAREACTIVATION_SERVICE = "org.open_power.Software.Host.Updater";
constexpr auto SOFTWAREACTIVATION_INTERFACE = "org.freedesktop.DBus.Properties";
constexpr auto SOFTWAREACTIVATION_OBJ = "xyz.openbmc_project.Software.Activation";
constexpr auto EXTENDEDVERSION_OBJ = "xyz.openbmc_project.Software.ExtendedVersion";
const std::string& MANIFESTFILE = "/tmp/pnor/MANIFEST";

int ItemUpdater::createActivation(sd_bus_message* msg,
                                  void* userData,
                                  sd_bus_error* retErr)
{

    std::string SOFTWAREACTIVATION_PATH = "/xyz/openbmc_project/software/";

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
        SOFTWAREACTIVATION_PATH += std::string(versionId.c_str());
        if (updater->activations.find(versionId) == updater->activations.end())
        {
            updater->activations.insert(std::make_pair(
                    versionId,
                    std::make_unique<Activation>(
                            updater->busItem,
                            resp)));
        }
    }

    auto method = updater->busItem.new_method_call(SOFTWAREACTIVATION_SERVICE,
                                                   SOFTWAREACTIVATION_PATH.c_str(),
                                                   SOFTWAREACTIVATION_INTERFACE,
                                                   "Set");
    method.append(EXTENDEDVERSION_OBJ,
                  "ExtendedVersion",
                  ItemUpdater::getExtendedVersion(MANIFESTFILE));
    auto reply = updater->busItem.call(method);
    if (reply.is_method_error())
    {
        log<level::ERR>("Error in mapper call");
        return false;
    }

    return 0;
}

std::string ItemUpdater::getExtendedVersion(const std::string& manifestFilePath)
{
    constexpr auto extendedversionKey = "extended_version=";
    constexpr auto extendedversionKeySize = strlen(extendedversionKey);

    if (manifestFilePath.empty())
    {
        log<level::ERR>("Error MANIFESTFilePath is empty");
        throw std::runtime_error("MANIFESTFilePath is empty");
    }

    std::string extendedversion{};
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
            if (line.compare(0, extendedversionKeySize,
                             extendedversionKey) == 0)
            {
                extendedversion = line.substr(extendedversionKeySize);
                break;
            }
        }
        efile.close();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error in reading Host MANIFEST file");
    }

    return extendedversion;
}

} // namespace manager
} // namespace software
} // namespace openpower

