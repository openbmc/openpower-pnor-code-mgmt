#include <experimental/filesystem>
#include "activation.hpp"
#include "config.h"

namespace openpower
{
namespace software
{
namespace updater
{

namespace fs = std::experimental::filesystem;
namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

auto Activation::activation(Activations value) ->
        Activations
{
    if (value == softwareServer::Activation::Activations::Activating)
    {
        softwareServer::Activation::activation(value);

        if (!activationBlocksTransition)
        {
            activationBlocksTransition =
                      std::make_unique<ActivationBlocksTransition>(
                                bus,
                                path);
        }

        constexpr auto ubimountService = "obmc-flash-bios-ubimount@";
        auto ubimountServiceFile = std::string(ubimountService) +
                                   versionId +
                                   ".service";
        auto method = bus.new_method_call(
                            SYSTEMD_BUSNAME,
                            SYSTEMD_PATH,
                            SYSTEMD_INTERFACE,
                            "StartUnit");
        method.append(ubimountServiceFile,
                      "replace");
        bus.call_noreply(method);

        // Check UBI volumes were created
        if ((fs::exists(PNOR_PRSV)) &&
            (fs::exists(PNOR_RW_PREFIX + versionId)))
        {
            if (!fs::exists(PNOR_ACTIVE_PATH))
            {
                fs::create_directories(PNOR_ACTIVE_PATH);
            }

            // If the RW or RO active links exist, remove them and create new
            // ones pointing to the active version.
            if (fs::exists(PNOR_RO_ACTIVE_PATH))
            {
                fs::remove(PNOR_RO_ACTIVE_PATH);
            }
            fs::create_directory_symlink(PNOR_RO_PREFIX + versionId,
                    PNOR_RO_ACTIVE_PATH);
            if (fs::exists(PNOR_RW_ACTIVE_PATH))
            {
                fs::remove(PNOR_RW_ACTIVE_PATH);
            }
            fs::create_directory_symlink(PNOR_RW_PREFIX + versionId,
                    PNOR_RW_ACTIVE_PATH);

            // There is only one preserved directory as it is not tied to a
            // version, so just create the link if it doesn't exist already.
            if (!fs::exists(PNOR_PRSV_ACTIVE_PATH))
            {
                fs::create_directory_symlink(PNOR_PRSV, PNOR_PRSV_ACTIVE_PATH);
            }
            return softwareServer::Activation::activation(
                    softwareServer::Activation::Activations::Active);
        }
        else
        {
            return softwareServer::Activation::activation(
                    softwareServer::Activation::Activations::Failed);
        }
    }
    else
    {
        activationBlocksTransition.reset(nullptr);
        return softwareServer::Activation::activation(value);
    }
}

auto Activation::requestedActivation(RequestedActivations value) ->
        RequestedActivations
{
    if ((value == softwareServer::Activation::RequestedActivations::Active) &&
        (softwareServer::Activation::requestedActivation() !=
                  softwareServer::Activation::RequestedActivations::Active))
    {
        if ((softwareServer::Activation::activation() ==
                    softwareServer::Activation::Activations::Ready) ||
            (softwareServer::Activation::activation() ==
                    softwareServer::Activation::Activations::Failed))
        {
            Activation::activation(
                    softwareServer::Activation::Activations::Activating);

        }
    }
    return softwareServer::Activation::requestedActivation(value);
}

} // namespace updater
} // namespace software
} // namespace openpower

