#include <experimental/filesystem>
#include "activation.hpp"
#include "config.h"
#include "item_updater.hpp"
#include "serialize.hpp"
#include <phosphor-logging/log.hpp>

#ifdef WANT_SIGNATURE_VERIFY
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include "image_verify.hpp"
#include "config.h"
#endif

namespace openpower
{
namespace software
{
namespace updater
{

namespace fs = std::experimental::filesystem;
namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

using namespace phosphor::logging;

#ifdef WANT_SIGNATURE_VERIFY
using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
#endif

constexpr auto SYSTEMD_SERVICE = "org.freedesktop.systemd1";
constexpr auto SYSTEMD_OBJ_PATH = "/org/freedesktop/systemd1";

void Activation::subscribeToSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_SERVICE, SYSTEMD_OBJ_PATH,
                                            SYSTEMD_INTERFACE, "Subscribe");
    this->bus.call_noreply(method);

    return;
}

void Activation::unsubscribeFromSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_SERVICE, SYSTEMD_OBJ_PATH,
                                            SYSTEMD_INTERFACE, "Unsubscribe");
    this->bus.call_noreply(method);

    return;
}

void Activation::startActivation()
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

void Activation::finishActivation()
{
    activationProgress->progress(90);

    // Set Redundancy Priority before setting to Active
    if (!redundancyPriority)
    {
        redundancyPriority =
            std::make_unique<RedundancyPriority>(bus, path, *this, 0);
    }

    activationProgress->progress(100);

    activationBlocksTransition.reset(nullptr);
    activationProgress.reset(nullptr);

    ubiVolumesCreated = false;
    Activation::unsubscribeFromSystemdSignals();
    // Remove version object from image manager
    Activation::deleteImageManagerObject();
    // Create active association
    parent.createActiveAssociation(path);
}

auto Activation::activation(Activations value) -> Activations
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

#ifdef WANT_SIGNATURE_VERIFY
            using Signature = openpower::software::image::Signature;

            fs::path imagePath(IMG_UPLOAD_DIR);

            Signature signature(imagePath / versionId, SIGNED_IMAGE_CONF_PATH);

            // Validate the signed image.
            if (!signature.verify())
            {
                log<level::ERR>("Error occurred during image validation");
                report<InternalFailure>();

                return softwareServer::Activation::activation(
                    softwareServer::Activation::Activations::Failed);
            }
#endif

            Activation::startActivation();
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

auto Activation::requestedActivation(RequestedActivations value)
    -> RequestedActivations
{
    ubiVolumesCreated = false;

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

void Activation::deleteImageManagerObject()
{
    // Get the Delete object for <versionID> inside image_manager
    auto method = this->bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                            MAPPER_INTERFACE, "GetObject");

    method.append(path);
    method.append(
        std::vector<std::string>({"xyz.openbmc_project.Object.Delete"}));
    auto mapperResponseMsg = bus.call(method);
    if (mapperResponseMsg.is_method_error())
    {
        log<level::ERR>("Error in Get Delete Object",
                        entry("VERSIONPATH=%s", path));
        return;
    }
    std::map<std::string, std::vector<std::string>> mapperResponse;
    mapperResponseMsg.read(mapperResponse);
    if (mapperResponse.begin() == mapperResponse.end())
    {
        log<level::ERR>("ERROR in reading the mapper response",
                        entry("VERSIONPATH=%s", path));
        return;
    }

    // Call the Delete object for <versionID> inside image_manager
    method = this->bus.new_method_call(
        (mapperResponse.begin()->first).c_str(), path.c_str(),
        "xyz.openbmc_project.Object.Delete", "Delete");
    mapperResponseMsg = bus.call(method);

    // Check that the bus call didn't result in an error
    if (mapperResponseMsg.is_method_error())
    {
        log<level::ERR>("Error in Deleting image from image manager",
                        entry("VERSIONPATH=%s", path));
        return;
    }
}

uint8_t RedundancyPriority::priority(uint8_t value)
{
    parent.parent.freePriority(value, parent.versionId);
    storeToFile(parent.versionId, value);
    return softwareServer::RedundancyPriority::priority(value);
}

void Activation::unitStateChange(sdbusplus::message::message& msg)
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
        Activation::activation(
            softwareServer::Activation::Activations::Activating);
    }

    if ((newStateUnit == ubimountServiceFile) &&
        (newStateResult == "failed" || newStateResult == "dependency"))
    {
        Activation::activation(softwareServer::Activation::Activations::Failed);
    }

    return;
}

} // namespace updater
} // namespace software
} // namespace openpower
