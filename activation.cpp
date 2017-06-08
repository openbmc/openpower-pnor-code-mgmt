#include <experimental/filesystem>
#include "activation.hpp"
#include "config.h"
#include <string>
#include <fstream>
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

        // The ubimount service files attemps to create the RW and Preserved
        // UBI volumes. If the service fails, the mount directories PNOR_PRSV
        // and PNOR_RW_PREFIX_<versionid> won't be present. Check for the
        // existence of those directories to validate the service file was
        // successful, also for the existence of the RO directory where the
        // image is supposed to reside.
        if ((fs::exists(PNOR_PRSV)) &&
            (fs::exists(PNOR_RW_PREFIX + versionId)) &&
            (fs::exists(PNOR_RO_PREFIX + versionId)))
        {
            // Set Redundancy Priority before setting to Active
            if (!redundancyPriority)
            {
                redundancyPriority =
                          std::make_unique<RedundancyPriority>(
                                    bus,
                                    path,
                                    versionId);
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

void RedundancyPriority::freePriority(uint8_t basePriority)
{
    std::string OBJECT_MANAGER("org.freedesktop.DBus.ObjectManager");
    std::string OBJECT_PROPERTIES("org.freedesktop.DBus.Properties");
    std::string PRIORITY("xyz.openbmc_project.Software.RedundancyPriority");

    auto method = this->bus.new_method_call(BUSNAME_UPDATER,
                                            SOFTWARE_OBJPATH,
                                            OBJECT_MANAGER.c_str(),
                                            "GetManagedObjects");
    auto reply = this->bus.call(method);
    if (reply.is_method_error())
    {
        log<level::ERR>("Failed to get Managed Objects",
                        entry("OBJPATH=%s", SOFTWARE_OBJPATH));
        return;
    }

    std::map<sdbusplus::message::object_path, std::map<std::string,
        std::map<std::string, sdbusplus::message::variant<uint8_t>>>>
        m;
    reply.read(m);

    for (const auto& intf1 : m)
    {
        std::string path(std::move(intf1.first));
        for (const auto& intf2 : intf1.second)
        {
            if (intf2.first == PRIORITY.c_str())
            {
                for (const auto& property : intf2.second)
                {
                    if (property.first == "Priority")
                    {
                        if(basePriority == sdbusplus::message::
                            variant_ns::get<uint8_t>(property.second));
                        {
                            method = this->bus.new_method_call(
                                            BUSNAME_UPDATER,
                                            path.c_str(),
                                            OBJECT_PROPERTIES.c_str(),
                                            "Set");
                            sdbusplus::message::variant<uint8_t> value =
                                basePriority + 1;
                            method.append(PRIORITY.c_str(),
                                          "Priority",
                                          value);
                            this->bus.call_noreply(method);
                            return;
                        }
                    }
                }
            }
        }
    }
    return;
}

uint8_t RedundancyPriority::priority(uint8_t value)
{
    // The RW and Preserved UBI volumes are already created during
    // the activation process for this particular <versionId>.
    if (value == 0)
    {
        // If the RW or RO active links exist, remove them and create new
        // ones pointing to the active version.
        if (!fs::exists(PNOR_ACTIVE_PATH))
        {
            fs::create_directories(PNOR_ACTIVE_PATH);
        }
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
    }
    RedundancyPriority::freePriority(value);
    return softwareServer::RedundancyPriority::priority(value);
}

} // namespace updater
} // namespace software
} // namespace openpower

