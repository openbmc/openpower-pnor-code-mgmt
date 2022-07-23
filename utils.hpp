#pragma once

// With OpenSSL 1.1.0, some functions were deprecated. Need to abstract them
// to make the code backward compatible with older OpenSSL veresions.
// Reference: https://wiki.openssl.org/index.php/OpenSSL_1.1.0_Changes
#if OPENSSL_VERSION_NUMBER < 0x10100000L

#include <openssl/evp.h>

#include <sdbusplus/bus.hpp>

#include <string>

extern "C"
{
    EVP_MD_CTX* EVP_MD_CTX_new(void);
    void EVP_MD_CTX_free(EVP_MD_CTX* ctx);
}

namespace utils
{

/**
 * @brief Gets the D-Bus Service name for the input D-Bus path
 *
 * @param[in] bus  -  Bus handler
 * @param[in] path -  Object Path
 * @param[in] intf -  Interface
 *
 * @return  Service name
 * @error   InternalFailure exception thrown
 */
std::string getService(sdbusplus::bus_t& bus, const std::string& path,
                       const std::string& intf);

/** @brief Suspend hiomapd.
 *
 * @param[in] bus - The D-Bus bus object.
 */
void hiomapdSuspend(sdbusplus::bus_t& bus);

/** @brief Resume hiomapd.
 *
 * @param[in] bus - The D-Bus bus object.
 */
void hiomapdResume(sdbusplus::bus_t& bus);

/** @brief Set the Hardware Management Console Managed bios attribute to
 *         Disabled to clear the indication that the system is HMC-managed.
 *
 * @param[in] bus - The D-Bus bus object.
 */
void clearHMCManaged(sdbusplus::bus_t& bus);

/** @brief Set the Clear hypervisor NVRAM bios attribute to Enabled to indicate
 *         to the hypervisor to clear its NVRAM.
 *
 * @param[in] bus - The D-Bus bus object.
 */
void setClearNvram(sdbusplus::bus_t& bus);

/** @brief DeleteAll error logs
 *
 * @param[in] bus - The D-Bus bus object.
 */
void deleteAllErrorLogs(sdbusplus::bus_t& bus);

} // namespace utils

#endif // OPENSSL_VERSION_NUMBER < 0x10100000L
