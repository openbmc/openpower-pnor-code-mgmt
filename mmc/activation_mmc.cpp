#include "activation_mmc.hpp"

namespace openpower
{
namespace software
{
namespace updater
{
namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

auto ActivationMMC::activation(Activations value) -> Activations
{
    return softwareServer::Activation::activation(value);
}

void ActivationMMC::startActivation()
{
}

void ActivationMMC::unitStateChange(sdbusplus::message::message& msg)
{
}

void ActivationMMC::finishActivation()
{
}

} // namespace updater
} // namespace software
} // namespace openpower
