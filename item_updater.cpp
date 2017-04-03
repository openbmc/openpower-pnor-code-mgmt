#include <phosphor-logging/log.hpp>
#include "config.h"
#include "item_updater.hpp"
#include "version_host_software_manager.hpp"

namespace openpower
{
namespace software
{
namespace manager
{

using namespace phosphor::logging;

constexpr auto SOFTWAREACTIVATION_SERVICE = "org.open_power.Software.Host.Updater";
constexpr auto SOFTWAREACTIVATION_PATH = "/xyz/openbmc_project/software/36b7936a";
constexpr auto SOFTWAREACTIVATION_INTERFACE = "org.freedesktop.DBus.Properties";

constexpr auto ACTIVATION_PATH = "xyz.openbmc_project.Software.Activation";
constexpr auto ACTIVATION_INTERFACE = "Activation";

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

        auto versionId = std::stoi(resp.substr(pos + 1), nullptr, 16);
        if (updater->activations.find(versionId) == updater->activations.end())
        {
            updater->activations.insert(std::make_pair(
                    versionId,
                    std::make_unique<Activation>(
                            updater->busItem,
                            resp)));
        }
    }

    auto result = ItemUpdater::validateVersion();
    auto method = updater->busItem.new_method_call(SOFTWAREACTIVATION_SERVICE,
                                                   SOFTWAREACTIVATION_PATH,
                                                   SOFTWAREACTIVATION_INTERFACE,
                                                   "Set"); 
    
    if (result == 0) // Validation using MANIFEST Passed
    {
        method.append(ACTIVATION_PATH,
                      ACTIVATION_INTERFACE,
                      "xyz.openbmc_project.Software.Activation.Activations.Ready");
    }
    else // Validation using MANIFEST failed
    {
        method.append(ACTIVATION_PATH,
                      ACTIVATION_INTERFACE,
                      "xyz.openbmc_project.Software.Activation.Activations.Invalid");
    }
  
    auto reply = updater->busItem.call(method);
    if (reply.is_method_error())
    {
        log<level::ERR>("Error in mapper call",
                        entry("PATH=%s", ACTIVATION_PATH),
                        entry("INTERFACE=%s", ACTIVATION_INTERFACE));
        return -1;
    }
    if (mapperResponse.empty())
    {
        log<level::ERR>("Error reading mapper response",
                        entry("PATH=%s", ACTIVATION_PATH),
                        entry("INTERFACE=%s", ACTIVATION_INTERFACE));
        return -1;
    }
    
    return 0;
}

int ItemUpdater::validateVersion(void)
{
    const std::string& manifestFilePath = "/tmp/pnor/MANIFEST";
    const std::string& tocFilePath = "/tmp/pnor/pnor.toc";

    auto version_MANIFEST = Version::getVersion(manifestFilePath);
    auto version_pnortoc = Version::getVersion(tocFilePath);

    if (version_MANIFEST == version_pnortoc)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

} // namespace manager
} // namespace software
} // namespace openpower

