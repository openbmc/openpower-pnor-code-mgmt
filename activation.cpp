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

    if (value != softwareServer::Activation::Activations::Active)
    {
        redundancyPriority.reset(nullptr);
    }

    if (value == softwareServer::Activation::Activations::Activating)
    {
        softwareServer::Activation::activation(value);

        if (squashfsLoaded == false && rwVolumesCreated == false) {
            // If the squashfs image has not yet been loaded to pnor and the
            // RW volumes have not yet been created, we need to start the
            // service files for each of those actions.

            if (!activationBlocksTransition)
            {
                activationBlocksTransition =
                          std::make_unique<ActivationBlocksTransition>(
                                    bus,
                                    path);
            }

            // Load the squashfs image to pnor so that it is available to be
            // activated when requested.
            // This is done since the image on the BMC can be removed.
            constexpr auto squashfsMountService =
                    "obmc-flash-bios-squashfsmount@";
            auto squashfsMountServiceFile = std::string(squashfsMountService) +
                    versionId + ".service";
            auto method = bus.new_method_call(
                    SYSTEMD_BUSNAME,
                    SYSTEMD_PATH,
                    SYSTEMD_INTERFACE,
                    "StartUnit");
            method.append(squashfsMountServiceFile, "replace");
            bus.call_noreply(method);

            constexpr auto ubimountService = "obmc-flash-bios-ubimount@";
            auto ubimountServiceFile = std::string(ubimountService) +
                   versionId +
                   ".service";
            method = bus.new_method_call(
                    SYSTEMD_BUSNAME,
                    SYSTEMD_PATH,
                    SYSTEMD_INTERFACE,
                    "StartUnit");
            method.append(ubimountServiceFile, "replace");
            bus.call_noreply(method);

            return softwareServer::Activation::activation(
                    softwareServer::Activation::Activations::Activating);
        }
        else if (squashfsLoaded == true && rwVolumesCreated == true)
        {
            // Only when the squashfs image is finished loading AND the RW
            // volumes have been created do we proceed with activation.

            // The ubimount service files attemps to create the RW and Preserved
            // UBI volumes. If the service fails, the mount dirs PNOR_PRSV
            // and PNOR_RW_PREFIX_<versionid> won't be present. Check for the
            // existence of those directories to validate the service file was
            // successful, also for the existence of the RO directory where the
            // image is supposed to reside.
            if ((fs::exists(PNOR_PRSV)) &&
                (fs::exists(PNOR_RW_PREFIX + versionId)) &&
                (fs::exists(PNOR_RO_PREFIX + versionId)))
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
                    fs::create_directory_symlink(PNOR_PRSV,
                            PNOR_PRSV_ACTIVE_PATH);
                }

                // Set Redundancy Priority before setting to Active
                if (!redundancyPriority)
                {
                    redundancyPriority =
                              std::make_unique<RedundancyPriority>(
                                        bus,
                                        path);
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
            // If either the squashfs image has not yet been loaded or the RW
            // volumes have not yet been created, the activation process is
            // ongoing, so we return "Activating" status.
            return softwareServer::Activation::activation(
                    softwareServer::Activation::Activations::Activating);
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

uint8_t RedundancyPriority::priority(uint8_t value)
{
    return softwareServer::RedundancyPriority::priority(value);
}

void Activation::unitStateChange(sdbusplus::message::message& msg)
{
    uint32_t newStateID {};
    sdbusplus::message::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};

    //Read the msg and populate each variable
    msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);

    auto squashfsMountServiceFile =
            "obmc-flash-bios-squashfsmount@" + versionId + ".service";

    auto ubimountServiceFile =
            "obmc-flash-bios-ubimount@" + versionId + ".service";

    if((newStateUnit == squashfsMountServiceFile) && (newStateResult == "done"))
    {
       squashfsLoaded = true;
    }

    if((newStateUnit == ubimountServiceFile) && (newStateResult == "done"))
    {
        rwVolumesCreated = true;
    }

    Activation::activation(softwareServer::Activation::Activations::Activating);

    return;
}

} // namespace updater
} // namespace software
} // namespace openpower
