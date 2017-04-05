#include "activation.hpp"

namespace openpower
{
namespace software
{
namespace manager
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
}

} // namespace manager
} // namespace software
} // namespace openpower

