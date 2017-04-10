#include "activation.hpp"

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
    std::string key;
    std::map<std::string, sdbusplus::message::variant<std::string>> msgData;
    msg.read(key, msgData);
    std::string value = sdbusplus::message::variant_ns::get<std::string>(
                                msgData.begin()->second);

    if (!value.compare(convertForMessage(
                Activation::Activations::Activating).c_str()))
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
}

} // namespace updater
} // namespace software
} // namespace openpower

