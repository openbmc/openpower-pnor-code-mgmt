#pragma once

// With OpenSSL 1.1.0, some functions were deprecated. Need to abstract them
// to make the code backward compatible with older OpenSSL veresions.
// Reference: https://wiki.openssl.org/index.php/OpenSSL_1.1.0_Changes
#if OPENSSL_VERSION_NUMBER < 0x10100000L

#include <openssl/evp.h>

#include <sdbusplus/bus.hpp>

extern "C" {
EVP_MD_CTX* EVP_MD_CTX_new(void);
void EVP_MD_CTX_free(EVP_MD_CTX* ctx);
}

namespace utils
{

/** @brief Suspend hiomapd.
 *
 * @param[in] bus - The D-Bus bus object.
 */
void hiomapdSuspend(sdbusplus::bus::bus& bus);

/** @brief Resume hiomapd.
 *
 * @param[in] bus - The D-Bus bus object.
 */
void hiomapdResume(sdbusplus::bus::bus& bus);

} // namespace utils

#endif // OPENSSL_VERSION_NUMBER < 0x10100000L
