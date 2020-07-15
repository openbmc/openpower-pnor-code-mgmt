#include "activation_static.hpp"

#include "item_updater.hpp"

#include <phosphor-logging/log.hpp>

namespace openpower
{
namespace software
{
namespace updater
{
namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

using namespace phosphor::logging;

auto ActivationStatic::activation(Activations value) -> Activations
{

    auto ret = value;
    if (value != softwareServer::Activation::Activations::Active)
    {
        redundancyPriority.reset(nullptr);
    }

    if (value == softwareServer::Activation::Activations::Activating)
    {
        fs::path imagePath(IMG_DIR);
        imagePath /= versionId;

        for (const auto& entry : fs::directory_iterator(imagePath))
        {
            if (entry.path().extension() == ".pnor")
            {
                pnorFilePath = entry;
                break;
            }
        }
        if (pnorFilePath.empty())
        {
            log<level::ERR>("Unable to find pnor file",
                            entry("DIR=%s", imagePath.c_str()));
            ret = softwareServer::Activation::Activations::Failed;
            goto out;
        }
#ifdef WANT_SIGNATURE_VERIFY
        // Validate the signed image.
        if (!validateSignature(pnorFilePath.filename()))
        {
            // Cleanup
            activationBlocksTransition.reset(nullptr);
            activationProgress.reset(nullptr);

            ret = softwareServer::Activation::Activations::Failed;
            goto out;
        }
#endif
        if (parent.freeSpace())
        {
            startActivation();
        }
        else
        {
            ret = softwareServer::Activation::Activations::Failed;
        }
    }
    else
    {
        activationBlocksTransition.reset(nullptr);
        activationProgress.reset(nullptr);
    }

out:
    return softwareServer::Activation::activation(ret);
}

void ActivationStatic::startActivation()
{
    if (!activationProgress)
    {
        activationProgress = std::make_unique<ActivationProgress>(bus, path);
    }

    if (!activationBlocksTransition)
    {
        activationBlocksTransition =
            std::make_unique<ActivationBlocksTransition>(bus, path);
    }

    // TODO: check why the signal is still received without calling this
    // function?
    subscribeToSystemdSignals();

    log<level::INFO>("Start programming...",
                     entry("PNOR=%s", pnorFilePath.c_str()));

    std::string pnorFileEscaped = pnorFilePath.string();
    // Escape all '/' to '-'
    std::replace(pnorFileEscaped.begin(), pnorFileEscaped.end(), '/', '-');

    constexpr auto updatePNORService = "openpower-pnor-update@";
    pnorUpdateUnit =
        std::string(updatePNORService) + pnorFileEscaped + ".service";
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append(pnorUpdateUnit, "replace");
    bus.call_noreply(method);

    activationProgress->progress(10);
}

void ActivationStatic::unitStateChange(sdbusplus::message::message& msg)
{
    uint32_t newStateID{};
    sdbusplus::message::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};

    // Read the msg and populate each variable
    msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);

    if (newStateUnit == pnorUpdateUnit)
    {
        if (newStateResult == "done")
        {
            finishActivation();
        }
        if (newStateResult == "failed" || newStateResult == "dependency")
        {
            Activation::activation(
                softwareServer::Activation::Activations::Failed);
        }
    }
}

void ActivationStatic::finishActivation()
{
    activationProgress->progress(90);

    // Set Redundancy Priority before setting to Active
    if (!redundancyPriority)
    {
        redundancyPriority =
            std::make_unique<RedundancyPriority>(bus, path, *this, 0);
    }

    activationProgress->progress(100);

    activationBlocksTransition.reset();
    activationProgress.reset();

    unsubscribeFromSystemdSignals();
    // Remove version object from image manager
    deleteImageManagerObject();
    // Create active association
    parent.createActiveAssociation(path);
    // Create updateable association as this
    // can be re-programmed.
    parent.createUpdateableAssociation(path);
    // Create functional assocaition
    parent.updateFunctionalAssociation(versionId);

    Activation::activation(Activation::Activations::Active);
}

} // namespace updater
} // namespace software
} // namespace openpower
