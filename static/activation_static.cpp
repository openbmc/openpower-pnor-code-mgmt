#include "activation_static.hpp"

#include "item_updater.hpp"
#include "pnor_utils.hpp"

#include <filesystem>
#include <phosphor-logging/log.hpp>

namespace openpower
{
namespace software
{
namespace updater
{
namespace fs = std::filesystem;
namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

using namespace phosphor::logging;

auto ActivationStatic::activation(Activations value) -> Activations
{

    if (value != softwareServer::Activation::Activations::Active)
    {
        redundancyPriority.reset(nullptr);
    }

    if (value == softwareServer::Activation::Activations::Activating)
    {
        parent.freeSpace();
        startActivation();
        return softwareServer::Activation::activation(value);
    }
    else
    {
        activationBlocksTransition.reset(nullptr);
        activationProgress.reset(nullptr);
    }

    return softwareServer::Activation::activation(value);
}

void ActivationStatic::startActivation()
{
    fs::path pnorFile;
    fs::path imagePath(IMG_DIR);
    imagePath /= versionId;

    for (const auto& entry : fs::directory_iterator(imagePath))
    {
        if (entry.path().extension() == ".pnor")
        {
            pnorFile = entry;
            break;
        }
    }
    if (pnorFile.empty())
    {
        log<level::ERR>("Unable to find pnor file",
                        entry("DIR=%s", imagePath.c_str()));
        return;
    }

    if (!activationProgress)
    {
        activationProgress = std::make_unique<ActivationProgress>(bus, path);
    }

    if (!activationBlocksTransition)
    {
        activationBlocksTransition =
            std::make_unique<ActivationBlocksTransition>(bus, path);
    }

    // TOOD: this function seem able to be skipped?
    subscribeToSystemdSignals();

    log<level::INFO>("Start programming...",
                     entry("PNOR=%s", pnorFile.c_str()));

    std::string pnorFileEscaped = pnorFile.string();
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
    // Create functional assocaition
    parent.updateFunctionalAssociation(versionId);

    Activation::activation(Activation::Activations::Active);
}

} // namespace updater
} // namespace software
} // namespace openpower
