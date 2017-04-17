#include "activation.hpp"
#include "config.h"

namespace openpower
{
namespace software
{
namespace updater
{

namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

auto Activation::activation(Activations value) ->
        Activations
{
    if (value == softwareServer::Activation::Activations::Activating)
    {
        activationBlocksTransitions.insert(std::make_pair(
                  versionId,
                  std::make_unique<ActivationBlocksTransition>(
                            busActivation,
                            pathActivation)));
    }
    else
    {
        auto block = activationBlocksTransitions.find(versionId);
        if (block != activationBlocksTransitions.end())
        {
            activationBlocksTransitions.erase(block);
        }
    }
    return softwareServer::Activation::activation(value);
}

auto Activation::requestedActivation(RequestedActivations value) ->
        RequestedActivations
{
    if (value == softwareServer::Activation::RequestedActivations::Active)
    {
        constexpr auto ubimountService = "obmc-flash-bios-ubimount@";
        auto ubimountServiceFile = std::string(ubimountService) +
                                   versionId +
                                   ".service";
        auto method = busActivation.new_method_call(
                SYSTEMD_BUSNAME,
                SYSTEMD_PATH,
                SYSTEMD_INTERFACE,
                "StartUnit");
        method.append(ubimountServiceFile,
                      "replace");
        busActivation.call_noreply(method);
    }
    return softwareServer::Activation::requestedActivation(value);
}

} // namespace updater
} // namespace software
} // namespace openpower

