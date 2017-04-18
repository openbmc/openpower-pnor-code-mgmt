#include "activation.hpp"
#include "config.h"

namespace openpower
{
namespace software
{
namespace updater
{

int Activation::handleActivationChangedSignal(sd_bus_message* msg,
                                              void* data,
                                              sd_bus_error* err)
{
    auto message = sdbusplus::message::message(msg);
    static_cast<Activation*>(data)->handleActivationChanged(message, err);
    return 0;
}

void Activation::handleActivationChanged(sdbusplus::message::message& msg,
                                         sd_bus_error* err)
{
    constexpr auto propertyActivation = "Activation";
    constexpr auto propertyRequestedActivation = "RequestedActivation";
    std::string key;
    std::map<std::string, sdbusplus::message::variant<std::string>> msgData;
    msg.read(key, msgData);
    std::string value = sdbusplus::message::variant_ns::get<std::string>(
                                msgData.begin()->second);

    // Activation Property
    if (!(msgData.begin()->first).compare(propertyActivation))
    {
        if (!value.compare(convertForMessage(
                    Activation::Activations::Activating).c_str()))
        {
            activationBlocksTransitions.insert(std::make_pair(
                    versionId,
                    std::make_unique<ActivationBlocksTransition>(
                                busActivation,
                                pathActivation)));

            // Creating a mount point to access squashfs image
            constexpr auto squashfsmountService = "obmc-flash-bios-squashfsmount@";
            auto squashfsServiceFile = std::string(squashfsmountService) +
                                       versionId +
                                       ".service";
            auto method = busActivation.new_method_call(
                    SYSTEMD_BUSNAME,
                    SYSTEMD_PATH,
                    SYSTEMD_INTERFACE,
                    "StartUnit");
            method.append(squashfsmountServiceFile,
                          "replace");
            busActivation.call_noreply(method);
        }
        else
        {
            auto block = activationBlocksTransitions.find(versionId);
            if (block != activationBlocksTransitions.end())
            {
                activationBlocksTransitions.erase(block);
            }
        }
    }
    // RequestedActivation Property
    else if (!(msgData.begin()->first).compare(propertyRequestedActivation))
    {
        if (!value.compare(convertForMessage(
                    Activation::RequestedActivations::Active).c_str()))
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
    }
}

} // namespace updater
} // namespace software
} // namespace openpower

