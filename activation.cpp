#include <experimental/filesystem>
#include "activation.hpp"
#include "config.h"
#include "item_updater.hpp"
#include "serialize.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

namespace fs = std::experimental::filesystem;
namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

constexpr auto SYSTEMD_SERVICE   = "org.freedesktop.systemd1";
constexpr auto SYSTEMD_OBJ_PATH  = "/org/freedesktop/systemd1";

void Activation::subscribeToSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_SERVICE,
                                            SYSTEMD_OBJ_PATH,
                                            SYSTEMD_INTERFACE,
                                            "Subscribe");
    this->bus.call_noreply(method);

    return;
}

void Activation::unsubscribeFromSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_SERVICE,
                                            SYSTEMD_OBJ_PATH,
                                            SYSTEMD_INTERFACE,
                                            "Unsubscribe");
    this->bus.call_noreply(method);

    return;
}

void Activation::createSymlinks()
{
    if (!fs::is_directory(PNOR_ACTIVE_PATH))
    {
        fs::create_directories(PNOR_ACTIVE_PATH);
    }

    // If the RW or RO active links exist, remove them and create new
    // ones pointing to the active version.
    if (fs::is_symlink(PNOR_RO_ACTIVE_PATH))
    {
        fs::remove(PNOR_RO_ACTIVE_PATH);
    }
    fs::create_directory_symlink(PNOR_RO_PREFIX + versionId,
            PNOR_RO_ACTIVE_PATH);
    if (fs::is_symlink(PNOR_RW_ACTIVE_PATH))
    {
        fs::remove(PNOR_RW_ACTIVE_PATH);
    }
    fs::create_directory_symlink(PNOR_RW_PREFIX + versionId,
            PNOR_RW_ACTIVE_PATH);

    // There is only one preserved directory as it is not tied to a
    // version, so just create the link if it doesn't exist already
    if (!fs::is_symlink(PNOR_PRSV_ACTIVE_PATH))
    {
        fs::create_directory_symlink(PNOR_PRSV,
                PNOR_PRSV_ACTIVE_PATH);
    }
}

void Activation::startActivation()
{
    // Since the squashfs image has not yet been loaded to pnor and the
    // RW volumes have not yet been created, we need to start the
    // service files for each of those actions.

    if (!activationProgress)
    {
        activationProgress = std::make_unique<ActivationProgress>(
                bus, path);
    }

    if (!activationBlocksTransition)
    {
        activationBlocksTransition =
                std::make_unique<ActivationBlocksTransition>(bus, path);
    }

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
           versionId + ".service";
    method = bus.new_method_call(
            SYSTEMD_BUSNAME,
            SYSTEMD_PATH,
            SYSTEMD_INTERFACE,
            "StartUnit");
    method.append(ubimountServiceFile, "replace");
    bus.call_noreply(method);

    activationProgress->progress(10);
}

void Activation::finishActivation()
{
    activationProgress->progress(90);
    createSymlinks();

    // Set Redundancy Priority before setting to Active
    if (!redundancyPriority)
    {
        redundancyPriority = std::make_unique<RedundancyPriority>(
                bus, path, *this, 0);
    }

    activationProgress->progress(100);

    activationBlocksTransition.reset(nullptr);
    activationProgress.reset(nullptr);

    squashfsLoaded = false;
    rwVolumesCreated = false;
    Activation::unsubscribeFromSystemdSignals();
}

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

        if (squashfsLoaded == false && rwVolumesCreated == false)
        {
            Activation::startActivation();
            return softwareServer::Activation::activation(value);
        }
        else if (squashfsLoaded == true && rwVolumesCreated == true)
        {
            // Only when the squashfs image is finished loading AND the RW
            // volumes have been created do we proceed with activation. To
            // verify that this happened, we check for the mount dirs PNOR_PRSV
            // and PNOR_RW_PREFIX_<versionid>, as well as the image dir R0.

            if ((fs::is_directory(PNOR_PRSV)) &&
                (fs::is_directory(PNOR_RW_PREFIX + versionId)) &&
                (fs::is_directory(PNOR_RO_PREFIX + versionId)))
            {
                Activation::finishActivation();
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

auto Activation::requestedActivation(RequestedActivations value) ->
        RequestedActivations
{
    squashfsLoaded = false;
    rwVolumesCreated = false;

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
    parent.parent.freePriority(value);

    if(parent.parent.isLowestPriority(value))
    {
        // Need to update the symlinks to point to Software Version
        // with lowest priority.
        parent.createSymlinks();
    }

    storeToFile(parent.versionId, value);
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

    if(newStateUnit == squashfsMountServiceFile && newStateResult == "done")
    {
        squashfsLoaded = true;
        activationProgress->progress(activationProgress->progress() + 20);
    }

    if(newStateUnit == ubimountServiceFile && newStateResult == "done")
    {
        rwVolumesCreated = true;
        activationProgress->progress(activationProgress->progress() + 50);
    }

    if(squashfsLoaded && rwVolumesCreated)
    {
        Activation::activation(
                softwareServer::Activation::Activations::Activating);
    }

    if((newStateUnit == squashfsMountServiceFile ||
        newStateUnit == ubimountServiceFile) &&
        (newStateResult == "failed" || newStateResult == "dependency"))
    {
        Activation::activation(softwareServer::Activation::Activations::Failed);
    }

    return;
}

void Activation::delete_()
{
    parent.erase(versionId);
}

} // namespace updater
} // namespace software
} // namespace openpower
