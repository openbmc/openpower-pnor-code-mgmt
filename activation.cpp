#include <experimental/filesystem>
#include "activation.hpp"
#include "config.h"
#include <string>
#include <fstream>

namespace openpower
{
namespace software
{
namespace updater
{

namespace fs = std::experimental::filesystem;
namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

constexpr auto HOSTUPDATER_SERVICE("org.open_power.Software.Host.Updater");
constexpr auto HOSTUPDATER_PATH("/xyz/openbmc_project/software");
constexpr auto ObjectManager("org.freedesktop.DBus.ObjectManager");
constexpr auto ACTIVATION_PRIORITY("xyz.openbmc_project.Software.RedundancyPriority");
constexpr auto ACTIVATION_INTERFACE("xyz.openbmc_project.Software.Activation");

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

            // Set Redundancy Priority before setting to Active
            if (!redundancyPriority)
            {
                redundancyPriority =
                          std::make_unique<RedundancyPriority>(
                                    bus,
                                    path);
                Activation::priorityUpdate();
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

void Activation::priorityUpdate()
{
    std::string HOST_PATH("/xyz/openbmc_project/software");
    std::string HOST_INTERFACE("org.freedesktop.DBus.ObjectManager");
    auto mapperCall = this->bus.new_method_call("xyz.openbmc_project.ObjectMapper",
                                                "/xyz/openbmc_project/object_mapper",
                                                "xyz.openbmc_project.ObjectMapper",
                                                "GetObject");
    mapperCall.append(HOST_PATH);
    mapperCall.append(std::vector<std::string>({HOST_INTERFACE}));
    auto mapperResponseMsg = bus.call(mapperCall);
    if (mapperResponseMsg.is_method_error())
    {
        printf("Error in GetObject\n");
    }

    std::map<std::string, std::vector<std::string>> mapperResponse;
    mapperResponseMsg.read(mapperResponse);
    if (mapperResponse.begin() == mapperResponse.end())
    {
        printf("ERROR in reading the mapper response\n");
    }
    auto busname = mapperResponse.begin()->first;
    auto method = this->bus.new_method_call(busname.c_str(),
                                            HOST_PATH.c_str(),
                                            HOST_INTERFACE.c_str(),
                                            "GetManagedObjects");
    auto reply = this->bus.call(method);
    if (reply.is_method_error())
    {
        printf("Error in GetManagedObjects\n");
    }
    std::map<sdbusplus::message::object_path, std::map<std::string,
        std::map<std::string, sdbusplus::message::variant<std::string>>>>
        m;
    reply.read(m);

    for (const auto& intf1 : m)
    {
        for (const auto& intf2 : intf1.second)
        {
            if (intf2.first == "/xyz/openbmc_project.Software.Activation")
            {
                for (const auto& property : intf2.second)
                {
                    if (property.first == "Activation")
                    {
                        std::string value = sdbusplus::message::variant_ns::
                            get<std::string>(property.second);
                        printf("Activation: " + value);
                    }
                    else if (property.first == "RequestedActivation")
                    {
                        std::string value = sdbusplus::message::variant_ns::
                            get<std::string>(property.second);
                        printf("RequestedActivation: " + value);
                    }
                }
            }
        }
    }
}

uint8_t RedundancyPriority::priority(uint8_t value)
{
    return softwareServer::RedundancyPriority::priority(value);
}

} // namespace updater
} // namespace software
} // namespace openpower

