#include "activation_static.hpp"

namespace openpower
{
namespace software
{
namespace updater
{
namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

void ActivationStatic::startActivation()
{
}

void ActivationStatic::unitStateChange(sdbusplus::message::message& msg)
{
}

void ActivationStatic::finishActivation()
{
}

} // namespace updater
} // namespace software
} // namespace openpower
