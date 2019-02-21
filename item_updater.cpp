#include "config.h"

#include "item_updater.hpp"

#include "xyz/openbmc_project/Common/error.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>

namespace openpower
{
namespace software
{
namespace updater
{
using namespace sdbusplus::xyz::openbmc_project::Common::Error;
using namespace phosphor::logging;

void ItemUpdater::createActiveAssociation(const std::string& path)
{
    assocs.emplace_back(
        std::make_tuple(ACTIVE_FWD_ASSOCIATION, ACTIVE_REV_ASSOCIATION, path));
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
    auto mapperResponseMsg = bus.call(mapperCall);
    if (mapperResponseMsg.is_method_error())
    {
        log<level::ERR>("Error in Mapper call");
        elog<InternalFailure>();
    }
    using MapperResponseType = std::map<std::string, std::vector<std::string>>;
    MapperResponseType mapperResponse;
    mapperResponseMsg.read(mapperResponse);
    if (mapperResponse.empty())
    {
        log<level::ERR>("Invalid Response from mapper");
        elog<InternalFailure>();
    }

    auto method = bus.new_method_call((mapperResponse.begin()->first).c_str(),
                                      CHASSIS_STATE_PATH,
                                      SYSTEMD_PROPERTY_INTERFACE, "Get");
    method.append(CHASSIS_STATE_OBJ, "CurrentPowerState");
    auto response = bus.call(method);
    if (response.is_method_error())
    {
        log<level::ERR>("Error in fetching current Chassis State",
                        entry("MAPPERRESPONSE=%s",
                              (mapperResponse.begin()->first).c_str()));
        elog<InternalFailure>();
    }
    sdbusplus::message::variant<std::string> currentChassisState;
    response.read(currentChassisState);
    auto strParam =
        sdbusplus::message::variant_ns::get<std::string>(currentChassisState);
    return (strParam != CHASSIS_STATE_OFF);
}

} // namespace updater
} // namespace software
} // namespace openpower
