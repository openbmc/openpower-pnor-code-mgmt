#pragma once

#include <sdbusplus/server.hpp>
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
         * @param[in] versionId  - The software version id
         * @param[in] extVersion - The extended version
         */
        Activation(sdbusplus::bus::bus& bus, const std::string& path,
                   std::string& versionId,
                   std::string& extVersion) :
                   ActivationInherit(bus, path.c_str(), true),
                   bus(bus),
                   path(path),
                   versionId(versionId)
        {
            // Set Properties.
            extendedVersion(extVersion);

            // Emit deferred signal.
            emit_object_added();
        }

        /** @brief Overloaded Activation property setter function
         *
         *  @param[in] value - One of Activation::Activations
         *
         *  @return Success or exception thrown
         */
        Activations activation(Activations value) override;

        /** @brief Overloaded requestedActivation property setter function
         *
         *  @param[in] value - One of Activation::RequestedActivations
         *
         *  @return Success or exception thrown
         */
        RequestedActivations requestedActivation(RequestedActivations value)
                override;

        /** @brief Persistent sdbusplus DBus bus connection */
        sdbusplus::bus::bus& bus;

        /** @brief Persistent DBus object path */
        std::string path;

        /** @brief Version id */
        std::string versionId;

        /** @brief Persistent ActivationBlocksTransition dbus object */
        std::unique_ptr<ActivationBlocksTransition> activationBlocksTransition;

    private:
        /** @brief Writes the PNOR partitions to flash
         *
         *  @return Success or error
         */
        int writePartition();
};

} // namespace updater
} // namespace software
} // namespace openpower

