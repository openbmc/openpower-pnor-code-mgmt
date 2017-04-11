#pragma once

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Software/Activation/server.hpp>
#include <xyz/openbmc_project/Software/ActivationBlocksTransition/server.hpp>
#include "xyz/openbmc_project/Software/ExtendedVersion/server.hpp"

namespace openpower
{
namespace software
{
namespace updater
{

using ActivationInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::ExtendedVersion,
    sdbusplus::xyz::openbmc_project::Software::server::Activation>;
using ActivationBlocksTransitionInherit = sdbusplus::server::object::object<
 sdbusplus::xyz::openbmc_project::Software::server::ActivationBlocksTransition>;

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
         * @param[in] versionId - The software version id
         */
        Activation(sdbusplus::bus::bus& bus, const std::string& path,
                   std::string& versionId,
                   std::string& extVersion) :
                   ActivationInherit(bus, path.c_str()),
                   activationChanged(
                        bus,
                        matchStr(path).c_str(),
                        handleActivationChangedSignal,
                        this),
                   busActivation(bus),
                   pathActivation(path),
                   versionId(versionId) 

        {
            // Set Properties.
            extendedVersion(extVersion);

            // Emit deferred signal.
            emit_object_added();
        }

    private:
        /** @brief Callback function for Activation PropertiesChanged
         *
         * @param[in]  msg   - Data associated with subscribed signal
         * @param[in]  data  - Pointer to this object instance
         * @param[out] err   - The sdbus error reference
         */
        static int handleActivationChangedSignal(sd_bus_message* msg,
                                                 void* data,
                                                 sd_bus_error* err);

        /**
         * @brief Handle when Activation value changes
         *
         * @param[in] msg - Expanded sdbusplus message data
         * @param[in] err - Contains any sdbus error reference if occurred
         */
        void handleActivationChanged(sdbusplus::message::message& msg,
                                     sd_bus_error* err);

        /** @brief Constructs the sdbusplus signal match string */
        static const std::string matchStr(const std::string& path)
        {
            return std::string("type='signal',"
                               "member='PropertiesChanged',"
                               "path='" + path + "',"
                               "interface='org.freedesktop.DBus.Properties',");
        }

        /** @brief sdbusplus signal match for PropertiesChanged */
        sdbusplus::server::match::match activationChanged;

        /** @brief Persistent sdbusplus DBus bus connection */
        sdbusplus::bus::bus& busActivation;

        /** @brief Persistent DBus object path */
        std::string pathActivation;

        /** @brief Version id */
        std::string versionId;
};

/** @class ActivationBlocksTransition
 *  @brief OpenBMC ActivationBlocksTransition implementation.
 *  @details A concrete implementation for
 *  xyz.openbmc_project.Software.ActivationBlocksTransition DBus API.
 */
class ActivationBlocksTransition : public ActivationBlocksTransitionInherit
{
    public:
        /** @brief Constructs ActivationBlocksTransition.
         *
         * @param[in] bus    - The Dbus bus object
         * @param[in] path   - The Dbus object path
         */
        ActivationBlocksTransition(sdbusplus::bus::bus& bus,
                                   const std::string& path) :
                   ActivationBlocksTransitionInherit(bus, path.c_str()) {}
};

} // namespace updater
} // namespace software
} // namespace openpower

