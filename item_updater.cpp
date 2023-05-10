#include "config.h"

#include "item_updater.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>

#include <filesystem>

namespace openpower
{
namespace software
{
namespace updater
{
namespace server = sdbusplus::xyz::openbmc_project::Software::server;
namespace fs = std::filesystem;

using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;

void ItemUpdater::createActivation(sdbusplus::message_t& m)
{
    using SVersion = server::Version;
    using VersionPurpose = SVersion::VersionPurpose;

    sdbusplus::message::object_path objPath;
    std::map<std::string, std::map<std::string, std::variant<std::string>>>
        interfaces;
    m.read(objPath, interfaces);

    std::string path(std::move(objPath));
    std::string filePath;
    auto purpose = VersionPurpose::Unknown;
    std::string version;

    for (const auto& intf : interfaces)
    {
        if (intf.first == VERSION_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Purpose")
                {
                    // Only process the Host and System images
                    auto value = SVersion::convertVersionPurposeFromString(
                        std::get<std::string>(property.second));

                    if (value == VersionPurpose::Host ||
                        value == VersionPurpose::System)
                    {
                        purpose = value;
                    }
                }
                else if (property.first == "Version")
                {
                    version = std::get<std::string>(property.second);
                }
            }
        }
        else if (intf.first == FILEPATH_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Path")
                {
                    filePath = std::get<std::string>(property.second);
                }
            }
        }
    }
    if ((filePath.empty()) || (purpose == VersionPurpose::Unknown))
    {
        return;
    }

    // Version id is the last item in the path
    auto pos = path.rfind("/");
    if (pos == std::string::npos)
    {
        log<level::ERR>("No version id found in object path",
                        entry("OBJPATH=%s", path.c_str()));
        return;
    }

    auto versionId = path.substr(pos + 1);

    if (activations.find(versionId) == activations.end())
    {
        // Determine the Activation state by processing the given image dir.
        auto activationState = server::Activation::Activations::Invalid;
        AssociationList associations = {};
        if (validateImage(filePath))
        {
            activationState = server::Activation::Activations::Ready;
            // Create an association to the host inventory item
            associations.emplace_back(std::make_tuple(
                ACTIVATION_FWD_ASSOCIATION, ACTIVATION_REV_ASSOCIATION,
                HOST_INVENTORY_PATH));
        }

        fs::path manifestPath(filePath);
        manifestPath /= MANIFEST_FILE;
        std::string extendedVersion =
            (Version::getValue(
                 manifestPath.string(),
                 std::map<std::string, std::string>{{"extended_version", ""}}))
                .begin()
                ->second;

        auto activation = createActivationObject(
            path, versionId, extendedVersion, activationState, associations);
        activations.emplace(versionId, std::move(activation));

        auto versionPtr = createVersionObject(path, versionId, version, purpose,
                                              filePath);
        versions.emplace(versionId, std::move(versionPtr));
    }
    return;
}

void ItemUpdater::createActiveAssociation(const std::string& path)
{
    assocs.emplace_back(
        std::make_tuple(ACTIVE_FWD_ASSOCIATION, ACTIVE_REV_ASSOCIATION, path));
    associations(assocs);
}

void ItemUpdater::createUpdateableAssociation(const std::string& path)
{
    assocs.emplace_back(std::make_tuple(UPDATEABLE_FWD_ASSOCIATION,
                                        UPDATEABLE_REV_ASSOCIATION, path));
    associations(assocs);
}

void ItemUpdater::updateFunctionalAssociation(const std::string& versionId)
{
    std::string path = std::string{SOFTWARE_OBJPATH} + '/' + versionId;
    // remove all functional associations
    for (auto iter = assocs.begin(); iter != assocs.end();)
    {
        if ((std::get<0>(*iter)).compare(FUNCTIONAL_FWD_ASSOCIATION) == 0)
        {
            iter = assocs.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
    assocs.emplace_back(std::make_tuple(FUNCTIONAL_FWD_ASSOCIATION,
                                        FUNCTIONAL_REV_ASSOCIATION, path));
    associations(assocs);
}

void ItemUpdater::removeAssociation(const std::string& path)
{
    for (auto iter = assocs.begin(); iter != assocs.end();)
    {
        if ((std::get<2>(*iter)).compare(path) == 0)
        {
            iter = assocs.erase(iter);
            associations(assocs);
        }
        else
        {
            ++iter;
        }
    }
}

bool ItemUpdater::erase(std::string entryId)
{
    if (isVersionFunctional(entryId) && isChassisOn())
    {
        log<level::ERR>(("Error: Version " + entryId +
                         " is currently active and running on the host."
                         " Unable to remove.")
                            .c_str());
        return false;
    }

    // Removing entry in versions map
    auto it = versions.find(entryId);
    if (it == versions.end())
    {
        log<level::ERR>(("Error: Failed to find version " + entryId +
                         " in item updater versions map."
                         " Unable to remove.")
                            .c_str());
    }
    else
    {
        versions.erase(entryId);
    }

    // Removing entry in activations map
    auto ita = activations.find(entryId);
    if (ita == activations.end())
    {
        log<level::ERR>(("Error: Failed to find version " + entryId +
                         " in item updater activations map."
                         " Unable to remove.")
                            .c_str());
    }
    else
    {
        removeAssociation(ita->second->path);
        activations.erase(entryId);
    }
    return true;
}

bool ItemUpdater::isChassisOn()
{
    auto mapperCall = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetObject");

    mapperCall.append(CHASSIS_STATE_PATH,
                      std::vector<std::string>({CHASSIS_STATE_OBJ}));

    std::map<std::string, std::vector<std::string>> mapperResponse;

    try
    {
        auto mapperResponseMsg = bus.call(mapperCall);
        mapperResponseMsg.read(mapperResponse);
        if (mapperResponse.empty())
        {
            log<level::ERR>("Invalid Response from mapper");
            elog<InternalFailure>();
        }
    }
    catch (const sdbusplus::exception_t& e)
    {
        log<level::ERR>("Error in Mapper call");
        elog<InternalFailure>();
    }

    auto method = bus.new_method_call((mapperResponse.begin()->first).c_str(),
                                      CHASSIS_STATE_PATH,
                                      SYSTEMD_PROPERTY_INTERFACE, "Get");
    method.append(CHASSIS_STATE_OBJ, "CurrentPowerState");

    std::variant<std::string> currentChassisState;

    try
    {
        auto response = bus.call(method);
        response.read(currentChassisState);
        auto strParam = std::get<std::string>(currentChassisState);
        return (strParam != CHASSIS_STATE_OFF);
    }
    catch (const sdbusplus::exception_t& e)
    {
        log<level::ERR>("Error in fetching current Chassis State",
                        entry("MAPPERRESPONSE=%s",
                              (mapperResponse.begin()->first).c_str()));
        elog<InternalFailure>();
    }
}

} // namespace updater
} // namespace software
} // namespace openpower
