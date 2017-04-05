#include "activation.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

auto Activation::activation(Activations value) ->
        Activations
{
    return sdbusplus::xyz::openbmc_project::Software::server::Activation::
            activation(value);
}

auto Activation::requestedActivation(RequestedActivations value) ->
        RequestedActivations
{
    return sdbusplus::xyz::openbmc_project::Software::server::Activation::
            requestedActivation(value);
}

} // namespace updater
} // namespace software
} // namespace openpower

