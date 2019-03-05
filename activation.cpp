#include "config.h"

#include "activation.hpp"

#include "item_updater.hpp"

#include <experimental/filesystem>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/exception.hpp>

#ifdef WANT_SIGNATURE_VERIFY
#include "image_verify.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
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
using sdbusplus::exception::SdBusError;

#ifdef WANT_SIGNATURE_VERIFY
using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

// Field mode path and interface.
constexpr auto FIELDMODE_PATH("/xyz/openbmc_project/software");
constexpr auto FIELDMODE_INTERFACE("xyz.openbmc_project.Control.FieldMode");
#endif

constexpr auto SYSTEMD_SERVICE = "org.freedesktop.systemd1";
constexpr auto SYSTEMD_OBJ_PATH = "/org/freedesktop/systemd1";

void Activation::subscribeToSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_SERVICE, SYSTEMD_OBJ_PATH,
                                            SYSTEMD_INTERFACE, "Subscribe");
    try
    {
        this->bus.call_noreply(method);
    }
    catch (const SdBusError& e)
    {
        if (e.name() != nullptr &&
            strcmp("org.freedesktop.systemd1.AlreadySubscribed", e.name()) == 0)
        {
            // If an Activation attempt fails, the Unsubscribe method is not
            // called. This may lead to an AlreadySubscribed error if the
            // Activation is re-attempted.
        }
        else
        {
            log<level::ERR>("Error subscribing to systemd",
                            entry("ERROR=%s", e.what()));
        }
    }
    return;
}

void Activation::unsubscribeFromSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_SERVICE, SYSTEMD_OBJ_PATH,
                                            SYSTEMD_INTERFACE, "Unsubscribe");
    this->bus.call_noreply(method);

    return;
}

auto Activation::requestedActivation(RequestedActivations value)
    -> RequestedActivations
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
            activation(softwareServer::Activation::Activations::Activating);
        }
    }
    return softwareServer::Activation::requestedActivation(value);
}

void Activation::deleteImageManagerObject()
{
    // Get the Delete object for <versionID> inside image_manager
    constexpr auto versionServiceStr = "xyz.openbmc_project.Software.Version";
    constexpr auto deleteInterface = "xyz.openbmc_project.Object.Delete";
    std::string versionService;
    auto method = this->bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                            MAPPER_INTERFACE, "GetObject");

    method.append(path);
    method.append(std::vector<std::string>({deleteInterface}));
    auto mapperResponseMsg = bus.call(method);
    if (mapperResponseMsg.is_method_error())
    {
        log<level::ERR>("Error in Get Delete Object",
                        entry("VERSIONPATH=%s", path.c_str()));
        return;
    }
    std::map<std::string, std::vector<std::string>> mapperResponse;
    mapperResponseMsg.read(mapperResponse);
    if (mapperResponse.begin() == mapperResponse.end())
    {
        log<level::ERR>("ERROR in reading the mapper response",
                        entry("VERSIONPATH=%s", path.c_str()));
        return;
    }

    // We need to find the phosphor-software-manager's version service
    // to invoke the delete interface
    for (auto resp : mapperResponse)
    {
        if (resp.first.find(versionServiceStr) != std::string::npos)
        {
            versionService = resp.first;
        }
    }

    if (versionService.empty())
    {
        log<level::ERR>("Error finding version service");
        return;
    }

    // Call the Delete object for <versionID> inside image_manager
    method = this->bus.new_method_call(versionService.c_str(), path.c_str(),
                                       deleteInterface, "Delete");
    try
    {
        mapperResponseMsg = bus.call(method);

        // Check that the bus call didn't result in an error
        if (mapperResponseMsg.is_method_error())
        {
            log<level::ERR>("Error in Deleting image from image manager",
                            entry("VERSIONPATH=%s", path.c_str()));
            return;
        }
    }
    catch (const SdBusError& e)
    {
        if (e.name() != nullptr && strcmp("System.Error.ELOOP", e.name()) == 0)
        {
            // TODO: Error being tracked with openbmc/openbmc#3311
        }
        else
        {
            log<level::ERR>("Error performing call to Delete object path",
                            entry("ERROR=%s", e.what()),
                            entry("PATH=%s", path.c_str()));
        }
        return;
    }
}

uint8_t RedundancyPriority::priority(uint8_t value)
{
    parent.parent.freePriority(value, parent.versionId);
    return softwareServer::RedundancyPriority::priority(value);
}

#ifdef WANT_SIGNATURE_VERIFY
inline bool Activation::validateSignature()
{
    using Signature = openpower::software::image::Signature;
    fs::path imageDir(IMG_DIR);

    Signature signature(imageDir / versionId, PNOR_SIGNED_IMAGE_CONF_PATH);

    // Validate the signed image.
    if (signature.verify())
    {
        return true;
    }
    // Log error and continue activation process, if field mode disabled.
    log<level::ERR>("Error occurred during image validation");
    report<InternalFailure>();

    try
    {
        if (!fieldModeEnabled())
        {
            return true;
        }
    }
    catch (const InternalFailure& e)
    {
        report<InternalFailure>();
    }
    return false;
}

bool Activation::fieldModeEnabled()
{
    auto fieldModeSvc = getService(bus, FIELDMODE_PATH, FIELDMODE_INTERFACE);

    auto method = bus.new_method_call(fieldModeSvc.c_str(), FIELDMODE_PATH,
                                      "org.freedesktop.DBus.Properties", "Get");

    method.append(FIELDMODE_INTERFACE, "FieldModeEnabled");
    auto reply = bus.call(method);
    if (reply.is_method_error())
    {
        log<level::ERR>("Error in fieldModeEnabled getValue");
        elog<InternalFailure>();
    }
    sdbusplus::message::variant<bool> fieldMode;
    reply.read(fieldMode);

    return sdbusplus::message::variant_ns::get<bool>(fieldMode);
}

std::string Activation::getService(sdbusplus::bus::bus& bus,
                                   const std::string& path,
                                   const std::string& intf)
{
    auto mapperCall = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetObject");

    mapperCall.append(path);
    mapperCall.append(std::vector<std::string>({intf}));

    auto mapperResponseMsg = bus.call(mapperCall);

    if (mapperResponseMsg.is_method_error())
    {
        log<level::ERR>("ERROR in getting service",
                        entry("PATH=%s", path.c_str()),
                        entry("INTERFACE=%s", intf.c_str()));

        elog<InternalFailure>();
    }

    std::map<std::string, std::vector<std::string>> mapperResponse;
    mapperResponseMsg.read(mapperResponse);

    if (mapperResponse.begin() == mapperResponse.end())
    {
        log<level::ERR>("ERROR reading mapper response",
                        entry("PATH=%s", path.c_str()),
                        entry("INTERFACE=%s", intf.c_str()));

        elog<InternalFailure>();
    }
    return mapperResponse.begin()->first;
}
#endif

} // namespace updater
} // namespace software
} // namespace openpower
