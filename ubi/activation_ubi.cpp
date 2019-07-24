#include "activation_ubi.hpp"

#include "item_updater.hpp"
#include "serialize.hpp"

#include <experimental/filesystem>
#include <phosphor-logging/log.hpp>

namespace openpower
{
namespace software
{
namespace updater
{
namespace fs = std::experimental::filesystem;
namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;
using namespace phosphor::logging;

uint8_t RedundancyPriorityUbi::priority(uint8_t value)
{
    storeToFile(parent.versionId, value);
    return RedundancyPriority::priority(value);
}

auto ActivationUbi::activation(Activations value) -> Activations
{

    if (value != softwareServer::Activation::Activations::Active)
    {
        redundancyPriority.reset(nullptr);
    }

    if (value == softwareServer::Activation::Activations::Activating)
    {
        parent.freeSpace();
        softwareServer::Activation::activation(value);

        if (ubiVolumesCreated == false)
        {
            // Enable systemd signals
            subscribeToSystemdSignals();

#ifdef WANT_SIGNATURE_VERIFY
            // Validate the signed image.
            if (!validateSignature(squashFSImage))
            {
                // Cleanup
                activationBlocksTransition.reset(nullptr);
                activationProgress.reset(nullptr);

                return softwareServer::Activation::activation(
                    softwareServer::Activation::Activations::Failed);
            }
#endif
            startActivation();
            return softwareServer::Activation::activation(value);
        }
        else if (ubiVolumesCreated == true)
        {
            // Only when the squashfs image is finished loading AND the RW
            // volumes have been created do we proceed with activation. To
            // verify that this happened, we check for the mount dirs PNOR_PRSV
            // and PNOR_RW_PREFIX_<versionid>, as well as the image dir R0.

            if ((fs::is_directory(PNOR_PRSV)) &&
                (fs::is_directory(PNOR_RW_PREFIX + versionId)) &&
                (fs::is_directory(PNOR_RO_PREFIX + versionId)))
            {
                finishActivation();
                if (Activation::checkApplyTimeImmediate())
                {
                    log<level::INFO>("Image Active. ApplyTime is immediate, "
                                     "rebooting Host.");
                    Activation::rebootHost();
                }
                return softwareServer::Activation::activation(
                    softwareServer::Activation::Activations::Active);
            }
            else
            {
                activationBlocksTransition.reset(nullptr);
                activationProgress.reset(nullptr);
                return softwareServer::Activation::activation(
                    softwareServer::Activation::Activations::Failed);
            }
        }
    }
    else
    {
        activationBlocksTransition.reset(nullptr);
        activationProgress.reset(nullptr);
    }

    return softwareServer::Activation::activation(value);
}

auto ActivationUbi::requestedActivation(RequestedActivations value)
    -> RequestedActivations
{
    ubiVolumesCreated = false;
    return Activation::requestedActivation(value);
}

void ActivationUbi::startActivation()
{
    // Since the squashfs image has not yet been loaded to pnor and the
    // RW volumes have not yet been created, we need to start the
    // service files for each of those actions.

    if (!activationProgress)
    {
        activationProgress = std::make_unique<ActivationProgress>(bus, path);
    }

    if (!activationBlocksTransition)
    {
        activationBlocksTransition =
            std::make_unique<ActivationBlocksTransition>(bus, path);
    }

    constexpr auto ubimountService = "obmc-flash-bios-ubimount@";
    auto ubimountServiceFile =
        std::string(ubimountService) + versionId + ".service";
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(ubimountServiceFile, "replace");
    bus.call_noreply(method);

    activationProgress->progress(10);
}

void ActivationUbi::unitStateChange(sdbusplus::message::message& msg)
{
    uint32_t newStateID{};
    sdbusplus::message::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};

    // Read the msg and populate each variable
    msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);

    auto ubimountServiceFile =
        "obmc-flash-bios-ubimount@" + versionId + ".service";

    if (newStateUnit == ubimountServiceFile && newStateResult == "done")
    {
        ubiVolumesCreated = true;
        activationProgress->progress(activationProgress->progress() + 50);
    }

    if (ubiVolumesCreated)
    {
        activation(softwareServer::Activation::Activations::Activating);
    }

    if ((newStateUnit == ubimountServiceFile) &&
        (newStateResult == "failed" || newStateResult == "dependency"))
    {
        activation(softwareServer::Activation::Activations::Failed);
    }

    return;
}

void ActivationUbi::finishActivation()
{
    activationProgress->progress(90);

    // Set Redundancy Priority before setting to Active
    if (!redundancyPriority)
    {
        redundancyPriority =
            std::make_unique<RedundancyPriorityUbi>(bus, path, *this, 0);
    }

    activationProgress->progress(100);

    activationBlocksTransition.reset(nullptr);
    activationProgress.reset(nullptr);

    ubiVolumesCreated = false;
    unsubscribeFromSystemdSignals();
    // Remove version object from image manager
    deleteImageManagerObject();
    // Create active association
    parent.createActiveAssociation(path);
}

} // namespace updater
} // namespace software
} // namespace openpower
