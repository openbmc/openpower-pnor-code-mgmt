#pragma once

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>

namespace openpower
{
namespace software
{
namespace manager
{

using ActivationInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::Activation>;

/** @class Activation
 *  @brief OpenBMC activation software management implementation.
 *  @details A concrete implementation for
 *  xyz.openbmc_project.Software.Activation DBus API.
 */
class Activation : public ActivationInherit
{
    public:
        /** @brief Constructs Activation Software Manager
         *
         * @param[in] bus    - The Dbus bus object
         * @param[in] path   - The Dbus object path
         */
        Activation(sdbusplus::bus::bus& bus, const std::string& path,
                   auto ActivationStatus) :
                   ActivationInherit(bus, path.c_str(), true)
        {
            // Set Properties.
            activation(ActivationStatus); 
            // Emit deferred signal.
            emit_object_added();
        }
};

} // namespace manager
} // namespace software
} // namespace openpower

