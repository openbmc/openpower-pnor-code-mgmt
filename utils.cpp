#include "config.h"

#include "utils.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#if OPENSSL_VERSION_NUMBER < 0x10100000L

#include <string.h>

static void* OPENSSL_zalloc(size_t num)
{
    void* ret = OPENSSL_malloc(num);

    if (ret != NULL)
    {
        memset(ret, 0, num);
    }
    return ret;
}

EVP_MD_CTX* EVP_MD_CTX_new(void)
{
    return (EVP_MD_CTX*)OPENSSL_zalloc(sizeof(EVP_MD_CTX));
}

void EVP_MD_CTX_free(EVP_MD_CTX* ctx)
{
    EVP_MD_CTX_cleanup(ctx);
    OPENSSL_free(ctx);
}

#endif // OPENSSL_VERSION_NUMBER < 0x10100000L

namespace utils
{

using namespace phosphor::logging;

constexpr auto HIOMAPD_PATH = "/xyz/openbmc_project/Hiomapd";
constexpr auto HIOMAPD_INTERFACE = "xyz.openbmc_project.Hiomapd.Control";

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

std::string getService(sdbusplus::bus_t& bus, const std::string& path,
                       const std::string& intf)
{
    auto mapper = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_INTERFACE, "GetObject");

    mapper.append(path, std::vector<std::string>({intf}));
    try
    {
        auto mapperResponseMsg = bus.call(mapper);

        std::vector<std::pair<std::string, std::vector<std::string>>>
            mapperResponse;
        mapperResponseMsg.read(mapperResponse);
        if (mapperResponse.empty())
        {
            log<level::ERR>("Error reading mapper response");
            throw std::runtime_error("Error reading mapper response");
        }
        return mapperResponse[0].first;
    }
    catch (const sdbusplus::exception_t& ex)
    {
        log<level::ERR>("Mapper call failed", entry("METHOD=%d", "GetObject"),
                        entry("PATH=%s", path.c_str()),
                        entry("INTERFACE=%s", intf.c_str()));
        throw std::runtime_error("Mapper call failed");
    }
}

void hiomapdSuspend(sdbusplus::bus_t& bus)
{
    auto service = getService(bus, HIOMAPD_PATH, HIOMAPD_INTERFACE);
    auto method = bus.new_method_call(service.c_str(), HIOMAPD_PATH,
                                      HIOMAPD_INTERFACE, "Suspend");

    try
    {
        bus.call_noreply(method);
    }
    catch (const sdbusplus::exception_t& e)
    {
        log<level::ERR>("Error in mboxd suspend call",
                        entry("ERROR=%s", e.what()));
    }
}

void hiomapdResume(sdbusplus::bus_t& bus)
{
    auto service = getService(bus, HIOMAPD_PATH, HIOMAPD_INTERFACE);
    auto method = bus.new_method_call(service.c_str(), HIOMAPD_PATH,
                                      HIOMAPD_INTERFACE, "Resume");

    method.append(true); // Indicate PNOR is modified

    try
    {
        bus.call_noreply(method);
    }
    catch (const sdbusplus::exception_t& e)
    {
        log<level::ERR>("Error in mboxd suspend call",
                        entry("ERROR=%s", e.what()));
    }
}

void setPendingAttributes(sdbusplus::bus_t& bus, const std::string& attrName,
                          const std::string& attrValue)
{
    constexpr auto biosConfigPath = "/xyz/openbmc_project/bios_config/manager";
    constexpr auto biosConfigIntf = "xyz.openbmc_project.BIOSConfig.Manager";
    constexpr auto dbusAttrType =
        "xyz.openbmc_project.BIOSConfig.Manager.AttributeType.Enumeration";

    using PendingAttributesType = std::vector<std::pair<
        std::string, std::tuple<std::string, std::variant<std::string>>>>;
    PendingAttributesType pendingAttributes;
    pendingAttributes.emplace_back(
        std::make_pair(attrName, std::make_tuple(dbusAttrType, attrValue)));

    try
    {
        auto service = getService(bus, biosConfigPath, biosConfigIntf);
        auto method = bus.new_method_call(service.c_str(), biosConfigPath,
                                          SYSTEMD_PROPERTY_INTERFACE, "Set");
        method.append(biosConfigIntf, "PendingAttributes",
                      std::variant<PendingAttributesType>(pendingAttributes));
        bus.call(method);
    }
    catch (const sdbusplus::exception_t& e)
    {
        log<level::ERR>("Error setting the bios attribute",
                        entry("ERROR=%s", e.what()),
                        entry("ATTRIBUTE=%s", attrName.c_str()),
                        entry("ATTRIBUTE_VALUE=%s", attrValue.c_str()));
        return;
    }
}

void clearHMCManaged(sdbusplus::bus_t& bus)
{
    setPendingAttributes(bus, "pvm_hmc_managed", "Disabled");
}

void setClearNvram(sdbusplus::bus_t& bus)
{
    setPendingAttributes(bus, "pvm_clear_nvram", "Enabled");
}

void deleteAllErrorLogs(sdbusplus::bus_t& bus)
{
    constexpr auto loggingPath = "/xyz/openbmc_project/logging";
    constexpr auto deleteAllIntf = "xyz.openbmc_project.Collection.DeleteAll";

    auto service = getService(bus, loggingPath, deleteAllIntf);
    auto method = bus.new_method_call(service.c_str(), loggingPath,
                                      deleteAllIntf, "DeleteAll");

    try
    {
        bus.call_noreply(method);
    }
    catch (const sdbusplus::exception_t& e)
    {
        log<level::ERR>("Error deleting all error logs",
                        entry("ERROR=%s", e.what()));
    }
}

} // namespace utils
