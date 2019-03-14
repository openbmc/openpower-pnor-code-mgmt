#include "utils.hpp"

#include <phosphor-logging/log.hpp>

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

// FIXME: Could we use hard-coded service here?
constexpr auto HIOMAPD_SERVICE = "org.openbmc.mboxd";
constexpr auto HIOMAPD_PATH = "/xyz/openbmc_project/Hiomapd";
constexpr auto HIOMAPD_INTERFACE = "xyz.openbmc_project.Hiomapd.Control";

void hiomapdSuspend(sdbusplus::bus::bus& bus)
{
    auto method = bus.new_method_call(HIOMAPD_SERVICE, HIOMAPD_PATH,
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
    auto method = bus.new_method_call(HIOMAPD_SERVICE, HIOMAPD_PATH,
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
