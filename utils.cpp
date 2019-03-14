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

using sdbusplus::exception::SdBusError;
using namespace phosphor::logging;

constexpr auto HIOMAPD_PATH = "/xyz/openbmc_project/Hiomapd";
constexpr auto HIOMAPD_INTERFACE = "xyz.openbmc_project.Hiomapd.Control";

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
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
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        log<level::ERR>("Mapper call failed", entry("METHOD=%d", "GetObject"),
                        entry("PATH=%s", path.c_str()),
                        entry("INTERFACE=%s", intf.c_str()));
        throw std::runtime_error("Mapper call failed");
    }
}

void hiomapdSuspend(sdbusplus::bus::bus& bus)
{
    auto service = getService(bus, HIOMAPD_PATH, HIOMAPD_INTERFACE);
    auto method = bus.new_method_call(service.c_str(), HIOMAPD_PATH,
                                      HIOMAPD_INTERFACE, "Suspend");

    try
    {
        bus.call_noreply(method);
    }
    catch (const SdBusError& e)
    {
        log<level::ERR>("Error in mboxd suspend call",
                        entry("ERROR=%s", e.what()));
    }
}

void hiomapdResume(sdbusplus::bus::bus& bus)
{
    auto service = getService(bus, HIOMAPD_PATH, HIOMAPD_INTERFACE);
    auto method = bus.new_method_call(service.c_str(), HIOMAPD_PATH,
                                      HIOMAPD_INTERFACE, "Resume");

    method.append(true); // Indicate PNOR is modified

    try
    {
        bus.call_noreply(method);
    }
    catch (const SdBusError& e)
    {
        log<level::ERR>("Error in mboxd suspend call",
                        entry("ERROR=%s", e.what()));
    }
}

} // namespace utils
